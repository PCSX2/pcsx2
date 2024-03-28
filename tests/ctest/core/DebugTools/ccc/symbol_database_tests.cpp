// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <gtest/gtest.h>
#include "ccc/ast.h"
#include "ccc/importer_flags.h"
#include "ccc/symbol_database.h"

using namespace ccc;

TEST(CCCSymbolDatabase, SymbolFromHandle)
{
	SymbolDatabase database;
	SymbolSourceHandle handles[10];
	
	// Create the symbols.
	for(s32 i = 0; i < 10; i++) {
		Result<SymbolSource*> source = database.symbol_sources.create_symbol(std::to_string(i), SymbolSourceHandle());
		CCC_GTEST_FAIL_IF_ERROR(source);
		handles[i] = (*source)->handle();
	}
	
	// Make sure we can still look them up.
	for(s32 i = 0; i < 10; i++) {
		SymbolSource* source = database.symbol_sources.symbol_from_handle(handles[i]);
		ASSERT_TRUE(source);
		EXPECT_EQ(source->name(), std::to_string(i));
	}
}

TEST(CCCSymbolDatabase, HandlesFromAddressRange)
{
	SymbolDatabase database;
	FunctionHandle handles[20];
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	// Create the symbols.
	for(u32 address = 10; address < 20; address++) {
		Result<Function*> function = database.functions.create_symbol("", address, (*source)->handle(), nullptr);
		CCC_GTEST_FAIL_IF_ERROR(function);
		handles[address] = (*function)->handle();
	}
	
	// Try various address ranges.
	auto empty_before = database.functions.handles_from_address_range(AddressRange(0, 10));
	EXPECT_EQ(empty_before.begin(), empty_before.end());
	
	auto empty_after = database.functions.handles_from_address_range(AddressRange(21, 30));
	EXPECT_EQ(empty_after.begin(), empty_after.end());
	
	auto empty_before_open = database.functions.handles_from_address_range(AddressRange(Address(), 10));
	EXPECT_EQ(empty_before_open.begin(), empty_before_open.end());
	
	auto empty_after_open = database.functions.handles_from_address_range(AddressRange(21, Address()));
	EXPECT_EQ(empty_after_open.begin(), empty_after_open.end());
	
	auto last = database.functions.handles_from_address_range(AddressRange(19, 30));
	EXPECT_EQ(last.begin()->second, handles[19]);
	
	auto last_open = database.functions.handles_from_address_range(AddressRange(19, Address()));
	EXPECT_EQ(last_open.begin()->second, handles[19]);
	
	auto first_half = database.functions.handles_from_address_range(AddressRange(5, 15));
	EXPECT_EQ(first_half.begin()->second, handles[10]);
	EXPECT_EQ(first_half.end()->second, handles[15]);
	
	auto second_half = database.functions.handles_from_address_range(AddressRange(15, 25));
	EXPECT_EQ(second_half.begin()->second, handles[15]);
	EXPECT_EQ((--second_half.end())->second, handles[19]);
	
	auto whole_range = database.functions.handles_from_address_range(AddressRange(10, 20));
	EXPECT_EQ(whole_range.begin()->second, handles[10]);
	EXPECT_EQ((--whole_range.end())->second, handles[19]);
}

TEST(CCCSymbolDatabase, HandleFromStartingAddress)
{
	SymbolDatabase database;
	FunctionHandle handles[10];
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	// Create the symbols.
	for(u32 address = 0; address < 10; address++) {
		Result<Function*> function = database.functions.create_symbol("", address, (*source)->handle(), nullptr);
		CCC_GTEST_FAIL_IF_ERROR(function);
		handles[address] = (*function)->handle();
	}
	
	// Make sure we can look them up by their address.
	for(u32 address = 0; address < 10; address++) {
		EXPECT_EQ(database.functions.first_handle_from_starting_address(address), handles[address]);
	}
}

TEST(CCCSymbolDatabase, HandlesFromName)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	// Create the symbols.
	Result<DataType*> a = database.data_types.create_symbol("A", (*source)->handle());
	Result<DataType*> b_1 = database.data_types.create_symbol("B", (*source)->handle());
	Result<DataType*> b_2 = database.data_types.create_symbol("B", (*source)->handle());
	Result<DataType*> c_1 = database.data_types.create_symbol("C", (*source)->handle());
	Result<DataType*> c_2 = database.data_types.create_symbol("C", (*source)->handle());
	Result<DataType*> c_3 = database.data_types.create_symbol("C", (*source)->handle());
	Result<DataType*> d = database.data_types.create_symbol("D", (*source)->handle());
	
	// Destroy D.
	database.data_types.mark_symbol_for_destruction((*d)->handle(), &database);
	database.destroy_marked_symbols();
	
	// Make sure we can look up A, B, and C by their names.
	auto as = database.data_types.handles_from_name("A");
	EXPECT_EQ(++as.begin(), as.end());
	
	auto bs = database.data_types.handles_from_name("B");
	EXPECT_EQ(++(++bs.begin()), bs.end());
	
	auto cs = database.data_types.handles_from_name("C");
	EXPECT_EQ(++(++(++(cs.begin()))), cs.end());
	
	// Make sure we can't look up D anymore.
	auto ds = database.data_types.handles_from_name("D");
	EXPECT_EQ(ds.begin(), ds.end());
}

static Result<FunctionHandle> create_function(SymbolDatabase& database, SymbolSourceHandle source, const char* name, Address address, u32 size)
{
	Result<Function*> function = database.functions.create_symbol("a", address, source, nullptr);
	CCC_RETURN_IF_ERROR(function);
	CCC_CHECK(*function, "*function");
	(*function)->set_size(size);
	return (*function)->handle();
}

static FunctionHandle handle_from_function(Function* function)
{
	if(function) {
		return function->handle();
	} else {
		return FunctionHandle();
	}
}

TEST(CCCSymbolDatabase, SymbolOverlappingAddress)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0)), FunctionHandle());
	
	Result<FunctionHandle> a = create_function(database, (*source)->handle(), "a", 0x1000, 0x1000);
	CCC_GTEST_FAIL_IF_ERROR(a);
	
	Result<FunctionHandle> b = create_function(database, (*source)->handle(), "b", 0x2000, 0x1500);
	CCC_GTEST_FAIL_IF_ERROR(b);
	
	Result<FunctionHandle> c = create_function(database, (*source)->handle(), "c", 0x3000, 0x1000);
	CCC_GTEST_FAIL_IF_ERROR(c);
	
	Result<FunctionHandle> d = create_function(database, (*source)->handle(), "d", 0x5000, 0x1000);
	CCC_GTEST_FAIL_IF_ERROR(d);
	
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0x0000)), FunctionHandle());
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0x1000)), *a);
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0x2000)), *b);
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0x3000)), *c);
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0x4000)), FunctionHandle());
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0x5000)), *d);
	EXPECT_EQ(handle_from_function(database.functions.symbol_overlapping_address(0x6000)), FunctionHandle());
	
}

TEST(CCCSymbolDatabase, MoveSymbol)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	Result<Function*> function = database.functions.create_symbol("func", 0x1000, (*source)->handle(), nullptr);
	CCC_GTEST_FAIL_IF_ERROR(function);
	
	EXPECT_TRUE(database.functions.move_symbol((*function)->handle(), 0x2000));
	
	EXPECT_TRUE(database.functions.first_handle_from_starting_address(0x2000).valid());
	EXPECT_FALSE(database.functions.first_handle_from_starting_address(0x1000).valid());
}

TEST(CCCSymbolDatabase, RenameSymbol)
{
	SymbolDatabase database;
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	Result<DataType*> data_type = database.data_types.create_symbol("Type1", 0x1000, (*source)->handle(), nullptr);
	EXPECT_TRUE(database.data_types.rename_symbol((*data_type)->handle(), "Type2"));
	
	auto old_handles = database.data_types.handles_from_name("Type1");
	EXPECT_EQ(old_handles.begin(), old_handles.end());
	
	auto new_handles = database.data_types.handles_from_name("Type2");
	EXPECT_NE(new_handles.begin(), new_handles.end());
}

TEST(CCCSymbolDatabase, MergeSymbolLists)
{
	SymbolList<SymbolSource> list;
	Result<SymbolSource*> source = list.create_symbol("Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	SymbolList<SymbolSource> other_list;
	Result<SymbolSource*> other_source = list.create_symbol("Other Source", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(other_source);
	
	list.merge_from(other_list);
	
	SymbolSourceHandle handle = list.first_handle_from_name("Source");
	SymbolSourceHandle other_handle = list.first_handle_from_name("Other Source");
	
	EXPECT_TRUE(handle.valid());
	EXPECT_TRUE(other_handle.valid());
	
	EXPECT_TRUE(list.symbol_from_handle(handle));
	EXPECT_TRUE(list.symbol_from_handle(other_handle));
}

TEST(CCCSymbolDatabase, DestroySymbolsDanglingHandles)
{
	SymbolDatabase database;
	SymbolSourceHandle handles[10];
	
	// Create the symbols.
	for(s32 i = 0; i < 10; i++) {
		Result<SymbolSource*> source = database.symbol_sources.create_symbol(std::to_string(i), SymbolSourceHandle());
		CCC_GTEST_FAIL_IF_ERROR(source);
		handles[i] = (*source)->handle();
	}
	
	// Destroy every other symbol.
	for(s32 i = 0; i < 10; i += 2) {
		database.symbol_sources.mark_symbol_for_destruction(handles[i], &database);
	}
	database.destroy_marked_symbols();
	
	// Make sure we can't look them up anymore.
	for(s32 i = 0; i < 10; i += 2) {
		EXPECT_FALSE(database.symbol_sources.symbol_from_handle(handles[i]));
	}
	
	// Make sure we can still lookup the other ones.
	for(s32 i = 1; i < 10; i += 2) {
		EXPECT_TRUE(database.symbol_sources.symbol_from_handle(handles[i]));
	}
}

TEST(CCCSymbolDatabase, DestroySymbolsFromSource)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> symbol_table_source = database.symbol_sources.create_symbol("Big Symbol Table", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(symbol_table_source);
	SymbolSourceHandle symbol_table_handle = (*symbol_table_source)->handle();
	
	Result<SymbolSource*> user_defined_source = database.symbol_sources.create_symbol("User Defined", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(user_defined_source);
	SymbolSourceHandle user_defined_handle = (*user_defined_source)->handle();
	
	for(s32 i = 0; i < 5; i++) {
		Result<DataType*> result = database.data_types.create_symbol("SymbolTableType", symbol_table_handle);
		CCC_GTEST_FAIL_IF_ERROR(result);
	}
	
	for(s32 i = 0; i < 5; i++) {
		Result<DataType*> result = database.data_types.create_symbol("UserDefinedType", user_defined_handle);
		CCC_GTEST_FAIL_IF_ERROR(result);
	}
	
	for(s32 i = 0; i < 5; i++) {
		Result<DataType*> result = database.data_types.create_symbol("SymbolTableType", symbol_table_handle);
		CCC_GTEST_FAIL_IF_ERROR(result);
	}
	
	for(s32 i = 0; i < 5; i++) {
		Result<DataType*> result = database.data_types.create_symbol("UserDefinedType", user_defined_handle);
		CCC_GTEST_FAIL_IF_ERROR(result);
	}
	
	// Simulate freeing a symbol table while retaining user-defined symbols.
	database.destroy_symbols_from_source(symbol_table_handle, true);
	
	s32 user_symbols_remaining = 0;
	for(const DataType& data_type : database.data_types) {
		ASSERT_TRUE(data_type.source() == user_defined_handle);
		user_symbols_remaining++;
	}
	
	EXPECT_TRUE(user_symbols_remaining == 10);
}

TEST(CCCSymbolDatabase, DestroyFunction)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Symbol Table", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	Result<Function*> function = database.functions.create_symbol("func", (*source)->handle());
	CCC_GTEST_FAIL_IF_ERROR(function);
	FunctionHandle function_handle = (*function)->handle();
	
	// Attach a parameter to the function.
	Result<ParameterVariable*> parameter_variable = database.parameter_variables.create_symbol("param", (*source)->handle());
	CCC_GTEST_FAIL_IF_ERROR(parameter_variable);
	ParameterVariableHandle parameter_handle = (*parameter_variable)->handle();
	(*function)->set_parameter_variables(std::vector<ParameterVariableHandle>{parameter_handle}, database);
	
	// Attach a local variable to the function.
	Result<LocalVariable*> local_variable = database.local_variables.create_symbol("local", (*source)->handle());
	CCC_GTEST_FAIL_IF_ERROR(local_variable);
	LocalVariableHandle local_handle = (*local_variable)->handle();
	(*function)->set_local_variables(std::vector<LocalVariableHandle>{local_handle}, database);
	
	// Destroy the function.
	EXPECT_TRUE(database.functions.mark_symbol_for_destruction(function_handle, &database));
	database.destroy_marked_symbols();
	
	// Make sure that when the function is destroyed, the variables are too.
	EXPECT_FALSE(database.parameter_variables.symbol_from_handle(parameter_handle));
	EXPECT_FALSE(database.local_variables.symbol_from_handle(local_handle));
}

TEST(CCCSymbolDatabase, DeduplicateEqualTypes)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Symbol Table", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	SymbolGroup group;
	group.source = *source;
	
	Result<SourceFile*> file = database.source_files.create_symbol("File", (*source)->handle());
	CCC_GTEST_FAIL_IF_ERROR(file);
	
	std::unique_ptr<ast::BuiltIn> first_type = std::make_unique<ast::BuiltIn>();
	Result<DataType*> first_symbol = database.create_data_type_if_unique(
		std::move(first_type), StabsTypeNumber{1,1}, "DataType", **file, group);
	CCC_GTEST_FAIL_IF_ERROR(first_symbol);
	
	std::unique_ptr<ast::BuiltIn> second_type = std::make_unique<ast::BuiltIn>();
	Result<DataType*> second_symbol = database.create_data_type_if_unique(
		std::move(second_type), StabsTypeNumber{1,2}, "DataType", **file, group);
	CCC_GTEST_FAIL_IF_ERROR(second_symbol);
	
	EXPECT_EQ(database.data_types.size(), 1);
}

TEST(CCCSymbolDatabase, DeduplicateWobblyTypedefs)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Symbol Table", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	SymbolGroup group;
	group.source = *source;
	
	Result<SourceFile*> file = database.source_files.create_symbol("File", (*source)->handle());
	CCC_GTEST_FAIL_IF_ERROR(file);
	
	// Create a raw builtin type.
	std::unique_ptr<ast::BuiltIn> underlying_type = std::make_unique<ast::BuiltIn>();
	Result<DataType*> underlying_symbol = database.create_data_type_if_unique(
		std::move(underlying_type), StabsTypeNumber{1,1}, "Underlying", **file, group);
	
	// Create a typedef for that builtin.
	std::unique_ptr<ast::TypeName> typedef_type = std::make_unique<ast::TypeName>();
	typedef_type->storage_class = STORAGE_CLASS_TYPEDEF;
	typedef_type->unresolved_stabs = std::make_unique<ast::TypeName::UnresolvedStabs>();
	typedef_type->unresolved_stabs->type_name = "Underlying";
	typedef_type->unresolved_stabs->referenced_file_handle = (*file)->handle().value;
	typedef_type->unresolved_stabs->stabs_type_number.file = 1;
	typedef_type->unresolved_stabs->stabs_type_number.type = 1;
	Result<DataType*> typedef_symbol = database.create_data_type_if_unique(
		std::move(typedef_type), StabsTypeNumber{1,2}, "Typedef", **file, group);
	
	// Create a struct referencing the builtin type directly.
	std::unique_ptr<ast::StructOrUnion> struct_underlying_type = std::make_unique<ast::StructOrUnion>();
	std::unique_ptr<ast::TypeName> member_underlying_type = std::make_unique<ast::TypeName>();
	member_underlying_type->unresolved_stabs = std::make_unique<ast::TypeName::UnresolvedStabs>();
	member_underlying_type->unresolved_stabs->type_name = "Underlying";
	member_underlying_type->unresolved_stabs->referenced_file_handle = (*file)->handle().value;
	member_underlying_type->unresolved_stabs->stabs_type_number.file = 1;
	member_underlying_type->unresolved_stabs->stabs_type_number.type = 1;
	struct_underlying_type->fields.emplace_back(std::move(member_underlying_type));
	Result<DataType*> struct_underlying_symbol = database.create_data_type_if_unique(
		std::move(struct_underlying_type), StabsTypeNumber{1,3}, "WobblyStruct", **file, group);
	
	// Create a struct referencing the builtin through the typedef.
	std::unique_ptr<ast::StructOrUnion> struct_typedef_type = std::make_unique<ast::StructOrUnion>();
	std::unique_ptr<ast::TypeName> member_typedef_type = std::make_unique<ast::TypeName>();
	member_typedef_type->unresolved_stabs = std::make_unique<ast::TypeName::UnresolvedStabs>();
	member_typedef_type->unresolved_stabs->type_name = "Typedef";
	member_typedef_type->unresolved_stabs->referenced_file_handle = (*file)->handle().value;
	member_typedef_type->unresolved_stabs->stabs_type_number.file = 1;
	member_typedef_type->unresolved_stabs->stabs_type_number.type = 2;
	struct_typedef_type->fields.emplace_back(std::move(member_typedef_type));
	Result<DataType*> struct_typedef_symbol = database.create_data_type_if_unique(
		std::move(struct_typedef_type), StabsTypeNumber{1,4}, "WobblyStruct", **file, group);
	
	// Validate that the two structs were deduplicated despite not being equal.
	auto handles = database.data_types.handles_from_name("WobblyStruct");
	ASSERT_TRUE(handles.begin() != handles.end());
	EXPECT_EQ(++handles.begin(), handles.end());
	
	// Validate that we can lookup the struct and that it has a single field which is a type name.
	DataType* chosen_type = database.data_types.symbol_from_handle(handles.begin()->second);
	ASSERT_TRUE(chosen_type && chosen_type->type() && chosen_type->type()->descriptor == ast::STRUCT_OR_UNION);
	ast::StructOrUnion& chosen_struct = chosen_type->type()->as<ast::StructOrUnion>();
	ASSERT_EQ(chosen_struct.fields.size(), 1);
	ASSERT_EQ(chosen_struct.fields[0]->descriptor, ast::TYPE_NAME);
	
	// Validate that the typedef'd struct was chosen over the other one.
	ast::TypeName::UnresolvedStabs* field = chosen_struct.fields[0]->as<ast::TypeName>().unresolved_stabs.get();
	ASSERT_TRUE(field);
	EXPECT_EQ(field->stabs_type_number.type, 2);
}

TEST(CCCSymbolDatabase, NodeHandle)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> source = database.symbol_sources.create_symbol("Symbol Table", SymbolSourceHandle());
	CCC_GTEST_FAIL_IF_ERROR(source);
	
	Result<DataType*> data_type = database.data_types.create_symbol("DataType", (*source)->handle());
	CCC_GTEST_FAIL_IF_ERROR(data_type);
	
	std::unique_ptr<ast::BuiltIn> node = std::make_unique<ast::BuiltIn>();
	(*data_type)->set_type(std::move(node));
	
	NodeHandle node_handle(**data_type, (*data_type)->type());
	
	// Make sure we can lookup the node from the handle.
	EXPECT_EQ(node_handle.lookup_node(database), (*data_type)->type());
	
	// Increment the generation counter.
	(*data_type)->invalidate_node_handles();
	
	// Make sure we can no longer lookup the node from the handle.
	EXPECT_EQ(node_handle.lookup_node(database), nullptr);
	
	// Destroy the symbol.
	database.data_types.mark_symbol_for_destruction((*data_type)->handle(), &database);
	database.destroy_marked_symbols();
	
	// Make sure we can still not lookup the node from the handle.
	EXPECT_EQ(node_handle.lookup_node(database), nullptr);
}
