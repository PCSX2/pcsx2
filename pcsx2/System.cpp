/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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
#include "GS/Renderers/Common/GSFunctionMap.h"
#include "CDVD/CDVD.h"
#include "Elfheader.h"
#include "GSDumpReplayer.h"
#include "Host.h"
#include "IopMem.h"
#include "MTVU.h"
#include "R3000A.h"
#include "VUmicro.h"
#include "ps2/BiosTools.h"
#include "svnrev.h"
#include "SysForwardDefs.h"
#include "x86/newVif.h"
#include "cpuinfo.h"

#include "common/BitUtils.h"
#include "common/Perf.h"
#include "common/StringUtil.h"

#ifdef _M_X86
#include "common/emitter/x86_intrin.h"
#endif

extern R5900cpu GSDumpReplayerCpu;

Pcsx2Config EmuConfig;

SSE_MXCSR g_sseMXCSR = {DEFAULT_sseMXCSR};
SSE_MXCSR g_sseVU0MXCSR = {DEFAULT_sseVUMXCSR};
SSE_MXCSR g_sseVU1MXCSR = {DEFAULT_sseVUMXCSR};

namespace SysMemory
{
	static u8* TryAllocateVirtualMemory(const char* name, void* file_handle, uptr base, size_t size);
	static u8* AllocateVirtualMemory(const char* name, void* file_handle, size_t size, size_t offset_from_base);

	static bool AllocateMemoryMap();
	static void DumpMemoryMap();
	static void ReleaseMemoryMap();

	static u8* s_data_memory;
	static void* s_data_memory_file_handle;
	static u8* s_code_memory;
} // namespace SysMemory

// SetCPUState -- for assignment of SSE roundmodes and clampmodes.
//
void SetCPUState(SSE_MXCSR sseMXCSR, SSE_MXCSR sseVU0MXCSR, SSE_MXCSR sseVU1MXCSR)
{
	//Msgbox::Alert("SetCPUState: Config.sseMXCSR = %x; Config.sseVUMXCSR = %x \n", Config.sseMXCSR, Config.sseVUMXCSR);

	g_sseMXCSR = sseMXCSR.ApplyReserveMask();
	g_sseVU0MXCSR = sseVU0MXCSR.ApplyReserveMask();
	g_sseVU1MXCSR = sseVU1MXCSR.ApplyReserveMask();

	_mm_setcsr(g_sseMXCSR.bitmask);
}

// This function should be called once during program execution.
void SysLogMachineCaps()
{
	if (!PCSX2_isReleaseVersion)
	{
		if (GIT_TAGGED_COMMIT) // Nightly builds
		{
			// tagged commit - more modern implementation of dev build versioning
			// - there is no need to include the commit - that is associated with the tag,
			// - git is implied and the tag is timestamped
			Console.WriteLn(Color_StrongGreen, "PCSX2 Nightly - %s Compiled on %s", GIT_TAG, __DATE__);
		}
		else
		{
			Console.WriteLn(Color_StrongGreen, "PCSX2 %u.%u.%u-%lld"
#ifndef DISABLE_BUILD_DATE
											   "- compiled on " __DATE__
#endif
				,
				PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo,
				SVN_REV);
		}
	}
	else
	{ // shorter release version string
		Console.WriteLn(Color_StrongGreen, "PCSX2 %u.%u.%u-%lld"
#ifndef DISABLE_BUILD_DATE
										   "- compiled on " __DATE__
#endif
			,
			PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo,
			SVN_REV);
	}

	Console.WriteLn("Savestate version: 0x%x", g_SaveVersion);
	Console.Newline();

	Console.WriteLn(Color_StrongBlack, "Host Machine Init:");

	Console.Indent().WriteLn(
		"Operating System = %s\n"
		"Physical RAM     = %u MB",

		GetOSVersionString().c_str(),
		(u32)(GetPhysicalMemory() / _1mb));

    cpuinfo_initialize();
	Console.Indent().WriteLn("Processor        = %s", cpuinfo_get_package(0)->name);
	Console.Indent().WriteLn("Core Count       = %u cores", cpuinfo_get_cores_count());
	Console.Indent().WriteLn("Thread Count     = %u threads", cpuinfo_get_processors_count());

	Console.Newline();

	std::string features;

	if (x86caps.hasAVX)
		features += "AVX ";
	if (x86caps.hasAVX2)
		features += "AVX2 ";

	StringUtil::StripWhitespace(&features);

	Console.WriteLn(Color_StrongBlack, "x86 Features Detected:");
	Console.Indent().WriteLn("%s", features.c_str());

	Console.Newline();
}

namespace HostMemoryMap
{
	// For debuggers
	extern "C" {
#ifdef _WIN32
	_declspec(dllexport) uptr EEmem, IOPmem, VUmem;
#else
	__attribute__((visibility("default"), used)) uptr EEmem, IOPmem, VUmem;
#endif
	}
} // namespace HostMemoryMap

u8* SysMemory::TryAllocateVirtualMemory(const char* name, void* file_handle, uptr base, size_t size)
{
	u8* baseptr;

	if (file_handle)
		baseptr = static_cast<u8*>(HostSys::MapSharedMemory(file_handle, 0, (void*)base, size, PageAccess_ReadWrite()));
	else
		baseptr = static_cast<u8*>(HostSys::Mmap((void*)base, size, PageAccess_Any()));

	if (!baseptr)
		return nullptr;

	if ((uptr)baseptr != base)
	{
		if (file_handle)
		{
			if (baseptr)
				HostSys::UnmapSharedMemory(baseptr, size);
		}
		else
		{
			if (baseptr)
				HostSys::Munmap(baseptr, size);
		}

		return nullptr;
	}

	DevCon.WriteLn(Color_Gray, "%-32s @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " %s", name,
		baseptr, (uptr)baseptr + size, fmt::format("[{}mb]", size / _1mb).c_str());

	return baseptr;
}

u8* SysMemory::AllocateVirtualMemory(const char* name, void* file_handle, size_t size, size_t offset_from_base)
{
	pxAssertRel(Common::IsAlignedPow2(size, __pagesize), "Virtual memory size is page aligned");

	// Everything looks nicer when the start of all the sections is a nice round looking number.
	// Also reduces the variation in the address due to small changes in code.
	// Breaks ASLR but so does anything else that tries to make addresses constant for our debugging pleasure
	uptr codeBase = (uptr)(void*)AllocateVirtualMemory / (1 << 28) * (1 << 28);

	// The allocation is ~640mb in size, slighly under 3*2^28.
	// We'll hope that the code generated for the PCSX2 executable stays under 512mb (which is likely)
	// On x86-64, code can reach 8*2^28 from its address [-6*2^28, 4*2^28] is the region that allows for code in the 640mb allocation to reach 512mb of code that either starts at codeBase or 256mb before it.
	// We start high and count down because on macOS code starts at the beginning of useable address space, so starting as far ahead as possible reduces address variations due to code size.  Not sure about other platforms.  Obviously this only actually affects what shows up in a debugger and won't affect performance or correctness of anything.
	for (int offset = 4; offset >= -6; offset--)
	{
		uptr base = codeBase + (offset << 28) + offset_from_base;
		if ((sptr)base < 0 || (sptr)(base + size - 1) < 0)
		{
			// VTLB will throw a fit if we try to put EE main memory here
			continue;
		}

		if (u8* ret = TryAllocateVirtualMemory(name, file_handle, base, size))
			return ret;

		DevCon.Warning("%s: host memory @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " is unavailable; attempting to map elsewhere...", name,
			base, base + size);
	}

	return nullptr;
}

bool SysMemory::AllocateMemoryMap()
{
	s_data_memory_file_handle = HostSys::CreateSharedMemory(HostSys::GetFileMappingName("pcsx2").c_str(), HostMemoryMap::MainSize);
	if (!s_data_memory_file_handle)
	{
		Host::ReportErrorAsync("Error", "Failed to create shared memory file.");
		ReleaseMemoryMap();
		return false;
	}

	if ((s_data_memory = AllocateVirtualMemory("Data Memory", s_data_memory_file_handle, HostMemoryMap::MainSize, 0)) == nullptr)
	{
		Host::ReportErrorAsync("Error", "Failed to map data memory at an acceptable location.");
		ReleaseMemoryMap();
		return false;
	}

	if ((s_code_memory = AllocateVirtualMemory("Code Memory", nullptr, HostMemoryMap::CodeSize, HostMemoryMap::MainSize)) == nullptr)
	{
		Host::ReportErrorAsync("Error", "Failed to allocate code memory at an acceptable location.");
		ReleaseMemoryMap();
		return false;
	}

	HostMemoryMap::EEmem = (uptr)(s_data_memory + HostMemoryMap::EEmemOffset);
	HostMemoryMap::IOPmem = (uptr)(s_data_memory + HostMemoryMap::IOPmemOffset);
	HostMemoryMap::VUmem = (uptr)(s_data_memory + HostMemoryMap::VUmemSize);

	DumpMemoryMap();
	return true;
}

void SysMemory::DumpMemoryMap()
{
#define DUMP_REGION(name, base, offset, size) \
	DevCon.WriteLn(Color_Gray, "%-32s @ 0x%016" PRIXPTR " -> 0x%016" PRIXPTR " %s", name, \
		(uptr)(base + offset), (uptr)(base + offset + size), fmt::format("[{}mb]", size / _1mb).c_str());

	DUMP_REGION("EE Main Memory", s_data_memory, HostMemoryMap::EEmemOffset, HostMemoryMap::EEmemSize);
	DUMP_REGION("IOP Main Memory", s_data_memory, HostMemoryMap::IOPmemOffset, HostMemoryMap::IOPmemSize);
	DUMP_REGION("VU0/1 On-Chip Memory", s_data_memory, HostMemoryMap::VUmemOffset, HostMemoryMap::VUmemSize);
	DUMP_REGION("VTLB Virtual Map", s_data_memory, HostMemoryMap::VTLBAddressMapOffset, HostMemoryMap::VTLBVirtualMapSize);
	DUMP_REGION("VTLB Address Map", s_data_memory, HostMemoryMap::VTLBAddressMapSize, HostMemoryMap::VTLBAddressMapSize);

	DUMP_REGION("R5900 Recompiler Cache", s_code_memory, HostMemoryMap::EErecOffset, HostMemoryMap::EErecSize);
	DUMP_REGION("R3000A Recompiler Cache", s_code_memory, HostMemoryMap::IOPrecOffset, HostMemoryMap::IOPrecSize);
	DUMP_REGION("Micro VU0 Recompiler Cache", s_code_memory, HostMemoryMap::mVU0recOffset, HostMemoryMap::mVU0recSize);
	DUMP_REGION("Micro VU0 Recompiler Cache", s_code_memory, HostMemoryMap::mVU1recOffset, HostMemoryMap::mVU1recSize);
	DUMP_REGION("VIF0 Unpack Recompiler Cache", s_code_memory, HostMemoryMap::VIF0recOffset, HostMemoryMap::VIF0recSize);
	DUMP_REGION("VIF1 Unpack Recompiler Cache", s_code_memory, HostMemoryMap::VIF1recOffset, HostMemoryMap::VIF1recSize);
	DUMP_REGION("VIF Unpack Recompiler Cache", s_code_memory, HostMemoryMap::VIFUnpackRecOffset, HostMemoryMap::VIFUnpackRecSize);
	DUMP_REGION("GS Software Renderer", s_code_memory, HostMemoryMap::SWrecOffset, HostMemoryMap::SWrecSize);


#undef DUMP_REGION
}

void SysMemory::ReleaseMemoryMap()
{
	if (s_code_memory)
	{
		HostSys::Munmap(s_code_memory, HostMemoryMap::CodeSize);
		s_code_memory = nullptr;
	}

	if (s_data_memory)
	{
		HostSys::UnmapSharedMemory(s_data_memory, HostMemoryMap::MainSize);
		s_data_memory = nullptr;
	}

	if (s_data_memory_file_handle)
	{
		HostSys::DestroySharedMemory(s_data_memory_file_handle);
		s_data_memory_file_handle = nullptr;
	}
}

bool SysMemory::Allocate()
{
	DevCon.WriteLn(Color_StrongBlue, "Allocating host memory for virtual systems...");

	ConsoleIndentScope indent(1);

	if (!AllocateMemoryMap())
		return false;

	memAllocate();
	iopMemAlloc();
	vuMemAllocate();

	if (!vtlb_Core_Alloc())
		return false;

	return true;
}

void SysMemory::Reset()
{
	DevCon.WriteLn(Color_StrongBlue, "Resetting host memory for virtual systems...");
	ConsoleIndentScope indent(1);

	memReset();
	iopMemReset();
	vuMemReset();

	// Note: newVif is reset as part of other VIF structures.
	// Software is reset on the GS thread.
}

void SysMemory::Release()
{
	Console.WriteLn(Color_Blue, "Releasing host memory for virtual systems...");
	ConsoleIndentScope indent(1);

	vtlb_Core_Free(); // Just to be sure... (calling order could result in it getting missed during Decommit).

	releaseNewVif(0);
	releaseNewVif(1);

	vuMemRelease();
	iopMemRelease();
	memRelease();

	ReleaseMemoryMap();
}

u8* SysMemory::GetDataPtr(size_t offset)
{
	pxAssert(offset <= HostMemoryMap::MainSize);
	return s_data_memory + offset;
}

u8* SysMemory::GetCodePtr(size_t offset)
{
	pxAssert(offset <= HostMemoryMap::CodeSize);
	return s_code_memory + offset;
}

void* SysMemory::GetDataFileHandle()
{
	return s_data_memory_file_handle;
}

// --------------------------------------------------------------------------------------
//  SysCpuProviderPack  (implementations)
// --------------------------------------------------------------------------------------
SysCpuProviderPack::SysCpuProviderPack()
{
	recCpu.Reserve();
	psxRec.Reserve();

	CpuMicroVU0.Reserve();
	CpuMicroVU1.Reserve();

	VifUnpackSSE_Init();
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
	if (GSDumpReplayer::IsReplayingDump())
	{
		Cpu = &GSDumpReplayerCpu;
		psxCpu = &psxInt;
		CpuVU0 = &CpuIntVU0;
		CpuVU1 = &CpuIntVU1;
		return;
	}

	Cpu = CHECK_EEREC ? &recCpu : &intCpu;
	psxCpu = CHECK_IOPREC ? &psxRec : &psxInt;

	CpuVU0 = &CpuIntVU0;
	CpuVU1 = &CpuIntVU1;

	if (EmuConfig.Cpu.Recompiler.EnableVU0)
		CpuVU0 = &CpuMicroVU0;

	if (EmuConfig.Cpu.Recompiler.EnableVU1)
		CpuVU1 = &CpuMicroVU1;
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
