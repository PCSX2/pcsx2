// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "symbol_database.h"

namespace ccc {

enum class SNDLLType {
	SNDATA_SECTION,
	DYNAMIC_LIBRARY
};

enum SNDLLVersion {
	SNDLL_V1,
	SNDLL_V2
};

enum SNDLLSymbolType : u8 {
	SNDLL_NIL = 0, // I think this is just so that the first real symbol has an index of 1.
	SNDLL_EXTERNAL = 1, // Symbol with an empty value, to be filled in from another module.
	SNDLL_RELATIVE = 2, // Global symbol, value is relative to the start of the SNDLL file.
	SNDLL_WEAK = 3, // Weak symbol, value is relative to the start of the SNDLL file.
	SNDLL_ABSOLUTE = 4 // Global symbol, value is an absolute address.
};

struct SNDLLSymbol {
	SNDLLSymbolType type = SNDLL_NIL;
	u32 value = 0;
	std::string string;
};

struct SNDLLFile {
	Address address;
	SNDLLType type;
	SNDLLVersion version;
	std::string elf_path;
	std::vector<SNDLLSymbol> symbols;
};

// If a valid address is passed, the pointers in the header will be treated as
// addresses, otherwise they will be treated as file offsets.
Result<SNDLLFile> parse_sndll_file(std::span<const u8> image, Address address, SNDLLType type);

Result<void> import_sndll_symbols(
	SymbolDatabase& database,
	const SNDLLFile& sndll,
	const SymbolGroup& group,
	u32 importer_flags,
	DemanglerFunctions demangler);

void print_sndll_symbols(FILE* out, const SNDLLFile& sndll);

}
