// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "RecompilerTestEnvironment.h"
#include "VuEncode.h"
#include "VuSnapshot.h"
// VuDiffMode is defined in VuSnapshot.h.

#include "common/Pcsx2Defs.h"

#include <gtest/gtest.h>
#include <initializer_list>
#include <vector>

namespace recompiler_tests {

// VU recompiler test harness. Drives one VU instance (0 or 1) through a short
// instruction-pair program and captures architectural state for diff. Run()
// executes the program through both the JIT and the interpreter and diffs the
// post-states; RunInterpOnly() drives the interpreter alone.
//
// Termination: the harness expects the user-supplied program to have its
// final pair tagged with the E-bit. `LoadProgram` validates this and appends
// a single NOP pair to honour the architectural one-pair delay slot after
// E-bit (see VU0microInterp.cpp:225-234 for the `VU->ebit-- == 1` cleanup
// cadence).
class VuTestHarness
{
public:
	explicit VuTestHarness(int vu_index);
	~VuTestHarness();

	VuTestHarness(const VuTestHarness&) = delete;
	VuTestHarness& operator=(const VuTestHarness&) = delete;

	// ---- Pre-state setters ----
	void SetVf(u32 reg_idx, float x, float y, float z, float w);
	void SetVfBits(u32 reg_idx, u32 x, u32 y, u32 z, u32 w);
	// Masks to the register's architectural width: 32-bit for the special VIs
	// (REG_R/I/Q/P/flags/TPC/FBRST/VPU_STAT), low 16 for the rest. For REG_Q/P
	// prefer SetQ/SetP, which also write the q.UL/p.UL float slot.
	void SetVi(u32 reg_idx, u32 value);
	void SetQ(u32 bits);
	void SetP(u32 bits);

	// VU data memory access — addr is a byte offset into VU.Mem and is
	// masked by VU0_MEMMASK / VU1_MEMMASK. Tracked windows are diffed
	// after Run().
	void WriteMemU32(u32 addr, u32 value);
	void WriteMemU128(u32 addr, u32 x, u32 y, u32 z, u32 w);
	u32  ReadMemU32(u32 addr) const;
	void TrackMemWindow(u32 addr, size_t bytes);

	// Skip a VI register in the JIT-vs-interp auto-diff. Use sparingly: this
	// is the right tool when JIT and interp legitimately disagree on a
	// bookkeeping register (e.g. REG_TPC after an M-bit mid-program break)
	// but the test's architectural assertions are still worth running.
	void IgnoreViInDiff(int reg_idx) { ignored_vi_.push_back(reg_idx); }

	// Override the JIT-vs-interp diff mode used by Run(). Default is
	// PipelinePermissive — XGKICK tests use XgkickPacketEquivalent to
	// silence the legitimately-divergent xgkick scratch fields and assert
	// architectural equivalence on the captured Path 1 packet stream.
	void SetDiffMode(VuDiffMode mode) { diff_mode_ = mode; }

	// ---- Program load ----
	// Each VuOp is a {lower, upper} pair written to VU.Micro at byte
	// offsets `pc, pc+8, ...` starting at `kProgramPc = 0`. The final
	// user-supplied pair must have the E-bit set; a follow-up NOP pair
	// is appended automatically as the architectural E-bit delay slot.
	// The vector overload exists for programs built at runtime (e.g. the
	// cache-exhaustion test's generated chains).
	void LoadProgram(std::initializer_list<vu::VuOp> pairs);
	void LoadProgram(std::vector<vu::VuOp> pairs);

	// ---- Execute ----
	// Runs the program through the interpreter and through the JIT,
	// captures both post-states, and gtest-fails on any architectural
	// divergence.
	void Run();

	// One-sided execution against the interpreter only. Both `JitSnapshot()`
	// and `InterpSnapshot()` reflect the interpreter result. Use when
	// authoring a new test before the JIT path is ready.
	void RunInterpOnly();

	// JIT-only re-run from the SAME pre-state as the last Run(), WITHOUT
	// resetting the VU block cache first — compiled (or hydrated) blocks
	// survive into this execution. Updates JitSnapshot(); performs no diff.
	// Used by the persisted-JIT round-trip tests, where the whole point is
	// asserting the pre-seeded block graph runs without a recompile.
	void RunJitPreserveBlockCache();

	// ---- Post-run accessors ----
	u32 GetVfBitsInterp(u32 reg_idx, char lane) const;
	u32 GetVfBitsJit(u32 reg_idx, char lane) const;
	float GetVfInterp(u32 reg_idx, char lane) const;
	float GetVfJit(u32 reg_idx, char lane) const;
	u32 GetViInterp(u32 reg_idx) const;
	u32 GetViJit(u32 reg_idx) const;
	u32 GetMemU32Interp(u32 addr) const;
	u32 GetMemU32Jit(u32 addr) const;

	// Returns true once the most recent Run() / RunInterpOnly() has cleared
	// the VU's running-bit in VU0.VI[REG_VPU_STAT]. The running-bit lives
	// in VU0's VI for both VU0 and VU1 (see VU1microInterp.cpp:309), so a
	// per-VU snapshot diff cannot reach it directly.
	bool HasTerminated() const;

	// Bytes of every GIF Path 1 packet emitted via XGKICK during the most
	// recent Run() / RunInterpOnly(). Captured by the test-only sink wired
	// into Gif_Unit::TransferGSPacketData (see Gif_Unit.h gif_test_hooks).
	// Packets are appended back-to-back; tests that care about boundaries
	// can re-parse the GIF tags.
	//
	// Path1PacketBytesJit/Interp() return the per-pass capture that Run()
	// builds — used by XGKICK tests to assert "JIT and interp emit the same
	// architectural GIF packet stream" even when their internal scratch
	// state legitimately diverges (see VuDiffMode::XgkickPacketEquivalent).
	const std::vector<u8>& Path1PacketBytes()      const { return path1_packets_; }
	const std::vector<u8>& Path1PacketBytesJit()   const { return path1_packets_jit_; }
	const std::vector<u8>& Path1PacketBytesInterp() const { return path1_packets_interp_; }

	const VuSnapshot& JitSnapshot() const { return jit_snapshot_; }
	const VuSnapshot& InterpSnapshot() const { return interp_snapshot_; }

	int VuIndex() const { return vu_index_; }

	// Program memory base — pair 0 lives here, pair N at +8N. Always 0
	// for the harness (start of VU.Micro).
	static constexpr u32 kProgramPc = 0;

private:
	void SeedEntryState(bool reset_block_cache = true);
	void RunInterpFromSeeded();
	void RunJitFromSeeded();
	void WriteProgramToMicro();
	void MergeTrackedWindow(u32 addr, size_t bytes);

	int vu_index_;
	std::vector<vu::VuOp> program_pairs_;
	std::vector<VuMemWindow> mem_windows_;
	std::vector<int> ignored_vi_;
	VuDiffMode diff_mode_ = VuDiffMode::PipelinePermissive;
	std::vector<u8> path1_packets_;
	std::vector<u8> path1_packets_jit_;
	std::vector<u8> path1_packets_interp_;

	VuSnapshot pre_snapshot_;
	VuSnapshot jit_snapshot_;
	VuSnapshot interp_snapshot_;
	bool has_run_ = false;

	// Cycle budget — generous for short 2-256-instruction test programs.
	// Real microprograms can run thousands of cycles but the harness's
	// E-bit terminator caps every test deterministically.
	static constexpr u32 kCycleBudget = 4096;
};

} // namespace recompiler_tests
