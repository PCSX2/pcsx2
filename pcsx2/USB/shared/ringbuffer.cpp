// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ringbuffer.h"
#include <cstring>
#include <cassert>
#include <algorithm>

RingBuffer::RingBuffer() = default;

RingBuffer::RingBuffer(size_t capacity)
	: RingBuffer()
{
	reset(capacity);
}

RingBuffer::~RingBuffer() = default;

void RingBuffer::reset(size_t capacity)
{
	m_rpos = 0;
	m_wpos = 0;
	m_full = false;
	m_data.reset();
	if ((m_capacity = capacity) > 0)
		m_data = std::make_unique<uint8_t[]>(capacity);
}

size_t RingBuffer::size() const
{
	if (m_wpos == m_rpos)
		return m_full ? m_capacity : 0;
	else if (m_wpos > m_rpos)
		return m_wpos - m_rpos;
	else
		return (m_capacity - m_rpos) + m_wpos;
}

size_t RingBuffer::read(void* dst, size_t nbytes)
{
	uint8_t* bdst = static_cast<uint8_t*>(dst);

	size_t to_read = nbytes;
	while (to_read > 0)
	{
		size_t available;
		if (m_wpos == m_rpos)
			available = m_full ? (m_capacity - m_rpos) : 0;
		else if (m_wpos > m_rpos)
			available = m_wpos - m_rpos;
		else
			available = m_capacity - m_rpos;

		if (available == 0)
			break;

		const size_t copy = std::min(available, to_read);
		std::memcpy(bdst, m_data.get() + m_rpos, copy);
		bdst += copy;
		to_read -= copy;

		m_rpos = (m_rpos + copy) % m_capacity;
		m_full = false;
	}

	return nbytes - to_read;
}

void RingBuffer::write(const void* src, size_t nbytes)
{
	const uint8_t* bsrc = static_cast<const uint8_t*>(src);
	while (nbytes > 0)
	{
		size_t free;
		if (m_wpos >= m_rpos)
			free = m_capacity - m_wpos;
		else
			free = m_rpos - m_wpos;

		const size_t copy = std::min(free, nbytes);
		std::memcpy(m_data.get() + m_wpos, bsrc, copy);
		bsrc += copy;
		nbytes -= copy;

		m_wpos = (m_wpos + copy) % m_capacity;
		m_full = m_full || (m_wpos == m_rpos);
	}
}
