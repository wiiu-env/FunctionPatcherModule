#include "FunctionAddressProvider.h"
#include "utils/logger.h"
#include <coreinit/dynload.h>
#include <function_patcher/fpatching_defines.h>

uint32_t FunctionAddressProvider::getEffectiveAddressOfFunction(function_replacement_library_type_t library, const char *functionName) {
    uint32_t real_addr          = 0;
    OSDynLoad_Module rpl_handle = nullptr;
    OSDynLoad_Error err         = OS_DYNLOAD_OK;
    int32_t rpl_handles_size    = sizeof rpl_handles / sizeof rpl_handles[0];

    for (int32_t i = 0; i < rpl_handles_size; i++) {
        if (rpl_handles[i].library == library) {
            if (rpl_handles[i].handle == nullptr) {
                DEBUG_FUNCTION_LINE_VERBOSE("Lets acquire handle for rpl: %s", rpl_handles[i].rplname);
                err = OSDynLoad_IsModuleLoaded((char *) rpl_handles[i].rplname, &rpl_handles[i].handle);
            }
            if (err != OS_DYNLOAD_OK || !rpl_handles[i].handle) {
                DEBUG_FUNCTION_LINE_VERBOSE("%s is not loaded yet", rpl_handles[i].rplname, err, rpl_handles[i].handle);
                return 0;
            }
            rpl_handle = rpl_handles[i].handle;
            break;
        }
    }

    if (!rpl_handle) {
        DEBUG_FUNCTION_LINE_ERR("Failed to find the RPL handle for %s", functionName);
        return 0;
    }

    OSDynLoad_FindExport(rpl_handle, 0, functionName, reinterpret_cast<void **>(&real_addr));

    if (!real_addr) {
        DEBUG_FUNCTION_LINE_VERBOSE("OSDynLoad_FindExport failed for %s", functionName);
        return 0;
    }

    if ((library == LIBRARY_NN_ACP) && (uint32_t) (*(volatile uint32_t *) (real_addr) &0x48000002) == 0x48000000) {
        auto address_diff = (uint32_t) (*(volatile uint32_t *) (real_addr) &0x03FFFFFC);
        if ((address_diff & 0x03000000) == 0x03000000) {
            address_diff |= 0xFC000000;
        }
        real_addr += (int32_t) address_diff;
        if ((uint32_t) (*(volatile uint32_t *) (real_addr) &0x48000002) == 0x48000000) {
            DEBUG_FUNCTION_LINE_ERR("Error");
            return 0;
        }
    }

    return real_addr;
}

void FunctionAddressProvider::resetHandles() {
    int32_t rpl_handles_size = sizeof rpl_handles / sizeof rpl_handles[0];

    for (int32_t i = 0; i < rpl_handles_size; i++) {
        if (rpl_handles[i].handle != nullptr) {
            DEBUG_FUNCTION_LINE_VERBOSE("Resetting handle for rpl: %s", rpl_handles[i].rplname);
            OSDynLoad_Release(rpl_handles[i].handle);
        }

        rpl_handles[i].handle = nullptr;
    }
}
