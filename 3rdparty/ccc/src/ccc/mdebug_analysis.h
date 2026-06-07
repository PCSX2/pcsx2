// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "importer_flags.h"
#include "mdebug_section.h"
#include "mdebug_symbols.h"
#include "stabs.h"
#include "stabs_to_ast.h"
#include "symbol_database.h"

namespace ccc::mdebug {
	
struct AnalysisContext {
	const mdebug::SymbolTableReader* reader = nullptr;
	const std::map<u32, const mdebug::Symbol*>* external_functions = nullptr;
	const std::map<std::string, const mdebug::Symbol*>* external_globals = nullptr;
	SymbolGroup group;
	u32 importer_flags = NO_IMPORTER_FLAGS;
	DemanglerFunctions demangler;
};

class LocalSymbolTableAnalyser {
public:
	LocalSymbolTableAnalyser(SymbolDatabase& database, const StabsToAstState& stabs_to_ast_state, const AnalysisContext& context, SourceFile& source_file)
		: m_database(database)
		, m_context(context)
		, m_stabs_to_ast_state(stabs_to_ast_state)
		, m_source_file(source_file) {}
	
	// Functions for processing individual symbols.
	//
	// In most cases these symbols will appear in the following order:
	//   PROC TEXT
	//   ... line numbers ... ($LM<N>)
	//   END TEXT
	//   LABEL TEXT FUN
	//   ... parameters ...
	//   ... blocks ... (... local variables ... LBRAC ... subblocks ... RBRAC)
	//   NIL NIL FUN
	//
	// For some compiler versions the symbols can appear in this order:
	//   LABEL TEXT FUN
	//   ... parameters ...
	//   first line number ($LM1)
	//   PROC TEXT
	//   ... line numbers ... ($LM<N>)
	//   END TEXT
	//   ... blocks ... (... local variables ... LBRAC ... subblocks ... RBRAC)
	Result<void> stab_magic(const char* magic);
	Result<void> source_file(const char* path, Address text_address);
	Result<void> data_type(const ParsedSymbol& symbol);
	Result<void> global_variable(
		const char* mangled_name, Address address, const StabsType& type, bool is_static, GlobalStorageLocation location);
	Result<void> sub_source_file(const char* name, Address text_address);
	Result<void> procedure(
		const char* mangled_name, Address address, const ProcedureDescriptor* procedure_descriptor, bool is_static);
	Result<void> label(const char* label, Address address, s32 line_number);
	Result<void> text_end(const char* name, s32 function_size);
	Result<void> function(const char* mangled_name, const StabsType& return_type, Address address);
	Result<void> function_end();
	Result<void> parameter(
		const char* name, const StabsType& type, bool is_stack, s32 value, bool is_by_reference);
	Result<void> local_variable(
		const char* name, const StabsType& type, u32 value, StabsSymbolDescriptor desc, SymbolClass sclass);
	Result<void> lbrac(s32 begin_offset);
	Result<void> rbrac(s32 end_offset);
	
	Result<void> finish();
	
	Result<void> create_function(const char* mangled_name, Address address);
	
protected:
	enum AnalysisState {
		NOT_IN_FUNCTION,
		IN_FUNCTION_BEGINNING,
		IN_FUNCTION_END
	};
	
	SymbolDatabase& m_database;
	const AnalysisContext& m_context;
	const StabsToAstState& m_stabs_to_ast_state;
	
	AnalysisState m_state = NOT_IN_FUNCTION;
	SourceFile& m_source_file;
	std::vector<FunctionHandle> m_functions;
	std::vector<GlobalVariableHandle> m_global_variables;
	Function* m_current_function = nullptr;
	std::vector<ParameterVariableHandle> m_current_parameter_variables;
	std::vector<LocalVariableHandle> m_current_local_variables;
	std::vector<std::vector<LocalVariableHandle>> m_blocks;
	std::vector<LocalVariableHandle> m_pending_local_variables;
	std::string m_next_relative_path;
};

std::optional<GlobalStorageLocation> symbol_class_to_global_variable_location(SymbolClass symbol_class);

};
