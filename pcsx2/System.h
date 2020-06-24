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

#include "Utilities/SafeArray.h"
#include "Utilities/Threading.h"		// to use threading stuff, include the Threading namespace in your file.
#include "CDVD/CDVDaccess.h"

#include "vtlb.h"

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
	// PS2 main memory, SPR, and ROMs
	extern __pagealigned EEVM_MemoryAllocMess EEmem;

	// IOP main memory and ROMs
	extern __pagealigned u8 IOPmem[(sizeof(IopVM_MemoryAllocMess) + __pagesize - 1) & -__pagesize];

	// VU0 and VU1 memory.
	extern __pagealigned u8 VUmem[_64kb];

	// EE recompiler code cache area (64mb)
	extern __pagealigned u8 EErec[_64mb];

	// IOP recompiler code cache area (16 or 32mb)
	extern __pagealigned u8 IOPrec[_32mb];

	// newVif0 recompiler code cache area (16mb)
	extern __pagealigned u8 VIF0rec[_8mb];

	// newVif1 recompiler code cache area (32mb)
	extern __pagealigned u8 VIF1rec[_8mb];

	// microVU1 recompiler code cache area (32 or 64mb)
	extern __pagealigned u8 mVU0rec[_64mb];

	// microVU0 recompiler code cache area (64mb)
	extern __pagealigned u8 mVU1rec[_64mb];

	// Bump allocator for any other small allocations
	extern __pagealigned u8 bumpAllocator[_64mb];
}

// --------------------------------------------------------------------------------------
//  SysMainMemory
// --------------------------------------------------------------------------------------
// This class provides the main memory for the virtual machines.
class SysMainMemory
{
protected:
	VirtualMemoryBumpAllocator m_bumpAllocator;
	eeMemoryReserve            m_ee;
	iopMemoryReserve           m_iop;
	vuMemoryReserve            m_vu;

public:
	SysMainMemory();
	virtual ~SysMainMemory();

	VirtualMemoryBumpAllocator& BumpAllocator() { return m_bumpAllocator; }

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
extern void SysOutOfMemory_EmergencyResponse(uptr blocksize);

extern u8 *SysMmapEx(uptr base, u32 size, uptr bounds, const char *caller="Unnamed");
extern void vSyncDebugStuff( uint frame );
extern void NTFS_CompressFile( const wxString& file, bool compressStatus=true );

extern wxString SysGetBiosDiscID();
extern wxString SysGetDiscID();

extern SysMainMemory& GetVmMemory();

// --------------------------------------------------------------------------------------
//  PCSX2_SEH - Defines existence of "built in" Structured Exception Handling support.
// --------------------------------------------------------------------------------------
// This should be available on Windows, via Microsoft or Intel compilers (I'm pretty sure Intel
// supports native SEH model).  GNUC in Windows, or any compiler in a non-windows platform, will
// need to use setjmp/longjmp instead to exit recompiled code.
//

//#define PCSX2_SEH		0		// use this to force disable SEH on win32, to test setjmp functionality.

#ifndef PCSX2_SEH
#	if defined(_WIN32) && !defined(__GNUC__)
#		define PCSX2_SEH	1
#	else
#		define PCSX2_SEH	0
#	endif
#endif

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

namespace Msgbox
{
	extern bool	Alert( const wxString& text, const wxString& caption=_("PCSX2 Message"), int icon=wxICON_EXCLAMATION );
	extern bool	OkCancel( const wxString& text, const wxString& caption=_("PCSX2 Message"), int icon=0 );
	extern bool	YesNo( const wxString& text, const wxString& caption=_("PCSX2 Message"), int icon=wxICON_QUESTION );

	extern int	Assertion( const wxString& text, const wxString& stacktrace );
}

