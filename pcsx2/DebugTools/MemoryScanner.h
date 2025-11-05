// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugInterface.h"
#include "common/Pcsx2Types.h"

#include <atomic>
#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace MemoryScanner
{
	// Type of value to scan for
	enum class ValueType
	{
		U8,
		U16,
		U32,
		U64,
		F32,
		F64
	};

	// Comparison operation for scanning
	enum class Comparison
	{
		Exact,         // Value matches exactly
		NotEqual,      // Value doesn't match
		GreaterThan,   // Value is greater than
		LessThan,      // Value is less than
		Changed,       // Value changed from previous scan
		Unchanged,     // Value unchanged from previous scan
		Increased,     // Value increased from previous scan
		Decreased,     // Value decreased from previous scan
		Relative,      // Value matches within relative delta
		Epsilon        // Value matches within epsilon (float comparison)
	};

	// Value storage for different types
	using Value = std::variant<u8, u16, u32, u64, float, double>;

	// Unique identifier for a scan operation
	using ScanId = u64;

	// Invalid scan ID constant
	static constexpr ScanId INVALID_SCAN_ID = 0;

	// Result of a scan operation
	struct Result
	{
		u32 address;      // Guest memory address
		Value value;      // Current value at this address
		ValueType type;   // Type of value

		Result(u32 addr, Value val, ValueType t)
			: address(addr), value(val), type(t) {}
	};

	// Query parameters for a memory scan
	struct Query
	{
		BreakPointCpu cpu;         // Which CPU's memory space to scan
		u32 begin;                 // Start address (inclusive)
		u32 end;                   // End address (exclusive)
		ValueType type;            // Type of values to scan
		Comparison cmp;            // Comparison operation
		Value value;               // Value to compare against
		double epsilon = 0.0001;   // Epsilon for float comparisons
		double relativeDelta = 0.1; // Relative delta as percentage (0.1 = 10%)

		Query() : cpu(BREAKPOINT_EE), begin(0), end(0), type(ValueType::U32),
				  cmp(Comparison::Exact), value(u32(0)) {}

		Query(BreakPointCpu c, u32 b, u32 e, ValueType t, Comparison cmp, Value v)
			: cpu(c), begin(b), end(e), type(t), cmp(cmp), value(v) {}
	};

	// Specification for dump-on-change functionality
	struct DumpSpec
	{
		ValueType type;            // Type of value to monitor
		bool includeContext;       // Include nearby memory in dump
		u32 contextSize;           // Bytes before/after to include
		bool appendTimestamp;      // Append timestamp to filename

		DumpSpec() : type(ValueType::U32), includeContext(false),
					 contextSize(0), appendTimestamp(true) {}
	};

	// Main MemoryScanner class
	class Scanner
	{
	public:
		Scanner();
		~Scanner();

		// Submit an initial scan across a memory range
		// Returns a unique ScanId for tracking results
		// Requires CPU to be paused
		ScanId SubmitInitial(const Query& query);

		// Submit a rescan operation based on previous scan results
		// Only scans addresses that matched the previous scan
		// Returns a new ScanId for the rescan results
		ScanId SubmitRescan(ScanId previousScanId, const Query& deltaQuery);

		// Cancel an in-progress scan operation
		void Cancel(ScanId scanId);

		// Get results for a completed scan
		// Returns empty vector if scan is still in progress or doesn't exist
		std::vector<Result> Results(ScanId scanId) const;

		// Get status of a scan operation
		enum class ScanStatus
		{
			NotFound,
			InProgress,
			Completed,
			Cancelled,
			Error
		};
		ScanStatus GetStatus(ScanId scanId) const;

		// Set up a memory check that dumps to file when value changes
		// Integrates with CBreakPoints::AddMemCheck
		// Returns true if successfully set up
		bool DumpOnChange(BreakPointCpu cpu, u64 addr, const fs::path& outPath, const DumpSpec& spec);

		// Remove a dump-on-change watch
		bool RemoveDumpWatch(BreakPointCpu cpu, u64 addr);

		// Clear all scan results and watches
		void ClearAll();

		// Get number of active scans
		size_t GetActiveScanCount() const;

		// Static utility methods (public for testing)
		static u32 GetTypeSize(ValueType type);
		static std::string ValueToString(const Value& value, ValueType type);

	private:
		// Internal scan state
		struct ScanState
		{
			Query query;
			std::vector<Result> results;
			ScanStatus status;
			std::atomic<bool> cancelRequested;

			ScanState() : status(ScanStatus::InProgress), cancelRequested(false) {}
		};

		// Internal dump watch state
		struct DumpWatch
		{
			BreakPointCpu cpu;
			u64 addr;
			fs::path outPath;
			DumpSpec spec;
			Value lastValue;
		};

		// Generate next scan ID
		ScanId GenerateId();

		// Perform the actual scanning operation
		void PerformScan(ScanId scanId, std::shared_ptr<ScanState> state);

		// Perform a rescan operation
		void PerformRescan(ScanId scanId, std::shared_ptr<ScanState> state,
						   const std::vector<Result>& previousResults);

		// Read a value from memory based on type
		Value ReadValue(DebugInterface& debugInterface, u32 address, ValueType type, bool& valid) const;

		// Compare values based on comparison type
		bool CompareValues(const Value& current, const Value& reference,
						   Comparison cmp, double epsilon, double relativeDelta) const;

		// Write dump file for a memory change
		void WriteDump(const DumpWatch& watch, const Value& newValue);

		// Thread-safe storage for scan states
		mutable std::mutex m_scansMutex;
		std::unordered_map<ScanId, std::shared_ptr<ScanState>> m_scans;

		// Thread-safe storage for dump watches
		mutable std::mutex m_watchesMutex;
		std::unordered_map<u64, std::unique_ptr<DumpWatch>> m_watches; // keyed by address

		// Scan ID generator
		std::atomic<ScanId> m_nextScanId;

		// Memory check callback for dump-on-change
		static void MemCheckCallback(u32 addr, bool write, int size, u32 pc, void* userData);
	};

	// Global scanner instance
	Scanner& GetGlobalScanner();

} // namespace MemoryScanner
