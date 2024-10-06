// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#include "sndll.h"

#include "importer_flags.h"

namespace ccc {

CCC_PACKED_STRUCT(SNDLLHeaderCommon,
	/* 0x00 */ u32 magic;
	/* 0x04 */ u32 relocations;
	/* 0x08 */ u32 relocation_count;
	/* 0x0c */ u32 symbols;
	/* 0x10 */ u32 symbol_count;
	/* 0x14 */ u32 elf_path;
	/* 0x18 */ u32 load_func;
	/* 0x1c */ u32 unload_func;
	/* 0x20 */ u32 unknown_20;
	/* 0x24 */ u32 unknown_24;
	/* 0x28 */ u32 unknown_28;
	/* 0x2c */ u32 file_size;
	/* 0x30 */ u32 unknown_30;
)

CCC_PACKED_STRUCT(SNDLLHeaderV1,
	/* 0x00 */ SNDLLHeaderCommon common;
)

CCC_PACKED_STRUCT(SNDLLHeaderV2,
	/* 0x00 */ SNDLLHeaderCommon common;
	/* 0x34 */ u32 unknown_34;
	/* 0x38 */ u32 unknown_38;
)

CCC_PACKED_STRUCT(SNDLLRelocation,
	/* 0x0 */ u32 unknown_0;
	/* 0x4 */ u32 unknown_4;
	/* 0x8 */ u32 unknown_8;
)

CCC_PACKED_STRUCT(SNDLLSymbolHeader,
	/* 0x0 */ u32 string;
	/* 0x4 */ u32 value;
	/* 0x8 */ u8 unknown_8;
	/* 0x9 */ u8 unknown_9;
	/* 0xa */ SNDLLSymbolType type;
	/* 0xb */ u8 processed;
)

static Result<SNDLLFile> parse_sndll_common(
	std::span<const u8> image, Address address, SNDLLType type, const SNDLLHeaderCommon& common, SNDLLVersion version);
static const char* sndll_symbol_type_to_string(SNDLLSymbolType type);

Result<SNDLLFile> parse_sndll_file(std::span<const u8> image, Address address, SNDLLType type)
{
	const u32* magic = get_packed<u32>(image, 0);
	CCC_CHECK((*magic & 0xffffff) == CCC_FOURCC("SNR\00"), "Not a SNDLL %s.", address.valid() ? "section" : "file");
	
	char version = *magic >> 24;
	switch(version) {
		case '1': {
			const SNDLLHeaderV1* header = get_packed<SNDLLHeaderV1>(image, 0);
			CCC_CHECK(header, "File too small to contain SNDLL V1 header.");
			return parse_sndll_common(image, address, type, header->common, SNDLL_V1);
		}
		case '2': {
			const SNDLLHeaderV2* header = get_packed<SNDLLHeaderV2>(image, 0);
			CCC_CHECK(header, "File too small to contain SNDLL V2 header.");
			return parse_sndll_common(image, address, type, header->common, SNDLL_V2);
		}
	}
	
	return CCC_FAILURE("Unknown SNDLL version '%c'.", version);
}

static Result<SNDLLFile> parse_sndll_common(
	std::span<const u8> image, Address address, SNDLLType type, const SNDLLHeaderCommon& common, SNDLLVersion version)
{
	SNDLLFile sndll;
	
	sndll.address = address;
	sndll.type = type;
	sndll.version = version;
	
	if(common.elf_path) {
		const char* elf_path = get_string(image, common.elf_path);
		if(elf_path) {
			sndll.elf_path = elf_path;
		}
	}
	
	CCC_CHECK(common.symbol_count < (32 * 1024 * 1024) / sizeof(SNDLLSymbol), "SNDLL symbol count is too high.");
	sndll.symbols.reserve(common.symbol_count);
	
	for(u32 i = 0; i < common.symbol_count; i++) {
		u32 symbol_offset = common.symbols - address.get_or_zero() + i * sizeof(SNDLLSymbolHeader);
		const SNDLLSymbolHeader* symbol_header = get_packed<SNDLLSymbolHeader>(image, symbol_offset);
		CCC_CHECK(symbol_header, "SNDLL symbol out of range.");
		
		const char* string = nullptr;
		if(symbol_header->string) {
			string = get_string(image, symbol_header->string - address.get_or_zero());
		}
		
		SNDLLSymbol& symbol = sndll.symbols.emplace_back();
		symbol.type = symbol_header->type;
		symbol.value = symbol_header->value;
		symbol.string = string;
	}
	
	return sndll;
}

Result<void> import_sndll_symbols(
	SymbolDatabase& database,
	const SNDLLFile& sndll,
	const SymbolGroup& group,
	u32 importer_flags,
	DemanglerFunctions demangler)
{
	for(const SNDLLSymbol& symbol : sndll.symbols) {
		if(symbol.value == 0 || symbol.string.empty()) {
			continue;
		}
		
		u32 address = symbol.value;
		if(symbol.type != SNDLL_ABSOLUTE && sndll.type == SNDLLType::DYNAMIC_LIBRARY) {
			address += sndll.address.get_or_zero();
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
		
		const Section* section = database.sections.symbol_overlapping_address(address);
		if(section) {
			if(section->contains_code()) {
				Result<Function*> function = database.functions.create_symbol(
					symbol.string, group.source, group.module_symbol, address, importer_flags, demangler);
				CCC_RETURN_IF_ERROR(function);
				continue;
			} else if(section->contains_data()) {
				Result<GlobalVariable*> global_variable = database.global_variables.create_symbol(
					symbol.string, group.source, group.module_symbol, address, importer_flags, demangler);
				CCC_RETURN_IF_ERROR(global_variable);
				continue;
			}
		}
		
		Result<Label*> label = database.labels.create_symbol(
			symbol.string, group.source, group.module_symbol, address, importer_flags, demangler);
		CCC_RETURN_IF_ERROR(label);
	}
	
	return Result<void>();
}

void print_sndll_symbols(FILE* out, const SNDLLFile& sndll)
{
	fprintf(out, "SNDLL SYMBOLS:\n");
	for(const SNDLLSymbol& symbol : sndll.symbols) {
		const char* type = sndll_symbol_type_to_string(symbol.type);
		const char* string = !symbol.string.empty() ? symbol.string.c_str() : "(no string)";
		fprintf(out, "%8s %08x %s\n", type, symbol.value, string);
	}
}

static const char* sndll_symbol_type_to_string(SNDLLSymbolType type)
{
	switch(type) {
		case SNDLL_NIL: return "NIL";
		case SNDLL_EXTERNAL: return "EXTERNAL";
		case SNDLL_RELATIVE: return "RELATIVE";
		case SNDLL_WEAK: return "WEAK";
		case SNDLL_ABSOLUTE: return "ABSOLUTE";
	}
	return "invalid";
}

}
