// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <atomic>
#include <thread>
#include <functional>
#include <shared_mutex>

#include <ccc/ast.h>
#include <ccc/symbol_database.h>
#include <ccc/symbol_file.h>

#include "common/Pcsx2Types.h"

class MemoryReader;

struct SymbolInfo
{
	std::optional<ccc::SymbolDescriptor> descriptor;
	u32 handle = (u32)-1;
	std::string name;
	ccc::Address address;
	u32 size = 0;
};

struct FunctionInfo
{
	ccc::FunctionHandle handle;
	std::string name;
	ccc::Address address;
	u32 size = 0;
	bool is_no_return = false;
};

// Guardian of the ancient symbols. This class provides a thread safe API for
// accessing the symbol database.
class SymbolGuardian
{
public:
	SymbolGuardian() = default;
	SymbolGuardian(const SymbolGuardian& rhs) = delete;
	SymbolGuardian(SymbolGuardian&& rhs) = delete;
	~SymbolGuardian() = default;
	SymbolGuardian& operator=(const SymbolGuardian& rhs) = delete;
	SymbolGuardian& operator=(SymbolGuardian&& rhs) = delete;

	using ReadCallback = std::function<void(const ccc::SymbolDatabase&)>;
	using ReadWriteCallback = std::function<void(ccc::SymbolDatabase&)>;

	// Take a shared lock on the symbol database and run the callback.
	void Read(ReadCallback callback) const noexcept;

	// Take an exclusive lock on the symbol database and run the callback.
	void ReadWrite(ReadWriteCallback callback) noexcept;

	// Copy commonly used attributes of a symbol into a temporary object.
	SymbolInfo SymbolStartingAtAddress(
		u32 address, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo SymbolAfterAddress(
		u32 address, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo SymbolOverlappingAddress(
		u32 address, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;
	SymbolInfo SymbolWithName(
		const std::string& name, u32 descriptors = ccc::ALL_SYMBOL_TYPES) const;

	bool FunctionExistsWithStartingAddress(u32 address) const;
	bool FunctionExistsThatOverlapsAddress(u32 address) const;

	// Copy commonly used attributes of a function so they can be used by the
	// calling thread without needing to keep the lock held.
	FunctionInfo FunctionStartingAtAddress(u32 address) const;
	FunctionInfo FunctionOverlappingAddress(u32 address) const;

	// Hash all the functions in the database and store the hashes in the
	// original hash field of said objects.
	static void GenerateFunctionHashes(ccc::SymbolDatabase& database, MemoryReader& reader);

	// Hash all the functions in the database that have original hashes and
	// store the results in the current hash fields of said objects.
	static void UpdateFunctionHashes(ccc::SymbolDatabase& database, MemoryReader& reader);

	// Hash a function and return the result.
	static std::optional<ccc::FunctionHash> HashFunction(const ccc::Function& function, MemoryReader& reader);

	// Delete all symbols from modules that have the "is_irx" flag set.
	void ClearIrxModules();

protected:
	ccc::SymbolDatabase m_database;
	mutable std::shared_mutex m_big_symbol_lock;
};

extern SymbolGuardian R5900SymbolGuardian;
extern SymbolGuardian R3000SymbolGuardian;
