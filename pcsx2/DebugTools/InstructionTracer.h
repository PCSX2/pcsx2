// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "DebugInterface.h"
#include "common/Pcsx2Types.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Tracer
{

/// Lightweight trace event structure for capturing instruction execution
struct TraceEvent
{
	BreakPointCpu cpu;        // EE or IOP
	u64 pc;                   // guest program counter
	u32 opcode;               // raw instruction word
	std::string disasm;       // disassembled instruction (optional symbol lookup)
	u64 cycles;               // CPU cycles from DebugInterface
	u64 timestamp_ns;         // host monotonic timestamp in nanoseconds

	// Optional memory access summaries (addr, size) to keep overhead bounded
	std::vector<std::pair<u64, u8>> mem_r;  // reads
	std::vector<std::pair<u64, u8>> mem_w;  // writes
};

/// Bounds specification for dumping trace events to file
struct DumpBounds
{
	size_t max_events = 0;    // 0 = all available events
	bool oldest_first = true; // false = most recent first
};

/// Enable or disable tracing for the specified CPU
/// When disabled, Record() becomes a near-zero-cost no-op
void Enable(BreakPointCpu cpu, bool on);

/// Check if tracing is currently enabled for the specified CPU
bool IsEnabled(BreakPointCpu cpu);

/// Record a trace event into the ring buffer for the specified CPU
/// This is a lock-free, non-blocking operation
/// If the ring is full, the oldest event is overwritten (drop-oldest policy)
void Record(BreakPointCpu cpu, const TraceEvent& ev);

/// Drain up to 'n' events from the ring buffer
/// OutputIt must be an output iterator accepting TraceEvent
/// Returns the number of events actually drained
template<typename OutputIt>
size_t Drain(BreakPointCpu cpu, size_t n, OutputIt it);

/// Dump trace events to a file in NDJSON format
/// Returns true on success, false on error
/// This operation runs on the calling thread and may block for I/O
bool DumpToFile(BreakPointCpu cpu, const std::string& path, const DumpBounds& bounds);

namespace detail
{
	// Forward declaration for ring buffer implementation
	class RingBuffer;
}

} // namespace Tracer
