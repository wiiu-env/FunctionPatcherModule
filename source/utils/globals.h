#pragma once
#include "../PatchedFunctionData.h"
#include <coreinit/memheap.h>
#include <memory>
#include <vector>

#define JUMP_HEAP_DATA_SIZE (32 * 1024)
extern char gJumpHeapData[];
extern MEMHeapHandle gJumpHeapHandle;

extern std::shared_ptr<FunctionAddressProvider> gFunctionAddressProvider;
extern std::mutex gPatchedFunctionsMutex;
extern std::vector<std::shared_ptr<PatchedFunctionData>> gPatchedFunctions;

extern void *(*gRealMEMAllocFromDefaultHeapEx)(uint32_t size, int align);
extern void (*gMEMFreeToDefaultHeap)(void *ptr);