// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// VU0 spin-wait fast-forward (mVUdetectSpinLoop / mVUemitSpinFF,
// microVU_Compile-arm64.inl).
//
// EE-handshake busy-wait loops — a conditional VI branch whose whole loop
// body is exact-NOP encodings — dominate VU0 execution in macro-handshake
// titles (UYA gameplay: 89% of VU0 micro cycles are three such loops). The
// FF consumes the entire remaining cycle grant in one step whenever the
// spin condition still holds: nothing can release the spin mid-grant (the
// EE thread is stalled inside Execute; only the EE writes the handshake
// VI), so N spin iterations are architecturally identical to none plus a
// cycle skip.
//
// Contracts pinned here:
//   1. A held spin consumes the whole grant (VU0.cycle advances by the
//      full budget) with ZERO architectural side effects, stays running,
//      and parks TPC at the loop head (the VE-07 resume slot).
//   2. Releasing the handshake VI exits the loop and the program completes
//      identically to a never-spun run.
//   3. Both recognized shapes FF: the 4-pair {cond; NOP; B ->head; NOP}
//      and the 2-pair self-loop {cond ->self; NOP}.
//   4. NEGATIVE: a loop whose body has any real op (non-NOP encoding) is
//      NOT fast-forwarded — it must execute its iterations for real.

#include "harness/VuTestHarness.h"

#include "VU.h"
#include "VUmicro.h"

#include <cstring>
#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace {

// The detector requires the EXACT architectural NOP encodings the games use
// (upper 0x000002FF, lower 0x8000033C) — the harness's generic NopPair()
// uses an I-bit literal-zero pair, which is deliberately NOT recognized.
constexpr u32 kUpperNop = 0x000002FFu;
constexpr u32 kLowerNop = 0x8000033Cu;
inline VuOp ExactNopPair() { return VuOp{kLowerNop, kUpperNop}; }
inline VuOp SpinOp(u32 lower) { return VuOp{lower, kUpperNop}; }
inline VuOp LowerOnly(u32 lower) { return VuOp{lower, VNOP_U()}; }

void SeedVu0Dispatch(u32 start_pc_bytes)
{
	vuRegs[0].VI[REG_TPC].UL = start_pc_bytes / 8u;
	CpuMicroVU0.SetStartPC(start_pc_bytes);
	vuRegs[0].VI[REG_VPU_STAT].UL |= 0x1u;
}

bool Vu0Busy()
{
	return (vuRegs[0].VI[REG_VPU_STAT].UL & 0x1u) != 0;
}

// The UYA shape: spin at head while vi1 != 0, then release code sets vi3
// and ends. Loop exit is the IBEQ taken arm (+3 pairs → pair 4).
// Loaded with vi1 = 0 (released) so Run()'s JIT-vs-interp diff exercises
// the deterministic exit path; the spin assertions re-dispatch directly.
void LoadUyaShapeProgram(VuTestHarness& h)
{
	h.SetVi(vi::vi1, 0); // released for the Run() equivalence pass
	h.LoadProgram({
		SpinOp(VIBEQ_L(vi::vi1, vi::vi0, 3)),  // pair 0: exit when vi1 == 0
		ExactNopPair(),                        // pair 1: branch delay slot
		SpinOp(VB_L(-3)),                      // pair 2: back to pair 0
		ExactNopPair(),                        // pair 3: branch delay slot
		LowerOnly(VIADDIU_L(vi::vi3, vi::vi0, 42)), // pair 4: loop exit
		EBitNopPair(),
	});
}

} // namespace

TEST(MvuSpinFF, HeldSpinConsumesWholeGrantWithoutSideEffects)
{
	VuTestHarness h(0);
	LoadUyaShapeProgram(h);
	h.Run(); // seeds micro mem + verifies base JIT-vs-interp equivalence of
	         // the released path (Run() flips nothing: vi1 == 1 spins until
	         // the harness budget starves both sides identically)

	// Direct production dispatch: a held spin must consume the entire grant.
	vuRegs[0].VI[vi::vi1].UL = 1;
	vuRegs[0].VI[vi::vi3].UL = 0;
	const u64 cycle_before = vuRegs[0].cycle;
	SeedVu0Dispatch(0);
	CpuMicroVU0.Execute(1000);

	EXPECT_TRUE(Vu0Busy()) << "a held spin must stay running";
	EXPECT_EQ(vuRegs[0].cycle - cycle_before, 1000u)
		<< "the FF must bank the whole grant into VU0.cycle";
	EXPECT_EQ(vuRegs[0].VI[vi::vi1].UL & 0xFFFFu, 1u) << "spin must not write VI";
	EXPECT_EQ(vuRegs[0].VI[vi::vi3].UL & 0xFFFFu, 0u) << "exit code must not run";
	EXPECT_EQ(vuRegs[0].VI[REG_TPC].UL, 0u)
		<< "TPC must park at the spin head for the resume re-entry";

	// Release: the next grant exits the loop and completes the program.
	vuRegs[0].VI[vi::vi1].UL = 0;
	CpuMicroVU0.Execute(1000);
	EXPECT_FALSE(Vu0Busy()) << "released spin must run to the E-bit end";
	EXPECT_EQ(vuRegs[0].VI[vi::vi3].UL & 0xFFFFu, 42u);
}

TEST(MvuSpinFF, SelfLoopShapeFastForwards)
{
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1);
	h.LoadProgram({
		SpinOp(VIBNE_L(vi::vi1, vi::vi0, -1)),      // pair 0: spin while vi1 != 0
		ExactNopPair(),                             // pair 1: delay slot
		LowerOnly(VIADDIU_L(vi::vi3, vi::vi0, 7)),  // pair 2: fallthrough exit
		EBitNopPair(),
	});
	h.Run();

	vuRegs[0].VI[vi::vi1].UL = 1;
	vuRegs[0].VI[vi::vi3].UL = 0;
	const u64 cycle_before = vuRegs[0].cycle;
	SeedVu0Dispatch(0);
	CpuMicroVU0.Execute(500);

	EXPECT_TRUE(Vu0Busy());
	EXPECT_EQ(vuRegs[0].cycle - cycle_before, 500u)
		<< "self-loop shape must FF the whole grant";
	EXPECT_EQ(vuRegs[0].VI[REG_TPC].UL, 0u);

	vuRegs[0].VI[vi::vi1].UL = 0;
	CpuMicroVU0.Execute(500);
	EXPECT_FALSE(Vu0Busy());
	EXPECT_EQ(vuRegs[0].VI[vi::vi3].UL & 0xFFFFu, 7u);
}

TEST(MvuSpinFF, SideEffectfulLoopIsNotFastForwarded)
{
	// Same 4-pair shape but the B delay slot increments vi2 — a real op, so
	// the detector must reject it and the loop must execute for real.
	VuTestHarness h(0);
	h.SetVi(vi::vi1, 1);
	h.SetVi(vi::vi2, 0);
	h.LoadProgram({
		SpinOp(VIBEQ_L(vi::vi1, vi::vi0, 3)),       // pair 0
		ExactNopPair(),                             // pair 1
		SpinOp(VB_L(-3)),                           // pair 2
		SpinOp(VIADDI_L(vi::vi2, vi::vi2, 1)),      // pair 3: REAL op in ds
		LowerOnly(VIADDIU_L(vi::vi3, vi::vi0, 42)), // pair 4: exit
		EBitNopPair(),
	});
	h.Run();

	vuRegs[0].VI[vi::vi1].UL = 1;
	vuRegs[0].VI[vi::vi2].UL = 0;
	SeedVu0Dispatch(0);
	CpuMicroVU0.Execute(400);

	EXPECT_TRUE(Vu0Busy());
	EXPECT_GT(vuRegs[0].VI[vi::vi2].UL & 0xFFFFu, 10u)
		<< "a side-effectful loop must iterate for real (no FF)";
}

} // namespace recompiler_tests
