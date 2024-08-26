// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#include "ast.h"

#include "importer_flags.h"
#include "symbol_database.h"

namespace ccc::ast {

static bool compare_nodes_and_merge(
	CompareResult& dest, const Node& node_lhs, const Node& node_rhs, const SymbolDatabase* database);
static bool try_to_match_wobbly_typedefs(
	const Node& node_lhs, const Node& node_rhs, const SymbolDatabase& database);

void Node::set_access_specifier(AccessSpecifier specifier, u32 importer_flags)
{
	if((importer_flags & NO_ACCESS_SPECIFIERS) == 0) {
		access_specifier = specifier;
	}
}

std::pair<Node*, DataType*> Node::physical_type(SymbolDatabase& database, s32 max_depth)
{
	Node* type = this;
	DataType* symbol = nullptr;
	for(s32 i = 0; i < max_depth && type->descriptor == TYPE_NAME; i++) {
		DataType* data_type = database.data_types.symbol_from_handle(type->as<TypeName>().data_type_handle);
		if (!data_type || !data_type->type()) {
			break;
		}
		
		type = data_type->type();
		symbol = data_type;
	}
	
	return std::pair(type, symbol);
}

std::pair<const Node*, const DataType*> Node::physical_type(const SymbolDatabase& database, s32 max_depth) const
{
	return const_cast<Node*>(this)->physical_type(const_cast<SymbolDatabase&>(database), max_depth);
}

const char* member_function_modifier_to_string(MemberFunctionModifier modifier)
{
	switch(modifier) {
		case MemberFunctionModifier::NONE: return "none";
		case MemberFunctionModifier::STATIC: return "static";
		case MemberFunctionModifier::VIRTUAL: return "virtual";
	}
	return "";
}

bool StructOrUnion::flatten_fields(
	std::vector<FlatField>& output,
	const DataType* symbol,
	const SymbolDatabase& database,
	bool skip_statics,
	s32 base_offset,
	s32 max_fields,
	s32 max_depth) const
{
	if(max_depth == 0) {
		return false;
	}
	
	for(const std::unique_ptr<Node>& type_name : base_classes) {
		if(type_name->descriptor != TYPE_NAME) {
			continue;
		}
		
		s32 new_base_offset = base_offset + type_name->offset_bytes;
		
		DataTypeHandle handle = type_name->as<TypeName>().data_type_handle;
		const DataType* base_class_symbol = database.data_types.symbol_from_handle(handle);
		if(!base_class_symbol || !base_class_symbol->type() || base_class_symbol->type()->descriptor != STRUCT_OR_UNION) {
			continue;
		}
		
		const StructOrUnion& base_class = base_class_symbol->type()->as<StructOrUnion>();
		if(!base_class.flatten_fields(output, base_class_symbol, database, skip_statics, new_base_offset, max_fields, max_depth - 1)) {
			return false;
		}
	}
	
	for(const std::unique_ptr<Node>& field : fields) {
		if(skip_statics && field->storage_class == STORAGE_CLASS_STATIC) {
			continue;
		}
		
		if((s32) output.size() >= max_fields) {
			return false;
		}
		
		FlatField& flat = output.emplace_back();
		flat.node = field.get();
		flat.symbol = symbol;
		flat.base_offset = base_offset;
	}
	
	return true;
}

const char* type_name_source_to_string(TypeNameSource source)
{
	switch(source) {
		case TypeNameSource::REFERENCE: return "reference";
		case TypeNameSource::CROSS_REFERENCE: return "cross_reference";
		case TypeNameSource::UNNAMED_THIS: return "this";
	}
	return "";
}

const char* forward_declared_type_to_string(ForwardDeclaredType type)
{
	switch(type) {
		case ForwardDeclaredType::STRUCT: return "struct";
		case ForwardDeclaredType::UNION: return "union";
		case ForwardDeclaredType::ENUM: return "enum";
	}
	return "";
}

DataTypeHandle TypeName::data_type_handle_unless_forward_declared() const
{
	if(!is_forward_declared) {
		return data_type_handle;
	} else {
		return DataTypeHandle();
	}
}

CompareResult compare_nodes(
	const Node& node_lhs, const Node& node_rhs, const SymbolDatabase* database, bool check_intrusive_fields)
{
	CompareResult result = CompareResultType::MATCHES_NO_SWAP;
	
	if(node_lhs.descriptor != node_rhs.descriptor) {
		return CompareFailReason::DESCRIPTOR;
	}
	
	if(check_intrusive_fields) {
		if(node_lhs.storage_class != node_rhs.storage_class) {
			// In some cases we can determine that a type was typedef'd for C
			// translation units, but not for C++ translation units, so we need
			// to add a special case for that here.
			if(node_lhs.storage_class == STORAGE_CLASS_TYPEDEF && node_rhs.storage_class == STORAGE_CLASS_NONE) {
				result = CompareResultType::MATCHES_FAVOUR_LHS;
			} else if(node_lhs.storage_class == STORAGE_CLASS_NONE && node_rhs.storage_class == STORAGE_CLASS_TYPEDEF) {
				result = CompareResultType::MATCHES_FAVOUR_RHS;
			} else {
				return CompareFailReason::STORAGE_CLASS;
			}
		}
		
		// Vtable pointers and constructors can sometimes contain type numbers
		// that are different between translation units, so we don't want to
		// compare them.
		bool is_vtable_pointer = node_lhs.is_vtable_pointer && node_rhs.is_vtable_pointer;
		bool is_numbered_constructor = node_lhs.name.starts_with("$_") && node_rhs.name.starts_with("$_");
		if(node_lhs.name != node_rhs.name && !is_vtable_pointer && !is_numbered_constructor) {
			return CompareFailReason::NAME;
		}
		
		if(node_lhs.offset_bytes != node_rhs.offset_bytes) {
			return CompareFailReason::RELATIVE_OFFSET_BYTES;
		}
		
		if(node_lhs.size_bits != node_rhs.size_bits) {
			return CompareFailReason::SIZE_BITS;
		}
		
		if(node_lhs.is_const != node_rhs.is_const) {
			return CompareFailReason::CONSTNESS;
		}
	}
	
	switch(node_lhs.descriptor) {
		case ARRAY: {
			const auto [lhs, rhs] = Node::as<Array>(node_lhs, node_rhs);
			
			if(compare_nodes_and_merge(result, *lhs.element_type.get(), *rhs.element_type.get(), database)) {
				return result;
			}
			
			if(lhs.element_count != rhs.element_count) {
				return CompareFailReason::ARRAY_ELEMENT_COUNT;
			}
			
			break;
		}
		case BITFIELD: {
			const auto [lhs, rhs] = Node::as<BitField>(node_lhs, node_rhs);
			
			if(lhs.bitfield_offset_bits != rhs.bitfield_offset_bits) {
				return CompareFailReason::BITFIELD_OFFSET_BITS;
			}
			
			if(compare_nodes_and_merge(result, *lhs.underlying_type.get(), *rhs.underlying_type.get(), database)) {
				return result;
			}
			
			break;
		}
		case BUILTIN: {
			const auto [lhs, rhs] = Node::as<BuiltIn>(node_lhs, node_rhs);
			
			if(lhs.bclass != rhs.bclass) {
				return CompareFailReason::BUILTIN_CLASS;
			}
			
			break;
		}
		case ENUM: {
			const auto [lhs, rhs] = Node::as<Enum>(node_lhs, node_rhs);
			
			if(lhs.constants != rhs.constants) {
				return CompareFailReason::ENUM_CONSTANTS;
			}
			
			break;
		}
		case ERROR_NODE: {
			break;
		}
		case FUNCTION: {
			const auto [lhs, rhs] = Node::as<Function>(node_lhs, node_rhs);
			
			if(lhs.return_type.has_value() != rhs.return_type.has_value()) {
				return CompareFailReason::FUNCTION_RETURN_TYPE_HAS_VALUE;
			}
			
			if(lhs.return_type.has_value()) {
				if(compare_nodes_and_merge(result, *lhs.return_type->get(), *rhs.return_type->get(), database)) {
					return result;
				}
			}
			
			if(lhs.parameters.has_value() && rhs.parameters.has_value()) {
				if(lhs.parameters->size() != rhs.parameters->size()) {
					return CompareFailReason::FUNCTION_PARAMAETER_COUNT;
				}
				for(size_t i = 0; i < lhs.parameters->size(); i++) {
					if(compare_nodes_and_merge(result, *(*lhs.parameters)[i].get(), *(*rhs.parameters)[i].get(), database)) {
						return result;
					}
				}
			} else if(lhs.parameters.has_value() != rhs.parameters.has_value()) {
				return CompareFailReason::FUNCTION_PARAMETERS_HAS_VALUE;
			}
			
			if(lhs.modifier != rhs.modifier) {
				return CompareFailReason::FUNCTION_MODIFIER;
			}
			
			break;
		}
		case POINTER_OR_REFERENCE: {
			const auto [lhs, rhs] = Node::as<PointerOrReference>(node_lhs, node_rhs);
			
			if(lhs.is_pointer != rhs.is_pointer) {
				return CompareFailReason::DESCRIPTOR;
			}
			
			if(compare_nodes_and_merge(result, *lhs.value_type.get(), *rhs.value_type.get(), database)) {
				return result;
			}
			
			break;
		}
		case POINTER_TO_DATA_MEMBER: {
			const auto [lhs, rhs] = Node::as<PointerToDataMember>(node_lhs, node_rhs);
			
			if(compare_nodes_and_merge(result, *lhs.class_type.get(), *rhs.class_type.get(), database)) {
				return result;
			}
			
			if(compare_nodes_and_merge(result, *lhs.member_type.get(), *rhs.member_type.get(), database)) {
				return result;
			}
			
			break;
		}
		case STRUCT_OR_UNION: {
			const auto [lhs, rhs] = Node::as<StructOrUnion>(node_lhs, node_rhs);
			
			if(lhs.is_struct != rhs.is_struct) {
				return CompareFailReason::DESCRIPTOR;
			}
			
			if(lhs.base_classes.size() != rhs.base_classes.size()) {
				return CompareFailReason::BASE_CLASS_COUNT;
			}
			
			for(size_t i = 0; i < lhs.base_classes.size(); i++) {
				if(compare_nodes_and_merge(result, *lhs.base_classes[i].get(), *rhs.base_classes[i].get(), database)) {
					return result;
				}
			}
			
			if(lhs.fields.size() != rhs.fields.size()) {
				return CompareFailReason::FIELDS_SIZE;
			}
			
			for(size_t i = 0; i < lhs.fields.size(); i++) {
				if(compare_nodes_and_merge(result, *lhs.fields[i].get(), *rhs.fields[i].get(), database)) {
					return result;
				}
			}
			
			if(lhs.member_functions.size() != rhs.member_functions.size()) {
				return CompareFailReason::MEMBER_FUNCTION_COUNT;
			}
			
			for(size_t i = 0; i < lhs.member_functions.size(); i++) {
				if(compare_nodes_and_merge(result, *lhs.member_functions[i].get(), *rhs.member_functions[i].get(), database)) {
					return result;
				}
			}
			
			break;
		}
		case TYPE_NAME: {
			const auto [lhs, rhs] = Node::as<TypeName>(node_lhs, node_rhs);
			
			// Don't check the source so that REFERENCE and CROSS_REFERENCE are
			// treated as the same.
			if(lhs.data_type_handle != rhs.data_type_handle) {
				return CompareFailReason::TYPE_NAME;
			}
			
			const TypeName::UnresolvedStabs* lhs_unresolved_stabs = lhs.unresolved_stabs.get();
			const TypeName::UnresolvedStabs* rhs_unresolved_stabs = rhs.unresolved_stabs.get();
			if(lhs_unresolved_stabs && rhs_unresolved_stabs) {
				if(lhs_unresolved_stabs->type_name != rhs_unresolved_stabs->type_name) {
					return CompareFailReason::TYPE_NAME;
				}
			} else if(lhs_unresolved_stabs || rhs_unresolved_stabs) {
				return CompareFailReason::TYPE_NAME;
			}
			
			break;
		}
	}
	return result;
}

static bool compare_nodes_and_merge(
	CompareResult& dest, const Node& node_lhs, const Node& node_rhs, const SymbolDatabase* database)
{
	CompareResult result = compare_nodes(node_lhs, node_rhs, database, true);
	if(database) {
		if(result.type == CompareResultType::DIFFERS && try_to_match_wobbly_typedefs(node_lhs, node_rhs, *database)) {
			result.type = CompareResultType::MATCHES_FAVOUR_LHS;
		} else if(result.type == CompareResultType::DIFFERS && try_to_match_wobbly_typedefs(node_rhs, node_lhs, *database)) {
			result.type = CompareResultType::MATCHES_FAVOUR_RHS;
		}
	}
	
	if(dest.type != result.type) {
		if(dest.type == CompareResultType::DIFFERS || result.type == CompareResultType::DIFFERS) {
			// If any of the inner types differ, the outer type does too.
			dest.type = CompareResultType::DIFFERS;
		} else if(dest.type == CompareResultType::MATCHES_CONFUSED || result.type == CompareResultType::MATCHES_CONFUSED) {
			// Propagate confusion.
			dest.type = CompareResultType::MATCHES_CONFUSED; 
		} else if(dest.type == CompareResultType::MATCHES_FAVOUR_LHS && result.type == CompareResultType::MATCHES_FAVOUR_RHS) {
			// One of the results favours the LHS node and the other favours the
			// RHS node so we are confused.
			dest.type = CompareResultType::MATCHES_CONFUSED; 
		} else if(dest.type == CompareResultType::MATCHES_FAVOUR_RHS && result.type == CompareResultType::MATCHES_FAVOUR_LHS) {
			// One of the results favours the LHS node and the other favours the
			// RHS node so we are confused.
			dest.type = CompareResultType::MATCHES_CONFUSED; 
		} else if(dest.type == CompareResultType::MATCHES_FAVOUR_LHS || result.type == CompareResultType::MATCHES_FAVOUR_LHS) {
			// One of the results favours the LHS node and the other is neutral
			// so go with the LHS node.
			dest.type = CompareResultType::MATCHES_FAVOUR_LHS;
		} else if(dest.type == CompareResultType::MATCHES_FAVOUR_RHS || result.type == CompareResultType::MATCHES_FAVOUR_RHS) {
			// One of the results favours the RHS node and the other is neutral
			// so go with the RHS node.
			dest.type = CompareResultType::MATCHES_FAVOUR_RHS;
		}
	}
	
	if(dest.fail_reason == CompareFailReason::NONE) {
		dest.fail_reason = result.fail_reason;
	}
	
	return dest.type == CompareResultType::DIFFERS;
}

static bool try_to_match_wobbly_typedefs(
	const Node& type_name_node, const Node& raw_node, const SymbolDatabase& database)
{
	// Detect if one side has a typedef when the other just has the plain type.
	// This was previously a common reason why type deduplication would fail.
	if(type_name_node.descriptor != TYPE_NAME) {
		return false;
	}
	
	const TypeName& type_name = type_name_node.as<TypeName>();
	if(const TypeName::UnresolvedStabs* unresolved_stabs = type_name.unresolved_stabs.get()) {
		if(unresolved_stabs->referenced_file_handle == (u32) -1 || !unresolved_stabs->stabs_type_number.valid()) {
			return false;
		}
		
		const SourceFile* source_file =
			database.source_files.symbol_from_handle(unresolved_stabs->referenced_file_handle);
		CCC_ASSERT(source_file);
		
		auto handle = source_file->stabs_type_number_to_handle.find(unresolved_stabs->stabs_type_number);
		if(handle != source_file->stabs_type_number_to_handle.end()) {
			const DataType* referenced_type = database.data_types.symbol_from_handle(handle->second);
			CCC_ASSERT(referenced_type && referenced_type->type());
			// Don't compare 'intrusive' fields e.g. the offset.
			CompareResult new_result = compare_nodes(*referenced_type->type(), raw_node, &database, false);
			if(new_result.type != CompareResultType::DIFFERS) {
				return true;
			}
		}
	}
	
	return false;
}

const char* compare_fail_reason_to_string(CompareFailReason reason)
{
	switch(reason) {
		case CompareFailReason::NONE: return "error";
		case CompareFailReason::DESCRIPTOR: return "descriptor";
		case CompareFailReason::STORAGE_CLASS: return "storage class";
		case CompareFailReason::NAME: return "name";
		case CompareFailReason::RELATIVE_OFFSET_BYTES: return "relative offset";
		case CompareFailReason::ABSOLUTE_OFFSET_BYTES: return "absolute offset";
		case CompareFailReason::BITFIELD_OFFSET_BITS: return "bitfield offset";
		case CompareFailReason::SIZE_BITS: return "size";
		case CompareFailReason::CONSTNESS: return "constness";
		case CompareFailReason::ARRAY_ELEMENT_COUNT: return "array element count";
		case CompareFailReason::BUILTIN_CLASS: return "builtin class";
		case CompareFailReason::FUNCTION_RETURN_TYPE_HAS_VALUE: return "function return type has value";
		case CompareFailReason::FUNCTION_PARAMAETER_COUNT: return "function paramaeter count";
		case CompareFailReason::FUNCTION_PARAMETERS_HAS_VALUE: return "function parameter";
		case CompareFailReason::FUNCTION_MODIFIER: return "function modifier";
		case CompareFailReason::ENUM_CONSTANTS: return "enum constant";
		case CompareFailReason::BASE_CLASS_COUNT: return "base class count";
		case CompareFailReason::FIELDS_SIZE: return "fields size";
		case CompareFailReason::MEMBER_FUNCTION_COUNT: return "member function count";
		case CompareFailReason::VTABLE_GLOBAL: return "vtable global";
		case CompareFailReason::TYPE_NAME: return "type name";
		case CompareFailReason::VARIABLE_CLASS: return "variable class";
		case CompareFailReason::VARIABLE_TYPE: return "variable type";
		case CompareFailReason::VARIABLE_STORAGE: return "variable storage";
		case CompareFailReason::VARIABLE_BLOCK: return "variable block";
	}
	return "";
}

const char* node_type_to_string(const Node& node)
{
	switch(node.descriptor) {
		case ARRAY: return "array";
		case BITFIELD: return "bitfield";
		case BUILTIN: return "builtin";
		case ENUM: return "enum";
		case ERROR_NODE: return "error";
		case FUNCTION: return "function";
		case POINTER_OR_REFERENCE: {
			const PointerOrReference& pointer_or_reference = node.as<PointerOrReference>();
			if(pointer_or_reference.is_pointer) {
				return "pointer";
			} else {
				return "reference";
			}
		}
		case POINTER_TO_DATA_MEMBER: return "pointer_to_data_member";
		case STRUCT_OR_UNION: {
			const StructOrUnion& struct_or_union = node.as<StructOrUnion>();
			if(struct_or_union.is_struct) {
				return "struct";
			} else {
				return "union";
			}
		}
		case TYPE_NAME: return "type_name";
	}
	return "";
}

const char* storage_class_to_string(StorageClass storage_class)
{
	switch(storage_class) {
		case STORAGE_CLASS_NONE: return "none";
		case STORAGE_CLASS_TYPEDEF: return "typedef";
		case STORAGE_CLASS_EXTERN: return "extern";
		case STORAGE_CLASS_STATIC: return "static";
		case STORAGE_CLASS_AUTO: return "auto";
		case STORAGE_CLASS_REGISTER: return "register";
	}
	return "";
}

const char* access_specifier_to_string(AccessSpecifier specifier)
{
	switch(specifier) {
		case AS_PUBLIC: return "public";
		case AS_PROTECTED: return "protected";
		case AS_PRIVATE: return "private";
	}
	return "";
}

const char* builtin_class_to_string(BuiltInClass bclass)
{
	switch(bclass) {
		case BuiltInClass::VOID_TYPE: return "void";
		case BuiltInClass::UNSIGNED_8: return "8-bit unsigned integer";
		case BuiltInClass::SIGNED_8: return "8-bit signed integer";
		case BuiltInClass::UNQUALIFIED_8: return "8-bit integer";
		case BuiltInClass::BOOL_8: return "8-bit boolean";
		case BuiltInClass::UNSIGNED_16: return "16-bit unsigned integer";
		case BuiltInClass::SIGNED_16: return "16-bit signed integer";
		case BuiltInClass::UNSIGNED_32: return "32-bit unsigned integer";
		case BuiltInClass::SIGNED_32: return "32-bit signed integer";
		case BuiltInClass::FLOAT_32: return "32-bit floating point";
		case BuiltInClass::UNSIGNED_64: return "64-bit unsigned integer";
		case BuiltInClass::SIGNED_64: return "64-bit signed integer";
		case BuiltInClass::FLOAT_64: return "64-bit floating point";
		case BuiltInClass::UNSIGNED_128: return "128-bit unsigned integer";
		case BuiltInClass::SIGNED_128: return "128-bit signed integer";
		case BuiltInClass::UNQUALIFIED_128: return "128-bit integer";
		case BuiltInClass::FLOAT_128: return "128-bit floating point";
	}
	return "";
}

s32 builtin_class_size(BuiltInClass bclass)
{
	switch(bclass) {
		case BuiltInClass::VOID_TYPE: return 0;
		case BuiltInClass::UNSIGNED_8: return 1;
		case BuiltInClass::SIGNED_8: return 1;
		case BuiltInClass::UNQUALIFIED_8: return 1;
		case BuiltInClass::BOOL_8: return 1;
		case BuiltInClass::UNSIGNED_16: return 2;
		case BuiltInClass::SIGNED_16: return 2;
		case BuiltInClass::UNSIGNED_32: return 4;
		case BuiltInClass::SIGNED_32: return 4;
		case BuiltInClass::FLOAT_32: return 4;
		case BuiltInClass::UNSIGNED_64: return 8;
		case BuiltInClass::SIGNED_64: return 8;
		case BuiltInClass::FLOAT_64: return 8;
		case BuiltInClass::UNSIGNED_128: return 16;
		case BuiltInClass::SIGNED_128: return 16;
		case BuiltInClass::UNQUALIFIED_128: return 16;
		case BuiltInClass::FLOAT_128: return 16;
	}
	return 0;
}

}
