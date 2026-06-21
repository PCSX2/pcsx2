// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ee_divtrace — EE (R5900) JIT-vs-interpreter divergence trace.
//
// Records a stream of architectural fingerprints from a deterministic
// full-system run so that an interpreter "golden" run and a JIT run, replayed
// from the same savestate, can be diffed OFFLINE (never live / in lockstep).
// The shared clock cpuRegs.cycle plus cpuRegs.pc is the alignment key between the two runs.
//
// Three-level funnel, all bounded in memory (pcsx2-eerunner drives it):
//   1. per-FRAME  — runner hashes FingerprintCpu()+HashMemory() once per frame
//                   over the whole run; cheap; localizes the divergent frame.
//   2. per-OP     — within the one divergent frame, the capture sites append a
//                   Sample (cycle, pc, fp) per executed op (interp, dense) and
//                   per block dispatch (JIT, sparse). Aligned by (cycle, pc).
//   3. full-snap  — a small window (ConfigureFullWindow) around the first
//                   divergent op stores whole cpuRegisters+fpuRegisters for the
//                   register-level report.
//
// Capture sites:
//   pcsx2/Interpreter.cpp            — interp, after execI() (dense)
//   pcsx2/arm64/iR5900-arm64.cpp     — JIT, C-hook at the block dispatcher (sparse)
//
// fp hashes EXACTLY the fields DiffEe (harness/StateSnapshot.cpp) compares —
// GPR (both doublewords), HI/LO, CP0 except {1 Random, 9 Count, 11 Compare},
// FPR, FPU ACC, pc, sa — so a fingerprint mismatch is a genuine architectural
// divergence, not dispatcher-bookkeeping noise.

#include "common/Pcsx2Defs.h"
#include "R5900.h"

#include <atomic>
#include <vector>

namespace ee_divtrace
{
	// One architectural sample, ~24 bytes.
	struct Sample
	{
		u64 cycle;   // cpuRegs.cycle at the sample
		u64 fp;      // FingerprintCpu()
		u32 pc;      // cpuRegs.pc at the sample (next instr / block entry)
		u32 _pad;
	};

	// Full architectural snapshot — stored only inside the detail window.
	struct FullSnap
	{
		cpuRegisters cpu;
		fpuRegisters fpu;
		u64          cycle;
		u32          pc;
		u32          _pad;
	};

	// Gates the per-op/per-block capture sites. Read on the hot path, so the
	// site is `if (g_enabled.load(relaxed)) RecordSample();`. The runner sets
	// it true only for the single frame being finely traced.
	extern std::atomic<bool>       g_enabled;

	// Current pass's sample stream (interp run, then JIT run — separate runs).
	extern std::vector<Sample>     g_stream;

	// Detail-window full snapshots; g_snaps[idx - g_full_lo] for idx in window.
	extern std::vector<FullSnap>   g_snaps;
	extern u32                     g_full_lo;
	extern u32                     g_full_hi;

	// When set, recRecompile emits a block-entry hook into every EE block
	// prologue (pcsx2-eerunner triage build). Read at codegen time, so it must
	// be set before the recompiler is initialized / reset. Default false →
	// production recompiles emit nothing. Checked in pcsx2/arm64/iR5900-arm64.cpp.
	extern bool                    g_emit_block_hook;

	// When set, FingerprintCpu() omits the FPU register file + ACC from the hash,
	// so FP-register divergences do not break the alignment walk. Used to hunt a
	// NON-FP (integer / control-flow) EE-jit divergence when the FP path is known
	// benign (e.g. Burnout 3: the hang persists with the EE-FPU fully converged to
	// the interp, so the cond_b off-by-0xC0 is an integer/pointer bug, and the
	// pervasive 1-ULP div.s noise just masks it). The eerunner sets this from
	// EERUNNER_NOFP; the Main.cpp diff helpers honor it too for consistent reports.
	extern bool                    g_fp_exclude;

	// When set, the interpreter's SYSCALL handler skips FlushCache (0x64) /
	// iFlushCache (0x68) — returning without raising the syscall exception —
	// exactly as the JIT's recSYSCALL does when $v1 is a known const. This keeps
	// the golden interp timeline in lockstep with the JIT across that ABI-benign
	// divergence (the JIT skip leaves caller-saved v0/v1/at/t0/t1 + EPC and the
	// BIOS exception-frame memory untouched; running the real handler in interp
	// only would desync both register state AND the kernel stack, masking real
	// codegen bugs downstream). Set by pcsx2-eerunner; default false → production
	// interp runs the real handler. Read in pcsx2/R5900OpcodeImpl.cpp SYSCALL().
	extern bool                    g_skip_flushcache_syscall;

	// Hash live cpuRegs+fpuRegs over exactly DiffEe's compared field set.
	u64  FingerprintCpu();

	// Append one Sample (cycle, pc, fp) from live cpuRegs, plus a FullSnap when
	// the new index is in the detail window. `pc` is passed explicitly because
	// the JIT block hook knows its startpc at compile time but cpuRegs.pc is
	// not reliable on a statically-linked entry. The caller (JIT site) must
	// flush its pinned cycle register to cpuRegs.cycle first.
	void RecordSample(u32 pc);

	// xxh3 of EE main RAM (32 MB) + scratchpad (16 KB) — frame memory fingerprint.
	u64  HashMemory();

	// Clear g_stream; zero the used portion of the detail window.
	void Reset();

	// Set the detail window to [lo, lo+len); (re)allocates g_snaps to `len`.
	void ConfigureFullWindow(u32 lo, u32 len);

	// Reserve g_stream capacity (samples) up front so the fine pass doesn't
	// reallocate mid-frame. Called by the runner before a traced frame.
	void ReserveStream(size_t samples);
} // namespace ee_divtrace

// C-callable JIT block-entry hook (plain symbol for armEmitCall). Emitted into
// every EE block prologue when ee_divtrace::g_emit_block_hook is set. Checks
// g_enabled and records a sample for block `startpc`. cpuRegs.cycle must
// already have been flushed from the pinned cycle register by the caller.
extern "C" void ee_divtrace_jit_block_hook(u32 startpc);
