#include "CThread.h"
#include "globals.h"
#include "logger.h"


#include <coreinit/cache.h>
#include <coreinit/core.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memorymap.h>
#include <kernel/kernel.h>

bool ReadFromPhysicalAddress(uint32_t srcPhys, uint32_t *out) {
    if (!out) {
        return false;
    }
    // Check if patched instruction is still loaded.
    volatile uint32_t currentInstruction;

    auto currentInstructionAddress = (uint32_t) &currentInstruction;
    uint32_t currentInstructionAddressPhys;
    if (currentInstructionAddress < 0x00800000 || currentInstructionAddress >= 0x01000000) {
        currentInstructionAddressPhys = (uint32_t) OSEffectiveToPhysical(currentInstructionAddress);
    } else {
        currentInstructionAddressPhys = currentInstructionAddress + 0x30800000 - 0x00800000;
    }

    if (currentInstructionAddressPhys == 0) {
        return false;
    }
    // Save the instruction we will replace.
    KernelCopyData(currentInstructionAddressPhys, srcPhys, 4);
    DCFlushRange((void *) &currentInstruction, 4);
    *out = currentInstruction;
    return true;
}

bool CheckMemExpHeapBlock(MEMExpHeap *heap, MEMExpHeapBlockList *block, uint32_t tag, const char *listName, uint32_t &totalSizeOut) {
    MEMExpHeapBlock *prevBlock = nullptr;
    for (auto *cur = block->head; cur != nullptr; cur = cur->next) {
        if (cur->prev != prevBlock) {
            DEBUG_FUNCTION_LINE_ERR("[Exp Heap Check] \"%s\" prev is invalid. expected %p actual %p", listName, prevBlock, cur->prev);

            return false;
        }
        if (cur < heap->header.dataStart || cur > heap->header.dataEnd || ((uint32_t) cur + sizeof(MEMExpHeapBlock) + cur->blockSize) > (uint32_t) heap->header.dataEnd) {
            DEBUG_FUNCTION_LINE_ERR("[Exp Heap Check] Block is not inside heap. block: %p size %d; heap start %p heap end %p", cur, sizeof(MEMExpHeapBlock) + cur->blockSize, heap->header.dataStart, heap->header.dataEnd);

            return false;
        }
        if (cur->tag != tag) {
            DEBUG_FUNCTION_LINE_ERR("[%p][%d][Exp Heap Check] Invalid block tag expected %04X, actual %04X", &cur->tag, OSGetCoreId(), tag, cur->tag);

            return false;
        }

        totalSizeOut = totalSizeOut + cur->blockSize + (cur->attribs >> 8 & 0x7fffff) + sizeof(MEMExpHeapBlock);
        prevBlock    = cur;
    }
    if (prevBlock != block->tail) {
        DEBUG_FUNCTION_LINE_ERR("[Exp Heap Check] \"%s\" tail is unexpected! expected %p, actual %p", listName, heap->usedList.tail, prevBlock);

        return false;
    }
    return true;
}

bool CheckMemExpHeapCore(MEMExpHeap *heap) {
    uint32_t totalSize = 0;
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    if (!CheckMemExpHeapBlock(heap, &heap->usedList, 0x5544, "used", totalSize)) {
        return false;
    }

#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
    if (!CheckMemExpHeapBlock(heap, &heap->freeList, 0x4652, "free", totalSize)) {
        return false;
    }

    if (totalSize != (uint32_t) heap->header.dataEnd - (uint32_t) heap->header.dataStart) {
        DEBUG_FUNCTION_LINE_ERR("[Exp Heap Check] heap size is unexpected! expected %08X, actual %08X", (uint32_t) heap->header.dataEnd - (uint32_t) heap->header.dataStart, totalSize);
        return false;
    }
    return true;
}


bool CheckMemExpHeap(MEMExpHeap *heap) {

    OSMemoryBarrier();
    if (heap->header.tag != MEM_EXPANDED_HEAP_TAG) {
        DEBUG_FUNCTION_LINE_ERR("[Exp Heap Check] Invalid heap handle. - %08X", heap->header.tag);
        return false;
    }

    if (heap->header.flags & MEM_HEAP_FLAG_USE_LOCK) {
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        OSUninterruptibleSpinLock_Acquire(&(heap->header).lock);
    }

    auto result = CheckMemExpHeapCore(heap);

    if (heap->header.flags & MEM_HEAP_FLAG_USE_LOCK) {
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
        OSUninterruptibleSpinLock_Release(&(heap->header).lock);
    }

    return result;
}


static void CheckMemExpHeapJumpDataCallback(CThread *, void *) {
    if (gJumpHeapHandle != nullptr) {
        if (!CheckMemExpHeap(reinterpret_cast<MEMExpHeap *>(gJumpHeapHandle))) {
            OSFatal("FunctionPatcherModule: Corrupted heap");
        } else {
            DEBUG_FUNCTION_LINE_VERBOSE("JumpData heap has no curruption. Checked on core %d", OSGetCoreId());
        }
    }
}

void CheckMemExpHeapJumpData() {
    CThread::runOnAllCores(CheckMemExpHeapJumpDataCallback, nullptr, 0, 16, 0x1000);
}
