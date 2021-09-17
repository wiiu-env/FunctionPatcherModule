#include <wums.h>
#include <whb/log_udp.h>
#include "function_patcher.h"
WUMS_MODULE_EXPORT_NAME("homebrew_functionpatcher");
WUMS_MODULE_SKIP_ENTRYPOINT();
WUMS_MODULE_INIT_BEFORE_RELOCATION_DONE_HOOK();

WUMS_APPLICATION_STARTS() {
    WHBLogUdpInit();
    FunctionPatcherResetLibHandles();
}

WUMS_EXPORT_FUNCTION(FunctionPatcherPatchFunction);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreFunctions);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreDynamicFunctions);
