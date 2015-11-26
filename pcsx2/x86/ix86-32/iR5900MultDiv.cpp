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

namespace Interp = R5900::Interpreter::OpcodeImpl;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl
{

/*********************************************************
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
#ifndef MULTDIV_RECOMPILE

REC_FUNC_DEL(MULT , _Rd_);
REC_FUNC_DEL(MULTU , _Rd_);
REC_FUNC_DEL( MULT1  , _Rd_);
REC_FUNC_DEL( MULTU1 , _Rd_);

REC_FUNC(DIV);
REC_FUNC(DIVU);
REC_FUNC( DIV1 );
REC_FUNC( DIVU1 );

REC_FUNC_DEL( MADD , _Rd_ );
REC_FUNC_DEL( MADDU  , _Rd_);
REC_FUNC_DEL( MADD1  , _Rd_);
REC_FUNC_DEL( MADDU1 , _Rd_ );

#else

// if upper is 1, write in upper 64 bits of LO/HI
void recWritebackHILO(int info, int writed, int upper)
{
	int regd, reglo = -1, reghi, savedlo = 0;
	uptr loaddr = (uptr)&cpuRegs.LO.UL[ upper ? 2 : 0 ];
	uptr hiaddr = (uptr)&cpuRegs.HI.UL[ upper ? 2 : 0 ];
	u8 testlive = upper?EEINST_LIVE2:EEINST_LIVE0;

	if( g_pCurInstInfo->regs[XMMGPR_HI] & testlive )
		xMOV(ecx, edx);

	if( g_pCurInstInfo->regs[XMMGPR_LO] & testlive ) {

		_deleteMMXreg(XMMGPR_LO, 2);

		if( (reglo = _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_LO, MODE_READ)) >= 0 ) {
			if( xmmregs[reglo].mode & MODE_WRITE ) {
				if( upper ) xMOVQ(ptr[(void*)(loaddr-8)], xRegisterSSE(reglo));
				else xMOVH.PS(ptr[(void*)(loaddr+8)], xRegisterSSE(reglo));
			}

			xmmregs[reglo].inuse = 0;
			reglo = -1;
		}

		xCDQ();
		xMOV(ptr[(void*)(loaddr)], eax);
		xMOV(ptr[(void*)(loaddr+4)], edx);
		savedlo = 1;
	}

	if ( writed && _Rd_ )
	{
		_eeOnWriteReg(_Rd_, 1);

		regd = -1;
		if( g_pCurInstInfo->regs[_Rd_] & EEINST_XMM ) {
			if( savedlo ) {
				regd = _checkXMMreg(XMMTYPE_GPRREG, _Rd_, MODE_WRITE|MODE_READ);
				if( regd >= 0 ) {
					xMOVL.PS(xRegisterSSE(regd), ptr[(void*)(loaddr)]);
				}
			}
		}

		if( regd < 0 ) {
			_deleteEEreg(_Rd_, 0);

			if( !savedlo ) xCDQ();
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
		}
	}

	if( g_pCurInstInfo->regs[XMMGPR_HI] & testlive ) {
		_deleteMMXreg(XMMGPR_HI, 2);

		if( (reghi = _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_HI, MODE_READ)) >= 0 ) {
			if( xmmregs[reghi].mode & MODE_WRITE ) {
				if( upper ) xMOVQ(ptr[(void*)(hiaddr-8)], xRegisterSSE(reghi));
				else xMOVH.PS(ptr[(void*)(hiaddr+8)], xRegisterSSE(reghi));
			}

			xmmregs[reghi].inuse = 0;
			reghi = -1;
		}

		xMOV(ptr[(void*)(hiaddr)], ecx);
		xSAR(ecx, 31);
		xMOV(ptr[(void*)(hiaddr+4)], ecx);
	}
}

void recWritebackHILOMMX(int info, int regsource, int writed, int upper)
{
	int regd, t0reg, t1reg = -1;
	uptr loaddr = (uptr)&cpuRegs.LO.UL[ upper ? 2 : 0 ];
	uptr hiaddr = (uptr)&cpuRegs.HI.UL[ upper ? 2 : 0 ];
	u8 testlive = upper?EEINST_LIVE2:EEINST_LIVE0;

	SetMMXstate();

	t0reg = _allocMMXreg(-1, MMX_TEMP, 0);
	xMOVQ(xRegisterMMX(t0reg), xRegisterMMX(regsource));
	xPSRA.D(xRegisterMMX(t0reg), 31); // shift in 0s

	if( (g_pCurInstInfo->regs[XMMGPR_LO] & testlive) || (writed && _Rd_) ) {
		if( (g_pCurInstInfo->regs[XMMGPR_HI] & testlive) )
		{
			if( !_hasFreeMMXreg() ) {
				if( g_pCurInstInfo->regs[XMMGPR_HI] & testlive )
					_deleteMMXreg(MMX_GPR+MMX_HI, 2);
			}

			t1reg = _allocMMXreg(-1, MMX_TEMP, 0);
			xMOVQ(xRegisterMMX(t1reg), xRegisterMMX(regsource));
		}

		xPUNPCK.LDQ(xRegisterMMX(regsource), xRegisterMMX(t0reg));
	}

	if( g_pCurInstInfo->regs[XMMGPR_LO] & testlive ) {
		int reglo;
		if( !upper && (reglo = _allocCheckGPRtoMMX(g_pCurInstInfo, XMMGPR_LO, MODE_WRITE)) >= 0 ) {
			xMOVQ(xRegisterMMX(reglo), xRegisterMMX(regsource));
		}
		else {
			reglo = _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_LO, MODE_READ);

			xMOVQ(ptr[(void*)(loaddr)], xRegisterMMX(regsource));

			if( reglo >= 0 ) {
				if( xmmregs[reglo].mode & MODE_WRITE ) {
					if( upper ) xMOVQ(ptr[(void*)(loaddr-8)], xRegisterSSE(reglo));
					else xMOVH.PS(ptr[(void*)(loaddr+8)], xRegisterSSE(reglo));
				}
				xmmregs[reglo].inuse = 0;
				reglo = -1;
			}
		}
	}

	if ( writed && _Rd_ )
	{
		regd = _allocCheckGPRtoMMX(g_pCurInstInfo, _Rd_, MODE_WRITE);

		if( regd >= 0 ) {
			if( regd != regsource ) xMOVQ(xRegisterMMX(regd), xRegisterMMX(regsource));
		}
		else {
			regd = _checkXMMreg(XMMTYPE_GPRREG, _Rd_, MODE_READ);

			if( regd >= 0 ) {
				if( g_pCurInstInfo->regs[XMMGPR_LO] & testlive ) {
					// lo written
					xMOVL.PS(xRegisterSSE(regd), ptr[(void*)(loaddr)]);
					xmmregs[regd].mode |= MODE_WRITE;
				}
				else {
					xMOVQ(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], xRegisterMMX(regsource));

					if( xmmregs[regd].mode & MODE_WRITE ) {
						xMOVH.PS(ptr[&cpuRegs.GPR.r[_Rd_].UL[2]], xRegisterSSE(regd));
					}

					xmmregs[regd].inuse = 0;
				}
			}
			else {
				xMOVQ(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], xRegisterMMX(regsource));
			}
		}
	}

	if( g_pCurInstInfo->regs[XMMGPR_HI] & testlive ) {

		int mmreg = -1, reghi;

		if( t1reg >= 0 ) {
			xPUNPCK.HDQ(xRegisterMMX(t1reg), xRegisterMMX(t0reg));
			mmreg = t1reg;
		}
		else {
			// can't modify regsource
			xPUNPCK.HDQ(xRegisterMMX(t0reg), xRegisterMMX(regsource));
			mmreg = t0reg;
			xPSHUF.W(xRegisterMMX(t0reg), xRegisterMMX(t0reg), 0x4e);
		}

		if( upper ) {
			reghi = _checkXMMreg(XMMTYPE_GPRREG, XMMGPR_HI, MODE_READ);
			if( reghi >= 0 ) {
				if( xmmregs[reghi].mode & MODE_WRITE ) xMOVQ(ptr[(void*)(hiaddr-8)], xRegisterSSE(reghi));
				xmmregs[reghi].inuse = 0;
			}
						
			xMOVQ(ptr[(void*)(hiaddr)], xRegisterMMX(mmreg));
		}
		else {
			_deleteGPRtoXMMreg(XMMGPR_HI, 2);
			_deleteMMXreg(MMX_GPR+MMX_HI, 2);
			mmxregs[mmreg].mode = MODE_WRITE;
			mmxregs[mmreg].reg = MMX_GPR+MMX_HI;

			if( t1reg >= 0 ) t1reg = -1;
			else t0reg = -1;
		}
	}

	if( t0reg >= 0 ) _freeMMXreg(t0reg&0xf);
	if( t1reg >= 0 ) _freeMMXreg(t1reg&0xf);
}

void recWritebackConstHILO(u64 res, int writed, int upper)
{
	int reglo, reghi;
	uptr loaddr = (uptr)&cpuRegs.LO.UL[ upper ? 2 : 0 ];
	uptr hiaddr = (uptr)&cpuRegs.HI.UL[ upper ? 2 : 0 ];
	u8 testlive = upper?EEINST_LIVE2:EEINST_LIVE0;

	if( g_pCurInstInfo->regs[XMMGPR_LO] & testlive ) {
		if( !upper && (reglo = _allocCheckGPRtoMMX(g_pCurInstInfo, XMMGPR_LO, MODE_WRITE)) >= 0 ) {
			xMOVQ(xRegisterMMX(reglo), ptr[recGetImm64(res & 0x80000000 ? -1 : 0, (u32)res)]);
		}
		else {
			reglo = _allocCheckGPRtoXMM(g_pCurInstInfo, XMMGPR_LO, MODE_WRITE|MODE_READ);

			if( reglo >= 0 ) {
				u32* mem_ptr = recGetImm64(res & 0x80000000 ? -1 : 0, (u32)res);
				if( upper ) xMOVH.PS(xRegisterSSE(reglo), ptr[mem_ptr]);
				else xMOVL.PS(xRegisterSSE(reglo), ptr[mem_ptr]);
			}
			else {
				xMOV(ptr32[(u32*)(loaddr)], res & 0xffffffff);
				xMOV(ptr32[(u32*)(loaddr+4)], (res&0x80000000)?0xffffffff:0);
			}
		}
	}

	if( g_pCurInstInfo->regs[XMMGPR_HI] & testlive ) {

		if( !upper && (reghi = _allocCheckGPRtoMMX(g_pCurInstInfo, XMMGPR_HI, MODE_WRITE)) >= 0 ) {
			xMOVQ(xRegisterMMX(reghi), ptr[recGetImm64((res >> 63) ? -1 : 0, res >> 32)]);
		}
		else {
			reghi = _allocCheckGPRtoXMM(g_pCurInstInfo, XMMGPR_HI, MODE_WRITE|MODE_READ);

			if( reghi >= 0 ) {
				u32* mem_ptr = recGetImm64((res >> 63) ? -1 : 0, res >> 32);
				if( upper ) xMOVH.PS(xRegisterSSE(reghi), ptr[mem_ptr]);
				else xMOVL.PS(xRegisterSSE(reghi), ptr[mem_ptr]);
			}
			else {
				_deleteEEreg(XMMGPR_HI, 0);
				xMOV(ptr32[(u32*)(hiaddr)], res >> 32);
				xMOV(ptr32[(u32*)(hiaddr+4)], (res>>63)?0xffffffff:0);
			}
		}
	}

	if (!writed || !_Rd_) return;
	g_cpuConstRegs[_Rd_].UD[0] = (s32)(res & 0xffffffff); //that is the difference
}

//// MULT
void recMULT_const()
{
	s64 res = (s64)g_cpuConstRegs[_Rs_].SL[0] * (s64)g_cpuConstRegs[_Rt_].SL[0];

	recWritebackConstHILO(res, 1, 0);
}

void recMULTUsuper(int info, int upper, int process);
void recMULTsuper(int info, int upper, int process)
{
	if( process & PROCESS_CONSTS ) {
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0] );
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}
	else if( process & PROCESS_CONSTT) {
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
	}
	else {
		xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}

	recWritebackHILO(info, 1, upper);
}

//void recMULT_process(int info, int process)
//{
//	if( EEINST_ISLIVE64(XMMGPR_HI) ) {
//		recMULTsuper(info, 0, process);
//	}
//	else {
//		// EEREC_D isn't set
//		int mmregd;
//
//		if( _Rd_ ) {
//			assert(EEREC_D == 0);
//			mmregd = _checkMMXreg(MMX_GPR+_Rd_, MODE_WRITE);
//
//			if( mmregd < 0 ) {
//				if( !(process&PROCESS_CONSTS) && ((g_pCurInstInfo->regs[_Rs_]&EEINST_LASTUSE)||!EEINST_ISLIVE64(_Rs_)) ) {
//					_freeMMXreg(EEREC_S);
//					_deleteGPRtoXMMreg(_Rd_, 2);
//					mmxregs[EEREC_S].inuse = 1;
//					mmxregs[EEREC_S].reg = _Rd_;
//					mmxregs[EEREC_S].mode = MODE_WRITE;
//					mmregd = EEREC_S;
//				}
//				else if( !(process&PROCESS_CONSTT) && ((g_pCurInstInfo->regs[_Rt_]&EEINST_LASTUSE)||!EEINST_ISLIVE64(_Rt_)) ) {
//					_freeMMXreg(EEREC_T);
//					_deleteGPRtoXMMreg(_Rd_, 2);
//					mmxregs[EEREC_T].inuse = 1;
//					mmxregs[EEREC_T].reg = _Rd_;
//					mmxregs[EEREC_T].mode = MODE_WRITE;
//					mmregd = EEREC_T;
//				}
//				else mmregd = _allocMMXreg(-1, MMX_GPR+_Rd_, MODE_WRITE);
//			}
//
//			info |= PROCESS_EE_SET_D(mmregd);
//		}
//		recMULTUsuper(info, 0, process);
//	}
//
//	// sometimes _Rd_ can be const
//	if( _Rd_ ) _eeOnWriteReg(_Rd_, 1);
//}

void recMULT_(int info)
{
	//recMULT_process(info, 0);
	recMULTsuper(info, 0, 0);
}

void recMULT_consts(int info)
{
	//recMULT_process(info, PROCESS_CONSTS);
	recMULTsuper(info, 0, PROCESS_CONSTS);
}

void recMULT_constt(int info)
{
	//recMULT_process(info, PROCESS_CONSTT);
	recMULTsuper(info, 0, PROCESS_CONSTT);
}

// don't set XMMINFO_WRITED|XMMINFO_WRITELO|XMMINFO_WRITEHI
EERECOMPILE_CODE0(MULT, XMMINFO_READS|XMMINFO_READT|(_Rd_?XMMINFO_WRITED:0) );

//// MULTU
void recMULTU_const()
{
	u64 res = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

	recWritebackConstHILO(res, 1, 0);
}

void recMULTUsuper(int info, int upper, int process)
{
	if( process & PROCESS_CONSTS ) {
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0] );
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}
	else if( process & PROCESS_CONSTT) {
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
	}
	else {
		xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}

	recWritebackHILO(info, 1, upper);
}

void recMULTU_(int info)
{
	recMULTUsuper(info, 0, 0);
}

void recMULTU_consts(int info)
{
	recMULTUsuper(info, 0, PROCESS_CONSTS);
}

void recMULTU_constt(int info)
{
	recMULTUsuper(info, 0, PROCESS_CONSTT);
}

// don't specify XMMINFO_WRITELO or XMMINFO_WRITEHI, that is taken care of
EERECOMPILE_CODE0(MULTU, XMMINFO_READS|XMMINFO_READT|(_Rd_?XMMINFO_WRITED:0));

////////////////////////////////////////////////////
void recMULT1_const()
{
	u64 res = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

	recWritebackConstHILO(res, 1, 1);
}

void recMULT1_(int info)
{
	recMULTsuper(info, 1, 0);
}

void recMULT1_consts(int info)
{
	recMULTsuper(info, 1, PROCESS_CONSTS);
}

void recMULT1_constt(int info)
{
	recMULTsuper(info, 1, PROCESS_CONSTT);
}

EERECOMPILE_CODE0(MULT1, XMMINFO_READS|XMMINFO_READT|(_Rd_?XMMINFO_WRITED:0) );

////////////////////////////////////////////////////
void recMULTU1_const()
{
	u64 res = (u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0];

	recWritebackConstHILO(res, 1, 1);
}

void recMULTU1_(int info)
{
	recMULTUsuper(info, 1, 0);
}

void recMULTU1_consts(int info)
{
	recMULTUsuper(info, 1, PROCESS_CONSTS);
}

void recMULTU1_constt(int info)
{
	recMULTUsuper(info, 1, PROCESS_CONSTT);
}

EERECOMPILE_CODE0(MULTU1, XMMINFO_READS|XMMINFO_READT|(_Rd_?XMMINFO_WRITED:0));

//// DIV

void recDIVconst(int upper)
{
	s32 quot, rem;
	if (g_cpuConstRegs[_Rs_].UL[0] == 0x80000000 && g_cpuConstRegs[_Rt_].SL[0] == -1)
	{
		quot = (s32)0x80000000;
		rem = 0;
	}
	else if (g_cpuConstRegs[_Rt_].SL[0] != 0)
	{
        quot = g_cpuConstRegs[_Rs_].SL[0] / g_cpuConstRegs[_Rt_].SL[0];
        rem = g_cpuConstRegs[_Rs_].SL[0] % g_cpuConstRegs[_Rt_].SL[0];
    }
	else
	{
		quot = (g_cpuConstRegs[_Rs_].SL[0] < 0) ? 1 : -1;
		rem = g_cpuConstRegs[_Rs_].SL[0];
	}
	recWritebackConstHILO((u64)quot|((u64)rem<<32), 0, upper);
}

void recDIV_const()
{
	recDIVconst(0);
}

void recDIVsuper(int info, int sign, int upper, int process)
{
	if( process & PROCESS_CONSTT )
		xMOV(ecx, g_cpuConstRegs[_Rt_].UL[0] );
	else
		xMOV(ecx, ptr[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);

	if( process & PROCESS_CONSTS )
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0] );
	else
		xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);

	u8 *end1;
	if (sign)  //test for overflow (x86 will just throw an exception)
	{
		xCMP(eax, 0x80000000 );
		u8 *cont1 = JNE8(0);
		xCMP(ecx, 0xffffffff );
		u8 *cont2 = JNE8(0);
		//overflow case:
		xXOR(edx, edx); //EAX remains 0x80000000
		end1 = JMP8(0);

		x86SetJ8(cont1);
		x86SetJ8(cont2);
	}

	xCMP(ecx, 0 );
	u8 *cont3 = JNE8(0);
	//divide by zero
	xMOV(edx, eax);
	if (sign) //set EAX to (EAX < 0)?1:-1
	{
		xSAR(eax, 31 ); //(EAX < 0)?-1:0
		xSHL(eax, 1 ); //(EAX < 0)?-2:0
		xNOT(eax); //(EAX < 0)?1:-1
	}
	else
		xMOV(eax, 0xffffffff );
	u8 *end2 = JMP8(0);

	x86SetJ8(cont3);
	if( sign ) {
		xCDQ();
		xDIV(ecx);
	}
	else {
		xXOR(edx, edx);
		xUDIV(ecx);
	}

	if (sign) x86SetJ8( end1 );
	x86SetJ8( end2 );

	// need to execute regardless of bad divide
	recWritebackHILO(info, 0, upper);
}

void recDIV_(int info)
{
	recDIVsuper(info, 1, 0, 0);
}

void recDIV_consts(int info)
{
	recDIVsuper(info, 1, 0, PROCESS_CONSTS);
}

void recDIV_constt(int info)
{
	recDIVsuper(info, 1, 0, PROCESS_CONSTT);
}

EERECOMPILE_CODE0(DIV, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITELO|XMMINFO_WRITEHI);

//// DIVU
void recDIVUconst(int upper)
{
	u32 quot, rem;
	if (g_cpuConstRegs[_Rt_].UL[0] != 0) {
		quot = g_cpuConstRegs[_Rs_].UL[0] / g_cpuConstRegs[_Rt_].UL[0];
		rem = g_cpuConstRegs[_Rs_].UL[0] % g_cpuConstRegs[_Rt_].UL[0];
	}
	else
	{
		quot = 0xffffffff;
		rem = g_cpuConstRegs[_Rs_].UL[0];
	}

	recWritebackConstHILO((u64)quot|((u64)rem<<32), 0, upper);
}

void recDIVU_const()
{
	recDIVUconst(0);
}

void recDIVU_(int info)
{
	recDIVsuper(info, 0, 0, 0);
}

void recDIVU_consts(int info)
{
	recDIVsuper(info, 0, 0, PROCESS_CONSTS);
}

void recDIVU_constt(int info)
{
	recDIVsuper(info, 0, 0, PROCESS_CONSTT);
}

EERECOMPILE_CODE0(DIVU, XMMINFO_READS|XMMINFO_READT|XMMINFO_WRITELO|XMMINFO_WRITEHI);

void recDIV1_const()
{
	recDIVconst(1);
}

void recDIV1_(int info)
{
	recDIVsuper(info, 1, 1, 0);
}

void recDIV1_consts(int info)
{
	recDIVsuper(info, 1, 1, PROCESS_CONSTS);
}

void recDIV1_constt(int info)
{
	recDIVsuper(info, 1, 1, PROCESS_CONSTT);
}

EERECOMPILE_CODE0(DIV1, XMMINFO_READS|XMMINFO_READT);

void recDIVU1_const()
{
	recDIVUconst(1);
}

void recDIVU1_(int info)
{
	recDIVsuper(info, 0, 1, 0);
}

void recDIVU1_consts(int info)
{
	recDIVsuper(info, 0, 1, PROCESS_CONSTS);
}

void recDIVU1_constt(int info)
{
	recDIVsuper(info, 0, 1, PROCESS_CONSTT);
}

EERECOMPILE_CODE0(DIVU1, XMMINFO_READS|XMMINFO_READT);

void recMADD()
{
	if( GPR_IS_CONST2(_Rs_, _Rt_) ) {
		u64 result = ((s64)g_cpuConstRegs[_Rs_].SL[0] * (s64)g_cpuConstRegs[_Rt_].SL[0]);
		_deleteEEreg(XMMGPR_LO, 1);
		_deleteEEreg(XMMGPR_HI, 1);

		// dadd
		xMOV(eax, ptr[&cpuRegs.LO.UL[ 0 ] ]);
		xMOV(ecx, ptr[&cpuRegs.HI.UL[ 0 ] ]);
		xADD(eax, (u32)result&0xffffffff );
		xADC(ecx, (u32)(result>>32) );
		xCDQ();
		if( _Rd_) {
			_eeOnWriteReg(_Rd_, 1);
			_deleteEEreg(_Rd_, 0);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
		}

		xMOV(ptr[&cpuRegs.LO.UL[0]], eax);
		xMOV(ptr[&cpuRegs.LO.UL[1]], edx);

		xMOV(ptr[&cpuRegs.HI.UL[0]], ecx);
		xMOV(eax, ecx);
		xCDQ();
		xMOV(ptr[&cpuRegs.HI.UL[1]], edx);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoXMMreg(_Rs_, 1);
	_deleteGPRtoXMMreg(_Rt_, 1);
	_deleteMMXreg(MMX_GPR+_Rs_, 1);
	_deleteMMXreg(MMX_GPR+_Rt_, 1);

	if( GPR_IS_CONST1(_Rs_) ) {
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0] );
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}
	else if ( GPR_IS_CONST1(_Rt_) ) {
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
	}
	else {
		xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}

	xMOV(ecx, edx);
	xADD(eax, ptr[&cpuRegs.LO.UL[0] ]);
	xADC(ecx, ptr[&cpuRegs.HI.UL[0] ]);
	xCDQ();
	if( _Rd_ ) {
		_eeOnWriteReg(_Rd_, 1);
		_deleteEEreg(_Rd_, 0);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
	}

	xMOV(ptr[&cpuRegs.LO.UL[0]], eax);
	xMOV(ptr[&cpuRegs.LO.UL[1]], edx);

	xMOV(ptr[&cpuRegs.HI.UL[0]], ecx);
	xMOV(eax, ecx);
	xCDQ();
	xMOV(ptr[&cpuRegs.HI.UL[1]], edx);
}

//static PCSX2_ALIGNED16(u32 s_MaddMask[]) = { 0x80000000, 0, 0x80000000, 0 };

void recMADDU()
{
	if( GPR_IS_CONST2(_Rs_, _Rt_) ) {
		u64 result = ((u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0]);
		_deleteEEreg(XMMGPR_LO, 1);
		_deleteEEreg(XMMGPR_HI, 1);

		// dadd
		xMOV(eax, ptr[&cpuRegs.LO.UL[ 0 ] ]);
		xMOV(ecx, ptr[&cpuRegs.HI.UL[ 0 ] ]);
		xADD(eax, (u32)result&0xffffffff );
		xADC(ecx, (u32)(result>>32) );
		xCDQ();
		if( _Rd_) {
			_eeOnWriteReg(_Rd_, 1);
			_deleteEEreg(_Rd_, 0);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
		}

		xMOV(ptr[&cpuRegs.LO.UL[0]], eax);
		xMOV(ptr[&cpuRegs.LO.UL[1]], edx);

		xMOV(ptr[&cpuRegs.HI.UL[0]], ecx);
		xMOV(eax, ecx);
		xCDQ();
		xMOV(ptr[&cpuRegs.HI.UL[1]], edx);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoXMMreg(_Rs_, 1);
	_deleteGPRtoXMMreg(_Rt_, 1);
	_deleteMMXreg(MMX_GPR+_Rs_, 1);
	_deleteMMXreg(MMX_GPR+_Rt_, 1);

	if( GPR_IS_CONST1(_Rs_) ) {
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0] );
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}
	else if ( GPR_IS_CONST1(_Rt_) ) {
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
	}
	else {
		xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}

	xMOV(ecx, edx);
	xADD(eax, ptr[&cpuRegs.LO.UL[0] ]);
	xADC(ecx, ptr[&cpuRegs.HI.UL[0] ]);
	xCDQ();
	if( _Rd_ ) {
		_eeOnWriteReg(_Rd_, 1);
		_deleteEEreg(_Rd_, 0);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
	}

	xMOV(ptr[&cpuRegs.LO.UL[0]], eax);
	xMOV(ptr[&cpuRegs.LO.UL[1]], edx);

	xMOV(ptr[&cpuRegs.HI.UL[0]], ecx);
	xMOV(eax, ecx);
	xCDQ();
	xMOV(ptr[&cpuRegs.HI.UL[1]], edx);
}

void recMADD1()
{
	if( GPR_IS_CONST2(_Rs_, _Rt_) ) {
		u64 result = ((s64)g_cpuConstRegs[_Rs_].SL[0] * (s64)g_cpuConstRegs[_Rt_].SL[0]);
		_deleteEEreg(XMMGPR_LO, 1);
		_deleteEEreg(XMMGPR_HI, 1);

		// dadd
		xMOV(eax, ptr[&cpuRegs.LO.UL[ 2 ] ]);
		xMOV(ecx, ptr[&cpuRegs.HI.UL[ 2 ] ]);
		xADD(eax, (u32)result&0xffffffff );
		xADC(ecx, (u32)(result>>32) );
		xCDQ();
		if( _Rd_) {
			_eeOnWriteReg(_Rd_, 1);
			_deleteEEreg(_Rd_, 0);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
		}

		xMOV(ptr[&cpuRegs.LO.UL[2]], eax);
		xMOV(ptr[&cpuRegs.LO.UL[3]], edx);

		xMOV(ptr[&cpuRegs.HI.UL[2]], ecx);
		xMOV(eax, ecx);
		xCDQ();
		xMOV(ptr[&cpuRegs.HI.UL[3]], edx);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoXMMreg(_Rs_, 1);
	_deleteGPRtoXMMreg(_Rt_, 1);
	_deleteMMXreg(MMX_GPR+_Rs_, 1);
	_deleteMMXreg(MMX_GPR+_Rt_, 1);

	if( GPR_IS_CONST1(_Rs_) ) {
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0] );
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}
	else if ( GPR_IS_CONST1(_Rt_) ) {
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
	}
	else {
		xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}

	xMOV(ecx, edx);
	xADD(eax, ptr[&cpuRegs.LO.UL[2] ]);
	xADC(ecx, ptr[&cpuRegs.HI.UL[2] ]);
	xCDQ();
	if( _Rd_ ) {
		_eeOnWriteReg(_Rd_, 1);
		_deleteEEreg(_Rd_, 0);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
	}

	xMOV(ptr[&cpuRegs.LO.UL[2]], eax);
	xMOV(ptr[&cpuRegs.LO.UL[3]], edx);

	xMOV(ptr[&cpuRegs.HI.UL[2]], ecx);
	xMOV(eax, ecx);
	xCDQ();
	xMOV(ptr[&cpuRegs.HI.UL[3]], edx);
}

//static PCSX2_ALIGNED16(u32 s_MaddMask[]) = { 0x80000000, 0, 0x80000000, 0 };

void recMADDU1()
{
	if( GPR_IS_CONST2(_Rs_, _Rt_) ) {
		u64 result = ((u64)g_cpuConstRegs[_Rs_].UL[0] * (u64)g_cpuConstRegs[_Rt_].UL[0]);
		_deleteEEreg(XMMGPR_LO, 1);
		_deleteEEreg(XMMGPR_HI, 1);

		// dadd
		xMOV(eax, ptr[&cpuRegs.LO.UL[ 2 ] ]);
		xMOV(ecx, ptr[&cpuRegs.HI.UL[ 2 ] ]);
		xADD(eax, (u32)result&0xffffffff );
		xADC(ecx, (u32)(result>>32) );
		xCDQ();
		if( _Rd_) {
			_eeOnWriteReg(_Rd_, 1);
			_deleteEEreg(_Rd_, 0);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
			xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
		}

		xMOV(ptr[&cpuRegs.LO.UL[2]], eax);
		xMOV(ptr[&cpuRegs.LO.UL[3]], edx);

		xMOV(ptr[&cpuRegs.HI.UL[2]], ecx);
		xMOV(eax, ecx);
		xCDQ();
		xMOV(ptr[&cpuRegs.HI.UL[3]], edx);
		return;
	}

	_deleteEEreg(XMMGPR_LO, 1);
	_deleteEEreg(XMMGPR_HI, 1);
	_deleteGPRtoXMMreg(_Rs_, 1);
	_deleteGPRtoXMMreg(_Rt_, 1);
	_deleteMMXreg(MMX_GPR+_Rs_, 1);
	_deleteMMXreg(MMX_GPR+_Rt_, 1);

	if( GPR_IS_CONST1(_Rs_) ) {
		xMOV(eax, g_cpuConstRegs[_Rs_].UL[0] );
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}
	else if ( GPR_IS_CONST1(_Rt_) ) {
		xMOV(eax, g_cpuConstRegs[_Rt_].UL[0] );
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
	}
	else {
		xMOV(eax, ptr[&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] ]);
		xUMUL(ptr32[&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] ]);
	}

	xMOV(ecx, edx);
	xADD(eax, ptr[&cpuRegs.LO.UL[2] ]);
	xADC(ecx, ptr[&cpuRegs.HI.UL[2] ]);
	xCDQ();
	if( _Rd_ ) {
		_eeOnWriteReg(_Rd_, 1);
		_deleteEEreg(_Rd_, 0);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ]], eax);
		xMOV(ptr[&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ]], edx);
	}

	xMOV(ptr[&cpuRegs.LO.UL[2]], eax);
	xMOV(ptr[&cpuRegs.LO.UL[3]], edx);

	xMOV(ptr[&cpuRegs.HI.UL[2]], ecx);
	xMOV(eax, ecx);
	xCDQ();
	xMOV(ptr[&cpuRegs.HI.UL[3]], edx);
}


#endif

} } }
