#include "export.h"
#include "PatchedFunctionData.h"
#include "function_patcher.h"
#include "utils/globals.h"
#include <ranges>
#include <vector>

bool FunctionPatcherPatchFunction(function_replacement_data_t *function_data, PatchedFunctionHandle *outHandle) {
    if (function_data->VERSION != FUNCTION_REPLACEMENT_DATA_STRUCT_VERSION) {
        DEBUG_FUNCTION_LINE_ERR("Failed to patch function. struct version mismatch");
        return false;
    }

    auto functionDataOpt = PatchedFunctionData::make_shared(gFunctionAddressProvider, function_data, gJumpHeapHandle);

    if (!functionDataOpt) {
        return false;
    }

    auto functionData = functionDataOpt.value();

    std::lock_guard<std::mutex> lock(gPatchedFunctionsMutex);

    if (!PatchFunction(functionData)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to patch function");
        return false;
    }

    if (outHandle) {
        *outHandle = functionData->getHandle();
    }

    gPatchedFunctions.push_back(std::move(functionData));

    OSMemoryBarrier();

    return true;
}

bool FunctionPatcherRestoreFunction(PatchedFunctionHandle handle) {
    std::lock_guard<std::mutex> lock(gPatchedFunctionsMutex);
    std::vector<std::shared_ptr<PatchedFunctionData>> toBeTempRestored;
    bool found            = false;
    int32_t erasePosition = 0;
    std::shared_ptr<PatchedFunctionData> toRemoved;
    for (auto &cur : gPatchedFunctions) {
        if (cur->getHandle() == handle) {
            toRemoved = cur;
            found     = true;
            continue;
        }
        // Check if something else patched the same function afterwards.
        if (found) {
            if (cur->realPhysicalFunctionAddress == toRemoved->realPhysicalFunctionAddress) {
                toBeTempRestored.push_back(cur);
            }
        } else {
            erasePosition++;
        }
    }
    if (!found) {
        DEBUG_FUNCTION_LINE_ERR("Failed to find PatchedFunctionData by handle %08X", handle);
        return false;
    }

    // Restore function patches that were done after the patch we actually want to restore.
    for (auto &cur : std::ranges::reverse_view(toBeTempRestored)) {
        RestoreFunction(cur);
    }

    // Restore the function we actually want to restore
    RestoreFunction(toRemoved);

    gPatchedFunctions.erase(gPatchedFunctions.begin() + erasePosition);

    // Apply the other patches again
    for (auto &cur : toBeTempRestored) {
        PatchFunction(cur);
    }

    OSMemoryBarrier();
    return true;
}