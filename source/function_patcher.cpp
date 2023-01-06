#include "function_patcher.h"
#include "FunctionAddressProvider.h"
#include "PatchedFunctionData.h"
#include "utils/CThread.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/memorymap.h>
#include <kernel/kernel.h>
#include <memory>

static void writeDataAndFlushIC(CThread *thread, void *arg) {
    auto *data = (PatchedFunctionData *) arg;

    uint32_t replace_instruction = data->replaceWithInstruction;
    uint32_t physical_address    = data->realPhysicalFunctionAddress;
    uint32_t effective_address   = data->realEffectiveFunctionAddress;
    DCFlushRange(&replace_instruction, 4);
    DCFlushRange(&physical_address, 4);

    auto replace_instruction_physical = (uint32_t) &replace_instruction;

    if (replace_instruction_physical < 0x00800000 || replace_instruction_physical >= 0x01000000) {
        replace_instruction_physical = OSEffectiveToPhysical(replace_instruction_physical);
    } else {
        replace_instruction_physical = replace_instruction_physical + 0x30800000 - 0x00800000;
    }

    KernelCopyData(physical_address, replace_instruction_physical, 4);
    ICInvalidateRange((void *) (effective_address), 4);
}

bool PatchFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction) {
    if (patchedFunction->isPatched) {
        return true;
    }

    // The addresses of a function might change every time with run another application.
    if (!patchedFunction->updateFunctionAddresses()) {
        return true;
    }

    if (patchedFunction->functionName) {
        DEBUG_FUNCTION_LINE("Patching function %s...", patchedFunction->functionName->c_str());
    } else {
        DEBUG_FUNCTION_LINE("Patching function @ %08X", patchedFunction->realEffectiveFunctionAddress);
    }

    if (!ReadFromPhysicalAddress(patchedFunction->realPhysicalFunctionAddress, &patchedFunction->replacedInstruction)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to read instruction.");
        OSFatal("FunctionPatcherModule: Failed to read instruction.");
        return false;
    }

    // Generate a jump to the original function so the unpatched function can still be called
    patchedFunction->generateJumpToOriginal();

    // Generate a code that is run when somebody calls the patched function.
    // If the correct process calls this, it'll jump the function replacement, otherwise the original function will be called.
    patchedFunction->generateReplacementJump();

    // Write this->replaceWithInstruction to the first instruction of the function we want to replace.
    CThread::runOnAllCores(writeDataAndFlushIC, patchedFunction.get());

    // Set patch status
    patchedFunction->isPatched = true;

    return true;
}

bool RestoreFunction(std::shared_ptr<PatchedFunctionData> &patchedFunction) {
    if (!patchedFunction->isPatched) {
        DEBUG_FUNCTION_LINE_VERBOSE("Skip restoring function because it's not patched");
        return true;
    }
    if (patchedFunction->replacedInstruction == 0 || patchedFunction->realEffectiveFunctionAddress == 0) {
        DEBUG_FUNCTION_LINE_ERR("Failed to restore function, information is missing.");
        return false;
    }

    auto targetAddrPhys = (uint32_t) patchedFunction->realPhysicalFunctionAddress;

    if (patchedFunction->library != LIBRARY_OTHER) {
        targetAddrPhys = (uint32_t) OSEffectiveToPhysical(patchedFunction->realEffectiveFunctionAddress);
    }

    // Check if patched instruction is still loaded.
    uint32_t currentInstruction;
    if (!ReadFromPhysicalAddress(patchedFunction->realPhysicalFunctionAddress, &currentInstruction)) {
        DEBUG_FUNCTION_LINE_ERR("Failed to read instruction.");
        return false;
    }

    if (currentInstruction != patchedFunction->replaceWithInstruction) {
        DEBUG_FUNCTION_LINE_WARN("Instruction is different than expected. Skip restoring. Expected: %08X Real: %08X", currentInstruction, patchedFunction->replaceWithInstruction);
        return false;
    }

    DEBUG_FUNCTION_LINE_VERBOSE("Restoring %08X to %08X [%08X]", (uint32_t) patchedFunction->replacedInstruction, patchedFunction->realEffectiveFunctionAddress, targetAddrPhys);
    auto sourceAddr = (uint32_t) &patchedFunction->replacedInstruction;

    auto sourceAddrPhys = (uint32_t) OSEffectiveToPhysical(sourceAddr);

    // These hardcoded values should be replaced with something more dynamic.
    if (sourceAddrPhys == 0 && (sourceAddr >= 0x00800000 && sourceAddr < 0x01000000)) {
        sourceAddrPhys = sourceAddr + (0x30800000 - 0x00800000);
    }

    if (sourceAddrPhys == 0) {
        OSFatal("FunctionPatcherModule: Failed to get physical address");
    }

    KernelCopyData(targetAddrPhys, sourceAddrPhys, 4);
    ICInvalidateRange((void *) patchedFunction->realEffectiveFunctionAddress, 4);
    DCFlushRange((void *) patchedFunction->realEffectiveFunctionAddress, 4);

    patchedFunction->isPatched = false;
    return true;
}
