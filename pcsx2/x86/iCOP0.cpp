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


// Important Note to Future Developers:
//   None of the COP0 instructions are really critical performance items,
//   so don't waste time converting any more them into recompiled code
//   unless it can make them nicely compact.  Calling the C versions will
//   suffice.

#include "PrecompiledHeader.h"

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "iCOP0.h"

namespace Interp = R5900::Interpreter::OpcodeImpl::COP0;
using namespace x86Emitter;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace COP0 {

/*********************************************************
*   COP0 opcodes                                         *
*                                                        *
*********************************************************/

// emits "setup" code for a COP0 branch test.  The instruction immediately following
// this should be a conditional Jump -- JZ or JNZ normally.
static void _setupBranchTest()
{
	_eeFlushAllUnused();

	// COP0 branch conditionals are based on the following equation:
	//  (((psHu16(DMAC_STAT) | ~psHu16(DMAC_PCR)) & 0x3ff) == 0x3ff)
	// BC0F checks if the statement is false, BC0T checks if the statement is true.

	// note: We only want to compare the 16 bit values of DMAC_STAT and PCR.
	// But using 32-bit loads here is ok (and faster), because we mask off
	// everything except the lower 10 bits away.

	xMOV(eax, ptr[(&psHu32(DMAC_PCR))]);
	xMOV(ecx, 0x3ff); // ECX is our 10-bit mask var
	xNOT(eax);
	xOR(eax, ptr[(&psHu32(DMAC_STAT))]);
	xAND(eax, ecx);
	xCMP(eax, ecx);
}

void recBC0F()
{
	_setupBranchTest();
	recDoBranchImm(JE32(0));
}

void recBC0T()
{
	_setupBranchTest();
	recDoBranchImm(JNE32(0));
}

void recBC0FL()
{
	_setupBranchTest();
	recDoBranchImm_Likely(JE32(0));
}

void recBC0TL()
{
	_setupBranchTest();
	recDoBranchImm_Likely(JNE32(0));
}

void recTLBR() { recCall(Interp::TLBR); }
void recTLBP() { recCall(Interp::TLBP); }
void recTLBWI() { recCall(Interp::TLBWI); }
void recTLBWR() { recCall(Interp::TLBWR); }

void recERET()
{
	recBranchCall(Interp::ERET);
}

void recEI()
{
	// must branch after enabling interrupts, so that anything
	// pending gets triggered properly.
	recBranchCall(Interp::EI);
}

void recDI()
{
	//// No need to branch after disabling interrupts...

	//iFlushCall(0);

	//xMOV(eax, ptr[&cpuRegs.cycle ]);
	//xMOV(ptr[&g_nextBranchCycle], eax);

	//xFastCall((void*)(uptr)Interp::DI );

	// Fixes booting issues in the following games:
	// Jak X, Namco 50th anniversary, Spongebob the Movie, Spongebob Battle for Bikini Bottom,
	// The Incredibles, The Incredibles rize of the underminer, Soukou kihei armodyne, Garfield Saving Arlene, Tales of Fandom Vol. 2.
	if (!g_recompilingDelaySlot)
		recompileNextInstruction(0); // DI execution is delayed by one instruction

	xMOV(eax, ptr[&cpuRegs.CP0.n.Status]);
	xTEST(eax, 0x20006); // EXL | ERL | EDI
	xForwardJNZ8 iHaveNoIdea;
	xTEST(eax, 0x18); // KSU
	xForwardJNZ8 inUserMode;
	iHaveNoIdea.SetTarget();
	xAND(eax, ~(u32)0x10000); // EIE
	xMOV(ptr[&cpuRegs.CP0.n.Status], eax);
	inUserMode.SetTarget();
}


#ifndef CP0_RECOMPILE

REC_SYS(MFC0);
REC_SYS(MTC0);

#else

void recMFC0()
{
	if (_Rd_ == 9)
	{
		// This case needs to be handled even if the write-back is ignored (_Rt_ == 0 )
		xMOV(ecx, ptr[&cpuRegs.cycle]);
		xMOV(eax, ecx);
		xSUB(eax, ptr[&s_iLastCOP0Cycle]);
		u8* skipInc = JNZ8(0);
		xINC(eax);
		x86SetJ8(skipInc);
		xADD(ptr[&cpuRegs.CP0.n.Count], eax);
		xMOV(ptr[&s_iLastCOP0Cycle], ecx);
		xMOV(eax, ptr[&cpuRegs.CP0.r[_Rd_]]);

		if (!_Rt_)
			return;

		_deleteEEreg(_Rt_, 0);
		eeSignExtendTo(_Rt_);
		return;
	}

	if (!_Rt_)
		return;

	if (_Rd_ == 25)
	{
		if (0 == (_Imm_ & 1)) // MFPS, register value ignored
		{
			xMOV(eax, ptr[&cpuRegs.PERF.n.pccr]);
		}
		else if (0 == (_Imm_ & 2)) // MFPC 0, only LSB of register matters
		{
			iFlushCall(FLUSH_INTERPRETER);
			xFastCall((void*)COP0_UpdatePCCR);
			xMOV(eax, ptr[&cpuRegs.PERF.n.pcr0]);
		}
		else // MFPC 1
		{
			iFlushCall(FLUSH_INTERPRETER);
			xFastCall((void*)COP0_UpdatePCCR);
			xMOV(eax, ptr[&cpuRegs.PERF.n.pcr1]);
		}
		_deleteEEreg(_Rt_, 0);
		eeSignExtendTo(_Rt_);

		return;
	}
	else if (_Rd_ == 24)
	{
		COP0_LOG("MFC0 Breakpoint debug Registers code = %x\n", cpuRegs.code & 0x3FF);
		return;
	}
	_eeOnWriteReg(_Rt_, 1);
	_deleteEEreg(_Rt_, 0);
	xMOV(eax, ptr[&cpuRegs.CP0.r[_Rd_]]);
	eeSignExtendTo(_Rt_);
}

void recMTC0()
{
	if (GPR_IS_CONST1(_Rt_))
	{
		switch (_Rd_)
		{
			case 12:
				iFlushCall(FLUSH_INTERPRETER);
				xFastCall((void*)WriteCP0Status, g_cpuConstRegs[_Rt_].UL[0]);
				break;

			case 16:
				iFlushCall(FLUSH_INTERPRETER);
				xFastCall((void*)WriteCP0Config, g_cpuConstRegs[_Rt_].UL[0]);
				break;

			case 9:
				xMOV(ecx, ptr[&cpuRegs.cycle]);
				xMOV(ptr[&s_iLastCOP0Cycle], ecx);
				xMOV(ptr32[&cpuRegs.CP0.r[9]], g_cpuConstRegs[_Rt_].UL[0]);
				break;

			case 25:
				if (0 == (_Imm_ & 1)) // MTPS
				{
					if (0 != (_Imm_ & 0x3E)) // only effective when the register is 0
						break;
					// Updates PCRs and sets the PCCR.
					iFlushCall(FLUSH_INTERPRETER);
					xFastCall((void*)COP0_UpdatePCCR);
					xMOV(ptr32[&cpuRegs.PERF.n.pccr], g_cpuConstRegs[_Rt_].UL[0]);
					xFastCall((void*)COP0_DiagnosticPCCR);
				}
				else if (0 == (_Imm_ & 2)) // MTPC 0, only LSB of register matters
				{
					xMOV(eax, ptr[&cpuRegs.cycle]);
					xMOV(ptr32[&cpuRegs.PERF.n.pcr0], g_cpuConstRegs[_Rt_].UL[0]);
					xMOV(ptr[&s_iLastPERFCycle[0]], eax);
				}
				else // MTPC 1
				{
					xMOV(eax, ptr[&cpuRegs.cycle]);
					xMOV(ptr32[&cpuRegs.PERF.n.pcr1], g_cpuConstRegs[_Rt_].UL[0]);
					xMOV(ptr[&s_iLastPERFCycle[1]], eax);
				}
				break;

			case 24:
				COP0_LOG("MTC0 Breakpoint debug Registers code = %x\n", cpuRegs.code & 0x3FF);
				break;

			default:
				xMOV(ptr32[&cpuRegs.CP0.r[_Rd_]], g_cpuConstRegs[_Rt_].UL[0]);
				break;
		}
	}
	else
	{
		switch (_Rd_)
		{
			case 12:
				iFlushCall(FLUSH_INTERPRETER);
				_eeMoveGPRtoR(ecx, _Rt_);
				xFastCall((void*)WriteCP0Status, ecx);
				break;

			case 16:
				iFlushCall(FLUSH_INTERPRETER);
				_eeMoveGPRtoR(ecx, _Rt_);
				xFastCall((void*)WriteCP0Config, ecx);
				break;

			case 9:
				xMOV(ecx, ptr[&cpuRegs.cycle]);
				_eeMoveGPRtoM((uptr)&cpuRegs.CP0.r[9], _Rt_);
				xMOV(ptr[&s_iLastCOP0Cycle], ecx);
				break;

			case 25:
				if (0 == (_Imm_ & 1)) // MTPS
				{
					if (0 != (_Imm_ & 0x3E)) // only effective when the register is 0
						break;
					iFlushCall(FLUSH_INTERPRETER);
					xFastCall((void*)COP0_UpdatePCCR);
					_eeMoveGPRtoM((uptr)&cpuRegs.PERF.n.pccr, _Rt_);
					xFastCall((void*)COP0_DiagnosticPCCR);
				}
				else if (0 == (_Imm_ & 2)) // MTPC 0, only LSB of register matters
				{
					xMOV(ecx, ptr[&cpuRegs.cycle]);
					_eeMoveGPRtoM((uptr)&cpuRegs.PERF.n.pcr0, _Rt_);
					xMOV(ptr[&s_iLastPERFCycle[0]], ecx);
				}
				else // MTPC 1
				{
					xMOV(ecx, ptr[&cpuRegs.cycle]);
					_eeMoveGPRtoM((uptr)&cpuRegs.PERF.n.pcr1, _Rt_);
					xMOV(ptr[&s_iLastPERFCycle[1]], ecx);
				}
				break;

			case 24:
				COP0_LOG("MTC0 Breakpoint debug Registers code = %x\n", cpuRegs.code & 0x3FF);
				break;

			default:
				_eeMoveGPRtoM((uptr)&cpuRegs.CP0.r[_Rd_], _Rt_);
				break;
		}
	}
}
#endif


/*void rec(COP0) {
}

void rec(BC0F) {
}

void rec(BC0T) {
}

void rec(BC0FL) {
}

void rec(BC0TL) {
}

void rec(TLBR) {
}

void rec(TLBWI) {
}

void rec(TLBWR) {
}

void rec(TLBP) {
}*/

} // namespace COP0
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
