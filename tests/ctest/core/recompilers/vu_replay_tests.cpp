// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// End-to-end coverage for the VU capture-replay path: synthesize a tiny
// CaptureRecord (VADD pre-state in VF1/VF2, E-bit terminator) and replay
// it through ReplayCapture. Asserts the JIT produces the architecturally
// expected result *and* converges with the interpreter — i.e. the replay
// driver primes both engines from the captured state correctly.

#include "harness/VuReplay.h"
#include "harness/VuEncode.h"

#include "vu_capture.h"
#include "VU.h"
#include "VUmicro.h"

#include <cstring>
#include <filesystem>
#include <string>
#include <gtest/gtest.h>

namespace recompiler_tests {

using namespace vu;

namespace
{

// Pad a one-pair-plus-NOP-delay-slot program into a full ProgSize buffer so
// the CaptureRecord's microcode field passes the format size check.
std::vector<u8> BuildProgramBytes(int vu_index, std::initializer_list<VuOp> pairs)
{
	const u32 prog_size = vu_index ? VU1_PROGSIZE : VU0_PROGSIZE;
	std::vector<u8> bytes(prog_size, 0);
	size_t off = 0;
	for (const auto& p : pairs)
	{
		std::memcpy(bytes.data() + off + 0, &p.lower, 4);
		std::memcpy(bytes.data() + off + 4, &p.upper, 4);
		off += 8;
	}
	return bytes;
}

vu_capture::CaptureRecord MakeVaddProgram(int vu_index)
{
	vu_capture::CaptureRecord rec;
	rec.vu_index = static_cast<u8>(vu_index);
	rec.start_pc = 0;
	rec.cycle_budget = 4096;

	// Pair 0: VADD.xyzw vf3, vf1, vf2 (upper) + I-bit-skipped lower.
	const VuOp p0 = IBit(VuOp{VLitZero(), VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)});
	// Pair 1: E-bit NOP.
	const VuOp p1 = EBitNopPair();
	// Pair 2: NOP (architectural delay slot after E-bit).
	const VuOp p2 = NopPair();
	rec.microcode = BuildProgramBytes(vu_index, {p0, p1, p2});
	rec.vumem.assign(vu_index ? VU1_MEMSIZE : VU0_MEMSIZE, 0);

	// Seed pre-state: VF1 = (1, 2, 3, 4), VF2 = (10, 20, 30, 40); expect
	// VF3 = (11, 22, 33, 44) post-execution.
	const float vf1[4] = {1.0f, 2.0f, 3.0f, 4.0f};
	const float vf2[4] = {10.0f, 20.0f, 30.0f, 40.0f};
	for (int i = 0; i < 4; ++i)
	{
		std::memcpy(&rec.state.VF[1][i], &vf1[i], 4);
		std::memcpy(&rec.state.VF[2][i], &vf2[i], 4);
	}
	return rec;
}

float AsFloat(u32 bits) { float f; std::memcpy(&f, &bits, 4); return f; }

// Portable, collision-free temp path (mirrors vu_capture_format_tests.cpp).
// std::filesystem::temp_directory_path() honors TMPDIR on macOS — /tmp is not
// the conventional temp dir there — and the getpid() suffix keeps parallel
// ctest -j workers from racing on a shared filename.
std::string MakeTempPath(const char* tag)
{
	std::filesystem::path p = std::filesystem::temp_directory_path() /
		(std::string("pcsx2-vureplay-test-") + tag + "-" + std::to_string(::getpid()) + ".vucap");
	return p.string();
}

} // namespace

TEST(VuReplay, ReplayVu1VaddProducesExpectedAndDoesNotDiverge)
{
	const auto rec = MakeVaddProgram(1);
	const auto result = ReplayCapture(rec);

	ASSERT_TRUE(result.ok);
	EXPECT_FALSE(result.diverged) << [&] {
		std::string s;
		for (const auto& l : result.diff_lines) { s += "  "; s += l; s += "\n"; }
		return s;
	}();

	const auto& jit_vf3 = result.jit_snapshot.regs.VF[3];
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.x), 11.0f);
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.y), 22.0f);
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.z), 33.0f);
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.w), 44.0f);

	const auto& interp_vf3 = result.interp_snapshot.regs.VF[3];
	EXPECT_FLOAT_EQ(AsFloat(interp_vf3.i.x), 11.0f);
	EXPECT_FLOAT_EQ(AsFloat(interp_vf3.i.y), 22.0f);
	EXPECT_FLOAT_EQ(AsFloat(interp_vf3.i.z), 33.0f);
	EXPECT_FLOAT_EQ(AsFloat(interp_vf3.i.w), 44.0f);
}

TEST(VuReplay, ReplayVu0VaddProducesExpectedAndDoesNotDiverge)
{
	const auto rec = MakeVaddProgram(0);
	const auto result = ReplayCapture(rec);

	ASSERT_TRUE(result.ok);
	EXPECT_FALSE(result.diverged);

	const auto& jit_vf3 = result.jit_snapshot.regs.VF[3];
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.x), 11.0f);
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.y), 22.0f);
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.z), 33.0f);
	EXPECT_FLOAT_EQ(AsFloat(jit_vf3.i.w), 44.0f);
}

TEST(VuReplay, LoadAndReplayRoundTripsThroughDisk)
{
	const auto rec = MakeVaddProgram(1);
	const std::string path = MakeTempPath("roundtrip");
	ASSERT_TRUE(vu_capture::WriteToFile(path, rec));

	const auto result = LoadAndReplay(path);
	std::remove(path.c_str());

	ASSERT_TRUE(result.ok);
	EXPECT_FALSE(result.diverged);
	const auto& vf3 = result.jit_snapshot.regs.VF[3];
	EXPECT_FLOAT_EQ(AsFloat(vf3.i.x), 11.0f);
}

TEST(VuReplay, LoadAndReplayReturnsNotOkOnMissingFile)
{
	const auto result = LoadAndReplay(MakeTempPath("missing-never-created"));
	EXPECT_FALSE(result.ok);
	EXPECT_FALSE(result.diverged);
}

} // namespace recompiler_tests
