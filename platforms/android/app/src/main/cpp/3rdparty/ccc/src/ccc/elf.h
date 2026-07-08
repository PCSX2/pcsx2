// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "symbol_database.h"

namespace ccc {

enum class ElfIdentClass : u8 {
	B32 = 0x1,
	B64 = 0x2
};

CCC_PACKED_STRUCT(ElfIdentHeader,
	/* 0x0 */ u32 magic; // 7f 45 4c 46
	/* 0x4 */ ElfIdentClass e_class;
	/* 0x5 */ u8 endianess;
	/* 0x6 */ u8 version;
	/* 0x7 */ u8 os_abi;
	/* 0x8 */ u8 abi_version;
	/* 0x9 */ u8 pad[7];
)

enum class ElfFileType : u16 {
	NONE   = 0x00,
	REL    = 0x01,
	EXEC   = 0x02,
	DYN    = 0x03,
	CORE   = 0x04,
	LOOS   = 0xfe00,
	HIOS   = 0xfeff,
	LOPROC = 0xff00,
	HIPROC = 0xffff
};

enum class ElfMachine : u16 {
	MIPS = 0x08
};

CCC_PACKED_STRUCT(ElfFileHeader,
	/* 0x10 */ ElfFileType type;
	/* 0x12 */ ElfMachine machine;
	/* 0x14 */ u32 version;
	/* 0x18 */ u32 entry;
	/* 0x1c */ u32 phoff;
	/* 0x20 */ u32 shoff;
	/* 0x24 */ u32 flags;
	/* 0x28 */ u16 ehsize;
	/* 0x2a */ u16 phentsize;
	/* 0x2c */ u16 phnum;
	/* 0x2e */ u16 shentsize;
	/* 0x30 */ u16 shnum;
	/* 0x32 */ u16 shstrndx;
)

enum class ElfSectionType : u32 {
	NULL_SECTION = 0x0,
	PROGBITS = 0x1,
	SYMTAB = 0x2,
	STRTAB = 0x3,
	RELA = 0x4,
	HASH = 0x5,
	DYNAMIC = 0x6,
	NOTE = 0x7,
	NOBITS = 0x8,
	REL = 0x9,
	SHLIB = 0xa,
	DYNSYM = 0xb,
	INIT_ARRAY = 0xe,
	FINI_ARRAY = 0xf,
	PREINIT_ARRAY = 0x10,
	GROUP = 0x11,
	SYMTAB_SHNDX = 0x12,
	NUM = 0x13,
	LOOS = 0x60000000,
	MIPS_DEBUG = 0x70000005
};

CCC_PACKED_STRUCT(ElfSectionHeader,
	/* 0x00 */ u32 name;
	/* 0x04 */ ElfSectionType type;
	/* 0x08 */ u32 flags;
	/* 0x0c */ u32 addr;
	/* 0x10 */ u32 offset;
	/* 0x14 */ u32 size;
	/* 0x18 */ u32 link;
	/* 0x1c */ u32 info;
	/* 0x20 */ u32 addralign;
	/* 0x24 */ u32 entsize;
)

struct ElfSection {
	std::string name;
	ElfSectionHeader header;
};

CCC_PACKED_STRUCT(ElfProgramHeader,
	/* 0x00 */ u32 type;
	/* 0x04 */ u32 offset;
	/* 0x08 */ u32 vaddr;
	/* 0x0c */ u32 paddr;
	/* 0x10 */ u32 filesz;
	/* 0x14 */ u32 memsz;
	/* 0x18 */ u32 flags;
	/* 0x1c */ u32 align;
)

struct ElfFile {
	ElfFileHeader file_header;
	std::vector<u8> image;
	std::vector<ElfSection> sections;
	std::vector<ElfProgramHeader> segments;
	
	// Parse the ELF file header, section headers and program headers.
	static Result<ElfFile> parse(std::vector<u8> image);
	
	// Create a section object for each section header in the ELF file.
	Result<void> create_section_symbols(SymbolDatabase& database, const SymbolGroup& group) const;
	
	const ElfSection* lookup_section(const char* name) const;
	std::optional<u32> file_offset_to_virtual_address(u32 file_offset) const;
	
	// Find the program header for the segment that contains the entry point.
	const ElfProgramHeader* entry_point_segment() const;
	
	// Retrieve a block of data in an ELF file given its address and size.
	std::optional<std::span<const u8>> get_virtual(u32 address, u32 size) const;
	
	// Copy a block of data in an ELF file to the destination buffer given its
	// address and size.
	bool copy_virtual(u8* dest, u32 address, u32 size) const;
	
	// Retrieve an object of type T from an ELF file given its address.
	template <typename T>
	std::optional<T> get_object_virtual(u32 address) const
	{
		std::optional<std::span<const u8>> result = get_virtual(address, sizeof(T));
		if(!result.has_value()) {
			return std::nullopt;
		}
		
		return *(T*) result->data();
	}
	
	// Retrieve an array of objects of type T from an ELF file given its
	// address and element count.
	template <typename T>
	std::optional<std::span<const T>> get_array_virtual(u32 address, u32 element_count) const
	{
		std::optional<std::span<const u8>> result = get_virtual(address, element_count * sizeof(T));
		if(!result.has_value()) {
			return std::nullopt;
		}
		
		return std::span<const T>((T*) result->data(), (T*) (result->data() + result->size()));
	}
};

}
