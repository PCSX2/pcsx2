// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#include "stabs_to_ast.h"

#include "importer_flags.h"

#define AST_DEBUG(...) //__VA_ARGS__
#define AST_DEBUG_PRINTF(...) AST_DEBUG(printf(__VA_ARGS__);)

namespace ccc {

struct MemberFunctionInfo {
	std::string name;
	bool is_constructor_or_destructor = false;
	bool is_special_member_function = false;
	bool is_operator_member_function = false;
};

static bool is_void_like(const StabsType& type);
static Result<ast::BuiltInClass> classify_range(const StabsRangeType& type);
static Result<std::unique_ptr<ast::Node>> field_to_ast(
	const StabsStructOrUnionType::Field& field,
	const StabsType& enclosing_struct,
	const StabsToAstState& state,
	s32 depth);
static Result<bool> detect_bitfield(const StabsStructOrUnionType::Field& field, const StabsToAstState& state);
static Result<std::vector<std::unique_ptr<ast::Node>>> member_functions_to_ast(
	const StabsStructOrUnionType& type, const StabsToAstState& state, s32 depth);
static MemberFunctionInfo check_member_function(
	const std::string& mangled_name,
	std::string_view type_name_no_template_args,
	const DemanglerFunctions& demangler,
	u32 importer_flags);

Result<std::unique_ptr<ast::Node>> stabs_type_to_ast(
	const StabsType& type,
	const StabsType* enclosing_struct,
	const StabsToAstState& state,
	s32 depth,
	bool substitute_type_name,
	bool force_substitute)
{
	AST_DEBUG_PRINTF("%-*stype desc=%hhx '%c' num=(%d,%d) name=%s\n",
		depth * 4, "",
		type.descriptor.has_value() ? (u8) *type.descriptor : 'X',
		(type.descriptor.has_value() && isprint((u8) *type.descriptor)) ? (u8) *type.descriptor : '!',
		type.type_number.file, type.type_number.type,
		type.name.has_value() ? type.name->c_str() : "");
	
	if(depth > 200) {
		const char* error_message = "Call depth greater than 200 in stabs_type_to_ast, probably infinite recursion.";
		if(state.importer_flags & STRICT_PARSING) {
			return CCC_FAILURE(error_message);
		} else {
			CCC_WARN(error_message);
			
			auto error = std::make_unique<ast::Error>();
			error->message = error_message;
			return std::unique_ptr<ast::Node>(std::move(error));
		}
	}
	
	// This makes sure that types are replaced with their type name in cases
	// where that would be more appropriate.
	if(type.name.has_value()) {
		bool try_substitute = depth > 0 && (type.is_root
			|| type.descriptor == StabsTypeDescriptor::RANGE
			|| type.descriptor == StabsTypeDescriptor::BUILTIN);
		// GCC emits anonymous enums with a name of " " since apparently some
		// debuggers can't handle zero-length names.
		bool is_name_empty = type.name == "" || type.name == " ";
		// Cross references will be handled below.
		bool is_cross_reference = type.descriptor == StabsTypeDescriptor::CROSS_REFERENCE;
		bool is_void = is_void_like(type);
		if((substitute_type_name || try_substitute) && !is_name_empty && !is_cross_reference && !is_void) {
			auto type_name = std::make_unique<ast::TypeName>();
			type_name->source = ast::TypeNameSource::REFERENCE;
			type_name->unresolved_stabs = std::make_unique<ast::TypeName::UnresolvedStabs>();
			type_name->unresolved_stabs->type_name = *type.name;
			type_name->unresolved_stabs->referenced_file_handle = state.file_handle;
			type_name->unresolved_stabs->stabs_type_number = type.type_number;
			return std::unique_ptr<ast::Node>(std::move(type_name));
		}
	}
	
	// This prevents infinite recursion when an automatically generated member
	// function references an unnamed type.
	bool can_compare_type_numbers = type.type_number.valid() && enclosing_struct && enclosing_struct->type_number.valid();
	if(force_substitute && can_compare_type_numbers && type.type_number == enclosing_struct->type_number) {
		// It's probably a this parameter (or return type) for an unnamed type.
		auto type_name = std::make_unique<ast::TypeName>();
		type_name->source = ast::TypeNameSource::UNNAMED_THIS;
		type_name->unresolved_stabs = std::make_unique<ast::TypeName::UnresolvedStabs>();
		type_name->unresolved_stabs->type_name = enclosing_struct->name.has_value() ? *enclosing_struct->name : "";
		type_name->unresolved_stabs->referenced_file_handle = state.file_handle;
		type_name->unresolved_stabs->stabs_type_number = type.type_number;
		return std::unique_ptr<ast::Node>(std::move(type_name));
	}
	
	if(!type.descriptor.has_value()) {
		// The definition of the type has been defined previously, so we have to
		// look it up by its type number.
		CCC_CHECK(type.type_number.valid(), "Cannot lookup type (type is anonymous).");
		auto stabs_type = state.stabs_types->find(type.type_number);
		if(stabs_type == state.stabs_types->end()) {
			std::string error_message = "Failed to lookup STABS type by its type number ("
				+ std::to_string(type.type_number.file) + "," + std::to_string(type.type_number.type) + ").";
			if(state.importer_flags & STRICT_PARSING) {
				return CCC_FAILURE("%s", error_message.c_str());
			} else {
				CCC_WARN("%s", error_message.c_str());
				std::unique_ptr<ast::Error> error = std::make_unique<ast::Error>();
				error->message = std::move(error_message);
				return std::unique_ptr<ast::Node>(std::move(error));
			}
		}
		return stabs_type_to_ast(
			*stabs_type->second,
			enclosing_struct,
			state,
			depth + 1,
			substitute_type_name,
			force_substitute);
	}
	
	std::unique_ptr<ast::Node> result;
	
	switch(*type.descriptor) {
		case StabsTypeDescriptor::TYPE_REFERENCE: {
			const auto& stabs_type_ref = type.as<StabsTypeReferenceType>();
			if(!type.type_number.valid() || !stabs_type_ref.type->type_number.valid() || stabs_type_ref.type->type_number != type.type_number) {
				auto node = stabs_type_to_ast(
					*stabs_type_ref.type,
					enclosing_struct,
					state,
					depth + 1,
					substitute_type_name,
					force_substitute);
				CCC_RETURN_IF_ERROR(node);
				result = std::move(*node);
			} else {
				// I still don't know why in STABS void is a reference to
				// itself, maybe because I'm not a philosopher.
				auto builtin = std::make_unique<ast::BuiltIn>();
				builtin->bclass = ast::BuiltInClass::VOID_TYPE;
				result = std::move(builtin);
			}
			break;
		}
		case StabsTypeDescriptor::ARRAY: {
			auto array = std::make_unique<ast::Array>();
			const auto& stabs_array = type.as<StabsArrayType>();
			
			auto element_node = stabs_type_to_ast(
				*stabs_array.element_type,
				enclosing_struct,
				state,
				depth + 1,
				true,
				force_substitute);
			CCC_RETURN_IF_ERROR(element_node);
			array->element_type = std::move(*element_node);
			
			const StabsRangeType& index = stabs_array.index_type->as<StabsRangeType>();
			
			char* end = nullptr;
			
			const char* low = index.low.c_str();
			s64 low_value = strtoll(low, &end, 10);
			CCC_CHECK(end != low, "Failed to parse low part of range as integer.");
			CCC_CHECK(low_value == 0, "Invalid index type for array.");
			
			const char* high = index.high.c_str();
			s64 high_value = strtoll(high, &end, 10);
			CCC_CHECK(end != high, "Failed to parse low part of range as integer.");
			
			if(high_value == 4294967295) {
				// Some compilers wrote out a wrapped around value here.
				array->element_count = 0;
			} else {
				array->element_count = (s32) high_value + 1;
			}
			
			result = std::move(array);
			break;
		}
		case StabsTypeDescriptor::ENUM: {
			auto inline_enum = std::make_unique<ast::Enum>();
			const auto& stabs_enum = type.as<StabsEnumType>();
			inline_enum->constants = stabs_enum.fields;
			result = std::move(inline_enum);
			break;
		}
		case StabsTypeDescriptor::FUNCTION: {
			auto function = std::make_unique<ast::Function>();
			
			auto node = stabs_type_to_ast(
				*type.as<StabsFunctionType>().return_type,
				enclosing_struct,
				state,
				depth + 1,
				true,
				force_substitute);
			CCC_RETURN_IF_ERROR(node);
			function->return_type = std::move(*node);
			
			result = std::move(function);
			break;
		}
		case StabsTypeDescriptor::VOLATILE_QUALIFIER: {
			const auto& volatile_qualifier = type.as<StabsVolatileQualifierType>();
			
			auto node = stabs_type_to_ast(
				*volatile_qualifier.type.get(),
				enclosing_struct,
				state,
				depth + 1,
				substitute_type_name,
				force_substitute);
			CCC_RETURN_IF_ERROR(node);
			result = std::move(*node);
			
			result->is_volatile = true;
			break;
		}
		case StabsTypeDescriptor::CONST_QUALIFIER: {
			const auto& const_qualifier = type.as<StabsConstQualifierType>();
			
			auto node = stabs_type_to_ast(
				*const_qualifier.type.get(),
				enclosing_struct,
				state,
				depth + 1,
				substitute_type_name,
				force_substitute);
			result = std::move(*node);
			
			result->is_const = true;
			break;
		}
		case StabsTypeDescriptor::RANGE: {
			auto builtin = std::make_unique<ast::BuiltIn>();
			Result<ast::BuiltInClass> bclass = classify_range(type.as<StabsRangeType>());
			CCC_RETURN_IF_ERROR(bclass);
			builtin->bclass = *bclass;
			result = std::move(builtin);
			break;
		}
		case StabsTypeDescriptor::STRUCT:
		case StabsTypeDescriptor::UNION: {
			const StabsStructOrUnionType* stabs_struct_or_union;
			if(type.descriptor == StabsTypeDescriptor::STRUCT) {
				stabs_struct_or_union = &type.as<StabsStructType>();
			} else {
				stabs_struct_or_union = &type.as<StabsUnionType>();
			}
			
			auto struct_or_union = std::make_unique<ast::StructOrUnion>();
			struct_or_union->is_struct = type.descriptor == StabsTypeDescriptor::STRUCT;
			struct_or_union->size_bits = (s32) stabs_struct_or_union->size * 8;
			
			for(const StabsStructOrUnionType::BaseClass& stabs_base_class : stabs_struct_or_union->base_classes) {
				auto base_class = stabs_type_to_ast(
					*stabs_base_class.type,
					&type,
					state,
					depth + 1,
					true,
					force_substitute);
				CCC_RETURN_IF_ERROR(base_class);
				
				(*base_class)->offset_bytes = stabs_base_class.offset;
				(*base_class)->set_access_specifier(stabs_field_visibility_to_access_specifier(stabs_base_class.visibility), state.importer_flags);
				
				if(stabs_base_class.is_virtual) {
					(*base_class)->is_virtual_base_class = true;
				}
				
				struct_or_union->base_classes.emplace_back(std::move(*base_class));
			}
			
			AST_DEBUG_PRINTF("%-*s beginfields\n", depth * 4, "");
			for(const StabsStructOrUnionType::Field& field : stabs_struct_or_union->fields) {
				auto node = field_to_ast(field, type, state, depth);
				CCC_RETURN_IF_ERROR(node);
				struct_or_union->fields.emplace_back(std::move(*node));
			}
			AST_DEBUG_PRINTF("%-*s endfields\n", depth * 4, "");
			
			AST_DEBUG_PRINTF("%-*s beginmemberfuncs\n", depth * 4, "");
			Result<std::vector<std::unique_ptr<ast::Node>>> member_functions =
				member_functions_to_ast(*stabs_struct_or_union, state, depth);
			CCC_RETURN_IF_ERROR(member_functions);
			struct_or_union->member_functions = std::move(*member_functions);
			AST_DEBUG_PRINTF("%-*s endmemberfuncs\n", depth * 4, "");
			
			result = std::move(struct_or_union);
			break;
		}
		case StabsTypeDescriptor::CROSS_REFERENCE: {
			const auto& cross_reference = type.as<StabsCrossReferenceType>();
			auto type_name = std::make_unique<ast::TypeName>();
			type_name->source = ast::TypeNameSource::CROSS_REFERENCE;
			type_name->unresolved_stabs = std::make_unique<ast::TypeName::UnresolvedStabs>();
			type_name->unresolved_stabs->type_name = cross_reference.identifier;
			type_name->unresolved_stabs->type = cross_reference.type;
			result = std::move(type_name);
			break;
		}
		case ccc::StabsTypeDescriptor::FLOATING_POINT_BUILTIN: {
			const auto& fp_builtin = type.as<StabsFloatingPointBuiltInType>();
			auto builtin = std::make_unique<ast::BuiltIn>();
			switch(fp_builtin.bytes) {
				case 1: builtin->bclass = ast::BuiltInClass::UNSIGNED_8; break;
				case 2: builtin->bclass = ast::BuiltInClass::UNSIGNED_16; break;
				case 4: builtin->bclass = ast::BuiltInClass::UNSIGNED_32; break;
				case 8: builtin->bclass = ast::BuiltInClass::UNSIGNED_64; break;
				case 16: builtin->bclass = ast::BuiltInClass::UNSIGNED_128; break;
				default: builtin->bclass = ast::BuiltInClass::UNSIGNED_8; break;
			}
			result = std::move(builtin);
			break;
		}
		case StabsTypeDescriptor::METHOD: {
			const auto& stabs_method = type.as<StabsMethodType>();
			auto function = std::make_unique<ast::Function>();
			
			auto return_node = stabs_type_to_ast(
				*stabs_method.return_type.get(),
				enclosing_struct,
				state,
				depth + 1,
				true,
				true);
			CCC_RETURN_IF_ERROR(return_node);
			function->return_type = std::move(*return_node);
			
			function->parameters.emplace();
			for(const std::unique_ptr<StabsType>& parameter_type : stabs_method.parameter_types) {
				auto parameter_node = stabs_type_to_ast(
					*parameter_type,
					enclosing_struct,
					state,
					depth + 1,
					true,
					true);
				CCC_RETURN_IF_ERROR(parameter_node);
				function->parameters->emplace_back(std::move(*parameter_node));
			}
			result = std::move(function);
			break;
		}
		case StabsTypeDescriptor::POINTER: {
			auto pointer = std::make_unique<ast::PointerOrReference>();
			pointer->is_pointer = true;
			
			auto value_node = stabs_type_to_ast(
				*type.as<StabsPointerType>().value_type,
				enclosing_struct,
				state,
				depth + 1,
				true,
				force_substitute);
			CCC_RETURN_IF_ERROR(value_node);
			pointer->value_type = std::move(*value_node);
			
			result = std::move(pointer);
			break;
		}
		case StabsTypeDescriptor::REFERENCE: {
			auto reference = std::make_unique<ast::PointerOrReference>();
			reference->is_pointer = false;
			
			auto value_node = stabs_type_to_ast(
				*type.as<StabsReferenceType>().value_type,
				enclosing_struct,
				state,
				depth + 1,
				true,
				force_substitute);
			CCC_RETURN_IF_ERROR(value_node);
			reference->value_type = std::move(*value_node);
			
			result = std::move(reference);
			break;
		}
		case StabsTypeDescriptor::TYPE_ATTRIBUTE: {
			const auto& stabs_type_attribute = type.as<StabsSizeTypeAttributeType>();
			
			auto node = stabs_type_to_ast(
				*stabs_type_attribute.type,
				enclosing_struct,
				state,
				depth + 1,
				substitute_type_name,
				force_substitute);
			CCC_RETURN_IF_ERROR(node);
			result = std::move(*node);
			
			result->size_bits = (s32) stabs_type_attribute.size_bits;
			break;
		}
		case StabsTypeDescriptor::POINTER_TO_DATA_MEMBER: {
			const auto& stabs_member_pointer = type.as<StabsPointerToDataMemberType>();
			auto member_pointer = std::make_unique<ast::PointerToDataMember>();
			
			auto class_node = stabs_type_to_ast(
				*stabs_member_pointer.class_type.get(),
				enclosing_struct,
				state,
				depth + 1,
				true,
				true);
			CCC_RETURN_IF_ERROR(class_node);
			member_pointer->class_type = std::move(*class_node);
			
			auto member_node = stabs_type_to_ast(
				*stabs_member_pointer.member_type.get(),
				enclosing_struct,
				state,
				depth + 1,
				true,
				true);
			CCC_RETURN_IF_ERROR(member_node);
			member_pointer->member_type = std::move(*member_node);
			
			result = std::move(member_pointer);
			break;
		}
		case StabsTypeDescriptor::BUILTIN: {
			CCC_CHECK(type.as<StabsBuiltInType>().type_id == 16,
				"Unknown built-in type!");
			auto builtin = std::make_unique<ast::BuiltIn>();
			builtin->bclass = ast::BuiltInClass::BOOL_8;
			result = std::move(builtin);
			break;
		}
	}
	
	CCC_CHECK(result, "Result of stabs_type_to_ast call is nullptr.");
	return result;
}

static bool is_void_like(const StabsType& type)
{
	// Unfortunately, a common case seems to be that various types (most
	// commonly __builtin_va_list) are indistinguishable from void or void*, so
	// we have to output them as a void built-in.
	if(type.descriptor.has_value()) {
		switch(*type.descriptor) {
			case StabsTypeDescriptor::POINTER: {
				return is_void_like(*type.as<StabsPointerType>().value_type.get());
			}
			case StabsTypeDescriptor::TYPE_REFERENCE: {
				return type.as<StabsTypeReferenceType>().type->type_number == type.type_number;
			}
			default: {
				break;
			}
		}
	}
	
	return false;
}

static Result<ast::BuiltInClass> classify_range(const StabsRangeType& type)
{
	const char* low = type.low.c_str();
	const char* high = type.high.c_str();
	
	// Handle some special cases and values that are too large to easily store
	// in a 64-bit integer.
	static const struct { const char* low; const char* high; ast::BuiltInClass classification; } strings[] = {
		{"4", "0", ast::BuiltInClass::FLOAT_32},
		{"000000000000000000000000", "001777777777777777777777", ast::BuiltInClass::UNSIGNED_64},
		{"00000000000000000000000000000000000000000000", "00000000000000000000001777777777777777777777", ast::BuiltInClass::UNSIGNED_64},
		{"0000000000000", "01777777777777777777777", ast::BuiltInClass::UNSIGNED_64}, // IOP
		{"0", "18446744073709551615", ast::BuiltInClass::UNSIGNED_64},
		{"001000000000000000000000", "000777777777777777777777", ast::BuiltInClass::SIGNED_64},
		{"00000000000000000000001000000000000000000000", "00000000000000000000000777777777777777777777", ast::BuiltInClass::SIGNED_64},
		{"01000000000000000000000", "0777777777777777777777", ast::BuiltInClass::SIGNED_64}, // IOP
		{"-9223372036854775808", "9223372036854775807", ast::BuiltInClass::SIGNED_64},
		{"8", "0", ast::BuiltInClass::FLOAT_64},
		{"00000000000000000000000000000000000000000000", "03777777777777777777777777777777777777777777", ast::BuiltInClass::UNSIGNED_128},
		{"02000000000000000000000000000000000000000000", "01777777777777777777777777777777777777777777", ast::BuiltInClass::SIGNED_128},
		{"000000000000000000000000", "0377777777777777777777777777777777", ast::BuiltInClass::UNQUALIFIED_128},
		{"16", "0", ast::BuiltInClass::FLOAT_128},
		{"0", "-1", ast::BuiltInClass::UNQUALIFIED_128} // Old homebrew toolchain
	};
	
	for(const auto& range : strings) {
		if(strcmp(range.low, low) == 0 && strcmp(range.high, high) == 0) {
			return range.classification;
		}
	}
	
	// For smaller values we actually parse the bounds as integers.
	char* end = nullptr;
	s64 low_value = strtoll(type.low.c_str(), &end, low[0] == '0' ? 8 : 10);
	CCC_CHECK(end != low, "Failed to parse low part of range as integer.");
	s64 high_value = strtoll(type.high.c_str(), &end, high[0] == '0' ? 8 : 10);
	CCC_CHECK(end != high, "Failed to parse high part of range as integer.");
	
	static const struct { s64 low; s64 high; ast::BuiltInClass classification; } integers[] = {
		{0, 255, ast::BuiltInClass::UNSIGNED_8},
		{-128, 127, ast::BuiltInClass::SIGNED_8},
		{0, 127, ast::BuiltInClass::UNQUALIFIED_8},
		{0, 65535, ast::BuiltInClass::UNSIGNED_16},
		{-32768, 32767, ast::BuiltInClass::SIGNED_16},
		{0, 4294967295, ast::BuiltInClass::UNSIGNED_32},
		{-2147483648, 2147483647, ast::BuiltInClass::SIGNED_32},
	};
	
	for(const auto& range : integers) {
		if((range.low == low_value || range.low == -low_value) && range.high == high_value) {
			return range.classification;
		}
	}
	
	return CCC_FAILURE("Failed to classify range.");
}

static Result<std::unique_ptr<ast::Node>> field_to_ast(
	const StabsStructOrUnionType::Field& field,
	const StabsType& enclosing_struct,
	const StabsToAstState& state,
	s32 depth)
{
	AST_DEBUG_PRINTF("%-*s  field %s\n", depth * 4, "", field.name.c_str());
	
	Result<bool> is_bitfield = detect_bitfield(field, state);
	CCC_RETURN_IF_ERROR(is_bitfield);
	
	if(*is_bitfield) {
		// Process bitfields.
		auto bitfield_node = stabs_type_to_ast(
			*field.type,
			&enclosing_struct,
			state,
			depth + 1,
			true,
			false);
		CCC_RETURN_IF_ERROR(bitfield_node);
		
		std::unique_ptr<ast::BitField> bitfield = std::make_unique<ast::BitField>();
		bitfield->name = (field.name == " ") ? "" : field.name;
		bitfield->offset_bytes = field.offset_bits / 8;
		bitfield->size_bits = field.size_bits;
		bitfield->underlying_type = std::move(*bitfield_node);
		bitfield->bitfield_offset_bits = field.offset_bits % 8;
		bitfield->set_access_specifier(stabs_field_visibility_to_access_specifier(field.visibility), state.importer_flags);
		
		return std::unique_ptr<ast::Node>(std::move(bitfield));
	} else {
		// Process a normal field.
		Result<std::unique_ptr<ast::Node>> node = stabs_type_to_ast(
			*field.type,
			&enclosing_struct,
			state,
			depth + 1,
			true,
			false);
		CCC_RETURN_IF_ERROR(node);
		
		(*node)->name = field.name;
		(*node)->offset_bytes = field.offset_bits / 8;
		(*node)->size_bits = field.size_bits;
		(*node)->set_access_specifier(stabs_field_visibility_to_access_specifier(field.visibility), state.importer_flags);
		
		if(field.name.starts_with("$vf") || field.name.starts_with("_vptr$") || field.name.starts_with("_vptr.")) {
			(*node)->is_vtable_pointer = true;
		}
		
		if(field.is_static) {
			(*node)->storage_class = STORAGE_CLASS_STATIC;
		}
		
		return node;
	}
}

static Result<bool> detect_bitfield(const StabsStructOrUnionType::Field& field, const StabsToAstState& state)
{
	// Static fields can't be bitfields.
	if(field.is_static) {
		return false;
	}
	
	// Resolve type references.
	const StabsType* type = field.type.get();
	for(s32 i = 0; i < 50; i++) {
		if(!type->descriptor.has_value()) {
			if(!type->type_number.valid()) {
				return false;
			}
			auto next_type = state.stabs_types->find(type->type_number);
			if(next_type == state.stabs_types->end() || next_type->second == type) {
				return false;
			}
			type = next_type->second;
		} else if(type->descriptor == StabsTypeDescriptor::TYPE_REFERENCE) {
			type = type->as<StabsTypeReferenceType>().type.get();
		} else if(type->descriptor == StabsTypeDescriptor::CONST_QUALIFIER) {
			type = type->as<StabsConstQualifierType>().type.get();
		} else if(type->descriptor == StabsTypeDescriptor::VOLATILE_QUALIFIER) {
			type = type->as<StabsVolatileQualifierType>().type.get();
		} else {
			break;
		}
		
		// Prevent an infinite loop if there's a cycle (fatal frame).
		if(i == 49) {
			return false;
		}
	}
	
	// Determine the size of the underlying type.
	s32 underlying_type_size_bits = 0;
	switch(*type->descriptor) {
		case ccc::StabsTypeDescriptor::RANGE: {
			Result<ast::BuiltInClass> bclass = classify_range(type->as<StabsRangeType>());
			CCC_RETURN_IF_ERROR(bclass);
			underlying_type_size_bits = builtin_class_size(*bclass) * 8;
			break;
		}
		case ccc::StabsTypeDescriptor::CROSS_REFERENCE: {
			if(type->as<StabsCrossReferenceType>().type == ast::ForwardDeclaredType::ENUM) {
				underlying_type_size_bits = 32;
			} else {
				return false;
			}
			break;
		}
		case ccc::StabsTypeDescriptor::TYPE_ATTRIBUTE: {
			underlying_type_size_bits = (s32) type->as<StabsSizeTypeAttributeType>().size_bits;
			break;
		}
		case ccc::StabsTypeDescriptor::BUILTIN: {
			underlying_type_size_bits = 8; // bool
			break;
		}
		default: {
			return false;
		}
	}
	
	if(underlying_type_size_bits == 0) {
		return false;
	}
	
	return field.size_bits != underlying_type_size_bits;
}

static Result<std::vector<std::unique_ptr<ast::Node>>> member_functions_to_ast(
	const StabsStructOrUnionType& type, const StabsToAstState& state, s32 depth)
{
	if(state.importer_flags & NO_MEMBER_FUNCTIONS) {
		return std::vector<std::unique_ptr<ast::Node>>();
	}
	
	std::string_view type_name_no_template_args;
	if(type.name.has_value()) {
		type_name_no_template_args =
			std::string_view(*type.name).substr(0, type.name->find("<"));
	}
	
	std::vector<std::unique_ptr<ast::Node>> member_functions;
	bool only_special_functions = true;
	
	for(const StabsStructOrUnionType::MemberFunctionSet& function_set : type.member_functions) {
		MemberFunctionInfo info = check_member_function(
			function_set.name, type_name_no_template_args, state.demangler, state.importer_flags);
		
		if(!info.is_special_member_function) {
			only_special_functions = false;
		}
		
		for(const StabsStructOrUnionType::MemberFunction& stabs_func : function_set.overloads) {
			auto node = stabs_type_to_ast(
				*stabs_func.type,
				&type,
				state,
				depth + 1,
				true,
				true);
			CCC_RETURN_IF_ERROR(node);
			
			(*node)->is_constructor_or_destructor = info.is_constructor_or_destructor;
			(*node)->is_special_member_function = info.is_special_member_function;
			(*node)->is_operator_member_function = info.is_operator_member_function;
			
			(*node)->name = info.name;
			(*node)->set_access_specifier(stabs_field_visibility_to_access_specifier(stabs_func.visibility), state.importer_flags);
			
			if((*node)->descriptor == ast::FUNCTION) {
				ast::Function& function = (*node)->as<ast::Function>();
				function.modifier = stabs_func.modifier;
				function.vtable_index = stabs_func.vtable_index;
			}
			
			member_functions.emplace_back(std::move(*node));
		}
	}
	
	if(only_special_functions && (state.importer_flags & INCLUDE_GENERATED_MEMBER_FUNCTIONS) == 0) {
		return std::vector<std::unique_ptr<ast::Node>>();
	}
	
	return member_functions;
}

static MemberFunctionInfo check_member_function(
	const std::string& mangled_name,
	std::string_view type_name_no_template_args,
	const DemanglerFunctions& demangler,
	u32 importer_flags)
{
	MemberFunctionInfo info;
	
	// Some compiler versions output gcc opnames for overloaded operators
	// instead of their proper names.
	if((importer_flags & DONT_DEMANGLE_NAMES) == 0 && demangler.cplus_demangle_opname) {
		char* demangled_name = demangler.cplus_demangle_opname(mangled_name.c_str(), 0);
		if(demangled_name) {
			info.name = demangled_name;
			free(reinterpret_cast<void*>(demangled_name));
		}
	}
	if(info.name.empty()) {
		info.name = mangled_name;
	}
	
	bool is_constructor =
		info.name == "__ct" || // Takes a parameter to decide whether or not to construct virtual base classes.
		info.name == "__comp_ctor" || // Constructs virtual base classes.
		info.name == "__base_ctor"; // Does not construct virtual base classes.
	
	if(!is_constructor && !type_name_no_template_args.empty()) {
		is_constructor |= info.name == type_name_no_template_args; // Named constructor.
	}
	
	bool is_destructor =
		info.name == "__dt" || // Takes parameters to decide whether or not to construct virtual base classes and/or delete the object.
		info.name == "__comp_dtor" || // Destructs virtual base classes.
		info.name == "__base_dtor" || // Does not construct virtual base classes.
		info.name == "__deleting_dtor"; // Destructs virtual base clases then deletes the entire object.
	
	if(!is_destructor && !info.name.empty()) {
		is_destructor |= info.name[0] == '~' && std::string_view(info.name).substr(1) == type_name_no_template_args; // Named destructor.
	}
	
	info.is_constructor_or_destructor = is_constructor || is_destructor || info.name.starts_with("$_");
	info.is_special_member_function = info.is_constructor_or_destructor || info.name == "operator=";
	
	return info;
}

void fix_recursively_emitted_structures(
	ast::StructOrUnion& outer_struct, const std::string& name, StabsTypeNumber type_number, SourceFileHandle file_handle)
{
	// This is a rather peculiar case. For some compiler versions, when a struct
	// or a union defined using a typedef is being emitted and it needs to
	// reference itself from a member function parameter, it will emit its
	// entire definition again in the middle of the first definition, although
	// thankfully it won't recurse more than once.
	//
	// The game Sega Soccer Slam is affected by this. See the PeculiarParameter
	// test case in mdebug_importer_tests.cpp for a bare bones example.
	
	for(std::unique_ptr<ast::Node>& node : outer_struct.member_functions) {
		if(node->descriptor != ast::FUNCTION) {
			continue;
		}
		
		ast::Function& function = node->as<ast::Function>();
		if(!function.parameters.has_value()) {
			continue;
		}
		
		for(std::unique_ptr<ast::Node>& parameter : *function.parameters) {
			if(parameter->descriptor != ast::POINTER_OR_REFERENCE) {
				continue;
			}
			
			ast::PointerOrReference& pointer_or_reference = parameter->as<ast::PointerOrReference>();
			if(pointer_or_reference.value_type->descriptor != ast::STRUCT_OR_UNION) {
				continue;
			}
			
			ast::StructOrUnion& inner_struct = pointer_or_reference.value_type->as<ast::StructOrUnion>();
			
			// Since C++ doesn't allow struct definitions in function parameter
			// lists normally, and most of the time the member function
			// parameters aren't even filled in by GCC, this is a really rare
			// case, so here we only bother to do some very basic checks to
			// verify that the inner struct is similar to the outer struct.
			if(inner_struct.base_classes.size() != outer_struct.base_classes.size()) {
				continue;
			}
			
			if(inner_struct.fields.size() != outer_struct.fields.size()) {
				continue;
			}
			
			if(inner_struct.member_functions.size() != outer_struct.member_functions.size()) {
				continue;
			}
			
			auto type_name = std::make_unique<ast::TypeName>();
			type_name->source = ast::TypeNameSource::REFERENCE;
			type_name->unresolved_stabs = std::make_unique<ast::TypeName::UnresolvedStabs>();
			type_name->unresolved_stabs->type_name = name;
			type_name->unresolved_stabs->referenced_file_handle = file_handle;
			type_name->unresolved_stabs->stabs_type_number = type_number;
			pointer_or_reference.value_type = std::move(type_name);
		}
	}
}

ast::AccessSpecifier stabs_field_visibility_to_access_specifier(StabsStructOrUnionType::Visibility visibility)
{
	ast::AccessSpecifier access_specifier = ast::AS_PUBLIC;
	switch(visibility) {
		case StabsStructOrUnionType::Visibility::NONE: access_specifier = ast::AS_PUBLIC; break;
		case StabsStructOrUnionType::Visibility::PUBLIC: access_specifier = ast::AS_PUBLIC; break;
		case StabsStructOrUnionType::Visibility::PROTECTED: access_specifier = ast::AS_PROTECTED; break;
		case StabsStructOrUnionType::Visibility::PRIVATE: access_specifier = ast::AS_PRIVATE; break;
		case StabsStructOrUnionType::Visibility::PUBLIC_OPTIMIZED_OUT: access_specifier = ast::AS_PUBLIC; break;
	}
	return access_specifier;
}

}
