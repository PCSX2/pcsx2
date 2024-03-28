// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "ast.h"
#include "util.h"

namespace ccc {

enum class StabsSymbolDescriptor : u8 {
	LOCAL_VARIABLE = '_',
	REFERENCE_PARAMETER_A = 'a',
	LOCAL_FUNCTION = 'f',
	GLOBAL_FUNCTION = 'F',
	GLOBAL_VARIABLE = 'G',
	REGISTER_PARAMETER = 'P',
	VALUE_PARAMETER = 'p',
	REGISTER_VARIABLE = 'r',
	STATIC_GLOBAL_VARIABLE = 'S',
	TYPE_NAME = 't',
	ENUM_STRUCT_OR_TYPE_TAG = 'T',
	STATIC_LOCAL_VARIABLE = 'V',
	REFERENCE_PARAMETER_V = 'v'
};

struct StabsType;

struct StabsSymbol {
	StabsSymbolDescriptor descriptor;
	std::string name;
	std::unique_ptr<StabsType> type;
};

Result<StabsSymbol> parse_stabs_symbol(const char*& input);

enum class StabsTypeDescriptor : u8 {
	TYPE_REFERENCE = 0xef, // '0'..'9','('
	ARRAY = 'a',
	ENUM = 'e',
	FUNCTION = 'f',
	CONST_QUALIFIER = 'k',
	RANGE = 'r',
	STRUCT = 's',
	UNION = 'u',
	CROSS_REFERENCE = 'x',
	VOLATILE_QUALIFIER = 'B',
	FLOATING_POINT_BUILTIN = 'R',
	METHOD = '#',
	REFERENCE = '&',
	POINTER = '*',
	TYPE_ATTRIBUTE = '@',
	POINTER_TO_DATA_MEMBER = 0xee, // also '@'
	BUILTIN = '-'
};

struct StabsBaseClass;
struct StabsField;
struct StabsMemberFunctionSet;

// e.g. for "123=*456" 123 would be the type_number, the type descriptor would
// be of type POINTER and StabsPointerType::value_type would point to a type
// with type_number = 456.
struct StabsType {
	StabsTypeNumber type_number;
	// The name field is only populated for root types and cross references.
	std::optional<std::string> name;
	bool is_typedef = false;
	bool is_root = false;
	std::optional<StabsTypeDescriptor> descriptor;
	
	StabsType(StabsTypeNumber n) : type_number(n) {}
	StabsType(StabsTypeDescriptor d) : descriptor(d) {}
	StabsType(StabsTypeNumber n, StabsTypeDescriptor d) : type_number(n), descriptor(d) {}
	virtual ~StabsType() {}
	
	template <typename SubType>
	SubType& as()
	{
		CCC_ASSERT(descriptor == SubType::DESCRIPTOR);
		return *static_cast<SubType*>(this);
	}
	
	template <typename SubType>
	const SubType& as() const
	{
		CCC_ASSERT(descriptor == SubType::DESCRIPTOR);
		return *static_cast<const SubType*>(this);
	}
	
	virtual void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const
	{
		if(type_number.valid() && descriptor.has_value()) {
			output.emplace(type_number, this);
		}
	}
};

struct StabsTypeReferenceType : StabsType {
	std::unique_ptr<StabsType> type;
	
	StabsTypeReferenceType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::TYPE_REFERENCE;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		type->enumerate_numbered_types(output);
	}
};

struct StabsArrayType : StabsType {
	std::unique_ptr<StabsType> index_type;
	std::unique_ptr<StabsType> element_type;
	
	StabsArrayType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::ARRAY;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		index_type->enumerate_numbered_types(output);
		element_type->enumerate_numbered_types(output);
	}
};

struct StabsEnumType : StabsType {
	std::vector<std::pair<s32, std::string>> fields;
	
	StabsEnumType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::ENUM;
};

struct StabsFunctionType : StabsType {
	std::unique_ptr<StabsType> return_type;
	
	StabsFunctionType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::FUNCTION;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		return_type->enumerate_numbered_types(output);
	}
};

struct StabsVolatileQualifierType : StabsType {
	std::unique_ptr<StabsType> type;
	
	StabsVolatileQualifierType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::VOLATILE_QUALIFIER;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		type->enumerate_numbered_types(output);
	}
};

struct StabsConstQualifierType : StabsType {
	std::unique_ptr<StabsType> type;
	
	StabsConstQualifierType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::CONST_QUALIFIER;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		type->enumerate_numbered_types(output);
	}
};

struct StabsRangeType : StabsType {
	std::unique_ptr<StabsType> type;
	std::string low;
	std::string high; // Some compilers wrote out a wrapped around value here for zero (or variable?) length arrays.
	
	StabsRangeType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::RANGE;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		type->enumerate_numbered_types(output);
	}
};

struct StabsStructOrUnionType : StabsType {
	enum class Visibility : u8 {
		NONE,
		PRIVATE,
		PROTECTED,
		PUBLIC,
		PUBLIC_OPTIMIZED_OUT
	};

	struct BaseClass {
		bool is_virtual;
		Visibility visibility;
		s32 offset = -1;
		std::unique_ptr<StabsType> type;
	};

	struct Field {
		std::string name;
		Visibility visibility = Visibility::NONE;
		std::unique_ptr<StabsType> type;
		bool is_static = false;
		s32 offset_bits = 0;
		s32 size_bits = 0;
		std::string type_name;
	};

	struct MemberFunction {
		std::unique_ptr<StabsType> type;
		std::unique_ptr<StabsType> virtual_type;
		Visibility visibility;
		bool is_const = false;
		bool is_volatile = false;
		ast::MemberFunctionModifier modifier = ast::MemberFunctionModifier::NONE;
		s32 vtable_index = -1;
	};

	struct MemberFunctionSet {
		std::string name;
		std::vector<MemberFunction> overloads;
	};
	
	s64 size = -1;
	std::vector<BaseClass> base_classes;
	std::vector<Field> fields;
	std::vector<MemberFunctionSet> member_functions;
	std::unique_ptr<StabsType> first_base_class;
	
	StabsStructOrUnionType(StabsTypeNumber n, StabsTypeDescriptor d) : StabsType(n, d) {}
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		for(const BaseClass& base_class : base_classes) {
			base_class.type->enumerate_numbered_types(output);
		}
		for(const Field& field : fields) {
			field.type->enumerate_numbered_types(output);
		}
		for(const MemberFunctionSet& member_function_set : member_functions) {
			for(const MemberFunction& member_function : member_function_set.overloads) {
				member_function.type->enumerate_numbered_types(output);
				if(member_function.virtual_type.get()) {
					member_function.virtual_type->enumerate_numbered_types(output);
				}
			}
		}
		if(first_base_class.get()) {
			first_base_class->enumerate_numbered_types(output);
		}
	}
};

struct StabsStructType : StabsStructOrUnionType {
	StabsStructType(StabsTypeNumber n) : StabsStructOrUnionType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::STRUCT;
};

struct StabsUnionType : StabsStructOrUnionType {
	StabsUnionType(StabsTypeNumber n) : StabsStructOrUnionType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::UNION;
};


struct StabsCrossReferenceType : StabsType {
	ast::ForwardDeclaredType type;
	std::string identifier;
	
	StabsCrossReferenceType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::CROSS_REFERENCE;
};

struct StabsFloatingPointBuiltInType : StabsType {
	s32 fpclass = -1;
	s32 bytes = -1;
	
	StabsFloatingPointBuiltInType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::FLOATING_POINT_BUILTIN;
};

struct StabsMethodType : StabsType {
	std::unique_ptr<StabsType> return_type;
	std::optional<std::unique_ptr<StabsType>> class_type;
	std::vector<std::unique_ptr<StabsType>> parameter_types;
	
	StabsMethodType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::METHOD;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		return_type->enumerate_numbered_types(output);
		if(class_type.has_value()) {
			(*class_type)->enumerate_numbered_types(output);
		}
		for(const std::unique_ptr<StabsType>& parameter_type : parameter_types) {
			parameter_type->enumerate_numbered_types(output);
		}
	}
};

struct StabsReferenceType : StabsType {
	std::unique_ptr<StabsType> value_type;
	
	StabsReferenceType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::REFERENCE;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		value_type->enumerate_numbered_types(output);
	}
};

struct StabsPointerType : StabsType {
	std::unique_ptr<StabsType> value_type;
	
	StabsPointerType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::POINTER;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		value_type->enumerate_numbered_types(output);
	}
};

struct StabsSizeTypeAttributeType : StabsType {
	s64 size_bits = -1;
	std::unique_ptr<StabsType> type;
	
	StabsSizeTypeAttributeType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::TYPE_ATTRIBUTE;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		type->enumerate_numbered_types(output);
	}
};

struct StabsPointerToDataMemberType : StabsType {
	std::unique_ptr<StabsType> class_type;
	std::unique_ptr<StabsType> member_type;
	
	StabsPointerToDataMemberType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::POINTER_TO_DATA_MEMBER;
	
	void enumerate_numbered_types(std::map<StabsTypeNumber, const StabsType*>& output) const override
	{
		StabsType::enumerate_numbered_types(output);
		class_type->enumerate_numbered_types(output);
		member_type->enumerate_numbered_types(output);
	}
};

struct StabsBuiltInType : StabsType {
	s64 type_id = -1;
	
	StabsBuiltInType(StabsTypeNumber n) : StabsType(n, DESCRIPTOR) {}
	static const constexpr StabsTypeDescriptor DESCRIPTOR = StabsTypeDescriptor::BUILTIN;
};

extern const char* STAB_TRUNCATED_ERROR_MESSAGE;

Result<std::unique_ptr<StabsType>> parse_top_level_stabs_type(const char*& input);
std::optional<s32> parse_number_s32(const char*& input);
std::optional<s64> parse_number_s64(const char*& input);
std::optional<std::string> parse_stabs_identifier(const char*& input, char terminator);
Result<std::string> parse_dodgy_stabs_identifier(const char*& input, char terminator);
const char* stabs_field_visibility_to_string(StabsStructOrUnionType::Visibility visibility);

}
