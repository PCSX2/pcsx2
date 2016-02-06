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

#define NO_MMX 0

using namespace x86Emitter;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl
{

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
#ifndef SHIFT_RECOMPILE

namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC_DEL(SLL, _Rd_);
REC_FUNC_DEL(SRL, _Rd_);
REC_FUNC_DEL(SRA, _Rd_);
REC_FUNC_DEL(DSLL, _Rd_);
REC_FUNC_DEL(DSRL, _Rd_);
REC_FUNC_DEL(DSRA, _Rd_);
REC_FUNC_DEL(DSLL32, _Rd_);
REC_FUNC_DEL(DSRL32, _Rd_);
REC_FUNC_DEL(DSRA32, _Rd_);

REC_FUNC_DEL(SLLV, _Rd_);
REC_FUNC_DEL(SRLV, _Rd_);
REC_FUNC_DEL(SRAV, _Rd_);
REC_FUNC_DEL(DSLLV, _Rd_);
REC_FUNC_DEL(DSRLV, _Rd_);
REC_FUNC_DEL(DSRAV, _Rd_);

#else

//// SLL
void recSLL_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << _Sa_);
}

void recSLLs_(int info, int sa)
{
	pxAssert( !(info & PROCESS_EE_XMM) );

	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	if ( sa != 0 )
	{
		xSHL(eax, sa );
	}

	xCDQ( );
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
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
	pxAssert( !(info & PROCESS_EE_XMM) );

	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	if ( sa != 0 ) xSHR(eax, sa);

	xCDQ( );
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
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
	pxAssert( !(info & PROCESS_EE_XMM) );

	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	if ( sa != 0 ) xSAR(eax, sa);

	xCDQ();
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
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
	int rtreg, rdreg;
	pxAssert( !(info & PROCESS_EE_XMM) );

#if NO_MMX
	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	if( rtreg != rdreg ) xMOVDQA(xRegisterSSE(rdreg), xRegisterSSE(rtreg));
	xPSLL.Q(xRegisterSSE(rdreg), sa);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#else
	_addNeededMMXreg(MMX_GPR+_Rt_);
	_addNeededMMXreg(MMX_GPR+_Rd_);
	rtreg = _allocMMXreg(-1, MMX_GPR+_Rt_, MODE_READ);
	rdreg = _allocMMXreg(-1, MMX_GPR+_Rd_, MODE_WRITE);
	SetMMXstate();

	if( rtreg != rdreg ) xMOVQ(xRegisterMMX(rdreg), xRegisterMMX(rtreg));
	xPSLL.Q(xRegisterMMX(rdreg), sa);
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
	int rtreg, rdreg;
	pxAssert( !(info & PROCESS_EE_XMM) );

#if NO_MMX
	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	if( rtreg != rdreg ) xMOVDQA(xRegisterSSE(rdreg), xRegisterSSE(rtreg));
	xPSRL.Q(xRegisterSSE(rdreg), sa);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#else
	_addNeededMMXreg(MMX_GPR+_Rt_);
	_addNeededMMXreg(MMX_GPR+_Rd_);
	rtreg = _allocMMXreg(-1, MMX_GPR+_Rt_, MODE_READ);
	rdreg = _allocMMXreg(-1, MMX_GPR+_Rd_, MODE_WRITE);
	SetMMXstate();

	if( rtreg != rdreg ) xMOVQ(xRegisterMMX(rdreg), xRegisterMMX(rtreg));
	xPSRL.Q(xRegisterMMX(rdreg), sa);
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
	int rtreg, rdreg, t0reg;
	pxAssert( !(info & PROCESS_EE_XMM) );

#if NO_MMX
	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	if( rtreg != rdreg ) xMOVDQA(xRegisterSSE(rdreg), xRegisterSSE(rtreg));

	if ( sa )  {

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
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#else
	_addNeededMMXreg(MMX_GPR+_Rt_);
	_addNeededMMXreg(MMX_GPR+_Rd_);
	rtreg = _allocMMXreg(-1, MMX_GPR+_Rt_, MODE_READ);
	rdreg = _allocMMXreg(-1, MMX_GPR+_Rd_, MODE_WRITE);
	SetMMXstate();

	if( rtreg != rdreg ) xMOVQ(xRegisterMMX(rdreg), xRegisterMMX(rtreg));

    if ( sa != 0 ) {
		t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
		xMOVQ(xRegisterMMX(t0reg), xRegisterMMX(rtreg));

		// it is a signed shift
		xPSRA.D(xRegisterMMX(t0reg), sa);
		xPSRL.Q(xRegisterMMX(rdreg), sa);

		xPUNPCK.HDQ(xRegisterMMX(t0reg), xRegisterMMX(t0reg)); // shift to lower
		// take lower dword of rdreg and lower dword of t0reg
		xPUNPCK.LDQ(xRegisterMMX(rdreg), xRegisterMMX(t0reg));

		_freeMMXreg(t0reg);
	}
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
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (_Sa_+32));
}

void recDSLL32s_(int info, int sa)
{
	pxAssert( !(info & PROCESS_EE_XMM) );

	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	if ( sa != 0 )
	{
		xSHL(eax, sa );
	}
	xMOV(ptr32[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], 0 );
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], eax);

}

void recDSLL32_(int info)
{
	recDSLL32s_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSLL32);

//// DSRL32
void recDSRL32_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (_Sa_+32));
}

void recDSRL32s_(int info, int sa)
{
	pxAssert( !(info & PROCESS_EE_XMM) );

	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] ]);
	if ( sa != 0 ) xSHR(eax, sa );

	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr32[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], 0 );
}

void recDSRL32_(int info)
{
	recDSRL32s_(info, _Sa_);
}

EERECOMPILE_CODEX(eeRecompileCode2, DSRL32);

//// DSRA32
void recDSRA32_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (u64)(g_cpuConstRegs[_Rt_].SD[0] >> (_Sa_+32));
}

void recDSRA32s_(int info, int sa)
{
	pxAssert( !(info & PROCESS_EE_XMM) );

	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] ]);
	xCDQ( );
	if ( sa != 0 ) xSAR(eax, sa );

	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);

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

__aligned16 u32 s_sa[4] = {0x1f, 0, 0x3f, 0};

void recSetShiftV(int info, int* rsreg, int* rtreg, int* rdreg, int* rstemp)
{
	pxAssert( !(info & PROCESS_EE_XMM) );

#if NO_MMX
	_addNeededGPRtoXMMreg(_Rt_);
	_addNeededGPRtoXMMreg(_Rd_);
	*rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	*rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	*rstemp = _allocTempXMMreg(XMMT_INT, -1);

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	xAND(eax, 0x3f);
	xMOVDZX(xRegisterSSE(*rstemp), eax);
	*rsreg = *rstemp;

	if( *rtreg != *rdreg ) xMOVDQA(xRegisterSSE(*rdreg), xRegisterSSE(*rtreg));
#else
	_addNeededMMXreg(MMX_GPR+_Rt_);
	_addNeededMMXreg(MMX_GPR+_Rd_);
	*rtreg = _allocMMXreg(-1, MMX_GPR+_Rt_, MODE_READ);
	*rdreg = _allocMMXreg(-1, MMX_GPR+_Rd_, MODE_WRITE);
	SetMMXstate();

	*rstemp = _allocMMXreg(-1, MMX_TEMP, 0);
	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	xAND(eax, 0x3f);
	xMOVDZX(xRegisterMMX(*rstemp), eax);
	*rsreg = *rstemp;

	if( *rtreg != *rdreg ) xMOVQ(xRegisterMMX(*rdreg), xRegisterMMX(*rtreg));
#endif
}

void recSetConstShiftV(int info, int* rsreg, int* rdreg, int* rstemp)
{
#if NO_MMX
	_addNeededGPRtoXMMreg(_Rd_);
	*rdreg = _allocGPRtoXMMreg(-1, _Rd_, MODE_WRITE);

	*rstemp = _allocTempXMMreg(XMMT_INT, -1);

	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	xAND(eax, 0x3f);
	xMOVDZX(xRegisterSSE(*rstemp), eax);
	*rsreg = *rstemp;

	_flushConstReg(_Rt_);
#else
	_addNeededMMXreg(MMX_GPR+_Rd_);
	*rdreg = _allocMMXreg(-1, MMX_GPR+_Rd_, MODE_WRITE);
	SetMMXstate();

	*rstemp = _allocMMXreg(-1, MMX_TEMP, 0);
	xMOV(eax, ptr[&cpuRegs.GPR.r[_Rs_].UL[0]]);
	xAND(eax, 0x3f);
	xMOVDZX(xRegisterMMX(*rstemp), eax);
	*rsreg = *rstemp;

	_flushConstReg(_Rt_);
#endif
}

//// SLLV
void recSLLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] << (g_cpuConstRegs[_Rs_].UL[0] &0x1f));
}

void recSLLV_consts(int info)
{
	recSLLs_(info, g_cpuConstRegs[_Rs_].UL[0]&0x1f);
}

void recSLLV_constt(int info)
{
	xMOV(ecx, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);

	xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
	xAND(ecx, 0x1f );
	xSHL(eax, cl);

	eeSignExtendTo(_Rd_);
}

void recSLLV_(int info)
{
	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	if ( _Rs_ != 0 )
	{
		xMOV(ecx, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xAND(ecx, 0x1f );
		xSHL(eax, cl);
	}
	xCDQ();
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
}

EERECOMPILE_CODE0(SLLV, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITED);

//// SRLV
void recSRLV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].UL[0] >> (g_cpuConstRegs[_Rs_].UL[0] &0x1f));
}

void recSRLV_consts(int info)
{
	recSRLs_(info, g_cpuConstRegs[_Rs_].UL[0]&0x1f);
}

void recSRLV_constt(int info)
{
	xMOV(ecx, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);

	xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
	xAND(ecx, 0x1f );
	xSHR(eax, cl);

	eeSignExtendTo(_Rd_);
}

void recSRLV_(int info)
{
	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	if ( _Rs_ != 0 )
	{
		xMOV(ecx, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xAND(ecx, 0x1f );
		xSHR(eax, cl);
	}
	xCDQ( );
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
}

EERECOMPILE_CODE0(SRLV, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITED);

//// SRAV
void recSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> (g_cpuConstRegs[_Rs_].UL[0] &0x1f));
}

void recSRAV_consts(int info)
{
	recSRAs_(info, g_cpuConstRegs[_Rs_].UL[0]&0x1f);
}

void recSRAV_constt(int info)
{
	xMOV(ecx, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);

	xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
	xAND(ecx, 0x1f );
	xSAR(eax, cl);

	eeSignExtendTo(_Rd_);
}

void recSRAV_(int info)
{
	xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	if ( _Rs_ != 0 )
	{
		xMOV(ecx, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xAND(ecx, 0x1f );
		xSAR(eax, cl);
	}
	xCDQ( );
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
	xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
}

EERECOMPILE_CODE0(SRAV, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITED);

//// DSLLV
void recDSLLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] << (g_cpuConstRegs[_Rs_].UL[0] &0x3f));
}

void recDSLLV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0]&0x3f;
	if( sa < 32 ) recDSLLs_(info, sa);
	else recDSLL32s_(info, sa-32);
}

void recDSLLV_constt(int info)
{
	int rsreg, rdreg, rstemp = -1;
	recSetConstShiftV(info, &rsreg, &rdreg, &rstemp);
#if NO_MMX
	xMOVDQA(xRegisterSSE(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPSLL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if( rstemp != -1 ) _freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#else

	xMOVQ(xRegisterMMX(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPSLL.Q(xRegisterMMX(rdreg), xRegisterMMX(rsreg));
	if( rstemp != -1 ) _freeMMXreg(rstemp);
#endif
}

void recDSLLV_(int info)
{
	int rsreg, rtreg, rdreg, rstemp = -1;
	recSetShiftV(info, &rsreg, &rtreg, &rdreg, &rstemp);

#if NO_MMX
	xPSLL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if( rstemp != -1 ) _freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#else
	xPSLL.Q(xRegisterMMX(rdreg), xRegisterMMX(rsreg));
	if( rstemp != -1 ) _freeMMXreg(rstemp);
#endif
}

EERECOMPILE_CODE0(DSLLV, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITED);

//// DSRLV
void recDSRLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u64)(g_cpuConstRegs[_Rt_].UD[0] >> (g_cpuConstRegs[_Rs_].UL[0] &0x3f));
}

void recDSRLV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0]&0x3f;
	if( sa < 32 ) recDSRLs_(info, sa);
	else recDSRL32s_(info, sa-32);
}

void recDSRLV_constt(int info)
{
	int rsreg, rdreg, rstemp = -1;
	recSetConstShiftV(info, &rsreg, &rdreg, &rstemp);

#if NO_MMX
	xMOVDQA(xRegisterSSE(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPSRL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if( rstemp != -1 ) _freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#else
	xMOVQ(xRegisterMMX(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPSRL.Q(xRegisterMMX(rdreg), xRegisterMMX(rsreg));
	if( rstemp != -1 ) _freeMMXreg(rstemp);
#endif
}

void recDSRLV_(int info)
{
	int rsreg, rtreg, rdreg, rstemp = -1;
	recSetShiftV(info, &rsreg, &rtreg, &rdreg, &rstemp);

#if NO_MMX
	xPSRL.Q(xRegisterSSE(rdreg), xRegisterSSE(rsreg));
	if( rstemp != -1 ) _freeXMMreg(rstemp);

	// flush lower 64 bits (as upper is wrong)
	// The others possibility could be a read back of the upper 64 bits
	// (better use of register but code will likely be flushed after anyway)
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);
#else
	xPSRL.Q(xRegisterMMX(rdreg), xRegisterMMX(rsreg));
	if( rstemp != -1 ) _freeMMXreg(rstemp);
#endif
}

EERECOMPILE_CODE0(DSRLV, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITED);

//// DSRAV
void recDSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s64)(g_cpuConstRegs[_Rt_].SD[0] >> (g_cpuConstRegs[_Rs_].UL[0] &0x3f));
}

void recDSRAV_consts(int info)
{
	int sa = g_cpuConstRegs[_Rs_].UL[0]&0x3f;
	if( sa < 32 ) recDSRAs_(info, sa);
	else recDSRA32s_(info, sa-32);
}

void recDSRAV_constt(int info)
{
	int rsreg, rdreg, rstemp = -1, t0reg, t1reg;
#if NO_MMX
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
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rd_, 3);

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	if( rstemp != -1 ) _freeXMMreg(rstemp);
#else
	t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
	t1reg = _allocMMXreg(-1, MMX_TEMP, 0);

	recSetConstShiftV(info, &rsreg, &rdreg, &rstemp);

	xMOVQ(xRegisterMMX(rdreg), ptr[&cpuRegs.GPR.r[_Rt_]]);
	xPXOR(xRegisterMMX(t0reg), xRegisterMMX(t0reg));

	// calc high bit
	xMOVQ(xRegisterMMX(t1reg), xRegisterMMX(rdreg));
	xPCMP.GTD(xRegisterMMX(t0reg), xRegisterMMX(rdreg));
	xPUNPCK.HDQ(xRegisterMMX(t0reg), xRegisterMMX(t0reg)); // shift to lower

	// shift highest bit, 64 - eax
	xMOV(eax, 64);
	xMOVDZX(xRegisterMMX(t1reg), eax);
	xPSUB.D(xRegisterMMX(t1reg), xRegisterMMX(rsreg));

	// right logical shift
	xPSRL.Q(xRegisterMMX(rdreg), xRegisterMMX(rsreg));
	xPSLL.Q(xRegisterMMX(t0reg), xRegisterMMX(t1reg)); // highest bits

	xPOR(xRegisterMMX(rdreg), xRegisterMMX(t0reg));

	_freeMMXreg(t0reg);
	_freeMMXreg(t1reg);
	if( rstemp != -1 ) _freeMMXreg(rstemp);
#endif
}

void recDSRAV_(int info)
{
	int rsreg, rtreg, rdreg, rstemp = -1, t0reg, t1reg;
#if NO_MMX
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
	xMOVL.PD(ptr64[&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ]] , xRegisterSSE(rdreg));
	_deleteGPRtoXMMreg(_Rt_, 3);
	_deleteGPRtoXMMreg(_Rd_, 3);

	_freeXMMreg(t0reg);
	_freeXMMreg(t1reg);
	if( rstemp != -1 ) _freeXMMreg(rstemp);
#else
	t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
	t1reg = _allocMMXreg(-1, MMX_TEMP, 0);
	recSetShiftV(info, &rsreg, &rtreg, &rdreg, &rstemp);

	xPXOR(xRegisterMMX(t0reg), xRegisterMMX(t0reg));

	// calc high bit
	xMOVQ(xRegisterMMX(t1reg), xRegisterMMX(rdreg));
	xPCMP.GTD(xRegisterMMX(t0reg), xRegisterMMX(rdreg));
	xPUNPCK.HDQ(xRegisterMMX(t0reg), xRegisterMMX(t0reg)); // shift to lower

	// shift highest bit, 64 - eax
	xMOV(eax, 64);
	xMOVDZX(xRegisterMMX(t1reg), eax);
	xPSUB.D(xRegisterMMX(t1reg), xRegisterMMX(rsreg));

	// right logical shift
	xPSRL.Q(xRegisterMMX(rdreg), xRegisterMMX(rsreg));
	xPSLL.Q(xRegisterMMX(t0reg), xRegisterMMX(t1reg)); // highest bits

	xPOR(xRegisterMMX(rdreg), xRegisterMMX(t0reg));

	_freeMMXreg(t0reg);
	_freeMMXreg(t1reg);
	if( rstemp != -1 ) _freeMMXreg(rstemp);
#endif
}

EERECOMPILE_CODE0(DSRAV, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITED);

#endif

} } }
