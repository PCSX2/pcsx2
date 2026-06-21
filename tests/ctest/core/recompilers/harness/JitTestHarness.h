// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "MipsEncode.h"
#include "RecompilerTestEnvironment.h"
#include "StateSnapshot.h"

#include "common/Pcsx2Defs.h"

#include <gtest/gtest.h>
#include <initializer_list>
#include <vector>

namespace recompiler_tests {

// In-process differential test harness for the IOP recompiler.
//
// Each test instance owns a fresh pre-state. The usage contract is:
//
//     JitTestHarness h;
//     h.SetGpr(reg::a0, 100);                  // seed register state
//     h.WriteU32(0x00020000, 0xDEADBEEF);      // seed memory
//     h.LoadProgram({                          // test program
//         ADDIU(reg::v0, reg::a0, 7),
//         JR(reg::ra),
//         NOP,                                 // branch delay slot
//     });
//     h.Run();                                 // runs BOTH paths, gtest-diffs
//     EXPECT_EQ(h.GetGprJit(reg::v0), 107u);   // optional spec-lock check
//
// Run() executes the program twice:
//   1. Through the IOP recompiler (psxRec.ExecuteBlock).
//   2. From the same pre-state, through the interpreter (psxInt.ExecuteBlock).
// It captures architectural state after each and fails the enclosing gtest
// assertion on any field-level divergence (GPR / HI / LO / CP0 / PC / mem).
//
// When the IOP recompiler is not available on the host, set `mode` to
// HarnessMode::InterpOnly — the JIT path is skipped and the interpreter
// output is locked as the spec.
class JitTestHarness
{
public:
	enum class Mode
	{
		DiffJitVsInterp,   // run both JIT + interp, gtest-diff
		InterpOnly,        // skip JIT; only captures interpreter post-state
	};

	explicit JitTestHarness(Mode mode = Mode::DiffJitVsInterp);
	~JitTestHarness();

	JitTestHarness(const JitTestHarness&) = delete;
	JitTestHarness& operator=(const JitTestHarness&) = delete;

	// ---- Pre-state setters (mutate psxRegs directly) ----

	void SetGpr(u32 reg_idx, u32 value);
	void SetHi(u32 value);
	void SetLo(u32 value);
	void SetCp0(u32 reg_idx, u32 value);

	// Write to IOP guest memory at the given byte address. Automatically
	// registers the write's address range as a "mem window" that Run() will
	// snapshot + diff after execution.
	void WriteU8 (u32 addr, u8  value);
	void WriteU16(u32 addr, u16 value);
	void WriteU32(u32 addr, u32 value);
	void WriteBytes(u32 addr, const void* src, size_t bytes);

	// Read back guest memory after Run(). For SW / SB / SH verification.
	u8  ReadU8 (u32 addr) const;
	u16 ReadU16(u32 addr) const;
	u32 ReadU32(u32 addr) const;

	// Declare a memory window that should be compared after Run(). Useful
	// for tests that expect a *store* to write something — this lets the
	// diff notice the write.
	void TrackMemWindow(u32 addr, size_t bytes);

	// ---- Program load ----

	// Writes `instructions` to IOP RAM at kProgramPc, then appends a
	// terminator (`jr ra; nop`). The caller is responsible for any in-block
	// branches / delay slots; the terminator's delay slot is a NOP.
	//
	// Before Run(), `ra` will be set to kParkingPc so the terminator lands
	// in the parking-lot tight loop.
	void LoadProgram(std::initializer_list<u32> instructions);

	// Writes `instructions` verbatim with no terminator. Use for branch /
	// jump tests where the author needs to control the exact program
	// layout — typically by ending each path with an explicit `JR(ra)`
	// or `J(kParkingPc)` + NOP.
	void LoadProgramNoTerm(std::initializer_list<u32> instructions);

	// Writes `instructions` to arbitrary IOP RAM address `pc`. Appends the
	// `jr ra; nop` terminator when `append_jr_ra_term` is true.
	//
	// Use for multi-block tests that lay blocks out at distinct addresses
	// and for SMC tests that want to mutate a specific region. Caller picks
	// the mirror (physical `0x00000000`, kseg0 `0x80000000`, kseg1
	// `0xa0000000` — the JIT dispatches via the same RAM either way).
	//
	// Each call adds a region to the harness's invalidation list; Run()
	// invalidates every registered region before compiling.
	void LoadProgramAt(u32 pc,
	                   std::initializer_list<u32> instructions,
	                   bool append_jr_ra_term = false);

	// Same, but takes a runtime-sized buffer — used by tests that compose
	// long programs via std::vector.
	void LoadProgramAt(u32 pc,
	                   const u32* instructions,
	                   size_t count,
	                   bool append_jr_ra_term = false);

	// ---- Entry-point overrides ----

	// Override the PC / RA seeded by Run(). If not set, Run() defaults to
	// `pc = kProgramPc` and `ra = kParkingPc`. Multi-block tests that enter
	// at block 1's address use the default for pc; SMC tests that jump back
	// through the just-mutated program after `RunResume()` call `SetPc()`
	// explicitly. Cleared on every Run/RunResume.
	void SetPc(u32 pc);
	void SetRa(u32 ra);

	// ---- Execution ----

	// Runs the program through both JIT and interpreter (or just interpreter
	// if `mode == InterpOnly`), captures post-state, and issues a gtest
	// non-fatal failure (EXPECT_*) if the two paths diverge.
	//
	// After Run(), the global psxRegs contains the *interpreter* post-state
	// (so GetGprInterp() and direct psxRegs reads agree).
	void Run();

	// Like Run(), but does NOT reset pc / ra or invalidate program regions.
	// Used after mutating program memory via WriteU32/iopMemWrite32 (SMC
	// tests) or as a follow-on after a previous Run() to drive the same
	// state through another dispatch. `SetPc()` before RunResume() is the
	// intended way to re-enter the program.
	void RunResume();

	// ---- Post-run accessors ----
	// Valid only after Run().

	u32 GetGprJit(u32 reg_idx) const;
	u32 GetGprInterp(u32 reg_idx) const;
	u32 GetHiJit() const;
	u32 GetLoJit() const;
	u32 GetHiInterp() const;
	u32 GetLoInterp() const;

	const IopSnapshot& JitSnapshot() const { return jit_snapshot_; }
	const IopSnapshot& InterpSnapshot() const { return interp_snapshot_; }

private:
	// EE-cycle budget passed to ExecuteBlock. The interpreter ALWAYS runs
	// at least one branch-delimited block to completion before checking the
	// budget (the inner loop exits only on a taken branch). For branch
	// tests where the taken path lands in a second block terminated by
	// `J kParkingPc`, we need the budget to allow a second iteration.
	//
	// 200 EE cycles covers ~25 IOP instructions — plenty for any unit test
	// layout without running the parking-lot self-loop forever. Any block
	// that lands in the parking lot hits `j self; nop`, a 2-instruction
	// IOP block that burns 16 EE cycles per iteration — the budget still
	// runs out in ~12 parking iterations, bounded at sub-microsecond.
	static constexpr s32 kCycleBudget = 200;

	struct ProgramRegion
	{
		u32 pc;
		u32 size_words; // recClearIOP / psxRec.Clear take a word count, not bytes
	};

	void InvalidateProgramRegions();
	void MergeTrackedWindow(u32 addr, size_t bytes);
	void ExecuteAndDiff();

	Mode mode_;
	R3000Acpu* saved_psxCpu_ = nullptr;           // restored in dtor
	std::vector<ProgramRegion> program_regions_; // every LoadProgramAt call
	std::vector<MemWindow> mem_windows_;         // windows the diff watches

	bool pc_override_ = false;
	u32 pc_override_value_ = 0;
	bool ra_override_ = false;
	u32 ra_override_value_ = 0;

	IopSnapshot pre_snapshot_;                   // captured at Run() entry
	IopSnapshot jit_snapshot_;                   // post-JIT
	IopSnapshot interp_snapshot_;                // post-interp
	bool has_run_ = false;
};

} // namespace recompiler_tests
