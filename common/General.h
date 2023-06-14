/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023 PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <cstring>

// This macro is actually useful for about any and every possible application of C++
// equality operators.
#define OpEqu(field) (field == right.field)

// Macro used for removing some of the redtape involved in defining bitfield/union helpers.
//
#define BITFIELD32() \
	union \
	{ \
		u32 bitset; \
		struct \
		{

#define BITFIELD_END \
		}; \
	};

// --------------------------------------------------------------------------------------
//  PageProtectionMode
// --------------------------------------------------------------------------------------
class PageProtectionMode
{
protected:
	bool m_read;
	bool m_write;
	bool m_exec;

public:
	PageProtectionMode()
	{
		All(false);
	}

	PageProtectionMode& Read(bool allow = true)
	{
		m_read = allow;
		return *this;
	}

	PageProtectionMode& Write(bool allow = true)
	{
		m_write = allow;
		return *this;
	}

	PageProtectionMode& Execute(bool allow = true)
	{
		m_exec = allow;
		return *this;
	}

	PageProtectionMode& All(bool allow = true)
	{
		m_read = m_write = m_exec = allow;
		return *this;
	}

	bool CanRead() const { return m_read; }
	bool CanWrite() const { return m_write; }
	bool CanExecute() const { return m_exec && m_read; }
	bool IsNone() const { return !m_read && !m_write; }

	std::string ToString() const;
};

static __fi PageProtectionMode PageAccess_None()
{
	return PageProtectionMode();
}

static __fi PageProtectionMode PageAccess_ReadOnly()
{
	return PageProtectionMode().Read();
}

static __fi PageProtectionMode PageAccess_WriteOnly()
{
	return PageProtectionMode().Write();
}

static __fi PageProtectionMode PageAccess_ReadWrite()
{
	return PageAccess_ReadOnly().Write();
}

static __fi PageProtectionMode PageAccess_ExecOnly()
{
	return PageAccess_ReadOnly().Execute();
}

static __fi PageProtectionMode PageAccess_Any()
{
	return PageProtectionMode().All();
}

struct PageFaultInfo
{
	uptr pc;
	uptr addr;
};

using PageFaultHandler = bool(*)(const PageFaultInfo& info);

// --------------------------------------------------------------------------------------
//  HostSys
// --------------------------------------------------------------------------------------
namespace HostSys
{
	// Maps a block of memory for use as a recompiled code buffer.
	// Returns NULL on allocation failure.
	extern void* Mmap(void* base, size_t size, const PageProtectionMode& mode);

	// Unmaps a block allocated by SysMmap
	extern void Munmap(void* base, size_t size);

	extern void MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode);

	template <uint size>
	void MemProtectStatic(u8 (&arr)[size], const PageProtectionMode& mode)
	{
		MemProtect(arr, size, mode);
	}

	extern std::string GetFileMappingName(const char* prefix);
	extern void* CreateSharedMemory(const char* name, size_t size);
	extern void DestroySharedMemory(void* ptr);
	extern void* MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode);
	extern void UnmapSharedMemory(void* baseaddr, size_t size);

	/// Installs the specified page fault handler. Only one handler can be active at once.
	bool InstallPageFaultHandler(PageFaultHandler handler);

	/// Removes the page fault handler. handler is only specified to check against the active callback.
	void RemovePageFaultHandler(PageFaultHandler handler);
}

class SharedMemoryMappingArea
{
public:
	static std::unique_ptr<SharedMemoryMappingArea> Create(size_t size);

	~SharedMemoryMappingArea();

	__fi size_t GetSize() const { return m_size; }
	__fi size_t GetNumPages() const { return m_num_pages; }

	__fi u8* BasePointer() const { return m_base_ptr; }
	__fi u8* OffsetPointer(size_t offset) const { return m_base_ptr + offset; }
	__fi u8* PagePointer(size_t page) const { return m_base_ptr + __pagesize * page; }

	u8* Map(void* file_handle, size_t file_offset, void* map_base, size_t map_size, const PageProtectionMode& mode);
	bool Unmap(void* map_base, size_t map_size);

private:
	SharedMemoryMappingArea(u8* base_ptr, size_t size, size_t num_pages);

	u8* m_base_ptr;
	size_t m_size;
	size_t m_num_pages;
	size_t m_num_mappings = 0;

#ifdef _WIN32
	using PlaceholderMap = std::map<size_t, size_t>;

	PlaceholderMap::iterator FindPlaceholder(size_t page);

	PlaceholderMap m_placeholder_ranges;
#endif
};


// Safe version of Munmap -- NULLs the pointer variable immediately after free'ing it.
#define SafeSysMunmap(ptr, size) \
	((void)(HostSys::Munmap(ptr, size), (ptr) = 0))

// This method can clear any object-like entity -- which is anything that is not a pointer.
// Structures, static arrays, etc.  No need to include sizeof() crap, this does it automatically
// for you!
template <typename T>
static __fi void memzero(T& object)
{
	static_assert(std::is_trivially_copyable_v<T>);
	std::memset(&object, 0, sizeof(T));
}

// This method clears an object with the given 8 bit value.
template <u8 data, typename T>
static __fi void memset8(T& object)
{
	static_assert(std::is_trivially_copyable_v<T>);
	std::memset(&object, data, sizeof(T));
}

extern u64 GetTickFrequency();
extern u64 GetCPUTicks();
extern u64 GetPhysicalMemory();
/// Spin for a short period of time (call while spinning waiting for a lock)
/// Returns the approximate number of ns that passed
extern u32 ShortSpin();
/// Number of ns to spin for before sleeping a thread
extern const u32 SPIN_TIME_NS;
/// Like C abort() but adds the given message to the crashlog
[[noreturn]] void AbortWithMessage(const char* msg);

extern std::string GetOSVersionString();

namespace Common
{
	/// Abstracts platform-specific code for asynchronously playing a sound.
	/// On Windows, this will use PlaySound(). On Linux, it will shell out to aplay. On MacOS, it uses NSSound.
	bool PlaySoundAsync(const char* path);
} // namespace Common
