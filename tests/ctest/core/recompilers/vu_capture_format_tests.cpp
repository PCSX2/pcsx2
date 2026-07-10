// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Round-trip coverage for the on-disk VU capture format (vu_capture.h).
// The format is consumed by pcsx2-vurunner and VuTestHarness::LoadFromFile;
// breaking the layout would silently make every captured replay misbehave.
// These tests pin the magic / version / sizes / field round-trip so a
// drift surfaces here before it reaches the replay side.

#include "vu_capture.h"

#include "Config.h"
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
	rec.config.flags = vu_capture::kConfigValid;
	std::strncpy(rec.config.serial, "SLES-52568", sizeof(rec.config.serial));
	rec.config.disc_crc = rng();
	rec.config.gamefixes = rng();
	rec.config.speedhacks = rng();
	rec.config.vu_clamp = rng();
	rec.config.vu0_fpcr = rng();
	rec.config.vu1_fpcr = rng();
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
	EXPECT_EQ(0, std::memcmp(&a.config, &b.config, sizeof(a.config)));
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
	// Fixed expected size: 32 (header) + 44 (config, v2+) + 2 * PROGSIZE
	// (microcode + vumem) + sizeof(CapturedState). Pinning this catches
	// accidental padding.
	const auto path = MakeTempPath("size");
	const auto rec = MakeRandomRecord(1, 7);
	ASSERT_TRUE(vu_capture::WriteToFile(path, rec));
	const auto sz = std::filesystem::file_size(path);
	EXPECT_EQ(sz, 32u + sizeof(vu_capture::CapturedConfig) + 2u * VU1_PROGSIZE +
					  sizeof(vu_capture::CapturedState));
	std::filesystem::remove(path);
}

TEST(VuCaptureFormat, CapturedConfigIsExactly44Bytes)
{
	EXPECT_EQ(44u, sizeof(vu_capture::CapturedConfig));
}

TEST(VuCaptureFormat, ReadAcceptsVersion1Legacy)
{
	// v1 layout: [FileHeader][microcode][vumem][CapturedState] — no
	// CapturedConfig block. The entire pre-2026-07 corpus (5818 caps / 25
	// games) is v1; the reader must keep accepting it forever, yielding a
	// zeroed (not-valid) config so replayers fall back to the pinned
	// harness baseline.
	const auto path = MakeTempPath("v1legacy");
	const auto rec = MakeRandomRecord(1, 0x1E6ACC);

	vu_capture::FileHeader hdr{};
	std::memcpy(hdr.magic, vu_capture::kMagic, sizeof(hdr.magic));
	hdr.version = 1;
	hdr.vu_index = rec.vu_index;
	hdr.start_pc = rec.start_pc;
	hdr.cycle_budget = rec.cycle_budget;
	hdr.microcode_size = static_cast<u32>(rec.microcode.size());
	hdr.vumem_size = static_cast<u32>(rec.vumem.size());

	std::FILE* f = std::fopen(path.c_str(), "wb");
	ASSERT_TRUE(f != nullptr);
	ASSERT_EQ(1u, std::fwrite(&hdr, sizeof(hdr), 1, f));
	ASSERT_EQ(rec.microcode.size(), std::fwrite(rec.microcode.data(), 1, rec.microcode.size(), f));
	ASSERT_EQ(rec.vumem.size(), std::fwrite(rec.vumem.data(), 1, rec.vumem.size(), f));
	ASSERT_EQ(1u, std::fwrite(&rec.state, sizeof(rec.state), 1, f));
	std::fclose(f);

	vu_capture::CaptureRecord read_back;
	ASSERT_TRUE(vu_capture::ReadFromFile(path, read_back));
	EXPECT_EQ(rec.start_pc, read_back.start_pc);
	EXPECT_EQ(0, std::memcmp(rec.microcode.data(), read_back.microcode.data(), rec.microcode.size()));
	EXPECT_EQ(0, std::memcmp(&rec.state, &read_back.state, sizeof(rec.state)));
	EXPECT_EQ(0u, read_back.config.flags & vu_capture::kConfigValid);
	const vu_capture::CapturedConfig zero{};
	EXPECT_EQ(0, std::memcmp(&zero, &read_back.config, sizeof(zero)));

	std::filesystem::remove(path);
}

TEST(VuCaptureFormat, GamefixBitOrderIsFrozen)
{
	// CapturedConfig::gamefixes is bit-indexed by GamefixId, so the enum
	// order in Config.h is part of the on-disk format from v2 on. Reordering
	// or inserting mid-enum silently changes what every existing capture
	// means — append new fixes at the end and bump this pin.
	EXPECT_EQ(0, static_cast<int>(Fix_FpuMultiply));
	EXPECT_EQ(11, static_cast<int>(Fix_VuAddSub));
	EXPECT_EQ(12, static_cast<int>(Fix_Ibit));
	EXPECT_EQ(13, static_cast<int>(Fix_VUSync));
	EXPECT_EQ(14, static_cast<int>(Fix_VUOverflow));
	EXPECT_EQ(15, static_cast<int>(Fix_XGKick));
	EXPECT_EQ(18, static_cast<int>(GamefixId_COUNT));
}

TEST(VuCaptureFormat, SnapshotConfigCapturesLiveEmuConfig)
{
	// SnapshotConfig is what the live dispatcher probe records; its mapping
	// to EmuConfig is the write half of the game-faithful replay contract
	// (vurunner's ApplyReplayConfig is the read half).
	const Pcsx2Config::GamefixOptions saved_fixes = EmuConfig.Gamefixes;
	const Pcsx2Config::SpeedhackOptions saved_hacks = EmuConfig.Speedhacks;
	const Pcsx2Config::CpuOptions saved_cpu = EmuConfig.Cpu;

	EmuConfig.Gamefixes = Pcsx2Config::GamefixOptions();
	EmuConfig.Gamefixes.Set(Fix_XGKick, true);
	EmuConfig.Gamefixes.Set(Fix_VuAddSub, true);
	EmuConfig.Speedhacks.vuFlagHack = true;
	EmuConfig.Speedhacks.vuThread = false;
	EmuConfig.Speedhacks.vu1Instant = true;
	EmuConfig.Cpu.Recompiler.vu0Overflow = true;
	EmuConfig.Cpu.Recompiler.vu0ExtraOverflow = true;
	EmuConfig.Cpu.Recompiler.vu0SignOverflow = false;
	EmuConfig.Cpu.Recompiler.vu1Overflow = true;
	EmuConfig.Cpu.Recompiler.vu1ExtraOverflow = false;
	EmuConfig.Cpu.Recompiler.vu1SignOverflow = false;
	// Distinct per-VU FPCR attributes, set through the portable accessors
	// (the raw bitmask representation is arch-specific). FTZ and DAZ are set
	// together: aarch64 aliases both onto FPCR.FZ, so mixed values are not
	// representable there.
	EmuConfig.Cpu.VU0FPCR.SetRoundMode(FPRoundMode::ChopZero);
	EmuConfig.Cpu.VU0FPCR.SetFlushToZero(true);
	EmuConfig.Cpu.VU0FPCR.SetDenormalsAreZero(true);
	EmuConfig.Cpu.VU1FPCR.SetRoundMode(FPRoundMode::NegativeInfinity);
	EmuConfig.Cpu.VU1FPCR.SetFlushToZero(false);
	EmuConfig.Cpu.VU1FPCR.SetDenormalsAreZero(false);

	vu_capture::CapturedConfig cfg;
	vu_capture::SnapshotConfig(cfg);

	EXPECT_EQ(vu_capture::kConfigValid, cfg.flags & vu_capture::kConfigValid);
	EXPECT_EQ((1u << Fix_XGKick) | (1u << Fix_VuAddSub), cfg.gamefixes);
	EXPECT_EQ(vu_capture::kSpeedhackVuFlagHack | vu_capture::kSpeedhackVu1Instant,
		cfg.speedhacks);
	EXPECT_EQ(vu_capture::kClampVu0Overflow | vu_capture::kClampVu0ExtraOverflow |
				  vu_capture::kClampVu1Overflow,
		cfg.vu_clamp);
	// ChopZero(3) + FTZ + DAZ / NegativeInfinity(1) bare, portable encoding.
	EXPECT_EQ(3u | vu_capture::kFpcrFlushToZero | vu_capture::kFpcrDenormalsAreZero,
		cfg.vu0_fpcr);
	EXPECT_EQ(1u, cfg.vu1_fpcr);
	// No VM running in the test env — serial empty, crc zero, still valid.
	EXPECT_EQ('\0', cfg.serial[0]);
	EXPECT_EQ(0u, cfg.disc_crc);

	EmuConfig.Gamefixes = saved_fixes;
	EmuConfig.Speedhacks = saved_hacks;
	EmuConfig.Cpu = saved_cpu;
}
