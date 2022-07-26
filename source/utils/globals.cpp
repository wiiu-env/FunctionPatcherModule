#include "globals.h"

char gJumpHeapData[JUMP_HEAP_DATA_SIZE] __attribute__((section(".data")));
MEMHeapHandle gJumpHeapHandle __attribute__((section(".data")));

std::shared_ptr<FunctionAddressProvider> gFunctionAddressProvider;
std::mutex gPatchedFunctionsMutex;
std::vector<std::shared_ptr<PatchedFunctionData>> gPatchedFunctions;

void *(*gMEMAllocFromDefaultHeapExForThreads)(uint32_t size, int align);
void (*gMEMFreeToDefaultHeapForThreads)(void *ptr);