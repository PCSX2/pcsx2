// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

#include <cstddef>
#include <vector>

struct microVU;
struct microProgram;
struct microBlock;
struct MvuPersistLog;

// Persisted-JIT VU program cache — relocation recorder + block-graph
// serializer.
//
// While recording is enabled, every mVU code-cache emission episode (the
// contiguous region emitted between mVUopenCodeCache and mVUcloseCodeCache)
// is captured as a "chunk" on the owning microProgram's persist log: the raw
// code bytes, the blocks whose entry points live inside it, and a fixup table
// for the only address classes that are NOT run-invariant under the
// deterministic process layout:
//
//   SelfBlockAbs    movz+movk×3 materializing a pointer into one of the
//                   program's own heap-allocated microBlock objects
//                   (&pState / &pStateEnd / the block itself).
//   BlockEntryRel26 direct B/BL to another block of the same program whose
//                   code lives in a different chunk.
//   StubRel26       direct B/BL to a per-VU dispatcher stub (exitFunct,
//                   copyPLState, ...) — re-resolved by name on hydration.
//   AdrpPage21      ADRP whose target is run-invariant (image/arena) but
//                   whose page displacement is PC-relative and must be
//                   re-paged when the chunk moves.
//
// Everything else a block bakes (absolute movz/movk or out-of-range BLR to
// image globals / C helpers, intra-chunk PC-relative branches and literal
// pools) is byte-valid at any 16-aligned slab position in any run with
// matching image/arena bases, and is verified-not-recorded.
//
// Fail-safe invariant: an address the recorder cannot classify marks the
// episode non-persistable and drops it from the log. Hydration of the
// surviving chunk prefix stays sound because chunks only ever reference
// blocks already present in the log (a chunk referencing a dropped block is
// itself dropped).
namespace mVUPersist
{
	// Master gate for emit-time recording. Default off. In production,
	// VMManager syncs it from the EmuCore/CPU/Recompiler EnableVUProgramCache
	// config bool (at CPU-provider init and on settings changes, before the
	// recompiler clear); tests and pcsx2-vurunner set it directly. Must be
	// set before the programs to persist are compiled — already-compiled
	// episodes are not retroactively recorded.
	//
	// Changing it changes emitted code forms (canonical movs / forced-long
	// cond branches), so it is mixed into the mVU options sentinel: a state
	// flip invalidates both VUs' sentinels, re-keying program identity and
	// the ProgCache VERSION handshake. SetProcessDisable(true) (the
	// offline-tooling determinism gate) overrides any enable request.
	void SetRecordingEnabled(bool enabled);
	bool IsRecordingEnabled();

	// Process-wide determinism kill switch for the offline tools. When set,
	// both the on-disk program cache and emit-time recording are forced off
	// for the whole process, making a JIT-vs-interp diff run byte-reproducible.
	// Driven by pcsx2-vurunner --no-progcache. Replaces the former
	// PCSX2_VU_PROGCACHE_DISABLE env var — no env gate in production code.
	void SetProcessDisable(bool disable);
	bool IsProcessDisabled();

	// Production recording sync: set recording to match the
	// EnableVUProgramCache config bool, unless the test-manual override is
	// engaged. Called from mVUinit/mVUreset before the options sentinel is
	// rebuilt, so the recording byte the sentinel bakes — and every program
	// compiled after the reset — reflects the live config. This is the
	// authoritative sync point (InitializeCPUProviders runs before settings
	// load the bool, so a one-shot enable there would latch the default).
	void SyncRecordingFromConfig(bool config_enabled);

	// Test-only: disable SyncRecordingFromConfig so the recompiler-test
	// harness can drive recording manually. Called once by
	// RecompilerTestEnvironment. No effect in production.
	void SetTestManualRecording(bool manual);

	// --- microVU integration points (called from the mVU core) ---

	// mVUopenCodeCache bound armAsm: begin a chunk. No-op when disabled.
	void BeginEpisode(microVU& mVU, u8* chunkBase);
	// mVUcloseCodeCache is about to unbind: finalize the chunk onto the
	// owning program's persist log (or drop it if the episode failed).
	// `chunkEnd` is the cursor after FinalizeCode (literal pool included).
	void EndEpisode(microVU& mVU, u8* chunkEnd);
	// mVUinitFirstPass registered a block with the block manager. Also
	// counts block compiles for the round-trip "no recompile" gate.
	void OnBlockCompiled(microVU& mVU, microBlock* block, u8* entry, u32 startPC_bytes);
	// mVUdeleteProg: free the program's persist log, if any.
	void OnProgramDeleted(microProgram& prog);

	// --- Serialization / hydration ---

	// Serialize a program's recorded block graph into `out`. Returns false
	// if the program has no log, the log is non-persistable, or recording
	// missed part of the program.
	bool SerializeProgram(microVU& mVU, const microProgram& prog, std::vector<u8>& out);

	// Rebuild a program from a serialized image: verifies the layout bases
	// and the content hash against live microMem, copies the chunks to the
	// current code-cache cursor, patches fixups, and registers every block
	// with a fresh microProgram (created via mVUcreateProg, so the normal
	// dispatch path resolves it through the content map). Returns the new
	// program, or nullptr on any mismatch (caller falls back to recompile).
	// Must be called with the code cache CLOSED (it opens its own episode).
	microProgram* HydrateProgram(microVU& mVU, const u8* data, size_t size);

	// --- Test hooks (vu_index-keyed so test TUs don't need microVU types) ---

	// Serialize the most-recently-created program of the given VU.
	bool TestSerializeNewestProgram(u32 vu_index, std::vector<u8>& out);
	// Hydrate into the given VU from a serialized image. Returns true on
	// success. The guest program bytes must already be in VU Micro memory
	// (the content hash is verified against them).
	bool TestHydrate(u32 vu_index, const u8* data, size_t size);
	// Compare the live code bytes of the most recent TestHydrate against the
	// chunk bytes in `image`, skipping the spans covered by fixup records.
	// Proves hydrated code is bit-identical modulo relocation operands.
	// Test-only (reads s_lastHydratedChunkBase, gated out of Release).
#ifdef PCSX2_RECOMPILER_TESTS
	bool TestVerifyHydratedCode(u32 vu_index, const u8* image, size_t size);
#endif
	// Total mVU block compiles since process start (bumped in
	// mVUinitFirstPass regardless of recording state).
	u64 GetBlockCompileCount(u32 vu_index);
	// Structural digest of the newest program's serialized image with every
	// address-bearing operand masked (sf=1 movz/movn/movk imm16, B/BL
	// imm26, ADRP imm, fixup target fields). What survives is the emitter's
	// SHAPE — opcode selection, register allocation, instruction order,
	// block/chunk/fixup structure — which is deterministic across runs,
	// machines, and PIE/ASLR. The ABI-digest guard test pins this per
	// kMvuCompilerAbiVersion: emitted-form drift without an ABI bump (=
	// stale on-disk programs would run wrong-shaped code) goes red there.
	bool TestComputeEmitDigest(u32 vu_index, u64& out_digest);

	struct Stats
	{
		u64 chunksRecorded = 0;
		u64 chunksDropped = 0;
		u64 blocksRecorded = 0;
		u64 fixupsRecorded = 0;
		u64 programsHydrated = 0;
		u64 blocksHydrated = 0;
		u64 hydrationRejects = 0;
	};
	Stats GetStats(u32 vu_index);
}
