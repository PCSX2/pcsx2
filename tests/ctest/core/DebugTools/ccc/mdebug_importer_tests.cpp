// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <gtest/gtest.h>
#include "ccc/mdebug_importer.h"

using namespace ccc;
using namespace ccc::mdebug;

// Tests for the whole STABS parsing and analysis pipeline. They are based on
// real compiler outputs from the old homebrew toolchain (GCC 3.2.3) except
// where otherwise stated.

static Result<SymbolDatabase> run_importer(const char* name, const mdebug::File& input)
{
	SymbolDatabase database;
	
	Result<SymbolSource*> symbol_source = database.symbol_sources.create_symbol(name, SymbolSourceHandle());
	CCC_RETURN_IF_ERROR(symbol_source);
	
	AnalysisContext context;
	context.group.source = (*symbol_source)->handle();
	context.importer_flags = DONT_DEDUPLICATE_SYMBOLS | STRICT_PARSING;
	
	Result<void> result = import_file(database, input, context);
	CCC_RETURN_IF_ERROR(result);
	
	return database;
}

#define MDEBUG_IMPORTER_TEST(name, symbols) \
	static void mdebug_importer_test_##name(SymbolDatabase& database); \
	TEST(CCCMdebugImporter, name) \
	{ \
		mdebug::File input = {std::vector<mdebug::Symbol>symbols}; \
		Result<SymbolDatabase> database = run_importer(#name, input); \
		CCC_GTEST_FAIL_IF_ERROR(database); \
		mdebug_importer_test_##name(*database); \
	} \
	static void mdebug_importer_test_##name(SymbolDatabase& database)

#define STABS_CODE(code) ((code) + 0x8f300)

// ee-g++ -gstabs
// enum Enum {};
MDEBUG_IMPORTER_TEST(Enum,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "Enum:t(1,1)=e;"}
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("Enum");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::ENUM);
	EXPECT_EQ(data_type->type()->storage_class, STORAGE_CLASS_NONE);
}

// ee-g++ -gstabs
// typedef enum NamedTypedefedEnum {} NamedTypedefedEnum;
MDEBUG_IMPORTER_TEST(NamedTypedefedEnum,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "Enum:t(1,1)=e;"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "Enum:t(1,2)=(1,1)"}
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("Enum");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::ENUM);
	EXPECT_EQ(data_type->type()->storage_class, STORAGE_CLASS_TYPEDEF);
}

// Synthetic example. Something like:
// typedef enum {} ErraticEnum;
MDEBUG_IMPORTER_TEST(ErraticEnum,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), " :T(1,1)=e;"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "ErraticEnum:t(1,2)=(1,1)"}
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("ErraticEnum");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::ENUM);
	EXPECT_EQ(data_type->type()->storage_class, STORAGE_CLASS_TYPEDEF);
}

// ee-g++ -gstabs
// struct Struct {};
MDEBUG_IMPORTER_TEST(Struct,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "Struct:T(1,1)=s1;"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "Struct:t(1,1)"}
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("Struct");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::STRUCT_OR_UNION);
	EXPECT_EQ(data_type->type()->storage_class, STORAGE_CLASS_NONE);
}

// ee-g++ -gstabs
// typedef struct {} TypedefedStruct;
MDEBUG_IMPORTER_TEST(TypedefedStruct,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "TypedefedStruct:t(1,1)=s1;"}
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("TypedefedStruct");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::STRUCT_OR_UNION);
	EXPECT_EQ(data_type->type()->storage_class, STORAGE_CLASS_TYPEDEF);
}

// ee-g++ -gstabs
// typedef struct NamedTypedefedStruct {} NamedTypedefedStruct;
MDEBUG_IMPORTER_TEST(NamedTypedefedStruct,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "NamedTypedefedStruct:T(1,1)=s1;"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "NamedTypedefedStruct:t(1,1)"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "NamedTypedefedStruct:t(1,2)=(1,1)"}
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("NamedTypedefedStruct");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::STRUCT_OR_UNION);
	EXPECT_EQ(data_type->type()->storage_class, STORAGE_CLASS_TYPEDEF);
}

// Synthetic example. Something like:
// typedef struct {} StrangeStruct;
MDEBUG_IMPORTER_TEST(StrangeStruct,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "StrangeStruct:T(1,1)=s1;"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "StrangeStruct:t(1,2)=(1,1)"}
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("StrangeStruct");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::STRUCT_OR_UNION);
	EXPECT_EQ(data_type->type()->storage_class, STORAGE_CLASS_TYPEDEF);
}

// Synthetic example. Something like:
// typedef struct {} PeculiarParameter;
// See the fix_recursively_emitted_structures function for more information.
MDEBUG_IMPORTER_TEST(PeculiarParameter,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM),
			"ReturnType:t(0,1)=r1;-2147483648;2147483647;"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM),
			"PeculiarParameter:t(1,1)="
				"s1;some_generated_func::#(1,1),(0,1),(1,2)=&(1,3)="
					"s1;some_generated_func::#(1,1),(0,1),"
						"(1,2)"
					";:RC17PeculiarParameter;2A.;;"
				";:RC17PeculiarParameter;2A.;;"},
	}))
{
	// Lookup the data type.
	DataTypeHandle handle = database.data_types.first_handle_from_name("PeculiarParameter");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	ASSERT_TRUE(data_type->type()->descriptor == ast::STRUCT_OR_UNION);
	ast::StructOrUnion& structure = data_type->type()->as<ast::StructOrUnion>();
	
	// Find the first member function.
	ASSERT_TRUE(structure.member_functions.size() == 1);
	ASSERT_TRUE(structure.member_functions[0]->descriptor == ast::FUNCTION);
	ast::Function& function = structure.member_functions[0]->as<ast::Function>();
	
	// Find the first parameter from the first member function.
	ASSERT_TRUE(function.parameters.has_value() && function.parameters->size() == 1);
	ASSERT_TRUE((*function.parameters)[0]->descriptor == ast::POINTER_OR_REFERENCE);
	ast::PointerOrReference& reference = (*function.parameters)[0]->as<ast::PointerOrReference>();
	
	// Make sure that the inner struct was replaced with a type name.
	ASSERT_TRUE(reference.value_type->descriptor == ast::TYPE_NAME);
}

// Synthetic example.
MDEBUG_IMPORTER_TEST(VexingVoid,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "VexingVoid:t1=1"},
	}))
{
	EXPECT_EQ(database.data_types.size(), 1);
	DataTypeHandle handle = database.data_types.first_handle_from_name("VexingVoid");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	EXPECT_EQ(data_type->type()->descriptor, ast::BUILTIN);
	EXPECT_EQ(data_type->type()->as<ast::BuiltIn>().bclass, ast::BuiltInClass::VOID_TYPE);
}

// ee-g++ -gstabs
// typedef void* VillanousVoid;
MDEBUG_IMPORTER_TEST(VillanousVoid,
	({
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "__builtin_va_list:t(0,22)=*(0,23)=(0,23)"},
		{0x00000000, SymbolType::NIL, SymbolClass::NIL, STABS_CODE(N_LSYM), "VillanousVoid:t(1,1)=(0,22)"},
	}))
{
	EXPECT_EQ(database.data_types.size(), 2);
	DataTypeHandle handle = database.data_types.first_handle_from_name("VillanousVoid");
	DataType* data_type = database.data_types.symbol_from_handle(handle);
	ASSERT_TRUE(data_type && data_type->type());
	ASSERT_EQ(data_type->type()->descriptor, ast::POINTER_OR_REFERENCE);
	ast::PointerOrReference& pointer = data_type->type()->as<ast::PointerOrReference>();
	ASSERT_EQ(pointer.value_type->descriptor, ast::BUILTIN);
	EXPECT_EQ(pointer.value_type->as<ast::BuiltIn>().bclass, ast::BuiltInClass::VOID_TYPE);
}

// ee-g++ -gstabs
// void SimpleFunction() {}
MDEBUG_IMPORTER_TEST(SimpleFunction,
	({
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM), "__builtin_va_list:t(0,22)=*(0,23)=(0,23)"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, STABS_CODE(N_FUN),  "_Z14SimpleFunctionv:F(0,23)"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, 1,                  "$LM1"},
		{0x00000000, SymbolType::PROC,  SymbolClass::TEXT, 1,                  "_Z14SimpleFunctionv"},
		{0x0000000c, SymbolType::LABEL, SymbolClass::TEXT, 1,                  "$LM2"},
		{0x00000020, SymbolType::END,   SymbolClass::TEXT, 31,                 "_Z14SimpleFunctionv"}
	}))
{
	EXPECT_EQ(database.functions.size(), 1);
	FunctionHandle handle = database.functions.first_handle_from_name("_Z14SimpleFunctionv");
	Function* function = database.functions.symbol_from_handle(handle);
	ASSERT_TRUE(function);
}

// iop-gcc -gstabs
// void SimpleFunctionIOP() {}
MDEBUG_IMPORTER_TEST(SimpleFunctionIOP,
	({
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM), "__builtin_va_list:t21=*22=22"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, 1,                  "$LM1"},
		{0x00000000, SymbolType::PROC,  SymbolClass::TEXT, 1,                  "SimpleFunctionIOP"},
		{0x0000000c, SymbolType::LABEL, SymbolClass::TEXT, 1,                  "$LM2"},
		{0x00000020, SymbolType::END,   SymbolClass::TEXT, 27,                 "SimpleFunctionIOP"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, STABS_CODE(N_FUN),  "SimpleFunctionIOP:F22"}
	}))
{
	EXPECT_EQ(database.functions.size(), 1);
	FunctionHandle handle = database.functions.first_handle_from_name("SimpleFunctionIOP");
	Function* function = database.functions.symbol_from_handle(handle);
	ASSERT_TRUE(function);
}

// ee-g++ -gstabs
// int ComplicatedFunction(int a, float b, char* c) {
// 	int x = b < 0;
// 	if(a) { int y = b + *c; return y; }
// 	for(int i = 0; i < 5; i++) { int z = b + i; x += z; }
// 	return x;
// }
MDEBUG_IMPORTER_TEST(ComplicatedFunction,
	({
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "int:t(0,1)=r(0,1);-2147483648;2147483647;"},
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "char:t(0,2)=r(0,2);0;127;"},
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "float:t(0,14)=r(0,1);4;0;"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, STABS_CODE(N_FUN),   "_Z19ComplicatedFunctionifPc:F(0,1)"},
		{0xffffffd0, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_PSYM),  "a:p(0,1)"},
		{0xffffffd4, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_PSYM),  "b:p(0,14)"},
		{0xffffffd8, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_PSYM),  "c:p(1,1)=*(0,2)"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, 1,                   "$LM1"},
		{0x00000000, SymbolType::PROC,  SymbolClass::TEXT, 1,                   "_Z19ComplicatedFunctionifPc"},
		{0x00000018, SymbolType::LABEL, SymbolClass::TEXT, 2,                   "$LM2"},
		{0x00000048, SymbolType::LABEL, SymbolClass::TEXT, 3,                   "$LM3"},
		{0x00000088, SymbolType::LABEL, SymbolClass::TEXT, 4,                   "$LM4"},
		{0x000000e0, SymbolType::LABEL, SymbolClass::TEXT, 5,                   "$LM5"},
		{0x000000e8, SymbolType::LABEL, SymbolClass::TEXT, 6,                   "$LM6"},
		{0x00000100, SymbolType::END,   SymbolClass::TEXT, 34,                  "_Z19ComplicatedFunctionifPc"},
		{0xffffffdc, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "x:(0,1)"},
		{0x00000018, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LBRAC), ""},
		{0xffffffe0, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "y:(0,1)"},
		{0x00000054, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LBRAC), ""},
		{0x00000088, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_RBRAC), ""},
		{0xffffffe0, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "i:(0,1)"},
		{0x00000088, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LBRAC), ""},
		{0xffffffe4, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "z:(0,1)"},
		{0x000000a4, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LBRAC), ""},
		{0x000000cc, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_RBRAC), ""},
		{0x000000e0, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_RBRAC), ""},
		{0x000000e8, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_RBRAC), ""}
	}))
{
	EXPECT_EQ(database.functions.size(), 1);
	EXPECT_EQ(database.local_variables.size(), 4);
	EXPECT_EQ(database.parameter_variables.size(), 3);
}

// iop-gcc -gstabs
// int ComplicatedFunctionIOP(int a, float b, char* c) {
// 	int x = b < 0, i;
// 	if(a) { int y = b + *c; return y; }
// 	for(i = 0; i < 5; i++) { int z = b + i; x += z; }
// 	return x;
// }
MDEBUG_IMPORTER_TEST(ComplicatedFunctionIOP,
	({
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "int:t1=r1;-2147483648;2147483647;"},
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "char:t2=r2;0;127;"},
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "float:t14=r1;4;0;"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, 1,                   "$LM1"},
		{0x00000000, SymbolType::PROC,  SymbolClass::TEXT, 1,                   "ComplicatedFunctionIOP"},
		{0x0000001c, SymbolType::LABEL, SymbolClass::TEXT, 2,                   "$LM2"},
		{0x00000054, SymbolType::LABEL, SymbolClass::TEXT, 3,                   "$LM3"},
		{0x000000b4, SymbolType::LABEL, SymbolClass::TEXT, 4,                   "$LM4"},
		{0x0000012c, SymbolType::LABEL, SymbolClass::TEXT, 5,                   "$LM5"},
		{0x00000138, SymbolType::LABEL, SymbolClass::TEXT, 6,                   "$LM6"},
		{0x00000154, SymbolType::END,   SymbolClass::TEXT, 27,                  "ComplicatedFunctionIOP"},
		{0x00000000, SymbolType::LABEL, SymbolClass::TEXT, STABS_CODE(N_FUN),   "ComplicatedFunctionIOP:F1"},
		{0x00000000, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_PSYM),  "a:p1"},
		{0x00000004, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_PSYM),  "b:p14"},
		{0x00000008, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_PSYM),  "c:p24=*2"},
		{0xffffffe0, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "x:1"},
		{0xffffffe4, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "i:1"},
		{0x0000001c, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LBRAC), "$LBB2"},
		{0xffffffe8, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "y:1"},
		{0x00000064, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LBRAC), "$LBB3"},
		{0x000000b4, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_RBRAC), "$LBE3"},
		{0xffffffe8, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LSYM),  "z:1"},
		{0x000000d4, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_LBRAC), "$LBB4"},
		{0x00000114, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_RBRAC), "$LBE4"},
		{0x00000138, SymbolType::NIL,   SymbolClass::NIL,  STABS_CODE(N_RBRAC), "$LBE2"}
	}))
{
	EXPECT_EQ(database.functions.size(), 1);
	EXPECT_EQ(database.local_variables.size(), 4);
	EXPECT_EQ(database.parameter_variables.size(), 3);
}
