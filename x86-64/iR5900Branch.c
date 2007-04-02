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
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/
#ifndef BRANCH_RECOMPILE

REC_SYS(BEQ);
REC_SYS(BEQL);
REC_SYS(BNE);
REC_SYS(BNEL);
REC_SYS(BLTZ);
REC_SYS(BGTZ);
REC_SYS(BLEZ);
REC_SYS(BGEZ);
REC_SYS(BGTZL);
REC_SYS(BLTZL);
REC_SYS(BLTZAL);
REC_SYS(BLTZALL);
REC_SYS(BLEZL);
REC_SYS(BGEZL);
REC_SYS(BGEZAL);
REC_SYS(BGEZALL);

#else


////////////////////////////////////////////////////
void recBEQ( void ) 
{

	   if ( _Rs_ == _Rt_ )
      {
         SetBranchImm( _Imm_ * 4 + pc/*, 1*/ );
	   }
      else
      {
         u32 branchTo = _Imm_ * 4 + pc;

         //	SetFPUstate();
	      MOV64MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	      CMP64MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	      j8Ptr[ 0 ] = JNE8( 0 );

	      MOV32ItoM( (u32)&target, branchTo );
         j8Ptr[ 2 ] = JMP8( 0 );

         x86SetJ8( j8Ptr[ 0 ] ); 
	      MOV32ItoM( (u32)&target, pc + 4 );
	      x86SetJ8( j8Ptr[ 2 ] );

         SetBranch( );
      }
}

////////////////////////////////////////////////////
void recBNE( void ) 
{
      u32 branchTo = _Imm_ * 4 + pc;

      if ( _Rs_ == _Rt_ )
      {
         return;
      }

      //	SetFPUstate();
	      MOV64MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	      CMP64MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	      j8Ptr[ 0 ] = JE8( 0 );

	      MOV32ItoM( (u32)&target, branchTo );
         j8Ptr[ 2 ] = JMP8( 0 );

         x86SetJ8( j8Ptr[ 0 ] ); 
	      MOV32ItoM( (u32)&target, pc + 4 );
	      x86SetJ8( j8Ptr[ 2 ] );

         SetBranch( );
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

//#if 0
////////////////////////////////////////////////////
void recBLTZAL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BLTZAL );
	   branch = 2; 
}

////////////////////////////////////////////////////
void recBGEZAL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BGEZAL );
	   branch = 2; 
}

////////////////////////////////////////////////////
void recBLEZ( void ) 
{
      u32 branchTo = _Imm_ * 4 + pc;

	   CMP32ItoM( (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ], 0 );
	   j8Ptr[ 0 ] = JL8( 0 );
	   j8Ptr[ 1 ] = JG8( 0 );

	   CMP32ItoM( (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ], 0 );
	   j8Ptr[ 2 ] = JNZ8( 0 );

      x86SetJ8( j8Ptr[ 0 ] );
	   MOV32ItoM( (u32)&target, branchTo );
	   j8Ptr[ 3 ] = JMP8( 0 );

      x86SetJ8( j8Ptr[ 1 ] );
      x86SetJ8( j8Ptr[ 2 ] );

	   MOV32ItoM( (u32)&target, pc + 4 );
	   x86SetJ8( j8Ptr[ 3 ] );

      SetBranch( );
}

////////////////////////////////////////////////////
void recBGTZ( void ) 
{
      u32 branchTo = _Imm_ * 4 + pc;

      CMP32ItoM( (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ], 0 );
      j8Ptr[ 0 ] = JG8( 0 );
      j8Ptr[ 1 ] = JL8( 0 );

      CMP32ItoM( (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ], 0 );
      j8Ptr[ 2 ] = JZ8( 0 );

      x86SetJ8( j8Ptr[ 0 ] );
      MOV32ItoM( (u32)&target, branchTo );
      j8Ptr[ 3 ] = JMP8( 0 );

      x86SetJ8( j8Ptr[ 1 ] );
      x86SetJ8( j8Ptr[ 2 ] );

	   MOV32ItoM( (u32)&target, pc + 4 );
	   x86SetJ8( j8Ptr[ 3 ] );

      SetBranch( );
}

////////////////////////////////////////////////////
void recBLTZ( void ) 
{
      u32 branchTo = _Imm_ * 4 + pc;

      CMP32ItoM( (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ], 0 );
      j8Ptr[ 0 ] = JGE8( 0 );

	   MOV32ItoM( (u32)&target, branchTo );
      j8Ptr[ 1 ] = JMP8( 0 );

      x86SetJ8( j8Ptr[ 0 ] );

	   MOV32ItoM( (u32)&target, pc + 4 );
	   x86SetJ8( j8Ptr[ 1 ] );

      SetBranch( );
}

////////////////////////////////////////////////////
void recBGEZ( void ) 
{
      u32 branchTo = _Imm_ * 4 + pc;

      CMP32ItoM( (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ], 0 );
      j8Ptr[ 0 ] = JL8( 0 );

	   MOV32ItoM( (u32)&target, branchTo );
      j8Ptr[ 1 ] = JMP8( 0 );

      x86SetJ8( j8Ptr[ 0 ] );

	   MOV32ItoM( (u32)&target, pc + 4 );
	   x86SetJ8( j8Ptr[ 1 ] );

      SetBranch( );
}

/*********************************************************
* Register branch logic  Likely                          *
* Format:  OP rs, offset                                 *
*********************************************************/

////////////////////////////////////////////////////
void recBLEZL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BLEZL );
	   branch = 2; 
}

////////////////////////////////////////////////////
void recBGTZL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BGTZL );
	   branch = 2; 
}

////////////////////////////////////////////////////
void recBLTZL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BLTZL );
	   branch = 2; 
}

////////////////////////////////////////////////////
void recBLTZALL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BLTZALL );
	   branch = 2; 
}

////////////////////////////////////////////////////
void recBGEZALL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BGEZALL );
	   branch = 2; 
}

////////////////////////////////////////////////////
void recBEQL( void ) 
{
      MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   MOV32MtoR( EDX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   CMP32RtoR( ECX, EDX );
	   j8Ptr[ 0 ] = JNE8( 0 );

	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ] );
	   MOV32MtoR( EDX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] );
	   CMP32RtoR( ECX, EDX );
	   j8Ptr[ 1 ] = JNE8( 0 );

	   MOV32ItoM( (u32)&target, _Imm_ * 4 + pc );
	   j8Ptr[ 2 ] = JMP8( 0 );

	   x86SetJ8( j8Ptr[ 0 ] ); 
      x86SetJ8( j8Ptr[ 1 ] );

	   MOV32ItoM( (u32)&cpuRegs.pc, pc + 4 );
	   iRet( TRUE );

	   x86SetJ8( j8Ptr[ 2 ] );
	   SetBranch( );
}

////////////////////////////////////////////////////
void recBNEL( void ) 
{
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   MOV32MtoR( EDX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   CMP32RtoR( ECX, EDX );
	   j8Ptr[ 0 ] = JNE8( 0 );

	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 1 ] );
	   MOV32MtoR( EDX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ] );
	   CMP32RtoR( ECX, EDX );
	   j8Ptr[ 1 ] = JNE8( 0 );

	   MOV32ItoM( (u32)&cpuRegs.pc, pc + 4 );
	   iRet( TRUE );

	   x86SetJ8( j8Ptr[ 0 ] ); 
      x86SetJ8( j8Ptr[ 1 ] );

	   MOV32ItoM( (u32)&target, _Imm_ * 4 + pc );

	   SetBranch( );
}

////////////////////////////////////////////////////
void recBGEZL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.code, cpuRegs.code );
	   MOV32ItoM( (u32)&cpuRegs.pc, pc );
	   iFlushCall();
	   CALLFunc( (u32)BGEZL );
	   branch = 2;
}
#endif
