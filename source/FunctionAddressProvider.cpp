#include "FunctionAddressProvider.h"
#include "utils/logger.h"
#include <coreinit/dynload.h>
#include <function_patcher/fpatching_defines.h>

uint32_t FunctionAddressProvider::getEffectiveAddressOfFunction(function_replacement_library_type_t library, const char *functionName) {
    uint32_t real_addr          = 0;
    OSDynLoad_Module rpl_handle = nullptr;
    OSDynLoad_Error err         = OS_DYNLOAD_OK;

    for (auto &rplHandle : rpl_handles) {
        if (rplHandle.library == library) {
            if (rplHandle.handle == nullptr) {
                DEBUG_FUNCTION_LINE_VERBOSE("Lets check if rpl is loaded: %s", rplHandle.rplname);
                err = OSDynLoad_IsModuleLoaded((char *) rplHandle.rplname, &rplHandle.handle);
            }
            if (err != OS_DYNLOAD_OK || !rplHandle.handle) {
                DEBUG_FUNCTION_LINE_VERBOSE("%s is not loaded yet", rplHandle.rplname, err, rplHandle.handle);
                return 0;
            }
            rpl_handle = rplHandle.handle;
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

    uint32_t realAddrData = *((volatile uint32_t *) real_addr);

    if ((realAddrData & 0xFC000003) == 0x48000000) {
        auto address_diff = (uint32_t) (realAddrData & 0x01FFFFFC);
        if ((realAddrData & 0x02000000) == 0x02000000) {
            address_diff = 0xFE000000 + address_diff;
        }
        real_addr += (int32_t) address_diff;
    }

    return real_addr;
}

void FunctionAddressProvider::resetHandles() {
    for (auto &rplHandle : rpl_handles) {
        if (rplHandle.handle != nullptr) {
            DEBUG_FUNCTION_LINE_VERBOSE("Resetting handle for rpl: %s", rplHandle.rplname);
        }

        rplHandle.handle = nullptr;
    }
}

function_replacement_library_type_t FunctionAddressProvider::getTypeForHandle(OSDynLoad_Module handle) {
    for (auto &rplHandle : rpl_handles) {
        if (rplHandle.handle == handle) {
            return rplHandle.library;
        }
    }
    return LIBRARY_OTHER;
}

bool FunctionAddressProvider::resetHandle(OSDynLoad_Module handle) {
    for (auto &rplHandle : rpl_handles) {
        if (rplHandle.handle == handle) {
            rplHandle.handle = nullptr;
            return true;
        }
    }
    return false;
}
