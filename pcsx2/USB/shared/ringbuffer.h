// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <algorithm> // for std::min
#include <cstdint>

class RingBuffer
{
	RingBuffer(RingBuffer&) = delete;

public:
	RingBuffer();
	RingBuffer(size_t capacity);
	~RingBuffer();

	//size_t write(const char *data, size_t bytes);
	//size_t read(char *data, size_t bytes);

	// Overwrites old data if nbytes > size()
	void write(uint8_t* src, size_t nbytes);
	size_t read(uint8_t* dst, size_t nbytes);

	// just move pointers around
	void write(size_t bytes);
	void read(size_t bytes);

	template <typename T>
	void write(size_t samples)
	{
		write(samples * sizeof(T));
	}

	template <typename T>
	void read(size_t samples)
	{
		read(samples * sizeof(T));
	}

	void reserve(size_t size);
	// if you care about old data, check how much can be written
	// may need to call available/write twice in case write pointer wraps
	size_t peek_write(bool overwrite = false) const;
	size_t peek_read() const;

	template <typename T>
	size_t peek_write(bool overwrite = false) const
	{
		return peek_write(overwrite) / sizeof(T);
	}

	template <typename T>
	size_t peek_read() const
	{
		return peek_read() / sizeof(T);
	}

	// amount of valid data, may need to read twice
	size_t size() const;
	template <typename T>
	size_t size() const
	{
		return size() / sizeof(T);
	}
	size_t capacity() const { return m_capacity; }
	char* front() { return m_data + m_begin; }
	char* back() { return m_data + m_end; }

	template <typename T>
	T* front()
	{
		return (T*)(m_data + m_begin);
	}

	template <typename T>
	T* back()
	{
		return (T*)(m_data + m_end);
	}

private:
	size_t m_begin;
	size_t m_end;
	size_t m_capacity;
	char* m_data;
	bool m_overrun;
};

