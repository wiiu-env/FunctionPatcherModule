#pragma once
#include <function_patcher/fpatching_defines.h>

bool FunctionPatcherPatchFunction(function_replacement_data_t *function_data, PatchedFunctionHandle *outHandle);

bool FunctionPatcherRestoreFunction(PatchedFunctionHandle handle);