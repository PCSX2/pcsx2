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
} // namespace ArmJitTelemetry
