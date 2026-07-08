// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "symbol_database.h"

namespace ccc::elf {

Result<void> import_symbols(
	SymbolDatabase& database,
	const SymbolGroup& group,
	std::span<const u8> symtab,
	std::span<const u8> strtab,
	u32 importer_flags,
	DemanglerFunctions demangler);
	
Result<void> print_symbol_table(FILE* out, std::span<const u8> symtab, std::span<const u8> strtab);

}
