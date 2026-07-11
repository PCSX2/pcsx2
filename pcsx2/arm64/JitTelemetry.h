// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#pragma once

#include "common/Pcsx2Defs.h"

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
} // namespace ArmJitTelemetry
