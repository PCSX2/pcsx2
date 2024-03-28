// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <atomic>

#include "mdebug_analysis.h"
#include "mdebug_section.h"
#include "symbol_database.h"

namespace ccc::mdebug {

// Perform all the main analysis passes on the mdebug symbol table and convert
// it to a set of C++ ASTs.
Result<void> import_symbol_table(
	SymbolDatabase& database,
	std::span<const u8> elf,
	s32 section_offset,
	const SymbolGroup& group,
	u32 importer_flags,
	const DemanglerFunctions& demangler,
	const std::atomic_bool* interrupt);
Result<void> import_files(SymbolDatabase& database, const AnalysisContext& context, const std::atomic_bool* interrupt);
Result<void> import_file(SymbolDatabase& database, const mdebug::File& input, const AnalysisContext& context);

// Try to add pointers from member function declarations to their definitions
// using a heuristic.
void fill_in_pointers_to_member_function_definitions(SymbolDatabase& database);

}
