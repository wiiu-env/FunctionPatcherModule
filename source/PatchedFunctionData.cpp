#include "PatchedFunctionData.h"
#include "utils/KernelFindExport.h"
#include "utils/utils.h"
#include <coreinit/mcp.h>
#include <coreinit/title.h>
#include <vector>

std::optional<std::shared_ptr<PatchedFunctionData>> PatchedFunctionData::make_shared_v3(std::shared_ptr<FunctionAddressProvider> functionAddressProvider,
                                                                                        function_replacement_data_v3_t *replacementData,
                                                                                        MEMHeapHandle heapHandle) {
    if (!replacementData) {
        return {};
    }

    auto ptr = make_shared_nothrow<PatchedFunctionData>(std::move(functionAddressProvider));
    if (!ptr) {
        DEBUG_FUNCTION_LINE_ERR("Failed to alloc PatchedFunctionData");
        return {};
    }

    ptr->isPatched                  = false;
    ptr->heapHandle                 = heapHandle;
    ptr->replacementFunctionAddress = replacementData->replaceAddr;
    ptr->realCallFunctionAddressPtr = replacementData->replaceCall;
    ptr->targetProcess              = replacementData->targetProcess;
    ptr->type                       = replacementData->type;

    switch (replacementData->type) {
        case FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_NAME:
        case FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS: {
            ptr->library = {};
            for (uint32_t i = 0; i < replacementData->ReplaceInRPX.targetTitleIdsCount; i++) {
                ptr->titleIds.insert(replacementData->ReplaceInRPX.targetTitleIds[i]);
            }
            ptr->titleVersionMin = replacementData->ReplaceInRPX.versionMin;
            ptr->titleVersionMax = replacementData->ReplaceInRPX.versionMax;
            ptr->executableName  = replacementData->ReplaceInRPX.executableName;
            if (replacementData->type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS) {
                ptr->textOffset = replacementData->ReplaceInRPX.textOffset;
            } else if (replacementData->type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_NAME) {
                ptr->functionName = replacementData->ReplaceInRPX.functionName;
            }
            break;
        }
        case FUNCTION_PATCHER_REPLACE_BY_LIB_OR_ADDRESS: {
            ptr->library = replacementData->ReplaceInRPL.library;
            if (replacementData->ReplaceInRPL.library != LIBRARY_OTHER) {
                ptr->functionName = replacementData->ReplaceInRPL.function_name;
            } else {
                ptr->realEffectiveFunctionAddress = replacementData->virtualAddr;
                ptr->realPhysicalFunctionAddress  = replacementData->physicalAddr;
            }
            break;
        }
    }

    if (!ptr->allocateDataForJumps()) {
        return {};
    }

    return ptr;
}

std::optional<std::shared_ptr<PatchedFunctionData>> PatchedFunctionData::make_shared_v2(std::shared_ptr<FunctionAddressProvider> functionAddressProvider,
                                                                                        function_replacement_data_v2_t *replacementData,
                                                                                        MEMHeapHandle heapHandle) {
    if (!replacementData) {
        return {};
    }

    auto ptr = make_shared_nothrow<PatchedFunctionData>(std::move(functionAddressProvider));
    if (!ptr) {
        return {};
    }

    ptr->type                       = FUNCTION_PATCHER_REPLACE_BY_LIB_OR_ADDRESS;
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

    if (!ptr->allocateDataForJumps()) {
        return {};
    }

    return ptr;
}


bool PatchedFunctionData::allocateDataForJumps() {
    if (this->jumpData != nullptr && this->jumpToOriginal != nullptr) {
        return true;
    }
    if (this->replacementFunctionAddress > 0x01FFFFFC || this->targetProcess != FP_TARGET_PROCESS_ALL) {
        this->jumpDataSize = 15; // We could predict the actual size and save some memory, but at the moment we don't need it.
        this->jumpData     = (uint32_t *) MEMAllocFromExpHeapEx(this->heapHandle, this->jumpDataSize * sizeof(uint32_t), 4);

        if (!this->jumpData) {
            DEBUG_FUNCTION_LINE_ERR("Failed to alloc jump data");
            return false;
        }
    }

    this->jumpToOriginal = (uint32_t *) MEMAllocFromExpHeapEx(this->heapHandle, 0x5 * sizeof(uint32_t), 4);

    if (!this->jumpToOriginal) {
        DEBUG_FUNCTION_LINE_ERR("Failed to alloc jump data");
        return false;
    }
    return true;
}

bool PatchedFunctionData::getAddressForExecutable(uint32_t *outAddress) const {
    if (!outAddress) {
        return false;
    }

    if (!executableName.has_value()) {
        return false;
    }

    uint32_t result = 0;
    if (type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS) {
        int num_rpls = OSDynLoad_GetNumberOfRPLs();
        if (num_rpls == 0) {
            DEBUG_FUNCTION_LINE_ERR("OSDynLoad_GetNumberOfRPLs failed. Missing patches?");
            OSFatal("OSDynLoad_GetNumberOfRPLs failed. This shouldn't happen. Missing patches?");
            return false;
        }

        std::vector<OSDynLoad_NotifyData> rpls;
        rpls.resize(num_rpls);

        bool ret = OSDynLoad_GetRPLInfo(0, num_rpls, rpls.data());
        if (!ret) {
            DEBUG_FUNCTION_LINE_ERR("OSDynLoad_GetRPLInfo failed. Missing patches?");
            OSFatal("OSDynLoad_GetNumberOfRPLs failed. This shouldn't happen. Missing patches?");
            return false;
        }
        bool found = false;
        for (auto &rpl : rpls) {
            if (std::string_view(rpl.name).ends_with(executableName.value())) {
                result = rpl.textAddr + textOffset;
                found  = true;
                break;
            }
        }
        if (!found) {
            if (executableName->ends_with(".rpx")) {
                DEBUG_FUNCTION_LINE_ERR("Can't patch function. \"%s\" is not loaded.", executableName->c_str());
            } else {
                DEBUG_FUNCTION_LINE_WARN("Can't patch function. \"%s\" is not loaded.", executableName->c_str());
            }
            return false;
        }
    } else if (type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_NAME) {
        if (!this->functionName) {
            DEBUG_FUNCTION_LINE_ERR("Function name was empty. This should never happen.");
            OSFatal("Function name was empty. This should never happen. Check logs for more information.");
            return false;
        }
        result = KernelFindExport(executableName.value(), functionName.value());
        if (result == 0) {
            DEBUG_FUNCTION_LINE_WARN("Failed to find function \"%s\" in \"%s\".", functionName->c_str(), executableName->c_str());
            return false;
        }
    } else {
        DEBUG_FUNCTION_LINE_ERR("Unexpected function patching type. %d", type);
        OSFatal("Unexpected function patching type.");
        return false;
    }

    *outAddress = result;
    return true;
}

bool PatchedFunctionData::updateFunctionAddresses() {
    uint32_t real_address;
    if (type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_NAME || type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS) {
        if (!getAddressForExecutable(&real_address)) {
            return false;
        }
    } else {
        if (!this->library) {
            DEBUG_FUNCTION_LINE_ERR("library name was empty. This should never happen.");
            OSFatal("library was empty. This should never happen. Check logs for more information.");
            return false;
        }
        if (this->library == LIBRARY_OTHER) {
            // Use the provided physical/effective address!
            return true;
        }

        if (!this->functionName) {
            DEBUG_FUNCTION_LINE_ERR("Function name was empty. This should never happen.");
            OSFatal("Function name was empty. This should never happen. Check logs for more information.");
            return false;
        }

        real_address = functionAddressProvider->getEffectiveAddressOfFunction(library.value(), this->functionName->c_str());
        if (!real_address) {
            DEBUG_FUNCTION_LINE("OSDynLoad_FindExport failed for %s, updating address not possible.", this->functionName->c_str());
            return false;
        }
    }

    this->realEffectiveFunctionAddress = real_address;
    auto physicalFunctionAddress       = (uint32_t) OSEffectiveToPhysical(real_address);
    if (!physicalFunctionAddress) {
        DEBUG_FUNCTION_LINE_ERR("Error. Something is wrong with the physical address");
        OSFatal("Error. Something is wrong with the physical address");
        return false;
    }
    this->realPhysicalFunctionAddress = physicalFunctionAddress;
    return true;
}

void PatchedFunctionData::generateJumpToOriginal() {
    if (!this->jumpToOriginal) {
        DEBUG_FUNCTION_LINE_ERR("this->jumpToOriginal is not allocated");
        OSFatal("FunctionPatcherModule: this->jumpToOriginal is not allocated");
    }

    uint32_t jumpToAddress = this->realEffectiveFunctionAddress + 4;

    if (((uint32_t) jumpToAddress & 0x01FFFFFC) != (uint32_t) jumpToAddress) {
        // We need to do a long jump
        this->jumpToOriginal[0] = 0x3d600000 | ((jumpToAddress >> 16) & 0x0000FFFF); // lis        r11 ,0x1234
        this->jumpToOriginal[1] = 0x616b0000 | (jumpToAddress & 0x0000ffff);         // ori        r11 ,r11 ,0x5678
        this->jumpToOriginal[2] = 0x7d6903a6;                                        // mtspr      CTR ,r11
        this->jumpToOriginal[3] = this->replacedInstruction;
        this->jumpToOriginal[4] = 0x4e800420; // bctr
    } else {
        this->jumpToOriginal[0] = this->replacedInstruction;
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
            OSFatal("FunctionPatcherModule: jumpData was not allocated");
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
            OSFatal("FunctionPatcherModule: Wrote too much data");
        }

        // Make sure the trampoline itself is usable.
        if (((uint32_t) this->jumpData & 0x01FFFFFC) != (uint32_t) this->jumpData) {
            DEBUG_FUNCTION_LINE_ERR("Jump is impossible");
            OSFatal("FunctionPatcherModule: Jump is impossible");
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

bool PatchedFunctionData::shouldBePatched() const {
    if (type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_NAME || type == FUNCTION_PATCHER_REPLACE_FOR_EXECUTABLE_BY_ADDRESS) {
        uint64_t curTitleId = OSGetTitleID();
        if (!this->titleIds.contains(curTitleId)) {
            DEBUG_FUNCTION_LINE_VERBOSE("Skip function patch. Patch is not for title %016llX", curTitleId);
            return false;
        }
        auto mcpHandle = MCP_Open();
        MCPTitleListType titleInfo;
        int32_t res = -1;
        if ((curTitleId & 0x0000000F00000000) == 0) {
            res = MCP_GetTitleInfo(mcpHandle, curTitleId | 0x0000000E00000000, &titleInfo);
        }
        if (res != 0) {
            res = MCP_GetTitleInfo(mcpHandle, curTitleId, &titleInfo);
        }
        MCP_Close(mcpHandle);
        if (res != 0) {
            DEBUG_FUNCTION_LINE_WARN("Failed to get title version of %016llX.", curTitleId);
            OSFatal("Failed to get title version. This should not happen.\n"
                    "Please report this with a crash log.");
            return false;
        }
        MCP_Close(mcpHandle);
        if (titleInfo.titleVersion < titleVersionMin || titleInfo.titleVersion > titleVersionMax) {
            DEBUG_FUNCTION_LINE("Skipping function patch. Title version does not match: Expected  >= %d && <= %d. Real version: %d", titleVersionMin, titleVersionMax, titleInfo.titleVersion);
            return false;
        }
    }
    return true;
}
