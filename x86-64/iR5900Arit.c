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
* Register arithmetic                                    *
* Format:  OP rd, rs, rt                                 *
*********************************************************/

#ifndef ARITHMETIC_RECOMPILE

REC_FUNC(ADD);
REC_FUNC(ADDU);
REC_FUNC(DADD);
REC_FUNC(DADDU);
REC_FUNC(SUB);
REC_FUNC(SUBU);
REC_FUNC(DSUB);
REC_FUNC(DSUBU);
REC_FUNC(AND);
REC_FUNC(OR);
REC_FUNC(XOR);
REC_FUNC(NOR);
REC_FUNC(SLT);
REC_FUNC(SLTU);

#else

////////////////////////////////////////////////////
void recADD( void ) {
	int rdreg;
	int rsreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		ADD32RtoR(rdreg, rtreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		ADD32RtoR(rdreg, rsreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(rdreg, rsreg);
		ADD32RtoR(rdreg, rtreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	}

	_clearNeededX86regs();
#else
	MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	if (_Rt_ != 0) {
		   ADD32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	}
	CDQ( );
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );
#endif
}

////////////////////////////////////////////////////
void recADDU( void ) 
{
	recADD( );
}

////////////////////////////////////////////////////
void recDADD( void ) {
	int rdreg;
	int rsreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		ADD64RtoR(rdreg, rtreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		ADD64RtoR(rdreg, rsreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rsreg);
		ADD64RtoR(rdreg, rtreg);
	}

	_clearNeededX86regs();

#else

	   MOV64MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Rt_ != 0 ) {
		   ADD64MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   }
	   MOV64RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );

#endif
}

////////////////////////////////////////////////////
void recDADDU( void )
{
	recDADD( );
}

////////////////////////////////////////////////////
void recSUB( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int t0reg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SUB32RtoR(rdreg, rtreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);
		t0reg = _allocTempX86reg(-1);

		MOV32RtoR(t0reg, rsreg);
		SUB32RtoR(t0reg, rdreg);
		MOV32RtoR(rdreg, t0reg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);

		_freeX86reg(t0reg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(rdreg, rsreg);
		SUB32RtoR(rdreg, rtreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	}

	_clearNeededX86regs();
#else

	   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Rt_ != 0 )
	   {
		   SUB32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   }
	   CDQ( );
	   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
	   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );

#endif
}

////////////////////////////////////////////////////
void recSUBU( void ) 
{
	recSUB( );
}

////////////////////////////////////////////////////
void recDSUB( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int t0reg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SUB64RtoR(rdreg, rtreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);
		t0reg = _allocTempX86reg(-1);

		MOV64RtoR(t0reg, rsreg);
		SUB64RtoR(t0reg, rdreg);
		MOV64RtoR(rdreg, t0reg);

		_freeX86reg(t0reg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rsreg);
		SUB64RtoR(rdreg, rtreg);
	}

	_clearNeededX86regs();
#else

	   MOV64MtoR( RAX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   if ( _Rt_ != 0 ) {
		   SUB64MtoR( RAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	   }
	   MOV64RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );

#endif
}

////////////////////////////////////////////////////
void recDSUBU( void ) 
{
	recDSUB( );
}

////////////////////////////////////////////////////
void recAND( void ) {
	int rdreg;
	int rsreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		AND64RtoR(rdreg, rtreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		AND64RtoR(rdreg, rsreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rsreg);
		AND64RtoR(rdreg, rtreg);
	}

	_clearNeededX86regs();
#else
	if (_Rt_ == _Rd_) { // Rd&= Rs
		MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
		AND64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);
	} else if (_Rs_ == _Rd_) { // Rd&= Rt
		MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
		AND64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);
	} else { // Rd = Rs & Rt
		MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
		AND64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
		MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);
	}
#endif
}

////////////////////////////////////////////////////
void recOR( void ) {
	int rdreg;
	int rsreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		OR64RtoR(rdreg, rtreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		OR64RtoR(rdreg, rsreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rsreg);
		OR64RtoR(rdreg, rtreg);
	}

	_clearNeededX86regs();

#else

	if ( ( _Rs_ == 0 ) && ( _Rt_ == 0  ) ) {
		XOR64RtoR(RAX, RAX);
		MOV64RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[0], RAX );
	} else if ( _Rs_ == 0 )
	   {
		MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
		MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);
	   } 
	else if ( _Rt_ == 0 )
	   {
		MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
		MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);
	   }
      else
      {
		MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
		OR64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
		MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);
      }
#endif
}

////////////////////////////////////////////////////
void recXOR( void ) {
	int rdreg;
	int rsreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		XOR64RtoR(rdreg, rtreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		XOR64RtoR(rdreg, rsreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rsreg);
		XOR64RtoR(rdreg, rtreg);
	}

	_clearNeededX86regs();

#else

	MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
	XOR64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
	MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);

#endif
}

////////////////////////////////////////////////////
void recNOR( void ) {
	int rdreg;
	int rsreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		OR64RtoR(rdreg, rtreg);
		NOT64R(rdreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_READ | MODE_WRITE);

		OR64RtoR(rdreg, rsreg);
		NOT64R(rdreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rsreg);
		OR64RtoR(rdreg, rtreg);
		NOT64R(rdreg);
	}

	_clearNeededX86regs();

#else

	MOV64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
	OR64MtoR(RAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
	NOT64R(RAX);
	MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);

#endif
}

////////////////////////////////////////////////////
void recSLT( void ) {
	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING
	_freeX86regs();
#endif
	MOV64MtoR(EAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
    CMP64MtoR(EAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
    SETL8R   (EAX);
    AND64ItoR(EAX, 0xff);
	MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], EAX);
}

////////////////////////////////////////////////////
void recSLTU( void ) {
	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING
	_freeX86regs();
#endif
	MOV64MtoR(EAX, (u32)&cpuRegs.GPR.r[_Rs_].UL[0]);
    CMP64MtoR(EAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
	SBB64RtoR(EAX, EAX);
	NEG64R   (EAX);
	MOV64RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], RAX);
}

#endif
