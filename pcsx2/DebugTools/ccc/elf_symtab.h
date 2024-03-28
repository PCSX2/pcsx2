// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
