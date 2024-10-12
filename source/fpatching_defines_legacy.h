#pragma once

/* Types that are kept only for ABI compatibility. */

#include <stdint.h>
#include <function_patcher/fpatching_defines.h>

typedef struct function_replacement_data_v2_t {
    uint32_t VERSION;
    uint32_t physicalAddr;                       /* [needs to be filled]  */
    uint32_t virtualAddr;                        /* [needs to be filled]  */
    uint32_t replaceAddr;                        /* [needs to be filled] Address of our replacement function */
    uint32_t *replaceCall;                       /* [needs to be filled] Address to access the real_function */
    function_replacement_library_type_t library; /* [needs to be filled] rpl where the function we want to replace is. */
    const char *function_name;                   /* [needs to be filled] name of the function we want to replace */
    FunctionPatcherTargetProcess targetProcess;  /* [will be filled] */
} function_replacement_data_v2_t;
