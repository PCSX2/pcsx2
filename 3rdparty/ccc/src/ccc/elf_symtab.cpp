// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#include "elf_symtab.h"

#include "importer_flags.h"

namespace ccc::elf {

enum class SymbolBind : u8 {
	LOCAL = 0,
	GLOBAL = 1,
	WEAK = 2,
	NUM = 3,
	GNU_UNIQUE = 10
};

enum class SymbolType : u8 {
	NOTYPE = 0,
	OBJECT = 1,
	FUNC = 2,
	SECTION = 3,
	FILE = 4,
	COMMON = 5,
	TLS = 6,
	NUM = 7,
	GNU_IFUNC = 10
};

enum class SymbolVisibility {
	DEFAULT = 0,
	INTERNAL = 1,
	HIDDEN = 2,
	PROTECTED = 3
};

CCC_PACKED_STRUCT(Symbol,
	/* 0x0 */ u32 name;
	/* 0x4 */ u32 value;
	/* 0x8 */ u32 size;
	/* 0xc */ u8 info;
	/* 0xd */ u8 other;
	/* 0xe */ u16 shndx;
	
	SymbolType type() const { return (SymbolType) (info & 0xf); }
	SymbolBind bind() const { return (SymbolBind) (info >> 4); }
	SymbolVisibility visibility() const { return (SymbolVisibility) (other & 0x3); }
)

static const char* symbol_bind_to_string(SymbolBind bind);
static const char* symbol_type_to_string(SymbolType type);
static const char* symbol_visibility_to_string(SymbolVisibility visibility);

Result<void> import_symbols(
	SymbolDatabase& database,
	const SymbolGroup& group,
	std::span<const u8> symtab,
	std::span<const u8> strtab,
	u32 importer_flags,
	DemanglerFunctions demangler)
{
	for(u32 i = 0; i < symtab.size() / sizeof(Symbol); i++) {
		const Symbol* symbol = get_packed<Symbol>(symtab, i * sizeof(Symbol));
		CCC_ASSERT(symbol);
		
		Address address;
		if(symbol->value != 0) {
			address = symbol->value;
		}
		
		if(!address.valid() || symbol->visibility() != SymbolVisibility::DEFAULT) {
			continue;
		}
		
		if(!(importer_flags & DONT_DEDUPLICATE_SYMBOLS)) {
			if(database.functions.first_handle_from_starting_address(address).valid()) {
				continue;
			}
			
			if(database.global_variables.first_handle_from_starting_address(address).valid()) {
				continue;
			}
			
			if(database.local_variables.first_handle_from_starting_address(address).valid()) {
				continue;
			}
		}
		
		const char* string = get_string(strtab, symbol->name);
		CCC_CHECK(string, "Symbol string out of range.");
		
		switch(symbol->type()) {
			case SymbolType::NOTYPE: {
				Result<Label*> label = database.labels.create_symbol(
					string, group.source, group.module_symbol, address, importer_flags, demangler);
				CCC_RETURN_IF_ERROR(label);
				
				// These symbols get emitted at the same addresses as functions
				// and aren't extremely useful, so we want to mark them to
				// prevent them from possibly being used as function names.
				(*label)->is_junk =
					(*label)->name() == "__gnu_compiled_c" ||
					(*label)->name() == "__gnu_compiled_cplusplus" ||
					(*label)->name() == "gcc2_compiled.";
				
				break;
			}
			case SymbolType::OBJECT: {
				if(symbol->size != 0) {
					Result<GlobalVariable*> global_variable = database.global_variables.create_symbol(
						string, group.source, group.module_symbol, address, importer_flags, demangler);
					CCC_RETURN_IF_ERROR(global_variable);
					
					if(*global_variable) {
						(*global_variable)->set_size(symbol->size);
					}
				} else {
					Result<Label*> label = database.labels.create_symbol(
						string, group.source, group.module_symbol, address, importer_flags, demangler);
					CCC_RETURN_IF_ERROR(label);
				}
				
				break;
			}
			case SymbolType::FUNC: {
				Result<Function*> function = database.functions.create_symbol(
					string, group.source, group.module_symbol, address, importer_flags, demangler);
				CCC_RETURN_IF_ERROR(function);
				
				if(*function) {
					(*function)->set_size(symbol->size);
				}
				
				break;
			}
			case SymbolType::FILE: {
				Result<SourceFile*> source_file = database.source_files.create_symbol(
					string, group.source, group.module_symbol);
				CCC_RETURN_IF_ERROR(source_file);
				
				break;
			}
			default: {}
		}
	}
	
	return Result<void>();
}

Result<void> print_symbol_table(FILE* out, std::span<const u8> symtab, std::span<const u8> strtab)
{
	fprintf(out, "ELF SYMBOLS:\n");
	fprintf(out, "   Num:    Value  Size Type    Bind   Vis      Ndx Name\n");
	
	for(u32 i = 0; i < symtab.size() / sizeof(Symbol); i++) {
		const Symbol* symbol = get_packed<Symbol>(symtab, i * sizeof(Symbol));
		CCC_ASSERT(symbol);
		
		const char* type = symbol_type_to_string(symbol->type());
		const char* bind = symbol_bind_to_string(symbol->bind());
		const char* visibility = symbol_visibility_to_string(symbol->visibility());
		
		const char* string = get_string(strtab, symbol->name);
		CCC_CHECK(string, "Symbol string out of range.");
		
		fprintf(out, "%6u: %08x %5u %-7s %-7s %-7s %3u %s\n",
			i, symbol->value, symbol->size, type, bind, visibility, symbol->shndx, string);
		
	}
	
	return Result<void>();
}

static const char* symbol_bind_to_string(SymbolBind bind)
{
	switch(bind) {
		case SymbolBind::LOCAL: return "LOCAL";
		case SymbolBind::GLOBAL: return "GLOBAL";
		case SymbolBind::WEAK: return "WEAK";
		case SymbolBind::NUM: return "NUM";
		case SymbolBind::GNU_UNIQUE: return "GNU_UNIQUE";
	}
	return "ERROR";
}

static const char* symbol_type_to_string(SymbolType type)
{
	switch(type) {
		case SymbolType::NOTYPE: return "NOTYPE";
		case SymbolType::OBJECT: return "OBJECT";
		case SymbolType::FUNC: return "FUNC";
		case SymbolType::SECTION: return "SECTION";
		case SymbolType::FILE: return "FILE";
		case SymbolType::COMMON: return "COMMON";
		case SymbolType::TLS: return "TLS";
		case SymbolType::NUM: return "NUM";
		case SymbolType::GNU_IFUNC: return "GNU_IFUNC";
	}
	return "ERROR";
}

static const char* symbol_visibility_to_string(SymbolVisibility visibility)
{
	switch(visibility) {
		case SymbolVisibility::DEFAULT: return "DEFAULT";
		case SymbolVisibility::INTERNAL: return "INTERNAL";
		case SymbolVisibility::HIDDEN: return "HIDDEN";
		case SymbolVisibility::PROTECTED: return "PROTECTED";
	}
	return "ERROR";
}

}
