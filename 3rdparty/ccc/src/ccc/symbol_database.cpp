// This file is part of the Chaos Compiler Collection.
// SPDX-License-Identifier: MIT

#include "symbol_database.h"

#include "ast.h"
#include "importer_flags.h"

namespace ccc {

template <typename SymbolType>
SymbolType* SymbolList<SymbolType>::symbol_from_handle(SymbolHandle<SymbolType> handle)
{
	if(!handle.valid()) {
		return nullptr;
	}
	
	size_t index = binary_search(handle);
	if(index >= m_symbols.size() || m_symbols[index].m_handle != handle) {
		return nullptr;
	}
	
	return &m_symbols[index];
}

template <typename SymbolType>
const SymbolType* SymbolList<SymbolType>::symbol_from_handle(SymbolHandle<SymbolType> handle) const
{
	return const_cast<SymbolList<SymbolType>*>(this)->symbol_from_handle(handle);
}

template <typename SymbolType>
std::vector<SymbolType*> SymbolList<SymbolType>::symbols_from_handles(
	const std::vector<SymbolHandle<SymbolType>>& handles)
{
	std::vector<SymbolType*> result;
	for(SymbolHandle<SymbolType> handle : handles) {
		SymbolType* symbol = symbol_from_handle(handle);
		if(symbol) {
			result.emplace_back(symbol);
		}
	}
	return result;
}

template <typename SymbolType>
std::vector<const SymbolType*> SymbolList<SymbolType>::symbols_from_handles(
	const std::vector<SymbolHandle<SymbolType>>& handles) const
{
	std::vector<const SymbolType*> result;
	for(SymbolHandle<SymbolType> handle : handles) {
		const SymbolType* symbol = symbol_from_handle(handle);
		if(symbol) {
			result.emplace_back(symbol);
		}
	}
	return result;
}

template <typename SymbolType>
std::vector<SymbolType*> SymbolList<SymbolType>::optional_symbols_from_handles(
	const std::optional<std::vector<SymbolHandle<SymbolType>>>& handles)
{
	if(handles.has_value()) {
		return symbols_from_handles(*handles);
	} else {
		return std::vector<SymbolType*>();
	}
}

template <typename SymbolType>
std::vector<const SymbolType*> SymbolList<SymbolType>::optional_symbols_from_handles(
	const std::optional<std::vector<SymbolHandle<SymbolType>>>& handles) const
{
	if(handles.has_value()) {
		return symbols_from_handles(*handles);
	} else {
		return std::vector<const SymbolType*>();
	}
}

template <typename SymbolType>
typename SymbolList<SymbolType>::Iterator SymbolList<SymbolType>::begin()
{
	return m_symbols.begin();
}

template <typename SymbolType>
typename SymbolList<SymbolType>::ConstIterator SymbolList<SymbolType>::begin() const
{
	return m_symbols.begin();
}

template <typename SymbolType>
typename SymbolList<SymbolType>::Iterator SymbolList<SymbolType>::end()
{
	return m_symbols.end();
}

template <typename SymbolType>
typename SymbolList<SymbolType>::ConstIterator SymbolList<SymbolType>::end() const
{
	return m_symbols.end();
}

template <typename SymbolType>
typename SymbolList<SymbolType>::AddressToHandleMapIterators SymbolList<SymbolType>::handles_from_starting_address(Address address) const
{
	auto iterators = m_address_to_handle.equal_range(address.value);
	return {iterators.first, iterators.second};
}

template <typename SymbolType>
typename SymbolList<SymbolType>::AddressToHandleMapIterators SymbolList<SymbolType>::handles_from_address_range(AddressRange range) const
{
	if(range.low.valid()) {
		return {m_address_to_handle.lower_bound(range.low.value), m_address_to_handle.lower_bound(range.high.value)};
	} else if(range.high.valid()) {
		return {m_address_to_handle.begin(), m_address_to_handle.lower_bound(range.high.value)};
	} else {
		return {m_address_to_handle.end(), m_address_to_handle.end()};
	}
}

template <typename SymbolType>
SymbolHandle<SymbolType> SymbolList<SymbolType>::first_handle_from_starting_address(Address address) const
{
	auto iterator = m_address_to_handle.find(address.value);
	if(iterator != m_address_to_handle.end()) {
		return iterator->second;
	} else {
		return SymbolHandle<SymbolType>();
	}
}

template <typename SymbolType>
typename SymbolList<SymbolType>::NameToHandleMapIterators SymbolList<SymbolType>::handles_from_name(const std::string& name) const
{
	auto iterators = m_name_to_handle.equal_range(name);
	return {iterators.first, iterators.second};
}

template <typename SymbolType>
SymbolHandle<SymbolType> SymbolList<SymbolType>::first_handle_after_address(Address address) const
{
	auto iterator = m_address_to_handle.upper_bound(address.value);
	if(iterator != m_address_to_handle.end()) {
		return iterator->second;
	} else {
		return SymbolHandle<SymbolType>();
	}
}

template <typename SymbolType>
SymbolHandle<SymbolType> SymbolList<SymbolType>::first_handle_from_name(const std::string& name) const
{
	auto iterator = m_name_to_handle.find(name);
	if(iterator != m_name_to_handle.end()) {
		return iterator->second;
	} else {
		return SymbolHandle<SymbolType>();
	}
}

template <typename SymbolType>
SymbolType* SymbolList<SymbolType>::symbol_overlapping_address(Address address)
{
	auto iterator = m_address_to_handle.upper_bound(address.value);
	if(iterator != m_address_to_handle.begin()) {
		iterator--; // Find the greatest element that is less than or equal to the address.
		SymbolType* symbol = symbol_from_handle(iterator->second);
		if(symbol && address.value < symbol->address().value + symbol->size()) {
			return symbol;
		}
	}
	return nullptr;
}

template <typename SymbolType>
const SymbolType* SymbolList<SymbolType>::symbol_overlapping_address(Address address) const
{
	return const_cast<SymbolList<SymbolType>*>(this)->symbol_overlapping_address(address);
}

template <typename SymbolType>
s32 SymbolList<SymbolType>::index_from_handle(SymbolHandle<SymbolType> handle) const
{
	if(!handle.valid()) {
		return -1;
	}
	
	size_t index = binary_search(handle);
	if(index >= m_symbols.size() || m_symbols[index].handle() != handle) {
		return -1;
	}
	
	return (s32) index;
}

template <typename SymbolType>
SymbolType& SymbolList<SymbolType>::symbol_from_index(s32 index)
{
	return m_symbols.at(index);
}

template <typename SymbolType>
const SymbolType& SymbolList<SymbolType>::symbol_from_index(s32 index) const
{
	return m_symbols.at(index);
}

template <typename SymbolType>
bool SymbolList<SymbolType>::empty() const
{
	return m_symbols.size() == 0;
}


template <typename SymbolType>
s32 SymbolList<SymbolType>::size() const
{
	return (s32) m_symbols.size();
}

template <typename SymbolType>
Result<SymbolType*> SymbolList<SymbolType>::create_symbol(
	std::string name, Address address, SymbolSourceHandle source, const Module* module_symbol)
{
	u32 handle;
	do {
		handle = m_next_handle;
		CCC_CHECK(handle != UINT32_MAX, "Ran out of handles to use for %s symbols.", SymbolType::NAME);
	} while(!m_next_handle.compare_exchange_weak(handle, handle + 1));
	
	SymbolType& symbol = m_symbols.emplace_back();
	
	symbol.m_handle = handle;
	symbol.m_name = std::move(name);
	symbol.m_source = source;
	
	if(module_symbol) {
		symbol.m_address = address.add_base_address(module_symbol->address());
		symbol.m_module = module_symbol->handle();
	} else {
		symbol.m_address = address;
	}
	
	symbol.on_create();
	
	CCC_ASSERT(symbol.source().valid());
	
	link_address_map(symbol);
	link_name_map(symbol);
	
	return &symbol;
}

template <typename SymbolType>
Result<SymbolType*> SymbolList<SymbolType>::create_symbol(
	std::string name, SymbolSourceHandle source, const Module* module_symbol)
{
	return create_symbol(std::move(name), Address(), source, module_symbol);
}

template <typename SymbolType>
Result<SymbolType*> SymbolList<SymbolType>::create_symbol(
	std::string name, SymbolSourceHandle source, const Module* module_symbol, Address address, u32 importer_flags, DemanglerFunctions demangler)
{
	static const int DMGL_PARAMS = 1 << 0;
	static const int DMGL_RET_POSTFIX = 1 << 5;
	
	std::string demangled_name;
	if constexpr(SymbolType::FLAGS & NAME_NEEDS_DEMANGLING) {
		if((importer_flags & DONT_DEMANGLE_NAMES) == 0 && demangler.cplus_demangle) {
			int demangler_flags = 0;
			if(importer_flags & DEMANGLE_PARAMETERS) demangler_flags |= DMGL_PARAMS;
			if(importer_flags & DEMANGLE_RETURN_TYPE) demangler_flags |= DMGL_RET_POSTFIX;
			char* demangled_name_ptr = demangler.cplus_demangle(name.c_str(), demangler_flags);
			if(demangled_name_ptr) {
				demangled_name = demangled_name_ptr;
				free(reinterpret_cast<void*>(demangled_name_ptr));
			}
		}
	}
	
	std::string& non_mangled_name = demangled_name.empty() ? name : demangled_name;
	
	Result<SymbolType*> symbol = create_symbol(non_mangled_name, address, source, module_symbol);
	CCC_RETURN_IF_ERROR(symbol);
	
	if constexpr(SymbolType::FLAGS & NAME_NEEDS_DEMANGLING) {
		if(!demangled_name.empty()) {
			(*symbol)->set_mangled_name(name);
		}
	}
	
	return symbol;
}

template <typename SymbolType>
bool SymbolList<SymbolType>::move_symbol(SymbolHandle<SymbolType> handle, Address new_address)
{
	SymbolType* symbol = symbol_from_handle(handle);
	if(!symbol) {
		return false;
	}
	
	if(symbol->address() != new_address) {
		unlink_address_map(*symbol);
		symbol->m_address = new_address;
		link_address_map(*symbol);
	}
	
	return true;
}

template <typename SymbolType>
bool SymbolList<SymbolType>::rename_symbol(SymbolHandle<SymbolType> handle, std::string new_name)
{
	SymbolType* symbol = symbol_from_handle(handle);
	if(!symbol) {
		return false;
	}
	
	if(symbol->name() != new_name) {
		unlink_name_map(*symbol);
		symbol->m_name = std::move(new_name);
		link_name_map(*symbol);
	}
	
	return true;
}

template <typename SymbolType>
void SymbolList<SymbolType>::merge_from(SymbolList<SymbolType>& list)
{
	m_address_to_handle.clear();
	m_name_to_handle.clear();
	
	std::vector<SymbolType> lhs = std::move(m_symbols);
	std::vector<SymbolType> rhs = std::move(list.m_symbols);
	
	m_symbols = std::vector<SymbolType>();
	m_symbols.reserve(lhs.size() + rhs.size());
	
	size_t lhs_pos = 0;
	size_t rhs_pos = 0;
	for(;;) {
		SymbolType* symbol;
		if(lhs_pos < lhs.size() && (rhs_pos >= rhs.size() || lhs[lhs_pos].handle() < rhs[rhs_pos].handle())) {
			symbol = &m_symbols.emplace_back(std::move(lhs[lhs_pos++]));
		} else if(rhs_pos < rhs.size()) {
			symbol = &m_symbols.emplace_back(std::move(rhs[rhs_pos++]));
		} else {
			break;
		}
		
		link_address_map(*symbol);
		link_name_map(*symbol);
	}
	
	CCC_ASSERT(m_symbols.size() == lhs.size() + rhs.size());
	
	list.m_symbols.clear();
	list.m_address_to_handle.clear();
	list.m_name_to_handle.clear();
}

template <typename SymbolType>
bool SymbolList<SymbolType>::mark_symbol_for_destruction(SymbolHandle<SymbolType> handle, SymbolDatabase* database)
{
	SymbolType* symbol = symbol_from_handle(handle);
	if(!symbol) {
		return false;
	}
	
	symbol->mark_for_destruction();
	
	symbol->on_destroy(database);
	
	return true;
}

template <typename SymbolType>
void SymbolList<SymbolType>::mark_symbols_from_source_for_destruction(SymbolSourceHandle source, SymbolDatabase* database)
{
	for(SymbolType& symbol : m_symbols) {
		if(symbol.source() != source) {
			continue;
		}
		
		symbol.mark_for_destruction();
		
		symbol.on_destroy(database);
	}
}

template <typename SymbolType>
void SymbolList<SymbolType>::mark_symbols_from_module_for_destruction(ModuleHandle module_handle, SymbolDatabase* database)
{
	for(SymbolType& symbol : m_symbols) {
		if(symbol.module_handle() != module_handle) {
			continue;
		}
		
		symbol.mark_for_destruction();
		
		symbol.on_destroy(database);
	}
}

template <typename SymbolType>
void SymbolList<SymbolType>::destroy_marked_symbols()
{
	std::vector<SymbolType> remaining_symbols;
	for(SymbolType& symbol : m_symbols) {
		if(symbol.m_marked_for_destruction) {
			unlink_address_map(symbol);
			unlink_name_map(symbol);
		} else {
			remaining_symbols.emplace_back(std::move(symbol));
		}
	}
	
	m_symbols = std::move(remaining_symbols);
}

template <typename SymbolType>
void SymbolList<SymbolType>::clear()
{
	m_symbols.clear();
	m_address_to_handle.clear();
	m_name_to_handle.clear();
}

template <typename SymbolType>
size_t SymbolList<SymbolType>::binary_search(SymbolHandle<SymbolType> handle) const
{
	size_t begin = 0;
	size_t end = m_symbols.size();
	
	while(begin < end) {
		size_t mid = (begin + end) / 2;
		if(m_symbols[mid].handle() < handle) {
			begin = mid + 1;
		} else if(m_symbols[mid].handle() > handle) {
			end = mid;
		} else {
			return mid;
		}
	}
	
	return end;
}

template <typename SymbolType>
void SymbolList<SymbolType>::link_address_map(SymbolType& symbol)
{
	if constexpr((SymbolType::FLAGS & WITH_ADDRESS_MAP)) {
		if(symbol.address().valid()) {
			m_address_to_handle.emplace(symbol.address().value, symbol.handle());
		}
	}
}

template <typename SymbolType>
void SymbolList<SymbolType>::unlink_address_map(SymbolType& symbol)
{
	if constexpr(SymbolType::FLAGS & WITH_ADDRESS_MAP) {
		if(symbol.address().valid()) {
			auto iterators = m_address_to_handle.equal_range(symbol.address().value);
			for(auto iterator = iterators.first; iterator != iterators.second; iterator++) {
				if(iterator->second == symbol.handle()) {
					m_address_to_handle.erase(iterator);
					break;
				}
			}
		}
	}
}

template <typename SymbolType>
void SymbolList<SymbolType>::link_name_map(SymbolType& symbol)
{
	if constexpr(SymbolType::FLAGS & WITH_NAME_MAP) {
		m_name_to_handle.emplace(symbol.name(), symbol.handle());
	}
}

template <typename SymbolType>
void SymbolList<SymbolType>::unlink_name_map(SymbolType& symbol)
{
	if constexpr(SymbolType::FLAGS & WITH_NAME_MAP) {
		auto iterators = m_name_to_handle.equal_range(symbol.name());
		for(auto iterator = iterators.first; iterator != iterators.second; iterator++) {
			if(iterator->second == symbol.handle()) {
				m_name_to_handle.erase(iterator);
				break;
			}
		}
	}
}

template <typename SymbolType>
std::atomic<u32> SymbolList<SymbolType>::m_next_handle = 0;

#define CCC_X(SymbolType, symbol_list) template class SymbolList<SymbolType>;
CCC_FOR_EACH_SYMBOL_TYPE_DO_X
#undef CCC_X

// *****************************************************************************

void Symbol::set_type(std::unique_ptr<ast::Node> type)
{
	m_type = std::move(type);
	invalidate_node_handles();
}

// *****************************************************************************

const char* global_storage_location_to_string(GlobalStorageLocation location)
{
	switch(location) {
		case NIL: return "nil";
		case DATA: return "data";
		case BSS: return "bss";
		case ABS: return "abs";
		case SDATA: return "sdata";
		case SBSS: return "sbss";
		case RDATA: return "rdata";
		case COMMON: return "common";
		case SCOMMON: return "scommon";
		case SUNDEFINED: return "sundefined";
	}
	return "";
}

// *****************************************************************************

const std::optional<std::vector<ParameterVariableHandle>>& Function::parameter_variables() const
{
	return m_parameter_variables;
}

void Function::set_parameter_variables(
	std::optional<std::vector<ParameterVariableHandle>> parameter_variables, SymbolDatabase& database)
{
	if(m_parameter_variables.has_value()) {
		for(ParameterVariableHandle parameter_variable_handle : *m_parameter_variables) {
			ParameterVariable* parameter_variable = database.parameter_variables.symbol_from_handle(parameter_variable_handle);
			if(parameter_variable && parameter_variable->m_function == handle()) {
				parameter_variable->m_function = FunctionHandle();
			}
		}
	}
	
	m_parameter_variables = std::move(parameter_variables);
	
	if(m_parameter_variables.has_value()) {
		for(ParameterVariableHandle parameter_variable_handle : *m_parameter_variables) {
			ParameterVariable* parameter_variable = database.parameter_variables.symbol_from_handle(parameter_variable_handle);
			if(parameter_variable) {
				parameter_variable->m_function = handle();
			}
		}
	}
}
	
const std::optional<std::vector<LocalVariableHandle>>& Function::local_variables() const
{
	return m_local_variables;
}

void Function::set_local_variables(
	std::optional<std::vector<LocalVariableHandle>> local_variables, SymbolDatabase& database)
{
	if(m_local_variables.has_value()) {
		for(LocalVariableHandle local_variable_handle : *m_local_variables) {
			LocalVariable* local_variable = database.local_variables.symbol_from_handle(local_variable_handle);
			if(local_variable && local_variable->m_function == handle()) {
				local_variable->m_function = FunctionHandle();
			}
		}
	}
	
	m_local_variables = std::move(local_variables);
	
	if(m_local_variables.has_value()) {
		for(LocalVariableHandle local_variable_handle : *m_local_variables) {
			LocalVariable* local_variable = database.local_variables.symbol_from_handle(local_variable_handle);
			if(local_variable) {
				local_variable->m_function = handle();
			}
		}
	}
}

const std::string& Function::mangled_name() const
{
	if(!m_mangled_name.empty()) {
		return m_mangled_name;
	} else {
		return name();
	}
}

void Function::set_mangled_name(std::string mangled)
{
	m_mangled_name = std::move(mangled);
}

u32 Function::original_hash() const
{
	return m_original_hash;
}

void Function::set_original_hash(u32 hash)
{
	m_original_hash = hash;
}

u32 Function::current_hash() const
{
	return m_current_hash;
}

void Function::set_current_hash(FunctionHash hash)
{
	m_current_hash = hash.get();
}

void Function::on_destroy(SymbolDatabase* database)
{
	if(!database) {
		return;
	}
	
	if(m_parameter_variables.has_value()) {
		for(ParameterVariableHandle parameter_variable : *m_parameter_variables) {
			database->parameter_variables.mark_symbol_for_destruction(parameter_variable, database);
		}
	}
	
	if(m_local_variables.has_value()) {
		for(LocalVariableHandle local_variable : *m_local_variables) {
			database->local_variables.mark_symbol_for_destruction(local_variable, database);
		}
	}
}

// *****************************************************************************

const std::string& GlobalVariable::mangled_name() const
{
	if(!m_mangled_name.empty()) {
		return m_mangled_name;
	} else {
		return name();
	}
}

void GlobalVariable::set_mangled_name(std::string mangled)
{
	m_mangled_name = std::move(mangled);
}

// *****************************************************************************

void Module::on_create()
{
	m_module = m_handle;
}

// *****************************************************************************

bool Section::contains_code() const
{
	return name() == ".text";
}

bool Section::contains_data() const
{
	return name() == ".bss"
		|| name() == ".data"
		|| name() == ".lit"
		|| name() == ".lita"
		|| name() == ".lit4"
		|| name() == ".lit8"
		|| name() == ".rdata"
		|| name() == ".rodata"
		|| name() == ".sbss"
		|| name() == ".sdata";
}

// *****************************************************************************

const std::vector<FunctionHandle>& SourceFile::functions() const
{
	return m_functions;
}

void SourceFile::set_functions(std::vector<FunctionHandle> functions, SymbolDatabase& database)
{
	for(FunctionHandle function_handle : m_functions) {
		Function* function = database.functions.symbol_from_handle(function_handle);
		if(function && function->m_source_file == handle()) {
			function->m_source_file = SourceFileHandle();
		}
	}
	
	m_functions = std::move(functions);
	
	for(FunctionHandle function_handle : m_functions) {
		Function* function = database.functions.symbol_from_handle(function_handle);
		if(function) {
			function->m_source_file = handle();
		}
	}
}

const std::vector<GlobalVariableHandle>& SourceFile::global_variables() const
{
	return m_global_variables;
}

void SourceFile::set_global_variables(std::vector<GlobalVariableHandle> global_variables, SymbolDatabase& database)
{
	for(GlobalVariableHandle global_variable_handle : m_global_variables) {
		GlobalVariable* global_variable = database.global_variables.symbol_from_handle(global_variable_handle);
		if(global_variable && global_variable->m_source_file == handle()) {
			global_variable->m_source_file = SourceFileHandle();
		}
	}
	
	m_global_variables = std::move(global_variables);
	
	for(GlobalVariableHandle global_variable_handle : m_global_variables) {
		GlobalVariable* global_variable = database.global_variables.symbol_from_handle(global_variable_handle);
		if(global_variable) {
			global_variable->m_source_file = handle();
		}
	}
}

bool SourceFile::functions_match() const
{
	return m_functions_match;
}

void SourceFile::check_functions_match(const SymbolDatabase& database)
{
	u32 matching = 0;
	u32 modified = 0;
	for(FunctionHandle function_handle : functions()) {
		const ccc::Function* function = database.functions.symbol_from_handle(function_handle);
		if(!function || function->original_hash() == 0) {
			continue;
		}
		
		if(function->current_hash() == function->original_hash()) {
			matching++;
		} else {
			modified++;
		}
	}
	
	m_functions_match = matching >= modified;
}

void SourceFile::on_destroy(SymbolDatabase* database)
{
	if(!database) {
		return;
	}
	
	for(FunctionHandle function : m_functions) {
		database->functions.mark_symbol_for_destruction(function, database);
	}
	
	for(GlobalVariableHandle global_variable : m_global_variables) {
		database->global_variables.mark_symbol_for_destruction(global_variable, database);
	}
}

// *****************************************************************************

void SymbolSource::on_create()
{
	m_source = m_handle;
}

// *****************************************************************************

bool SymbolGroup::is_in_group(const Symbol& symbol) const
{
	return symbol.source() == source && symbol.module_handle() == ModuleHandle(module_symbol);
}

// *****************************************************************************

s32 SymbolDatabase::symbol_count() const
{
	s32 sum = 0;
	#define CCC_X(SymbolType, symbol_list) sum += symbol_list.size();
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
	return sum;
}

const Symbol* SymbolDatabase::symbol_starting_at_address(
	Address address, u32 descriptors, SymbolDescriptor* descriptor_out) const
{
	#define CCC_X(SymbolType, symbol_list) \
		if constexpr(SymbolType::FLAGS & WITH_ADDRESS_MAP) { \
			if(descriptors & SymbolType::DESCRIPTOR) { \
				const SymbolHandle<SymbolType> handle = symbol_list.first_handle_from_starting_address(address); \
				const SymbolType* symbol = symbol_list.symbol_from_handle(handle); \
				if(symbol) { \
					if(descriptor_out) { \
						*descriptor_out = SymbolType::DESCRIPTOR; \
					} \
					return symbol; \
				} \
			} \
		}
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
	return nullptr;
}

const Symbol* SymbolDatabase::symbol_after_address(
	Address address, u32 descriptors, SymbolDescriptor* descriptor_out) const
{
	const Symbol* result = nullptr;
	#define CCC_X(SymbolType, symbol_list) \
		if constexpr(SymbolType::FLAGS & WITH_ADDRESS_MAP) { \
			if(descriptors & SymbolType::DESCRIPTOR) { \
				const SymbolHandle<SymbolType> handle = symbol_list.first_handle_after_address(address); \
				const SymbolType* symbol = symbol_list.symbol_from_handle(handle); \
				if(symbol && (!result || symbol->address() < result->address())) { \
					if(descriptor_out) { \
						*descriptor_out = SymbolType::DESCRIPTOR; \
					} \
					result = symbol; \
				} \
			} \
		}
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
	return result;
}

const Symbol* SymbolDatabase::symbol_overlapping_address(
	Address address, u32 descriptors, SymbolDescriptor* descriptor_out) const
{
	#define CCC_X(SymbolType, symbol_list) \
		if constexpr(SymbolType::FLAGS & WITH_ADDRESS_MAP) { \
			if(descriptors & SymbolType::DESCRIPTOR) { \
				const SymbolType* symbol = symbol_list.symbol_overlapping_address(address); \
				if(symbol) { \
					if(descriptor_out) { \
						*descriptor_out = SymbolType::DESCRIPTOR; \
					} \
					return symbol; \
				} \
			} \
		}
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
	return nullptr;
}

const Symbol* SymbolDatabase::symbol_with_name(
	const std::string& name, u32 descriptors, SymbolDescriptor* descriptor_out) const
{
	#define CCC_X(SymbolType, symbol_list) \
		if constexpr(SymbolType::FLAGS & WITH_ADDRESS_MAP) { \
			if(descriptors & SymbolType::DESCRIPTOR) { \
				const SymbolHandle<SymbolType> handle = symbol_list.first_handle_from_name(name); \
				const SymbolType* symbol = symbol_list.symbol_from_handle(handle); \
				if(symbol) { \
					if(descriptor_out) { \
						*descriptor_out = SymbolType::DESCRIPTOR; \
					} \
					return symbol; \
				} \
			} \
		}
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
	return nullptr;
}

Result<SymbolSourceHandle> SymbolDatabase::get_symbol_source(const std::string& name)
{
	SymbolSourceHandle handle = symbol_sources.first_handle_from_name(name);
	if(!handle.valid()) {
		Result<SymbolSource*> source = symbol_sources.create_symbol(name, SymbolSourceHandle(), nullptr);
		CCC_RETURN_IF_ERROR(source);
		handle = (*source)->handle();
	}
	return handle;
}

Result<DataType*> SymbolDatabase::create_data_type_if_unique(
	std::unique_ptr<ast::Node> node,
	StabsTypeNumber number,
	const char* name,
	SourceFile& source_file,
	const SymbolGroup& group)
{
	auto types_with_same_name = data_types.handles_from_name(name);
	const char* compare_fail_reason = nullptr;
	if(types_with_same_name.begin() == types_with_same_name.end()) {
		// No types with this name have previously been processed.
		Result<DataType*> data_type = data_types.create_symbol(name, group.source, group.module_symbol);
		CCC_RETURN_IF_ERROR(data_type);
		
		(*data_type)->files = {source_file.handle()};
		if(number.type > -1) {
			source_file.stabs_type_number_to_handle[number] = (*data_type)->handle();
		}
		
		(*data_type)->set_type(std::move(node));
		
		return *data_type;
	} else {
		// Types with this name have previously been processed, we need to
		// figure out if this one matches any of the previous ones.
		bool match = false;
		for(auto [key, existing_type_handle] : types_with_same_name) {
			DataType* existing_type = data_types.symbol_from_handle(existing_type_handle);
			CCC_ASSERT(existing_type);
			
			// We don't want to merge together types from different sources or
			// modules so that we can destroy all the types from one source
			// without breaking anything else.
			if(!group.is_in_group(*existing_type)) {
				continue;
			}
			
			CCC_ASSERT(existing_type->type());
			ast::CompareResult compare_result = compare_nodes(*existing_type->type(), *node.get(), this, true);
			if(compare_result.type == ast::CompareResultType::DIFFERS) {
				// The new node doesn't match this existing node.
				bool is_anonymous_enum = existing_type->type()->descriptor == ast::ENUM
					&& existing_type->name().empty();
				if(!is_anonymous_enum) {
					existing_type->compare_fail_reason = compare_fail_reason_to_string(compare_result.fail_reason);
					compare_fail_reason = compare_fail_reason_to_string(compare_result.fail_reason);
				}
			} else {
				// The new node matches this existing node.
				existing_type->files.emplace_back(source_file.handle());
				if(number.type > -1) {
					source_file.stabs_type_number_to_handle[number] = existing_type->handle();
				}
				if(compare_result.type == ast::CompareResultType::MATCHES_FAVOUR_RHS) {
					// The new node almost matches the old one, but the new one
					// is slightly better, so we replace the old type.
					existing_type->set_type(std::move(node));
				}
				match = true;
				break;
			}
		}
		
		if(!match) {
			// This type doesn't match any of the others with the same name
			// that have already been processed.
			Result<DataType*> data_type = data_types.create_symbol(name, group.source, group.module_symbol);
			CCC_RETURN_IF_ERROR(data_type);
			
			(*data_type)->files = {source_file.handle()};
			if(number.type > -1) {
				source_file.stabs_type_number_to_handle[number] = (*data_type)->handle();
			}
			(*data_type)->compare_fail_reason = compare_fail_reason;
			
			(*data_type)->set_type(std::move(node));
			
			return *data_type;
		}
	}
	
	return nullptr;
}

void SymbolDatabase::merge_from(SymbolDatabase& database)
{
	#define CCC_X(SymbolType, symbol_list) symbol_list.merge_from(database.symbol_list);
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
}

void SymbolDatabase::destroy_symbols_from_source(SymbolSourceHandle source, bool destroy_descendants)
{
	SymbolDatabase* database = destroy_descendants ? this : nullptr;
	
	#define CCC_X(SymbolType, symbol_list) symbol_list.mark_symbols_from_source_for_destruction(source, database);
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
	
	destroy_marked_symbols();
}

void SymbolDatabase::destroy_symbols_from_module(ModuleHandle module_handle, bool destroy_descendants)
{
	SymbolDatabase* database = destroy_descendants ? this : nullptr;
	
	#define CCC_X(SymbolType, symbol_list) symbol_list.mark_symbols_from_module_for_destruction(module_handle, database);
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
	
	destroy_marked_symbols();
}

void SymbolDatabase::destroy_marked_symbols()
{
	#define CCC_X(SymbolType, symbol_list) symbol_list.destroy_marked_symbols();
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
}

void SymbolDatabase::clear()
{
	#define CCC_X(SymbolType, symbol_list) symbol_list.clear();
	CCC_FOR_EACH_SYMBOL_TYPE_DO_X
	#undef CCC_X
}

// *****************************************************************************

MultiSymbolHandle::MultiSymbolHandle() {}

template <typename SymbolType>
MultiSymbolHandle::MultiSymbolHandle(const SymbolType& symbol)
	: MultiSymbolHandle(SymbolType::DESCRIPTOR, symbol.raw_handle()) {}

MultiSymbolHandle::MultiSymbolHandle(SymbolDescriptor descriptor, u32 handle)
	: m_descriptor(descriptor)
	, m_handle(handle) {}

bool MultiSymbolHandle::valid() const
{
	return m_handle != (u32) -1;
}

SymbolDescriptor MultiSymbolHandle::descriptor() const
{
	return m_descriptor;
}

u32 MultiSymbolHandle::handle() const
{
	return m_handle;
}

Symbol* MultiSymbolHandle::lookup_symbol(SymbolDatabase& database)
{
	if(m_handle == (u32) -1) {
		return nullptr;
	}
	
	switch(m_descriptor) {
		#define CCC_X(SymbolType, symbol_list) \
			case SymbolType::DESCRIPTOR: \
				return database.symbol_list.symbol_from_handle(m_handle);
		CCC_FOR_EACH_SYMBOL_TYPE_DO_X
		#undef CCC_X
	}
	
	return nullptr;
}

const Symbol* MultiSymbolHandle::lookup_symbol(const SymbolDatabase& database) const
{
	return const_cast<MultiSymbolHandle*>(this)->lookup_symbol(const_cast<SymbolDatabase&>(database));
}

bool MultiSymbolHandle::is_flag_set(SymbolFlag flag) const
{
	if(m_handle != (u32) -1) {
		switch(m_descriptor) {
			#define CCC_X(SymbolType, symbol_list) \
				case SymbolType::DESCRIPTOR: \
					return SymbolType::FLAGS & flag;
			CCC_FOR_EACH_SYMBOL_TYPE_DO_X
			#undef CCC_X
		}
	}
	
	return false;
}

bool MultiSymbolHandle::move_symbol(Address new_address, SymbolDatabase& database) const
{
	if(m_handle != (u32) -1) {
		switch(m_descriptor) {
			#define CCC_X(SymbolType, symbol_list) \
				case SymbolType::DESCRIPTOR: \
					return database.symbol_list.move_symbol(m_handle, new_address);
			CCC_FOR_EACH_SYMBOL_TYPE_DO_X
			#undef CCC_X
		}
	}
	
	return false;
}

bool MultiSymbolHandle::rename_symbol(std::string new_name, SymbolDatabase& database) const
{
	if(m_handle != (u32) -1) {
		switch(m_descriptor) {
			#define CCC_X(SymbolType, symbol_list) \
				case SymbolType::DESCRIPTOR: \
					return database.symbol_list.rename_symbol(m_handle, std::move(new_name));
			CCC_FOR_EACH_SYMBOL_TYPE_DO_X
			#undef CCC_X
		}
	}
	
	return false;
}

bool MultiSymbolHandle::destroy_symbol(SymbolDatabase& database, bool destroy_descendants) const
{
	bool success = false;
	
	if(m_handle != (u32) -1) {
		SymbolDatabase* database_ptr = destroy_descendants ? &database : nullptr;
		
		switch(m_descriptor) {
			#define CCC_X(SymbolType, symbol_list) \
				case SymbolType::DESCRIPTOR: \
					success = database.symbol_list.mark_symbol_for_destruction(m_handle, database_ptr); \
					break;
			CCC_FOR_EACH_SYMBOL_TYPE_DO_X
			#undef CCC_X
		}
	}
	
	if(success) {
		database.destroy_marked_symbols();
	}
	
	return success;
}

#define CCC_X(SymbolType, symbol_list) template MultiSymbolHandle::MultiSymbolHandle(const SymbolType& symbol);
CCC_FOR_EACH_SYMBOL_TYPE_DO_X
#undef CCC_X

// *****************************************************************************

NodeHandle::NodeHandle() {}

NodeHandle::NodeHandle(const ast::Node* node)
	: m_node(node) {}

template <typename SymbolType>
NodeHandle::NodeHandle(const SymbolType& symbol, const ast::Node* node)
	: NodeHandle(SymbolType::DESCRIPTOR, symbol, node) {}

NodeHandle::NodeHandle(SymbolDescriptor descriptor, const Symbol& symbol, const ast::Node* node)
	: m_symbol(descriptor, symbol.raw_handle())
	, m_node(node)
	, m_generation(symbol.generation()) {}

bool NodeHandle::valid() const
{
	return m_node != nullptr;
}

const MultiSymbolHandle& NodeHandle::symbol() const
{
	return m_symbol;
}

const ast::Node* NodeHandle::lookup_node(const SymbolDatabase& database) const
{
	if(m_symbol.valid()) {
		const Symbol* symbol = m_symbol.lookup_symbol(database);
		if(!symbol || symbol->generation() != m_generation) {
			return nullptr;
		}
	}
	return m_node;
}

NodeHandle NodeHandle::handle_for_child(const ast::Node* child_node) const
{
	NodeHandle child_handle;
	child_handle.m_symbol = m_symbol;
	child_handle.m_node = child_node;
	child_handle.m_generation = m_generation;
	return child_handle;
}

#define CCC_X(SymbolType, symbol_list) template NodeHandle::NodeHandle(const SymbolType& symbol, const ast::Node* node);
CCC_FOR_EACH_SYMBOL_TYPE_DO_X
#undef CCC_X

}
