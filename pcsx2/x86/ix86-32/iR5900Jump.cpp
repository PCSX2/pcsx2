/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


// recompiler reworked to add dynamic linking zerofrog(@gmail.com) Jan06

#include "PrecompiledHeader.h"

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

using namespace x86Emitter;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
#ifndef JUMP_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_SYS(J);
REC_SYS_DEL(JAL, 31);
REC_SYS(JR);
REC_SYS_DEL(JALR, _Rd_);

#else

////////////////////////////////////////////////////
void recJ()
{
	EE::Profiler.EmitOp(eeOpcode::J);

	// SET_FPUSTATE;
	u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	recompileNextInstruction(1);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
	else
		SetBranchImm(newpc);
}

////////////////////////////////////////////////////
void recJAL()
{
	EE::Profiler.EmitOp(eeOpcode::JAL);

	u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	_deleteEEreg(31, 0);
	if (EE_CONST_PROP)
	{
		GPR_SET_CONST(31);
		g_cpuConstRegs[31].UL[0] = pc + 4;
		g_cpuConstRegs[31].UL[1] = 0;
	}
	else
	{
		xMOV(ptr32[&cpuRegs.GPR.r[31].UL[0]], pc + 4);
		xMOV(ptr32[&cpuRegs.GPR.r[31].UL[1]], 0);
	}

	recompileNextInstruction(1);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		SetBranchImm(vtlb_V2P(newpc));
	else
		SetBranchImm(newpc);
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/

////////////////////////////////////////////////////
void recJR()
{
	EE::Profiler.EmitOp(eeOpcode::JR);

	SetBranchReg(_Rs_);
}

////////////////////////////////////////////////////
void recJALR()
{
	EE::Profiler.EmitOp(eeOpcode::JALR);

	int newpc = pc + 4;
	_allocX86reg(calleeSavedReg2d, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
	_eeMoveGPRtoR(calleeSavedReg2d, _Rs_);

	if (EmuConfig.Gamefixes.GoemonTlbHack)
	{
		xMOV(ecx, calleeSavedReg2d);
		vtlb_DynV2P();
		xMOV(calleeSavedReg2d, eax);
	}
	// uncomment when there are NO instructions that need to call interpreter
//	int mmreg;
//	if (GPR_IS_CONST1(_Rs_))
//		xMOV(ptr32[&cpuRegs.pc], g_cpuConstRegs[_Rs_].UL[0]);
//	else
//	{
//		int mmreg;
//
//		if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ)) >= 0)
//		{
//			xMOVSS(ptr[&cpuRegs.pc], xRegisterSSE(mmreg));
//		}
//		else {
//			xMOV(eax, ptr[(void*)((int)&cpuRegs.GPR.r[_Rs_].UL[0])]);
//			xMOV(ptr[&cpuRegs.pc], eax);
//		}
//	}


	if (_Rd_)
	{
		_deleteEEreg(_Rd_, 0);
		if (EE_CONST_PROP)
		{
			GPR_SET_CONST(_Rd_);
			g_cpuConstRegs[_Rd_].UL[0] = newpc;
			g_cpuConstRegs[_Rd_].UL[1] = 0;
		}
		else
		{
			xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[0]], newpc);
			xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[1]], 0);
		}
	}

	_clearNeededXMMregs();
	recompileNextInstruction(1);

	if (x86regs[calleeSavedReg2d.GetId()].inuse)
	{
		pxAssert(x86regs[calleeSavedReg2d.GetId()].type == X86TYPE_PCWRITEBACK);
		xMOV(ptr[&cpuRegs.pc], calleeSavedReg2d);
		x86regs[calleeSavedReg2d.GetId()].inuse = 0;
	}
	else
	{
		xMOV(eax, ptr[&g_recWriteback]);
		xMOV(ptr[&cpuRegs.pc], eax);
	}

	SetBranchReg(0xffffffff);
}

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
