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

#include "PrecompiledHeader.h"

#include "VirtualMemory.h"

#include "common/BitUtils.h"
#include "common/Console.h"
#include "common/Perf.h"

#include "fmt/core.h"

#include <cinttypes>

// --------------------------------------------------------------------------------------
//  VirtualMemoryManager  (implementations)
// --------------------------------------------------------------------------------------

VirtualMemoryManager::VirtualMemoryManager(std::string name, const char* file_mapping_name, uptr base, size_t size, uptr upper_bounds, bool strict)
	: m_name(std::move(name))
	, m_file_handle(nullptr)
	, m_baseptr(0)
	, m_pageuse(nullptr)
	, m_pages_reserved(0)
{
	if (!size)
		return;

	size_t reserved_bytes = Common::PageAlign(size);
	m_pages_reserved = reserved_bytes / __pagesize;

	if (file_mapping_name && file_mapping_name[0])
	{
		std::string real_file_mapping_name(HostSys::GetFileMappingName(file_mapping_name));
		m_file_handle = HostSys::CreateSharedMemory(real_file_mapping_name.c_str(), reserved_bytes);
		if (!m_file_handle)
			return;

		m_baseptr = static_cast<u8*>(HostSys::MapSharedMemory(m_file_handle, 0, (void*)base, reserved_bytes, PageAccess_ReadWrite()));
		if (!m_baseptr || (upper_bounds != 0 && (((uptr)m_baseptr + reserved_bytes) > upper_bounds)))
		{
			DevCon.Warning("%s: host memory @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " is unavailable; attempting to map elsewhere...",
				m_name.c_str(), base, base + size);

			SafeSysMunmap(m_baseptr, reserved_bytes);

			if (base)
			{
				// Let's try again at an OS-picked memory area, and then hope it meets needed
				// boundschecking criteria below.
				m_baseptr = static_cast<u8*>(HostSys::MapSharedMemory(m_file_handle, 0, nullptr, reserved_bytes, PageAccess_ReadWrite()));
			}
		}
	}
	else
	{
		m_baseptr = static_cast<u8*>(HostSys::Mmap((void*)base, reserved_bytes, PageAccess_Any()));

		if (!m_baseptr || (upper_bounds != 0 && (((uptr)m_baseptr + reserved_bytes) > upper_bounds)))
		{
			DevCon.Warning("%s: host memory @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " is unavailable; attempting to map elsewhere...",
				m_name.c_str(), base, base + size);

			SafeSysMunmap(m_baseptr, reserved_bytes);

			if (base)
			{
				// Let's try again at an OS-picked memory area, and then hope it meets needed
				// boundschecking criteria below.
				m_baseptr = static_cast<u8*>(HostSys::Mmap(0, reserved_bytes, PageAccess_Any()));
			}
		}
	}

	bool fulfillsRequirements = true;
	if (strict && (uptr)m_baseptr != base)
		fulfillsRequirements = false;
	if ((upper_bounds != 0) && ((uptr)(m_baseptr + reserved_bytes) > upper_bounds))
		fulfillsRequirements = false;
	if (!fulfillsRequirements)
	{
		if (m_file_handle)
		{
			if (m_baseptr)
				HostSys::UnmapSharedMemory(m_baseptr, reserved_bytes);
			m_baseptr = 0;

			HostSys::DestroySharedMemory(m_file_handle);
			m_file_handle = nullptr;
		}
		else
		{
			SafeSysMunmap(m_baseptr, reserved_bytes);
		}
	}

	if (!m_baseptr)
		return;

	m_pageuse = new std::atomic<bool>[m_pages_reserved]();

	std::string mbkb;
	uint mbytes = reserved_bytes / _1mb;
	if (mbytes)
		mbkb = fmt::format("[{}mb]", mbytes);
	else
		mbkb = fmt::format("[{}kb]", reserved_bytes / 1024);

	DevCon.WriteLn(Color_Gray, "%-32s @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " %s", m_name.c_str(),
		m_baseptr, (uptr)m_baseptr + reserved_bytes, mbkb.c_str());
}

VirtualMemoryManager::~VirtualMemoryManager()
{
	if (m_pageuse)
		delete[] m_pageuse;
	if (m_baseptr)
	{
		if (m_file_handle)
			HostSys::UnmapSharedMemory((void*)m_baseptr, m_pages_reserved * __pagesize);
		else
			HostSys::Munmap(m_baseptr, m_pages_reserved * __pagesize);
	}
	if (m_file_handle)
		HostSys::DestroySharedMemory(m_file_handle);
}

static bool VMMMarkPagesAsInUse(std::atomic<bool>* begin, std::atomic<bool>* end)
{
	for (auto current = begin; current < end; current++)
	{
		bool expected = false;
		if (!current->compare_exchange_strong(expected, true, std::memory_order_relaxed))
		{
			// This was already allocated!  Undo the things we've set until this point
			while (--current >= begin)
			{
				if (!current->compare_exchange_strong(expected, false, std::memory_order_relaxed))
				{
					// In the time we were doing this, someone set one of the things we just set to true back to false
					// This should never happen, but if it does we'll just stop and hope nothing bad happens
					pxAssert(0);
					return false;
				}
			}
			return false;
		}
	}
	return true;
}

u8* VirtualMemoryManager::Alloc(uptr offsetLocation, size_t size) const
{
	size = Common::PageAlign(size);
	if (!pxAssertDev(offsetLocation % __pagesize == 0, "(VirtualMemoryManager) alloc at unaligned offsetLocation"))
		return nullptr;
	if (!pxAssertDev(size + offsetLocation <= m_pages_reserved * __pagesize, "(VirtualMemoryManager) alloc outside reserved area"))
		return nullptr;
	if (m_baseptr == 0)
		return nullptr;
	auto puStart = &m_pageuse[offsetLocation / __pagesize];
	auto puEnd = &m_pageuse[(offsetLocation + size) / __pagesize];
	if (!pxAssertDev(VMMMarkPagesAsInUse(puStart, puEnd), "(VirtualMemoryManager) allocation requests overlapped"))
		return nullptr;
	return m_baseptr + offsetLocation;
}

void VirtualMemoryManager::Free(void* address, size_t size) const
{
	uptr offsetLocation = (uptr)address - (uptr)m_baseptr;
	if (!pxAssertDev(offsetLocation % __pagesize == 0, "(VirtualMemoryManager) free at unaligned address"))
	{
		uptr newLoc = Common::PageAlign(offsetLocation);
		size -= (offsetLocation - newLoc);
		offsetLocation = newLoc;
	}
	if (!pxAssertDev(size % __pagesize == 0, "(VirtualMemoryManager) free with unaligned size"))
		size -= size % __pagesize;
	if (!pxAssertDev(size + offsetLocation <= m_pages_reserved * __pagesize, "(VirtualMemoryManager) free outside reserved area"))
		return;
	auto puStart = &m_pageuse[offsetLocation / __pagesize];
	auto puEnd = &m_pageuse[(offsetLocation + size) / __pagesize];
	for (; puStart < puEnd; puStart++)
	{
		bool expected = true;
		if (!puStart->compare_exchange_strong(expected, false, std::memory_order_relaxed))
		{
			pxAssertDev(0, "(VirtaulMemoryManager) double-free");
		}
	}
}

// --------------------------------------------------------------------------------------
//  VirtualMemoryBumpAllocator  (implementations)
// --------------------------------------------------------------------------------------
VirtualMemoryBumpAllocator::VirtualMemoryBumpAllocator(VirtualMemoryManagerPtr allocator, uptr offsetLocation, size_t size)
	: m_allocator(std::move(allocator))
	, m_baseptr(m_allocator->Alloc(offsetLocation, size))
	, m_endptr(m_baseptr + size)
{
	if (m_baseptr.load() == 0)
		pxAssertDev(0, "(VirtualMemoryBumpAllocator) tried to construct from bad VirtualMemoryManager");
}

u8* VirtualMemoryBumpAllocator::Alloc(size_t size)
{
	if (m_baseptr.load() == 0) // True if constructed from bad VirtualMemoryManager (assertion was on initialization)
		return nullptr;

	size_t reservedSize = Common::PageAlign(size);

	u8* out = m_baseptr.fetch_add(reservedSize, std::memory_order_relaxed);

	if (!pxAssertDev(out - reservedSize + size <= m_endptr, "(VirtualMemoryBumpAllocator) ran out of memory"))
		return nullptr;

	return out;
}

// --------------------------------------------------------------------------------------
//  VirtualMemoryReserve  (implementations)
// --------------------------------------------------------------------------------------
VirtualMemoryReserve::VirtualMemoryReserve(std::string name)
	: m_name(std::move(name))
{
}

VirtualMemoryReserve::~VirtualMemoryReserve()
{
	pxAssertRel(!m_baseptr, "VirtualMemoryReserve has not been released.");
}

// Notes:
//  * This method should be called if the object is already in an released (unreserved) state.
//    Subsequent calls will be ignored, and the existing reserve will be returned.
//
// Parameters:
//   baseptr - the new base pointer that's about to be assigned
//   size - size of the region pointed to by baseptr
//
void VirtualMemoryReserve::Assign(VirtualMemoryManagerPtr allocator, u8* baseptr, size_t size)
{
	pxAssertRel(size > 0 && Common::IsAlignedPow2(size, __pagesize), "VM allocation is not page aligned");
	pxAssertRel(!m_baseptr, "Virtual memory reserve has already been assigned");

	m_allocator = std::move(allocator);
	m_baseptr = baseptr;
	m_size = size;

	std::string mbkb;
	uint mbytes = size / _1mb;
	if (mbytes)
		mbkb = fmt::format("[{}mb]", mbytes);
	else
		mbkb = fmt::format("[{}kb]", size / 1024);

	DevCon.WriteLn(Color_Gray, "%-32s @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " %s", m_name.c_str(),
		m_baseptr, (uptr)m_baseptr + size, mbkb.c_str());
}

u8* VirtualMemoryReserve::BumpAllocate(VirtualMemoryBumpAllocator& allocator, size_t size)
{
	u8* base = allocator.Alloc(size);
	if (base)
		Assign(allocator.GetAllocator(), base, size);

	return base;
}

void VirtualMemoryReserve::Release()
{
	if (!m_baseptr)
		return;

	m_allocator->Free(m_baseptr, m_size);
	m_baseptr = nullptr;
	m_size = 0;
}

// --------------------------------------------------------------------------------------
//  RecompiledCodeReserve  (implementations)
// --------------------------------------------------------------------------------------

// Constructor!
// Parameters:
//   name - a nice long name that accurately describes the contents of this reserve.
RecompiledCodeReserve::RecompiledCodeReserve(std::string name)
	: VirtualMemoryReserve(std::move(name))
{
}

RecompiledCodeReserve::~RecompiledCodeReserve()
{
	Release();
}

void RecompiledCodeReserve::Assign(VirtualMemoryManagerPtr allocator, size_t offset, size_t size)
{
	// Anything passed to the memory allocator must be page aligned.
	size = Common::PageAlign(size);

	// Since the memory has already been allocated as part of the main memory map, this should never fail.
	u8* base = allocator->Alloc(offset, size);
	if (!base)
	{
		Console.WriteLn("(RecompiledCodeReserve) Failed to allocate %zu bytes for %s at offset %zu", size, m_name.c_str(), offset);
		pxFailRel("RecompiledCodeReserve allocation failed.");
	}

	VirtualMemoryReserve::Assign(std::move(allocator), base, size);
}

void RecompiledCodeReserve::Reset()
{
	if (IsDevBuild && m_baseptr)
	{
		// Clear the recompiled code block to 0xcc (INT3) -- this helps disasm tools show
		// the assembly dump more cleanly.  We don't clear the block on Release builds since
		// it can add a noticeable amount of overhead to large block recompilations.

		std::memset(m_baseptr, 0xCC, m_size);
	}
}

void RecompiledCodeReserve::AllowModification()
{
	HostSys::MemProtect(m_baseptr, m_size, PageAccess_Any());
}

void RecompiledCodeReserve::ForbidModification()
{
	HostSys::MemProtect(m_baseptr, m_size, PageProtectionMode().Read().Execute());
}
