// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <atomic>
#include <map>
#include <memory>
#include <string>

// --------------------------------------------------------------------------------------
//  PageProtectionMode
// --------------------------------------------------------------------------------------
class PageProtectionMode
{
protected:
	bool m_read = false;
	bool m_write = false;
	bool m_exec = false;

public:
	__fi constexpr PageProtectionMode() = default;

	__fi constexpr PageProtectionMode& Read(bool allow = true)
	{
		m_read = allow;
		return *this;
	}

	__fi constexpr PageProtectionMode& Write(bool allow = true)
	{
		m_write = allow;
		return *this;
	}

	__fi constexpr PageProtectionMode& Execute(bool allow = true)
	{
		m_exec = allow;
		return *this;
	}

	__fi constexpr PageProtectionMode& All(bool allow = true)
	{
		m_read = m_write = m_exec = allow;
		return *this;
	}

	__fi constexpr bool CanRead() const { return m_read; }
	__fi constexpr bool CanWrite() const { return m_write; }
	__fi constexpr bool CanExecute() const { return m_exec && m_read; }
	__fi constexpr bool IsNone() const { return !m_read && !m_write; }
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

	extern std::string GetFileMappingName(const char* prefix);
	extern void* CreateSharedMemory(const char* name, size_t size);
	extern void DestroySharedMemory(void* ptr);
	extern void* MapSharedMemory(void* handle, size_t offset, void* baseaddr, size_t size, const PageProtectionMode& mode);
	extern void UnmapSharedMemory(void* baseaddr, size_t size);

	/// Installs the specified page fault handler. Only one handler can be active at once.
	bool InstallPageFaultHandler(PageFaultHandler handler);

	/// Removes the page fault handler. handler is only specified to check against the active callback.
	void RemovePageFaultHandler(PageFaultHandler handler);

	/// JIT write protect for Apple Silicon. Needs to be called prior to writing to any RWX pages.
#if !defined(__APPLE__) || !defined(_M_ARM64)
	// clang-format -off
	[[maybe_unused]] __fi static void BeginCodeWrite() {}
	[[maybe_unused]] __fi static void EndCodeWrite() {}
	// clang-format on
#else
	void BeginCodeWrite();
	void EndCodeWrite();
#endif

	/// Flushes the instruction cache on the host for the specified range.
	/// Only needed on ARM64, X86 has coherent D/I cache.
#ifdef _M_X86
	[[maybe_unused]] __fi static void FlushInstructionCache(void* address, u32 size) {}
#else
	void FlushInstructionCache(void* address, u32 size);
#endif
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
