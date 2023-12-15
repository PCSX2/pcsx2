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

#if defined(_WIN32)

#include "common/BitUtils.h"
#include "common/RedtapeWindows.h"
#include "common/Console.h"
#include "common/General.h"
#include "common/StringUtil.h"
#include "common/AlignedMalloc.h"
#include "common/Assertions.h"

#include "fmt/core.h"
#include "fmt/format.h"

#include <mutex>

static std::recursive_mutex s_exception_handler_mutex;
static PageFaultHandler s_exception_handler_callback;
static void* s_exception_handler_handle;
static bool s_in_exception_handler;

long __stdcall SysPageFaultExceptionFilter(EXCEPTION_POINTERS* eps)
{
	// Executing the handler concurrently from multiple threads wouldn't go down well.
	std::unique_lock lock(s_exception_handler_mutex);

	// Prevent recursive exception filtering.
	if (s_in_exception_handler)
		return EXCEPTION_CONTINUE_SEARCH;

	// Only interested in page faults.
	if (eps->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
		return EXCEPTION_CONTINUE_SEARCH;

	void* const exception_pc = reinterpret_cast<void*>(eps->ContextRecord->Rip);

	const PageFaultInfo pfi{(uptr)exception_pc, (uptr)eps->ExceptionRecord->ExceptionInformation[1]};

	s_in_exception_handler = true;

	const bool handled = s_exception_handler_callback(pfi);

	s_in_exception_handler = false;
	
	return handled ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

bool HostSys::InstallPageFaultHandler(PageFaultHandler handler)
{
	std::unique_lock lock(s_exception_handler_mutex);
	pxAssertRel(!s_exception_handler_callback, "A page fault handler is already registered.");
	if (!s_exception_handler_handle)
	{
		s_exception_handler_handle = AddVectoredExceptionHandler(TRUE, SysPageFaultExceptionFilter);
		if (!s_exception_handler_handle)
			return false;
	}

	s_exception_handler_callback = handler;
	return true;
}

void HostSys::RemovePageFaultHandler(PageFaultHandler handler)
{
	std::unique_lock lock(s_exception_handler_mutex);
	pxAssertRel(!s_exception_handler_callback || s_exception_handler_callback == handler,
		"Not removing the same handler previously registered.");
	s_exception_handler_callback = nullptr;

	if (s_exception_handler_handle)
	{
		RemoveVectoredExceptionHandler(s_exception_handler_handle);
		s_exception_handler_handle = {};
	}
}

static DWORD ConvertToWinApi(const PageProtectionMode& mode)
{
	DWORD winmode = PAGE_NOACCESS;

	// Windows has some really bizarre memory protection enumeration that uses bitwise
	// numbering (like flags) but is in fact not a flag value.  *Someone* from the early
	// microsoft days wasn't a very good coder, me thinks.  --air

	if (mode.CanExecute())
	{
		winmode = mode.CanWrite() ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ;
	}
	else if (mode.CanRead())
	{
		winmode = mode.CanWrite() ? PAGE_READWRITE : PAGE_READONLY;
	}

	return winmode;
}

void* HostSys::Mmap(void* base, size_t size, const PageProtectionMode& mode)
{
	if (mode.IsNone())
		return nullptr;

	return VirtualAlloc(base, size, MEM_RESERVE | MEM_COMMIT, ConvertToWinApi(mode));
}

void HostSys::Munmap(void* base, size_t size)
{
	if (!base)
		return;

	VirtualFree((void*)base, 0, MEM_RELEASE);
}

void HostSys::MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	pxAssert((size & (__pagesize - 1)) == 0);

	DWORD OldProtect; // enjoy my uselessness, yo!
	if (!VirtualProtect(baseaddr, size, ConvertToWinApi(mode), &OldProtect))
		pxFail("VirtualProtect() failed");
}

std::string HostSys::GetFileMappingName(const char* prefix)
{
	const unsigned pid = GetCurrentProcessId();
	return fmt::format("{}_{}", prefix, pid);
}

void* HostSys::CreateSharedMemory(const char* name, size_t size)
{
	return static_cast<void*>(CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
		static_cast<DWORD>(size >> 32), static_cast<DWORD>(size), StringUtil::UTF8StringToWideString(name).c_str()));
}

void HostSys::DestroySharedMemory(void* ptr)
{
	CloseHandle(static_cast<HANDLE>(ptr));
}

void* HostSys::MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode)
{
	void* ret = MapViewOfFileEx(static_cast<HANDLE>(handle), FILE_MAP_READ | FILE_MAP_WRITE,
		static_cast<DWORD>(offset >> 32), static_cast<DWORD>(offset), size, baseaddr);
	if (!ret)
		return nullptr;

	const DWORD prot = ConvertToWinApi(mode);
	if (prot != PAGE_READWRITE)
	{
		DWORD old_prot;
		if (!VirtualProtect(ret, size, prot, &old_prot))
			pxFail("Failed to protect memory mapping");
	}
	return ret;
}

void HostSys::UnmapSharedMemory(void* baseaddr, size_t size)
{
	if (!UnmapViewOfFile(baseaddr))
		pxFail("Failed to unmap shared memory");
}

SharedMemoryMappingArea::SharedMemoryMappingArea(u8* base_ptr, size_t size, size_t num_pages)
	: m_base_ptr(base_ptr)
	, m_size(size)
	, m_num_pages(num_pages)
{
	m_placeholder_ranges.emplace(0, size);
}

SharedMemoryMappingArea::~SharedMemoryMappingArea()
{
	pxAssertRel(m_num_mappings == 0, "No mappings left");

	// hopefully this will be okay, and we don't need to coalesce all the placeholders...
	if (!VirtualFreeEx(GetCurrentProcess(), m_base_ptr, 0, MEM_RELEASE))
		pxFailRel("Failed to release shared memory area");
}

SharedMemoryMappingArea::PlaceholderMap::iterator SharedMemoryMappingArea::FindPlaceholder(size_t offset)
{
	if (m_placeholder_ranges.empty())
		return m_placeholder_ranges.end();

	// this will give us an iterator equal or after page
	auto it = m_placeholder_ranges.lower_bound(offset);
	if (it == m_placeholder_ranges.end())
	{
		// check the last page
		it = (++m_placeholder_ranges.rbegin()).base();
	}

	// it's the one we found?
	if (offset >= it->first && offset < it->second)
		return it;

	// otherwise try the one before
	if (it == m_placeholder_ranges.begin())
		return m_placeholder_ranges.end();

	--it;
	if (offset >= it->first && offset < it->second)
		return it;
	else
		return m_placeholder_ranges.end();
}

std::unique_ptr<SharedMemoryMappingArea> SharedMemoryMappingArea::Create(size_t size)
{
	pxAssertRel(Common::IsAlignedPow2(size, __pagesize), "Size is page aligned");

	void* alloc = VirtualAlloc2(GetCurrentProcess(), nullptr, size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
	if (!alloc)
		return nullptr;

	return std::unique_ptr<SharedMemoryMappingArea>(new SharedMemoryMappingArea(static_cast<u8*>(alloc), size, size / __pagesize));
}

u8* SharedMemoryMappingArea::Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const size_t map_offset = static_cast<u8*>(map_base) - m_base_ptr;
	pxAssert(Common::IsAlignedPow2(map_offset, __pagesize));
	pxAssert(Common::IsAlignedPow2(map_size, __pagesize));

	// should be a placeholder. unless there's some other mapping we didn't free.
	PlaceholderMap::iterator phit = FindPlaceholder(map_offset);
	pxAssertMsg(phit != m_placeholder_ranges.end(), "Page we're mapping is a placeholder");
	pxAssertMsg(map_offset >= phit->first && map_offset < phit->second, "Page is in returned placeholder range");
	pxAssertMsg((map_offset + map_size) <= phit->second, "Page range is in returned placeholder range");

	// do we need to split to the left? (i.e. is there a placeholder before this range)
	const size_t old_ph_end = phit->second;
	if (map_offset != phit->first)
	{
		phit->second = map_offset;

		// split it (i.e. left..start and start..end are now separated)
		if (!VirtualFreeEx(GetCurrentProcess(), OffsetPointer(phit->first),
				(map_offset - phit->first), MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
		{
			pxFailRel("Failed to left split placeholder for map");
		}
	}
	else
	{
		// start of the placeholder is getting used, we'll split it right below if there's anything left over
		m_placeholder_ranges.erase(phit);
	}

	// do we need to split to the right? (i.e. is there a placeholder after this range)
	if ((map_offset + map_size) != old_ph_end)
	{
		// split out end..ph_end
		m_placeholder_ranges.emplace(map_offset + map_size, old_ph_end);

		if (!VirtualFreeEx(GetCurrentProcess(), OffsetPointer(map_offset), map_size,
				MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
		{
			pxFailRel("Failed to right split placeholder for map");
		}
	}

	// actually do the mapping, replacing the placeholder on the range
	if (!MapViewOfFile3(static_cast<HANDLE>(file_handle), GetCurrentProcess(),
			map_base, file_offset, map_size, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0))
	{
		Console.Error("(SharedMemoryMappingArea) MapViewOfFile3() failed: %u", GetLastError());
		return nullptr;
	}

	const DWORD prot = ConvertToWinApi(mode);
	if (prot != PAGE_READWRITE)
	{
		DWORD old_prot;
		if (!VirtualProtect(map_base, map_size, prot, &old_prot))
			pxFail("Failed to protect memory mapping");
	}

	m_num_mappings++;
	return static_cast<u8*>(map_base);
}

bool SharedMemoryMappingArea::Unmap(void* map_base, size_t map_size)
{
	pxAssert(static_cast<u8*>(map_base) >= m_base_ptr && static_cast<u8*>(map_base) < (m_base_ptr + m_size));

	const size_t map_offset = static_cast<u8*>(map_base) - m_base_ptr;
	pxAssert(Common::IsAlignedPow2(map_offset, __pagesize));
	pxAssert(Common::IsAlignedPow2(map_size, __pagesize));

	// unmap the specified range
	if (!UnmapViewOfFile2(GetCurrentProcess(), map_base, MEM_PRESERVE_PLACEHOLDER))
	{
		Console.Error("(SharedMemoryMappingArea) UnmapViewOfFile2() failed: %u", GetLastError());
		return false;
	}

	// can we coalesce to the left?
	PlaceholderMap::iterator left_it = (map_offset > 0) ? FindPlaceholder(map_offset - 1) : m_placeholder_ranges.end();
	if (left_it != m_placeholder_ranges.end())
	{
		// the left placeholder should end at our start
		pxAssert(map_offset == left_it->second);
		left_it->second = map_offset + map_size;

		// combine placeholders before and the range we're unmapping, i.e. to the left
		if (!VirtualFreeEx(GetCurrentProcess(), OffsetPointer(left_it->first),
				 left_it->second - left_it->first, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
		{
			pxFail("Failed to coalesce placeholders left for unmap");
		}
	}
	else
	{
		// this is a new placeholder
		left_it = m_placeholder_ranges.emplace(map_offset, map_offset + map_size).first;
	}

	// can we coalesce to the right?
	PlaceholderMap::iterator right_it = ((map_offset + map_size) < m_size) ? FindPlaceholder(map_offset + map_size) : m_placeholder_ranges.end();
	if (right_it != m_placeholder_ranges.end())
	{
		// should start at our end
		pxAssert(right_it->first == (map_offset + map_size));
		left_it->second = right_it->second;
		m_placeholder_ranges.erase(right_it);

		// combine our placeholder and the next, i.e. to the right
		if (!VirtualFreeEx(GetCurrentProcess(), OffsetPointer(left_it->first),
				left_it->second - left_it->first, MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
		{
			pxFail("Failed to coalescae placeholders right for unmap");
		}
	}

	m_num_mappings--;
	return true;
}

#endif
