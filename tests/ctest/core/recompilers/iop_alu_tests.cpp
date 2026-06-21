// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

#include <vector>

using namespace recompiler_tests;
using namespace mips;

// Sanity: JIT and interp agree on addiu semantics including sign-extension
// of the 16-bit immediate. The go-to "does the pipeline work end-to-end?"
// test.
TEST(IopAlu, AddiuSignExtend)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 100);
	h.LoadProgram({
		ADDIU(reg::v0, reg::a0, -7),  // immediate sign-extended to 0xFFFFFFF9
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 93u);
	EXPECT_EQ(h.GetGprJit(reg::v0), 93u);
}

TEST(IopAlu, AddiuWrapPositive)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x7FFFFFFFu);
	h.LoadProgram({ADDIU(reg::v0, reg::a0, 1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x80000000u);
}

TEST(IopAlu, AddiuZero)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 42);
	h.LoadProgram({ADDIU(reg::v0, reg::a0, 0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 42u);
}

TEST(IopAlu, AdduBasic)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 10);
	h.SetGpr(reg::a1, 20);
	h.LoadProgram({ADDU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 30u);
}

TEST(IopAlu, SubuUnderflow)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 5);
	h.SetGpr(reg::a1, 10);
	h.LoadProgram({SUBU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), static_cast<u32>(-5));
}

// SUBU with Rs==Rt is the zero fast-path: the JIT materializes 0 directly
// instead of subtracting a register from itself. Result must still be 0 and
// match interp.
TEST(IopAlu, SubuSelfIsZero)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xDEADBEEFu);
	h.LoadProgram({SUBU(reg::v0, reg::a0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0u);
	EXPECT_EQ(h.GetGprJit(reg::v0), 0u);
}

// Regression: SUB/SUBU with a CONST minuend (Rs const, e.g. from a preceding
// LUI) and the destination aliasing the subtrahend (Rd == Rt). A naive const
// path that materializes the constant into Rd before subtracting clobbers Rt
// with the constant first when Rd and Rt share a host register, so the
// subtract degenerates to cv - cv = 0 instead of cv - rt_old. Route the
// constant through a scratch register. `x = CONST - y` written back over y is
// a common idiom.
TEST(IopAlu, SubConstMinuendDestAliasesSubtrahend)
{
	JitTestHarness h;
	h.SetGpr(reg::s6, 0xFFFFAC45u);          // r22 = subtrahend AND dest (Rd == Rt)
	h.LoadProgram({
		LUI(reg::t5, 0x0FE7),                // r13 = 0x0FE70000 (const minuend)
		SUB(reg::s6, reg::t5, reg::s6),      // r22 = r13 - r22 = 0x0FE753BB (not 0)
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::s6), 0x0FE753BBu);
	EXPECT_EQ(h.GetGprJit(reg::s6), 0x0FE753BBu);
}

TEST(IopAlu, SubuConstMinuendDestAliasesSubtrahend)
{
	JitTestHarness h;
	h.SetGpr(reg::s6, 0xFFFFAC45u);
	h.LoadProgram({
		LUI(reg::t5, 0x0FE7),
		SUBU(reg::s6, reg::t5, reg::s6),
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::s6), 0x0FE753BBu);
	EXPECT_EQ(h.GetGprJit(reg::s6), 0x0FE753BBu);
}

TEST(IopAlu, AndAllBits)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xFFFFFFFFu);
	h.SetGpr(reg::a1, 0xAAAAAAAAu);
	h.LoadProgram({AND(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xAAAAAAAAu);
}

TEST(IopAlu, OrCombine)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x00FF00FFu);
	h.SetGpr(reg::a1, 0xFF00FF00u);
	h.LoadProgram({OR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFFFFFFFFu);
}

TEST(IopAlu, XorSelfIsZero)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x12345678u);
	h.LoadProgram({XOR(reg::v0, reg::a0, reg::a0)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0u);
}

TEST(IopAlu, LuiShifts)
{
	JitTestHarness h;
	h.LoadProgram({LUI(reg::v0, 0x1234)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x12340000u);
}

TEST(IopAlu, SltSigned)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, static_cast<u32>(-5));
	h.SetGpr(reg::a1, 3);
	h.LoadProgram({SLT(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopAlu, SltuUnsignedMaxIsNotLess)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xFFFFFFFFu);
	h.SetGpr(reg::a1, 3u);
	h.LoadProgram({SLTU(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0u);
}

TEST(IopAlu, AndiZeroExtends)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xFFFFFFFFu);
	h.LoadProgram({ANDI(reg::v0, reg::a0, 0x1234)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00001234u);
}

TEST(IopAlu, OriMergesImmediate)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0xFFFF0000u);
	h.LoadProgram({ORI(reg::v0, reg::a0, 0xABCD)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0xFFFFABCDu);
}

TEST(IopAlu, XoriToggleLowBits)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x0000FFFFu);
	h.LoadProgram({XORI(reg::v0, reg::a0, 0xAAAA)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00005555u);
}

TEST(IopAlu, SltiPositiveLess)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 3);
	h.LoadProgram({SLTI(reg::v0, reg::a0, 10)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopAlu, SltiNegativeCompare)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, static_cast<u32>(-1));
	h.LoadProgram({SLTI(reg::v0, reg::a0, 0)});  // -1 < 0 → 1
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopAlu, SltiuSignExtendedImmediate)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0u);
	// imm=-1 sign-extends to 0xFFFFFFFF; 0 <u 0xFFFFFFFF → 1
	h.LoadProgram({SLTIU(reg::v0, reg::a0, -1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
}

TEST(IopAlu, NorSwappedMask)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x0000FFFFu);
	h.SetGpr(reg::a1, 0xFF000000u);
	h.LoadProgram({NOR(reg::v0, reg::a0, reg::a1)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x00FF0000u);
}

TEST(IopAlu, AddiCopiesWithSignedExtension)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 1000);
	h.LoadProgram({ADDI(reg::v0, reg::a0, -200)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 800u);
}

TEST(IopAlu, MoveViaOrFromZero)
{
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x13579BDFu);
	h.LoadProgram({OR(reg::v0, reg::a0, reg::zero)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 0x13579BDFu);
}

TEST(IopAlu, WritesToZeroAreNoOp)
{
	// r0 is hardwired to 0. Trying to write to it should leave it zero.
	JitTestHarness h;
	h.LoadProgram({ADDIU(reg::zero, reg::zero, 1234)});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::zero), 0u);
}

// ── Regression guards: allocator-aware immediate ops ─────────────────────────
// The IOP immediate ALU ops (ADDIU/ANDI/ORI/XORI/SLTI/SLTIU) keep their
// results live in allocated host registers across IOP instruction boundaries,
// so a value produced by one op is observable by a later op without a
// round-trip through guest memory. These tests exercise cross-instruction
// liveness, in-place RMW, and survival of a spill/reload under register
// pressure. JitTestHarness::Run() diffs JIT vs interp across all fields, so any
// lost/stale value surfaces.

TEST(IopAlu, ImmOpResultFeedsChainWithoutFlush)
{
	// Each imm-op result is consumed by the next op with no intervening flush.
	JitTestHarness h;
	h.SetGpr(reg::a0, 0x05);
	h.LoadProgram({
		ORI(reg::t0, reg::a0, 0x0010),    // 0x05 | 0x10 = 0x15
		ANDI(reg::t1, reg::t0, 0x001F),   // 0x15 & 0x1F = 0x15
		XORI(reg::t2, reg::t1, 0x000F),   // 0x15 ^ 0x0F = 0x1A
		ADDIU(reg::t3, reg::t2, 0x06),    // 0x1A + 0x06 = 0x20
		SLTI(reg::v0, reg::t3, 0x21),     // (0x20 < 0x21) → 1
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::t3), 0x20u);
	EXPECT_EQ(h.GetGprInterp(reg::v0), 1u);
	EXPECT_EQ(h.GetGprJit(reg::v0), 1u);
}

TEST(IopAlu, InPlaceImmAccumulateAcrossOps)
{
	// Rs == Rt read-modify-write on the same allocated host reg, repeated.
	JitTestHarness h;
	h.SetGpr(reg::a0, 100);
	h.LoadProgram({
		ADDIU(reg::t0, reg::a0, 0),       // t0 = 100
		ADDIU(reg::t0, reg::t0, 1),       // t0 = 101  (Rs==Rt)
		ADDIU(reg::t0, reg::t0, 1),       // t0 = 102
		ANDI(reg::t0, reg::t0, 0xFFFF),   // t0 = 102  (Rs==Rt)
		ORI(reg::v0, reg::t0, 0),         // v0 = 102  (ori-with-0 move idiom)
	});
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), 102u);
	EXPECT_EQ(h.GetGprJit(reg::v0), 102u);
}

TEST(IopAlu, ImmOpResultSurvivesRegisterPressure)
{
	// An imm-op result (s0) is left live in a host reg, then 14 LWs into other
	// regs push the live set to 15 (pool is 14) — forcing an eviction during
	// which s0 may be the LRU spill victim. The final sum reads s0 back. If the
	// imm op failed to mark its host reg dirty (MODE_WRITE), the spill would
	// write stale memory and the sum would diverge from interp.
	JitTestHarness h;
	constexpr u32 kData = RecompilerTestEnvironment::kScratchAddr;
	h.SetGpr(reg::a0, 0x00000100u);   // s0 seed base
	h.SetGpr(reg::a1, kData);         // LW base

	constexpr u32 kImmDelta = 0x111;
	const u32 s0_val = 0x100u + kImmDelta;

	const u32 ld[14] = {
		reg::t0, reg::t1, reg::t2, reg::t3, reg::t4,
		reg::t5, reg::t6, reg::t7, reg::t8, reg::t9,
		reg::s1, reg::s2, reg::s3, reg::s4,
	};
	u32 expected = s0_val;
	for (u32 i = 0; i < 14; ++i)
	{
		const u32 v = 0x01010101u * (i + 1);
		h.WriteU32(kData + i * 4, v);
		expected += v;
	}

	std::vector<u32> prog;
	prog.push_back(ADDIU(reg::s0, reg::a0, static_cast<s16>(kImmDelta)));  // s0 = imm result
	for (u32 i = 0; i < 14; ++i)
		prog.push_back(LW(ld[i], static_cast<s16>(i * 4), reg::a1));
	// Sum all 14 loads, then add s0 last so s0 is the stale LRU candidate.
	prog.push_back(ADDU(reg::v0, ld[0], reg::zero));
	for (u32 i = 1; i < 14; ++i)
		prog.push_back(ADDU(reg::v0, reg::v0, ld[i]));
	prog.push_back(ADDU(reg::v0, reg::v0, reg::s0));

	h.LoadProgramAt(RecompilerTestEnvironment::kProgramPc,
		prog.data(), prog.size(),
		/*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), expected);
	EXPECT_EQ(h.GetGprJit(reg::v0), expected);
	EXPECT_EQ(h.GetGprInterp(reg::s0), s0_val);
}
