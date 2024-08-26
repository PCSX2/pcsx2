// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#include "elf.h"

namespace ccc {

Result<ElfFile> ElfFile::parse(std::vector<u8> image)
{
	ElfFile elf;
	elf.image = std::move(image);
	
	const ElfIdentHeader* ident = get_packed<ElfIdentHeader>(elf.image, 0);
	CCC_CHECK(ident, "ELF ident header out of range.");
	CCC_CHECK(ident->magic == CCC_FOURCC("\x7f\x45\x4c\x46"), "Not an ELF file.");
	CCC_CHECK(ident->e_class == ElfIdentClass::B32, "Wrong ELF class (not 32 bit).");
	
	const ElfFileHeader* header = get_packed<ElfFileHeader>(elf.image, sizeof(ElfIdentHeader));
	CCC_CHECK(header, "ELF file header out of range.");
	elf.file_header = *header;
	
	const ElfSectionHeader* shstr_section_header = get_packed<ElfSectionHeader>(elf.image, header->shoff + header->shstrndx * sizeof(ElfSectionHeader));
	CCC_CHECK(shstr_section_header, "ELF section name header out of range.");
	
	for(u32 i = 0; i < header->shnum; i++) {
		u64 header_offset = header->shoff + i * sizeof(ElfSectionHeader);
		const ElfSectionHeader* section_header = get_packed<ElfSectionHeader>(elf.image, header_offset);
		CCC_CHECK(section_header, "ELF section header out of range.");
		
		const char* name = get_string(elf.image, shstr_section_header->offset + section_header->name);
		CCC_CHECK(section_header, "ELF section name out of range.");
		
		ElfSection& section = elf.sections.emplace_back();
		section.name = name;
		section.header = *section_header;
	}
	
	for(u32 i = 0; i < header->phnum; i++) {
		u64 header_offset = header->phoff + i * sizeof(ElfProgramHeader);
		const ElfProgramHeader* program_header = get_packed<ElfProgramHeader>(elf.image, header_offset);
		CCC_CHECK(program_header, "ELF program header out of range.");
		
		elf.segments.emplace_back(*program_header);
	}
	
	return elf;
}

Result<void> ElfFile::create_section_symbols(
	SymbolDatabase& database, const SymbolGroup& group) const
{
	for(const ElfSection& section : sections) {
		Address address = Address::non_zero(section.header.addr);
		
		Result<Section*> symbol = database.sections.create_symbol(
			section.name, address, group.source, group.module_symbol);
		CCC_RETURN_IF_ERROR(symbol);
		
		(*symbol)->set_size(section.header.size);
	}
	
	return Result<void>();
}

const ElfSection* ElfFile::lookup_section(const char* name) const
{
	for(const ElfSection& section : sections) {
		if(section.name == name) {
			return &section;
		}
	}
	return nullptr;
}

std::optional<u32> ElfFile::file_offset_to_virtual_address(u32 file_offset) const
{
	for(const ElfProgramHeader& segment : segments) {
		if(file_offset >= segment.offset && file_offset < segment.offset + segment.filesz) {
			return segment.vaddr + file_offset - segment.offset;
		}
	}
	return std::nullopt;
}

const ElfProgramHeader* ElfFile::entry_point_segment() const
{
	const ccc::ElfProgramHeader* entry_segment = nullptr;
	for(const ccc::ElfProgramHeader& segment : segments) {
		if(file_header.entry >= segment.vaddr && file_header.entry < segment.vaddr + segment.filesz) {
			entry_segment = &segment;
		}
	}
	return entry_segment;
}

Result<std::span<const u8>> ElfFile::get_virtual(u32 address, u32 size) const
{
	u32 end_address = address + size;
	
	if(end_address >= address) {
		for(const ElfProgramHeader& segment : segments) {
			if(address >= segment.vaddr && end_address <= segment.vaddr + segment.filesz) {
				size_t begin_offset = segment.offset + (address - segment.vaddr);
				size_t end_offset = begin_offset + size;
				if(begin_offset <= image.size() && end_offset <= image.size()) {
					return std::span<const u8>(image.data() + begin_offset, image.data() + end_offset);
				}
			}
		}
	}
	
	return CCC_FAILURE("No ELF segment for address range 0x%x to 0x%x.", address, end_address);
}

Result<void> ElfFile::copy_virtual(u8* dest, u32 address, u32 size) const
{
	Result<std::span<const u8>> block = get_virtual(address, size);
	CCC_RETURN_IF_ERROR(block);
	
	memcpy(dest, block->data(), size);
	
	return Result<void>();
}

}
