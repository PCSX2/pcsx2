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


/*********************************************************
*   cached MMI opcodes                                   *
*                                                        *
*********************************************************/

#include "PrecompiledHeader.h"

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "iMMI.h"
#include "common/MathUtils.h"

using namespace x86Emitter;

namespace Interp = R5900::Interpreter::OpcodeImpl::MMI;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace MMI {

#ifndef MMI_RECOMPILE

REC_FUNC_DEL(PLZCW, _Rd_);

REC_FUNC_DEL(PMFHL, _Rd_);
REC_FUNC_DEL(PMTHL, _Rd_);

REC_FUNC_DEL(PSRLW, _Rd_);
REC_FUNC_DEL(PSRLH, _Rd_);

REC_FUNC_DEL(PSRAH, _Rd_);
REC_FUNC_DEL(PSRAW, _Rd_);

REC_FUNC_DEL(PSLLH, _Rd_);
REC_FUNC_DEL(PSLLW, _Rd_);

#else

void recPLZCW()
{
	int regs = -1;

	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PLZCW);

	if (GPR_IS_CONST1(_Rs_))
	{
		_eeOnWriteReg(_Rd_, 0);
		_deleteEEreg(_Rd_, 0);
		GPR_SET_CONST(_Rd_);

		// Return the leading sign bits, excluding the original bit
		g_cpuConstRegs[_Rd_].UL[0] = count_leading_sign_bits(g_cpuConstRegs[_Rs_].SL[0]) - 1;
		g_cpuConstRegs[_Rd_].UL[1] = count_leading_sign_bits(g_cpuConstRegs[_Rs_].SL[1]) - 1;

		return;
	}

	_eeOnWriteReg(_Rd_, 0);

	if ((regs = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ)) >= 0)
	{
		xMOVD(eax, xRegisterSSE(regs));
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	}

	_deleteEEreg(_Rd_, 0);

	// Count the number of leading bits (MSB) that match the sign bit, excluding the sign
	// bit itself.

	// Strategy: If the sign bit is set, then negate the value.  And that way the same
	// bitcompare can be used for either bit status.  but be warned!  BSR returns undefined
	// results if the EAX is zero, so we need to have special checks for zeros before
	// using it.

	// --- first word ---

	xMOV(ecx, 31);
	xTEST(eax, eax); // TEST sets the sign flag accordingly.
	u8* label_notSigned = JNS8(0);
	xNOT(eax);
	x86SetJ8(label_notSigned);

	xBSR(eax, eax);
	u8* label_Zeroed = JZ8(0); // If BSR sets the ZF, eax is "trash"
	xSUB(ecx, eax);
	xDEC(ecx); // PS2 doesn't count the first bit

	x86SetJ8(label_Zeroed);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UL[0]], ecx);

	// second word

	if (regs >= 0)
	{
		xPSHUF.D(xRegisterSSE(regs & 0xf), xRegisterSSE(regs & 0xf), 0xe1);
		xMOVD(eax, xRegisterSSE(regs & 0xf));
		xPSHUF.D(xRegisterSSE(regs & 0xf), xRegisterSSE(regs & 0xf), 0xe1);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[1]]);
	}

	xMOV(ecx, 31);
	xTEST(eax, eax); // TEST sets the sign flag accordingly.
	label_notSigned = JNS8(0);
	xNOT(eax);
	x86SetJ8(label_notSigned);

	xBSR(eax, eax);
	label_Zeroed = JZ8(0); // If BSR sets the ZF, eax is "trash"
	xSUB(ecx, eax);
	xDEC(ecx); // PS2 doesn't count the first bit

	x86SetJ8(label_Zeroed);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UL[1]], ecx);

	GPR_DEL_CONST(_Rd_);
}

void recPMFHL()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMFHL);

	int info = eeRecompileCodeXMM(XMMINFO_WRITED | XMMINFO_READLO | XMMINFO_READHI);

	int t0reg;

	switch (_Sa_)
	{
		case 0x00: // LW

			t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI), 0x88);
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO), 0x88);
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

			_freeXMMreg(t0reg);
			break;

		case 0x01: // UW
			t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI), 0xdd);
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO), 0xdd);
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
			break;

		case 0x02: // SLW
			// fall to interp
			_deleteEEreg(_Rd_, 0);
			iFlushCall(FLUSH_INTERPRETER); // since calling CALLFunc
			xFastCall((void*)(uptr)R5900::Interpreter::OpcodeImpl::MMI::PMFHL);
			break;

		case 0x03: // LH
			t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xPSHUF.LW(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI), 0x88);
			xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO), 0x88);
			xPSHUF.HW(xRegisterSSE(t0reg), xRegisterSSE(t0reg), 0x88);
			xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0x88);
			xPSRL.DQ(xRegisterSSE(t0reg), 4);
			xPSRL.DQ(xRegisterSSE(EEREC_D), 4);
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
			break;

		case 0x04: // SH
			if (EEREC_D == EEREC_HI)
			{
				xPACK.SSDW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO));
				xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0x72);
			}
			else
			{
				xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO));
				xPACK.SSDW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_HI));

				// shuffle so a1a0b1b0->a1b1a0b0
				xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0xd8);
			}
			break;
		default:
			Console.Error("PMFHL??  *pcsx2 head esplode!*");
			pxFail("PMFHL??  *pcsx2 head esplode!*");
	}

	_clearNeededXMMregs();
}

void recPMTHL()
{
	if (_Sa_ != 0)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMTHL);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READLO | XMMINFO_READHI | XMMINFO_WRITELO | XMMINFO_WRITEHI);

	xBLEND.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_S), 0x5);
	xSHUF.PS(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S), 0xdd);
	xSHUF.PS(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0x72);

	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSRLH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSRLH);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	if ((_Sa_ & 0xf) == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.W(xRegisterSSE(EEREC_D), _Sa_ & 0xf);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSRLW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSRLW);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	if (_Sa_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.D(xRegisterSSE(EEREC_D), _Sa_);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSRAH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSRAH);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	if ((_Sa_ & 0xf) == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRA.W(xRegisterSSE(EEREC_D), _Sa_ & 0xf);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSRAW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSRAW);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	if (_Sa_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRA.D(xRegisterSSE(EEREC_D), _Sa_);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSLLH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSLLH);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	if ((_Sa_ & 0xf) == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSLL.W(xRegisterSSE(EEREC_D), _Sa_ & 0xf);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSLLW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSLLW);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	if (_Sa_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSLL.D(xRegisterSSE(EEREC_D), _Sa_);
	}
	_clearNeededXMMregs();
}

/*
void recMADD()
{
}

void recMADDU()
{
}

void recPLZCW()
{
}
*/

#endif

/*********************************************************
*   MMI0 opcodes                                         *
*                                                        *
*********************************************************/
#ifndef MMI0_RECOMPILE

REC_FUNC_DEL(PADDB,  _Rd_);
REC_FUNC_DEL(PADDH,  _Rd_);
REC_FUNC_DEL(PADDW,  _Rd_);
REC_FUNC_DEL(PADDSB, _Rd_);
REC_FUNC_DEL(PADDSH, _Rd_);
REC_FUNC_DEL(PADDSW, _Rd_);
REC_FUNC_DEL(PSUBB,  _Rd_);
REC_FUNC_DEL(PSUBH,  _Rd_);
REC_FUNC_DEL(PSUBW,  _Rd_);
REC_FUNC_DEL(PSUBSB, _Rd_);
REC_FUNC_DEL(PSUBSH, _Rd_);
REC_FUNC_DEL(PSUBSW, _Rd_);

REC_FUNC_DEL(PMAXW,  _Rd_);
REC_FUNC_DEL(PMAXH,  _Rd_);

REC_FUNC_DEL(PCGTW,  _Rd_);
REC_FUNC_DEL(PCGTH,  _Rd_);
REC_FUNC_DEL(PCGTB,  _Rd_);

REC_FUNC_DEL(PEXTLW, _Rd_);

REC_FUNC_DEL(PPACW,  _Rd_);
REC_FUNC_DEL(PEXTLH, _Rd_);
REC_FUNC_DEL(PPACH,  _Rd_);
REC_FUNC_DEL(PEXTLB, _Rd_);
REC_FUNC_DEL(PPACB,  _Rd_);
REC_FUNC_DEL(PEXT5,  _Rd_);
REC_FUNC_DEL(PPAC5,  _Rd_);

#else

////////////////////////////////////////////////////
void recPMAXW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMAXW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_S == EEREC_T)
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else if (EEREC_D == EEREC_S)
		xPMAX.SD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPMAX.SD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPMAX.SD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPPACW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PPACW);

	int info = eeRecompileCodeXMM(((_Rs_ != 0) ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);

	if (_Rs_ == 0)
	{
		xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x88);
		xPSRL.DQ(xRegisterSSE(EEREC_D), 8);
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		if (EEREC_D == EEREC_T)
		{
			xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S), 0x88);
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x88);
			xPUNPCK.LQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T), 0x88);
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S), 0x88);
			xPUNPCK.LQDQ(xRegisterSSE(t0reg), xRegisterSSE(EEREC_D));

			// swap mmx regs.. don't ask
			xmmregs[t0reg] = xmmregs[EEREC_D];
			xmmregs[EEREC_D].inuse = 0;
		}
	}

	_clearNeededXMMregs();
}

void recPPACH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PPACH);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x88);
		xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0x88);
		xPSLL.DQ(xRegisterSSE(EEREC_D), 4);
		xPSRL.DQ(xRegisterSSE(EEREC_D), 8);
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xPSHUF.LW(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S), 0x88);
		xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x88);
		xPSHUF.HW(xRegisterSSE(t0reg), xRegisterSSE(t0reg), 0x88);
		xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0x88);

		xPSRL.DQ(xRegisterSSE(t0reg), 4);
		xPSRL.DQ(xRegisterSSE(EEREC_D), 4);
		xPUNPCK.LQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

		_freeXMMreg(t0reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPPACB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PPACB);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		if (_hasFreeXMMreg())
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPSLL.W(xRegisterSSE(EEREC_D), 8);
			xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
			xPSRL.W(xRegisterSSE(EEREC_D), 8);
			xPACK.USWB(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPSLL.W(xRegisterSSE(EEREC_D), 8);
			xPSRL.W(xRegisterSSE(EEREC_D), 8);
			xPACK.USWB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
			xPSRL.DQ(xRegisterSSE(EEREC_D), 8);
		}
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);

		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSLL.W(xRegisterSSE(t0reg), 8);
		xPSLL.W(xRegisterSSE(EEREC_D), 8);
		xPSRL.W(xRegisterSSE(t0reg), 8);
		xPSRL.W(xRegisterSSE(EEREC_D), 8);

		xPACK.USWB(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPEXT5()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXT5);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	int t1reg = _allocTempXMMreg(XMMT_INT, -1);

	xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T)); // for bit 5..9
	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T)); // for bit 15

	xPSLL.D(xRegisterSSE(t0reg), 22);
	xPSRL.W(xRegisterSSE(t1reg), 15);
	xPSRL.D(xRegisterSSE(t0reg), 27);
	xPSLL.D(xRegisterSSE(t1reg), 20);
	xPOR(xRegisterSSE(t0reg), xRegisterSSE(t1reg));

	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T)); // for bit 10..14
	xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T)); // for bit 0..4

	xPSLL.D(xRegisterSSE(EEREC_D), 27);
	xPSLL.D(xRegisterSSE(t1reg), 17);
	xPSRL.D(xRegisterSSE(EEREC_D), 27);
	xPSRL.W(xRegisterSSE(t1reg), 11);
	xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));

	xPSLL.W(xRegisterSSE(EEREC_D), 3);
	xPSLL.W(xRegisterSSE(t0reg), 11);
	xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPPAC5()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PPAC5);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	int t1reg = _allocTempXMMreg(XMMT_INT, -1);

	xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T)); // for bit 10..14
	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T)); // for bit 15

	xPSLL.D(xRegisterSSE(t0reg), 8);
	xPSRL.D(xRegisterSSE(t1reg), 31);
	xPSRL.D(xRegisterSSE(t0reg), 17);
	xPSLL.D(xRegisterSSE(t1reg), 15);
	xPOR(xRegisterSSE(t0reg), xRegisterSSE(t1reg));

	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T)); // for bit 5..9
	xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T)); // for bit 0..4

	xPSLL.D(xRegisterSSE(EEREC_D), 24);
	xPSRL.D(xRegisterSSE(t1reg), 11);
	xPSRL.D(xRegisterSSE(EEREC_D), 27);
	xPSLL.D(xRegisterSSE(t1reg), 5);
	xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));

	xPCMP.EQD(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
	xPSRL.D(xRegisterSSE(t1reg), 22);
	xPAND(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
	xPANDN(xRegisterSSE(t1reg), xRegisterSSE(t0reg));
	xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMAXH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMAXH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPMAX.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPMAX.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPMAX.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCGTB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCGTB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D != EEREC_T)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.GTB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.GTB(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCGTH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCGTH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D != EEREC_T)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.GTW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.GTW(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCGTW()
{
	//TODO:optimize RS | RT== 0
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCGTW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D != EEREC_T)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.GTD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.GTD(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDSB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDSB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPADD.SB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPADD.SB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPADD.SB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDSH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDSH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPADD.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPADD.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPADD.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
//NOTE: check kh2 movies if changing this
void recPADDSW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDSW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	int t1reg = _allocTempXMMreg(XMMT_INT, -1);
	int t2reg = _allocTempXMMreg(XMMT_INT, -1);

	// The idea is:
	//  s = x + y; (wrap-arounded)
	//  if Sign(x) == Sign(y) && Sign(s) != Sign(x) && Sign(x) == 0 then positive overflow (clamp with 0x7fffffff)
	//  if Sign(x) == Sign(y) && Sign(s) != Sign(x) && Sign(x) == 1 then negative overflow (clamp with 0x80000000)

	// get sign bit
	xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));

	// normal addition
	if (EEREC_D == EEREC_S)
		xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}

	xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); // Sign(Rs) != Sign(Rt)
	xPXOR(xRegisterSSE(t1reg), xRegisterSSE(EEREC_D)); // Sign(Rs) != Sign(Rd)
	xPANDN(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); // (Sign(Rs) == Sign(Rt)) & (Sign(Rs) != Sign(Rd))
	xPSRA.D(xRegisterSSE(t0reg), 31);

	xPCMP.EQD(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
	xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); // could've been avoided if Intel wasn't too prudish for a PORN instruction
	xPSLL.D(xRegisterSSE(t1reg), 31); // 0x80000000

	xMOVDQA(xRegisterSSE(t2reg), xRegisterSSE(EEREC_D));
	xPSRA.D(xRegisterSSE(t2reg), 31);
	xPXOR(xRegisterSSE(t1reg), xRegisterSSE(t2reg)); // t2reg = (Rd < 0) ? 0x7fffffff : 0x80000000

	xPAND(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
	xPANDN(xRegisterSSE(t0reg), xRegisterSSE(t1reg));
	xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	_freeXMMreg(t2reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBSB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBSB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPSUB.SB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.SB(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.SB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBSH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBSH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPSUB.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.SW(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
//NOTE: check kh2 movies if changing this
void recPSUBSW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBSW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	int t1reg = _allocTempXMMreg(XMMT_INT, -1);
	int t2reg = _allocTempXMMreg(XMMT_INT, -1);

	// The idea is:
	//  s = x - y; (wrap-arounded)
	//  if Sign(x) != Sign(y) && Sign(s) != Sign(x) && Sign(x) == 0 then positive overflow (clamp with 0x7fffffff)
	//  if Sign(x) != Sign(y) && Sign(s) != Sign(x) && Sign(x) == 1 then negative overflow (clamp with 0x80000000)

	// get sign bit
	xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
	xPSRL.D(xRegisterSSE(t0reg), 31);
	xPSRL.D(xRegisterSSE(t1reg), 31);

	// normal subtraction
	if (EEREC_D == EEREC_S)
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		xMOVDQA(xRegisterSSE(t2reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(t2reg));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}

	// overflow check
	// t2reg = 0xffffffff if NOT overflow, else 0
	xMOVDQA(xRegisterSSE(t2reg), xRegisterSSE(EEREC_D));
	xPSRL.D(xRegisterSSE(t2reg), 31);
	xPCMP.EQD(xRegisterSSE(t1reg), xRegisterSSE(t0reg)); // Sign(Rs) == Sign(Rt)
	xPCMP.EQD(xRegisterSSE(t2reg), xRegisterSSE(t0reg)); // Sign(Rs) == Sign(Rd)
	xPOR(xRegisterSSE(t2reg), xRegisterSSE(t1reg)); // (Sign(Rs) == Sign(Rt)) | (Sign(Rs) == Sign(Rd))
	xPCMP.EQD(xRegisterSSE(t1reg), xRegisterSSE(t1reg));
	xPSRL.D(xRegisterSSE(t1reg), 1); // 0x7fffffff
	xPADD.D(xRegisterSSE(t1reg), xRegisterSSE(t0reg)); // t1reg = (Rs < 0) ? 0x80000000 : 0x7fffffff

	// saturation
	xPAND(xRegisterSSE(EEREC_D), xRegisterSSE(t2reg));
	xPANDN(xRegisterSSE(t2reg), xRegisterSSE(t1reg));
	xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t2reg));

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	_freeXMMreg(t2reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPADD.B(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPADD.B(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPADD.B(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDH);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | (_Rt_ != 0 ? XMMINFO_READT : 0) | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		else
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else if (_Rt_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
	else
	{
		if (EEREC_D == EEREC_S)
			xPADD.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPADD.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPADD.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDW);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | (_Rt_ != 0 ? XMMINFO_READT : 0) | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		else
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else if (_Rt_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
	else
	{
		if (EEREC_D == EEREC_S)
			xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPSUB.B(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.B(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.B(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPSUB.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.W(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPEXTLW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXTLW);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.Q(xRegisterSSE(EEREC_D), 32);
	}
	else
	{
		if (EEREC_D == EEREC_T)
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else if (EEREC_D == EEREC_S)
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	_clearNeededXMMregs();
}

void recPEXTLB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXTLB);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		xPUNPCK.LBW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.W(xRegisterSSE(EEREC_D), 8);
	}
	else
	{
		if (EEREC_D == EEREC_T)
			xPUNPCK.LBW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else if (EEREC_D == EEREC_S)
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.LBW(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.LBW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	_clearNeededXMMregs();
}

void recPEXTLH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXTLH);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		xPUNPCK.LWD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.D(xRegisterSSE(EEREC_D), 16);
	}
	else
	{
		if (EEREC_D == EEREC_T)
			xPUNPCK.LWD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else if (EEREC_D == EEREC_S)
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.LWD(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.LWD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	_clearNeededXMMregs();
}

#endif

/*********************************************************
*   MMI1 opcodes                                         *
*                                                        *
*********************************************************/
#ifndef MMI1_RECOMPILE

REC_FUNC_DEL(PABSW,  _Rd_);
REC_FUNC_DEL(PABSH,  _Rd_);

REC_FUNC_DEL(PMINW,  _Rd_);
REC_FUNC_DEL(PADSBH, _Rd_);
REC_FUNC_DEL(PMINH,  _Rd_);
REC_FUNC_DEL(PCEQB,  _Rd_);
REC_FUNC_DEL(PCEQH,  _Rd_);
REC_FUNC_DEL(PCEQW,  _Rd_);

REC_FUNC_DEL(PADDUB, _Rd_);
REC_FUNC_DEL(PADDUH, _Rd_);
REC_FUNC_DEL(PADDUW, _Rd_);

REC_FUNC_DEL(PSUBUB, _Rd_);
REC_FUNC_DEL(PSUBUH, _Rd_);
REC_FUNC_DEL(PSUBUW, _Rd_);

REC_FUNC_DEL(PEXTUW, _Rd_);
REC_FUNC_DEL(PEXTUH, _Rd_);
REC_FUNC_DEL(PEXTUB, _Rd_);
REC_FUNC_DEL(QFSRV,  _Rd_);

#else

////////////////////////////////////////////////////

void recPABSW() //needs clamping
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PABSW);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
	xPSLL.D(xRegisterSSE(t0reg), 31);
	xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T)); //0xffffffff if equal to 0x80000000
	xPABS.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T)); //0x80000000 -> 0x80000000
	xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg)); //0x80000000 -> 0x7fffffff
	_freeXMMreg(t0reg);
	_clearNeededXMMregs();
}


////////////////////////////////////////////////////
void recPABSH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PABSH);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	xPCMP.EQW(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
	xPSLL.W(xRegisterSSE(t0reg), 15);
	xPCMP.EQW(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T)); //0xffff if equal to 0x8000
	xPABS.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T)); //0x8000 -> 0x8000
	xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg)); //0x8000 -> 0x7fff
	_freeXMMreg(t0reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMINW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMINW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_S == EEREC_T)
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else if (EEREC_D == EEREC_S)
		xPMIN.SD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPMIN.SD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPMIN.SD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADSBH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADSBH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	int t0reg;

	if (EEREC_S == EEREC_T)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPADD.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		// reset lower bits to 0s
		xPSRL.DQ(xRegisterSSE(EEREC_D), 8);
		xPSLL.DQ(xRegisterSSE(EEREC_D), 8);
	}
	else
	{
		t0reg = _allocTempXMMreg(XMMT_INT, -1);

		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));

		if (EEREC_D == EEREC_S)
		{
			xPADD.W(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xPSUB.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPSUB.W(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPADD.W(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		}

		// t0reg - adds, EEREC_D - subs
		xPSRL.DQ(xRegisterSSE(t0reg), 8);
		xMOVLH.PS(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}

	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDUW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDUW);

	int info = eeRecompileCodeXMM((_Rs_ ? XMMINFO_READS : 0) | (_Rt_ ? XMMINFO_READT : 0) | XMMINFO_WRITED);

	if (_Rt_ == 0)
	{
		if (_Rs_ == 0)
		{
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
	else if (_Rs_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		int t1reg = _allocTempXMMreg(XMMT_INT, -1);

		xPCMP.EQB(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSLL.D(xRegisterSSE(t0reg), 31); // 0x80000000
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(t0reg));
		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S)); // invert MSB of Rs (for unsigned comparison)

		// normal 32-bit addition
		if (EEREC_D == EEREC_S)
			xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPADD.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}

		// unsigned 32-bit comparison
		xPXOR(xRegisterSSE(t1reg), xRegisterSSE(EEREC_D)); // invert MSB of Rd (for unsigned comparison)
		xPCMP.GTD(xRegisterSSE(t0reg), xRegisterSSE(t1reg));

		// saturate
		xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg)); // clear word with 0xFFFFFFFF if (Rd < Rs)

		_freeXMMreg(t0reg);
		_freeXMMreg(t1reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBUB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBUB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPSUB.USB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.USB(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.USB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBUH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBUH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPSUB.USW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.USW(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.USW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSUBUW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSUBUW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	int t1reg = _allocTempXMMreg(XMMT_INT, -1);

	xPCMP.EQB(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
	xPSLL.D(xRegisterSSE(t0reg), 31); // 0x80000000

	// normal 32-bit subtraction
	// and invert MSB of Rs and Rt (for unsigned comparison)
	if (EEREC_D == EEREC_S)
	{
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(t0reg));
		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xPXOR(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else if (EEREC_D == EEREC_T)
	{
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
		xPXOR(xRegisterSSE(t1reg), xRegisterSSE(t0reg));
		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSUB.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(t0reg));
		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xPXOR(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
	}

	// unsigned 32-bit comparison
	xPCMP.GTD(xRegisterSSE(t0reg), xRegisterSSE(t1reg));

	// saturate
	xPAND(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg)); // clear word with zero if (Rs <= Rt)

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPEXTUH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXTUH);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		xPUNPCK.HWD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.D(xRegisterSSE(EEREC_D), 16);
	}
	else
	{
		if (EEREC_D == EEREC_T)
			xPUNPCK.HWD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else if (EEREC_D == EEREC_S)
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.HWD(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.HWD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	_clearNeededXMMregs();
}

alignas(16) static u32 tempqw[8];

void recQFSRV()
{
	if (!_Rd_)
		return;
	//Console.WriteLn("recQFSRV()");

	EE::Profiler.EmitOp(eeOpcode::QFSRV);

	if (_Rs_ == _Rt_ + 1)
	{
		_flushEEreg(_Rs_);
		_flushEEreg(_Rt_);
		int info = eeRecompileCodeXMM(XMMINFO_WRITED);

		xMOV(eax, ptr32[&cpuRegs.sa]);
		xLEA(rcx, ptr[&cpuRegs.GPR.r[_Rt_]]);
		xMOVDQU(xRegisterSSE(EEREC_D), ptr32[rax + rcx]);
		return;
	}

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

	xMOV(eax, ptr32[&cpuRegs.sa]);
	xLEA(rcx, ptr[tempqw]);
	xMOVDQA(ptr32[rcx], xRegisterSSE(EEREC_T));
	xMOVDQA(ptr32[rcx + 16], xRegisterSSE(EEREC_S));
	xMOVDQU(xRegisterSSE(EEREC_D), ptr32[rax + rcx]);

	_clearNeededXMMregs();
}


void recPEXTUB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXTUB);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);

	if (_Rs_ == 0)
	{
		xPUNPCK.HBW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.W(xRegisterSSE(EEREC_D), 8);
	}
	else
	{
		if (EEREC_D == EEREC_T)
			xPUNPCK.HBW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else if (EEREC_D == EEREC_S)
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.HBW(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.HBW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPEXTUW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXTUW);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | XMMINFO_READT | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		xPUNPCK.HDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPSRL.Q(xRegisterSSE(EEREC_D), 32);
	}
	else
	{
		if (EEREC_D == EEREC_T)
			xPUNPCK.HDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else if (EEREC_D == EEREC_S)
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.HDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.HDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMINH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMINH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPMIN.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPMIN.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPMIN.SW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCEQB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCEQB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPCMP.EQB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPCMP.EQB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.EQB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCEQH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCEQH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPCMP.EQW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPCMP.EQW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.EQW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCEQW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCEQW);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPCMP.EQD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPCMP.EQD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPCMP.EQD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDUB()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDUB);

	int info = eeRecompileCodeXMM(XMMINFO_READS | (_Rt_ ? XMMINFO_READT : 0) | XMMINFO_WRITED);
	if (_Rt_)
	{
		if (EEREC_D == EEREC_S)
			xPADD.USB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPADD.USB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPADD.USB(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
	}
	else
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPADDUH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PADDUH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
		xPADD.USW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	else if (EEREC_D == EEREC_T)
		xPADD.USW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPADD.USW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

#endif
/*********************************************************
*   MMI2 opcodes                                         *
*                                                        *
*********************************************************/
#ifndef MMI2_RECOMPILE

REC_FUNC_DEL(PMFHI,  _Rd_);
REC_FUNC_DEL(PMFLO,  _Rd_);
REC_FUNC_DEL(PCPYLD, _Rd_);
REC_FUNC_DEL(PAND,  _Rd_);
REC_FUNC_DEL(PXOR,  _Rd_);

REC_FUNC_DEL(PMADDW, _Rd_);
REC_FUNC_DEL(PSLLVW, _Rd_);
REC_FUNC_DEL(PSRLVW, _Rd_);
REC_FUNC_DEL(PMSUBW, _Rd_);
REC_FUNC_DEL(PINTH,  _Rd_);
REC_FUNC_DEL(PMULTW, _Rd_);
REC_FUNC_DEL(PDIVW,  _Rd_);
REC_FUNC_DEL(PMADDH, _Rd_);
REC_FUNC_DEL(PHMADH, _Rd_);
REC_FUNC_DEL(PMSUBH, _Rd_);
REC_FUNC_DEL(PHMSBH, _Rd_);
REC_FUNC_DEL(PEXEH,  _Rd_);
REC_FUNC_DEL(PREVH,  _Rd_);
REC_FUNC_DEL(PMULTH, _Rd_);
REC_FUNC_DEL(PDIVBW, _Rd_);
REC_FUNC_DEL(PEXEW,  _Rd_);
REC_FUNC_DEL(PROT3W, _Rd_);

#else

////////////////////////////////////////////////////
void recPMADDW()
{
	EE::Profiler.EmitOp(eeOpcode::PMADDW);

	int info = eeRecompileCodeXMM((((_Rs_) && (_Rt_)) ? XMMINFO_READS : 0) | (((_Rs_) && (_Rt_)) ? XMMINFO_READT : 0) | (_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_WRITELO | XMMINFO_WRITEHI | XMMINFO_READLO | XMMINFO_READHI);
	xSHUF.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
	xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO), 0xd8); // LO = {LO[0], HI[0], LO[2], HI[2]}
	if (_Rd_)
	{
		if (!_Rs_ || !_Rt_)
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		else if (EEREC_D == EEREC_S)
			xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
	}
	else
	{
		if (!_Rs_ || !_Rt_)
			xPXOR(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));
			xPMUL.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_T));
		}
	}

	// add from LO/HI
	if (_Rd_)
		xPADD.Q(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO));
	else
		xPADD.Q(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_LO));

	// interleave & sign extend
	if (_Rd_)
	{
		xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_D), 0x88);
		xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_D), 0xdd);
	}
	else
	{
		xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
		xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0xdd);
	}
	xPMOVSX.DQ(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
	xPMOVSX.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSLLVW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSLLVW);

	int info = eeRecompileCodeXMM((_Rs_ ? XMMINFO_READS : 0) | (_Rt_ ? XMMINFO_READT : 0) | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
		{
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
		{
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x88);
			xPMOVSX.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
	}
	else if (_Rt_ == 0)
	{
		xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		int t1reg = _allocTempXMMreg(XMMT_INT, -1);

		// shamt is 5-bit
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xPSLL.Q(xRegisterSSE(t0reg), 27 + 32);
		xPSRL.Q(xRegisterSSE(t0reg), 27 + 32);

		// EEREC_D[0] <- Rt[0], t1reg[0] <- Rt[2]
		xMOVHL.PS(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
		if (EEREC_D != EEREC_T)
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));

		// shift (left) Rt[0]
		xPSLL.D(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

		// shift (left) Rt[2]
		xMOVHL.PS(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSLL.D(xRegisterSSE(t1reg), xRegisterSSE(t0reg));

		// merge & sign extend
		xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
		xPMOVSX.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));

		_freeXMMreg(t0reg);
		_freeXMMreg(t1reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPSRLVW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSRLVW);

	int info = eeRecompileCodeXMM((_Rs_ ? XMMINFO_READS : 0) | (_Rt_ ? XMMINFO_READT : 0) | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
		{
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
		{
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x88);
			xPMOVSX.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
	}
	else if (_Rt_ == 0)
	{
		xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		int t1reg = _allocTempXMMreg(XMMT_INT, -1);

		// shamt is 5-bit
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xPSLL.Q(xRegisterSSE(t0reg), 27 + 32);
		xPSRL.Q(xRegisterSSE(t0reg), 27 + 32);

		// EEREC_D[0] <- Rt[0], t1reg[0] <- Rt[2]
		xMOVHL.PS(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
		if (EEREC_D != EEREC_T)
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));

		// shift (right logical) Rt[0]
		xPSRL.D(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

		// shift (right logical) Rt[2]
		xMOVHL.PS(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSRL.D(xRegisterSSE(t1reg), xRegisterSSE(t0reg));

		// merge & sign extend
		xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
		xPMOVSX.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));

		_freeXMMreg(t0reg);
		_freeXMMreg(t1reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMSUBW()
{
	EE::Profiler.EmitOp(eeOpcode::PMSUBW);

	int info = eeRecompileCodeXMM((((_Rs_) && (_Rt_)) ? XMMINFO_READS : 0) | (((_Rs_) && (_Rt_)) ? XMMINFO_READT : 0) | (_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_WRITELO | XMMINFO_WRITEHI | XMMINFO_READLO | XMMINFO_READHI);
	xSHUF.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
	xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO), 0xd8); // LO = {LO[0], HI[0], LO[2], HI[2]}
	if (_Rd_)
	{
		if (!_Rs_ || !_Rt_)
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		else if (EEREC_D == EEREC_S)
			xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
	}
	else
	{
		if (!_Rs_ || !_Rt_)
			xPXOR(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));
			xPMUL.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_T));
		}
	}

	// sub from LO/HI
	if (_Rd_)
	{
		xPSUB.Q(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_D));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO));
	}
	else
	{
		xPSUB.Q(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI));
		xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_LO));
	}

	// interleave & sign extend
	if (_Rd_)
	{
		xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_D), 0x88);
		xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_D), 0xdd);
	}
	else
	{
		xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
		xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0xdd);
	}
	xPMOVSX.DQ(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
	xPMOVSX.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMULTW()
{
	EE::Profiler.EmitOp(eeOpcode::PMULTW);

	int info = eeRecompileCodeXMM((((_Rs_) && (_Rt_)) ? XMMINFO_READS : 0) | (((_Rs_) && (_Rt_)) ? XMMINFO_READT : 0) | (_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_WRITELO | XMMINFO_WRITEHI);
	if (!_Rs_ || !_Rt_)
	{
		if (_Rd_)
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		xPXOR(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
		xPXOR(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
	}
	else
	{
		if (_Rd_)
		{
			if (EEREC_D == EEREC_S)
				xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			else if (EEREC_D == EEREC_T)
				xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			else
			{
				xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
				xPMUL.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			}
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));
			xPMUL.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_T));
		}

		// interleave & sign extend
		if (_Rd_)
		{
			xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_D), 0x88);
			xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_D), 0xdd);
		}
		else
		{
			xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
			xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0xdd);
		}
		xPMOVSX.DQ(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
		xPMOVSX.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
	}
	_clearNeededXMMregs();
}
////////////////////////////////////////////////////
void recPDIVW()
{
	EE::Profiler.EmitOp(eeOpcode::PDIVW);

	_deleteEEreg(_Rd_, 0);
	recCall(Interp::PDIVW);
}

////////////////////////////////////////////////////
void recPDIVBW()
{
	EE::Profiler.EmitOp(eeOpcode::PDIVBW);

	_deleteEEreg(_Rd_, 0);
	recCall(Interp::PDIVBW); //--
}

////////////////////////////////////////////////////

//upper word of each doubleword in LO and HI is undocumented/undefined
//contains the upper multiplication result (before the addition with the lower multiplication result)
void recPHMADH()
{
	EE::Profiler.EmitOp(eeOpcode::PHMADH);

	int info = eeRecompileCodeXMM((_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITELO | XMMINFO_WRITEHI);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);

	xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
	xPSRL.D(xRegisterSSE(t0reg), 16);
	xPSLL.D(xRegisterSSE(t0reg), 16);
	xPMADD.WD(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));

	if (_Rd_)
	{
		if (EEREC_D == EEREC_S)
		{
			xPMADD.WD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			xPMADD.WD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPMADD.WD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
		xMOVDQA(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_D));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_T));
		xPMADD.WD(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_S));
	}

	xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_LO));

	xSHUF.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg), 0x88);
	xSHUF.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO), 0xd8);

	xSHUF.PS(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg), 0xdd);
	xSHUF.PS(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0xd8);

	_freeXMMreg(t0reg);
	_clearNeededXMMregs();
}

void recPMSUBH()
{
	EE::Profiler.EmitOp(eeOpcode::PMSUBH);

	int info = eeRecompileCodeXMM((_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_READS | XMMINFO_READT | XMMINFO_READLO | XMMINFO_READHI | XMMINFO_WRITELO | XMMINFO_WRITEHI);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	int t1reg = _allocTempXMMreg(XMMT_INT, -1);

	if (!_Rd_)
	{
		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSHUF.D(xRegisterSSE(t1reg), xRegisterSSE(EEREC_S), 0xd8); //S0, S1, S4, S5, S2, S3, S6, S7
		xPUNPCK.LWD(xRegisterSSE(t1reg), xRegisterSSE(t0reg)); //S0, 0, S1, 0, S4, 0, S5, 0
		xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T), 0xd8); //T0, T1, T4, T5, T2, T3, T6, T7
		xPUNPCK.LWD(xRegisterSSE(t0reg), xRegisterSSE(t0reg)); //T0, T0, T1, T1, T4, T4, T5, T5
		xPMADD.WD(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); //S0*T0+0*T0, S1*T1+0*T1, S4*T4+0*T4, S5*T5+0*T5

		xPSUB.D(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));

		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSHUF.D(xRegisterSSE(t1reg), xRegisterSSE(EEREC_S), 0xd8); //S0, S1, S4, S5, S2, S3, S6, S7
		xPUNPCK.HWD(xRegisterSSE(t1reg), xRegisterSSE(t0reg)); //S2, 0, S3, 0, S6, 0, S7, 0
		xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T), 0xd8); //T0, T1, T4, T5, T2, T3, T6, T7
		xPUNPCK.HWD(xRegisterSSE(t0reg), xRegisterSSE(t0reg)); //T2, T2, T3, T3, T6, T6, T7, T7
		xPMADD.WD(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); //S2*T2+0*T2, S3*T3+0*T3, S6*T6+0*T6, S7*T7+0*T7

		xPSUB.D(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg));
	}
	else
	{
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_S));

		xPMUL.LW(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xPMUL.HW(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

		// 0-3
		xPUNPCK.LWD(xRegisterSSE(t0reg), xRegisterSSE(t1reg));
		// 4-7
		xPUNPCK.HWD(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(t0reg));

		// 0,1,4,5, L->H
		xPUNPCK.LQDQ(xRegisterSSE(t0reg), xRegisterSSE(EEREC_D));
		// 2,3,6,7, L->H
		xPUNPCK.HQDQ(xRegisterSSE(t1reg), xRegisterSSE(EEREC_D));

		xPSUB.D(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));
		xPSUB.D(xRegisterSSE(EEREC_HI), xRegisterSSE(t1reg));

		// 0,2,4,6, L->H
		xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO), 0x88);
		xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI), 0x88);
		xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
	}

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);

	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
//upper word of each doubleword in LO and HI is undocumented/undefined
//it contains the NOT of the upper multiplication result (before the substraction of the lower multiplication result)
void recPHMSBH()
{
	EE::Profiler.EmitOp(eeOpcode::PHMSBH);

	int info = eeRecompileCodeXMM((_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITELO | XMMINFO_WRITEHI);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);

	xPCMP.EQD(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
	xPSRL.D(xRegisterSSE(EEREC_LO), 16);
	xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));
	xPAND(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_LO));
	xPMADD.WD(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_T));
	xPSLL.D(xRegisterSSE(EEREC_LO), 16);
	xPAND(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_S));
	xPMADD.WD(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_T));
	xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_LO));
	xPSUB.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI));
	if (_Rd_)
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO));

	xPCMP.EQD(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
	xPXOR(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI));

	xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_LO));

	xSHUF.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg), 0x88);
	xSHUF.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO), 0xd8);

	xSHUF.PS(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg), 0xdd);
	xSHUF.PS(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0xd8);

	_freeXMMreg(t0reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPEXEH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXEH);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0xc6);
	xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0xc6);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPREVH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PREVH);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x1B);
	xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0x1B);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPINTH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PINTH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);
	if (EEREC_D == EEREC_S)
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xMOVHL.PS(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		if (EEREC_D != EEREC_T)
			xMOVQZX(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPUNPCK.LWD(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	else
	{
		xMOVLH.PS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		xPUNPCK.HWD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
	_clearNeededXMMregs();
}

void recPEXEW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PEXEW);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0xc6);
	_clearNeededXMMregs();
}

void recPROT3W()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PROT3W);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0xc9);
	_clearNeededXMMregs();
}

void recPMULTH()
{
	EE::Profiler.EmitOp(eeOpcode::PMULTH);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | (_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_WRITELO | XMMINFO_WRITEHI);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);

	xMOVDQA(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_S));
	xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));

	xPMUL.LW(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_T));
	xPMUL.HW(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_T));
	xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_LO));

	// 0-3
	xPUNPCK.LWD(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI));
	// 4-7
	xPUNPCK.HWD(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI));

	if (_Rd_)
	{
		// 0,2,4,6, L->H
		xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO), 0x88);
		xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg), 0x88);
		xPUNPCK.LQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_HI));
	}

	xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_LO));

	// 0,1,4,5, L->H
	xPUNPCK.LQDQ(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));
	// 2,3,6,7, L->H
	xPUNPCK.HQDQ(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg));

	_freeXMMreg(t0reg);
	_clearNeededXMMregs();
}

void recPMFHI()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMFHI);

	int info = eeRecompileCodeXMM(XMMINFO_WRITED | XMMINFO_READHI);
	xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_HI));
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMFLO()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PMFLO);

	int info = eeRecompileCodeXMM(XMMINFO_WRITED | XMMINFO_READLO);
	xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO));
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPAND()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PAND);

	int info = eeRecompileCodeXMM(XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
	if (EEREC_D == EEREC_T)
	{
		xPAND(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
	else if (EEREC_D == EEREC_S)
	{
		xPAND(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPAND(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPXOR()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PXOR);

	int info = eeRecompileCodeXMM(XMMINFO_WRITED | XMMINFO_READS | XMMINFO_READT);
	if (EEREC_D == EEREC_T)
	{
		xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
	else if (EEREC_D == EEREC_S)
	{
		xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCPYLD()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCPYLD);

	int info = eeRecompileCodeXMM(XMMINFO_WRITED | ((_Rs_ == 0) ? 0 : XMMINFO_READS) | XMMINFO_READT);
	if (_Rs_ == 0)
	{
		xMOVQZX(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else
	{
		if (EEREC_D == EEREC_T)
			xPUNPCK.LQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else if (EEREC_S == EEREC_T)
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S), 0x44);
		else if (EEREC_D == EEREC_S)
		{
			xPUNPCK.LQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0x4e);
		}
		else
		{
			xMOVQZX(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPUNPCK.LQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	_clearNeededXMMregs();
}

void recPMADDH()
{
	EE::Profiler.EmitOp(eeOpcode::PMADDH);

	int info = eeRecompileCodeXMM((_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_READS | XMMINFO_READT | XMMINFO_READLO | XMMINFO_READHI | XMMINFO_WRITELO | XMMINFO_WRITEHI);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	int t1reg = _allocTempXMMreg(XMMT_INT, -1);

	if (!_Rd_)
	{
		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSHUF.D(xRegisterSSE(t1reg), xRegisterSSE(EEREC_S), 0xd8); //S0, S1, S4, S5, S2, S3, S6, S7
		xPUNPCK.LWD(xRegisterSSE(t1reg), xRegisterSSE(t0reg)); //S0, 0, S1, 0, S4, 0, S5, 0
		xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T), 0xd8); //T0, T1, T4, T5, T2, T3, T6, T7
		xPUNPCK.LWD(xRegisterSSE(t0reg), xRegisterSSE(t0reg)); //T0, T0, T1, T1, T4, T4, T5, T5
		xPMADD.WD(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); //S0*T0+0*T0, S1*T1+0*T1, S4*T4+0*T4, S5*T5+0*T5

		xPADD.D(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));

		xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSHUF.D(xRegisterSSE(t1reg), xRegisterSSE(EEREC_S), 0xd8); //S0, S1, S4, S5, S2, S3, S6, S7
		xPUNPCK.HWD(xRegisterSSE(t1reg), xRegisterSSE(t0reg)); //S2, 0, S3, 0, S6, 0, S7, 0
		xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T), 0xd8); //T0, T1, T4, T5, T2, T3, T6, T7
		xPUNPCK.HWD(xRegisterSSE(t0reg), xRegisterSSE(t0reg)); //T2, T2, T3, T3, T6, T6, T7, T7
		xPMADD.WD(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); //S2*T2+0*T2, S3*T3+0*T3, S6*T6+0*T6, S7*T7+0*T7

		xPADD.D(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg));
	}
	else
	{
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(EEREC_S));

		xPMUL.LW(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
		xPMUL.HW(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

		// 0-3
		xPUNPCK.LWD(xRegisterSSE(t0reg), xRegisterSSE(t1reg));
		// 4-7
		xPUNPCK.HWD(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
		xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(t0reg));

		// 0,1,4,5, L->H
		xPUNPCK.LQDQ(xRegisterSSE(t0reg), xRegisterSSE(EEREC_D));
		// 2,3,6,7, L->H
		xPUNPCK.HQDQ(xRegisterSSE(t1reg), xRegisterSSE(EEREC_D));

		xPADD.D(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));
		xPADD.D(xRegisterSSE(EEREC_HI), xRegisterSSE(t1reg));

		// 0,2,4,6, L->H
		xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO), 0x88);
		xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI), 0x88);
		xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
	}

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);

	_clearNeededXMMregs();
}

#endif
/*********************************************************
*   MMI3 opcodes                                         *
*                                                        *
*********************************************************/
#ifndef MMI3_RECOMPILE

REC_FUNC_DEL(PMADDUW, _Rd_);
REC_FUNC_DEL(PSRAVW,  _Rd_);
REC_FUNC_DEL(PMTHI,   _Rd_);
REC_FUNC_DEL(PMTLO,   _Rd_);
REC_FUNC_DEL(PINTEH,  _Rd_);
REC_FUNC_DEL(PMULTUW, _Rd_);
REC_FUNC_DEL(PDIVUW,  _Rd_);
REC_FUNC_DEL(PCPYUD,  _Rd_);
REC_FUNC_DEL(POR,     _Rd_);
REC_FUNC_DEL(PNOR,    _Rd_);
REC_FUNC_DEL(PCPYH,   _Rd_);
REC_FUNC_DEL(PEXCW,   _Rd_);
REC_FUNC_DEL(PEXCH,   _Rd_);

#else

////////////////////////////////////////////////////
//REC_FUNC( PSRAVW, _Rd_ );

void recPSRAVW()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PSRAVW);

	int info = eeRecompileCodeXMM((_Rs_ ? XMMINFO_READS : 0) | (_Rt_ ? XMMINFO_READT : 0) | XMMINFO_WRITED);
	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
		{
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
		{
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0x88);
			xPMOVSX.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
	}
	else if (_Rt_ == 0)
	{
		xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		int t1reg = _allocTempXMMreg(XMMT_INT, -1);

		// shamt is 5-bit
		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
		xPSLL.Q(xRegisterSSE(t0reg), 27 + 32);
		xPSRL.Q(xRegisterSSE(t0reg), 27 + 32);

		// EEREC_D[0] <- Rt[0], t1reg[0] <- Rt[2]
		xMOVHL.PS(xRegisterSSE(t1reg), xRegisterSSE(EEREC_T));
		if (EEREC_D != EEREC_T)
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));

		// shift (right arithmetic) Rt[0]
		xPSRA.D(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));

		// shift (right arithmetic) Rt[2]
		xMOVHL.PS(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPSRA.D(xRegisterSSE(t1reg), xRegisterSSE(t0reg));

		// merge & sign extend
		if (x86caps.hasStreamingSIMD4Extensions)
		{
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
			xPMOVSX.DQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
		{
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t1reg));
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_D));
			xPSRA.D(xRegisterSSE(t0reg), 31); // get the signs
			xPUNPCK.LDQ(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		}

		_freeXMMreg(t0reg);
		_freeXMMreg(t1reg);
	}

	_clearNeededXMMregs();
}


////////////////////////////////////////////////////
alignas(16) static const u32 s_tempPINTEH[4] = {0x0000ffff, 0x0000ffff, 0x0000ffff, 0x0000ffff};

void recPINTEH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PINTEH);

	int info = eeRecompileCodeXMM((_Rs_ ? XMMINFO_READS : 0) | (_Rt_ ? XMMINFO_READT : 0) | XMMINFO_WRITED);

	int t0reg = -1;

	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
		{
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			xPAND(xRegisterSSE(EEREC_D), ptr[s_tempPINTEH]);
		}
	}
	else if (_Rt_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		xPSLL.D(xRegisterSSE(EEREC_D), 16);
	}
	else
	{
		if (EEREC_S == EEREC_T)
		{
			xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S), 0xa0);
			xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0xa0);
		}
		else if (EEREC_D == EEREC_T)
		{
			pxAssert(EEREC_D != EEREC_S);
			t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xPSLL.D(xRegisterSSE(EEREC_D), 16);
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_S));
			xPSRL.D(xRegisterSSE(EEREC_D), 16);
			xPSLL.D(xRegisterSSE(t0reg), 16);
			xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		}
		else
		{
			t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(EEREC_T));
			xPSLL.D(xRegisterSSE(t0reg), 16);
			xPSLL.D(xRegisterSSE(EEREC_D), 16);
			xPSRL.D(xRegisterSSE(t0reg), 16);
			xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		}
	}

	if (t0reg >= 0)
		_freeXMMreg(t0reg);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMULTUW()
{
	EE::Profiler.EmitOp(eeOpcode::PMULTUW);

	int info = eeRecompileCodeXMM((((_Rs_) && (_Rt_)) ? XMMINFO_READS : 0) | (((_Rs_) && (_Rt_)) ? XMMINFO_READT : 0) | (_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_WRITELO | XMMINFO_WRITEHI);
	if (!_Rs_ || !_Rt_)
	{
		if (_Rd_)
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		xPXOR(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
		xPXOR(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
	}
	else
	{
		if (_Rd_)
		{
			if (EEREC_D == EEREC_S)
				xPMUL.UDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			else if (EEREC_D == EEREC_T)
				xPMUL.UDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			else
			{
				xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
				xPMUL.UDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			}
			xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_D));
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));
			xPMUL.UDQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_T));
		}

		// interleave & sign extend
		if (x86caps.hasStreamingSIMD4Extensions)
		{
			xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
			xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0xdd);
			xPMOVSX.DQ(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
			xPMOVSX.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
		}
		else
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI), 0xd8);
			xMOVDQA(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));
			xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg));
			xPSRA.D(xRegisterSSE(t0reg), 31); // get the signs

			xPUNPCK.LDQ(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));
			xPUNPCK.HDQ(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMADDUW()
{
	EE::Profiler.EmitOp(eeOpcode::PMADDUW);

	int info = eeRecompileCodeXMM((((_Rs_) && (_Rt_)) ? XMMINFO_READS : 0) | (((_Rs_) && (_Rt_)) ? XMMINFO_READT : 0) | (_Rd_ ? XMMINFO_WRITED : 0) | XMMINFO_WRITELO | XMMINFO_WRITEHI | XMMINFO_READLO | XMMINFO_READHI);
	xSHUF.PS(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
	xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO), 0xd8); // LO = {LO[0], HI[0], LO[2], HI[2]}
	if (_Rd_)
	{
		if (!_Rs_ || !_Rt_)
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		else if (EEREC_D == EEREC_S)
			xPMUL.UDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPMUL.UDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPMUL.UDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
	}
	else
	{
		if (!_Rs_ || !_Rt_)
			xPXOR(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));
			xPMUL.UDQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_T));
		}
	}

	// add from LO/HI
	if (_Rd_)
	{
		xPADD.Q(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_LO));
		xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_D));
	}
	else
		xPADD.Q(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_LO));

	// interleave & sign extend
	if (x86caps.hasStreamingSIMD4Extensions)
	{
		xPSHUF.D(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_HI), 0x88);
		xPSHUF.D(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI), 0xdd);
		xPMOVSX.DQ(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_LO));
		xPMOVSX.DQ(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_HI));
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);
		xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(EEREC_HI), 0xd8);
		xMOVDQA(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));
		xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg));
		xPSRA.D(xRegisterSSE(t0reg), 31); // get the signs

		xPUNPCK.LDQ(xRegisterSSE(EEREC_LO), xRegisterSSE(t0reg));
		xPUNPCK.HDQ(xRegisterSSE(EEREC_HI), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPDIVUW()
{
	EE::Profiler.EmitOp(eeOpcode::PDIVUW);

	_deleteEEreg(_Rd_, 0);
	recCall(Interp::PDIVUW);
}

////////////////////////////////////////////////////
void recPEXCW()
{
	EE::Profiler.EmitOp(eeOpcode::PEXCW);

	if (!_Rd_)
		return;

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0xd8);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPEXCH()
{
	EE::Profiler.EmitOp(eeOpcode::PEXCH);

	if (!_Rd_)
		return;

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0xd8);
	xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0xd8);
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPNOR()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PNOR);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | (_Rt_ != 0 ? XMMINFO_READT : 0) | XMMINFO_WRITED);

	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
		{
			xPCMP.EQD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
		{
			if (EEREC_D == EEREC_T)
			{
				int t0reg = _allocTempXMMreg(XMMT_INT, -1);
				xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
				xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
				_freeXMMreg(t0reg);
			}
			else
			{
				xPCMP.EQD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
				if (_Rt_ != 0)
					xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			}
		}
	}
	else if (_Rt_ == 0)
	{
		if (EEREC_D == EEREC_S)
		{
			int t0reg = _allocTempXMMreg(XMMT_INT, -1);
			xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
			_freeXMMreg(t0reg);
		}
		else
		{
			xPCMP.EQD(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
	}
	else
	{
		int t0reg = _allocTempXMMreg(XMMT_INT, -1);

		if (EEREC_D == EEREC_S)
			xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
			xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			if (EEREC_S != EEREC_T)
				xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}

		xPCMP.EQD(xRegisterSSE(t0reg), xRegisterSSE(t0reg));
		xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(t0reg));
		_freeXMMreg(t0reg);
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMTHI()
{
	EE::Profiler.EmitOp(eeOpcode::PMTHI);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_WRITEHI);
	xMOVDQA(xRegisterSSE(EEREC_HI), xRegisterSSE(EEREC_S));
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPMTLO()
{
	EE::Profiler.EmitOp(eeOpcode::PMTLO);

	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_WRITELO);
	xMOVDQA(xRegisterSSE(EEREC_LO), xRegisterSSE(EEREC_S));
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCPYUD()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCPYUD);

	int info = eeRecompileCodeXMM(XMMINFO_READS | ((_Rt_ == 0) ? 0 : XMMINFO_READT) | XMMINFO_WRITED);

	if (_Rt_ == 0)
	{
		if (EEREC_D == EEREC_S)
		{
			xPUNPCK.HQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xMOVQZX(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
		{
			xMOVHL.PS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xMOVQZX(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
	}
	else
	{
		if (EEREC_D == EEREC_S)
			xPUNPCK.HQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		else if (EEREC_D == EEREC_T)
		{
			//TODO
			xPUNPCK.HQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0x4e);
		}
		else
		{
			if (EEREC_S == EEREC_T)
			{
				xPSHUF.D(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S), 0xee);
			}
			else
			{
				xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
				xPUNPCK.HQDQ(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			}
		}
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPOR()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::POR);

	int info = eeRecompileCodeXMM((_Rs_ != 0 ? XMMINFO_READS : 0) | (_Rt_ != 0 ? XMMINFO_READT : 0) | XMMINFO_WRITED);

	if (_Rs_ == 0)
	{
		if (_Rt_ == 0)
		{
			xPXOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
		}
		else
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
	}
	else if (_Rt_ == 0)
	{
		xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
	}
	else
	{
		if (EEREC_D == EEREC_S)
		{
			xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
		}
		else if (EEREC_D == EEREC_T)
		{
			xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
		}
		else
		{
			xMOVDQA(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T));
			if (EEREC_S != EEREC_T)
			{
				xPOR(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_S));
			}
		}
	}
	_clearNeededXMMregs();
}

////////////////////////////////////////////////////
void recPCPYH()
{
	if (!_Rd_)
		return;

	EE::Profiler.EmitOp(eeOpcode::PCPYH);

	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
	xPSHUF.LW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_T), 0);
	xPSHUF.HW(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D), 0);
	_clearNeededXMMregs();
}

#endif // else MMI3_RECOMPILE

} // namespace MMI
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
