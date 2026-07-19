// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#ifndef XXH_versionNumber
#define XXH_STATIC_LINKING_ONLY 1
#define XXH_INLINE_ALL 1
#include "xxhash.h"
#endif

struct microVU;
struct microProgram;

// Disk-side scaffolding for the persisted-JIT VU program cache. Owns:
//   1. The per-VU vu_jit/vu{0,1}/{VERSION,INDEX,<shard>/...} filesystem layout.
//   2. Version handshake: a build-time ABI version + options sentinel + arch tag
//      anchors the cache to a specific PCSX2 binary + config. Mismatch on startup
//      atomically renames vu_jit.../<old>/ → vu_jit.../<old>.stale.<ns>/ and
//      starts fresh.
//   3. In-memory INDEX populated from disk at Init. Subsequent Save calls append
//      one variable-length entry per program to disk + in-memory map.
//   4. TryLoadProgram hydrates the block graph via mVUPersist::HydrateProgram
//      on an INDEX+payload hit. Constant-VA arena layout + a placement-relative
//      fixup table make the persisted vixl output relocatable (absolute
//      armEmitCall / armMoveAddressToReg targets are patched on load); any
//      miss or payload rejection falls back to a normal recompile.
namespace mVUProgCache
{
	// Bumped together with kMvuCompilerAbiVersion when the on-disk layout
	// changes. Mismatch nukes the cache directory at startup.
	//
	// IndexEntry is variable-length: a fixed 64-byte header carrying
	// `entryCount`, followed by `entryCount` × `u32` entry-PC values
	// (microMem byte offsets), so multi-entry programs carry every
	// dispatched PC. Older fixed-layout entries are structurally
	// incompatible with this format — a version mismatch evicts rather
	// than silently loading wrong data.
	static constexpr u32 kProgCacheFormatVersion = 3;

	// Initialize the per-VU on-disk cache. Gated on the
	// EmuCore/CPU/Recompiler EnableVUProgramCache config bool (off = no disk
	// side effects at all). Reads VERSION; if it diverges from the running
	// build's (compiler ABI, options sentinel, arch tag), evicts the stale
	// dir and starts fresh. Loads INDEX into memory. Idempotent — called
	// from mVUinit AND every mVUreset (the reset call is what activates a
	// runtime config toggle); only does real work on first effective call.
	void Init(microVU& mVU);

	// Same Init logic but takes (vu_index, options_sentinel) directly so
	// test TUs don't have to pull microVU-arm64.h (which includes the .inl
	// bodies and would link-collide with libpcsx2.a). Not for production
	// use; mVU.optionsSentinel is built by mVUbuildOptionsSentinel during
	// mVUreset and that path must run before any Init.
	void InitWithSentinel(u32 vu_index, const XXH128_hash_t& options_sentinel);

	// Persist pending writes, close handles. Called from mVUclose.
	void Close(microVU& mVU);

	// Walk mVU.mvuContentMap and save every program whose contentHash is not
	// already represented in the on-disk INDEX (or whose observed entry-PC
	// set widened). Called from mVUreset (pre-program-free) and mVUclose so
	// the next process boot has artifacts.
	void SaveAllPrograms(microVU& mVU);

	// Save a single program: append an INDEX entry, and — when the program
	// carries a persist log (mVUPersist recording was enabled while it
	// compiled) — write the serialized block graph as a content-addressed
	// .vuprog payload (<root>/<hh>/<hash32hex>.vuprog, tmp+rename). The
	// payload is rewritten when the recorded block graph grew past what is
	// on disk; the INDEX append still short-circuits when neither the
	// entry-PC set nor the payload changed.
	void SaveProgram(microVU& mVU, const microProgram& prog);

	// Promote an on-disk program to a live one. On an INDEX hit with a
	// readable .vuprog payload, hydrates the block graph through
	// mVUPersist::HydrateProgram and returns the new program (registered in
	// mVU.mvuContentMap, dispatchable immediately). Returns nullptr on
	// INDEX miss, missing/corrupt payload, or any hydration rejection
	// (layout-base / content-hash / structural mismatch) — the caller falls
	// back to recompiling. Called from mVUsearchProg with the code cache
	// OPEN; the (empty) emission episode is closed around the hydration and
	// reopened before returning.
	microProgram* TryLoadProgram(microVU& mVU, const XXH128_hash_t& contentHash);

	// Dispatch-time probe used by mVUsearchProg on the in-process-miss
	// path. Tracks whether the INDEX already knows the (program, startPC)
	// tuple the dispatcher is about to recompile — the hit-rate equals the
	// upper bound on what a real hydration path would have served. Cheap
	// (one unordered_map lookup), called once per program-switch — kept
	// off the per-dispatch fast path.
	void ObserveDispatchHash(microVU& mVU, const XXH128_hash_t& contentHash,
	                          u32 startPC_bytes);

	// DevCon summary of save / hit / miss / evict counters.
	void DumpStats(const microVU& mVU, const char* tag);

	// Test-only: drop per-VU in-memory state (entries, stats, initialized
	// flag) so the next Init() re-runs the disk-handshake path. Does NOT
	// touch the on-disk cache directory; the caller is responsible for
	// that.
	void ResetForTest(u32 vu_index);

	// The running build's kMvuCompilerAbiVersion (microVU-arm64.h can't be
	// included from test TUs — it re-emits the .inl bodies). Used by the
	// ABI-digest guard test to key its pinned-digest table.
	u32 GetCompilerAbiVersion();

	// Test-only: re-run Init for the given VU using the LIVE microVU's
	// options sentinel (production Init is config-gated and only fires from
	// mVUinit/mVUreset — this lets a test point EmuFolders::Cache at a temp
	// dir and bring the disk cache up mid-process without rebuilding the
	// whole VU, regardless of EmuConfig). Rebuilds the sentinel first if a
	// recording-state change invalidated it. Call ResetForTest first.
	void TestReinitFromLiveSentinel(u32 vu_index);

	// Test-only: the given VU's live options sentinel, rebuilt first if
	// stale. Lets tests pin sentinel-identity properties (e.g. that the
	// mVUPersist recording state is mixed in — a cache of recording-enabled
	// code forms must never be served to a recording-disabled run and vice
	// versa).
	bool TestGetLiveSentinel(u32 vu_index, XXH128_hash_t* out_sentinel);

	// Test-only: block until the background payload preload spawned by Init
	// (if any) has finished. Lets tests deterministically assert the RAM-
	// served hydration path (e.g. by deleting the on-disk payload after the
	// preload and proving hydration still succeeds).
	void TestWaitForPreload(u32 vu_index);

	// Test-only: append an INDEX entry for `hash` carrying the given
	// entry-PC list, applying the same persistence path SaveProgram uses
	// (single atomic append + in-memory registration; dedupe on existing
	// hash). Returns true iff the entry was written. Lets format tests
	// drive the variable-length serializer without constructing a
	// microVU/microProgram.
	bool TestAppendIndexEntry(u32 vu_index, const XXH128_hash_t& hash,
	                          const u32* entry_pcs, size_t entry_pc_count);

	// Test-only: read back the entry-PC list the cache holds for a given
	// content hash. Returns true if the hash is in the in-memory entries
	// map and writes the entry_pcs (microMem byte offsets) into `out_pcs`;
	// false otherwise. Used by the IndexFormat tests to pin the
	// variable-length serializer's round-trip behavior.
	bool TestGetEntryPcs(u32 vu_index, const XXH128_hash_t& hash,
	                      u32* out_pcs, size_t out_pcs_cap,
	                      size_t* out_count);

	// Test-only: read back the per-VU counters DumpStats would log. Used by
	// the versioning tests to assert that eviction fired (staleEvictions++)
	// or that the cache came up cleanly.
	struct Stats
	{
		bool initialized        = false;
		bool enabled            = false;
		u64  entries            = 0;
		u64  savesAttempted     = 0;
		u64  savesWritten       = 0;
		u64  wouldBeHits        = 0;
		u64  misses             = 0;
		u64  staleEvictions     = 0;
		u64  dispatchProbes     = 0;
		u64  dispatchIndexAvail = 0;
		u64  payloadWrites      = 0; // .vuprog files written (tmp+rename)
		u64  payloadHits        = 0; // TryLoadProgram hydrations served
		u64  payloadMissing     = 0; // INDEX hit but no .vuprog on disk
		u64  payloadRejects     = 0; // payload read but hydration refused
		u64  preloadedPayloads  = 0; // .vuprog files read by the Init-time
		                             // background preload thread
		u64  preloadedBytes     = 0; // bytes currently held by the preload
		                             // map (consumed buffers are released)
		u64  preloadHits        = 0; // hydrations served from RAM instead of
		                             // a dispatch-thread disk read
	};
	Stats GetStats(u32 vu_index);
}
