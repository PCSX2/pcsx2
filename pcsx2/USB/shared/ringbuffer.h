/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RINGBUFFER_H
#define RINGBUFFER_H
#include <algorithm> // for std::min
#include <cstdint>
#include <chrono>

using hrc = std::chrono::high_resolution_clock;
using ms = std::chrono::milliseconds;
using us = std::chrono::microseconds;
using ns = std::chrono::nanoseconds;
using sec = std::chrono::seconds;

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

	long long MilliSecsSinceLastWrite()
	{
		return std::chrono::duration_cast<ms>(hrc::now() - mLastWrite).count();
	}

private:
	size_t m_begin;
	size_t m_end;
	size_t m_capacity;
	char* m_data;
	bool m_overrun;
	hrc::time_point mLastWrite = hrc::time_point(ns(0));
};

#endif
