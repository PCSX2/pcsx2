// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Trap instructions on the EE: TEQ/TGE/TLT/TNE/TGEU/TLTU family +
// TEQI/TGEI/TLTI/TNEI/TGEIU/TLTIU immediate variants + SYSCALL + BREAK.
//
// Semantics per MIPS-III: when the condition is true, a trap exception
// fires and control transfers to the common exception vector (0x80000180
// with BEV=0). PC is saved in EPC, Cause is updated.
//
// For "trap taken" tests the BEV=0 exception vectors are pre-installed with
// `jr ra; nop` stubs by RecompilerTestEnvironment::SetUp() — the handler
// returns to the harness's parking lot via ra=kParkingPc, giving a
// well-defined post-state without per-test bootstrap.

#include "harness/EeRecTestHarness.h"

#include "R5900.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kCauseTrapExCode = 0x34;  // ExcCode=0x0D (trap), shifted << 2
} // namespace

// ---------------- Register-register trap: not taken ----------------

TEST(EeRecTraps, TeqNotTakenFallsThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 20);
	h.LoadProgram({
		ee::TEQ(reg::a0, reg::a1),                // 10 != 20 → not taken
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TneNotTakenFallsThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 10);
	h.SetGpr64(reg::a1, 10);
	h.LoadProgram({
		ee::TNE(reg::a0, reg::a1),                // 10 == 10 → not taken
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TltNotTakenForGreaterOrEqual)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 20);
	h.SetGpr64(reg::a1, 10);                       // 20 < 10 → false → no trap
	h.LoadProgram({
		ee::TLT(reg::a0, reg::a1),
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TgeNotTakenForLess)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 5);
	h.SetGpr64(reg::a1, 10);                       // 5 >= 10 → false → no trap
	h.LoadProgram({
		ee::TGE(reg::a0, reg::a1),
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

// ---------------- Immediate trap: not taken ----------------

TEST(EeRecTraps, TeqiNotTakenFallsThrough)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.LoadProgram({
		ee::TEQI(reg::a0, 99),                     // 42 != 99 → no trap
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

TEST(EeRecTraps, TltiNotTakenForGreaterOrEqual)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 100);
	h.LoadProgram({
		ee::TLTI(reg::a0, 50),                     // 100 < 50 → false → no trap
		ADDIU(reg::v0, reg::zero, 7),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

// ---------------- Register-register trap: taken ----------------

TEST(EeRecTraps, TeqTakenRaisesException)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 42);
	h.SetGpr64(reg::a1, 42);
	h.LoadProgram({
		ee::TEQ(reg::a0, reg::a1),                 // 42 == 42 → trap fires
		ADDIU(reg::v0, reg::zero, 99),             // should NOT execute
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);                  // fall-through did not run
	// Cause low byte has ExcCode<<2 = 0x34 (trap).
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, kCauseTrapExCode);
}

// ---------------- SYSCALL / BREAK (always taken) ----------------

TEST(EeRecTraps, SyscallAlwaysTaken)
{
	EeRecTestHarness h;
	h.LoadProgram({
		SYSCALL_(),
		ADDIU(reg::v0, reg::zero, 99),             // should NOT execute
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	// SYSCALL ExcCode=8, Cause.ExcCode<<2 = 0x20.
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, 0x20u);
}

TEST(EeRecTraps, BreakAlwaysTaken)
{
	EeRecTestHarness h;
	h.LoadProgram({
		BREAK,
		ADDIU(reg::v0, reg::zero, 99),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	// BREAK ExcCode=9, Cause.ExcCode<<2 = 0x24.
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, 0x24u);
}

// ---------------- Register-register trap: full taken coverage ----------------

namespace {
template <typename Encode>
void RunRegRegTrapTaken(Encode enc, u64 a0, u64 a1)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, a0);
	h.SetGpr64(reg::a1, a1);
	h.LoadProgram({
		enc(reg::a0, reg::a1),
		ADDIU(reg::v0, reg::zero, 99),  // should NOT execute
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, kCauseTrapExCode);
}

template <typename Encode>
void RunImmTrapTaken(Encode enc, u64 a0, s16 imm)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, a0);
	h.LoadProgram({
		enc(reg::a0, imm),
		ADDIU(reg::v0, reg::zero, 99),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0ull);
	EXPECT_EQ(h.GetCp0Interp(13) & 0xFFu, kCauseTrapExCode);
}
} // namespace

TEST(EeRecTraps, TgeTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TGE, 100, 50);  // 100 >= 50 (signed) → trap
}

TEST(EeRecTraps, TgeuTakenRaisesException)
{
	// TGEU compares unsigned; -1 (= 0xFF..) >= 1 unsigned → trap.
	RunRegRegTrapTaken(ee::TGEU, static_cast<u64>(-1), 1);
}

TEST(EeRecTraps, TltTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TLT, static_cast<u64>(-5), 5);  // -5 < 5 (signed) → trap
}

TEST(EeRecTraps, TltuTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TLTU, 1, static_cast<u64>(-1));  // 1 < ~0 unsigned → trap
}

TEST(EeRecTraps, TneTakenRaisesException)
{
	RunRegRegTrapTaken(ee::TNE, 7, 9);
}

// ---------------- Immediate trap: full taken coverage ----------------

TEST(EeRecTraps, TgeiTakenRaisesException)
{
	RunImmTrapTaken(ee::TGEI, 100, 50);  // 100 >= 50 → trap
}

TEST(EeRecTraps, TgeiuTakenRaisesException)
{
	// _Imm_ is sign-extended to 64-bit before the unsigned compare per MIPS-III,
	// so an imm of -1 produces 0xFFFF_FFFF_FFFF_FFFF; rs=0xFF..F is >= that → trap.
	RunImmTrapTaken(ee::TGEIU, static_cast<u64>(-1), -1);
}

TEST(EeRecTraps, TltiTakenRaisesException)
{
	RunImmTrapTaken(ee::TLTI, static_cast<u64>(-5), 5);  // -5 < 5 → trap
}

TEST(EeRecTraps, TltiuTakenRaisesException)
{
	// 1 < (sign-extended) -1 = 0xFF..F unsigned → trap.
	RunImmTrapTaken(ee::TLTIU, 1, -1);
}

TEST(EeRecTraps, TeqiTakenRaisesException)
{
	RunImmTrapTaken(ee::TEQI, static_cast<u64>(-1), -1);
}

TEST(EeRecTraps, TneiTakenRaisesException)
{
	RunImmTrapTaken(ee::TNEI, 5, 9);
}

// ---------------- MFSA / MTSA / MTSAB / MTSAH ----------------
//
// SA is u32 in cpuRegs.sa; MFSA zero-extends to 64. Test JIT vs interp via
// the snapshot machinery (DiffEe asserts both paths match) — sa is included
// in the EeSnapshot so any divergence fails the test.

TEST(EeRecTraps, MtsaCopiesFullRegister)
{
	// PS2 spec (R5900OpcodeImpl.cpp:1265): cpuRegs.sa = (u32)rs[lo]. No mask.
	// Only MTSAB/MTSAH narrow the value (low 4 / low 3 bits respectively).
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0xFFFFFFF7u);
	h.LoadProgram({
		ee::MTSA(reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xFFFFFFF7u);
}

TEST(EeRecTraps, MtsabXorsLow4BitsWithImmediate)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x5u);
	h.LoadProgram({
		ee::MTSAB(reg::a0, 0x3),  // (5 & 0xF) ^ (3 & 0xF) = 6
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0x6u);
}

TEST(EeRecTraps, MtsahShiftsLeftByOne)
{
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x5u);
	h.LoadProgram({
		ee::MTSAH(reg::a0, 0x3),  // ((5 & 7) ^ (3 & 7)) << 1 = 6 << 1 = 0xC
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xCu);
}

TEST(EeRecTraps, MfsaReadsSaZeroExtended)
{
	EeRecTestHarness h;
	h.LoadProgram({
		ee::MTSA(reg::a0),
		ee::MFSA(reg::v0),
	});
	h.SetGpr64(reg::a0, 0xCu);
	h.Run();
	// rd should hold the value of sa, zero-extended to 64-bit.
	h.ExpectGpr64(reg::v0, 0xCull);
}

TEST(EeRecTraps, MfsaToZeroIsNoOp)
{
	// rd=r0 should not modify state; the JIT path early-returns.
	EeRecTestHarness h;
	h.SetGpr64(reg::a0, 0x5u);
	h.LoadProgram({
		ee::MTSA(reg::a0),
		ee::MFSA(reg::zero),
	});
	h.Run();
	h.ExpectGpr64(reg::zero, 0ull);
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0x5u);
}

// ---------------- SYNC (no-op) ----------------

TEST(EeRecTraps, SyncIsNoOp)
{
	EeRecTestHarness h;
	h.LoadProgram({
		ADDIU(reg::v0, reg::zero, 7),
		ee::SYNC,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 7ull);
}

// ---------------- Const-prop fold paths for MTSA family ----------------

TEST(EeRecTraps, MtsaConstFoldsAtCompile)
{
	// Exercise the GPR_IS_CONST1 fast path: rs is set via a LUI+ORI sequence
	// that the const-prop tracker captures. Same end-state as the runtime
	// path (full 32-bit copy, no mask); a divergence here would point at a
	// const-fold bug.
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0),
		ORI(reg::a0, reg::a0, 0xF7u),    // a0 = 0xF7 (const-tracked)
		ee::MTSA(reg::a0),
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xF7u);
}

TEST(EeRecTraps, MtsabConstFoldsAtCompile)
{
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0),
		ORI(reg::a0, reg::a0, 0xAu),
		ee::MTSAB(reg::a0, 0x5),  // (0xA & 0xF) ^ (0x5 & 0xF) = 0xF
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xFu);
}

TEST(EeRecTraps, MtsahConstFoldsAtCompile)
{
	EeRecTestHarness h;
	h.LoadProgram({
		LUI(reg::a0, 0),
		ORI(reg::a0, reg::a0, 0x6u),
		ee::MTSAH(reg::a0, 0x3),  // ((6 & 7) ^ (3 & 7)) << 1 = 5 << 1 = 0xA
	});
	h.Run();
	EXPECT_EQ(h.InterpSnapshot().regs.sa, 0xAu);
}
