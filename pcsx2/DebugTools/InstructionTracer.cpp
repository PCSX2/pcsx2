// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "InstructionTracer.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <mutex>

namespace Tracer
{

namespace detail
{
	/// Lock-free ring buffer for trace events
	/// Uses atomic operations for thread-safe producer-consumer pattern
	class RingBuffer
	{
	public:
		static constexpr size_t DEFAULT_CAPACITY = 65536; // 64K events

		RingBuffer(size_t capacity = DEFAULT_CAPACITY)
			: m_capacity(capacity)
			, m_buffer(new TraceEvent[capacity])
			, m_write_pos(0)
			, m_read_pos(0)
			, m_enabled(false)
		{
		}

		~RingBuffer() = default;

		// Enable or disable recording
		void SetEnabled(bool enabled)
		{
			m_enabled.store(enabled, std::memory_order_release);
		}

		bool IsEnabled() const
		{
			return m_enabled.load(std::memory_order_acquire);
		}

		// Record an event (lock-free, may overwrite oldest if full)
		void Record(const TraceEvent& ev)
		{
			if (!IsEnabled())
				return;

			// Get current write position and advance atomically
			size_t pos = m_write_pos.fetch_add(1, std::memory_order_relaxed) % m_capacity;

			// Write the event (may race with reads, but that's acceptable for tracing)
			m_buffer[pos] = ev;

			// Update read position if we're about to overwrite it (drop-oldest policy)
			size_t current_read = m_read_pos.load(std::memory_order_relaxed);
			size_t current_write = m_write_pos.load(std::memory_order_relaxed);
			if (current_write - current_read >= m_capacity)
			{
				// Try to advance read position to avoid reading stale data
				m_read_pos.compare_exchange_weak(current_read, current_read + 1,
					std::memory_order_release, std::memory_order_relaxed);
			}
		}

		// Drain up to n events into output iterator
		template<typename OutputIt>
		size_t Drain(size_t n, OutputIt it)
		{
			size_t drained = 0;
			size_t current_write = m_write_pos.load(std::memory_order_acquire);
			size_t current_read = m_read_pos.load(std::memory_order_relaxed);

			// Calculate available events
			size_t available = current_write - current_read;
			if (available > m_capacity)
				available = m_capacity; // Limit to capacity in case of overflow

			size_t to_drain = std::min(n, available);

			for (size_t i = 0; i < to_drain; ++i)
			{
				size_t pos = (current_read + i) % m_capacity;
				*it++ = m_buffer[pos];
				++drained;
			}

			// Advance read position
			if (drained > 0)
			{
				m_read_pos.fetch_add(drained, std::memory_order_release);
			}

			return drained;
		}

		// Get all available events
		std::vector<TraceEvent> GetAll()
		{
			std::vector<TraceEvent> events;
			size_t current_write = m_write_pos.load(std::memory_order_acquire);
			size_t current_read = m_read_pos.load(std::memory_order_relaxed);

			size_t available = current_write - current_read;
			if (available > m_capacity)
				available = m_capacity;

			events.reserve(available);
			Drain(available, std::back_inserter(events));

			return events;
		}

		// Clear all events
		void Clear()
		{
			m_read_pos.store(m_write_pos.load(std::memory_order_relaxed), std::memory_order_release);
		}

	private:
		size_t m_capacity;
		std::unique_ptr<TraceEvent[]> m_buffer;
		std::atomic<size_t> m_write_pos;
		std::atomic<size_t> m_read_pos;
		std::atomic<bool> m_enabled;
	};

	// Per-CPU ring buffers
	static RingBuffer s_ee_buffer;
	static RingBuffer s_iop_buffer;

	// Get the ring buffer for a specific CPU
	static RingBuffer& GetBuffer(BreakPointCpu cpu)
	{
		switch (cpu)
		{
			case BREAKPOINT_EE:
				return s_ee_buffer;
			case BREAKPOINT_IOP:
				return s_iop_buffer;
			default:
				// Should not happen, but default to EE
				return s_ee_buffer;
		}
	}

} // namespace detail

void Enable(BreakPointCpu cpu, bool on)
{
	detail::GetBuffer(cpu).SetEnabled(on);
}

bool IsEnabled(BreakPointCpu cpu)
{
	return detail::GetBuffer(cpu).IsEnabled();
}

void Record(BreakPointCpu cpu, const TraceEvent& ev)
{
	detail::GetBuffer(cpu).Record(ev);
}

// Template instantiation for Drain
template<typename OutputIt>
size_t Drain(BreakPointCpu cpu, size_t n, OutputIt it)
{
	return detail::GetBuffer(cpu).Drain(n, it);
}

// Explicit instantiation for common iterator types
template size_t Drain(BreakPointCpu cpu, size_t n, std::back_insert_iterator<std::vector<TraceEvent>> it);

bool DumpToFile(BreakPointCpu cpu, const std::string& path, const DumpBounds& bounds)
{
	try
	{
		// Get all available events
		std::vector<TraceEvent> events = detail::GetBuffer(cpu).GetAll();

		// Apply bounds
		if (bounds.max_events > 0 && events.size() > bounds.max_events)
		{
			if (bounds.oldest_first)
			{
				events.resize(bounds.max_events);
			}
			else
			{
				// Keep most recent
				events.erase(events.begin(), events.begin() + (events.size() - bounds.max_events));
			}
		}

		// Reverse if newest first is requested
		if (!bounds.oldest_first)
		{
			std::reverse(events.begin(), events.end());
		}

		// Write to file in NDJSON format
		std::ofstream file(path);
		if (!file.is_open())
		{
			return false;
		}

		const char* cpu_name = DebugInterface::cpuName(cpu);

		for (const auto& ev : events)
		{
			// Convert timestamp to seconds with fractional part
			double timestamp_sec = static_cast<double>(ev.timestamp_ns) / 1'000'000'000.0;

			// Write JSON line
			file << "{";
			file << "\"ts\":" << timestamp_sec << ",";
			file << "\"cpu\":\"" << cpu_name << "\",";
			file << "\"pc\":\"0x" << std::hex << ev.pc << std::dec << "\",";
			file << "\"op\":\"" << ev.disasm << "\",";
			file << "\"cycles\":" << ev.cycles;

			// Add memory accesses if present
			if (!ev.mem_r.empty() || !ev.mem_w.empty())
			{
				file << ",\"mem\":{";

				if (!ev.mem_r.empty())
				{
					file << "\"r\":[";
					for (size_t i = 0; i < ev.mem_r.size(); ++i)
					{
						if (i > 0) file << ",";
						file << "[\"0x" << std::hex << ev.mem_r[i].first << std::dec << "\"," << static_cast<int>(ev.mem_r[i].second) << "]";
					}
					file << "]";
				}

				if (!ev.mem_w.empty())
				{
					if (!ev.mem_r.empty()) file << ",";
					file << "\"w\":[";
					for (size_t i = 0; i < ev.mem_w.size(); ++i)
					{
						if (i > 0) file << ",";
						file << "[\"0x" << std::hex << ev.mem_w[i].first << std::dec << "\"," << static_cast<int>(ev.mem_w[i].second) << "]";
					}
					file << "]";
				}

				file << "}";
			}

			file << "}\n";
		}

		file.close();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

} // namespace Tracer
