// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "elf.h"
#include "sndll.h"
#include "symbol_table.h"

namespace ccc {

struct SymbolTableLocation {
	std::string section_name;
	SymbolTableFormat format;
};

class SymbolFile {
public:
	virtual ~SymbolFile() {}
	
	virtual std::string name() const = 0;
	
	virtual Result<std::vector<std::unique_ptr<SymbolTable>>> get_all_symbol_tables() const = 0;
	virtual Result<std::vector<std::unique_ptr<SymbolTable>>> get_symbol_tables_from_sections(
		const std::vector<SymbolTableLocation>& sections) const = 0;
};

// Determine the type of the input file and parse it.
Result<std::unique_ptr<SymbolFile>> parse_symbol_file(std::vector<u8> image, std::string file_name);

class ElfSymbolFile : public SymbolFile {
public:
	ElfSymbolFile(ElfFile elf, std::string elf_name);
	
	std::string name() const override;
	
	Result<std::vector<std::unique_ptr<SymbolTable>>> get_all_symbol_tables() const override;
	Result<std::vector<std::unique_ptr<SymbolTable>>> get_symbol_tables_from_sections(
		const std::vector<SymbolTableLocation>& sections) const override;
	
	const ElfFile& elf() const;
	
protected:
	ElfFile m_elf;
	std::string m_name;
};

class SNDLLSymbolFile : public SymbolFile {
public:
	SNDLLSymbolFile(SNDLLFile sndll);
	
	std::string name() const override;
	
	Result<std::vector<std::unique_ptr<SymbolTable>>> get_all_symbol_tables() const override;
	Result<std::vector<std::unique_ptr<SymbolTable>>> get_symbol_tables_from_sections(
		const std::vector<SymbolTableLocation>& sections) const override;
	
protected:
	SNDLLFile m_sndll;
};

}
