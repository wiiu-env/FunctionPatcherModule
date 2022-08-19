#include "FunctionAddressProvider.h"
#include "export.h"
#include "function_patcher.h"
#include "utils/globals.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <wums.h>

WUMS_MODULE_EXPORT_NAME("homebrew_functionpatcher");
WUMS_MODULE_INIT_BEFORE_RELOCATION_DONE_HOOK();

void UpdateFunctionPointer() {
    // We need the real MEMAllocFromDefaultHeapEx/MEMFreeToDefaultHeap function pointer to force-allocate memory on the default heap.
    // Our custom heap doesn't work (yet) for threads and causes an app panic.
    OSDynLoad_Module coreinitModule;
    if (OSDynLoad_Acquire("coreinit", &coreinitModule) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("Failed to acquire coreinit.rpl");
        OSFatal("Failed to acquire coreinit.rpl");
    }
    /* Memory allocation functions */
    uint32_t *allocPtr, *freePtr;
    /* Memory allocation functions */
    if (OSDynLoad_FindExport(coreinitModule, true, "MEMAllocFromDefaultHeapEx", reinterpret_cast<void **>(&allocPtr)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("OSDynLoad_FindExport for MEMAllocFromDefaultHeapEx");
        OSFatal("OSDynLoad_FindExport for MEMAllocFromDefaultHeapEx");
    }
    if (OSDynLoad_FindExport(coreinitModule, true, "MEMFreeToDefaultHeap", reinterpret_cast<void **>(&freePtr)) != OS_DYNLOAD_OK) {
        DEBUG_FUNCTION_LINE_ERR("OSDynLoad_FindExport for MEMFreeToDefaultHeap");
        OSFatal("OSDynLoad_FindExport for MEMFreeToDefaultHeap");
    }

    gMEMAllocFromDefaultHeapExForThreads = (void *(*) (uint32_t, int) ) * allocPtr;
    gMEMFreeToDefaultHeapForThreads      = (void (*)(void *)) * freePtr;

    OSDynLoad_Release(coreinitModule);
}

uint32_t gDoFunctionResets;

WUMS_INITIALIZE() {
    UpdateFunctionPointer();

    // don't reset the patch status on the first launch.
    gDoFunctionResets = false;

    memset(gJumpHeapData, 0, JUMP_HEAP_DATA_SIZE);
    gJumpHeapHandle = MEMCreateExpHeapEx((void *) (gJumpHeapData), JUMP_HEAP_DATA_SIZE, 1);
    if (gJumpHeapHandle == nullptr) {
        DEBUG_FUNCTION_LINE_ERR("Failed to create heap for jump data");
        OSFatal("Failed to create heap for jump data");
    }

    gFunctionAddressProvider = make_shared_nothrow<FunctionAddressProvider>();
    if (!gFunctionAddressProvider) {
        DEBUG_FUNCTION_LINE_ERR("Failed to create gFunctionAddressProvider");
        OSFatal("Failed to create gFunctionAddressProvider");
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
    }
}

WUMS_APPLICATION_STARTS() {
    uint32_t upid = OSGetUPID();
    if (upid != 2 && upid != 15) {
        return;
    }

    // Now we can update the pointer with the "real" functions
    gMEMAllocFromDefaultHeapExForThreads = MEMAllocFromDefaultHeapEx;
    gMEMFreeToDefaultHeapForThreads      = MEMFreeToDefaultHeap;

    initLogging();

    std::lock_guard<std::mutex> lock(gPatchedFunctionsMutex);

    // Avoid resetting the patch status of function on the first start.
    // WUMS_INITIALIZE & WUMS_APPLICATION_STARTS are called during the same application => the .rpl won't get reloaded.
    // If the .rpl won't get reloaded, old patches will still be present. This can be an issue if a module patches a
    // dynamic function in WUMS_INITIALIZE, which is called right before the first time this function will be called.
    // This reset code would mark it as unpatched, while the code is actually still patched, leading to patching an
    // already patched function.
    // To avoid this issues, the need to skip the reset status part the first time.
    if (gDoFunctionResets) {
        DEBUG_FUNCTION_LINE_VERBOSE("Reset patch status");
        // Reset all dynamic functions
        for (auto &cur : gPatchedFunctions) {
            if (cur->isDynamicFunction()) {
                if (cur->functionName) {
                    DEBUG_FUNCTION_LINE_VERBOSE("%s is dynamic, reset patched status", cur->functionName->c_str());
                } else {
                    DEBUG_FUNCTION_LINE_VERBOSE("is dynamic, reset patched status");
                }
                cur->isPatched = false;
            } else {
                if (cur->functionName) {
                    DEBUG_FUNCTION_LINE_VERBOSE("Skip %s for targetProcess %d", cur->functionName->c_str(), cur->targetProcess);
                } else {
                    DEBUG_FUNCTION_LINE_VERBOSE("Skip %08X for targetProcess %d", cur->realEffectiveFunctionAddress, cur->targetProcess);
                }
            }
        }
    }
    gDoFunctionResets = true;

    OSMemoryBarrier();

    DEBUG_FUNCTION_LINE_VERBOSE("Patch all functions");
    for (auto &cur : gPatchedFunctions) {
        PatchFunction(cur);
    }

    OSDynLoad_AddNotifyCallback(notify_callback, nullptr);
}

WUMS_APPLICATION_REQUESTS_EXIT() {
    deinitLogging();
}
WUMS_APPLICATION_ENDS() {
    gFunctionAddressProvider->resetHandles();
}

WUMS_EXPORT_FUNCTION(FunctionPatcherPatchFunction);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreFunction);
