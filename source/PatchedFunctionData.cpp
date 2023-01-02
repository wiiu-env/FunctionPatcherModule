#include "PatchedFunctionData.h"
#include "utils/utils.h"

std::optional<std::shared_ptr<PatchedFunctionData>> PatchedFunctionData::make_shared(std::shared_ptr<FunctionAddressProvider> functionAddressProvider,
                                                                                     function_replacement_data_t *replacementData,
                                                                                     MEMHeapHandle heapHandle) {
    if (!replacementData) {
        return {};
    }

    auto ptr = make_shared_nothrow<PatchedFunctionData>(std::move(functionAddressProvider));
    if (!ptr) {
        return {};
    }

    ptr->isPatched                  = false;
    ptr->heapHandle                 = heapHandle;
    ptr->library                    = replacementData->library;
    ptr->targetProcess              = replacementData->targetProcess;
    ptr->replacementFunctionAddress = replacementData->replaceAddr;
    ptr->realCallFunctionAddressPtr = replacementData->replaceCall;

    if (replacementData->library != LIBRARY_OTHER) {
        ptr->functionName = replacementData->function_name;
    } else {
        ptr->realEffectiveFunctionAddress = replacementData->virtualAddr;
        ptr->realPhysicalFunctionAddress  = replacementData->physicalAddr;
    }

    ptr->jumpToOriginal = (uint32_t *) MEMAllocFromExpHeapEx(ptr->heapHandle, 0x5 * sizeof(uint32_t), 4);

    if (ptr->replacementFunctionAddress > 0x01FFFFFC || ptr->targetProcess != FP_TARGET_PROCESS_ALL) {
        ptr->jumpDataSize = 15; // We could predict the actual size and save some memory, but at the moment we don't need it.
        ptr->jumpData     = (uint32_t *) MEMAllocFromExpHeapEx(ptr->heapHandle, ptr->jumpDataSize * sizeof(uint32_t), 4);

        if (!ptr->jumpData) {
            DEBUG_FUNCTION_LINE_ERR("Failed to alloc jump data");
            return {};
        }
    }

    if (!ptr->jumpToOriginal) {
        DEBUG_FUNCTION_LINE_ERR("Failed to alloc jump data");
        return {};
    }

    return ptr;
}

bool PatchedFunctionData::updateFunctionAddresses() {
    if (this->library == LIBRARY_OTHER) {
        return true;
    }

    if (!this->functionName) {
        DEBUG_FUNCTION_LINE_ERR("Function name was empty. This should never happen.");
        OSFatal("function name was empty");
        return false;
    }

    auto real_address = functionAddressProvider->getEffectiveAddressOfFunction(library, this->functionName->c_str());
    if (!real_address) {
        DEBUG_FUNCTION_LINE("OSDynLoad_FindExport failed for %s, updating address not possible.", this->functionName->c_str());
        return false;
    }

    this->realEffectiveFunctionAddress = real_address;
    auto physicalFunctionAddress       = (uint32_t) OSEffectiveToPhysical(real_address);
    if (!physicalFunctionAddress) {
        DEBUG_FUNCTION_LINE_ERR("Error. Something is wrong with the physical address");
        return false;
    }
    this->realPhysicalFunctionAddress = physicalFunctionAddress;
    return true;
}

void PatchedFunctionData::generateJumpToOriginal() {
    if (!this->jumpToOriginal) {
        DEBUG_FUNCTION_LINE_ERR("this->jumpToOriginal is not allocated");
        OSFatal("this->jumpToOriginal is not allocated");
    }

    uint32_t jumpToAddress = this->realEffectiveFunctionAddress + 4;

    this->jumpToOriginal[0] = this->replacedInstruction;

    if (((uint32_t) jumpToAddress & 0x01FFFFFC) != (uint32_t) jumpToAddress) {
        // We need to do a long jump
        this->jumpToOriginal[1] = 0x3d600000 | ((jumpToAddress >> 16) & 0x0000FFFF); // lis        r11 ,0x1234
        this->jumpToOriginal[2] = 0x616b0000 | (jumpToAddress & 0x0000ffff);         // ori        r11 ,r11 ,0x5678
        this->jumpToOriginal[3] = 0x7d6903a6;                                        // mtspr      CTR ,r11
        this->jumpToOriginal[4] = 0x4e800420;                                        // bctr
    } else {
        this->jumpToOriginal[1] = 0x48000002 | (jumpToAddress & 0x01FFFFFC);
    }

    DCFlushRange((void *) this->jumpToOriginal, sizeof(uint32_t) * 5);
    ICInvalidateRange((void *) this->jumpToOriginal, sizeof(uint32_t) * 5);

    *(this->realCallFunctionAddressPtr) = (uint32_t) this->jumpToOriginal;
    OSMemoryBarrier();
}

void PatchedFunctionData::generateReplacementJump() {
    //setting jump back
    this->replaceWithInstruction = 0x48000002 | (this->replacementFunctionAddress & 0x01FFFFFC);

    // If the jump is too big, or we want only patch for certain processes we need a trampoline
    if (this->replacementFunctionAddress > 0x01FFFFFC || this->targetProcess != FP_TARGET_PROCESS_ALL) {
        if (!this->jumpData) {
            DEBUG_FUNCTION_LINE_ERR("jumpData was not allocated");
            OSFatal("jumpData was not allocated");
        }
        uint32_t offset = 0;
        if (this->targetProcess != FP_TARGET_PROCESS_ALL) {
            auto originalFunctionAddrWithOffset = this->realEffectiveFunctionAddress + 4;
            bool shortBranchToOriginalPossible  = ((uint32_t) originalFunctionAddrWithOffset & 0x01FFFFFC) == (uint32_t) originalFunctionAddrWithOffset;
            // Only use patched function if OSGetUPID matches function_data->targetProcess
            this->jumpData[offset++] = 0x3d600000 | (((uint32_t *) OSGetUPID)[0] & 0x0000FFFF); // lis        r11 ,0x0
            this->jumpData[offset++] = 0x816b0000 | (((uint32_t *) OSGetUPID)[1] & 0x0000FFFF); // lwz        r11 ,0x0(r11)
            if (this->targetProcess == FP_TARGET_PROCESS_GAME_AND_MENU) {
                this->jumpData[offset++] = 0x2c0b0000 | FP_TARGET_PROCESS_WII_U_MENU;                              // cmpwi      r11 ,FP_TARGET_PROCESS_WII_U_MENU
                this->jumpData[offset++] = 0x41820000 | (shortBranchToOriginalPossible ? 0x00000014 : 0x00000020); // beq        myfunc
                this->jumpData[offset++] = 0x2c0b0000 | FP_TARGET_PROCESS_GAME;                                    // cmpwi      r11 ,FP_TARGET_PROCESS_GAME
                this->jumpData[offset++] = 0x41820000 | (shortBranchToOriginalPossible ? 0x0000000C : 0x00000018); // beq        myfunc
            } else {
                this->jumpData[offset++] = 0x2c0b0000 | this->targetProcess;                                       // cmpwi      r11 ,function_data->targetProcess
                this->jumpData[offset++] = 0x41820000 | (shortBranchToOriginalPossible ? 0x0000000C : 0x00000018); // beq        myfunc
            }

            this->jumpData[offset++] = this->replacedInstruction;
            if (((uint32_t) originalFunctionAddrWithOffset & 0x01FFFFFC) != (uint32_t) originalFunctionAddrWithOffset) {
                this->jumpData[offset++] = 0x3d600000 | (((this->realEffectiveFunctionAddress + 4) >> 16) & 0x0000FFFF); // lis        r11 ,(real_addr + 4)@hi
                this->jumpData[offset++] = 0x616b0000 | ((this->realEffectiveFunctionAddress + 4) & 0x0000ffff);         // ori        r11 ,(real_addr + 4)@lo
                this->jumpData[offset++] = 0x7d6903a6;                                                                   // mtspr      CTR ,r11
                this->jumpData[offset++] = 0x4e800420;                                                                   // bctr
            } else {
                this->jumpData[offset++] = 0x48000002 | (originalFunctionAddrWithOffset & 0x01FFFFFC);
            }
        }
        // myfunc:
        if (((uint32_t) this->replacementFunctionAddress & 0x01FFFFFC) != (uint32_t) this->replacementFunctionAddress) {
            this->jumpData[offset++] = 0x3d600000 | (((this->replacementFunctionAddress) >> 16) & 0x0000FFFF); // lis        r11 ,repl_addr@hi
            this->jumpData[offset++] = 0x616b0000 | ((this->replacementFunctionAddress) & 0x0000ffff);         // ori        r11 ,r11 ,repl_addr@lo
            this->jumpData[offset++] = 0x7d6903a6;                                                             // mtspr      CTR ,r11
            this->jumpData[offset]   = 0x4e800420;                                                             // bctr
        } else {
            this->jumpData[offset] = 0x48000002 | (replacementFunctionAddress & 0x01FFFFFC);
        }

        if (offset >= this->jumpDataSize) {
            DEBUG_FUNCTION_LINE_ERR("Tried to overflow buffer. offset: %08X vs array size: %08X", offset, this->jumpDataSize);
            OSFatal("Wrote too much data");
        }

        // Make sure the trampoline itself is usable.
        if (((uint32_t) this->jumpData & 0x01FFFFFC) != (uint32_t) this->jumpData) {
            DEBUG_FUNCTION_LINE_ERR("Jump is impossible");
            OSFatal("Jump is impossible");
        }

        this->replaceWithInstruction = 0x48000002 | ((uint32_t) this->jumpData & 0x01FFFFFC);

        DCFlushRange((void *) this->jumpData, sizeof(uint32_t) * 15);
        ICInvalidateRange((void *) this->jumpData, sizeof(uint32_t) * 15);
    }

    DCFlushRange((void *) &replaceWithInstruction, 4);
    ICInvalidateRange((void *) &replaceWithInstruction, 4);

    OSMemoryBarrier();
}

PatchedFunctionData::~PatchedFunctionData() {
    if (this->jumpToOriginal) {
        MEMFreeToExpHeap(this->heapHandle, this->jumpToOriginal);
        this->jumpToOriginal = nullptr;
    }
    if (this->jumpData) {
        MEMFreeToExpHeap(this->heapHandle, this->jumpData);
        this->jumpData = nullptr;
    }
}
