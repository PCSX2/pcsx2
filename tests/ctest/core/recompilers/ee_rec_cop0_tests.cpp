// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// COP0 opcodes on the EE: MFC0/MTC0 register moves, ERET return-from-
// exception, DI/EI enable/disable interrupts. The TLB-manipulation opcodes
// (TLBWI/TLBR/TLBP/TLBWR) are heavy interpreter delegations (COP0 is not
// perf-critical) and are not exercised here.
//
// Note on privileged mode: the test harness leaves Status in its zero-init
// state (KSU=0 kernel). COP0 access from kernel mode doesn't require
// Status.CU[0]; it's implicitly allowed. EnableCop0() is provided for tests
// that explicitly set CU[0] regardless, matching how real game code may
// run (MTC0 from a PS2 app is usually in kernel mode via SYSCALL).

#include "harness/EeRecTestHarness.h"

#include "R5900.h"
#include "Hw.h"
#include "Memory.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
// CP0 register indices (the CP0regs::n union).
constexpr u32 kCp0Count    = 9;
constexpr u32 kCp0Status   = 12;
constexpr u32 kCp0Cause    = 13;
constexpr u32 kCp0Epc      = 14;
constexpr u32 kCp0PRid     = 15;
} // namespace

TEST(EeRecCop0, Mfc0StatusReadsDefaultZero)
{
	// Fresh harness: Status is zero-initialized. MFC0 from Status into v0
	// should yield zero. The Diff path also confirms JIT and interp agree.
	EeRecTestHarness h;
	h.LoadProgram({
		MFC0(reg::v0, kCp0Status),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
}

TEST(EeRecCop0, Mtc0ThenMfc0Roundtrip)
{
	// Write to PRid (a writable-but-unused-by-dispatch register; Status would
	// trigger cpuUpdateOperationMode side effects that would interfere with this test).
	// Then read it back via MFC0.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xDEADBEEFu);
	h.LoadProgram({
		MTC0(reg::a0, kCp0PRid),
		MFC0(reg::v0, kCp0PRid),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, static_cast<s64>(static_cast<s32>(0xDEADBEEFu)));
	EXPECT_EQ(h.GetCp0Interp(kCp0PRid), 0xDEADBEEFu);
}

TEST(EeRecCop0, Mtc0EpcRoundtrip)
{
	// Write an EPC value via MTC0 then read via MFC0. Exercises a register
	// index (14) that some TLB-refill/Eret tests rely on.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x12345678u);
	h.LoadProgram({
		MTC0(reg::a0, kCp0Epc),
		MFC0(reg::v0, kCp0Epc),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, static_cast<s64>(static_cast<s32>(0x12345678u)));
	EXPECT_EQ(h.GetCp0Interp(kCp0Epc), 0x12345678u);
}

TEST(EeRecCop0, EnableCop0HelperSetsCu0)
{
	// EnableCop0 sets Status.CU[0]. Observed via MFC0 after Run().
	EeRecTestHarness h;
	h.EnableCop0();
	h.LoadProgram({
		MFC0(reg::v0, kCp0Status),
	});
	h.Run();
	EXPECT_NE(h.GetGpr64Interp(reg::v0) & (1ull << 28), 0ull);
}

TEST(EeRecCop0, DiClearsStatusEIE)
{
	// DI clears Status.EIE (bit 16). Pre-set EIE then DI clears it.
	// Status bit layout: EIE = bit 16. DI's effect is delayed by
	// one instruction (matches x86), so the inline EIE-clear is
	// emitted *after* the next instruction — follow DI with a NOP so the clear
	// lands in live code (a DI immediately before a block-terminating jump would
	// emit the clear after the branch, same as x86 — not a realistic position).
	EeRecTestHarness h;
	h.SetStatusBits(1u << 16);   // EIE = 1
	h.LoadProgram({
		ee::DI, NOP,
	});
	h.Run();
	EXPECT_EQ(h.GetCp0Interp(kCp0Status) & (1u << 16), 0u);
	EXPECT_EQ(h.GetCp0Jit(kCp0Status)    & (1u << 16), 0u);
}

TEST(EeRecCop0, EiSetsStatusEIE)
{
	// EI sets Status.EIE. Requires COP0 mode — kernel mode is fine from
	// zero-init. Precondition: Status.EDI/EXL/ERL all clear (default).
	EeRecTestHarness h;
	h.LoadProgram({
		ee::EI,
	});
	h.Run();
	EXPECT_NE(h.GetCp0Interp(kCp0Status) & (1u << 16), 0u);
}

TEST(EeRecCop0, Mtc0CountWritesCountRegisterInline)
{
	// MTC0 to Count (rd=9) is inlined (no iFlushCall + Interp::MTC0
	// call). Both JIT and interp write CP0.r[9] = rt[31:0] and lastCOP0Cycle =
	// cycle. CP0[9] is excluded from the auto-diff because _cpuTestTIMR advances
	// Count by a few cycles at block end (nondeterministic). The write itself is
	// deterministic, so assert Count is the written value plus only that small
	// timer drift — a broken store (wrong reg / missing lastCOP0Cycle update)
	// would land far outside this window (near 0, or base + full cycle count).
	constexpr u32 kBase = 0x0BADF00Du;
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, kBase);
	h.LoadProgram({
		MTC0(reg::a0, kCp0Count),
	});
	h.Run();
	EXPECT_GE(h.GetCp0Jit(kCp0Count), kBase);
	EXPECT_LT(h.GetCp0Jit(kCp0Count), kBase + 0x100u);
	EXPECT_GE(h.GetCp0Interp(kCp0Count), kBase);
	EXPECT_LT(h.GetCp0Interp(kCp0Count), kBase + 0x100u);
}

TEST(EeRecCop0, DiInUserModeDoesNotClearEIE)
{
	// Inline DI guard: EIE is cleared only when (EXL|ERL|EDI)
	// set OR KSU == 0 (kernel). In user mode (KSU != 0) with no exception level,
	// DI must leave EIE untouched. Exercises the skip branch of the new inline
	// emitter. CU0 set so the COP0 op is usable in user mode. Status (CP0[12])
	// is auto-diffed, so JIT and interp are cross-checked.
	EeRecTestHarness h;
	h.EnableCop0();                       // CU0 — COP0 usable in user mode
	h.SetStatusBits((1u << 16) | 0x10);   // EIE=1, KSU=user (bit 4)
	h.LoadProgram({
		ee::DI, NOP,   // NOP so DI's delayed inline guard lands in live code
	});
	h.Run();
	EXPECT_NE(h.GetCp0Interp(kCp0Status) & (1u << 16), 0u); // EIE preserved
	EXPECT_NE(h.GetCp0Jit(kCp0Status)    & (1u << 16), 0u);
}

TEST(EeRecCop0, MfC0CauseReadsBackSetCause)
{
	// Harness direct-set Cause via SetCp0, program MFC0 Cause, observe
	// GPR has the value. Exercises the MFC0 path for a register the
	// exception dispatcher also writes.
	EeRecTestHarness h;
	h.SetCp0(kCp0Cause, 0x12345678u);
	h.LoadProgram({
		MFC0(reg::v0, kCp0Cause),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, static_cast<s64>(static_cast<s32>(0x12345678u)));
}

TEST(EeRecCop0, Mtc0BreakpointRegisterIsLogOnly)
{
	// MTC0 to rd=24 (Debug breakpoint register) must be a no-op
	// in the emulation model. Interp logs only; the JIT special-cases it and
	// must not fall through to the default-branch store.
	constexpr u32 kCp0Brk = 24;
	EeRecTestHarness h;
	h.SetCp0(kCp0Brk, 0xCAFEBABEu);          // pre-set sentinel
	h.SetGpr64(reg::a0, 0xDEADBEEFu);
	h.LoadProgram({
		MTC0(reg::a0, kCp0Brk),
	});
	h.Run();
	// Both JIT and interp must leave the sentinel untouched.
	EXPECT_EQ(h.GetCp0Interp(kCp0Brk), 0xCAFEBABEu);
	EXPECT_EQ(h.GetCp0Jit(kCp0Brk),    0xCAFEBABEu);
}

TEST(EeRecCop0, Mfc0StatusMasksReservedBits)
{
	// MFC0 from Status (rd=12) must mask CP0.r[12] with 0xf0c79c1f
	// before sign-extending to the GPR (per the interpreter's MFC0). Use a value
	// where ONLY reserved bits (outside the mask) are set, so the live
	// Status fields stay zero and don't perturb dispatcher interrupt logic.
	// Reserved bits: 5, 6, 7, 8, 9, 13, 14, 19, 20, 21, 24, 25, 26, 27.
	const u32 reserved_only = 0x0F3863E0u; // bits above ∧ ~0xf0c79c1f
	EeRecTestHarness h;
	h.SetCp0(kCp0Status, reserved_only);
	h.LoadProgram({
		MFC0(reg::v0, kCp0Status),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull); // masked → 0
}

// ---- COP0 branch on DMAC condition (BC0F/T/FL/TL) --------------------------
// Native emitter replaces the interpreter fallback. The condition is
//   (((DMAC_STAT | ~DMAC_PCR) & 0x3ff) == 0x3ff).
// PCR=0 -> ~PCR low bits all set -> condition always TRUE regardless of STAT.
// PCR=0x3ff with STAT=0 -> condition FALSE. Both JIT and interp read the same
// eeHw, so Run() diffs the taken/not-taken control flow; v0 marks the path.

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;
constexpr s16 kTakenOffset = 5;

// 0x00: <branch>; 0x04: NOP delay; 0x08: v0=1 (not taken); J park;
// 0x18: v0=2 (taken); J park.
inline void LoadBc0Layout(EeRecTestHarness& h, u32 branch_instr)
{
	h.LoadProgramNoTerm({
		branch_instr, NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
}

void SetDmac(u32 pcr, u32 stat)
{
	psHu32(DMAC_PCR) = pcr;
	psHu32(DMAC_STAT) = stat;
}
} // namespace

TEST(EeRecCop0, Bc0tTakenWhenConditionTrue)
{
	EeRecTestHarness h;
	SetDmac(/*pcr=*/0, /*stat=*/0); // condition TRUE
	LoadBc0Layout(h, BC0T(kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull); // taken
}

TEST(EeRecCop0, Bc0tNotTakenWhenConditionFalse)
{
	EeRecTestHarness h;
	SetDmac(/*pcr=*/0x3ff, /*stat=*/0); // condition FALSE
	LoadBc0Layout(h, BC0T(kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull); // fall through
}

TEST(EeRecCop0, Bc0fTakenWhenConditionFalse)
{
	EeRecTestHarness h;
	SetDmac(/*pcr=*/0x3ff, /*stat=*/0); // condition FALSE → BC0F branches
	LoadBc0Layout(h, BC0F(kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 2ull); // taken
}

TEST(EeRecCop0, Bc0fNotTakenWhenConditionTrue)
{
	EeRecTestHarness h;
	SetDmac(/*pcr=*/0, /*stat=*/0); // condition TRUE → BC0F falls through
	LoadBc0Layout(h, BC0F(kTakenOffset));
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull); // fall through
}

TEST(EeRecCop0, Bc0tlSquashesDelaySlotWhenNotTaken)
{
	// Likely variant: when not taken, the delay slot must NOT execute.
	EeRecTestHarness h;
	SetDmac(/*pcr=*/0x3ff, /*stat=*/0); // condition FALSE → BC0TL not taken
	h.LoadProgramNoTerm({
		BC0TL(kTakenOffset),
		ADDIU(reg::a0, reg::zero, 99),       // squashed delay slot
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.SetGpr64(reg::a0, 7);
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0), 1ull); // not taken
	EXPECT_EQ(h.GetGpr64Interp(reg::a0), 7ull); // delay slot squashed
}
