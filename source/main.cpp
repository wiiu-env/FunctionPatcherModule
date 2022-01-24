#include <wums.h>
#ifdef DEBUG
#include <whb/log_udp.h>
#include <whb/log_cafe.h>
#include <whb/log_module.h>
#endif // DEBUG

#include "function_patcher.h"

WUMS_MODULE_EXPORT_NAME("homebrew_functionpatcher");
WUMS_MODULE_SKIP_INIT_FINI();

#ifdef DEBUG
uint32_t moduleLogInit = false;
uint32_t cafeLogInit = false;
uint32_t udpLogInit = false;
#endif

WUMS_APPLICATION_STARTS() {
#ifdef DEBUG
    if (!(moduleLogInit = WHBLogModuleInit())) {
        cafeLogInit = WHBLogCafeInit();
        udpLogInit = WHBLogUdpInit();
    }
#endif // DEBUG
    FunctionPatcherResetLibHandles();
}

WUMS_APPLICATION_REQUESTS_EXIT() {
#ifdef DEBUG
    if (moduleLogInit) {
        WHBLogModuleDeinit();
        moduleLogInit = false;
    }
    if (cafeLogInit) {
        WHBLogCafeDeinit();
        cafeLogInit = false;
    }
    if (udpLogInit) {
        WHBLogUdpDeinit();
        udpLogInit = false;
    }
#endif // DEBUG
}

WUMS_EXPORT_FUNCTION(FunctionPatcherPatchFunction);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreFunctions);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreDynamicFunctions);
