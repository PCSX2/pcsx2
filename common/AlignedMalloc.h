/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#pragma once

#include "common/Exceptions.h"

// pxUSE_SECURE_MALLOC - enables bounds checking on scoped malloc allocations.

#ifndef pxUSE_SECURE_MALLOC
#define pxUSE_SECURE_MALLOC 0
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Safe deallocation macros -- checks pointer validity (non-null) when needed, and sets
// pointer to null after deallocation.

#define safe_delete(ptr) \
	((void)(delete (ptr)), (ptr) = NULL)

#define safe_delete_array(ptr) \
	((void)(delete[](ptr)), (ptr) = NULL)

// No checks for NULL -- wxWidgets says it's safe to skip NULL checks and it runs on
// just about every compiler and libc implementation of any recentness.
#define safe_free(ptr) \
	((void)(free(ptr), !!0), (ptr) = NULL)
//((void) (( ( (ptr) != NULL ) && (free( ptr ), !!0) ), (ptr) = NULL))

#define safe_fclose(ptr) \
	((void)((((ptr) != NULL) && (fclose(ptr), !!0)), (ptr) = NULL))

// Implementation note: all known implementations of _aligned_free check the pointer for
// NULL status (our implementation under GCC, and microsoft's under MSVC), so no need to
// do it here.
#define safe_aligned_free(ptr) \
	((void)(_aligned_free(ptr), (ptr) = NULL))

// aligned_malloc: Implement/declare linux equivalents here!
#if !defined(_MSC_VER)
extern void* __fastcall _aligned_malloc(size_t size, size_t align);
extern void* __fastcall pcsx2_aligned_realloc(void* handle, size_t new_size, size_t align, size_t old_size);
extern void _aligned_free(void* pmem);
#else
#define pcsx2_aligned_realloc(handle, new_size, align, old_size) \
	_aligned_realloc(handle, new_size, align)
#endif

// --------------------------------------------------------------------------------------
//  pxDoOutOfMemory
// --------------------------------------------------------------------------------------

typedef void FnType_OutOfMemory(uptr blocksize);
typedef FnType_OutOfMemory* Fnptr_OutOfMemory;

// This method is meant to be assigned by applications that link against pxWex.  It is called
// (invoked) prior to most pxWex built-in memory/array classes throwing exceptions, and can be
// used by an application to remove unneeded memory allocations and/or reduce internal cache
// reserves.
//
// Example: PCSX2 uses several bloated recompiler code caches.  Larger caches improve performance,
// however a rouge cache growth could cause memory constraints in the operating system.  If an out-
// of-memory error occurs, PCSX2's implementation of this function attempts to reset all internal
// recompiler caches.  This can typically free up 100-150 megs of memory, and will allow the app
// to continue running without crashing or hanging the operating system, etc.
//
extern Fnptr_OutOfMemory pxDoOutOfMemory;


// --------------------------------------------------------------------------------------
//  AlignedBuffer
// --------------------------------------------------------------------------------------
// A simple container class for an aligned allocation.  By default, no bounds checking is
// performed, and there is no option for enabling bounds checking.  If bounds checking and
// other features are needed, use the more robust SafeArray<> instead.
//
template <typename T, uint align>
class AlignedBuffer
{
	static_assert(std::is_pod<T>::value, "Must use a POD type");

	struct Deleter
	{
		void operator()(T* ptr)
		{
			_aligned_free(ptr);
		}
	};

	std::unique_ptr<T[], Deleter> m_buffer;
	std::size_t m_size;

public:
	AlignedBuffer(size_t size = 0)
	{
		Alloc(size);
	}

	size_t GetSize() const { return m_size; }
	size_t GetLength() const { return m_size; }

	void Alloc(size_t newsize)
	{
		m_size = newsize;
		m_buffer.reset();
		if (!m_size)
			return;

		m_buffer.reset(reinterpret_cast<T*>(_aligned_malloc(this->m_size * sizeof(T), align)));
		if (!m_buffer)
			throw std::bad_alloc();
	}

	void Resize(size_t newsize)
	{
		m_buffer.reset(reinterpret_cast<T*>(pcsx2_aligned_realloc(m_buffer.release(), newsize * sizeof(T), align, m_size * sizeof(T))));
		m_size = newsize;

		if (!m_buffer)
			throw std::bad_alloc();
	}

	void Free()
	{
		Alloc(0);
	}

	// Makes enough room for the requested size.  Existing data in the array is retained.
	void MakeRoomFor(uint size)
	{
		if (size <= m_size)
			return;
		Resize(size);
	}

	T* GetPtr(uint idx = 0) const
	{
#if pxUSE_SECURE_MALLOC
		IndexBoundsAssumeDev("ScopedAlloc", idx, m_size);
#endif
		return &m_buffer[idx];
	}

	T& operator[](uint idx)
	{
#if pxUSE_SECURE_MALLOC
		IndexBoundsAssumeDev("ScopedAlloc", idx, m_size);
#endif
		return m_buffer[idx];
	}

	const T& operator[](uint idx) const
	{
#if pxUSE_SECURE_MALLOC
		IndexBoundsAssumeDev("ScopedAlloc", idx, m_size);
#endif
		return m_buffer[idx];
	}
};
