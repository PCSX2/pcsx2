// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ast.h"
#include "stabs.h"

namespace ccc {
	
struct StabsToAstState {
	u32 file_handle;
	std::map<StabsTypeNumber, const StabsType*>* stabs_types;
	u32 importer_flags;
	DemanglerFunctions demangler;
};

Result<std::unique_ptr<ast::Node>> stabs_type_to_ast(
	const StabsType& type,
	const StabsType* enclosing_struct,
	const StabsToAstState& state,
	s32 depth,
	bool substitute_type_name,
	bool force_substitute);
void fix_recursively_emitted_structures(
	ast::StructOrUnion& outer_struct, const std::string& name, StabsTypeNumber type_number, SourceFileHandle file_handle);
ast::AccessSpecifier stabs_field_visibility_to_access_specifier(StabsStructOrUnionType::Visibility visibility);

}
