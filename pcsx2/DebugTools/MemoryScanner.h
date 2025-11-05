// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "DebugTools/DebugInterface.h"

#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Stub implementation of MemoryScanner API
// This is a placeholder for the full implementation as specified in AGENT.md
// TODO: Implement full MemoryScanner with:
//  - Snapshot → scan → rescan across range using DebugInterface::read*
//  - Typed compares: u8/u16/u32/u64/f32/f64, exact/relative/epsilon
//  - Watch & dump: integrate with CBreakPoints::AddMemCheck
//  - Async-friendly submit/cancel API

namespace MemoryScanner
{
	using ScanId = u64;

	enum class ValueType
	{
		U8,
		U16,
		U32,
		U64,
		F32,
		F64,
		String,
		Array
	};

	enum class Comparison
	{
		Equals,
		NotEquals,
		GreaterThan,
		GreaterThanOrEqual,
		LessThan,
		LessThanOrEqual,
		Increased,
		IncreasedBy,
		Decreased,
		DecreasedBy,
		Changed,
		ChangedBy,
		NotChanged
	};

	struct Query
	{
		BreakPointCpu cpu;
		u32 begin;
		u32 end;
		ValueType type;
		Comparison cmp;
		std::string value;
	};

	struct Result
	{
		u32 address;
		ValueType type;
		std::string value;
	};

	struct DumpSpec
	{
		bool includeDisasm = false;
		bool includeRegisters = false;
		u32 contextBytes = 64;
	};

	// Submit initial scan - returns a ScanId for tracking
	// STUB: Not yet implemented
	inline ScanId SubmitInitial(const Query& query)
	{
		// TODO: Implement initial scan logic
		return 0;
	}

	// Submit rescan on previous results - returns new ScanId
	// STUB: Not yet implemented
	inline ScanId SubmitRescan(ScanId previousScan, const Query& deltaQuery)
	{
		// TODO: Implement rescan logic that filters previous results
		return previousScan + 1;
	}

	// Cancel ongoing scan
	// STUB: Not yet implemented
	inline void Cancel(ScanId scanId)
	{
		// TODO: Implement cancellation logic
	}

	// Get results for a scan
	// STUB: Not yet implemented
	inline std::vector<Result> Results(ScanId scanId)
	{
		// TODO: Implement results retrieval
		return {};
	}

	// Set up dump-on-change for an address
	// Integrates with CBreakPoints::AddMemCheck to trigger dump when address changes
	// STUB: Not yet implemented
	inline bool DumpOnChange(BreakPointCpu cpu, u32 addr, const fs::path& outPath, const DumpSpec& spec)
	{
		// TODO: Implement dump-on-change logic
		// Should:
		// 1. Add memory check via CBreakPoints::AddMemCheck
		// 2. Set up callback to dump when triggered
		// 3. Return true if successful
		return false;
	}

	// Remove dump-on-change for an address
	// STUB: Not yet implemented
	inline bool RemoveDumpOnChange(BreakPointCpu cpu, u32 addr)
	{
		// TODO: Implement removal of dump-on-change
		return false;
	}
}
