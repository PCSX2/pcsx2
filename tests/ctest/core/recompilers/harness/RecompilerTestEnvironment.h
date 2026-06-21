// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"

namespace recompiler_tests {

// Stand-up logic for a headless PCSX2 core suitable for in-process
// recompiler work — no Qt, no VMManager, no BIOS. Used by both the gtest
// recompiler suite (via RecompilerTestGtestEnvironment) and the
// pcsx2-vurunner binary, which is gtest-free.
//
// Lifecycle (Initialize):
//   1. FPU default rounding mode.
//   2. cpuinfo_initialize().
//   3. SysMemory::Allocate()    — host VM memory (EE/IOP/VU RAM + rec buffers).
//   4. psxRec.Reserve()          — BASEBLOCK tables + dispatcher.
//   5. psxInt.Reserve()          — no-op, for API symmetry.
//   6. SysMemory::Reset()        — zero + remap mirrors.
//   7. psxRec.Reset()            — compile dispatcher, clear LUT.
//   8. psxInt.Reset()            — no-op.
//   9. Install a parking-lot (infinite NOP loop) at kParkingPc so any test
//      program's `jr ra` with ra=kParkingPc settles in predictable code.
//
// Shutdown reverses 4-8 and releases SysMemory.
class RecompilerTestEnvironment
{
public:
	// Guest addresses reserved for the harness.
	//   [kProgramPc, kProgramPc + 4KB)   test program emit region
	//   [kScratchPc, kScratchPc + 4KB)   scratch data memory for load/store tests
	//   [kParkingPc, kParkingPc + 8)     `j self; nop` — safe `ra` target
	static constexpr u32 kProgramPc = 0x00010000;
	static constexpr u32 kScratchAddr = 0x00020000;
	static constexpr u32 kParkingPc   = 0x001F0000;

	// Idempotent: returns true if already initialized. Returns false if
	// SysMemory::Allocate or other one-shot setup failed; the caller must
	// not invoke any harness/replay primitives in that case.
	static bool Initialize();
	static void Shutdown();

	// Returns true once Initialize() has completed successfully.
	static bool IsReady();

	// Invalidate microVU's per-VU block cache via mVUreset so a test's JIT
	// compile cannot inherit a cached block from a prior test that happened
	// to land at the same start_pc with a matching microRegInfo. Call from
	// the VU harness's SeedEntryState() (i.e. once per Run()).
	static void ResetVuBlockCache(int vu_index);
};

} // namespace recompiler_tests
