// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GroovyMiSTer/GroovyMiSTerAudioTap.h"

#include <algorithm>
#include <cstring>

namespace GroovyMiSTer
{
	static inline s16 FloatToS16(float v)
	{
		// SPU2 delivers roughly [-1, 1], but the DC filter and volume scaling can push past
		// it, so clamp before scaling or loud passages wrap and click.
		v = std::clamp(v, -1.0f, 1.0f);
		return static_cast<s16>(v * 32767.0f);
	}

	void AudioTap::Reset()
	{
		std::lock_guard<std::mutex> guard(m_lock);
		m_buffer.assign(CAPACITY, 0);
		m_read_pos = 0;
		m_size = 0;
		m_dropped_bytes.store(0, std::memory_order_relaxed);
	}

	void AudioTap::Write(const float* samples, u32 frames)
	{
		if (!IsActive() || frames == 0)
			return;

		const size_t bytes = static_cast<size_t>(frames) * BYTES_PER_SAMPLE;

		std::lock_guard<std::mutex> guard(m_lock);
		if (m_buffer.size() != CAPACITY) [[unlikely]]
			m_buffer.assign(CAPACITY, 0);

		// A write larger than the ring itself means something upstream is wrong; keep the
		// newest tail rather than corrupt the ring.
		if (bytes >= CAPACITY)
		{
			const u32 keep_frames = static_cast<u32>(CAPACITY / BYTES_PER_SAMPLE);
			const u32 skip = frames - keep_frames;
			samples += static_cast<size_t>(skip) * CHANNELS;
			frames = keep_frames;
			m_dropped_bytes.fetch_add(static_cast<u64>(skip) * BYTES_PER_SAMPLE, std::memory_order_relaxed);
			m_read_pos = 0;
			m_size = 0;
		}

		const size_t incoming = static_cast<size_t>(frames) * BYTES_PER_SAMPLE;

		// Drop-oldest: make room by advancing the read cursor.
		if (m_size + incoming > CAPACITY)
		{
			const size_t overflow = (m_size + incoming) - CAPACITY;
			m_read_pos = (m_read_pos + overflow) % CAPACITY;
			m_size -= overflow;
			m_dropped_bytes.fetch_add(overflow, std::memory_order_relaxed);
		}

		size_t write_pos = (m_read_pos + m_size) % CAPACITY;
		for (u32 i = 0; i < frames * CHANNELS; i++)
		{
			const s16 s = FloatToS16(samples[i]);
			std::memcpy(&m_buffer[write_pos], &s, sizeof(s16));
			write_pos = (write_pos + sizeof(s16)) % CAPACITY;
		}

		m_size += incoming;
	}

	u32 AudioTap::Read(u8* dst, u32 max_bytes)
	{
		std::lock_guard<std::mutex> guard(m_lock);

		// Whole stereo frames only; a half-frame desyncs the L/R interleave from there on.
		u32 avail = static_cast<u32>(std::min<size_t>(m_size, max_bytes));
		avail -= (avail % BYTES_PER_SAMPLE);
		if (avail == 0)
			return 0;

		const size_t first = std::min<size_t>(avail, CAPACITY - m_read_pos);
		std::memcpy(dst, &m_buffer[m_read_pos], first);
		if (avail > first)
			std::memcpy(dst + first, &m_buffer[0], avail - first);

		m_read_pos = (m_read_pos + avail) % CAPACITY;
		m_size -= avail;
		return avail;
	}
} // namespace GroovyMiSTer
