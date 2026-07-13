// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ABI-digest guard — the #1 corruption backstop for the persisted-JIT VU
// program cache.
//
// The on-disk cache trusts kMvuCompilerAbiVersion (mixed into the VERSION
// handshake) to mean "the emitters that produced a .vuprog are the emitters
// running now." An emitter change WITHOUT an ABI bump silently runs stale
// code shapes hydrated from disk — the payload checksum can't see it (the
// bytes match what was saved; it's the saver that changed). This test pins
// the emitted SHAPE per ABI version so that drift fails loudly at commit
// time instead.
//
// The digest (mVUPersist::TestComputeEmitDigest) masks every operand that
// may legitimately differ between correct emissions — 64-bit mov-chain
// immediates, B/BL displacements, ADRP pages, fixup address payloads — and
// hashes what remains: opcode selection, register allocation, instruction
// order, block/chunk/fixup structure. That is deterministic across runs,
// machines, and PIE/ASLR.
//
// WHEN THIS TEST GOES RED:
//   1. You changed mVU codegen (any microVU_*-arm64 emit path, AsmHelpers
//      canonical forms, the serializer layout): bump kMvuCompilerAbiVersion
//      in microVU-arm64.h (+ the mirror in mvu_progcache_versioning_tests),
//      then add the new {abi, digests} row below. The bump evicts every
//      stale on-disk cache — that's the point.
//   2. You changed a default config value that alters emitted forms (clamp
//      mode etc.): the options sentinel already evicts those caches; just
//      re-pin the digests here (no ABI bump needed).
//   Never "fix" this test by re-pinning without deciding which case you
//   are in.
//
// Recording is enabled during compilation because the cache only ever
// stores recording-enabled forms (canonical movs, forced-long cond
// branches) — those are the shapes worth pinning.

#include "harness/VuTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include "VU.h"
#include "VUmicro.h"
#include "Config.h"
#include "arm64/microVU_Persist-arm64.h"
#include "arm64/microVU_ProgCache-arm64.h"

#include <gtest/gtest.h>

#include <cinttypes>
#include <cstdio>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp UpperOnly(u32 upper)
{
	return IBit(VuOp{VLitZero(), upper});
}

inline VuOp LowerOnly(u32 lower)
{
	return VuOp{lower, VNOP_U()};
}

// One digest per probe program. The set mirrors the round-trip suite's
// coverage axes: FMAC straight-line (upper pipeline + clamp emitters),
// conditional branch both-arms (block linking, SelfBlockAbs fixups), and
// indirect jump (two emission episodes, jump-cache path, stub calls).
struct DigestSet
{
	u64 straightLine;
	u64 branchBothArms;
	u64 indirectJump;
	// Broadcast-FMAC transform chain — pins the lane-indexed FMUL fold shape
	// (AX-14, unconditional since ABI v6). 0 in a pin row means "probe did
	// not exist for that ABI"; the assert skips zero pins.
	u64 broadcastChain;
	// Conditional branch in a branch delay slot — pins the condEvilBranch
	// target-select emission (ported at ABI v7; MGS2 VU0 solver hang).
	u64 condEvilBranch;
};

struct AbiPin
{
	u32 abi;
	DigestSet digests;
};

// === THE PIN TABLE === (see header comment for the update protocol)
constexpr AbiPin kPins[] = {
	// abi 4: vi00 const-addr loadstore fold (6018936dc). The probes below use no
	// LQ/SQ/ILW/ISW, so the folded ops leave their emitted shape unchanged — the
	// digests are bit-identical to abi 3; the bump is to evict on-disk caches
	// recorded with the pre-fold loadstore shape.
	{4, {0x4c3b6e1330199619, 0xd6f530cc13f0d0aa, 0xfcead342cc0b7df8, 0, 0}},
	// abi 5: mVUclamp2 2-row sign-clamp bounds (AX-02). The probes run under
	// the default clamp config, where the sign-overflow path never emits —
	// digests are bit-identical to abi 4; the bump evicts on-disk caches
	// recorded with the old all-lane sign-clamp shape (the options sentinel
	// can't distinguish those: same config, different emitter).
	{5, {0x4c3b6e1330199619, 0xd6f530cc13f0d0aa, 0xfcead342cc0b7df8, 0, 0}},
	// abi 6: lane-indexed FMUL broadcast fold unconditional (AX-14). The three
	// original probes contain no broadcast ops, so their digests are
	// bit-identical to abi 5; the bump evicts caches recorded with the old
	// Dup-materialized broadcast shape, and the new broadcastChain probe pins
	// the folded emission from here on (harvested from the first,
	// deliberately red, run).
	{6, {0x4c3b6e1330199619, 0xd6f530cc13f0d0aa, 0xfcead342cc0b7df8, 0x44bd2acfb23dff74, 0}},
	// abi 7: condEvilBranch ported (conditional branch in a branch delay slot
	// emits the badBranch/evilBranch target-select sequence; MGS2 VU0 solver
	// hang). The four original probes contain no branch-in-delay-slot, so
	// their digests are bit-identical to abi 6; the bump evicts caches
	// recorded when that sequence emitted nothing, and the new condEvilBranch
	// probe pins the ported emission from here on (harvested from the first,
	// deliberately red, run).
	{7, {0x4c3b6e1330199619, 0xd6f530cc13f0d0aa, 0xfcead342cc0b7df8, 0x44bd2acfb23dff74, 0xd04db07f3eb1a343}},
	// abi 8: hot microVU scalars (divFlag/branch/VIbackup/VIxgkick/cycles/…)
	// moved adjacent to the flag block and addressed as [gprMVUFlag, #imm]
	// via mVUfieldMem instead of per-site absolute materialization. Every
	// probe that touches those fields changes shape, and pre-8 payloads
	// bake the old field addresses/offsets, so the bump must evict them.
	{8, {0xb35dd0237372d734, 0xc3c40fd5a5ec19c7, 0x23682664f86a2f8d, 0xbdfce8a7ecebe6a6, 0x45837d5d1d23009f}},
	// abi 9: IBcc condition carry (doBranchCondCarry) — the condition
	// computes into a pool temp and condBranch's tail Cmps it directly
	// instead of the Ldrsh reload. Only the branch-bearing probe moved;
	// the other four contain no conditional branch and are bit-identical
	// to abi 8.
	{9, {0xb35dd0237372d734, 0xb6dfab5c9a56d900, 0x23682664f86a2f8d, 0xbdfce8a7ecebe6a6, 0x45837d5d1d23009f}},
	// abi 10: inline jump-cache probe in normJumpCompile. The two
	// jump-bearing probes (indirectJump, condEvilBranch — its continuation
	// compiles a normal JR tail) change shape; the branch-only and
	// straight-line probes are bit-identical to abi 9.
	{10, {0xb35dd0237372d734, 0xb6dfab5c9a56d900, 0xc9abe2f224fb5710, 0xbdfce8a7ecebe6a6, 0x1fe80e2917de1c2d}},
};

u64 CompileAndDigest(std::initializer_list<vu::VuOp> pairs)
{
	// The ABI digest pins the emitted shape that lands in the on-disk cache in
	// PRODUCTION, where vuFlagHack defaults on (and the options sentinel keeps
	// flaghack-on/off caches separate). The recompiler test environment now pins
	// vuFlagHack off for JIT-vs-interp determinism, so force it back on here —
	// otherwise the pins would track the non-production flaghack-off shape and
	// drift with the harness default rather than with real emitter changes.
	const bool savedFlagHack = EmuConfig.Speedhacks.vuFlagHack;
	EmuConfig.Speedhacks.vuFlagHack = true;

	VuTestHarness h(0);
	h.SetVf(1, 1.5f, -2.25f, 3.0f, 0.0625f);
	h.SetVf(2, 4.0f, 0.5f, -1.0f, 8.0f);
	h.SetVi(1, 1);
	h.LoadProgram(pairs);
	h.Run();
	h.RunJitPreserveBlockCache();
	u64 digest = 0;
	EXPECT_TRUE(mVUPersist::TestComputeEmitDigest(0, digest));
	RecompilerTestEnvironment::ResetVuBlockCache(0);

	EmuConfig.Speedhacks.vuFlagHack = savedFlagHack;
	return digest;
}

} // namespace

TEST(MvuAbiDigest, EmittedShapePinnedPerAbiVersion)
{
	ASSERT_TRUE(RecompilerTestEnvironment::IsReady());
	mVUPersist::SetRecordingEnabled(true);

	DigestSet actual = {};
	actual.straightLine = CompileAndDigest({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		UpperOnly(VMUL_U(mask::xyzw, vf::vf4, vf::vf3, vf::vf2)),
		UpperOnly(bits::E | VSUB_U(mask::xyzw, vf::vf5, vf::vf4, vf::vf1)),
	});
	actual.branchBothArms = CompileAndDigest({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, 3)),
		UpperOnly(VADD_U(mask::xyzw, vf::vf4, vf::vf1, vf::vf2)),
		UpperOnly(bits::E | VSUB_U(mask::xyzw, vf::vf5, vf::vf1, vf::vf2)),
		NopPair(),
		UpperOnly(bits::E | VMUL_U(mask::xyzw, vf::vf6, vf::vf1, vf::vf2)),
	});
	actual.indirectJump = CompileAndDigest({
		LowerOnly(VIADDIU_L(vi::vi1, vi::vi0, 4)),
		LowerOnly(VJR_L(vi::vi1)),
		NopPair(),
		NopPair(),
		UpperOnly(bits::E | VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
	});
	actual.broadcastChain = CompileAndDigest({
		UpperOnly(VMULAx_U(mask::xyzw, vf::vf3, vf::vf2)),
		UpperOnly(VMADDAy_U(mask::xyzw, vf::vf4, vf::vf2)),
		UpperOnly(VMSUBAz_U(mask::xyzw, vf::vf5, vf::vf2)),
		UpperOnly(bits::E | VMADDw_U(mask::xyzw, vf::vf7, vf::vf6, vf::vf2)),
	});
	// Taken branch with a taken conditional branch in its delay slot. The
	// E-bit stays OUT of the evil continuation window (one plain op at #1's
	// target, then #2 lands on a common E-bit tail) — E-bit inside an evil
	// branch is not implemented on x86 either and diverges from the interp.
	actual.condEvilBranch = CompileAndDigest({
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, 2)),                 // pair 0 → pair 3
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, 3)),                 // pair 1 (delay slot) → pair 5
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)), // pair 2: skipped
		UpperOnly(VMUL_U(mask::xyzw, vf::vf4, vf::vf1, vf::vf2)), // pair 3: #1 target (1 op runs)
		UpperOnly(VSUB_U(mask::xyzw, vf::vf5, vf::vf1, vf::vf2)), // pair 4: skipped
		UpperOnly(bits::E | VADD_U(mask::xyzw, vf::vf6, vf::vf1, vf::vf2)), // pair 5: #2 target
	});

	mVUPersist::SetRecordingEnabled(false);

	ASSERT_NE(actual.straightLine, 0u);
	ASSERT_NE(actual.branchBothArms, 0u);
	ASSERT_NE(actual.indirectJump, 0u);
	ASSERT_NE(actual.broadcastChain, 0u);
	ASSERT_NE(actual.condEvilBranch, 0u);

	const u32 abi = mVUProgCache::GetCompilerAbiVersion();
	const AbiPin* pin = nullptr;
	for (const AbiPin& p : kPins)
	{
		if (p.abi == abi)
			pin = &p;
	}
	ASSERT_NE(pin, nullptr)
		<< "kMvuCompilerAbiVersion=" << abi << " has no digest pin — add a "
		<< "row to kPins with the values printed below.\n"
		<< "  actual: {0x" << std::hex << actual.straightLine
		<< ", 0x" << actual.branchBothArms
		<< ", 0x" << actual.indirectJump
		<< ", 0x" << actual.broadcastChain
		<< ", 0x" << actual.condEvilBranch << "}";

	const auto explain = [&](const char* which, u64 got, u64 want) {
		char buf[256];
		std::snprintf(buf, sizeof(buf),
			"%s digest drifted for ABI v%u: got 0x%016" PRIx64 ", pinned 0x%016" PRIx64 ".\n"
			"Emitted code shape changed — bump kMvuCompilerAbiVersion (emitter "
			"change) or re-pin (config-default change). See file header.",
			which, abi, got, want);
		return std::string(buf);
	};
	EXPECT_EQ(actual.straightLine, pin->digests.straightLine)
		<< explain("straightLine", actual.straightLine, pin->digests.straightLine);
	EXPECT_EQ(actual.branchBothArms, pin->digests.branchBothArms)
		<< explain("branchBothArms", actual.branchBothArms, pin->digests.branchBothArms);
	EXPECT_EQ(actual.indirectJump, pin->digests.indirectJump)
		<< explain("indirectJump", actual.indirectJump, pin->digests.indirectJump);
	if (pin->digests.broadcastChain != 0) // probe added at abi 6; older rows unpinned
	{
		EXPECT_EQ(actual.broadcastChain, pin->digests.broadcastChain)
			<< explain("broadcastChain", actual.broadcastChain, pin->digests.broadcastChain);
	}
	if (pin->digests.condEvilBranch != 0) // probe added at abi 7; older rows unpinned
	{
		EXPECT_EQ(actual.condEvilBranch, pin->digests.condEvilBranch)
			<< explain("condEvilBranch", actual.condEvilBranch, pin->digests.condEvilBranch);
	}
}

// A program's emitted shape must depend ONLY on the program — never on what
// compiled before it. mVUcompile passes endCount = whole-micro-memory size to
// mvuPreloadRegisters; the preload walks until it runs out of free registers,
// not until the block ends. mVUreset does NOT clear mVU.prog.IRinfo.info[], so
// an E-bit-terminated block whose preload over-runs its own end reads stale
// VF/VI usage left by a PRIOR compile and preloads registers the program never
// touches. The runtime result is identical (an unused reg load), but the
// emitted bytes drift with compile history — which corrupts the persisted-JIT
// ABI digest's "same emitter ⇒ same shape" contract. The mvuPreloadRegisters
// isEOB break is the fix; this is its deterministic regression guard (the main
// pin test only catches it under --gtest_shuffle, which CI may not run).
TEST(MvuAbiDigest, EmittedShapeIndependentOfPriorCompile)
{
	ASSERT_TRUE(RecompilerTestEnvironment::IsReady());
	mVUPersist::SetRecordingEnabled(true);

	// The probe: a short pure-FMAC, E-bit-terminated block (no branch — so the
	// only thing that can bound its preload is the isEOB break).
	const auto probe = {
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		UpperOnly(bits::E | VMUL_U(mask::xyzw, vf::vf4, vf::vf3, vf::vf2)),
	};

	// Two "polluter" programs, longer than the probe, that leave DIFFERENT VI
	// read-usage in IRinfo.info[] at indices PAST the probe's own end. The probe
	// (2 ops + E-bit delay) clears info[0..~3]; the leading NOPs push the
	// VI-reading VIADDs out to indices >= 4 so they survive the probe's analysis.
	// VIADD reads two source VIs. If the probe's preload over-runs its block end,
	// it picks up these (differing) VIs and the two digests diverge.
	const auto polluteViLow = {
		NopPair(), NopPair(), NopPair(), NopPair(),
		LowerOnly(VIADD_L(vi::vi3, vi::vi5, vi::vi6)),
		LowerOnly(VIADD_L(vi::vi3, vi::vi5, vi::vi6)),
		LowerOnly(VIADD_L(vi::vi3, vi::vi5, vi::vi6)),
		UpperOnly(bits::E | VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
	};
	const auto polluteViHigh = {
		NopPair(), NopPair(), NopPair(), NopPair(),
		LowerOnly(VIADD_L(vi::vi3, vi::vi9, vi::vi10)),
		LowerOnly(VIADD_L(vi::vi3, vi::vi9, vi::vi10)),
		LowerOnly(VIADD_L(vi::vi3, vi::vi9, vi::vi10)),
		UpperOnly(bits::E | VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
	};

	(void)CompileAndDigest(polluteViLow);
	const u64 afterLow = CompileAndDigest(probe);
	(void)CompileAndDigest(polluteViHigh);
	const u64 afterHigh = CompileAndDigest(probe);

	mVUPersist::SetRecordingEnabled(false);

	ASSERT_NE(afterLow, 0u);
	EXPECT_EQ(afterLow, afterHigh)
		<< "Probe digest changed with the preceding compile (0x" << std::hex
		<< afterLow << " after VIADD vi5,vi6 vs 0x" << afterHigh
		<< " after VIADD vi9,vi10) — mvuPreloadRegisters over-ran the block end "
		   "into stale IRinfo.info[]. Emitted shape must be history-independent.";
}

} // namespace recompiler_tests
