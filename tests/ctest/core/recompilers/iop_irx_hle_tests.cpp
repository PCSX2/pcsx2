// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// IRX-import HLE backdoor for the IOP recompiler (DT-01).
//
// The IOP rec's psxRecompileCodeConst1 short-circuits an `ADDIU $0,$0,<idx>`
// (opcode top half 0x2400) — the v1.0 IRX module-import trampoline marker —
// into a direct call to the registered HLE handler when:
//   - irxImportTableAddr(psxpc-4) finds a 0x41e00000 marker nearby,
//   - the 8-byte libname at marker+0xc resolves a registered handler, and
//   - the handler returns non-zero ("I handled it; psxRegs.pc is now the new
//     target"). The emitted cbnz must re-enter the dispatcher so the block at
//     the HLE-set pc runs next — NOT unwind the whole recompiled stint.
//
// This is the same shared host hook x86 carries (pcsx2/x86/iR3000A.cpp:670-722).
// It was absent from the tokyo-merged arm64 IOP rec (0 refs to irxImport/0x2400
// under pcsx2/arm64/), so HostFS (ioman/iomanx) redirection and IOP module
// symbol recovery were silently dead under the JIT. Restored per the Discord
// triage DT-01 (annotations/_discord-triage). Re-dispatch (not exit) matches the
// earlier iopdiff finding on Katamari BIOS boot (commit 89c7eb3a2) and x86.
//
// Uses sysmem::Kprintf_HLE (index 14): the common-path handler that sets
// pc=ra and returns 1 without needing HostFs state.

#include "harness/JitTestHarness.h"

#include "IopMem.h"
#include "R3000A.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kProgramPc   = RecompilerTestEnvironment::kProgramPc;   // 0x00010000
constexpr u32 kParkingPc   = RecompilerTestEnvironment::kParkingPc;   // 0x001F0000
constexpr u32 kScratchAddr = RecompilerTestEnvironment::kScratchAddr; // 0x00020000
constexpr u32 kFollowOnPc  = 0x00014000;

// irxImportTableAddr scans backward from pc-0x18 looking for 0x41e00000.
// Place the marker 0x20 bytes before the J so the first scan probe hits it.
// Libname at marker+0xc; sysmem::Kprintf_HLE is index 14.
constexpr u32 kMarkerAddr = kProgramPc - 0x20;
} // namespace

TEST(IopIrxHle, KprintfInterceptReDispatchesToRa)
{
	JitTestHarness h;

	h.WriteU32(kMarkerAddr, 0x41e00000u);
	const char kLibname[8] = {'s', 'y', 's', 'm', 'e', 'm', '\0', '\0'};
	h.WriteBytes(kMarkerAddr + 0xc, kLibname, 8);

	// Kprintf_HLE writes a0..a3 to sp..sp+12 and reads a NUL-terminated fmt
	// string at a0. Point both at zero-initialized scratch so the writes land
	// somewhere valid and the fmt loop sees an empty string.
	h.SetGpr(reg::sp, kScratchAddr);
	h.SetGpr(reg::a0, kScratchAddr + 0x40);

	// Re-dispatch target. After the HLE sets pc = ra, the dispatcher should
	// find and execute the block here.
	h.SetRa(kFollowOnPc);

	// Block 1: J somewhere (target irrelevant — the HLE short-circuits) with a
	// delay slot of ADDIU $0,$0,14 → opcode 0x2400_000E, the IRX marker.
	h.LoadProgramAt(kProgramPc, {
		J(kParkingPc),
		ADDIU(reg::zero, reg::zero, 14),
	}, /*append_jr_ra_term=*/false);

	// Block 2: runs only if the JIT re-dispatched after the HLE returned 1.
	// v0 |= 0x1234 is the witness — missing in the buggy (no-hook) path, where
	// block 1's J simply falls through to the parking lot and v0 stays 0.
	h.LoadProgramAt(kFollowOnPc, {
		ORI(reg::v0, reg::v0, 0x1234),
		J(kParkingPc),
		NOP,
	}, /*append_jr_ra_term=*/false);

	h.Run();

	// Run()'s DiffJitVsInterp has already asserted field-level state equality;
	// this locks the witness so the test keeps its meaning if the diff
	// machinery ever grows an escape hatch.
	EXPECT_EQ(h.GetGprJit(reg::v0), 0x1234u);
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x1234u);
}
