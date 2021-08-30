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

#include "PrecompiledHeader.h"

#include "Common.h"
#include "iR5900.h"
#include "R5900OpcodeTables.h"

using namespace x86Emitter;

namespace R5900 {
namespace Dynarec {

// R5900 branch helper!
// Recompiles code for a branch test and/or skip, complete with delay slot
// handling.  Note, for "likely" branches use iDoBranchImm_Likely instead, which
// handles delay slots differently.
// Parameters:
//   jmpSkip - This parameter is the result of the appropriate J32 instruction
//   (usually JZ32 or JNZ32).
void recDoBranchImm(u32* jmpSkip, bool isLikely)
{
	// All R5900 branches use this format:
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;

	// First up is the Branch Taken Path : Save the recompiler's state, compile the
	// DelaySlot, and issue a BranchTest insertion.  The state is reloaded below for
	// the "did not branch" path (maintains consts, register allocations, and other optimizations).

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	// Jump target when the branch is *not* taken, skips the branchtest code
	// insertion above.
	x86SetJ32(jmpSkip);

	// if it's a likely branch then we'll need to skip the delay slot here, since
	// MIPS cancels the delay slot instruction when branches aren't taken.
	LoadBranchState();
	if (!isLikely)
	{
		pc -= 4; // instruction rewinder for delay slot, if non-likely.
		recompileNextInstruction(1);
	}
	SetBranchImm(pc); // start a new recompiled block.
}

void recDoBranchImm_Likely(u32* jmpSkip)
{
	recDoBranchImm(jmpSkip, true);
}

namespace OpcodeImpl {

////////////////////////////////////////////////////
//static void recCACHE() {
//	xMOV(ptr32[&cpuRegs.code], cpuRegs.code );
//	xMOV(ptr32[&cpuRegs.pc], pc );
//	iFlushCall(FLUSH_EVERYTHING);
//	xFastCall((void*)(uptr)CACHE );
//	//branch = 2;
//
//	xCMP(ptr32[(u32*)((int)&cpuRegs.pc)], pc);
//	j8Ptr[0] = JE8(0);
//	xRET();
//	x86SetJ8(j8Ptr[0]);
//}


void recPREF()
{
}

void recSYNC()
{
}

void recMFSA()
{
	int mmreg;
	if (!_Rd_)
		return;

	mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rd_, MODE_WRITE);
	if (mmreg >= 0)
	{
		xMOVL.PS(xRegisterSSE(mmreg), ptr[&cpuRegs.sa]);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.sa]);
		_deleteEEreg(_Rd_, 0);
		xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UL[0]], eax);
		xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[1]], 0);
	}
}

// SA is 4-bit and contains the amount of bytes to shift
void recMTSA()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		xMOV(ptr32[&cpuRegs.sa], g_cpuConstRegs[_Rs_].UL[0] & 0xf);
	}
	else
	{
		int mmreg;

		if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ)) >= 0)
		{
			xMOVSS(ptr[&cpuRegs.sa], xRegisterSSE(mmreg));
		}
		else
		{
			xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
			xMOV(ptr[&cpuRegs.sa], eax);
		}
		xAND(ptr32[&cpuRegs.sa], 0xf);
	}
}

void recMTSAB()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		xMOV(ptr32[&cpuRegs.sa], ((g_cpuConstRegs[_Rs_].UL[0] & 0xF) ^ (_Imm_ & 0xF)));
	}
	else
	{
		_eeMoveGPRtoR(eax, _Rs_);
		xAND(eax, 0xF);
		xXOR(eax, _Imm_ & 0xf);
		xMOV(ptr[&cpuRegs.sa], eax);
	}
}

void recMTSAH()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		xMOV(ptr32[&cpuRegs.sa], ((g_cpuConstRegs[_Rs_].UL[0] & 0x7) ^ (_Imm_ & 0x7)) << 1);
	}
	else
	{
		_eeMoveGPRtoR(eax, _Rs_);
		xAND(eax, 0x7);
		xXOR(eax, _Imm_ & 0x7);
		xSHL(eax, 1);
		xMOV(ptr[&cpuRegs.sa], eax);
	}
}

////////////////////////////////////////////////////
void recNULL()
{
	Console.Error("EE: Unimplemented op %x", cpuRegs.code);
}

////////////////////////////////////////////////////
void recUnknown()
{
	// TODO : Unknown ops should throw an exception.
	Console.Error("EE: Unrecognized op %x", cpuRegs.code);
}

void recMMI_Unknown()
{
	// TODO : Unknown ops should throw an exception.
	Console.Error("EE: Unrecognized MMI op %x", cpuRegs.code);
}

void recCOP0_Unknown()
{
	// TODO : Unknown ops should throw an exception.
	Console.Error("EE: Unrecognized COP0 op %x", cpuRegs.code);
}

void recCOP1_Unknown()
{
	// TODO : Unknown ops should throw an exception.
	Console.Error("EE: Unrecognized FPU/COP1 op %x", cpuRegs.code);
}

/**********************************************************
*    UNHANDLED YET OPCODES
*
**********************************************************/

// Suikoden 3 uses it a lot
void recCACHE() //Interpreter only!
{
	//xMOV(ptr32[&cpuRegs.code], (u32)cpuRegs.code );
	//xMOV(ptr32[&cpuRegs.pc], (u32)pc );
	//iFlushCall(FLUSH_EVERYTHING);
	//xFastCall((void*)(uptr)R5900::Interpreter::OpcodeImpl::CACHE );
	//branch = 2;
}

void recTGE()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TGE);
}

void recTGEU()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TGEU);
}

void recTLT()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TLT);
}

void recTLTU()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TLTU);
}

void recTEQ()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TEQ);
}

void recTNE()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TNE);
}

void recTGEI()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TGEI);
}

void recTGEIU()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TGEIU);
}

void recTLTI()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TLTI);
}

void recTLTIU()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TLTIU);
}

void recTEQI()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TEQI);
}

void recTNEI()
{
	recBranchCall(R5900::Interpreter::OpcodeImpl::TNEI);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
