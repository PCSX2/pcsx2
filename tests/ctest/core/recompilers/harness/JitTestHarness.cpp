// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "JitTestHarness.h"

#include "IopMem.h"
#include "R3000A.h"

#include <cstring>
#include <sstream>

namespace recompiler_tests {

namespace {

constexpr u32 kProgramPc = RecompilerTestEnvironment::kProgramPc;
constexpr u32 kParkingPc = RecompilerTestEnvironment::kParkingPc;

// Writes a single 32-bit word to IOP RAM via the memory system.
void WriteIopU32(u32 addr, u32 value)
{
	iopMemWrite32(addr, value);
}

// Zero-init every user-visible field of psxRegs. The harness owns the IOP
// register file for the duration of Run(), so any pre-existing contents are
// the previous test's post-state and must be wiped before setting up the
// new pre-state. (cycle / interrupt / event bookkeeping is also zeroed;
// psxInt.ExecuteBlock re-derives iopCycleEE from its parameter.)
void ZeroPsxRegs()
{
	std::memset(&psxRegs, 0, sizeof(psxRegisters));
}

} // namespace

JitTestHarness::JitTestHarness(Mode mode)
	: mode_(mode)
{
	EXPECT_TRUE(RecompilerTestEnvironment::IsReady())
		<< "RecompilerTestEnvironment was not set up — harness cannot run. "
		   "Make sure the binary's main.cpp registered the environment.";
	ZeroPsxRegs();

	// The test env installs `psxCpu = &psxInt` to avoid dragging IOP-rec
	// compilation into every EE test (see RecompilerTestEnvironment.cpp).
	// For the IOP harness, guest-level stores via iopMemWrite* must
	// cascade through `psxCpu->Clear` → psxRecClearMem → LUT invalidation,
	// which is the real SMC path. Flip psxCpu to &psxRec for the harness
	// lifetime in DiffJitVsInterp mode; restore on destruction.
	saved_psxCpu_ = psxCpu;
	if (mode_ == Mode::DiffJitVsInterp)
		psxCpu = &psxRec;
}

JitTestHarness::~JitTestHarness()
{
	psxCpu = saved_psxCpu_;
}

void JitTestHarness::SetGpr(u32 reg_idx, u32 value)
{
	if (reg_idx == 0)
		return; // r0 is hardwired zero
	psxRegs.GPR.r[reg_idx] = value;
}

void JitTestHarness::SetHi(u32 value) { psxRegs.GPR.r[32] = value; }
void JitTestHarness::SetLo(u32 value) { psxRegs.GPR.r[33] = value; }

void JitTestHarness::SetCp0(u32 reg_idx, u32 value)
{
	psxRegs.CP0.r[reg_idx] = value;
}

void JitTestHarness::WriteU8(u32 addr, u8 value)
{
	iopMemWrite8(addr, value);
	MergeTrackedWindow(addr & ~0x3u, 4);
}

void JitTestHarness::WriteU16(u32 addr, u16 value)
{
	iopMemWrite16(addr, value);
	MergeTrackedWindow(addr & ~0x3u, 4);
}

void JitTestHarness::WriteU32(u32 addr, u32 value)
{
	iopMemWrite32(addr, value);
	MergeTrackedWindow(addr & ~0x3u, 4);
}

void JitTestHarness::WriteBytes(u32 addr, const void* src, size_t bytes)
{
	const u8* p = static_cast<const u8*>(src);
	for (size_t i = 0; i < bytes; ++i)
		iopMemWrite8(addr + static_cast<u32>(i), p[i]);
	MergeTrackedWindow(addr, bytes);
}

void JitTestHarness::TrackMemWindow(u32 addr, size_t bytes)
{
	MergeTrackedWindow(addr, bytes);
}

u8  JitTestHarness::ReadU8 (u32 addr) const { return iopMemRead8(addr); }
u16 JitTestHarness::ReadU16(u32 addr) const { return iopMemRead16(addr); }
u32 JitTestHarness::ReadU32(u32 addr) const { return iopMemRead32(addr); }

void JitTestHarness::MergeTrackedWindow(u32 addr, size_t bytes)
{
	// Round to 4-byte boundaries and merge contiguous windows.
	u32 start = addr & ~0x3u;
	size_t end = ((addr + bytes + 3u) & ~0x3u);
	size_t new_size = end - start;

	for (auto& w : mem_windows_)
	{
		const u32 w_end = w.addr + static_cast<u32>(w.bytes.size());
		if (start + new_size < w.addr || start > w_end)
			continue; // disjoint
		// Overlap or touch — merge.
		const u32 merged_start = (start < w.addr) ? start : w.addr;
		const u32 merged_end = (start + new_size > w_end) ? static_cast<u32>(start + new_size) : w_end;
		w.addr = merged_start;
		w.bytes.resize(merged_end - merged_start);
		return;
	}

	mem_windows_.push_back(MemWindow{start, std::vector<u8>(new_size)});
}

void JitTestHarness::LoadProgram(std::initializer_list<u32> instructions)
{
	LoadProgramAt(kProgramPc, instructions, /*append_jr_ra_term=*/true);
}

void JitTestHarness::LoadProgramNoTerm(std::initializer_list<u32> instructions)
{
	LoadProgramAt(kProgramPc, instructions, /*append_jr_ra_term=*/false);
}

void JitTestHarness::LoadProgramAt(u32 pc,
                                   std::initializer_list<u32> instructions,
                                   bool append_jr_ra_term)
{
	LoadProgramAt(pc, instructions.begin(), instructions.size(), append_jr_ra_term);
}

void JitTestHarness::LoadProgramAt(u32 pc,
                                   const u32* instructions,
                                   size_t count,
                                   bool append_jr_ra_term)
{
	const size_t total = count + (append_jr_ra_term ? 2 : 0);

	for (size_t i = 0; i < count; ++i)
		WriteIopU32(pc + static_cast<u32>(i * 4), instructions[i]);

	if (append_jr_ra_term)
	{
		WriteIopU32(pc + static_cast<u32>(count * 4), mips::JR(mips::reg::ra));
		WriteIopU32(pc + static_cast<u32>((count + 1) * 4), mips::NOP);
	}

	program_regions_.push_back({pc, static_cast<u32>(total)});
}

void JitTestHarness::SetPc(u32 pc)
{
	pc_override_ = true;
	pc_override_value_ = pc;
}

void JitTestHarness::SetRa(u32 ra)
{
	ra_override_ = true;
	ra_override_value_ = ra;
}

void JitTestHarness::InvalidateProgramRegions()
{
	if (!psxRec.Clear)
		return;
	for (const auto& r : program_regions_)
		psxRec.Clear(r.pc, r.size_words);
	// Parking lot — belt-and-suspenders; iopMemWrite32 at env setup
	// should have cleared, but a previous test's block may linger.
	psxRec.Clear(kParkingPc, 2);
}

void JitTestHarness::Run()
{
	ASSERT_FALSE(program_regions_.empty())
		<< "LoadProgram*() must be called before Run()";

	// Starting PC and return address — entry point defaults to kProgramPc
	// (the typical single-block test layout); ra lands in the parking-lot
	// self-loop so `jr ra` terminators exit cleanly. Either can be
	// overridden via SetPc() / SetRa() for multi-block or resume tests.
	psxRegs.pc = pc_override_ ? pc_override_value_ : kProgramPc;
	psxRegs.GPR.n.ra = ra_override_ ? ra_override_value_ : kParkingPc;

	InvalidateProgramRegions();

	ExecuteAndDiff();
}

void JitTestHarness::RunResume()
{
	ASSERT_FALSE(program_regions_.empty())
		<< "LoadProgram*() must be called before RunResume()";

	// RunResume picks up from whatever state the caller left behind. If
	// the caller wants to re-enter from a specific PC, they do
	// `h.SetPc(...)` before RunResume. Override fields are respected
	// if they've been set; otherwise psxRegs.pc / ra are left alone.
	if (pc_override_)
		psxRegs.pc = pc_override_value_;
	if (ra_override_)
		psxRegs.GPR.n.ra = ra_override_value_;

	// No invalidation — the point of Resume is to exercise the cache's
	// behavior as of now. Guest stores to program memory via
	// iopMemWrite32 already invalidate through psxCpu->Clear, which is
	// exactly the SMC path under test.
	ExecuteAndDiff();
}

void JitTestHarness::ExecuteAndDiff()
{
	// Capture pre-state AFTER any PC/RA seeding so the pre-snapshot used
	// for the interpreter side's restore matches what the JIT saw at entry.
	pre_snapshot_ = IopSnapshot::Capture(mem_windows_);

	if (mode_ == Mode::DiffJitVsInterp)
	{
		psxRec.ExecuteBlock(kCycleBudget);
		jit_snapshot_ = IopSnapshot::Capture(mem_windows_);

		// Restore pre-state, then run interpreter from the same initial
		// conditions.
		pre_snapshot_.Restore();
	}

	psxInt.ExecuteBlock(kCycleBudget);
	interp_snapshot_ = IopSnapshot::Capture(mem_windows_);

	// In InterpOnly mode, lock the interpreter output as the "spec" for
	// GetGprJit() / JitSnapshot() accessors. That way tests that compare
	// against specific architectural values pass symmetrically regardless
	// of whether the JIT is wired or not.
	if (mode_ == Mode::InterpOnly)
		jit_snapshot_ = interp_snapshot_;

	has_run_ = true;

	// Overrides apply to ONE run only — consume them.
	pc_override_ = false;
	ra_override_ = false;

	if (mode_ == Mode::DiffJitVsInterp)
	{
		const auto diffs = DiffIop(jit_snapshot_, interp_snapshot_);
		if (!diffs.empty())
		{
			std::ostringstream ss;
			ss << "JIT vs INTERP divergence (" << diffs.size() << "):\n";
			for (const auto& d : diffs)
				ss << "  " << d << "\n";
			ss << "Pre-state:\n";
			PrintIop(ss, pre_snapshot_);
			ss << "JIT post-state:\n";
			PrintIop(ss, jit_snapshot_);
			ss << "INTERP post-state:\n";
			PrintIop(ss, interp_snapshot_);
			ADD_FAILURE() << ss.str();
		}
	}
}

u32 JitTestHarness::GetGprJit(u32 reg_idx) const
{
	return jit_snapshot_.regs.GPR.r[reg_idx];
}

u32 JitTestHarness::GetGprInterp(u32 reg_idx) const
{
	return interp_snapshot_.regs.GPR.r[reg_idx];
}

u32 JitTestHarness::GetHiJit() const    { return jit_snapshot_.regs.GPR.r[32]; }
u32 JitTestHarness::GetLoJit() const    { return jit_snapshot_.regs.GPR.r[33]; }
u32 JitTestHarness::GetHiInterp() const { return interp_snapshot_.regs.GPR.r[32]; }
u32 JitTestHarness::GetLoInterp() const { return interp_snapshot_.regs.GPR.r[33]; }

} // namespace recompiler_tests
