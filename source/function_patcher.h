#pragma once

#include "PatchedFunctionData.h"
#include <memory>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

bool PatchFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction);
bool RestoreFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction);

bool PatchFunctions(std::vector<std::shared_ptr<PatchedFunctionData>> &patchedFunctions);

#ifdef __cplusplus
}
#endif