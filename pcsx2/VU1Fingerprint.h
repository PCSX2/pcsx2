// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

// VU1 microprogram fingerprinting.
//
// PS2 retail games share a small set of VU code libraries (Sony sceVu*,
// Renderware RpVU1*, Criterion character anim, Konami/Capcom in-house). The
// same 20-30 microprograms show up in hundreds of games — by exact byte hash.
// This module recognizes them on dispatch and lets the VU1 JIT swap in
// hand-tuned NEON kernels for the hot ones.
//
// Phase 1.5 (this file): infrastructure + dump-on-upload + dispatch-frequency
// telemetry. The kernel database is intentionally empty. Two log streams:
//   - `[VU1FP] new program: …`  — emitted once per unique upload hash
//   - `[VU1FP-HOT] …`           — emitted every ~10s with top-N by dispatch count
// Together these tell us BOTH which programs exist AND which are actually hot.
//
// Phase 2 (future): populate g_kernels with hash → NEON-kernel entries for
// the most-dispatched programs. A kernel runs as a complete program-execution
// unit, handling VU1.cycle, fmaccount, MAC/STATUS/CLIP flag updates, XGKICK
// queue posts, and VPU_STAT bit transitions itself.
namespace VU1Fingerprint
{
    struct KernelEntry
    {
        u64 hash;            // xxh3-64 of the program extent (pc → E-bit + delay slot)
        u32 size_bytes;      // extent length in bytes
        const char* name;    // for logging — e.g. "sceVu1MatrixMultiply"
        void (*kernel_fn)(); // VU1 block-ABI compatible — see header comment
        u32 fake_cycles;     // cycles to inject into VU1.cycle after the kernel runs
    };

    // Runtime diagnostic gate. Disabled by default; set
    // ARMSX2_VU1_FINGERPRINT=1 when collecting VU1 telemetry.
    bool Enabled();

    // Compute the xxh3-64 hash of a byte range.
    u64 ComputeHash(const u8* code, size_t bytes);

    // Observe a microprogram upload. Called from both the single-threaded VIF
    // MPG path and the MTVU WriteMicroMem dispatch. Dedupes by raw-upload
    // hash so re-uploads of the same program don't spam the log. NOTE: the
    // upload hash is NOT the same as the dispatch hash — uploads can include
    // trailing padding or be split across multiple MPGs. Upload logging is
    // for "what does this game push to VU memory" telemetry; dispatch
    // logging (OnDispatch) is for "what does this game actually execute".
    void OnUpload(u32 vu_idx, u32 addr, const u8* code, size_t bytes);

    // Dispatcher entry point. Called from the VU1 Execute loop at every
    // block dispatch. Returns a kernel substitute if one matches, else null.
    //
    // Side effects (always run, regardless of match):
    //   - Walks program extent from `pc` to next E-bit pair (+ delay slot)
    //   - Computes the extent hash (cached per-PC, invalidated on upload)
    //   - Bumps a per-slot dispatch counter
    //   - Every ~10s, emits a "[VU1FP-HOT]" log with the top-N hottest
    //     programs by dispatch count in the last period, then resets counts
    //
    // Cheap when there's no kernel match — cache hit collapses to:
    //   u64 read + 2-way compare + counter++ + interval check.
    const KernelEntry* OnDispatch(u32 pc);
} // namespace VU1Fingerprint
