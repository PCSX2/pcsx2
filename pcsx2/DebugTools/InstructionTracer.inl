// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Template implementation for InstructionTracer
// This file is included by InstructionTracer.h and should not be included directly

#pragma once

#include <algorithm>

namespace Tracer
{

namespace detail
{
	template<typename OutputIt>
	size_t RingBuffer::Drain(size_t n, OutputIt it)
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
}

template<typename OutputIt>
size_t Drain(BreakPointCpu cpu, size_t n, OutputIt it)
{
	return detail::GetBuffer(cpu).Drain(n, it);
}

} // namespace Tracer
