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
#ifndef SHIFT_RECOMPILE

REC_FUNC(SLL);
REC_FUNC(SRL);
REC_FUNC(SRA);
REC_FUNC(DSLL);
REC_FUNC(DSRL);
REC_FUNC(DSRA);
REC_FUNC(DSLL32);
REC_FUNC(DSRL32);
REC_FUNC(DSRA32);

REC_FUNC(SLLV);
REC_FUNC(SRLV);
REC_FUNC(SRAV);
REC_FUNC(DSLLV);
REC_FUNC(DSRLV);
REC_FUNC(DSRAV);

#else


////////////////////////////////////////////////////
void recDSRA( void ) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SAR64ItoR(rdreg, _Sa_);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rtreg);
		if (_Sa_) {
			SAR64ItoR(rdreg, _Sa_);
		}
	}

	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	if ( _Sa_ != 0 ) {
		SAR64ItoR( RAX, _Sa_ );
	}
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

////////////////////////////////////////////////////
void recDSRA32(void) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SAR64ItoR(rdreg, _Sa_ + 32);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rtreg);
		SAR64ItoR(rdreg, _Sa_ + 32);
	}

	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	SAR64ItoR( RAX, _Sa_ + 32 );
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

////////////////////////////////////////////////////
void recSLL(void) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SHL64ItoR(rdreg, _Sa_ + 32);
		SAR64ItoR(rdreg, 32);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(rdreg, rtreg);
		SHL64ItoR(rdreg, _Sa_ + 32);
		SAR64ItoR(rdreg, 32);
	}

	_clearNeededX86regs();
#else
	MOV32MtoR(EAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
	if (_Sa_ != 0) {
		SHL32ItoR(EAX, _Sa_);
	}
	CDQ();
	MOV32RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], EAX);
	MOV32RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[1], EDX);
#endif
}

////////////////////////////////////////////////////
void recSRL(void) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SHR32ItoR(rdreg, _Sa_);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(rdreg, rtreg);
		if (_Sa_) {
			SHR32ItoR(rdreg, _Sa_);
		}
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	}

	_clearNeededX86regs();
#else
	MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[_Rt_].UL[0]);
	if (_Sa_ != 0) {
		SHR32ItoR(EAX, _Sa_);
	}
	CDQ();
	MOV32RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[0], EAX);
	MOV32RtoM((u32)&cpuRegs.GPR.r[_Rd_].UL[1], EDX);
#endif
}

////////////////////////////////////////////////////
void recSRA(void) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SAR32ItoR(rdreg, _Sa_);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(rdreg, rtreg);
		if (_Sa_) {
			SAR32ItoR(rdreg, _Sa_);
		}
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	}

	_clearNeededX86regs();
#else
	MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	if ( _Sa_ != 0 ) {
		SAR32ItoR( EAX, _Sa_);
	}
	CDQ();
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );
#endif
}

////////////////////////////////////////////////////
void recDSLL(void) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SHL64ItoR(rdreg, _Sa_);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rtreg);
		if (_Sa_) {
			SHL64ItoR(rdreg, _Sa_);
		}
	}

	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	if ( _Sa_ != 0 ) {
		SHL64ItoR( RAX, _Sa_ );
	}
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

////////////////////////////////////////////////////
void recDSRL( void ) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SHR64ItoR(rdreg, _Sa_);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rtreg);
		if (_Sa_) {
			SHR64ItoR(rdreg, _Sa_);
		}
	}

	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	if ( _Sa_ != 0 ) {
		SHR64ItoR( RAX, _Sa_ );
	}
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

////////////////////////////////////////////////////
void recDSLL32(void) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SHL64ItoR(rdreg, _Sa_ + 32);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rtreg);
		SHL64ItoR(rdreg, _Sa_ + 32);
	}

	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	SHL64ItoR( RAX, _Sa_ + 32 );
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

////////////////////////////////////////////////////
void recDSRL32( void ) {
	int rdreg;
	int rtreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	if (_Rd_ == _Rt_) {
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		SHR64ItoR(rdreg, _Sa_ + 32);
	} else {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV64RtoR(rdreg, rtreg);
		SHR64ItoR(rdreg, _Sa_ + 32);
	}

	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	SHR64ItoR( RAX, _Sa_ + 32 );
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

/*********************************************************
* Shift arithmetic with variant register shift           *
* Format:  OP rd, rt, rs                                 *
*********************************************************/


////////////////////////////////////////////////////
void recSLLV( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int clreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	clreg = _allocTempX86reg(ECX);
	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rdreg);
		MOV32RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHL32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rsreg);
		AND32ItoR(clreg, 0x1f);
		SHL32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(clreg, rsreg);
		MOV32RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHL32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	}

	_freeX86reg(clreg);
	_clearNeededX86regs();
#else
	MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
	if ( _Rs_ != 0 ) {
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   AND32ItoR( ECX, 0x1f );
	   SHL32CLtoR( EAX );
	}
	CDQ();
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
	MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );
#endif
}

////////////////////////////////////////////////////
void recSRLV( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int clreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	clreg = _allocTempX86reg(ECX);
	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rdreg);
		MOV32RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHR32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rsreg);
		AND32ItoR(clreg, 0x1f);
		SHR32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(clreg, rsreg);
		MOV32RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHR32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	}

	_freeX86reg(clreg);
	_clearNeededX86regs();
#else
   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
   if ( _Rs_ != 0 )	
   {
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   AND32ItoR( ECX, 0x1f );
	   SHR32CLtoR( EAX );
   }
   CDQ( );
   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );
#endif
}

////////////////////////////////////////////////////
void recSRAV( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int clreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	clreg = _allocTempX86reg(ECX);
	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rdreg);
		MOV32RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SAR32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rsreg);
		AND32ItoR(clreg, 0x1f);
		SAR32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(clreg, rsreg);
		MOV32RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SAR32CLtoR(rdreg);
		SHL64ItoR(rdreg, 32);
		SAR64ItoR(rdreg, 32);
	}

	_freeX86reg(clreg);
	_clearNeededX86regs();
#else
   MOV32MtoR( EAX, (u32)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
   if ( _Rs_ != 0 )
   {
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   AND32ItoR( ECX, 0x1f );
	   SAR32CLtoR( EAX );
   }
   CDQ( );
   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], EAX );
   MOV32RtoM( (u32)&cpuRegs.GPR.r[ _Rd_ ].UL[ 1 ], EDX );
#endif
}

////////////////////////////////////////////////////
void recDSLLV( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int clreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	clreg = _allocTempX86reg(ECX);
	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rdreg);
		MOV64RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHL64CLtoR(rdreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rsreg);
		AND32ItoR(clreg, 0x1f);
		SHL64CLtoR(rdreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(clreg, rsreg);
		MOV64RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHL64CLtoR(rdreg);
	}

	_freeX86reg(clreg);
	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
   if ( _Rs_ != 0 )
   {
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   AND32ItoR( ECX, 0x3f );
	   SHL64CLtoR( RAX );
   }
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

////////////////////////////////////////////////////
void recDSRLV( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int clreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	clreg = _allocTempX86reg(ECX);
	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rdreg);
		MOV64RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHR64CLtoR(rdreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rsreg);
		AND32ItoR(clreg, 0x1f);
		SHR64CLtoR(rdreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(clreg, rsreg);
		MOV64RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SHR64CLtoR(rdreg);
	}

	_freeX86reg(clreg);
	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
   if ( _Rs_ != 0 )
   {
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   AND32ItoR( ECX, 0x3f );
	   SHR64CLtoR( RAX );
   }
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

////////////////////////////////////////////////////
void recDSRAV( void ) {
	int rdreg;
	int rsreg;
	int rtreg;
	int clreg;

	if (!_Rd_) return;

#ifdef ENABLE_REGCACHING

	clreg = _allocTempX86reg(ECX);
	if (_Rd_ == _Rs_) {
		_addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rdreg);
		MOV64RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SAR64CLtoR(rdreg);
	} else
	if (_Rd_ == _Rt_) {
		_addNeededGPRtoX86reg(_Rs_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE | MODE_READ);

		MOV32RtoR(clreg, rsreg);
		AND32ItoR(clreg, 0x1f);
		SAR64CLtoR(rdreg);
	} else {
		_addNeededGPRtoX86reg(_Rs_); _addNeededGPRtoX86reg(_Rt_);
		_addNeededGPRtoX86reg(_Rd_);
		rsreg = _allocGPRtoX86reg(-1, _Rs_, MODE_READ);
		rtreg = _allocGPRtoX86reg(-1, _Rt_, MODE_READ);
		rdreg = _allocGPRtoX86reg(-1, _Rd_, MODE_WRITE);

		MOV32RtoR(clreg, rsreg);
		MOV64RtoR(rdreg, rtreg);
		AND32ItoR(clreg, 0x1f);
		SAR64CLtoR(rdreg);
	}

	_freeX86reg(clreg);
	_clearNeededX86regs();
#else
	MOV64MtoR( RAX, (u64)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
   if ( _Rs_ != 0 )
   {
	   MOV32MtoR( ECX, (u32)&cpuRegs.GPR.r[ _Rs_ ].UL[ 0 ] );
	   AND32ItoR( ECX, 0x3f );
	   SAR64CLtoR( RAX );
   }
	MOV64RtoM( (u64)&cpuRegs.GPR.r[ _Rd_ ].UL[ 0 ], RAX );
#endif
}

#endif

