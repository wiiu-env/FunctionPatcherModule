#pragma once
#include "../PatchedFunctionData.h"
#include "version.h"
#include <coreinit/memheap.h>
#include <memory>
#include <vector>

#define MODULE_VERSION      "v0.2.1"
#define MODULE_VERSION_FULL MODULE_VERSION MODULE_VERSION_EXTRA

#define JUMP_HEAP_DATA_SIZE (32 * 1024)
extern char gJumpHeapData[];
extern MEMHeapHandle gJumpHeapHandle;

extern std::shared_ptr<FunctionAddressProvider> gFunctionAddressProvider;
extern std::mutex gPatchedFunctionsMutex;
extern std::vector<std::shared_ptr<PatchedFunctionData>> gPatchedFunctions;

extern void *(*gMEMAllocFromDefaultHeapExForThreads)(uint32_t size, int align);
extern void (*gMEMFreeToDefaultHeapForThreads)(void *ptr);
