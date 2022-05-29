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

#include "SysForwardDefs.h"

#include "common/Exceptions.h"
#include "common/SafeArray.h"
#include "common/Threading.h"		// to use threading stuff, include the Threading namespace in your file.

#include "vtlb.h"

#include "Config.h"

typedef SafeArray<u8> VmStateBuffer;

class BaseVUmicroCPU;
class RecompiledCodeReserve;

// This is a table of default virtual map addresses for ps2vm components.  These locations
// are provided and used to assist in debugging and possibly hacking; as it makes it possible
// for a programmer to know exactly where to look (consistently!) for the base address of
// the various virtual machine components.  These addresses can be keyed directly into the
// debugger's disasm window to get disassembly of recompiled code, and they can be used to help
// identify recompiled code addresses in the callstack.

// All of these areas should be reserved as soon as possible during program startup, and its
// important that none of the areas overlap.  In all but superVU's case, failure due to overlap
// or other conflict will result in the operating system picking a preferred address for the mapping.

namespace HostMemoryMap
{
	static const u32 Size = 0x28000000;

	// The actual addresses may not be equivalent to Base + Offset in the event that allocation at Base failed
	// Each of these offsets has a debugger-accessible equivalent variable without the Offset suffix that will hold the actual address (not here because we don't want code using it)

	// PS2 main memory, SPR, and ROMs
	static const u32 EEmemOffset   = 0x00000000;

	// IOP main memory and ROMs
	static const u32 IOPmemOffset  = 0x04000000;

	// VU0 and VU1 memory.
	static const u32 VUmemOffset   = 0x08000000;

	// EE recompiler code cache area (64mb)
	static const u32 EErecOffset   = 0x10000000;

	// IOP recompiler code cache area (16 or 32mb)
	static const u32 IOPrecOffset  = 0x14000000;

	// newVif0 recompiler code cache area (16mb)
	static const u32 VIF0recOffset = 0x16000000;

	// newVif1 recompiler code cache area (32mb)
	static const u32 VIF1recOffset = 0x18000000;

	// microVU1 recompiler code cache area (32 or 64mb)
	static const u32 mVU0recOffset = 0x1C000000;

	// microVU0 recompiler code cache area (64mb)
	static const u32 mVU1recOffset = 0x20000000;

	// Bump allocator for any other small allocations
	// size: Difference between it and HostMemoryMap::Size, so nothing should allocate higher than it!
	static const u32 bumpAllocatorOffset = 0x24000000;
}

// --------------------------------------------------------------------------------------
//  SysMainMemory
// --------------------------------------------------------------------------------------
// This class provides the main memory for the virtual machines.
class SysMainMemory
{
protected:
	const VirtualMemoryManagerPtr m_mainMemory;
	VirtualMemoryBumpAllocator    m_bumpAllocator;
	eeMemoryReserve               m_ee;
	iopMemoryReserve              m_iop;
	vuMemoryReserve               m_vu;

public:
	SysMainMemory();
	virtual ~SysMainMemory();

	const VirtualMemoryManagerPtr& MainMemory()    { return m_mainMemory; }
	VirtualMemoryBumpAllocator&    BumpAllocator() { return m_bumpAllocator; }

	virtual void ReserveAll();
	virtual void CommitAll();
	virtual void ResetAll();
	virtual void DecommitAll();
	virtual void ReleaseAll();
};

// --------------------------------------------------------------------------------------
//  SysAllocVM
// --------------------------------------------------------------------------------------
class SysAllocVM
{
public:
	SysAllocVM();
	virtual ~SysAllocVM();

protected:
	void CleanupMess() noexcept;
};

// --------------------------------------------------------------------------------------
//  SysCpuProviderPack
// --------------------------------------------------------------------------------------
class SysCpuProviderPack
{
protected:
	ScopedExcept m_RecExceptionEE;
	ScopedExcept m_RecExceptionIOP;

public:
	std::unique_ptr<CpuInitializerSet> CpuProviders;

	SysCpuProviderPack();
	virtual ~SysCpuProviderPack();

	void ApplyConfig() const;

	bool HadSomeFailures( const Pcsx2Config::RecompilerOptions& recOpts ) const;

	bool IsRecAvailable_EE() const		{ return !m_RecExceptionEE; }
	bool IsRecAvailable_IOP() const		{ return !m_RecExceptionIOP; }

	BaseException* GetException_EE() const	{ return m_RecExceptionEE.get(); }
	BaseException* GetException_IOP() const	{ return m_RecExceptionIOP.get(); }

	bool IsRecAvailable_MicroVU0() const;
	bool IsRecAvailable_MicroVU1() const;
	BaseException* GetException_MicroVU0() const;
	BaseException* GetException_MicroVU1() const;

protected:
	void CleanupMess() noexcept;
};

// GetCpuProviders - this function is not implemented by PCSX2 core -- it must be
// implemented by the provisioning interface.
extern SysCpuProviderPack& GetCpuProviders();

extern void SysLogMachineCaps();		// Detects cpu type and fills cpuInfo structs.
extern void SysClearExecutionCache();	// clears recompiled execution caches!

extern u8 *SysMmapEx(uptr base, u32 size, uptr bounds, const char *caller="Unnamed");

extern std::string SysGetBiosDiscID();
extern std::string SysGetDiscID();

extern SysMainMemory& GetVmMemory();

// special macro which disables inlining on functions that require their own function stackframe.
// This is due to how Win32 handles structured exception handling.  Linux uses signals instead
// of SEH, and so these functions can be inlined.
#ifdef _WIN32
#	define __unique_stackframe __noinline
#else
#	define __unique_stackframe
#endif


//////////////////////////////////////////////////////////////////////////////////////////
// Different types of message boxes that the emulator can employ from the friendly confines
// of it's blissful unawareness of whatever GUI it runs under. :)  All message boxes exhibit
// blocking behavior -- they prompt the user for action and only return after the user has
// responded to the prompt.
//

#ifndef PCSX2_CORE
#include <wx/string.h>

namespace Msgbox
{
	extern bool	Alert( const wxString& text, const wxString& caption="PCSX2 Message", int icon=wxICON_EXCLAMATION );
	extern bool	OkCancel( const wxString& text, const wxString& caption="PCSX2 Message", int icon=0 );
	extern bool	YesNo( const wxString& text, const wxString& caption="PCSX2 Message", int icon=wxICON_QUESTION );

	extern int	Assertion( const wxString& text, const wxString& stacktrace );
}
#endif

#ifdef _WIN32
extern void CheckIsUserOnHighPerfPowerPlan();
#endif

extern void SetCPUState(SSE_MXCSR sseMXCSR, SSE_MXCSR sseVUMXCSR);
extern SSE_MXCSR g_sseVUMXCSR, g_sseMXCSR;
