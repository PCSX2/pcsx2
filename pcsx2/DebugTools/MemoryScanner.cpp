// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryScanner.h"
#include "Breakpoints.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

namespace MemoryScanner
{
	// Global scanner instance
	static Scanner* s_globalScanner = nullptr;

	Scanner& GetGlobalScanner()
	{
		if (!s_globalScanner)
		{
			s_globalScanner = new Scanner();
		}
		return *s_globalScanner;
	}

	// Constructor
	Scanner::Scanner()
		: m_nextScanId(1)
	{
	}

	// Destructor
	Scanner::~Scanner()
	{
		ClearAll();
	}

	// Generate next scan ID
	ScanId Scanner::GenerateId()
	{
		return m_nextScanId.fetch_add(1);
	}

	// Get type size in bytes
	u32 Scanner::GetTypeSize(ValueType type)
	{
		switch (type)
		{
			case ValueType::U8: return 1;
			case ValueType::U16: return 2;
			case ValueType::U32: return 4;
			case ValueType::U64: return 8;
			case ValueType::F32: return 4;
			case ValueType::F64: return 8;
			default: return 4;
		}
	}

	// Read a value from memory based on type
	Value Scanner::ReadValue(DebugInterface& debugInterface, u32 address, ValueType type, bool& valid) const
	{
		valid = true;
		switch (type)
		{
			case ValueType::U8:
				return debugInterface.read8(address, valid);

			case ValueType::U16:
				return static_cast<u16>(debugInterface.read16(address, valid));

			case ValueType::U32:
				return debugInterface.read32(address, valid);

			case ValueType::U64:
				return debugInterface.read64(address, valid);

			case ValueType::F32:
			{
				u32 intVal = debugInterface.read32(address, valid);
				float floatVal;
				std::memcpy(&floatVal, &intVal, sizeof(float));
				return floatVal;
			}

			case ValueType::F64:
			{
				u64 intVal = debugInterface.read64(address, valid);
				double doubleVal;
				std::memcpy(&doubleVal, &intVal, sizeof(double));
				return doubleVal;
			}

			default:
				valid = false;
				return u32(0);
		}
	}

	// Compare values based on comparison type
	bool Scanner::CompareValues(const Value& current, const Value& reference,
								 Comparison cmp, double epsilon, double relativeDelta) const
	{
		// Helper to get numeric value from variant
		auto toDouble = [](const Value& v) -> double {
			return std::visit([](auto&& arg) -> double {
				return static_cast<double>(arg);
			}, v);
		};

		double curVal = toDouble(current);
		double refVal = toDouble(reference);

		switch (cmp)
		{
			case Comparison::Exact:
				return curVal == refVal;

			case Comparison::NotEqual:
				return curVal != refVal;

			case Comparison::GreaterThan:
				return curVal > refVal;

			case Comparison::LessThan:
				return curVal < refVal;

			case Comparison::Changed:
				return curVal != refVal;

			case Comparison::Unchanged:
				return curVal == refVal;

			case Comparison::Increased:
				return curVal > refVal;

			case Comparison::Decreased:
				return curVal < refVal;

			case Comparison::Relative:
			{
				double delta = std::abs(curVal - refVal);
				double threshold = std::abs(refVal * relativeDelta);
				return delta <= threshold;
			}

			case Comparison::Epsilon:
			{
				double delta = std::abs(curVal - refVal);
				return delta <= epsilon;
			}

			default:
				return false;
		}
	}

	// Convert Value to string
	std::string Scanner::ValueToString(const Value& value, ValueType type)
	{
		std::ostringstream oss;

		switch (type)
		{
			case ValueType::U8:
				oss << "0x" << std::hex << std::setw(2) << std::setfill('0')
					<< static_cast<u32>(std::get<u8>(value));
				break;

			case ValueType::U16:
				oss << "0x" << std::hex << std::setw(4) << std::setfill('0')
					<< std::get<u16>(value);
				break;

			case ValueType::U32:
				oss << "0x" << std::hex << std::setw(8) << std::setfill('0')
					<< std::get<u32>(value);
				break;

			case ValueType::U64:
				oss << "0x" << std::hex << std::setw(16) << std::setfill('0')
					<< std::get<u64>(value);
				break;

			case ValueType::F32:
				oss << std::scientific << std::setprecision(8)
					<< std::get<float>(value);
				break;

			case ValueType::F64:
				oss << std::scientific << std::setprecision(16)
					<< std::get<double>(value);
				break;
		}

		return oss.str();
	}

	// Submit an initial scan
	ScanId Scanner::SubmitInitial(const Query& query)
	{
		// Enforce paused state for full scans
		DebugInterface& debugInterface = DebugInterface::get(query.cpu);
		if (!debugInterface.isCpuPaused())
		{
			// CPU must be paused for memory scanning
			return INVALID_SCAN_ID;
		}

		// Validate query parameters
		if (query.begin >= query.end)
		{
			return INVALID_SCAN_ID;
		}

		// Create new scan state
		ScanId scanId = GenerateId();
		auto state = std::make_shared<ScanState>();
		state->query = query;
		state->status = ScanStatus::InProgress;

		// Store the scan state
		{
			std::lock_guard<std::mutex> lock(m_scansMutex);
			m_scans[scanId] = state;
		}

		// Launch scan in a separate thread (async-friendly)
		std::thread([this, scanId, state]() {
			PerformScan(scanId, state);
		}).detach();

		return scanId;
	}

	// Perform the actual scanning operation
	void Scanner::PerformScan(ScanId scanId, std::shared_ptr<ScanState> state)
	{
		DebugInterface& debugInterface = DebugInterface::get(state->query.cpu);

		// Check if CPU is still paused
		if (!debugInterface.isCpuPaused())
		{
			state->status = ScanStatus::Error;
			return;
		}

		u32 typeSize = GetTypeSize(state->query.type);
		u32 address = state->query.begin;

		while (address < state->query.end && !state->cancelRequested.load())
		{
			// Check alignment based on type size
			if (address % typeSize != 0)
			{
				address = (address + typeSize - 1) & ~(typeSize - 1);
				continue;
			}

			bool valid = false;
			Value currentValue = ReadValue(debugInterface, address, state->query.type, valid);

			if (valid)
			{
				// For initial scan, compare against query value
				if (CompareValues(currentValue, state->query.value,
								  state->query.cmp, state->query.epsilon, state->query.relativeDelta))
				{
					state->results.emplace_back(address, currentValue, state->query.type);
				}
			}

			address += typeSize;
		}

		// Update status
		if (state->cancelRequested.load())
		{
			state->status = ScanStatus::Cancelled;
		}
		else
		{
			state->status = ScanStatus::Completed;
		}
	}

	// Submit a rescan operation
	ScanId Scanner::SubmitRescan(ScanId previousScanId, const Query& deltaQuery)
	{
		// Get previous scan results
		std::vector<Result> previousResults;
		{
			std::lock_guard<std::mutex> lock(m_scansMutex);
			auto it = m_scans.find(previousScanId);
			if (it == m_scans.end() || it->second->status != ScanStatus::Completed)
			{
				return INVALID_SCAN_ID;
			}
			previousResults = it->second->results;
		}

		if (previousResults.empty())
		{
			return INVALID_SCAN_ID;
		}

		// Enforce paused state
		DebugInterface& debugInterface = DebugInterface::get(deltaQuery.cpu);
		if (!debugInterface.isCpuPaused())
		{
			return INVALID_SCAN_ID;
		}

		// Create new scan state for rescan
		ScanId scanId = GenerateId();
		auto state = std::make_shared<ScanState>();
		state->query = deltaQuery;
		state->status = ScanStatus::InProgress;

		// Store the scan state
		{
			std::lock_guard<std::mutex> lock(m_scansMutex);
			m_scans[scanId] = state;
		}

		// Launch rescan in a separate thread
		std::thread([this, scanId, state, previousResults]() {
			PerformRescan(scanId, state, previousResults);
		}).detach();

		return scanId;
	}

	// Perform a rescan operation
	void Scanner::PerformRescan(ScanId scanId, std::shared_ptr<ScanState> state,
								 const std::vector<Result>& previousResults)
	{
		DebugInterface& debugInterface = DebugInterface::get(state->query.cpu);

		// Check if CPU is still paused
		if (!debugInterface.isCpuPaused())
		{
			state->status = ScanStatus::Error;
			return;
		}

		// Rescan only the addresses from previous results
		for (const Result& prevResult : previousResults)
		{
			if (state->cancelRequested.load())
			{
				break;
			}

			bool valid = false;
			Value currentValue = ReadValue(debugInterface, prevResult.address, state->query.type, valid);

			if (valid)
			{
				// For rescans, we might be comparing against the previous value
				Value comparisonValue = state->query.value;

				// If comparison is relative (Changed, Unchanged, Increased, Decreased),
				// use the previous value as reference
				if (state->query.cmp == Comparison::Changed ||
					state->query.cmp == Comparison::Unchanged ||
					state->query.cmp == Comparison::Increased ||
					state->query.cmp == Comparison::Decreased)
				{
					comparisonValue = prevResult.value;
				}

				if (CompareValues(currentValue, comparisonValue,
								  state->query.cmp, state->query.epsilon, state->query.relativeDelta))
				{
					state->results.emplace_back(prevResult.address, currentValue, state->query.type);
				}
			}
		}

		// Update status
		if (state->cancelRequested.load())
		{
			state->status = ScanStatus::Cancelled;
		}
		else
		{
			state->status = ScanStatus::Completed;
		}
	}

	// Cancel a scan
	void Scanner::Cancel(ScanId scanId)
	{
		std::lock_guard<std::mutex> lock(m_scansMutex);
		auto it = m_scans.find(scanId);
		if (it != m_scans.end())
		{
			it->second->cancelRequested.store(true);
		}
	}

	// Get results for a scan
	std::vector<Result> Scanner::Results(ScanId scanId) const
	{
		std::lock_guard<std::mutex> lock(m_scansMutex);
		auto it = m_scans.find(scanId);
		if (it != m_scans.end() && it->second->status == ScanStatus::Completed)
		{
			return it->second->results;
		}
		return {};
	}

	// Get status of a scan
	Scanner::ScanStatus Scanner::GetStatus(ScanId scanId) const
	{
		std::lock_guard<std::mutex> lock(m_scansMutex);
		auto it = m_scans.find(scanId);
		if (it != m_scans.end())
		{
			return it->second->status;
		}
		return ScanStatus::NotFound;
	}

	// Write dump file
	void Scanner::WriteDump(const DumpWatch& watch, const Value& newValue)
	{
		fs::path outPath = watch.outPath;

		// Append timestamp if requested
		if (watch.spec.appendTimestamp)
		{
			auto now = std::chrono::system_clock::now();
			auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
				now.time_since_epoch()).count();

			std::string filename = outPath.stem().string() + "_" + std::to_string(timestamp);
			outPath = outPath.parent_path() / (filename + outPath.extension().string());
		}

		std::ofstream out(outPath, std::ios::binary);
		if (!out)
		{
			return;
		}

		// Write header (JSON-like format as per spec)
		out << "{\n";
		out << "  \"address\": \"0x" << std::hex << watch.addr << "\",\n";
		out << "  \"cpu\": \"" << DebugInterface::cpuName(watch.cpu) << "\",\n";
		out << "  \"type\": \"";
		switch (watch.spec.type)
		{
			case ValueType::U8: out << "u8"; break;
			case ValueType::U16: out << "u16"; break;
			case ValueType::U32: out << "u32"; break;
			case ValueType::U64: out << "u64"; break;
			case ValueType::F32: out << "f32"; break;
			case ValueType::F64: out << "f64"; break;
		}
		out << "\",\n";
		out << "  \"old_value\": \"" << ValueToString(watch.lastValue, watch.spec.type) << "\",\n";
		out << "  \"new_value\": \"" << ValueToString(newValue, watch.spec.type) << "\"\n";
		out << "}\n";

		// Include context if requested
		if (watch.spec.includeContext && watch.spec.contextSize > 0)
		{
			DebugInterface& debugInterface = DebugInterface::get(watch.cpu);

			u32 startAddr = static_cast<u32>(watch.addr > watch.spec.contextSize ?
											 watch.addr - watch.spec.contextSize : 0);
			u32 endAddr = static_cast<u32>(watch.addr + watch.spec.contextSize);

			out << "\n-- Context Memory --\n";
			for (u32 addr = startAddr; addr <= endAddr; addr += 4)
			{
				bool valid = false;
				u32 value = debugInterface.read32(addr, valid);
				if (valid)
				{
					out << "0x" << std::hex << std::setw(8) << std::setfill('0') << addr
						<< ": 0x" << std::setw(8) << std::setfill('0') << value << "\n";
				}
			}
		}

		out.close();
	}

	// Set up dump-on-change watch
	bool Scanner::DumpOnChange(BreakPointCpu cpu, u64 addr, const fs::path& outPath, const DumpSpec& spec)
	{
		// Read current value
		DebugInterface& debugInterface = DebugInterface::get(cpu);
		bool valid = false;
		Value currentValue = ReadValue(debugInterface, static_cast<u32>(addr), spec.type, valid);

		if (!valid)
		{
			return false;
		}

		// Create dump watch
		auto watch = std::make_unique<DumpWatch>();
		watch->cpu = cpu;
		watch->addr = addr;
		watch->outPath = outPath;
		watch->spec = spec;
		watch->lastValue = currentValue;

		// Store the watch
		{
			std::lock_guard<std::mutex> lock(m_watchesMutex);
			m_watches[addr] = std::move(watch);
		}

		// Set up memory check using CBreakPoints
		// Use MEMCHECK_WRITE_ONCHANGE to trigger only when value actually changes
		u32 typeSize = GetTypeSize(spec.type);
		CBreakPoints::AddMemCheck(cpu, static_cast<u32>(addr), static_cast<u32>(addr + typeSize),
								   MEMCHECK_WRITE_ONCHANGE, MEMCHECK_LOG);

		return true;
	}

	// Remove dump watch
	bool Scanner::RemoveDumpWatch(BreakPointCpu cpu, u64 addr)
	{
		{
			std::lock_guard<std::mutex> lock(m_watchesMutex);
			auto it = m_watches.find(addr);
			if (it == m_watches.end())
			{
				return false;
			}

			u32 typeSize = GetTypeSize(it->second->spec.type);

			// Remove the memory check
			CBreakPoints::RemoveMemCheck(cpu, static_cast<u32>(addr), static_cast<u32>(addr + typeSize));

			m_watches.erase(it);
		}

		return true;
	}

	// Clear all scans and watches
	void Scanner::ClearAll()
	{
		// Cancel all in-progress scans
		{
			std::lock_guard<std::mutex> lock(m_scansMutex);
			for (auto& [scanId, state] : m_scans)
			{
				state->cancelRequested.store(true);
			}
			m_scans.clear();
		}

		// Remove all watches
		{
			std::lock_guard<std::mutex> lock(m_watchesMutex);
			for (auto& [addr, watch] : m_watches)
			{
				u32 typeSize = GetTypeSize(watch->spec.type);
				CBreakPoints::RemoveMemCheck(watch->cpu, static_cast<u32>(addr),
											  static_cast<u32>(addr + typeSize));
			}
			m_watches.clear();
		}
	}

	// Get number of active scans
	size_t Scanner::GetActiveScanCount() const
	{
		std::lock_guard<std::mutex> lock(m_scansMutex);
		size_t count = 0;
		for (const auto& [scanId, state] : m_scans)
		{
			if (state->status == ScanStatus::InProgress)
			{
				count++;
			}
		}
		return count;
	}

} // namespace MemoryScanner
