// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "mdebug_importer.h"

namespace ccc::mdebug {

static Result<void> resolve_type_names(
	SymbolDatabase& database, const SymbolGroup& group, u32 importer_flags);
static Result<void> resolve_type_name(
	ast::TypeName& type_name,
	SymbolDatabase& database,
	const SymbolGroup& group,
	u32 importer_flags);
static void compute_size_bytes(ast::Node& node, SymbolDatabase& database);

Result<void> import_symbol_table(
	SymbolDatabase& database,
	std::span<const u8> elf,
	s32 section_offset,
	const SymbolGroup& group,
	u32 importer_flags,
	const DemanglerFunctions& demangler,
	const std::atomic_bool* interrupt)
{
	SymbolTableReader reader;
	
	Result<void> reader_result = reader.init(elf, section_offset);
	CCC_RETURN_IF_ERROR(reader_result);
	
	Result<std::vector<mdebug::Symbol>> external_symbols = reader.parse_external_symbols();
	CCC_RETURN_IF_ERROR(external_symbols);
	
	// The addresses of the global variables aren't present in the local symbol
	// table, so here we extract them from the external table.
	std::map<std::string, const mdebug::Symbol*> globals;
	for(const mdebug::Symbol& external : *external_symbols) {
		if(external.symbol_type == mdebug::SymbolType::GLOBAL
			&& (external.symbol_class != mdebug::SymbolClass::UNDEFINED)) {
			globals[external.string] = &external;
		}
	}
	
	// Bundle together some unchanging state to pass to import_files.
	AnalysisContext context;
	context.reader = &reader;
	context.globals = &globals;
	context.group = group;
	context.importer_flags = importer_flags;
	context.demangler = demangler;
	
	Result<void> result = import_files(database, context, interrupt);
	CCC_RETURN_IF_ERROR(result);
	
	return Result<void>();
}

Result<void> import_files(SymbolDatabase& database, const AnalysisContext& context, const std::atomic_bool* interrupt)
{
	Result<s32> file_count = context.reader->file_count();
	CCC_RETURN_IF_ERROR(file_count);
	
	for(s32 i = 0; i < *file_count; i++) {
		if(interrupt && *interrupt) {
			return CCC_FAILURE("Operation interrupted by user.");
		}
		
		Result<mdebug::File> file = context.reader->parse_file(i);
		CCC_RETURN_IF_ERROR(file);
		
		Result<void> result = import_file(database, *file, context);
		CCC_RETURN_IF_ERROR(result);
	}
	
	// The files field may be modified by further analysis passes, so we
	// need to save this information here.
	for(DataType& data_type : database.data_types) {
		if(context.group.is_in_group(data_type) && data_type.files.size() == 1) {
			data_type.only_defined_in_single_translation_unit = true;
		}
	}
	
	// Lookup data types and store data type handles in type names.
	Result<void> type_name_result = resolve_type_names(database, context.group, context.importer_flags);
	CCC_RETURN_IF_ERROR(type_name_result);
	
	// Compute the size in bytes of all the AST nodes.
	database.for_each_symbol([&](ccc::Symbol& symbol) {
		if(context.group.is_in_group(symbol) && symbol.type()) {
			compute_size_bytes(*symbol.type(), database);
		}
	});
	
	// Propagate the size information to the global variable symbols.
	for(GlobalVariable& global_variable : database.global_variables) {
		if(global_variable.type() && global_variable.type()->size_bytes > -1) {
			global_variable.set_size((u32) global_variable.type()->size_bytes);
		}
	}
	
	// Propagate the size information to the static local variable symbols.
	for(LocalVariable& local_variable : database.local_variables) {
		bool is_static_local = std::holds_alternative<GlobalStorage>(local_variable.storage);
		if(is_static_local && local_variable.type() && local_variable.type()->size_bytes > -1) {
			local_variable.set_size((u32) local_variable.type()->size_bytes);
		}
	}
	
	return Result<void>();
}

Result<void> import_file(SymbolDatabase& database, const mdebug::File& input, const AnalysisContext& context)
{
	// Parse the stab strings into a data structure that's vaguely
	// one-to-one with the text-based representation.
	u32 importer_flags_for_this_file = context.importer_flags;
	Result<std::vector<ParsedSymbol>> symbols = parse_symbols(input.symbols, importer_flags_for_this_file);
	CCC_RETURN_IF_ERROR(symbols);
	
	// In stabs, types can be referenced by their number from other stabs,
	// so here we build a map of type numbers to the parsed types.
	std::map<StabsTypeNumber, const StabsType*> stabs_types;
	for(const ParsedSymbol& symbol : *symbols) {
		if(symbol.type == ParsedSymbolType::NAME_COLON_TYPE) {
			symbol.name_colon_type.type->enumerate_numbered_types(stabs_types);
		}
	}
	
	Result<SourceFile*> source_file = database.source_files.create_symbol(
		input.full_path, input.address, context.group.source, context.module_symbol);
	CCC_RETURN_IF_ERROR(source_file);
	
	(*source_file)->working_dir = input.working_dir;
	(*source_file)->command_line_path = input.command_line_path;
	
	// Sometimes the INFO symbols contain information about what toolchain
	// version was used for building the executable.
	for(const mdebug::Symbol& symbol : input.symbols) {
		if(symbol.symbol_class == mdebug::SymbolClass::INFO && strcmp(symbol.string, "@stabs") != 0) {
			(*source_file)->toolchain_version_info.emplace(symbol.string);
		}
	}
	
	StabsToAstState stabs_to_ast_state;
	stabs_to_ast_state.file_handle = (*source_file)->handle().value;
	stabs_to_ast_state.stabs_types = &stabs_types;
	stabs_to_ast_state.importer_flags = importer_flags_for_this_file;
	stabs_to_ast_state.demangler = context.demangler;
	
	// Convert the parsed stabs symbols to a more standard C AST.
	LocalSymbolTableAnalyser analyser(database, stabs_to_ast_state, context, **source_file);
	for(const ParsedSymbol& symbol : *symbols) {
		if(symbol.duplicate) {
			continue;
		}
		
		switch(symbol.type) {
			case ParsedSymbolType::NAME_COLON_TYPE: {
				switch(symbol.name_colon_type.descriptor) {
					case StabsSymbolDescriptor::LOCAL_FUNCTION:
					case StabsSymbolDescriptor::GLOBAL_FUNCTION: {
						const char* name = symbol.name_colon_type.name.c_str();
						const StabsType& type = *symbol.name_colon_type.type.get();
						Result<void> result = analyser.function(name, type, symbol.raw->value);
						CCC_RETURN_IF_ERROR(result);
						break;
					}
					case StabsSymbolDescriptor::REFERENCE_PARAMETER_A:
					case StabsSymbolDescriptor::REGISTER_PARAMETER:
					case StabsSymbolDescriptor::VALUE_PARAMETER:
					case StabsSymbolDescriptor::REFERENCE_PARAMETER_V: {
						const char* name = symbol.name_colon_type.name.c_str();
						const StabsType& type = *symbol.name_colon_type.type.get();
						bool is_stack_variable = symbol.name_colon_type.descriptor == StabsSymbolDescriptor::VALUE_PARAMETER;
						bool is_by_reference = symbol.name_colon_type.descriptor == StabsSymbolDescriptor::REFERENCE_PARAMETER_A
							|| symbol.name_colon_type.descriptor == StabsSymbolDescriptor::REFERENCE_PARAMETER_V;
						
						Result<void> result = analyser.parameter(name, type, is_stack_variable, symbol.raw->value, is_by_reference);
						CCC_RETURN_IF_ERROR(result);
						break;
					}
					case StabsSymbolDescriptor::REGISTER_VARIABLE:
					case StabsSymbolDescriptor::LOCAL_VARIABLE:
					case StabsSymbolDescriptor::STATIC_LOCAL_VARIABLE: {
						const char* name = symbol.name_colon_type.name.c_str();
						const StabsType& type = *symbol.name_colon_type.type.get();
						Result<void> result = analyser.local_variable(
							name, type, symbol.raw->value, symbol.name_colon_type.descriptor, symbol.raw->symbol_class);
						CCC_RETURN_IF_ERROR(result);
						break;
					}
					case StabsSymbolDescriptor::GLOBAL_VARIABLE:
					case StabsSymbolDescriptor::STATIC_GLOBAL_VARIABLE: {
						const char* name = symbol.name_colon_type.name.c_str();
						u32 address = -1;
						std::optional<GlobalStorageLocation> location =
							symbol_class_to_global_variable_location(symbol.raw->symbol_class);
						if(symbol.name_colon_type.descriptor == StabsSymbolDescriptor::GLOBAL_VARIABLE) {
							// The address for non-static global variables is
							// only stored in the external symbol table (and
							// the ELF symbol table), so we pull that
							// information in here.
							if(context.globals) {
								auto global_symbol = context.globals->find(symbol.name_colon_type.name);
								if(global_symbol != context.globals->end()) {
									address = (u32) global_symbol->second->value;
									location = symbol_class_to_global_variable_location(global_symbol->second->symbol_class);
								}
							}
						} else {
							// And for static global variables it's just stored
							// in the local symbol table.
							address = (u32) symbol.raw->value;
						}
						CCC_CHECK(location.has_value(), "Invalid global variable location.")
						const StabsType& type = *symbol.name_colon_type.type.get();
						bool is_static = symbol.name_colon_type.descriptor == StabsSymbolDescriptor::STATIC_GLOBAL_VARIABLE;
						Result<void> result = analyser.global_variable(name, address, type, is_static, *location);
						CCC_RETURN_IF_ERROR(result);
						break;
					}
					case StabsSymbolDescriptor::TYPE_NAME:
					case StabsSymbolDescriptor::ENUM_STRUCT_OR_TYPE_TAG: {
						Result<void> result = analyser.data_type(symbol);
						CCC_RETURN_IF_ERROR(result);
						break;
					}
				}
				break;
			}
			case ParsedSymbolType::SOURCE_FILE: {
				Result<void> result = analyser.source_file(symbol.raw->string, symbol.raw->value);
				CCC_RETURN_IF_ERROR(result);
				break;
			}
			case ParsedSymbolType::SUB_SOURCE_FILE: {
				Result<void> result = analyser.sub_source_file(symbol.raw->string, symbol.raw->value);
				CCC_RETURN_IF_ERROR(result);
				break;
			}
			case ParsedSymbolType::LBRAC: {
				Result<void> result = analyser.lbrac(symbol.raw->value);
				CCC_RETURN_IF_ERROR(result);
				break;
			}
			case ParsedSymbolType::RBRAC: {
				Result<void> result = analyser.rbrac(symbol.raw->value);
				CCC_RETURN_IF_ERROR(result);
				break;
			}
			case ParsedSymbolType::FUNCTION_END: {
				Result<void> result = analyser.function_end();
				CCC_RETURN_IF_ERROR(result);
				break;
			}
			case ParsedSymbolType::NON_STABS: {
				if(symbol.raw->symbol_class == mdebug::SymbolClass::TEXT) {
					if(symbol.raw->symbol_type == mdebug::SymbolType::PROC) {
						Result<void> result = analyser.procedure(symbol.raw->string, symbol.raw->value, false);
						CCC_RETURN_IF_ERROR(result);
					} else if(symbol.raw->symbol_type == mdebug::SymbolType::STATICPROC) {
						Result<void> result = analyser.procedure(symbol.raw->string, symbol.raw->value, true);
						CCC_RETURN_IF_ERROR(result);
					} else if(symbol.raw->symbol_type == mdebug::SymbolType::LABEL) {
						Result<void> result = analyser.label(symbol.raw->string, symbol.raw->value, symbol.raw->index);
						CCC_RETURN_IF_ERROR(result);
					} else if(symbol.raw->symbol_type == mdebug::SymbolType::END) {
						Result<void> result = analyser.text_end(symbol.raw->string, symbol.raw->value);
						CCC_RETURN_IF_ERROR(result);
					}
				}
				break;
			}
		}
	}
	
	Result<void> result = analyser.finish();
	CCC_RETURN_IF_ERROR(result);
	
	return Result<void>();
}

static Result<void> resolve_type_names(
	SymbolDatabase& database, const SymbolGroup& group, u32 importer_flags)
{
	Result<void> result;
	database.for_each_symbol([&](ccc::Symbol& symbol) {
		if(group.is_in_group(symbol) && symbol.type()) {
			ast::for_each_node(*symbol.type(), ast::PREORDER_TRAVERSAL, [&](ast::Node& node) {
				if(node.descriptor == ast::TYPE_NAME) {
					Result<void> type_name_result = resolve_type_name(node.as<ast::TypeName>(), database, group, importer_flags);
					if(!type_name_result.success()) {
						result = std::move(type_name_result);
					}
				}
				return ast::EXPLORE_CHILDREN;
			});
		}
	});
	return result;
}

static Result<void> resolve_type_name(
	ast::TypeName& type_name,
	SymbolDatabase& database,
	const SymbolGroup& group,
	u32 importer_flags)
{
	ast::TypeName::UnresolvedStabs* unresolved_stabs = type_name.unresolved_stabs.get();
	if(!unresolved_stabs) {
		return Result<void>();
	}
	
	// Lookup the type by its STABS type number. This path ensures that the
	// correct type is found even if multiple types have the same name.
	if(unresolved_stabs->referenced_file_handle != (u32) -1 && unresolved_stabs->stabs_type_number.valid()) {
		const SourceFile* source_file = database.source_files.symbol_from_handle(unresolved_stabs->referenced_file_handle);
		CCC_ASSERT(source_file);
		auto handle = source_file->stabs_type_number_to_handle.find(unresolved_stabs->stabs_type_number);
		if(handle != source_file->stabs_type_number_to_handle.end()) {
			type_name.data_type_handle = handle->second.value;
			type_name.is_forward_declared = false;
			type_name.unresolved_stabs.reset();
			return Result<void>();
		}
	}
	
	// Looking up the type by its STABS type number failed, so look for it by
	// its name instead. This happens when a type is forward declared but not
	// defined in a given translation unit.
	if(!unresolved_stabs->type_name.empty()) {
		for(auto& name_handle : database.data_types.handles_from_name(unresolved_stabs->type_name)) {
			DataType* data_type = database.data_types.symbol_from_handle(name_handle.second);
			if(data_type && group.is_in_group(*data_type)) {
				type_name.data_type_handle = name_handle.second.value;
				type_name.is_forward_declared = true;
				type_name.unresolved_stabs.reset();
				return Result<void>();
			}
		}
	}
	
	// If this branch is taken it means the type name was probably from an
	// automatically generated member function of a nested struct trying to
	// reference the struct (for the this parameter). We shouldn't create a
	// forward declared type in this case.
	if(type_name.source == ast::TypeNameSource::UNNAMED_THIS) {
		return Result<void>();
	}
	
	// Type lookup failed. This happens when a type is forward declared in a
	// translation unit with symbols but is not defined in one. We haven't
	// already created a forward declared type, so we create one now.
	std::unique_ptr<ast::Node> forward_declared_node;
	if(unresolved_stabs->type.has_value()) {
		switch(*unresolved_stabs->type) {
			case ast::ForwardDeclaredType::STRUCT: {
				std::unique_ptr<ast::StructOrUnion> node = std::make_unique<ast::StructOrUnion>();
				node->is_struct = true;
				forward_declared_node = std::move(node);
				break;
			}
			case ast::ForwardDeclaredType::UNION: {
				std::unique_ptr<ast::StructOrUnion> node = std::make_unique<ast::StructOrUnion>();
				node->is_struct = false;
				forward_declared_node = std::move(node);
				break;
			}
			case ast::ForwardDeclaredType::ENUM: {
				std::unique_ptr<ast::Enum> node = std::make_unique<ast::Enum>();
				forward_declared_node = std::move(node);
				break;
			}
		}
	}
	
	if(forward_declared_node) {
		Result<DataType*> forward_declared_type = database.data_types.create_symbol(
			unresolved_stabs->type_name, group.source, group.module_symbol);
		CCC_RETURN_IF_ERROR(forward_declared_type);
		
		(*forward_declared_type)->set_type(std::move(forward_declared_node));
		(*forward_declared_type)->not_defined_in_any_translation_unit = true;
		
		type_name.data_type_handle = (*forward_declared_type)->handle().value;
		type_name.is_forward_declared = true;
		type_name.unresolved_stabs.reset();
		
		return Result<void>();
	}
	
	const char* error_message = "Unresolved %s type name '%s' with STABS type number (%d,%d).";
	if(importer_flags & STRICT_PARSING) {
		return CCC_FAILURE(error_message,
			ast::type_name_source_to_string(type_name.source),
			type_name.unresolved_stabs->type_name.c_str(),
			type_name.unresolved_stabs->stabs_type_number.file,
			type_name.unresolved_stabs->stabs_type_number.type);
	} else {
		CCC_WARN(error_message,
			ast::type_name_source_to_string(type_name.source),
			type_name.unresolved_stabs->type_name.c_str(),
			type_name.unresolved_stabs->stabs_type_number.file,
			type_name.unresolved_stabs->stabs_type_number.type);
	}
	
	return Result<void>();
}

static void compute_size_bytes(ast::Node& node, SymbolDatabase& database)
{
	for_each_node(node, ast::POSTORDER_TRAVERSAL, [&](ast::Node& node) {
		// Skip nodes that have already been processed.
		if(node.size_bytes > -1 || node.cannot_compute_size) {
			return ast::EXPLORE_CHILDREN;
		}
		
		// Can't compute size recursively.
		node.cannot_compute_size = true;
		
		switch(node.descriptor) {
			case ast::ARRAY: {
				ast::Array& array = node.as<ast::Array>();
				if(array.element_type->size_bytes > -1) {
					array.size_bytes = array.element_type->size_bytes * array.element_count;
				}
				break;
			}
			case ast::BITFIELD: {
				break;
			}
			case ast::BUILTIN: {
				ast::BuiltIn& built_in = node.as<ast::BuiltIn>();
				built_in.size_bytes = builtin_class_size(built_in.bclass);
				break;
			}
			case ast::FUNCTION: {
				break;
			}
			case ast::ENUM: {
				node.size_bytes = 4;
				break;
			}
			case ast::ERROR_NODE: {
				break;
			}
			case ast::STRUCT_OR_UNION: {
				node.size_bytes = node.size_bits / 8;
				break;
			}
			case ast::POINTER_OR_REFERENCE: {
				node.size_bytes = 4;
				break;
			}
			case ast::POINTER_TO_DATA_MEMBER: {
				break;
			}
			case ast::TYPE_NAME: {
				ast::TypeName& type_name = node.as<ast::TypeName>();
				DataType* resolved_type = database.data_types.symbol_from_handle(type_name.data_type_handle_unless_forward_declared());
				if(resolved_type) {
					ast::Node* resolved_node = resolved_type->type();
					CCC_ASSERT(resolved_node);
					if(resolved_node->size_bytes < 0 && !resolved_node->cannot_compute_size) {
						compute_size_bytes(*resolved_node, database);
					}
					type_name.size_bytes = resolved_node->size_bytes;
				}
				break;
			}
		}
		
		if(node.size_bytes > -1) {
			node.cannot_compute_size = false;
		}
		
		return ast::EXPLORE_CHILDREN;
	});
}

void fill_in_pointers_to_member_function_definitions(SymbolDatabase& database)
{
	// Fill in pointers from member function declaration to corresponding definitions.
	for(Function& function : database.functions) {
		const std::string& qualified_name = function.name();
		std::string::size_type name_separator_pos = qualified_name.find_last_of("::");
		if(name_separator_pos == std::string::npos || name_separator_pos < 2) {
			continue;
		}
			
		std::string function_name = qualified_name.substr(name_separator_pos + 1);
		
		// This won't work for some template types.
		std::string::size_type type_separator_pos = qualified_name.find_last_of("::", name_separator_pos - 2);
		std::string type_name;
		if(type_separator_pos != std::string::npos) {
			type_name = qualified_name.substr(type_separator_pos + 1, name_separator_pos - type_separator_pos - 2);
		} else {
			type_name = qualified_name.substr(0, name_separator_pos - 1);
		}
		
		for(const auto& name_handle : database.data_types.handles_from_name(type_name)) {
			DataType* data_type = database.data_types.symbol_from_handle(name_handle.second);
			if(!data_type || !data_type->type() || data_type->type()->descriptor != ast::STRUCT_OR_UNION) {
				continue;
			}
				
			ast::StructOrUnion& struct_or_union = data_type->type()->as<ast::StructOrUnion>();
			for(std::unique_ptr<ast::Node>& declaration : struct_or_union.member_functions) {
				if(declaration->name == function_name) {
					declaration->as<ast::Function>().definition_handle = function.handle().value;
					function.is_member_function_ish = true;
					break;
				}
			}
			
			if(function.is_member_function_ish) {
				break;
			}
		}
	}
}

}
