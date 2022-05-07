#include <wums.h>

#include "function_patcher.h"
#include "utils/logger.h"

WUMS_MODULE_EXPORT_NAME("homebrew_functionpatcher");
WUMS_MODULE_SKIP_INIT_FINI();


WUMS_APPLICATION_STARTS() {
    initLogging();
    FunctionPatcherResetLibHandles();
}

WUMS_APPLICATION_REQUESTS_EXIT() {
    deinitLogging();
}

WUMS_EXPORT_FUNCTION(FunctionPatcherPatchFunction);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreFunctions);
WUMS_EXPORT_FUNCTION(FunctionPatcherRestoreDynamicFunctions);
