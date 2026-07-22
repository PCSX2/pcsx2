// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "MipsEncode.h"
#include "RecompilerTestEnvironment.h"
#include "StateSnapshot.h"
#include "VuEncode.h"
#include "VuSnapshot.h"

#include "common/Pcsx2Defs.h"

#include <gtest/gtest.h>
#include <initializer_list>
#include <vector>

namespace recompiler_tests {

// EE recompiler test harness. Always executes both JIT and interpreter
// paths and diffs architectural state.
class EeRecTestHarness
{
public:
	EeRecTestHarness();
	~EeRecTestHarness();

	EeRecTestHarness(const EeRecTestHarness&) = delete;
	EeRecTestHarness& operator=(const EeRecTestHarness&) = delete;

	// ---- Pre-state setters ----

	void SetGpr64(u32 reg_idx, u64 value);
	void SetGpr128(u32 reg_idx, u64 lo, u64 hi);
	void SetGpr(u32 reg_idx, u32 value) { SetGpr64(reg_idx, static_cast<s64>(static_cast<s32>(value))); }
	void SetHi64(u64 value);
	void SetLo64(u64 value);
	// Full 128-bit LO/HI setters — PMFHL reads HI.UD[1] / LO.UD[1] (the
	// "MULT1/DIV1 upper half") in addition to the lower 64 bits, so tests
	// for that op need to seed both halves independently.
	void SetLoPair(u64 lo_qw, u64 hi_qw);
	void SetHiPair(u64 lo_qw, u64 hi_qw);
	void SetCp0(u32 reg_idx, u32 value);
	void SetFpr(u32 reg_idx, float value);
	void SetFprSingle(u32 reg_idx, float value) { SetFpr(reg_idx, value); }
	void SetFprBits(u32 reg_idx, u32 bits);
	void SetAcc(float value);
	void SetAccBits(u32 bits);
	void SetFcr31(u32 value);

	// MMI alias — the PS2's 128-bit paired-word MMI ops address the same GPR
	// file as the scalar ops; SetMmiPair is purely a documentation hint at
	// call-sites that the test is exercising the upper 64 bits.
	void SetMmiPair(u32 reg_idx, u64 lo_qw, u64 hi_qw) { SetGpr128(reg_idx, lo_qw, hi_qw); }

	// Privileged-mode bringup. Tests that exercise MTC0/MFC0/ERET/trap
	// delivery want CU[0]=1 so the opcode doesn't trap on a coprocessor-
	// unusable exception; FPU tests want CU[1]=1 for the same reason.
	// Both are no-ops if Status is already configured.
	void EnableCop0();
	void EnableCop1();
	void SetStatusBits(u32 mask);

	// Enables the PS2 "Full" FPU clamp mode (CHECK_FPU_FULL / GameDB eeClampMode:3)
	// for the JIT recompile. The shared interpreter has no double-precision path,
	// so full-mode tests must use RunJitNoDiff() and assert GetFprBitsJit() against
	// a hand-computed double-mode value rather than the auto-diffing Run(). Restored
	// to its previous value in the dtor.
	void EnableFpuFullMode();
	void EnableFpuMulHack();

	// Turns OFF the (default-ON) fpuGuardedAddSub Recompiler option so the JIT
	// emits a plain single-precision add/sub with no guard-bit masking — the
	// opt-out perf path. Off makes the JIT bit-identical to the single-precision
	// interpreter (which never masks). Restored to its previous value in the dtor.
	void DisableFpuGuarded();

	// ---- Memory ----

	void WriteU8 (u32 addr, u8  value);
	void WriteU16(u32 addr, u16 value);
	void WriteU32(u32 addr, u32 value);
	void WriteU64(u32 addr, u64 value);
	void WriteBytes(u32 addr, const void* src, size_t bytes);

	u8  ReadU8 (u32 addr) const;
	u16 ReadU16(u32 addr) const;
	u32 ReadU32(u32 addr) const;
	u64 ReadU64(u32 addr) const;

	void TrackMemWindow(u32 addr, size_t bytes);

	// Exercises the SMC invalidation path directly — stores one word at
	// `hw_addr` via the memWrite/vtlb path that triggers `Cpu->Clear()` on
	// any compiled EE block covering that address.
	void TriggerSmc(u32 hw_addr, u32 value);

	// Mimics vtlb.cpp's `Cpu->Clear(faulting_pc, 1)` call from the SIGSEGV
	// fastmem-backpatch handler — the production entry point for mid-block
	// SMC invalidation. Tests that need to assert the recClear straddler
	// behavior call this between a Run(PreserveCache) pair so the second
	// dispatch must re-recompile rather than reuse the stale block.fnptr
	// from the first run.
	void SimulateFastmemFault(u32 faulting_pc);

	// ---- Program load ----

	void LoadProgram(std::initializer_list<u32> instructions);
	void LoadProgram(const std::vector<u32>& instructions);
	void LoadProgramNoTerm(std::initializer_list<u32> instructions);
	void LoadProgramNoTerm(const std::vector<u32>& instructions);

	// ---- Execute ----

	// FreshCache (default) — Run() invalidates any cached block at kProgramPc /
	// kParkingPc before the JIT pass. Standard mode for one-shot tests where
	// the JIT must compile the program from scratch.
	//
	// PreserveCache — skips the pre-JIT Cpu->Clear calls. Used by SMC tests
	// that want a *second* Run() after SimulateFastmemFault() to re-dispatch
	// against partially-invalidated blocks (verifying that recClear correctly
	// resets straddler block.fnptr so the second dispatch re-compiles cleanly,
	// rather than jumping into removed compiled code).
	enum class RunMode { FreshCache, PreserveCache };

	// Runs both interpreter and JIT paths from the same pre-state,
	// captures both post-states, and fails the test via gtest if any
	// architecturally-significant field diverges.
	void Run(RunMode mode = RunMode::FreshCache);

	// JIT-only execution with no interp run and no JIT-vs-interp diff. For tests
	// whose JIT path legitimately diverges from the single-precision interpreter
	// (the FPU full-mode DOUBLE path) — assert GetFprBitsJit()/GetAccBitsJit()
	// against an independently-computed expected value.
	void RunJitNoDiff(RunMode mode = RunMode::FreshCache);

	// Authoring mode — one-sided execution, useful when drafting a new
	// test before the corresponding JIT opcode handler exists. The JIT
	// snapshot is set to equal the interp snapshot so `GetGprJit*()`
	// calls return the expected value regardless.
	void RunInterpOnly();

	// ---- Post-run accessors ----

	u64 GetGpr64Interp(u32 reg_idx) const;
	u64 GetGpr64Jit   (u32 reg_idx) const;
	u32 GetGprInterp  (u32 reg_idx) const { return static_cast<u32>(GetGpr64Interp(reg_idx)); }
	u32 GetGprJit     (u32 reg_idx) const { return static_cast<u32>(GetGpr64Jit(reg_idx)); }
	u64 GetHi64Interp() const;
	u64 GetLo64Interp() const;
	u32 GetFprBitsInterp(u32 reg_idx) const;
	u32 GetFprBitsJit   (u32 reg_idx) const;
	u32 GetAccBitsInterp() const;
	u32 GetAccBitsJit   () const;
	u32 GetCp0Interp(u32 reg_idx) const;
	u32 GetCp0Jit   (u32 reg_idx) const;

	// ---- Expect helpers (paired-side architectural assertions) ----
	//
	// Each helper asserts that *both* the JIT and interp snapshots hold the
	// expected value. Divergence between the two is already caught by Run()'s
	// internal diff; these helpers go further and require that both sides
	// match the *expected* value — catches cases where the test and the real
	// behavior agree with each other but disagree with the spec.

	void ExpectGpr64(u32 reg_idx, u64 expected) const;
	void ExpectGpr128(u32 reg_idx, u64 lo, u64 hi) const;
	void ExpectMmiPair(u32 reg_idx, u64 lo_qw, u64 hi_qw) const { ExpectGpr128(reg_idx, lo_qw, hi_qw); }
	void ExpectFpr(u32 reg_idx, u32 bits) const;
	void ExpectAcc(u32 bits) const;

	// Introspects the `recBlocks` BaseBlocks map after Run() and asserts that
	// a LinkArm64 patch site within the block containing `src_pc` targets
	// `dst_pc`. Backed by the `recEeIsBlockLinked(src_pc, dst_pc)` query
	// using BaseBlocks::Arm64Links().
	void ExpectBlockLinked(u32 src_pc, u32 dst_pc) const;

	const EeSnapshot& JitSnapshot() const { return jit_snapshot_; }
	const EeSnapshot& InterpSnapshot() const { return interp_snapshot_; }

	// ---- VU0 cross-tree handoff support ----
	//
	// Off by default. Tests that exercise COP2 macro mode, VCALLMS,
	// CFC2/CTC2/QMFC2/QMTC2/LQC2/SQC2 must call EnableVu0Capture() once
	// before LoadProgram() so Run() snapshots VU0 state on both sides
	// and includes any divergence in the diff. The VU0 snapshot is
	// scoped to the architectural register file; pipeline flags are
	// compared in PipelinePermissive mode (the JIT and interp legitimately
	// reorder writes within a stage — see VuSnapshot.h).
	void EnableVu0Capture();

	// Opt a specific VU0 VI index out of Run()'s JIT-vs-interp auto-diff.
	// Used when the JIT and interp legitimately disagree on a register by
	// design (e.g. REG_STATUS_FLAG: the microVU JIT path masks the write to
	// the 0xFC0 sticky field + denormalizes into micro_statusflags, while the
	// shared interpreter CTC2 does a plain full-width VI store). The test then
	// asserts the JIT post-state directly via Vu0JitSnapshot(). Forwarded to
	// DiffVu's ignored_vi parameter.
	void IgnoreVu0Vi(u32 reg_idx) { vu0_ignored_vi_.push_back(static_cast<int>(reg_idx)); }

	void SeedVu0Vf(u32 reg_idx, float x, float y, float z, float w);
	void SeedVu0VfBits(u32 reg_idx, u32 x, u32 y, u32 z, u32 w);
	void SeedVu0Acc(float x, float y, float z, float w);
	void SeedVu0AccBits(u32 x, u32 y, u32 z, u32 w);
	void SeedVu0Vi(u32 reg_idx, u32 value);
	void SeedVu0Microprogram(u32 byte_offset, std::initializer_list<vu::VuOp> pairs);

	u32   GetVu0VfBitsJit   (u32 reg_idx, char lane) const;
	u32   GetVu0VfBitsInterp(u32 reg_idx, char lane) const;
	float GetVu0VfJit       (u32 reg_idx, char lane) const;
	float GetVu0VfInterp    (u32 reg_idx, char lane) const;
	u32   GetVu0AccBitsJit   (char lane) const;
	u32   GetVu0AccBitsInterp(char lane) const;
	u32   GetVu0ViJit       (u32 reg_idx) const;
	u32   GetVu0ViInterp    (u32 reg_idx) const;

	const VuSnapshot& Vu0JitSnapshot() const { return vu0_jit_snapshot_; }
	const VuSnapshot& Vu0InterpSnapshot() const { return vu0_interp_snapshot_; }

	// ---- EE↔VU1 VIF dispatch support ----
	//
	// Off by default. Tests that exercise MSCAL / MSCALF / MSCNT call
	// EnableVu1VifCapture() once before LoadProgram() so Run() will:
	//   (1) reset vif1 + vif1Regs to a known baseline,
	//   (2) seed VIF1 state set via SetVif1*/SetGifPath1Busy,
	//   (3) after each EE pass, swap CpuVU1 to the matching engine
	//       (CpuMicroVU1 for the JIT pass, CpuIntVU1 for the interp pass)
	//       and fire each queued VIF1 command via vifExecQueue(1) under
	//       INSTANT_VU1=true so the microprogram runs to completion,
	//   (4) snapshot VU1 (architectural register file + a 1 KiB scratchpad
	//       window) on both sides and diff in PipelinePermissive mode.
	//
	// vif1Reset() + path-1-busy stub run between passes so the second pass
	// fires from the same baseline as the first.
	void EnableVu1VifCapture();

	void SeedVu1Microprogram(u32 byte_offset, std::initializer_list<vu::VuOp> pairs);
	void SeedVu1Vf(u32 reg_idx, float x, float y, float z, float w);
	void SeedVu1VfBits(u32 reg_idx, u32 x, u32 y, u32 z, u32 w);
	void SeedVu1Vi(u32 reg_idx, u32 value);

	// VIF1 command queue. Each command is fired in the order queued during
	// each pass (JIT and interp) — direct injection bypasses the DMAC + FIFO,
	// mimicking the post-tag state that vif1Transfer would land in.
	void QueueVif1Mscal(u16 microprogram_addr);   // 0x14XXXXXX
	void QueueVif1Mscalf(u16 microprogram_addr);  // 0x15XXXXXX (path-1 wait)
	void QueueVif1Mscnt();                        // 0x17000000

	// State applied to vif1Regs / vif1 / gif_test_hooks at the start of each
	// VIF1 pass (after vif1Reset, before any queued command fires).
	void SetGifPath1Busy(bool busy);
	void SetVif1WaitForVu(bool wait);
	void SetVif1Doublebuffer(u16 base_qw, u16 ofst_qw);

	// Post-Run() VU1 register accessors. *Jit / *Interp variants return the
	// snapshot taken at the end of the matching pass. Use ExpectVu1* to
	// assert both sides agree on a specific value.
	float GetVu1VfJit       (u32 reg_idx, char lane) const;
	float GetVu1VfInterp    (u32 reg_idx, char lane) const;
	u32   GetVu1VfBitsJit   (u32 reg_idx, char lane) const;
	u32   GetVu1VfBitsInterp(u32 reg_idx, char lane) const;
	u32   GetVu1ViJit       (u32 reg_idx) const;
	u32   GetVu1ViInterp    (u32 reg_idx) const;

	bool HasVu1TerminatedJit()    const;
	bool HasVu1TerminatedInterp() const;

	// Post-Run() VIF1 register accessors — the dispatch-side state both
	// engines should agree on (stat.VGW after a path-1-busy MSCALF, the
	// double-buffered tops/top values, etc.).
	u32 GetVif1StatJit()   const;
	u32 GetVif1StatInterp() const;
	u32 GetVif1TopsJit()   const;
	u32 GetVif1TopsInterp() const;
	u32 GetVif1TopJit()    const;
	u32 GetVif1TopInterp() const;

	// Convenience — both sides must equal `expected`.
	void ExpectVu1Vf(u32 reg_idx, float x, float y, float z, float w) const;
	void ExpectVu1Vi(u32 reg_idx, u32 expected) const;

	const VuSnapshot& Vu1JitSnapshot()    const { return vu1_jit_snapshot_; }
	const VuSnapshot& Vu1InterpSnapshot() const { return vu1_interp_snapshot_; }

private:
	static constexpr s32 kCycleBudget = 1024;
	static constexpr u32 kMaxInstructions = 2048;

	void LoadProgramImpl(std::initializer_list<u32> instructions, bool append_term);
	void SeedEntryState();
	void StepInterpUntilParkedOrTimeout();
	void MergeTrackedWindow(u32 addr, size_t bytes);

	// Fire one VIF1 dispatch pass (JIT or interp). Resets vif1 state,
	// applies SetVif1*/SetGifPath1Busy values, swaps CpuVU1 to the
	// matching engine, and runs each queued command via vifExecQueue(1).
	void FireVif1Pass(bool jit);

	std::vector<u32> program_words_;
	std::vector<MemWindow> mem_windows_;

	EeSnapshot pre_snapshot_;
	EeSnapshot jit_snapshot_;
	EeSnapshot interp_snapshot_;

	bool capture_vu0_ = false;
	std::vector<int> vu0_ignored_vi_;
	VuSnapshot vu0_pre_snapshot_;
	VuSnapshot vu0_jit_snapshot_;
	VuSnapshot vu0_interp_snapshot_;

	// VIF1 dispatch capture state.
	struct PendingVifCmd
	{
		enum Kind { Mscal, Mscalf, Mscnt };
		Kind kind;
		u32 code; // full vif1Regs.code value to install
	};
	bool capture_vu1_ = false;
	bool vu1_state_path1_busy_ = false;
	bool vu1_state_waitforvu_ = false;
	bool vu1_state_dbf_set_ = false;
	u16 vu1_state_base_qw_ = 0;
	u16 vu1_state_ofst_qw_ = 0;
	std::vector<PendingVifCmd> vif1_queue_;
	VuSnapshot vu1_pre_snapshot_;
	VuSnapshot vu1_jit_snapshot_;
	VuSnapshot vu1_interp_snapshot_;

	// VIF1-side post-state captured at the end of each FireVif1Pass — kept
	// as raw u32 to avoid pulling Vif.h into this header.
	struct Vif1PostState
	{
		u32 stat;
		u32 tops;
		u32 top;
	};
	Vif1PostState vif1_jit_post_{};
	Vif1PostState vif1_interp_post_{};

	// Owned storage for the GIF path-1 sink while capture_vu1_ is on. Drains
	// any incidental XGKICK during VU1 termination so MTGS::WaitGS doesn't
	// fire. Cleared in the dtor.
	std::vector<u8> vu1_path1_sink_;

	bool has_run_ = false;

	bool fpu_full_mode_changed_ = false;
	bool prev_fpu_full_mode_ = false;
	bool fpu_mul_hack_changed_ = false;
	bool prev_fpu_mul_hack_ = false;
	bool fpu_guarded_changed_ = false;
	bool prev_fpu_guarded_ = false;
};

} // namespace recompiler_tests
