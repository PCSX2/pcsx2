// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE Branch Instruction Codegen — NEON-based
// Branches read GPR values for comparison. Values are extracted from
// NEON registers via FMOV or loaded from memory after flush.

#include "arm64/iR5900-arm64.h"
#include "common/Assertions.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

#ifdef FORCE_INTERP_BRANCH
REC_SYS(BEQ);
REC_SYS(BNE);
REC_SYS(BEQL);
REC_SYS(BNEL);
REC_SYS(BLEZ);
REC_SYS(BGTZ);
REC_SYS(BLTZ);
REC_SYS(BGEZ);
REC_SYS(BLEZL);
REC_SYS(BGTZL);
REC_SYS(BLTZL);
REC_SYS(BGEZL);
REC_SYS(BLTZAL);
REC_SYS(BGEZAL);
REC_SYS(BLTZALL);
REC_SYS(BGEZALL);
#else

// Thread-local label for the "not taken" forward branch
static thread_local a64::Label* s_pBranchLabel = nullptr;

// Load a GPR value into an ARM64 X register for comparison. Allocator-aware:
// _eeFlushAllDirty leaves slots in MODE_READ (clean) so any GPR/NEON slot
// holding `gprreg` is still authoritative and can be read via Mov/Fmov,
// saving an LDR per branch operand. Falls back to memory when unallocated.
static void loadGPRtoX(const a64::Register& dst, int gprreg)
{
	_eeMoveGPRtoR(dst, gprreg);
}

// Emit comparison for BEQ/BNE and set up forward branch.
//
// `bne` selects the emitted forward-skip condition, NOT the instruction:
// bne==0 skips on `ne` (used by BEQ and, with its inverted structure, BNEL);
// bne==1 skips on `eq`. The forward branch jumps over the delay slot on the
// not-taken edge.
//
// Const-operand fast paths (one side const-folded): let vixl pick the optimal
// CMP/CMN-immediate encoding, and for compare-against-zero (BEQ/BNE $zero — the
// most common branch shape) collapse the whole test to a single Cbz/Cbnz with
// no Cmp at all.
// Cbz/Cbnz reach ±1MB, strictly wider than the Tbz already proven safe over
// this same skip distance in recSetBranchL.
static void recSetBranchEQ(int bne, int process)
{
	s_pBranchLabel = new a64::Label();

	if (process & (PROCESS_CONSTS | PROCESS_CONSTT))
	{
		const int constReg = (process & PROCESS_CONSTS) ? _Rs_ : _Rt_;
		const int liveReg = (process & PROCESS_CONSTS) ? _Rt_ : _Rs_;
		const s64 cval = g_cpuConstRegs[constReg].SD[0];

		_eeFlushAllDirty();
		loadGPRtoX(RXARG1, liveReg);

		if (cval == 0)
		{
			// Single test-and-branch against $zero — no Cmp.
			// bne==0 skips on ne → live != 0 → Cbnz; bne==1 skips on eq → Cbz.
			if (bne)
				armAsm->Cbz(RXARG1, s_pBranchLabel);
			else
				armAsm->Cbnz(RXARG1, s_pBranchLabel);
			return;
		}

		// vixl emits a single CMP/CMN immediate when cval fits, else materializes.
		armAsm->Cmp(RXARG1, cval);
	}
	else
	{
		_eeFlushAllDirty();
		loadGPRtoX(RXARG1, _Rs_);
		loadGPRtoX(RXSCRATCH, _Rt_);
		armAsm->Cmp(RXARG1, RXSCRATCH);
	}

	if (bne)
		armAsm->B(s_pBranchLabel, a64::eq);
	else
		armAsm->B(s_pBranchLabel, a64::ne);
}

// Emit comparison for BLTZ/BGEZ (rs vs 0) and set up forward branch.
//
// The "forward branch" jumps over the delay slot when the BLTZ/BGEZ would
// NOT be taken. For BLTZ (ltz=1) we skip when rs >= 0, i.e. bit 63 of the
// 64-bit GPR is zero → Tbz. For BGEZ (ltz=0) we skip when rs < 0, i.e.
// bit 63 is one → Tbnz. Tbz/Tbnz directly test a bit and branch, so no
// Cmp insn is needed. Shares the centralised setup across all 8 BLTZ/BGEZ/L/AL/ALL
// callers in this file.
static void recSetBranchL(int ltz)
{
	_eeFlushAllDirty();
	loadGPRtoX(RXSCRATCH, _Rs_);

	s_pBranchLabel = new a64::Label();
	if (ltz)
		armAsm->Tbz(RXSCRATCH, 63, s_pBranchLabel);
	else
		armAsm->Tbnz(RXSCRATCH, 63, s_pBranchLabel);
}

// Bind the forward branch label
static void recBindBranchLabel()
{
	pxAssert(s_pBranchLabel != nullptr);
	armAsm->Bind(s_pBranchLabel);
	delete s_pBranchLabel;
	s_pBranchLabel = nullptr;
}

//// BEQ — branch if rs == rt
static void recBEQ_const()
{
	u32 branchTo;
	if (g_cpuConstRegs[_Rs_].SD[0] == g_cpuConstRegs[_Rt_].SD[0])
		branchTo = ((s32)_Imm_ * 4) + pc;
	else
		branchTo = pc + 4;
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);
}

static void recBEQ_process(int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (_Rs_ == _Rt_)
	{
		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, _Rt_, 0, true);
	recSetBranchEQ(0, process);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBranchLabel();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

void recBEQ()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
		recBEQ_const();
	else if (GPR_IS_CONST1(_Rs_))
		recBEQ_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_))
		recBEQ_process(PROCESS_CONSTT);
	else
		recBEQ_process(0);
}

//// BNE — branch if rs != rt
static void recBNE_const()
{
	u32 branchTo;
	if (g_cpuConstRegs[_Rs_].SD[0] != g_cpuConstRegs[_Rt_].SD[0])
		branchTo = ((s32)_Imm_ * 4) + pc;
	else
		branchTo = pc + 4;
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);
}

static void recBNE_process(int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (_Rs_ == _Rt_)
	{
		recompileNextInstruction(true, false);
		SetBranchImm(pc);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, _Rt_, 0, true);
	recSetBranchEQ(1, process);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBranchLabel();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

void recBNE()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
		recBNE_const();
	else if (GPR_IS_CONST1(_Rs_))
		recBNE_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_))
		recBNE_process(PROCESS_CONSTT);
	else
		recBNE_process(0);
}

//// BEQL — branch likely if rs == rt
static void recBEQL_const()
{
	// Capture the taken target BEFORE recompileNextInstruction advances pc by 4
	// (consistent with recBEQ_const / recBEQL_process).
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	if (g_cpuConstRegs[_Rs_].SD[0] == g_cpuConstRegs[_Rt_].SD[0])
	{
		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
	}
	else
		SetBranchImm(pc + 4);
}

static void recBEQL_process(int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;
	recSetBranchEQ(0, process);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBranchLabel();
	LoadBranchState();
	SetBranchImm(pc);
}

void recBEQL()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
		recBEQL_const();
	else if (GPR_IS_CONST1(_Rs_))
		recBEQL_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_))
		recBEQL_process(PROCESS_CONSTT);
	else
		recBEQL_process(0);
}

//// BNEL — branch likely if rs != rt
static void recBNEL_const()
{
	// Capture the taken target BEFORE recompileNextInstruction advances pc by 4
	// (consistent with recBNE_const / recBNEL_process).
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	if (g_cpuConstRegs[_Rs_].SD[0] != g_cpuConstRegs[_Rt_].SD[0])
	{
		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
	}
	else
		SetBranchImm(pc + 4);
}

static void recBNEL_process(int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;
	recSetBranchEQ(0, process);

	SaveBranchState();
	SetBranchImm(pc + 4);

	recBindBranchLabel();
	LoadBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);
}

void recBNEL()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
		recBNEL_const();
	else if (GPR_IS_CONST1(_Rs_))
		recBNEL_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_))
		recBNEL_process(PROCESS_CONSTT);
	else
		recBNEL_process(0);
}

/*********************************************************
 * Single-register branches: BLTZ, BGEZ, BLEZ, BGTZ     *
 *********************************************************/

static void recBranchSingle(a64::Condition skip_cond)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		bool taken;
		s64 val = g_cpuConstRegs[_Rs_].SD[0];
		if (skip_cond == a64::gt) taken = (val <= 0);
		else if (skip_cond == a64::le) taken = (val > 0);
		else if (skip_cond == a64::ge) taken = (val < 0);
		else if (skip_cond == a64::lt) taken = (val >= 0);
		else taken = false;

		if (!taken) branchTo = pc + 4;
		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);

	if (skip_cond == a64::ge || skip_cond == a64::lt)
	{
		recSetBranchL(skip_cond == a64::ge ? 1 : 0);
	}
	else
	{
		_eeFlushAllDirty();
		loadGPRtoX(RXSCRATCH, _Rs_);
		armAsm->Cmp(RXSCRATCH, 0);
		s_pBranchLabel = new a64::Label();
		armAsm->B(s_pBranchLabel, skip_cond);
	}

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBranchLabel();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

static void recBranchSingleLikely(a64::Condition skip_cond)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		bool taken;
		s64 val = g_cpuConstRegs[_Rs_].SD[0];
		if (skip_cond == a64::gt) taken = (val <= 0);
		else if (skip_cond == a64::le) taken = (val > 0);
		else if (skip_cond == a64::ge) taken = (val < 0);
		else if (skip_cond == a64::lt) taken = (val >= 0);
		else taken = false;

		if (taken)
		{
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		else
			SetBranchImm(pc + 4);
		return;
	}

	if (skip_cond == a64::ge || skip_cond == a64::lt)
	{
		recSetBranchL(skip_cond == a64::ge ? 1 : 0);
	}
	else
	{
		_eeFlushAllDirty();
		loadGPRtoX(RXSCRATCH, _Rs_);
		armAsm->Cmp(RXSCRATCH, 0);
		s_pBranchLabel = new a64::Label();
		armAsm->B(s_pBranchLabel, skip_cond);
	}

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBranchLabel();
	LoadBranchState();
	SetBranchImm(pc);
}

void recBLEZ() { recBranchSingle(a64::gt); }
void recBGTZ() { recBranchSingle(a64::le); }
void recBLTZ() { recBranchSingle(a64::ge); }
void recBGEZ() { recBranchSingle(a64::lt); }

void recBLEZL() { recBranchSingleLikely(a64::gt); }
void recBGTZL() { recBranchSingleLikely(a64::le); }
void recBLTZL() { recBranchSingleLikely(a64::ge); }
void recBGEZL() { recBranchSingleLikely(a64::lt); }

/*********************************************************
 * Branch-and-link: BLTZAL, BGEZAL, BLTZALL, BGEZALL    *
 *********************************************************/

static void recBranchLink(bool ltz)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllDirty();

	_deleteEEreg(31, 0);
	// Store return address directly to memory
	armAsm->Mov(RXSCRATCH, (u64)(pc + 4));
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.n.ra.UD[0]);

	if (GPR_IS_CONST1(_Rs_))
	{
		bool taken = ltz ? (g_cpuConstRegs[_Rs_].SD[0] < 0) : (g_cpuConstRegs[_Rs_].SD[0] >= 0);
		if (!taken) branchTo = pc + 4;
		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);
	recSetBranchL(ltz ? 1 : 0);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBranchLabel();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

static void recBranchLinkLikely(bool ltz)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllDirty();

	_deleteEEreg(31, 0);
	armAsm->Mov(RXSCRATCH, (u64)(pc + 4));
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.n.ra.UD[0]);

	if (GPR_IS_CONST1(_Rs_))
	{
		bool taken = ltz ? (g_cpuConstRegs[_Rs_].SD[0] < 0) : (g_cpuConstRegs[_Rs_].SD[0] >= 0);
		if (taken)
		{
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		else
			SetBranchImm(pc + 4);
		return;
	}

	recSetBranchL(ltz ? 1 : 0);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBranchLabel();
	LoadBranchState();
	SetBranchImm(pc);
}

void recBLTZAL()  { recBranchLink(true); }
void recBGEZAL()  { recBranchLink(false); }
void recBLTZALL() { recBranchLinkLikely(true); }
void recBGEZALL() { recBranchLinkLikely(false); }

#endif // !FORCE_INTERP_BRANCH

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
