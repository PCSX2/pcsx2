// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Pressure tests for the IOP recompiler's host-register allocator. The
// allocatable pool for normal PSX GPR work holds 14 host registers; once
// the live set exceeds that, the allocator must evict a slot — choose a
// victim that isn't currently needed, flush it back to psxRegs.GPR.r[], and
// reuse it for the new value.
//
// LUI+ORI sequences don't work as pressure: PSX const-prop folds them and the
// allocator never materializes a host reg. Guest loads (LW) do materialize —
// rpsxLoad always allocates rt with MODE_WRITE — so enough back-to-back LWs
// force the pool to exhaust and exercise the eviction path.

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kData = RecompilerTestEnvironment::kScratchAddr;  // 0x00020000

// Distinct seed values small enough that their sum fits in u32 comfortably.
// Each slot is `0x01010101 * (i+1)` to make sums unambiguous. 15 slots:
// sum = 0x01010101 * (1+2+...+15) = 0x01010101 * 120 = 0x78787878.
constexpr u32 PatternFor(u32 i) { return 0x01010101u * (i + 1); }
} // namespace

TEST(IopRegallocPressure, FifteenLiveGprsTriggerEviction)
{
	// 15 LWs into distinct guest regs → forces at least one eviction (pool
	// is 14). Then an ADDU chain sums all 15 into v0. A botched
	// eviction/reload shows as a wrong sum.
	JitTestHarness h;
	h.SetGpr(reg::a0, kData);

	u32 expected = 0;
	for (u32 i = 0; i < 15; ++i)
	{
		const u32 v = PatternFor(i);
		h.WriteU32(kData + i * 4, v);
		expected += v;
	}

	// Guest regs t0..t9, s0..s4 — 15 regs, disjoint from a0 (base) and v0.
	const u32 rr[15] = {
		reg::t0, reg::t1, reg::t2, reg::t3, reg::t4,
		reg::t5, reg::t6, reg::t7, reg::t8, reg::t9,
		reg::s0, reg::s1, reg::s2, reg::s3, reg::s4,
	};

	std::vector<u32> prog;
	for (u32 i = 0; i < 15; ++i)
		prog.push_back(LW(rr[i], static_cast<s16>(i * 4), reg::a0));
	// v0 = t0
	prog.push_back(ADDU(reg::v0, rr[0], reg::zero));
	for (u32 i = 1; i < 15; ++i)
		prog.push_back(ADDU(reg::v0, reg::v0, rr[i]));

	h.LoadProgramAt(RecompilerTestEnvironment::kProgramPc,
		prog.data(), prog.size(),
		/*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), expected);
	// Every individual reg also carried its loaded value at the final ADDU:
	for (u32 i = 0; i < 15; ++i)
		EXPECT_EQ(h.GetGprInterp(rr[i]), PatternFor(i))
			<< "guest reg index " << i << " (mips reg " << rr[i] << ")";
}

TEST(IopRegallocPressure, TwentyLiveGprsCascadeEviction)
{
	// 20 LWs → at minimum 6 evictions required to finish the block.
	// Sums should still land correctly.
	JitTestHarness h;
	h.SetGpr(reg::a0, kData);

	u32 expected = 0;
	for (u32 i = 0; i < 20; ++i)
	{
		const u32 v = PatternFor(i);
		h.WriteU32(kData + i * 4, v);
		expected += v;
	}

	// Use t0..t9, s0..s7, v1, a1, a2 — 20 regs, disjoint from a0 + v0.
	const u32 rr[20] = {
		reg::t0, reg::t1, reg::t2, reg::t3, reg::t4,
		reg::t5, reg::t6, reg::t7, reg::t8, reg::t9,
		reg::s0, reg::s1, reg::s2, reg::s3, reg::s4,
		reg::s5, reg::s6, reg::s7, reg::v1, reg::a1,
	};

	std::vector<u32> prog;
	for (u32 i = 0; i < 20; ++i)
		prog.push_back(LW(rr[i], static_cast<s16>(i * 4), reg::a0));
	prog.push_back(ADDU(reg::v0, rr[0], reg::zero));
	for (u32 i = 1; i < 20; ++i)
		prog.push_back(ADDU(reg::v0, reg::v0, rr[i]));

	h.LoadProgramAt(RecompilerTestEnvironment::kProgramPc,
		prog.data(), prog.size(),
		/*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), expected);
}

TEST(IopRegallocPressure, WritesOnlyWithoutSubsequentReadStillWritesBack)
{
	// 15 LWs without a subsequent ADDU chain. Each load still allocates
	// and must eventually write back to psxRegs.GPR.r[] on block exit —
	// so the final snapshot should reflect each loaded value. Regression
	// guard: the eviction path must not lose write-only loads.
	JitTestHarness h;
	h.SetGpr(reg::a0, kData);

	for (u32 i = 0; i < 15; ++i)
		h.WriteU32(kData + i * 4, PatternFor(i));

	const u32 rr[15] = {
		reg::t0, reg::t1, reg::t2, reg::t3, reg::t4,
		reg::t5, reg::t6, reg::t7, reg::t8, reg::t9,
		reg::s0, reg::s1, reg::s2, reg::s3, reg::s4,
	};

	std::vector<u32> prog;
	for (u32 i = 0; i < 15; ++i)
		prog.push_back(LW(rr[i], static_cast<s16>(i * 4), reg::a0));

	h.LoadProgramAt(RecompilerTestEnvironment::kProgramPc,
		prog.data(), prog.size(),
		/*append_jr_ra_term=*/true);
	h.Run();
	for (u32 i = 0; i < 15; ++i)
		EXPECT_EQ(h.GetGprInterp(rr[i]), PatternFor(i))
			<< "guest reg index " << i;
}

TEST(IopRegallocPressure, ReadAfterEvictionRoundsTripsThroughMemory)
{
	// Interleave LWs and reads: load the first reg, then 13 more; then
	// read the very first reg back into v0. By the time it is read back,
	// the allocator must have evicted and reloaded it (because 13 more
	// LWs fill the pool). A broken spill target address would show as v0
	// holding garbage instead of the first pattern.
	JitTestHarness h;
	h.SetGpr(reg::a0, kData);

	for (u32 i = 0; i < 14; ++i)
		h.WriteU32(kData + i * 4, PatternFor(i));

	const u32 rr[14] = {
		reg::t0, reg::t1, reg::t2, reg::t3, reg::t4,
		reg::t5, reg::t6, reg::t7, reg::t8, reg::t9,
		reg::s0, reg::s1, reg::s2, reg::s3,
	};

	std::vector<u32> prog;
	for (u32 i = 0; i < 14; ++i)
		prog.push_back(LW(rr[i], static_cast<s16>(i * 4), reg::a0));
	// After 14 loads + base reg a0 + potential scratch, the allocator is
	// exhausted. The next read of t0 forces eviction of something else
	// and a reload of t0 from psxRegs (or the cache).
	prog.push_back(ADDU(reg::v0, rr[0], reg::zero));   // reads t0 late
	prog.push_back(ADDU(reg::v1, rr[13], reg::zero));  // reads s3 late

	h.LoadProgramAt(RecompilerTestEnvironment::kProgramPc,
		prog.data(), prog.size(),
		/*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), PatternFor(0));
	EXPECT_EQ(h.GetGprInterp(reg::v1), PatternFor(13));
}

TEST(IopRegallocPressure, PairedHiLoStayLiveDuringPoolBurn)
{
	// MULT sets both HI and LO. A follow-up MFHI + MFLO reads them. In
	// between, 14 unrelated LWs exhaust the pool and force eviction of
	// whatever slots HI/LO chose. HI/LO writebacks must survive the
	// eviction and the MFHI/MFLO reads must see the correct values.
	JitTestHarness h;
	h.SetGpr(reg::a0, kData);
	h.SetGpr(reg::a1, 0x10000);   // multiplicand
	h.SetGpr(reg::a2, 0x10001);   // multiplier → 64-bit product 0x1'00010000

	for (u32 i = 0; i < 14; ++i)
		h.WriteU32(kData + i * 4, PatternFor(i));

	// 0x10000 * 0x10001 = 0x1_00010000. So LO = 0x00010000, HI = 0x00000001.
	constexpr u32 kExpectLo = 0x00010000u;
	constexpr u32 kExpectHi = 0x00000001u;

	const u32 rr[14] = {
		reg::t0, reg::t1, reg::t2, reg::t3, reg::t4,
		reg::t5, reg::t6, reg::t7, reg::t8, reg::t9,
		reg::s0, reg::s1, reg::s2, reg::s3,
	};

	std::vector<u32> prog;
	// RType encoder for MULT: funct 0x18, rs, rt, rd=0, sa=0
	prog.push_back(mips::RType(0, reg::a1, reg::a2, 0, 0, 0x18));  // mult a1, a2
	for (u32 i = 0; i < 14; ++i)
		prog.push_back(LW(rr[i], static_cast<s16>(i * 4), reg::a0));
	// MFHI rd: funct 0x10, rs=0, rt=0, rd=rd
	prog.push_back(mips::RType(0, 0, 0, reg::v0, 0, 0x10));  // mfhi v0
	// MFLO rd: funct 0x12
	prog.push_back(mips::RType(0, 0, 0, reg::v1, 0, 0x12));  // mflo v1

	h.LoadProgramAt(RecompilerTestEnvironment::kProgramPc,
		prog.data(), prog.size(),
		/*append_jr_ra_term=*/true);
	h.Run();
	EXPECT_EQ(h.GetGprInterp(reg::v0), kExpectHi) << "MFHI after pool burn";
	EXPECT_EQ(h.GetGprInterp(reg::v1), kExpectLo) << "MFLO after pool burn";
}

TEST(IopRegallocPressure, EvictedRegIsReaderObservable)
{
	// Cascade: 15 LWs (forces the first eviction), then read ALL of
	// them. A silent miscopy of the evicted reg's spill destination
	// would show as ONE of the GPR reads holding wrong data, without
	// affecting the others.
	JitTestHarness h;
	h.SetGpr(reg::a0, kData);

	for (u32 i = 0; i < 15; ++i)
		h.WriteU32(kData + i * 4, PatternFor(i));

	const u32 rr[15] = {
		reg::t0, reg::t1, reg::t2, reg::t3, reg::t4,
		reg::t5, reg::t6, reg::t7, reg::t8, reg::t9,
		reg::s0, reg::s1, reg::s2, reg::s3, reg::s4,
	};

	std::vector<u32> prog;
	for (u32 i = 0; i < 15; ++i)
		prog.push_back(LW(rr[i], static_cast<s16>(i * 4), reg::a0));
	// A single ADDU chain "touches" each reg in order, promoting a read
	// of each. If any eviction mis-wrote to psxRegs, the individual
	// GetGprInterp checks below catch it by reg-name.
	prog.push_back(ADDU(reg::v0, rr[0], rr[1]));
	for (u32 i = 2; i < 15; ++i)
		prog.push_back(ADDU(reg::v0, reg::v0, rr[i]));

	h.LoadProgramAt(RecompilerTestEnvironment::kProgramPc,
		prog.data(), prog.size(),
		/*append_jr_ra_term=*/true);
	h.Run();
	for (u32 i = 0; i < 15; ++i)
		EXPECT_EQ(h.GetGprInterp(rr[i]), PatternFor(i))
			<< "reg index " << i;
}
