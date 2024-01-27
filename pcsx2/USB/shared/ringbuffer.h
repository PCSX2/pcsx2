// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <cstdint>
#include <memory>

class RingBuffer
{
	RingBuffer(const RingBuffer&) = delete;
	RingBuffer& operator=(const RingBuffer&) = delete;

public:
	RingBuffer();
	RingBuffer(size_t capacity);
	~RingBuffer();

	void reset(size_t size);
	size_t capacity() const;
	size_t size() const;

	// Overwrites old data if nbytes > size()
	void write(const void* src, size_t nbytes);
	size_t read(void* dst, size_t nbytes);

private:
	std::unique_ptr<uint8_t[]> m_data;
	size_t m_capacity = 0;
	size_t m_rpos = 0;
	size_t m_wpos = 0;
	bool m_full = false;
};
