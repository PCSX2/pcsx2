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

#ifdef __M_X86_64
	xMOV(rax, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	if (sa != 0)
		xSHL(rax, sa);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
#else
	int rtreg, rdreg;
	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	if (rtreg != rdreg)
		xMOVDQA(xRegisterSSE(rdreg), xRegisterSSE(rtreg));
	xPSLL.Q(xRegisterSSE(rdreg), sa);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#endif
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

#ifdef __M_X86_64
	xMOV(rax, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	if (sa != 0)
		xSHR(rax, sa);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
#else
	int rtreg, rdreg;
	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	if (rtreg != rdreg)
		xMOVDQA(xRegisterSSE(rdreg), xRegisterSSE(rtreg));
	xPSRL.Q(xRegisterSSE(rdreg), sa);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#endif
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

#ifdef __M_X86_64
	xMOV(rax, ptr[&cpuRegs.GPR.r[_Rt_].UD[0]]);
	if (sa != 0)
		xSAR(rax, sa);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
#else
	int rtreg, rdreg, t0reg;
	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	if (rtreg != rdreg)
		xMOVDQA(xRegisterSSE(rdreg), xRegisterSSE(rtreg));

	if (sa)
	{

		t0reg = _allocTempXMMreg(XMMT_INT, -1);

		xMOVDQA(xRegisterSSE(t0reg), xRegisterSSE(rtreg));

		// it is a signed shift (but 64 bits operands aren't supported on 32 bits even on SSE)
		xPSRA.D(xRegisterSSE(t0reg), sa);
		xPSRL.Q(xRegisterSSE(rdreg), sa);

		// It can be done in one blend instruction in SSE4.1
		// Goal is to move 63:32 of t0reg to 63:32 rdreg
		{
			xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(t0reg), 0x55);
			// take lower dword of rdreg and lower dword of t0reg
			xPUNPCK.LDQ(xRegisterSSE(rdreg), xRegisterSSE(t0reg));
		}

		_freeXMMreg(t0reg);
	}

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#endif
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
#ifdef __M_X86_64
	xSHL(rax, sa + 32);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
#else
	if (sa != 0)
	{
		xSHL(eax, sa);
	}
	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[0]], 0);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UL[1]], eax);
#endif
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

#ifdef __M_X86_64
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UD[0]], rax);
#else
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UL[0]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[_Rd_].UL[1]], 0);
#endif
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
#ifdef __M_X86_64
	recDSRAs_(info, sa + 32);
#else
	pxAssert(!(info & PROCESS_EE_XMM));

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rt_].UL[1]]);
	xCDQ();
	if (sa != 0)
		xSAR(eax, sa);

	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UL[0]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[_Rd_].UL[1]], edx);
#endif
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

#ifdef __M_X86_64

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

#else

__aligned16 u32 s_sa[4] = {0x1f, 0, 0x3f, 0};

void recSetShiftV(int info, int* rsreg, int* rtreg, int* rdreg, int* rstemp)
{
	pxAssert(!(info & PROCESS_EE_XMM));

	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	*rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	*rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	*rstemp = _allocTempXMMreg(XMMT_INT, -1);

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	xAND(eax, 0x3f);
	xMOVDZX(xRegisterSSE(*rstemp), eax);
	*rsreg = *rstemp;

	if (*rtreg != *rdreg)
		xMOVDQA(xRegisterSSE(*rdreg), xRegisterSSE(*rtreg));
}

void recSetConstShiftV(int info, int* rsreg, int* rdreg, int* rstemp)
{
	// Note: do it first.
	// 1/ It doesn't work in SSE if you did it in the end (I suspect
	// a conflict with _allocGPRtoXMMreg when rt==rd)
	// 2/ CPU has minimum cycle delay between read/write
	_flushConstReg(_Rt_);

	_addNeededGPRtoXMMreg(_Rd_);
	*rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	*rstemp = _allocTempXMMreg(XMMT_INT, -1);

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	xAND(eax, 0x3f);
	xMOVDZX(xRegisterSSE(*rstemp), eax);
	*rsreg = *rstemp;
}
#endif

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
#ifdef __M_X86_64
	recDShiftV_constt(xSHL);
#else
	int rsreg, rdreg, rstemp = -1;
	recSetConstShiftV(info, &rsreg, &rdreg, &rstemp);
	xMOVDQA(xRegisterSSE(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPSLL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if (rstemp != -1)
		_freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	//_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#endif
}

void recDSLLV_(int info)
{
#ifdef __M_X86_64
	recDShiftV(xSHL);
#else
	int rsreg, rtreg, rdreg, rstemp = -1;
	recSetShiftV(info, &rsreg, &rtreg, &rdreg, &rstemp);

	xPSLL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if (rstemp != -1)
		_freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#endif
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
#ifdef __M_X86_64
	recDShiftV_constt(xSHR);
#else
	int rsreg, rdreg, rstemp = -1;
	recSetConstShiftV(info, &rsreg, &rdreg, &rstemp);

	xMOVDQA(xRegisterSSE(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPSRL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if (rstemp != -1)
		_freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	//_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#endif
}

void recDSRLV_(int info)
{
#ifdef __M_X86_64
	recDShiftV(xSHR);
#else
	int rsreg, rtreg, rdreg, rstemp = -1;
	recSetShiftV(info, &rsreg, &rtreg, &rdreg, &rstemp);

	xPSRL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if (rstemp != -1)
		_freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#endif
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
#ifdef __M_X86_64
	recDShiftV_constt(xSAR);
#else
	int rsreg, rdreg, rstemp = -1, t0reg, t1reg;
	t0reg = _allocTempXMMreg(XMMT_INT, -1);
	t1reg = _allocTempXMMreg(XMMT_INT, -1);

	recSetConstShiftV(info, &rsreg, &rdreg, &rstemp);

	xMOVDQA(xRegisterSSE(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t0reg));

	// calc high bit
	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(rdreg));
	xPCMP.GTD(xRegisterSSE(t0reg), xRegisterSSE(rdreg));
	xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(t0reg), 0x55);

	// shift highest bit, 64 - eax
	xMOV(eax, 64);
	xMOVDZX(xRegisterSSE(t1reg), eax);
	xPSUB.D(xRegisterSSE(t1reg), xRegisterSSE(rsreg));

	// right logical shift
	xPSRL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	xPSLL.Q(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); // highest bits

	xPOR(xRegisterSSE(rdreg), xRegisterSSE(t0reg));

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rd_, 3);

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	if (rstemp != -1)
		_freeXMMreg(rstemp);
#endif
}

void recDSRAV_(int info)
{
#ifdef __M_X86_64
	recDShiftV(xSAR);
#else
	int rsreg, rtreg, rdreg, rstemp = -1, t0reg, t1reg;
	t0reg = _allocTempXMMreg(XMMT_INT, -1);
	t1reg = _allocTempXMMreg(XMMT_INT, -1);
	recSetShiftV(info, &rsreg, &rtreg, &rdreg, &rstemp);

	xPXOR(xRegisterSSE(t0reg), xRegisterSSE(t0reg));

	// calc high bit
	xMOVDQA(xRegisterSSE(t1reg), xRegisterSSE(rdreg));
	xPCMP.GTD(xRegisterSSE(t0reg), xRegisterSSE(rdreg));
	xPSHUF.D(xRegisterSSE(t0reg), xRegisterSSE(t0reg), 0x55);

	// shift highest bit, 64 - eax
	xMOV(eax, 64);
	xMOVDZX(xRegisterSSE(t1reg), eax);
	xPSUB.D(xRegisterSSE(t1reg), xRegisterSSE(rsreg));

	// right logical shift
	xPSRL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	xPSLL.Q(xRegisterSSE(t0reg), xRegisterSSE(t1reg)); // highest bits

	xPOR(xRegisterSSE(rdreg), xRegisterSSE(t0reg));

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[_Rd_].UD[0]], xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	if (rstemp != -1)
		_freeXMMreg(rstemp);
#endif
}

EERECOMPILE_CODE0(DSRAV, XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED);

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
