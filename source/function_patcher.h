#pragma once

#include "PatchedFunctionData.h"
#include <coreinit/dynload.h>
#include <function_patcher/fpatching_defines.h>
#include <memory>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

bool PatchFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction);
bool RestoreFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction);

#ifdef __cplusplus
}
#endif