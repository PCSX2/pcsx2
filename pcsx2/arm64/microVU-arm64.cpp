// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "microVU-arm64.h"

#include "microVU_ProgCache-arm64.h"
#include "arm64/iCore-arm64.h"
#include "vtlb.h"
#include "common/AlignedMalloc.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/Perf.h"
#include "common/StringUtil.h"
#include "SaveState.h"
#include "VU1Trace.h"
#include "vu_capture.h"

// Program-cache telemetry. Uncomment and rebuild to enable; off in
// shipped builds. Same pattern as mVUlogProg / mVUprofileProg in microVU.h.
//#define mVUcacheTrace

#include <atomic>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#ifdef mVUcacheTrace
#include <algorithm>
#include <limits>
#include <vector>
#endif

#include "fmt/format.h"

//------------------------------------------------------------------
// Micro VU - Globals
//------------------------------------------------------------------

alignas(16) microVU microVU0;
alignas(16) microVU microVU1;

//------------------------------------------------------------------
// Micro VU - Observed-entry-PC tracking on microProgram. Single-
// threaded per VU; the dispatcher records each `startPC` it hands
// off into the resolved program so additional entry trampolines
// can be emitted for previously-unseen PCs.
//------------------------------------------------------------------

bool MvuObservedEntries::record(u32 startPC_bytes)
{
	for (u32 i = 0; i < count; ++i)
	{
		if (pcs[i] == startPC_bytes)
			return false;
	}
	if (count >= kMax)
		return false;
	pcs[count++] = startPC_bytes;
	++version;
	return true;
}

void MvuObservedEntries::clear()
{
	count   = 0;
	version = 0;
	for (u32 i = 0; i < kMax; ++i)
		pcs[i] = 0;
}

//------------------------------------------------------------------
// Micro VU - Program-cache range overlap helper
//
// Returns true iff any compiled range in `prog->ranges` overlaps the byte
// interval [addr, addr+size). Used by both the cache-trace telemetry path
// (mVUCacheTraceObserveClear) and the range-aware mVUclear fast path.
//------------------------------------------------------------------

static __fi bool mVUProgRangesOverlap(const microProgram* prog, u32 addr, u32 size)
{
	if (!prog || !prog->ranges)
		return false;
	const s32 lo = static_cast<s32>(addr);
	const s32 hi = static_cast<s32>(addr + size);
	for (const auto& r : *prog->ranges)
	{
		if (r.start < hi && r.end > lo)
			return true;
	}
	return false;
}

//------------------------------------------------------------------
// Micro VU - Program-cache instrumentation (mVUcacheTrace)
//
// Enabled by uncommenting `#define mVUcacheTrace` near the top of this file.
// Tracks program-creation/clear/reset counts, deque-walk lengths, and per-VU
// dup histograms via mVUrangesHash — same machinery as x86 mVUprintUniqueRatio.
// Dumped on mVUreset and mVUclose; per-window counters reset after each dump,
// lifetime counters (resets, programs_created) persist. When the define is
// commented out (the default), every helper and call site below compiles to
// nothing.
//------------------------------------------------------------------

#ifdef mVUcacheTrace
u64 mVUrangesHash(microVU& mVU, microProgram& prog);

namespace
{
	struct mVUCacheTrace
	{
		u64 programs_created       = 0;
		u64 reset_calls            = 0;
		u64 clear_calls            = 0;
		u64 clear_real             = 0;
		u64 clear_quick_nuked      = 0;
		u64 clear_quick_would_keep = 0;
		u64 search_walks           = 0;
		u64 search_walk_total      = 0;
		u64 search_walk_max        = 0;
		u64 search_walk_min        = std::numeric_limits<u64>::max();
		u64 search_matches         = 0;
		u64 search_match_pos_total = 0;
	};

	alignas(64) mVUCacheTrace g_mVUCacheTrace[2];

	__fi void mVUCacheTraceObserveWalk(u32 vuIdx, u64 iterations, bool matched, u64 matchPos)
	{
		auto& t = g_mVUCacheTrace[vuIdx & 1];
		++t.search_walks;
		t.search_walk_total += iterations;
		if (iterations > t.search_walk_max)
			t.search_walk_max = iterations;
		if (iterations < t.search_walk_min)
			t.search_walk_min = iterations;
		if (matched)
		{
			++t.search_matches;
			t.search_match_pos_total += matchPos;
		}
	}

	void mVUCacheTraceObserveClear(microVU& mVU, u32 addr, u32 size, bool wasRealClear)
	{
		auto& t = g_mVUCacheTrace[mVU.index & 1];
		++t.clear_calls;
		if (!wasRealClear)
			return;
		++t.clear_real;
		for (u32 i = 0; i < (mVU.progSize / 2); i++)
		{
			const microProgram* p = mVU.prog.quick[i].prog;
			if (!p)
				continue;
			++t.clear_quick_nuked;
			if (!mVUProgRangesOverlap(p, addr, size))
				++t.clear_quick_would_keep;
		}
	}

	void mVUCacheTraceDump(microVU& mVU, const char* tag)
	{
		auto& t = g_mVUCacheTrace[mVU.index & 1];
		u32 dequeSlots = 0;
		u32 dequeProgs = 0;
		u32 maxBucket  = 0;
		for (u32 i = 0; i < (mVU.progSize / 2); i++)
		{
			const microProgramList* list = mVU.prog.prog[i];
			if (!list || list->empty())
				continue;
			++dequeSlots;
			const u32 sz = static_cast<u32>(list->size());
			dequeProgs += sz;
			if (sz > maxBucket)
				maxBucket = sz;
		}
		const u64 walkAvg  = t.search_walks   ? (t.search_walk_total      / t.search_walks)   : 0;
		const u64 matchAvg = t.search_matches ? (t.search_match_pos_total / t.search_matches) : 0;
		const u64 walkMin  = (t.search_walk_min == std::numeric_limits<u64>::max()) ? 0 : t.search_walk_min;
		const ConsoleColors color = mVU.index ? Color_Orange : Color_Magenta;
		DevCon.WriteLn(color,
			"mVU%u trace [%s]: created=%llu resets=%llu clears=%llu (real=%llu nuked=%llu wouldKeep=%llu) "
			"walks=%llu walkLen(min/avg/max)=%llu/%llu/%llu matches=%llu matchPosAvg=%llu "
			"liveSlots=%u liveProgs=%u maxBucket=%u",
			mVU.index, tag,
			(unsigned long long)t.programs_created,
			(unsigned long long)t.reset_calls,
			(unsigned long long)t.clear_calls,
			(unsigned long long)t.clear_real,
			(unsigned long long)t.clear_quick_nuked,
			(unsigned long long)t.clear_quick_would_keep,
			(unsigned long long)t.search_walks,
			(unsigned long long)walkMin,
			(unsigned long long)walkAvg,
			(unsigned long long)t.search_walk_max,
			(unsigned long long)t.search_matches,
			(unsigned long long)matchAvg,
			dequeSlots, dequeProgs, maxBucket);

		std::vector<u64> v;
		v.reserve(dequeProgs);
		for (u32 pc = 0; pc < (mVU.progSize / 2); pc++)
		{
			microProgramList* list = mVU.prog.prog[pc];
			if (!list)
				continue;
			for (auto it = list->begin(); it != list->end(); ++it)
				v.push_back(mVUrangesHash(mVU, *it[0]));
		}
		const u32 total = static_cast<u32>(v.size());
		std::sort(v.begin(), v.end());
		v.erase(std::unique(v.begin(), v.end()), v.end());
		if (total)
		{
			DevCon.WriteLn(color,
				"mVU%u trace [%s]: dup ratio %u unique / %u total [%3.1f%% dup]",
				mVU.index, tag,
				static_cast<u32>(v.size()), total,
				100.0 - (double)v.size() / (double)total * 100.0);
		}
	}

	void mVUCacheTraceResetWindow(u32 vuIdx)
	{
		auto& t = g_mVUCacheTrace[vuIdx & 1];
		const u64 keep_resets  = t.reset_calls;
		const u64 keep_created = t.programs_created;
		t = mVUCacheTrace{};
		t.reset_calls      = keep_resets;
		t.programs_created = keep_created;
	}
}
#endif // mVUcacheTrace

//------------------------------------------------------------------
// Micro VU - Content-hash plumbing (xxhash3-128 program identity)
//
// Builds two hashes:
//
//   mVU.optionsSentinel     — 128-bit hash of every codegen-affecting build-time
//                             constexpr (doRegAlloc / noFlagOpts / doSFlagInsts
//                             / doMFlagInsts / doCFlagInsts / doBranchInDelaySlot
//                             / doConstProp / doJumpCaching / doJumpAsSameProgram
//                             / doDBitHandling / doWholeProgCompare) plus the
//                             runtime knobs the arm64 emitter branches on (VU0/1
//                             clamp modes, FPCR bitmasks, vuFlagHack, EECycleRate
//                             / EECycleSkip, IbitHack, VUSyncHack /
//                             FullVU0SyncHack, VuAddSubHack, VUOverflowHack).
//                             Rebuilt at init and reset.
//
//   prog.contentHash        — 128-bit hash of (kMvuCompilerAbiVersion |
//                             optionsSentinel | VU index | whole microMem image
//                             as cached on prog.data). Computed by mVUcacheProg
//                             after data is filled. This is the cross-process
//                             identity used as the on-disk cache key and the
//                             in-process contentMap key.
//
// contentHash is only *populated* here — it is not yet wired into the search
// fast path, because under doWholeProgCompare=false (the default) the bytes
// outside the recorded ranges are stale and short-circuiting on the whole-image
// hash would over-restrict matches. The contentMap consumes the hash with
// whole-image semantics.
//------------------------------------------------------------------

void mVUbuildOptionsSentinel(microVU& mVU)
{
	// 64-byte fixed-layout snapshot. Order is load-bearing for stability across
	// rebuilds: changing it bumps the ABI version (kMvuCompilerAbiVersion).
	struct alignas(8) Snapshot
	{
		u32 abiVersion;
		u32 vuIndex; // 0/1 — guards against accidentally sharing sentinels across VUs

		// Build-time constexprs (microVU_Misc.h). One byte each, packed.
		u8  doRegAlloc_;
		u8  noFlagOpts_;
		u8  doSFlagInsts_;
		u8  doMFlagInsts_;
		u8  doCFlagInsts_;
		u8  doBranchInDelaySlot_;
		u8  doConstProp_;
		u8  doJumpCaching_;
		u8  doJumpAsSameProgram_;
		u8  doDBitHandling_;
		u8  doWholeProgCompare_;
		u8  pad0;

		// Clamp modes (Cpu.Recompiler.vu{0,1}{Overflow,ExtraOverflow,SignOverflow,Underflow}).
		u8  vu0Overflow;
		u8  vu0ExtraOverflow;
		u8  vu0SignOverflow;
		u8  vu0Underflow;
		u8  vu1Overflow;
		u8  vu1ExtraOverflow;
		u8  vu1SignOverflow;
		u8  vu1Underflow;

		// Speedhacks / Gamefixes that gate emit shape.
		u8  vuFlagHack;
		s8  EECycleRate;
		u8  EECycleSkip;
		u8  IbitHack;
		u8  VUSyncHack;
		u8  FullVU0SyncHack;
		u8  VuAddSubHack;
		u8  VUOverflowHack;

		// FPCR bitmasks. mVU emits MSR FPCR loads that pick between EE / VU0 /
		// VU1 FPCRs based on the configured rounding/flush bits.
		u32 fpuFpcr;
		u32 vu0Fpcr;
		u32 vu1Fpcr;
		// mVUPersist emit-time recording (the persisted-JIT program cache).
		// Recording changes emitted code forms — canonical movz+movk×3 for
		// self-block pointers, forced-long cross-chunk cond branches — so a
		// recording-enabled cache must never be matched against a recording-
		// disabled run. This field reclaims a zeroed reserved byte, so the
		// recording-OFF sentinel is bit-identical to the pre-recording one.
		u8  progCacheRecording;
		// Reserved tail so adding a future option byte doesn't shift downstream
		// fields. Reclaim bytes with 0 == "feature off / old behavior" so the
		// off-state sentinel stays bit-identical (no wholesale eviction for
		// users who never enable the feature); a reclaimed byte whose zero
		// state is NOT emission-identical needs a kMvuCompilerAbiVersion bump
		// in the same commit.
		u8  reserved[11];
	};
	static_assert(sizeof(Snapshot) == 64, "options sentinel layout drifted — bump kMvuCompilerAbiVersion");

	Snapshot s = {};
	s.abiVersion = kMvuCompilerAbiVersion;
	s.vuIndex    = mVU.index;

	s.doRegAlloc_           = doRegAlloc           ? 1 : 0;
	s.noFlagOpts_           = noFlagOpts           ? 1 : 0;
	s.doSFlagInsts_         = doSFlagInsts         ? 1 : 0;
	s.doMFlagInsts_         = doMFlagInsts         ? 1 : 0;
	s.doCFlagInsts_         = doCFlagInsts         ? 1 : 0;
	s.doBranchInDelaySlot_  = doBranchInDelaySlot  ? 1 : 0;
	s.doConstProp_          = doConstProp          ? 1 : 0;
	s.doJumpCaching_        = doJumpCaching        ? 1 : 0;
	s.doJumpAsSameProgram_  = doJumpAsSameProgram  ? 1 : 0;
	s.doDBitHandling_       = doDBitHandling       ? 1 : 0;
	s.doWholeProgCompare_   = doWholeProgCompare   ? 1 : 0;

	s.vu0Overflow      = EmuConfig.Cpu.Recompiler.vu0Overflow      ? 1 : 0;
	s.vu0ExtraOverflow = EmuConfig.Cpu.Recompiler.vu0ExtraOverflow ? 1 : 0;
	s.vu0SignOverflow  = EmuConfig.Cpu.Recompiler.vu0SignOverflow  ? 1 : 0;
	s.vu0Underflow     = EmuConfig.Cpu.Recompiler.vu0Underflow     ? 1 : 0;
	s.vu1Overflow      = EmuConfig.Cpu.Recompiler.vu1Overflow      ? 1 : 0;
	s.vu1ExtraOverflow = EmuConfig.Cpu.Recompiler.vu1ExtraOverflow ? 1 : 0;
	s.vu1SignOverflow  = EmuConfig.Cpu.Recompiler.vu1SignOverflow  ? 1 : 0;
	s.vu1Underflow     = EmuConfig.Cpu.Recompiler.vu1Underflow     ? 1 : 0;

	s.vuFlagHack      = EmuConfig.Speedhacks.vuFlagHack ? 1 : 0;
	s.EECycleRate     = static_cast<s8>(EmuConfig.Speedhacks.EECycleRate);
	s.EECycleSkip     = static_cast<u8>(EmuConfig.Speedhacks.EECycleSkip);
	s.IbitHack        = EmuConfig.Gamefixes.IbitHack         ? 1 : 0;
	s.VUSyncHack      = EmuConfig.Gamefixes.VUSyncHack       ? 1 : 0;
	s.FullVU0SyncHack = EmuConfig.Gamefixes.FullVU0SyncHack  ? 1 : 0;
	s.VuAddSubHack    = EmuConfig.Gamefixes.VuAddSubHack     ? 1 : 0;
	s.VUOverflowHack  = EmuConfig.Gamefixes.VUOverflowHack   ? 1 : 0;

	s.fpuFpcr = EmuConfig.Cpu.FPUFPCR.bitmask;
	s.vu0Fpcr = EmuConfig.Cpu.VU0FPCR.bitmask;
	s.vu1Fpcr = EmuConfig.Cpu.VU1FPCR.bitmask;

	s.progCacheRecording = mVUPersist::IsRecordingEnabled() ? 1 : 0;

	mVU.optionsSentinel      = XXH3_128bits(&s, sizeof(s));
	mVU.optionsSentinelValid = true;
}

XXH128_hash_t mVUcomputeProgramHash(microVU& mVU)
{
	if (!mVU.optionsSentinelValid)
		mVUbuildOptionsSentinel(mVU);

	// Streaming hash: ABI | sentinel | VU index | whole microMem snapshot.
	// Total input is 16 KB + ~28 B for VU1, 4 KB + ~28 B for VU0 — single shot
	// would force a temporary buffer; streaming lets us fold the prologue
	// without allocation.
	XXH3_state_t state;
	XXH3_128bits_reset(&state);

	const u32 abi = kMvuCompilerAbiVersion;
	XXH3_128bits_update(&state, &abi, sizeof(abi));
	XXH3_128bits_update(&state, &mVU.optionsSentinel, sizeof(mVU.optionsSentinel));

	const u8 idx = static_cast<u8>(mVU.index);
	XXH3_128bits_update(&state, &idx, sizeof(idx));

	XXH3_128bits_update(&state, mVU.regs().Micro, mVU.microMemSize);

	return XXH3_128bits_digest(&state);
}

//------------------------------------------------------------------
// Micro VU - contentMap helpers
//
// The contentMap is the single owner of every live microProgram. Per-startPC
// deques and quick slots are non-owning references whose lifetimes are bounded
// by mVUreset / explicit eviction. These helpers keep the map / refcount /
// deque invariants in one place so mVUsearchProg / mVUcreateProg don't open-
// code the bookkeeping.
//------------------------------------------------------------------

// Insert `prog` into the contentMap. Caller must have set `prog->contentHash`
// + `contentHashValid` already (mVUcreateProg does this immediately after
// computing the hash). Asserts the entry is unique: emplace on the unordered_map
// is a no-op if the hash already maps a program, which would silently drop the
// new prog while leaving the stale one in place.
static __fi void mVUcontentMapInsert(microVU& mVU, microProgram* prog)
{
	[[maybe_unused]] const bool inserted = mVU.mvuContentMap.emplace(prog->contentHash, prog).second;
	pxAssert(inserted);
}

// Push `prog` onto the front of `list` if not already present; bump refcount
// once per insertion. Idempotent — re-finding via contentMap shouldn't grow
// the deque past a single entry per program-per-PC.
static __fi void mVUdequePushUnique(microProgramList* list, microProgram* prog)
{
	for (microProgram* p : *list)
	{
		if (p == prog)
			return;
	}
	list->push_front(prog);
	++prog->refcount;
}

//------------------------------------------------------------------
// Micro VU - Main Functions
//------------------------------------------------------------------

void mVUinit(microVU& mVU, uint vuIndex)
{
	std::memset(&mVU.prog, 0, sizeof(mVU.prog));

	mVU.index        =  vuIndex;
	mVU.cop2         =  0;
	mVU.vuMemSize    = (mVU.index ? 0x4000 : 0x1000);
	mVU.microMemSize = (mVU.index ? 0x4000 : 0x1000);
	mVU.progSize     = (mVU.index ? 0x4000 : 0x1000) / 4;
	mVU.progMemMask  =  mVU.progSize-1;
	mVU.cache        = vuIndex ? SysMemory::GetVU1Rec() : SysMemory::GetVU0Rec();
	mVU.prog.x86end  = (vuIndex ? SysMemory::GetVU1RecEnd() : SysMemory::GetVU0RecEnd()) - (mVUcacheSafeZone * _1mb);

	mVU.regAlloc.reset(new microRegAlloc(mVU.index));

	// Persisted-JIT recording follows the config bool — established before the
	// sentinel (which bakes the recording byte). At boot this runs before
	// settings finish loading, so it typically latches off and the first
	// mVUreset corrects it. No-op under the test-manual override. See mVUreset.
	mVUPersist::SyncRecordingFromConfig(EmuConfig.Cpu.Recompiler.EnableVUProgramCache);

	// Seed options sentinel from current config snapshot. Reset will rebuild it
	// in case the user toggled clamp / FPCR / speedhack settings since init.
	mVUbuildOptionsSentinel(mVU);

	// Open the on-disk program cache for this VU. Must run after
	// mVUbuildOptionsSentinel because the VERSION-header handshake mixes the
	// sentinel; a cache built with a different options layout is evicted here.
	mVUProgCache::Init(mVU);
}

//------------------------------------------------------------------
// ARM64 Stub Dispatchers
//------------------------------------------------------------------

// Real dispatchers that enter/exit JIT blocks properly.
// Matches x86 mVUdispatcherAB pattern: save callee regs, call execute
// (returns block ptr), load VU state, jump to block, exit saves state
// and calls cleanup.

// Emit: ldr x9, [addr]; msr FPCR, x9 — switches FPCR to the value stored at
// `addr`. Called at dispatcher entry to force the VU's FPCR before JIT blocks
// run, and at exit to restore the EE's FPCR.
static void mVUemitLoadFPCR(const u64* addr)
{
	armMoveAddressToReg(a64::x8, (void*)addr);
	armAsm->Ldr(a64::x9, a64::MemOperand(a64::x8));
	armAsm->Msr(a64::FPCR, a64::x9);
}

// Mirrors x86 microVU_Execute.inl:mvuNeedsFPCRUpdate. The MTVU thread starts
// with a stale FPCR so we always reload there; otherwise reload is only
// needed when the configured EE/VU rounding modes differ.
static bool mvuNeedsFPCRUpdate(mV)
{
	if (isVU1 && THREAD_VU1)
		return true;

	return EmuConfig.Cpu.FPUFPCR.bitmask != (isVU0 ? EmuConfig.Cpu.VU0FPCR.bitmask : EmuConfig.Cpu.VU1FPCR.bitmask);
}

static void mVUdispatcherAB(mV)
{
	mVU.startFunct = armStartBlock();

	// Save callee-saved GPRs and LR
	armAsm->Stp(a64::x29, a64::x30, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::x19, a64::x20, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::x21, a64::x22, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::x23, a64::x24, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::x25, a64::x26, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::x27, a64::x28, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	// Save callee-saved NEON (d8-d15)
	armAsm->Stp(a64::d8,  a64::d9,  a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::d10, a64::d11, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::d12, a64::d13, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armAsm->Stp(a64::d14, a64::d15, a64::MemOperand(a64::sp, -16, a64::PreIndex));

	// Park PS2 FPU clamp constants. AAPCS64 preserves the lower 64 bits of
	// d8/d9 across mVUexecuteVU0/1, mVUcompile, and every other C call
	// reachable from inside this dispatcher. Matches the EE dispatcher's
	// convention so iCOP2 scalar clamps (which can execute inside macro-mode
	// VU emissions) and any future scalar FPU work share s8/s9.
	armAsm->Ldr(a64::s8, FLT_MAX);
	armAsm->Ldr(a64::s9, -FLT_MAX);

	// Inline mVUlookupProg fast path: stash w0/w1 into callee-saved
	// w26/w27, BL the lookup-only wrapper (mVUlookupProg_VU0/1) which
	// does the cycle setup + lookup but not the slow path. If the
	// lookup returns nullptr, fall through to the full mVUexecuteVU0/1
	// BL with the original args restored from w26/w27. This adds one
	// BL on the rare miss path (the slow path re-runs the cycle setup
	// harmlessly) and keeps the hit-path callee body to just the
	// lookup; the BL count on the hit path is unchanged.
	//
	// Returns compiled block entry point in x0; x0 may be nullptr if
	// the slow path's compile failed (cbz exitLabel below catches it).
	armAsm->Mov(a64::w26, a64::w0);  // stash startPC
	armAsm->Mov(a64::w27, a64::w1);  // stash cycles

	armEmitCall(isVU1 ? (void*)mVUlookupProg_VU1 : (void*)mVUlookupProg_VU0);

	a64::Label gotHostEntry;
	armAsm->Cbnz(a64::x0, &gotHostEntry);

	// Miss: restore args and run the full slow path.
	armAsm->Mov(a64::w0, a64::w26);
	armAsm->Mov(a64::w1, a64::w27);
	armEmitCall(isVU1 ? (void*)mVUexecuteVU1 : (void*)mVUexecuteVU0);

	armAsm->Bind(&gotHostEntry);
	// x0 holds the block pointer; we keep it there until the Br below.
	// FPCR setup, gprVUState pin, and the flag/PQ loads emit no calls and
	// don't touch x0, so it survives across them.

	// Pin gprVUState (x19) = &mVU.regs(). All subsequent regs() field accesses
	// (here in the dispatcher and in compiled blocks) use [gprVUState, #off]
	// instead of paying the 3-insn movz/movk/movk address-materialization tax.
	// Address is constant per-VU (= &vuRegs[mVU.index]), so we set this once
	// per dispatch and never re-pin. Survives armEmitCall (callee-saved).
	armMoveAddressToReg(gprVUState, &mVU.regs());

	// Pin gprMVUFlag (x24) = &mVU.macFlag[0]. Reaches statFlag / macFlag /
	// clipFlag / neonCTemp / neonBackup via signed [+/-imm12] (see
	// microVU_Misc-arm64.h). Lets every flag-touching FMAC drop the 3-insn
	// abs-addr materialization down to a single ldr/str.
	armMoveAddressToReg(gprMVUFlag, mVU.macFlag);

	// Pin gprMVUglob (x25) = &mVUglob. Every clamp / FTOI / ITOF / EATAN /
	// SQRT et al. constant load goes via [gprMVUglob, #imm12] instead of
	// materializing the global's absolute address.
	armMoveAddressToReg(gprMVUglob, (void*)&mVUglob);

	// Load VU-specific FPCR (round-toward-zero + FZ/DaZ) — only when needed.
	// PS2 VU float ops require this rounding mode; the gating skips the
	// reload when EE and VU FPCR configs already match (the default).
	if (mvuNeedsFPCRUpdate(mVU))
		mVUemitLoadFPCR(isVU0 ? &EmuConfig.Cpu.VU0FPCR.bitmask : &EmuConfig.Cpu.VU1FPCR.bitmask);

	// Load macro/clip flags from VU state into microVU shadow copies via the
	// pinned base. Shadow copies live in `microVU` not `VURegs`.
	armAsm->Ldr(a64::q0, mVUstateMem(offsetof(VURegs, micro_macflags)));
	armAsm->Str(a64::q0, a64::MemOperand(gprMVUFlag));

	armAsm->Ldr(a64::q0, mVUstateMem(offsetof(VURegs, micro_clipflags)));
	armAsm->Str(a64::q0, a64::MemOperand(gprMVUFlag, 16));

	// Load status flag instances into callee-saved GPRs
	armAsm->Ldr(gprF0, mVUstateMem(offsetof(VURegs, micro_statusflags) + 0));
	armAsm->Ldr(gprF1, mVUstateMem(offsetof(VURegs, micro_statusflags) + 4));
	armAsm->Ldr(gprF2, mVUstateMem(offsetof(VURegs, micro_statusflags) + 8));
	armAsm->Ldr(gprF3, mVUstateMem(offsetof(VURegs, micro_statusflags) + 12));

	// Load P/Q into qmmPQ
	// x86 packs P, Q, pending_q, pending_p into xmmPQ via shuffles.
	// For now, load Q and pending_q into lanes 0,1 (P is VU1-only).
	armAsm->Ldr(a64::s0, mVUstateMem(offsetof(VURegs, VI) + REG_Q * sizeof(REG_VI)));
	armAsm->Ldr(a64::s1, mVUstateMem(offsetof(VURegs, pending_q)));
	// Pack into qmmPQ: [0]=Q, [1]=pending_q, [2]=P, [3]=pending_p
	armAsm->Ins(qmmPQ.V4S(), 0, a64::q0.V4S(), 0);
	armAsm->Ins(qmmPQ.V4S(), 1, a64::q1.V4S(), 0);
	if (isVU1)
	{
		armAsm->Ldr(a64::s0, mVUstateMem(offsetof(VURegs, VI) + REG_P * sizeof(REG_VI)));
		armAsm->Ldr(a64::s1, mVUstateMem(offsetof(VURegs, pending_p)));
		armAsm->Ins(qmmPQ.V4S(), 2, a64::q0.V4S(), 0);
		armAsm->Ins(qmmPQ.V4S(), 3, a64::q1.V4S(), 0);
	}

	// Jump to compiled block (address still in x0 from mVUexecuteVU return).
	// Safety: if block ptr is NULL, fall through to exit path
	a64::Label exitLabel;
	armAsm->Cbz(a64::x0, &exitLabel);
	armAsm->Br(a64::x0);

	// === Exit path === (blocks jump here when done)
	armAsm->Bind(&exitLabel);
	mVU.exitFunct = armGetCurrentCodePointer();

	// Restore EE FPCR before returning to C++ (mVUcleanUp + caller) — same
	// gating as the entry path.
	if (mvuNeedsFPCRUpdate(mVU))
		mVUemitLoadFPCR(&EmuConfig.Cpu.FPUFPCR.bitmask);

	// Save status flags back to VU state via gprVUState (still pinned across
	// the block-exit path; restored only by the final Ldp below).
	armAsm->Str(gprF0, mVUstateMem(offsetof(VURegs, micro_statusflags) + 0));
	armAsm->Str(gprF1, mVUstateMem(offsetof(VURegs, micro_statusflags) + 4));
	armAsm->Str(gprF2, mVUstateMem(offsetof(VURegs, micro_statusflags) + 8));
	armAsm->Str(gprF3, mVUstateMem(offsetof(VURegs, micro_statusflags) + 12));

	// mVUcleanUp logic inlined in this exit stub. The C++ helper (mVUcleanUpVU0/1)
	// is only called on the rare bounds-violation path (program cache exhausted →
	// reset). Cycle accounting and the EE-cycle-skip math (the common cost)
	// are emitted inline so dispatch costs no `bl` on the hot path.
	//
	// 1) Cycle accounting (always hot).
	// 2) Cache-bounds check (rare false): if out-of-range, tail to C++ which
	//    runs mVUreset + EE-skip math + profiler.Print().
	// 3) EE cycle skip math (inline; default EECycleSkip=0 short-circuits at
	//    the first cbz). VU1+THREAD_VU1 skips the math (matches C++).
	// 4) profiler.Print() is __fi {} in default builds → elided.
	//
	// gprVUState (x19) stays pinned to &mVU.regs() across this whole stub.
	a64::Label cleanUpReturn, cleanUpResetTail, eeSkipDone;

	// (1) Cache-bounds check: out-of-range → reset tail. This MUST precede the
	// inline cycle math below. On the reset path the C++ mVUcleanUp re-runs the
	// same cycle accounting (microVU_Execute.inl: mVU.cycles = totalCycles -
	// max(0,mVU.cycles); mVU.regs().cycle += mVU.cycles), so if the inline math
	// ran first regs().cycle would be incremented twice — double-counting VU
	// cycles up to totalCycles. Branching before the inline math leaves exactly
	// one cycle update on each path: inline on the normal path, C++ on the reset
	// path. x86ptr / x86start / x86end are three consecutive 8-byte
	// fields in microProgManager.
	static_assert(offsetof(microProgManager, x86start) == offsetof(microProgManager, x86ptr) + 8,
		"inline bounds check expects x86ptr/x86start/x86end adjacent");
	static_assert(offsetof(microProgManager, x86end)   == offsetof(microProgManager, x86ptr) + 16,
		"inline bounds check expects x86ptr/x86start/x86end adjacent");
	armMoveAddressToReg(a64::x8, &mVU.prog.x86ptr);
	armAsm->Ldr(a64::x9,  a64::MemOperand(a64::x8));            // x86ptr
	armAsm->Ldr(a64::x10, a64::MemOperand(a64::x8, 8));         // x86start
	armAsm->Cmp(a64::x9, a64::x10);
	armAsm->B(&cleanUpResetTail, a64::lt);
	armAsm->Ldr(a64::x10, a64::MemOperand(a64::x8, 16));        // x86end
	armAsm->Cmp(a64::x9, a64::x10);
	armAsm->B(&cleanUpResetTail, a64::ge);

	// (2) Cycle math (inline; normal in-range path only — the reset tail lets
	// the C++ mVUcleanUp do this exactly once instead).
	armMoveAddressToReg(a64::x8, &mVU.totalCycles);
	armAsm->Ldr(a64::w10, a64::MemOperand(a64::x8));           // totalCycles
	armAsm->Ldr(a64::w9,  a64::MemOperand(a64::x8, 4));        // cycles
	armAsm->Cmp(a64::w9, 0);
	armAsm->Csel(a64::w9, a64::w9, a64::wzr, a64::gt);          // max(0, cycles)
	armAsm->Sub(a64::w9, a64::w10, a64::w9);                    // totalCycles - max(0,c)
	armAsm->Str(a64::w9, a64::MemOperand(a64::x8, 4));          // mVU.cycles = ...
	// VURegs::cycle is u64 (matching cpuRegs.cycle); the add MUST be 64-bit or
	// the carry is dropped once the low 32 bits wrap (~every 14s of EE time),
	// leaving VU0.cycle ~4 billion below the EE clock and detonating every
	// _vu0run (s64)(cpuRegs.cycle - VU0.cycle) sync. x86 mVUcleanUp does the same
	// u64 += s32 cycle add; a 32-bit add is only correct when cycle fields
	// are u32 (prior to widening to u64). w9 holds the consumed
	// cycle count (s32, non-negative here) — sign-extend into the 64-bit add.
	armAsm->Ldr(a64::x10, mVUstateMem(offsetof(VURegs, cycle)));
	armAsm->Add(a64::x10, a64::x10, a64::Operand(a64::w9, a64::SXTW));
	armAsm->Str(a64::x10, mVUstateMem(offsetof(VURegs, cycle)));

	// (3) EE cycle skip math (inline). Equivalent C++ (mVUcleanUp body):
	//     u32 cycles_passed = std::min(mVU.cycles, 3000) * EECycleSkip;
	//     if (cycles_passed > 0) {
	//         cpuRegs.cycle += cycles_passed;   // u64 += u32
	//         VU0.cycle     += cycles_passed;   // u64 += u32
	//     }
	// Both arms of the C++ if (`!vuIndex` and `else`) collapse to the same
	// arithmetic effect (VU0.cycle += cycles_passed) because
	//   VU0.cycle = (cpuRegs.cycle + cycles_passed) + (VU0.cycle - cpuRegs.cycle)
	//             = VU0.cycle + cycles_passed.
	// The 64-bit add is preserved here so the long-running cpuRegs.cycle
	// counter doesn't lose the carry across its low-32-bit boundary
	// (cpuRegs.cycle wraps low-32 every ~14 s of EE time at 1.0×).
	//
	// VU1+THREAD_VU1 skips the math entirely — MTVU runs the dispatcher on
	// the VU1 thread, where touching cpuRegs.cycle is wrong (matches C++).
	armMoveAddressToReg(a64::x8, &EmuConfig.Speedhacks.EECycleSkip);
	armAsm->Ldrb(a64::w11, a64::MemOperand(a64::x8));
	armAsm->Cbz(a64::w11, &eeSkipDone);                         // EECycleSkip == 0 → skip

	if (isVU1)
	{
		// THREAD_VU1 = REC_VU1 && Speedhacks.vuThread.
		// EnableVU1 = bit 3 of EmuConfig.Cpu.Recompiler.bitset.
		// vuThread  = bit 4 of EmuConfig.Speedhacks.bitset.
		a64::Label vu1DoEEAdjust;
		armMoveAddressToReg(a64::x8, &EmuConfig.Cpu.Recompiler.bitset);
		armAsm->Ldr(a64::w10, a64::MemOperand(a64::x8));
		armAsm->Tbz(a64::w10, 3, &vu1DoEEAdjust);                // !EnableVU1 → !THREAD_VU1
		armMoveAddressToReg(a64::x8, &EmuConfig.Speedhacks.bitset);
		armAsm->Ldr(a64::w10, a64::MemOperand(a64::x8));
		armAsm->Tbnz(a64::w10, 4, &eeSkipDone);                  // vuThread → THREAD_VU1, skip
		armAsm->Bind(&vu1DoEEAdjust);
	}

	// cycles_passed = min(mVU.cycles, 3000) * EECycleSkip (w11)
	armMoveAddressToReg(a64::x8, &mVU.totalCycles);
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8, 4));           // mVU.cycles (post-update)
	armAsm->Mov(a64::w10, 3000);
	armAsm->Cmp(a64::w9, a64::w10);
	armAsm->Csel(a64::w9, a64::w9, a64::w10, a64::lt);            // min(cycles, 3000)
	armAsm->Mul(a64::w9, a64::w9, a64::w11);                      // * EECycleSkip; W-write zeroes top of x9
	armAsm->Cbz(a64::w9, &eeSkipDone);                            // cycles_passed == 0 → skip

	// cpuRegs.cycle (u64) += cycles_passed (zero-extended in x9 by the Mul).
	armMoveAddressToReg(a64::x8, &cpuRegs.cycle);
	armAsm->Ldr(a64::x10, a64::MemOperand(a64::x8));
	armAsm->Add(a64::x10, a64::x10, a64::x9);
	armAsm->Str(a64::x10, a64::MemOperand(a64::x8));

	// VU0.cycle (u64) += cycles_passed.
	if (isVU0)
	{
		// gprVUState already points at &VU0; piggyback off the pin.
		armAsm->Ldr(a64::x10, mVUstateMem(offsetof(VURegs, cycle)));
		armAsm->Add(a64::x10, a64::x10, a64::x9);
		armAsm->Str(a64::x10, mVUstateMem(offsetof(VURegs, cycle)));
	}
	else
	{
		// VU1 dispatcher: gprVUState points at &VU1, so address &VU0 directly.
		armMoveAddressToReg(a64::x8, &VU0.cycle);
		armAsm->Ldr(a64::x10, a64::MemOperand(a64::x8));
		armAsm->Add(a64::x10, a64::x10, a64::x9);
		armAsm->Str(a64::x10, a64::MemOperand(a64::x8));
	}

	armAsm->Bind(&eeSkipDone);
	armAsm->B(&cleanUpReturn);

	// Rare reset tail. C++ helper does the bounds check + mVUreset + the cycle
	// accounting + the EE-skip math + profiler.Print(). On this bounds-violated
	// path the inline cycle math (2) and inline EE-skip (3) above are BOTH
	// skipped — the bounds branch precedes them — so the C++ mVUcleanUp performs
	// each exactly once with no double-counting. ≪0.01% of dispatches.
	armAsm->Bind(&cleanUpResetTail);
	armEmitCall(isVU1 ? (void*)mVUcleanUpVU1 : (void*)mVUcleanUpVU0);

	armAsm->Bind(&cleanUpReturn);

	// Restore callee-saved NEON
	armAsm->Ldp(a64::d14, a64::d15, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::d12, a64::d13, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::d10, a64::d11, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::d8,  a64::d9,  a64::MemOperand(a64::sp, 16, a64::PostIndex));
	// Restore callee-saved GPRs and LR
	armAsm->Ldp(a64::x27, a64::x28, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::x25, a64::x26, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::x23, a64::x24, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::x21, a64::x22, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::x19, a64::x20, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ldp(a64::x29, a64::x30, a64::MemOperand(a64::sp, 16, a64::PostIndex));

	armAsm->Ret();

	u8* end = armEndBlock();

	Perf::any.Register(mVU.startFunct, static_cast<u32>(end - mVU.startFunct),
		mVU.index ? "VU1StartFunc" : "VU0StartFunc");
}

static void mVUdispatcherCD(mV)
{
	// XGkick resume dispatcher: a bare Ret. The resume-from-XGKICK-break path
	// that this would jump into is #if-0'd out upstream (resumePtrXG is only
	// written from that dead block), so there are no callers and the stub never
	// needs to restore state or jump anywhere.
	mVU.startFunctXG = armStartBlock();
	armAsm->Ret();
	u8* end = armEndBlock();
	mVU.exitFunctXG = end;

	Perf::any.Register(mVU.startFunctXG, static_cast<u32>(end - mVU.startFunctXG),
		mVU.index ? "VU1StartFuncXG" : "VU0StartFuncXG");
}

static void mVUGenerateWaitMTVU(mV)
{
	mVU.waitMTVU = armStartBlock();
	armAsm->Ret();
	armEndBlock();
}

static void mVUGenerateCopyPipelineState(mV)
{
	mVU.copyPLState = armStartBlock();

	// x0 = source pointer to microRegInfo (96 bytes)
	// Copy 96 bytes (6 x 16-byte loads) to mVU.prog.lpState
	const a64::Register src = a64::x0;

	armMoveAddressToReg(a64::x1, &mVU.prog.lpState);

	// 96 bytes = 6 x LDR/STR Q or 3 x LDP/STP Q
	armAsm->Ldp(a64::q0, a64::q1, a64::MemOperand(src, 0));
	armAsm->Ldp(a64::q2, a64::q3, a64::MemOperand(src, 32));
	armAsm->Ldp(a64::q4, a64::q5, a64::MemOperand(src, 64));

	armAsm->Stp(a64::q0, a64::q1, a64::MemOperand(a64::x1, 0));
	armAsm->Stp(a64::q2, a64::q3, a64::MemOperand(a64::x1, 32));
	armAsm->Stp(a64::q4, a64::q5, a64::MemOperand(a64::x1, 64));

	armAsm->Ret();
	armEndBlock();
}

// Emit the two SFLAGc + microflag tail helpers used by mVUendProgram.
// Each exit thunk calls one of these helpers with a single mov+bl pair
// rather than inlining the full STATUS-denorm + micro_flag
// backup-or-broadcast sequence, keeping per-thunk code size small.
//
// ABI:
//   Input  : w11 (gprT3) = caller-evaluated getFlagReg(fStatus) value
//   Clobbers: w9 (gprT1), w11, q0/v0
//   Reads  : pinned x19 (gprVUState), x24 (gprMVUFlag), w20..w23 (gprF0..3)
//   Returns via ret using LR set by the bl at the caller
static void mVUGenerateEndProgramFlagsHelper(mV)
{
	auto emitSFLAGc = [&]() {
		// Mirrors mVUallocSFLAGc body byte-for-byte but with reg=w9, regT=w11
		armAsm->Mov(a64::w9, 0);
		auto setBit = [&](int bitTest, int bitSet) {
			armAsm->Tst(a64::w11, bitTest);
			a64::Label skip;
			armAsm->B(&skip, a64::eq);
			armAsm->Orr(a64::w9, a64::w9, bitSet);
			armAsm->Bind(&skip);
		};
		setBit(0x0f00, 0x0001); // Z  bit
		setBit(0xf000, 0x0002); // S  bit
		setBit(0x000f, 0x0040); // ZS bit
		setBit(0x00f0, 0x0080); // SS bit
		armAsm->And(a64::w11, a64::w11, 0xffff0000u);
		armAsm->Lsr(a64::w11, a64::w11, 14);
		armAsm->Orr(a64::w9, a64::w9, a64::w11);
		armAsm->Str(a64::w9,
			mVUstateMem(offsetof(VURegs, VI) + REG_STATUS_FLAG * sizeof(REG_VI)));
	};

	// Helper A — non-Ebit (isEbit == 0 || isEbit == 3): backup all 4 flag
	// instances into micro_*flags[] for block-link restore.
	mVU.endProgramFlagsA = armStartBlock();
	{
		emitSFLAGc();
		armAsm->Ldr(a64::q0, a64::MemOperand(gprMVUFlag));
		armAsm->Str(a64::q0, mVUstateMem(offsetof(VURegs, micro_macflags)));
		armAsm->Ldr(a64::q0, a64::MemOperand(gprMVUFlag, 16));
		armAsm->Str(a64::q0, mVUstateMem(offsetof(VURegs, micro_clipflags)));
		armAsm->Str(gprF0, mVUstateMem(offsetof(VURegs, micro_statusflags) + 0));
		armAsm->Str(gprF1, mVUstateMem(offsetof(VURegs, micro_statusflags) + 4));
		armAsm->Str(gprF2, mVUstateMem(offsetof(VURegs, micro_statusflags) + 8));
		armAsm->Str(gprF3, mVUstateMem(offsetof(VURegs, micro_statusflags) + 12));
		armAsm->Ret();
	}
	armEndBlock();

	// Helper B — Ebit (isEbit && isEbit != 3): broadcast the just-stored
	// MAC/CLIP/STATUS values across all 4 instances. The caller must have
	// already stored MAC_FLAG and CLIP_FLAG to VURegs before calling
	// (per-callsite, because fMac/fClip vary). Broadcast happens before
	// SFLAGc because SFLAGc destroys w11.
	mVU.endProgramFlagsB = armStartBlock();
	{
		armAsm->Ldr(a64::w9,
			mVUstateMem(offsetof(VURegs, VI) + REG_CLIP_FLAG * sizeof(REG_VI)));
		armAsm->Dup(a64::q0.V4S(), a64::w9);
		armAsm->Str(a64::q0, mVUstateMem(offsetof(VURegs, micro_clipflags)));
		armAsm->Ldr(a64::w9,
			mVUstateMem(offsetof(VURegs, VI) + REG_MAC_FLAG * sizeof(REG_VI)));
		armAsm->Dup(a64::q0.V4S(), a64::w9);
		armAsm->Str(a64::q0, mVUstateMem(offsetof(VURegs, micro_macflags)));
		armAsm->Dup(a64::q0.V4S(), a64::w11);
		armAsm->Str(a64::q0, mVUstateMem(offsetof(VURegs, micro_statusflags)));

		emitSFLAGc();
		armAsm->Ret();
	}
	armEndBlock();
}

// Resets Rec Data
void mVUreset(microVU& mVU, bool resetReserve)
{
#ifdef mVUcacheTrace
	mVUCacheTraceDump(mVU, "pre-reset");
	++g_mVUCacheTrace[mVU.index & 1].reset_calls;
#endif

	// Persisted-JIT recording follows the EnableVUProgramCache config bool, and
	// MUST be established here — before mVUbuildOptionsSentinel bakes the
	// recording byte and before any gameplay program compiles. This is the
	// authoritative sync point: InitializeCPUProviders runs before settings
	// load the bool, so a one-shot enable there would latch the default (off)
	// and never correct, leaving the disk cache writing telemetry-only entries
	// with no payloads. The disk Init below is re-synced on the same reset, so
	// recording and the cache move in lockstep. No-op under the test-manual
	// override (the recompiler-test harness drives recording itself).
	mVUPersist::SyncRecordingFromConfig(EmuConfig.Cpu.Recompiler.EnableVUProgramCache);

	// Rebuild options sentinel before any program rebuilds — config may have
	// changed since the last init/reset (clamp flips, FPCR edits, speedhack
	// toggles, gamefix overrides). New programs created after this reset will
	// hash against the up-to-date sentinel.
	mVUbuildOptionsSentinel(mVU);

	// Re-enter the disk-cache init: a no-op when already up (or when the
	// EnableVUProgramCache config bool is off), but the activation point when
	// the user toggled the cache on — settings changes funnel through
	// ClearCPUExecutionCaches → this reset.
	mVUProgCache::Init(mVU);

	if (THREAD_VU1)
	{
		DevCon.Warning("mVU Reset");
		if (VU0.VI[REG_VPU_STAT].UL & 0x100)
		{
			CpuVU1->Execute(vu1RunCycles);
		}
		VU0.VI[REG_VPU_STAT].UL &= ~0x100;
	}

	// Set up the code cache for vixl emission. Capacity deliberately runs
	// mVUcacheSafeZone past prog.x86end, i.e. to the PHYSICAL end of the rec
	// region: x86end is only the mVUcleanUp reset threshold, and the safe
	// zone beyond it exists so an in-flight compile session can overshoot
	// the threshold and still emit — the bounds check runs at dispatcher
	// exit, after the session (matching x86, where the raw emitter does
	// exactly this). Binding capacity at x86end instead makes vixl abort
	// (CodeBuffer::Grow on an unmanaged buffer) the moment the cache fills,
	// turning every cache exhaustion into a SIGABRT with the reset path
	// unreachable (SM8650 OutRun 2006 core dumps, 2026-07-02).
	const size_t cacheCapacity =
		static_cast<size_t>(mVU.prog.x86end - mVU.cache) + (mVUcacheSafeZone * _1mb);
	armSetAsmPtr(mVU.cache, cacheCapacity, nullptr);

	mVUdispatcherAB(mVU);
	mVUdispatcherCD(mVU);
	mVUGenerateWaitMTVU(mVU);
	mVUGenerateCopyPipelineState(mVU);
	mVUGenerateEndProgramFlagsHelper(mVU);

	mVU.regs().nextBlockCycles = 0;
	memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));
	mVU.profiler.Reset(mVU.index);

	// Program Variables
	mVU.prog.cleared  =  1;
	mVU.prog.isSame   = -1;
	mVU.prog.cur      = NULL;
	mVU.prog.total    =  0;
	mVU.prog.curFrame =  0;

	// Setup Dynarec Cache Limits for Each Program
	// Note: armAsm is null between blocks, so use armGetAsmPtr() directly
	mVU.prog.x86start = armGetAsmPtr();
	mVU.prog.x86ptr   = mVU.prog.x86start;

	// Build the persistent MacroAssembler over the post-dispatcher region
	// of the code cache. From here on, mVUopenCodeCache binds armAsm to
	// this MA instead of allocating a fresh one per dispatch. Capacity runs
	// to the physical rec-region end, mVUcacheSafeZone past the x86end reset
	// threshold, for the same reason as cacheCapacity above — a session that
	// overshoots x86end must complete so mVUcleanUp can reset afterward.
	// mVUopenCodeCache's armAsmCapacity mirrors this value.
	{
		namespace a64 = vixl::aarch64;
		const size_t blockCacheCapacity =
			static_cast<size_t>(mVU.prog.x86end - mVU.prog.x86start) + (mVUcacheSafeZone * _1mb);
		mVU.jitAsm = std::make_unique<a64::MacroAssembler>(
			static_cast<vixl::byte*>(mVU.prog.x86start), blockCacheCapacity);
		mVU.jitAsm->GetScratchVRegisterList()->Remove(31);
		mVU.jitAsm->GetScratchRegisterList()->Remove(RSCRATCHADDR.GetCode());
	}

	// Checkpoint the live programs to the on-disk program cache before we
	// free them. Subsequent process boots can hit this VU image without
	// re-emitting. SaveAllPrograms is idempotent (no-op on hashes already
	// in the on-disk INDEX) so calling on every reset is cheap.
	mVUProgCache::SaveAllPrograms(mVU);

	// Single ownership lives in mVU.mvuContentMap. Free each program
	// exactly once via the map iteration; per-PC deques and quick slots are
	// non-owning references and just get cleared.
	for (auto& entry : mVU.mvuContentMap)
	{
		microProgram* prog = entry.second;
		mVUdeleteProg(mVU, prog);
	}
	mVU.mvuContentMap.clear();

	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		if (!mVU.prog.prog[i])
		{
			mVU.prog.prog[i] = new std::deque<microProgram*>();
			continue;
		}
		mVU.prog.prog[i]->clear();
		mVU.prog.quick[i].block = NULL;
		mVU.prog.quick[i].prog = NULL;
	}

#ifdef mVUcacheTrace
	mVUCacheTraceResetWindow(mVU.index);
#endif
}

// Free Allocated Resources
void mVUclose(microVU& mVU)
{
#ifdef mVUcacheTrace
	mVUCacheTraceDump(mVU, "shutdown");
#endif

	// Final checkpoint of live programs to the on-disk cache before we let
	// the contentMap go. Mirrors the mVUreset path; harmless if nothing
	// new has been added since the last reset.
	mVUProgCache::SaveAllPrograms(mVU);
	mVUProgCache::Close(mVU);

	// Same ownership rule as mVUreset: free via contentMap, then
	// drop the per-PC deque shells.
	for (auto& entry : mVU.mvuContentMap)
	{
		microProgram* prog = entry.second;
		mVUdeleteProg(mVU, prog);
	}
	mVU.mvuContentMap.clear();

	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		if (!mVU.prog.prog[i])
			continue;
		safe_delete(mVU.prog.prog[i]);
	}
}

// Clears Block Data in specified range
__fi void mVUclear(mV, u32 addr, u32 size)
{
#ifdef mVUcacheTrace
	mVUCacheTraceObserveClear(mVU, addr, size, /*wasRealClear=*/!mVU.prog.cleared);
#endif

	if (doWholeProgCompare)
	{
		// Whole-program compare — every program cares about every byte in
		// microMem, so any overlap check is moot. Fall back to the original
		// unconditional invalidate.
		if (!mVU.prog.cleared)
		{
			mVU.prog.cleared = 1;
			std::memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));
			for (u32 i = 0; i < (mVU.progSize / 2); i++)
			{
				mVU.prog.quick[i].block = NULL;
				mVU.prog.quick[i].prog  = NULL;
			}
		}
		return;
	}

	// Range-aware path: only invalidate quick[i] whose cached program has a
	// compiled range overlapping [addr, addr+size). Programs whose ranges are
	// disjoint from the touched bytes stay quick-cached, skipping the per-PC
	// deque walk on the next dispatch.
	bool anyInvalidated = false;
	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		const microProgram* p = mVU.prog.quick[i].prog;
		if (!p)
			continue;
		if (mVUProgRangesOverlap(p, addr, size))
		{
			mVU.prog.quick[i].block = NULL;
			mVU.prog.quick[i].prog  = NULL;
			anyInvalidated = true;
		}
	}

	// lpState + cleared bookkeeping: only set cleared=1 / zero lpState when we
	// actually invalidated something. Otherwise we leave the existing pipeline
	// state intact — surviving quick.prog entries will re-enter with the same
	// lpState they exited with, which remains valid because their compiled
	// bytes weren't touched.
	if (anyInvalidated && !mVU.prog.cleared)
	{
		mVU.prog.cleared = 1;
		std::memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));
	}
}

//------------------------------------------------------------------
// Micro VU - Private Functions
//------------------------------------------------------------------

__ri void mVUdeleteProg(microVU& mVU, microProgram*& prog)
{
	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		safe_delete(prog->block[i]);
	}
	safe_delete(prog->ranges);
	mVUPersist::OnProgramDeleted(*prog);
	safe_aligned_free(prog);
}

__ri microProgram* mVUcreateProg(microVU& mVU, int startPC)
{
#ifdef mVUcacheTrace
	++g_mVUCacheTrace[mVU.index & 1].programs_created;
#endif

	microProgram* prog = (microProgram*)_aligned_malloc(sizeof(microProgram), 64);
	memset(prog, 0, sizeof(microProgram));
	prog->idx = mVU.prog.total++;
	prog->ranges = new std::deque<microRange>();
	prog->startPC = startPC;
	prog->refcount = 0; // Caller increments when pushing into a per-PC deque.
	// Record the creator's startPC (microMem byte offset) as the first
	// observed entry. mVUsearchProg appends any additional PCs seen
	// during the program's lifetime.
	prog->observed.clear();
	prog->observed.record(static_cast<u32>(startPC) * 8u);
	if(doWholeProgCompare)
		mVUcacheProg(mVU, *prog);

	// Anchor the program's content identity from live microMem and register it
	// in the per-VU contentMap. Done here (not in mVUcacheProg) so the hash is
	// set even under !doWholeProgCompare where mVUcacheProg fires only later
	// from mVUsetupRange. The hash stays stable for the program's lifetime —
	// subsequent re-caches don't shift the contentMap key.
	prog->contentHash      = mVUcomputeProgramHash(mVU);
	prog->contentHashValid = true;
	mVUcontentMapInsert(mVU, prog);

	double cacheSize = (double)((uptr)mVU.prog.x86end - (uptr)mVU.prog.x86start);
	double cacheUsed = ((double)((uptr)mVU.prog.x86ptr - (uptr)mVU.prog.x86start)) / (double)_1mb;
	double cachePerc = ((double)((uptr)mVU.prog.x86ptr - (uptr)mVU.prog.x86start)) / cacheSize * 100;
	ConsoleColors c = mVU.index ? Color_Orange : Color_Magenta;
	DevCon.WriteLn(c, "microVU%d: Cached Prog = [%03d] [PC=%04x] [List=%02d] (Cache=%3.3f%%) [%3.1fmb]",
		mVU.index, prog->idx, startPC * 8, mVU.prog.prog[startPC]->size() + 1, cachePerc, cacheUsed);
	return prog;
}

__ri void mVUcacheProg(microVU& mVU, microProgram& prog)
{
	if (!doWholeProgCompare)
	{
		auto cmpOffset = [&](void* x) { return (u8*)x + mVUrange.start; };
		memcpy(cmpOffset(prog.data), cmpOffset(mVU.regs().Micro), (mVUrange.end - mVUrange.start));
	}
	else
	{
		if (!mVU.index)
			memcpy(prog.data, mVU.regs().Micro, 0x1000);
		else
			memcpy(prog.data, mVU.regs().Micro, 0x4000);
	}
	mVUdumpProg(mVU, prog);

	// Do NOT recompute contentHash here. The hash is anchored at mVUcreateProg
	// from live microMem and pinned for the program's lifetime so the contentMap
	// key stays stable across mVUsetupRange-driven re-caches (range expansions
	// don't change identity).
}

u64 mVUrangesHash(microVU& mVU, microProgram& prog)
{
	union
	{
		u64 v64;
		u32 v32[2];
	} hash = {0};

	std::deque<microRange>::const_iterator it(prog.ranges->begin());
	for (; it != prog.ranges->end(); ++it)
	{
		if ((it[0].start < 0) || (it[0].end < 0))
		{
			DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU.index, it[0].start, it[0].end);
		}
		for (int i = it[0].start / 4; i < it[0].end / 4; i++)
		{
			hash.v32[0] -= prog.data[i];
			hash.v32[1] ^= prog.data[i];
		}
	}
	return hash.v64;
}

__fi bool mVUcmpProg(microVU& mVU, microProgram& prog)
{
	if (doWholeProgCompare)
	{
		if (memcmp((u8*)prog.data, mVU.regs().Micro, mVU.microMemSize))
			return false;
	}
	else
	{
		for (const auto& range : *prog.ranges)
		{
#if defined(PCSX2_DEVBUILD) || defined(_DEBUG)
			if ((range.start < 0) || (range.end < 0))
				DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU.index, range.start, range.end);
#endif
			auto cmpOffset = [&](void* x) { return (u8*)x + range.start; };

			if (memcmp(cmpOffset(prog.data), cmpOffset(mVU.regs().Micro), (range.end - range.start)))
				return false;
		}
	}
	mVU.prog.cleared = 0;
	mVU.prog.cur = &prog;
	mVU.prog.isSame = doWholeProgCompare ? 1 : -1;
	return true;
}

// Searches for Cached Micro Program and sets prog.cur to it
_mVUt __fi void* mVUsearchProg(u32 startPC, uptr pState)
{
	microVU& mVU = mVUx;
	microProgramQuick& quick = mVU.prog.quick[mVU.regs().start_pc / 8];
	microProgramList*  list  = mVU.prog.prog [mVU.regs().start_pc / 8];

	if (!quick.prog)
	{
		// Cross-PC content-hash fast path. Hash the live microMem once;
		// a contentMap hit reuses the existing microProgram across all
		// startPCs that produce the same image, without paying the per-PC
		// deque walk. Trusts the 128-bit xxh3 hash as the identity (no
		// memcmp confirm — collision odds are astronomical and the on-disk
		// cache uses the same key).
		const XXH128_hash_t liveHash = mVUcomputeProgramHash(mVU);
		auto cmIt = mVU.mvuContentMap.find(liveHash);
		if (cmIt != mVU.mvuContentMap.end())
		{
			microProgram* shared = cmIt->second;
			mVUdequePushUnique(list, shared);
			mVU.prog.cleared = 0;
			mVU.prog.isSame  = 1;
			mVU.prog.cur     = shared;
			quick.prog       = shared;
			quick.block      = shared->block[startPC / 8];
			// Record the dispatched entry on the resolved program.
			// Idempotent on duplicates; bumps `observed.version` only
			// when this PC is new for the program.
			shared->observed.record(startPC);
			if (quick.block == nullptr)
			{
				// First time this startPC is compiled for the shared program
				// — drop through to mVUblockFetch to build the block.
				void* entryPoint = mVUblockFetch(mVU, startPC, pState);
				return entryPoint;
			}
			return mVUentryGet(mVU, quick.block, startPC, pState);
		}

#ifdef mVUcacheTrace
		u64 walkIters = 0;
#endif
		for (auto it = list->begin(); it != list->end(); ++it)
		{
#ifdef mVUcacheTrace
			++walkIters;
#endif
			bool b = mVUcmpProg(mVU, *it[0]);

			if (b)
			{
#ifdef mVUcacheTrace
				mVUCacheTraceObserveWalk(mVU.index, walkIters, /*matched=*/true, /*matchPos=*/walkIters - 1);
#endif
				quick.block = it[0]->block[startPC / 8];
				quick.prog  = it[0];
				list->erase(it);
				list->push_front(quick.prog);
				// Per-PC deque match resolved to this program;
				// record the dispatched entry.
				quick.prog->observed.record(startPC);

				if (quick.block == nullptr)
				{
					void* entryPoint = mVUblockFetch(mVU, startPC, pState);
					return entryPoint;
				}
				return mVUentryGet(mVU, quick.block, startPC, pState);
			}
		}

#ifdef mVUcacheTrace
		mVUCacheTraceObserveWalk(mVU.index, walkIters, /*matched=*/false, /*matchPos=*/0);
#endif
		// Full in-process miss (contentMap + per-PC deque) — this PC needs a
		// program. Try hydrating the block graph from the on-disk cache
		// before compiling from scratch. Placed after the deque walk so a
		// range-equal program already in memory wins over creating a
		// duplicate from disk.
		mVUProgCache::ObserveDispatchHash(mVU, liveHash, startPC);
		if (microProgram* hydrated = mVUProgCache::TryLoadProgram(mVU, liveHash))
		{
			// Same install sequence as the contentMap-hit path above —
			// HydrateProgram registered the program in the contentMap, so
			// from here on it is indistinguishable from a shared program.
			mVUdequePushUnique(list, hydrated);
			mVU.prog.cleared = 0;
			mVU.prog.isSame  = 1;
			mVU.prog.cur     = hydrated;
			quick.prog       = hydrated;
			quick.block      = hydrated->block[startPC / 8];
			hydrated->observed.record(startPC);
			if (quick.block == nullptr)
			{
				// The image carried no block for this entry PC — compile it
				// into the hydrated program (the recorder attaches it to the
				// rebuilt persist log as a growth chunk).
				void* entryPoint = mVUblockFetch(mVU, startPC, pState);
				return entryPoint;
			}
			return mVUentryGet(mVU, quick.block, startPC, pState);
		}

		mVU.prog.cleared = 0;
		mVU.prog.isSame  = 1;
		mVU.prog.cur     = mVUcreateProg(mVU, mVU.regs().start_pc/8);
		// createProg seeded `observed` with its own startPC; record
		// the dispatcher's `startPC` too (idempotent if they match,
		// which is the common case).
		mVU.prog.cur->observed.record(startPC);
		void* entryPoint = mVUblockFetch(mVU,  startPC, pState);
		quick.block      = mVU.prog.cur->block[startPC/8];
		quick.prog       = mVU.prog.cur;
		// Count this deque insertion in the program's refcount.
		// contentMap owns the program; per-PC deques are non-owning refs.
		list->push_front(mVU.prog.cur);
		++mVU.prog.cur->refcount;
		return entryPoint;
	}

	mVU.prog.isSame = -1;
	mVU.prog.cur = quick.prog;
	quick.block = mVU.prog.cur->block[startPC / 8];
	// Quick-slot hit; record the dispatched entry on the resolved
	// program (idempotent if already observed).
	mVU.prog.cur->observed.record(startPC);

	if (quick.block == nullptr)
	{
		void* entryPoint = mVUblockFetch(mVU, startPC, pState);
		return entryPoint;
	}
	return mVUentryGet(mVU, quick.block, startPC, pState);
}

// Read-only fast path: returns the cached block entry on a pure cache hit,
// or nullptr if any compilation, allocation, or full-list comparison is
// required. When nullptr, the caller must open the code cache and call
// mVUsearchProg. Mirrors the bookkeeping mVUsearchProg's hot path performs
// (mVU.prog.cur, quick.block, isSame) so downstream consumers don't see
// stale state on hit. Saves ~12% of CPU thread time by skipping the
// per-dispatch MacroAssembler ctor/dtor + BeginCodeWrite/EndCodeWrite +
// FlushInstructionCache wrapper that mVUopenCodeCache/mVUcloseCodeCache pay.
_mVUt __fi void* mVUlookupProg(u32 startPC, uptr pState)
{
	microVU& mVU = mVUx;
	microProgramQuick& quick = mVU.prog.quick[mVU.regs().start_pc / 8];

	if (!quick.prog)
		return nullptr;

	microBlockManager* block = quick.prog->block[startPC / 8];
	if (!block)
		return nullptr;

	microBlock* pBlock = block->search(mVU, (microRegInfo*)pState);
	if (!pBlock)
		return nullptr;

	mVU.prog.isSame = -1;
	mVU.prog.cur = quick.prog;
	quick.block = block;
	// Fast-path also resolves to a program; record the dispatched entry
	// (idempotent on duplicates). observed.pcs is consumed ONLY by the
	// on-disk program-cache persistence path, which is inactive unless
	// recording is enabled — so skip the per-dispatch linear scan on the
	// hot path when recording is off (the default). IsRecordingEnabled()
	// inlines to a TU-local bool read here. If recording flips on later,
	// createProg's slow-path seed keeps observed valid going forward.
	if (mVUPersist::IsRecordingEnabled())
		quick.prog->observed.record(startPC);
	return pBlock->hostEntry;
}

//------------------------------------------------------------------
// Execution Functions
//------------------------------------------------------------------

_mVUt void* mVUexecute(u32 startPC, u32 cycles)
{
	microVU& mVU = mVUx;
	u32 vuLimit = vuIndex ? 0x3ff8 : 0xff8;
	if (startPC > vuLimit + 7)
	{
		DevCon.Warning("microVU%x Warning: startPC = 0x%x, cycles = 0x%x", vuIndex, startPC, cycles);
	}

	mVU.cycles = cycles;
	mVU.totalCycles = cycles;

#ifdef PCSX2_RECOMPILER_TESTS
	// Live-game capture probe — no-op unless PCSX2_VU_CAPTURE_DIR is set.
	// Snapshots microcode + VU memory + entry register state so the
	// program can be replayed in pcsx2-vurunner without booting a game.
	vu_capture::MaybeCapture(static_cast<int>(vuIndex), startPC & vuLimit, cycles,
		mVU.regs().Micro, mVU.microMemSize,
		mVU.regs().Mem, mVU.microMemSize,
		mVU.regs());
#endif

	const u32 maskedPC = startPC & vuLimit;
	const uptr pState = (uptr)&mVU.prog.lpState;

	void* result = mVUlookupProg<vuIndex>(maskedPC, pState);
	if (!result)
	{
		mVUopenCodeCache(mVU);
		result = mVUsearchProg<vuIndex>(maskedPC, pState);
		mVUcloseCodeCache(mVU);
	}

	if (!result)
	{
		DevCon.Error("microVU%d: mVUexecute got NULL block! startPC=0x%04x", vuIndex, startPC);
	}
	// Upper bound is the PHYSICAL end of the rec region (x86end + safe
	// zone), not the x86end reset threshold: a compile session is allowed
	// to overshoot x86end into the safe zone, and blocks landing there must
	// still execute this once — mVUcleanUp resets the cache at this
	// execution's dispatcher exit.
	else if ((u8*)result < mVU.prog.x86start ||
			 (u8*)result >= mVU.prog.x86end + (mVUcacheSafeZone * _1mb))
	{
		DevCon.Error("microVU%d: Block pointer %p OUTSIDE code cache [%p-%p]! startPC=0x%04x",
			vuIndex, result, mVU.prog.x86start, mVU.prog.x86end, startPC);
		result = nullptr;
	}

	return result;
}

_mVUt void mVUcleanUp()
{
	microVU& mVU = mVUx;

	// Cycle accounting (mVU.cycles update + regs().cycle bump) is emitted
	// inline in the dispatcher exit stub — see mVUdispatcherAB; it is not
	// repeated here.

	// x86ptr is updated in mVUexecute after compilation.
	if ((mVU.prog.x86ptr < mVU.prog.x86start) || (mVU.prog.x86ptr >= mVU.prog.x86end))
	{
		Console.WriteLn(vuIndex ? Color_Orange : Color_Magenta, "microVU%d: Program cache limit reached.", mVU.index);
		mVUreset(mVU, false);
	}

	if (!vuIndex || !THREAD_VU1)
	{
		u32 cycles_passed = std::min(mVU.cycles, 3000) * EmuConfig.Speedhacks.EECycleSkip;
		if (cycles_passed > 0)
		{
			s64 vu0_offset = VU0.cycle - cpuRegs.cycle;
			cpuRegs.cycle += cycles_passed;

			if (!vuIndex)
				VU0.cycle = cpuRegs.cycle + vu0_offset;
			else
				VU0.cycle += cycles_passed;
		}
	}
	mVU.profiler.Print();
}

void* mVUexecuteVU0(u32 startPC, u32 cycles) { return mVUexecute<0>(startPC, cycles); }
void* mVUexecuteVU1(u32 startPC, u32 cycles) { return mVUexecute<1>(startPC, cycles); }
void mVUcleanUpVU0() { mVUcleanUp<0>(); }
void mVUcleanUpVU1() { mVUcleanUp<1>(); }

// Non-template entry points callable from the dispatcher BL. They
// mirror mVUexecute's hot-path body (cycle field set + maskedPC +
// cache lookup) without the slow-path compile fallback. Return
// nullptr on miss; the dispatcher then BLs mVUexecuteVUx for the
// full slow path (which re-runs the cycle set + maskedPC harmlessly
// and then mVUopenCodeCache + mVUsearchProg). Cache hits skip the
// slow-path BL entirely.
//
// Skipping the bounds check that mVUexecute does on its result is
// safe: mVUlookupProg always returns either nullptr or a
// pBlock->hostEntry from a block emitted into the post-dispatcher
// region of the same code cache. The bounds check in mVUexecute
// only fires when the slow-path mVUsearchProg returns an
// out-of-cache pointer; that path still runs through mVUexecute
// here unchanged.
void* mVUlookupProg_VU0(u32 startPC, u32 cycles)
{
	microVU0.cycles      = cycles;
	microVU0.totalCycles = cycles;
	const u32 maskedPC = startPC & 0xff8;
	return mVUlookupProg<0>(maskedPC, (uptr)&microVU0.prog.lpState);
}
void* mVUlookupProg_VU1(u32 startPC, u32 cycles)
{
	microVU1.cycles      = cycles;
	microVU1.totalCycles = cycles;
	const u32 maskedPC = startPC & 0x3ff8;
	return mVUlookupProg<1>(maskedPC, (uptr)&microVU1.prog.lpState);
}

#ifdef PCSX2_RECOMPILER_TESTS
// Exposed for the vu_capture replay harness (VuReplay::DumpJitAsm) so it
// doesn't have to include microVU-arm64.h (which has __fi defs that can't
// safely be cross-TU'd). Returns the program-cache range currently in use
// by the named VU.
namespace vu_capture_internal
{
	void GetCompiledRange(int vu_index, const u8** out_start, const u8** out_end)
	{
		const microVU& mVU = (vu_index == 0) ? microVU0 : microVU1;
		*out_start = mVU.prog.x86start;
		*out_end = mVU.prog.x86ptr;
	}
}

// Geometry hooks for the cache-exhaustion regression test
// (mvu_cache_exhaustion_tests.cpp). Same cross-TU pattern as
// vu_capture_internal above.
namespace mvu_test_hooks
{
	// Shrinks prog.x86end — mVUcleanUp's reset threshold — to sit
	// `bytes_after_start` past prog.x86start, so a test can exhaust the code
	// cache with dozens of programs instead of 64 MB worth. The patched value
	// survives mVUreset (only mVUinit derives x86end from SysMemory). Callers
	// must reset the VU block cache after BOTH hooks so the vixl buffer
	// capacity derived from x86end is rebound.
	void ShrinkCacheForTest(int vu_index, size_t bytes_after_start)
	{
		microVU& mVU = vu_index ? microVU1 : microVU0;
		mVU.prog.x86end = mVU.prog.x86start + bytes_after_start;
	}

	// Restores the production geometry (mirrors mVUinit).
	void RestoreCacheGeometry(int vu_index)
	{
		microVU& mVU = vu_index ? microVU1 : microVU0;
		mVU.prog.x86end = (vu_index ? SysMemory::GetVU1RecEnd() : SysMemory::GetVU0RecEnd()) - (mVUcacheSafeZone * _1mb);
	}
}
#endif

//------------------------------------------------------------------
// Block Fetch / Compile
//------------------------------------------------------------------

void* mVUblockFetch(microVU& mVU, u32 startPC, uptr pState)
{
	pxAssert((startPC & 7) == 0);
	pxAssert(startPC <= mVU.microMemSize - 8);
	startPC &= mVU.microMemSize - 8;

	blockCreate(startPC / 8);
	return mVUentryGet(mVU, mVUblocks[startPC / 8], startPC, pState);
}

//------------------------------------------------------------------
// recMicroVU0 / recMicroVU1
//------------------------------------------------------------------

recMicroVU0 CpuMicroVU0;
recMicroVU1 CpuMicroVU1;

recMicroVU0::recMicroVU0() { m_Idx = 0; IsInterpreter = false; }
recMicroVU1::recMicroVU1() { m_Idx = 1; IsInterpreter = false; }

void recMicroVU0::Reserve()
{
	mVUinit(microVU0, 0);
}
void recMicroVU1::Reserve()
{
	mVUinit(microVU1, 1);
	vu1Thread.Open();
}

void recMicroVU0::Shutdown()
{
	mVUclose(microVU0);
}
void recMicroVU1::Shutdown()
{
	if (vu1Thread.IsOpen())
		vu1Thread.WaitVU();
	mVUclose(microVU1);
}

void recMicroVU0::Reset()
{
	mVUreset(microVU0, true);
}

void recMicroVU0::Step()
{
}

void recMicroVU1::Reset()
{
	vu1Thread.WaitVU();
	vu1Thread.Get_MTVUChanges();
	mVUreset(microVU1, true);
}

void recMicroVU0::SetStartPC(u32 startPC)
{
	VU0.start_pc = startPC;
}

void recMicroVU0::Execute(u32 cycles)
{
	VU0.flags &= ~VUFLAG_MFLAGSET;

	if (!(VU0.VI[REG_VPU_STAT].UL & 1))
		return;
	VU0.VI[REG_TPC].UL <<= 3;

	((mVUrecCall)microVU0.startFunct)(VU0.VI[REG_TPC].UL, cycles);
	VU0.VI[REG_TPC].UL >>= 3;
	if (microVU0.regs().flags & 0x4)
	{
		microVU0.regs().flags &= ~0x4;
		hwIntcIrq(6);
	}
}

void recMicroVU1::SetStartPC(u32 startPC)
{
	VU1.start_pc = startPC;
}

void recMicroVU1::Step()
{
}

void recMicroVU1::Execute(u32 cycles)
{
	if (!THREAD_VU1)
	{
		if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
			return;
	}
	VU1.VI[REG_TPC].UL <<= 3;
#ifdef PCSX2_RECOMPILER_TESTS
	vu1_trace::Entry* trace = vu1_trace::g_enabled.load(std::memory_order_relaxed)
		? vu1_trace::begin('r', VU1.VI[REG_TPC].UL, cycles)
		: nullptr;
#endif
	((mVUrecCall)microVU1.startFunct)(VU1.VI[REG_TPC].UL, cycles);
	VU1.VI[REG_TPC].UL >>= 3;
#ifdef PCSX2_RECOMPILER_TESTS
	vu1_trace::finish(trace);
#endif

	if (microVU1.regs().flags & 0x4 && !THREAD_VU1)
	{
		microVU1.regs().flags &= ~0x4;
		hwIntcIrq(7);
	}
}

void recMicroVU0::Clear(u32 addr, u32 size)
{
	mVUclear(microVU0, addr, size);
}
void recMicroVU1::Clear(u32 addr, u32 size)
{
	mVUclear(microVU1, addr, size);
}

void recMicroVU1::ResumeXGkick()
{
	if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
		return;
	((mVUrecCallXG)microVU1.startFunctXG)();
}

//------------------------------------------------------------------
// COP2 Macro-Mode State Helpers
//------------------------------------------------------------------
// Ports x86 setupMacroOp / endMacroOp's microVU0-state work (microVU_Macro.inl
// lines 26, 33-36, 42-58, 84, 102-103). Lives here because microVU0 + its
// regAlloc/prog/code/cop2 fields are only fully visible inside microVU-arm64.cpp
// (microVU_Lower-arm64.inl is #include'd from microVU-arm64.h with file-local
// static emitters — including them in iCOP2-arm64.cpp would require pulling
// in the full header context).
//
// setupMacroOp_arm64 / endMacroOp_arm64 in iCOP2-arm64.cpp call into these
// after the existing sync + flag denorm/norm work; mode bits and eeinstInfo
// (g_pCurInstInfo->info) come from the caller.

void mVUmacroSetupCOP2State(int mode, u32 eeinstInfo)
{
	microVU0.regAlloc->reset(true);
	microVU0.cop2 = 1;
	microVU0.prog.IRinfo.curPC = 0;
	microVU0.code = cpuRegs.code;
	std::memset(&microVU0.prog.IRinfo.info[0], 0, sizeof(microVU0.prog.IRinfo.info[0]));

	if ((mode & 0x08) && (!CHECK_VU_FLAGHACK || (eeinstInfo & EEINST_COP2_CLIP_FLAG)))
	{
		microVU0.prog.IRinfo.info[0].cFlag.write     = 0xff;
		microVU0.prog.IRinfo.info[0].cFlag.lastWrite = 0xff;
	}
	if ((mode & 0x10) && (!CHECK_VU_FLAGHACK || (eeinstInfo & EEINST_COP2_STATUS_FLAG)))
	{
		microVU0.prog.IRinfo.info[0].sFlag.doFlag      = true;
		microVU0.prog.IRinfo.info[0].sFlag.doNonSticky = true;
		microVU0.prog.IRinfo.info[0].sFlag.write       = 0;
		microVU0.prog.IRinfo.info[0].sFlag.lastWrite   = 0;
	}
	if ((mode & 0x10) && (!CHECK_VU_FLAGHACK || (eeinstInfo & EEINST_COP2_MAC_FLAG)))
	{
		microVU0.prog.IRinfo.info[0].mFlag.doFlag = true;
		microVU0.prog.IRinfo.info[0].mFlag.write  = 0xff;
	}
}

void mVUmacroEndCOP2State()
{
	// regAlloc writebacks happened inside the per-op adapter (while x19 still
	// held &VU0). The only cleanup needed here is clearing the map state.
	microVU0.cop2 = 0;
	microVU0.regAlloc->reset(false);
}

// COP2 macro-mode emit adapters. The 12 mVU_* emitters in microVU_Lower-arm64.inl
// are file-static (the .inl is #include'd here), so iR5900Misc-arm64.cpp can't
// take their address. Each adapter runs the standard pass1+pass2 dispatch x86
// uses in REC_COP2_mVU0 (microVU_Macro.inl:127-133): when mode bit 0x04 is set,
// run pass1 (analyze) first, then pass2 (codegen) unless the analysis flagged
// the op as NOP; otherwise run pass2 directly.
//
// gprVUState (x19) bridging: the mVU emitters address VURegs via x19 = &VU0,
// but the EE recompiler pins x19 to RFASTMEMBASE for the whole block. We
// rebase x19 = RVU0 (x24, which the EE rec already loaded with &VU0) before
// the mVU emit, then reload x19 from vtlbdata.fastmem_base afterward so any
// subsequent fastmem ldr/str in the EE block keeps working.
static void mVUmacroEmitPrologue()
{
	// Evict the EE register cache before the mVU emit runs. The mVU regAlloc
	// allocates VI into host x14/x15/x26-x28 and VF into Q0-Q27, all of which
	// overlap the EE allocator's pool — and the EE recompiler keeps GPR/NEON
	// values cached across instruction boundaries (recompileNextInstruction
	// only _clearNeeded's, it does not write back). With no cross-allocator
	// coordination on arm64 (clearRegCOP2/clearGPRCOP2 are stubs) the mVU emit
	// would silently clobber a live EE GPR/NEON value. Free+writeback all EE
	// allocations here so the mVU emit gets a clean slate; subsequent EE code
	// reloads from cpuRegs as needed. This is at most the cost of the old
	// REC_COP2_INTERP path (which full-flushed via recCall). Pinned regs
	// (x19/x20/x24/x25) are outside the allocatable pool and untouched.
	_freeArm64GPRregs();
	_freeNEONregs();

	armAsm->Mov(gprVUState, RVU0);
}

static void mVUmacroEmitEpilogue()
{
	if (CHECK_FASTMEM)
	{
		armMoveAddressToReg(RSCRATCHADDR, &vtlb_private::vtlbdata.fastmem_base);
		armAsm->Ldr(RFASTMEMBASE, a64::MemOperand(RSCRATCHADDR));
	}
}

#define MVU_MACRO_EMIT_ADAPTER(opname)                                 \
	void mVUmacroEmit_##opname(int mode)                               \
	{                                                                  \
		mVUmacroEmitPrologue();                                        \
		if (mode & 0x04)                                               \
		{                                                              \
			mVU_##opname(microVU0, 0);                                 \
			if (!microVU0.prog.IRinfo.info[0].lOp.isNOP)               \
				mVU_##opname(microVU0, 1);                             \
		}                                                              \
		else                                                           \
		{                                                              \
			mVU_##opname(microVU0, 1);                                 \
		}                                                              \
		/* Writebacks MUST happen while x19 still holds &VU0 — the    \
		 * epilogue restores x19 to RFASTMEMBASE and any later store  \
		 * via mVUstateMem would land in fastmem garbage.             \
		 * Arm64 regAlloc has no x86-style cross-op VI preservation,  \
		 * so flushAll vs flushPartialForCOP2 is correctness-required.*/ \
		microVU0.regAlloc->flushAll(true);                             \
		mVUmacroEmitEpilogue();                                        \
	}

MVU_MACRO_EMIT_ADAPTER(LQI)
MVU_MACRO_EMIT_ADAPTER(SQI)
MVU_MACRO_EMIT_ADAPTER(LQD)
MVU_MACRO_EMIT_ADAPTER(SQD)
MVU_MACRO_EMIT_ADAPTER(MTIR)
MVU_MACRO_EMIT_ADAPTER(MFIR)
MVU_MACRO_EMIT_ADAPTER(ILWR)
MVU_MACRO_EMIT_ADAPTER(ISWR)
MVU_MACRO_EMIT_ADAPTER(RNEXT)
MVU_MACRO_EMIT_ADAPTER(RGET)
MVU_MACRO_EMIT_ADAPTER(RINIT)
MVU_MACRO_EMIT_ADAPTER(RXOR)

#undef MVU_MACRO_EMIT_ADAPTER

//------------------------------------------------------------------
// On-disk program cache implementation — single-TU inclusion to share the
// mVU header context. See microVU_ProgCache-arm64.inl for the rationale.
//------------------------------------------------------------------
#include "microVU_ProgCache-arm64.inl"

//------------------------------------------------------------------
// Persisted-JIT relocation recorder + block-graph serializer — same
// single-TU inclusion pattern (needs mVUcreateProg / mVUcomputeProgramHash
// and the TU-local mVUopenCodeCache / mVUcloseCodeCache).
//------------------------------------------------------------------
#include "microVU_Persist-arm64.inl"

//------------------------------------------------------------------
// Save State
//------------------------------------------------------------------

bool SaveStateBase::vuJITFreeze()
{
	if (IsSaving())
		vu1Thread.WaitVU();

	Freeze(microVU0.prog.lpState);
	Freeze(microVU1.prog.lpState);
	return IsOkay();
}
