// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Config.h"
#include "vtlb.h"

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
	//////////////////////////////////////////////////////////////////////////
	// Main
	//////////////////////////////////////////////////////////////////////////

	// PS2 main memory, SPR, and ROMs (approximately 138.5MB, but we round up to 139MB for simplicity).
	static constexpr u32 EEmemOffset = 0x00000000;
	static constexpr u32 EEmemSize = 0x8B00000;

	// IOP main memory (2MB + 64K + 256b, rounded up to 3MB for simplicity).
	static constexpr u32 IOPmemOffset = EEmemOffset + EEmemSize;
	static constexpr u32 IOPmemSize = 0x300000;

	// VU0 and VU1 memory (40KB, rounded up to 1MB for simplicity).
	static constexpr u32 VUmemOffset = IOPmemOffset + IOPmemSize;
	static constexpr u32 VUmemSize = 0x100000;

	// VTLB virtual map ((4GB / 4096) * sizeof(ptr))
	static constexpr u32 VTLBVirtualMapOffset = VUmemOffset + VUmemSize;
	static constexpr u32 VTLBVirtualMapSize = (0x100000000ULL / 4096) * sizeof(void*);

	// VTLB address map ((4GB / 4096) * sizeof(u32))
	static constexpr u32 VTLBAddressMapOffset = VTLBVirtualMapOffset + VTLBVirtualMapSize;
	static constexpr u32 VTLBAddressMapSize = (0x100000000ULL / 4096) * sizeof(u32);

	// Overall size.
	static constexpr u32 MainSize = VTLBAddressMapOffset + VTLBAddressMapSize;

	//////////////////////////////////////////////////////////////////////////
	// Code
	//////////////////////////////////////////////////////////////////////////

	// EE recompiler code cache area (64mb)
	static constexpr u32 EErecOffset = 0x00000000;
	static constexpr u32 EErecSize = 0x4000000;

	// IOP recompiler code cache area (32mb)
	static constexpr u32 IOPrecOffset = EErecOffset + EErecSize;
	static constexpr u32 IOPrecSize = 0x2000000;

	// newVif0 recompiler code cache area (8mb)
	static constexpr u32 VIF0recOffset = IOPrecOffset + IOPrecSize;
	static constexpr u32 VIF0recSize = 0x800000;

	// newVif1 recompiler code cache area (8mb)
	static constexpr u32 VIF1recOffset = VIF0recOffset + VIF0recSize;
	static constexpr u32 VIF1recSize = 0x800000;

	// microVU1 recompiler code cache area (64mb)
	static constexpr u32 mVU0recOffset = VIF1recOffset + VIF1recSize;
	static constexpr u32 mVU0recSize = 0x4000000;

	// microVU0 recompiler code cache area (64mb)
	static constexpr u32 mVU1recOffset = mVU0recOffset + mVU0recSize;
	static constexpr u32 mVU1recSize = 0x4000000;

	// SSE-optimized VIF unpack functions (1mb)
	static constexpr u32 VIFUnpackRecOffset = mVU1recOffset + mVU1recSize;
	static constexpr u32 VIFUnpackRecSize = 0x100000;

	// Software Renderer JIT buffer (64mb)
	static constexpr u32 SWrecOffset = VIFUnpackRecOffset + VIFUnpackRecSize;
	static constexpr u32 SWrecSize = 0x04000000;

	// Overall size.
	static constexpr u32 CodeSize = SWrecOffset + SWrecSize; // 305 mb
} // namespace HostMemoryMap

// --------------------------------------------------------------------------------------
// HostMemory
// --------------------------------------------------------------------------------------
// This class provides the main memory for the virtual machines.

namespace SysMemory
{
	bool Allocate();
	void Reset();
	void Release();

	/// Returns data memory (Main in Memory Map).
	u8* GetDataPtr(size_t offset);

	/// Returns memory used for the recompilers.
	u8* GetCodePtr(size_t offset);

	/// Returns the file mapping which backs the data memory.
	void* GetDataFileHandle();

	// clang-format off

	//////////////////////////////////////////////////////////////////////////
	// Data Memory Accessors
	//////////////////////////////////////////////////////////////////////////
	__fi static u8* GetEEMem() { return GetDataPtr(HostMemoryMap::EEmemOffset); }
	__fi static u8* GetEEMemEnd() { return GetDataPtr(HostMemoryMap::EEmemOffset + HostMemoryMap::EEmemSize); }
	__fi static u8* GetIOPMem() { return GetDataPtr(HostMemoryMap::IOPmemOffset); }
	__fi static u8* GetIOPMemEnd() { return GetDataPtr(HostMemoryMap::IOPmemOffset + HostMemoryMap::IOPmemSize); }
	__fi static u8* GetVUMem() { return GetDataPtr(HostMemoryMap::VUmemOffset); }
	__fi static u8* GetVUMemEnd() { return GetDataPtr(HostMemoryMap::VUmemOffset + HostMemoryMap::VUmemSize); }
	__fi static u8* GetVTLBVirtualMap() { return GetDataPtr(HostMemoryMap::VTLBVirtualMapOffset); }
	__fi static u8* GetVTLBVirtualMapEnd() { return GetDataPtr(HostMemoryMap::VTLBVirtualMapOffset + HostMemoryMap::VTLBVirtualMapSize); }
	__fi static u8* GetVTLBAddressMap() { return GetDataPtr(HostMemoryMap::VTLBAddressMapOffset); }
	__fi static u8* GetVTLBAddressMapEnd() { return GetDataPtr(HostMemoryMap::VTLBAddressMapOffset + HostMemoryMap::VTLBAddressMapSize); }

	//////////////////////////////////////////////////////////////////////////
	// Code Memory Accessors
	//////////////////////////////////////////////////////////////////////////
	__fi static u8* GetEERec() { return GetCodePtr(HostMemoryMap::EErecOffset); }
	__fi static u8* GetEERecEnd() { return GetCodePtr(HostMemoryMap::EErecOffset + HostMemoryMap::EErecSize); }
	__fi static u8* GetIOPRec() { return GetCodePtr(HostMemoryMap::IOPrecOffset); }
	__fi static u8* GetIOPRecEnd() { return GetCodePtr(HostMemoryMap::IOPrecOffset + HostMemoryMap::IOPrecSize); }
	__fi static u8* GetVU0Rec() { return GetCodePtr(HostMemoryMap::mVU0recOffset); }
	__fi static u8* GetVU0RecEnd() { return GetCodePtr(HostMemoryMap::mVU0recOffset + HostMemoryMap::mVU0recSize); }
	__fi static u8* GetVU1Rec() { return GetCodePtr(HostMemoryMap::mVU1recOffset); }
	__fi static u8* GetVU1RecEnd() { return GetCodePtr(HostMemoryMap::mVU1recOffset + HostMemoryMap::mVU1recSize); }
	__fi static u8* GetVIFUnpackRec() { return GetCodePtr(HostMemoryMap::VIFUnpackRecOffset); }
	__fi static u8* GetVIFUnpackRecEnd() { return GetCodePtr(HostMemoryMap::VIFUnpackRecOffset + HostMemoryMap::VIFUnpackRecSize); }
	__fi static u8* GetSWRec() { return GetCodePtr(HostMemoryMap::SWrecOffset); }
	__fi static u8* GetSWRecEnd() { return GetCodePtr(HostMemoryMap::SWrecOffset + HostMemoryMap::SWrecSize); }

	// clang-format on
} // namespace SysMemory


// --------------------------------------------------------------------------------------
//  SysCpuProviderPack
// --------------------------------------------------------------------------------------
class SysCpuProviderPack
{
public:
	SysCpuProviderPack();
	~SysCpuProviderPack();

	void ApplyConfig() const;
};

// GetCpuProviders - this function is not implemented by PCSX2 core -- it must be
// implemented by the provisioning interface.
extern SysCpuProviderPack& GetCpuProviders();

extern void SysLogMachineCaps(); // Detects cpu type and fills cpuInfo structs.
extern void SysClearExecutionCache(); // clears recompiled execution caches!

extern void SetCPUState(SSE_MXCSR sseMXCSR, SSE_MXCSR sseVU0MXCSR, SSE_MXCSR sseVU1MXCSR);
extern SSE_MXCSR g_sseVU0MXCSR, g_sseVU1MXCSR, g_sseMXCSR;
