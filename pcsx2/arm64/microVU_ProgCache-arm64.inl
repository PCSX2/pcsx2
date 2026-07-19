// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
//
// Implementation of microVU_ProgCache-arm64.h. Included exactly once from
// microVU-arm64.cpp — the mVU module is a single TU (the microVU_*-arm64.inl
// files emit non-inline function bodies), so this implementation lives next to
// them rather than as a stand-alone .cpp.

#include "Config.h"
#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mVUProgCache
{
namespace
{
//------------------------------------------------------------------
// Disk layout
//   $EmuFolders::Cache/vu_jit/
//     vu0/
//       VERSION                          (64-byte handshake block)
//       INDEX                            (append-only stream of entries)
//       <hh>/...                         (payload artifacts)
//     vu1/  (same shape)
//
// VERSION + INDEX are per-VU so each VU initializes independently from the
// sequential mVUinit(microVU0) / mVUinit(microVU1) calls in cpuMicroVU::Init.
//------------------------------------------------------------------
static constexpr u32  kVersionMagic       = 0x4A56554Du; // 'MVUJ'
static constexpr char kRootDirName[]      = "vu_jit";
static constexpr char kVersionFileName[]  = "VERSION";
static constexpr char kIndexFileName[]    = "INDEX";

static_assert(sizeof(XXH128_hash_t) == 16, "XXH128_hash_t expected 16 bytes");

#pragma pack(push, 1)
struct VersionHeader
{
	u32           magic;              // kVersionMagic
	u32           formatVersion;      // kProgCacheFormatVersion
	u32           compilerAbiVersion; // kMvuCompilerAbiVersion
	u32           vuIndex;            // 0 or 1 — sentinel mixes VU index
	XXH128_hash_t optionsSentinel;    // this VU's mVU.optionsSentinel
	char          archTag[32];        // "arm64-jit"
};
static_assert(sizeof(VersionHeader) == 64, "VersionHeader layout drift");

// On-disk header for an IndexEntry. Each record is the 64-byte header
// followed by `entryCount` × u32 entry-PC values (microMem byte
// offsets). entryCount must be >= 1 for a valid entry; entryCount == 0
// indicates a tombstone alongside the flags-bit-0 check.
struct IndexEntryHeader
{
	XXH128_hash_t contentHash;  // 16 — the program's identity
	u32           vuIndex;      //  4
	u32           codeSize;     //  4 — size of the last-written .vuprog
	                            //      payload (0 = no payload on disk)
	u32           blockCount;   //  4
	u32           flags;        //  4 — bit 0 = valid; tombstones leave it clear
	u64           lastUsedNs;   //  8
	u64           execCount;    //  8 — running dispatch count, reserved
	u32           entryCount;   //  4 — number of trailing u32 entry-PC values
	u8            pad[12];      // 12 — round to 64 for trivial mmap
};
static_assert(sizeof(IndexEntryHeader) == 64,
              "IndexEntryHeader layout drift");
#pragma pack(pop)

// In-memory companion. Holds the on-disk header plus the trailing
// entry-PC list. Tests + dispatcher consume IndexEntry; serialization
// in/out of disk uses IndexEntryHeader + the appended u32 array.
struct IndexEntry
{
	IndexEntryHeader hdr;
	std::vector<u32> entry_pcs;
};

struct State
{
	bool        initialized   = false;
	bool        enabled       = true;
	std::string root;        // $EmuFolders::Cache/vu_jit/vu{0,1}
	std::string indexPath;   // $root/INDEX
	std::string versionPath; // $root/VERSION

	std::unordered_map<XXH128_hash_t, IndexEntry,
	                   MvuContentHashHash, MvuContentHashEq> entries;

	u64 wouldBeHits        = 0;
	u64 misses             = 0;
	u64 savesAttempted     = 0;
	u64 savesWritten       = 0;
	u64 staleEvictions     = 0;
	u64 dispatchProbes     = 0; // mVUsearchProg dispatch-time probes
	u64 dispatchIndexAvail = 0; // subset where the hash is in the INDEX
	u64 payloadWrites      = 0;
	u64 payloadHits        = 0;
	u64 payloadMissing     = 0;
	u64 payloadRejects     = 0;

	// Background payload preload. InitImpl spawns the thread when the INDEX
	// carries payload-bearing entries; it fills `preloaded` (most-recently-
	// used first, kPreloadBudgetBytes bound) and exits, so first-dispatch
	// hydration on the EE/MTVU threads is a map lookup instead of an eMMC
	// read. Buffers are consumed (moved out + erased) on hydration, so the
	// resident cost drains as the game warms up. preloadMutex guards
	// `preloaded` + the preload* counters; everything else in State keeps
	// its existing single-thread/reset-quiesced discipline.
	std::thread       preloadThread;
	std::mutex        preloadMutex;
	std::atomic<bool> preloadStop{false};
	std::unordered_map<XXH128_hash_t, std::vector<u8>,
	                   MvuContentHashHash, MvuContentHashEq> preloaded;
	u64 preloadedPayloads = 0;
	u64 preloadedBytes    = 0;
	u64 preloadHits       = 0;

	void StopPreload()
	{
		preloadStop.store(true, std::memory_order_relaxed);
		if (preloadThread.joinable())
			preloadThread.join();
	}

	// Joinable-thread backstop: a std::thread destroyed while joinable
	// terminates the process. Close/ResetForTest join explicitly; this
	// covers exit paths that skip mVUclose.
	~State() { StopPreload(); }
};

static State g_state[2];

static u64 NowNs()
{
	return static_cast<u64>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(
			std::chrono::steady_clock::now().time_since_epoch()).count());
}

static void BuildVersionHeader(u32 vu_index,
                                const XXH128_hash_t& options_sentinel,
                                VersionHeader& out)
{
	std::memset(&out, 0, sizeof(out));
	out.magic              = kVersionMagic;
	out.formatVersion      = kProgCacheFormatVersion;
	out.compilerAbiVersion = kMvuCompilerAbiVersion;
	out.vuIndex            = vu_index & 1u;
	out.optionsSentinel    = options_sentinel;
	std::strncpy(out.archTag, "arm64-jit", sizeof(out.archTag) - 1);
}

static bool VersionMatches(const VersionHeader& a, const VersionHeader& b)
{
	return a.magic              == b.magic
	    && a.formatVersion      == b.formatVersion
	    && a.compilerAbiVersion == b.compilerAbiVersion
	    && a.vuIndex            == b.vuIndex
	    && a.optionsSentinel.low64  == b.optionsSentinel.low64
	    && a.optionsSentinel.high64 == b.optionsSentinel.high64
	    && std::memcmp(a.archTag, b.archTag, sizeof(a.archTag)) == 0;
}

static bool WriteVersion(const std::string& path, const VersionHeader& hdr)
{
	// Atomic-replace via tmp+rename — keeps a partial write from poisoning
	// the cache on power-loss.
	const std::string tmp = path + ".tmp";
	if (!FileSystem::WriteBinaryFile(tmp.c_str(), &hdr, sizeof(hdr)))
		return false;
	return std::rename(tmp.c_str(), path.c_str()) == 0;
}

static void EvictStaleCache(State& s)
{
	if (!FileSystem::DirectoryExists(s.root.c_str()))
		return;
	const std::string stale = s.root + ".stale." + std::to_string(NowNs());
	if (std::rename(s.root.c_str(), stale.c_str()) != 0)
	{
		Console.Warning("mVUProgCache: rename %s -> %s failed",
		                s.root.c_str(), stale.c_str());
	}
	else
	{
		Console.WriteLn("mVUProgCache: evicted stale cache %s -> %s",
		                s.root.c_str(), stale.c_str());
		++s.staleEvictions;
	}
}

// Parse a variable-length INDEX. Each record is a fixed 64-byte
// IndexEntryHeader followed by `entryCount` × u32 entry-PC values.
// Truncated tails (partial header, partial trailing array) are silently
// dropped — the next SaveProgram will rewrite a clean copy, and the
// partial bytes don't poison any in-memory state. Last-writer-wins on
// duplicate contentHash so re-saves can extend a program's entry list
// in place.
static void LoadIndex(State& s)
{
	auto bytes = FileSystem::ReadBinaryFile(s.indexPath.c_str());
	if (!bytes.has_value())
		return;
	const u8* base = bytes->data();
	const size_t total = bytes->size();
	size_t off = 0;
	while (off + sizeof(IndexEntryHeader) <= total)
	{
		IndexEntryHeader hdr;
		std::memcpy(&hdr, base + off, sizeof(hdr));
		off += sizeof(hdr);

		const size_t trail_bytes = static_cast<size_t>(hdr.entryCount) * sizeof(u32);
		if (off + trail_bytes > total)
		{
			Console.Warning("mVUProgCache: INDEX %s truncated at off=%zu "
			                "(need %zu trailing bytes, have %zu) — ignoring tail",
			                s.indexPath.c_str(), off, trail_bytes,
			                total - off);
			return;
		}

		if ((hdr.flags & 1u) == 0u || hdr.entryCount == 0u)
		{
			off += trail_bytes;
			continue;
		}

		IndexEntry e;
		e.hdr = hdr;
		e.entry_pcs.resize(hdr.entryCount);
		std::memcpy(e.entry_pcs.data(), base + off, trail_bytes);
		off += trail_bytes;

		s.entries[hdr.contentHash] = std::move(e); // last-writer-wins
	}
	if (off != total)
	{
		Console.Warning("mVUProgCache: INDEX %s has %zu trailing bytes past last "
		                "header — ignoring", s.indexPath.c_str(), total - off);
	}
}

// Serialize one in-memory IndexEntry as a single contiguous
// header+trailing block. Single fwrite keeps the append atomic against
// process death between header and trailing writes.
static bool AppendIndexEntry(const State& s, const IndexEntry& e)
{
	std::FILE* fp = FileSystem::OpenCFile(s.indexPath.c_str(), "ab");
	if (!fp)
		return false;

	std::vector<u8> buf(sizeof(IndexEntryHeader)
	                    + e.entry_pcs.size() * sizeof(u32));
	std::memcpy(buf.data(), &e.hdr, sizeof(IndexEntryHeader));
	if (!e.entry_pcs.empty())
	{
		std::memcpy(buf.data() + sizeof(IndexEntryHeader),
		            e.entry_pcs.data(),
		            e.entry_pcs.size() * sizeof(u32));
	}
	const bool ok = std::fwrite(buf.data(), buf.size(), 1, fp) == 1;
	std::fclose(fp);
	return ok;
}

// Persist an entry: append to disk + register in the in-memory map.
// Keeps header.entryCount and entry_pcs.size() in lock-step — callers
// populate entry_pcs; this is the single point that updates the
// on-disk count field.
static bool PersistIndexEntry(State& s, const IndexEntry& e_in)
{
	IndexEntry e = e_in;
	e.hdr.entryCount = static_cast<u32>(e.entry_pcs.size());

	if (!AppendIndexEntry(s, e))
	{
		Console.Warning("mVUProgCache[%u]: INDEX append failed", e.hdr.vuIndex);
		return false;
	}
	s.entries[e.hdr.contentHash] = std::move(e);
	++s.savesWritten;
	return true;
}

//------------------------------------------------------------------
// .vuprog payload files — the serialized block graph (mVUPersist image),
// content-addressed under a 256-way shard:
//   $root/<hh>/<hash32hex>.vuprog
// where <hh> is the first hex byte of the 128-bit content hash.
//------------------------------------------------------------------

static std::string PayloadPath(const State& s, const XXH128_hash_t& hash,
                                std::string* out_shard_dir = nullptr)
{
	char name[48];
	std::snprintf(name, sizeof(name), "%016" PRIx64 "%016" PRIx64 ".vuprog",
		hash.high64, hash.low64);
	const char shard[3] = {name[0], name[1], 0};
	std::string shard_dir = Path::Combine(s.root, shard);
	std::string path = Path::Combine(shard_dir, name);
	if (out_shard_dir)
		*out_shard_dir = std::move(shard_dir);
	return path;
}

// Atomic-replace write (tmp+rename), creating the shard dir on demand.
static bool WritePayload(State& s, const XXH128_hash_t& hash,
                          const std::vector<u8>& image)
{
	std::string shard_dir;
	const std::string path = PayloadPath(s, hash, &shard_dir);
	Error error;
	if (!FileSystem::EnsureDirectoryExists(shard_dir.c_str(), false, &error))
	{
		Console.Warning("mVUProgCache: cannot create payload shard for %s: %s",
			path.c_str(), error.GetDescription().c_str());
		return false;
	}
	const std::string tmp = path + ".tmp";
	if (!FileSystem::WriteBinaryFile(tmp.c_str(), image.data(), image.size()))
		return false;
	if (std::rename(tmp.c_str(), path.c_str()) != 0)
		return false;

	// A rewritten payload supersedes any not-yet-consumed preloaded copy
	// (which would otherwise serve the pre-growth image — sound under the
	// consistent-prefix rule, but pointlessly stale).
	{
		std::lock_guard<std::mutex> lock(s.preloadMutex);
		auto pit = s.preloaded.find(hash);
		if (pit != s.preloaded.end())
		{
			s.preloadedBytes -= pit->second.size();
			s.preloaded.erase(pit);
		}
	}
	return true;
}

// Read the payload-bearing INDEX entries into RAM on a background thread,
// most-recently-used first, bounded by kPreloadBudgetBytes. The bound is a
// cap, not a target — resident cost matters on low-memory devices, but
// buffers drain as hydrations consume them.
static constexpr size_t kPreloadBudgetBytes = 32 * 1024 * 1024;

static void StartPreload(State& s)
{
	struct Item
	{
		XXH128_hash_t hash;
		u64 lastUsedNs;
		u32 size;
	};
	std::vector<Item> items;
	for (const auto& kv : s.entries)
	{
		if (kv.second.hdr.codeSize > 0)
			items.push_back({kv.first, kv.second.hdr.lastUsedNs, kv.second.hdr.codeSize});
	}
	if (items.empty())
		return;
	std::sort(items.begin(), items.end(),
		[](const Item& a, const Item& b) { return a.lastUsedNs > b.lastUsedNs; });

	// Strict MRU prefix (first over-budget item stops the walk, no
	// skip-and-refill) so what got preloaded is a deterministic function of
	// the INDEX, not of file-size interleaving.
	std::vector<std::pair<XXH128_hash_t, std::string>> files;
	size_t budget = 0;
	for (const Item& it : items)
	{
		if (budget + it.size > kPreloadBudgetBytes)
			break;
		budget += it.size;
		files.emplace_back(it.hash, PayloadPath(s, it.hash));
	}
	if (files.size() < items.size())
	{
		Console.WriteLn("mVUProgCache: preload budget (%zu MiB) reached — "
		                "%zu of %zu payloads stay on disk",
		                kPreloadBudgetBytes / (1024 * 1024),
		                items.size() - files.size(), items.size());
	}

	s.preloadStop.store(false, std::memory_order_relaxed);
	// The &s capture is intentional and safe: s aliases a process-lifetime
	// static (g_state[]), and StopPreload() joins this thread before Close/
	// ResetForTest ever mutate that state. Don't "fix" this into a by-value or
	// shared_ptr capture — there is no lifetime bug to solve.
	s.preloadThread = std::thread([&s, files = std::move(files)]() {
		for (const auto& [hash, path] : files)
		{
			if (s.preloadStop.load(std::memory_order_relaxed))
				return;
			auto bytes = FileSystem::ReadBinaryFile(path.c_str());
			if (!bytes.has_value() || bytes->empty())
				continue; // dispatch-time read will count payloadMissing
			std::lock_guard<std::mutex> lock(s.preloadMutex);
			s.preloadedBytes += bytes->size();
			++s.preloadedPayloads;
			s.preloaded[hash] = std::move(*bytes);
		}
	});
}
} // anonymous namespace

namespace
{
// microVU-free initialization core. Production Init wraps this with
// (mVU.index, mVU.optionsSentinel); tests reach the same code path
// through InitWithSentinel without dragging the microVU struct through
// the test TU (which would re-emit the .inl bodies).
static void InitImpl(u32 vu_index, const XXH128_hash_t& options_sentinel)
{
	State& s = g_state[vu_index & 1];
	if (s.initialized)
		return;

	// Determinism gate for offline tooling (pcsx2-vurunner --no-progcache). The
	// program cache is a process-persistent, on-disk side effect, so two
	// "identical" runs are not byte-identical. SetProcessDisable(true) forces the
	// cache off for the whole process so a JIT-vs-interp divergence run is
	// reproducible. No effect in production (never set there).
	if (mVUPersist::IsProcessDisabled())
	{
		s.enabled = false;
		s.initialized = true;
		return;
	}

	// EmuFolders::Cache is set by the Qt frontend after parsing user paths; the
	// unit-test harness leaves it empty (RecompilerTestEnvironment doesn't run
	// the full VMManager bring-up). Skip the on-disk cache cleanly in that
	// mode — tests run faster and don't leave a ./vu_jit/ in their cwd.
	if (EmuFolders::Cache.empty())
	{
		s.enabled = false;
		s.initialized = true;
		return;
	}

	const std::string root = Path::Combine(EmuFolders::Cache, kRootDirName);
	s.root        = Path::Combine(root, (vu_index & 1) ? "vu1" : "vu0");
	s.indexPath   = Path::Combine(s.root, kIndexFileName);
	s.versionPath = Path::Combine(s.root, kVersionFileName);

	if (!s.enabled)
	{
		s.initialized = true;
		return;
	}

	// Read VERSION BEFORE creating $root so a true fresh-init (no $root,
	// no VERSION) doesn't trip the EvictStaleCache path (which would
	// rename the just-created empty $root aside and inflate
	// staleEvictions). EvictStaleCache itself is a no-op if $root doesn't
	// exist, so the rebuild branch below covers both "directory was
	// missing entirely" and "directory existed with mismatched VERSION."
	VersionHeader live;
	BuildVersionHeader(vu_index, options_sentinel, live);

	bool versionOk = false;
	auto vbytes = FileSystem::ReadBinaryFile(s.versionPath.c_str());
	if (vbytes.has_value() && vbytes->size() == sizeof(VersionHeader))
	{
		VersionHeader disk;
		std::memcpy(&disk, vbytes->data(), sizeof(disk));
		versionOk = VersionMatches(disk, live);
	}

	Error error;
	if (!versionOk)
	{
		// If the directory exists, treat it as stale (rename it aside).
		// If it doesn't, this is a no-op and execution falls through to mkdir +
		// WriteVersion below.
		EvictStaleCache(s);
		if (!FileSystem::EnsureDirectoryExists(s.root.c_str(), true, &error))
		{
			Console.Warning("mVUProgCache[%u]: cannot create %s: %s — disabling",
			                vu_index, s.root.c_str(),
			                error.GetDescription().c_str());
			s.enabled = false;
			s.initialized = true;
			return;
		}
		if (!WriteVersion(s.versionPath, live))
		{
			Console.Warning("mVUProgCache[%u]: write VERSION failed — disabling",
			                vu_index);
			s.enabled = false;
			s.initialized = true;
			return;
		}
	}

	s.entries.clear();
	LoadIndex(s);

	Console.WriteLn(Color_StrongBlue,
		"mVUProgCache[%u]: init root=%s entries=%zu",
		vu_index, s.root.c_str(), s.entries.size());

	StartPreload(s);

	s.initialized = true;
}
} // anonymous namespace

void Init(microVU& mVU)
{
	// Production gate: the whole on-disk cache — init, INDEX
	// telemetry, .vuprog payloads, dispatch-time hydration — sits behind the
	// EmuCore/CPU/Recompiler EnableVUProgramCache INI bool. Off (the default)
	// means no disk side effects at all. Toggling it on at runtime works
	// because mVUreset re-enters here after every settings-driven recompiler
	// clear; toggling it OFF leaves the already-initialized state up for the
	// rest of the session (entries written after the toggle are keyed to the
	// post-toggle sentinel and are evicted wholesale on the next handshake —
	// bounded garbage, never corruption). Tests bypass this gate through
	// InitWithSentinel / TestReinitFromLiveSentinel.
	if (!EmuConfig.Cpu.Recompiler.EnableVUProgramCache)
		return;
	InitImpl(mVU.index & 1u, mVU.optionsSentinel);
}

void InitWithSentinel(u32 vu_index, const XXH128_hash_t& options_sentinel)
{
	InitImpl(vu_index & 1u, options_sentinel);
}

void Close(microVU& mVU)
{
	State& s = g_state[mVU.index & 1];
	if (!s.initialized)
		return;
	DumpStats(mVU, "close");
	// No file handles kept open (per-call open/append/close). Clear the
	// in-memory index so the next mVUreset starts fresh.
	s.StopPreload();
	{
		std::lock_guard<std::mutex> lock(s.preloadMutex);
		s.preloaded.clear();
		s.preloadedBytes = 0;
		// Zero the cumulative preload counters too (mirror ResetForTest) so a
		// Close+Init cycle starts from a consistent baseline — otherwise
		// GetStats would report preloadedBytes==0 next to stale nonzero
		// preloadedPayloads/preloadHits. DumpStats("close") above already
		// emitted this lifecycle's lifetime totals.
		s.preloadedPayloads = 0;
		s.preloadHits = 0;
	}
	s.initialized = false;
	s.entries.clear();
}

void SaveProgram(microVU& mVU, const microProgram& prog)
{
	State& s = g_state[mVU.index & 1];
	if (!s.initialized || !s.enabled || !prog.contentHashValid)
		return;

	++s.savesAttempted;

	u32 blockCount = 0;
	for (u32 i = 0; i < (mVU.progSize / 2); ++i)
	{
		microBlockManager* bm = prog.block[i];
		if (!bm)
			continue;
		blockCount += static_cast<u32>(bm->getFullListCount());
	}

	// Serialize the recorded block graph. Empty when mVUPersist recording
	// was off while this program compiled, or the program was marked
	// non-persistable — the INDEX entry is then telemetry-only (no
	// payload).
	std::vector<u8> image;
	const bool have_image = mVUPersist::SerializeProgram(
		mVU, prog, image);

	// Write/refresh the .vuprog payload when the serialized graph grew (a
	// hydrated program compiled new blocks since the last save) or the
	// file is missing. hdr.codeSize tracks the last-written payload size —
	// the cheap growth signal (same program + same emitters can't change
	// bytes without changing size).
	auto write_payload_if_grown = [&](IndexEntry& e) -> bool {
		if (!have_image)
			return false;
		if (e.hdr.codeSize == static_cast<u32>(image.size()) &&
			FileSystem::FileExists(PayloadPath(s, prog.contentHash).c_str()))
			return false;
		if (!WritePayload(s, prog.contentHash, image))
		{
			Console.Warning("mVUProgCache[%u]: payload write failed", mVU.index);
			return false;
		}
		e.hdr.codeSize = static_cast<u32>(image.size());
		++s.payloadWrites;
		return true;
	};

	// When this hash has been seen before, the on-disk entry_pcs is the
	// cumulative truth. Union the existing set with what the current
	// program instance observed; only re-persist if the union widens past
	// what's already on disk. This handles the cross-program-instance
	// case: the dispatcher hits N PCs across several mVUreset cycles, but
	// each instance starts fresh with observed={its createProg PC}.
	// Without the union, the FIRST instance's SaveProgram wins and
	// subsequent calls short-circuit via s.entries dedupe. Reading the
	// existing set and unioning lets the on-disk entry list converge to
	// all observed PCs over enough boots.
	const u32 primary_pc = static_cast<u32>(prog.startPC) * 8u;

	auto it_existing = s.entries.find(prog.contentHash);
	if (it_existing != s.entries.end())
	{
		// Existing entry — widening check. Probe for genuinely-new PCs
		// against the on-disk set without copying it first: once a
		// program's observed-PC set has converged (the steady state
		// after a few boots) this branch runs on every SaveProgram and
		// the copy would be pure waste. Only materialize the wider list
		// when a new PC actually appears.
		IndexEntry& existing = it_existing->second;
		std::unordered_map<u32, bool> seen;
		for (u32 pc : existing.entry_pcs) seen.emplace(pc, true);

		std::vector<u32> new_pcs;
		auto note = [&](u32 pc) {
			if (seen.emplace(pc, true).second) new_pcs.push_back(pc);
		};
		// Ensure primary_pc is present (it would be, but seed
		// defensively in case a prior save was malformed).
		note(primary_pc);
		for (size_t i = 0; i < prog.observed.count; ++i)
			note(prog.observed.pcs[i]);

		const bool widened = !new_pcs.empty();
		const bool payload_written = write_payload_if_grown(existing);
		if (!widened && !payload_written)
			return; // No new PCs, payload current — short-circuit.

		// Re-persist with the wider set / refreshed payload size
		// (last-writer-wins on the next LoadIndex).
		IndexEntry e = existing;
		if (widened)
		{
			e.entry_pcs.reserve(e.entry_pcs.size() + new_pcs.size());
			e.entry_pcs.insert(e.entry_pcs.end(), new_pcs.begin(), new_pcs.end());
		}
		e.hdr.lastUsedNs  = NowNs();
		e.hdr.blockCount  = blockCount;
		PersistIndexEntry(s, e);
		return;
	}

	// First-time save for this hash.
	IndexEntry e;
	e.hdr.contentHash = prog.contentHash;
	e.hdr.vuIndex     = mVU.index & 1u;
	e.hdr.codeSize    = 0; // set by write_payload_if_grown on success
	e.hdr.blockCount  = blockCount;
	e.hdr.flags       = 1u;
	e.hdr.lastUsedNs  = NowNs();
	e.hdr.execCount   = 0;
	// Primary entry is always the creator's startPC at index 0; observed
	// PCs follow.
	e.entry_pcs.push_back(primary_pc);
	const auto& observed = prog.observed;
	for (u32 i = 0; i < observed.count; ++i)
	{
		const u32 pc = observed.pcs[i];
		if (pc == primary_pc)
			continue;
		e.entry_pcs.push_back(pc);
	}

	write_payload_if_grown(e);
	PersistIndexEntry(s, e);
}

bool TestGetEntryPcs(u32 vu_index, const XXH128_hash_t& hash,
                      u32* out_pcs, size_t out_pcs_cap,
                      size_t* out_count)
{
	if (out_count)
		*out_count = 0;
	State& s = g_state[vu_index & 1];
	if (!s.initialized)
		return false;
	auto it = s.entries.find(hash);
	if (it == s.entries.end())
		return false;
	const auto& pcs = it->second.entry_pcs;
	const size_t n  = pcs.size();
	if (out_count)
		*out_count = n;
	if (out_pcs && out_pcs_cap > 0)
	{
		const size_t copy = (n < out_pcs_cap) ? n : out_pcs_cap;
		for (size_t i = 0; i < copy; ++i)
			out_pcs[i] = pcs[i];
	}
	return true;
}

bool TestAppendIndexEntry(u32 vu_index, const XXH128_hash_t& hash,
                          const u32* entry_pcs, size_t entry_pc_count)
{
	State& s = g_state[vu_index & 1];
	if (!s.initialized || !s.enabled)
		return false;

	++s.savesAttempted;

	if (s.entries.find(hash) != s.entries.end())
		return false;

	IndexEntry e;
	e.hdr.contentHash = hash;
	e.hdr.vuIndex     = vu_index & 1u;
	e.hdr.codeSize    = 0;
	e.hdr.blockCount  = 0;
	e.hdr.flags       = 1u;
	e.hdr.lastUsedNs  = NowNs();
	e.hdr.execCount   = 0;
	e.entry_pcs.assign(entry_pcs, entry_pcs + entry_pc_count);

	return PersistIndexEntry(s, e);
}

void SaveAllPrograms(microVU& mVU)
{
	State& s = g_state[mVU.index & 1];
	if (!s.initialized || !s.enabled)
		return;
	for (const auto& kv : mVU.mvuContentMap)
	{
		if (kv.second)
			SaveProgram(mVU, *kv.second);
	}
}

microProgram* TryLoadProgram(microVU& mVU, const XXH128_hash_t& contentHash)
{
	State& s = g_state[mVU.index & 1];
	if (!s.initialized || !s.enabled)
	{
		// Cache disabled/not initialized: the INDEX was never consulted, so
		// this is not an INDEX miss. Leave `misses` alone (it counts genuine
		// hash-not-in-INDEX lookups below) — otherwise the production default
		// (EnableVUProgramCache OFF) inflates the miss count on every dispatch.
		return nullptr;
	}
	auto it = s.entries.find(contentHash);
	if (it == s.entries.end())
	{
		++s.misses;
		return nullptr;
	}
	++s.wouldBeHits;

	// RAM first: consume the preloaded buffer if the background preload got
	// to this payload (moved out + erased, so the resident footprint drains
	// as the game warms up). Disk is the fallback for entries past the
	// preload budget or dispatched before the preload thread reached them.
	std::optional<std::vector<u8>> payload;
	{
		std::lock_guard<std::mutex> lock(s.preloadMutex);
		auto pit = s.preloaded.find(contentHash);
		if (pit != s.preloaded.end())
		{
			payload = std::move(pit->second);
			s.preloadedBytes -= payload->size();
			s.preloaded.erase(pit);
			++s.preloadHits;
		}
	}
	if (!payload.has_value())
		payload = FileSystem::ReadBinaryFile(PayloadPath(s, contentHash).c_str());
	if (!payload.has_value() || payload->empty())
	{
		// Telemetry-only entry (saved with recording off) or the payload
		// was evicted independently of the INDEX. Recompile.
		++s.payloadMissing;
		return nullptr;
	}

	// mVUsearchProg calls this inside a freshly-opened — still empty —
	// emission episode, but HydrateProgram needs the code cache closed so
	// it can run its own open/close pair. Close around the hydration and
	// restore on the way out; closing an empty episode is a recorded
	// no-op, and new blocks compiled after the reopen attach to the
	// hydrated program's rebuilt persist log as growth chunks.
	const bool was_open = (armAsm != nullptr);
	if (was_open)
		mVUcloseCodeCache(mVU);
	microProgram* prog = mVUPersist::HydrateProgram(mVU, payload->data(), payload->size());
	if (was_open)
		mVUopenCodeCache(mVU);

	if (!prog)
	{
		// Layout-base / content-hash / structural rejection — degrade to
		// recompile, never corrupt. HydrateProgram counted the reason.
		++s.payloadRejects;
		return nullptr;
	}
	++s.payloadHits;
	it->second.hdr.lastUsedNs = NowNs();
	return prog;
}

void ObserveDispatchHash(microVU& mVU, const XXH128_hash_t& contentHash,
                          u32 startPC_bytes)
{
	State& s = g_state[mVU.index & 1];
	if (!s.initialized || !s.enabled)
		return;
	++s.dispatchProbes;
	(void)startPC_bytes;
	if (s.entries.find(contentHash) != s.entries.end())
		++s.dispatchIndexAvail;
}

Stats GetStats(u32 vu_index)
{
	State& s = g_state[vu_index & 1];
	Stats out;
	out.initialized        = s.initialized;
	out.enabled            = s.enabled;
	out.entries            = s.entries.size();
	out.savesAttempted     = s.savesAttempted;
	out.savesWritten       = s.savesWritten;
	out.wouldBeHits        = s.wouldBeHits;
	out.misses             = s.misses;
	out.staleEvictions     = s.staleEvictions;
	out.dispatchProbes     = s.dispatchProbes;
	out.dispatchIndexAvail = s.dispatchIndexAvail;
	out.payloadWrites      = s.payloadWrites;
	out.payloadHits        = s.payloadHits;
	out.payloadMissing     = s.payloadMissing;
	out.payloadRejects     = s.payloadRejects;
	{
		std::lock_guard<std::mutex> lock(s.preloadMutex);
		out.preloadedPayloads = s.preloadedPayloads;
		out.preloadedBytes    = s.preloadedBytes;
		out.preloadHits       = s.preloadHits;
	}
	return out;
}

u32 GetCompilerAbiVersion()
{
	return kMvuCompilerAbiVersion;
}

void TestReinitFromLiveSentinel(u32 vu_index)
{
	microVU& mVU = (vu_index & 1) ? microVU1 : microVU0;
	if (!mVU.optionsSentinelValid)
		mVUbuildOptionsSentinel(mVU);
	InitImpl(vu_index & 1, mVU.optionsSentinel);
}

bool TestGetLiveSentinel(u32 vu_index, XXH128_hash_t* out_sentinel)
{
	if (!out_sentinel)
		return false;
	microVU& mVU = (vu_index & 1) ? microVU1 : microVU0;
	if (!mVU.optionsSentinelValid)
		mVUbuildOptionsSentinel(mVU);
	*out_sentinel = mVU.optionsSentinel;
	return true;
}

void ResetForTest(u32 vu_index)
{
	State& s = g_state[vu_index & 1];
	s.StopPreload();
	s.initialized      = false;
	s.enabled          = true;
	s.root.clear();
	s.indexPath.clear();
	s.versionPath.clear();
	s.entries.clear();
	s.wouldBeHits        = 0;
	s.misses             = 0;
	s.savesAttempted     = 0;
	s.savesWritten       = 0;
	s.staleEvictions     = 0;
	s.dispatchProbes     = 0;
	s.dispatchIndexAvail = 0;
	s.payloadWrites      = 0;
	s.payloadHits        = 0;
	s.payloadMissing     = 0;
	s.payloadRejects     = 0;
	{
		std::lock_guard<std::mutex> lock(s.preloadMutex);
		s.preloaded.clear();
		s.preloadedPayloads = 0;
		s.preloadedBytes    = 0;
		s.preloadHits       = 0;
	}
}

void TestWaitForPreload(u32 vu_index)
{
	State& s = g_state[vu_index & 1];
	if (s.preloadThread.joinable())
		s.preloadThread.join();
}

void DumpStats(const microVU& mVU, const char* tag)
{
	const Stats s = GetStats(mVU.index & 1);
	const u64 entries = s.entries;
	Console.WriteLn(Color_StrongGreen,
		"mVUProgCache[%u] %s: entries=%llu saves(att/written)=%llu/%llu "
		"loads(indexHit/miss)=%llu/%llu staleEvicts=%llu "
		"dispatchProbes=%llu dispatchIndexAvail=%llu "
		"payload(writes/hits/missing/rejects)=%llu/%llu/%llu/%llu "
		"preload(loaded/residentB/hits)=%llu/%llu/%llu",
		mVU.index, tag,
		(unsigned long long)entries,
		(unsigned long long)s.savesAttempted,
		(unsigned long long)s.savesWritten,
		(unsigned long long)s.wouldBeHits,
		(unsigned long long)s.misses,
		(unsigned long long)s.staleEvictions,
		(unsigned long long)s.dispatchProbes,
		(unsigned long long)s.dispatchIndexAvail,
		(unsigned long long)s.payloadWrites,
		(unsigned long long)s.payloadHits,
		(unsigned long long)s.payloadMissing,
		(unsigned long long)s.payloadRejects,
		(unsigned long long)s.preloadedPayloads,
		(unsigned long long)s.preloadedBytes,
		(unsigned long long)s.preloadHits);
}
}
