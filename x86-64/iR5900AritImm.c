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
* Arithmetic with immediate operand                      *
* Format:  OP rt, rs, immediate                          *
*********************************************************/
#ifndef ARITHMETICIMM_RECOMPILE

REC_FUNC(ADDI);
REC_FUNC(ADDIU);
REC_FUNC(DADDI);
REC_FUNC(DADDIU);
REC_FUNC(ANDI);
REC_FUNC(ORI);
REC_FUNC(XORI);

REC_FUNC(SLTI);
REC_FUNC(SLTIU);

#else

////////////////////////////////////////////////////
void recADDI( void ) {
	int rsreg;
	int rtreg;

	if (!_Rt_) return;

#ifdef ENABLE_REGCACHING

	if (_Rt_ == _Rs_) {
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE | MODE_READ);
		ADD64ItoR(rtreg, _Imm_);
	} else
	if (_Rs_ == 0) {
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);
		MOV64ItoR(rtreg, _Imm_);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);

		if (_Imm_ == 0) {
			MOV64RtoR(rtreg, rsreg);
		} else {
			MOV32ItoR(rtreg, _Imm_);
			ADD32RtoR(rtreg, rsreg);
			SHL64ItoR(rtreg, 32);
			SAR64ItoR(rtreg, 32);
		}
	}

	_clearNeededX86regs();

#else

	MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	if (_Imm_ != 0) {
		ADD32ItoR( EAX, _Imm_ );
	}

	CDQ( );
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX );
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ], EDX );

#endif
}

////////////////////////////////////////////////////
void recADDIU( void ) 
{
	recADDI( );
}

////////////////////////////////////////////////////
void recDADDI( void ) {
	int rsreg;
	int rtreg;

	if (!_Rt_) return;

#ifdef ENABLE_REGCACHING

	if (_Rt_ == _Rs_) {
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE | MODE_READ);
		ADD64ItoR(rtreg, _Imm_);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);

		if (_Imm_ == 0) {
			MOV64RtoR(rtreg, rsreg);
		} else {
			MOV64ItoR(rtreg, _Imm_);
			ADD64RtoR(rtreg, rsreg);
		}
	}

	_clearNeededX86regs();

#else

	MOV64MtoR( RAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	if ( _Imm_ != 0 )
	   {
		   ADD64ItoR( EAX, _Imm_ );
	   }
	MOV64RtoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], RAX );

#endif
}

////////////////////////////////////////////////////
void recDADDIU( void ) 
{
	recDADDI( );
}

////////////////////////////////////////////////////
void recSLTIU( void )
{
   if ( ! _Rt_ )
   {
      return;
   }
#ifdef ENABLE_REGCACHING
	_freeX86regs();
#endif

	MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]);
    CMP64ItoR(RAX, _Imm_);
    SETB8R   (EAX);
    AND64ItoR(EAX, 0xff);
	MOV64RtoM((u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX);
}

////////////////////////////////////////////////////
void recSLTI( void )
{
   if ( ! _Rt_ )
   {
      return;
   }
#ifdef ENABLE_REGCACHING
	_freeX86regs();
#endif

	MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ]);
    CMP64ItoR(RAX, _Imm_);
    SETL8R   (EAX);
    AND64ItoR(EAX, 0xff);
	MOV64RtoM((u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX);
}

////////////////////////////////////////////////////
void recANDI( void ) {
	int rsreg;
	int rtreg;

	if (!_Rt_) return;

#ifdef ENABLE_REGCACHING

	if (_Rt_ == _Rs_) {
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE | MODE_READ);
		AND64ItoR(rtreg, _ImmU_);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);

		if (_Imm_ == 0) {
			MOV64RtoR(rtreg, rsreg);
		} else {
			MOV64RtoR(rtreg, rsreg);
			AND64ItoR(rtreg, _ImmU_);
		}
	}

	_clearNeededX86regs();

#else

	if ( _ImmU_ != 0 ) {
		if (_Rs_ == _Rt_) {
			MOV32ItoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ], 0 );
			AND32ItoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], _ImmU_ );
		} else {
			MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
			AND32ItoR( EAX, _ImmU_ );
			MOV32ItoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ], 0 );
			MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX );
		}
	}
	   else
	   {
		   MOV32ItoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ], 0 );
		   MOV32ItoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], 0 );
	   }
#endif
}

////////////////////////////////////////////////////
void recORI( void ) {
	int rsreg;
	int rtreg;

	if (!_Rt_) return;

#ifdef ENABLE_REGCACHING

	if (_Rt_ == _Rs_) {
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE | MODE_READ);
		OR64ItoR(rtreg, _ImmU_);
	} else
	if (_Rs_ == 0) {
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);
		MOV64ItoR(rtreg, _ImmU_);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);

		if (_Imm_ == 0) {
			MOV64RtoR(rtreg, rsreg);
		} else {
			MOV64RtoR(rtreg, rsreg);
			OR64ItoR(rtreg, _ImmU_);
		}
	}

	_clearNeededX86regs();

#else

	if (_Rs_ == _Rt_) {
		OR32ItoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ], _ImmU_ );
	} else {
		MOV64MtoR( RAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UD[ 0 ] );
	   if ( _ImmU_ != 0 )
	   {
		   OR64ItoR( RAX, _ImmU_ );
	   }
		MOV64RtoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ], RAX );
	}

#endif
}

////////////////////////////////////////////////////
void recXORI( void ) {
	int rsreg;
	int rtreg;

	if (!_Rt_) return;

#ifdef ENABLE_REGCACHING

	if (_Rt_ == _Rs_) {
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE | MODE_READ);
		XOR64ItoR(rtreg, _ImmU_);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_WRITE);

		MOV64RtoR(rtreg, rsreg);
		XOR64ItoR(rtreg, _ImmU_);
	}

	_clearNeededX86regs();

#else

	MOV64MtoR( RAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UD[ 0 ] );
	XOR64ItoR( RAX, _ImmU_ );
	MOV64RtoM( (u32)&cpuRegs.GPR.r[ _Rt_ ].UD[ 0 ], RAX );

#endif
}

#endif
