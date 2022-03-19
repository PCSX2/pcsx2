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
#ifndef SHIFT_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(SLL,    _Rd_);
REC_FUNC_DEL(SRL,    _Rd_);
REC_FUNC_DEL(SRA,    _Rd_);
REC_FUNC_DEL(DSLL,   _Rd_);
REC_FUNC_DEL(DSRL,   _Rd_);
REC_FUNC_DEL(DSRA,   _Rd_);
REC_FUNC_DEL(DSLL32, _Rd_);
REC_FUNC_DEL(DSRL32, _Rd_);
REC_FUNC_DEL(DSRA32, _Rd_);

REC_FUNC_DEL(SLLV,   _Rd_);
REC_FUNC_DEL(SRLV,   _Rd_);
REC_FUNC_DEL(SRAV,   _Rd_);
REC_FUNC_DEL(DSLLV,  _Rd_);
REC_FUNC_DEL(DSRLV,  _Rd_);
REC_FUNC_DEL(DSRAV,  _Rd_);

#else

//// SLL
void recSLL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << _Sa_);
}

void recSLLs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	if (sa != 0)
	{
		xSHL(eax, sa);
	}

	eeSignExtendTo(_Rd_);
}

void recSLL_(int info)
{
	recSLLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, SLL);

//// SRL
void recSRL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> _Sa_);
}

void recSRLs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	if (sa != 0)
		xSHR(eax, sa);

	eeSignExtendTo(_Rd_);
}

void recSRL_(int info)
{
	recSRLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, SRL);

//// SRA
void recSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> _Sa_);
}

void recSRAs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	if (sa != 0)
		xSAR(eax, sa);

	eeSignExtendTo(_Rd_);
}

void recSRA_(int info)
{
	recSRAs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, SRA);

////////////////////////////////////////////////////
void recDSLL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << _Sa_);
}

void recDSLLs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(rax, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	if (sa != 0)
		xSHL(rax, sa);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
}

void recDSLL_(int info)
{
	recDSLLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSLL);

////////////////////////////////////////////////////
void recDSRL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> _Sa_);
}

void recDSRLs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(rax, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	if (sa != 0)
		xSHR(rax, sa);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
}

void recDSRL_(int info)
{
	recDSRLs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSRL);

//// DSRA
void recDSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (u64)(g_cpuConstRegs[_Rt_].SD[0] >> _Sa_);
}

void recDSRAs_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(rax, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	if (sa != 0)
		xSAR(rax, sa);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
}

void recDSRA_(int info)
{
	recDSRAs_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSRA);

///// DSLL32
void recDSLL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (_Sa_ + 32));
}

void recDSLL32s_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	xSHL(rax, sa + 32);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
}

void recDSLL32_(int info)
{
	recDSLL32s_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSLL32);

//// DSRL32
void recDSRL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (_Sa_ + 32));
}

void recDSRL32s_(int info, int sa)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[1]]);
	if (sa != 0)
		xSHR(eax, sa);

	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
}

void recDSRL32_(int info)
{
	recDSRL32s_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSRL32);

//// DSRA32
void recDSRA32_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (u64)(g_cpuConstRegs[_Rt_].SD[0] >> (_Sa_ + 32));
}

void recDSRA32s_(int info, int sa)
{
	recDSRAs_(info, sa + 32);
}

void recDSRA32_(int info)
{
	recDSRA32s_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSRA32);

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/

static void recShiftV_constt(const xImpl_Group2& shift)
{
	xMOV(ecx, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);

	xMOV(eax, g_cpuConstRegs[_Rt_].UL[0]);
	shift(eax, cl);

	eeSignExtendTo(_Rd_);
}

static void recShiftV(const xImpl_Group2& shift)
{
	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	if (_Rs_ != 0)
	{
		xMOV(ecx, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		shift(eax, cl);
	}
	eeSignExtendTo(_Rd_);
}

static void recDShiftV_constt(const xImpl_Group2& shift)
{
	xMOV(ecx, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);

	xMOV64(rax, g_cpuConstRegs[_Rt_].UD[0]);
	shift(rax, cl);

	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
}

static void recDShiftV(const xImpl_Group2& shift)
{
	xMOV(rax, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	if (_Rs_ != 0)
	{
		xMOV(ecx, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
		shift(rax, cl);
	}
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
}

//// SLLV
void recSLLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

void recSLLV_consts(int info)
{
	recSLLs_(info, g_cpuConstRegs[_Rs_].UL[0] & 0x1f);
}

void recSLLV_constt(int info)
{
	recShiftV_constt(xSHL);
}

void recSLLV_(int info)
{
	recShiftV(xSHL);
}

EERECOMPILE_CODE0(SLLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SRLV
void recSRLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

void recSRLV_consts(int info)
{
	recSRLs_(info, g_cpuConstRegs[_Rs_].UL[0] & 0x1f);
}

void recSRLV_constt(int info)
{
	recShiftV_constt(xSHR);
}

void recSRLV_(int info)
{
	recShiftV(xSHR);
}

EERECOMPILE_CODE0(SRLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// SRAV
void recSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1f));
}

void recSRAV_consts(int info)
{
	recSRAs_(info, g_cpuConstRegs[_Rs_].UL[0] & 0x1f);
}

void recSRAV_constt(int info)
{
	recShiftV_constt(xSAR);
}

void recSRAV_(int info)
{
	recShiftV(xSAR);
}

EERECOMPILE_CODE0(SRAV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// DSLLV
void recDSLLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

void recDSLLV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	if (sa < 32)
		recDSLLs_(info, sa);
	else
		recDSLL32s_(info, sa - 32);
}

void recDSLLV_constt(int info)
{
	recDShiftV_constt(xSHL);
}

void recDSLLV_(int info)
{
	recDShiftV(xSHL);
}

EERECOMPILE_CODE0(DSLLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// DSRLV
void recDSRLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

void recDSRLV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	if (sa < 32)
		recDSRLs_(info, sa);
	else
		recDSRL32s_(info, sa - 32);
}

void recDSRLV_constt(int info)
{
	recDShiftV_constt(xSHR);
}

void recDSRLV_(int info)
{
	recDShiftV(xSHR);
}

EERECOMPILE_CODE0(DSRLV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

//// DSRAV
void recDSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s64)(g_cpuConstRegs[_Rt_].SD[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x3f));
}

void recDSRAV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0] & 0x3f;
	if (sa < 32)
		recDSRAs_(info, sa);
	else
		recDSRA32s_(info, sa - 32);
}

void recDSRAV_constt(int info)
{
	recDShiftV_constt(xSAR);
}

void recDSRAV_(int info)
{
	recDShiftV(xSAR);
}

EERECOMPILE_CODE0(DSRAV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
