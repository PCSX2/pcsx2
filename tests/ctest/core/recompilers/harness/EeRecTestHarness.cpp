// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "EeRecTestHarness.h"

#include "Config.h"
#include "Gif_Unit.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900.h"
#include "VU.h"
#include "VUmicro.h"
#include "Vif.h"
#include "Vif_Dma.h"

#include <cstring>
#include <sstream>

// Defined by the EE recompiler. Not part of R5900cpu (which has no
// ExecuteBlock member); called directly to drive the EE rec path for a
// bounded number of guest cycles. Returns cycles actually consumed.
extern s32 recEeExecuteBlock(s32 cycles, u32 park_pc);
extern void vifExecQueue(int idx);

namespace recompiler_tests {

namespace {

constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kParkingPc = RecompilerTestEnvironment::kParkingPc;

void ZeroCpuRegs()
{
	std::memset(&cpuRegs, 0, sizeof(cpuRegs));
	std::memset(&fpuRegs, 0, sizeof(fpuRegs));
}

} // namespace

EeRecTestHarness::EeRecTestHarness()
{
	EXPECT_TRUE(RecompilerTestEnvironment::IsReady())
		<< "RecompilerTestEnvironment was not set up — harness cannot run. "
		   "Make sure the binary's main.cpp registered the environment.";
	ZeroCpuRegs();
}

EeRecTestHarness::~EeRecTestHarness()
{
	if (capture_vu1_)
	{
		gif_test_hooks::g_path1_sink = nullptr;
		gif_test_hooks::g_force_path1_busy = false;
		// Restore the env-default INSTANT_VU1 flag (matches the value
		// RecompilerTestEnvironment sets at setup).
		EmuConfig.Speedhacks.vu1Instant = false;
	}

	if (fpu_full_mode_changed_)
		EmuConfig.Cpu.Recompiler.fpuFullMode = prev_fpu_full_mode_;

	if (fpu_mul_hack_changed_)
		EmuConfig.Gamefixes.FpuMulHack = prev_fpu_mul_hack_;
}

void EeRecTestHarness::SetGpr64(u32 reg_idx, u64 value)
{
	if (reg_idx == 0)
		return;
	cpuRegs.GPR.r[reg_idx].UD[0] = value;
	cpuRegs.GPR.r[reg_idx].UD[1] = 0;
}

void EeRecTestHarness::SetGpr128(u32 reg_idx, u64 lo, u64 hi)
{
	if (reg_idx == 0)
		return;
	cpuRegs.GPR.r[reg_idx].UD[0] = lo;
	cpuRegs.GPR.r[reg_idx].UD[1] = hi;
}

void EeRecTestHarness::SetHi64(u64 value) { cpuRegs.HI.UD[0] = value; cpuRegs.HI.UD[1] = 0; }
void EeRecTestHarness::SetLo64(u64 value) { cpuRegs.LO.UD[0] = value; cpuRegs.LO.UD[1] = 0; }
void EeRecTestHarness::SetLoPair(u64 lo_qw, u64 hi_qw) { cpuRegs.LO.UD[0] = lo_qw; cpuRegs.LO.UD[1] = hi_qw; }
void EeRecTestHarness::SetHiPair(u64 lo_qw, u64 hi_qw) { cpuRegs.HI.UD[0] = lo_qw; cpuRegs.HI.UD[1] = hi_qw; }

void EeRecTestHarness::SetCp0(u32 reg_idx, u32 value) { cpuRegs.CP0.r[reg_idx] = value; }

void EeRecTestHarness::SetFpr(u32 reg_idx, float value)    { fpuRegs.fpr[reg_idx].f = value; }
void EeRecTestHarness::SetFprBits(u32 reg_idx, u32 bits)   { fpuRegs.fpr[reg_idx].UL = bits; }
void EeRecTestHarness::SetAcc(float value)                 { fpuRegs.ACC.f = value; }
void EeRecTestHarness::SetAccBits(u32 bits)                { fpuRegs.ACC.UL = bits; }
void EeRecTestHarness::SetFcr31(u32 value)                 { fpuRegs.fprc[31] = value; }

void EeRecTestHarness::EnableCop0()      { cpuRegs.CP0.n.Status.val |= (1u << 28); /* CU0 */ }
void EeRecTestHarness::EnableCop1()      { cpuRegs.CP0.n.Status.val |= (1u << 29); /* CU1 */ }

void EeRecTestHarness::EnableFpuFullMode()
{
	if (!fpu_full_mode_changed_)
	{
		prev_fpu_full_mode_ = EmuConfig.Cpu.Recompiler.fpuFullMode;
		fpu_full_mode_changed_ = true;
	}
	EmuConfig.Cpu.Recompiler.fpuFullMode = true;
}

void EeRecTestHarness::EnableFpuMulHack()
{
	if (!fpu_mul_hack_changed_)
	{
		prev_fpu_mul_hack_ = EmuConfig.Gamefixes.FpuMulHack;
		fpu_mul_hack_changed_ = true;
	}
	EmuConfig.Gamefixes.FpuMulHack = true;
}
void EeRecTestHarness::SetStatusBits(u32 mask) { cpuRegs.CP0.n.Status.val |= mask; }

// EE vtlb_memWrite on a direct RAM hit bypasses Cpu->Clear — upstream relies
// on mmap-level write protection + a SIGSEGV handler to catch SMC, which
// isn't wired in the harness. Invalidate any compiled block covering the
// target address explicitly so multi-block tests that write into neighbour
// regions (e.g. kBlockBPc) don't execute a previous test's cached block.
// (IOP's iopMemWrite* sidesteps this by calling psxCpu->Clear itself.)
static void HarnessInvalidate(u32 addr, u32 size_words)
{
	if (recCpu.Clear)
		recCpu.Clear(addr & ~0x3u, size_words);
}

void EeRecTestHarness::WriteU8(u32 addr, u8 value)
{
	memWrite8(addr, value);
	HarnessInvalidate(addr, 1);
	MergeTrackedWindow(addr & ~0x3u, 4);
}
void EeRecTestHarness::WriteU16(u32 addr, u16 value)
{
	memWrite16(addr, value);
	HarnessInvalidate(addr, 1);
	MergeTrackedWindow(addr & ~0x3u, 4);
}
void EeRecTestHarness::WriteU32(u32 addr, u32 value)
{
	memWrite32(addr, value);
	HarnessInvalidate(addr, 1);
	MergeTrackedWindow(addr & ~0x3u, 4);
}
void EeRecTestHarness::WriteU64(u32 addr, u64 value)
{
	memWrite64(addr, value);
	HarnessInvalidate(addr, 2);
	MergeTrackedWindow(addr & ~0x7u, 8);
}
void EeRecTestHarness::WriteBytes(u32 addr, const void* src, size_t bytes)
{
	const u8* p = static_cast<const u8*>(src);
	for (size_t i = 0; i < bytes; ++i)
		memWrite8(addr + static_cast<u32>(i), p[i]);
	HarnessInvalidate(addr, static_cast<u32>((bytes + 3) / 4));
	MergeTrackedWindow(addr, bytes);
}

u8  EeRecTestHarness::ReadU8 (u32 addr) const { return memRead8 (addr); }
u16 EeRecTestHarness::ReadU16(u32 addr) const { return memRead16(addr); }
u32 EeRecTestHarness::ReadU32(u32 addr) const { return memRead32(addr); }
u64 EeRecTestHarness::ReadU64(u32 addr) const { return memRead64(addr); }

void EeRecTestHarness::TrackMemWindow(u32 addr, size_t bytes) { MergeTrackedWindow(addr, bytes); }

void EeRecTestHarness::TriggerSmc(u32 hw_addr, u32 value)
{
	// memWrite32 runs through the vtlb store path which calls Cpu->Clear on
	// a hit — the SMC invalidation trigger. recRecompile compiles blocks
	// (and caches them in recBlocks/recLUT), so recClear's range-invalidate
	// removes those entries and re-seeds the BASEBLOCK slots with eeJITCompile
	// so the next dispatch re-compiles.
	memWrite32(hw_addr, value);
	MergeTrackedWindow(hw_addr & ~0x3u, 4);
}

void EeRecTestHarness::SimulateFastmemFault(u32 faulting_pc)
{
	// vtlb.cpp's SIGSEGV backpatch handler calls Cpu->Clear(faulting_pc, 1)
	// on a fastmem store-into-code-page fault. This mirrors that single
	// entry point — recClear should reset BLOCK->fnptr to JITCompile for
	// every block whose extent covers `faulting_pc`, including straddlers
	// (block startpc < faulting_pc < block endpc). This helper is the
	// regression gate for straddler coverage.
	if (recCpu.Clear)
		recCpu.Clear(faulting_pc & ~0x3u, 1);
}

void EeRecTestHarness::MergeTrackedWindow(u32 addr, size_t bytes)
{
	u32 start = addr & ~0x3u;
	size_t end = ((addr + bytes + 3u) & ~0x3u);
	size_t new_size = end - start;

	for (auto& w : mem_windows_)
	{
		const u32 w_end = w.addr + static_cast<u32>(w.bytes.size());
		if (start + new_size < w.addr || start > w_end)
			continue;
		const u32 merged_start = (start < w.addr) ? start : w.addr;
		const u32 merged_end = (start + new_size > w_end) ? static_cast<u32>(start + new_size) : w_end;
		w.addr = merged_start;
		w.bytes.resize(merged_end - merged_start);
		return;
	}

	mem_windows_.push_back(MemWindow{start, std::vector<u8>(new_size)});
}

void EeRecTestHarness::LoadProgramImpl(std::initializer_list<u32> instructions, bool append_term)
{
	program_words_.assign(instructions.begin(), instructions.end());
	if (append_term)
	{
		program_words_.push_back(mips::JR(mips::reg::ra));
		program_words_.push_back(mips::NOP);
	}
	for (size_t i = 0; i < program_words_.size(); ++i)
		memWrite32(kProgramPc + static_cast<u32>(i * 4), program_words_[i]);
}

void EeRecTestHarness::LoadProgram(std::initializer_list<u32> instructions)
{
	LoadProgramImpl(instructions, /*append_term=*/true);
}

void EeRecTestHarness::LoadProgramNoTerm(std::initializer_list<u32> instructions)
{
	LoadProgramImpl(instructions, /*append_term=*/false);
}

void EeRecTestHarness::SeedEntryState()
{
	cpuRegs.GPR.n.ra.UD[0] = static_cast<s64>(static_cast<s32>(kParkingPc));
	cpuRegs.pc = kProgramPc;
	cpuRegs.branch = 0;

	// An EE branch triggers cpuEventTest → psxCpu->ExecuteBlock(EEsCycle),
	// which dispatches IOP code from psxRegs.pc. If that PC is 0 the IOP
	// walks zero-bytes-as-NOPs forever. Park the IOP at its parking lot.
	psxRegs.pc = RecompilerTestEnvironment::kParkingPc;
}

void EeRecTestHarness::StepInterpUntilParkedOrTimeout()
{
	for (u32 i = 0; i < kMaxInstructions; ++i)
	{
		if (cpuRegs.pc == kParkingPc)
			return;
		intCpu.Step();
	}
	ADD_FAILURE() << "EeRecTestHarness exhausted instruction budget ("
	              << kMaxInstructions << "); PC=0x" << std::hex << cpuRegs.pc
	              << " never reached parking lot 0x" << kParkingPc;
}

void EeRecTestHarness::Run(RunMode mode)
{
	ASSERT_FALSE(program_words_.empty())
		<< "LoadProgram() must be called before Run()";

	SeedEntryState();
	pre_snapshot_ = EeSnapshot::Capture(mem_windows_);

	// Drop any cached block from a previous test that occupies kProgramPc
	// or the parking lot. memWrite32 on harness program load *should* trip
	// vtlb's Cpu->Clear path, but a belt-and-suspenders explicit Clear keeps
	// the harness decoupled from that wiring (matches JitTestHarness::Run for IOP).
	// Required once opcode handlers bake immediates into the emitted block,
	// since a body that re-fetches from memory each dispatch is inadvertently
	// SMC-immune.
	//
	// PreserveCache mode skips this — used by SMC tests that want a second
	// dispatch against blocks left over from a previous Run(), with mid-
	// block invalidation injected via SimulateFastmemFault() between calls.
	if (mode == RunMode::FreshCache && recCpu.Clear)
	{
		recCpu.Clear(kProgramPc, static_cast<u32>(program_words_.size()));
		recCpu.Clear(kParkingPc, 2);
	}

	if (capture_vu0_)
		vu0_pre_snapshot_ = VuSnapshot::Capture(0, {});
	if (capture_vu1_)
		vu1_pre_snapshot_ = VuSnapshot::Capture(1, {});
	// A JIT block may legitimately leave host FPCR set to EmuConfig FPUFPCR
	// (e.g. native DIV.S swaps to the div rounding mode and restores FPUFPCR,
	// not the host default). The real EE thread holds FPUFPCR for its whole
	// lifetime, but the harness never establishes that invariant, so contain
	// the mutation to the JIT block: snapshot host FPCR and restore it before
	// the interp oracle runs and before the next test.
	const FPControlRegister saved_fpcr = FPControlRegister::GetCurrent();
	recEeExecuteBlock(kCycleBudget, kParkingPc);
	FPControlRegister::SetCurrent(saved_fpcr);
	if (capture_vu1_)
		FireVif1Pass(/*jit=*/true);
	jit_snapshot_ = EeSnapshot::Capture(mem_windows_);
	if (capture_vu0_)
		vu0_jit_snapshot_ = VuSnapshot::Capture(0, {});
	if (capture_vu1_)
		vu1_jit_snapshot_ = VuSnapshot::Capture(1, {});

	// Restore pre-state and run interp from the same initial conditions.
	pre_snapshot_.Restore();
	if (capture_vu0_)
		vu0_pre_snapshot_.Restore();
	if (capture_vu1_)
		vu1_pre_snapshot_.Restore();
	SeedEntryState();
	StepInterpUntilParkedOrTimeout();
	if (capture_vu1_)
		FireVif1Pass(/*jit=*/false);
	interp_snapshot_ = EeSnapshot::Capture(mem_windows_);
	if (capture_vu0_)
		vu0_interp_snapshot_ = VuSnapshot::Capture(0, {});
	if (capture_vu1_)
		vu1_interp_snapshot_ = VuSnapshot::Capture(1, {});
	has_run_ = true;

	const auto diffs = DiffEe(jit_snapshot_, interp_snapshot_);
	if (!diffs.empty())
	{
		std::ostringstream ss;
		ss << "EE JIT vs INTERP divergence (" << diffs.size() << "):\n";
		for (const auto& d : diffs)
			ss << "  " << d << "\n";
		ss << "Pre-state:\n";
		PrintEe(ss, pre_snapshot_);
		ss << "JIT post-state:\n";
		PrintEe(ss, jit_snapshot_);
		ss << "INTERP post-state:\n";
		PrintEe(ss, interp_snapshot_);
		ADD_FAILURE() << ss.str();
	}

	if (capture_vu0_)
	{
		const auto vudiffs = DiffVu(vu0_jit_snapshot_, vu0_interp_snapshot_,
			VuDiffMode::PipelinePermissive, vu0_ignored_vi_);
		if (!vudiffs.empty())
		{
			std::ostringstream ss;
			ss << "VU0 JIT vs INTERP divergence (" << vudiffs.size() << "):\n";
			for (const auto& d : vudiffs)
				ss << "  " << d << "\n";
			ss << "VU0 JIT post-state:\n";
			PrintVu(ss, vu0_jit_snapshot_);
			ss << "VU0 INTERP post-state:\n";
			PrintVu(ss, vu0_interp_snapshot_);
			ADD_FAILURE() << ss.str();
		}
	}

	if (capture_vu1_)
	{
		const auto vudiffs = DiffVu(vu1_jit_snapshot_, vu1_interp_snapshot_,
			VuDiffMode::PipelinePermissive);
		if (!vudiffs.empty())
		{
			std::ostringstream ss;
			ss << "VU1 JIT vs INTERP divergence (" << vudiffs.size() << "):\n";
			for (const auto& d : vudiffs)
				ss << "  " << d << "\n";
			ss << "VU1 JIT post-state:\n";
			PrintVu(ss, vu1_jit_snapshot_);
			ss << "VU1 INTERP post-state:\n";
			PrintVu(ss, vu1_interp_snapshot_);
			ADD_FAILURE() << ss.str();
		}
	}
}

void EeRecTestHarness::RunJitNoDiff(RunMode mode)
{
	ASSERT_FALSE(program_words_.empty())
		<< "LoadProgram() must be called before RunJitNoDiff()";

	SeedEntryState();
	pre_snapshot_ = EeSnapshot::Capture(mem_windows_);

	if (mode == RunMode::FreshCache && recCpu.Clear)
	{
		recCpu.Clear(kProgramPc, static_cast<u32>(program_words_.size()));
		recCpu.Clear(kParkingPc, 2);
	}

	// Contain any FPCR mutation the JIT block makes (see note in Run()).
	const FPControlRegister saved_fpcr = FPControlRegister::GetCurrent();
	recEeExecuteBlock(kCycleBudget, kParkingPc);
	FPControlRegister::SetCurrent(saved_fpcr);
	jit_snapshot_ = EeSnapshot::Capture(mem_windows_);
	// Mirror the JIT post-state into the interp snapshot so accessors that read
	// either side return the JIT value (there is no interp double-mode oracle).
	interp_snapshot_ = jit_snapshot_;
	has_run_ = true;
}

void EeRecTestHarness::RunInterpOnly()
{
	ASSERT_FALSE(program_words_.empty())
		<< "LoadProgram() must be called before RunInterpOnly()";

	SeedEntryState();
	pre_snapshot_ = EeSnapshot::Capture(mem_windows_);
	StepInterpUntilParkedOrTimeout();
	interp_snapshot_ = EeSnapshot::Capture(mem_windows_);
	jit_snapshot_ = interp_snapshot_;
	has_run_ = true;
}

u64 EeRecTestHarness::GetGpr64Interp(u32 r) const { return interp_snapshot_.regs.GPR.r[r].UD[0]; }
u64 EeRecTestHarness::GetGpr64Jit   (u32 r) const { return jit_snapshot_.regs.GPR.r[r].UD[0]; }
u64 EeRecTestHarness::GetHi64Interp() const       { return interp_snapshot_.regs.HI.UD[0]; }
u64 EeRecTestHarness::GetLo64Interp() const       { return interp_snapshot_.regs.LO.UD[0]; }
u32 EeRecTestHarness::GetFprBitsInterp(u32 r) const { return interp_snapshot_.fprs.fpr[r].UL; }
u32 EeRecTestHarness::GetFprBitsJit   (u32 r) const { return jit_snapshot_.fprs.fpr[r].UL; }
u32 EeRecTestHarness::GetAccBitsInterp() const      { return interp_snapshot_.fprs.ACC.UL; }
u32 EeRecTestHarness::GetAccBitsJit   () const      { return jit_snapshot_.fprs.ACC.UL; }
u32 EeRecTestHarness::GetCp0Interp(u32 r) const     { return interp_snapshot_.regs.CP0.r[r]; }
u32 EeRecTestHarness::GetCp0Jit   (u32 r) const     { return jit_snapshot_.regs.CP0.r[r]; }

void EeRecTestHarness::ExpectGpr64(u32 reg_idx, u64 expected) const
{
	EXPECT_EQ(interp_snapshot_.regs.GPR.r[reg_idx].UD[0], expected)
		<< "r" << reg_idx << ".lo (interp)";
	EXPECT_EQ(jit_snapshot_.regs.GPR.r[reg_idx].UD[0], expected)
		<< "r" << reg_idx << ".lo (jit)";
}

void EeRecTestHarness::ExpectGpr128(u32 reg_idx, u64 lo, u64 hi) const
{
	EXPECT_EQ(interp_snapshot_.regs.GPR.r[reg_idx].UD[0], lo) << "r" << reg_idx << ".lo (interp)";
	EXPECT_EQ(interp_snapshot_.regs.GPR.r[reg_idx].UD[1], hi) << "r" << reg_idx << ".hi (interp)";
	EXPECT_EQ(jit_snapshot_.regs.GPR.r[reg_idx].UD[0], lo)    << "r" << reg_idx << ".lo (jit)";
	EXPECT_EQ(jit_snapshot_.regs.GPR.r[reg_idx].UD[1], hi)    << "r" << reg_idx << ".hi (jit)";
}

void EeRecTestHarness::ExpectFpr(u32 reg_idx, u32 bits) const
{
	EXPECT_EQ(interp_snapshot_.fprs.fpr[reg_idx].UL, bits) << "fpr" << reg_idx << " (interp)";
	EXPECT_EQ(jit_snapshot_.fprs.fpr[reg_idx].UL, bits)    << "fpr" << reg_idx << " (jit)";
}

void EeRecTestHarness::ExpectAcc(u32 bits) const
{
	EXPECT_EQ(interp_snapshot_.fprs.ACC.UL, bits) << "ACC (interp)";
	EXPECT_EQ(jit_snapshot_.fprs.ACC.UL, bits)    << "ACC (jit)";
}

// ---- VU0 cross-tree handoff helpers ----

void EeRecTestHarness::EnableVu0Capture()
{
	capture_vu0_ = true;

	// Give every VU0-capturing test a clean architectural baseline.
	//
	// vuRegs[0] is a global: whatever flags/ACC/VF a previous test left behind
	// survive into this test's pre-snapshot, which Run() then feeds to BOTH the
	// JIT and interp passes. Most state leaks harmlessly (it lands equally on
	// both sides). The MAC/STATUS flags do not: masked FMACs (e.g. OPMULA/OPMSUB
	// only touch xyz) PRESERVE the untouched lane's MAC bit in the interp
	// (VUflags.cpp VU_MAC_UPDATE keys off the per-op shift; VU_STAT_UPDATE then
	// folds macflag&0x000F into STATUS), whereas the JIT recomputes the full
	// flag word. With a stale non-zero w-lane MAC bit incoming, the two diverge
	// — surfacing as order-dependent failures in EeVu0Cop2Macro.Vopm* under
	// --gtest_shuffle. Zero the register/flag file here so test order can't leak.
	//
	// Only architectural register + flag state is cleared. Infra the VU0 engine
	// needs (Mem/Micro pointers, idx, cycle) and the control registers at
	// VI[24..31] (TPC/CMSAR0/FBRST/VPU_STAT/CMSAR1) are left intact; tests that
	// care seed them explicitly after this call.
	VURegs& vu = vuRegs[0];
	for (int i = 1; i < 32; i++)
		vu.VF[i].UD[0] = vu.VF[i].UD[1] = 0;
	for (int i = 1; i <= REG_P; i++) // VI[1..15] integer + VI[16..23] flags/R/I/Q/P
		vu.VI[i].UL = 0;
	vu.ACC.UD[0] = vu.ACC.UD[1] = 0;
	vu.q.UL = 0;
	vu.p.UL = 0;
	vu.macflag = 0;
	vu.statusflag = 0;
	vu.clipflag = 0;
	for (int i = 0; i < 4; i++)
	{
		vu.micro_macflags[i] = 0;
		vu.micro_statusflags[i] = 0;
		vu.micro_clipflags[i] = 0;
	}

	// Mirror the VuTestHarness's first-touch invariant: VF[0] must read as
	// (0,0,0,1.0). _vu0Exec asserts on drift via DbgCon.Error. The interp
	// run will trigger that assertion on whatever stale state the previous
	// test left behind unless this is reset here.
	vu.VF[0].f.x = 0.0f;
	vu.VF[0].f.y = 0.0f;
	vu.VF[0].f.z = 0.0f;
	vu.VF[0].f.w = 1.0f;
	vu.VI[0].UL = 0;
}

void EeRecTestHarness::SeedVu0Vf(u32 reg_idx, float x, float y, float z, float w)
{
	if (reg_idx == 0)
		return;
	auto& vf = vuRegs[0].VF[reg_idx];
	vf.f.x = x; vf.f.y = y; vf.f.z = z; vf.f.w = w;
}

void EeRecTestHarness::SeedVu0VfBits(u32 reg_idx, u32 x, u32 y, u32 z, u32 w)
{
	if (reg_idx == 0)
		return;
	auto& vf = vuRegs[0].VF[reg_idx];
	vf.i.x = x; vf.i.y = y; vf.i.z = z; vf.i.w = w;
}

void EeRecTestHarness::SeedVu0Acc(float x, float y, float z, float w)
{
	auto& acc = vuRegs[0].ACC;
	acc.f.x = x; acc.f.y = y; acc.f.z = z; acc.f.w = w;
}

void EeRecTestHarness::SeedVu0AccBits(u32 x, u32 y, u32 z, u32 w)
{
	auto& acc = vuRegs[0].ACC;
	acc.i.x = x; acc.i.y = y; acc.i.z = z; acc.i.w = w;
}

void EeRecTestHarness::SeedVu0Vi(u32 reg_idx, u32 value)
{
	if (reg_idx == 0)
		return;
	// REG_FBRST / REG_VPU_STAT / REG_TPC etc. hold full 32-bit values; the
	// rest are 16-bit. Since callers know the register, just trust them.
	vuRegs[0].VI[reg_idx].UL = value;
}

void EeRecTestHarness::SeedVu0Microprogram(u32 byte_offset, std::initializer_list<vu::VuOp> pairs)
{
	auto& vu = vuRegs[0];
	const u32 mask = VU0_PROGMASK;
	u32 base = byte_offset & mask;
	for (const auto& p : pairs)
	{
		std::memcpy(vu.Micro + ((base + 0) & mask), &p.lower, 4);
		std::memcpy(vu.Micro + ((base + 4) & mask), &p.upper, 4);
		base += 8;
	}
}

namespace {
inline u32 Vu0LaneIdx(char lane)
{
	switch (lane) { case 'x': return 0; case 'y': return 1; case 'z': return 2; default: return 3; }
}
inline u32 Vu0ViMask(u32 reg_idx)
{
	switch (reg_idx)
	{
	case REG_R: case REG_I: case REG_Q: case REG_P:
	case REG_STATUS_FLAG: case REG_MAC_FLAG: case REG_CLIP_FLAG:
	case REG_TPC: case REG_FBRST: case REG_VPU_STAT:
		return 0xFFFFFFFFu;
	default:
		return 0x0000FFFFu;
	}
}
} // namespace

u32 EeRecTestHarness::GetVu0VfBitsJit(u32 reg_idx, char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	return vu0_jit_snapshot_.regs.VF[reg_idx].UL[li];
}
u32 EeRecTestHarness::GetVu0VfBitsInterp(u32 reg_idx, char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	return vu0_interp_snapshot_.regs.VF[reg_idx].UL[li];
}
float EeRecTestHarness::GetVu0VfJit(u32 reg_idx, char lane) const
{
	const u32 b = GetVu0VfBitsJit(reg_idx, lane);
	float f; std::memcpy(&f, &b, sizeof(f)); return f;
}
float EeRecTestHarness::GetVu0VfInterp(u32 reg_idx, char lane) const
{
	const u32 b = GetVu0VfBitsInterp(reg_idx, lane);
	float f; std::memcpy(&f, &b, sizeof(f)); return f;
}
u32 EeRecTestHarness::GetVu0AccBitsJit(char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	return vu0_jit_snapshot_.regs.ACC.UL[li];
}
u32 EeRecTestHarness::GetVu0AccBitsInterp(char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	return vu0_interp_snapshot_.regs.ACC.UL[li];
}
u32 EeRecTestHarness::GetVu0ViJit(u32 reg_idx) const
{
	return vu0_jit_snapshot_.regs.VI[reg_idx].UL & Vu0ViMask(reg_idx);
}
u32 EeRecTestHarness::GetVu0ViInterp(u32 reg_idx) const
{
	return vu0_interp_snapshot_.regs.VI[reg_idx].UL & Vu0ViMask(reg_idx);
}

// ---- EE↔VU1 VIF dispatch helpers ----

void EeRecTestHarness::EnableVu1VifCapture()
{
	capture_vu1_ = true;

	// Cross-test isolation. A previous fixture (Vu1Xgkick, EeVu0*) may have
	// left VU1 dirty: VPU_STAT.bit8 still set, ebit > 0, xgkickenable on,
	// VU1.Mem holding GIF tags, etc. Without this, the JIT/interp passes
	// here either hang in Execute (running bit never clears) or trigger
	// MTGS::WaitGS during xgkick drain (MTGS thread isn't open in tests).
	VuSnapshot::ZeroGlobals(1);
	std::memset(vuRegs[1].Mem, 0, VU1_MEMSIZE);
	std::memset(vuRegs[1].Micro, 0, VU1_PROGSIZE);
	VU0.VI[REG_VPU_STAT].UL &= ~0xFF00u; // clear VU1 running/T/D bits

	// Install the path 1 sink so any incidental XGKICK during VU1 termination
	// (the interp's vu1Exec drains xgkick on E-bit, even if the program never
	// kicked) hits the test-only sink instead of MTGS::WaitGS.
	gif_test_hooks::g_path1_sink = &vu1_path1_sink_;

	// vu1ExecMicro with INSTANT_VU1=true runs CpuVU1->Execute(vu1RunCycles)
	// synchronously to completion (E-bit termination clears VPU_STAT bit 8).
	// The test environment defaults this to false (RecompilerTestEnvironment.
	// cpp:160) so cycle-driven tests stay deterministic; flip it on for the
	// VIF dispatch tests where the program must fully execute inside
	// vifExecQueue. The dtor restores the env default.
	EmuConfig.Speedhacks.vu1Instant = true;
}

void EeRecTestHarness::SeedVu1Microprogram(u32 byte_offset, std::initializer_list<vu::VuOp> pairs)
{
	auto& vu = vuRegs[1];
	const u32 mask = VU1_PROGMASK;
	u32 base = byte_offset & mask;
	for (const auto& p : pairs)
	{
		std::memcpy(vu.Micro + ((base + 0) & mask), &p.lower, 4);
		std::memcpy(vu.Micro + ((base + 4) & mask), &p.upper, 4);
		base += 8;
	}
}

void EeRecTestHarness::SeedVu1Vf(u32 reg_idx, float x, float y, float z, float w)
{
	if (reg_idx == 0)
		return;
	auto& vf = vuRegs[1].VF[reg_idx];
	vf.f.x = x; vf.f.y = y; vf.f.z = z; vf.f.w = w;
}

void EeRecTestHarness::SeedVu1VfBits(u32 reg_idx, u32 x, u32 y, u32 z, u32 w)
{
	if (reg_idx == 0)
		return;
	auto& vf = vuRegs[1].VF[reg_idx];
	vf.i.x = x; vf.i.y = y; vf.i.z = z; vf.i.w = w;
}

void EeRecTestHarness::SeedVu1Vi(u32 reg_idx, u32 value)
{
	if (reg_idx == 0)
		return;
	vuRegs[1].VI[reg_idx].UL = value;
}

void EeRecTestHarness::QueueVif1Mscal(u16 microprogram_addr)
{
	vif1_queue_.push_back({PendingVifCmd::Mscal, 0x14000000u | (microprogram_addr & 0xFFFFu)});
}

void EeRecTestHarness::QueueVif1Mscalf(u16 microprogram_addr)
{
	vif1_queue_.push_back({PendingVifCmd::Mscalf, 0x15000000u | (microprogram_addr & 0xFFFFu)});
}

void EeRecTestHarness::QueueVif1Mscnt()
{
	vif1_queue_.push_back({PendingVifCmd::Mscnt, 0x17000000u});
}

void EeRecTestHarness::SetGifPath1Busy(bool busy) { vu1_state_path1_busy_ = busy; }
void EeRecTestHarness::SetVif1WaitForVu(bool wait) { vu1_state_waitforvu_ = wait; }
void EeRecTestHarness::SetVif1Doublebuffer(u16 base_qw, u16 ofst_qw)
{
	vu1_state_dbf_set_ = true;
	vu1_state_base_qw_ = base_qw & 0x3ffu;
	vu1_state_ofst_qw_ = ofst_qw & 0x3ffu;
}

void EeRecTestHarness::FireVif1Pass(bool jit)
{
	// Reset vif1 + vif1Regs to a known baseline. vif1Reset() also calls
	// resetNewVif(1) which clears the dynamic VIF unpack JIT cache — fine.
	vif1Reset();

	// Apply per-test state.
	gif_test_hooks::g_force_path1_busy = vu1_state_path1_busy_;
	if (vu1_state_waitforvu_)
		vif1.waitforvu = true;
	if (vu1_state_dbf_set_)
	{
		vif1Regs.base = vu1_state_base_qw_;
		vif1Regs.ofst = vu1_state_ofst_qw_;
	}

	// Swap the active VU1 engine for this pass. Restored on exit so the
	// EE harness's default (CpuVU1 = CpuIntVU1 in RecompilerTestEnvironment)
	// is preserved for any subsequent test run in this binary.
	BaseVUmicroCPU* const saved_cpu_vu1 = CpuVU1;
	CpuVU1 = jit ? static_cast<BaseVUmicroCPU*>(&CpuMicroVU1)
	             : static_cast<BaseVUmicroCPU*>(&CpuIntVU1);

	// Reset the JIT engine before each JIT pass so a previous test's
	// compiled blocks can't be reused against this test's seeded state.
	if (jit)
		CpuMicroVU1.Reset();

	for (const auto& cmd : vif1_queue_)
	{
		// Mimic vifCode_MSCAL/MSCALF/MSCNT pass1 — write the command and
		// invoke the dispatch helpers directly. No vifFlush() because the
		// FIFO is empty in the harness (no DMA tag chain).
		vif1Regs.code = cmd.code;

		switch (cmd.kind)
		{
		case PendingVifCmd::Mscal:
		{
			if (vif1.waitforvu)
				break; // Production sets DMASTALL and returns.
			const u32 addr = static_cast<u16>(vif1Regs.code);
			// vuExecMicro body — sets up itop/top/tops/DBF and queues the
			// program. With INSTANT_VU1=true, vifExecQueue→vu1ExecMicro→
			// CpuVU1->Execute runs to completion synchronously.
			vif1Regs.itop = vif1Regs.itops;
			vif1Regs.top = vif1Regs.tops & 0x3ffu;
			if (vif1Regs.stat.DBF)
			{
				vif1Regs.tops = vif1Regs.base;
				vif1Regs.stat.DBF = false;
			}
			else
			{
				vif1Regs.tops = vif1Regs.base + vif1Regs.ofst;
				vif1Regs.stat.DBF = true;
			}
			vif1.queued_program = true;
			vif1.queued_pc = addr & 0x7ffu;
			vif1.unpackcalls = 0;
			vif1.queued_gif_wait = false;
			vifExecQueue(1);
			break;
		}
		case PendingVifCmd::Mscalf:
		{
			vif1Regs.stat.VGW = false;
			if (gif_test_hooks::g_force_path1_busy)
			{
				vif1Regs.stat.VGW = true;
				break; // Production stalls; no microprogram dispatch.
			}
			if (vif1.waitforvu)
				break;
			const u32 addr = static_cast<u16>(vif1Regs.code);
			vif1Regs.itop = vif1Regs.itops;
			vif1Regs.top = vif1Regs.tops & 0x3ffu;
			if (vif1Regs.stat.DBF)
			{
				vif1Regs.tops = vif1Regs.base;
				vif1Regs.stat.DBF = false;
			}
			else
			{
				vif1Regs.tops = vif1Regs.base + vif1Regs.ofst;
				vif1Regs.stat.DBF = true;
			}
			vif1.queued_program = true;
			vif1.queued_pc = addr & 0x7ffu;
			vif1.unpackcalls = 0;
			vif1.queued_gif_wait = true;
			vifExecQueue(1);
			break;
		}
		case PendingVifCmd::Mscnt:
		{
			if (vif1.waitforvu)
				break;
			vif1Regs.itop = vif1Regs.itops;
			vif1Regs.top = vif1Regs.tops & 0x3ffu;
			if (vif1Regs.stat.DBF)
			{
				vif1Regs.tops = vif1Regs.base;
				vif1Regs.stat.DBF = false;
			}
			else
			{
				vif1Regs.tops = vif1Regs.base + vif1Regs.ofst;
				vif1Regs.stat.DBF = true;
			}
			vif1.queued_program = true;
			vif1.queued_pc = static_cast<u32>(-1); // MSCNT keeps last PC
			vif1.unpackcalls = 0;
			vif1.queued_gif_wait = false;
			vifExecQueue(1);
			break;
		}
		}
	}

	// Snapshot VIF1 dispatch-side post-state for this pass before any
	// later pass overwrites the globals. Architectural fields only —
	// vif1.fifo etc. are dispatcher bookkeeping, not captured here.
	Vif1PostState& post = jit ? vif1_jit_post_ : vif1_interp_post_;
	post.stat      = vif1Regs.stat._u32;
	post.tops      = vif1Regs.tops;
	post.top       = vif1Regs.top;

	CpuVU1 = saved_cpu_vu1;
	gif_test_hooks::g_force_path1_busy = false;
}

float EeRecTestHarness::GetVu1VfJit(u32 reg_idx, char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	const u32 b = vu1_jit_snapshot_.regs.VF[reg_idx].UL[li];
	float f; std::memcpy(&f, &b, sizeof(f)); return f;
}
float EeRecTestHarness::GetVu1VfInterp(u32 reg_idx, char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	const u32 b = vu1_interp_snapshot_.regs.VF[reg_idx].UL[li];
	float f; std::memcpy(&f, &b, sizeof(f)); return f;
}
u32 EeRecTestHarness::GetVu1VfBitsJit(u32 reg_idx, char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	return vu1_jit_snapshot_.regs.VF[reg_idx].UL[li];
}
u32 EeRecTestHarness::GetVu1VfBitsInterp(u32 reg_idx, char lane) const
{
	const u32 li = Vu0LaneIdx(lane);
	return vu1_interp_snapshot_.regs.VF[reg_idx].UL[li];
}
u32 EeRecTestHarness::GetVu1ViJit(u32 reg_idx) const
{
	return vu1_jit_snapshot_.regs.VI[reg_idx].UL & Vu0ViMask(reg_idx);
}
u32 EeRecTestHarness::GetVu1ViInterp(u32 reg_idx) const
{
	return vu1_interp_snapshot_.regs.VI[reg_idx].UL & Vu0ViMask(reg_idx);
}

bool EeRecTestHarness::HasVu1TerminatedJit() const
{
	return (vu1_jit_snapshot_.regs.VI[REG_VPU_STAT].UL & 0x100u) == 0;
}
bool EeRecTestHarness::HasVu1TerminatedInterp() const
{
	return (vu1_interp_snapshot_.regs.VI[REG_VPU_STAT].UL & 0x100u) == 0;
}

u32 EeRecTestHarness::GetVif1StatJit()   const { return vif1_jit_post_.stat; }
u32 EeRecTestHarness::GetVif1StatInterp() const { return vif1_interp_post_.stat; }
u32 EeRecTestHarness::GetVif1TopsJit()   const { return vif1_jit_post_.tops; }
u32 EeRecTestHarness::GetVif1TopsInterp() const { return vif1_interp_post_.tops; }
u32 EeRecTestHarness::GetVif1TopJit()    const { return vif1_jit_post_.top; }
u32 EeRecTestHarness::GetVif1TopInterp()  const { return vif1_interp_post_.top; }

void EeRecTestHarness::ExpectVu1Vf(u32 reg_idx, float x, float y, float z, float w) const
{
	auto check = [&](float ex, char lane, const char* side, float v) {
		EXPECT_EQ(v, ex) << "vf" << reg_idx << "." << lane << " (" << side << ")";
	};
	check(x, 'x', "jit",    GetVu1VfJit   (reg_idx, 'x'));
	check(y, 'y', "jit",    GetVu1VfJit   (reg_idx, 'y'));
	check(z, 'z', "jit",    GetVu1VfJit   (reg_idx, 'z'));
	check(w, 'w', "jit",    GetVu1VfJit   (reg_idx, 'w'));
	check(x, 'x', "interp", GetVu1VfInterp(reg_idx, 'x'));
	check(y, 'y', "interp", GetVu1VfInterp(reg_idx, 'y'));
	check(z, 'z', "interp", GetVu1VfInterp(reg_idx, 'z'));
	check(w, 'w', "interp", GetVu1VfInterp(reg_idx, 'w'));
}

void EeRecTestHarness::ExpectVu1Vi(u32 reg_idx, u32 expected) const
{
	EXPECT_EQ(GetVu1ViJit   (reg_idx), expected) << "vi" << reg_idx << " (jit)";
	EXPECT_EQ(GetVu1ViInterp(reg_idx), expected) << "vi" << reg_idx << " (interp)";
}

} // namespace recompiler_tests

extern bool recEeIsBlockLinked(u32 src_pc, u32 dst_pc);

namespace recompiler_tests {

void EeRecTestHarness::ExpectBlockLinked(u32 src_pc, u32 dst_pc) const
{
	// The live LinkArm64 consumer is the page-boundary / pre-compiled-neighbor
	// fall-through tail in recRecompile. Branch-opcode handlers (J/JAL/BEQ/BNE/...)
	// add more link sites via their SetBranchImm callers.
	EXPECT_TRUE(recEeIsBlockLinked(src_pc, dst_pc))
		<< "no LinkArm64 patch site from block containing src_pc=" << std::hex << src_pc
		<< " to dst_pc=" << dst_pc;
}

} // namespace recompiler_tests
