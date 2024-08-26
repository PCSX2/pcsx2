// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include "symbol_database.h"

namespace ccc::ast {

enum NodeDescriptor : u8 {
	ARRAY,
	BITFIELD,
	BUILTIN,
	ENUM,
	ERROR_NODE,
	FUNCTION,
	POINTER_OR_REFERENCE,
	POINTER_TO_DATA_MEMBER,
	STRUCT_OR_UNION,
	TYPE_NAME
};

enum AccessSpecifier {
	AS_PUBLIC = 0,
	AS_PROTECTED = 1,
	AS_PRIVATE = 2
};

// To add a new type of node:
//  1. Add it to the NodeDescriptor enum.
//  2. Create a struct for it.
//  3. Add support for it in for_each_node.
//  4. Add support for it in compute_size_bytes_recursive.
//  5. Add support for it in compare_nodes.
//  6. Add support for it in node_type_to_string.
//  7. Add support for it in CppPrinter::ast_node.
//  8. Add support for it in write_json.
//  9. Add support for it in refine_node.
struct Node {
	const NodeDescriptor descriptor;
	u8 is_const : 1 = false;
	u8 is_volatile : 1 = false;
	u8 is_virtual_base_class : 1 = false;
	u8 is_vtable_pointer : 1 = false;
	u8 is_constructor_or_destructor : 1 = false;
	u8 is_special_member_function : 1 = false;
	u8 is_operator_member_function : 1 = false;
	u8 cannot_compute_size : 1 = false;
	u8 storage_class : 4 = STORAGE_CLASS_NONE;
	u8 access_specifier : 2 = AS_PUBLIC;
	
	s32 size_bytes = -1;
	
	// If the name isn't populated for a given node, the name from the last
	// ancestor to have one should be used i.e. when processing the tree you
	// should pass the name down.
	std::string name;
	
	s32 offset_bytes = -1; // Offset relative to start of last inline struct/union.
	s32 size_bits = -1; // Size stored in the .mdebug symbol table, may not be set.
	
	Node(NodeDescriptor d) : descriptor(d) {}
	Node(const Node& rhs) = default;
	virtual ~Node() {}
	
	template <typename SubType>
	SubType& as() {
		CCC_ASSERT(descriptor == SubType::DESCRIPTOR);
		return *static_cast<SubType*>(this);
	}
	
	template <typename SubType>
	const SubType& as() const {
		CCC_ASSERT(descriptor == SubType::DESCRIPTOR);
		return *static_cast<const SubType*>(this);
	}
	
	template <typename SubType>
	static std::pair<const SubType&, const SubType&> as(const Node& lhs, const Node& rhs) {
		CCC_ASSERT(lhs.descriptor == SubType::DESCRIPTOR && rhs.descriptor == SubType::DESCRIPTOR);
		return std::pair<const SubType&, const SubType&>(static_cast<const SubType&>(lhs), static_cast<const SubType&>(rhs));
	}
	
	void set_access_specifier(AccessSpecifier specifier, u32 importer_flags);
	
	// If this node is a type name, repeatedly resolve it to the type it's
	// referencing, otherwise return (this, nullptr).
	std::pair<Node*, DataType*> physical_type(SymbolDatabase& database, s32 max_depth = 100);
	std::pair<const Node*, const DataType*> physical_type(const SymbolDatabase& database, s32 max_depth = 100) const;
};

struct Array : Node {
	std::unique_ptr<Node> element_type;
	s32 element_count = -1;
	
	Array() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = ARRAY;
};

struct BitField : Node {
	s32 bitfield_offset_bits = -1; // Offset relative to the last byte (not the position of the underlying type!).
	std::unique_ptr<Node> underlying_type;
	
	BitField() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = BITFIELD;
};

enum class BuiltInClass {
	VOID_TYPE,
	UNSIGNED_8, SIGNED_8, UNQUALIFIED_8, BOOL_8,
	UNSIGNED_16, SIGNED_16,
	UNSIGNED_32, SIGNED_32, FLOAT_32,
	UNSIGNED_64, SIGNED_64, FLOAT_64,
	UNSIGNED_128, SIGNED_128, UNQUALIFIED_128, FLOAT_128
};

struct BuiltIn : Node {
	BuiltInClass bclass = BuiltInClass::VOID_TYPE;
	
	BuiltIn() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = BUILTIN;
};

struct Enum : Node {
	std::vector<std::pair<s32, std::string>> constants;
	
	Enum() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = ENUM;
};

struct Error : Node {
	std::string message;
	
	Error() : Node(ERROR_NODE) {}
	static const constexpr NodeDescriptor DESCRIPTOR = ERROR_NODE;
};

enum class MemberFunctionModifier {
	NONE,
	STATIC,
	VIRTUAL
};

const char* member_function_modifier_to_string(MemberFunctionModifier modifier);

struct Function : Node {
	std::optional<std::unique_ptr<Node>> return_type;
	std::optional<std::vector<std::unique_ptr<Node>>> parameters;
	MemberFunctionModifier modifier = MemberFunctionModifier::NONE;
	s32 vtable_index = -1;
	FunctionHandle definition_handle; // Filled in by fill_in_pointers_to_member_function_definitions.
	
	Function() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = FUNCTION;
};

struct PointerOrReference : Node {
	bool is_pointer = true;
	std::unique_ptr<Node> value_type;
	
	PointerOrReference() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = POINTER_OR_REFERENCE;
};

struct PointerToDataMember : Node {
	std::unique_ptr<Node> class_type;
	std::unique_ptr<Node> member_type;
	
	PointerToDataMember() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = POINTER_TO_DATA_MEMBER;
};

struct StructOrUnion : Node {
	bool is_struct = true;
	std::vector<std::unique_ptr<Node>> base_classes;
	std::vector<std::unique_ptr<Node>> fields;
	std::vector<std::unique_ptr<Node>> member_functions;
	
	StructOrUnion() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = STRUCT_OR_UNION;
	
	struct FlatField {
		// The field itself.
		const Node* node;
		// The symbol that owns the node.
		const DataType* symbol;
		// Offset of the innermost enclosing base class in the object.
		s32 base_offset = 0;
	};
	
	// Generate a flat list of all the fields in this class as well as all the
	// base classes recursively, but only until the max_fields or max_depth
	// limits are reached. Return true if all the fields were enumerated.
	bool flatten_fields(
		std::vector<FlatField>& output,
		const DataType* symbol,
		const SymbolDatabase& database,
		bool skip_statics,
		s32 base_offset = 0,
		s32 max_fields = 100000,
		s32 max_depth = 100) const;
};

enum class TypeNameSource : u8 {
	REFERENCE, // A STABS type reference.
	CROSS_REFERENCE, // A STABS cross reference.
	UNNAMED_THIS // A this parameter (or return type) referencing an unnamed type.
};

const char* type_name_source_to_string(TypeNameSource source);

enum class ForwardDeclaredType {
	STRUCT,
	UNION,
	ENUM // Should be illegal but STABS supports cross references to enums so it's here.
};

const char* forward_declared_type_to_string(ForwardDeclaredType type);

struct TypeName : Node {
	DataTypeHandle data_type_handle;
	TypeNameSource source = TypeNameSource::REFERENCE;
	bool is_forward_declared = false;
	
	DataTypeHandle data_type_handle_unless_forward_declared() const;
	
	struct UnresolvedStabs {
		std::string type_name;
		SourceFileHandle referenced_file_handle;
		StabsTypeNumber stabs_type_number;
		std::optional<ForwardDeclaredType> type;
	};
	
	std::unique_ptr<UnresolvedStabs> unresolved_stabs;
	
	TypeName() : Node(DESCRIPTOR) {}
	static const constexpr NodeDescriptor DESCRIPTOR = TYPE_NAME;
};

enum class CompareResultType {
	MATCHES_NO_SWAP,    // Both lhs and rhs are identical.
	MATCHES_CONFUSED,   // Both lhs and rhs are almost identical, and we don't which is better.
	MATCHES_FAVOUR_LHS, // Both lhs and rhs are almost identical, but lhs is better.
	MATCHES_FAVOUR_RHS, // Both lhs and rhs are almost identical, but rhs is better.
	DIFFERS,            // The two nodes differ substantially.
};

enum class CompareFailReason {
	NONE,
	DESCRIPTOR,
	STORAGE_CLASS,
	NAME,
	RELATIVE_OFFSET_BYTES,
	ABSOLUTE_OFFSET_BYTES,
	BITFIELD_OFFSET_BITS,
	SIZE_BITS,
	CONSTNESS,
	ARRAY_ELEMENT_COUNT,
	BUILTIN_CLASS,
	FUNCTION_RETURN_TYPE_HAS_VALUE,
	FUNCTION_PARAMAETER_COUNT,
	FUNCTION_PARAMETERS_HAS_VALUE,
	FUNCTION_MODIFIER,
	ENUM_CONSTANTS,
	BASE_CLASS_COUNT,
	FIELDS_SIZE,
	MEMBER_FUNCTION_COUNT,
	VTABLE_GLOBAL,
	TYPE_NAME,
	VARIABLE_CLASS,
	VARIABLE_TYPE,
	VARIABLE_STORAGE,
	VARIABLE_BLOCK
};

struct CompareResult {
	CompareResult(CompareResultType type) : type(type), fail_reason(CompareFailReason::NONE) {}
	CompareResult(CompareFailReason reason) : type(CompareResultType::DIFFERS), fail_reason(reason) {}
	CompareResultType type;
	CompareFailReason fail_reason;
};

// Compare two AST nodes and their children recursively. This will only check
// fields that will be equal for two versions of the same type from different
// translation units.
CompareResult compare_nodes(const Node& lhs, const Node& rhs, const SymbolDatabase* database, bool check_intrusive_fields);

const char* compare_fail_reason_to_string(CompareFailReason reason);
const char* node_type_to_string(const Node& node);
const char* storage_class_to_string(StorageClass storage_class);
const char* access_specifier_to_string(AccessSpecifier specifier);
const char* builtin_class_to_string(BuiltInClass bclass);

s32 builtin_class_size(BuiltInClass bclass);

enum TraversalOrder {
	PREORDER_TRAVERSAL,
	POSTORDER_TRAVERSAL
};

enum ExplorationMode {
	EXPLORE_CHILDREN,
	DONT_EXPLORE_CHILDREN
};

template <typename ThisNode, typename Callback>
void for_each_node(ThisNode& node, TraversalOrder order, Callback callback)
{
	if(order == PREORDER_TRAVERSAL && callback(node) == DONT_EXPLORE_CHILDREN) {
		return;
	}
	switch(node.descriptor) {
		case ARRAY: {
			auto& array = node.template as<Array>();
			for_each_node(*array.element_type.get(), order, callback);
			break;
		}
		case BITFIELD: {
			auto& bitfield = node.template as<BitField>();
			for_each_node(*bitfield.underlying_type.get(), order, callback);
			break;
		}
		case BUILTIN: {
			break;
		}
		case ENUM: {
			break;
		}
		case ERROR_NODE: {
			break;
		}
		case FUNCTION: {
			auto& func = node.template as<Function>();
			if(func.return_type.has_value()) {
				for_each_node(*func.return_type->get(), order, callback);
			}
			if(func.parameters.has_value()) {
				for(auto& child : *func.parameters) {
					for_each_node(*child.get(), order, callback);
				}
			}
			break;
		}
		case POINTER_OR_REFERENCE: {
			auto& pointer_or_reference = node.template as<PointerOrReference>();
			for_each_node(*pointer_or_reference.value_type.get(), order, callback);
			break;
		}
		case POINTER_TO_DATA_MEMBER: {
			auto& pointer = node.template as<PointerToDataMember>();
			for_each_node(*pointer.class_type.get(), order, callback);
			for_each_node(*pointer.member_type.get(), order, callback);
			break;
		}
		case STRUCT_OR_UNION: {
			auto& struct_or_union = node.template as<StructOrUnion>();
			for(auto& child : struct_or_union.base_classes) {
				for_each_node(*child.get(), order, callback);
			}
			for(auto& child : struct_or_union.fields) {
				for_each_node(*child.get(), order, callback);
			}
			for(auto& child : struct_or_union.member_functions) {
				for_each_node(*child.get(), order, callback);
			}
			break;
		}
		case TYPE_NAME: {
			break;
		}
	}
	if(order == POSTORDER_TRAVERSAL) {
		callback(node);
	}
}

}
