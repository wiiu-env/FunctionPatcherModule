#include "function_patcher.h"
#include "FunctionAddressProvider.h"
#include "PatchedFunctionData.h"
#include "utils/CThread.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <coreinit/atomic.h>
#include <coreinit/cache.h>
#include <coreinit/core.h>
#include <coreinit/debug.h>
#include <coreinit/memorymap.h>
#include <coreinit/spinlock.h>

#include <kernel/kernel.h>

#include <memory>
#include <mutex>
#include <vector>

enum class PatchState {
    PREPARE,
    MAIN_CORE_PATCHING_DONE,
    ALL_CORES_PATCHING_DONE
};
static volatile PatchState gPatchState = PatchState::PREPARE;
static volatile int32_t gCoresReady    = 0;
static volatile int32_t gCoresFlushed  = 0;

static std::recursive_mutex sPatch_RestoreMutex;
static OSSpinLock sGlobalSpinLock;
static bool sSpinLockInitialized = false;

struct WorkerTask {
    uint32_t targetPhys;
    uint32_t sourcePhys;
    uint32_t effectiveAddr;
    void *jumpData;
    uint32_t jumpDataSize;
    void *jumpToOriginal;
    void *realCallFunctionAddressPtr;
};

static void applyKernelPatchOnCore(CThread *thread, void *arg) {
    (void) thread;
    auto *tasks = (std::vector<WorkerTask> *) arg;

    OSSpinLock localLock;
    OSInitSpinLock(&localLock);
    OSUninterruptibleSpinLock_Acquire(&localLock);

    OSAddAtomic(&gCoresReady, 1);

    // Wait for the main core to finish preparing physical addresses
    while (gPatchState == PatchState::PREPARE) {
        asm volatile("nop");
    }

    for (const auto &task : *tasks) {
        KernelCopyData(task.targetPhys, task.sourcePhys, 4);
    }

    for (const auto &task : *tasks) {
        if (task.jumpData) {
            ICInvalidateRange(task.jumpData, task.jumpDataSize * sizeof(uint32_t));
        }
        if (task.jumpToOriginal) {
            ICInvalidateRange(task.jumpToOriginal, 5 * sizeof(uint32_t));
        }
        if (task.realCallFunctionAddressPtr) {
            ICInvalidateRange(task.realCallFunctionAddressPtr, sizeof(uint32_t));
        }
        if (task.effectiveAddr) {
            ICInvalidateRange((void *) task.effectiveAddr, 4);
        }
    }

    // Force pipeline flush
    asm volatile("sync; isync");

    // Atomically signal main core that our caches are clean
    OSAddAtomic(&gCoresFlushed, 1);

    // Wait for the release signal
    while (gPatchState == PatchState::ALL_CORES_PATCHING_DONE) {}

    // RESTORE INTERRUPTS
    OSUninterruptibleSpinLock_Release(&localLock);
}

struct PatchDispatchCtx {
    std::shared_ptr<PatchedFunctionData> func;
    bool result;
};

struct BatchPatchDispatchCtx {
    std::vector<std::shared_ptr<PatchedFunctionData>> *list;
    bool result;
};

static void PatchFunctionsBatchDispatcher(CThread *thread, void *arg) {
    (void) thread;
    auto *ctx   = (BatchPatchDispatchCtx *) arg;
    ctx->result = PatchFunctions(*(ctx->list));
}

static void RestoreFunctionDispatcher(CThread *thread, void *arg) {
    (void) thread;
    auto *ctx   = (PatchDispatchCtx *) arg;
    ctx->result = RestoreFunction(ctx->func);
}

bool PatchFunctions(std::vector<std::shared_ptr<PatchedFunctionData>> &patchedFunctions) {
    if (OSGetCoreId() != OSGetMainCoreId()) {
        DEBUG_FUNCTION_LINE_INFO("PatchFunctions called from Core %d. Dispatching to Main Core %d...", OSGetCoreId(), OSGetMainCoreId());
        BatchPatchDispatchCtx ctx = {&patchedFunctions, false};
        {
            CThread thread(CThread::eAttributeAffCore1, OSGetCurrentThread()->priority, 0x1000, PatchFunctionsBatchDispatcher, &ctx);
            thread.resumeThread();
        }
        return ctx.result;
    }
    std::lock_guard lock(sPatch_RestoreMutex);

    gPatchState   = PatchState::PREPARE;
    gCoresReady   = 0;
    gCoresFlushed = 0;

    std::vector<std::shared_ptr<PatchedFunctionData>> validToPatch;
    validToPatch.reserve(patchedFunctions.size());

    for (auto &patch : patchedFunctions) {
        if (patch->isPatched) { continue; }
        if (!patch->shouldBePatched()) { continue; }
        if (!patch->updateFunctionAddresses()) { continue; }

        if (!ReadFromPhysicalAddress(patch->realPhysicalFunctionAddress, &patch->replacedInstruction)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to read instruction for %s", patch->functionName.value_or("").c_str());
            continue;
        }

        patch->generateJumpToOriginal();
        patch->generateReplacementJump();
        validToPatch.push_back(patch);
    }

    if (validToPatch.empty()) {
        return true;
    }


    // This is very important. Otherwise the heap meta data might not up to date on all cores
    DCFlushRange(gJumpHeapData, JUMP_HEAP_DATA_SIZE);
    ICInvalidateRange(gJumpHeapData, JUMP_HEAP_DATA_SIZE);

    std::vector<WorkerTask> tasks;
    tasks.reserve(validToPatch.size());

    for (auto &pf : validToPatch) {
        WorkerTask task                 = {};
        task.targetPhys                 = pf->realPhysicalFunctionAddress;
        task.effectiveAddr              = pf->realEffectiveFunctionAddress;
        task.jumpData                   = pf->jumpData;
        task.jumpDataSize               = pf->jumpDataSize;
        task.jumpToOriginal             = pf->jumpToOriginal;
        task.realCallFunctionAddressPtr = pf->realCallFunctionAddressPtr;

        uint32_t replace_ptr = (uint32_t) &pf->replaceWithInstruction;
        if (replace_ptr < 0x00800000 || replace_ptr >= 0x01000000) {
            task.sourcePhys = OSEffectiveToPhysical(replace_ptr);
        } else {
            task.sourcePhys = replace_ptr + 0x30800000 - 0x00800000;
        }
        tasks.push_back(task);
    }

    if (!sSpinLockInitialized) {
        OSInitSpinLock(&sGlobalSpinLock);
        sSpinLockInitialized = true;
    }


    CThread *threadA = CThread::create(applyKernelPatchOnCore, &tasks, CThread::eAttributeAffCore2, 0);
    CThread *threadB = CThread::create(applyKernelPatchOnCore, &tasks, CThread::eAttributeAffCore0, 0);
    threadA->resumeThread();
    threadB->resumeThread();

    while (gCoresReady < 2) {
        OSSleepTicks(1);
    }

    DEBUG_FUNCTION_LINE_VERBOSE("Applying %d patches on Core %d", validToPatch.size(), OSGetCoreId());

    OSUninterruptibleSpinLock_Acquire(&sGlobalSpinLock);

    // D-Cache flush and Kernel Copy for all tasks
    for (size_t i = 0; i < validToPatch.size(); ++i) {
        auto &pf   = validToPatch[i];
        auto &task = tasks[i];

        uint32_t replace_ptr = (uint32_t) &pf->replaceWithInstruction;
        DCFlushRange((void *) replace_ptr, 4);

        if (pf->jumpData) DCFlushRange(pf->jumpData, pf->jumpDataSize * sizeof(uint32_t));
        if (pf->jumpToOriginal) DCFlushRange(pf->jumpToOriginal, 5 * sizeof(uint32_t));
        if (pf->realCallFunctionAddressPtr) DCFlushRange(pf->realCallFunctionAddressPtr, sizeof(uint32_t));

        KernelCopyData(task.targetPhys, task.sourcePhys, 4);
    }

    DCFlushRange(tasks.data(), sizeof(WorkerTask) * tasks.size());
    DCFlushRange(&tasks, sizeof(tasks));
    OSMemoryBarrier();

    // Release remote cores
    gPatchState = PatchState::MAIN_CORE_PATCHING_DONE;

    // Invalidate Main Core I-Caches
    for (auto &task : tasks) {
        if (task.jumpData) { ICInvalidateRange(task.jumpData, task.jumpDataSize * sizeof(uint32_t)); }
        if (task.jumpToOriginal) { ICInvalidateRange(task.jumpToOriginal, 5 * sizeof(uint32_t)); }
        if (task.realCallFunctionAddressPtr) { ICInvalidateRange(task.realCallFunctionAddressPtr, sizeof(uint32_t)); }
        ICInvalidateRange((void *) task.effectiveAddr, 4);
    }
    asm volatile("sync; isync");

    while (gCoresFlushed < 2) { asm volatile("nop"); }

    OSUninterruptibleSpinLock_Release(&sGlobalSpinLock);
    gPatchState = PatchState::ALL_CORES_PATCHING_DONE;

    delete threadA;
    delete threadB;

    for (auto &pf : validToPatch) {
        pf->isPatched = true;
    }

    return true;
}

// Single Patch function simply wraps the Batch function
bool PatchFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction) {
    std::vector list = {patchedFunction};
    return PatchFunctions(list);
}

bool RestoreFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction) {
    if (OSGetCoreId() != OSGetMainCoreId()) {
        DEBUG_FUNCTION_LINE_INFO("RestoreFunction called from Core %d. Dispatching to Main Core %d...", OSGetCoreId(), OSGetMainCoreId());
        PatchDispatchCtx ctx    = {patchedFunction, false};
        CThread *dispatchThread = CThread::create(RestoreFunctionDispatcher, &ctx, CThread::eAttributeAffCore1, 0);
        delete dispatchThread;
        return ctx.result;
    }

    if (!patchedFunction->isPatched) {
        DEBUG_FUNCTION_LINE_VERBOSE("Skip restoring function because it's not patched");
        return true;
    }
    if (patchedFunction->replacedInstruction == 0 || patchedFunction->realEffectiveFunctionAddress == 0) {
        DEBUG_FUNCTION_LINE_ERR("Failed to restore function, information is missing.");
        return false;
    }

    std::lock_guard lock(sPatch_RestoreMutex);

    auto targetAddrPhys = (uint32_t) patchedFunction->realPhysicalFunctionAddress;

    if (patchedFunction->library != LIBRARY_OTHER) {
        targetAddrPhys = (uint32_t) OSEffectiveToPhysical(patchedFunction->realEffectiveFunctionAddress);
    }

    // Check if patched instruction is still loaded.
    uint32_t currentInstruction;
    if (!ReadFromPhysicalAddress(patchedFunction->realPhysicalFunctionAddress, &currentInstruction)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to read instruction.");
        return false;
    }

    if (currentInstruction != patchedFunction->replaceWithInstruction) {
        DEBUG_FUNCTION_LINE_WARN("Instruction is different than expected. Skip restoring. Expected: %08X Real: %08X", currentInstruction, patchedFunction->replaceWithInstruction);
        return false;
    }

    DEBUG_FUNCTION_LINE_VERBOSE("Restoring %08X to %08X [%08X]", (uint32_t) patchedFunction->replacedInstruction, patchedFunction->realEffectiveFunctionAddress, targetAddrPhys);
    auto sourceAddr = (uint32_t) &patchedFunction->replacedInstruction;

    auto sourceAddrPhys = (uint32_t) OSEffectiveToPhysical(sourceAddr);

    // These hardcoded values should be replaced with something more dynamic.
    if (sourceAddrPhys == 0 && (sourceAddr >= 0x00800000 && sourceAddr < 0x01000000)) {
        sourceAddrPhys = sourceAddr + (0x30800000 - 0x00800000);
    }

    if (sourceAddrPhys == 0) {
        OSFatal("FunctionPatcherModule: Failed to get physical address");
    }

    // Map Restore into the exact same Task structure for the generic worker
    std::vector<WorkerTask> tasks;
    WorkerTask task    = {};
    task.targetPhys    = targetAddrPhys;
    task.sourcePhys    = sourceAddrPhys;
    task.effectiveAddr = patchedFunction->realEffectiveFunctionAddress;
    tasks.push_back(task);

    DCFlushRange(tasks.data(), sizeof(task) * tasks.size());
    DCFlushRange(&tasks, sizeof(tasks));
    OSMemoryBarrier();

    if (!sSpinLockInitialized) {
        OSInitSpinLock(&sGlobalSpinLock);
        sSpinLockInitialized = true;
    }

    gPatchState   = PatchState::PREPARE;
    gCoresReady   = 0;
    gCoresFlushed = 0;

    CThread *threadA = CThread::create(applyKernelPatchOnCore, &tasks, CThread::eAttributeAffCore2, 0, 0x1000);
    CThread *threadB = CThread::create(applyKernelPatchOnCore, &tasks, CThread::eAttributeAffCore0, 0, 0x1000);
    threadA->resumeThread();
    threadB->resumeThread();

    while (gCoresReady < 2) {
        OSSleepTicks(1);
    }

    DEBUG_FUNCTION_LINE_VERBOSE("Restore on thread for %08X on Core %d", patchedFunction->realPhysicalFunctionAddress, OSGetCoreId());

    OSUninterruptibleSpinLock_Acquire(&sGlobalSpinLock);

    DCFlushRange(tasks.data(), tasks.size() * sizeof(WorkerTask));
    KernelCopyData(task.targetPhys, task.sourcePhys, 4);
    OSMemoryBarrier();

    DCFlushRange((void *) task.effectiveAddr, 4);

    gPatchState = PatchState::MAIN_CORE_PATCHING_DONE;

    ICInvalidateRange((void *) task.effectiveAddr, 4);
    asm volatile("sync; isync");

    while (gCoresFlushed < 2) { asm volatile("nop");}

    OSUninterruptibleSpinLock_Release(&sGlobalSpinLock);
    gPatchState = PatchState::ALL_CORES_PATCHING_DONE;

    delete threadA;
    delete threadB;

    patchedFunction->isPatched = false;
    return true;
}
