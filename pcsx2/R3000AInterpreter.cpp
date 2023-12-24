// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "R3000A.h"
#include "Common.h"
#include "Config.h"
#include "VMManager.h"

#include "R5900OpcodeTables.h"
#include "DebugTools/Breakpoints.h"
#include "IopBios.h"
#include "IopHw.h"

using namespace R3000A;

// Used to flag delay slot instructions when throwig exceptions.
bool iopIsDelaySlot = false;

static bool branch2 = 0;
static u32 branchPC;

static void doBranch(s32 tar);	// forward declared prototype

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

void psxBGEZ()         // Branch if Rs >= 0
{
	if (_i32(_rRs_) >= 0) doBranch(_BranchTarget_);
}

void psxBGEZAL()   // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if (_i32(_rRs_) >= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void psxBGTZ()          // Branch if Rs >  0
{
	if (_i32(_rRs_) > 0) doBranch(_BranchTarget_);
}

void psxBLEZ()         // Branch if Rs <= 0
{
	if (_i32(_rRs_) <= 0) doBranch(_BranchTarget_);
}
void psxBLTZ()          // Branch if Rs <  0
{
	if (_i32(_rRs_) < 0) doBranch(_BranchTarget_);
}

void psxBLTZAL()    // Branch if Rs <  0 and link
{
	_SetLink(31);
	if (_i32(_rRs_) < 0)
		{
			doBranch(_BranchTarget_);
		}
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/

void psxBEQ()   // Branch if Rs == Rt
{
	if (_i32(_rRs_) == _i32(_rRt_)) doBranch(_BranchTarget_);
}

void psxBNE()   // Branch if Rs != Rt
{
	if (_i32(_rRs_) != _i32(_rRt_)) doBranch(_BranchTarget_);
}

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
void psxJ()
{
	// check for iop module import table magic
	u32 delayslot = iopMemRead32(psxRegs.pc);
	if (delayslot >> 16 == 0x2400 && irxImportExec(irxImportTableAddr(psxRegs.pc), delayslot & 0xffff))
		return;

	doBranch(_JumpTarget_);
}

void psxJAL()
{
	_SetLink(31);
	doBranch(_JumpTarget_);
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void psxJR()
{
	doBranch(_u32(_rRs_));
}

void psxJALR()
{
	if (_Rd_)
	{
		_SetLink(_Rd_);
	}
	doBranch(_u32(_rRs_));
}

void psxBreakpoint(bool memcheck)
{
	u32 pc = psxRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_IOP, pc) != 0)
		return;

	if (!memcheck)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_IOP, pc);
		if (cond && !cond->Evaluate())
			return;
	}

	CBreakPoints::SetBreakpointTriggered(true, BREAKPOINT_IOP);
	VMManager::SetPaused(true);
	Cpu->ExitExecution();
}

void psxMemcheck(u32 op, u32 bits, bool store)
{
	// compute accessed address
	u32 start = psxRegs.GPR.r[(op >> 21) & 0x1F];
	if ((s16)op != 0)
		start += (s16)op;

	u32 end = start + bits / 8;

	auto checks = CBreakPoints::GetMemChecks(BREAKPOINT_IOP);
	for (size_t i = 0; i < checks.size(); i++)
	{
		auto& check = checks[i];

		if (check.result == 0)
			continue;
		if ((check.cond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((check.cond & MEMCHECK_READ) == 0 && !store)
			continue;

		if (start < check.end && check.start < end)
			psxBreakpoint(true);
	}
}

void psxCheckMemcheck()
{
	u32 pc = psxRegs.pc;
	int needed = psxIsMemcheckNeeded(pc);
	if (needed == 0)
		return;

	u32 op = iopMemRead32(needed == 2 ? pc + 4 : pc);
	// Yeah, we use the R5900 opcode table for the R3000
	const R5900::OPCODE& opcode = R5900::GetInstruction(op);

	bool store = (opcode.flags & IS_STORE) != 0;
	switch (opcode.flags & MEMTYPE_MASK)
	{
	case MEMTYPE_BYTE:
		psxMemcheck(op, 8, store);
		break;
	case MEMTYPE_HALF:
		psxMemcheck(op, 16, store);
		break;
	case MEMTYPE_WORD:
		psxMemcheck(op, 32, store);
		break;
	case MEMTYPE_DWORD:
		psxMemcheck(op, 64, store);
		break;
	}
}

///////////////////////////////////////////
// These macros are used to assemble the repassembler functions

static __fi void execI()
{
	// This function is called for every instruction.
	// Enabling the define below will probably, no, will cause the interpretor to be slower.
//#define EXTRA_DEBUG
#if defined(EXTRA_DEBUG) || defined(PCSX2_DEVBUILD)
	if (psxIsBreakpointNeeded(psxRegs.pc))
		psxBreakpoint(false);

	psxCheckMemcheck();
#endif

	// Inject IRX hack
	if (psxRegs.pc == 0x1630 && EmuConfig.CurrentIRX.length() > 3) {
		if (iopMemRead32(0x20018) == 0x1F) {
			// FIXME do I need to increase the module count (0x1F -> 0x20)
			iopMemWrite32(0x20094, 0xbffc0000);
		}
	}

	psxRegs.code = iopMemRead32(psxRegs.pc);

		PSXCPU_LOG("%s", disR3000AF(psxRegs.code, psxRegs.pc));

	psxRegs.pc+= 4;
	psxRegs.cycle++;

	if ((psxHu32(HW_ICFG) & (1 << 3)))
	{
		//One of the Iop to EE delta clocks to be set in PS1 mode.
		psxRegs.iopCycleEE -= 9;
	}
	else
	{   //default ps2 mode value
		psxRegs.iopCycleEE -= 8;
	}
	psxBSC[psxRegs.code >> 26]();
}

static void doBranch(s32 tar) {
	if (tar == 0x0)
		DevCon.Warning("[R3000 Interpreter] Warning: Branch to 0x0!");

	// When upgrading the IOP, there are two resets, the second of which is a 'fake' reset
	// This second 'reset' involves UDNL calling SYSMEM and LOADCORE directly, resetting LOADCORE's modules
	// This detects when SYSMEM is called and clears the modules then
	if(tar == 0x890)
	{
		DevCon.WriteLn(Color_Gray, "[R3000 Debugger] Branch to 0x890 (SYSMEM). Clearing modules.");
		R3000SymbolMap.ClearModules();
	}

	branch2 = iopIsDelaySlot = true;
	branchPC = tar;
	execI();
	PSXCPU_LOG( "\n" );
	iopIsDelaySlot = false;
	psxRegs.pc = branchPC;

	iopEventTest();
}

static void intReserve() {
}

static void intAlloc() {
}

static void intReset() {
	intAlloc();
}

static s32 intExecuteBlock( s32 eeCycles )
{
	psxRegs.iopBreak = 0;
	psxRegs.iopCycleEE = eeCycles;

	while (psxRegs.iopCycleEE > 0)
	{
		if ((psxHu32(HW_ICFG) & 8) && ((psxRegs.pc & 0x1fffffffU) == 0xa0 || (psxRegs.pc & 0x1fffffffU) == 0xb0 || (psxRegs.pc & 0x1fffffffU) == 0xc0))
			psxBiosCall();

		branch2 = 0;
		while (!branch2)
			execI();
	}

	return psxRegs.iopBreak + psxRegs.iopCycleEE;
}

static void intClear(u32 Addr, u32 Size) {
}

static void intShutdown() {
}

R3000Acpu psxInt = {
	intReserve,
	intReset,
	intExecuteBlock,
	intClear,
	intShutdown
};
