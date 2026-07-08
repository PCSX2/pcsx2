// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "util.h"

namespace ccc {

enum ImporterFlags {
	NO_IMPORTER_FLAGS = 0,
	DEMANGLE_PARAMETERS = (1 << 0),
	DEMANGLE_RETURN_TYPE = (1 << 1),
	DONT_DEDUPLICATE_SYMBOLS = (1 << 2),
	DONT_DEDUPLICATE_TYPES = (1 << 3),
	DONT_DEMANGLE_NAMES = (1 << 4),
	INCLUDE_GENERATED_MEMBER_FUNCTIONS = (1 << 5),
	NO_ACCESS_SPECIFIERS = (1 << 6),
	NO_MEMBER_FUNCTIONS = (1 << 7),
	NO_OPTIMIZED_OUT_FUNCTIONS = (1 << 8),
	STRICT_PARSING = (1 << 9),
	TYPEDEF_ALL_ENUMS = (1 << 10),
	TYPEDEF_ALL_STRUCTS = (1 << 11),
	TYPEDEF_ALL_UNIONS = (1 << 12),
	UNIQUE_FUNCTIONS = (1 << 13)
};

struct ImporterFlagInfo {
	ImporterFlags flag;
	const char* argument;
	std::vector<const char*> help_text;
};

extern const std::vector<ImporterFlagInfo> IMPORTER_FLAGS;

u32 parse_importer_flag(const char* argument);
void print_importer_flags_help(FILE* out);

}
