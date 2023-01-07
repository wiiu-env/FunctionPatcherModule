#include "KernelFindExport.h"
#include <coreinit/cache.h>
#include <elf.h>
#include <kernel/kernel.h>
#include <string_view>

#define KernelGetLoadedRPL ((LOADED_RPL * (*) (uint32_t))(0xfff13524))
#define KernelGetRAMPID    ((int32_t(*)(uint32_t *))(0xfff10ea0))
#define KernelSetRAMPID    ((uint32_t(*)(uint32_t, uint32_t))(0xfff10cc0))

#define ELF_ST_TYPE(i)     ((i) &0xf)

/*
 * Based on the findClosestSymbol implementation. See:
 * https://github.com/decaf-emu/decaf-emu/blob/e8c9af3057a7d94f6e970406eb1ba1c37c87b4d1/src/libdecaf/src/cafe/kernel/cafe_kernel_loader.cpp#L251
 */
uint32_t FindExportKernel(const char *rplName, const char *functionName) {
    if (rplName == nullptr || functionName == nullptr) {
        return 0;
    }
    uint32_t result = 0;
    uint32_t currentRamPID;
    auto err = KernelGetRAMPID(&currentRamPID);
    if (err != -1) {
        // Switch to loader address space view.
        KernelSetRAMPID(err, 2);
        for (auto rpl = KernelGetLoadedRPL(0); rpl != nullptr; rpl = rpl->nextLoadedRpl) {
            if (std::string_view(rpl->moduleNameBuffer) != rplName) {
                continue;
            }

            uint32_t textSectionIndex = 0xFFFFFFFF;
            for (auto i = 0u; i < rpl->elfHeader.shnum; ++i) {
                auto sectionAddress = rpl->sectionAddressBuffer[i];
                if (!sectionAddress) {
                    continue;
                }
                auto sectionHeader = (ElfSectionHeader *) (((uint32_t) (rpl->sectionHeaderBuffer)) + rpl->elfHeader.shentsize * i);

                if (auto shstrndx = rpl->elfHeader.shstrndx) {
                    auto shStrSection = (char *) (rpl->sectionAddressBuffer[shstrndx]);
                    auto sectionName  = (shStrSection + sectionHeader->name);
                    if (std::string_view(sectionName) == ".text") {
                        textSectionIndex = i;
                    }
                }
            }
            for (auto i = 0u; i < rpl->elfHeader.shnum; ++i) {
                auto sectionAddress = rpl->sectionAddressBuffer[i];
                if (!sectionAddress) {
                    continue;
                }
                auto sectionHeader = (ElfSectionHeader *) (((uint32_t) (rpl->sectionHeaderBuffer)) + rpl->elfHeader.shentsize * i);

                if (sectionHeader->type == SHT_SYMTAB) {
                    auto strTab        = rpl->sectionAddressBuffer[sectionHeader->link];
                    auto symTabEntSize = sectionHeader->entsize ? static_cast<uint32_t>(sectionHeader->entsize) : sizeof(ElfSymbol);
                    auto numSymbols    = sectionHeader->size / symTabEntSize;
                    bool found         = false;
                    for (auto j = 0u; j < numSymbols; ++j) {
                        auto symbol = (ElfSymbol *) (sectionAddress + j * symTabEntSize);
                        if (symbol->shndx == textSectionIndex && ELF_ST_TYPE(symbol->info) == STT_FUNC) {
                            auto symbolName = (const char *) (strTab + symbol->name);
                            if (std::string_view(symbolName) == functionName) {
                                result = symbol->value;
                                found  = true;
                                break;
                            }
                        }
                    }
                    if (found) {
                        break;
                    }
                }
            }
            break;
        }
        // Switch back to "old" space address view
        KernelSetRAMPID(err, currentRamPID);
    }
    return result;
}

extern "C" uint32_t SC_0x51(const char *rplname, const char *functionName);

uint32_t KernelFindExport(const std::string_view &rplName, const std::string_view &functionName) {
    KernelPatchSyscall(0x51, (uint32_t) &FindExportKernel);
    OSMemoryBarrier();
    if (rplName.ends_with(".rpx") || rplName.ends_with(".rpl")) {
        auto pureRPLName = std::string(rplName).substr(0, rplName.length() - 4);
        return SC_0x51(pureRPLName.c_str(), functionName.data());
    }
    return SC_0x51(rplName.data(), functionName.data());
}