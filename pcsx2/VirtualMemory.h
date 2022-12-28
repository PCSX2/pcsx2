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

#include "common/General.h"
#include "common/Assertions.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

// --------------------------------------------------------------------------------------
//  VirtualMemoryManager: Manages the allocation of PCSX2 VM
//    Ensures that all memory is close enough together for rip-relative addressing
// --------------------------------------------------------------------------------------
class VirtualMemoryManager
{
	DeclareNoncopyableObject(VirtualMemoryManager);

	std::string m_name;

	void* m_file_handle;
	u8* m_baseptr;

	// An array to track page usage (to trigger asserts if things try to overlap)
	std::atomic<bool>* m_pageuse;

	// reserved memory (in pages)
	u32 m_pages_reserved;

public:
	// If upper_bounds is nonzero and the OS fails to allocate memory that is below it,
	// calls to IsOk() will return false and Alloc() will always return null pointers
	// strict indicates that the allocation should quietly fail if the memory can't be mapped at `base`
	VirtualMemoryManager(std::string name, const char* file_mapping_name, uptr base, size_t size, uptr upper_bounds = 0, bool strict = false);
	~VirtualMemoryManager();

	bool IsSharedMemory() const { return (m_file_handle != nullptr); }
	void* GetFileHandle() const { return m_file_handle; }
	u8* GetBase() const { return m_baseptr; }
	u8* GetEnd() const { return (m_baseptr + m_pages_reserved * __pagesize); }

	// Request the use of the memory at offsetLocation bytes from the start of the reserved memory area
	// offsetLocation must be page-aligned
	u8* Alloc(uptr offsetLocation, size_t size) const;

	u8* AllocAtAddress(void* address, size_t size) const
	{
		return Alloc(size, static_cast<const u8*>(address) - m_baseptr);
	}

	void Free(void* address, size_t size) const;

	// Was this VirtualMemoryManager successfully able to get its memory mapping?
	// (If not, calls to Alloc will return null pointers)
	bool IsOk() const { return m_baseptr != 0; }
};

typedef std::shared_ptr<const VirtualMemoryManager> VirtualMemoryManagerPtr;

// --------------------------------------------------------------------------------------
//  VirtualMemoryBumpAllocator: Allocates memory for things that don't have explicitly-reserved spots
// --------------------------------------------------------------------------------------
class VirtualMemoryBumpAllocator
{
	const VirtualMemoryManagerPtr m_allocator;
	std::atomic<u8*> m_baseptr{0};
	const u8* m_endptr = 0;

public:
	VirtualMemoryBumpAllocator(VirtualMemoryManagerPtr allocator, size_t size, uptr offsetLocation);
	u8* Alloc(size_t size);
	const VirtualMemoryManagerPtr& GetAllocator() { return m_allocator; }
};

// --------------------------------------------------------------------------------------
//  VirtualMemoryReserve
// --------------------------------------------------------------------------------------
class VirtualMemoryReserve
{
	DeclareNoncopyableObject(VirtualMemoryReserve);

protected:
	std::string m_name;

	// Where the memory came from (so we can return it)
	VirtualMemoryManagerPtr m_allocator;

	u8* m_baseptr = nullptr;
	size_t m_size = 0;

public:
	VirtualMemoryReserve(std::string name);
	virtual ~VirtualMemoryReserve();

	// Initialize with the given piece of memory
	// Note: The memory is already allocated, the allocator is for future use to free the region
	// It may be null in which case there is no way to free the memory in a way it will be usable again
	void Assign(VirtualMemoryManagerPtr allocator, u8* baseptr, size_t size);

	u8* BumpAllocate(VirtualMemoryBumpAllocator& allocator, size_t size);

	void Release();

	bool IsOk() const { return m_baseptr != NULL; }
	const std::string& GetName() const { return m_name; }

	u8* GetPtr() { return m_baseptr; }
	const u8* GetPtr() const { return m_baseptr; }
	u8* GetPtrEnd() { return m_baseptr + m_size; }
	const u8* GetPtrEnd() const { return m_baseptr + m_size; }

	size_t GetSize() const { return m_size; }

	operator void*() { return m_baseptr; }
	operator const void*() const { return m_baseptr; }

	operator u8*() { return (u8*)m_baseptr; }
	operator const u8*() const { return (u8*)m_baseptr; }

	u8& operator[](uint idx)
	{
		pxAssert(idx < m_size);
		return *((u8*)m_baseptr + idx);
	}

	const u8& operator[](uint idx) const
	{
		pxAssert(idx < m_size);
		return *((u8*)m_baseptr + idx);
	}
};

// --------------------------------------------------------------------------------------
//  RecompiledCodeReserve
// --------------------------------------------------------------------------------------
// A recompiled code reserve is a simple sequential-growth block of memory which is auto-
// cleared to INT 3 (0xcc) as needed.
//
class RecompiledCodeReserve : public VirtualMemoryReserve
{
	typedef VirtualMemoryReserve _parent;

protected:
	std::string m_profiler_name;

public:
	RecompiledCodeReserve(std::string name);
	~RecompiledCodeReserve();

	void Assign(VirtualMemoryManagerPtr allocator, size_t offset, size_t size);
	void Reset();

	RecompiledCodeReserve& SetProfilerName(std::string name);

	void ForbidModification();
	void AllowModification();

	operator u8*() { return m_baseptr; }
	operator const u8*() const { return m_baseptr; }

protected:
	void _registerProfiler();
};