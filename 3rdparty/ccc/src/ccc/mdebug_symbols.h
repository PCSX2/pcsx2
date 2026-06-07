// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "util.h"
#include "stabs.h"
#include "mdebug_section.h"

namespace ccc::mdebug {

enum class ParsedSymbolType {
	NAME_COLON_TYPE,
	SOURCE_FILE,
	SUB_SOURCE_FILE,
	LBRAC,
	RBRAC,
	FUNCTION_END,
	NON_STABS
};

struct ParsedSymbol {
	ParsedSymbolType type;
	const mdebug::Symbol* raw;
	StabsSymbol name_colon_type;
	bool duplicate = false;
	bool is_typedef = false;
};

Result<std::vector<ParsedSymbol>> parse_symbols(const std::vector<mdebug::Symbol>& input, u32& importer_flags);

}
