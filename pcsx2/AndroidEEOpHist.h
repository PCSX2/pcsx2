// SPDX-License-Identifier: GPL-3.0+
#pragma once

#include "common/Pcsx2Types.h"

// Gated EE opcode histogram for diagnosing interpreter-fallback hotspots on the mac
// ARM64 backend. Counts how often each opcode runs through the two fallback paths:
//   - STEP   : block-terminating single-step (intExecuteOneInst) — the expensive one
//   - INLINE : in-block interpreter call      (recEmitInterpInline)
// Every ~8M recorded fallback ops the EE thread prints "@@ANDROID_EE_OPHIST@@" lines
// listing the hottest primary opcodes + SPECIAL/REGIMM/COP2/MMI sub-ops, then resets.
// EE-thread-only (no atomics). Default 0 = zero overhead; flip to 1 for a diag build.
#ifndef ARMSX2_ANDROID_EE_OPHIST
#define ARMSX2_ANDROID_EE_OPHIST 0
#endif

#if ARMSX2_ANDROID_EE_OPHIST
namespace AndroidEEOpHist
{
	// path: 0 = STEP (single-step), 1 = INLINE (in-block interp).
	void Record(int path, u32 op);
	// Emit-time tally of natively-compiled trap ops (proves the trap codegen engaged).
	void NoteTrapCompiled();
}

// Counting thunk emitted by recEmitInterpInline in place of the raw interp handler
// when the histogram is on: records cpuRegs.code on the INLINE path, then dispatches.
void recOphistInlineThunk();
#endif
