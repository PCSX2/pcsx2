// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "VuTestHarness.h"

#include "Config.h"
#include "Gif_Unit.h"
#include "VU.h"
#include "VUmicro.h"
#include "common/FPControl.h"

#include <cstring>
#include <sstream>

namespace recompiler_tests {

namespace {

VURegs& Regs(int idx) { return vuRegs[idx]; }

u32 MemMask(int idx) { return idx == 0 ? VU0_MEMMASK : VU1_MEMMASK; }
u32 MemSize(int idx) { return idx == 0 ? VU0_MEMSIZE : VU1_MEMSIZE; }
u32 ProgMask(int idx) { return idx == 0 ? VU0_PROGMASK : VU1_PROGMASK; }
u32 ProgSize(int idx) { return idx == 0 ? VU0_PROGSIZE : VU1_PROGSIZE; }

BaseVUmicroCPU* InterpCpu(int idx)
{
	return idx == 0 ? static_cast<BaseVUmicroCPU*>(&CpuIntVU0)
	                : static_cast<BaseVUmicroCPU*>(&CpuIntVU1);
}

BaseVUmicroCPU* JitCpu(int idx)
{
	return idx == 0 ? static_cast<BaseVUmicroCPU*>(&CpuMicroVU0)
	                : static_cast<BaseVUmicroCPU*>(&CpuMicroVU1);
}

// VPU_STAT bit indicating "VU<idx> running". The interpreter's Execute loop
// breaks once this clears, which the E-bit cleanup does automatically.
// Both VU0 and VU1's running-bits live in VU0.VI[REG_VPU_STAT] — the VU1
// interpreter's Execute loop explicitly checks
// `VU0.VI[REG_VPU_STAT].UL & 0x100`, not VU1's own VI. Same wiring on the
// cleanup side.
u32 RunningBit(int idx) { return idx == 0 ? 0x1u : 0x100u; }

// Special VIs (REG_R/I/Q/P/STATUS/MAC/CLIP/TPC/FBRST/VPU_STAT) hold full
// 32-bit values; the others use only the low 16 (per VURegs.h). Mask only
// the 16-bit ones so callers reading/writing REG_Q etc. see the full bits,
// not just the low half.
inline u32 ViMaskFor(u32 reg_idx)
{
	switch (reg_idx)
	{
		case REG_R: case REG_I: case REG_Q: case REG_P:
		case REG_STATUS_FLAG: case REG_MAC_FLAG: case REG_CLIP_FLAG:
		case REG_TPC: case REG_FBRST: case REG_VPU_STAT:
			return 0xFFFFFFFFu;
		default: return 0x0000FFFFu;
	}
}

} // namespace

VuTestHarness::VuTestHarness(int vu_index)
	: vu_index_(vu_index)
{
	EXPECT_TRUE(RecompilerTestEnvironment::IsReady())
		<< "RecompilerTestEnvironment was not set up — VU harness cannot run.";
	EXPECT_TRUE(vu_index == 0 || vu_index == 1)
		<< "VuTestHarness: vu_index must be 0 or 1, got " << vu_index;
	VuSnapshot::ZeroGlobals(vu_index_);
	// Cross-test isolation: a previous fixture may have left VU.Mem dirty
	// at addresses the new test doesn't touch via WriteMemU32 but does read
	// (via VU loads). VuSnapshot::ZeroGlobals only resets the register file.
	std::memset(Regs(vu_index_).Mem, 0, MemSize(vu_index_));
	gif_test_hooks::g_path1_sink = &path1_packets_;
}

VuTestHarness::~VuTestHarness()
{
	gif_test_hooks::g_path1_sink = nullptr;
}

void VuTestHarness::SetVf(u32 reg_idx, float x, float y, float z, float w)
{
	if (reg_idx == 0) // VF[0] is hardwired (0,0,0,1)
		return;
	auto& vf = Regs(vu_index_).VF[reg_idx];
	vf.f.x = x; vf.f.y = y; vf.f.z = z; vf.f.w = w;
}

void VuTestHarness::SetVfBits(u32 reg_idx, u32 x, u32 y, u32 z, u32 w)
{
	if (reg_idx == 0)
		return;
	auto& vf = Regs(vu_index_).VF[reg_idx];
	vf.i.x = x; vf.i.y = y; vf.i.z = z; vf.i.w = w;
}

void VuTestHarness::SetVi(u32 reg_idx, u32 value)
{
	if (reg_idx == 0) // VI[0] is hardwired zero
		return;
	// The special VIs (REG_R/I/Q/P/flags/TPC/FBRST/VPU_STAT) are full-width;
	// the rest are 16-bit. Mask to match, mirroring GetViInterp/GetViJit.
	Regs(vu_index_).VI[reg_idx].UL = value & ViMaskFor(reg_idx);
}

void VuTestHarness::SetQ(u32 bits)
{
	Regs(vu_index_).q.UL = bits;
	Regs(vu_index_).VI[REG_Q].UL = bits;
}

void VuTestHarness::SetP(u32 bits)
{
	Regs(vu_index_).p.UL = bits;
	Regs(vu_index_).VI[REG_P].UL = bits;
}

void VuTestHarness::WriteMemU32(u32 addr, u32 value)
{
	const u32 a = addr & MemMask(vu_index_);
	std::memcpy(Regs(vu_index_).Mem + a, &value, sizeof(value));
	MergeTrackedWindow(a & ~0x3u, 4);
}

void VuTestHarness::WriteMemU128(u32 addr, u32 x, u32 y, u32 z, u32 w)
{
	const u32 a = addr & MemMask(vu_index_);
	const u32 quad[4] = {x, y, z, w};
	std::memcpy(Regs(vu_index_).Mem + a, quad, sizeof(quad));
	MergeTrackedWindow(a & ~0xFu, 16);
}

u32 VuTestHarness::ReadMemU32(u32 addr) const
{
	u32 value = 0;
	const u32 a = addr & MemMask(vu_index_);
	std::memcpy(&value, Regs(vu_index_).Mem + a, sizeof(value));
	return value;
}

void VuTestHarness::TrackMemWindow(u32 addr, size_t bytes)
{
	MergeTrackedWindow(addr, bytes);
}

void VuTestHarness::MergeTrackedWindow(u32 addr, size_t bytes)
{
	const u32 start = addr & ~0x3u;
	const size_t end = ((addr + bytes + 3u) & ~0x3u);
	const size_t new_size = end - start;

	for (auto& w : mem_windows_)
	{
		const u32 w_end = w.addr + static_cast<u32>(w.bytes.size());
		if (start + new_size < w.addr || start > w_end)
			continue;
		const u32 merged_start = (start < w.addr) ? start : w.addr;
		const u32 merged_end = (start + new_size > w_end)
			? static_cast<u32>(start + new_size) : w_end;
		w.addr = merged_start;
		w.bytes.resize(merged_end - merged_start);
		return;
	}
	mem_windows_.push_back(VuMemWindow{start, std::vector<u8>(new_size)});
}

void VuTestHarness::LoadProgram(std::initializer_list<vu::VuOp> pairs)
{
	LoadProgram(std::vector<vu::VuOp>(pairs.begin(), pairs.end()));
}

void VuTestHarness::LoadProgram(std::vector<vu::VuOp> pairs)
{
	program_pairs_ = std::move(pairs);
	ASSERT_FALSE(program_pairs_.empty())
		<< "VuTestHarness::LoadProgram requires at least one instruction pair";
	ASSERT_TRUE((program_pairs_.back().upper & vu::bits::E) != 0)
		<< "VuTestHarness::LoadProgram requires E-bit on the final user-supplied "
		   "pair so the interpreter terminates deterministically.";

	// Architectural E-bit cleanup runs on the *next* exec step (one delay-slot
	// pair after the E-bit pair). Append a NOP pair so the test program is
	// self-terminating without each test having to remember.
	program_pairs_.push_back(vu::NopPair());

	const u32 prog_bytes = static_cast<u32>(program_pairs_.size() * 8u);
	ASSERT_LE(prog_bytes, ProgSize(vu_index_))
		<< "VuTestHarness program (" << prog_bytes << " bytes) exceeds VU"
		<< vu_index_ << " micro memory (" << ProgSize(vu_index_) << " bytes)";

	WriteProgramToMicro();
}

void VuTestHarness::WriteProgramToMicro()
{
	auto& vu = Regs(vu_index_);
	// Zero the entire micro memory first so a shorter program in this test
	// cannot read leftover instruction bytes from a previous longer test
	// running through the same fixture.
	std::memset(vu.Micro, 0, ProgSize(vu_index_));
	for (size_t i = 0; i < program_pairs_.size(); ++i)
	{
		const u32 base = static_cast<u32>(i * 8u) & ProgMask(vu_index_);
		std::memcpy(vu.Micro + base + 0, &program_pairs_[i].lower, 4);
		std::memcpy(vu.Micro + base + 4, &program_pairs_[i].upper, 4);
	}
}

void VuTestHarness::SeedEntryState(bool reset_block_cache)
{
	auto& vu = Regs(vu_index_);

	// Start at pair 0 (kProgramPc / 8 = 0). Stored as a pair index in
	// REG_TPC; InterpVU<idx>::Execute will internally `<<= 3` it to a
	// byte address before fetching from VU.Micro.
	vu.VI[REG_TPC].UL = kProgramPc / 8u;
	vu.start_pc = kProgramPc;

	// Mark VU<idx> as "running" so Execute's loop predicate fires. The
	// running-bit lives in VU0.VI[REG_VPU_STAT] for *both* VUs (bit 0x1
	// for VU0, bit 0x100 for VU1) — always poke VU0's copy here, then
	// also stamp the local instance for snapshot consistency on VU1.
	vuRegs[0].VI[REG_VPU_STAT].UL = (vuRegs[0].VI[REG_VPU_STAT].UL & ~0xFFFu) | RunningBit(vu_index_);
	vu.VI[REG_VPU_STAT].UL = vuRegs[0].VI[REG_VPU_STAT].UL;

	// Cycle counter / pipeline state — start fresh so the first test in
	// a fixture doesn't inherit decay from a prior test.
	vu.cycle = 0;
	vu.ebit = 0;
	vu.branch = 0;
	vu.branchpc = 0;
	vu.delaybranchpc = 0;
	vu.takedelaybranch = false;
	vu.flags = 0;
	vu.fmacreadpos = vu.fmacwritepos = vu.fmaccount = 0;
	vu.ialureadpos = vu.ialuwritepos = vu.ialucount = 0;
	std::memset(&vu.fdiv, 0, sizeof(vu.fdiv));
	std::memset(&vu.efu, 0, sizeof(vu.efu));
	std::memset(&vu.fmac, 0, sizeof(vu.fmac));
	std::memset(&vu.ialu, 0, sizeof(vu.ialu));

	InterpCpu(vu_index_)->SetStartPC(kProgramPc);
	JitCpu(vu_index_)->SetStartPC(kProgramPc);

	// Drop any compiled block left by a prior test. mVUreset re-emits the
	// dispatcher and zeroes mVU.prog.lpState — cheapest correct invalidation.
	// RunJitPreserveBlockCache skips this so pre-seeded (hydrated) blocks
	// survive into the execution.
	if (reset_block_cache)
		RecompilerTestEnvironment::ResetVuBlockCache(vu_index_);
}

void VuTestHarness::RunInterpFromSeeded()
{
	// Match production's REC_VU1 ↔ CpuVU1 invariant (the CpuVU1 = EnableVU1 ?
	// rec : interp selection made when CPU providers are initialized): the
	// interp's `_vuXGKICKTransfer` is only reachable in production when
	// EnableVU1=false (interp is the active VU1 engine), and `GetGSPacketSize`
	// only emits its bit-31 EOP signal when `(CHECK_XGKICKHACK || !REC_VU1)`
	// holds. The harness violates the
	// invariant by calling CpuIntVU1::Execute while EnableVU1=true to diff
	// against the JIT — which leaves `_vuXGKICKTransfer` looping forever on
	// any back-to-back kick because xgkickendpacket never gets set. Flip the
	// flag for the duration of the interp pass so the production guard fires
	// and the loop terminates correctly.
	const bool saved = EmuConfig.Cpu.Recompiler.EnableVU1;
	EmuConfig.Cpu.Recompiler.EnableVU1 = false;
	InterpCpu(vu_index_)->Execute(kCycleBudget);
	EmuConfig.Cpu.Recompiler.EnableVU1 = saved;
}

void VuTestHarness::RunJitFromSeeded()
{
	// recMicroVU0/1::Execute already implements a bounded-cycle entry —
	// mVU.cycles is the budget and the per-block cycle test exits to
	// the dispatcher when it exhausts. No new entry point on the microVU
	// side is required for E-bit-terminated programs.
	JitCpu(vu_index_)->Execute(kCycleBudget);
}

void VuTestHarness::Run()
{
	ASSERT_FALSE(program_pairs_.empty())
		<< "LoadProgram() must be called before Run()";

	// Set host FPCR to the VU's FPCR (FZ + ChopZero) for BOTH passes.
	// The interpreter's `_vuDIV`/`_vuSQRT` etc. compute via host scalar
	// FP (`fs / ft`), which uses the host thread FPCR. The JIT switches
	// FPCR to VU FPCR at dispatcher entry. If the harness leaves host
	// FPCR at default (round-to-nearest, no flush) for the interp pass,
	// interp produces IEEE-rounded results while JIT produces VU-rounded
	// (round-toward-zero) results — a 1-ULP false-positive divergence on
	// any FMAC/DIV with non-trivial operands. Setting both passes to
	// vu_fpcr removes the asymmetry; any divergence is then a real
	// codegen difference. Mirrors VuReplay.cpp's pattern.
	const FPControlRegister saved_fpcr = FPControlRegister::GetCurrent();
	const FPControlRegister vu_fpcr = (vu_index_ == 0)
		? EmuConfig.Cpu.VU0FPCR
		: EmuConfig.Cpu.VU1FPCR;
	FPControlRegister::SetCurrent(vu_fpcr);

	// JIT side first. SeedEntryState resets the block cache, so the JIT
	// compiles a fresh variant against this test's seeded entry pState.
	SeedEntryState();
	pre_snapshot_ = VuSnapshot::Capture(vu_index_, mem_windows_);
	path1_packets_.clear();
	RunJitFromSeeded();
	jit_snapshot_ = VuSnapshot::Capture(vu_index_, mem_windows_);
	path1_packets_jit_ = path1_packets_;

	// Restore registers + tracked memory windows, re-seed the dispatch
	// state, then run the interpreter from the same pre-state. Capture
	// the interp packet stream separately so XGKICK tests can compare.
	pre_snapshot_.Restore();
	SeedEntryState();
	path1_packets_.clear();
	RunInterpFromSeeded();
	interp_snapshot_ = VuSnapshot::Capture(vu_index_, mem_windows_);
	path1_packets_interp_ = path1_packets_;
	has_run_ = true;

	FPControlRegister::SetCurrent(saved_fpcr);

	// PipelinePermissive (default): the interpreter doesn't populate the
	// 4-stage micro_*flags pipeline arrays (those are microVU's shadow),
	// so a strict diff would fire on every run. The architectural snapshot
	// of MAC/STATUS/CLIP in VI is still strict. XgkickPacketEquivalent —
	// further skips xgkickaddr/diff/cyclecount/enable/endpacket because
	// the non-XGKICKHACK microVU path doesn't write VU1.xgkick* (see
	// VuDiffMode docstring).
	const auto diffs = DiffVu(jit_snapshot_, interp_snapshot_,
		diff_mode_, ignored_vi_);
	if (!diffs.empty())
	{
		std::ostringstream ss;
		ss << "VU" << vu_index_ << " JIT-vs-interp divergence ("
		   << diffs.size() << "):\n";
		for (const auto& d : diffs)
			ss << "  " << d << "\n";
		ss << "Pre-state:\n";
		PrintVu(ss, pre_snapshot_);
		ss << "JIT post-state:\n";
		PrintVu(ss, jit_snapshot_);
		ss << "Interp post-state:\n";
		PrintVu(ss, interp_snapshot_);
		ADD_FAILURE() << ss.str();
	}
}

void VuTestHarness::RunJitPreserveBlockCache()
{
	ASSERT_FALSE(program_pairs_.empty())
		<< "LoadProgram() must be called before RunJitPreserveBlockCache()";
	ASSERT_TRUE(has_run_)
		<< "RunJitPreserveBlockCache() replays the pre-state captured by a "
		   "prior Run() — call Run() first.";

	const FPControlRegister saved_fpcr = FPControlRegister::GetCurrent();
	const FPControlRegister vu_fpcr = (vu_index_ == 0)
		? EmuConfig.Cpu.VU0FPCR
		: EmuConfig.Cpu.VU1FPCR;
	FPControlRegister::SetCurrent(vu_fpcr);

	pre_snapshot_.Restore();
	SeedEntryState(/*reset_block_cache=*/false);
	path1_packets_.clear();
	RunJitFromSeeded();
	jit_snapshot_ = VuSnapshot::Capture(vu_index_, mem_windows_);
	path1_packets_jit_ = path1_packets_;

	FPControlRegister::SetCurrent(saved_fpcr);
}

void VuTestHarness::RunInterpOnly()
{
	ASSERT_FALSE(program_pairs_.empty())
		<< "LoadProgram() must be called before RunInterpOnly()";

	const FPControlRegister saved_fpcr = FPControlRegister::GetCurrent();
	const FPControlRegister vu_fpcr = (vu_index_ == 0)
		? EmuConfig.Cpu.VU0FPCR
		: EmuConfig.Cpu.VU1FPCR;
	FPControlRegister::SetCurrent(vu_fpcr);

	SeedEntryState();
	pre_snapshot_ = VuSnapshot::Capture(vu_index_, mem_windows_);
	path1_packets_.clear();
	RunInterpFromSeeded();
	interp_snapshot_ = VuSnapshot::Capture(vu_index_, mem_windows_);
	jit_snapshot_ = interp_snapshot_;
	path1_packets_interp_ = path1_packets_;
	path1_packets_jit_ = path1_packets_;
	has_run_ = true;

	FPControlRegister::SetCurrent(saved_fpcr);
}

u32 VuTestHarness::GetVfBitsInterp(u32 reg_idx, char lane) const
{
	const auto& vf = interp_snapshot_.regs.VF[reg_idx];
	switch (lane) { case 'x': return vf.i.x; case 'y': return vf.i.y;
	                 case 'z': return vf.i.z; case 'w': return vf.i.w; }
	return 0;
}
u32 VuTestHarness::GetVfBitsJit(u32 reg_idx, char lane) const
{
	const auto& vf = jit_snapshot_.regs.VF[reg_idx];
	switch (lane) { case 'x': return vf.i.x; case 'y': return vf.i.y;
	                 case 'z': return vf.i.z; case 'w': return vf.i.w; }
	return 0;
}
float VuTestHarness::GetVfInterp(u32 reg_idx, char lane) const
{
	const auto& vf = interp_snapshot_.regs.VF[reg_idx];
	switch (lane) { case 'x': return vf.f.x; case 'y': return vf.f.y;
	                 case 'z': return vf.f.z; case 'w': return vf.f.w; }
	return 0.0f;
}
float VuTestHarness::GetVfJit(u32 reg_idx, char lane) const
{
	const auto& vf = jit_snapshot_.regs.VF[reg_idx];
	switch (lane) { case 'x': return vf.f.x; case 'y': return vf.f.y;
	                 case 'z': return vf.f.z; case 'w': return vf.f.w; }
	return 0.0f;
}
u32 VuTestHarness::GetViInterp(u32 reg_idx) const
{
	return interp_snapshot_.regs.VI[reg_idx].UL & ViMaskFor(reg_idx);
}
u32 VuTestHarness::GetViJit(u32 reg_idx) const
{
	return jit_snapshot_.regs.VI[reg_idx].UL & ViMaskFor(reg_idx);
}
u32 VuTestHarness::GetMemU32Interp(u32 addr) const
{
	const u32 a = addr & MemMask(vu_index_);
	for (const auto& w : interp_snapshot_.mem_windows)
	{
		if (a >= w.addr && a + 4 <= w.addr + w.bytes.size())
		{
			u32 value = 0;
			std::memcpy(&value, w.bytes.data() + (a - w.addr), 4);
			return value;
		}
	}
	return 0;
}
bool VuTestHarness::HasTerminated() const
{
	return (vuRegs[0].VI[REG_VPU_STAT].UL & RunningBit(vu_index_)) == 0;
}

u32 VuTestHarness::GetMemU32Jit(u32 addr) const
{
	const u32 a = addr & MemMask(vu_index_);
	for (const auto& w : jit_snapshot_.mem_windows)
	{
		if (a >= w.addr && a + 4 <= w.addr + w.bytes.size())
		{
			u32 value = 0;
			std::memcpy(&value, w.bytes.data() + (a - w.addr), 4);
			return value;
		}
	}
	return 0;
}

} // namespace recompiler_tests
