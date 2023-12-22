// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "x86/iR5900.h"

using namespace x86Emitter;

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#ifndef BRANCH_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_SYS(BEQ);
REC_SYS(BEQL);
REC_SYS(BNE);
REC_SYS(BNEL);
REC_SYS(BLTZ);
REC_SYS(BGTZ);
REC_SYS(BLEZ);
REC_SYS(BGEZ);
REC_SYS(BGTZL);
REC_SYS(BLTZL);
REC_SYS_DEL(BLTZAL, 31);
REC_SYS_DEL(BLTZALL, 31);
REC_SYS(BLEZL);
REC_SYS(BGEZL);
REC_SYS_DEL(BGEZAL, 31);
REC_SYS_DEL(BGEZALL, 31);

#else

static void recSetBranchEQ(int bne, int process)
{
	// TODO(Stenzek): This is suboptimal if the registers are in XMMs.
	// If the constant register is already in a host register, we don't need the immediate...

	if (process & PROCESS_CONSTS)
	{
		_eeFlushAllDirty();

		_deleteGPRtoXMMreg(_Rt_, DELETE_REG_FLUSH_AND_FREE);
		const int regt = _checkX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
		if (regt >= 0)
			xImm64Op(xCMP, xRegister64(regt), rax, g_cpuConstRegs[_Rs_].UD[0]);
		else
			xImm64Op(xCMP, ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], rax, g_cpuConstRegs[_Rs_].UD[0]);
	}
	else if (process & PROCESS_CONSTT)
	{
		_eeFlushAllDirty();

		_deleteGPRtoXMMreg(_Rs_, DELETE_REG_FLUSH_AND_FREE);
		const int regs = _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
		if (regs >= 0)
			xImm64Op(xCMP, xRegister64(regs), rax, g_cpuConstRegs[_Rt_].UD[0]);
		else
			xImm64Op(xCMP, ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], rax, g_cpuConstRegs[_Rt_].UD[0]);
	}
	else
	{
		// force S into register, since we need to load it, may as well cache.
		_deleteGPRtoXMMreg(_Rt_, DELETE_REG_FLUSH_AND_FREE);
		const int regs = _allocX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
		const int regt = _checkX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
		_eeFlushAllDirty();

		if (regt >= 0)
			xCMP(xRegister64(regs), xRegister64(regt));
		else
			xCMP(xRegister64(regs), ptr64[&cpuRegs.GPR.r[_Rt_]]);
	}

	if (bne)
		j32Ptr[0] = JE32(0);
	else
		j32Ptr[0] = JNE32(0);
}

static void recSetBranchL(int ltz)
{
	const int regs = _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
	const int regsxmm = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ);
	_eeFlushAllDirty();

	if (regsxmm >= 0)
	{
		xMOVMSKPS(eax, xRegisterSSE(regsxmm));
		xTEST(al, 2);

		if (ltz)
			j32Ptr[0] = JZ32(0);
		else
			j32Ptr[0] = JNZ32(0);

		return;
	}

	if (regs >= 0)
		xCMP(xRegister64(regs), 0);
	else
		xCMP(ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], 0);

	if (ltz)
		j32Ptr[0] = JGE32(0);
	else
		j32Ptr[0] = JL32(0);
}

//// BEQ
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
	}
	else
	{
		const bool swap = TrySwapDelaySlot(_Rs_, _Rt_, 0, true);

		recSetBranchEQ(0, process);

		if (!swap)
		{
			SaveBranchState();
			recompileNextInstruction(true, false);
		}

		SetBranchImm(branchTo);

		x86SetJ32(j32Ptr[0]);

		if (!swap)
		{
			// recopy the next inst
			pc -= 4;
			LoadBranchState();
			recompileNextInstruction(true, false);
		}

		SetBranchImm(pc);
	}
}

void recBEQ()
{
	// prefer using the host register over an immediate, it'll be smaller code.
	if (GPR_IS_CONST2(_Rs_, _Rt_))
		recBEQ_const();
	else if (GPR_IS_CONST1(_Rs_) && _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ) < 0)
		recBEQ_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_) && _checkX86reg(X86TYPE_GPR, _Rt_, MODE_READ) < 0)
		recBEQ_process(PROCESS_CONSTT);
	else
		recBEQ_process(0);
}

//// BNE
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

	x86SetJ32(j32Ptr[0]);

	if (!swap)
	{
		// recopy the next inst
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
	else if (GPR_IS_CONST1(_Rs_) && _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ) < 0)
		recBNE_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_) && _checkX86reg(X86TYPE_GPR, _Rt_, MODE_READ) < 0)
		recBNE_process(PROCESS_CONSTT);
	else
		recBNE_process(0);
}

//// BEQL
static void recBEQL_const()
{
	if (g_cpuConstRegs[_Rs_].SD[0] == g_cpuConstRegs[_Rt_].SD[0])
	{
		u32 branchTo = ((s32)_Imm_ * 4) + pc;
		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
	}
	else
	{
		SetBranchImm(pc + 4);
	}
}

static void recBEQL_process(int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;
	recSetBranchEQ(0, process);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}

void recBEQL()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
		recBEQL_const();
	else if (GPR_IS_CONST1(_Rs_) && _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ) < 0)
		recBEQL_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_) && _checkX86reg(X86TYPE_GPR, _Rt_, MODE_READ) < 0)
		recBEQL_process(PROCESS_CONSTT);
	else
		recBEQL_process(0);
}

//// BNEL
static void recBNEL_const()
{
	if (g_cpuConstRegs[_Rs_].SD[0] != g_cpuConstRegs[_Rt_].SD[0])
	{
		u32 branchTo = ((s32)_Imm_ * 4) + pc;
		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
	}
	else
	{
		SetBranchImm(pc + 4);
	}
}

static void recBNEL_process(int process)
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	recSetBranchEQ(0, process);

	SaveBranchState();
	SetBranchImm(pc + 4);

	x86SetJ32(j32Ptr[0]);

	// recopy the next inst
	LoadBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);
}

void recBNEL()
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
		recBNEL_const();
	else if (GPR_IS_CONST1(_Rs_) && _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ) < 0)
		recBNEL_process(PROCESS_CONSTS);
	else if (GPR_IS_CONST1(_Rt_) && _checkX86reg(X86TYPE_GPR, _Rt_, MODE_READ) < 0)
		recBNEL_process(PROCESS_CONSTT);
	else
		recBNEL_process(0);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

////////////////////////////////////////////////////
//void recBLTZAL()
//{
//	Console.WriteLn("BLTZAL");
//	_eeFlushAllUnused();
//	xMOV(ptr32[(u32*)((int)&cpuRegs.code)], cpuRegs.code );
//	xMOV(ptr32[(u32*)((int)&cpuRegs.pc)], pc );
//	iFlushCall(FLUSH_EVERYTHING);
//	xFastCall((void*)(int)BLTZAL );
//	branch = 2;
//}

////////////////////////////////////////////////////
void recBLTZAL()
{
	EE::Profiler.EmitOp(eeOpcode::BLTZAL);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllDirty();

	_deleteEEreg(31, 0);
	xMOV64(rax, pc + 4);
	xMOV(ptr64[&cpuRegs.GPR.n.ra.UD[0]], rax);

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] < 0))
			branchTo = pc + 4;

		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);

	recSetBranchL(1);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	if (!swap)
	{
		// recopy the next inst
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGEZAL()
{
	EE::Profiler.EmitOp(eeOpcode::BGEZAL);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllDirty();

	_deleteEEreg(31, 0);
	xMOV64(rax, pc + 4);
	xMOV(ptr64[&cpuRegs.GPR.n.ra.UD[0]], rax);

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] >= 0))
			branchTo = pc + 4;

		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);

	recSetBranchL(0);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	if (!swap)
	{
		// recopy the next inst
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBLTZALL()
{
	EE::Profiler.EmitOp(eeOpcode::BLTZALL);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllDirty();

	_deleteEEreg(31, 0);
	xMOV64(rax, pc + 4);
	xMOV(ptr64[&cpuRegs.GPR.n.ra.UD[0]], rax);

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] < 0))
			SetBranchImm(pc + 4);
		else
		{
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		return;
	}

	recSetBranchL(1);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGEZALL()
{
	EE::Profiler.EmitOp(eeOpcode::BGEZALL);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	_eeOnWriteReg(31, 0);
	_eeFlushAllDirty();

	_deleteEEreg(31, 0);
	xMOV64(rax, pc + 4);
	xMOV(ptr64[&cpuRegs.GPR.n.ra.UD[0]], rax);

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] >= 0))
			SetBranchImm(pc + 4);
		else
		{
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		return;
	}

	recSetBranchL(0);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}


//// BLEZ
void recBLEZ()
{
	EE::Profiler.EmitOp(eeOpcode::BLEZ);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] <= 0))
			branchTo = pc + 4;

		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);
	const int regs = _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
	_eeFlushAllDirty();

	if (regs >= 0)
		xCMP(xRegister64(regs), 0);
	else
		xCMP(ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], 0);

	j32Ptr[0] = JG32(0);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	if (!swap)
	{
		// recopy the next inst
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(pc);
}

//// BGTZ
void recBGTZ()
{
	EE::Profiler.EmitOp(eeOpcode::BGTZ);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] > 0))
			branchTo = pc + 4;

		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);
	const int regs = _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
	_eeFlushAllDirty();

	if (regs >= 0)
		xCMP(xRegister64(regs), 0);
	else
		xCMP(ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], 0);

	j32Ptr[0] = JLE32(0);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	if (!swap)
	{
		// recopy the next inst
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBLTZ()
{
	EE::Profiler.EmitOp(eeOpcode::BLTZ);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] < 0))
			branchTo = pc + 4;

		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);
	_eeFlushAllDirty();
	recSetBranchL(1);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	if (!swap)
	{
		// recopy the next inst
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGEZ()
{
	EE::Profiler.EmitOp(eeOpcode::BGEZ);

	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] >= 0))
			branchTo = pc + 4;

		recompileNextInstruction(true, false);
		SetBranchImm(branchTo);
		return;
	}

	const bool swap = TrySwapDelaySlot(_Rs_, 0, 0, true);
	_eeFlushAllDirty();

	recSetBranchL(0);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	if (!swap)
	{
		// recopy the next inst
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}

	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBLTZL()
{
	EE::Profiler.EmitOp(eeOpcode::BLTZL);

	const u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] < 0))
			SetBranchImm(pc + 4);
		else
		{
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		return;
	}

	_eeFlushAllDirty();
	recSetBranchL(1);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}


////////////////////////////////////////////////////
void recBGEZL()
{
	EE::Profiler.EmitOp(eeOpcode::BGEZL);

	const u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] >= 0))
			SetBranchImm(pc + 4);
		else
		{
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		return;
	}

	_eeFlushAllDirty();
	recSetBranchL(0);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}



/*********************************************************
* Register branch logic  Likely                          *
* Format:  OP rs, offset                                 *
*********************************************************/

////////////////////////////////////////////////////
void recBLEZL()
{
	EE::Profiler.EmitOp(eeOpcode::BLEZL);

	const u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] <= 0))
			SetBranchImm(pc + 4);
		else
		{
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		return;
	}

	const int regs = _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
	_eeFlushAllDirty();

	if (regs >= 0)
		xCMP(xRegister64(regs), 0);
	else
		xCMP(ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], 0);

	j32Ptr[0] = JG32(0);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBGTZL()
{
	EE::Profiler.EmitOp(eeOpcode::BGTZL);

	const u32 branchTo = ((s32)_Imm_ * 4) + pc;

	if (GPR_IS_CONST1(_Rs_))
	{
		if (!(g_cpuConstRegs[_Rs_].SD[0] > 0))
			SetBranchImm(pc + 4);
		else
		{
			_clearNeededXMMregs();
			recompileNextInstruction(true, false);
			SetBranchImm(branchTo);
		}
		return;
	}

	const int regs = _checkX86reg(X86TYPE_GPR, _Rs_, MODE_READ);
	_eeFlushAllDirty();

	if (regs >= 0)
		xCMP(xRegister64(regs), 0);
	else
		xCMP(ptr64[&cpuRegs.GPR.r[_Rs_].UD[0]], 0);

	j32Ptr[0] = JLE32(0);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}

#endif

} // namespace R5900::Dynarec::OpcodeImpl
