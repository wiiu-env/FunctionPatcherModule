#pragma once
#include <cstdint>
#include <string>
#include <wut.h>

/*
 * https://github.com/decaf-emu/decaf-emu/blob/e8c9af3057a7d94f6e970406eb1ba1c37c87b4d1/src/libdecaf/src/cafe/loader/cafe_loader_rpl.h#L150
 */
struct ElfHeader {
    uint8_t magic[4];   // File identification.
    uint8_t fileClass;  // File class.
    uint8_t encoding;   // Data encoding.
    uint8_t elfVersion; // File version.
    uint8_t abi;        // OS/ABI identification.
    uint8_t abiVersion; // OS/ABI version.
    uint8_t pad[7];

    uint16_t type;      // Type of file (ET_*)
    uint16_t machine;   // Required architecture for this file (EM_*)
    uint32_t version;   // Must be equal to 1
    uint32_t entry;     // Address to jump to in order to start program
    uint32_t phoff;     // Program header table's file offset, in bytes
    uint32_t shoff;     // Section header table's file offset, in bytes
    uint32_t flags;     // Processor-specific flags
    uint16_t ehsize;    // Size of ELF header, in bytes
    uint16_t phentsize; // Size of an entry in the program header table
    uint16_t phnum;     // Number of entries in the program header table
    uint16_t shentsize; // Size of an entry in the section header table
    uint16_t shnum;     // Number of entries in the section header table
    uint16_t shstrndx;  // Sect hdr table index of sect name string table
};
WUT_CHECK_OFFSET(ElfHeader, 0x00, magic);
WUT_CHECK_OFFSET(ElfHeader, 0x04, fileClass);
WUT_CHECK_OFFSET(ElfHeader, 0x05, encoding);
WUT_CHECK_OFFSET(ElfHeader, 0x06, elfVersion);
WUT_CHECK_OFFSET(ElfHeader, 0x07, abi);
WUT_CHECK_OFFSET(ElfHeader, 0x08, abiVersion);
WUT_CHECK_OFFSET(ElfHeader, 0x10, type);
WUT_CHECK_OFFSET(ElfHeader, 0x12, machine);
WUT_CHECK_OFFSET(ElfHeader, 0x14, version);
WUT_CHECK_OFFSET(ElfHeader, 0x18, entry);
WUT_CHECK_OFFSET(ElfHeader, 0x1C, phoff);
WUT_CHECK_OFFSET(ElfHeader, 0x20, shoff);
WUT_CHECK_OFFSET(ElfHeader, 0x24, flags);
WUT_CHECK_OFFSET(ElfHeader, 0x28, ehsize);
WUT_CHECK_OFFSET(ElfHeader, 0x2A, phentsize);
WUT_CHECK_OFFSET(ElfHeader, 0x2C, phnum);
WUT_CHECK_OFFSET(ElfHeader, 0x2E, shentsize);
WUT_CHECK_OFFSET(ElfHeader, 0x30, shnum);
WUT_CHECK_OFFSET(ElfHeader, 0x32, shstrndx);
WUT_CHECK_SIZE(ElfHeader, 0x34);

/*
 * https://github.com/decaf-emu/decaf-emu/blob/e8c9af3057a7d94f6e970406eb1ba1c37c87b4d1/src/libdecaf/src/cafe/loader/cafe_loader_rpl.h#L216
 */
struct ElfSectionHeader {
    //! Section name (index into string table)
    uint32_t name;

    //! Section type (SHT_*)
    uint32_t type;

    //! Section flags (SHF_*)
    uint32_t flags;

    //! Address where section is to be loaded
    uint32_t addr;

    //! File offset of section data, in bytes
    uint32_t offset;

    //! Size of section, in bytes
    uint32_t size;

    //! Section type-specific header table index link
    uint32_t link;

    //! Section type-specific extra information
    uint32_t info;

    //! Section address alignment
    uint32_t addralign;

    //! Size of records contained within the section
    uint32_t entsize;
};
WUT_CHECK_OFFSET(ElfSectionHeader, 0x00, name);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x04, type);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x08, flags);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x0C, addr);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x10, offset);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x14, size);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x18, link);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x1C, info);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x20, addralign);
WUT_CHECK_OFFSET(ElfSectionHeader, 0x24, entsize);
WUT_CHECK_SIZE(ElfSectionHeader, 0x28);


/*
 * https://github.com/decaf-emu/decaf-emu/blob/e8c9af3057a7d94f6e970406eb1ba1c37c87b4d1/src/libdecaf/src/cafe/loader/cafe_loader_rpl.h#L260
 */
struct ElfSymbol {
    //! Symbol name (index into string table)
    uint32_t name;

    //! Value or address associated with the symbol
    uint32_t value;

    //! Size of the symbol
    uint32_t size;

    //! Symbol's type and binding attributes
    uint8_t info;

    //! Must be zero; reserved
    uint8_t other;

    //! Which section (header table index) it's defined in (SHN_*)
    uint16_t shndx;
};
WUT_CHECK_OFFSET(ElfSymbol, 0x00, name);
WUT_CHECK_OFFSET(ElfSymbol, 0x04, value);
WUT_CHECK_OFFSET(ElfSymbol, 0x08, size);
WUT_CHECK_OFFSET(ElfSymbol, 0x0C, info);
WUT_CHECK_OFFSET(ElfSymbol, 0x0D, other);
WUT_CHECK_OFFSET(ElfSymbol, 0x0E, shndx);
WUT_CHECK_SIZE(ElfSymbol, 0x10);

/*
 * https://github.com/decaf-emu/decaf-emu/blob/6feb1be1db3938e6da2d4a65fc0a7a8599fc8dd6/src/libdecaf/src/cafe/loader/cafe_loader_loaded_rpl.h#L26
 */
struct LOADED_RPL {
    WUT_UNKNOWN_BYTES(0x08);
    char *moduleNameBuffer;
    WUT_UNKNOWN_BYTES(0x10);
    ElfHeader elfHeader;
    void *sectionHeaderBuffer;
    WUT_UNKNOWN_BYTES(0xA0);
    uint32_t *sectionAddressBuffer;
    WUT_UNKNOWN_BYTES(0x1C);
    LOADED_RPL *nextLoadedRpl;
};
WUT_CHECK_OFFSET(LOADED_RPL, 0x08, moduleNameBuffer);
WUT_CHECK_OFFSET(LOADED_RPL, 0x1C, elfHeader);
WUT_CHECK_OFFSET(LOADED_RPL, 0x50, sectionHeaderBuffer);
WUT_CHECK_OFFSET(LOADED_RPL, 0xF4, sectionAddressBuffer);
WUT_CHECK_OFFSET(LOADED_RPL, 0x114, nextLoadedRpl);
WUT_CHECK_SIZE(LOADED_RPL, 0x118);

uint32_t KernelFindExport(const std::string_view &rplName, const std::string_view &functioName);