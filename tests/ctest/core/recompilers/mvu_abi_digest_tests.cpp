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
};

struct AbiPin
{
	u32 abi;
	DigestSet digests;
};

// === THE PIN TABLE === (see header comment for the update protocol)
constexpr AbiPin kPins[] = {
	{3, {0x4c3b6e1330199619, 0xd6f530cc13f0d0aa, 0xfcead342cc0b7df8}},
};

u64 CompileAndDigest(std::initializer_list<vu::VuOp> pairs)
{
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

	mVUPersist::SetRecordingEnabled(false);

	ASSERT_NE(actual.straightLine, 0u);
	ASSERT_NE(actual.branchBothArms, 0u);
	ASSERT_NE(actual.indirectJump, 0u);

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
		<< ", 0x" << actual.indirectJump << "}";

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
}

} // namespace recompiler_tests
