// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SymbolGuardian.h"

#include "DebugInterface.h"

SymbolGuardian R5900SymbolGuardian;
SymbolGuardian R3000SymbolGuardian;

void SymbolGuardian::Read(ReadCallback callback) const noexcept
{
	std::shared_lock lock(m_big_symbol_lock);
	callback(m_database);
}

void SymbolGuardian::ReadWrite(ReadWriteCallback callback) noexcept
{
	std::unique_lock lock(m_big_symbol_lock);
	callback(m_database);
}

SymbolInfo SymbolGuardian::SymbolStartingAtAddress(
	u32 address, u32 descriptors) const
{
	SymbolInfo info;
	Read([&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_starting_at_address(address, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.name = symbol->name();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

SymbolInfo SymbolGuardian::SymbolAfterAddress(
	u32 address, u32 descriptors) const
{
	SymbolInfo info;
	Read([&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_after_address(address, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.name = symbol->name();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

SymbolInfo SymbolGuardian::SymbolOverlappingAddress(
	u32 address, u32 descriptors) const
{
	SymbolInfo info;
	Read([&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_overlapping_address(address, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.name = symbol->name();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

SymbolInfo SymbolGuardian::SymbolWithName(
	const std::string& name, u32 descriptors) const
{
	SymbolInfo info;
	Read([&](const ccc::SymbolDatabase& database) {
		ccc::SymbolDescriptor descriptor;
		const ccc::Symbol* symbol = database.symbol_with_name(name, descriptors, &descriptor);
		if (!symbol)
			return;

		info.descriptor = descriptor;
		info.handle = symbol->raw_handle();
		info.name = symbol->name();
		info.address = symbol->address();
		info.size = symbol->size();
	});
	return info;
}

bool SymbolGuardian::FunctionExistsWithStartingAddress(u32 address) const
{
	bool exists = false;
	Read([&](const ccc::SymbolDatabase& database) {
		ccc::FunctionHandle handle = database.functions.first_handle_from_starting_address(address);
		exists = handle.valid();
	});
	return exists;
}

bool SymbolGuardian::FunctionExistsThatOverlapsAddress(u32 address) const
{
	bool exists = false;
	Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_overlapping_address(address);
		exists = function != nullptr;
	});
	return exists;
}

FunctionInfo SymbolGuardian::FunctionStartingAtAddress(u32 address) const
{
	FunctionInfo info;
	Read([&](const ccc::SymbolDatabase& database) {
		ccc::FunctionHandle handle = database.functions.first_handle_from_starting_address(address);
		const ccc::Function* function = database.functions.symbol_from_handle(handle);
		if (!function)
			return;

		info.handle = function->handle();
		info.name = function->name();
		info.address = function->address();
		info.size = function->size();
		info.is_no_return = function->is_no_return;
	});
	return info;
}

FunctionInfo SymbolGuardian::FunctionOverlappingAddress(u32 address) const
{
	FunctionInfo info;
	Read([&](const ccc::SymbolDatabase& database) {
		const ccc::Function* function = database.functions.symbol_overlapping_address(address);
		if (!function)
			return;

		info.handle = function->handle();
		info.name = function->name();
		info.address = function->address();
		info.size = function->size();
		info.is_no_return = function->is_no_return;
	});
	return info;
}

void SymbolGuardian::GenerateFunctionHashes(ccc::SymbolDatabase& database, MemoryReader& reader)
{
	for (ccc::Function& function : database.functions)
	{
		std::optional<ccc::FunctionHash> hash = HashFunction(function, reader);
		if (!hash.has_value())
			continue;

		function.set_original_hash(hash->get());
	}
}

void SymbolGuardian::UpdateFunctionHashes(ccc::SymbolDatabase& database, MemoryReader& reader)
{
	for (ccc::Function& function : database.functions)
	{
		if (function.original_hash() == 0)
			continue;

		std::optional<ccc::FunctionHash> hash = HashFunction(function, reader);
		if (!hash.has_value())
			continue;

		function.set_current_hash(*hash);
	}

	for (ccc::SourceFile& source_file : database.source_files)
		source_file.check_functions_match(database);
}

std::optional<ccc::FunctionHash> SymbolGuardian::HashFunction(const ccc::Function& function, MemoryReader& reader)
{
	if (!function.address().valid())
		return std::nullopt;

	if (function.size() == 0 || function.size() > _1mb)
		return std::nullopt;

	ccc::FunctionHash hash;

	for (u32 i = 0; i < function.size() / 4; i++)
	{
		bool valid;
		u32 value = reader.read32(function.address().value + i * 4, valid);
		if (!valid)
			return std::nullopt;

		hash.update(value);
	}

	return hash;
}

void SymbolGuardian::ClearIrxModules()
{
	ReadWrite([&](ccc::SymbolDatabase& database) {
		std::vector<ccc::ModuleHandle> irx_modules;
		for (const ccc::Module& module : m_database.modules)
			if (module.is_irx)
				irx_modules.emplace_back(module.handle());

		for (ccc::ModuleHandle module : irx_modules)
			m_database.destroy_symbols_from_module(module, false);
	});
}
