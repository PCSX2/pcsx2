// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include <atomic>

#include "symbol_database.h"

namespace ccc {

// Determine which symbol tables are present in a given file.

enum SymbolTableFormat {
	MDEBUG = 0, // The infamous Third Eye symbol table
	SYMTAB = 1, // Standard ELF symbol table
	SNDLL  = 2  // SNDLL section
};

struct SymbolTableFormatInfo {
	SymbolTableFormat format;
	const char* format_name;
	const char* section_name;
};

// All the supported symbol table formats, sorted from best to worst.
extern const std::vector<SymbolTableFormatInfo> SYMBOL_TABLE_FORMATS;

const SymbolTableFormatInfo* symbol_table_format_from_enum(SymbolTableFormat format);
const SymbolTableFormatInfo* symbol_table_format_from_name(const char* format_name);
const SymbolTableFormatInfo* symbol_table_format_from_section(const char* section_name);

enum SymbolPrintFlags {
	PRINT_LOCALS = 1 << 0,
	PRINT_PROCEDURE_DESCRIPTORS = 1 << 1,
	PRINT_EXTERNALS = 1 << 2
};

class SymbolTable {
public:
	virtual ~SymbolTable() {}
	
	virtual const char* name() const = 0;
	
	// Imports this symbol table into the passed database.
	virtual Result<void> import(
		SymbolDatabase& database,
		const SymbolGroup& group,
		u32 importer_flags,
		DemanglerFunctions demangler,
		const std::atomic_bool* interrupt) const = 0;
	
	// Print out all the field in the header structure if one exists.
	virtual Result<void> print_headers(FILE* out) const = 0;
	
	// Print out all the symbols in the symbol table. For .mdebug symbol tables
	// the symbols are split between those that are local to a specific
	// translation unit and those that are external, which is what the
	// print_locals and print_externals parameters control.
	virtual Result<void> print_symbols(FILE* out, u32 flags) const = 0;
};

struct ElfSection;
struct ElfFile;

// Create a symbol table from an ELF section. The return value may be null.
Result<std::unique_ptr<SymbolTable>> create_elf_symbol_table(
	const ElfSection& section, const ElfFile& elf, SymbolTableFormat format);

// Utility function to call import_symbol_table on all the passed symbol tables
// and to generate a module handle.
Result<ModuleHandle> import_symbol_tables(
	SymbolDatabase& database,
	const std::vector<std::unique_ptr<SymbolTable>>& symbol_tables,
	std::string module_name,
	Address base_address,
	u32 importer_flags,
	DemanglerFunctions demangler,
	const std::atomic_bool* interrupt);

class MdebugSymbolTable : public SymbolTable {
public:
	MdebugSymbolTable(std::span<const u8> image, s32 section_offset);
	
	const char* name() const override;
	
	Result<void> import(
		SymbolDatabase& database,
		const SymbolGroup& group,
		u32 importer_flags,
		DemanglerFunctions demangler,
		const std::atomic_bool* interrupt) const override;
	
	Result<void> print_headers(FILE* out) const override;
	Result<void> print_symbols(FILE* out, u32 flags) const override;
	
protected:
	std::span<const u8> m_image;
	s32 m_section_offset;
};

class SymtabSymbolTable : public SymbolTable {
public:
	SymtabSymbolTable(std::span<const u8> symtab, std::span<const u8> strtab);
	
	const char* name() const override;
	
	Result<void> import(
		SymbolDatabase& database,
		const SymbolGroup& group,
		u32 importer_flags,
		DemanglerFunctions demangler,
		const std::atomic_bool* interrupt) const override;
	
	Result<void> print_headers(FILE* out) const override;
	Result<void> print_symbols(FILE* out, u32 flags) const override;
	
protected:
	std::span<const u8> m_symtab;
	std::span<const u8> m_strtab;
};

struct SNDLLFile;

class SNDLLSymbolTable : public SymbolTable {
public:
	SNDLLSymbolTable(std::shared_ptr<SNDLLFile> sndll);
	
	const char* name() const override;
	
	Result<void> import(
		SymbolDatabase& database,
		const SymbolGroup& group,
		u32 importer_flags,
		DemanglerFunctions demangler,
		const std::atomic_bool* interrupt) const override;
	
	Result<void> print_headers(FILE* out) const override;
	Result<void> print_symbols(FILE* out, u32 flags) const override;
	
protected:
	std::shared_ptr<SNDLLFile> m_sndll;
};

class ElfSectionHeadersSymbolTable : public SymbolTable {
public:
	ElfSectionHeadersSymbolTable(const ElfFile& elf);
	
	const char* name() const override;
	
	Result<void> import(
		SymbolDatabase& database,
		const SymbolGroup& group,
		u32 importer_flags,
		DemanglerFunctions demangler,
		const std::atomic_bool* interrupt) const override;
	
	Result<void> print_headers(FILE* out) const override;
	Result<void> print_symbols(FILE* out, u32 flags) const override;
protected:
	const ElfFile& m_elf;
};

}
