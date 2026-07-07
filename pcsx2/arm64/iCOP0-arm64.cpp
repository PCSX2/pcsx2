// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE COP0 Instruction Codegen — NEON-based

#include "arm64/iR5900-arm64.h"
#include "arm64/AsmHelpers.h"

#include "Hw.h"
#include "Memory.h"

namespace a64 = vixl::aarch64;

namespace Interp = R5900::Interpreter::OpcodeImpl::COP0;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP0 {

// COP0 branch (BC0F/T/FL/TL) — native DMAC-condition test. The branch
// condition is (((DMAC_STAT | ~DMAC_PCR) & 0x3ff) == 0x3ff): BC0T branches
// when true, BC0F when false. Emitting the test inline avoids a costly
// iFlushCall + interpreter dispatch per iteration in DMAC-poll spin loops.
// BC0 uses the same SaveBranchState/recompileNextInstruction/SetBranchImm
// branch-emission pattern as recBC1F/recBC1T on this target.

// Emit the DMAC condition test, leaving the result in the flags.
// 32-bit loads are fine: the result is masked to the low 10 bits, matching x86.
// De Morgan (AX-12, after ARMSX2 ac56c2523):
//   ((STAT | ~PCR) & 0x3ff) == 0x3ff  ⇔  (PCR & ~STAT & 0x3ff) == 0
// so one BIC + TST replaces MVN/ORR/AND/CMP, and a single materialized base
// covers both registers (DMAC_STAT 0xE010 / DMAC_PCR 0xE020, 16 B apart in
// eeHw). Flag sense unchanged: EQ ⇔ condition true.
static void _setupBranchTestBC0()
{
	_eeFlushAllDirty();
	armMoveAddressToReg(RSCRATCHADDR, &psHu32(DMAC_STAT));
	armAsm->Ldr(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));  // STAT
	armAsm->Ldr(RWARG1, a64::MemOperand(RSCRATCHADDR, 16)); // PCR
	armAsm->Bic(RWARG1, RWARG1, RWSCRATCH);                 // PCR & ~STAT
	armAsm->Tst(RWARG1, 0x3ff);                             // EQ ⇔ condition true
}

static a64::Label* s_pBC0Label = nullptr;

static void recSetBranchBC0(bool branchOnTrue)
{
	_setupBranchTestBC0();
	s_pBC0Label = new a64::Label();
	// Emit the "skip the taken-branch" jump: the negation of the branch cond.
	if (branchOnTrue)
		armAsm->B(s_pBC0Label, a64::ne);  // BC0T: skip (fall through) when condition false
	else
		armAsm->B(s_pBC0Label, a64::eq);  // BC0F: skip when condition true
}

static void recBindBC0Label()
{
	armAsm->Bind(s_pBC0Label);
	delete s_pBC0Label;
	s_pBC0Label = nullptr;
}

void recBC0F()
{
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	const bool swap = TrySwapDelaySlot(0, 0, 0, false);
	recSetBranchBC0(false);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBC0Label();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

void recBC0T()
{
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	const bool swap = TrySwapDelaySlot(0, 0, 0, false);
	recSetBranchBC0(true);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBC0Label();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

void recBC0FL()
{
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	recSetBranchBC0(false);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBC0Label();
	LoadBranchState();
	SetBranchImm(pc);
}

void recBC0TL()
{
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	recSetBranchBC0(true);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBC0Label();
	LoadBranchState();
	SetBranchImm(pc);
}

REC_FUNC(TLBR);
REC_FUNC(TLBP);
REC_FUNC(TLBWI);
REC_FUNC(TLBWR);

REC_SYS(ERET);
REC_SYS(EI);

// DI — inline, non-branching. Unlike EI (which must branch so newly-enabled
// interrupts fire), disabling interrupts needs no block exit, so DI is emitted
// inline and the block stays open. Mirrors x86 iCOP0.cpp. The next instruction
// is recompiled BEFORE applying DI so the interrupt-mask change takes effect one
// instruction late (fixes booting in Jak X, Namco 50th, Spongebob, The
// Incredibles, etc.).
void recDI()
{
	if (!g_recompilingDelaySlot)
		recompileNextInstruction(false, false); // DI delayed by one instruction

	// Clear Status.EIE (bit 16) unless in user mode with no exception level:
	//   clear iff (EXL|ERL|EDI) set OR KSU == 0 (kernel). Matches Interp::DI
	//   (COP0.cpp:708-717) and the x86 TEST 0x20006 / TEST 0x18 guard.
	a64::Label doClear;
	a64::Label done;
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.CP0.r[12]); // Status
	armAsm->Tst(RWSCRATCH, 0x20006);               // EXL | ERL | EDI
	armAsm->B(&doClear, a64::ne);
	armAsm->Tst(RWSCRATCH, 0x18);                  // KSU (non-zero => user mode)
	armAsm->B(&done, a64::ne);
	armAsm->Bind(&doClear);
	armAsm->And(RWSCRATCH, RWSCRATCH, ~static_cast<u32>(0x10000)); // clear EIE
	armStoreEERegPtr(RWSCRATCH, &cpuRegs.CP0.r[12]);
	armAsm->Bind(&done);
}

#ifdef FORCE_INTERP_COP0
REC_FUNC(MFC0);
REC_FUNC(MTC0);
#else

// Apply pending block cycles to the RECCYCLE delta and flush the ABSOLUTE
// cycle to cpuRegs.cycle so the interpreter helper (which reads
// cpuRegs.cycle directly) sees the right value. The helper is called inline
// mid-block (not via DispatcherEvent), so the caller must follow up with
// emitReloadCycle() once it returns to keep RECCYCLE in sync with anything
// the helper wrote (MTC0 Status/Compare reschedule nextEventCycle too).
static void emitFlushBlockCycles()
{
	u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	armFlushCycleDelta();
}

// Same as emitFlushBlockCycles, but additionally leaves the absolute cycle
// value in `absOut` for inline Count arithmetic (RECCYCLE itself stays the
// delta and no longer holds the absolute value).
static void emitFlushBlockCyclesAbs(const a64::Register& absOut)
{
	u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	armAsm->Ldr(absOut, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Add(absOut, RECCYCLE, absOut);
	armAsm->Str(absOut, armCpuRegMem(&cpuRegs.cycle));
}

static void emitReloadCycle()
{
	armReloadCycleDelta();
}

// MFC0: rt = sign_extend(COP0[rd])
void recMFC0()
{
	// Count (rd=9) must still tick even when writing to $zero — interpreter
	// MFC0 increments CP0.n.Count and updates lastCOP0Cycle before checking
	// _Rt_. Match that gate exactly.
	if (!_Rt_ && _Rd_ != 9)
		return;

	switch (_Rd_)
	{
		case 9: // Count — inline cycle update; no iFlushCall/interp call
		{       // (matches interp COP0.cpp:564-575).
			// Bring cpuRegs.cycle current and get the ABSOLUTE cycle in x9
			// (Count arithmetic needs the absolute value; RECCYCLE is the
			// delta). x9/x10 are the non-allocatable load/store scratches —
			// there is no iFlushCall on this path, so allocatable
			// caller-saved regs (x0-x7) may hold cached guest values here
			// (e.g. a preceding load's dest) and must not be clobbered.
			emitFlushBlockCyclesAbs(a64::x9); // x9 = absolute cycle, also in memory
			// incr = cycle - lastCOP0Cycle; if (incr == 0) incr++; (interp :566-568)
			armAsm->Ldr(RXSCRATCH, armCpuRegMem(&cpuRegs.lastCOP0Cycle));
			armAsm->Sub(RXSCRATCH, a64::x9, RXSCRATCH);
			armAsm->Cmp(RXSCRATCH, 0);
			armAsm->Csinc(RXSCRATCH, RXSCRATCH, a64::xzr, a64::ne); // 0 -> 1
			// CP0.n.Count += incr (32-bit register; low 32 of incr)
			armAsm->Ldr(a64::w10, armCpuRegMem(&cpuRegs.CP0.r[9]));
			armAsm->Add(a64::w10, a64::w10, RWSCRATCH);
			armAsm->Str(a64::w10, armCpuRegMem(&cpuRegs.CP0.r[9]));
			// lastCOP0Cycle = cycle (interp :569)
			armAsm->Str(a64::x9, armCpuRegMem(&cpuRegs.lastCOP0Cycle));
			if (!_Rt_)
				return;
			// rt = sign_extend(CP0.r[9]) (interp :571-577)
			_deleteEEreg(_Rt_, 0);
			GPR_DEL_CONST(_Rt_);
			armLoadEERegPtr(RWSCRATCH, &cpuRegs.CP0.r[9]);
			armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
			armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
			return;
		}

		case 25: // Performance counters — cycle-dependent
			iFlushCall(FLUSH_INTERPRETER);
			emitFlushBlockCycles();
			armEmitCall((void*)Interp::MFC0);
			emitReloadCycle();
			// Interp::MFC0 wrote GPR[rt] in canonical memory behind the JIT's
			// back, and the C call itself clobbered the caller-saved pins —
			// the full table reload covers both.
			armReloadEEGPRPins();
			return;

		case 24: // Debug breakpoint register — ignore
			return;

		case 12: // Status — mask reserved bits, matching interp MFC0 case 12 (COP0.cpp)
			_deleteEEreg(_Rt_, 0);
			GPR_DEL_CONST(_Rt_);
			armLoadEERegPtr(RWSCRATCH, &cpuRegs.CP0.r[_Rd_]);
			// 0xf0c79c1f is not a valid AArch64 logical immediate, so materialize it
			// in a scratch register. Use the non-allocatable w10, not the reserved
			// address scratch x17 (RSCRATCHADDR, clobbered by armLoad*Ptr) and not
			// an allocatable caller-saved reg like w0 — with no iFlushCall on this
			// path a cached guest value (e.g. a preceding load's dest) may live there.
			armAsm->Mov(a64::w10, 0xf0c79c1fu);
			armAsm->And(RWSCRATCH, RWSCRATCH, a64::w10);
			armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
			armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
			return;

		default:
			// Simple case: rt = sign_extend(cpuRegs.CP0.r[rd])
			_deleteEEreg(_Rt_, 0);
			GPR_DEL_CONST(_Rt_);
			armLoadEERegPtr(RWSCRATCH, &cpuRegs.CP0.r[_Rd_]);
			armAsm->Sxtw(RXSCRATCH, RWSCRATCH);
			armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rt_].UD[0]);
			return;
	}
}

// MTC0: COP0[rd] = rt
void recMTC0()
{
	switch (_Rd_)
	{
		case 9: // Count — inline; no iFlushCall/interp call
		{       // (matches interp COP0.cpp:583-585): lastCOP0Cycle = cycle;
			// CP0.r[9] = rt[31:0]. Bring cycle current first; the absolute
			// value (x9, non-allocatable scratch — see recMFC0 case 9) is
			// what lastCOP0Cycle stores.
			emitFlushBlockCyclesAbs(a64::x9); // x9 = absolute cycle, also in memory
			armAsm->Str(a64::x9, armCpuRegMem(&cpuRegs.lastCOP0Cycle));
			if (GPR_IS_CONST1(_Rt_))
			{
				armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
				armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.r[9]));
			}
			else
			{
				_deleteEEreg(_Rt_, 1);
				armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
				armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.r[9]));
			}
			return;
		}

		case 12: // Status — has side effects (interrupt check)
		case 16: // Config — has side effects
		case 25: // Performance counters
			iFlushCall(FLUSH_INTERPRETER);
			emitFlushBlockCycles();
			armFlushEEClobberedPins(); // lazy-dirty seam: pairs with the reload below
			armEmitCall((void*)Interp::MTC0);
			emitReloadCycle();
			// MTC0 writes no guest GPRs; restore the caller-saved pins the
			// C call clobbered.
			armReloadEEClobberedPins();
			return;

		case 24: // Debug breakpoint register — log-only in interp (COP0.cpp:599-601)
			return;

		default:
			// Simple case: cpuRegs.CP0.r[rd] = rt[31:0]
			if (GPR_IS_CONST1(_Rt_))
			{
				armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
				armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.r[_Rd_]));
			}
			else
			{
				_deleteEEreg(_Rt_, 1);
				armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
				armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.r[_Rd_]));
			}
			return;
	}
}

#endif // !FORCE_INTERP_COP0

} // namespace COP0
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
