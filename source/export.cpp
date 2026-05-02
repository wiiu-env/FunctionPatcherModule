#include "export.h"
#include "PatchedFunctionData.h"
#include "function_patcher.h"
#include "utils/globals.h"

#include <mutex>
#include <ranges>
#include <vector>

#include <wums/exports.h>

WUT_CHECK_OFFSET(function_replacement_data_v2_t, 0x00, VERSION);
WUT_CHECK_OFFSET(function_replacement_data_v3_t, 0x00, version);

FunctionPatcherStatus FPAddFunctionPatches(function_replacement_data_t **function_data_array, uint32_t count, PatchedFunctionHandle *outHandles, bool *outHasBeenPatchedArray);

FunctionPatcherStatus FPAddFunctionPatch(function_replacement_data_t *function_data, PatchedFunctionHandle *outHandle, bool *outHasBeenPatched) {
    // Wrap the single patch into an array of size 1 and pass it to the batcher
    function_replacement_data_t *arr[] = {function_data};
    return FPAddFunctionPatches(arr, 1, outHandle, outHasBeenPatched);
}

FunctionPatcherStatus FPAddFunctionPatches(function_replacement_data_t **function_data_array, uint32_t count, PatchedFunctionHandle *outHandles, bool *outHasBeenPatchedArray) {
    if (function_data_array == nullptr || count == 0) {
        DEBUG_FUNCTION_LINE_ERR("function_data_array was NULL or count was 0");
        return FUNCTION_PATCHER_RESULT_INVALID_ARGUMENT;
    }

    DEBUG_FUNCTION_LINE_ERR("Patching %d functions", count);

    std::vector<std::shared_ptr<PatchedFunctionData>> functionsToPatch;
    functionsToPatch.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        auto *function_data = function_data_array[i];
        if (function_data == nullptr) {
            DEBUG_FUNCTION_LINE_ERR("A function_data entry was NULL");
            return FUNCTION_PATCHER_RESULT_INVALID_ARGUMENT;
        }

        if (function_data->version < 2 || function_data->version > 3) {
            DEBUG_FUNCTION_LINE_ERR("Failed to patch function. struct version mismatch");
            return FUNCTION_PATCHER_RESULT_UNSUPPORTED_STRUCT_VERSION;
        }

        std::optional<std::shared_ptr<PatchedFunctionData>> functionDataOpt;
        if (function_data->version == 2) {
            functionDataOpt = PatchedFunctionData::make_shared_v2(gFunctionAddressProvider, (function_replacement_data_v2_t *) function_data, gJumpHeapHandle);
        } else if (function_data->version == 3) {
            functionDataOpt = PatchedFunctionData::make_shared_v3(gFunctionAddressProvider, (function_replacement_data_v3_t *) function_data, gJumpHeapHandle);
        } else {
            DEBUG_FUNCTION_LINE_ERR("Unknown function_replacement_data_t struct version");
            OSFatal("Unknown function patching struct version. Update FunctionPatcherModule/Aroma.");
        }

        if (!functionDataOpt) {
            return FUNCTION_PATCHER_RESULT_UNKNOWN_ERROR;
        }

        functionsToPatch.push_back(functionDataOpt.value());
    }

    PatchFunctions(functionsToPatch);

    {
        std::lock_guard lock(gPatchedFunctionsMutex);
        for (uint32_t i = 0; i < count; ++i) {
            auto &funcData = functionsToPatch[i];

            if (outHasBeenPatchedArray) {
                outHasBeenPatchedArray[i] = funcData->isPatched;
            }
            if (outHandles) {
                outHandles[i] = funcData->getHandle();
            }

            gPatchedFunctions.push_back(std::move(funcData));
        }
        OSMemoryBarrier();
    }

    return FUNCTION_PATCHER_RESULT_SUCCESS;
}

bool FunctionPatcherPatchFunction(function_replacement_data_t *function_data, PatchedFunctionHandle *outHandle) {
    return FPAddFunctionPatch(function_data, outHandle, nullptr) == FUNCTION_PATCHER_RESULT_SUCCESS;
}

FunctionPatcherStatus FPRemoveFunctionPatch(PatchedFunctionHandle handle) {
    std::lock_guard lock(gPatchedFunctionsMutex);
    std::vector<std::shared_ptr<PatchedFunctionData>> toBeTempRestored;
    bool found            = false;
    int32_t erasePosition = 0;
    std::shared_ptr<PatchedFunctionData> toBeRemoved;
    for (auto &cur : gPatchedFunctions) {
        if (cur->getHandle() == handle) {
            toBeRemoved = cur;
            found       = true;
            if (!cur->isPatched) {
                // Early return if the function is not patched.
                break;
            }
            continue;
        }
        // Check if something else patched the same function afterwards.
        if (found) {
            if (cur->realPhysicalFunctionAddress == toBeRemoved->realPhysicalFunctionAddress) {
                toBeTempRestored.push_back(cur);
            }
        } else {
            erasePosition++;
        }
    }
    if (!found) {
        DEBUG_FUNCTION_LINE_ERR("Failed to find PatchedFunctionData by handle %08X", handle);
        return FUNCTION_PATCHER_RESULT_PATCH_NOT_FOUND;
    }

    if (toBeRemoved->isPatched) {
        // Restore function patches that were done after the patch we actually want to restore.
        for (auto &cur : std::ranges::reverse_view(toBeTempRestored)) {
            RestoreFunction(cur);
        }

        // Restore the function we actually want to restore
        RestoreFunction(toBeRemoved);
    }

    gPatchedFunctions.erase(gPatchedFunctions.begin() + erasePosition);

    if (toBeRemoved->isPatched) {
        // Apply the other patches again
        for (auto &cur : toBeTempRestored) {
            PatchFunction(cur);
        }
    }

    OSMemoryBarrier();
    return FUNCTION_PATCHER_RESULT_SUCCESS;
}

bool FunctionPatcherRestoreFunction(PatchedFunctionHandle handle) {
    return FPRemoveFunctionPatch(handle) == FUNCTION_PATCHER_RESULT_SUCCESS;
}

FunctionPatcherStatus FPGetVersion(FunctionPatcherAPIVersion *outVersion) {
    if (outVersion == nullptr) {
        return FUNCTION_PATCHER_RESULT_INVALID_ARGUMENT;
    }
    *outVersion = 3;
    return FUNCTION_PATCHER_RESULT_SUCCESS;
}

FunctionPatcherStatus FPIsFunctionPatched(PatchedFunctionHandle handle, bool *outIsFunctionPatched) {
    if (outIsFunctionPatched == nullptr) {
        return FUNCTION_PATCHER_RESULT_INVALID_ARGUMENT;
    }
    std::lock_guard lock(gPatchedFunctionsMutex);
    for (auto &cur : gPatchedFunctions) {
        if (cur->getHandle() == handle) {
            *outIsFunctionPatched = cur->isPatched;
            return FUNCTION_PATCHER_RESULT_SUCCESS;
        }
    }
    return FUNCTION_PATCHER_RESULT_PATCH_NOT_FOUND;
}

WUMS_EXPORT_FUNCTION(FPGetVersion);
WUMS_EXPORT_FUNCTION(FPAddFunctionPatch);
WUMS_EXPORT_FUNCTION(FPAddFunctionPatches);
WUMS_EXPORT_FUNCTION(FPRemoveFunctionPatch);
WUMS_EXPORT_FUNCTION(FPIsFunctionPatched);