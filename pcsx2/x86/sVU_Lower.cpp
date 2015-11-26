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
#include "GS.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "iMMI.h"
#include "iFPU.h"
#include "iCOP0.h"
#include "VUmicro.h"
#include "sVU_Micro.h"
#include "sVU_Debug.h"
#include "sVU_zerorec.h"
#include "Gif_Unit.h"

using namespace x86Emitter;
//------------------------------------------------------------------

//------------------------------------------------------------------
// Helper Macros
//------------------------------------------------------------------
#define _Ft_ (( VU->code >> 16) & 0x1F)  // The rt part of the instruction register
#define _Fs_ (( VU->code >> 11) & 0x1F)  // The rd part of the instruction register
#define _Fd_ (( VU->code >>  6) & 0x1F)  // The sa part of the instruction register
#define _It_ (_Ft_ & 15)
#define _Is_ (_Fs_ & 15)
#define _Id_ (_Fd_ & 15)

#define _X (( VU->code>>24) & 0x1)
#define _Y (( VU->code>>23) & 0x1)
#define _Z (( VU->code>>22) & 0x1)
#define _W (( VU->code>>21) & 0x1)

#define _XYZW_SS (_X+_Y+_Z+_W==1)

#define _Fsf_ (( VU->code >> 21) & 0x03)
#define _Ftf_ (( VU->code >> 23) & 0x03)

#define _Imm11_ 	(s32)(VU->code & 0x400 ? 0xfffffc00 | (VU->code & 0x3ff) : VU->code & 0x3ff)
#define _UImm11_	(s32)(VU->code & 0x7ff)

#define VU_VFx_ADDR(x)  (uptr)&VU->VF[x].UL[0]
#define VU_VFy_ADDR(x)  (uptr)&VU->VF[x].UL[1]
#define VU_VFz_ADDR(x)  (uptr)&VU->VF[x].UL[2]
#define VU_VFw_ADDR(x)  (uptr)&VU->VF[x].UL[3]

#define VU_REGR_ADDR    (uptr)&VU->VI[REG_R]
#define VU_REGQ_ADDR    (uptr)&VU->VI[REG_Q]
#define VU_REGMAC_ADDR  (uptr)&VU->VI[REG_MAC_FLAG]

#define VU_VI_ADDR(x, read) GetVIAddr(VU, x, read, info)

#define VU_ACCx_ADDR    (uptr)&VU->ACC.UL[0]
#define VU_ACCy_ADDR    (uptr)&VU->ACC.UL[1]
#define VU_ACCz_ADDR    (uptr)&VU->ACC.UL[2]
#define VU_ACCw_ADDR    (uptr)&VU->ACC.UL[3]

#define _X_Y_Z_W  ((( VU->code >> 21 ) & 0xF ) )


static const __aligned16 u32 VU_ONE[4] = {0x3f800000, 0xffffffff, 0xffffffff, 0xffffffff};
//------------------------------------------------------------------


//------------------------------------------------------------------
// *VU Lower Instructions!*
//
// Note: * = Checked for errors by cottonvibes
//------------------------------------------------------------------


//------------------------------------------------------------------
// DIV*
//------------------------------------------------------------------
void recVUMI_DIV(VURegs *VU, int info)
{
	u8 *pjmp, *pjmp1;
	u32 *ajmp32, *bjmp32;

	//Console.WriteLn("recVUMI_DIV()");
	xAND(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0xFCF); // Clear D/I flags

	// FT can be zero here! so we need to check if its zero and set the correct flag.
	xXOR.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP)); // Clear EEREC_TEMP
	xCMPEQ.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_T)); // Set all F's if each vector is zero

	xMOVMSKPS(eax, xRegisterSSE(EEREC_TEMP)); // Move the sign bits of the previous calculation

	xAND(eax, (1<<_Ftf_) );  // Grab "Is Zero" bits from the previous calculation
	ajmp32 = JZ32(0); // Skip if none are

		xXOR.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP)); // Clear EEREC_TEMP
		xCMPEQ.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S)); // Set all F's if each vector is zero
		xMOVMSKPS(eax, xRegisterSSE(EEREC_TEMP)); // Move the sign bits of the previous calculation

		xAND(eax, (1<<_Fsf_) );  // Grab "Is Zero" bits from the previous calculation
		pjmp = JZ8(0);
			xOR(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0x410 ); // Set invalid flag (0/0)
			pjmp1 = JMP8(0);
		x86SetJ8(pjmp);
			xOR(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0x820 ); // Zero divide (only when not 0/0)
		x86SetJ8(pjmp1);

		_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);

		_vuFlipRegSS_xyzw(EEREC_T, _Ftf_);
		xXOR.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_T));
		_vuFlipRegSS_xyzw(EEREC_T, _Ftf_);

		xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[&const_clip[4]]);
		xOR.PS(xRegisterSSE(EEREC_TEMP), ptr[&g_maxvals[0]]); // If division by zero, then EEREC_TEMP = +/- fmax

		bjmp32 = JMP32(0);

	x86SetJ32(ajmp32);

	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5_useEAX(EEREC_S, EEREC_TEMP, (1 << (3-_Fsf_)));
		vuFloat5_useEAX(EEREC_T, EEREC_TEMP, (1 << (3-_Ftf_)));
	}

	_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);

	_vuFlipRegSS_xyzw(EEREC_T, _Ftf_);
	xDIV.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_T));
	_vuFlipRegSS_xyzw(EEREC_T, _Ftf_);

	vuFloat_useEAX(info, EEREC_TEMP, 0x8);

	x86SetJ32(bjmp32);

	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_Q, 0))], xRegisterSSE(EEREC_TEMP));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQRT*
//------------------------------------------------------------------
void recVUMI_SQRT( VURegs *VU, int info )
{
	u8* pjmp;
	//Console.WriteLn("recVUMI_SQRT()");

	_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, _Ftf_);
	xAND(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0xFCF); // Clear D/I flags

	/* Check for negative sqrt */
	xMOVMSKPS(eax, xRegisterSSE(EEREC_TEMP));
	xAND(eax, 1);  //Check sign
	pjmp = JZ8(0); //Skip if none are
		xOR(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0x410); // Invalid Flag - Negative number sqrt
	x86SetJ8(pjmp);

	xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[const_clip]); // Do a cardinal sqrt
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_TEMP), ptr[g_maxvals]); // Clamp infinities (only need to do positive clamp since EEREC_TEMP is positive)
	xSQRT.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP));
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_Q, 0))], xRegisterSSE(EEREC_TEMP));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// RSQRT*
//------------------------------------------------------------------
__aligned16 u64 RSQRT_TEMP_XMM[2];
void recVUMI_RSQRT(VURegs *VU, int info)
{
	u8 *ajmp8, *bjmp8;
	u8 *qjmp1, *qjmp2;
	int t1reg, t1boolean;
	//Console.WriteLn("recVUMI_RSQRT()");

	_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, _Ftf_);
	xAND(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0xFCF); // Clear D/I flags

	/* Check for negative divide */
	xMOVMSKPS(eax, xRegisterSSE(EEREC_TEMP));
	xAND(eax, 1);  //Check sign
	ajmp8 = JZ8(0); //Skip if none are
		xOR(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0x410); // Invalid Flag - Negative number sqrt
	x86SetJ8(ajmp8);

	xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[const_clip]); // Do a cardinal sqrt
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_TEMP), ptr[g_maxvals]); // Clamp Infinities to Fmax
	xSQRT.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP));

	t1reg = _vuGetTempXMMreg(info);
	if( t1reg < 0 ) {
		for (t1reg = 0; ( (t1reg == EEREC_TEMP) || (t1reg == EEREC_S) ); t1reg++)
			; // Makes t1reg not be EEREC_TEMP or EEREC_S.
		xMOVAPS(ptr[&RSQRT_TEMP_XMM[0]], xRegisterSSE(t1reg )); // backup data in t1reg to a temp address
		t1boolean = 1;
	}
	else t1boolean = 0;

	// Ft can still be zero here! so we need to check if its zero and set the correct flag.
	xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg)); // Clear t1reg
	xCMPEQ.SS(xRegisterSSE(t1reg), xRegisterSSE(EEREC_TEMP)); // Set all F's if each vector is zero

	xMOVMSKPS(eax, xRegisterSSE(t1reg)); // Move the sign bits of the previous calculation

	xAND(eax, 0x01 );  // Grab "Is Zero" bits from the previous calculation
	ajmp8 = JZ8(0); // Skip if none are

		//check for 0/0
		_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);

		xXOR.PS(xRegisterSSE(t1reg), xRegisterSSE(t1reg)); // Clear EEREC_TEMP
		xCMPEQ.PS(xRegisterSSE(t1reg), xRegisterSSE(EEREC_TEMP)); // Set all F's if each vector is zero
		xMOVMSKPS(eax, xRegisterSSE(t1reg)); // Move the sign bits of the previous calculation

		xAND(eax, 0x01 );  // Grab "Is Zero" bits from the previous calculation
		qjmp1 = JZ8(0);
			xOR(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0x410 ); // Set invalid flag (0/0)
			qjmp2 = JMP8(0);
		x86SetJ8(qjmp1);
			xOR(ptr32[(u32*)(VU_VI_ADDR(REG_STATUS_FLAG, 2))], 0x820 ); // Zero divide (only when not 0/0)
		x86SetJ8(qjmp2);

		xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[&const_clip[4]]);
		xOR.PS(xRegisterSSE(EEREC_TEMP), ptr[&g_maxvals[0]]); // If division by zero, then EEREC_TEMP = +/- fmax
		xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_Q, 0))], xRegisterSSE(EEREC_TEMP));
		bjmp8 = JMP8(0);
	x86SetJ8(ajmp8);

	_unpackVFSS_xyzw(t1reg, EEREC_S, _Fsf_);
	if (CHECK_VU_EXTRA_OVERFLOW) vuFloat_useEAX(info, t1reg, 0x8); // Clamp Infinities
	xDIV.SS(xRegisterSSE(t1reg), xRegisterSSE(EEREC_TEMP));
	vuFloat_useEAX(info, t1reg, 0x8);
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_Q, 0))], xRegisterSSE(t1reg));

	x86SetJ8(bjmp8);

	if (t1boolean) xMOVAPS(xRegisterSSE(t1reg), ptr[&RSQRT_TEMP_XMM[0] ]); // restore t1reg data
	else _freeXMMreg(t1reg); // free t1reg
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// _addISIMMtoIT() - Used in IADDI, IADDIU, and ISUBIU instructions
//------------------------------------------------------------------
void _addISIMMtoIT(VURegs *VU, s16 imm, int info)
{
	int isreg = -1, itreg;
	if (_It_ == 0) return;

	if( _Is_ == 0 ) {
		itreg = ALLOCVI(_It_, MODE_WRITE);
		xMOV(xRegister32(itreg), imm&0xffff);
		return;
	}

	ADD_VI_NEEDED(_It_);
	isreg = ALLOCVI(_Is_, MODE_READ);
	itreg = ALLOCVI(_It_, MODE_WRITE);

	if ( _It_ == _Is_ ) {
		if (imm != 0 ) xADD(xRegister16(itreg), imm);
	}
	else {
		if( imm ) {
			xLEA(xRegister32(itreg), ptr[xAddressReg(isreg)+imm]);
			xMOVZX(xRegister32(itreg), xRegister16(itreg));
		}
		else xMOV(xRegister32(itreg), xRegister32(isreg));
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// IADDI
//------------------------------------------------------------------
void recVUMI_IADDI(VURegs *VU, int info)
{
	s16 imm;

	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_IADDI");
	imm = ( VU->code >> 6 ) & 0x1f;
	imm = ( imm & 0x10 ? 0xfff0 : 0) | ( imm & 0xf );
	_addISIMMtoIT(VU, imm, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// IADDIU
//------------------------------------------------------------------
void recVUMI_IADDIU(VURegs *VU, int info)
{
	s16 imm;

	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_IADDIU");
	imm = ( ( VU->code >> 10 ) & 0x7800 ) | ( VU->code & 0x7ff );
	_addISIMMtoIT(VU, imm, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// IADD
//------------------------------------------------------------------
void recVUMI_IADD( VURegs *VU, int info )
{
	int idreg, isreg = -1, itreg = -1;
	if ( _Id_ == 0 ) return;
	//Console.WriteLn("recVUMI_IADD");
	if ( ( _It_ == 0 ) && ( _Is_ == 0 ) ) {
		idreg = ALLOCVI(_Id_, MODE_WRITE);
		xXOR(xRegister32(idreg), xRegister32(idreg));
		return;
	}

	ADD_VI_NEEDED(_Is_);
	ADD_VI_NEEDED(_It_);
	idreg = ALLOCVI(_Id_, MODE_WRITE);

	if ( _Is_ == 0 )
	{
		if( (itreg = _checkX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _It_, MODE_READ)) >= 0 ) {
			if( idreg != itreg ) xMOV(xRegister32(idreg), xRegister32(itreg));
		}
		else xMOVZX(xRegister32(idreg), ptr16[(u16*)(VU_VI_ADDR(_It_, 1))]);
	}
	else if ( _It_ == 0 )
	{
		if( (isreg = _checkX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _Is_, MODE_READ)) >= 0 ) {
			if( idreg != isreg ) xMOV(xRegister32(idreg), xRegister32(isreg));
		}
		else xMOVZX(xRegister32(idreg), ptr16[(u16*)(VU_VI_ADDR(_Is_, 1))]);
	}
	else {
		//ADD_VI_NEEDED(_It_);
		isreg = ALLOCVI(_Is_, MODE_READ);
		itreg = ALLOCVI(_It_, MODE_READ);

		if( idreg == isreg ) xADD(xRegister32(idreg), xRegister32(itreg));
		else if( idreg == itreg ) xADD(xRegister32(idreg), xRegister32(isreg));
		else xLEA(xRegister32(idreg), ptr[xAddressReg(isreg) + xAddressReg(itreg)]);
		xMOVZX(xRegister32(idreg), xRegister16(idreg)); // needed since don't know if idreg's upper bits are 0
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// IAND
//------------------------------------------------------------------
void recVUMI_IAND( VURegs *VU, int info )
{
	int idreg, isreg = -1, itreg = -1;
	if ( _Id_ == 0 ) return;
	//Console.WriteLn("recVUMI_IAND");
	if ( ( _Is_ == 0 ) || ( _It_ == 0 ) ) {
		idreg = ALLOCVI(_Id_, MODE_WRITE);
		xXOR(xRegister32(idreg), xRegister32(idreg));
		return;
	}

	ADD_VI_NEEDED(_Is_);
	ADD_VI_NEEDED(_It_);
	idreg = ALLOCVI(_Id_, MODE_WRITE);

	isreg = ALLOCVI(_Is_, MODE_READ);
	itreg = ALLOCVI(_It_, MODE_READ);

	if( idreg == isreg ) xAND(xRegister16(idreg), xRegister16(itreg));
	else if( idreg == itreg ) xAND(xRegister16(idreg), xRegister16(isreg));
	else {
		xMOV(xRegister32(idreg), xRegister32(itreg));
		xAND(xRegister32(idreg), xRegister32(isreg));
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// IOR
//------------------------------------------------------------------
void recVUMI_IOR( VURegs *VU, int info )
{
	int idreg, isreg = -1, itreg = -1;
	if ( _Id_ == 0 ) return;
	//Console.WriteLn("recVUMI_IOR");
	if ( ( _It_ == 0 ) && ( _Is_ == 0 ) ) {
		idreg = ALLOCVI(_Id_, MODE_WRITE);
		xXOR(xRegister32(idreg), xRegister32(idreg));
		return;
	}

	ADD_VI_NEEDED(_Is_);
	ADD_VI_NEEDED(_It_);
	idreg = ALLOCVI(_Id_, MODE_WRITE);

	if ( _Is_ == 0 )
	{
		if( (itreg = _checkX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _It_, MODE_READ)) >= 0 ) {
			if( idreg != itreg ) xMOV(xRegister32(idreg), xRegister32(itreg));
		}
		else xMOVZX(xRegister32(idreg), ptr16[(u16*)(VU_VI_ADDR(_It_, 1))]);
	}
	else if ( _It_ == 0 )
	{
		if( (isreg = _checkX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _Is_, MODE_READ)) >= 0 ) {
			if( idreg != isreg ) xMOV(xRegister32(idreg), xRegister32(isreg));
		}
		else xMOVZX(xRegister32(idreg), ptr16[(u16*)(VU_VI_ADDR(_Is_, 1))]);
	}
	else
	{
		isreg = ALLOCVI(_Is_, MODE_READ);
		itreg = ALLOCVI(_It_, MODE_READ);

		if( idreg == isreg ) xOR(xRegister16(idreg), xRegister16(itreg));
		else if( idreg == itreg ) xOR(xRegister16(idreg), xRegister16(isreg));
		else {
			xMOV(xRegister32(idreg), xRegister32(isreg));
			xOR(xRegister32(idreg), xRegister32(itreg));
		}
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ISUB
//------------------------------------------------------------------
void recVUMI_ISUB( VURegs *VU, int info )
{
	int idreg, isreg = -1, itreg = -1;
	if ( _Id_ == 0 ) return;
	//Console.WriteLn("recVUMI_ISUB");
	if ( ( _It_ == 0 ) && ( _Is_ == 0 ) ) {
		idreg = ALLOCVI(_Id_, MODE_WRITE);
		xXOR(xRegister32(idreg), xRegister32(idreg));
		return;
	}

	ADD_VI_NEEDED(_Is_);
	ADD_VI_NEEDED(_It_);
	idreg = ALLOCVI(_Id_, MODE_WRITE);

	if ( _Is_ == 0 )
	{
		if( (itreg = _checkX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _It_, MODE_READ)) >= 0 ) {
			if( idreg != itreg ) xMOV(xRegister32(idreg), xRegister32(itreg));
		}
		else xMOVZX(xRegister32(idreg), ptr16[(u16*)(VU_VI_ADDR(_It_, 1))]);
		xNEG(xRegister16(idreg));
	}
	else if ( _It_ == 0 )
	{
		if( (isreg = _checkX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _Is_, MODE_READ)) >= 0 ) {
			if( idreg != isreg ) xMOV(xRegister32(idreg), xRegister32(isreg));
		}
		else xMOVZX(xRegister32(idreg), ptr16[(u16*)(VU_VI_ADDR(_Is_, 1))]);
	}
	else
	{
		isreg = ALLOCVI(_Is_, MODE_READ);
		itreg = ALLOCVI(_It_, MODE_READ);

		if( idreg == isreg ) xSUB(xRegister16(idreg), xRegister16(itreg));
		else if( idreg == itreg ) {
			xSUB(xRegister16(idreg), xRegister16(isreg));
			xNEG(xRegister16(idreg));
		}
		else {
			xMOV(xRegister32(idreg), xRegister32(isreg));
			xSUB(xRegister16(idreg), xRegister16(itreg));
		}
	}
}
//------------------------------------------------------------------

//------------------------------------------------------------------
// ISUBIU
//------------------------------------------------------------------
void recVUMI_ISUBIU( VURegs *VU, int info )
{
	s16 imm;

	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_ISUBIU");
	imm = ( ( VU->code >> 10 ) & 0x7800 ) | ( VU->code & 0x7ff );
	imm = -imm;
	_addISIMMtoIT(VU, imm, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MOVE*
//------------------------------------------------------------------
void recVUMI_MOVE( VURegs *VU, int info )
{
	if ( (_Ft_ == 0) || (_X_Y_Z_W == 0) ) return;
	//Console.WriteLn("recVUMI_MOVE");
	if (_X_Y_Z_W == 0x8)  xMOVSS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_S));
	else if (_X_Y_Z_W == 0xf) xMOVAPS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_S));
	else {
		xMOVAPS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MFIR*
//------------------------------------------------------------------
void recVUMI_MFIR( VURegs *VU, int info )
{
	if ( (_Ft_ == 0)  || (_X_Y_Z_W == 0) ) return;
	//Console.WriteLn("recVUMI_MFIR");
	_deleteX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _Is_, 1);

	if( _XYZW_SS ) {
		xMOVDZX(xRegisterSSE(EEREC_TEMP), ptr[(void*)(VU_VI_ADDR(_Is_, 1)-2)]);

		_vuFlipRegSS(VU, EEREC_T);
		xPSRA.D(xRegisterSSE(EEREC_TEMP), 16);
		xMOVSS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_TEMP));
		_vuFlipRegSS(VU, EEREC_T);
	}
	else if (_X_Y_Z_W != 0xf) {
		xMOVDZX(xRegisterSSE(EEREC_TEMP), ptr[(void*)(VU_VI_ADDR(_Is_, 1)-2)]);

		xPSRA.D(xRegisterSSE(EEREC_TEMP), 16);
		xSHUF.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP), 0);
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
	else {
		xMOVDZX(xRegisterSSE(EEREC_T), ptr[(void*)(VU_VI_ADDR(_Is_, 1)-2)]);

		xPSRA.D(xRegisterSSE(EEREC_T), 16);
		xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MTIR*
//------------------------------------------------------------------
void recVUMI_MTIR( VURegs *VU, int info )
{
	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_MTIR");
	_deleteX86reg(X86TYPE_VI|((VU==&VU1)?X86TYPE_VU1:0), _It_, 2);

	if( _Fsf_ == 0 ) {
		xMOVSS(ptr[(void*)(VU_VI_ADDR(_It_, 0))], xRegisterSSE(EEREC_S));
	}
	else {
		_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);
		xMOVSS(ptr[(void*)(VU_VI_ADDR(_It_, 0))], xRegisterSSE(EEREC_TEMP));
	}

	xAND(ptr32[(u32*)(VU_VI_ADDR(_It_, 0))], 0xffff);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MR32*
//------------------------------------------------------------------
void recVUMI_MR32( VURegs *VU, int info )
{
	if ( (_Ft_ == 0) || (_X_Y_Z_W == 0) ) return;
	//Console.WriteLn("recVUMI_MR32");
	if (_X_Y_Z_W != 0xf) {
		xMOVAPS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
		xSHUF.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP), 0x39);
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
	else {
		xMOVAPS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_S));
		xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0x39);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// _loadEAX()
//
// NOTE: If x86reg < 0, reads directly from offset
//------------------------------------------------------------------
void _loadEAX(VURegs *VU, int x86reg, uptr offset, int info)
{
    pxAssert( offset < 0x80000000 );

	if( x86reg >= 0 ) {
		switch(_X_Y_Z_W) {
			case 3: // ZW
				xMOVH.PS(xRegisterSSE(EEREC_T), ptr[xAddressReg(x86reg)+offset+8]);
				break;
			case 6: // YZ
				xSHUF.PS(xRegisterSSE(EEREC_T), ptr[xAddressReg(x86reg)+offset], 0x9c);
				xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0x78);
				break;

			case 8: // X
				xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[xAddressReg(x86reg)+offset]);
				xMOVSS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_TEMP));
				break;
			case 9: // XW
				xSHUF.PS(xRegisterSSE(EEREC_T), ptr[xAddressReg(x86reg)+offset], 0xc9);
				xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0xd2);
				break;
			case 12: // XY
				xMOVL.PS(xRegisterSSE(EEREC_T), ptr[xAddressReg(x86reg)+offset]);
				break;
			case 15:
				if( VU == &VU1 ) xMOVAPS(xRegisterSSE(EEREC_T), ptr[xAddressReg(x86reg)+offset]);
				else xMOVUPS(xRegisterSSE(EEREC_T), ptr[xAddressReg(x86reg)+offset]);
				break;
			default:
				if( VU == &VU1 ) xMOVAPS(xRegisterSSE(EEREC_TEMP), ptr[xAddressReg(x86reg)+offset]);
				else xMOVUPS(xRegisterSSE(EEREC_TEMP), ptr[xAddressReg(x86reg)+offset]);

				VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
				break;
		}
	}
	else {
		switch(_X_Y_Z_W) {
			case 3: // ZW
				xMOVH.PS(xRegisterSSE(EEREC_T), ptr[(void*)(offset+8)]);
				break;
			case 6: // YZ
				xSHUF.PS(xRegisterSSE(EEREC_T), ptr[(void*)(offset)], 0x9c);
				xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0x78);
				break;
			case 8: // X
				xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[(void*)(offset)]);
				xMOVSS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_TEMP));
				break;
			case 9: // XW
				xSHUF.PS(xRegisterSSE(EEREC_T), ptr[(void*)(offset)], 0xc9);
				xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0xd2);
				break;
			case 12: // XY
				xMOVL.PS(xRegisterSSE(EEREC_T), ptr[(void*)(offset)]);
				break;
			case 15:
				if( VU == &VU1 ) xMOVAPS(xRegisterSSE(EEREC_T), ptr[(void*)(offset)]);
				else xMOVUPS(xRegisterSSE(EEREC_T), ptr[(void*)(offset)]);
				break;
			default:
				if( VU == &VU1 ) xMOVAPS(xRegisterSSE(EEREC_TEMP), ptr[(void*)(offset)]);
				else xMOVUPS(xRegisterSSE(EEREC_TEMP), ptr[(void*)(offset)]);
				VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
				break;
		}
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// recVUTransformAddr()
//------------------------------------------------------------------
int recVUTransformAddr(int x86reg, VURegs* VU, int vireg, int imm)
{
	if( x86reg == eax.GetId() ) {
		if (imm) xADD(xRegister32(x86reg), imm);
	}
	else {
		if( imm ) xLEA(eax, ptr[xAddressReg(x86reg)+imm]);
		else xMOV(eax, xRegister32(x86reg));
	}

	if( VU == &VU1 ) {
		xAND(eax, 0x3ff); // wrap around
		xSHL(eax, 4);
	}
	else {

		// VU0 has a somewhat interesting memory mapping:
		// if addr & 0x4000, reads VU1's VF regs and VI regs
		// otherwise, wrap around at 0x1000

		xTEST(eax, 0x400);
		xForwardJNZ8 vu1regs; // if addr & 0x4000, reads VU1's VF regs and VI regs
			xAND(eax, 0xff); // if !(addr & 0x4000), wrap around
			xForwardJump8 done;
		vu1regs.SetTarget();
			xAND(eax, 0x3f);
			xADD(eax, (u128*)VU1.VF - (u128*)VU0.Mem);
		done.SetTarget();

		xSHL(eax, 4); // multiply by 16 (shift left by 4)
	}

	return eax.GetId();
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// LQ
//------------------------------------------------------------------
void recVUMI_LQ(VURegs *VU, int info)
{
	s16 imm;
	if ( _Ft_ == 0 ) return;
	//Console.WriteLn("recVUMI_LQ");
	imm = (VU->code & 0x400) ? (VU->code & 0x3ff) | 0xfc00 : (VU->code & 0x3ff);
	if (_Is_ == 0) {
		_loadEAX(VU, -1, (uptr)GET_VU_MEM(VU, (u32)imm*16), info);
	}
	else {
		int isreg = ALLOCVI(_Is_, MODE_READ);
		_loadEAX(VU, recVUTransformAddr(isreg, VU, _Is_, imm), (uptr)VU->Mem, info);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// LQD
//------------------------------------------------------------------
void recVUMI_LQD( VURegs *VU, int info )
{
	int isreg;
	//Console.WriteLn("recVUMI_LQD");
	if ( _Is_ != 0 ) {
		isreg = ALLOCVI(_Is_, MODE_READ|MODE_WRITE);
		xSUB(xRegister16(isreg), 1 );
	}

	if ( _Ft_ == 0 ) return;

	if ( _Is_ == 0 ) _loadEAX(VU, -1, (uptr)VU->Mem, info);
	else _loadEAX(VU, recVUTransformAddr(isreg, VU, _Is_, 0), (uptr)VU->Mem, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// LQI
//------------------------------------------------------------------
void recVUMI_LQI(VURegs *VU, int info)
{
	int isreg;
	//Console.WriteLn("recVUMI_LQI");
	if ( _Ft_ == 0 ) {
		if( _Is_ != 0 ) {
			if( (isreg = _checkX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), _Is_, MODE_WRITE|MODE_READ)) >= 0 ) {
				xADD(xRegister16(isreg), 1);
			}
			else {
				xADD(ptr16[(u16*)(VU_VI_ADDR( _Is_, 0 ))], 1 );
			}
		}
		return;
	}

    if (_Is_ == 0) {
		_loadEAX(VU, -1, (uptr)VU->Mem, info);
    }
	else {
		isreg = ALLOCVI(_Is_, MODE_READ|MODE_WRITE);
		_loadEAX(VU, recVUTransformAddr(isreg, VU, _Is_, 0), (uptr)VU->Mem, info);
		xADD(xRegister16(isreg), 1 );
    }
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// _saveEAX()
//------------------------------------------------------------------
void _saveEAX(VURegs *VU, int x86reg, uptr offset, int info)
{
	pxAssert( offset < 0x80000000 );

	if ( _Fs_ == 0 ) {
		if ( _XYZW_SS ) {
			u32 c = _W ? 0x3f800000 : 0;
			if ( x86reg >= 0 ) xMOV(ptr32[xAddressReg(x86reg)+offset+(_W?12:(_Z?8:(_Y?4:0)))], c);
			else xMOV(ptr32[(u32*)(offset+(_W?12:(_Z?8:(_Y?4:0))))], c);
		}
		else {

			// (this is one of my test cases for the new emitter --air)
			using namespace x86Emitter;
			xAddressVoid indexer( offset );
			if( x86reg != -1 ) indexer.Add( xAddressReg( x86reg ) );

			if ( _X ) xMOV(ptr32[indexer],    0x00000000);
			if ( _Y ) xMOV(ptr32[indexer+4],  0x00000000);
			if ( _Z ) xMOV(ptr32[indexer+8],  0x00000000);
			if ( _W ) xMOV(ptr32[indexer+12], 0x3f800000);
		}
		return;
	}

	switch ( _X_Y_Z_W ) {
		case 1: // W
			xPSHUF.D(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S), 0x27);
			if ( x86reg >= 0 ) xMOVSS(ptr[xAddressReg(x86reg)+offset+12], xRegisterSSE(EEREC_TEMP));
			else xMOVSS(ptr[(void*)(offset+12)], xRegisterSSE(EEREC_TEMP));
			break;
		case 2: // Z
			xMOVHL.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			if ( x86reg >= 0 ) xMOVSS(ptr[xAddressReg(x86reg)+offset+8], xRegisterSSE(EEREC_TEMP));
			else xMOVSS(ptr[(void*)(offset+8)], xRegisterSSE(EEREC_TEMP));
			break;
		case 3: // ZW
			if ( x86reg >= 0 ) xMOVH.PS(ptr[xAddressReg(x86reg)+offset+8], xRegisterSSE(EEREC_S));
			else xMOVH.PS(ptr[(void*)(offset+8)], xRegisterSSE(EEREC_S));
			break;
		case 4: // Y
			xPSHUF.LW(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S), 0x4e);
			if ( x86reg >= 0 ) xMOVSS(ptr[xAddressReg(x86reg)+offset+4], xRegisterSSE(EEREC_TEMP));
			else xMOVSS(ptr[(void*)(offset+4)], xRegisterSSE(EEREC_TEMP));
			break;
		case 5: // YW
			xSHUF.PS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0xB1);
			xMOVHL.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			if ( x86reg >= 0 ) {
				xMOVSS(ptr[xAddressReg(x86reg)+offset+4], xRegisterSSE(EEREC_S));
				xMOVSS(ptr[xAddressReg(x86reg)+offset+12], xRegisterSSE(EEREC_TEMP));
			}
			else {
				xMOVSS(ptr[(void*)(offset+4)], xRegisterSSE(EEREC_S));
				xMOVSS(ptr[(void*)(offset+12)], xRegisterSSE(EEREC_TEMP));
			}
			xSHUF.PS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0xB1);
			break;
		case 6: // YZ
			xPSHUF.D(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S), 0xc9);
			if ( x86reg >= 0 ) xMOVL.PS(ptr[xAddressReg(x86reg)+offset+4], xRegisterSSE(EEREC_TEMP));
			else xMOVL.PS(ptr[(void*)(offset+4)], xRegisterSSE(EEREC_TEMP));
			break;
		case 7: // YZW
			xPSHUF.D(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S), 0x93); //ZYXW
			if ( x86reg >= 0 ) {
				xMOVH.PS(ptr[xAddressReg(x86reg)+offset+4], xRegisterSSE(EEREC_TEMP));
				xMOVSS(ptr[xAddressReg(x86reg)+offset+12], xRegisterSSE(EEREC_TEMP));
			}
			else {
				xMOVH.PS(ptr[(void*)(offset+4)], xRegisterSSE(EEREC_TEMP));
				xMOVSS(ptr[(void*)(offset+12)], xRegisterSSE(EEREC_TEMP));
			}
			break;
		case 8: // X
			if ( x86reg >= 0 ) xMOVSS(ptr[xAddressReg(x86reg)+offset], xRegisterSSE(EEREC_S));
			else xMOVSS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
			break;
		case 9: // XW
			if ( x86reg >= 0 ) xMOVSS(ptr[xAddressReg(x86reg)+offset], xRegisterSSE(EEREC_S));
			else xMOVSS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));

			xPSHUF.D(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S), 0xff); //WWWW

			if ( x86reg >= 0 ) xMOVSS(ptr[xAddressReg(x86reg)+offset+12], xRegisterSSE(EEREC_TEMP));
			else xMOVSS(ptr[(void*)(offset+12)], xRegisterSSE(EEREC_TEMP));

			break;
		case 10: //XZ
			xMOVHL.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			if ( x86reg >= 0 ) {
				xMOVSS(ptr[xAddressReg(x86reg)+offset], xRegisterSSE(EEREC_S));
				xMOVSS(ptr[xAddressReg(x86reg)+offset+8], xRegisterSSE(EEREC_TEMP));
			}
			else {
				xMOVSS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
				xMOVSS(ptr[(void*)(offset+8)], xRegisterSSE(EEREC_TEMP));
			}
			break;
		case 11: //XZW
			if ( x86reg >= 0 ) {
				xMOVSS(ptr[xAddressReg(x86reg)+offset], xRegisterSSE(EEREC_S));
				xMOVH.PS(ptr[xAddressReg(x86reg)+offset+8], xRegisterSSE(EEREC_S));
			}
			else {
				xMOVSS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
				xMOVH.PS(ptr[(void*)(offset+8)], xRegisterSSE(EEREC_S));
			}
			break;
		case 12: // XY
			if ( x86reg >= 0 ) xMOVL.PS(ptr[xAddressReg(x86reg)+offset+0], xRegisterSSE(EEREC_S));
			else xMOVL.PS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
			break;
		case 13: // XYW
			xPSHUF.D(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S), 0x4b); //YXZW
			if ( x86reg >= 0 ) {
				xMOVH.PS(ptr[xAddressReg(x86reg)+offset+0], xRegisterSSE(EEREC_TEMP));
				xMOVSS(ptr[xAddressReg(x86reg)+offset+12], xRegisterSSE(EEREC_TEMP));
			}
			else {
				xMOVH.PS(ptr[(void*)(offset)], xRegisterSSE(EEREC_TEMP));
				xMOVSS(ptr[(void*)(offset+12)], xRegisterSSE(EEREC_TEMP));
			}
			break;
		case 14: // XYZ
			xMOVHL.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			if ( x86reg >= 0 ) {
				xMOVL.PS(ptr[xAddressReg(x86reg)+offset+0], xRegisterSSE(EEREC_S));
				xMOVSS(ptr[xAddressReg(x86reg)+offset+8], xRegisterSSE(EEREC_TEMP));
			}
			else {
				xMOVL.PS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
				xMOVSS(ptr[(void*)(offset+8)], xRegisterSSE(EEREC_TEMP));
			}
			break;
		case 15: // XYZW
			if ( VU == &VU1 ) {
				if( x86reg >= 0 ) xMOVAPS(ptr[xAddressReg(x86reg)+offset+0], xRegisterSSE(EEREC_S));
				else xMOVAPS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
			}
			else {
				if( x86reg >= 0 ) xMOVUPS(ptr[xAddressReg(x86reg)+offset+0], xRegisterSSE(EEREC_S));
				else {
					if( offset & 15 ) xMOVUPS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
					else xMOVAPS(ptr[(void*)(offset)], xRegisterSSE(EEREC_S));
				}
			}
			break;
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQ
//------------------------------------------------------------------
void recVUMI_SQ(VURegs *VU, int info)
{
	s16 imm;
	//Console.WriteLn("recVUMI_SQ");
	imm = ( VU->code & 0x400) ? ( VU->code & 0x3ff) | 0xfc00 : ( VU->code & 0x3ff);
	if ( _It_ == 0 ) _saveEAX(VU, -1, (uptr)GET_VU_MEM(VU, (int)imm * 16), info);
	else {
		int itreg = ALLOCVI(_It_, MODE_READ);
		_saveEAX(VU, recVUTransformAddr(itreg, VU, _It_, imm), (uptr)VU->Mem, info);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQD
//------------------------------------------------------------------
void recVUMI_SQD(VURegs *VU, int info)
{
	//Console.WriteLn("recVUMI_SQD");
	if (_It_ == 0) _saveEAX(VU, -1, (uptr)VU->Mem, info);
	else {
		int itreg = ALLOCVI(_It_, MODE_READ|MODE_WRITE);
		xSUB(xRegister16(itreg), 1 );
		_saveEAX(VU, recVUTransformAddr(itreg, VU, _It_, 0), (uptr)VU->Mem, info);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQI
//------------------------------------------------------------------
void recVUMI_SQI(VURegs *VU, int info)
{
	//Console.WriteLn("recVUMI_SQI");
	if (_It_ == 0) _saveEAX(VU, -1, (uptr)VU->Mem, info);
	else {
		int itreg = ALLOCVI(_It_, MODE_READ|MODE_WRITE);
		_saveEAX(VU, recVUTransformAddr(itreg, VU, _It_, 0), (uptr)VU->Mem, info);
		xADD(xRegister16(itreg), 1 );
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ILW
//------------------------------------------------------------------
void recVUMI_ILW(VURegs *VU, int info)
{
	int itreg;
	s16 imm, off;

	if ( ( _It_ == 0 ) || ( _X_Y_Z_W == 0 ) ) return;
	//Console.WriteLn("recVUMI_ILW");
	imm = ( VU->code & 0x400) ? ( VU->code & 0x3ff) | 0xfc00 : ( VU->code & 0x3ff);
	if (_X) off = 0;
	else if (_Y) off = 4;
	else if (_Z) off = 8;
	else if (_W) off = 12;

	ADD_VI_NEEDED(_Is_);
	itreg = ALLOCVI(_It_, MODE_WRITE);

	if ( _Is_ == 0 ) {
		xMOVZX(xRegister32(itreg), ptr16[GET_VU_MEM(VU, (int)imm * 16 + off)]);
	}
	else {
		int isreg = ALLOCVI(_Is_, MODE_READ);
		xMOV(xRegister32(itreg), ptr[xAddressReg(recVUTransformAddr(isreg, VU, _Is_, imm))+(uptr)VU->Mem + off]);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ISW
//------------------------------------------------------------------
void recVUMI_ISW( VURegs *VU, int info )
{
	s16 imm;
	//Console.WriteLn("recVUMI_ISW");
	imm = ( VU->code & 0x400) ? ( VU->code & 0x3ff) | 0xfc00 : ( VU->code & 0x3ff);

	if (_Is_ == 0) {
		uptr off = (uptr)GET_VU_MEM(VU, (int)imm * 16);
		int itreg = ALLOCVI(_It_, MODE_READ);

		if (_X) xMOV(ptr[(void*)(off)], xRegister32(itreg));
		if (_Y) xMOV(ptr[(void*)(off+4)], xRegister32(itreg));
		if (_Z) xMOV(ptr[(void*)(off+8)], xRegister32(itreg));
		if (_W) xMOV(ptr[(void*)(off+12)], xRegister32(itreg));
	}
	else {
		int x86reg, isreg, itreg;

		ADD_VI_NEEDED(_It_);
		isreg = ALLOCVI(_Is_, MODE_READ);
		itreg = ALLOCVI(_It_, MODE_READ);

		x86reg = recVUTransformAddr(isreg, VU, _Is_, imm);

		if (_X) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem], xRegister32(itreg));
		if (_Y) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem+4], xRegister32(itreg));
		if (_Z) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem+8], xRegister32(itreg));
		if (_W) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem+12], xRegister32(itreg));
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ILWR
//------------------------------------------------------------------
void recVUMI_ILWR( VURegs *VU, int info )
{
	int off, itreg;

	if ( ( _It_ == 0 ) || ( _X_Y_Z_W == 0 ) ) return;
	//Console.WriteLn("recVUMI_ILWR");
	if (_X) off = 0;
	else if (_Y) off = 4;
	else if (_Z) off = 8;
	else if (_W) off = 12;

	ADD_VI_NEEDED(_Is_);
	itreg = ALLOCVI(_It_, MODE_WRITE);

	if ( _Is_ == 0 ) {
		xMOVZX(xRegister32(itreg), ptr16[(u16*)((uptr)VU->Mem + off )]);
	}
	else {
		int isreg = ALLOCVI(_Is_, MODE_READ);
		xMOVZX(xRegister32(itreg), ptr16[xAddressReg( recVUTransformAddr(isreg, VU, _Is_, 0) ) + (uptr)VU->Mem + off]);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ISWR
//------------------------------------------------------------------
void recVUMI_ISWR( VURegs *VU, int info )
{
	int itreg;
	//Console.WriteLn("recVUMI_ISWR");
	ADD_VI_NEEDED(_Is_);
	itreg = ALLOCVI(_It_, MODE_READ);

	if (_Is_ == 0) {
		if (_X) xMOV(ptr[(VU->Mem)], xRegister32(itreg));
		if (_Y) xMOV(ptr[(void*)((uptr)VU->Mem+4)], xRegister32(itreg));
		if (_Z) xMOV(ptr[(void*)((uptr)VU->Mem+8)], xRegister32(itreg));
		if (_W) xMOV(ptr[(void*)((uptr)VU->Mem+12)], xRegister32(itreg));
	}
	else {
		int x86reg;
		int isreg = ALLOCVI(_Is_, MODE_READ);
		x86reg = recVUTransformAddr(isreg, VU, _Is_, 0);

		if (_X) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem], xRegister32(itreg));
		if (_Y) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem+4], xRegister32(itreg));
		if (_Z) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem+8], xRegister32(itreg));
		if (_W) xMOV(ptr[xAddressReg(x86reg)+(uptr)VU->Mem+12], xRegister32(itreg));
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// RINIT*
//------------------------------------------------------------------
void recVUMI_RINIT(VURegs *VU, int info)
{
	//Console.WriteLn("recVUMI_RINIT()");
	if( (xmmregs[EEREC_S].mode & MODE_WRITE) && (xmmregs[EEREC_S].mode & MODE_NOFLUSH) ) {
		_deleteX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), REG_R, 2);
		_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);

		xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[s_mask]);
		xOR.PS(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]);
		xMOVSS(ptr[(void*)(VU_REGR_ADDR)], xRegisterSSE(EEREC_TEMP));
	}
	else {
		int rreg = ALLOCVI(REG_R, MODE_WRITE);

		if( xmmregs[EEREC_S].mode & MODE_WRITE ) {
			xMOVAPS(ptr[(&VU->VF[_Fs_])], xRegisterSSE(EEREC_S));
			xmmregs[EEREC_S].mode &= ~MODE_WRITE;
		}

		xMOV(xRegister32(rreg), ptr[(void*)(VU_VFx_ADDR( _Fs_ ) + 4 * _Fsf_ )]);
		xAND(xRegister32(rreg), 0x7fffff );
		xOR(xRegister32(rreg), 0x7f << 23 );

		_deleteX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), REG_R, 1);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// RGET*
//------------------------------------------------------------------
void recVUMI_RGET(VURegs *VU, int info)
{
	//Console.WriteLn("recVUMI_RGET()");
	if ( (_Ft_ == 0) || (_X_Y_Z_W == 0)  ) return;

	_deleteX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), REG_R, 1);

	if (_X_Y_Z_W != 0xf) {
		xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[(void*)(VU_REGR_ADDR)]);
		xSHUF.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP), 0);
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
	else {
		xMOVSSZX(xRegisterSSE(EEREC_T), ptr[(void*)(VU_REGR_ADDR)]);
		xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// RNEXT*
//------------------------------------------------------------------
void recVUMI_RNEXT( VURegs *VU, int info )
{
	int rreg, x86temp0, x86temp1;
	//Console.WriteLn("recVUMI_RNEXT()");

	rreg = ALLOCVI(REG_R, MODE_WRITE|MODE_READ);

	x86temp0 = ALLOCTEMPX86(0);
	x86temp1 = ALLOCTEMPX86(0);

	// code from www.project-fao.org
	//xMOV(xRegister32(rreg), ptr[(void*)(VU_REGR_ADDR)]);
	xMOV(xRegister32(x86temp0), xRegister32(rreg));
	xSHR(xRegister32(x86temp0), 4);
	xAND(xRegister32(x86temp0), 1);

	xMOV(xRegister32(x86temp1), xRegister32(rreg));
	xSHR(xRegister32(x86temp1), 22);
	xAND(xRegister32(x86temp1), 1);

	xSHL(xRegister32(rreg), 1);
	xXOR(xRegister32(x86temp0), xRegister32(x86temp1));
	xXOR(xRegister32(rreg), xRegister32(x86temp0));
	xAND(xRegister32(rreg), 0x7fffff);
	xOR(xRegister32(rreg), 0x3f800000);

	_freeX86reg(x86temp0);
	_freeX86reg(x86temp1);

	if ( (_Ft_ == 0) || (_X_Y_Z_W == 0)  ) {
		_deleteX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), REG_R, 1);
		return;
	}

	recVUMI_RGET(VU, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// RXOR*
//------------------------------------------------------------------
void recVUMI_RXOR( VURegs *VU, int info )
{
	//Console.WriteLn("recVUMI_RXOR()");
	if( (xmmregs[EEREC_S].mode & MODE_WRITE) && (xmmregs[EEREC_S].mode & MODE_NOFLUSH) ) {
		_deleteX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), REG_R, 1);
		_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);

		xXOR.PS(xRegisterSSE(EEREC_TEMP), ptr[(void*)(VU_REGR_ADDR)]);
		xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[s_mask]);
		xOR.PS(xRegisterSSE(EEREC_TEMP), ptr[s_fones]);
		xMOVSS(ptr[(void*)(VU_REGR_ADDR)], xRegisterSSE(EEREC_TEMP));
	}
	else {
		int rreg = ALLOCVI(REG_R, MODE_WRITE|MODE_READ);

		if( xmmregs[EEREC_S].mode & MODE_WRITE ) {
			xMOVAPS(ptr[(&VU->VF[_Fs_])], xRegisterSSE(EEREC_S));
			xmmregs[EEREC_S].mode &= ~MODE_WRITE;
		}

		xXOR(xRegister32(rreg), ptr[(void*)(VU_VFx_ADDR( _Fs_ ) + 4 * _Fsf_ )]);
		xAND(xRegister32(rreg), 0x7fffff );
		xOR(xRegister32(rreg), 0x3f800000 );

		_deleteX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), REG_R, 1);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// WAITQ
//------------------------------------------------------------------
void recVUMI_WAITQ( VURegs *VU, int info )
{
	//Console.WriteLn("recVUMI_WAITQ");
//	if( info & PROCESS_VU_SUPER ) {
//		//xCALL((void*)waitqfn);
//		SuperVUFlush(0, 1);
//	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FSAND
//------------------------------------------------------------------
void recVUMI_FSAND( VURegs *VU, int info )
{
	int itreg;
	u16 imm;
	//Console.WriteLn("recVUMI_FSAND");
	imm = (((VU->code >> 21 ) & 0x1) << 11) | (VU->code & 0x7ff);
	if(_It_ == 0) return;

	itreg = ALLOCVI(_It_, MODE_WRITE);
	xMOV(xRegister32(itreg), ptr[(void*)(VU_VI_ADDR(REG_STATUS_FLAG, 1))]);
	xAND(xRegister32(itreg), imm );
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FSEQ
//------------------------------------------------------------------
void recVUMI_FSEQ( VURegs *VU, int info )
{
	int itreg;
	u16 imm;
	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_FSEQ");
	imm = (((VU->code >> 21 ) & 0x1) << 11) | (VU->code & 0x7ff);

	itreg = ALLOCVI(_It_, MODE_WRITE|MODE_8BITREG);

	xMOVZX(eax, ptr16[(u16*)(VU_VI_ADDR(REG_STATUS_FLAG, 1))]);
	xXOR(xRegister32(itreg), xRegister32(itreg));
	xCMP(ax, imm);
	xSETE(xRegister8(itreg));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FSOR
//------------------------------------------------------------------
void recVUMI_FSOR( VURegs *VU, int info )
{
	int itreg;
	u32 imm;
	if(_It_ == 0) return;
	//Console.WriteLn("recVUMI_FSOR");
	imm = (((VU->code >> 21 ) & 0x1) << 11) | (VU->code & 0x7ff);

	itreg = ALLOCVI(_It_, MODE_WRITE);

	xMOVZX(xRegister32(itreg), ptr16[(u16*)(VU_VI_ADDR(REG_STATUS_FLAG, 1))]);
	xOR(xRegister32(itreg), imm );
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FSSET
//------------------------------------------------------------------
void recVUMI_FSSET(VURegs *VU, int info)
{
	u32 writeaddr = VU_VI_ADDR(REG_STATUS_FLAG, 0);
	u32 prevaddr = VU_VI_ADDR(REG_STATUS_FLAG, 2);

	u16 imm = 0;
	//Console.WriteLn("recVUMI_FSSET");
	imm = (((VU->code >> 21 ) & 0x1) << 11) | (VU->code & 0x7FF);

    // keep the low 6 bits ONLY if the upper instruction is an fmac instruction (otherwise rewrite) - metal gear solid 3
    //if( (info & PROCESS_VU_SUPER) && VUREC_FMAC ) {
        xMOV(eax, ptr[(void*)(prevaddr)]);
	    xAND(eax, 0x3f);
	    if ((imm&0xfc0) != 0) xOR(eax, imm & 0xFC0);
        xMOV(ptr[(void*)(writeaddr ? writeaddr : prevaddr)], eax);
    //}
    //else {
    //    xMOV(ptr32[(u32*)(writeaddr ? writeaddr : prevaddr)], imm&0xfc0);
	//}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FMAND
//------------------------------------------------------------------
void recVUMI_FMAND( VURegs *VU, int info )
{
	int isreg, itreg;
	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_FMAND");
	isreg = _checkX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), _Is_, MODE_READ);
	itreg = ALLOCVI(_It_, MODE_WRITE);//|MODE_8BITREG);

	if( isreg >= 0 ) {
		if( itreg != isreg ) xMOV(xRegister32(itreg), xRegister32(isreg));
	}
	else xMOVZX(xRegister32(itreg), ptr16[(u16*)(VU_VI_ADDR(_Is_, 1))]);

	xAND(xRegister16(itreg), ptr[(void*)(VU_VI_ADDR(REG_MAC_FLAG, 1))]);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FMEQ
//------------------------------------------------------------------
void recVUMI_FMEQ( VURegs *VU, int info )
{
	int itreg, isreg;
	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_FMEQ");
	if( _It_ == _Is_ ) {
		itreg = ALLOCVI(_It_, MODE_WRITE|MODE_READ);//|MODE_8BITREG

		xCMP(xRegister16(itreg), ptr[(void*)(VU_VI_ADDR(REG_MAC_FLAG, 1))]);
		xSETE(al);
		xMOVZX(xRegister32(itreg), al);
	}
	else {
		ADD_VI_NEEDED(_Is_);
		itreg = ALLOCVI(_It_, MODE_WRITE|MODE_8BITREG);
		isreg = ALLOCVI(_Is_, MODE_READ);

		xXOR(xRegister32(itreg), xRegister32(itreg));

		xCMP(xRegister16(isreg), ptr[(void*)(VU_VI_ADDR(REG_MAC_FLAG, 1))]);
		xSETE(xRegister8(itreg));
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FMOR
//------------------------------------------------------------------
void recVUMI_FMOR( VURegs *VU, int info )
{
	int isreg, itreg;
	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_FMOR");
	if( _Is_ == 0 ) {
		itreg = ALLOCVI(_It_, MODE_WRITE);//|MODE_8BITREG);
		xMOVZX(xRegister32(itreg), ptr16[(u16*)(VU_VI_ADDR(REG_MAC_FLAG, 1))]);
	}
	else if( _It_ == _Is_ ) {
		itreg = ALLOCVI(_It_, MODE_WRITE|MODE_READ);//|MODE_8BITREG);
		xOR(xRegister16(itreg), ptr[(void*)(VU_VI_ADDR(REG_MAC_FLAG, 1))]);
	}
	else {
		isreg = _checkX86reg(X86TYPE_VI|(VU==&VU1?X86TYPE_VU1:0), _Is_, MODE_READ);
		itreg = ALLOCVI(_It_, MODE_WRITE);

		xMOVZX(xRegister32(itreg), ptr16[(u16*)(VU_VI_ADDR(REG_MAC_FLAG, 1))]);

		if( isreg >= 0 )
			xOR(xRegister16(itreg), xRegister16(isreg ));
		else
			xOR(xRegister16(itreg), ptr[(void*)(VU_VI_ADDR(_Is_, 1))]);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FCAND
//------------------------------------------------------------------
void recVUMI_FCAND( VURegs *VU, int info )
{
	int itreg = ALLOCVI(1, MODE_WRITE|MODE_8BITREG);
	//Console.WriteLn("recVUMI_FCAND");
	xMOV(eax, ptr[(void*)(VU_VI_ADDR(REG_CLIP_FLAG, 1))]);
	xXOR(xRegister32(itreg), xRegister32(itreg ));
	xAND(eax, VU->code & 0xFFFFFF );

	xSETNZ(xRegister8(itreg));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FCEQ
//------------------------------------------------------------------
void recVUMI_FCEQ( VURegs *VU, int info )
{
	int itreg = ALLOCVI(1, MODE_WRITE|MODE_8BITREG);
	//Console.WriteLn("recVUMI_FCEQ");
	xMOV(eax, ptr[(void*)(VU_VI_ADDR(REG_CLIP_FLAG, 1))]);
	xAND(eax, 0xffffff );
	xXOR(xRegister32(itreg), xRegister32(itreg ));
	xCMP(eax, VU->code&0xffffff );

	xSETE(xRegister8(itreg));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FCOR
//------------------------------------------------------------------
void recVUMI_FCOR( VURegs *VU, int info )
{
	int itreg;
	//Console.WriteLn("recVUMI_FCOR");
	itreg = ALLOCVI(1, MODE_WRITE);
	xMOV(xRegister32(itreg), ptr[(void*)(VU_VI_ADDR(REG_CLIP_FLAG, 1))]);
	xOR(xRegister32(itreg), VU->code );
	xAND(xRegister32(itreg), 0xffffff );
	xADD(xRegister32(itreg), 1 );	// If 24 1's will make 25th bit 1, else 0
	xSHR(xRegister32(itreg), 24 );	// Get the 25th bit (also clears the rest of the garbage in the reg)
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FCSET
//------------------------------------------------------------------
void recVUMI_FCSET( VURegs *VU, int info )
{
	u32 addr = VU_VI_ADDR(REG_CLIP_FLAG, 0);
	//Console.WriteLn("recVUMI_FCSET");
	xMOV(ptr32[(u32*)(addr ? addr : VU_VI_ADDR(REG_CLIP_FLAG, 2))], VU->code&0xffffff);

	if( !(info & (PROCESS_VU_SUPER|PROCESS_VU_COP2)) )
		xMOV(ptr32[(u32*)(VU_VI_ADDR(REG_CLIP_FLAG, 1))], VU->code&0xffffff );
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FCGET
//------------------------------------------------------------------
void recVUMI_FCGET( VURegs *VU, int info )
{
	int itreg;
	if(_It_ == 0) return;
	//Console.WriteLn("recVUMI_FCGET");
	itreg = ALLOCVI(_It_, MODE_WRITE);

	xMOV(xRegister32(itreg), ptr[(void*)(VU_VI_ADDR(REG_CLIP_FLAG, 1))]);
	xAND(xRegister32(itreg), 0x0fff);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// _recbranchAddr()
//
// NOTE: Due to static var dependencies, several SuperVU branch instructions
// are still located in iVUzerorec.cpp.
//------------------------------------------------------------------

//------------------------------------------------------------------
// MFP*
//------------------------------------------------------------------
void recVUMI_MFP(VURegs *VU, int info)
{
	if ( (_Ft_ == 0) || (_X_Y_Z_W == 0) ) return;
	//Console.WriteLn("recVUMI_MFP");
	if( _XYZW_SS ) {
		_vuFlipRegSS(VU, EEREC_T);
		xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[(void*)(VU_VI_ADDR(REG_P, 1))]);
		xMOVSS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_TEMP));
		_vuFlipRegSS(VU, EEREC_T);
	}
	else if (_X_Y_Z_W != 0xf) {
		xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[(void*)(VU_VI_ADDR(REG_P, 1))]);
		xSHUF.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP), 0);
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
	else {
		xMOVSSZX(xRegisterSSE(EEREC_T), ptr[(void*)(VU_VI_ADDR(REG_P, 1))]);
		xSHUF.PS(xRegisterSSE(EEREC_T), xRegisterSSE(EEREC_T), 0);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// WAITP
//------------------------------------------------------------------
static __aligned16 float s_tempmem[4];
void recVUMI_WAITP(VURegs *VU, int info)
{
	//Console.WriteLn("recVUMI_WAITP");
//	if( info & PROCESS_VU_SUPER )
//		SuperVUFlush(1, 1);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// vuSqSumXYZ()*
//
// NOTE: In all EFU insts, EEREC_D is a temp reg
//------------------------------------------------------------------
void vuSqSumXYZ(int regd, int regs, int regtemp) // regd.x =  x ^ 2 + y ^ 2 + z ^ 2
{
	//Console.WriteLn("VU: SUMXYZ");
	if( x86caps.hasStreamingSIMD4Extensions )
	{
		xMOVAPS(xRegisterSSE(regd), xRegisterSSE(regs));
		if (CHECK_VU_EXTRA_OVERFLOW) vuFloat2(regd, regtemp, 0xf);
		xDP.PS(xRegisterSSE(regd), xRegisterSSE(regd), 0x71);
	}
	else
	{
		xMOVAPS(xRegisterSSE(regtemp), xRegisterSSE(regs));
		if (CHECK_VU_EXTRA_OVERFLOW) vuFloat2(regtemp, regd, 0xf);
		xMUL.PS(xRegisterSSE(regtemp), xRegisterSSE(regtemp)); // xyzw ^ 2

		if( x86caps.hasStreamingSIMD3Extensions ) {
			xHADD.PS(xRegisterSSE(regd), xRegisterSSE(regtemp));
			xADD.PS(xRegisterSSE(regd), xRegisterSSE(regtemp)); // regd.z = x ^ 2 + y ^ 2 + z ^ 2
			xMOVHL.PS(xRegisterSSE(regd), xRegisterSSE(regd)); // regd.x = regd.z
		}
		else {
			xMOVSS(xRegisterSSE(regd), xRegisterSSE(regtemp));
			xPSHUF.LW(xRegisterSSE(regtemp), xRegisterSSE(regtemp), 0x4e); // wzyx -> wzxy
			xADD.SS(xRegisterSSE(regd), xRegisterSSE(regtemp)); // x ^ 2 + y ^ 2
			xSHUF.PS(xRegisterSSE(regtemp), xRegisterSSE(regtemp), 0xD2); // wzxy -> wxyz
			xADD.SS(xRegisterSSE(regd), xRegisterSSE(regtemp)); // x ^ 2 + y ^ 2 + z ^ 2
		}
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ESADD*
//------------------------------------------------------------------
void recVUMI_ESADD( VURegs *VU, int info)
{
	//Console.WriteLn("VU: ESADD");
	pxAssert( VU == &VU1 );
	if( EEREC_TEMP == EEREC_D ) { // special code to reset P ( FixMe: don't know if this is still needed! (cottonvibes) )
		Console.Warning("ESADD: Resetting P reg!!!\n");
		xMOV(ptr32[(u32*)(VU_VI_ADDR(REG_P, 0))], 0);
		return;
	}
	vuSqSumXYZ(EEREC_D, EEREC_S, EEREC_TEMP);
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_D), ptr[g_maxvals]); // Only need to do positive clamp since (x ^ 2 + y ^ 2 + z ^ 2) is positive
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_D));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ERSADD*
//------------------------------------------------------------------
void recVUMI_ERSADD( VURegs *VU, int info )
{
	//Console.WriteLn("VU: ERSADD");
	pxAssert( VU == &VU1 );
	vuSqSumXYZ(EEREC_D, EEREC_S, EEREC_TEMP);
	// don't use RCPSS (very bad precision)
	xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]);
	xDIV.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_D));
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_TEMP), ptr[g_maxvals]); // Only need to do positive clamp since (x ^ 2 + y ^ 2 + z ^ 2) is positive
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_TEMP));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ELENG*
//------------------------------------------------------------------
void recVUMI_ELENG( VURegs *VU, int info )
{
	//Console.WriteLn("VU: ELENG");
	pxAssert( VU == &VU1 );
	vuSqSumXYZ(EEREC_D, EEREC_S, EEREC_TEMP);
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_D), ptr[g_maxvals]); // Only need to do positive clamp since (x ^ 2 + y ^ 2 + z ^ 2) is positive
	xSQRT.SS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D));
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_D));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ERLENG*
//------------------------------------------------------------------
void recVUMI_ERLENG( VURegs *VU, int info )
{
	//Console.WriteLn("VU: ERLENG");
	pxAssert( VU == &VU1 );
	vuSqSumXYZ(EEREC_D, EEREC_S, EEREC_TEMP);
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_D), ptr[g_maxvals]); // Only need to do positive clamp since (x ^ 2 + y ^ 2 + z ^ 2) is positive
	xSQRT.SS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_D)); // regd <- sqrt(x^2 + y^2 + z^2)
	xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]); // temp <- 1
	xDIV.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_D)); // temp = 1 / sqrt(x^2 + y^2 + z^2)
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_TEMP), ptr[g_maxvals]); // Only need to do positive clamp
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_TEMP));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// EATANxy
//------------------------------------------------------------------
void recVUMI_EATANxy( VURegs *VU, int info )
{
	pxAssert( VU == &VU1 );
	//Console.WriteLn("recVUMI_EATANxy");
	if( (xmmregs[EEREC_S].mode & MODE_WRITE) && (xmmregs[EEREC_S].mode&MODE_NOFLUSH) ) {
		xMOVL.PS(ptr[s_tempmem], xRegisterSSE(EEREC_S));
		FLD32((uptr)&s_tempmem[0]);
		FLD32((uptr)&s_tempmem[1]);
	}
	else {
		if( xmmregs[EEREC_S].mode & MODE_WRITE ) {
			xMOVAPS(ptr[(&VU->VF[_Fs_])], xRegisterSSE(EEREC_S));
			xmmregs[EEREC_S].mode &= ~MODE_WRITE;
		}

		FLD32((uptr)&VU->VF[_Fs_].UL[0]);
		FLD32((uptr)&VU->VF[_Fs_].UL[1]);
	}

	FPATAN();
	FSTP32(VU_VI_ADDR(REG_P, 0));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// EATANxz
//------------------------------------------------------------------
void recVUMI_EATANxz( VURegs *VU, int info )
{
	pxAssert( VU == &VU1 );
	//Console.WriteLn("recVUMI_EATANxz");
	if( (xmmregs[EEREC_S].mode & MODE_WRITE) && (xmmregs[EEREC_S].mode&MODE_NOFLUSH) ) {
		xMOVL.PS(ptr[s_tempmem], xRegisterSSE(EEREC_S));
		FLD32((uptr)&s_tempmem[0]);
		FLD32((uptr)&s_tempmem[2]);
	}
	else {
		if( xmmregs[EEREC_S].mode & MODE_WRITE ) {
			xMOVAPS(ptr[(&VU->VF[_Fs_])], xRegisterSSE(EEREC_S));
			xmmregs[EEREC_S].mode &= ~MODE_WRITE;
		}

		FLD32((uptr)&VU->VF[_Fs_].UL[0]);
		FLD32((uptr)&VU->VF[_Fs_].UL[2]);
	}
	FPATAN();
	FSTP32(VU_VI_ADDR(REG_P, 0));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ESUM*
//------------------------------------------------------------------
void recVUMI_ESUM( VURegs *VU, int info )
{
	//Console.WriteLn("VU: ESUM");
	pxAssert( VU == &VU1 );

	if( x86caps.hasStreamingSIMD3Extensions ) {
		xMOVAPS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
		if (CHECK_VU_EXTRA_OVERFLOW) vuFloat_useEAX(info, EEREC_TEMP, 0xf);
		xHADD.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP));
		xHADD.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP));
	}
	else {
		xMOVHL.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S)); // z, w, z, w
		xADD.PS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S)); // z+x, w+y, z+z, w+w
		xUNPCK.LPS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP)); // z+x, z+x, w+y, w+y
		xMOVHL.PS(xRegisterSSE(EEREC_D), xRegisterSSE(EEREC_TEMP)); // w+y, w+y, w+y, w+y
		xADD.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_D)); // x+y+z+w, w+y, w+y, w+y
	}

	vuFloat_useEAX(info, EEREC_TEMP, 8);
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_TEMP));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ERCPR*
//------------------------------------------------------------------
void recVUMI_ERCPR( VURegs *VU, int info )
{
	pxAssert( VU == &VU1 );
	//Console.WriteLn("VU1: ERCPR");

	// don't use RCPSS (very bad precision)
	switch ( _Fsf_ ) {
		case 0: //0001
			if (CHECK_VU_EXTRA_OVERFLOW) vuFloat5_useEAX(EEREC_S, EEREC_TEMP, 8);
			xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]); // temp <- 1
			xDIV.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			break;
		case 1: //0010
			xPSHUF.LW(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0x4e);
			if (CHECK_VU_EXTRA_OVERFLOW) vuFloat5_useEAX(EEREC_S, EEREC_TEMP, 8);
			xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]); // temp <- 1
			xDIV.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			xPSHUF.LW(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0x4e);
			break;
		case 2: //0100
			xSHUF.PS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0xc6);
			if (CHECK_VU_EXTRA_OVERFLOW) vuFloat5_useEAX(EEREC_S, EEREC_TEMP, 8);
			xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]); // temp <- 1
			xDIV.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			xSHUF.PS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0xc6);
			break;
		case 3: //1000
			xSHUF.PS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0x27);
			if (CHECK_VU_EXTRA_OVERFLOW) vuFloat5_useEAX(EEREC_S, EEREC_TEMP, 8);
			xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]); // temp <- 1
			xDIV.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_S));
			xSHUF.PS(xRegisterSSE(EEREC_S), xRegisterSSE(EEREC_S), 0x27);
			break;
	}

	vuFloat_useEAX(info, EEREC_TEMP, 8);
	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_TEMP));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ESQRT*
//------------------------------------------------------------------
void recVUMI_ESQRT( VURegs *VU, int info )
{
	pxAssert( VU == &VU1 );

	//Console.WriteLn("VU1: ESQRT");
	_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);
	xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[const_clip]); // abs(x)
	if (CHECK_VU_OVERFLOW) xMIN.SS(xRegisterSSE(EEREC_TEMP), ptr[g_maxvals]); // Only need to do positive clamp
	xSQRT.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP));

	xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_TEMP));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ERSQRT*
//------------------------------------------------------------------
void recVUMI_ERSQRT( VURegs *VU, int info )
{
	int t1reg = _vuGetTempXMMreg(info);

	pxAssert( VU == &VU1 );
	//Console.WriteLn("VU1: ERSQRT");

	_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Fsf_);
	xAND.PS(xRegisterSSE(EEREC_TEMP), ptr[const_clip]); // abs(x)
	xMIN.SS(xRegisterSSE(EEREC_TEMP), ptr[g_maxvals]); // Clamp Infinities to Fmax
	xSQRT.SS(xRegisterSSE(EEREC_TEMP), xRegisterSSE(EEREC_TEMP)); // SQRT(abs(x))

	if( t1reg >= 0 )
	{
		xMOVSSZX(xRegisterSSE(t1reg), ptr[VU_ONE]);
		xDIV.SS(xRegisterSSE(t1reg), xRegisterSSE(EEREC_TEMP));
		vuFloat_useEAX(info, t1reg, 8);
		xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(t1reg));
		_freeXMMreg(t1reg);
	}
	else
	{
		xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_TEMP));
		xMOVSSZX(xRegisterSSE(EEREC_TEMP), ptr[VU_ONE]);
		xDIV.SS(xRegisterSSE(EEREC_TEMP), ptr[(void*)(VU_VI_ADDR(REG_P, 0))]);
		vuFloat_useEAX(info, EEREC_TEMP, 8);
		xMOVSS(ptr[(void*)(VU_VI_ADDR(REG_P, 0))], xRegisterSSE(EEREC_TEMP));
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ESIN
//------------------------------------------------------------------
void recVUMI_ESIN( VURegs *VU, int info )
{
	pxAssert( VU == &VU1 );

	//Console.WriteLn("recVUMI_ESIN");
	if( (xmmregs[EEREC_S].mode & MODE_WRITE) && (xmmregs[EEREC_S].mode&MODE_NOFLUSH) ) {
		switch(_Fsf_) {
			case 0: xMOVSS(ptr[s_tempmem], xRegisterSSE(EEREC_S)); break;
			case 1: xMOVL.PS(ptr[s_tempmem], xRegisterSSE(EEREC_S)); break;
			default: xMOVH.PS(ptr[&s_tempmem[2]], xRegisterSSE(EEREC_S)); break;
		}
		FLD32((uptr)&s_tempmem[_Fsf_]);
	}
	else {
		if( xmmregs[EEREC_S].mode & MODE_WRITE ) {
			xMOVAPS(ptr[(&VU->VF[_Fs_])], xRegisterSSE(EEREC_S));
			xmmregs[EEREC_S].mode &= ~MODE_WRITE;
		}

		FLD32((uptr)&VU->VF[_Fs_].UL[_Fsf_]);
	}

	FSIN();
	FSTP32(VU_VI_ADDR(REG_P, 0));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// EATAN
//------------------------------------------------------------------
void recVUMI_EATAN( VURegs *VU, int info )
{
	pxAssert( VU == &VU1 );

	//Console.WriteLn("recVUMI_EATAN");
	if( (xmmregs[EEREC_S].mode & MODE_WRITE) && (xmmregs[EEREC_S].mode&MODE_NOFLUSH) ) {
		switch(_Fsf_) {
			case 0: xMOVSS(ptr[s_tempmem], xRegisterSSE(EEREC_S)); break;
			case 1: xMOVL.PS(ptr[s_tempmem], xRegisterSSE(EEREC_S));  break;
			default: xMOVH.PS(ptr[&s_tempmem[2]], xRegisterSSE(EEREC_S)); break;
		}
		FLD32((uptr)&s_tempmem[_Fsf_]);
	}
	else {
		if( xmmregs[EEREC_S].mode & MODE_WRITE ) {
			xMOVAPS(ptr[(&VU->VF[_Fs_])], xRegisterSSE(EEREC_S));
			xmmregs[EEREC_S].mode &= ~MODE_WRITE;
		}
	}

	FLD1();
	FLD32((uptr)&VU->VF[_Fs_].UL[_Fsf_]);
	FPATAN();
	FSTP32(VU_VI_ADDR(REG_P, 0));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// EEXP
//------------------------------------------------------------------
void recVUMI_EEXP( VURegs *VU, int info )
{
	pxAssert( VU == &VU1 );
	//Console.WriteLn("recVUMI_EEXP");
	FLDL2E();

	if( (xmmregs[EEREC_S].mode & MODE_WRITE) && (xmmregs[EEREC_S].mode&MODE_NOFLUSH) ) {
		switch(_Fsf_) {
		case 0: xMOVSS(ptr[s_tempmem], xRegisterSSE(EEREC_S)); break;
			case 1: xMOVL.PS(ptr[s_tempmem], xRegisterSSE(EEREC_S)); break;
			default: xMOVH.PS(ptr[&s_tempmem[2]], xRegisterSSE(EEREC_S)); break;
		}
		FMUL32((uptr)&s_tempmem[_Fsf_]);
	}
	else {
		if( xmmregs[EEREC_S].mode & MODE_WRITE ) {
			xMOVAPS(ptr[(&VU->VF[_Fs_])], xRegisterSSE(EEREC_S));
			xmmregs[EEREC_S].mode &= ~MODE_WRITE;
		}

		FMUL32((uptr)&VU->VF[_Fs_].UL[_Fsf_]);
	}

	// basically do 2^(log_2(e) * val)
	FLD(0);
	FRNDINT();
	FXCH(1);
	FSUB32Rto0(1);
	F2XM1();
	FLD1();
	FADD320toR(1);
	FSCALE();
	FSTP(1);

	FSTP32(VU_VI_ADDR(REG_P, 0));
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// XITOP
//------------------------------------------------------------------
void recVUMI_XITOP( VURegs *VU, int info )
{
	int itreg;
	if (_It_ == 0) return;
	//Console.WriteLn("recVUMI_XITOP");
	itreg = ALLOCVI(_It_, MODE_WRITE);
	xMOVZX(xRegister32(itreg), ptr16[(u16*)((uptr)&VU->GetVifRegs().itop )]);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// XTOP
//------------------------------------------------------------------
void recVUMI_XTOP( VURegs *VU, int info )
{
	int itreg;
	if ( _It_ == 0 ) return;
	//Console.WriteLn("recVUMI_XTOP");
	itreg = ALLOCVI(_It_, MODE_WRITE);
	xMOVZX(xRegister32(itreg), ptr16[(u16*)((uptr)&VU->GetVifRegs().top )]);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// VU1XGKICK_MTGSTransfer() - Called by ivuZerorec.cpp
//------------------------------------------------------------------

void __fastcall VU1XGKICK_MTGSTransfer(u32 addr)
{
	addr &= 0x3fff;
	u32 diff = 0x4000 - addr;
	u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, vuRegs[1].Mem, addr);

	if (size > diff) {
		//DevCon.WriteLn(Color_Green, "superVU1: XGkick Wrap!");
		gifUnit.gifPath[0].CopyGSPacketData(&vuRegs[1].Mem[addr], diff, true);
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[0],size-diff,true);
	}
	else {
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[addr], size, true);
	}
}
//------------------------------------------------------------------
