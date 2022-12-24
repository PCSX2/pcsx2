/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "Common.h"
#include "R3000A.h"
#include "VUmicro.h"
#include "newVif.h"
#include "MTVU.h"

#include "Elfheader.h"

#include "System/RecTypes.h"

#include "common/Align.h"
#include "common/MemsetFast.inl"
#include "common/Perf.h"
#include "common/StringUtil.h"
#include "CDVD/CDVD.h"

#include "common/emitter/x86_intrin.h"

#include "GSDumpReplayer.h"

extern R5900cpu GSDumpReplayerCpu;

SSE_MXCSR g_sseMXCSR   = {DEFAULT_sseMXCSR};
SSE_MXCSR g_sseVUMXCSR = {DEFAULT_sseVUMXCSR};

// SetCPUState -- for assignment of SSE roundmodes and clampmodes.
//
void SetCPUState(SSE_MXCSR sseMXCSR, SSE_MXCSR sseVUMXCSR)
{
	//Msgbox::Alert("SetCPUState: Config.sseMXCSR = %x; Config.sseVUMXCSR = %x \n", Config.sseMXCSR, Config.sseVUMXCSR);

	g_sseMXCSR   = sseMXCSR.ApplyReserveMask();
	g_sseVUMXCSR = sseVUMXCSR.ApplyReserveMask();

	_mm_setcsr(g_sseMXCSR.bitmask);
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

void RecompiledCodeReserve::_registerProfiler()
{
	if (m_profiler_name.empty() || !IsOk())
		return;

	Perf::any.map((uptr)m_baseptr, m_size, m_profiler_name.c_str());
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
	_registerProfiler();
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
	// Apple Silicon enforces write protection in hardware.
#if !defined(__APPLE__) || !defined(_M_ARM64)
	HostSys::MemProtect(m_baseptr, m_size, PageAccess_Any());
#endif
}

void RecompiledCodeReserve::ForbidModification()
{
	// Apple Silicon enforces write protection in hardware.
#if !defined(__APPLE__) || !defined(_M_ARM64)
	HostSys::MemProtect(m_baseptr, m_size, PageProtectionMode().Read().Execute());
#endif
}

// Sets the abbreviated name used by the profiler.  Name should be under 10 characters long.
// After a name has been set, a profiler source will be automatically registered and cleared
// in accordance with changes in the reserve area.
RecompiledCodeReserve& RecompiledCodeReserve::SetProfilerName(std::string name)
{
	m_profiler_name = std::move(name);
	_registerProfiler();
	return *this;
}

#include "svnrev.h"

Pcsx2Config EmuConfig;


// This function should be called once during program execution.
void SysLogMachineCaps()
{
	if ( !PCSX2_isReleaseVersion )
	{
		if (GIT_TAGGED_COMMIT) // Nightly builds
		{
			// tagged commit - more modern implementation of dev build versioning
			// - there is no need to include the commit - that is associated with the tag,
			// - git is implied and the tag is timestamped
			Console.WriteLn(Color_StrongGreen, "PCSX2 Nightly - %s Compiled on %s", GIT_TAG, __DATE__);
		} else {
			Console.WriteLn(Color_StrongGreen, "PCSX2 %u.%u.%u-%lld"
#ifndef DISABLE_BUILD_DATE
											   "- compiled on " __DATE__
#endif
				,
				PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo,
				SVN_REV);
		}
	}
	else { // shorter release version string
		Console.WriteLn(Color_StrongGreen, "PCSX2 %u.%u.%u-%lld"
#ifndef DISABLE_BUILD_DATE
			"- compiled on " __DATE__
#endif
			, PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo,
			SVN_REV );
	}

	Console.WriteLn( "Savestate version: 0x%x", g_SaveVersion);
	Console.Newline();

	Console.WriteLn( Color_StrongBlack, "Host Machine Init:" );

	Console.Indent().WriteLn(
		"Operating System =  %s\n"
		"Physical RAM     =  %u MB",

		GetOSVersionString().c_str(),
		(u32)(GetPhysicalMemory() / _1mb)
	);

	u32 speed = x86caps.CalculateMHz();

	Console.Indent().WriteLn(
		"CPU name         =  %s\n"
		"Vendor/Model     =  %s (stepping %02X)\n"
		"CPU speed        =  %u.%03u ghz (%u logical thread%ls)\n"
		"x86PType         =  %s\n"
		"x86Flags         =  %08x %08x\n"
		"x86EFlags        =  %08x",
			x86caps.FamilyName,
			x86caps.VendorName, x86caps.StepID,
			speed / 1000, speed % 1000,
			x86caps.LogicalCores, (x86caps.LogicalCores==1) ? L"" : L"s",
			x86caps.GetTypeName(),
			x86caps.Flags, x86caps.Flags2,
			x86caps.EFlags
	);

	Console.Newline();

	std::string features;

	if( x86caps.hasStreamingSIMD2Extensions )		features += "SSE2 ";
	if( x86caps.hasStreamingSIMD3Extensions )		features += "SSE3 ";
	if( x86caps.hasSupplementalStreamingSIMD3Extensions ) features += "SSSE3 ";
	if( x86caps.hasStreamingSIMD4Extensions )		features += "SSE4.1 ";
	if( x86caps.hasStreamingSIMD4Extensions2 )		features += "SSE4.2 ";
	if( x86caps.hasAVX )							features += "AVX ";
	if( x86caps.hasAVX2 )							features += "AVX2 ";
	if( x86caps.hasFMA)								features += "FMA ";

	if( x86caps.hasStreamingSIMD4ExtensionsA )		features += "SSE4a ";

	StringUtil::StripWhitespace(&features);

	Console.WriteLn(Color_StrongBlack,	"x86 Features Detected:");
	Console.Indent().WriteLn("%s", features.c_str());

	Console.Newline();
}

namespace HostMemoryMap {
	// For debuggers
	extern "C" {
#ifdef _WIN32
	_declspec(dllexport) uptr EEmem, IOPmem, VUmem, EErec, IOPrec, VIF0rec, VIF1rec, mVU0rec, mVU1rec, bumpAllocator;
#else
	__attribute__((visibility("default"), used)) uptr EEmem, IOPmem, VUmem, EErec, IOPrec, VIF0rec, VIF1rec, mVU0rec, mVU1rec, bumpAllocator;
#endif
	}
}

/// Attempts to find a spot near static variables for the main memory
static VirtualMemoryManagerPtr makeMemoryManager(const char* name, const char* file_mapping_name, size_t size, size_t offset_from_base)
{
	// Everything looks nicer when the start of all the sections is a nice round looking number.
	// Also reduces the variation in the address due to small changes in code.
	// Breaks ASLR but so does anything else that tries to make addresses constant for our debugging pleasure
	uptr codeBase = (uptr)(void*)makeMemoryManager / (1 << 28) * (1 << 28);

	// The allocation is ~640mb in size, slighly under 3*2^28.
	// We'll hope that the code generated for the PCSX2 executable stays under 512mb (which is likely)
	// On x86-64, code can reach 8*2^28 from its address [-6*2^28, 4*2^28] is the region that allows for code in the 640mb allocation to reach 512mb of code that either starts at codeBase or 256mb before it.
	// We start high and count down because on macOS code starts at the beginning of useable address space, so starting as far ahead as possible reduces address variations due to code size.  Not sure about other platforms.  Obviously this only actually affects what shows up in a debugger and won't affect performance or correctness of anything.
	for (int offset = 4; offset >= -6; offset--) {
		uptr base = codeBase + (offset << 28) + offset_from_base;
		if ((sptr)base < 0 || (sptr)(base + size - 1) < 0) {
			// VTLB will throw a fit if we try to put EE main memory here
			continue;
		}
		auto mgr = std::make_shared<VirtualMemoryManager>(name, file_mapping_name, base, size, /*upper_bounds=*/0, /*strict=*/true);
		if (mgr->IsOk()) {
			return mgr;
		}
	}

	// If the above failed and it's x86-64, recompiled code is going to break!
	// If it's i386 anything can reach anything so it doesn't matter
	if (sizeof(void*) == 8) {
		pxAssertRel(0, "Failed to find a good place for the memory allocation, recompilers may fail");
	}
	return std::make_shared<VirtualMemoryManager>(name, file_mapping_name, 0, size);
}

// --------------------------------------------------------------------------------------
//  SysReserveVM  (implementations)
// --------------------------------------------------------------------------------------
SysMainMemory::SysMainMemory()
	: m_mainMemory(makeMemoryManager("Main Memory Manager", "pcsx2", HostMemoryMap::MainSize, 0))
	, m_codeMemory(makeMemoryManager("Code Memory Manager", nullptr, HostMemoryMap::CodeSize, HostMemoryMap::MainSize))
	, m_bumpAllocator(m_mainMemory, HostMemoryMap::bumpAllocatorOffset, HostMemoryMap::MainSize - HostMemoryMap::bumpAllocatorOffset)
{
	uptr main_base = (uptr)MainMemory()->GetBase();
	uptr code_base = (uptr)MainMemory()->GetBase();
	HostMemoryMap::EEmem   = main_base + HostMemoryMap::EEmemOffset;
	HostMemoryMap::IOPmem  = main_base + HostMemoryMap::IOPmemOffset;
	HostMemoryMap::VUmem   = main_base + HostMemoryMap::VUmemOffset;
	HostMemoryMap::EErec   = code_base + HostMemoryMap::EErecOffset;
	HostMemoryMap::IOPrec  = code_base + HostMemoryMap::IOPrecOffset;
	HostMemoryMap::VIF0rec = code_base + HostMemoryMap::VIF0recOffset;
	HostMemoryMap::VIF1rec = code_base + HostMemoryMap::VIF1recOffset;
	HostMemoryMap::mVU0rec = code_base + HostMemoryMap::mVU0recOffset;
	HostMemoryMap::mVU1rec = code_base + HostMemoryMap::mVU1recOffset;
	HostMemoryMap::bumpAllocator = main_base + HostMemoryMap::bumpAllocatorOffset;
}

SysMainMemory::~SysMainMemory()
{
	Release();
}

bool SysMainMemory::Allocate()
{
	DevCon.WriteLn(Color_StrongBlue, "Allocating host memory for virtual systems...");
	pxInstallSignalHandler();

	ConsoleIndentScope indent(1);

	m_ee.Assign(MainMemory());
	m_iop.Assign(MainMemory());
	m_vu.Assign(MainMemory());
	vtlb_Core_Alloc();
	return true;
}

void SysMainMemory::Reset()
{
	DevCon.WriteLn( Color_StrongBlue, "Resetting host memory for virtual systems..." );
	ConsoleIndentScope indent(1);

	m_ee.Reset();
	m_iop.Reset();
	m_vu.Reset();

	// Note: newVif is reset as part of other VIF structures.
}

void SysMainMemory::Release()
{
	Console.WriteLn( Color_Blue, "Releasing host memory for virtual systems..." );
	ConsoleIndentScope indent(1);

	hwShutdown();

	vtlb_Core_Free();		// Just to be sure... (calling order could result in it getting missed during Decommit).

	releaseNewVif(0);
	releaseNewVif(1);

	m_ee.Release();
	m_iop.Release();
	m_vu.Release();

	safe_delete(Source_PageFault);
}


// --------------------------------------------------------------------------------------
//  SysCpuProviderPack  (implementations)
// --------------------------------------------------------------------------------------
SysCpuProviderPack::SysCpuProviderPack()
{
	Console.WriteLn( Color_StrongBlue, "Reserving memory for recompilers..." );
	ConsoleIndentScope indent(1);

	recCpu.Reserve();
	psxRec.Reserve();

	CpuMicroVU0.Reserve();
	CpuMicroVU1.Reserve();

	if constexpr (newVifDynaRec)
	{
		dVifReserve(0);
		dVifReserve(1);
	}
}

SysCpuProviderPack::~SysCpuProviderPack()
{
	if (newVifDynaRec)
	{
		dVifRelease(1);
		dVifRelease(0);
	}

	CpuMicroVU1.Shutdown();
	CpuMicroVU0.Shutdown();

	psxRec.Shutdown();
	recCpu.Shutdown();
}

BaseVUmicroCPU* CpuVU0 = nullptr;
BaseVUmicroCPU* CpuVU1 = nullptr;

void SysCpuProviderPack::ApplyConfig() const
{
	Cpu		= CHECK_EEREC	? &recCpu : &intCpu;
	psxCpu	= CHECK_IOPREC	? &psxRec : &psxInt;

	CpuVU0 = &CpuIntVU0;
	CpuVU1 = &CpuIntVU1;

	if( EmuConfig.Cpu.Recompiler.EnableVU0 )
		CpuVU0 = &CpuMicroVU0;

	if( EmuConfig.Cpu.Recompiler.EnableVU1 )
		CpuVU1 = &CpuMicroVU1;

	if (GSDumpReplayer::IsReplayingDump())
		Cpu = &GSDumpReplayerCpu;
}

// Resets all PS2 cpu execution caches, which does not affect that actual PS2 state/condition.
// This can be called at any time outside the context of a Cpu->Execute() block without
// bad things happening (recompilers will slow down for a brief moment since rec code blocks
// are dumped).
// Use this method to reset the recs when important global pointers like the MTGS are re-assigned.
void SysClearExecutionCache()
{
	Cpu->Reset();
	psxCpu->Reset();

	// mVU's VU0 needs to be properly initialized for macro mode even if it's not used for micro mode!
	if (CHECK_EEREC && !EmuConfig.Cpu.Recompiler.EnableVU0)
		CpuMicroVU0.Reset();

	CpuVU0->Reset();
	CpuVU1->Reset();

	if (newVifDynaRec)
	{
		dVifReset(0);
		dVifReset(1);
	}
}

std::string SysGetBiosDiscID()
{
	// FIXME: we should return a serial based on
	// the BIOS being run (either a checksum of the BIOS roms, and/or a string based on BIOS
	// region and revision).
	// Good candidate can be first part of EXTINFO data in the BIOS rom:
	// Example for romver 0160EC20010704
	// 20010704-160707,ROMconf,PS20160EC20010704.bin,kuma@rom-server/~/f10k/g/app/rom
	// 20010704-160707 can be used as unique ID for Bios
	// rom0:EXTINFO first 15 bytes

	return {};
}

// This function always returns a valid DiscID -- using the Sony serial when possible, and
// falling back on the CRC checksum of the ELF binary if the PS2 software being run is
// homebrew or some other serial-less item.
std::string SysGetDiscID()
{
	if( !DiscSerial.empty() ) return DiscSerial;

	if( !ElfCRC )
	{
		// system is currently running the BIOS
		return SysGetBiosDiscID();
	}

	return StringUtil::StdStringFromFormat("%08x", ElfCRC);
}
