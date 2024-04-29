// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "common/Pcsx2Defs.h"

namespace Ps2MemSize
{
	static constexpr u32 MainRam = _32mb;      // 32 MB main memory.
	static constexpr u32 ExtraRam = _1mb * 96; // 32+96 MB devkit memory.
	static constexpr u32 TotalRam = _1mb * 128;// 128 MB total memory.
	static constexpr u32 Rom = _1mb * 4;       // 4 MB main rom
	static constexpr u32 Rom1 = _1mb * 4;      // DVD player
	static constexpr u32 Rom2 = 0x00080000;    // Chinese rom extension
	static constexpr u32 Hardware = _64kb;
	static constexpr u32 Scratch = _16kb;

	static constexpr u32 IopRam = _1mb * 2; // 2MB main ram on the IOP.
	static constexpr u32 IopHardware = _64kb;

	static constexpr u32 GSregs = 0x00002000; // 8k for the GS registers and stuff.

	extern u32 ExposedRam;
} // namespace Ps2MemSize

typedef u8 mem8_t;
typedef u16 mem16_t;
typedef u32 mem32_t;
typedef u64 mem64_t;
typedef u128 mem128_t;

struct EEVM_MemoryAllocMess
{
	u8 Main[Ps2MemSize::MainRam];         // Main memory (hard-wired to 32MB)
	u8 ExtraMemory[Ps2MemSize::ExtraRam]; // Extra memory (32MB up to 128MB => 96MB).
	u8 Scratch[Ps2MemSize::Scratch];      // Scratchpad!
	u8 ROM[Ps2MemSize::Rom];              // Boot rom (4MB)
	u8 ROM1[Ps2MemSize::Rom1];            // DVD player (4MB)
	u8 ROM2[Ps2MemSize::Rom2];            // Chinese extensions

	// Two 1 megabyte (max DMA) buffers for reading and writing to high memory (>32MB).
	// Such accesses are not documented as causing bus errors but as the memory does
	// not exist, reads should continue to return 0 and writes should be discarded.
	// Probably.

	u8 ZeroRead[_1mb];
	u8 ZeroWrite[_1mb];
};

struct IopVM_MemoryAllocMess
{
	u8 Main[Ps2MemSize::IopRam]; // Main memory (hard-wired to 2MB)
	u8 P[_64kb];                 // I really have no idea what this is... --air
	u8 Sif[0x100];               // a few special SIF/SBUS registers (likely not needed)
};


// DevNote: EE and IOP hardware registers are done as a static array instead of a pointer in
// order to allow for simpler macros and reference handles to be defined  (we can safely use
// compile-time references to registers instead of having to use instance variables).

alignas(__pagesize) extern u8 eeHw[Ps2MemSize::Hardware];
alignas(__pagesize) extern u8 iopHw[Ps2MemSize::IopHardware];


extern EEVM_MemoryAllocMess* eeMem;
extern IopVM_MemoryAllocMess* iopMem;
