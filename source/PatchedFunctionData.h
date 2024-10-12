#pragma once

#include "fpatching_defines_legacy.h"
#include "FunctionAddressProvider.h"
#include "PatchedFunctionData.h"
#include "utils/logger.h"
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memorymap.h>
#include <cstdint>
#include <function_patcher/fpatching_defines.h>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>

class PatchedFunctionData {

public:
    ~PatchedFunctionData();

    explicit PatchedFunctionData(std::shared_ptr<FunctionAddressProvider> functionAddressProvider) : functionAddressProvider(std::move(functionAddressProvider)) {
    }

    static std::optional<std::shared_ptr<PatchedFunctionData>> make_shared_v2(std::shared_ptr<FunctionAddressProvider> functionAddressProvider,
                                                                              function_replacement_data_v2_t *replacementData,
                                                                              MEMHeapHandle heapHandle);
    static std::optional<std::shared_ptr<PatchedFunctionData>> make_shared_v3(std::shared_ptr<FunctionAddressProvider> functionAddressProvider,
                                                                              function_replacement_data_v3_t *replacementData,
                                                                              MEMHeapHandle heapHandle);

    bool allocateDataForJumps();

    bool getAddressForExecutable(uint32_t *outAddress) const;

    bool updateFunctionAddresses();

    void generateJumpToOriginal();

    void generateReplacementJump();

    [[nodiscard]] bool shouldBePatched() const;

    uint32_t getHandle() {
        return (uint32_t) this;
    }

    uint32_t *jumpToOriginal = {};
    uint32_t *jumpData       = {};

    uint32_t realEffectiveFunctionAddress = {};
    uint32_t realPhysicalFunctionAddress  = {};

    uint32_t *realCallFunctionAddressPtr = {};

    uint32_t replacementFunctionAddress = {};

    uint32_t replacedInstruction = {};

    uint32_t replaceWithInstruction = {};
    uint32_t jumpDataSize           = 15;
    MEMHeapHandle heapHandle        = nullptr;

    FunctionPatcherFunctionType type = {};
    std::set<uint64_t> titleIds;
    uint16_t titleVersionMin                  = 0;
    uint16_t titleVersionMax                  = 0xFFFF;
    std::optional<std::string> executableName = {};
    uint32_t textOffset                       = 0;

    bool isPatched                                                   = {};
    std::optional<function_replacement_library_type_t> library       = {};
    FunctionPatcherTargetProcess targetProcess                       = {};
    std::optional<std::string> functionName                          = {};
    std::shared_ptr<FunctionAddressProvider> functionAddressProvider = {};
};
