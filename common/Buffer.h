// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "Pcsx2Types.h"
#include <cstdlib>
#include <cstring>

namespace Common
{

/// Like `std::vector<u8>` but doesn't zero bytes on resize
class Buffer
{
	void* ptr_;
	size_t size_;
	size_t cap_;
public:
	constexpr Buffer(): ptr_(nullptr), size_(0), cap_(0) {}
	explicit Buffer(size_t size): ptr_(malloc(size)), size_(size), cap_(size) {}
	~Buffer() { if (ptr_) free(ptr_); }
	Buffer(const Buffer& other): Buffer(other.size_)
	{
		memcpy(ptr_, other.ptr_, other.size_);
	}
	Buffer(Buffer&& other): ptr_(other.ptr_), size_(other.size_), cap_(other.cap_)
	{
		other.ptr_ = nullptr;
		other.size_ = 0;
		other.cap_ = 0;
	}
	Buffer& operator=(const Buffer& other)
	{
		resize(other.size_);
		memcpy(ptr_, other.ptr_, other.size_);
		return *this;
	}
	Buffer& operator=(Buffer&& other)
	{
		if (this != &other)
		{
			if (ptr_) free(ptr_);
			ptr_ = other.ptr_;
			size_ = other.size_;
			cap_ = other.cap_;
			other.ptr_ = nullptr;
			other.size_ = 0;
			other.cap_ = 0;
		}
		return *this;
	}

	template <typename T = void> const T* get() const { return static_cast<T*>(ptr_); }
	template <typename T = void>       T* get()       { return static_cast<T*>(ptr_); }

	size_t capacity() const { return cap_; }
	size_t size() const { return size_; }

	void resize(size_t size)
	{
		size_ = size;
		reserve(size);
	}

	void reserve(size_t size)
	{
		if (cap_ >= size)
			return;
		cap_ = size;
		ptr_ = realloc(ptr_, size);
	}
};

} // namespace Common
