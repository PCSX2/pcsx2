// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// FPU / COP1 coverage for the EE recompiler. Single-precision only (the
// PS2 FPU is 32-bit; the `DOUBLE::` namespace is internal accuracy emulation,
// not a user-visible double-precision ISA).
//
// Ops covered: MTC1/MFC1 bit moves, CTC1/CFC1 control-register moves,
// ADD.S/SUB.S/MUL.S/DIV.S, NEG.S/ABS.S/MOV.S, CVT.W.S, compare family
// (C.EQ.S/C.LT.S) + BC1T/BC1F.
//
// Value discipline: tests use small-integer float values and simple
// ratios to avoid PS2 FPU quirks (denormal flush-to-zero, peculiar NaN
// propagation) that only matter for full FPU correctness.

#include "harness/EeRecTestHarness.h"

#include "Config.h"
#include "common/FPControl.h"

#include <cstring>
#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kPark = RecompilerTestEnvironment::kParkingPc;

// Scoped enable of CHECK_FPU_EXTRA_OVERFLOW (per-game GameDB clampMode>=2),
// which the harness leaves at its default-off. Restores on scope exit so the
// flag never leaks into sibling tests.
struct FpuExtraOverflowGuard
{
	bool saved = EmuConfig.Cpu.Recompiler.fpuExtraOverflow;
	FpuExtraOverflowGuard() { EmuConfig.Cpu.Recompiler.fpuExtraOverflow = true; }
	~FpuExtraOverflowGuard() { EmuConfig.Cpu.Recompiler.fpuExtraOverflow = saved; }
};

u32 FloatBits(float f)
{
	u32 bits;
	std::memcpy(&bits, &f, sizeof(bits));
	return bits;
}

float BitsToFloat(u32 bits)
{
	float f;
	std::memcpy(&f, &bits, sizeof(f));
	return f;
}
} // namespace

TEST(EeRecFpu, Mtc1MovesGprBitsToFpr)
{
	// MTC1 copies GPR bits verbatim to the FPR; no conversion. 0x40490FDB
	// is the IEEE-754 bit pattern for a value near π.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetGpr64(reg::a0, 0x40490FDBu);
	h.LoadProgram({
		ee::MTC1(reg::a0, 1),            // fpr1 = bits(a0)
	});
	h.Run();
	h.ExpectFpr(1, 0x40490FDBu);
}

TEST(EeRecFpu, Mfc1MovesFprBitsToGprWithSignExtend)
{
	// MFC1 copies the 32-bit FPR bit pattern into rt, sign-extended to 64-bit.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(2, 0x80000001u);       // negative single-precision pattern
	h.LoadProgram({
		ee::MFC1(reg::v0, 2),
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0xFFFFFFFF80000001ull);
}

// ---------------------------------------------------------------------------
//  LWC1 / SWC1 — FPU 32-bit load/store
//
//  These take the inline fastmem path (LDR/STR off RFASTMEMBASE +
//  backpatch) when CHECK_FASTMEM is set, with the softmem C-call as the
//  faulting-PC fallback. In the test build the harness wires fastmem, so
//  these exercise the fast path. Round-trip + bit-exactness are the spec:
//  LWC1 copies 32 raw bits into fpr[ft] verbatim (no FP conversion), and
//  SWC1 copies fpr[ft]'s 32 bits to memory verbatim.
// ---------------------------------------------------------------------------
namespace {
constexpr u32 kScratch = RecompilerTestEnvironment::kScratchAddr;
}

TEST(EeRecFpu, Lwc1LoadsRawBitsIntoFpr)
{
	EeRecTestHarness h;
	h.EnableCop1();
	// Bit pattern with the sign bit set — proves no sign-extend / FP munge.
	h.WriteU32(kScratch, 0x80490FDBu);
	h.SetGpr64(reg::a0, kScratch);
	h.LoadProgram({
		ee::LWC1(2, 0, reg::a0),         // fpr2 = mem32[a0]
	});
	h.Run();
	h.ExpectFpr(2, 0x80490FDBu);
}

TEST(EeRecFpu, Swc1StoresRawFprBitsToMemory)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(3, 0xDEADBEEFu);        // raw pattern, not a clean float
	h.SetGpr64(reg::a0, kScratch);
	h.TrackMemWindow(kScratch, 4);
	h.LoadProgram({
		ee::SWC1(3, 0, reg::a0),         // mem32[a0] = fpr3
	});
	h.Run();
	EXPECT_EQ(h.ReadU32(kScratch), 0xDEADBEEFu);
}

TEST(EeRecFpu, Lwc1Swc1RoundtripWithOffset)
{
	// Load from one slot, store to another via a non-zero immediate offset —
	// exercises recComputeAddr's Add path and the fastmem index register.
	EeRecTestHarness h;
	h.EnableCop1();
	h.WriteU32(kScratch + 4, 0x3F800000u);   // 1.0f bits
	h.SetGpr64(reg::a0, kScratch);
	h.TrackMemWindow(kScratch, 16);
	h.LoadProgram({
		ee::LWC1(4, 4, reg::a0),         // fpr4 = mem32[a0+4]
		ee::SWC1(4, 8, reg::a0),         // mem32[a0+8] = fpr4
	});
	h.Run();
	h.ExpectFpr(4, 0x3F800000u);
	EXPECT_EQ(h.ReadU32(kScratch + 8), 0x3F800000u);
}

TEST(EeRecFpu, AddSInteger)
{
	// 3.0 + 4.0 = 7.0 — no rounding quirks.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(7.0f));
}

TEST(EeRecFpu, SubSInteger)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 10.0f);
	h.SetFpr(2, 3.0f);
	h.LoadProgram({
		ee::SUB_S(3, 1, 2),
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(7.0f));
}

TEST(EeRecFpu, MulSInteger)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 6.0f);
	h.SetFpr(2, 7.0f);
	h.LoadProgram({
		ee::MUL_S(3, 1, 2),
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(42.0f));
}

TEST(EeRecFpu, DivSExactRatio)
{
	// 20 / 4 = 5. Exact IEEE-754 result, no rounding divergence.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 20.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({
		ee::DIV_S(3, 1, 2),
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(5.0f));
}

// ---- Native DIV.S: divide-by-zero corners. interp DIV_S is the oracle, so
//      Run() diffs the value; both snapshots' FCR31 are also asserted directly
//      to pin the sticky flags. ---------------------------------------------

TEST(EeRecFpu, DivSNegativeQuotientExact)
{
	// 6 / -2 = -3, exact — no rounding-mode sensitivity, no D/I flags.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFcr31(0);
	h.SetFpr(1, 6.0f);
	h.SetFpr(2, -2.0f);
	h.LoadProgram({ee::DIV_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, FloatBits(-3.0f));
	const u32 mask = 0x20000u | 0x10000u; // I | D
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask, 0u);
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0u);
}

TEST(EeRecFpu, DivSByZeroSetsDenormFlagsAndMax)
{
	// 4 / +0 = +posFmax, with D|SD raised (x/0).
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFcr31(0);
	h.SetFpr(1, 4.0f);
	h.SetFprBits(2, 0x00000000u); // +0
	h.LoadProgram({ee::DIV_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, 0x7F7FFFFFu);
	const u32 mask = 0x20000u | 0x10000u | 0x40u | 0x20u; // I|D|SI|SD
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask,    0x10000u | 0x20u); // D|SD
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0x10000u | 0x20u);
}

TEST(EeRecFpu, DivSByZeroNegativeDividendSignedMax)
{
	// -4 / +0 : sign(Fs^Ft) is negative -> -posFmax (0xff7fffff), D|SD.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFcr31(0);
	h.SetFpr(1, -4.0f);
	h.SetFprBits(2, 0x00000000u); // +0
	h.LoadProgram({ee::DIV_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, 0xFF7FFFFFu);
	const u32 mask = 0x10000u | 0x20u; // D|SD
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask,    0x10000u | 0x20u);
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0x10000u | 0x20u);
}

TEST(EeRecFpu, DivSByNegativeZeroDivisorSign)
{
	// 8 / -0 : divisor is -0 (caught by the float==0 compare under FtZ); sign is
	// driven by the divisor -> -posFmax, D|SD.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFcr31(0);
	h.SetFpr(1, 8.0f);
	h.SetFprBits(2, 0x80000000u); // -0
	h.LoadProgram({ee::DIV_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, 0xFF7FFFFFu);
	const u32 mask = 0x10000u | 0x20u; // D|SD
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask,    0x10000u | 0x20u);
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0x10000u | 0x20u);
}

TEST(EeRecFpu, DivSZeroByZeroSetsInvalidFlags)
{
	// 0 / 0 -> +posFmax with I|SI raised (invalid), not D|SD.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFcr31(0);
	h.SetFprBits(1, 0x00000000u); // +0
	h.SetFprBits(2, 0x00000000u); // +0
	h.LoadProgram({ee::DIV_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, 0x7F7FFFFFu);
	const u32 mask = 0x20000u | 0x10000u | 0x40u | 0x20u; // I|D|SI|SD
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask,    0x20000u | 0x40u); // I|SI
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0x20000u | 0x40u);
}

TEST(EeRecFpu, NegSFlipsSignBit)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.5f);
	h.LoadProgram({
		ee::NEG_S(2, 1),
	});
	h.Run();
	h.ExpectFpr(2, FloatBits(-3.5f));
}

// NEG.S must preserve the sign when clamping a poisoned (raw Inf/NaN bits)
// operand. NEG_S of a +NaN produces a -NaN intermediate (Fneg = sign flip),
// which the result clamp must fold to -FLT_MAX (sign preserved), not +FLT_MAX.
// The arm64 rec used fpuClampResult (Fminnm/Fmaxnm), which folds every NaN to
// +fMax (sign lost); the fix uses fpuClampCompareOperand (Smin/Umin, sign-
// preserving), mirroring x86's switch from ClampValues to fpuFloat3 (upstream
// 4ffbe0bbf).
//
// JIT-only: the single-precision interp NEG_S (FPU.cpp:334) just XORs the sign
// bit with no clamp at all (-> raw -NaN), so neither the pre- nor post-fix rec
// matches it. Assert GetFprBitsJit() directly via RunJitNoDiff().
TEST(EeRecFpu, NegSPreservesSignOnPoisonedNan)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x7FC00000u); // +NaN raw bits (poisoned fpr)
	h.LoadProgram({
		ee::NEG_S(2, 1),
	});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), 0xFF7FFFFFu); // -FLT_MAX (sign preserved)
}

TEST(EeRecFpu, AbsSClearsSignBit)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, -4.25f);
	h.LoadProgram({
		ee::ABS_S(2, 1),
	});
	h.Run();
	h.ExpectFpr(2, FloatBits(4.25f));
}

TEST(EeRecFpu, MovSBitCopy)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x12345678u);
	h.LoadProgram({
		ee::MOV_S(2, 1),
	});
	h.Run();
	h.ExpectFpr(2, 0x12345678u);
}

// MOV.S fd,fd aliases the same host reg; the emit is skipped. The
// value must be preserved verbatim (the no-op is a true identity, not a drop).
TEST(EeRecFpu, MovSSelfMoveIsIdentity)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(5, 0xCAFEB0BAu);
	h.LoadProgram({
		ee::MOV_S(5, 5),
	});
	h.Run();
	h.ExpectFpr(5, 0xCAFEB0BAu);
}

TEST(EeRecFpu, CvtWSTruncatesToward)
{
	// 3.7 → 3 (FCR31 rounding mode is RZ/RN/... — use a value where
	// every IEEE-754 rounding mode agrees to avoid harness-dependent
	// results).
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.LoadProgram({
		ee::CVT_W_S(2, 1),
	});
	h.Run();
	h.ExpectFpr(2, 3u);       // integer 3 stored as bit pattern in the FPR
}

// ----- CVT.W NaN saturation ------------------------------------------
//
// ARM64 Fcvtzs converts NaN → 0, but the PS2 (interp CVT_W) saturates a
// NaN input by sign: +NaN → 0x7fffffff, -NaN → 0x80000000 (never 0). Inject
// raw NaN via SetFprBits (MTC1/LWC1 bit-copies bypass the arithmetic clamp).
// Run()'s auto-diff compares the FPR result, so an unfixed bare Fcvtzs (→0)
// diverges from interp; ExpectFpr pins the PS2 spec value on both sides.
TEST(EeRecFpu, CvtWPositiveNanSaturatesToIntMax)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x7FC00000u);   // +NaN
	h.LoadProgram({
		ee::CVT_W_S(2, 1),
	});
	h.Run();
	h.ExpectFpr(2, 0x7fffffffu);    // +NaN → INT_MAX
}

TEST(EeRecFpu, CvtWNegativeNanSaturatesToIntMin)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0xFFC00000u);   // -NaN
	h.LoadProgram({
		ee::CVT_W_S(2, 1),
	});
	h.Run();
	h.ExpectFpr(2, 0x80000000u);    // -NaN → INT_MIN
}

// ----- SQRT.S sticky-flag handling -----------------------------------
//
// PS2 SQRT.S clears the I|D cause flags unconditionally and sets I|SI when
// Ft is negative non-zero (interp SQRT_S, FPU.cpp; CHECK_FPU_EXTRA_FLAGS is
// hardcoded on). Run()'s auto-diff does not gate on fprc[31], so assert the
// flag bits directly on both snapshots (they must agree — the JIT matches
// interp). Result value (sqrt(|Ft|)) is unchanged and stays in the auto-diff.
TEST(EeRecFpu, SqrtSNegativeSetsInvalidStickyFlags)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, -4.0f);             // negative non-zero
	h.LoadProgram({
		ee::SQRT_S(2, 1),          // fd=2, ft=1; result sqrt(4)=2.0
	});
	h.Run();
	h.ExpectFpr(2, 0x40000000u);   // 2.0f
	const u32 mask = 0x20000u | 0x10000u | 0x40u;   // I | D | SI
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask,    0x20000u | 0x40u);  // I|SI set, D clear
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0x20000u | 0x40u);
}

TEST(EeRecFpu, SqrtSPositiveClearsStaleIDFlags)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFcr31(0x20000u | 0x10000u);   // pre-set stale I|D
	h.SetFpr(1, 4.0f);                  // positive → no flag set
	h.LoadProgram({
		ee::SQRT_S(2, 1),              // result sqrt(4)=2.0
	});
	h.Run();
	h.ExpectFpr(2, 0x40000000u);       // 2.0f
	// I and D must be cleared; SI must NOT have been set (positive input).
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & (0x20000u | 0x10000u | 0x40u), 0u);
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & (0x20000u | 0x10000u | 0x40u), 0u);
}

// ----- SQRT.S rounds to nearest regardless of FCR31 mode -------------
// PS2 SQRT.S (like DIV.S) always rounds to nearest, independent of the
// configured EE rounding mode. The EE rec runs under host FPCR = FPUFPCR
// (ChopZero by default), so recSQRT_S must swap to the nearest-rounding
// FPUDivFPCR around the Fsqrt. This is not observable via Run()'s JIT-vs-interp
// auto-diff (the harness runs both under the host-default nearest FPCR, where
// the swap is a no-op). Instead replicate the real EE thread: set host FPCR to
// FPUFPCR (chop) and assert the JIT result directly. sqrt(5) is rounding-
// sensitive — nearest 0x400F1BBD vs round-toward-zero 0x400F1BBC. Without the
// in-op swap the Fsqrt would round under the ambient chop and produce
// 0x400F1BBC; with it the result is the PS2-correct nearest value.
TEST(EeRecFpu, SqrtSRoundsToNearestUnderChopFpcr)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprSingle(1, 5.0f);
	h.LoadProgram({ee::SQRT_S(2, 1)});
	const FPControlRegister saved = FPControlRegister::GetCurrent();
	FPControlRegister::SetCurrent(EmuConfig.Cpu.FPUFPCR); // EE-thread ambient (chop)
	h.RunJitNoDiff();
	FPControlRegister::SetCurrent(saved);
	EXPECT_EQ(h.GetFprBitsJit(2), 0x400F1BBDu); // nearest-rounded sqrt(5)
}

// ----- RSQRT.S deferred to interpreter --------------------------------
//
// RSQRT.S sets D|SD when the divisor Ft is zero and I|SI when Ft is negative
// (interp RSQRT_S, FPU.cpp), and its Ft==0 branch returns ±posFmax keyed off
// the Ft sign. The op defers to the interpreter to handle flags and the
// zero-divisor result correctly. Assert the flag bits directly (Run() doesn't
// diff fprc[31]).
TEST(EeRecFpu, RsqrtSZeroDivisorSetsDenormFlags)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 1.0f);             // fs (dividend)
	h.SetFprBits(2, 0x00000000u);  // ft (divisor) = +0
	h.LoadProgram({
		ee::RSQRT_S(3, 1, 2),     // fd=3, fs=1, ft=2
	});
	h.Run();
	h.ExpectFpr(3, 0x7F7FFFFFu);   // +posFmax (zero-divisor result)
	const u32 mask = 0x20000u | 0x10000u | 0x40u | 0x20u;   // I | D | SI | SD
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask,    0x10000u | 0x20u);  // D|SD set
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0x10000u | 0x20u);
}

TEST(EeRecFpu, RsqrtSNegativeDivisorSetsInvalidFlags)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 2.0f);             // fs (dividend)
	h.SetFpr(2, -4.0f);            // ft (divisor) negative
	h.LoadProgram({
		ee::RSQRT_S(3, 1, 2),     // fd=3; result 2/sqrt(4)=1.0
	});
	h.Run();
	h.ExpectFpr(3, 0x3F800000u);   // 1.0f
	const u32 mask = 0x20000u | 0x10000u | 0x40u | 0x20u;
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & mask,    0x20000u | 0x40u);  // I|SI set
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & mask, 0x20000u | 0x40u);
}

TEST(EeRecFpu, CEqSTrueSetsCc)
{
	// Pre: set fpr1 = fpr2 = 5.0. Expect FCR31.CC (bit 23) set to 1.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 5.0f);
	h.SetFpr(2, 5.0f);
	h.LoadProgram({
		ee::C_EQ_S(1, 2),
	});
	h.Run();
	// Assert both sides: Run()'s auto-diff does not gate on fprc[31], so a JIT
	// that never updates FCR31.CC would pass an interp-only assert.
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (1u << 23), 0u);
	EXPECT_NE(h.InterpSnapshot().fprs.fprc[31] & (1u << 23), 0u);
}

TEST(EeRecFpu, CEqSFalseClearsCc)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFcr31(1u << 23);                  // pre-set CC to 1
	h.SetFpr(1, 5.0f);
	h.SetFpr(2, 6.0f);
	h.LoadProgram({
		ee::C_EQ_S(1, 2),
	});
	h.Run();
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & (1u << 23), 0u);
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & (1u << 23), 0u);
}

// ----- compare-operand clamping --------------------------------------
//
// The PS2 FPU has no Inf/NaN: C.cond.S clamps both operands to ±FLT_MAX
// (sign-preserving) before comparing, matching interp fpuDouble and the
// x86 JIT's fpuFloat3 (PMIN.SD vs 0x7f7fffff, PMIN.UD vs 0xff7fffff).
// A raw Fcmp on unclamped bit patterns makes the compare unordered on a
// NaN operand (all-false) where the PS2 wants an ordered compare against
// ±FLT_MAX. Inject raw Inf/NaN via SetFprBits (MTC1/LWC1 bit-copies bypass
// the arithmetic clamp in real games).
//
// Run()'s internal JIT-vs-interp diff catches the divergence; the explicit
// assert pins the PS2 spec value. The -NaN case validates *sign* preservation
// (fpuClampResult / Fminnm would wrongly fold -NaN to +FLT_MAX).

// Assert on the JIT snapshot directly: the bug is JIT-side, and Run()'s
// auto-diff does not gate on fprc[31]. Without the operand clamp the raw Fcmp
// goes unordered on a NaN operand and leaves CC clear.
// Sign preservation: 0 < -NaN. The PS2 clamps -NaN to -FLT_MAX, so
// 0 < -FLT_MAX is FALSE (CC clear). A raw Fcmp on the NaN goes unordered,
// where ARM's "lt" (N!=V) is TRUE — so without the clamp CC is wrongly set.
// A sign-STRIPPING clamp (-NaN -> +FLT_MAX) would also wrongly set CC
// (0 < +FLT_MAX), so this case pins the sign-preserving SMIN/UMIN path.
TEST(EeRecFpu, CLtSZeroVsNegativeNaNIsFalse)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(2, 0.0f);
	h.SetFprBits(1, 0xFFC00000u);   // -NaN -> -FLT_MAX
	h.LoadProgram({
		ee::C_LT_S(2, 1),           // 0 < -FLT_MAX -> CC clear
	});
	h.Run();
	EXPECT_EQ(h.JitSnapshot().fprs.fprc[31] & (1u << 23), 0u);
	EXPECT_EQ(h.InterpSnapshot().fprs.fprc[31] & (1u << 23), 0u);
}

TEST(EeRecFpu, CEqSPositiveNaNBothClampToMax)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x7FC00000u);   // +NaN -> +FLT_MAX
	h.SetFprBits(2, 0x7FC00000u);   // +NaN -> +FLT_MAX
	h.LoadProgram({
		ee::C_EQ_S(1, 2),           // +FLT_MAX == +FLT_MAX -> CC set
	});
	h.Run();
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (1u << 23), 0u);
	EXPECT_NE(h.InterpSnapshot().fprs.fprc[31] & (1u << 23), 0u);
}

TEST(EeRecFpu, CEqSInfinityClampsToMax)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x7F800000u);   // +Inf -> +FLT_MAX
	h.SetFprBits(2, 0x7F7FFFFFu);   // +FLT_MAX (finite)
	h.LoadProgram({
		ee::C_EQ_S(1, 2),           // +FLT_MAX == +FLT_MAX -> CC set
	});
	h.Run();
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (1u << 23), 0u);
	EXPECT_NE(h.InterpSnapshot().fprs.fprc[31] & (1u << 23), 0u);
}

TEST(EeRecFpu, Bc1tTakenWhenCcSet)
{
	// Layout:
	//   0x00: C.EQ.S   fpr1, fpr2        — equal → CC = 1
	//   0x04: BC1T     +3                — taken, delay+3 words to target
	//   0x08: NOP      delay slot
	//   0x0C: ADDIU v0, zero, 1          — not-taken marker
	//   0x10: J park; NOP; NOP
	//   0x1C: ADDIU v0, zero, 2          — taken target
	//   0x20: J park; NOP
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 5.0f);
	h.SetFpr(2, 5.0f);
	h.LoadProgramNoTerm({
		ee::C_EQ_S(1, 2),
		ee::BC1T(3),
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);
}

TEST(EeRecFpu, Bc1fTakenWhenCcClear)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 5.0f);
	h.SetFpr(2, 6.0f);
	h.LoadProgramNoTerm({
		ee::C_EQ_S(1, 2),                 // 5 != 6 → CC = 0
		ee::BC1F(3),                      // taken
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 2ull);
}

// The not-taken edge exercises the forward-skip test-bit-and-branch
// (Tbz/Tbnz on fprc[31] bit 23). The "Taken" tests above only prove the skip
// does NOT fire; these prove it fires in the right direction.
TEST(EeRecFpu, Bc1tNotTakenWhenCcClear)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 5.0f);
	h.SetFpr(2, 6.0f);                     // 5 != 6 → CC = 0
	h.LoadProgramNoTerm({
		ee::C_EQ_S(1, 2),
		ee::BC1T(3),                       // CC clear → not taken (skip fires)
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 1ull);
}

TEST(EeRecFpu, Bc1fNotTakenWhenCcSet)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 5.0f);
	h.SetFpr(2, 5.0f);                     // 5 == 5 → CC = 1
	h.LoadProgramNoTerm({
		ee::C_EQ_S(1, 2),
		ee::BC1F(3),                       // CC set → not taken (skip fires)
		NOP,
		ADDIU(reg::v0, reg::zero, 1), J(kPark), NOP, NOP,
		ADDIU(reg::v0, reg::zero, 2), J(kPark), NOP,
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 1ull);
}

// ===========================================================================
//  FPU accumulator family — ADDA / SUBA / MULA write to ACC (no Fd field).
//  MADD / MSUB combine multiplication with ACC for Fd. MADDA / MSUBA do the
//  same but write back to ACC. The PS2 ISA mandates two separate roundings
//  (mul then add/sub) — these are NOT fused FMA.
// ===========================================================================

TEST(EeRecFpu, AddaSWritesAccumulator)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetAcc(99.0f);                       // pre-state: ACC should be overwritten, not accumulated
	h.LoadProgram({ee::ADDA_S(1, 2)});
	h.Run();
	h.ExpectAcc(FloatBits(7.0f));
}

TEST(EeRecFpu, SubaSWritesAccumulator)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 10.0f);
	h.SetFpr(2, 3.0f);
	h.SetAcc(99.0f);
	h.LoadProgram({ee::SUBA_S(1, 2)});
	h.Run();
	h.ExpectAcc(FloatBits(7.0f));
}

TEST(EeRecFpu, MulaSWritesAccumulator)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 6.0f);
	h.SetFpr(2, 7.0f);
	h.SetAcc(99.0f);
	h.LoadProgram({ee::MULA_S(1, 2)});
	h.Run();
	h.ExpectAcc(FloatBits(42.0f));
}

TEST(EeRecFpu, MaddSAddsProductToAccumulator)
{
	// fd = ACC + fs * ft = 10 + 3*4 = 22
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAcc(10.0f);
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({ee::MADD_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, FloatBits(22.0f));
}

TEST(EeRecFpu, MsubSSubtractsProductFromAccumulator)
{
	// fd = ACC - fs * ft = 100 - 3*4 = 88
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAcc(100.0f);
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({ee::MSUB_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, FloatBits(88.0f));
}

TEST(EeRecFpu, MaddaSAccumulatesIntoAccumulator)
{
	// ACC = ACC + fs * ft = 10 + 3*4 = 22; Fd field is not used
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAcc(10.0f);
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({ee::MADDA_S(1, 2)});
	h.Run();
	h.ExpectAcc(FloatBits(22.0f));
}

TEST(EeRecFpu, MsubaSSubtractsProductFromAccumulator)
{
	// ACC = ACC - fs * ft = 100 - 3*4 = 88
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAcc(100.0f);
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({ee::MSUBA_S(1, 2)});
	h.Run();
	h.ExpectAcc(FloatBits(88.0f));
}

// ----- MADDA/MSUBA must NOT clamp the intermediate product -----------
//
// Interp MADD_S/MSUB_S route the fs*ft product through fpuDouble (clamping it
// to +-fMax) before the accumulate, but MADDA_S/MSUBA_S (FPU.cpp) add the raw
// product directly and overflow-check only the final ACC. Clamping the product
// in all four ops diverges when fs*ft overflows: an overflowing product
// clamped to +fMax cancels against an opposite-signed ACC (-> 0) instead of
// overflowing the accumulate (-> +-fMax). Run()'s auto-diff compares ACC, and
// these cases are chosen so JIT and interp agree only without the product clamp.
TEST(EeRecFpu, MaddaSDoesNotClampIntermediateProduct)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAccBits(0xFF7FFFFFu); // ACC = -fMax
	h.SetFpr(1, 1e20f);
	h.SetFpr(2, 1e20f);        // fs*ft overflows single precision
	h.LoadProgram({ee::MADDA_S(1, 2)});
	h.Run();
	// interp: -fMax + overflow -> +fMax. with product clamped: -fMax + (+fMax) = 0.
	h.ExpectAcc(0x7F7FFFFFu);
}

TEST(EeRecFpu, MsubaSDoesNotClampIntermediateProduct)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAccBits(0x7F7FFFFFu); // ACC = +fMax
	h.SetFpr(1, 1e20f);
	h.SetFpr(2, 1e20f);        // fs*ft overflows single precision
	h.LoadProgram({ee::MSUBA_S(1, 2)});
	h.Run();
	// interp: +fMax - overflow -> -fMax. with product clamped: +fMax - (+fMax) = 0.
	h.ExpectAcc(0xFF7FFFFFu);
}

// ----- CHECK_FPU_EXTRA_OVERFLOW source clamp --------------------------
//
// With the extra-overflow gate on (per-game clampMode>=2), the PS2 FPU recs
// clamp each fpr SOURCE to +-fMax before the op, matching interp fpuDouble and
// x86 recCommutativeOp/recMADDtemp (fpuFloat2). The divergence needs a poisoned
// fpr (raw Inf/NaN bits, reachable via MOV.S/LWC1/MTC1) AND fs*ft -> NaN: e.g.
// Inf*0. Without the input clamp the JIT computes Inf*0 = NaN and the result
// clamp folds it to +fMax; interp clamps Inf->fMax first, so fMax*0 = 0.
// Run()'s auto-diff plus ExpectFpr both pin JIT to interp.
TEST(EeRecFpu, MulSExtraOverflowClampsInfOperand)
{
	FpuExtraOverflowGuard guard;
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(1, 0x7F800000u); // +Inf raw bits (poisoned fpr)
	h.SetFpr(2, 0.0f);
	h.LoadProgram({ee::MUL_S(3, 1, 2)});
	h.Run();
	h.ExpectFpr(3, 0x00000000u); // clamp(+Inf)*0 = +0; without input clamp -> +fMax
}

TEST(EeRecFpu, MaddSExtraOverflowClampsInfOperand)
{
	FpuExtraOverflowGuard guard;
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetAcc(5.0f);
	h.SetFprBits(1, 0x7F800000u); // +Inf raw bits
	h.SetFpr(2, 0.0f);
	h.LoadProgram({ee::MADD_S(3, 1, 2)});
	h.Run();
	// fd = ACC + clamp(+Inf)*0 = 5 + 0 = 5; without input clamp: 5 + (Inf*0=NaN->fMax) -> +fMax
	h.ExpectFpr(3, FloatBits(5.0f));
}

TEST(EeRecFpu, SqrtSPositiveValue)
{
	// PS2 SQRT.S takes sqrt of |ft|; argument is Ft, NOT Fs.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(2, 16.0f);
	h.LoadProgram({ee::SQRT_S(3, 2)});
	h.Run();
	h.ExpectFpr(3, FloatBits(4.0f));
}

TEST(EeRecFpu, SqrtSNegativeArgumentReturnsAbsRoot)
{
	// SQRT.S of a negative value returns sqrt(|ft|) (no NaN) — PS2 quirk.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(2, -25.0f);
	h.LoadProgram({ee::SQRT_S(3, 2)});
	h.Run();
	h.ExpectFpr(3, FloatBits(5.0f));
}

// ===========================================================================
//  Allocator-state interaction patterns — consecutive ops sharing operands.
//  The allocator path keeps operands in NEON across opcodes; an aliasing or
//  writeback bug surfaces here, not in the single-op tests above.
// ===========================================================================

TEST(EeRecFpu, AddSChainSameSourceTwice)
{
	// f3 = f1 + f2; then f4 = f3 + f2 (f2 re-used, allocator should keep it live)
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 1.0f);
	h.SetFpr(2, 2.0f);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),
		ee::ADD_S(4, 3, 2),
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(3.0f));
	h.ExpectFpr(4, FloatBits(5.0f));
}

TEST(EeRecFpu, AddSWriteSameAsRead)
{
	// f1 = f1 + f2 — destination aliases source.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 10.0f);
	h.SetFpr(2, 5.0f);
	h.LoadProgram({ee::ADD_S(1, 1, 2)});
	h.Run();
	h.ExpectFpr(1, FloatBits(15.0f));
}

TEST(EeRecFpu, AddaThenMaddChain)
{
	// Common geometry pattern: ADDA.S sets up ACC; MADD.S reads it.
	// ACC = f1 + f2 = 7; fd = ACC + f3*f4 = 7 + 6 = 13
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetFpr(3, 2.0f);
	h.SetFpr(4, 3.0f);
	h.LoadProgram({
		ee::ADDA_S(1, 2),
		ee::MADD_S(5, 3, 4),
	});
	h.Run();
	h.ExpectAcc(FloatBits(7.0f));
	h.ExpectFpr(5, FloatBits(13.0f));
}

TEST(EeRecFpu, MaddaChainAccumulates)
{
	// MULA.S then MADDA.S — common dot-product / vertex transform pattern.
	// ACC = f1*f2 = 6; ACC += f3*f4 = 6 + 20 = 26
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 2.0f);
	h.SetFpr(2, 3.0f);
	h.SetFpr(3, 4.0f);
	h.SetFpr(4, 5.0f);
	h.LoadProgram({
		ee::MULA_S(1, 2),
		ee::MADDA_S(3, 4),
	});
	h.Run();
	h.ExpectAcc(FloatBits(26.0f));
}

// ---------------------------------------------------------------------------
// MTC1 / MFC1 — allocator coherence with ADD_S-resident FPRs.
//
// ADD_S (routed through eeFPURecompileCode) leaves its destination FPR live
// in a NEON slot (MODE_WRITE) until block-end flush. MTC1 and MFC1 still
// bypass the allocator and go straight through memory at &fpuRegs.fpr[fs], so:
//   - MFC1 after ADD_S reads stale fpr[fs] from memory.
//   - MTC1 before block-end has its memory write clobbered when the
//     allocator flushes the stale-but-now-live NEON slot.
// ---------------------------------------------------------------------------

TEST(EeRecFpu, AddSThenMfc1ReadsAllocatorLiveResult)
{
	// ADD_S writes f3 (allocator-resident), MFC1 reads bits(f3) -> a0.
	// MFC1 must see 7.0f, not the pre-test memory contents of fpr[3].
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetFprBits(3, 0xDEADBEEFu);   // poison memory so a stale read is obvious
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),
		ee::MFC1(reg::a0, 3),
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(7.0f));
	h.ExpectGpr64(reg::a0, static_cast<u64>(static_cast<s64>(static_cast<s32>(FloatBits(7.0f)))));
}

TEST(EeRecFpu, Mtc1ThenAddSUsesFreshSource)
{
	// MTC1 writes fpr[1] from a0; ADD_S then reads f1.
	// If the allocator had cached f1 from a SetFpr-time prefetch (or any
	// earlier block), ADD_S would read the stale value rather than the
	// MTC1 result.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetGpr64(reg::a0, FloatBits(10.0f));
	h.SetFpr(1, 99.0f);            // pre-state: f1 = 99 in memory
	h.SetFpr(2, 4.0f);
	h.LoadProgram({
		ee::MTC1(reg::a0, 1),       // f1 <- bits(10.0)
		ee::ADD_S(3, 1, 2),         // f3 = f1 + f2 = 14
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(14.0f));
}

TEST(EeRecFpu, Mtc1AfterAddSOverwritesAllocatorCachedFpr)
{
	// ADD_S leaves f3 live in the allocator. MTC1 then writes f3 in
	// memory only. Block-end flush must NOT clobber the MTC1 write with
	// the stale-but-allocator-held ADD_S result.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetGpr64(reg::a0, FloatBits(123.0f));
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),         // f3 = 7 (allocator)
		ee::MTC1(reg::a0, 3),       // f3 = 123 (memory)
	});
	h.Run();
	h.ExpectFpr(3, FloatBits(123.0f));
}

TEST(EeRecFpu, Mtc1ThenAddSReadsMtc1Value)
{
	// ADD_S writes f3 first (so f3 is allocator-resident), then MTC1
	// updates f3 in memory, then a SECOND ADD_S consumes f3. The second
	// ADD_S must see the MTC1 value, not the cached allocator value.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetGpr64(reg::a0, FloatBits(100.0f));
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),         // f3 = 7 (allocator-live)
		ee::MTC1(reg::a0, 3),       // f3 := 100 (memory)
		ee::ADD_S(4, 3, 2),         // f4 = f3 + f2; must read 100, not 7
	});
	h.Run();
	h.ExpectFpr(4, FloatBits(104.0f));
}

// ---------------------------------------------------------------------------
// Direct-memory ops that bypass the FPR allocator — must flush dirty NEON
// slots before reading and invalidate on writes. Each test pairs an ADD_S
// (writes live to allocator) with the op being tested.
// ---------------------------------------------------------------------------

TEST(EeRecFpu, MovSAfterAddSPropagatesLiveValue)
{
	// ADD_S writes f3; MOV_S f4 = f3. MOV_S goes through memory copy.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetFprBits(4, 0xCAFEBABEu);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),
		ee::MOV_S(4, 3),
	});
	h.Run();
	h.ExpectFpr(4, FloatBits(7.0f));
}

// MFC1 reading an FPR that a preceding ADD_S left allocator-resident must read
// the live value straight from the host reg (the resident-read fast path),
// sign-extended into rt. f3=7.0 -> v0 = 0x0000000040E00000.
TEST(EeRecFpu, Mfc1AfterAddSReadsResidentValue)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),         // f3 = 7 (allocator-resident)
		ee::MFC1(reg::v0, 3),       // v0 = sign_extend(bits(f3))
	});
	h.Run();
	h.ExpectGpr64(reg::v0, 0x0000000040E00000ull);   // 7.0f bits, +ve sign
}

TEST(EeRecFpu, CEqAfterAddSReadsLiveOperand)
{
	// ADD_S writes f3 = 7; C_EQ_S f3, f4 (f4 = 7) -> CC should set.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetFpr(4, 7.0f);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),
		ee::C_EQ_S(3, 4),
	});
	h.Run();
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (1u << 23), 0u);
	EXPECT_NE(h.InterpSnapshot().fprs.fprc[31] & (1u << 23), 0u);
}

TEST(EeRecFpu, CltAfterAddSReadsLiveOperand)
{
	// ADD_S writes f3 = 7; C_LT_S f3, f4 (f4 = 10) -> CC should set.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.SetFpr(4, 10.0f);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),
		ee::C_LT_S(3, 4),
	});
	h.Run();
	EXPECT_NE(h.JitSnapshot().fprs.fprc[31] & (1u << 23), 0u);
	EXPECT_NE(h.InterpSnapshot().fprs.fprc[31] & (1u << 23), 0u);
}

TEST(EeRecFpu, CvtWAfterAddSReadsLiveSource)
{
	// ADD_S writes f3 = 7.0; CVT_W_S f4 = (int)f3 = 7.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 3.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),
		ee::CVT_W_S(4, 3),
	});
	h.Run();
	h.ExpectFpr(4, 7u);  // int bits, not float bits
}

TEST(EeRecFpu, DivSAfterAddSReadsLiveOperands)
{
	// ADD_S writes f3 = 20.0; DIV_S f4 = f3 / f1 = 20 / 4 = 5.
	// DIV_S is natively emitted via eeFPURecompileCode (recDIV_S_xmm), which
	// must read the allocator-live operands rather than stale memory.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFpr(1, 16.0f);
	h.SetFpr(2, 4.0f);
	h.LoadProgram({
		ee::ADD_S(3, 1, 2),       // f3 = 20
		ee::DIV_S(4, 3, 2),       // f4 = f3 / f2 = 5
	});
	h.Run();
	h.ExpectFpr(4, FloatBits(5.0f));
}

// ---- FpuMulHack (Tales of Destiny Remake gamefix) --------------------------
// JIT-only: the interpreter MUL_S has no hack, so a hack-hit legitimately
// diverges from interp — assert GetFprBitsJit() under RunJitNoDiff(). The hack
// patches exactly 0.25 * (π) (0x3e800000 * 0x40490fdb) to 0x3f490fda. The
// shared emitFpuMul helper wires it into all of MUL/MULA/MADD/MSUB/MADDA/MSUBA.

TEST(EeRecFpu, MulSFpuMulHackPatchesMagicProduct)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuMulHack();
	h.SetFprBits(0, 0x3e800000u);
	h.SetFprBits(1, 0x40490fdbu);
	h.LoadProgram({ee::MUL_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), 0x3f490fdau);
}

TEST(EeRecFpu, MulSFpuMulHackOffGivesNativeProduct)
{
	// Gamefix off (default): the same operands produce the ordinary product,
	// which JIT and interp agree on (Run diff), and which is NOT the patch value.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(0, 0x3e800000u);
	h.SetFprBits(1, 0x40490fdbu);
	h.LoadProgram({ee::MUL_S(2, 0, 1)});
	h.Run();
	EXPECT_NE(h.GetFprBitsJit(2), 0x3f490fdau);
}

TEST(EeRecFpu, MaddSFpuMulHackAppliesToProduct)
{
	// MADD routes its multiply through the same helper: ACC=0 + hack(Fs*Ft) ->
	// the patched product. Proves the family-wide wiring, not just MUL_S.
	EeRecTestHarness h;
	h.EnableCop1();
	h.EnableFpuMulHack();
	h.SetAccBits(0x00000000u); // +0
	h.SetFprBits(0, 0x3e800000u);
	h.SetFprBits(1, 0x40490fdbu);
	h.LoadProgram({ee::MADD_S(2, 0, 1)});
	h.RunJitNoDiff();
	EXPECT_EQ(h.GetFprBitsJit(2), 0x3f490fdau);
}

// ---- GE-11: MTC1 allocates a used destination FPR slot ---------------------
// MTC1 followed by consumers of fs must produce the same results whether fs
// went through the newly-allocated write-only slot (used-dest) or memory
// (unused dest). Chains through CVT.S and ADD.S pin the residency handoff.

TEST(EeRecFpu, Mtc1ThenCvtSThenAddSUsesResidentSlot)
{
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetGpr64(reg::a0, 7);          // int bits for CVT.S
	h.SetFprBits(5, 0x3F800000u);    // f5 = 1.0f
	h.LoadProgram({
		ee::MTC1(reg::a0, 1),        // f1 = raw 7 (int bits) — dest USED below
		ee::CVT_S_W(2, 1),           // f2 = 7.0f
		ee::ADD_S(3, 2, 5),          // f3 = 8.0f
		ee::MFC1(reg::v0, 3),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0) & 0xFFFFFFFFull, 0x41000000ull); // 8.0f
}

TEST(EeRecFpu, Mtc1UnusedDestStoresToMemory)
{
	// fs never touched again in-block: the alloc-if-used gate must decline and
	// the value must still land in fpr memory for the post-state diff.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetGpr64(reg::a0, 0xDEADBEEFull);
	h.LoadProgram({
		ee::MTC1(reg::a0, 4),
	});
	h.Run();
	EXPECT_EQ(h.GetFprBitsInterp(4), 0xDEADBEEFu);
}

TEST(EeRecFpu, Mtc1ConstSourceAllocatesUsedDest)
{
	// Const-propagated rt (LUI) through the same alloc-if-used path.
	EeRecTestHarness h;
	h.EnableCop1();
	h.SetFprBits(5, 0x40000000u);    // f5 = 2.0f
	h.LoadProgram({
		LUI(reg::a1, 0x4040),        // a1 = 0x40400000 = 3.0f bits (const)
		ee::MTC1(reg::a1, 1),        // f1 = 3.0f via const path
		ee::ADD_S(2, 1, 5),          // f2 = 5.0f
		ee::MFC1(reg::v0, 2),
	});
	h.Run();
	EXPECT_EQ(h.GetGpr64Interp(reg::v0) & 0xFFFFFFFFull, 0x40A00000ull); // 5.0f
}
