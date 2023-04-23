#include "FunctionAddressProvider.h"
#include "export.h"
#include "function_patcher.h"
#include "utils/globals.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <kernel/kernel.h>
#include <ranges>
#include <set>
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_functionpatcher");
WUMS_MODULE_INIT_BEFORE_RELOCATION_DONE_HOOK();
WUMS_DEPENDS_ON(homebrew_kernel);

void UpdateFunctionPointer() {
    // We need the real MEMAllocFromDefaultHeapEx/MEMFreeToDefaultHeap function pointer to force-allocate memory on the default heap.
    // Our custom heap doesn't work (yet) for threads and causes an app panic.
    OSDynLoad_Module coreinitModule;
    if (OSDynLoad_Acquire("coreinit", &coreinitModule) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("Failed to acquire coreinit.rpl");
        OSFatal("FunctionPatcherModule: Failed to acquire coreinit.rpl");
    }
    /* Memory allocation functions */
    uint32_t *allocPtr, *freePtr;
    if (OSDynLoad_FindExport(coreinitModule, OS_DYNLOAD_EXPORT_DATA, "MEMAllocFromDefaultHeapEx", reinterpret_cast<void **>(&allocPtr)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("OSDynLoad_FindExport for MEMAllocFromDefaultHeapEx");
        OSFatal("FunctionPatcherModule: OSDynLoad_FindExport for MEMAllocFromDefaultHeapEx");
    }
    if (OSDynLoad_FindExport(coreinitModule, OS_DYNLOAD_EXPORT_DATA, "MEMFreeToDefaultHeap", reinterpret_cast<void **>(&freePtr)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("OSDynLoad_FindExport for MEMFreeToDefaultHeap");
        OSFatal("FunctionPatcherModule: OSDynLoad_FindExport for MEMFreeToDefaultHeap");
    }

    gMEMAllocFromDefaultHeapExForThreads = (void *(*) (uint32_t, int) ) * allocPtr;
    gMEMFreeToDefaultHeapForThreads      = (void (*)(void *)) * freePtr;

    OSDynLoad_Release(coreinitModule);
}

void CheckIfPatchedFunctionsAreStillInMemory() {
    std::lock_guard<std::mutex> lock(gPatchedFunctionsMutex);
    // Check if rpl has been unloaded by comparing the instruction.
    std::set<uint32_t> physicalAddressesUnchanged;
    std::set<uint32_t> physicalAddressesChanged;
    // Restore function patches that were done after the patch we actually want to restore.
    for (auto &cur : std::ranges::reverse_view(gPatchedFunctions)) {
        if (!cur->isPatched || physicalAddressesUnchanged.contains(cur->realPhysicalFunctionAddress)) {
            continue;
        }
        if (physicalAddressesChanged.contains(cur->realPhysicalFunctionAddress)) {
            cur->isPatched = false;
            continue;
        }

        // Check if patched instruction is still loaded.
        uint32_t currentInstruction;
        if (!ReadFromPhysicalAddress(cur->realPhysicalFunctionAddress, &currentInstruction)) {
            DEBUG_FUNCTION_LINE_ERR("Failed to read instruction.");
            continue;
        }

        if (currentInstruction == cur->replaceWithInstruction) {
            physicalAddressesUnchanged.insert(cur->realPhysicalFunctionAddress);
        } else {
            cur->isPatched = false;
            physicalAddressesChanged.insert(cur->realPhysicalFunctionAddress);
        }
    }
}

bool PatchInstruction(void *instr, uint32_t original, uint32_t replacement) {
    uint32_t current = *(uint32_t *) instr;
    if (current != original) {
        return current == replacement;
    }

    KernelCopyData(OSEffectiveToPhysical((uint32_t) instr), OSEffectiveToPhysical((uint32_t) &replacement), sizeof(replacement));

    DCFlushRange(instr, 4);
    ICInvalidateRange(instr, 4);

    current = *(uint32_t *) instr;

    return true;
}

bool PatchDynLoadFunctions() {
    uint32_t *patch1 = ((uint32_t *) &OSDynLoad_GetNumberOfRPLs) + 6;
    uint32_t *patch2 = ((uint32_t *) &OSDynLoad_GetRPLInfo) + 22;

    if (!PatchInstruction(patch1, 0x41820038 /* beq +38 */, 0x60000000 /*nop*/)) {
        return false;
    }
    if (!PatchInstruction(patch2, 0x41820100 /* beq +100 */, 0x60000000 /*nop*/)) {
        return false;
    }

    return true;
}

WUMS_INITIALIZE() {
    UpdateFunctionPointer();
    initLogging();

    if (!PatchDynLoadFunctions()) {
        DEBUG_FUNCTION_LINE_ERR("Failed to patch OSDynLoad_GetRPLInfo or OSDynLoad_GetNumberOfRPLs");
        OSFatal("Failed to patch OSDynLoad_GetRPLInfo or OSDynLoad_GetNumberOfRPLs");
    }

    memset(gJumpHeapData, 0, JUMP_HEAP_DATA_SIZE);
    gJumpHeapHandle = MEMCreateExpHeapEx((void *) (gJumpHeapData), JUMP_HEAP_DATA_SIZE, 1);
    if (gJumpHeapHandle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("Failed to create heap for jump data");
        OSFatal("FunctionPatcherModule: Failed to create heap for jump data");
    }

    gFunctionAddressProvider = make_shared_nothrow<FunctionAddressProvider>();
    if (!gFunctionAddressProvider) {
        DEBUG_FUNCTION_LINE_ERR("Failed to create gFunctionAddressProvider");
        OSFatal("FunctionPatcherModule: Failed to create gFunctionAddressProvider");
    }
}

void notify_callback(OSDynLoad_Module module,
                     void *userContext,
                     OSDynLoad_NotifyReason reason,
                     OSDynLoad_NotifyData *infos) {
    if (reason == OS_DYNLOAD_NOTIFY_LOADED) {
        std::lock_guard<std::mutex> lock(gPatchedFunctionsMutex);
        for (auto &cur : gPatchedFunctions) {
            PatchFunction(cur);
        }
    } else if (reason == OS_DYNLOAD_NOTIFY_UNLOADED) {
        std::lock_guard<std::mutex> lock(gPatchedFunctionsMutex);
        auto library = gFunctionAddressProvider->getTypeForHandle(module);
        if (library != LIBRARY_OTHER) {
            for (auto &cur : gPatchedFunctions) {
                if (cur->type == FUNCTION_PATCHER_REPLACE_BY_LIB_OR_ADDRESS && cur->library.has_value() && cur->library == library) {
                    cur->isPatched = false;
                }
            }
        }
        gFunctionAddressProvider->resetHandle(module);
        CheckIfPatchedFunctionsAreStillInMemory();
    }
}

WUMS_APPLICATION_STARTS() {
    uint32_t upid = OSGetUPID();
    if (upid != 2 && upid != 15) {
        return;
    }

    OSReport("Running FunctionPatcherModule " MODULE_VERSION_FULL "\n");

    // Now we can update the pointer with the "real" functions
    gMEMAllocFromDefaultHeapExForThreads = MEMAllocFromDefaultHeapEx;
    gMEMFreeToDefaultHeapForThreads      = MEMFreeToDefaultHeap;

    initLogging();
    {
        std::lock_guard<std::mutex> lock(gPatchedFunctionsMutex);
        // reset function patch status if the rpl they were patching has been unloaded from memory.
        CheckIfPatchedFunctionsAreStillInMemory();
        DEBUG_FUNCTION_LINE_VERBOSE("Patch all functions");
        for (auto &cur : gPatchedFunctions) {
            PatchFunction(cur);
        }

        OSMemoryBarrier();
        OSDynLoad_AddNotifyCallback(notify_callback, nullptr);
    }
}

WUMS_APPLICATION_REQUESTS_EXIT() {
    deinitLogging();
}
WUMS_APPLICATION_ENDS() {
    gFunctionAddressProvider->resetHandles();
}

WUMS_EXPORT_FUNCTION(FunctionPatcherPatchFunction);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreFunction);