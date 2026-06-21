// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Round-trip coverage for the on-disk VU capture format (vu_capture.h).
// The format is consumed by pcsx2-vurunner and VuTestHarness::LoadFromFile;
// breaking the layout would silently make every captured replay misbehave.
// These tests pin the magic / version / sizes / field round-trip so a
// drift surfaces here before it reaches the replay side.

#include "vu_capture.h"

#include "VUmicro.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

namespace
{

std::string MakeTempPath(const char* tag)
{
	std::filesystem::path p = std::filesystem::temp_directory_path() /
		(std::string("pcsx2-vucap-test-") + tag + "-" + std::to_string(::getpid()) + ".vucap");
	return p.string();
}

vu_capture::CaptureRecord MakeRandomRecord(u8 vu_index, u32 seed)
{
	const u32 size = vu_index ? VU1_PROGSIZE : VU0_PROGSIZE;
	vu_capture::CaptureRecord rec;
	rec.vu_index = vu_index;
	rec.start_pc = 0xDEAD0000u | seed;
	rec.cycle_budget = 4096 + seed;
	rec.microcode.resize(size);
	rec.vumem.resize(size);

	std::mt19937 rng(seed);
	for (u32 i = 0; i < size; ++i)
	{
		rec.microcode[i] = static_cast<u8>(rng());
		rec.vumem[i] = static_cast<u8>(rng());
	}
	for (auto& vf : rec.state.VF)
		for (auto& lane : vf)
			lane = rng();
	for (auto& vi : rec.state.VI)
		vi = rng();
	for (auto& a : rec.state.ACC)
		a = rng();
	rec.state.q = rng();
	rec.state.p = rng();
	rec.state.pending_q = rng();
	rec.state.pending_p = rng();
	for (auto& f : rec.state.micro_macflags)
		f = rng();
	for (auto& f : rec.state.micro_clipflags)
		f = rng();
	for (auto& f : rec.state.micro_statusflags)
		f = rng();
	rec.state.xgkickaddr = rng();
	rec.state.xgkickdiff = rng();
	rec.state.xgkicksizeremaining = rng();
	rec.state.xgkicklastcycle = (static_cast<u64>(rng()) << 32) | rng();
	rec.state.xgkickcyclecount = rng();
	rec.state.xgkickenable = rng();
	rec.state.xgkickendpacket = rng();
	return rec;
}

void ExpectEqual(const vu_capture::CaptureRecord& a, const vu_capture::CaptureRecord& b)
{
	EXPECT_EQ(a.vu_index, b.vu_index);
	EXPECT_EQ(a.start_pc, b.start_pc);
	EXPECT_EQ(a.cycle_budget, b.cycle_budget);
	ASSERT_EQ(a.microcode.size(), b.microcode.size());
	ASSERT_EQ(a.vumem.size(), b.vumem.size());
	EXPECT_EQ(0, std::memcmp(a.microcode.data(), b.microcode.data(), a.microcode.size()));
	EXPECT_EQ(0, std::memcmp(a.vumem.data(), b.vumem.data(), a.vumem.size()));
	EXPECT_EQ(0, std::memcmp(&a.state, &b.state, sizeof(a.state)));
}

} // namespace

TEST(VuCaptureFormat, FileHeaderIsExactly32Bytes)
{
	EXPECT_EQ(32u, sizeof(vu_capture::FileHeader));
}

TEST(VuCaptureFormat, MagicIsPCSX2VUC)
{
	EXPECT_EQ(0, std::memcmp(vu_capture::kMagic, "PCSX2VUC", 8));
}

TEST(VuCaptureFormat, RoundTripVu0)
{
	const auto path = MakeTempPath("vu0");
	const auto written = MakeRandomRecord(0, 0xC0FFEE);
	ASSERT_TRUE(vu_capture::WriteToFile(path, written));

	vu_capture::CaptureRecord read_back;
	ASSERT_TRUE(vu_capture::ReadFromFile(path, read_back));
	ExpectEqual(written, read_back);

	std::filesystem::remove(path);
}

TEST(VuCaptureFormat, RoundTripVu1)
{
	const auto path = MakeTempPath("vu1");
	const auto written = MakeRandomRecord(1, 0xBEEF);
	ASSERT_TRUE(vu_capture::WriteToFile(path, written));

	vu_capture::CaptureRecord read_back;
	ASSERT_TRUE(vu_capture::ReadFromFile(path, read_back));
	ExpectEqual(written, read_back);

	std::filesystem::remove(path);
}

TEST(VuCaptureFormat, ReadRejectsBadMagic)
{
	const auto path = MakeTempPath("badmagic");
	const auto rec = MakeRandomRecord(0, 1);
	ASSERT_TRUE(vu_capture::WriteToFile(path, rec));

	// Corrupt magic.
	std::FILE* f = std::fopen(path.c_str(), "r+b");
	ASSERT_TRUE(f != nullptr);
	std::fputc('X', f);
	std::fclose(f);

	vu_capture::CaptureRecord out;
	EXPECT_FALSE(vu_capture::ReadFromFile(path, out));

	std::filesystem::remove(path);
}

TEST(VuCaptureFormat, ReadRejectsBadVersion)
{
	const auto path = MakeTempPath("badver");
	const auto rec = MakeRandomRecord(0, 2);
	ASSERT_TRUE(vu_capture::WriteToFile(path, rec));

	// Bump the version byte at offset 8.
	std::FILE* f = std::fopen(path.c_str(), "r+b");
	ASSERT_TRUE(f != nullptr);
	std::fseek(f, 8, SEEK_SET);
	const u32 wrong_version = vu_capture::kVersion + 99;
	std::fwrite(&wrong_version, sizeof(wrong_version), 1, f);
	std::fclose(f);

	vu_capture::CaptureRecord out;
	EXPECT_FALSE(vu_capture::ReadFromFile(path, out));

	std::filesystem::remove(path);
}

TEST(VuCaptureFormat, WriteRejectsWrongSizedBuffers)
{
	vu_capture::CaptureRecord bad;
	bad.vu_index = 0;
	bad.microcode.resize(VU0_PROGSIZE - 1);
	bad.vumem.resize(VU0_MEMSIZE);
	EXPECT_FALSE(vu_capture::WriteToFile(MakeTempPath("badsize"), bad));
}

TEST(VuCaptureFormat, OnDiskByteSize)
{
	// Fixed expected size: 32 (header) + 2 * PROGSIZE (microcode + vumem)
	// + sizeof(CapturedState). Pinning this catches accidental padding.
	const auto path = MakeTempPath("size");
	const auto rec = MakeRandomRecord(1, 7);
	ASSERT_TRUE(vu_capture::WriteToFile(path, rec));
	const auto sz = std::filesystem::file_size(path);
	EXPECT_EQ(sz, 32u + 2u * VU1_PROGSIZE + sizeof(vu_capture::CapturedState));
	std::filesystem::remove(path);
}
