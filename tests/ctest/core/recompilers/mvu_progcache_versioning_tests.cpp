// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// mVUProgCache VERSION handshake + stale-cache eviction + INDEX format.
//
// mVUProgCache::Init writes a VersionHeader to $root/VERSION on first
// boot. On every subsequent boot it reads back the on-disk header and
// compares against the live build's (kProgCacheFormatVersion,
// kMvuCompilerAbiVersion, optionsSentinel, vuIndex, archTag). Any
// mismatch atomically renames $root → $root.stale.<ns> and starts
// fresh.
//
// What this proves:
//
//   1. First Init on an empty cache dir writes a VERSION + creates
//      $root.
//   2. Re-init with the same options sentinel keeps the cache alive
//      (staleEvictions stays at 0).
//   3. Re-init with a different optionsSentinel triggers an eviction
//      (staleEvictions ticks; $root.stale.* exists; INDEX entries
//      from the prior run aren't visible).
//   4. A manually-poisoned VERSION (bad magic / bad formatVersion /
//      bad compilerAbiVersion / bad archTag) triggers the same
//      eviction path — defends the format-bump invariant.
//
// References:
//   pcsx2/arm64/microVU_ProgCache-arm64.inl  VersionHeader, VersionMatches,
//                                            EvictStaleCache, Init
//   pcsx2/arm64/microVU-arm64.h             kMvuCompilerAbiVersion
//   pcsx2/arm64/microVU_ProgCache-arm64.h    kProgCacheFormatVersion, ResetForTest

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Config.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include "arm64/microVU_ProgCache-arm64.h"

// kMvuCompilerAbiVersion lives in microVU-arm64.h, but that header
// also #includes the .inl files at the bottom — including it from
// a test TU collides with libpcsx2.a's emitted bodies. Mirror the
// constant here; a future bump must keep the two in sync. Drift is
// caught directly by `mirror_matches_production_abi_version` below
// (which compares this against the linkable accessor
// mVUProgCache::GetCompilerAbiVersion()), with a clear message —
// rather than only surfacing as a confusing eviction in the
// round-trip tests.
namespace pcsx2_test
{
	static constexpr u32 kMvuCompilerAbiVersionMirror = 10;
}

namespace
{
	// Mirrors the anonymous-namespace layout in microVU_ProgCache-arm64.inl.
	// Test-side copy so we can poison the on-disk VERSION file without
	// adding a backdoor to production. If the production layout drifts,
	// the static_assert below will fail-loudly at compile time.
#pragma pack(push, 1)
	struct VersionHeaderMirror
	{
		u32           magic;
		u32           formatVersion;
		u32           compilerAbiVersion;
		u32           vuIndex;
		XXH128_hash_t optionsSentinel;
		char          archTag[32];
	};
#pragma pack(pop)
	static_assert(sizeof(VersionHeaderMirror) == 64,
		"VersionHeader test mirror drifted from production layout");

	constexpr u32 kVersionMagicMirror = 0x4A56554Du; // 'MVUJ'

	// RAII: make a fresh tmpdir at EmuFolders::Cache and clean it up
	// on destruction. Most tests do not actually exercise file ops, so
	// using mkdtemp + nftw isn't worth pulling in — the tests below
	// only create a small fixed set of files which we list explicitly.
	class TempCacheRoot
	{
	public:
		TempCacheRoot()
		{
			char tmpl[] = "/tmp/progcache_ver_XXXXXX";
			char* p = ::mkdtemp(tmpl);
			EXPECT_NE(p, nullptr) << "mkdtemp failed: " << std::strerror(errno);
			m_root = p ? p : std::string();
			m_prev_cache = EmuFolders::Cache;
			EmuFolders::Cache = m_root;
		}
		~TempCacheRoot()
		{
			EmuFolders::Cache = m_prev_cache;
			// If mkdtemp failed the ctor left m_root empty (EXPECT_NE already
			// flagged it). Don't proceed: Path::Combine("", "vu_jit/...")
			// yields absolute /vu_jit paths, so cleanup would walk and
			// RecursiveRm system-root locations. Bail out instead.
			if (m_root.empty())
				return;
			// Wipe everything we know we might have created under m_root.
			// Order matters: deepest first.
			std::vector<std::string> dirs_to_try;
			dirs_to_try.push_back(Path::Combine(m_root, "vu_jit/vu0"));
			dirs_to_try.push_back(Path::Combine(m_root, "vu_jit/vu1"));
			dirs_to_try.push_back(Path::Combine(m_root, "vu_jit"));
			dirs_to_try.push_back(m_root);
			// Also remove any *.stale.* dirs the eviction path created.
			FileSystem::FindResultsArray staleDirs;
			FileSystem::FindFiles(
				Path::Combine(m_root, "vu_jit").c_str(),
				"vu0.stale.*",
				FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES,
				&staleDirs);
			for (const auto& sd : staleDirs)
				RecursiveRm(sd.FileName);
			staleDirs.clear();
			FileSystem::FindFiles(
				Path::Combine(m_root, "vu_jit").c_str(),
				"vu1.stale.*",
				FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES,
				&staleDirs);
			for (const auto& sd : staleDirs)
				RecursiveRm(sd.FileName);
			for (const auto& d : dirs_to_try)
				RecursiveRm(d);
		}
		const std::string& root() const { return m_root; }
		std::string vu_subdir(u32 vu_index) const
		{
			return Path::Combine(Path::Combine(m_root, "vu_jit"),
			                      (vu_index & 1) ? "vu1" : "vu0");
		}
	private:
		static void RecursiveRm(const std::string& dir)
		{
			if (!FileSystem::DirectoryExists(dir.c_str()))
			{
				// Maybe a regular file.
				::unlink(dir.c_str());
				return;
			}
			FileSystem::FindResultsArray ents;
			FileSystem::FindFiles(dir.c_str(), "*",
				FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS
				| FILESYSTEM_FIND_HIDDEN_FILES, &ents);
			for (const auto& e : ents)
			{
				if (e.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY)
					RecursiveRm(e.FileName);
				else
					::unlink(e.FileName.c_str());
			}
			::rmdir(dir.c_str());
		}
		std::string m_root;
		std::string m_prev_cache;
	};

	// Write the given header verbatim to `version_path`. Callers supply
	// either a current-format header or a deliberately-poisoned one.
	void WriteVersionFile(const std::string& version_path,
	                           VersionHeaderMirror& hdr)
	{
		// Atomic-replace via tmp + rename, same pattern Init uses.
		const std::string tmp = version_path + ".tmp";
		std::FILE* fp = std::fopen(tmp.c_str(), "wb");
		ASSERT_NE(fp, nullptr) << "fopen: " << std::strerror(errno);
		ASSERT_EQ(std::fwrite(&hdr, sizeof(hdr), 1, fp), 1u);
		std::fclose(fp);
		ASSERT_EQ(std::rename(tmp.c_str(), version_path.c_str()), 0);
	}

	// Read the on-disk VERSION back, asserting the file exists + the
	// magic + format/abi/vuIndex/archTag agree with the running build.
	void ReadAndVerifyCurrentVersion(const std::string& version_path,
	                                  u32 vu_index)
	{
		auto bytes = FileSystem::ReadBinaryFile(version_path.c_str());
		ASSERT_TRUE(bytes.has_value())
			<< "VERSION not on disk at " << version_path;
		ASSERT_EQ(bytes->size(), sizeof(VersionHeaderMirror));
		VersionHeaderMirror hdr;
		std::memcpy(&hdr, bytes->data(), sizeof(hdr));
		EXPECT_EQ(hdr.magic, kVersionMagicMirror);
		EXPECT_EQ(hdr.formatVersion,
		          mVUProgCache::kProgCacheFormatVersion);
		EXPECT_EQ(hdr.compilerAbiVersion,
		          pcsx2_test::kMvuCompilerAbiVersionMirror);
		EXPECT_EQ(hdr.vuIndex, vu_index & 1u);
		EXPECT_STREQ(hdr.archTag, "arm64-jit");
	}
}

// 1. Init on an empty cache dir creates $root + writes a VERSION file
//    matching the running build.
TEST(ProgCacheVersioning, fresh_init_writes_version_file)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64  = 0xAAAAAAAAAAAAAAAAull;
	sentinel.high64 = 0xBBBBBBBBBBBBBBBBull;
	mVUProgCache::InitWithSentinel(1, sentinel);

	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_TRUE(stats.initialized);
	EXPECT_TRUE(stats.enabled);
	EXPECT_EQ(stats.staleEvictions, 0u);
	EXPECT_EQ(stats.entries, 0u);

	const std::string version_path = Path::Combine(tmp.vu_subdir(1), "VERSION");
	ReadAndVerifyCurrentVersion(version_path, /*vu_index=*/1);

	mVUProgCache::ResetForTest(1);
}

// 2. Re-init with the same sentinel keeps the cache alive (no eviction).
TEST(ProgCacheVersioning, reinit_same_sentinel_keeps_cache)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64  = 0x1111111111111111ull;
	sentinel.high64 = 0x2222222222222222ull;
	mVUProgCache::InitWithSentinel(1, sentinel);
	mVUProgCache::ResetForTest(1);

	// Second Init — same sentinel.
	mVUProgCache::InitWithSentinel(1, sentinel);
	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_TRUE(stats.initialized);
	EXPECT_EQ(stats.staleEvictions, 0u);

	mVUProgCache::ResetForTest(1);
}

// 3. Re-init with a different optionsSentinel triggers an eviction.
TEST(ProgCacheVersioning, optionsSentinel_mismatch_evicts_cache)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t s_a{};
	s_a.low64 = 0xDEAD0000ull; s_a.high64 = 0xBEEF0000ull;
	mVUProgCache::InitWithSentinel(1, s_a);
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t s_b{};
	s_b.low64 = 0x1234ull; s_b.high64 = 0x5678ull; // differs from s_a
	mVUProgCache::InitWithSentinel(1, s_b);

	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_TRUE(stats.initialized);
	EXPECT_EQ(stats.staleEvictions, 1u)
		<< "optionsSentinel mismatch must rename the cache aside";
	EXPECT_EQ(stats.entries, 0u);

	// $root.stale.* should exist somewhere under $root_parent.
	FileSystem::FindResultsArray stales;
	FileSystem::FindFiles(
		Path::Combine(tmp.root(), "vu_jit").c_str(),
		"vu1.stale.*",
		FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES,
		&stales);
	EXPECT_FALSE(stales.empty())
		<< "expected at least one vu1.stale.<ns> directory after eviction";

	// Fresh VERSION on the new $root reflects s_b.
	const std::string version_path = Path::Combine(tmp.vu_subdir(1), "VERSION");
	auto bytes = FileSystem::ReadBinaryFile(version_path.c_str());
	ASSERT_TRUE(bytes.has_value());
	VersionHeaderMirror hdr;
	std::memcpy(&hdr, bytes->data(), sizeof(hdr));
	EXPECT_EQ(hdr.optionsSentinel.low64, s_b.low64);
	EXPECT_EQ(hdr.optionsSentinel.high64, s_b.high64);

	mVUProgCache::ResetForTest(1);
}

// 4. Poisoned VERSION on disk (wrong magic) triggers eviction. Same
//    code path as (3), exercising the VersionMatches.magic check.
TEST(ProgCacheVersioning, bad_magic_evicts_cache)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(0);

	XXH128_hash_t sentinel{};
	sentinel.low64 = 0x7777ull; sentinel.high64 = 0x8888ull;
	mVUProgCache::InitWithSentinel(0, sentinel);
	mVUProgCache::ResetForTest(0);

	// Poison: overwrite VERSION with bogus magic but otherwise-current
	// fields. Init should reject and evict.
	const std::string version_path = Path::Combine(tmp.vu_subdir(0), "VERSION");
	VersionHeaderMirror hdr{};
	hdr.magic              = 0xDEADBEEFu;             // wrong
	hdr.formatVersion      = mVUProgCache::kProgCacheFormatVersion;
	hdr.compilerAbiVersion = pcsx2_test::kMvuCompilerAbiVersionMirror;
	hdr.vuIndex            = 0;
	hdr.optionsSentinel    = sentinel;
	std::strncpy(hdr.archTag, "arm64-jit", sizeof(hdr.archTag) - 1);
	WriteVersionFile(version_path, hdr);

	mVUProgCache::InitWithSentinel(0, sentinel);
	const auto stats = mVUProgCache::GetStats(0);
	EXPECT_EQ(stats.staleEvictions, 1u);
	ReadAndVerifyCurrentVersion(version_path, /*vu_index=*/0);

	mVUProgCache::ResetForTest(0);
}

// 5. Poisoned formatVersion triggers eviction. Defends the format-version
//    bump invariant: a future bump must invalidate any older cache on disk.
TEST(ProgCacheVersioning, bad_formatVersion_evicts_cache)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64 = 0xAAAAull; sentinel.high64 = 0xBBBBull;
	mVUProgCache::InitWithSentinel(1, sentinel);
	mVUProgCache::ResetForTest(1);

	const std::string version_path = Path::Combine(tmp.vu_subdir(1), "VERSION");
	VersionHeaderMirror hdr{};
	hdr.magic              = kVersionMagicMirror;
	hdr.formatVersion      = mVUProgCache::kProgCacheFormatVersion + 100u; // wrong
	hdr.compilerAbiVersion = pcsx2_test::kMvuCompilerAbiVersionMirror;
	hdr.vuIndex            = 1;
	hdr.optionsSentinel    = sentinel;
	std::strncpy(hdr.archTag, "arm64-jit", sizeof(hdr.archTag) - 1);
	WriteVersionFile(version_path, hdr);

	mVUProgCache::InitWithSentinel(1, sentinel);
	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_EQ(stats.staleEvictions, 1u);
	ReadAndVerifyCurrentVersion(version_path, /*vu_index=*/1);

	mVUProgCache::ResetForTest(1);
}

// 6. Poisoned compilerAbiVersion triggers eviction. The user-visible
//    incantation in the docs: "to nuke the cache, bump
//    kMvuCompilerAbiVersion".
TEST(ProgCacheVersioning, bad_compilerAbiVersion_evicts_cache)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(0);

	XXH128_hash_t sentinel{};
	sentinel.low64 = 0xCCCCull; sentinel.high64 = 0xDDDDull;
	mVUProgCache::InitWithSentinel(0, sentinel);
	mVUProgCache::ResetForTest(0);

	const std::string version_path = Path::Combine(tmp.vu_subdir(0), "VERSION");
	VersionHeaderMirror hdr{};
	hdr.magic              = kVersionMagicMirror;
	hdr.formatVersion      = mVUProgCache::kProgCacheFormatVersion;
	hdr.compilerAbiVersion =
		pcsx2_test::kMvuCompilerAbiVersionMirror + 100u; // wrong
	hdr.vuIndex            = 0;
	hdr.optionsSentinel    = sentinel;
	std::strncpy(hdr.archTag, "arm64-jit", sizeof(hdr.archTag) - 1);
	WriteVersionFile(version_path, hdr);

	mVUProgCache::InitWithSentinel(0, sentinel);
	const auto stats = mVUProgCache::GetStats(0);
	EXPECT_EQ(stats.staleEvictions, 1u);
	ReadAndVerifyCurrentVersion(version_path, /*vu_index=*/0);

	mVUProgCache::ResetForTest(0);
}

// 0. The hand-mirrored ABI constant must equal production. This pins the
//    mirror against the linkable accessor so drift fails here with a clear
//    message, not as a downstream eviction in the round-trip tests.
TEST(ProgCacheVersioning, mirror_matches_production_abi_version)
{
	EXPECT_EQ(pcsx2_test::kMvuCompilerAbiVersionMirror,
		mVUProgCache::GetCompilerAbiVersion())
		<< "kMvuCompilerAbiVersionMirror drifted from production "
		   "kMvuCompilerAbiVersion — bump the mirror in lockstep.";
}

// 6b. Poisoned archTag triggers eviction. Pins the production
//     VersionMatches archTag memcmp (microVU_ProgCache-arm64.inl): a cache
//     written by a different-arch build must not be hydrated.
TEST(ProgCacheVersioning, bad_archTag_evicts_cache)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64 = 0xEEEEull; sentinel.high64 = 0xFFFFull;
	mVUProgCache::InitWithSentinel(1, sentinel);
	mVUProgCache::ResetForTest(1);

	const std::string version_path = Path::Combine(tmp.vu_subdir(1), "VERSION");
	VersionHeaderMirror hdr{};
	hdr.magic              = kVersionMagicMirror;
	hdr.formatVersion      = mVUProgCache::kProgCacheFormatVersion;
	hdr.compilerAbiVersion = pcsx2_test::kMvuCompilerAbiVersionMirror;
	hdr.vuIndex            = 1;
	hdr.optionsSentinel    = sentinel;
	std::strncpy(hdr.archTag, "x86-64-jit", sizeof(hdr.archTag) - 1); // wrong arch
	WriteVersionFile(version_path, hdr);

	mVUProgCache::InitWithSentinel(1, sentinel);
	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_EQ(stats.staleEvictions, 1u);
	ReadAndVerifyCurrentVersion(version_path, /*vu_index=*/1);

	mVUProgCache::ResetForTest(1);
}

// 7. Empty EmuFolders::Cache (e.g. test harness with no VMManager
//    init) disables the ProgCache silently. Init must NOT touch /tmp
//    or anywhere unsolicited.
TEST(ProgCacheVersioning, empty_emu_folders_disables_cache)
{
	const std::string prev = EmuFolders::Cache;
	EmuFolders::Cache.clear();
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	mVUProgCache::InitWithSentinel(1, sentinel);

	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_TRUE(stats.initialized);
	EXPECT_FALSE(stats.enabled)
		<< "empty EmuFolders::Cache must result in enabled=false";
	EXPECT_EQ(stats.staleEvictions, 0u);

	mVUProgCache::ResetForTest(1);
	EmuFolders::Cache = prev;
}

// --- Variable-length INDEX format ---------------------------------
//
// IndexEntry on disk is a fixed 64-byte header followed by
// `entryCount` × u32 entry-PC values (microMem byte offsets). The
// tests below mirror that header layout test-side so they can write
// synthetic records and assert the loader parses them. A
// static_assert on the mirror size catches any drift from the
// production layout at compile time.

namespace
{
#pragma pack(push, 1)
	struct IndexEntryHeaderMirror
	{
		XXH128_hash_t contentHash;
		u32           vuIndex;
		u32           codeSize;
		u32           blockCount;
		u32           flags;
		u64           lastUsedNs;
		u64           execCount;
		u32           entryCount;
		u8            pad[12];
	};
#pragma pack(pop)
	static_assert(sizeof(IndexEntryHeaderMirror) == 64,
		"IndexEntryHeader mirror drifted from production layout");

	// Build a current-format VERSION file in `vu_subdir/VERSION` so Init
	// doesn't evict the synthetic INDEX we're about to write.
	void WriteCurrentVersion(const std::string& version_path, u32 vu_index,
	                         const XXH128_hash_t& sentinel)
	{
		VersionHeaderMirror hdr{};
		hdr.magic              = kVersionMagicMirror;
		hdr.formatVersion      = mVUProgCache::kProgCacheFormatVersion;
		hdr.compilerAbiVersion = pcsx2_test::kMvuCompilerAbiVersionMirror;
		hdr.vuIndex            = vu_index & 1u;
		hdr.optionsSentinel    = sentinel;
		std::strncpy(hdr.archTag, "arm64-jit", sizeof(hdr.archTag) - 1);
		WriteVersionFile(version_path, hdr);
	}

	void WriteRawIndex(const std::string& index_path,
	                   const std::vector<u8>& bytes)
	{
		std::FILE* fp = std::fopen(index_path.c_str(), "wb");
		ASSERT_NE(fp, nullptr) << "fopen: " << std::strerror(errno);
		if (!bytes.empty())
		{
			ASSERT_EQ(std::fwrite(bytes.data(), bytes.size(), 1, fp), 1u);
		}
		std::fclose(fp);
	}

	std::vector<u8> SerializeOne(const IndexEntryHeaderMirror& hdr,
	                              const std::vector<u32>& entry_pcs)
	{
		IndexEntryHeaderMirror h = hdr;
		h.entryCount = static_cast<u32>(entry_pcs.size());
		std::vector<u8> out(sizeof(h) + entry_pcs.size() * sizeof(u32));
		std::memcpy(out.data(), &h, sizeof(h));
		if (!entry_pcs.empty())
		{
			std::memcpy(out.data() + sizeof(h),
			            entry_pcs.data(),
			            entry_pcs.size() * sizeof(u32));
		}
		return out;
	}
}

TEST(ProgCacheIndexFormatV3, single_entry_round_trip_via_test_helper)
{
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64  = 0xC0FFEE77C0FFEE77ull;
	sentinel.high64 = 0xC0FFEE77C0FFEE77ull;
	mVUProgCache::InitWithSentinel(1, sentinel);

	XXH128_hash_t hash{};
	hash.low64  = 0x1111222233334444ull;
	hash.high64 = 0x5555666677778888ull;

	// Append a single-entry record through the production persistence
	// path (atomic append + in-memory registration), then assert the
	// entry-PC round-trips in-process and across a simulated process
	// boundary (ResetForTest + re-Init re-reads the on-disk INDEX).
	const u32 seeded_pc = 0x58u;
	ASSERT_TRUE(mVUProgCache::TestAppendIndexEntry(1, hash, &seeded_pc, 1));

	// Duplicate hash short-circuits via the in-memory dedupe.
	EXPECT_FALSE(mVUProgCache::TestAppendIndexEntry(1, hash, &seeded_pc, 1));

	// On-disk record is the 64-byte header + 1 trailing u32 = 68 bytes.
	const std::string idx = Path::Combine(tmp.vu_subdir(1), "INDEX");
	ASSERT_TRUE(FileSystem::FileExists(idx.c_str()));
	const auto bytes = FileSystem::ReadBinaryFile(idx.c_str());
	ASSERT_TRUE(bytes.has_value());
	EXPECT_EQ(bytes->size(), 68u);

	// In-process: in-memory entry has the seeded PC.
	u32 pcs[8] = {0};
	size_t cnt = 0;
	ASSERT_TRUE(mVUProgCache::TestGetEntryPcs(1, hash, pcs, 8, &cnt));
	ASSERT_EQ(cnt, 1u);
	EXPECT_EQ(pcs[0], seeded_pc);

	// Reset + Init: cross-process reload reads it back from disk.
	mVUProgCache::ResetForTest(1);
	mVUProgCache::InitWithSentinel(1, sentinel);

	std::memset(pcs, 0, sizeof(pcs));
	cnt = 0;
	ASSERT_TRUE(mVUProgCache::TestGetEntryPcs(1, hash, pcs, 8, &cnt));
	EXPECT_EQ(cnt, 1u);
	EXPECT_EQ(pcs[0], seeded_pc);

	mVUProgCache::ResetForTest(1);
}

TEST(ProgCacheIndexFormatV3, multi_entry_synthetic_load)
{
	// Synthesize a 3-entry record. Init must round-trip every entry
	// PC through LoadIndex.
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64  = 0xC0FFEE88C0FFEE88ull;
	sentinel.high64 = 0xC0FFEE88C0FFEE88ull;

	// Lay down a valid VERSION + INDEX before Init runs.
	const std::string sub = tmp.vu_subdir(1);
	ASSERT_TRUE(FileSystem::EnsureDirectoryExists(sub.c_str(), true));
	WriteCurrentVersion(Path::Combine(sub, "VERSION"), 1, sentinel);

	XXH128_hash_t hash{};
	hash.low64  = 0xAAAA111122223333ull;
	hash.high64 = 0xBBBB444455556666ull;

	IndexEntryHeaderMirror hdr{};
	hdr.contentHash = hash;
	hdr.vuIndex     = 1u;
	hdr.flags       = 1u; // valid
	const std::vector<u32> entry_pcs = {0x000u, 0x058u, 0x148u};
	const auto bytes = SerializeOne(hdr, entry_pcs);
	WriteRawIndex(Path::Combine(sub, "INDEX"), bytes);

	mVUProgCache::InitWithSentinel(1, sentinel);

	const auto stats = mVUProgCache::GetStats(1);
	ASSERT_EQ(stats.entries, 1u);

	u32 pcs[8] = {0};
	size_t cnt = 0;
	ASSERT_TRUE(mVUProgCache::TestGetEntryPcs(1, hash, pcs, 8, &cnt));
	ASSERT_EQ(cnt, 3u);
	EXPECT_EQ(pcs[0], 0x000u);
	EXPECT_EQ(pcs[1], 0x058u);
	EXPECT_EQ(pcs[2], 0x148u);

	mVUProgCache::ResetForTest(1);
}

TEST(ProgCacheIndexFormatV3, truncated_trailing_drops_record_cleanly)
{
	// Header claims entryCount=4 but only 2 trailing u32s follow.
	// LoadIndex must drop the (partial) record and not poison
	// in-memory state.
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64  = 0xC0FFEE99C0FFEE99ull;
	sentinel.high64 = 0xC0FFEE99C0FFEE99ull;

	const std::string sub = tmp.vu_subdir(1);
	ASSERT_TRUE(FileSystem::EnsureDirectoryExists(sub.c_str(), true));
	WriteCurrentVersion(Path::Combine(sub, "VERSION"), 1, sentinel);

	XXH128_hash_t hash{};
	hash.low64  = 0xC0DEFADEC0DEFADEull;
	hash.high64 = 0xC0DEC0DEC0DEC0DEull;

	IndexEntryHeaderMirror hdr{};
	hdr.contentHash = hash;
	hdr.vuIndex     = 1u;
	hdr.flags       = 1u;
	hdr.entryCount  = 4u; // claims 4
	std::vector<u8> bytes(sizeof(hdr) + 2 * sizeof(u32));
	std::memcpy(bytes.data(), &hdr, sizeof(hdr));
	const u32 trail[2] = {0x10u, 0x20u};
	std::memcpy(bytes.data() + sizeof(hdr), trail, sizeof(trail));
	WriteRawIndex(Path::Combine(sub, "INDEX"), bytes);

	mVUProgCache::InitWithSentinel(1, sentinel);

	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_EQ(stats.entries, 0u)
		<< "truncated tail must not be loaded as a valid entry";

	u32 pcs[8] = {0};
	size_t cnt = 0;
	EXPECT_FALSE(mVUProgCache::TestGetEntryPcs(1, hash, pcs, 8, &cnt));

	mVUProgCache::ResetForTest(1);
}

TEST(ProgCacheIndexFormatV3, tombstone_skipped_on_load)
{
	// flags-bit-0 cleared OR entryCount==0 → tombstone; LoadIndex
	// must skip the record (advancing past the trailing bytes if
	// any) and continue parsing the rest of the file.
	TempCacheRoot tmp;
	mVUProgCache::ResetForTest(1);

	XXH128_hash_t sentinel{};
	sentinel.low64  = 0xC0FFEEAAC0FFEEAAull;
	sentinel.high64 = 0xC0FFEEAAC0FFEEAAull;

	const std::string sub = tmp.vu_subdir(1);
	ASSERT_TRUE(FileSystem::EnsureDirectoryExists(sub.c_str(), true));
	WriteCurrentVersion(Path::Combine(sub, "VERSION"), 1, sentinel);

	XXH128_hash_t deadHash{};
	deadHash.low64  = 0xDEADAAAA00000001ull;
	deadHash.high64 = 0xDEADAAAA00000002ull;
	XXH128_hash_t liveHash{};
	liveHash.low64  = 0xA11EAAAA00000001ull;
	liveHash.high64 = 0xA11EAAAA00000002ull;

	IndexEntryHeaderMirror dead{};
	dead.contentHash = deadHash;
	dead.vuIndex     = 1u;
	dead.flags       = 0u; // tombstone via cleared valid-bit
	const std::vector<u32> dead_pcs = {0x000u, 0x004u};

	IndexEntryHeaderMirror live{};
	live.contentHash = liveHash;
	live.vuIndex     = 1u;
	live.flags       = 1u;
	const std::vector<u32> live_pcs = {0x100u};

	std::vector<u8> bytes;
	{
		const auto a = SerializeOne(dead, dead_pcs);
		const auto b = SerializeOne(live, live_pcs);
		bytes.insert(bytes.end(), a.begin(), a.end());
		bytes.insert(bytes.end(), b.begin(), b.end());
	}
	WriteRawIndex(Path::Combine(sub, "INDEX"), bytes);

	mVUProgCache::InitWithSentinel(1, sentinel);

	const auto stats = mVUProgCache::GetStats(1);
	EXPECT_EQ(stats.entries, 1u) << "tombstone must not count";

	u32 pcs[8] = {0};
	size_t cnt = 0;
	EXPECT_FALSE(mVUProgCache::TestGetEntryPcs(1, deadHash, pcs, 8, &cnt));
	ASSERT_TRUE(mVUProgCache::TestGetEntryPcs(1, liveHash, pcs, 8, &cnt));
	EXPECT_EQ(cnt, 1u);
	EXPECT_EQ(pcs[0], 0x100u);

	mVUProgCache::ResetForTest(1);
}
