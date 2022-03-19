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
#include "R5900OpcodeTables.h"
#include "iR5900.h"

using namespace x86Emitter;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
#ifndef MOVE_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(LUI,  _Rt_);
REC_FUNC_DEL(MFLO, _Rd_);
REC_FUNC_DEL(MFHI, _Rd_);
REC_FUNC(MTLO);
REC_FUNC(MTHI);

REC_FUNC_DEL(MFLO1, _Rd_);
REC_FUNC_DEL(MFHI1, _Rd_);
REC_FUNC(MTHI1);
REC_FUNC(MTLO1);

REC_FUNC_DEL(MOVZ, _Rd_);
REC_FUNC_DEL(MOVN, _Rd_);

#else

static void xCopy64(u64* dst, u64* src)
{
	xMOV(rax, ptr64[src]);
	xMOV(ptr64[dst], rax);
}

static void xCMPToZero64(u64* mem)
{
	xCMP(ptr64[mem], 0);
}

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/

//// LUI
void recLUI()
{
	int mmreg;
	if (!_Rt_)
		return;

	_eeOnWriteReg(_Rt_, 1);

	if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_WRITE)) >= 0)
	{
		if (xmmregs[mmreg].mode & MODE_WRITE)
		{
			xMOVH.PS(ptr[&cpuRegs.GPR.r[_Rt_].UL[2]], xRegisterSSE(mmreg));
		}
		xmmregs[mmreg].inuse = 0;
	}

	_deleteEEreg(_Rt_, 0);

	if (EE_CONST_PROP)
	{
		GPR_SET_CONST(_Rt_);
		g_cpuConstRegs[_Rt_].UD[0] = (s32)(cpuRegs.code << 16);
	}
	else
	{
		xMOV(eax, (s32)(cpuRegs.code << 16));
		eeSignExtendTo(_Rt_);
	}

	EE::Profiler.EmitOp(eeOpcode::LUI);
}

////////////////////////////////////////////////////
void recMFHILO(int hi)
{
	int reghi, regd, xmmhilo;
	if (!_Rd_)
		return;

	xmmhilo = hi ? XMMGPR_HI : XMMGPR_LO;
	reghi = _checkXMMreg(XMMTYPE_GPRREG, xmmhilo, MODE_READ);

	_eeOnWriteReg(_Rd_, 0);

	regd = _checkXMMreg(XMMTYPE_GPRREG, _Rd_, MODE_READ | MODE_WRITE);

	if (reghi >= 0)
	{
		if (regd >= 0)
		{
			pxAssert(regd != reghi);

			xmmregs[regd].inuse = 0;

			xMOVQ(ptr[&cpuRegs.GPR.r[_Rd_].UL[0]], xRegisterSSE(reghi));

			if (xmmregs[regd].mode & MODE_WRITE)
			{
				xMOVH.PS(ptr[&cpuRegs.GPR.r[_Rd_].UL[2]], xRegisterSSE(regd));
			}
		}
		else
		{
			_deleteEEreg(_Rd_, 0);
			xMOVQ(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(reghi));
		}
	}
	else
	{
		if (regd >= 0)
		{
			if (EEINST_ISLIVE2(_Rd_))
				xMOVL.PS(xRegisterSSE(regd), ptr[(void*)(hi ? (uptr)&cpuRegs.HI.UD[0] : (uptr)&cpuRegs.LO.UD[0])]);
			else
				xMOVQZX(xRegisterSSE(regd), ptr[(void*)(hi ? (uptr)&cpuRegs.HI.UD[0] : (uptr)&cpuRegs.LO.UD[0])]);
		}
		else
		{
			_deleteEEreg(_Rd_, 0);
			xCopy64(&cpuRegs.GPR.r[_Rd_].UD[0], hi ? &cpuRegs.HI.UD[0] : &cpuRegs.LO.UD[0]);
		}
	}
}

void recMTHILO(int hi)
{
	int reghi, regs, xmmhilo;
	uptr addrhilo;

	xmmhilo = hi ? XMMGPR_HI : XMMGPR_LO;
	addrhilo = hi ? (uptr)&cpuRegs.HI.UD[0] : (uptr)&cpuRegs.LO.UD[0];

	regs = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ);
	reghi = _checkXMMreg(XMMTYPE_GPRREG, xmmhilo, MODE_READ | MODE_WRITE);

	if (reghi >= 0)
	{
		if (regs >= 0)
		{
			pxAssert(reghi != regs);

			_deleteGPRtoXMMreg(_Rs_, 0);
			xPUNPCK.HQDQ(xRegisterSSE(reghi), xRegisterSSE(reghi));
			xPUNPCK.LQDQ(xRegisterSSE(regs), xRegisterSSE(reghi));

			// swap regs
			xmmregs[regs] = xmmregs[reghi];
			xmmregs[reghi].inuse = 0;
			xmmregs[regs].mode |= MODE_WRITE;
		}
		else
		{
			_flushConstReg(_Rs_);
			xMOVL.PS(xRegisterSSE(reghi), ptr[&cpuRegs.GPR.r[_Rs_].UD[0]]);
			xmmregs[reghi].mode |= MODE_WRITE;
		}
	}
	else
	{
		if (regs >= 0)
		{
			xMOVQ(ptr[(void*)(addrhilo)], xRegisterSSE(regs));
		}
		else
		{
			if (GPR_IS_CONST1(_Rs_))
			{
				xWriteImm64ToMem((u64*)addrhilo, rax, g_cpuConstRegs[_Rs_].UD[0]);
			}
			else
			{
				_eeMoveGPRtoR(ecx, _Rs_);
				_flushEEreg(_Rs_);
				xCopy64((u64*)addrhilo, &cpuRegs.GPR.r[_Rs_].UD[0]);
			}
		}
	}
}

void recMFHI()
{
	recMFHILO(1);
	EE::Profiler.EmitOp(eeOpcode::MFHI);
}

void recMFLO()
{
	recMFHILO(0);
	EE::Profiler.EmitOp(eeOpcode::MFLO);
}

void recMTHI()
{
	recMTHILO(1);
	EE::Profiler.EmitOp(eeOpcode::MTHI);
}

void recMTLO()
{
	recMTHILO(0);
	EE::Profiler.EmitOp(eeOpcode::MTLO);
}

////////////////////////////////////////////////////
void recMFHILO1(int hi)
{
	int reghi, regd, xmmhilo;
	if (!_Rd_)
		return;

	xmmhilo = hi ? XMMGPR_HI : XMMGPR_LO;
	reghi = _checkXMMreg(XMMTYPE_GPRREG, xmmhilo, MODE_READ);

	_eeOnWriteReg(_Rd_, 0);

	regd = _checkXMMreg(XMMTYPE_GPRREG, _Rd_, MODE_READ | MODE_WRITE);

	if (reghi >= 0)
	{
		if (regd >= 0)
		{
			xMOVHL.PS(xRegisterSSE(regd), xRegisterSSE(reghi));
			xmmregs[regd].mode |= MODE_WRITE;
		}
		else
		{
			_deleteEEreg(_Rd_, 0);
			xMOVH.PS(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(reghi));
		}
	}
	else
	{
		if (regd >= 0)
		{
			if (EEINST_ISLIVE2(_Rd_))
			{
				xPUNPCK.HQDQ(xRegisterSSE(regd), ptr[(void*)(hi ? (uptr)&cpuRegs.HI.UD[0] : (uptr)&cpuRegs.LO.UD[0])]);
				xPSHUF.D(xRegisterSSE(regd), xRegisterSSE(regd), 0x4e);
			}
			else
			{
				xMOVQZX(xRegisterSSE(regd), ptr[(void*)(hi ? (uptr)&cpuRegs.HI.UD[1] : (uptr)&cpuRegs.LO.UD[1])]);
			}

			xmmregs[regd].mode |= MODE_WRITE;
		}
		else
		{
			_deleteEEreg(_Rd_, 0);
			xCopy64(&cpuRegs.GPR.r[_Rd_].UD[0], hi ? &cpuRegs.HI.UD[1] : &cpuRegs.LO.UD[1]);
		}
	}
}

void recMTHILO1(int hi)
{
	int reghi, regs, xmmhilo;
	uptr addrhilo;

	xmmhilo = hi ? XMMGPR_HI : XMMGPR_LO;
	addrhilo = hi ? (uptr)&cpuRegs.HI.UD[0] : (uptr)&cpuRegs.LO.UD[0];

	regs = _checkXMMreg(XMMTYPE_GPRREG, _Rs_, MODE_READ);
	reghi = _allocCheckGPRtoXMM(g_pCurInstInfo, xmmhilo, MODE_WRITE | MODE_READ);

	if (reghi >= 0)
	{
		if (regs >= 0)
		{
			xPUNPCK.LQDQ(xRegisterSSE(reghi), xRegisterSSE(regs));
		}
		else
		{
			_flushEEreg(_Rs_);
			xPUNPCK.LQDQ(xRegisterSSE(reghi), ptr[&cpuRegs.GPR.r[_Rs_].UD[0]]);
		}
	}
	else
	{
		if (regs >= 0)
		{
			xMOVQ(ptr[(void*)(addrhilo + 8)], xRegisterSSE(regs));
		}
		else
		{
			if (GPR_IS_CONST1(_Rs_))
			{
				xWriteImm64ToMem((u64*)(addrhilo + 8), rax, g_cpuConstRegs[_Rs_].UD[0]);
			}
			else
			{
				_flushEEreg(_Rs_);
				xCopy64((u64*)(addrhilo + 8), &cpuRegs.GPR.r[_Rs_].UD[0]);
			}
		}
	}
}

void recMFHI1()
{
	recMFHILO1(1);
	EE::Profiler.EmitOp(eeOpcode::MFHI1);
}

void recMFLO1()
{
	recMFHILO1(0);
	EE::Profiler.EmitOp(eeOpcode::MFLO1);
}

void recMTHI1()
{
	recMTHILO1(1);
	EE::Profiler.EmitOp(eeOpcode::MTHI1);
}

void recMTLO1()
{
	recMTHILO1(0);
	EE::Profiler.EmitOp(eeOpcode::MTLO1);
}

//// MOVZ
void recMOVZtemp_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0];
}

void recMOVZtemp_consts(int info)
{
	xCMPToZero64(&cpuRegs.GPR.r[_Rt_].UD[0]);
	j8Ptr[0] = JNZ8(0);

	xWriteImm64ToMem(&cpuRegs.GPR.r[_Rd_].UD[0], rax, g_cpuConstRegs[_Rs_].UD[0]);

	x86SetJ8(j8Ptr[0]);
}

void recMOVZtemp_constt(int info)
{
	xCopy64(&cpuRegs.GPR.r[_Rd_].UD[0], &cpuRegs.GPR.r[_Rs_].UD[0]);
}

void recMOVZtemp_(int info)
{
	xCMPToZero64(&cpuRegs.GPR.r[_Rt_].UD[0]);
	j8Ptr[0] = JNZ8(0);

	xCopy64(&cpuRegs.GPR.r[_Rd_].UD[0], &cpuRegs.GPR.r[_Rs_].UD[0]);

	x86SetJ8(j8Ptr[0]);
}

EERECOMPILE_CODE0(MOVZtemp, XMMINFO_READS | XMMINFO_READD | XMMINFO_READD | XMMINFO_WRITED);

void recMOVZ()
{
	if (_Rs_ == _Rd_)
		return;

	if (GPR_IS_CONST1(_Rt_))
	{
		if (g_cpuConstRegs[_Rt_].UD[0] != 0)
			return;
	}
	else
		_deleteEEreg(_Rd_, 1);

	recMOVZtemp();
}

//// MOVN
void recMOVNtemp_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rs_].UD[0];
}

void recMOVNtemp_consts(int info)
{
	xCMPToZero64(&cpuRegs.GPR.r[_Rt_].UD[0]);
	j8Ptr[0] = JZ8(0);

	xWriteImm64ToMem(&cpuRegs.GPR.r[_Rd_].UD[0], rax, g_cpuConstRegs[_Rs_].UD[0]);

	x86SetJ8(j8Ptr[0]);
}

void recMOVNtemp_constt(int info)
{
	xCopy64(&cpuRegs.GPR.r[_Rd_].UD[0], &cpuRegs.GPR.r[_Rs_].UD[0]);
}

void recMOVNtemp_(int info)
{
	xCMPToZero64(&cpuRegs.GPR.r[_Rt_].UD[0]);
	j8Ptr[0] = JZ8(0);

	xCopy64(&cpuRegs.GPR.r[_Rd_].UD[0], &cpuRegs.GPR.r[_Rs_].UD[0]);

	x86SetJ8(j8Ptr[0]);
}

EERECOMPILE_CODE0(MOVNtemp, XMMINFO_READS | XMMINFO_READD | XMMINFO_READD | XMMINFO_WRITED);

void recMOVN()
{
	if (_Rs_ == _Rd_)
		return;

	if (GPR_IS_CONST1(_Rt_))
	{
		if (g_cpuConstRegs[_Rt_].UD[0] == 0)
			return;
	}
	else
		_deleteEEreg(_Rd_, 1);

	recMOVNtemp();
}

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
