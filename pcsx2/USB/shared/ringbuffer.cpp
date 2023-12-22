// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ringbuffer.h"
#include <cstring>
#include <cassert>


RingBuffer::RingBuffer()
	: m_begin(0)
	, m_end(0)
	, m_capacity(0)
	, m_data(nullptr)
	, m_overrun(false)
{
}

RingBuffer::RingBuffer(size_t capacity)
	: RingBuffer()
{
	reserve(capacity);
}

RingBuffer::~RingBuffer()
{
	delete[] m_data;
}

void RingBuffer::reserve(size_t capacity)
{
	delete[] m_data;
	m_data = new char[capacity];
	memset(m_data, 0, capacity);
	m_capacity = capacity;
}

size_t RingBuffer::size() const
{
	size_t size = 0;
	if (m_begin == m_end)
	{
		if (m_overrun)
			size = m_capacity;
		else
			size = 0;
	}
	else if (m_begin < m_end)
		size = m_end - m_begin; // [   b...e   ]
	else
		size = m_capacity - m_begin + m_end; // [...e   b...]

	return size;
}

size_t RingBuffer::read(uint8_t* dst, size_t nbytes)
{
	size_t to_read = nbytes;
	while (to_read > 0 && size() > 0)
	{
		size_t bytes = std::min(to_read, peek_read());
		memcpy(dst, front(), bytes);
		read(bytes);
		dst += bytes;
		to_read -= bytes;
	}
	return nbytes - to_read;
}

void RingBuffer::write(uint8_t* src, size_t nbytes)
{
	while (nbytes > 0)
	{
		size_t bytes = std::min(nbytes, m_capacity - m_end);
		memcpy(back(), src, bytes);
		write(bytes);
		src += bytes;
		nbytes -= bytes;
	}
}

size_t RingBuffer::peek_write(bool overwrite) const
{
	size_t peek = 0;

	if (overwrite)
		return m_capacity - m_end;

	if (m_end < m_begin) // [...e   b...]
		peek = m_begin - m_end;
	else if (m_end < m_capacity) // [   b...e   ]
		peek = m_capacity - m_end;
	else
		peek = m_begin; // [   b.......e]

	return peek;
}

size_t RingBuffer::peek_read() const
{
	size_t peek = 0;
	if (m_begin == m_end)
	{
		if (m_overrun)
			peek = m_capacity - m_begin;
		else
			peek = 0;
	}
	else if (m_begin < m_end) // [   b...e   ]
		peek = m_end - m_begin;
	else if (m_begin < m_capacity) // [...e   b...]
		peek = m_capacity - m_begin;
	else
		peek = m_end; // [...e      b]

	return peek;
}

/*size_t RingBuffer::write(const char *data, size_t bytes)
{
	size_t bytes_to_write;

	if (m_end < m_begin)
	{
		bytes_to_write = std::min(m_begin - m_end, bytes);
		memcpy(m_data + m_end, data, bytes_to_write);
		m_end += bytes_to_write;
		return bytes_to_write;
	}
	else
	{
		size_t in_bytes = bytes;
		while (in_bytes > 0)
		{
			bytes_to_write = std::min(m_capacity - m_end, in_bytes);
			if (m_end < m_begin && m_end + bytes_to_write > m_begin)
				m_begin = (m_end + bytes_to_write + 1) % m_capacity;

			memcpy(m_data + m_end, data, bytes_to_write);
			in_bytes -= bytes_to_write;
			m_end = (m_end + bytes_to_write) % m_capacity;
		}
		return bytes;
	}
}*/

void RingBuffer::write(size_t bytes)
{
	//assert( bytes <= m_capacity - size() );

	// push m_begin forward if m_end overlaps it
	if ((m_end < m_begin && m_end + bytes > m_begin) ||
		m_end + bytes >= m_begin + m_capacity)
	{
		m_overrun = true;
		m_begin = (m_end + bytes) % m_capacity;
		m_end = m_begin;
	}
	else
		m_end = (m_end + bytes) % m_capacity;
}

void RingBuffer::read(size_t bytes)
{
	assert(bytes <= size());

	m_overrun = false;
	if ((m_begin < m_end && m_begin + bytes > m_end) ||
		m_begin + bytes > m_end + m_capacity)
	{
		m_begin = m_end = 0;
		return;
	}

	m_begin = (m_begin + bytes) % m_capacity;
}
