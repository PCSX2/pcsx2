// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "SymbolGuardian.h"

class DebugInterface;

struct SymbolImporterOptions
{
	std::vector<ccc::SymbolSourceHandle> symbols_to_destroy;
	
};

class SymbolImporter
{
public:
	SymbolImporter(SymbolGuardian& guardian);

	// These functions are used to receive events from the rest of the emulator
	// that are used to determine when symbol tables should be loaded, and
	// should be called from the CPU thread.
	void OnElfChanged(std::vector<u8> elf, const std::string& elf_file_name);

	void AutoAnalyse();

	// Delete all stored symbols and create some default built-ins. Should be
	// called from the CPU thread.
	void Reset();

	// Import symbols from the ELF file, nocash symbols, and scan for functions.
	// Should be called from the CPU thread.
	void AnalyseElf(std::vector<u8> elf, const std::string& elf_file_name);

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

protected:
	SymbolGuardian& m_guardian;

	std::thread m_import_thread;
	std::atomic_bool m_interrupt_import_thread = false;
};

extern SymbolImporter R3000SymbolImporter;
extern SymbolImporter R5900SymbolImporter;
