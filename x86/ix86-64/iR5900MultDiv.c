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
* Register mult/div & Register trap logic                *
* Format:  OP rs, rt                                     *
*********************************************************/
#ifndef MULTDIV_RECOMPILE

REC_FUNC(MULT);
REC_FUNC(MULTU);
REC_FUNC(DIV);
REC_FUNC(DIVU);

#else

////////////////////////////////////////////////////
void recMULT( void ) 
{
	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   IMUL32M( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );

	   MOV32RtoR( ECX, EDX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 1 ], EDX );
	   if ( _Rd_ )
	   {
		   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
		   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );
	   }
	   MOV32RtoR( EAX, ECX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 1 ], EDX );
}

////////////////////////////////////////////////////
void recMULTU( void ) 
{
      MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   MUL32M( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );

	   MOV32RtoR( ECX, EDX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 1 ], EDX );
	   if ( _Rd_ != 0 )
	   {
		   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
		   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );
	   }
	   MOV32RtoR( EAX, ECX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 0 ], ECX );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 1 ], EDX );
}

////////////////////////////////////////////////////
void recDIV( void ) 
{
      MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   OR32RtoR( ECX, ECX );
	   j8Ptr[ 0 ] = JE8( 0 );
   	
	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
//   	XOR32RtoR( EDX,EDX );
	   CDQ();
	   IDIV32R( ECX );
   	
	   MOV32RtoR( ECX, EDX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 1 ], EDX );
   	
	   MOV32RtoR( EAX, ECX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 0 ], ECX );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 1 ], EDX );

	   x86SetJ8( j8Ptr[ 0 ] );
}

////////////////////////////////////////////////////
void recDIVU( void ) 
{
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   OR32RtoR( ECX, ECX );
	   j8Ptr[ 0 ] = JE8( 0 );
   	
	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   XOR32RtoR( EDX, EDX );
	   //	CDQ();
	   DIV32R( ECX );
   	
	   MOV32RtoR( ECX, EDX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.LO.UL[ 1 ], EDX );
   	
	   MOV32RtoR( EAX,ECX );
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 0 ], ECX );
	   MOV32RtoM( (u32)&cpuRegs.HI.UL[ 1 ], EDX );
	   x86SetJ8( j8Ptr[ 0 ] );
}

#endif
