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

#include <atomic>
#include <string>
#include "common/Pcsx2Defs.h"

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

// --------------------------------------------------------------------------------------
//  HostSys
// --------------------------------------------------------------------------------------
// (this namespace name sucks, and is a throw-back to an older attempt to make things cross
// platform prior to wxWidgets .. it should prolly be removed -- air)
namespace HostSys
{
	void* MmapReserve(uptr base, size_t size);
	bool MmapCommit(uptr base, size_t size, const PageProtectionMode& mode);
	void MmapReset(uptr base, size_t size);

	void* MmapReservePtr(void* base, size_t size);
	bool MmapCommitPtr(void* base, size_t size, const PageProtectionMode& mode);
	void MmapResetPtr(void* base, size_t size);

	// Maps a block of memory for use as a recompiled code buffer.
	// Returns NULL on allocation failure.
	extern void* Mmap(uptr base, size_t size);

	// Unmaps a block allocated by SysMmap
	extern void Munmap(uptr base, size_t size);

	extern void MemProtect(void* baseaddr, size_t size, const PageProtectionMode& mode);

	extern void Munmap(void* base, size_t size);

	template <uint size>
	void MemProtectStatic(u8 (&arr)[size], const PageProtectionMode& mode)
	{
		MemProtect(arr, size, mode);
	}
} // namespace HostSys

// Safe version of Munmap -- NULLs the pointer variable immediately after free'ing it.
#define SafeSysMunmap(ptr, size) \
	((void)(HostSys::Munmap((uptr)(ptr), size), (ptr) = 0))

extern void InitCPUTicks();
extern u64 GetTickFrequency();
extern u64 GetCPUTicks();
extern u64 GetPhysicalMemory();
/// Spin for a short period of time (call while spinning waiting for a lock)
/// Returns the approximate number of ns that passed
extern u32 ShortSpin();
/// Number of ns to spin for before sleeping a thread
extern const u32 SPIN_TIME_NS;

extern std::string GetOSVersionString();

void ScreensaverAllow(bool allow);
