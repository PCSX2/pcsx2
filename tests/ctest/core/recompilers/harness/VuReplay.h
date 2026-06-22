// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "VuSnapshot.h"

#include "common/Pcsx2Defs.h"
#include "common/PmuCounters.h"
#include "vu_capture.h"

#include <string>
#include <vector>

namespace recompiler_tests {

// Result of replaying one captured VU microprogram through both engines.
// Populated in full whether or not the JIT and interp diverged — the diff
// list is empty on a clean replay.
struct VuReplayResult
{
	// True if both engines ran to completion without setup error. False on
	// load failure / unsupported vu_index / cycle exhaustion in either engine.
	bool ok = false;

	// True iff `diff_lines` is non-empty.
	bool diverged = false;

	// Architectural divergences between JIT and interp post-state. Same
	// format as DiffVu() in VuSnapshot.h.
	std::vector<std::string> diff_lines;

	// Full post-state from each engine's run. Always populated when ok=true.
	VuSnapshot jit_snapshot;
	VuSnapshot interp_snapshot;

	// XGKICK Path 1 packet bytes captured during each engine's run.
	std::vector<u8> path1_packets_jit;
	std::vector<u8> path1_packets_interp;

	// Termination signal: true if the run cleared its VPU_STAT running bit
	// (E-bit terminated naturally), false if it was truncated by the cycle
	// budget. Read from the authoritative vuRegs[0].VI[REG_VPU_STAT] right
	// after each pass — the running bit lives in VU0's VI for both VUs, so a
	// VU1 run's own VI[REG_VPU_STAT] mirror is stale and can't be used.
	//
	// Why it matters: a budget-truncated looping program runs a different
	// number of iterations under JIT vs interp, producing large memory/reg
	// diff counts that are NOT codegen bugs. interp_ebit==false ⇒ deprioritize
	// the divergence (it's loop/budget noise, not a mis-emitted op).
	bool jit_ebit = false;
	bool interp_ebit = false;

	// Cycles consumed by each engine for this program+entry-state+budget.
	// PrimeFromCapture zeroes vu.cycle before each pass, so this is the delta
	// the engine added to VURegs::cycle. The mVU dispatcher reports this back
	// to the EE as how long VU0 ran (VU0.cpp _vu0run: cpuRegs.cycle += delta),
	// so a JIT-vs-interp mismatch drives EE<->VU0 timing drift even when the
	// architectural post-state is bit-identical — the one quantity the
	// register/memory diff is structurally blind to. Compare apples-to-apples
	// only when BOTH jit_ebit && interp_ebit (whole program ran in both); a
	// budget-truncated program legitimately overshoots to a block boundary
	// under JIT while interp stops mid-op at the exact budget.
	u64 jit_cycles = 0;
	u64 interp_cycles = 0;
};

// Drives the JIT and interpreter against one captured program. Restores the
// captured microcode + VU memory + register state, runs the JIT, snapshots,
// restores pre-state, runs the interpreter, snapshots, diffs.
//
// `cycle_budget_override` of 0 means "use the budget from the capture". The
// pcsx2-vurunner --bench mode sets a tight per-iteration budget to avoid
// running the program multiple times when it's a long loop.
//
// Caller must ensure RecompilerTestEnvironment::IsReady() — the SysMemory
// allocations and CpuMicroVU0/1 reservations need to be in place.
VuReplayResult ReplayCapture(const vu_capture::CaptureRecord& rec,
	VuDiffMode diff_mode = VuDiffMode::PipelinePermissive,
	u32 cycle_budget_override = 0);

// Convenience: load + replay in one call. Returns a result with ok=false
// if the file can't be read or has bad magic/version/sizes.
VuReplayResult LoadAndReplay(const std::string& path,
	VuDiffMode diff_mode = VuDiffMode::PipelinePermissive,
	u32 cycle_budget_override = 0);

// Run the JIT against the capture `iters` times, measuring PMU counters
// per iteration. The first iter pays a one-time JIT compile cost; the
// per-iter cycles will drop after that. Caller is expected to throw out
// the warmup iter when computing summary stats.
//
// Returns one Values entry per iter. Empty on setup failure (caller should
// check RecompilerTestEnvironment::IsReady() and PmuCounters::Group::Open()
// availability — the vector is empty if either fails).
std::vector<PmuCounters::Values> BenchJit(const vu_capture::CaptureRecord& rec,
	u32 iters,
	u32 cycle_budget_override = 0,
	bool reprime_per_iter = true);

// Re-seed the architectural + pipeline state from a capture WITHOUT dropping
// the compiled VU block cache (unlike the implicit prime BenchJit does, which
// resets the cache). Lets a bench loop re-run the whole program from its
// captured entry every iteration with no E-bit drift and no recompile.
void ReseedFromCapture(const vu_capture::CaptureRecord& rec);

// Steady-state JIT bench: prime+compile once on iter 0 (warmup), then re-seed
// (no cache reset) and measure Execute each subsequent iter, so every timed
// iter runs the full program through the already-compiled block plus the real
// mVU dispatch/search/exit envelope. Drop iter 0 when summarizing.
//
// Re-seeding each iter is what keeps an E-bit-terminating program measurable:
// without it the program halts at its E-bit, leaving the VU stopped so every
// later Execute returns having run nothing (~tens of insns of dispatch), which
// badly under-reports the real cost.
std::vector<PmuCounters::Values> BenchJitSteady(const vu_capture::CaptureRecord& rec,
	u32 iters);

// Compile the captured program once and dump the host code (ARM64) emitted
// for it as a textual disassembly, written to `out_path`. Intended for
// codegen-iteration A/B diffs: run --dump-asm, change an emitter, run
// --dump-asm again, diff the .codegen.s files.
//
// The dump covers `mVU.prog.x86start` to `mVU.prog.x86ptr` after a fresh
// Reset and one execute call — i.e. the bytes that this single program's
// compile placed in the cache. The first few entries before the program
// proper are typically the per-program dispatcher epilogue; everything
// after is real microcode codegen. ARM64 only.
//
// Returns false on file-open failure or if the recompiler environment
// isn't ready. On non-ARM64 hosts, the dump is a placeholder note
// indicating the disassembler isn't wired for this architecture.
bool DumpJitAsm(const vu_capture::CaptureRecord& rec, const std::string& out_path);

} // namespace recompiler_tests
