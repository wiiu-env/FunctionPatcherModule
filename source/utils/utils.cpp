#include <coreinit/cache.h>
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