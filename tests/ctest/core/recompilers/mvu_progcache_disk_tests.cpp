// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Persisted-JIT VU program cache — disk round-trip gate.
//
// The in-process serialize → hydrate path is covered via test hooks elsewhere.
// This file proves the PRODUCTION path end to end, through the real disk
// cache and the real dispatcher seam:
//
//   compile (recording on) → mVUreset → SaveAllPrograms writes INDEX +
//   .vuprog payload → next dispatch misses contentMap + per-PC deque →
//   mVUsearchProg calls mVUProgCache::TryLoadProgram → payload read +
//   HydrateProgram (episode closed/reopened around it) → program runs
//   with ZERO block compiles and a bit-identical post-state.
//
// Only process death separates this from the cross-process flow — the
// in-memory INDEX is rebuilt from disk by Init, and the payload bytes
// travel exclusively through the filesystem (the vurunner --cache-dir
// gate covers the true two-process case on corpus captures).
//
// Fail-safe gates: a corrupt payload (checksum), a missing payload file,
// and a telemetry-only INDEX entry (recording off when saved) must all
// degrade to a clean recompile.
//
// All tests drive VU0 — same thread-affinity reasoning as
// mvu_persist_roundtrip_tests.cpp.

#include "harness/VuTestHarness.h"
#include "harness/RecompilerTestEnvironment.h"

#include "Config.h"
#include "VU.h"
#include "VUmicro.h"
#include "arm64/microVU_Persist-arm64.h"
#include "arm64/microVU_ProgCache-arm64.h"

#include "common/FileSystem.h"
#include "common/Path.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

namespace recompiler_tests {

using namespace vu;

namespace {

inline VuOp UpperOnly(u32 upper)
{
	return IBit(VuOp{VLitZero(), upper});
}

void RecursiveRm(const std::string& dir)
{
	if (!FileSystem::DirectoryExists(dir.c_str()))
	{
		::unlink(dir.c_str());
		return;
	}
	FileSystem::FindResultsArray ents;
	FileSystem::FindFiles(dir.c_str(), "*",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES,
		&ents);
	for (const auto& e : ents)
	{
		if (e.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
			RecursiveRm(e.FileName);
		else
			::unlink(e.FileName.c_str());
	}
	::rmdir(dir.c_str());
}

// Collect every *.vuprog under `dir` (payloads live one shard level down).
void FindPayloads(const std::string& dir, std::vector<std::string>& out)
{
	if (!FileSystem::DirectoryExists(dir.c_str()))
		return;
	FileSystem::FindResultsArray ents;
	FileSystem::FindFiles(dir.c_str(), "*",
		FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES,
		&ents);
	for (const auto& e : ents)
	{
		if (e.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
			FindPayloads(e.FileName, out);
		else if (e.FileName.size() > 7 &&
			e.FileName.compare(e.FileName.size() - 7, 7, ".vuprog") == 0)
			out.push_back(e.FileName);
	}
}

class MvuProgCacheDisk : public ::testing::Test
{
protected:
	void SetUp() override
	{
		ASSERT_TRUE(RecompilerTestEnvironment::IsReady());
		// Recording is under manual test control (RecompilerTestEnvironment
		// engaged SetTestManualRecording), so mVUreset's SyncRecordingFromConfig
		// won't fight these tests — drive it directly.
		mVUPersist::SetRecordingEnabled(true);

		char tmpl[] = "/tmp/progcache_disk_XXXXXX";
		char* p = ::mkdtemp(tmpl);
		ASSERT_NE(p, nullptr);
		root_ = p;
		prev_cache_ = EmuFolders::Cache;
		EmuFolders::Cache = root_;
	}

	void TearDown() override
	{
		// Free live programs while the cache is still up (the reset-side
		// SaveAllPrograms writing into the doomed temp dir is harmless),
		// then disable the disk cache for everything that runs after us.
		RecompilerTestEnvironment::ResetVuBlockCache(0);
		mVUProgCache::ResetForTest(0);
		mVUPersist::SetRecordingEnabled(false);
		EmuConfig.Cpu.Recompiler.EnableVUProgramCache = false;
		EmuFolders::Cache = prev_cache_;
		RecursiveRm(root_);
	}

	// Bring the on-disk cache up at the temp root. Deferred out of SetUp so
	// tests can compile a genuine no-cache baseline first.
	void InitDiskCache()
	{
		mVUProgCache::ResetForTest(0);
		mVUProgCache::TestReinitFromLiveSentinel(0);
		ASSERT_TRUE(mVUProgCache::GetStats(0).enabled)
			<< "disk cache failed to init at " << root_;
	}

	std::string vu0_root() const
	{
		return Path::Combine(Path::Combine(root_, "vu_jit"), "vu0");
	}

	std::string root_;
	std::string prev_cache_;
};

// The shared three-op straight-line program. Kept identical across tests so
// each test's expectations stay easy to eyeball.
const std::initializer_list<vu::VuOp> kProgram = {
	UpperOnly(VADD_U(mask::xyzw, vf::vf3, vf::vf1, vf::vf2)),
	UpperOnly(VMUL_U(mask::xyzw, vf::vf4, vf::vf3, vf::vf2)),
	UpperOnly(bits::E | VSUB_U(mask::xyzw, vf::vf5, vf::vf4, vf::vf1)),
};

void SeedAndLoad(VuTestHarness& h)
{
	h.SetVf(1, 1.5f, -2.25f, 3.0f, 0.0625f);
	h.SetVf(2, 4.0f, 0.5f, -1.0f, 8.0f);
	h.LoadProgram(kProgram);
}

} // namespace

TEST_F(MvuProgCacheDisk, SaveThenHydrateAcrossReset)
{
	// 1. Genuine fresh compile, no disk cache in the loop yet.
	VuTestHarness h(0);
	SeedAndLoad(h);
	h.Run();
	h.RunJitPreserveBlockCache();
	const VuSnapshot fresh_jit = h.JitSnapshot();

	// 2. Cache up; reset writes INDEX + payload for the live program.
	InitDiskCache();
	RecompilerTestEnvironment::ResetVuBlockCache(0);

	auto cstats = mVUProgCache::GetStats(0);
	ASSERT_GE(cstats.payloadWrites, 1u) << "reset-side SaveAllPrograms wrote no payload";
	std::vector<std::string> payloads;
	FindPayloads(vu0_root(), payloads);
	ASSERT_EQ(payloads.size(), 1u);

	// 3. Next dispatch must hydrate from disk, not compile.
	h.LoadProgram(kProgram);
	CpuMicroVU0.Clear(0, VU0_PROGSIZE);
	const u64 compiles_before = mVUPersist::GetBlockCompileCount(0);
	h.RunJitPreserveBlockCache();
	EXPECT_EQ(mVUPersist::GetBlockCompileCount(0), compiles_before)
		<< "dispatch recompiled instead of hydrating from disk";

	cstats = mVUProgCache::GetStats(0);
	EXPECT_GE(cstats.payloadHits, 1u);
	EXPECT_EQ(cstats.payloadRejects, 0u);
	EXPECT_GE(mVUPersist::GetStats(0).programsHydrated, 1u);

	const auto diffs = DiffVu(fresh_jit, h.JitSnapshot(), VuDiffMode::Strict);
	EXPECT_TRUE(diffs.empty()) << [&] {
		std::string s = "disk-hydrated JIT diverged from fresh JIT:\n";
		for (const auto& d : diffs)
			s += "  " + d + "\n";
		return s;
	}();
	EXPECT_FLOAT_EQ(h.GetVfJit(3, 'x'), 5.5f);
}

TEST_F(MvuProgCacheDisk, CorruptPayloadFallsBackToRecompile)
{
	VuTestHarness h(0);
	SeedAndLoad(h);
	h.Run();
	h.RunJitPreserveBlockCache();
	const VuSnapshot fresh_jit = h.JitSnapshot();

	InitDiskCache();
	RecompilerTestEnvironment::ResetVuBlockCache(0);

	std::vector<std::string> payloads;
	FindPayloads(vu0_root(), payloads);
	ASSERT_EQ(payloads.size(), 1u);

	// Flip one byte past the image header — valid structure, wrong code.
	// Only the payload checksum can catch this.
	auto bytes = FileSystem::ReadBinaryFile(payloads[0].c_str());
	ASSERT_TRUE(bytes.has_value());
	ASSERT_GT(bytes->size(), 96u);
	(*bytes)[bytes->size() - 4] ^= 0xFF;
	ASSERT_TRUE(FileSystem::WriteBinaryFile(payloads[0].c_str(), bytes->data(), bytes->size()));

	h.LoadProgram(kProgram);
	CpuMicroVU0.Clear(0, VU0_PROGSIZE);
	const u64 compiles_before = mVUPersist::GetBlockCompileCount(0);
	h.RunJitPreserveBlockCache();

	EXPECT_GT(mVUPersist::GetBlockCompileCount(0), compiles_before)
		<< "corrupt payload should force a recompile";
	const auto cstats = mVUProgCache::GetStats(0);
	EXPECT_GE(cstats.payloadRejects, 1u);
	EXPECT_EQ(cstats.payloadHits, 0u);

	// Degrade, never corrupt: the recompiled run is still correct.
	const auto diffs = DiffVu(fresh_jit, h.JitSnapshot(), VuDiffMode::Strict);
	EXPECT_TRUE(diffs.empty());
}

TEST_F(MvuProgCacheDisk, MissingPayloadFallsBackToRecompile)
{
	VuTestHarness h(0);
	SeedAndLoad(h);
	h.Run();
	h.RunJitPreserveBlockCache();

	InitDiskCache();
	RecompilerTestEnvironment::ResetVuBlockCache(0);

	std::vector<std::string> payloads;
	FindPayloads(vu0_root(), payloads);
	ASSERT_EQ(payloads.size(), 1u);
	ASSERT_EQ(::unlink(payloads[0].c_str()), 0);

	h.LoadProgram(kProgram);
	CpuMicroVU0.Clear(0, VU0_PROGSIZE);
	const u64 compiles_before = mVUPersist::GetBlockCompileCount(0);
	h.RunJitPreserveBlockCache();

	EXPECT_GT(mVUPersist::GetBlockCompileCount(0), compiles_before);
	const auto cstats = mVUProgCache::GetStats(0);
	EXPECT_GE(cstats.payloadMissing, 1u);
	EXPECT_EQ(cstats.payloadHits, 0u);
}

TEST_F(MvuProgCacheDisk, RecordingDisabledSavesIndexOnly)
{
	// With recording off the INDEX keeps its telemetry role and no payload
	// appears on disk.
	mVUPersist::SetRecordingEnabled(false);

	VuTestHarness h(0);
	SeedAndLoad(h);
	h.Run();
	h.RunJitPreserveBlockCache();

	InitDiskCache();
	RecompilerTestEnvironment::ResetVuBlockCache(0);

	auto cstats = mVUProgCache::GetStats(0);
	EXPECT_GE(cstats.entries, 1u);
	EXPECT_EQ(cstats.payloadWrites, 0u);
	std::vector<std::string> payloads;
	FindPayloads(vu0_root(), payloads);
	EXPECT_TRUE(payloads.empty());

	// Dispatch hits the INDEX, finds no payload, recompiles.
	h.LoadProgram(kProgram);
	CpuMicroVU0.Clear(0, VU0_PROGSIZE);
	const u64 compiles_before = mVUPersist::GetBlockCompileCount(0);
	h.RunJitPreserveBlockCache();
	EXPECT_GT(mVUPersist::GetBlockCompileCount(0), compiles_before);
	cstats = mVUProgCache::GetStats(0);
	EXPECT_GE(cstats.payloadMissing, 1u);
}

TEST_F(MvuProgCacheDisk, PreloadServesHydrationWithoutDisk)
{
	// Init's background preload reads payload-bearing INDEX entries into RAM
	// so first-dispatch hydration never does file I/O on the EE/MTVU
	// threads. Proven by force: preload, then DELETE the .vuprog from disk —
	// hydration must still succeed, served from the preload map.
	VuTestHarness h(0);
	SeedAndLoad(h);
	h.Run();
	h.RunJitPreserveBlockCache();
	const VuSnapshot fresh_jit = h.JitSnapshot();

	InitDiskCache();
	RecompilerTestEnvironment::ResetVuBlockCache(0);
	ASSERT_GE(mVUProgCache::GetStats(0).payloadWrites, 1u);

	// Re-init from disk: the INDEX now carries a payload-bearing entry, so
	// this Init spawns the preload.
	mVUProgCache::ResetForTest(0);
	mVUProgCache::TestReinitFromLiveSentinel(0);
	mVUProgCache::TestWaitForPreload(0);
	auto cstats = mVUProgCache::GetStats(0);
	ASSERT_GE(cstats.preloadedPayloads, 1u)
		<< "Init did not preload the payload-bearing INDEX entry";
	ASSERT_GT(cstats.preloadedBytes, 0u);

	std::vector<std::string> payloads;
	FindPayloads(vu0_root(), payloads);
	ASSERT_EQ(payloads.size(), 1u);
	ASSERT_EQ(::unlink(payloads[0].c_str()), 0);

	h.LoadProgram(kProgram);
	CpuMicroVU0.Clear(0, VU0_PROGSIZE);
	const u64 compiles_before = mVUPersist::GetBlockCompileCount(0);
	h.RunJitPreserveBlockCache();
	EXPECT_EQ(mVUPersist::GetBlockCompileCount(0), compiles_before)
		<< "dispatch recompiled — preloaded payload was not served";

	cstats = mVUProgCache::GetStats(0);
	EXPECT_GE(cstats.preloadHits, 1u);
	EXPECT_GE(cstats.payloadHits, 1u);
	EXPECT_EQ(cstats.payloadMissing, 0u);
	EXPECT_EQ(cstats.preloadedBytes, 0u) << "consumed buffer was not released";

	const auto diffs = DiffVu(fresh_jit, h.JitSnapshot(), VuDiffMode::Strict);
	EXPECT_TRUE(diffs.empty());
}

TEST_F(MvuProgCacheDisk, RecordingStateJoinsOptionsSentinel)
{
	// Safety property: recording changes emitted code forms
	// (canonical movs, forced-long cond branches), so the recording state
	// MUST be part of program identity. A cache built with recording on
	// must never be served to a recording-off run (or vice versa) — the
	// sentinel divergence below is what makes the VERSION handshake evict
	// it and what keys contentHash lookups apart.
	XXH128_hash_t on{}, off{}, on_again{};
	ASSERT_TRUE(mVUProgCache::TestGetLiveSentinel(0, &on)); // SetUp: recording on

	mVUPersist::SetRecordingEnabled(false);
	ASSERT_TRUE(mVUProgCache::TestGetLiveSentinel(0, &off));

	mVUPersist::SetRecordingEnabled(true);
	ASSERT_TRUE(mVUProgCache::TestGetLiveSentinel(0, &on_again));

	EXPECT_TRUE(on.low64 != off.low64 || on.high64 != off.high64)
		<< "recording on/off produced the SAME options sentinel — the "
		   "recording bit is not mixed into program identity";
	EXPECT_EQ(on.low64, on_again.low64);
	EXPECT_EQ(on.high64, on_again.high64);
}

TEST_F(MvuProgCacheDisk, ConfigBoolGatesProductionInit)
{
	// The production seam: mVUreset must bring the disk cache up when (and
	// only when) EmuCore/CPU/Recompiler EnableVUProgramCache is set — even
	// though EmuFolders::Cache points at a perfectly writable directory.
	mVUProgCache::ResetForTest(0);

	ASSERT_FALSE(EmuConfig.Cpu.Recompiler.EnableVUProgramCache);
	RecompilerTestEnvironment::ResetVuBlockCache(0);
	EXPECT_FALSE(mVUProgCache::GetStats(0).initialized)
		<< "disk cache initialized with the config bool OFF";

	// As VMManager would after a settings toggle: flip the bool; the next
	// recompiler reset activates the cache.
	EmuConfig.Cpu.Recompiler.EnableVUProgramCache = true;
	RecompilerTestEnvironment::ResetVuBlockCache(0);
	const auto stats = mVUProgCache::GetStats(0);
	EXPECT_TRUE(stats.initialized)
		<< "reset with the config bool ON did not initialize the disk cache";
	EXPECT_TRUE(stats.enabled);
}

TEST_F(MvuProgCacheDisk, HydratedProgramResavesWithoutGrowth)
{
	// A hydrated program that runs and resets again must NOT rewrite its
	// payload (the rebuilt persist log serializes to the same bytes) —
	// pins the size-based growth signal that keeps reset-side saves cheap.
	VuTestHarness h(0);
	SeedAndLoad(h);
	h.Run();
	h.RunJitPreserveBlockCache();

	InitDiskCache();
	RecompilerTestEnvironment::ResetVuBlockCache(0);
	auto cstats = mVUProgCache::GetStats(0);
	ASSERT_EQ(cstats.payloadWrites, 1u);

	h.LoadProgram(kProgram);
	CpuMicroVU0.Clear(0, VU0_PROGSIZE);
	h.RunJitPreserveBlockCache(); // hydrates
	ASSERT_GE(mVUProgCache::GetStats(0).payloadHits, 1u);

	RecompilerTestEnvironment::ResetVuBlockCache(0); // saves again
	cstats = mVUProgCache::GetStats(0);
	EXPECT_EQ(cstats.payloadWrites, 1u)
		<< "unchanged hydrated program rewrote its payload on re-save";
}

} // namespace recompiler_tests
