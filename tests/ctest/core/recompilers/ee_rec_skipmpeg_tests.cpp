// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Regression coverage for the EE recompiler's Skip-MPEG speedhack
// (skipMPEG_By_Pattern in recRecompile) — DT-01's sibling host hook B4.
//
// The IOP-side FMV decode path calls sceMpegIsEnd, which compiles to the exact
// 3-instruction leaf:
//
//     lw  reg, 0x40(a0)   ; load the "is end" flag pointer
//     jr  ra
//     lw  v0,  0(reg)     ; delay slot: v0 = *flag
//
// When CHECK_SKIPMPEGHACK is on, the EE rec recognizes that signature at a
// block boundary (block ends at startpc+12, middle word == `jr ra`, masked
// opcode/operand checks on the two lw's) and replaces the whole block with
// "v0 = 1; pc = ra" — telling the game the video already finished so the FMV is
// skipped. Several games (Katamari — our benchmark — among them) hard-depend on
// this to boot past unplayable/looping FMVs.
//
// This hook is x86-only upstream (pcsx2/x86/ix86-32/iR5900.cpp). It was absent
// from the tokyo-merged arm64 EE rec — the doRecompilation gate carried only
// `!recSkipTimeoutLoop(...)`, with 0 refs to skipMPEG/sceMpeg/0x40(a0) anywhere
// under pcsx2/arm64/ — so the SkipMPEGHack gamefix bit was a silent no-op on
// arm64 and the affected games hung at their FMVs. Restored per the Discord
// triage DT-03 (annotations/_discord-triage); dropped-host-hook class B4 in
// feedback_arm64_ee_recrecompile_dropped_host_hooks.
//
// The skip is JIT-only: the shared interpreter has no skipMPEG handling and
// executes the real lw/jr/lw. So the POSITIVE test runs JIT-only and asserts
// the hacked value directly (RunJitNoDiff + GetGpr64Jit), while the NEGATIVE
// near-miss (wrong offset → no patch) recompiles normally and is validated by
// the full jit-vs-interp auto-diff (Run()).

#include "harness/EeRecTestHarness.h"

#include "Config.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace {
constexpr u32 kPark    = RecompilerTestEnvironment::kParkingPc;
constexpr u32 kScratch = RecompilerTestEnvironment::kScratchAddr;

// The "is end" flag pointer the first lw fetches, and the flag word it points
// at. Chosen positive (no lw sign-extension surprises) and clearly != 1 so the
// natural (un-hacked) value is unambiguously distinct from the hack's v0 = 1.
constexpr u32 kFlagPtr  = kScratch + 0x80;
constexpr u32 kNaturalV0 = 0x0EADBEEFu;

// RAII toggle for the SkipMPEGHack gamefix (the CHECK_SKIPMPEGHACK gate).
struct ScopedSkipMpeg
{
	bool prev;
	explicit ScopedSkipMpeg(bool on) : prev(EmuConfig.Gamefixes.SkipMPEGHack) { EmuConfig.Gamefixes.SkipMPEGHack = on; }
	~ScopedSkipMpeg() { EmuConfig.Gamefixes.SkipMPEGHack = prev; }
};
} // namespace

// Exact sceMpegIsEnd signature with SkipMPEGHack ON. The hack must replace the
// block with v0 = 1 / pc = ra and skip the real loads entirely. Memory is
// seeded so the *natural* execution would yield kNaturalV0 — so v0 == 1 proves
// the hack fired (not an incidental match). ra parks immediately.
TEST(EeRecSkipMpeg, SceMpegIsEndPatternForcesV0One)
{
	ScopedSkipMpeg sm(true);

	EeRecTestHarness h;
	h.SetGpr64(reg::a0, kScratch);
	h.WriteU32(kScratch + 0x40, kFlagPtr); // *(a0+0x40) = flag pointer
	h.WriteU32(kFlagPtr, kNaturalV0);      // *flag = natural (un-hacked) v0
	h.SetGpr64(reg::v0, 0);                // start != 1 so the witness is meaningful
	h.SetGpr64(reg::ra, kPark);            // hack sets pc = ra → park

	h.LoadProgramNoTerm({
		LW(reg::t0, 0x40, reg::a0), // lw t0, 0x40(a0)
		JR(reg::ra),                // jr ra
		LW(reg::v0, 0, reg::t0),    // delay slot: lw v0, 0(t0)
	});
	h.RunJitNoDiff();

	// v0 = 1 (both halves: hack writes UL[0]=1, UL[1]=0). The natural path would
	// have left kNaturalV0 here.
	EXPECT_EQ(h.GetGpr64Jit(reg::v0), 1ull);
	EXPECT_NE(h.GetGpr64Jit(reg::v0), static_cast<u64>(kNaturalV0));
}

// Near-miss: identical shape but the first lw uses offset 0x44, not 0x40. The
// masked `(code & 0xffe0ffff) != 0x8c800040` check fails, so the block is NOT
// patched and recompiles as real lw/jr/lw. Both the JIT and interp then load
// the natural value — Run()'s auto-diff asserts they agree, and we pin that the
// value flowed through (== kNaturalV0, NOT the hack's 1). This is the
// false-positive guard: the hack must not fire on a non-matching block.
TEST(EeRecSkipMpeg, WrongOffsetIsNotPatched)
{
	ScopedSkipMpeg sm(true);

	EeRecTestHarness h;
	h.SetGpr64(reg::a0, kScratch);
	h.WriteU32(kScratch + 0x44, kFlagPtr); // first lw now reads a0+0x44
	h.WriteU32(kFlagPtr, kNaturalV0);
	h.SetGpr64(reg::v0, 0);
	h.SetGpr64(reg::ra, kPark);

	h.LoadProgramNoTerm({
		LW(reg::t0, 0x44, reg::a0), // lw t0, 0x44(a0)  <-- off-by-4, breaks the pattern
		JR(reg::ra),                // jr ra
		LW(reg::v0, 0, reg::t0),    // delay slot: lw v0, 0(t0)
	});
	h.Run();

	// Real loads ran on both sides: v0 holds the natural value, not the hack's 1.
	h.ExpectGpr64(reg::v0, static_cast<u64>(kNaturalV0));
}
