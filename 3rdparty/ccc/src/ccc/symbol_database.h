// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <atomic>
#include <variant>

#include "util.h"

namespace ccc {

// An X macro for all the symbol types.
#define CCC_FOR_EACH_SYMBOL_TYPE_DO_X \
	CCC_X(DataType, data_types) \
	CCC_X(Function, functions) \
	CCC_X(GlobalVariable, global_variables) \
	CCC_X(Label, labels) \
	CCC_X(LocalVariable, local_variables) \
	CCC_X(Module, modules) \
	CCC_X(ParameterVariable, parameter_variables) \
	CCC_X(Section, sections) \
	CCC_X(SourceFile, source_files) \
	CCC_X(SymbolSource, symbol_sources)

// An enum for all the symbol types.
enum SymbolDescriptor {
	DATA_TYPE          = 1 << 0,
	FUNCTION           = 1 << 1,
	GLOBAL_VARIABLE    = 1 << 2,
	LABEL              = 1 << 3,
	LOCAL_VARIABLE     = 1 << 4,
	MODULE             = 1 << 5,
	PARAMETER_VARIABLE = 1 << 6,
	SECTION            = 1 << 7,
	SOURCE_FILE        = 1 << 8,
	SYMBOL_SOURCE      = 1 << 9
};

enum {
	ALL_SYMBOL_TYPES = 0xffff
};

// Forward declare all the different types of symbol objects.
#define CCC_X(SymbolType, symbol_list) class SymbolType;
CCC_FOR_EACH_SYMBOL_TYPE_DO_X
#undef CCC_X

class SymbolDatabase;

// Strongly typed handles for all of the symbol objects. These are here to solve
// the problem of dangling references to symbols.
template <typename SymbolType>
struct SymbolHandle {
	u32 value = (u32) -1;
	
	SymbolHandle() {}
	SymbolHandle(u32 v) : value(v) {}
	SymbolHandle(const SymbolType* symbol)
		: value(symbol ? symbol->handle().value : (u32) -1) {}
	
	// Check if this symbol handle has been initialised. Note that this doesn't
	// determine whether or not the symbol it points to has been deleted!
	bool valid() const { return value != (u32) -1; }
	
	friend auto operator<=>(const SymbolHandle& lhs, const SymbolHandle& rhs) = default;
};

#define CCC_X(SymbolType, symbol_list) using SymbolType##Handle = SymbolHandle<SymbolType>;
CCC_FOR_EACH_SYMBOL_TYPE_DO_X
#undef CCC_X

enum SymbolFlag {
	NO_SYMBOL_FLAGS = 0,
	WITH_ADDRESS_MAP = 1 << 0,
	WITH_NAME_MAP = 1 << 1,
	NAME_NEEDS_DEMANGLING = 1 << 2
};

// A container class for symbols of a given type that maintains maps of their
// names and addresses depending on the value of SymbolType::FLAGS.
template <typename SymbolType>
class SymbolList {
public:
	// Lookup symbols from their handles using binary search.
	SymbolType* symbol_from_handle(SymbolHandle<SymbolType> handle);
	const SymbolType* symbol_from_handle(SymbolHandle<SymbolType> handle) const;
	
	// Lookup multiple symbols from their handles using binary search.
	std::vector<SymbolType*> symbols_from_handles(const std::vector<SymbolHandle<SymbolType>>& handles);
	std::vector<const SymbolType*> symbols_from_handles(const std::vector<SymbolHandle<SymbolType>>& handles) const;
	std::vector<SymbolType*> optional_symbols_from_handles(const std::optional<std::vector<SymbolHandle<SymbolType>>>& handles);
	std::vector<const SymbolType*> optional_symbols_from_handles(const std::optional<std::vector<SymbolHandle<SymbolType>>>& handles) const;
	
	using Iterator = typename std::vector<SymbolType>::iterator;
	using ConstIterator = typename std::vector<SymbolType>::const_iterator;
	
	// For iterating over all the symbols.
	Iterator begin();
	ConstIterator begin() const;
	Iterator end();
	ConstIterator end() const;
	
	using AddressToHandleMap = std::multimap<u32, SymbolHandle<SymbolType>>;
	using NameToHandleMap = std::multimap<std::string, SymbolHandle<SymbolType>>;
	
	template <typename Iterator>
	class Iterators {
	public:
		Iterators(Iterator b, Iterator e)
			: m_begin(b), m_end(e) {}
		Iterator begin() const { return m_begin; }
		Iterator end() const { return m_end; }
	protected:
		Iterator m_begin;
		Iterator m_end;
	};
	
	using AddressToHandleMapIterators = Iterators<typename AddressToHandleMap::const_iterator>;
	using NameToHandleMapIterators = Iterators<typename NameToHandleMap::const_iterator>;
	
	// Lookup symbols by their address.
	AddressToHandleMapIterators handles_from_starting_address(Address address) const;
	AddressToHandleMapIterators handles_from_address_range(AddressRange range) const;
	SymbolHandle<SymbolType> first_handle_from_starting_address(Address address) const;
	SymbolHandle<SymbolType> first_handle_after_address(Address address) const;
	
	// Lookup symbols by their name.
	NameToHandleMapIterators handles_from_name(const std::string& name) const;
	SymbolHandle<SymbolType> first_handle_from_name(const std::string& name) const;
	
	// Find a symbol with an address range that contains the provided address.
	// For example, to find which function an instruction belongs to.
	SymbolType* symbol_overlapping_address(Address address);
	const SymbolType* symbol_overlapping_address(Address address) const;
	
	// Convert handles to underlying array indices.
	s32 index_from_handle(SymbolHandle<SymbolType> handle) const;
	
	// Index into the underlying array.
	SymbolType& symbol_from_index(s32 index);
	const SymbolType& symbol_from_index(s32 index) const;
	
	// Determine if any symbols are being stored.
	bool empty() const;
	
	// Retrieve the number of symbols stored.
	s32 size() const;
	
	// Create a new symbol. If it's a SymbolSource symbol, source can be left
	// empty, otherwise it has to be valid.
	Result<SymbolType*> create_symbol(
		std::string name, Address address, SymbolSourceHandle source, const Module* module_symbol = nullptr);
	
	// Create a new symbol. Similar to above, but for symbols without addresses.
	Result<SymbolType*> create_symbol(
		std::string name, SymbolSourceHandle source, const Module* module_symbol = nullptr);
	
	// Create a new symbol. Similar to above, but unless DONT_DEMANGLE_NAMES is
	// set, the name of the symbol will be demangled.
	Result<SymbolType*> create_symbol(
		std::string name,
		SymbolSourceHandle source,
		const Module* module_symbol,
		Address address,
		u32 importer_flags,
		DemanglerFunctions demangler);
	
	// Update the address of a symbol without changing its handle.
	bool move_symbol(SymbolHandle<SymbolType> handle, Address new_address);
	
	// Update the name of a symbol without changing its handle.
	bool rename_symbol(SymbolHandle<SymbolType> handle, std::string new_name);
	
	// Move all the symbols from the passed list into this list.
	void merge_from(SymbolList<SymbolType>& list);
	
	// Mark a symbol for destruction. If the correct symbol database pointer is
	// passed, all descendants will also be marked. For example, marking a
	// function will also mark its parameters and local variables.
	bool mark_symbol_for_destruction(SymbolHandle<SymbolType> handle, SymbolDatabase* database);
	
	// Mark all the symbols from a given symbol source for destruction. For
	// example you can use this to free a symbol table without destroying
	// user-defined symbols. The behaviour for marking descendants is the same
	// as destroy_symbol.
	void mark_symbols_from_source_for_destruction(SymbolSourceHandle source, SymbolDatabase* database);
	
	// Mark all the symbols from a given module for destruction. The behaviour
	// for marking descendants is the same as destroy_symbol.
	void mark_symbols_from_module_for_destruction(ModuleHandle module_handle, SymbolDatabase* database);
	
	// Destroy all symbols that have previously been marked for destruction.
	// This invalidates all pointers to symbols in this list.
	void destroy_marked_symbols();
	
	// Destroy all symbols, but don't reset m_next_handle so we don't have to
	// worry about dangling handles.
	void clear();
	
protected:
	// Do a binary search for a handle, and return either its index, or the
	// index where it could be inserted.
	size_t binary_search(SymbolHandle<SymbolType> handle) const;
	
	// Keep the address map in sync with the symbol list.
	void link_address_map(SymbolType& symbol);
	void unlink_address_map(SymbolType& symbol);
	
	// Keep the name map in sync with the symbol list.
	void link_name_map(SymbolType& symbol);
	void unlink_name_map(SymbolType& symbol);
	
	std::vector<SymbolType> m_symbols;
	AddressToHandleMap m_address_to_handle;
	NameToHandleMap m_name_to_handle;
	
	// We share this between symbol lists of the same type so that we can merge
	// them without having to rewrite all the handles.
	static std::atomic<u32> m_next_handle;
};

// Base class for all the symbols.
class Symbol {
	template <typename SymbolType>
	friend class SymbolList;
public:
	const std::string& name() const { return m_name; }
	u32 raw_handle() const { return m_handle; }
	SymbolSourceHandle source() const { return m_source; }
	ModuleHandle module_handle() const { return m_module; }
	
	Address address() const { return m_address; }
	u32 size() const { return m_size; }
	void set_size(u32 size) { m_size = size; }
	AddressRange address_range() const { return AddressRange(m_address, m_address.get_or_zero() + m_size); }
	
	ast::Node* type() { return m_type.get(); }
	const ast::Node* type() const { return m_type.get(); }
	void set_type(std::unique_ptr<ast::Node> type);
	
	u32 generation() const { return m_generation; }
	
	// This MUST be called after any AST nodes have been created/deleted/moved.
	// For the set_type function this is done for you.
	void invalidate_node_handles() { m_generation++; }
	
	// Mark a single symbol for destruction, not including its descendants.
	void mark_for_destruction() { m_marked_for_destruction = true; }
	bool is_marked_for_destruction() { return m_marked_for_destruction; }
	
protected:
	void on_create() {}
	void on_destroy(SymbolDatabase* database) {}
	
	u32 m_handle = (u32) -1;
	SymbolSourceHandle m_source;
	Address m_address;
	u32 m_size = 0;
	std::string m_name;
	std::unique_ptr<ast::Node> m_type;
	u32 m_generation : 31 = 0;
	u32 m_marked_for_destruction : 1 = false;
	ModuleHandle m_module;
};

// Variable storage types. This is different to whether the variable is a
// global, local or parameter. For example local variables can have global
// storage (static locals).

enum GlobalStorageLocation {
	NIL,
	DATA,
	BSS,
	ABS,
	SDATA,
	SBSS,
	RDATA,
	COMMON,
	SCOMMON,
	SUNDEFINED
};

const char* global_storage_location_to_string(GlobalStorageLocation location);

struct GlobalStorage {
	GlobalStorageLocation location = GlobalStorageLocation::NIL;
	
	GlobalStorage() {}
	friend auto operator<=>(const GlobalStorage& lhs, const GlobalStorage& rhs) = default;
};

struct RegisterStorage {
	s32 dbx_register_number = -1;
	bool is_by_reference;
	
	RegisterStorage() {}
	friend auto operator<=>(const RegisterStorage& lhs, const RegisterStorage& rhs) = default;
};

struct StackStorage {
	s32 stack_pointer_offset = -1;
	
	StackStorage() {}
	friend auto operator<=>(const StackStorage& lhs, const StackStorage& rhs) = default;
};

// The hashing algorithm for functions. If you change this algorithm make sure
// to bump the version number for the JSON format so we can know if a hash was
// generated using the new algorithm or not.
class FunctionHash {
public:
	void update(u32 instruction)
	{
		// Separate out the opcode so that the hash remains the same regardless
		// of if relocations are applied or not.
		u32 opcode = instruction >> 26;
		m_hash = m_hash * 31 + opcode;
	}
	
	u32 get() const
	{
		return m_hash;
	}
	
protected:
	u32 m_hash = 0;
};

// All the different types of symbol objects.

// A C/C++ data type.
class DataType : public Symbol {
	friend SourceFile;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = DATA_TYPE;
	static constexpr const char* NAME = "Data Type";
	static constexpr const u32 FLAGS = WITH_NAME_MAP;
	
	DataTypeHandle handle() const { return m_handle; }
	
	std::vector<SourceFileHandle> files; // List of files for which a given top-level type is present.
	const char* compare_fail_reason = nullptr;
	
	bool not_defined_in_any_translation_unit : 1 = false;
	bool only_defined_in_single_translation_unit : 1 = false;
};

// A function. The type stored is the return type.
class Function : public Symbol {
	friend SourceFile;
	friend SymbolList<Function>;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = FUNCTION;
	static constexpr const char* NAME = "Function";
	static constexpr const u32 FLAGS = WITH_ADDRESS_MAP | WITH_NAME_MAP | NAME_NEEDS_DEMANGLING;
	
	FunctionHandle handle() const { return m_handle; }
	SourceFileHandle source_file() const { return m_source_file; }
	
	const std::optional<std::vector<ParameterVariableHandle>>& parameter_variables() const;
	void set_parameter_variables(std::optional<std::vector<ParameterVariableHandle>> parameter_variables, SymbolDatabase& database);
	
	const std::optional<std::vector<LocalVariableHandle>>& local_variables() const;
	void set_local_variables(std::optional<std::vector<LocalVariableHandle>> local_variables, SymbolDatabase& database);
	
	const std::string& mangled_name() const;
	void set_mangled_name(std::string mangled);
	
	// A hash of all the opcodes in the function, read from file.
	u32 original_hash() const;
	void set_original_hash(u32 hash);
	
	// A hash of all the opcodes in the function, read from memory.
	u32 current_hash() const;
	void set_current_hash(FunctionHash hash);
	
	struct LineNumberPair {
		Address address;
		s32 line_number;
	};

	struct SubSourceFile {
		Address address;
		std::string relative_path;
	};
	
	std::string relative_path;
	StorageClass storage_class;
	s32 stack_frame_size = -1;
	std::vector<LineNumberPair> line_numbers;
	std::vector<SubSourceFile> sub_source_files;
	bool is_member_function_ish = false; // Filled in by fill_in_pointers_to_member_function_definitions.
	bool is_no_return = false;
	
protected:
	void on_destroy(SymbolDatabase* database);
	
	SourceFileHandle m_source_file;
	std::optional<std::vector<ParameterVariableHandle>> m_parameter_variables;
	std::optional<std::vector<LocalVariableHandle>> m_local_variables;
	
	std::string m_mangled_name;
	
	u32 m_original_hash = 0;
	u32 m_current_hash = 0;
};

// A global variable.
class GlobalVariable : public Symbol {
	friend SourceFile;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = GLOBAL_VARIABLE;
	static constexpr const char* NAME = "Global Variable";
	static constexpr u32 FLAGS = WITH_ADDRESS_MAP | WITH_NAME_MAP | NAME_NEEDS_DEMANGLING;
	
	GlobalVariableHandle handle() const { return m_handle; }
	SourceFileHandle source_file() const { return m_source_file; };
	
	const std::string& mangled_name() const;
	void set_mangled_name(std::string mangled);
	
	GlobalStorage storage;
	StorageClass storage_class;
	
protected:
	SourceFileHandle m_source_file;
	std::string m_mangled_name;
};

// A label. This could be a label defined in assembly, C/C++, or just a symbol
// that we can't automatically determine the type of (e.g. SNDLL symbols).
class Label : public Symbol {
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = LABEL;
	static constexpr const char* NAME = "Label";
	static constexpr u32 FLAGS = WITH_ADDRESS_MAP | WITH_NAME_MAP;
	
	LabelHandle handle() const { return m_handle; }
	
	// Indicates that this label should not be used as a function name.
	bool is_junk = false;
};

// A local variable. This includes static local variables which have global
// storage.
class LocalVariable : public Symbol {
	friend Function;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = LOCAL_VARIABLE;
	static constexpr const char* NAME = "Local Variable";
	static constexpr u32 FLAGS = WITH_ADDRESS_MAP;
	
	LocalVariableHandle handle() const { return m_handle; }
	FunctionHandle function() const { return m_function; };
	
	std::variant<GlobalStorage, RegisterStorage, StackStorage> storage;
	AddressRange live_range;
	
protected:
	FunctionHandle m_function;
};

// A program module e.g. an ELF file or an SNDLL file. Every symbol has a module
// field indicating what module the symbol belongs to. This can be used to
// delete all the symbols associated with a given module. Additionally, when a
// valid module pointer is passed to SymbolList<>::create_symbol, the address of
// the symbol will be added to the address of the new symbol.
class Module : public Symbol {
	friend SymbolList<Module>;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = MODULE;
	static constexpr const char* NAME = "Module";
	static constexpr u32 FLAGS = WITH_NAME_MAP;
	
	ModuleHandle handle() const { return m_handle; }
	
	// These are used for IRX modules.
	bool is_irx = false;
	s32 version_major = -1;
	s32 version_minor = -1;
	
protected:
	void on_create();
};

// A parameter variable.
class ParameterVariable : public Symbol {
	friend Function;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = PARAMETER_VARIABLE;
	static constexpr const char* NAME = "Parameter Variable";
	static constexpr u32 FLAGS = NO_SYMBOL_FLAGS;
	
	ParameterVariableHandle handle() const { return m_handle; }
	FunctionHandle function() const { return m_function; };
	
	std::variant<RegisterStorage, StackStorage> storage;
	
protected:
	FunctionHandle m_function;
};

// An ELF section. These are created from the ELF section headers.
class Section : public Symbol {
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = SECTION;
	static constexpr const char* NAME = "Section";
	static constexpr u32 FLAGS = WITH_ADDRESS_MAP | WITH_NAME_MAP;
	
	SectionHandle handle() const { return m_handle; }
	
	// Check if the section name is ".text".
	bool contains_code() const;
	
	// Check for known data section names.
	bool contains_data() const;
};

// A source file (.c or .cpp file). One of these will be created for every
// translation unit in the program (but only if debugging symbols are present).
class SourceFile : public Symbol {
	friend SymbolList<SourceFile>;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = SOURCE_FILE;
	static constexpr const char* NAME = "Source File";
	static constexpr u32 FLAGS = WITH_ADDRESS_MAP | WITH_NAME_MAP;
	
	SourceFileHandle handle() const { return m_handle; }
	const std::string& full_path() const { return name(); }
	
	const std::vector<FunctionHandle>& functions() const;
	void set_functions(std::vector<FunctionHandle> functions, SymbolDatabase& database);
	
	const std::vector<GlobalVariableHandle>& global_variables() const;
	void set_global_variables(std::vector<GlobalVariableHandle> global_variables, SymbolDatabase& database);
	
	// Check whether at least half of the functions associated with the source
	// file match their original hash (meaning they haven't been overwritten).
	bool functions_match() const;
	void check_functions_match(const SymbolDatabase& database);
	
	std::string working_dir;
	std::string command_line_path;
	std::map<StabsTypeNumber, DataTypeHandle> stabs_type_number_to_handle;
	std::set<std::string> toolchain_version_info;
	
protected:
	void on_destroy(SymbolDatabase* database);
	
	std::vector<FunctionHandle> m_functions;
	std::vector<GlobalVariableHandle> m_global_variables;
	bool m_functions_match = true;
};

// A symbol source. Every symbol has a symbol source field indicating how the
// symbol was created. For example, the symbol table importers will each create
// one of these (if it doesn't already exist).
class SymbolSource : public Symbol {
	friend SymbolList<SymbolSource>;
public:
	static constexpr const SymbolDescriptor DESCRIPTOR = SYMBOL_SOURCE;
	static constexpr const char* NAME = "Symbol Source";
	static constexpr u32 FLAGS = WITH_NAME_MAP;
	
	SymbolSourceHandle handle() const { return m_handle; }
	
protected:
	void on_create();
};

// Bundles together all the information needed to identify if a symbol came from
// a specific symbol table import operation. For example, this is used to make
// sure that we don't reference symbols from another symbol table during the
// import process.
struct SymbolGroup {
	SymbolSourceHandle source;
	Module* module_symbol = nullptr;
	
	bool is_in_group(const Symbol& symbol) const;
};

// The symbol database itself. This owns all the symbols.
class SymbolDatabase {
public:
	SymbolList<DataType> data_types;
	SymbolList<Function> functions;
	SymbolList<GlobalVariable> global_variables;
	SymbolList<Label> labels;
	SymbolList<LocalVariable> local_variables;
	SymbolList<Module> modules;
	SymbolList<ParameterVariable> parameter_variables;
	SymbolList<Section> sections;
	SymbolList<SourceFile> source_files;
	SymbolList<SymbolSource> symbol_sources;
	
	// Sum up the symbol counts for each symbol list.
	s32 symbol_count() const;
	
	// Find a symbol of any of the specified types given an address. Symbols of
	// the types specified higher up in the CCC_FOR_EACH_SYMBOL_TYPE_DO_X macro
	// are checked for first.
	const Symbol* symbol_starting_at_address(
		Address address, u32 descriptors = ALL_SYMBOL_TYPES, SymbolDescriptor* descriptor_out = nullptr) const;
	const Symbol* symbol_after_address(
		Address address, u32 descriptors = ALL_SYMBOL_TYPES, SymbolDescriptor* descriptor_out = nullptr) const;
	const Symbol* symbol_overlapping_address(
		Address address, u32 descriptors = ALL_SYMBOL_TYPES, SymbolDescriptor* descriptor_out = nullptr) const;
	
	// Find a symbol of any of the specified types given its name. Symbols of
	// the types specified higher up in the CCC_FOR_EACH_SYMBOL_TYPE_DO_X macro
	// are checked for first.
	const Symbol* symbol_with_name(
		const std::string& name, u32 descriptors = ALL_SYMBOL_TYPES, SymbolDescriptor* descriptor_out = nullptr) const;
	
	// Finds a symbol source object with the given name or creates one if it
	// doesn't already exist.
	Result<SymbolSourceHandle> get_symbol_source(const std::string& name);
	
	// Deduplicate matching data types with the same name. May replace the
	// existing data type with the new one if the new one is better.
	Result<DataType*> create_data_type_if_unique(
		std::unique_ptr<ast::Node> node,
		StabsTypeNumber number,
		const char* name,
		SourceFile& source_file,
		const SymbolGroup& group);
	
	// Move all the symbols in the passed database into this database.
	void merge_from(SymbolDatabase& database);
	
	// Destroy all the symbols from a given symbol source. For example you can
	// use this to free a symbol table without destroying user-defined symbols.
	void destroy_symbols_from_source(SymbolSourceHandle source, bool destroy_descendants);
	
	// Destroy all the symbols from a given module.
	void destroy_symbols_from_module(ModuleHandle module_handle, bool destroy_descendants);
	
	// Destroy all the symbols that have previously been marked for destruction.
	// This invalidates all pointers to symbols in this database.
	void destroy_marked_symbols();
	
	// Destroy all the symbols in the symbol database.
	void clear();
	
	template <typename Callback>
	void for_each_symbol(Callback callback) {
		// Use indices here to avoid iterator invalidation.
		#define CCC_X(SymbolType, symbol_list) \
			for(s32 i = 0; i < symbol_list.size(); i++) { \
				callback(symbol_list.symbol_from_index(i)); \
			}
		CCC_FOR_EACH_SYMBOL_TYPE_DO_X
		#undef CCC_X
	}
};

// A handle to a symbol of any type.
class MultiSymbolHandle {
public:
	// Create an empty multi symbol handle.
	MultiSymbolHandle();
	
	// Create a multi symbol handle of the specified type.
	template <typename SymbolType>
	MultiSymbolHandle(const SymbolType& symbol);
	MultiSymbolHandle(SymbolDescriptor descriptor, u32 handle);
	
	bool valid() const;
	SymbolDescriptor descriptor() const;
	u32 handle() const;
	
	Symbol* lookup_symbol(SymbolDatabase& database);
	const Symbol* lookup_symbol(const SymbolDatabase& database) const;
	
	bool is_flag_set(SymbolFlag flag) const;
	bool move_symbol(Address new_address, SymbolDatabase& database) const;
	bool rename_symbol(std::string new_name, SymbolDatabase& database) const;
	bool destroy_symbol(SymbolDatabase& database, bool destroy_descendants) const;
	
	friend auto operator<=>(const MultiSymbolHandle& lhs, const MultiSymbolHandle& rhs) = default;
	
protected:
	SymbolDescriptor m_descriptor = DATA_TYPE;
	u32 m_handle = (u32) -1;
};

// A handle to an AST node.
class NodeHandle {
	friend SymbolDatabase;
public:
	// Create an empty node handle.
	NodeHandle();
	
	// Create a node handle that will always allow accesses to its node. You
	// should only use this if you know the lifetime of the handle is a subset
	// of the lifetime of the node.
	NodeHandle(const ast::Node* node);
	
	// Create a node handle pointing to an AST node from a given symbol that
	// will prevent accesses to the node if the symbol is deleted.
	template <typename SymbolType>
	NodeHandle(const SymbolType& symbol, const ast::Node* node);
	NodeHandle(SymbolDescriptor descriptor, const Symbol& symbol, const ast::Node* node);
	
	bool valid() const;
	const MultiSymbolHandle& symbol() const;
	
	const ast::Node* lookup_node(const SymbolDatabase& database) const;
	NodeHandle handle_for_child(const ast::Node* child_node) const;
	
	friend auto operator<=>(const NodeHandle& lhs, const NodeHandle& rhs) = default;
	
protected:
	MultiSymbolHandle m_symbol;
	const ast::Node* m_node = nullptr;
	u32 m_generation = 0;
};

}
