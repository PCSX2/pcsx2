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


#ifdef __WIN32__
#pragma warning(disable:4244)
#pragma warning(disable:4761)
#endif

/*********************************************************
* Shift arithmetic with constant shift                   *
* Format:  OP rd, rt, sa                                 *
*********************************************************/
#ifndef MOVE_RECOMPILE

REC_FUNC(LUI);
REC_FUNC(MFLO);
REC_FUNC(MFHI);
REC_FUNC(MTLO);
REC_FUNC(MTHI);
REC_FUNC(MOVZ);
REC_FUNC(MOVN);

#else
REC_FUNC(MFLO);
REC_FUNC(MFHI);
REC_FUNC(MTLO);
REC_FUNC(MTHI);
REC_FUNC(MOVZ);
REC_FUNC(MOVN);

/*********************************************************
* Load higher 16 bits of the first word in GPR with imm  *
* Format:  OP rt, immediate                              *
*********************************************************/

////////////////////////////////////////////////////
void recLUI( void ) {
	int rtreg;

	if (!_Rt_) return;

#ifdef ENABLE_REGCACHING
	rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);
	MOV64ItoR(rtreg, (s32)(_Imm_ << 16));
	_clearNeededX86regs();
#else
	MOV64ItoM((u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], (s32)(_Imm_ << 16));
#endif
}

/*********************************************************
* Move from HI/LO to GPR                                 *
* Format:  OP rd                                         *
*********************************************************/
/*
////////////////////////////////////////////////////
void recMFHI( void ) 
{
   if ( ! _Rd_ )
   {
      return;
   }

   if ( Config.Regcaching && ConfigNewRec )
   {
      GRecLog( "MFHI: PC: 0x%.8X, RD: %d, x86Ptr: 0x%.8x\n", pc, _Rd_, x86Ptr );

      GRec_Instruction( GREC_INST_MOV64, &cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], &cpuRegs.HI.UD[ 0 ] );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOVQMtoR( MM0, (u32)&cpuRegs.HI.UD[ 0 ] );
	   MOVQRtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ], MM0 );
	   SetMMXstate();
   }
}

////////////////////////////////////////////////////
void recMFLO( void ) 
{
   if ( ! _Rd_ ) 
   {
      return;
   }

   if ( Config.Regcaching && ConfigNewRec )
   {
      GRecAssert( GREC_FALSE, "MFLO not implemented" );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOVQMtoR( MM0, (u32)&cpuRegs.LO.UD[ 0 ] );
	   MOVQRtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UD[ 0 ], MM0 );
	   SetMMXstate();
   }
}
*/
/*********************************************************
* Move to GPR to HI/LO & Register jump                   *
* Format:  OP rs                                         *
*********************************************************/
/*
////////////////////////////////////////////////////
void recMTHI( void ) 
{
   if ( Config.Regcaching && ConfigNewRec )
   {
      GRecAssert( GREC_FALSE, "MTHI not implemented" );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOVQMtoR( MM0, (u32)&cpuRegs.GPR.r[ _Rs_ ].UD[ 0 ] );
	   MOVQRtoM( (u32)&cpuRegs.HI.UD[ 0 ], MM0 );
	   SetMMXstate();
   }
}

////////////////////////////////////////////////////
void recMTLO( void )
{
   if ( Config.Regcaching && ConfigNewRec )
   {
      GRecAssert( GREC_FALSE, "MTLO not implemented" );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOVQMtoR( MM0, (u32)&cpuRegs.GPR.r[ _Rs_ ].UD[ 0 ] );
	   MOVQRtoM( (u32)&cpuRegs.LO.UD[ 0 ], MM0 );
	   SetMMXstate();
   }
}

*/
/*********************************************************
* Conditional Move                                       *
* Format:  OP rd, rs, rt                                 *
*********************************************************/
/*
////////////////////////////////////////////////////
void recMOVZ( void )
{
   if ( ! _Rd_ ) 
   {
      return;
   }

   if ( Config.Regcaching )
   {
      GRecLog( "MOVZ: PC: 0x%.8X, RD: %d, RT: %d, RS: %d, x86Ptr: 0x%.8x\n", 
               pc, _Rd_, _Rt_, _Rs_, x86Ptr );

      GRec_Instruction( GREC_INST_CMPIMM64, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ], (GRec_u64)0 );
      GRec_Instruction( GREC_INST_MOVCC64, GREC_CMP_Z, &cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], &cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   OR32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] );
	   j8Ptr[ 0 ] = JNZ8( 0 );

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   MOV32MtoR( EDX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ] );
	   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );

	   x86SetJ8( j8Ptr[ 0 ] ); 
   }
}

////////////////////////////////////////////////////
void recMOVN( void ) 
{
   if ( ! _Rd_ ) 
   {
      return;
   }

   if ( Config.Regcaching )
   {
      GRecLog( "MOVN: PC: 0x%.8X, RD: %d, RT: %d, RS: %d, x86Ptr: 0x%.8x\n", 
               pc, _Rd_, _Rt_, _Rs_, x86Ptr );

      GRec_Instruction( GREC_INST_CMPIMM64, &cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ], (GRec_u64)0 );
      GRec_Instruction( GREC_INST_MOVCC64, GREC_CMP_NZ, &cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], &cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
   }
   else
   {
      if ( Config.Regcaching )
      {
         GRecReleaseAll( );
      }

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   OR32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] );
	   j8Ptr[ 0 ] = JZ8( 0 );

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ] );
	   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], ECX );

	   x86SetJ8( j8Ptr[ 0 ] );
   }
}
*/
#endif
