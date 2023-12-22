// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"

#include "R5900OpcodeTables.h"
#include "VUmicro.h"

using namespace R5900;
using namespace R5900::Interpreter;
#define CP2COND (((VU0.VI[REG_VPU_STAT].US[0] >> 8) & 1))
//#define CP2COND (vif1Regs.stat.VEW)

//Run the FINISH either side of the VCALL's as we have no control over it past here.
void VCALLMS() {
	_vu0FinishMicro();
	vu0ExecMicro(((cpuRegs.code >> 6) & 0x7FFF));
	//vif0Regs.stat.VEW = false;
}

void VCALLMSR() {
	_vu0FinishMicro();
	vu0ExecMicro(VU0.VI[REG_CMSAR0].US[0]);
	//vif0Regs.stat.VEW = false;
}

void BC2F()
{
	if (CP2COND == 0)
	{
		Console.WriteLn("VU0 Macro Branch");
		intDoBranch(_BranchTarget_);
	}
}
void BC2T()
{
	if (CP2COND == 1)
	{
		Console.WriteLn("VU0 Macro Branch");
		intDoBranch(_BranchTarget_);
	}
}

void BC2FL()
{
	if (CP2COND == 0)
	{
		Console.WriteLn("VU0 Macro Branch");
		intDoBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc+= 4;
	}
}
void BC2TL()
{
	if (CP2COND == 1)
	{
		Console.WriteLn("VU0 Macro Branch");
		intDoBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc+= 4;
	}
}
