// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "common/Pcsx2Defs.h"

// Allocator live-value eviction census (FX-07 ceiling gate) — see the struct
// at the bottom of this header. Defined up here because it needs <atomic>
// outside the namespace: mVU compiles run on the EE and MTVU threads.
#ifndef JIT_ALLOC_CENSUS
#define JIT_ALLOC_CENSUS 0
#endif

#if JIT_ALLOC_CENSUS
#include <atomic>
#endif

// Devel-only ARM64 JIT maintenance telemetry (icache-locality campaign Phase 1c —
// see tools/perf/icache-2026-07.md): counts code-cache flushes, link patches, and
// SMC invalidation events, printing one "JITTELEM" rate line at most every ~5s
// while events occur. Silence between lines means maintenance traffic is quiet —
// that absence is itself the steady-state datum. Compiled out of Release builds.
namespace ArmJitTelemetry
{
#ifdef PCSX2_DEVBUILD
	void AddLinkPatch(); // Arm64BaseBlocks::PatchAtomic (EE + IOP link/unlink)
	void AddBlockFlush(u32 bytes); // armEndBlock icache flush (EE/IOP block compile)
	void AddVUFlush(u32 bytes); // mVUcloseCodeCache episode flush
	void AddBlockDiscard(); // dyna_block_discard (SMC stale block)
	void AddPageReset(); // dyna_page_reset (SMC page knockout)
	void AddEEFullReset();
	void AddIOPFullReset();
#else
	__forceinline_odr void AddLinkPatch() {}
	__forceinline_odr void AddBlockFlush(u32) {}
	__forceinline_odr void AddVUFlush(u32) {}
	__forceinline_odr void AddBlockDiscard() {}
	__forceinline_odr void AddPageReset() {}
	__forceinline_odr void AddEEFullReset() {}
	__forceinline_odr void AddIOPFullReset() {}
#endif

// Call-ret validation counters are OFF by default: unlike the C-side rate
// telemetry above, they cost emitted instructions on hot JIT tails, so the
// A/B timing binaries must not carry them. -DEE_CALLRET_TELEM=1 builds the
// hit-rate validation binary.
#ifndef EE_CALLRET_TELEM
#define EE_CALLRET_TELEM 0
#endif

#if EE_CALLRET_TELEM
	// Call-ret ring dynamic counters (P2-2 validation). Incremented from
	// EMITTED code at every push / pop-hit / pop-miss — real per-event cost,
	// so this is gated separately from PCSX2_DEVBUILD and must stay OFF in
	// A/B timing binaries (-DEE_CALLRET_TELEM=1 builds a validation binary;
	// bar: >=80% hit rate in real games). Totals print at every EE full
	// reset and process exit via the JITTELEM channel.
	extern u64 g_callret_push, g_callret_hit, g_callret_miss;
	void ReportCallRet(const char* when);
#endif

// Manual-mode SMC re-check census (FX-03 ceiling gate). Same deal as the
// call-ret counters: the bump is EMITTED at the top of every ProtMode_Manual
// block check, so it stays OFF in A/B timing binaries. -DEE_SMC_TELEM=1
// builds the census binary. Bar for funding the NEON-widened re-check
// rewrite: >= ~1M manual words/s sustained on some title.
#ifndef EE_SMC_TELEM
#define EE_SMC_TELEM 0
#endif

#if EE_SMC_TELEM
	// [0] = check executions, [1] = words compared. Must stay adjacent —
	// the emitted bump is a single Ldp/Stp pair. EE thread is the only writer.
	// Two exclusive classes: blocks in genuinely-SMC-flagged pages, and blocks
	// the contains_thread_stack heuristic FORCES manual (0x81/0x80001 pages) —
	// the fix shape differs if the tax is dominated by the forced class.
	extern u64 g_smc_manual_exec[2]; // real manual pages
	extern u64 g_smc_manual_exec_ts[2]; // thread-stack-forced pages
	// Compile-side tallies (C increments, no emitted cost): manual check
	// sequences emitted and their total word count — counts re-emissions
	// after discard/reset too, so it doubles as the static code-size tax.
	extern u64 g_smc_manual_emits, g_smc_manual_words_emitted;
	extern u64 g_smc_manual_emits_ts, g_smc_manual_words_emitted_ts;
	void MaybeReportSmc(); // 5s-window words/s rate line (call from recEventTest)
	void ReportSmc(const char* when); // totals at ee-reset / shutdown
#endif

// EE block-ending branch-shape census (FX-16 ceiling gate). Pure compile-time
// C counters — nothing is emitted, so the instrumented binary's codegen is
// bit-identical to a plain build; OFF by default only to keep census output
// out of normal sessions. -DEE_BRSHAPE_CENSUS=1. Answers: how many EE blocks
// end in a short forward conditional skip that scoped fusion could absorb,
// and what disqualifies the rest (likely-nullification, link variants,
// unsafe skipped ops, 4K page crossings).
#ifndef EE_BRSHAPE_CENSUS
#define EE_BRSHAPE_CENSUS 0
#endif

#if EE_BRSHAPE_CENSUS
	struct BrShapeCensus
	{
		u64 blocks; // recRecompile invocations (recompiles count again — shape distribution, not unique blocks)
		u64 fwd_cond; // block ends with a forward conditional branch skipping >=1 op
		u64 link; // of fwd_cond: AL/ALL variants (write $ra)
		u64 likely; // of fwd_cond: L variants (delay-slot nullification)
		u64 dist[6]; // skipped-op histogram: 1-2, 3-4, 5-8, 9-16, 17-32, >32
		u64 crosspage_le8; // dist<=8 but skip region crosses a 4K page
		u64 unsafe_sys_le8; // dist<=8, region has syscall/break/COP0/cache/trap class
		u64 unsafe_ctrl_le8; // dist<=8, region has control flow that isn't the diamond tail shape
		u64 diamond_le8; // dist<=8, region ends in a single unconditional forward J (if/else join)
		u64 fusable_plain_le8; // dist<=8, plain cond, non-link, same page, all ops safe — THE number
		u64 fusable_likely_le8; // same but L-variant (fusable only if nullification is modeled)
	};
	extern BrShapeCensus g_brshape;
	void ReportBrShape(const char* when);
#endif

#if EE_BRSHAPE_CENSUS || JIT_ALLOC_CENSUS
	// Kill-proof collection: 30s cadence totals from recEventTest, so census
	// data survives a hard kill (shutdown/ee-reset prints are lost to SIGKILL).
	void MaybeReportCensusTick();
#endif

#if JIT_ALLOC_CENSUS
	// FX-07: compile-time counters only, nothing emitted. -DJIT_ALLOC_CENSUS=1.
	// Answers three ceilings at once: EE GPR live-evictions (proven ~0 by code
	// audit — this confirms it dynamically), IOP live-evictions (nonzero funds
	// the ~10-line EEINST_USEDTEST dead-first rider), and mVU VF/VI evictions
	// (the only real-pressure allocator; bar 0.1% of exec-weighted VU ops
	// before an ABI-bump fix is considered).
	struct AllocCensus
	{
		// GPR-class pool, _getFreeArm64GPR second pass (live value displaced),
		// split by victim type.
		std::atomic<u64> gpr_allocs{0};
		std::atomic<u64> gpr_evict_ee{0}; // ARM64TYPE_GPR
		std::atomic<u64> gpr_evict_iop{0}; // ARM64TYPE_PSX
		std::atomic<u64> gpr_evict_other{0}; // VIREG/FPRC/PCWRITEBACK
		std::atomic<u64> gpr_evict_dirty{0}; // of all the above: MODE_WRITE (writeback emitted)
		// EE NEON pool, _getFreeArm64NEON: dead-first tier already exists, so
		// dead evictions are benign; the last-resort tier displaces a
		// USEDTEST-live value.
		std::atomic<u64> neon_allocs{0};
		std::atomic<u64> neon_evict_dead{0};
		std::atomic<u64> neon_evict_live{0};
		// mVU findFreeNeon/findFreeGPR: no liveness info at all (pure LRU) —
		// every displaced mapping counts, dirty ones force a store.
		std::atomic<u64> mvu_vf_allocs{0};
		std::atomic<u64> mvu_vf_evict{0}; // victim VFreg >= 0
		std::atomic<u64> mvu_vf_evict_dirty{0}; // victim xyzw != 0
		std::atomic<u64> mvu_vi_evict{0}; // victim VIreg >= 0 in findFreeGPR
	};
	extern AllocCensus g_allocCensus;
	// use_stdio: print via fprintf(stderr) instead of Console — for the
	// static-destruction exit fallback where Console may already be dead.
	void ReportAllocCensus(const char* when, bool use_stdio = false);
#endif
} // namespace ArmJitTelemetry
