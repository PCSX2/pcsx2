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
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
#ifndef JUMP_RECOMPILE

REC_SYS(J);
REC_SYS(JAL);
REC_SYS(JR);
REC_SYS(JALR);

#else

////////////////////////////////////////////////////
void recJ( void ) 
{
   //	SET_FPUSTATE;
	SetBranchImm(_Target_ * 4 + ( pc & 0xf0000000 )/*, 0*/);
}

////////////////////////////////////////////////////
void recJAL( void ) 
{
	   MOV32ItoM( (u32)&cpuRegs.GPR.r[31].UL[ 0 ], pc + 4 );
	   MOV32ItoM( (u32)&cpuRegs.GPR.r[31].UL[ 1 ], 0 );

	   SetBranchImm(_Target_ * 4 + ( pc & 0xf0000000 )/*, 0*/);
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/

////////////////////////////////////////////////////
void recJR( void ) 
{
	SetBranchReg( _Rs_ );
}

////////////////////////////////////////////////////
void recJALR( void ) 
{
	if ( _Rd_ ) {
		MOV64ItoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], pc + 4 );
	}

	SetBranchReg( _Rs_ );
}

#endif
