#pragma once
#include "defines.h"
#include <coreinit/dynload.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rpl_handling {
    function_replacement_library_type_t library;
    const char rplname[15];
    OSDynLoad_Module handle;
} rpl_handling;


void FunctionPatcherPatchFunction(function_replacement_data_t *replacements, uint32_t size);

void FunctionPatcherResetLibHandles();

uint32_t getAddressOfFunction(char * functionName, function_replacement_library_type_t type);

bool isDynamicFunction(uint32_t physicalAddress);

#ifdef __cplusplus
}
#endif