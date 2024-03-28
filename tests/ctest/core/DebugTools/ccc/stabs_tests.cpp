// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <gtest/gtest.h>
#include "ccc/stabs.h"

using namespace ccc;

// Tests for the STABS parser. They are based on real compiler outputs from the
// old homebrew toolchain (GCC 3.2.3).

#define STABS_TEST(name, stab) \
	static void stabs_test_##name(StabsSymbol& symbol); \
	TEST(CCCStabs, name) \
	{ \
		const char* input = stab; \
		Result<StabsSymbol> symbol = parse_stabs_symbol(input); \
		CCC_GTEST_FAIL_IF_ERROR(symbol); \
		stabs_test_##name(*symbol); \
	} \
	static void stabs_test_##name(StabsSymbol& symbol)

#define STABS_IDENTIFIER_TEST(name, identifier) \
	TEST(CCCStabs, name) \
	{ \
		const char* input = identifier ":"; \
		Result<std::string> result = parse_dodgy_stabs_identifier(input, ':'); \
		CCC_GTEST_FAIL_IF_ERROR(result); \
		ASSERT_EQ(*result, identifier); \
	}

// ee-g++ -gstabs
// typedef int s32;
STABS_TEST(TypeNumber, "s32:t1=0")
{
	ASSERT_FALSE(!symbol.type->type_number.valid());
	ASSERT_EQ(symbol.type->type_number.file, -1);
	ASSERT_EQ(symbol.type->type_number.type, 1);
	ASSERT_TRUE(symbol.type->descriptor.has_value());
}

// ee-g++ -gstabs
// typedef int s32;
STABS_TEST(FancyTypeNumber, "s32:t(1,1)=(0,1)")
{
	ASSERT_FALSE(!symbol.type->type_number.valid());
	ASSERT_EQ(symbol.type->type_number.file, 1);
	ASSERT_EQ(symbol.type->type_number.type, 1);
	ASSERT_TRUE(symbol.type->descriptor.has_value());
}

// ee-g++ -gstabs
// typedef int s32;
STABS_TEST(TypeReference, "s32:t(1,1)=(0,1)")
{
	StabsTypeReferenceType& type_reference = symbol.type->as<StabsTypeReferenceType>();
	ASSERT_FALSE(!type_reference.type->type_number.valid());
	ASSERT_EQ(type_reference.type->type_number.file, 0);
	ASSERT_EQ(type_reference.type->type_number.type, 1);
	ASSERT_FALSE(type_reference.type->descriptor.has_value());
}

// ee-g++ -gstabs
// typedef int Array[1][2];
STABS_TEST(MultiDimensionalArray, "Array:t(1,1)=(1,2)=ar(1,3)=r(1,3);0;4294967295;;0;0;(1,4)=ar(1,3);0;1;(1,5)=ar(1,3);0;2;(0,1)")
{
	StabsTypeReferenceType& type_reference = symbol.type->as<StabsTypeReferenceType>();
	StabsArrayType& outer_array = type_reference.type->as<StabsArrayType>();
	ASSERT_EQ(outer_array.index_type->as<StabsRangeType>().high, "0");
	StabsArrayType& inner_array = outer_array.element_type->as<StabsArrayType>();
	ASSERT_EQ(inner_array.index_type->as<StabsRangeType>().high, "1");
}

// ee-g++ -gstabs
// enum E { A = 0, B = 1, C = 2147483647, D = -2147483648 };
STABS_TEST(Enum, "E:t(1,1)=eA:0,B:1,C:2147483647,D:-2147483648,;")
{
	StabsEnumType& enum_type = symbol.type->as<StabsEnumType>();
	ASSERT_EQ(enum_type.fields.size(), 4);
	ASSERT_EQ(enum_type.fields.at(0).first, 0);
	ASSERT_EQ(enum_type.fields.at(0).second, "A");
	ASSERT_EQ(enum_type.fields.at(1).first, 1);
	ASSERT_EQ(enum_type.fields.at(1).second, "B");
	ASSERT_EQ(enum_type.fields.at(2).first, 2147483647);
	ASSERT_EQ(enum_type.fields.at(2).second, "C");
	ASSERT_EQ(enum_type.fields.at(3).first, -2147483648);
	ASSERT_EQ(enum_type.fields.at(3).second, "D");
}

// ee-g++ -gstabs
// typedef int (function)();
STABS_TEST(Function, "function:t(1,1)=(1,2)=f(0,1)")
{
	StabsTypeReferenceType& type_reference = symbol.type->as<StabsTypeReferenceType>();
	StabsFunctionType& function = type_reference.type->as<StabsFunctionType>();
	ASSERT_EQ(function.return_type->type_number.file, 0);
	ASSERT_EQ(function.return_type->type_number.type, 1);
}

// -gstabs+
// typedef volatile int VolatileInt;
STABS_TEST(VolatileQualifier, "VolatileInt:t(1,1)=(1,2)=B(0,1)") {}

// -gstabs+
// typedef const int ConstInt;
STABS_TEST(ConstQualifier, "ConstInt:t(1,1)=(1,2)=k(0,1)") {}

// ee-g++ -gstabs
// int
STABS_TEST(RangeBuiltIn, "int:t(0,1)=r(0,1);-2147483648;2147483647;")
{
	StabsRangeType& range = symbol.type->as<StabsRangeType>();
	ASSERT_EQ(range.low, "-2147483648");
	ASSERT_EQ(range.high, "2147483647");
}

// ee-g++ -gstabs
// struct SimpleStruct { int a; };
STABS_TEST(SimpleStruct, "SimpleStruct:T(1,1)=s4a:(0,1),0,32;;")
{
	StabsStructType& struct_type = symbol.type->as<StabsStructType>();
	ASSERT_EQ(struct_type.size, 4);
	ASSERT_EQ(struct_type.base_classes.size(), 0);
	ASSERT_EQ(struct_type.fields.size(), 1);
	ASSERT_EQ(struct_type.member_functions.size(), 0);
	
	StabsStructOrUnionType::Field& field = struct_type.fields.at(0);
	ASSERT_EQ(field.name, "a");
	ASSERT_EQ(field.offset_bits, 0);
	ASSERT_EQ(field.size_bits, 32);
}

// ee-g++ -gstabs
// union Union { int i; float f; };
STABS_TEST(Union, "Union:T(1,1)=u4i:(0,1),0,32;f:(0,14),0,32;;")
{
	StabsUnionType& union_type = symbol.type->as<StabsUnionType>();
	ASSERT_EQ(union_type.size, 4);
	ASSERT_EQ(union_type.base_classes.size(), 0);
	ASSERT_EQ(union_type.fields.size(), 2);
	ASSERT_EQ(union_type.member_functions.size(), 0);
}

// ee-g++ -gstabs
// struct NestedStructsAndUnions {
// 	union { struct { int a; } b; } c;
// 	struct { int d; } e;
// };
STABS_TEST(NestedStructsAndUnions, "NestedStructsAndUnions:T(1,1)=s8c:(1,2)=u4b:(1,3)=s4a:(0,1),0,32;;,0,32;;,0,32;e:(1,4)=s4d:(0,1),0,32;;,32,32;;")
{
	StabsStructType& struct_type = symbol.type->as<StabsStructType>();
	StabsStructOrUnionType::Field& c = struct_type.fields.at(0);
	ASSERT_EQ(c.name, "c");
	StabsUnionType& c_type = c.type->as<StabsUnionType>();
	StabsStructOrUnionType::Field& b = c_type.fields.at(0);
	ASSERT_EQ(c_type.fields.at(0).name, "b");
	StabsStructOrUnionType::Field& a = b.type->as<StabsStructType>().fields.at(0);
	ASSERT_EQ(a.name, "a");
	StabsStructOrUnionType::Field& e = struct_type.fields.at(1);
	ASSERT_EQ(e.name, "e");
	StabsStructOrUnionType::Field& d = e.type->as<StabsStructType>().fields.at(0);
	ASSERT_EQ(d.name, "d");
}

// ee-g++ -gstabs+
// struct DefaultMemberFunctions {};
STABS_TEST(DefaultMemberFunctions,
	"DefaultMemberFunctions:Tt(1,1)=s1"
		"operator=::(1,2)=#(1,1),(1,3)=&(1,1),(1,4)=*(1,1),(1,5)=&(1,6)=k(1,1),(0,23);:_ZN22DefaultMemberFunctionsaSERKS_;2A.;"
		"__base_ctor::(1,7)=#(1,1),(0,23),(1,4),(1,5),(0,23);:_ZN22DefaultMemberFunctionsC2ERKS_;2A.;"
		"__comp_ctor::(1,7):_ZN22DefaultMemberFunctionsC1ERKS_;2A.;"
		"__base_ctor::(1,8)=#(1,1),(0,23),(1,4),(0,23);:_ZN22DefaultMemberFunctionsC2Ev;2A.;"
		"__comp_ctor::(1,8):_ZN22DefaultMemberFunctionsC1Ev;2A.;;")
{
	StabsStructType& struct_type = symbol.type->as<StabsStructType>();
	ASSERT_EQ(struct_type.member_functions.size(), 5);
	EXPECT_EQ(struct_type.member_functions[0].name, "operator=");
	EXPECT_EQ(struct_type.member_functions[1].name, "__base_ctor");
	EXPECT_EQ(struct_type.member_functions[2].name, "__comp_ctor");
	EXPECT_EQ(struct_type.member_functions[3].name, "__base_ctor");
	EXPECT_EQ(struct_type.member_functions[4].name, "__comp_ctor");
}

// ee-g++ -gstabs+
// class FirstBaseClass {};
// class SecondBaseClass {};
// class MultipleInheritance : FirstBaseClass, SecondBaseClass {};
STABS_TEST(MultipleInheritance,
	"MultipleInheritance:Tt(1,17)=s1"
		"!2,000,(1,1);000,(1,9);"
		"operator=::(1,18)=#(1,17),(1,19)=&(1,17),(1,20)=*(1,17),(1,21)=&(1,22)=k(1,17),(0,23);:_ZN19MultipleInheritanceaSERKS_;2A.;"
		"__base_ctor::(1,23)=#(1,17),(0,23),(1,20),(1,21),(0,23);:_ZN19MultipleInheritanceC2ERKS_;2A.;"
		"__comp_ctor::(1,23):_ZN19MultipleInheritanceC1ERKS_;2A.;"
		"__base_ctor::(1,24)=#(1,17),(0,23),(1,20),(0,23);:_ZN19MultipleInheritanceC2Ev;2A.;"
		"__comp_ctor::(1,24):_ZN19MultipleInheritanceC1Ev;2A.;;")
{
	StabsStructType& struct_type = symbol.type->as<StabsStructType>();
	ASSERT_EQ(struct_type.base_classes.size(), 2);
	EXPECT_EQ(struct_type.base_classes[0].type->type_number.type, 1);
	EXPECT_EQ(struct_type.base_classes[1].type->type_number.type, 9);
}

// ee-g++ -gstabs
// struct ForwardDeclared;
// typedef ForwardDeclared* ForwardDeclaredPtr;
STABS_TEST(CrossReference, "ForwardDeclaredPtr:t(1,1)=(1,2)=*(1,3)=xsForwardDeclared:")
{
	StabsTypeReferenceType& type_reference = symbol.type->as<StabsTypeReferenceType>();
	StabsPointerType& pointer = type_reference.type->as<StabsPointerType>();
	StabsCrossReferenceType& cross_reference = pointer.value_type->as<StabsCrossReferenceType>();
	ASSERT_EQ(cross_reference.type, ast::ForwardDeclaredType::STRUCT);
	ASSERT_EQ(cross_reference.identifier, "ForwardDeclared");
}

// ee-g++ -gstabs
// struct Struct;
// typedef int Struct::*pointer_to_data_member;
STABS_TEST(PointerToDataMember, "pointer_to_data_member:t(1,1)=(1,2)=*(1,3)=@(1,4)=xsStruct:,(0,1)")
{
	StabsTypeReferenceType& type_reference = symbol.type->as<StabsTypeReferenceType>();
	StabsPointerType& pointer = type_reference.type->as<StabsPointerType>();
	StabsPointerToDataMemberType& pointer_to_data_member = pointer.value_type->as<StabsPointerToDataMemberType>();
	StabsCrossReferenceType& class_type = pointer_to_data_member.class_type->as<StabsCrossReferenceType>();
	ASSERT_EQ(class_type.identifier, "Struct");
}

// ee-g++ -gstabs
// namespace Namespace { struct A; }
// template <typename T> struct DodgyTypeName {};
// template struct DodgyTypeName<Namespace::A>;
STABS_TEST(DodgyTypeName, "DodgyTypeName<Namespace::A>:T(1,1)=s1;")
{
	ASSERT_EQ(symbol.name, "DodgyTypeName<Namespace::A>");
}

// ee-g++ -gstabs
// namespace Namespace { struct A; }
// template <typename T> struct ColonInTypeName {};
// template struct ColonInTypeName<Namespace::A>;
STABS_IDENTIFIER_TEST(ColonInTypeName, "ColonInTypeName<Namespace::A>");

// ee-g++ -gstabs
// template <char c> struct LessThanCharacterLiteralInTypeName {};
// template struct LessThanCharacterLiteralInTypeName<'<'>;
STABS_IDENTIFIER_TEST(LessThanCharacterLiteralInTypeName, "LessThanCharacterLiteralInTypeName<'<'>");

// ee-g++ -gstabs
// template <char c> struct GreaterThanCharacterLiteralInTypeName {};
// template struct GreaterThanCharacterLiteralInTypeName<'>'>;
STABS_IDENTIFIER_TEST(GreaterThanCharacterLiteralInTypeName, "GreaterThanCharacterLiteralInTypeName<'>'>");

// ee-g++ -gstabs
// template <char c> struct SingleQuoteCharacterLiteralInTypeName {};
// template struct SingleQuoteCharacterLiteralInTypeName<'\''>;
STABS_IDENTIFIER_TEST(SingleQuoteCharacterLiteralInTypeName, "SingleQuoteCharacterLiteralInTypeName<'''>");

// ee-g++ -gstabs
// template <char c> struct NonPrintableCharacterLiteralInTypeName {};
// template struct NonPrintableCharacterLiteralInTypeName<'\xff'>;
STABS_IDENTIFIER_TEST(NonPrintableCharacterLiteralInTypeName, "NonPrintableCharacterLiteralInTypeName<'\xff" "77777777'>");
