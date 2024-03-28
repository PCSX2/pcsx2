// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "symbol_file.h"

namespace ccc {

Result<std::unique_ptr<SymbolFile>> parse_symbol_file(std::vector<u8> image, std::string file_name)
{
	const u32* magic = get_packed<u32>(image, 0);
	CCC_CHECK(magic, "File too small.");
	
	std::unique_ptr<SymbolFile> symbol_file;
	
	switch(*magic) {
		case CCC_FOURCC("\x7f""ELF"): {
			Result<ElfFile> elf = ElfFile::parse(std::move(image));
			CCC_RETURN_IF_ERROR(elf);
			
			symbol_file = std::make_unique<ElfSymbolFile>(std::move(*elf), std::move(file_name));
			break;
		}
		case CCC_FOURCC("SNR1"):
		case CCC_FOURCC("SNR2"): {
			Result<SNDLLFile> sndll = parse_sndll_file(image, Address(), SNDLLType::DYNAMIC_LIBRARY);
			CCC_RETURN_IF_ERROR(sndll);
			
			symbol_file = std::make_unique<SNDLLSymbolFile>(std::move(*sndll));
			break;
		}
		default: {
			return CCC_FAILURE("Unknown file type.");
		}
	}
	
	return symbol_file;
}

ElfSymbolFile::ElfSymbolFile(ElfFile elf, std::string elf_name)
	: m_elf(std::move(elf)), m_name(std::move(elf_name)) {}

std::string ElfSymbolFile::name() const
{
	return m_name;
}

Result<std::vector<std::unique_ptr<SymbolTable>>> ElfSymbolFile::get_all_symbol_tables() const
{
	std::vector<std::unique_ptr<SymbolTable>> symbol_tables;
	
	symbol_tables.emplace_back(std::make_unique<ElfSectionHeadersSymbolTable>(m_elf));
	
	for(size_t i = 0; i < SYMBOL_TABLE_FORMATS.size(); i++) {
		const SymbolTableFormatInfo& info = SYMBOL_TABLE_FORMATS[i];
		
		const ElfSection* section = m_elf.lookup_section(info.section_name);
		if(section) {
			Result<std::unique_ptr<SymbolTable>> symbol_table = create_elf_symbol_table(*section, m_elf, info.format);
			CCC_RETURN_IF_ERROR(symbol_table);
			if(*symbol_table) {
				symbol_tables.emplace_back(std::move(*symbol_table));
			}
		}
	}
	
	return symbol_tables;
}

Result<std::vector<std::unique_ptr<SymbolTable>>> ElfSymbolFile::get_symbol_tables_from_sections(
	const std::vector<SymbolTableLocation>& sections) const
{
	std::vector<std::unique_ptr<SymbolTable>> symbol_tables;
	
	for(const SymbolTableLocation& location : sections) {
		const ElfSection* section = m_elf.lookup_section(location.section_name.c_str());
		CCC_CHECK(section, "No '%s' section.", location.section_name.c_str());
		
		Result<std::unique_ptr<SymbolTable>> symbol_table = create_elf_symbol_table(*section, m_elf, location.format);
		CCC_RETURN_IF_ERROR(symbol_table);
		if(*symbol_table) {
			symbol_tables.emplace_back(std::move(*symbol_table));
		}
	}
	
	return symbol_tables;
}

const ElfFile& ElfSymbolFile::elf() const
{
	return m_elf;
}

SNDLLSymbolFile::SNDLLSymbolFile(SNDLLFile sndll)
	: m_sndll(std::move(sndll)) {}

std::string SNDLLSymbolFile::name() const
{
	return m_sndll.elf_path;
}

Result<std::vector<std::unique_ptr<SymbolTable>>> SNDLLSymbolFile::get_all_symbol_tables() const
{
	std::vector<std::unique_ptr<SymbolTable>> symbol_tables;
	symbol_tables.emplace_back(std::make_unique<SNDLLSymbolTable>(std::make_shared<SNDLLFile>(std::move(m_sndll))));
	return symbol_tables;
}

Result<std::vector<std::unique_ptr<SymbolTable>>> SNDLLSymbolFile::get_symbol_tables_from_sections(
	const std::vector<SymbolTableLocation>& sections) const
{
	return CCC_FAILURE("An SNDLL file is not composed of sections.");
}

}
