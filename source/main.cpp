#include <wums.h>
#include <whb/log_udp.h>
#include "function_patcher.h"
WUMS_MODULE_EXPORT_NAME("homebrew_functionpatcher");

WUMS_INITIALIZE(){
    WHBLogUdpInit();
}

WUMS_APPLICATION_STARTS() {
    FunctionPatcherResetLibHandles();
}

WUMS_EXPORT_FUNCTION(FunctionPatcherPatchFunction);
