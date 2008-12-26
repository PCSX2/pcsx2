/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
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
#include <assert.h>
#include <string.h>

#include "Common.h"
#include "GS.h"
#include "InterTables.h"
#include "ix86/ix86.h"
#include "iR5900.h"
#include "iMMI.h"
#include "iFPU.h"
#include "iCP0.h"
#include "VUmicro.h"
#include "VUflags.h"
#include "iVUmicro.h"
#include "iVU0micro.h"
#include "iVU1micro.h"
#include "iVUops.h"
#include "iVUzerorec.h"
//------------------------------------------------------------------


//------------------------------------------------------------------
// Helper Macros
//------------------------------------------------------------------
#define _Ft_ (( VU->code >> 16) & 0x1F)  // The rt part of the instruction register 
#define _Fs_ (( VU->code >> 11) & 0x1F)  // The rd part of the instruction register 
#define _Fd_ (( VU->code >>  6) & 0x1F)  // The sa part of the instruction register

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
//------------------------------------------------------------------


//------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------
static const PCSX2_ALIGNED16(int SSEmovMask[ 16 ][ 4 ]) =
{
	{ 0x00000000, 0x00000000, 0x00000000, 0x00000000 },
	{ 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF },
	{ 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000 },
	{ 0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF },
	{ 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000 },
	{ 0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF },
	{ 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 },
	{ 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF },
	{ 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000 },
	{ 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF },
	{ 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000 },
	{ 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF },
	{ 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000 },
	{ 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF },
	{ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 },
	{ 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF }
};

static const PCSX2_ALIGNED16(u32 const_abs_table[16][4]) = 
{
   { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff }, //0000
   { 0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff }, //0001
   { 0xffffffff, 0xffffffff, 0x7fffffff, 0xffffffff }, //0010
   { 0xffffffff, 0xffffffff, 0x7fffffff, 0x7fffffff }, //0011
   { 0xffffffff, 0x7fffffff, 0xffffffff, 0xffffffff }, //0100
   { 0xffffffff, 0x7fffffff, 0xffffffff, 0x7fffffff }, //0101
   { 0xffffffff, 0x7fffffff, 0x7fffffff, 0xffffffff }, //0110
   { 0xffffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff }, //0111
   { 0x7fffffff, 0xffffffff, 0xffffffff, 0xffffffff }, //1000
   { 0x7fffffff, 0xffffffff, 0xffffffff, 0x7fffffff }, //1001
   { 0x7fffffff, 0xffffffff, 0x7fffffff, 0xffffffff }, //1010
   { 0x7fffffff, 0xffffffff, 0x7fffffff, 0x7fffffff }, //1011
   { 0x7fffffff, 0x7fffffff, 0xffffffff, 0xffffffff }, //1100
   { 0x7fffffff, 0x7fffffff, 0xffffffff, 0x7fffffff }, //1101
   { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0xffffffff }, //1110
   { 0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff }, //1111
};

static const PCSX2_ALIGNED16(float recMult_float_to_int4[4])	= { 16.0, 16.0, 16.0, 16.0 };
static const PCSX2_ALIGNED16(float recMult_float_to_int12[4])	= { 4096.0, 4096.0, 4096.0, 4096.0 };
static const PCSX2_ALIGNED16(float recMult_float_to_int15[4])	= { 32768.0, 32768.0, 32768.0, 32768.0 };

static const PCSX2_ALIGNED16(float recMult_int_to_float4[4])	= { 0.0625f, 0.0625f, 0.0625f, 0.0625f };
static const PCSX2_ALIGNED16(float recMult_int_to_float12[4])	= { 0.000244140625, 0.000244140625, 0.000244140625, 0.000244140625 };
static const PCSX2_ALIGNED16(float recMult_int_to_float15[4])	= { 0.000030517578125, 0.000030517578125, 0.000030517578125, 0.000030517578125 };

static const PCSX2_ALIGNED16(u32 VU_Underflow_Mask1[4])			= {0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000};
static const PCSX2_ALIGNED16(u32 VU_Underflow_Mask2[4])			= {0x007fffff, 0x007fffff, 0x007fffff, 0x007fffff};
static const PCSX2_ALIGNED16(u32 VU_Zero_Mask[4])				= {0x00000000, 0x00000000, 0x00000000, 0x00000000};
static const PCSX2_ALIGNED16(u32 VU_Zero_Helper_Mask[4])		= {0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};
static const PCSX2_ALIGNED16(u32 VU_Signed_Zero_Mask[4])		= {0x80000000, 0x80000000, 0x80000000, 0x80000000};
static const PCSX2_ALIGNED16(u32 VU_Pos_Infinity[4])			= {0x7f800000, 0x7f800000, 0x7f800000, 0x7f800000};
static const PCSX2_ALIGNED16(u32 VU_Neg_Infinity[4])			= {0xff800000, 0xff800000, 0xff800000, 0xff800000};
//------------------------------------------------------------------


//------------------------------------------------------------------
// recUpdateFlags() - Computes the flags for the Upper Opcodes
//
// NOTE: Computes under/overflow flags if CHECK_VU_EXTRA_FLAGS is 1
//------------------------------------------------------------------
PCSX2_ALIGNED16(u64 TEMPXMMData[2]);
void recUpdateFlags(VURegs * VU, int reg, int info)
{
	static u8* pjmp;
	static u32* pjmp32;
	static u32 macaddr, stataddr, prevstataddr;
	static int x86macflag, x86temp;
	static int t1reg, t1regBoolean;
	static const int flipMask[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

	if( !(info & PROCESS_VU_UPDATEFLAGS) ) return;

	macaddr = VU_VI_ADDR(REG_MAC_FLAG, 0);
	stataddr = VU_VI_ADDR(REG_STATUS_FLAG, 0); // write address
	prevstataddr = VU_VI_ADDR(REG_STATUS_FLAG, 2); // previous address

	if( stataddr == 0 ) stataddr = prevstataddr;
	if( macaddr == 0 ) {
		SysPrintf( "VU ALLOCATION WARNING: Using Mac Flag Previous Address!\n" );
		macaddr = VU_VI_ADDR(REG_MAC_FLAG, 2);
	}
	
	x86macflag	= ALLOCTEMPX86(0);
	x86temp		= ALLOCTEMPX86(0);
	
	if (reg == EEREC_TEMP) {
		t1reg = _vuGetTempXMMreg(info);
		if (t1reg < 0) {
			//SysPrintf( "VU ALLOCATION ERROR: Temp reg can't be allocated!!!!\n" );
			t1reg = (reg == 0) ? (reg + 1) : (reg - 1);
			SSE_MOVAPS_XMM_to_M128( (uptr)TEMPXMMData, t1reg );
			t1regBoolean = 1;
		}
		else t1regBoolean = 0;
	}
	else {
		t1reg = EEREC_TEMP;
		t1regBoolean = 2;
	}

	SSE_SHUFPS_XMM_to_XMM(reg, reg, 0x1B); // Flip wzyx to xyzw 
	XOR32RtoR(x86macflag, x86macflag); // Clear Mac Flag
	XOR32RtoR(x86temp, x86temp); //Clear x86temp

	if (CHECK_VU_EXTRA_FLAGS) {
		//-------------------------Check for Overflow flags------------------------------

		//SSE_XORPS_XMM_to_XMM(t1reg, t1reg); // Clear t1reg
		//SSE_CMPUNORDPS_XMM_to_XMM(t1reg, reg); // If reg == NaN then set Vector to 0xFFFFFFFF

		//SSE_MOVAPS_XMM_to_XMM(t1reg, reg);
		//SSE_MINPS_M128_to_XMM(t1reg, (uptr)g_maxvals);
		//SSE_MAXPS_M128_to_XMM(t1reg, (uptr)g_minvals);
		//SSE_CMPNEPS_XMM_to_XMM(t1reg, reg); // If they're not equal, then overflow has occured

		SSE_MOVAPS_XMM_to_XMM(t1reg, reg);
		SSE_ANDPS_M128_to_XMM(t1reg, (uptr)VU_Zero_Helper_Mask);
		SSE_CMPEQPS_M128_to_XMM(t1reg, (uptr)VU_Pos_Infinity); // If infinity, then overflow has occured (NaN's don't report as overflow)

		SSE_MOVMSKPS_XMM_to_R32(EAX, t1reg); // Move the sign bits of the previous calculation

		AND32ItoR(EAX, _X_Y_Z_W );  // Grab "Has Overflowed" bits from the previous calculation (also make sure we're only grabbing from the XYZW being modified)
		pjmp = JZ8(0); // Skip if none are
			OR32ItoR(x86temp, 8); // Set if they are
			OR32RtoR(x86macflag, EAX);
			SHL32ItoR(x86macflag, 8); // Shift the Overflow flags left 8
			pjmp32 = JMP32(0); // Skip Underflow Check
		x86SetJ8(pjmp);

		//-------------------------Check for Underflow flags------------------------------

		SSE_MOVAPS_XMM_to_XMM(t1reg, reg); // t1reg <- reg

		SSE_ANDPS_M128_to_XMM(t1reg, (uptr)&VU_Underflow_Mask1[ 0 ]);
		SSE_CMPEQPS_M128_to_XMM(t1reg, (uptr)&VU_Zero_Mask[ 0 ]); // If (t1reg == zero exponent) then set Vector to 0xFFFFFFFF

		SSE_ANDPS_XMM_to_XMM(t1reg, reg);
		SSE_ANDPS_M128_to_XMM(t1reg, (uptr)&VU_Underflow_Mask2[ 0 ]);
		SSE_CMPNEPS_M128_to_XMM(t1reg, (uptr)&VU_Zero_Mask[ 0 ]); // If (t1reg != zero mantisa) then set Vector to 0xFFFFFFFF

		SSE_MOVMSKPS_XMM_to_R32(EAX, t1reg); // Move the sign bits of the previous calculation

		AND32ItoR(EAX, _X_Y_Z_W );  // Grab "Has Underflowed" bits from the previous calculation
		pjmp = JZ8(0); // Skip if none are
			OR32ItoR(x86temp, 4); // Set if they are
			OR32RtoR(x86macflag, EAX);
			SHL32ItoR(x86macflag, 4); // Shift the Overflow and Underflow flags left 4
		x86SetJ8(pjmp);

		//-------------------------Optional Code: Denormals Are Zero------------------------------
		if (CHECK_VU_UNDERFLOW) {  // Sets underflow/denormals to zero
			SSE_ANDNPS_XMM_to_XMM(t1reg, reg); // t1reg = !t1reg & reg (t1reg = denormals are positive zero)
			VU_MERGE_REGS_SAFE(t1reg, reg, (15 - flipMask[_X_Y_Z_W])); // Send t1reg the vectors that shouldn't be modified (since reg was flipped, we need a mask to get the unmodified vectors)
			// Now we have Denormals are Positive Zero in t1reg; the next two lines take Signed Zero into account
			SSE_ANDPS_M128_to_XMM(reg, (uptr)&VU_Signed_Zero_Mask[ 0 ]); // Only keep the sign bit for each vector
			SSE_ORPS_XMM_to_XMM(reg, t1reg); // Denormals are Signed Zero, and unmodified vectors stay the same!
		}

		x86SetJ32(pjmp32); // If we skipped the Underflow Flag Checking (when we had an Overflow), return here
	}

	vuFloat2(reg, t1reg, flipMask[_X_Y_Z_W]); // Clamp overflowed vectors that were modified (remember reg's vectors have been flipped, so have to use a flipmask)

	//-------------------------Check for Signed flags------------------------------

	// The following code makes sure the Signed Bit isn't set with Negative Zero
	SSE_XORPS_XMM_to_XMM(t1reg, t1reg); // Clear t1reg
	SSE_CMPNEPS_XMM_to_XMM(t1reg, reg); // Set all F's if each vector is not zero
	SSE_ANDPS_XMM_to_XMM(t1reg, reg);

	SSE_MOVMSKPS_XMM_to_R32(EAX, t1reg); // Move the sign bits of the t1reg

	AND32ItoR(EAX, _X_Y_Z_W );  // Grab "Is Signed" bits from the previous calculation
	pjmp = JZ8(0); // Skip if none are
		OR32ItoR(x86temp, 2); // Set if they are
		OR32RtoR(x86macflag, EAX);
		SHL32ItoR(x86macflag, 4); // Shift the Overflow, Underflow, and Zero flags left 4
		pjmp32 = JMP32(0); // If negative and not Zero, we can skip the Zero Flag checking
	x86SetJ8(pjmp);

	SHL32ItoR(x86macflag, 4); // Shift the Overflow, Underflow, and Zero flags left 4

	//-------------------------Check for Zero flags------------------------------
	
	SSE_XORPS_XMM_to_XMM(t1reg, t1reg); // Clear t1reg
	SSE_CMPEQPS_XMM_to_XMM(t1reg, reg); // Set all F's if each vector is zero

	SSE_MOVMSKPS_XMM_to_R32(EAX, t1reg); // Move the sign bits of the previous calculation

	AND32ItoR(EAX, _X_Y_Z_W );  // Grab "Is Zero" bits from the previous calculation
	pjmp = JZ8(0); // Skip if none are
		OR32ItoR(x86temp, 1); // Set if they are
		OR32RtoR(x86macflag, EAX);
	x86SetJ8(pjmp);

	//-------------------------Finally: Send the Flags to the Mac Flag Address------------------------------

	x86SetJ32(pjmp32); // If we skipped the Zero Flag Checking, return here

	SSE_SHUFPS_XMM_to_XMM(reg, reg, 0x1B); // Flip back reg to wzyx

	if (t1regBoolean == 1) SSE_MOVAPS_M128_to_XMM( t1reg, (uptr)TEMPXMMData );
	else if (t1regBoolean == 0) _freeXMMreg(t1reg);

	MOV16RtoM(macaddr, x86macflag);

	MOV32MtoR(x86macflag, prevstataddr); // Load the previous status in to x86macflag
	AND32ItoR(x86macflag, 0xff0); // Keep Sticky and D/I flags
	OR32RtoR(x86macflag, x86temp);
	SHL32ItoR(x86temp, 6);
	OR32RtoR(x86macflag, x86temp);
	MOV32RtoM(stataddr, x86macflag);

	_freeX86reg(x86macflag);
	_freeX86reg(x86temp);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// *VU Upper Instructions!*
//
// Note: * = Checked for errors by cottonvibes
//------------------------------------------------------------------


//------------------------------------------------------------------
// ABS*
//------------------------------------------------------------------
void recVUMI_ABS(VURegs *VU, int info) 
{
	//SysPrintf("recVUMI_ABS()\n");
	if ( (_Ft_ == 0) || (_X_Y_Z_W == 0) ) return;

	if ((_X_Y_Z_W == 0x8) || (_X_Y_Z_W == 0xf)) {
		VU_MERGE_REGS(EEREC_T, EEREC_S);
		SSE_ANDPS_M128_to_XMM(EEREC_T, (uptr)&const_abs_table[ _X_Y_Z_W ][ 0 ] );
	}
	else { // Use a temp reg because VU_MERGE_REGS() modifies source reg!
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_ANDPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_abs_table[ _X_Y_Z_W ][ 0 ] );
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ADD*, ADD_iq*, ADD_xyzw*
//------------------------------------------------------------------
PCSX2_ALIGNED16(float s_two[4]) = {0,0,0,2};
void recVUMI_ADD(VURegs *VU, int info)
{
	//SysPrintf("recVUMI_ADD()\n");
	if ( _X_Y_Z_W == 0 ) goto flagUpdate;
	if ( !_Fd_ ) info = (info & ~PROCESS_EE_SET_D(0xf)) | PROCESS_EE_SET_D(EEREC_TEMP);

	if ( _Fs_ == 0 && _Ft_ == 0 ) { // if adding VF00 with VF00, then the result is always 0,0,0,2
		if ( _X_Y_Z_W == 0x8 ) SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)s_two);
		else if ( _X_Y_Z_W != 0xf ) {
			SSE_MOVAPS_M128_to_XMM(EEREC_TEMP, (uptr)s_two);
			VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
		}
		else SSE_MOVAPS_M128_to_XMM(EEREC_D, (uptr)s_two);
	}
	else {
		if (CHECK_VU_EXTRA_OVERFLOW) {
			vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W); 
			vuFloat5( EEREC_T, EEREC_TEMP, _X_Y_Z_W);
		}
		if( _X_Y_Z_W == 8 ) { // If only adding x, then we can do a Scalar Add
			if (EEREC_D == EEREC_S) SSE_ADDSS_XMM_to_XMM(EEREC_D, EEREC_T);
			else if (EEREC_D == EEREC_T) SSE_ADDSS_XMM_to_XMM(EEREC_D, EEREC_S);
			else {
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_ADDSS_XMM_to_XMM(EEREC_D, EEREC_T);
			}
		}
		else if (_X_Y_Z_W != 0xf) { // If xyzw != 1111, then we have to use a temp reg
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
			VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
		}
		else { // All xyzw being modified (xyzw == 1111)
			if (EEREC_D == EEREC_S) SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_T);
			else if (EEREC_D == EEREC_T) SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_S);
			else {
				SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_T);
			}
		}
	}
flagUpdate:
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_ADD_iq(VURegs *VU, uptr addr, int info)
{
	//SysPrintf("recVUMI_ADD_iq()\n");
	if ( _X_Y_Z_W == 0 ) goto flagUpdate;
	if ( !_Fd_ ) info = (info & ~PROCESS_EE_SET_D(0xf)) | PROCESS_EE_SET_D(EEREC_TEMP);
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat3(addr);
		if (_Fs_) vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
	}

	if ( _XYZW_SS ) {
		if ( EEREC_D == EEREC_TEMP ) {
			_vuFlipRegSS(VU, EEREC_S);
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_ADDSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_S);
			_vuFlipRegSS(VU, EEREC_D); // have to flip over EEREC_D for computing flags!
		}
		else if ( EEREC_D == EEREC_S ) {
			_vuFlipRegSS(VU, EEREC_D);
			SSE_ADDSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_D);
		}
		else {
			if ( _X ) {
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_ADDSS_M32_to_XMM(EEREC_D, addr);
			}
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
				SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
			}
		}
	}
	else {
		if ( (_X_Y_Z_W != 0xf) || (EEREC_D == EEREC_S) || (EEREC_D == EEREC_TEMP) ) {
			SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
			SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
		}

		if (_X_Y_Z_W != 0xf) {
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
		} 
		else {
			if ( EEREC_D == EEREC_TEMP ) SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_S);
			else if ( EEREC_D == EEREC_S ) SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_D, addr); 
				SSE_SHUFPS_XMM_to_XMM(EEREC_D, EEREC_D, 0x00);
				SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_S);
			}
		}
	}
flagUpdate:
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_ADD_xyzw(VURegs *VU, int xyzw, int info)
{
	//SysPrintf("recVUMI_ADD_xyzw()\n");
	if ( _X_Y_Z_W == 0 ) goto flagUpdate;
	if ( !_Fd_ ) info = (info & ~PROCESS_EE_SET_D(0xf)) | PROCESS_EE_SET_D(EEREC_TEMP);
	if (CHECK_VU_EXTRA_OVERFLOW) {
		if (_Fs_) vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		if (_Ft_) vuFloat5( EEREC_T, EEREC_TEMP, ( 1 << (3 - xyzw) ) );
	}

	if ( _Ft_ == 0 && xyzw < 3 ) { // just move since adding zero
		if ( _X_Y_Z_W == 0x8 ) { VU_MERGE_REGS(EEREC_D, EEREC_S); }
		else if ( _X_Y_Z_W != 0xf ) {
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
		}
		else if ( EEREC_D != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_S);
	}
	else if ( _X_Y_Z_W == 8 && (EEREC_D != EEREC_TEMP) ) {
		if ( xyzw == 0 ) {
			if ( EEREC_D == EEREC_T ) SSE_ADDSS_XMM_to_XMM(EEREC_D, EEREC_S);
			else {
				if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_ADDSS_XMM_to_XMM(EEREC_D, EEREC_T);
			}
		}
		else {
			_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			if ( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_ADDSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
		}
	}
	else if( _Fs_ == 0 && !_W ) { // just move
		_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
		VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
	}
	else {
		if ( _X_Y_Z_W != 0xf ) {
			_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
		} 
		else {
			if( EEREC_D == EEREC_TEMP )	  { _unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw); SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_S); }
			else if( EEREC_D == EEREC_S ) { _unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw); SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_TEMP); }
			else { _unpackVF_xyzw(EEREC_D, EEREC_T, xyzw); SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_S); }
		}
	}
flagUpdate:
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_ADDi(VURegs *VU, int info) { recVUMI_ADD_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_ADDq(VURegs *VU, int info) { recVUMI_ADD_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_ADDx(VURegs *VU, int info) { recVUMI_ADD_xyzw(VU, 0, info); }
void recVUMI_ADDy(VURegs *VU, int info) { recVUMI_ADD_xyzw(VU, 1, info); }
void recVUMI_ADDz(VURegs *VU, int info) { recVUMI_ADD_xyzw(VU, 2, info); }
void recVUMI_ADDw(VURegs *VU, int info) { recVUMI_ADD_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// ADDA*, ADDA_iq*, ADDA_xyzw*
//------------------------------------------------------------------
void recVUMI_ADDA(VURegs *VU, int info)
{
	//SysPrintf("recVUMI_ADDA()\n");
	if ( _X_Y_Z_W == 0 ) goto flagUpdate;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		if (_Fs_) vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		if (_Ft_) vuFloat5( EEREC_T, EEREC_TEMP, _X_Y_Z_W);
	}

	if( _X_Y_Z_W == 8 ) {
		if (EEREC_ACC == EEREC_S) SSE_ADDSS_XMM_to_XMM(EEREC_ACC, EEREC_T);	// Can this case happen? (cottonvibes)
		else if (EEREC_ACC == EEREC_T) SSE_ADDSS_XMM_to_XMM(EEREC_ACC, EEREC_S); // Can this case happen?
		else {
			SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_ADDSS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);

		VU_MERGE_REGS(EEREC_ACC, EEREC_TEMP);
	}
	else {
		if( EEREC_ACC == EEREC_S ) SSE_ADDPS_XMM_to_XMM(EEREC_ACC, EEREC_T); // Can this case happen?
		else if( EEREC_ACC == EEREC_T ) SSE_ADDPS_XMM_to_XMM(EEREC_ACC, EEREC_S); // Can this case happen?
		else {
			SSE_MOVAPS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_ADDPS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		}
	}
flagUpdate:
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_ADDA_iq(VURegs *VU, uptr addr, int info)
{
	//SysPrintf("recVUMI_ADDA_iq()\n");
	if ( _X_Y_Z_W == 0 ) goto flagUpdate;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat3(addr);
		if (_Fs_) vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
	}

	if( _XYZW_SS ) {
		assert( EEREC_ACC != EEREC_TEMP );
		if( EEREC_ACC == EEREC_S ) {
			_vuFlipRegSS(VU, EEREC_ACC);
			SSE_ADDSS_M32_to_XMM(EEREC_ACC, addr);
			_vuFlipRegSS(VU, EEREC_ACC);
		}
		else {
			if( _X ) {
				SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
				SSE_ADDSS_M32_to_XMM(EEREC_ACC, addr);
			}
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
				SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(EEREC_ACC, EEREC_TEMP);
			}
		}
	}
	else {
		if( _X_Y_Z_W != 0xf || EEREC_ACC == EEREC_S ) {
			SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
			SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
		}

		if (_X_Y_Z_W != 0xf) {
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			VU_MERGE_REGS(EEREC_ACC, EEREC_TEMP);
		}
		else {
			if( EEREC_ACC == EEREC_S ) SSE_ADDPS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_ACC, addr); 
				SSE_SHUFPS_XMM_to_XMM(EEREC_ACC, EEREC_ACC, 0x00);
				SSE_ADDPS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			}
		}
	}
flagUpdate:
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_ADDA_xyzw(VURegs *VU, int xyzw, int info)
{
	//SysPrintf("recVUMI_ADDA_xyzw()\n");
	if ( _X_Y_Z_W == 0 ) goto flagUpdate;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		if (_Fs_) vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		if (_Ft_) vuFloat5( EEREC_T, EEREC_TEMP, ( 1 << (3 - xyzw) ) );
	}

	if( _X_Y_Z_W == 8 ) {
		assert( EEREC_ACC != EEREC_T );
		if( xyzw == 0 ) {
			SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_ADDSS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		}
		else {
			_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			if( _Fs_ == 0 ) {
				SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
			}
			else {
				SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
				SSE_ADDSS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
			}
		}
	}
	else {
		if( _X_Y_Z_W != 0xf || EEREC_ACC == EEREC_S )
			_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);

		if (_X_Y_Z_W != 0xf) {
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			VU_MERGE_REGS(EEREC_ACC, EEREC_TEMP);
		} 
		else {
			if( EEREC_ACC == EEREC_S ) SSE_ADDPS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
			else {
				_unpackVF_xyzw(EEREC_ACC, EEREC_T, xyzw);
				SSE_ADDPS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			}
		}
	}
flagUpdate:
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_ADDAi(VURegs *VU, int info) { recVUMI_ADDA_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_ADDAq(VURegs *VU, int info) { recVUMI_ADDA_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_ADDAx(VURegs *VU, int info) { recVUMI_ADDA_xyzw(VU, 0, info); }
void recVUMI_ADDAy(VURegs *VU, int info) { recVUMI_ADDA_xyzw(VU, 1, info); }
void recVUMI_ADDAz(VURegs *VU, int info) { recVUMI_ADDA_xyzw(VU, 2, info); }
void recVUMI_ADDAw(VURegs *VU, int info) { recVUMI_ADDA_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// SUB
//------------------------------------------------------------------
void recVUMI_SUB(VURegs *VU, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		vuFloat5( EEREC_T, EEREC_TEMP, _X_Y_Z_W);
	}
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);

	if( EEREC_S == EEREC_T ) {
		if (_X_Y_Z_W != 0xf) SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&SSEmovMask[15-_X_Y_Z_W][0]);
		else SSE_XORPS_XMM_to_XMM(EEREC_D, EEREC_D);
	}
	else if( _X_Y_Z_W == 8 ) {
		if (EEREC_D == EEREC_S) SSE_SUBSS_XMM_to_XMM(EEREC_D, EEREC_T);
		else if (EEREC_D == EEREC_T) {
			SSE_MOVSS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			SSE_SUBSS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
		}
		else {
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_SUBSS_XMM_to_XMM(EEREC_D, EEREC_T);
		}
	}
	else {
		if (_X_Y_Z_W != 0xf) {
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if( _Ft_ > 0 || _W ) SSE_SUBPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);

			VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
		}
		else {
			if (EEREC_D == EEREC_S) SSE_SUBPS_XMM_to_XMM(EEREC_D, EEREC_T);
			else if (EEREC_D == EEREC_T) {
				SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
				SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			}
			else {
				SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(EEREC_D, EEREC_T);
			}
		}
	}
	recUpdateFlags(VU, EEREC_D, info);
	// neopets works better with Overflow Checking?
}

void recVUMI_SUB_iq(VURegs *VU, uptr addr, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat3(addr);
		vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
	}
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);

	if( _XYZW_SS ) {
		if( EEREC_D == EEREC_TEMP ) {
			_vuFlipRegSS(VU, EEREC_S);
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_SUBSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_S);

			// have to flip over EEREC_D if computing flags!
			//if( (info & PROCESS_VU_UPDATEFLAGS) )
				_vuFlipRegSS(VU, EEREC_D);
		}
		else if( EEREC_D == EEREC_S ) {
			_vuFlipRegSS(VU, EEREC_D);
			SSE_SUBSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_D);
		}
		else {
			if( _X ) {
				if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_SUBSS_M32_to_XMM(EEREC_D, addr);
			}
			else {
				_vuMoveSS(VU, EEREC_TEMP, EEREC_S);
				_vuFlipRegSS(VU, EEREC_D);
				SSE_SUBSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
				_vuFlipRegSS(VU, EEREC_D);
			}
		}
	}
	else {
		SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
		SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);

		if (_X_Y_Z_W != 0xf) {
			int t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) {
				SSE_MOVAPS_XMM_to_XMM(t1reg, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(t1reg, EEREC_TEMP);

				VU_MERGE_REGS(EEREC_D, t1reg);
				_freeXMMreg(t1reg);
			}
			else {
				// negate
				SSE_XORPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[4]);
				SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
			}
		}
		else {
			if( EEREC_D == EEREC_TEMP ) {
				SSE_XORPS_M128_to_XMM(EEREC_D, (uptr)&const_clip[4]);
				SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_S);
			}
			else {
				if (EEREC_D != EEREC_S) SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			}
		}
	}
	recUpdateFlags(VU, EEREC_D, info);
}

static const PCSX2_ALIGNED16(u32 s_unaryminus[4]) = {0x80000000, 0, 0, 0};

void recVUMI_SUB_xyzw(VURegs *VU, int xyzw, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		vuFloat5( EEREC_T, EEREC_TEMP, ( 1 << (3 - xyzw) ) );
	}
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);

	if( _X_Y_Z_W == 8 ) {
        if( EEREC_D == EEREC_TEMP ) {
			
            switch (xyzw) {
		        case 0:
			        if( EEREC_TEMP != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			        break;
		        case 1:
			        if( cpucaps.hasStreamingSIMD3Extensions ) SSE3_MOVSLDUP_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			        else {
				        if( EEREC_TEMP != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				        SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
			        }
			        break;
		        case 2:
			        SSE_MOVLHPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			        break;
		        case 3:
			        if( cpucaps.hasStreamingSIMD3Extensions && EEREC_TEMP != EEREC_S ) {
				        SSE3_MOVSLDUP_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				        SSE_MOVLHPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP);
			        }
			        else {
				        if( EEREC_TEMP != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				        SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0);
			        }
			        break;
	        }

			SSE_SUBPS_XMM_to_XMM(EEREC_D, EEREC_T);

			// have to flip over EEREC_D if computing flags!
            //if( (info & PROCESS_VU_UPDATEFLAGS) ) {
                if( xyzw == 1 ) SSE_SHUFPS_XMM_to_XMM(EEREC_D, EEREC_D, 0xe1); // y
	            else if( xyzw == 2 ) SSE_SHUFPS_XMM_to_XMM(EEREC_D, EEREC_D, 0xc6); // z
	            else if( xyzw == 3 ) SSE_SHUFPS_XMM_to_XMM(EEREC_D, EEREC_D, 0x27); // w
            //}
		}
		else {	
		    if( xyzw == 0 ) {
			    if( EEREC_D == EEREC_T ) {
				    if( _Fs_ > 0 ) SSE_SUBSS_XMM_to_XMM(EEREC_D, EEREC_S);
				    SSE_XORPS_M128_to_XMM(EEREC_D, (uptr)s_unaryminus);
			    }
			    else {
				    if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				    SSE_SUBSS_XMM_to_XMM(EEREC_D, EEREC_T);
			    }
		    }
		    else {
			    _unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			    SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			    SSE_SUBSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
		    }
        }
	}
	// ToDo: Check if we can remove the commented code below (cottonvibes)
//	else if( _XYZW_SS && xyzw == 0 ) {
//		if( EEREC_D == EEREC_S ) {
//			if( EEREC_D == EEREC_T ) {
//				SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
//				_vuFlipRegSS(VU, EEREC_D);
//				SSE_SUBSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
//				_vuFlipRegSS(VU, EEREC_D);
//			}
//			else {
//				_vuFlipRegSS(VU, EEREC_D);
//				SSE_SUBSS_XMM_to_XMM(EEREC_D, EEREC_T);
//				_vuFlipRegSS(VU, EEREC_D);
//			}
//		}
//		else if( EEREC_D == EEREC_T ) {
//			_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Y?1:(_Z?2:3));
//			SSE_SUBSS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
//			_vuFlipRegSS(VU, EEREC_D);
//			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
//			_vuFlipRegSS(VU, EEREC_D);
//		}
//		else {
//			_unpackVFSS_xyzw(EEREC_TEMP, EEREC_S, _Y?1:(_Z?2:3));
//			_vuFlipRegSS(VU, EEREC_D);
//			SSE_SUBSS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
//			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
//			_vuFlipRegSS(VU, EEREC_D);
//		}
//	}
	else {
		_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);

		if (_X_Y_Z_W != 0xf) {
			int t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) {
				SSE_MOVAPS_XMM_to_XMM(t1reg, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(t1reg, EEREC_TEMP);

				VU_MERGE_REGS(EEREC_D, t1reg);
				_freeXMMreg(t1reg);
			}
			else {
				// negate
				SSE_XORPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[4]);
				SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
			}
		}
		else {
			if( EEREC_D == EEREC_TEMP ) {
				SSE_XORPS_M128_to_XMM(EEREC_D, (uptr)&const_clip[4]);
				SSE_ADDPS_XMM_to_XMM(EEREC_D, EEREC_S);
			}
			else {
				if( EEREC_D != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			}
		}
	}
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_SUBi(VURegs *VU, int info) { recVUMI_SUB_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_SUBq(VURegs *VU, int info) { recVUMI_SUB_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_SUBx(VURegs *VU, int info) { recVUMI_SUB_xyzw(VU, 0, info); }
void recVUMI_SUBy(VURegs *VU, int info) { recVUMI_SUB_xyzw(VU, 1, info); }
void recVUMI_SUBz(VURegs *VU, int info) { recVUMI_SUB_xyzw(VU, 2, info); }
void recVUMI_SUBw(VURegs *VU, int info) { recVUMI_SUB_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// SUBA
//------------------------------------------------------------------
void recVUMI_SUBA(VURegs *VU, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		vuFloat5( EEREC_T, EEREC_TEMP, _X_Y_Z_W);
		vuFloat5( EEREC_ACC, EEREC_TEMP, _X_Y_Z_W);
	}

	if( EEREC_S == EEREC_T ) {
		if (_X_Y_Z_W != 0xf) SSE_ANDPS_M128_to_XMM(EEREC_ACC, (uptr)&SSEmovMask[15-_X_Y_Z_W][0]);
		else SSE_XORPS_XMM_to_XMM(EEREC_ACC, EEREC_ACC);
	}
	else if( _X_Y_Z_W == 8 ) {
		if (EEREC_ACC == EEREC_S) SSE_SUBSS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		else if (EEREC_ACC == EEREC_T) {
			SSE_MOVSS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			SSE_SUBSS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
			SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
		}
		else {
			SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_SUBSS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_SUBPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);

		VU_MERGE_REGS(EEREC_ACC, EEREC_TEMP);
	}
	else {
		if( EEREC_ACC == EEREC_S ) SSE_SUBPS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		else if( EEREC_ACC == EEREC_T ) {
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			SSE_SUBPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
			SSE_MOVAPS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
		}
		else {
			SSE_MOVAPS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_SUBPS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		}
	}
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_SUBA_iq(VURegs *VU, uptr addr, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat3(addr);
		vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		vuFloat5( EEREC_ACC, EEREC_TEMP, _X_Y_Z_W);
	}

	if( _XYZW_SS ) {
		if( EEREC_ACC == EEREC_S ) {
			_vuFlipRegSS(VU, EEREC_ACC);
			SSE_SUBSS_M32_to_XMM(EEREC_ACC, addr);
			_vuFlipRegSS(VU, EEREC_ACC);
		}
		else {
			if( _X ) {
				SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
				SSE_SUBSS_M32_to_XMM(EEREC_ACC, addr);
			}
			else {
				_vuMoveSS(VU, EEREC_TEMP, EEREC_S);
				_vuFlipRegSS(VU, EEREC_ACC);
				SSE_SUBSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
				_vuFlipRegSS(VU, EEREC_ACC);
			}
		}
	}
	else {
		SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
		SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);

		if (_X_Y_Z_W != 0xf) {
			int t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) {
				SSE_MOVAPS_XMM_to_XMM(t1reg, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(t1reg, EEREC_TEMP);

				VU_MERGE_REGS(EEREC_ACC, t1reg);
				_freeXMMreg(t1reg);
			}
			else {
				// negate
				SSE_XORPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[4]);
				SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(EEREC_ACC, EEREC_TEMP);
			}
		}
		else {
			if( EEREC_ACC != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_SUBPS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
		}
	}
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_SUBA_xyzw(VURegs *VU, int xyzw, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		vuFloat5( EEREC_T, EEREC_TEMP, ( 1 << (3 - xyzw) ) );
		vuFloat5( EEREC_ACC, EEREC_TEMP, _X_Y_Z_W);
	}

	if( _X_Y_Z_W == 8 ) {
		if( xyzw == 0 ) {
			SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_SUBSS_XMM_to_XMM(EEREC_ACC, EEREC_T);
		}
		else {
			_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			SSE_MOVSS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_SUBSS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
		}
	}
	else {
		_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);

		if (_X_Y_Z_W != 0xf) {
			int t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) {
				SSE_MOVAPS_XMM_to_XMM(t1reg, EEREC_S);
				SSE_SUBPS_XMM_to_XMM(t1reg, EEREC_TEMP);

				VU_MERGE_REGS(EEREC_ACC, t1reg);
				_freeXMMreg(t1reg);
			}
			else {
				// negate
				SSE_XORPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[4]);
				SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(EEREC_ACC, EEREC_TEMP);
			}
		}
		else {
			if( EEREC_ACC != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(EEREC_ACC, EEREC_S);
			SSE_SUBPS_XMM_to_XMM(EEREC_ACC, EEREC_TEMP);
		}
	}
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_SUBAi(VURegs *VU, int info) { recVUMI_SUBA_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_SUBAq(VURegs *VU, int info) { recVUMI_SUBA_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_SUBAx(VURegs *VU, int info) { recVUMI_SUBA_xyzw(VU, 0, info); }
void recVUMI_SUBAy(VURegs *VU, int info) { recVUMI_SUBA_xyzw(VU, 1, info); }
void recVUMI_SUBAz(VURegs *VU, int info) { recVUMI_SUBA_xyzw(VU, 2, info); }
void recVUMI_SUBAw(VURegs *VU, int info) { recVUMI_SUBA_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// MUL
//------------------------------------------------------------------
void recVUMI_MUL_toD(VURegs *VU, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		//using vuFloat instead of vuFloat2 incase regd == EEREC_TEMP
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, EEREC_T, _X_Y_Z_W);
		vuFloat( info, regd, _X_Y_Z_W);
	}

	if (_X_Y_Z_W == 1 && (_Ft_ == 0 || _Fs_==0) ) { // W
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, _Ft_ ? EEREC_T : EEREC_S);
		VU_MERGE_REGS(regd, EEREC_TEMP);
	}
	else if( _Fd_ == _Fs_ && _Fs_ == _Ft_ && _XYZW_SS ) {
		_vuFlipRegSS(VU, EEREC_D);
		SSE_MULSS_XMM_to_XMM(EEREC_D, EEREC_D);
		_vuFlipRegSS(VU, EEREC_D);
	}
	else if( _X_Y_Z_W == 8 ) {
		if (regd == EEREC_S) SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
		else if (regd == EEREC_T) SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
		else {
			SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
			SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);

		VU_MERGE_REGS(regd, EEREC_TEMP);
	}
	else {
		if (regd == EEREC_S) SSE_MULPS_XMM_to_XMM(regd, EEREC_T);
		else if (regd == EEREC_T) SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
		else {
			SSE_MOVAPS_XMM_to_XMM(regd, EEREC_S);
			SSE_MULPS_XMM_to_XMM(regd, EEREC_T);
		}
	}
}

void recVUMI_MUL_iq_toD(VURegs *VU, uptr addr, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat3(addr);
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, regd, _X_Y_Z_W);
	}

	if( _XYZW_SS ) {
		if( regd == EEREC_TEMP ) {
			_vuFlipRegSS(VU, EEREC_S);
			SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
			SSE_MULSS_M32_to_XMM(regd, addr);
			_vuFlipRegSS(VU, EEREC_S);
		}
		else if( regd == EEREC_S ) {
			_vuFlipRegSS(VU, regd);
			SSE_MULSS_M32_to_XMM(regd, addr);
			_vuFlipRegSS(VU, regd);
		}
		else {
			if( _X ) {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				SSE_MULSS_M32_to_XMM(regd, addr);
			}
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
				SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(regd, EEREC_TEMP);
			}
		}
	}
	else {
		if( _X_Y_Z_W != 0xf || regd == EEREC_TEMP || regd == EEREC_S ) {
			SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
			SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
		}

		if (_X_Y_Z_W != 0xf) {
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			VU_MERGE_REGS(regd, EEREC_TEMP);
		}
		else {
			if( regd == EEREC_TEMP ) SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
			else if (regd == EEREC_S) SSE_MULPS_XMM_to_XMM(regd, EEREC_TEMP);
			else {
				SSE_MOVSS_M32_to_XMM(regd, addr); 
				SSE_SHUFPS_XMM_to_XMM(regd, regd, 0x00);
				SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
			}		
		}
	}
}

void recVUMI_MUL_xyzw_toD(VURegs *VU, int xyzw, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, regd, _X_Y_Z_W);
		vuFloat( info, EEREC_T, ( 1 << (3 - xyzw) ) );
	}
	// This is needed for alot of games
	vFloats1[_X_Y_Z_W]( EEREC_S, EEREC_S ); // Always clamp EEREC_S, regardless if CHECK_VU_OVERFLOW is set

	if( _Ft_ == 0 ) {
		if( xyzw < 3 ) {
			if (_X_Y_Z_W != 0xf) {	
				SSE_XORPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP);
				VU_MERGE_REGS(regd, EEREC_TEMP);
			}
			else SSE_XORPS_XMM_to_XMM(regd, regd);
		}
		else {
			assert(xyzw==3);
			if (_X_Y_Z_W != 0xf) {
				SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				VU_MERGE_REGS(regd, EEREC_TEMP);
			}
			else if( regd != EEREC_S ) SSE_MOVAPS_XMM_to_XMM(regd, EEREC_S);
		}
	}
	else if( _X_Y_Z_W == 8 ) {
		if( regd == EEREC_TEMP ) {
			_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
		}
		else {
			if( xyzw == 0 ) {
				if( regd == EEREC_T ) {
					SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
				} 
				else {
					SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
					SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				}
			}
			else {
				_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				SSE_MULSS_XMM_to_XMM(regd, EEREC_TEMP);
			}
		}
	}
	else {
		if( _X_Y_Z_W != 0xf || regd == EEREC_TEMP || regd == EEREC_S )
			_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);

		if (_X_Y_Z_W != 0xf) {	
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			VU_MERGE_REGS(regd, EEREC_TEMP);
		}
		else {
			if( regd == EEREC_TEMP ) SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
			else if (regd == EEREC_S) SSE_MULPS_XMM_to_XMM(regd, EEREC_TEMP);
			else {
				_unpackVF_xyzw(regd, EEREC_T, xyzw);
				SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
			}
		}
	}
}

void recVUMI_MUL(VURegs *VU, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MUL_toD(VU, EEREC_D, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MUL_iq(VURegs *VU, int addr, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MUL_iq_toD(VU, addr, EEREC_D, info);
	recUpdateFlags(VU, EEREC_D, info);
	// spacefisherman needs overflow checking on MULi.z
}

void recVUMI_MUL_xyzw(VURegs *VU, int xyzw, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MUL_xyzw_toD(VU, xyzw, EEREC_D, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MULi(VURegs *VU, int info) { recVUMI_MUL_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_MULq(VURegs *VU, int info) { recVUMI_MUL_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_MULx(VURegs *VU, int info) { recVUMI_MUL_xyzw(VU, 0, info); }
void recVUMI_MULy(VURegs *VU, int info) { recVUMI_MUL_xyzw(VU, 1, info); }
void recVUMI_MULz(VURegs *VU, int info) { recVUMI_MUL_xyzw(VU, 2, info); }
void recVUMI_MULw(VURegs *VU, int info) { recVUMI_MUL_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// MULA
//------------------------------------------------------------------
void recVUMI_MULA( VURegs *VU, int info )
{
	recVUMI_MUL_toD(VU, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MULA_iq(VURegs *VU, int addr, int info)
{	
	recVUMI_MUL_iq_toD(VU, addr, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MULA_xyzw(VURegs *VU, int xyzw, int info)
{
	recVUMI_MUL_xyzw_toD(VU, xyzw, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MULAi(VURegs *VU, int info) { recVUMI_MULA_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_MULAq(VURegs *VU, int info) { recVUMI_MULA_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_MULAx(VURegs *VU, int info) { recVUMI_MULA_xyzw(VU, 0, info); }
void recVUMI_MULAy(VURegs *VU, int info) { recVUMI_MULA_xyzw(VU, 1, info); }
void recVUMI_MULAz(VURegs *VU, int info) { recVUMI_MULA_xyzw(VU, 2, info); }
void recVUMI_MULAw(VURegs *VU, int info) { recVUMI_MULA_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// MADD
//------------------------------------------------------------------
void recVUMI_MADD_toD(VURegs *VU, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, EEREC_T, _X_Y_Z_W);
		vuFloat( info, regd, _X_Y_Z_W);
	}

	if( _X_Y_Z_W == 8 ) {
		if( regd == EEREC_ACC ) {
			SSE_MOVSS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			SSE_MULSS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_TEMP);
		}
		else if (regd == EEREC_T) {
			SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
		}
		else if (regd == EEREC_S) {
			SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
		}
		else {
			SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
			SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
		if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
		SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);

		VU_MERGE_REGS(regd, EEREC_TEMP);
	}
	else {
		if( regd == EEREC_ACC ) {
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
			SSE_ADDPS_XMM_to_XMM(regd, EEREC_TEMP);
		}
		else if (regd == EEREC_T) {
			SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
		}
		else if (regd == EEREC_S) {
			SSE_MULPS_XMM_to_XMM(regd, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
		}
		else {
			SSE_MOVAPS_XMM_to_XMM(regd, EEREC_S);
			SSE_MULPS_XMM_to_XMM(regd, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
		}
	}
}

void recVUMI_MADD_iq_toD(VURegs *VU, uptr addr, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat3(addr);
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, regd, _X_Y_Z_W);
	}

	if( _X_Y_Z_W == 8 ) {
		if( regd == EEREC_ACC ) {
			if( _Fs_ == 0 ) {
				// add addr to w
				SSE_SHUFPS_XMM_to_XMM(regd, regd, 0x27);
				SSE_ADDSS_M32_to_XMM(regd, addr);
				SSE_SHUFPS_XMM_to_XMM(regd, regd, 0x27);
			}
			else {
				assert( EEREC_TEMP < XMMREGS );
				SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_MULSS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, 8); }
				SSE_ADDSS_XMM_to_XMM(regd, EEREC_TEMP);
			}
		}
		else if( regd == EEREC_S ) {
			SSE_MULSS_M32_to_XMM(regd, addr);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
		}
		else {
			SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
			SSE_MULSS_M32_to_XMM(regd, addr);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
		}
	}
	else {
		if( _Fs_ == 0 ) {
			// add addr to w
			if( _W ) {
				SSE_SHUFPS_XMM_to_XMM(regd, regd, 0x27);
				SSE_ADDSS_M32_to_XMM(regd, addr);
				SSE_SHUFPS_XMM_to_XMM(regd, regd, 0x27);
			}

			return;
		}

		if( _X_Y_Z_W != 0xf || regd == EEREC_ACC || regd == EEREC_TEMP || regd == EEREC_S ) {
			SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr);
			SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
		}

		if (_X_Y_Z_W != 0xf) {
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);

			VU_MERGE_REGS(regd, EEREC_TEMP);
		}
		else {
			if( regd == EEREC_ACC ) {
				SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_TEMP);
			}
			else if( regd == EEREC_S ) {
				SSE_MULPS_XMM_to_XMM(regd, EEREC_TEMP);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
			}
			else if( regd == EEREC_TEMP ) {
				SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
			}
			else {
				SSE_MOVSS_M32_to_XMM(regd, addr);
				SSE_SHUFPS_XMM_to_XMM(regd, regd, 0x00);
				SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
			}
		}
	}
}

void recVUMI_MADD_xyzw_toD(VURegs *VU, int xyzw, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_T, ( 1 << (3 - xyzw) ) );
		vuFloat( info, EEREC_ACC, _X_Y_Z_W);
		vuFloat( info, regd, _X_Y_Z_W);
	}
	// This is needed for alot of games
	vFloats1[_X_Y_Z_W]( EEREC_S, EEREC_S ); // Always clamp EEREC_S, regardless if CHECK_VU_OVERFLOW is set
	
	if( _Ft_ == 0 ) {

		if( xyzw == 3 ) {
			// just add
			if( _X_Y_Z_W == 8 ) {
				if( regd == EEREC_S ) SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				else {
					if( regd != EEREC_ACC ) SSE_MOVSS_XMM_to_XMM(regd, EEREC_ACC);
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_S);
				}
			}
			else {
				if( _X_Y_Z_W != 0xf ) {
					SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
					SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);

					VU_MERGE_REGS(regd, EEREC_TEMP);
				}
				else {
					if( regd == EEREC_S ) SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
					else {
						if( regd != EEREC_ACC ) SSE_MOVAPS_XMM_to_XMM(regd, EEREC_ACC);
						SSE_ADDPS_XMM_to_XMM(regd, EEREC_S);
					}
				}
			}
		}
		else {
			// just move acc to regd
			if( _X_Y_Z_W != 0xf ) {
				SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);
				VU_MERGE_REGS(regd, EEREC_TEMP);
			}
			else if( regd != EEREC_ACC ) SSE_MOVAPS_XMM_to_XMM(regd, EEREC_ACC);
		}

		return;
	}

	if( _X_Y_Z_W == 8 ) {
		_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);

		if( regd == EEREC_ACC ) {
			SSE_MULSS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_TEMP);
		}
		else if( regd == EEREC_S ) {
			SSE_MULSS_XMM_to_XMM(regd, EEREC_TEMP);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
		}
		else if( regd == EEREC_TEMP ) {
			SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
		}
		else {
			SSE_MOVSS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_MULSS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, 8); }
			SSE_ADDSS_XMM_to_XMM(regd, EEREC_TEMP);
		}
	}
	else {
		if( _X_Y_Z_W != 0xf || regd == EEREC_ACC || regd == EEREC_TEMP || regd == EEREC_S ) {
			_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
		}

		if (_X_Y_Z_W != 0xf) {
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);

			VU_MERGE_REGS(regd, EEREC_TEMP);
		}
		else {
			if( regd == EEREC_ACC ) {
				SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_TEMP);
			}
			else if( regd == EEREC_S ) {
				SSE_MULPS_XMM_to_XMM(regd, EEREC_TEMP);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
			}
			else if( regd == EEREC_TEMP ) {
				SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
			}
			else {
				_unpackVF_xyzw(regd, EEREC_T, xyzw);
				SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
				if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
				SSE_ADDPS_XMM_to_XMM(regd, EEREC_ACC);
			}
		}
	}
}

void recVUMI_MADD(VURegs *VU, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MADD_toD(VU, EEREC_D, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MADD_iq(VURegs *VU, int addr, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MADD_iq_toD(VU, addr, EEREC_D, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MADD_xyzw(VURegs *VU, int xyzw, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MADD_xyzw_toD(VU, xyzw, EEREC_D, info);
	recUpdateFlags(VU, EEREC_D, info);
	// super bust-a-move arrows needs overflow clamping
}

void recVUMI_MADDi(VURegs *VU, int info) { recVUMI_MADD_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_MADDq(VURegs *VU, int info) { recVUMI_MADD_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_MADDx(VURegs *VU, int info) { recVUMI_MADD_xyzw(VU, 0, info); }
void recVUMI_MADDy(VURegs *VU, int info) { recVUMI_MADD_xyzw(VU, 1, info); }
void recVUMI_MADDz(VURegs *VU, int info) { recVUMI_MADD_xyzw(VU, 2, info); }
void recVUMI_MADDw(VURegs *VU, int info) { recVUMI_MADD_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// MADDA
//------------------------------------------------------------------
void recVUMI_MADDA( VURegs *VU, int info )
{
	recVUMI_MADD_toD(VU, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MADDAi( VURegs *VU , int info)
{
	recVUMI_MADD_iq_toD( VU, VU_VI_ADDR(REG_I, 1), EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MADDAq( VURegs *VU , int info)
{
	recVUMI_MADD_iq_toD( VU, VU_REGQ_ADDR, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MADDAx( VURegs *VU , int info)
{
	recVUMI_MADD_xyzw_toD(VU, 0, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MADDAy( VURegs *VU , int info)
{
	recVUMI_MADD_xyzw_toD(VU, 1, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MADDAz( VURegs *VU , int info)
{
	recVUMI_MADD_xyzw_toD(VU, 2, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MADDAw( VURegs *VU , int info)
{
	recVUMI_MADD_xyzw_toD(VU, 3, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MSUB
//------------------------------------------------------------------
void recVUMI_MSUB_toD(VURegs *VU, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, EEREC_T, _X_Y_Z_W);
		vuFloat( info, regd, _X_Y_Z_W);
	}

	if (_X_Y_Z_W != 0xf) {
		int t1reg = _vuGetTempXMMreg(info);

		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
		if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }

		if( t1reg >= 0 ) {
			SSE_MOVAPS_XMM_to_XMM(t1reg, EEREC_ACC);
			SSE_SUBPS_XMM_to_XMM(t1reg, EEREC_TEMP);

			VU_MERGE_REGS(regd, t1reg);
			_freeXMMreg(t1reg);
		}
		else {
			SSE_XORPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[4]);
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);
			VU_MERGE_REGS(regd, EEREC_TEMP);
		}
	}
	else {
		if( regd == EEREC_S ) {
			assert( regd != EEREC_ACC );
			SSE_MULPS_XMM_to_XMM(regd, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_XORPS_M128_to_XMM(regd, (uptr)&const_clip[4]);
		}
		else if( regd == EEREC_T ) {
			assert( regd != EEREC_ACC );
			SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_XORPS_M128_to_XMM(regd, (uptr)&const_clip[4]);
		}
		else if( regd == EEREC_TEMP ) {
			SSE_MOVAPS_XMM_to_XMM(regd, EEREC_S);
			SSE_MULPS_XMM_to_XMM(regd, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_XORPS_M128_to_XMM(regd, (uptr)&const_clip[4]);
		}
		else {
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if( regd != EEREC_ACC ) SSE_MOVAPS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_TEMP);	
		}
	}
}

void recVUMI_MSUB_temp_toD(VURegs *VU, int regd, int info)
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, EEREC_ACC, _X_Y_Z_W);
		vuFloat( info, regd, _X_Y_Z_W);
	}

	if (_X_Y_Z_W != 0xf) {
		int t1reg = _vuGetTempXMMreg(info);

		SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }

		if( t1reg >= 0 ) {
			SSE_MOVAPS_XMM_to_XMM(t1reg, EEREC_ACC);
			SSE_SUBPS_XMM_to_XMM(t1reg, EEREC_TEMP);

			if ( regd != EEREC_TEMP ) { VU_MERGE_REGS(regd, t1reg); }
			else SSE_MOVAPS_XMM_to_XMM(regd, t1reg);

			_freeXMMreg(t1reg);
		}
		else {
			SSE_XORPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[4]);
			SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);
			VU_MERGE_REGS(regd, EEREC_TEMP);
		}
	}
	else {
		if( regd == EEREC_ACC ) {
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_TEMP);	
		}
		else if( regd == EEREC_S ) {
			SSE_MULPS_XMM_to_XMM(regd, EEREC_TEMP);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_XORPS_M128_to_XMM(regd, (uptr)&const_clip[4]);
		}
		else if( regd == EEREC_TEMP ) {
			SSE_MULPS_XMM_to_XMM(regd, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, regd, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_XORPS_M128_to_XMM(regd, (uptr)&const_clip[4]);
		}
		else {
			if( regd != EEREC_ACC ) SSE_MOVAPS_XMM_to_XMM(regd, EEREC_ACC);
			SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat( info, EEREC_TEMP, _X_Y_Z_W); }
			SSE_SUBPS_XMM_to_XMM(regd, EEREC_TEMP);	
		}
	}
}

void recVUMI_MSUB_iq_toD(VURegs *VU, int regd, int addr, int info)
{
	SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
	SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
	recVUMI_MSUB_temp_toD(VU, regd, info);
}

void recVUMI_MSUB_xyzw_toD(VURegs *VU, int regd, int xyzw, int info)
{
	_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
	recVUMI_MSUB_temp_toD(VU, regd, info);
}

void recVUMI_MSUB(VURegs *VU, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MSUB_toD(VU, EEREC_D, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MSUB_iq(VURegs *VU, int addr, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MSUB_iq_toD(VU, EEREC_D, addr, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MSUBi(VURegs *VU, int info) { recVUMI_MSUB_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_MSUBq(VURegs *VU, int info) { recVUMI_MSUB_iq(VU, VU_REGQ_ADDR, info); }
void recVUMI_MSUBx(VURegs *VU, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MSUB_xyzw_toD(VU, EEREC_D, 0, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MSUBy(VURegs *VU, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MSUB_xyzw_toD(VU, EEREC_D, 1, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MSUBz(VURegs *VU, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MSUB_xyzw_toD(VU, EEREC_D, 2, info);
	recUpdateFlags(VU, EEREC_D, info);
}

void recVUMI_MSUBw(VURegs *VU, int info)
{
	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	recVUMI_MSUB_xyzw_toD(VU, EEREC_D, 3, info);
	recUpdateFlags(VU, EEREC_D, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MSUBA
//------------------------------------------------------------------
void recVUMI_MSUBA( VURegs *VU, int info )
{
	recVUMI_MSUB_toD(VU, EEREC_ACC, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MSUBAi( VURegs *VU, int info )
{
	recVUMI_MSUB_iq_toD( VU, EEREC_ACC, VU_VI_ADDR(REG_I, 1), info );
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MSUBAq( VURegs *VU, int info )
{
	recVUMI_MSUB_iq_toD( VU, EEREC_ACC, VU_REGQ_ADDR, info );
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MSUBAx( VURegs *VU, int info )
{
	recVUMI_MSUB_xyzw_toD(VU, EEREC_ACC, 0, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MSUBAy( VURegs *VU, int info )
{
	recVUMI_MSUB_xyzw_toD(VU, EEREC_ACC, 1, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MSUBAz( VURegs *VU, int info )
{
	recVUMI_MSUB_xyzw_toD(VU, EEREC_ACC, 2, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}

void recVUMI_MSUBAw( VURegs *VU, int info )
{
	recVUMI_MSUB_xyzw_toD(VU, EEREC_ACC, 3, info);
	recUpdateFlags(VU, EEREC_ACC, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MAX
//------------------------------------------------------------------
void recVUMI_MAX(VURegs *VU, int info)
{	
	if ( _Fd_ == 0 ) return;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W);
		vuFloat5( EEREC_T, EEREC_TEMP, _X_Y_Z_W);
	}

	if( _X_Y_Z_W == 8 ) {
		if (EEREC_D == EEREC_S) SSE_MAXSS_XMM_to_XMM(EEREC_D, EEREC_T);
		else if (EEREC_D == EEREC_T) SSE_MAXSS_XMM_to_XMM(EEREC_D, EEREC_S);
		else {
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MAXSS_XMM_to_XMM(EEREC_D, EEREC_T);
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_MAXPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);

		VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
	}
	else {
		if( EEREC_D == EEREC_S ) SSE_MAXPS_XMM_to_XMM(EEREC_D, EEREC_T);
		else if( EEREC_D == EEREC_T ) SSE_MAXPS_XMM_to_XMM(EEREC_D, EEREC_S);
		else {
			SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MAXPS_XMM_to_XMM(EEREC_D, EEREC_T);
		}
	}
}

void recVUMI_MAX_iq(VURegs *VU, uptr addr, int info)
{	
	if ( _Fd_ == 0 ) return;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat3(addr);
	}

	if( _XYZW_SS ) {
		if( EEREC_D == EEREC_TEMP ) {
			_vuFlipRegSS(VU, EEREC_S);
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MAXSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_S);

			// have to flip over EEREC_D if computing flags!
			//if( (info & PROCESS_VU_UPDATEFLAGS) )
				_vuFlipRegSS(VU, EEREC_D);
		}
		else if( EEREC_D == EEREC_S ) {
			_vuFlipRegSS(VU, EEREC_D);
			SSE_MAXSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_D);
		}
		else {
			if( _X ) {
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_MAXSS_M32_to_XMM(EEREC_D, addr);
			}
			else {
				_vuMoveSS(VU, EEREC_TEMP, EEREC_S);
				_vuFlipRegSS(VU, EEREC_D);
				SSE_MAXSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
				_vuFlipRegSS(VU, EEREC_D);
			}
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
		SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
		SSE_MAXPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
	}
	else {
		if(EEREC_D == EEREC_S) {
			SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr);
			SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
			SSE_MAXPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
		}
		else {
			SSE_MOVSS_M32_to_XMM(EEREC_D, addr);
			SSE_SHUFPS_XMM_to_XMM(EEREC_D, EEREC_D, 0x00);
			SSE_MAXPS_XMM_to_XMM(EEREC_D, EEREC_S);
		}
	}
}

void recVUMI_MAX_xyzw(VURegs *VU, int xyzw, int info)
{	
	if ( _Fd_ == 0 ) return;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, EEREC_T, ( 1 << (3 - xyzw) ) );
	}

	if( _X_Y_Z_W == 8 && (EEREC_D != EEREC_TEMP)) {
		if( _Fs_ == 0 && _Ft_ == 0 ) {
			if( xyzw < 3 ) {
				SSE_XORPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP);
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			}
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_TEMP, (uptr)s_fones);
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			}
		}
		else {
			if( xyzw == 0 ) {
				if( EEREC_D == EEREC_S ) SSE_MAXSS_XMM_to_XMM(EEREC_D, EEREC_T);
				else if( EEREC_D == EEREC_T ) SSE_MAXSS_XMM_to_XMM(EEREC_D, EEREC_S);
				else {
					SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
					SSE_MAXSS_XMM_to_XMM(EEREC_D, EEREC_T);
				}
			}
			else {
				_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
				if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_MAXSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			}
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		if( _Fs_ == 0 && _Ft_ == 0 ) {
			if( xyzw < 3 ) {
				if( _X_Y_Z_W & 1 ) SSE_MOVAPS_M128_to_XMM(EEREC_TEMP, (uptr)&VU->VF[0].UL[0]); // w included, so insert the whole reg
				else SSE_XORPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP); // w not included, can zero out
			}
			else SSE_MOVAPS_M128_to_XMM(EEREC_TEMP, (uptr)s_fones);
		}
		else {
			_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			SSE_MAXPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		}
		VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
	}
	else {
		if( _Fs_ == 0 && _Ft_ == 0 ) {
			if( xyzw < 3 ) SSE_XORPS_XMM_to_XMM(EEREC_D, EEREC_D);
			else SSE_MOVAPS_M128_to_XMM(EEREC_D, (uptr)s_fones);
		}
		else {
			if (EEREC_D == EEREC_S) {
				_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
				SSE_MAXPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
			}
			else {
				_unpackVF_xyzw(EEREC_D, EEREC_T, xyzw);
				SSE_MAXPS_XMM_to_XMM(EEREC_D, EEREC_S);
			}
		}
	}
}

void recVUMI_MAXi(VURegs *VU, int info) { recVUMI_MAX_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_MAXx(VURegs *VU, int info) { recVUMI_MAX_xyzw(VU, 0, info); }
void recVUMI_MAXy(VURegs *VU, int info) { recVUMI_MAX_xyzw(VU, 1, info); }
void recVUMI_MAXz(VURegs *VU, int info) { recVUMI_MAX_xyzw(VU, 2, info); }
void recVUMI_MAXw(VURegs *VU, int info) { recVUMI_MAX_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// MINI
//------------------------------------------------------------------
void recVUMI_MINI(VURegs *VU, int info)
{
	if ( _Fd_ == 0 ) return;

	if( _X_Y_Z_W == 8 ) {
		if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat5( EEREC_S, EEREC_TEMP, 8); vuFloat5( EEREC_T, EEREC_TEMP, 8); }
		if (EEREC_D == EEREC_S) SSE_MINSS_XMM_to_XMM(EEREC_D, EEREC_T);
		else if (EEREC_D == EEREC_T) SSE_MINSS_XMM_to_XMM(EEREC_D, EEREC_S);
		else {
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MINSS_XMM_to_XMM(EEREC_D, EEREC_T);
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat5( EEREC_S, EEREC_TEMP, _X_Y_Z_W); vuFloat5( EEREC_T, EEREC_TEMP, _X_Y_Z_W); }
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_MINPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);

		VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
	}
	else {
		if (CHECK_VU_EXTRA_OVERFLOW) { vuFloat5( EEREC_S, EEREC_TEMP, 0xf); vuFloat5( EEREC_T, EEREC_TEMP, 0xf); }
		if( EEREC_D == EEREC_S ) {
			//ClampUnordered(EEREC_T, EEREC_TEMP, 0); // need for GT4 vu0rec
			SSE_MINPS_XMM_to_XMM(EEREC_D, EEREC_T);
		}
		else if( EEREC_D == EEREC_T ) {
			//ClampUnordered(EEREC_S, EEREC_TEMP, 0); // need for GT4 vu0rec
			SSE_MINPS_XMM_to_XMM(EEREC_D, EEREC_S);
		}
		else {
			SSE_MOVAPS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MINPS_XMM_to_XMM(EEREC_D, EEREC_T);
		}
	}
}

void recVUMI_MINI_iq(VURegs *VU, uptr addr, int info)
{
	if ( _Fd_ == 0 ) return;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat3(addr);
	}

	if( _XYZW_SS ) {
		if( EEREC_D == EEREC_TEMP ) {
			_vuFlipRegSS(VU, EEREC_S);
			SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MINSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_S);

			// have to flip over EEREC_D if computing flags!
			//if( (info & PROCESS_VU_UPDATEFLAGS) )
				_vuFlipRegSS(VU, EEREC_D);
		}
		else if( EEREC_D == EEREC_S ) {
			_vuFlipRegSS(VU, EEREC_D);
			SSE_MINSS_M32_to_XMM(EEREC_D, addr);
			_vuFlipRegSS(VU, EEREC_D);
		}
		else {
			if( _X ) {
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_MINSS_M32_to_XMM(EEREC_D, addr);
			}
			else {
				_vuMoveSS(VU, EEREC_TEMP, EEREC_S);
				_vuFlipRegSS(VU, EEREC_D);
				SSE_MINSS_M32_to_XMM(EEREC_TEMP, addr);
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
				_vuFlipRegSS(VU, EEREC_D);
			}
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr); 
		SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
		SSE_MINPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
	}
	else {
		if(EEREC_D == EEREC_S) {
			SSE_MOVSS_M32_to_XMM(EEREC_TEMP, addr);
			SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0x00);
			SSE_MINPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
		}
		else {
			SSE_MOVSS_M32_to_XMM(EEREC_D, addr);
			SSE_SHUFPS_XMM_to_XMM(EEREC_D, EEREC_D, 0x00);
			SSE_MINPS_XMM_to_XMM(EEREC_D, EEREC_S);
		}
	}
}

void recVUMI_MINI_xyzw(VURegs *VU, int xyzw, int info)
{
	if ( _Fd_ == 0 ) return;
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat( info, EEREC_S, _X_Y_Z_W);
		vuFloat( info, EEREC_T, ( 1 << (3 - xyzw) ) );
	}

	if( _X_Y_Z_W == 8 && (EEREC_D != EEREC_TEMP)) {
		if( xyzw == 0 ) {
			if( EEREC_D == EEREC_S ) SSE_MINSS_XMM_to_XMM(EEREC_D, EEREC_T);
			else if( EEREC_D == EEREC_T ) SSE_MINSS_XMM_to_XMM(EEREC_D, EEREC_S);
			else {
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				SSE_MINSS_XMM_to_XMM(EEREC_D, EEREC_T);
			}
		}
		else {
			_unpackVFSS_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MINSS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
		}
	}
	else if (_X_Y_Z_W != 0xf) {
		_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
		SSE_MINPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		VU_MERGE_REGS(EEREC_D, EEREC_TEMP);
	}
	else {
		if (EEREC_D == EEREC_S) {
			_unpackVF_xyzw(EEREC_TEMP, EEREC_T, xyzw);
			SSE_MINPS_XMM_to_XMM(EEREC_D, EEREC_TEMP);
		}
		else {
			_unpackVF_xyzw(EEREC_D, EEREC_T, xyzw);
			SSE_MINPS_XMM_to_XMM(EEREC_D, EEREC_S);
		}
	}
}

void recVUMI_MINIi(VURegs *VU, int info) { recVUMI_MINI_iq(VU, VU_VI_ADDR(REG_I, 1), info); }
void recVUMI_MINIx(VURegs *VU, int info) { recVUMI_MINI_xyzw(VU, 0, info); }
void recVUMI_MINIy(VURegs *VU, int info) { recVUMI_MINI_xyzw(VU, 1, info); }
void recVUMI_MINIz(VURegs *VU, int info) { recVUMI_MINI_xyzw(VU, 2, info); }
void recVUMI_MINIw(VURegs *VU, int info) { recVUMI_MINI_xyzw(VU, 3, info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// OPMULA
//------------------------------------------------------------------
void recVUMI_OPMULA( VURegs *VU, int info )
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5( EEREC_S, EEREC_TEMP, 0xE);
		vuFloat5( EEREC_T, EEREC_TEMP, 0xE);
	}

	SSE_MOVAPS_XMM_to_XMM( EEREC_TEMP, EEREC_S );
	SSE_SHUFPS_XMM_to_XMM( EEREC_T, EEREC_T, 0xD2 );		// EEREC_T = WYXZ
	SSE_SHUFPS_XMM_to_XMM( EEREC_TEMP, EEREC_TEMP, 0xC9 );	// EEREC_TEMP = WXZY
	SSE_MULPS_XMM_to_XMM( EEREC_TEMP, EEREC_T );

	VU_MERGE_REGS_CUSTOM(EEREC_ACC, EEREC_TEMP, 14);

	// revert EEREC_T
	if( EEREC_T != EEREC_ACC )
		SSE_SHUFPS_XMM_to_XMM(EEREC_T, EEREC_T, 0xC9);

	recUpdateFlags(VU, EEREC_ACC, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// OPMSUB
//------------------------------------------------------------------
void recVUMI_OPMSUB( VURegs *VU, int info )
{
	if (CHECK_VU_EXTRA_OVERFLOW) {
		vuFloat5( EEREC_S, EEREC_TEMP, 0xE);
		vuFloat5( EEREC_T, EEREC_TEMP, 0xE);
	}

	if( !_Fd_ ) info |= PROCESS_EE_SET_D(EEREC_TEMP);
	SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
	SSE_SHUFPS_XMM_to_XMM(EEREC_T, EEREC_T, 0xD2);			// EEREC_T = WYXZ
	SSE_SHUFPS_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP, 0xC9);	// EEREC_TEMP = WXZY
	SSE_MULPS_XMM_to_XMM(EEREC_TEMP, EEREC_T);

	// negate and add
	SSE_XORPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[4]);
	SSE_ADDPS_XMM_to_XMM(EEREC_TEMP, EEREC_ACC);
	VU_MERGE_REGS_CUSTOM(EEREC_D, EEREC_TEMP, 14);
	
	// revert EEREC_T
	if( EEREC_T != EEREC_D ) SSE_SHUFPS_XMM_to_XMM(EEREC_T, EEREC_T, 0xC9);

	recUpdateFlags(VU, EEREC_D, info);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// NOP
//------------------------------------------------------------------
void recVUMI_NOP( VURegs *VU, int info ) 
{
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// recVUMI_FTOI_Saturate() - Saturates result from FTOI Instructions
//------------------------------------------------------------------
static const PCSX2_ALIGNED16(int rec_const_0x8000000[4]) = { 0x80000000, 0x80000000, 0x80000000, 0x80000000 };

void recVUMI_FTOI_Saturate(int rec_s, int rec_t, int rec_tmp1, int rec_tmp2)
{
	//Duplicate the xor'd sign bit to the whole value
	//FFFF FFFF for positive,  0 for negative
	SSE_MOVAPS_XMM_to_XMM(rec_tmp1, rec_s);
	SSE2_PXOR_M128_to_XMM(rec_tmp1, (uptr)&const_clip[4]);
	SSE2_PSRAD_I8_to_XMM(rec_tmp1, 31);

	//Create mask: 0 where !=8000 0000
	SSE_MOVAPS_XMM_to_XMM(rec_tmp2, rec_t);
	SSE2_PCMPEQD_M128_to_XMM(rec_tmp2, (uptr)&const_clip[4]);
	
	//AND the mask w/ the edit values
	SSE_ANDPS_XMM_to_XMM(rec_tmp1, rec_tmp2);

	//if v==8000 0000 && positive -> 8000 0000 + FFFF FFFF -> 7FFF FFFF
	//if v==8000 0000 && negative -> 8000 0000 + 0 -> 8000 0000
	//if v!=8000 0000 -> v+0 (masked from the and)

	//Add the values as needed
	SSE2_PADDD_XMM_to_XMM(rec_t, rec_tmp1);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FTOI 0/4/12/15
//------------------------------------------------------------------
static PCSX2_ALIGNED16(float FTIO_Temp1[4]) = { 0x00000000, 0x00000000, 0x00000000, 0x00000000 };
static PCSX2_ALIGNED16(float FTIO_Temp2[4]) = { 0x00000000, 0x00000000, 0x00000000, 0x00000000 };
void recVUMI_FTOI0(VURegs *VU, int info)
{	
	int t1reg, t2reg; // Temp XMM regs

	if ( _Ft_ == 0 ) return; 

	//SysPrintf("recVUMI_FTOI0()\n");

	if (_X_Y_Z_W != 0xf) {
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		vuFloat( info, EEREC_TEMP, 0xf ); // Clamp Infs and NaNs to pos/neg fmax (NaNs always to positive fmax)
		SSE2_CVTTPS2DQ_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP);
		
		t1reg = _vuGetTempXMMreg(info);

		if( t1reg >= 0 ) { // If theres a temp XMM reg available
			for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP) || (t2reg == t1reg) ); t2reg++)
				; // Find unused reg (For second temp reg)
			SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t2reg); // Backup XMM reg
			
			recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
			
			SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp1); // Restore XMM reg
			_freeXMMreg(t1reg); // Free temp reg
		}
		else { // No temp reg available
			for (t1reg = 0; ( (t1reg == EEREC_S) || (t1reg == EEREC_T) || (t1reg == EEREC_TEMP) ); t1reg++)
				; // Find unused reg (For first temp reg)
			SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t1reg); // Backup t1reg XMM reg

			for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP) || (t2reg == t1reg) ); t2reg++)
				; // Find unused reg (For second temp reg)
			SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp2, t2reg); // Backup t2reg XMM reg
			
			recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
			
			SSE_MOVAPS_M128_to_XMM(t1reg, (uptr)FTIO_Temp1); // Restore t1reg XMM reg
			SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp2); // Restore t2reg XMM reg
		}

		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
	else {
		if (EEREC_T != EEREC_S) {
			SSE_MOVAPS_XMM_to_XMM(EEREC_T, EEREC_S);
			vuFloat( info, EEREC_T, 0xf ); // Clamp Infs and NaNs to pos/neg fmax (NaNs always to positive fmax)
			SSE2_CVTTPS2DQ_XMM_to_XMM(EEREC_T, EEREC_T); 

			t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) { // If theres a temp XMM reg available
				recVUMI_FTOI_Saturate(EEREC_S, EEREC_T, EEREC_TEMP, t1reg); // Saturate if Float->Int conversion returned illegal result
				_freeXMMreg(t1reg); // Free temp reg
			}
			else { // No temp reg available
				for (t1reg = 0; ( (t1reg == EEREC_S) || (t1reg == EEREC_T) || (t1reg == EEREC_TEMP) ); t1reg++)
					; // Find unused reg
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t1reg); // Backup t1reg XMM reg

				recVUMI_FTOI_Saturate(EEREC_S, EEREC_T, EEREC_TEMP, t1reg); // Saturate if Float->Int conversion returned illegal result
				
				SSE_MOVAPS_M128_to_XMM(t1reg, (uptr)FTIO_Temp1); // Restore t1reg XMM reg
			}
		}
		else {
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			vuFloat( info, EEREC_TEMP, 0xf ); // Clamp Infs and NaNs to pos/neg fmax (NaNs always to positive fmax)
			SSE2_CVTTPS2DQ_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP);
			
			t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) { // If theres a temp XMM reg available
				for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP) || (t2reg == t1reg)); t2reg++)
					; // Find unused reg (For second temp reg)
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t2reg); // Backup XMM reg
				
				recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
				
				SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp1); // Restore XMM reg
				_freeXMMreg(t1reg); // Free temp reg
			}
			else { // No temp reg available
				for (t1reg = 0; ( (t1reg == EEREC_S) || (t1reg == EEREC_T) || (t1reg == EEREC_TEMP) ); t1reg++)
					; // Find unused reg (For first temp reg)
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t1reg); // Backup t1reg XMM reg

				for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP) || (t2reg == t1reg) ); t2reg++)
					; // Find unused reg (For second temp reg)
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp2, t2reg); // Backup t2reg XMM reg
				
				recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
				
				SSE_MOVAPS_M128_to_XMM(t1reg, (uptr)FTIO_Temp1); // Restore t1reg XMM reg
				SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp2); // Restore t2reg XMM reg
			}

			SSE_MOVAPS_XMM_to_XMM(EEREC_T, EEREC_TEMP);
		}
	}
}

void recVUMI_FTOIX(VURegs *VU, int addr, int info)
{
	int t1reg, t2reg; // Temp XMM regs

	if ( _Ft_ == 0 ) return; 

	//SysPrintf("recVUMI_FTOIX()\n");

	if (_X_Y_Z_W != 0xf) {
		SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_MULPS_M128_to_XMM(EEREC_TEMP, addr);
		vuFloat( info, EEREC_TEMP, 0xf ); // Clamp Infs and NaNs to pos/neg fmax (NaNs always to positive fmax)
		SSE2_CVTTPS2DQ_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP);

		t1reg = _vuGetTempXMMreg(info);

		if( t1reg >= 0 ) { // If theres a temp XMM reg available
			for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP)  || (t2reg == t1reg)); t2reg++)
				; // Find unused reg (For second temp reg)
			SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t2reg); // Backup XMM reg
			
			recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
			
			SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp1); // Restore XMM reg
			_freeXMMreg(t1reg); // Free temp reg
		}
		else { // No temp reg available
			for (t1reg = 0; ( (t1reg == EEREC_S) || (t1reg == EEREC_T) || (t1reg == EEREC_TEMP) ); t1reg++)
				; // Find unused reg (For first temp reg)
			SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t1reg); // Backup t1reg XMM reg

			for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP) || (t2reg == t1reg) ); t2reg++)
				; // Find unused reg (For second temp reg)
			SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp2, t2reg); // Backup t2reg XMM reg
			
			recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
			
			SSE_MOVAPS_M128_to_XMM(t1reg, (uptr)FTIO_Temp1); // Restore t1reg XMM reg
			SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp2); // Restore t2reg XMM reg
		}

		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
	}
	else {
		if (EEREC_T != EEREC_S) {
			SSE_MOVAPS_XMM_to_XMM(EEREC_T, EEREC_S);
			SSE_MULPS_M128_to_XMM(EEREC_T, addr);
			vuFloat( info, EEREC_T, 0xf ); // Clamp Infs and NaNs to pos/neg fmax (NaNs always to positive fmax)
			SSE2_CVTTPS2DQ_XMM_to_XMM(EEREC_T, EEREC_T);

			t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) { // If theres a temp XMM reg available
				recVUMI_FTOI_Saturate(EEREC_S, EEREC_T, EEREC_TEMP, t1reg); // Saturate if Float->Int conversion returned illegal result
				_freeXMMreg(t1reg); // Free temp reg
			}
			else { // No temp reg available
				for (t1reg = 0; ( (t1reg == EEREC_S) || (t1reg == EEREC_T) || (t1reg == EEREC_TEMP) ); t1reg++)
					; // Find unused reg
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t1reg); // Backup t1reg XMM reg

				recVUMI_FTOI_Saturate(EEREC_S, EEREC_T, EEREC_TEMP, t1reg); // Saturate if Float->Int conversion returned illegal result
				
				SSE_MOVAPS_M128_to_XMM(t1reg, (uptr)FTIO_Temp1); // Restore t1reg XMM reg
			}
		}
		else {
			SSE_MOVAPS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
			SSE_MULPS_M128_to_XMM(EEREC_TEMP, addr);
			vuFloat( info, EEREC_TEMP, 0xf ); // Clamp Infs and NaNs to pos/neg fmax (NaNs always to positive fmax)
			SSE2_CVTTPS2DQ_XMM_to_XMM(EEREC_TEMP, EEREC_TEMP);
			
			t1reg = _vuGetTempXMMreg(info);

			if( t1reg >= 0 ) { // If theres a temp XMM reg available
				for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP) || (t2reg == t1reg)); t2reg++)
					; // Find unused reg (For second temp reg)
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t2reg); // Backup XMM reg
				
				recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
				
				SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp1); // Restore XMM reg
				_freeXMMreg(t1reg); // Free temp reg
			}
			else { // No temp reg available
				for (t1reg = 0; ( (t1reg == EEREC_S) || (t1reg == EEREC_T) || (t1reg == EEREC_TEMP) ); t1reg++)
					; // Find unused reg (For first temp reg)
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp1, t1reg); // Backup t1reg XMM reg

				for (t2reg = 0; ( (t2reg == EEREC_S) || (t2reg == EEREC_T) || (t2reg == EEREC_TEMP) || (t2reg == t1reg) ); t2reg++)
					; // Find unused reg (For second temp reg)
				SSE_MOVAPS_XMM_to_M128((uptr)FTIO_Temp2, t2reg); // Backup t2reg XMM reg
				
				recVUMI_FTOI_Saturate(EEREC_S, EEREC_TEMP, t1reg, t2reg); // Saturate if Float->Int conversion returned illegal result
				
				SSE_MOVAPS_M128_to_XMM(t1reg, (uptr)FTIO_Temp1); // Restore t1reg XMM reg
				SSE_MOVAPS_M128_to_XMM(t2reg, (uptr)FTIO_Temp2); // Restore t2reg XMM reg
			}

			SSE_MOVAPS_XMM_to_XMM(EEREC_T, EEREC_TEMP);
		}
	}
}

void recVUMI_FTOI4( VURegs *VU, int info ) { recVUMI_FTOIX(VU, (uptr)&recMult_float_to_int4[0], info); }
void recVUMI_FTOI12( VURegs *VU, int info ) { recVUMI_FTOIX(VU, (uptr)&recMult_float_to_int12[0], info); }
void recVUMI_FTOI15( VURegs *VU, int info ) { recVUMI_FTOIX(VU, (uptr)&recMult_float_to_int15[0], info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// ITOF 0/4/12/15
//------------------------------------------------------------------
void recVUMI_ITOF0( VURegs *VU, int info )
{
	if ( _Ft_ == 0 ) return;

	if (_X_Y_Z_W != 0xf) {
		SSE2_CVTDQ2PS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		vuFloat( info, EEREC_TEMP, 15); // Clamp infinities
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
		xmmregs[EEREC_T].mode |= MODE_WRITE;
	}
	else {
		SSE2_CVTDQ2PS_XMM_to_XMM(EEREC_T, EEREC_S);
		vuFloat2(EEREC_T, EEREC_TEMP, 15); // Clamp infinities
	}
}

void recVUMI_ITOFX(VURegs *VU, int addr, int info)
{
	if ( _Ft_ == 0 ) return; 

	if (_X_Y_Z_W != 0xf) {
		SSE2_CVTDQ2PS_XMM_to_XMM(EEREC_TEMP, EEREC_S);
		SSE_MULPS_M128_to_XMM(EEREC_TEMP, addr);
		vuFloat( info, EEREC_TEMP, 15); // Clamp infinities
		VU_MERGE_REGS(EEREC_T, EEREC_TEMP);
		xmmregs[EEREC_T].mode |= MODE_WRITE;
	} 
	else {
		SSE2_CVTDQ2PS_XMM_to_XMM(EEREC_T, EEREC_S);
		SSE_MULPS_M128_to_XMM(EEREC_T, addr);
		vuFloat2(EEREC_T, EEREC_TEMP, 15); // Clamp infinities
	}
}

void recVUMI_ITOF4( VURegs *VU, int info ) { recVUMI_ITOFX(VU, (uptr)&recMult_int_to_float4[0], info); }
void recVUMI_ITOF12( VURegs *VU, int info ) { recVUMI_ITOFX(VU, (uptr)&recMult_int_to_float12[0], info); }
void recVUMI_ITOF15( VURegs *VU, int info ) { recVUMI_ITOFX(VU, (uptr)&recMult_int_to_float15[0], info); }
//------------------------------------------------------------------


//------------------------------------------------------------------
// CLIP
//------------------------------------------------------------------
void recVUMI_CLIP(VURegs *VU, int info)
{
	int t1reg = EEREC_D;
	int t2reg = EEREC_ACC;
	int x86temp1, x86temp2;

	u32 clipaddr = VU_VI_ADDR(REG_CLIP_FLAG, 0);
	u32 prevclipaddr = VU_VI_ADDR(REG_CLIP_FLAG, 2);

	if( clipaddr == 0 ) { // battle star has a clip right before fcset
		SysPrintf("skipping vu clip\n");
		return;
	}
	assert( clipaddr != 0 );
	assert( t1reg != t2reg && t1reg != EEREC_TEMP && t2reg != EEREC_TEMP );
	
	x86temp1 = ALLOCTEMPX86(MODE_8BITREG);
	x86temp2 = ALLOCTEMPX86(MODE_8BITREG);

	//if ( (x86temp1 == 0) || (x86temp2 == 0) ) SysPrintf("VU CLIP Allocation Error: EAX being allocated! \n");

	_freeXMMreg(t1reg); // These should have been freed at allocation in eeVURecompileCode()
	_freeXMMreg(t2reg); // but if they've been used since then, then free them. (just doing this incase :p (cottonvibes))

	if( _Ft_ == 0 ) {
		SSE_MOVAPS_M128_to_XMM(EEREC_TEMP, (uptr)&s_fones[0]); // all 1s
		SSE_MOVAPS_M128_to_XMM(t1reg, (uptr)&s_fones[4]);
	}
	else {
		_unpackVF_xyzw(EEREC_TEMP, EEREC_T, 3);
		SSE_ANDPS_M128_to_XMM(EEREC_TEMP, (uptr)&const_clip[0]);
		SSE_MOVAPS_XMM_to_XMM(t1reg, EEREC_TEMP);
		SSE_ORPS_M128_to_XMM(t1reg, (uptr)&const_clip[4]);
	}

	MOV32MtoR(EAX, prevclipaddr);

	SSE_CMPNLEPS_XMM_to_XMM(t1reg, EEREC_S);  //-w, -z, -y, -x
	SSE_CMPLTPS_XMM_to_XMM(EEREC_TEMP, EEREC_S); //+w, +z, +y, +x

	SHL32ItoR(EAX, 6);

	SSE_MOVAPS_XMM_to_XMM(t2reg, EEREC_TEMP); //t2 = +w, +z, +y, +x
	SSE_UNPCKLPS_XMM_to_XMM(EEREC_TEMP, t1reg); //EEREC_TEMP = -y,+y,-x,+x
	SSE_UNPCKHPS_XMM_to_XMM(t2reg, t1reg); //t2reg = -w,+w,-z,+z
	SSE_MOVMSKPS_XMM_to_R32(x86temp2, EEREC_TEMP); // -y,+y,-x,+x
	SSE_MOVMSKPS_XMM_to_R32(x86temp1, t2reg); // -w,+w,-z,+z

	AND8ItoR(x86temp1, 0x3);
	SHL8ItoR(x86temp1, 4);
	OR8RtoR(EAX, x86temp1);
	AND8ItoR(x86temp2, 0xf);
	OR8RtoR(EAX, x86temp2);
	AND32ItoR(EAX, 0xffffff);

	MOV32RtoM(clipaddr, EAX);

	// God of War needs this additional move, but it breaks Rockstar games; ideally this hack shouldn't be needed, i think its a clipflag allocation bug in iVUzerorec.cpp
	if ( ( CHECK_VUCLIPHACK ) || ( !(info & (PROCESS_VU_SUPER|PROCESS_VU_COP2)) ) ) 
		MOV32RtoM((uptr)&VU->VI[REG_CLIP_FLAG], EAX);

	_freeX86reg(x86temp1);
	_freeX86reg(x86temp2);
}