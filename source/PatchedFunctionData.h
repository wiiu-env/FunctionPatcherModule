#pragma once

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
#include <string>
#include <utility>

class PatchedFunctionData {

public:
    ~PatchedFunctionData();

    explicit PatchedFunctionData(std::shared_ptr<FunctionAddressProvider> functionAddressProvider) : functionAddressProvider(std::move(functionAddressProvider)) {
    }

    static std::optional<std::shared_ptr<PatchedFunctionData>> make_shared(std::shared_ptr<FunctionAddressProvider> functionAddressProvider,
                                                                           function_replacement_data_t *replacementData,
                                                                           MEMHeapHandle heapHandle);

    bool updateFunctionAddresses();

    void generateJumpToOriginal();

    void generateReplacementJump();

    uint32_t getHandle() {
        return (uint32_t) this;
    }

    uint32_t *jumpToOriginal{};
    uint32_t *jumpData{};

    uint32_t realEffectiveFunctionAddress{};
    uint32_t realPhysicalFunctionAddress{};

    uint32_t *realCallFunctionAddressPtr{};

    uint32_t replacementFunctionAddress{};

    uint32_t replacedInstruction{};

    uint32_t replaceWithInstruction{};
    uint32_t jumpDataSize    = 15;
    MEMHeapHandle heapHandle = nullptr;

    bool isPatched{};
    function_replacement_library_type_t library{};
    FunctionPatcherTargetProcess targetProcess{};
    std::optional<std::string> functionName = {};
    std::shared_ptr<FunctionAddressProvider> functionAddressProvider;
};
