/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2003  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "Common.h"
#include "InterTables.h"
#include "ix86/ix86.h"
#include "iR5900.h"
#include "VU0.h"

#ifdef __WIN32__
#pragma warning(disable:4244)
#pragma warning(disable:4761)
#endif

/*********************************************************
* Load and store for GPR                                 *
* Format:  OP rt, offset(base)                           *
*********************************************************/
#ifndef LOADSTORE_RECOMPILE

REC_FUNC(LB);
REC_FUNC(LBU);
REC_FUNC(LH);
REC_FUNC(LHU);
REC_FUNC(LW);
REC_FUNC(LWU);
REC_FUNC(LWL);
REC_FUNC(LWR);
REC_FUNC(LD);
REC_FUNC(LDR);
REC_FUNC(LDL);
REC_FUNC(LQ);
REC_FUNC(SB);
REC_FUNC(SH);
REC_FUNC(SW);
REC_FUNC(SWL);
REC_FUNC(SWR);
REC_FUNC(SD);
REC_FUNC(SDL);
REC_FUNC(SDR);
REC_FUNC(SQ);
REC_FUNC(LWC1);
REC_FUNC(SWC1);
REC_FUNC(LQC2);
REC_FUNC(SQC2);

#else
REC_FUNC(LWC1);
REC_FUNC(SWC1);
REC_FUNC(LQC2);
REC_FUNC(SQC2);

u64 retValue;
u64 dummyValue[ 4 ];

////////////////////////////////////////////////////
void recLB( void ) {
	iFlushCall();

	MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	if ( _Imm_ != 0 ) {
		ADD32ItoR( EDI, _Imm_ );
	}

	if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	} else {
		MOV32ItoR( ESI, &dummyValue );
	}
	CALLFunc( (u32)memRead8RS );
}

////////////////////////////////////////////////////
void recLBU( void ) {
	iFlushCall();

	MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
   if ( _Imm_ != 0 )
   {
	   ADD32ItoR( EDI, _Imm_ );
   }
      if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
      } else {
		MOV32ItoR( ESI, &dummyValue );
      }
   iFlushCall();
   CALLFunc( (u32)memRead8RU );
}

////////////////////////////////////////////////////
void recLH( void ) {
	iFlushCall();

   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_ );
	   }
      if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
      } else {
		MOV32ItoR( ESI, &dummyValue );
      }
	iFlushCall();
	CALLFunc( (u32)memRead16RS );
}

////////////////////////////////////////////////////
void recLHU( void ) {
	iFlushCall();

	MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	if ( _Imm_ != 0 ) {
		ADD32ItoR( EDI, _Imm_ );
	}

	if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	} else {
		MOV32ItoR( ESI, &dummyValue );
	}
	CALLFunc( (u32)memRead16RU );
}

void tests() {
	SysPrintf("Err\n");
}

////////////////////////////////////////////////////
void recLW( void ) {
	int rsreg;
	int rtreg;
	int t0reg;
	int t1reg;
	int t2reg;

#if 0
//def ENABLE_REGCACHING

	_freeX86regs();
	_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
	rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
	t0reg = _allocTempX86reg(-1);
	t1reg = _allocTempX86reg(-1);
	t2reg = _allocTempX86reg(-1);

	MOV32RtoR(t0reg, rsreg);
	if (_Imm_ != 0) {
		ADD32ItoR(t0reg, _Imm_);
	}
	MOV32RtoR(t2reg, t0reg);
	SHR32ItoR(t0reg, 12);

	MOV64MtoR(t1reg, (u32)&memLUTR);
	MOV64RmStoR(t1reg, t1reg, t0reg, 3);

	CMP64ItoR(t1reg, 0x10);
	j8Ptr[0] = JL8(0);

	rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);
	AND32ItoR(t2reg, 0xfff);
	LogX86();
	MOV64RmStoR(rtreg, t1reg, t2reg, 0);
	_clearNeededX86regs();
	_freeX86regs();

//	j8Ptr[1] = JMP8(0);

	x86SetJ8(j8Ptr[0]);

	iFlushCall();

	MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	if ( _Imm_ != 0 ) {
		ADD32ItoR( EDI, _Imm_ );
	}

	if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	} else {
		MOV32ItoR( ESI, &dummyValue );
	}
	CALLFunc( (u32)memRead32RS );

//	x86SetJ8(j8Ptr[1]);

#else

	iFlushCall();

	MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	if ( _Imm_ != 0 ) {
		ADD32ItoR( EDI, _Imm_ );
	}

	if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	} else {
		MOV32ItoR( ESI, &dummyValue );
	}
	CALLFunc( (u32)memRead32RS );

#endif
}

////////////////////////////////////////////////////
void recLWU( void ) {
	iFlushCall();

	   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_ );
	   }
      if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
      } else {
		MOV32ItoR( ESI, &dummyValue );
      }
	   CALLFunc( (u32)memRead32RU );
}

////////////////////////////////////////////////////
void recLWL( void ) 
{
	iFlushCall();

	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   CALLFunc( (u32)LWL );
}

////////////////////////////////////////////////////
void recLWR( void ) { 
	iFlushCall();

	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   CALLFunc( (u32)LWR );
}

////////////////////////////////////////////////////
void recLD( void ) {
	iFlushCall();

	   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_ );
	   }
      if ( _Rt_ )
      {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
      }
      else
      {
		MOV32ItoR( ESI, &dummyValue );
      }
	   CALLFunc( (u32)memRead64 );
}

////////////////////////////////////////////////////
void recLDL( void ) {
	iFlushCall();

	MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	MOV32ItoM( (u32)&cpuRegs.pc, pc );
	CALLFunc( (u32)LDL );
}

////////////////////////////////////////////////////
void recLDR( void ) {
	iFlushCall();

	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   CALLFunc( (u32)LDR );
}

////////////////////////////////////////////////////
void recLQ( void ) {
	iFlushCall();

	   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_);
	   }
	   AND32ItoR( EDI, ~0xf );

      if ( _Rt_ ) {
		MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
      } else {
		MOV32ItoR( ESI, &dummyValue );
      }
	   CALLFunc( (u32)memRead128 );
}

////////////////////////////////////////////////////
void recSB( void ) {
	iFlushCall();

	MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_);
	   }
	MOV32MtoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	   CALLFunc( (u32)memWrite8 );
}

////////////////////////////////////////////////////
void recSH( void ) {
	iFlushCall();

	   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_ );
	   }
	MOV32MtoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	   CALLFunc( (u32)memWrite16 );
}

////////////////////////////////////////////////////
void recSW( void ) {
	iFlushCall();

	   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_ );
	   }
	MOV32MtoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	   CALLFunc( (u32)memWrite32 );
}

////////////////////////////////////////////////////
void recSWL( void ) {
	iFlushCall();

	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   CALLFunc( (u32)SWL );
}

////////////////////////////////////////////////////
void recSWR( void ) {
	iFlushCall();

	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
   	CALLFunc( (u32)SWR );
}

////////////////////////////////////////////////////
void recSD( void ) {
	iFlushCall();

	   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_ );
	   }
	MOV64MtoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	   CALLFunc( (u32)memWrite64 );
}

////////////////////////////////////////////////////
void recSDL( void ) {
	iFlushCall();

	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   CALLFunc( (u32)SDL );
}

////////////////////////////////////////////////////
void recSDR( void ) {
	iFlushCall();

	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   CALLFunc( (u32)SDR );
}

////////////////////////////////////////////////////
void recSQ( void ) {
	iFlushCall();

	   MOV32MtoR( EDI, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EDI, _Imm_ );
	   }
	   AND32ItoR( EDI, ~0xf );

	MOV32ItoR( ESI, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ] );
	   CALLFunc( (u32)memWrite128 );
}
/*********************************************************
* Load and store for COP1                                *
* Format:  OP rt, offset(base)                           *
*********************************************************/
/*
////////////////////////////////////////////////////
void recLWC1( void )
{
   if ( Config.Regcaching )
   {
      GRec_Instruction( GREC_INST_MOVIMM32, &cpuRegs.code, cpuRegs.code );
      GRec_Instruction( GREC_INST_MOVIMM32, &cpuRegs.pc, pc );

      GRec_Instruction( GREC_INST_CALL, LWC1, NULL, 0, GREC_TRUE );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )	
	   {
		   ADD32ItoR( EAX, _Imm_ );
	   }
      PUSH32I( (u32)&fpuRegs.fpr[ _Rt_ ].UL );
	   PUSH32R( EAX );
	   iFlushCall();
	   CALLFunc( (u32)memRead32 );
      ADD32ItoR( ESP, 8 );
   }
}

////////////////////////////////////////////////////
void recSWC1( void )
{
   if ( Config.Regcaching )
   {
      GRec_Instruction( GREC_INST_MOVIMM32, &cpuRegs.code, cpuRegs.code );
      GRec_Instruction( GREC_INST_MOVIMM32, &cpuRegs.pc, pc );

      GRec_Instruction( GREC_INST_CALL, SWC1, NULL, 0, GREC_TRUE );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )	
	   {
		   ADD32ItoR( EAX, _Imm_ );
	   }

	   PUSH32M( (u32)&fpuRegs.fpr[ _Rt_ ].UL );
	   PUSH32R( EAX );
	   iFlushCall();
	   CALLFunc( (u32)memWrite32 );
	   ADD32ItoR( ESP, 8 );
   }
}

////////////////////////////////////////////////////

#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

void recLQC2( void ) 
{
   if ( Config.Regcaching && ConfigNewRec )
   {
      GRecAssert( GREC_FALSE, "LQC2 not implemented" );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EAX, _Imm_);
	   }

      if ( _Rt_ )
      {
         PUSH32I( (u32)&VU0.VF[_Ft_].UD[0] );
      }
      else
      {
         PUSH32I( (u32)&dummyValue );
      }
	   PUSH32R( EAX );
	   iFlushCall();
	   CALLFunc( (u32)memRead128 );
	   ADD32ItoR( ESP, 8 );
   }
}

////////////////////////////////////////////////////
void recSQC2( void ) 
{
   if ( Config.Regcaching )
   {
      GRecAssert( GREC_FALSE, "SQC2 not implemented" );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Imm_ != 0 )
	   {
		   ADD32ItoR( EAX, _Imm_ );
	   }

	   PUSH32I( (u32)&VU0.VF[_Ft_].UD[0] );
	   PUSH32R( EAX );
	   iFlushCall();
	   CALLFunc( (u32)memWrite128 );
	   ADD32ItoR( ESP, 8 );
   }
}
*/
#endif
