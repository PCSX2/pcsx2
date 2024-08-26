// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <queue>
#include <atomic>
#include <thread>
#include <functional>
#include <shared_mutex>

#include <ccc/symbol_database.h>
#include <ccc/symbol_file.h>

#include "common/Pcsx2Types.h"

class DebugInterface;

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

struct SymbolGuardian
{
public:
	SymbolGuardian();
	SymbolGuardian(const SymbolGuardian& rhs) = delete;
	SymbolGuardian(SymbolGuardian&& rhs) = delete;
	~SymbolGuardian();
	SymbolGuardian& operator=(const SymbolGuardian& rhs) = delete;
	SymbolGuardian& operator=(SymbolGuardian&& rhs) = delete;

	using ReadCallback = std::function<void(const ccc::SymbolDatabase&)>;
	using ReadWriteCallback = std::function<void(ccc::SymbolDatabase&)>;

	// Take a shared lock on the symbol database and run the callback.
	void Read(ReadCallback callback) const noexcept;

	// Take an exclusive lock on the symbol database and run the callback.
	void ReadWrite(ReadWriteCallback callback) noexcept;

	// Delete all stored symbols and create some default built-ins. Should be
	// called from the CPU thread.
	void Reset();

	// Import symbols from the ELF file, nocash symbols, and scan for functions.
	// Should be called from the CPU thread.
	void ImportElf(std::vector<u8> elf, std::string elf_file_name, const std::string& nocash_path);

	// Interrupt the import thread. Should be called from the CPU thread.
	void ShutdownWorkerThread();

	static ccc::ModuleHandle ImportSymbolTables(
		ccc::SymbolDatabase& database, const ccc::SymbolFile& symbol_file, const std::atomic_bool* interrupt);
	static bool ImportNocashSymbols(ccc::SymbolDatabase& database, const std::string& file_name);

	// Compute original hashes for all the functions based on the code stored in
	// the ELF file.
	static void ComputeOriginalFunctionHashes(ccc::SymbolDatabase& database, const ccc::ElfFile& elf);

	// Compute new hashes for all the functions to check if any of them have
	// been overwritten.
	void UpdateFunctionHashes(DebugInterface& cpu);

	// Delete all symbols from modules that have the "is_irx" flag set.
	void ClearIrxModules();

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

protected:
	ccc::SymbolDatabase m_database;
	mutable std::shared_mutex m_big_symbol_lock;

	std::thread m_import_thread;
	std::atomic_bool m_interrupt_import_thread = false;

	std::queue<ccc::SymbolDatabase> m_load_queue;
	std::mutex m_load_queue_lock;
};

extern SymbolGuardian R5900SymbolGuardian;
extern SymbolGuardian R3000SymbolGuardian;
