// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Persisted-JIT VU program cache — in-process round-trip gate.
//
// The contract under test (see microVU_Persist-arm64.h):
//
//   compile (recording on) → serialize → mVUreset → hydrate → re-run
//
//   (a) the hydrated run produces a bit-identical post-VU-state to the
//       freshly-compiled run from the same pre-state;
//   (b) the hydrated code bytes equal the recorded chunk bytes everywhere
//       outside the fixup spans (relocation operands are the ONLY thing
//       the patcher may touch);
//   (c) the hydrated run performs ZERO block compiles;
//   (d) a corrupted image (hash, layout bases, fixup table, truncation) is
//       rejected before any side effect — fail-safe means recompile, never
//       run wrong code.
//
// All tests drive VU0: its compile + execution stay on the test thread
// (VU1 routes through the MTVU thread, which TestHydrate must not cross).

#include "harness/VuTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include "VU.h"
#include "VUmicro.h"
#include "arm64/microVU_Persist-arm64.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

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

// Serialized-image header offsets (pinned by mVUPersist::ImageHeader v1;
// the corruption tests poke these fields directly).
constexpr size_t kOffHashLo = 16;
constexpr size_t kOffImageAnchor = 32;
constexpr size_t kOffChunkCount = 68;
constexpr size_t kHeaderSize = 88;

class MvuPersistRoundTrip : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_TRUE(RecompilerTestEnvironment::IsReady());
		mVUPersist::SetRecordingEnabled(true);
	}

	void TearDown() override
	{
		mVUPersist::SetRecordingEnabled(false);
		// Drop any recorded program so recording state can't leak into
		// later (non-persist) tests through a surviving block cache.
		RecompilerTestEnvironment::ResetVuBlockCache(0);
	}

	// Compile `h`'s loaded program fresh with recording enabled and leave it
	// LIVE in the VU0 program cache (VuTestHarness::Run's interp pass resets
	// the block cache, deleting the JIT-pass program — so after the
	// correctness diff we re-run the JIT side alone, which recompiles into a
	// surviving program).
	void RunAndKeepProgram(VuTestHarness& h)
	{
		h.Run(); // JIT-vs-interp correctness diff (program gets reset away)
		h.RunJitPreserveBlockCache(); // fresh compile, program survives
	}

	// Full round-trip with gates (a)-(c); leaves the hydrated program live.
	// `pairs` is the same program handed to h.LoadProgram by the caller —
	// the padding step below clobbers VU Micro, so it must be re-written
	// before hydration (the content hash is computed over live Micro).
	// `out_image` (optional) receives the serialized image so callers can
	// pin format facts (e.g. chunk counts). (void return: ASSERT_* needs it.)
	void RoundTrip(VuTestHarness& h, std::initializer_list<vu::VuOp> pairs,
		std::vector<u8>* out_image = nullptr)
	{
		RunAndKeepProgram(h);
		const VuSnapshot fresh_jit = h.JitSnapshot();

		std::vector<u8> image;
		ASSERT_TRUE(mVUPersist::TestSerializeNewestProgram(0, image))
			<< "program was not recorded — recorder dropped the episode?";
		ASSERT_GT(image.size(), kHeaderSize);

		// Wipe every compiled block + program, then occupy the front of the
		// code cache with an unrelated program so the hydrated chunks land at
		// a DIFFERENT slab offset than they were recorded at. Without this
		// shift the deterministic cursor would replay every chunk at its
		// original address and an unpatched (broken) fixup would still pass.
		{
			VuTestHarness pad(0);
			pad.SetVf(7, 9.0f, 9.0f, 9.0f, 9.0f);
			pad.LoadProgram({
				UpperOnly(VMAX_U(mask::xyzw, vf::vf8, vf::vf7, vf::vf7)),
				UpperOnly(bits::E | VMINI_U(mask::xyzw, vf::vf9, vf::vf7, vf::vf8)),
			});
			pad.Run();
			pad.RunJitPreserveBlockCache(); // pad program now holds the cursor base
		}

		// Restore the main program's bytes in VU Micro (the pad harness
		// zeroed and rewrote it). LoadProgram memcpys into Micro directly,
		// bypassing the vtlb write path that calls mVUclear in production —
		// without the explicit Clear, the stale quick slot would dispatch
		// the PAD program for startPC 0 instead of re-resolving by hash.
		h.LoadProgram(pairs);
		CpuMicroVU0.Clear(0, VU0_PROGSIZE);

		const u64 compiles_before = mVUPersist::GetBlockCompileCount(0);
		ASSERT_TRUE(mVUPersist::TestHydrate(0, image.data(), image.size()));
		EXPECT_EQ(mVUPersist::GetBlockCompileCount(0), compiles_before)
			<< "hydration itself must not compile";

		// Gate (b): live code == recorded code modulo fixup operands.
		EXPECT_TRUE(mVUPersist::TestVerifyHydratedCode(0, image.data(), image.size()));

		// Gate (c): the hydrated run must not compile anything.
		h.RunJitPreserveBlockCache();
		EXPECT_EQ(mVUPersist::GetBlockCompileCount(0), compiles_before)
			<< "hydrated program recompiled at dispatch — block graph not resolved";

		// Gate (a): bit-identical post-state vs the fresh compile.
		const auto diffs = DiffVu(fresh_jit, h.JitSnapshot(), VuDiffMode::Strict);
		EXPECT_TRUE(diffs.empty()) << [&] {
			std::string s = "hydrated JIT diverged from fresh JIT:\n";
			for (const auto& d : diffs)
				s += "  " + d + "\n";
			return s;
		}();

		if (out_image)
			*out_image = std::move(image);
	}
};

} // namespace

//------------------------------------------------------------------
// Round trips
//------------------------------------------------------------------

TEST_F(MvuPersistRoundTrip, StraightLineProgram)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.5f, -2.25f, 3.0f, 0.0625f);
	h.SetVf(2, 4.0f, 0.5f, -1.0f, 8.0f);
	const std::initializer_list<vu::VuOp> pairs = {
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
		UpperOnly(VMUL_U(mask::xyzw, vf::vf4, vf::vf3, vf::vf2)),
		UpperOnly(bits::E | VSUB_U(mask::xyzw, vf::vf5, vf::vf4, vf::vf1)),
	};
	h.LoadProgram(pairs);
	RoundTrip(h, pairs);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 5.5f);

	const auto stats = mVUPersist::GetStats(0);
	EXPECT_GE(stats.chunksRecorded, 1u);
	EXPECT_GE(stats.blocksRecorded, 1u);
	EXPECT_GE(stats.programsHydrated, 1u);
}

TEST_F(MvuPersistRoundTrip, BranchBothArms)
{
	// IBNE with both arms compiled eagerly (condBranch compiles the
	// not-taken block inline and resolves the taken target via
	// mVUblockFetch) — multi-block, single-chunk program.
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.SetVf(2, 0.5f, 0.5f, 0.5f, 0.5f);
	h.SetVi(1, 1); // branch taken
	const std::initializer_list<vu::VuOp> pairs = {
		LowerOnly(VIBNE_L(vi::vi1, vi::vi0, 3)), // pair 0 → taken target pair 4
		UpperOnly(VADD_U(mask::xyzw, vf::vf4, vf::vf1, vf::vf2)), // pair 1: delay slot
		UpperOnly(bits::E | VSUB_U(mask::xyzw, vf::vf5, vf::vf1, vf::vf2)), // pair 2: not-taken exit
		NopPair(), // pair 3: not-taken E-bit delay slot
		UpperOnly(bits::E | VMUL_U(mask::xyzw, vf::vf6, vf::vf1, vf::vf2)), // pair 4: taken exit
	};
	h.LoadProgram(pairs);
	RoundTrip(h, pairs);
	EXPECT_FLOAT_EQ(h.GetVfJit(6, 'x'), 0.5f);

	const auto stats = mVUPersist::GetStats(0);
	EXPECT_GE(stats.chunksRecorded, 1u);
	EXPECT_GE(stats.blocksRecorded, 2u);
	EXPECT_GE(stats.fixupsRecorded, 1u);
	EXPECT_GE(stats.programsHydrated, 1u);
}

TEST_F(MvuPersistRoundTrip, IndirectJumpTwoChunks)
{
	// JR compiles the jump target in a SECOND emission episode (the runtime
	// mVUcompileJIT path) — exercises multi-chunk programs and the
	// jumpCache restore (mVUcompileJIT dereferences it unguarded).
	VuTestHarness h(0);
	h.SetVf(1, 2.0f, 4.0f, 8.0f, 16.0f);
	h.SetVf(2, 1.0f, 1.0f, 1.0f, 1.0f);
	const std::initializer_list<vu::VuOp> pairs = {
		LowerOnly(VIADDIU_L(vi::vi1, vi::vi0, 4)), // pair 0: vi1 = 4 (target pair)
		LowerOnly(VJR_L(vi::vi1)), // pair 1: JR vi1 → pair 4
		NopPair(), // pair 2: delay slot
		NopPair(), // pair 3: skipped
		UpperOnly(bits::E | VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)), // pair 4
	};
	h.LoadProgram(pairs);
	std::vector<u8> image;
	RoundTrip(h, pairs, &image);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 3.0f);
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'w'), 17.0f);

	// The JR target compiles in a SECOND emission episode (runtime
	// mVUcompileJIT) — pin that the image really carries two chunks, i.e.
	// the multi-chunk path (cross-chunk placement + per-chunk fixups) was
	// exercised and not silently collapsed into one episode.
	ASSERT_GE(image.size(), kHeaderSize);
	u32 chunk_count = 0;
	std::memcpy(&chunk_count, image.data() + kOffChunkCount, sizeof(chunk_count));
	EXPECT_GE(chunk_count, 2u);
}

//------------------------------------------------------------------
// Gate (d): fail-safe rejection
//------------------------------------------------------------------

namespace {

// Compile + serialize a minimal program and hand back the image.
std::vector<u8> MakeImage(VuTestHarness& h)
{
	h.Run();
	h.RunJitPreserveBlockCache();
	std::vector<u8> image;
	EXPECT_TRUE(mVUPersist::TestSerializeNewestProgram(0, image));
	RecompilerTestEnvironment::ResetVuBlockCache(0);
	return image;
}

} // namespace

TEST_F(MvuPersistRoundTrip, RejectsContentHashMismatch)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf1)),
		EBitNopPair(),
	});
	std::vector<u8> image = MakeImage(h);
	ASSERT_GT(image.size(), kHeaderSize);

	image[kOffHashLo] ^= 0xFF;
	EXPECT_FALSE(mVUPersist::TestHydrate(0, image.data(), image.size()));
}

TEST_F(MvuPersistRoundTrip, RejectsLayoutBaseMismatch)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf1)),
		EBitNopPair(),
	});
	std::vector<u8> image = MakeImage(h);

	// Pretend the image was produced by a process with a different
	// executable base (PIE dev box / layout drift).
	image[kOffImageAnchor + 5] ^= 0x01;
	EXPECT_FALSE(mVUPersist::TestHydrate(0, image.data(), image.size()));
}

TEST_F(MvuPersistRoundTrip, RejectsTruncatedImage)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf1)),
		EBitNopPair(),
	});
	std::vector<u8> image = MakeImage(h);

	EXPECT_FALSE(mVUPersist::TestHydrate(0, image.data(), image.size() - 8));
	EXPECT_FALSE(mVUPersist::TestHydrate(0, image.data(), kHeaderSize / 2));
}

TEST_F(MvuPersistRoundTrip, RejectsStructuralCorruption)
{
	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf1)),
		EBitNopPair(),
	});
	std::vector<u8> image = MakeImage(h);

	// Inflate chunkCount so the parser walks past the payload.
	u32 chunk_count = 0;
	std::memcpy(&chunk_count, image.data() + kOffChunkCount, sizeof(chunk_count));
	chunk_count += 7;
	std::memcpy(image.data() + kOffChunkCount, &chunk_count, sizeof(chunk_count));
	EXPECT_FALSE(mVUPersist::TestHydrate(0, image.data(), image.size()));
}

TEST_F(MvuPersistRoundTrip, NoLogWhenRecordingDisabled)
{
	mVUPersist::SetRecordingEnabled(false);

	VuTestHarness h(0);
	h.SetVf(1, 1.0f, 2.0f, 3.0f, 4.0f);
	h.LoadProgram({
		UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf1)),
		EBitNopPair(),
	});
	h.Run();
	h.RunJitPreserveBlockCache();

	std::vector<u8> image;
	EXPECT_FALSE(mVUPersist::TestSerializeNewestProgram(0, image))
		<< "programs compiled with recording off must have no persist log";
}

} // namespace recompiler_tests
