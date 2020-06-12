#include <wums.h>
#include <whb/log_udp.h>
#include "function_patcher.h"
WUMS_MODULE_EXPORT_NAME("homebrew_functionpatcher");
WUMS_MODULE_INIT_BEFORE_ENTRYPOINT();

WUMS_INITIALIZE(){
    WHBLogUdpInit();
}

WUMS_APPLICATION_STARTS() {
    FunctionPatcherResetLibHandles();
}

WUMS_EXPORT_FUNCTION(FunctionPatcherPatchFunction);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreFunctions);
