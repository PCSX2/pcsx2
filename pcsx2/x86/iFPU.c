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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

// stop compiling if NORECBUILD build (only for Visual Studio)
#if !(defined(_MSC_VER) && defined(PCSX2_NORECBUILD))

#include "Common.h"
#include "InterTables.h"
#include "ix86/ix86.h"
#include "iR5900.h"
#include "iFPU.h"
#include "stdio.h" //Linux needs this?
#include "stdlib.h" //Linux needs this?
//------------------------------------------------------------------


//------------------------------------------------------------------
// Helper Macros
//------------------------------------------------------------------
#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

// FCR31 Flags
#define FPUflagC	0X00800000
#define FPUflagI	0X00020000
#define FPUflagD	0X00010000
#define FPUflagO	0X00008000
#define FPUflagU	0X00004000
#define FPUflagSI	0X00000040
#define FPUflagSD	0X00000020
#define FPUflagSO	0X00000010
#define FPUflagSU	0X00000008

extern PCSX2_ALIGNED16_DECL(u32 g_minvals[4]);
extern PCSX2_ALIGNED16_DECL(u32 g_maxvals[4]);

static u32 PCSX2_ALIGNED16(s_neg[4]) = { 0x80000000, 0xffffffff, 0xffffffff, 0xffffffff };
static u32 PCSX2_ALIGNED16(s_pos[4]) = { 0x7fffffff, 0xffffffff, 0xffffffff, 0xffffffff };

#define REC_FPUBRANCH(f) \
	void f(); \
	void rec##f() { \
	MOV32ItoM((uptr)&cpuRegs.code, cpuRegs.code); \
	MOV32ItoM((uptr)&cpuRegs.pc, pc); \
	iFlushCall(FLUSH_EVERYTHING); \
	CALLFunc((uptr)f); \
	branch = 2; \
}

#define REC_FPUFUNC(f) \
	void f(); \
	void rec##f() { \
	MOV32ItoM((uptr)&cpuRegs.code, cpuRegs.code); \
	MOV32ItoM((uptr)&cpuRegs.pc, pc); \
	iFlushCall(FLUSH_EVERYTHING); \
	CALLFunc((uptr)f); \
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// Misc...
//------------------------------------------------------------------
//static u32 _mxcsr = 0x7F80;
//static u32 _mxcsrs;
static u32 fpucw = 0x007f;
static u32 fpucws = 0;

void recCOP1_BC1()
{
	recCP1BC1[_Rt_]();
}

void SaveCW(int type) {
	if (iCWstate & type) return;

	if (type == 2) {
//		SSE_STMXCSR((uptr)&_mxcsrs);
//		SSE_LDMXCSR((uptr)&_mxcsr);
	} else {
		FNSTCW( (uptr)&fpucws );
		FLDCW( (uptr)&fpucw );
	}
	iCWstate|= type;
}

void LoadCW( void ) {
	if (iCWstate == 0) return;

	if (iCWstate & 2) {
		//SSE_LDMXCSR((uptr)&_mxcsrs);
	}
	if (iCWstate & 1) {
		FLDCW( (uptr)&fpucws );
	}
	iCWstate = 0;
}

void recCOP1_S( void ) {
#ifndef __x86_64__
	if( !cpucaps.hasStreamingSIMD2Extensions ) { // Not sure if this is needed anymore... (cottonvibes)
		_freeMMXreg(6);
		_freeMMXreg(7);
	}
#endif
	recCP1S[ _Funct_ ]( );
}

void recCOP1_W( void ) {
    recCP1W[ _Funct_ ]( );
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// FPU Opcodes!
//------------------------------------------------------------------
#ifndef FPU_RECOMPILE

REC_FPUFUNC(ABS_S);
REC_FPUFUNC(ADD_S);
REC_FPUFUNC(ADDA_S);
REC_FPUBRANCH(BC1F);
REC_FPUBRANCH(BC1T);
REC_FPUBRANCH(BC1FL);
REC_FPUBRANCH(BC1TL);
REC_FPUFUNC(C_EQ);
REC_FPUFUNC(C_F);
REC_FPUFUNC(C_LE);
REC_FPUFUNC(C_LT);
REC_FPUFUNC(CVT_S);
REC_FPUFUNC(CVT_W);
REC_FPUFUNC(DIV_S);
REC_FPUFUNC(MAX_S);
REC_FPUFUNC(MIN_S);
REC_FPUFUNC(MADD_S);
REC_FPUFUNC(MADDA_S);
REC_FPUFUNC(MOV_S);
REC_FPUFUNC(MSUB_S);
REC_FPUFUNC(MSUBA_S);
REC_FPUFUNC(MUL_S);
REC_FPUFUNC(MULA_S);
REC_FPUFUNC(NEG_S);
REC_FPUFUNC(SUB_S);
REC_FPUFUNC(SUBA_S);
REC_FPUFUNC(SQRT_S);
REC_FPUFUNC(RSQRT_S);

#else // FPU_RECOMPILE

//------------------------------------------------------------------
// Clamp Functions (Converts NaN's and Infinities to Normal Numbers)
//------------------------------------------------------------------
void fpuFloat(regd) { 
	if (CHECK_FPU_OVERFLOW && !CHECK_FPUCLAMPHACK) { // Tekken 5 doesn't like clamping infinities.
		SSE_MINSS_M32_to_XMM(regd, (uptr)&g_maxvals[0]); // MIN() must be before MAX()! So that NaN's become +Maximum
		SSE_MAXSS_M32_to_XMM(regd, (uptr)&g_minvals[0]);
	}
}

void ClampValues(regd) { 
	fpuFloat(regd);
}

void ClampValues2(regd) { 
	if (CHECK_FPUCLAMPHACK) { // Fixes Tekken 5 ( Makes NaN equal 0, infinities stay the same )
		int t5reg = _allocTempXMMreg(XMMT_FPS, -1);

		SSE_XORPS_XMM_to_XMM(t5reg, t5reg); 
		SSE_CMPORDSS_XMM_to_XMM(t5reg, regd); 

		SSE_ANDPS_XMM_to_XMM(regd, t5reg); 

		/* --- Its odd but tekken dosn't like Infinities to be clamped. --- */
		//SSE_MINSS_M32_to_XMM(regd, (uptr)&g_maxvals[0]);
		//SSE_MAXSS_M32_to_XMM(regd, (uptr)&g_minvals[0]);
		
		_freeXMMreg(t5reg); 
	}
	else fpuFloat(regd);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ABS XMM
//------------------------------------------------------------------
void recABS_S_xmm(int info)
{	
	if( info & PROCESS_EE_S ) {
		if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
	}
	else SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);

	SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&s_pos[0]);
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags

	if (CHECK_FPU_OVERFLOW) // Only need to do positive clamp, since EEREC_D is positive
		SSE_MINSS_M32_to_XMM(EEREC_D, (uptr)&g_maxvals[0]);
}

FPURECOMPILE_CONSTCODE(ABS_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// CommutativeOp XMM (used for ADD, MUL, MAX, and MIN opcodes)
//------------------------------------------------------------------
static void (*recComOpXMM_to_XMM[] )(x86SSERegType, x86SSERegType) = {
	SSE_ADDSS_XMM_to_XMM, SSE_MULSS_XMM_to_XMM, SSE_MAXSS_XMM_to_XMM, SSE_MINSS_XMM_to_XMM };

static void (*recComOpM32_to_XMM[] )(x86SSERegType, uptr) = {
	SSE_ADDSS_M32_to_XMM, SSE_MULSS_M32_to_XMM, SSE_MAXSS_M32_to_XMM, SSE_MINSS_M32_to_XMM };

int recCommutativeOp(int info, int regd, int op) 
{
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
    //if (t0reg == -1) {SysPrintf("FPU: CommutativeOp Allocation Error!\n");}

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if (regd == EEREC_S) {
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW && /*!CHECK_FPUCLAMPHACK &&*/ (op < 2)) { fpuFloat(regd); fpuFloat(t0reg); }
				recComOpXMM_to_XMM[op](regd, t0reg);
			}
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW && (op < 2)) { fpuFloat(regd); fpuFloat(EEREC_S); }
				recComOpXMM_to_XMM[op](regd, EEREC_S);
			}
			break;
		case PROCESS_EE_T:
			if (regd == EEREC_T) {
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW && (op < 2)) { fpuFloat(regd); fpuFloat(t0reg); }
				recComOpXMM_to_XMM[op](regd, t0reg);
			}
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW && (op < 2)) { fpuFloat(regd); fpuFloat(EEREC_T); }
				recComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if (regd == EEREC_T) {
				if (CHECK_FPU_EXTRA_OVERFLOW && (op < 2)) { fpuFloat(regd); fpuFloat(EEREC_S); }
				recComOpXMM_to_XMM[op](regd, EEREC_S);
			}
			else {
				if (regd != EEREC_S) SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				if (CHECK_FPU_EXTRA_OVERFLOW && (op < 2)) { fpuFloat(regd); fpuFloat(EEREC_T); }
				recComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			break;
		default:
			SysPrintf("FPU: recCommutativeOp case 4\n");
			SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
			SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
			if (CHECK_FPU_EXTRA_OVERFLOW && (op < 2)) { fpuFloat(regd); fpuFloat(t0reg); }
			recComOpXMM_to_XMM[op](regd, t0reg);
			break;
	}

	_freeXMMreg(t0reg);
	return regd;
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// ADD XMM
//------------------------------------------------------------------
void recADD_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    ClampValues2(recCommutativeOp(info, EEREC_D, 0));
	//REC_FPUOP(ADD_S);
}

FPURECOMPILE_CONSTCODE(ADD_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recADDA_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    ClampValues(recCommutativeOp(info, EEREC_ACC, 0));
}

FPURECOMPILE_CONSTCODE(ADDA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// BC1x XMM
//------------------------------------------------------------------
void recBC1F( void ) {
	u32 branchTo = (s32)_Imm_ * 4 + pc;
	
	_eeFlushAllUnused();
	MOV32MtoR(EAX, (uptr)&fpuRegs.fprc[31]);
	TEST32ItoR(EAX, FPUflagC);
	j32Ptr[0] = JNZ32(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);
	
	x86SetJ32(j32Ptr[0]);

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
}

void recBC1T( void ) {
	u32 branchTo = (s32)_Imm_ * 4 + pc;

	_eeFlushAllUnused();
	MOV32MtoR(EAX, (uptr)&fpuRegs.fprc[31]);
	TEST32ItoR(EAX, FPUflagC);
	j32Ptr[0] = JZ32(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);
	//j32Ptr[1] = JMP32(0);

	x86SetJ32(j32Ptr[0]);

	// recopy the next inst
	pc -= 4;
	LoadBranchState();
	recompileNextInstruction(1);

	SetBranchImm(pc);
	//x86SetJ32(j32Ptr[1]);	
}

void recBC1FL( void ) {
	u32 branchTo = _Imm_ * 4 + pc;

	_eeFlushAllUnused();
	MOV32MtoR(EAX, (uptr)&fpuRegs.fprc[31]);
	TEST32ItoR(EAX, FPUflagC);
	j32Ptr[0] = JNZ32(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}

void recBC1TL( void ) {
	u32 branchTo = _Imm_ * 4 + pc;

	_eeFlushAllUnused();
	MOV32MtoR(EAX, (uptr)&fpuRegs.fprc[31]);
	TEST32ItoR(EAX, FPUflagC);
	j32Ptr[0] = JZ32(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);
	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// C.x.S XMM
//------------------------------------------------------------------
void recC_EQ_xmm(int info)
{
	int tempReg;

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S: 
			SSE_MINSS_M32_to_XMM(EEREC_S, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_M32_to_XMM(EEREC_S, (uptr)&fpuRegs.fpr[_Ft_]); 
			break;
		case PROCESS_EE_T: 
			SSE_MINSS_M32_to_XMM(EEREC_T, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_M32_to_XMM(EEREC_T, (uptr)&fpuRegs.fpr[_Fs_]); 
			break;
		case (PROCESS_EE_S|PROCESS_EE_T): 
			SSE_MINSS_M32_to_XMM(EEREC_S, (uptr)&g_maxvals[0]);
			SSE_MINSS_M32_to_XMM(EEREC_T, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_XMM_to_XMM(EEREC_S, EEREC_T); 
			break;
		default: 
			SysPrintf("recC_EQ_xmm: Default\n");
			tempReg = _allocX86reg(-1, X86TYPE_TEMP, 0, 0);
			if (tempReg == -1) {SysPrintf("FPU: DIV Allocation Error!\n"); tempReg = EAX;}
			MOV32MtoR(tempReg, (uptr)&fpuRegs.fpr[_Fs_]);
			CMP32MtoR(tempReg, (uptr)&fpuRegs.fpr[_Ft_]); 

			j8Ptr[0] = JZ8(0);
				AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
			x86SetJ8(j8Ptr[1]);

			_freeX86reg(tempReg);
			return;
	}

	j8Ptr[0] = JZ8(0);
		AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_EQ, XMMINFO_READS|XMMINFO_READT);
//REC_FPUFUNC(C_EQ);

void recC_F()
{
	AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
}
//REC_FPUFUNC(C_F);

void recC_LE_xmm(int info )
{
	int tempReg;

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S: 
			SSE_MINSS_M32_to_XMM(EEREC_S, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_M32_to_XMM(EEREC_S, (uptr)&fpuRegs.fpr[_Ft_]); 
			break;
		case PROCESS_EE_T: 
			SSE_MINSS_M32_to_XMM(EEREC_T, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_M32_to_XMM(EEREC_T, (uptr)&fpuRegs.fpr[_Fs_]);
			j8Ptr[0] = JAE8(0);
				AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
			x86SetJ8(j8Ptr[1]);
			return;
		case (PROCESS_EE_S|PROCESS_EE_T):
			SSE_MINSS_M32_to_XMM(EEREC_S, (uptr)&g_maxvals[0]);
			SSE_MINSS_M32_to_XMM(EEREC_T, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_XMM_to_XMM(EEREC_S, EEREC_T); 
			break;
		default:
			SysPrintf("recC_LE_xmm: Default\n");
			tempReg = _allocX86reg(-1, X86TYPE_TEMP, 0, 0);
			if (tempReg == -1) {SysPrintf("FPU: DIV Allocation Error!\n"); tempReg = EAX;}
			MOV32MtoR(tempReg, (uptr)&fpuRegs.fpr[_Fs_]);
			CMP32MtoR(tempReg, (uptr)&fpuRegs.fpr[_Ft_]); 
			
			j8Ptr[0] = JLE8(0);
				AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
			x86SetJ8(j8Ptr[1]);

			_freeX86reg(tempReg);
			return;
	}

	j8Ptr[0] = JBE8(0);
		AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_LE, XMMINFO_READS|XMMINFO_READT);
//REC_FPUFUNC(C_LE);

void recC_LT_xmm(int info)
{
	int tempReg;

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			SSE_MINSS_M32_to_XMM(EEREC_S, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_M32_to_XMM(EEREC_S, (uptr)&fpuRegs.fpr[_Ft_]); 
			break;
		case PROCESS_EE_T:
			SSE_MINSS_M32_to_XMM(EEREC_T, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_M32_to_XMM(EEREC_T, (uptr)&fpuRegs.fpr[_Fs_]);
			j8Ptr[0] = JA8(0);
				AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
			x86SetJ8(j8Ptr[1]);
			return;
		case (PROCESS_EE_S|PROCESS_EE_T):
			// Makes NaNs and +Infinity be +maximum; -Infinity stays 
			// the same, but this is okay for a Compare operation.
			// Note: This fixes a crash in Rule of Rose.
			SSE_MINSS_M32_to_XMM(EEREC_S, (uptr)&g_maxvals[0]);
			SSE_MINSS_M32_to_XMM(EEREC_T, (uptr)&g_maxvals[0]);
			SSE_UCOMISS_XMM_to_XMM(EEREC_S, EEREC_T); 
			break;
		default:
			SysPrintf("recC_LT_xmm: Default\n");
			tempReg = _allocX86reg(-1, X86TYPE_TEMP, 0, 0);
			if (tempReg == -1) {SysPrintf("FPU: DIV Allocation Error!\n"); tempReg = EAX;}
			MOV32MtoR(tempReg, (uptr)&fpuRegs.fpr[_Fs_]);
			CMP32MtoR(tempReg, (uptr)&fpuRegs.fpr[_Ft_]); 
			
			j8Ptr[0] = JL8(0);
				AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
				j8Ptr[1] = JMP8(0);
			x86SetJ8(j8Ptr[0]);
				OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
			x86SetJ8(j8Ptr[1]);

			_freeX86reg(tempReg);
			return;
	}

	j8Ptr[0] = JB8(0);
		AND32ItoM( (uptr)&fpuRegs.fprc[31], ~FPUflagC );
		j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
		OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagC);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_LT, XMMINFO_READS|XMMINFO_READT);
//REC_FPUFUNC(C_LT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// CFC1 / CTC1 XMM
//------------------------------------------------------------------
void recCFC1(void)
{
	if ( ! _Rt_ ) return;

	_eeOnWriteReg(_Rt_, 1);

	MOV32MtoR( EAX, (uptr)&fpuRegs.fprc[ _Fs_ ] );
	_deleteEEreg(_Rt_, 0);

	if(EEINST_ISLIVE1(_Rt_)) {
#ifdef __x86_64__
        CDQE();
        MOV64RtoM( (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], RAX );
#else
		CDQ( );
		MOV32RtoM( (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX );
		MOV32RtoM( (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ], EDX );
#endif
	}
	else {
		EEINST_RESETHASLIVE1(_Rt_);
		MOV32RtoM( (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX );
	}
}

void recCTC1( void )
{
	if( GPR_IS_CONST1(_Rt_)) {
		MOV32ItoM((uptr)&fpuRegs.fprc[ _Fs_ ], g_cpuConstRegs[_Rt_].UL[0]);
	}
	else {
		int mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
		if( mmreg >= 0 ) {
			SSEX_MOVD_XMM_to_M32((uptr)&fpuRegs.fprc[ _Fs_ ], mmreg);
		}
#ifdef __x86_64__
        else if( (mmreg = _checkX86reg(X86TYPE_GPR, _Rt_, MODE_READ)) >= 0 ) {
			MOV32RtoM((uptr)&fpuRegs.fprc[ _Fs_ ], mmreg);
		}
#else
		else if( (mmreg = _checkMMXreg(MMX_GPR+_Rt_, MODE_READ)) >= 0 ) {
			MOVDMMXtoM((uptr)&fpuRegs.fprc[ _Fs_ ], mmreg);
			SetMMXstate();
		}
#endif
		else {
			_deleteGPRtoXMMreg(_Rt_, 1);

#ifdef __x86_64__
            _deleteX86reg(X86TYPE_GPR, _Rt_, 1);
#else
			_deleteMMXreg(MMX_GPR+_Rt_, 1);
#endif
			MOV32MtoR( EAX, (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ] );
			MOV32RtoM( (uptr)&fpuRegs.fprc[ _Fs_ ], EAX );
		}
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// CVT.x XMM
//------------------------------------------------------------------
void recCVT_S_xmm(int info)
{
	if( !(info&PROCESS_EE_S) || (EEREC_D != EEREC_S && !(info&PROCESS_EE_MODEWRITES)) ) {
		SSE_CVTSI2SS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
	}
	else {
		if( cpucaps.hasStreamingSIMD2Extensions ) {
			SSE2_CVTDQ2PS_XMM_to_XMM(EEREC_D, EEREC_S);
		}
		else {
			if( info&PROCESS_EE_MODEWRITES ) {
				if( xmmregs[EEREC_S].reg == _Fs_ )
					_deleteFPtoXMMreg(_Fs_, 1);
				else {
					// force sync
					SSE_MOVSS_XMM_to_M32((uptr)&fpuRegs.fpr[_Fs_], EEREC_S);
				}
			}
			SSE_CVTSI2SS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
			xmmregs[EEREC_D].mode |= MODE_WRITE; // in the case that _Fs_ == _Fd_
		}
	}
}

FPURECOMPILE_CONSTCODE(CVT_S, XMMINFO_WRITED|XMMINFO_READS);

static u32 s_signbit = 0x80000000;

void recCVT_W() 
{
	if( cpucaps.hasStreamingSIMDExtensions ) {
		int t0reg;
		int regs = _checkXMMreg(XMMTYPE_FPREG, _Fs_, MODE_READ);

		if( regs >= 0 ) {
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			_freeXMMreg(t0reg);
			SSE_MOVSS_M32_to_XMM(t0reg, (u32)&s_signbit);
			SSE_CVTTSS2SI_XMM_to_R32(EAX, regs);
			SSE_MOVSS_XMM_to_M32((uptr)&fpuRegs.fpr[ _Fs_ ], regs);
		}
		else SSE_CVTTSS2SI_M32_to_R32(EAX, (uptr)&fpuRegs.fpr[ _Fs_ ]);
		_deleteFPtoXMMreg(_Fd_, 2);

		MOV32MtoR(ECX, (uptr)&fpuRegs.fpr[ _Fs_ ]);
		AND32ItoR(ECX, 0x7f800000);
		CMP32ItoR(ECX, 0x4E800000);
		j8Ptr[0] = JLE8(0);

		// need to detect if reg is positive
		/*if( regs >= 0 ) {
			SSE_UCOMISS_XMM_to_XMM(regs, t0reg);
			j8Ptr[2] = JB8(0);
		}
		else {*/
			TEST32ItoM((uptr)&fpuRegs.fpr[ _Fs_ ], 0x80000000);
			j8Ptr[2] = JNZ8(0);
		//}

		MOV32ItoM((uptr)&fpuRegs.fpr[_Fd_], 0x7fffffff);
		j8Ptr[1] = JMP8(0);

		x86SetJ8( j8Ptr[2] );
		MOV32ItoM((uptr)&fpuRegs.fpr[_Fd_], 0x80000000);
		j8Ptr[1] = JMP8(0);

		x86SetJ8( j8Ptr[0] );
		
		MOV32RtoM((uptr)&fpuRegs.fpr[_Fd_], EAX);

		x86SetJ8( j8Ptr[1] );
	}
#ifndef __x86_64__
	else {
		MOV32ItoM((uptr)&cpuRegs.code, cpuRegs.code);
		iFlushCall(FLUSH_EVERYTHING);
		_flushConstRegs();
		CALLFunc((uptr)CVT_W);
	}
#endif
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// DIV XMM
//------------------------------------------------------------------
void recDIVhelper1(int regd, int regt) // Sets flags
{
	u8 *pjmp1, *pjmp2;
	u32 *ajmp32, *bjmp32;
	int t1reg = _allocTempXMMreg(XMMT_FPS, -1);
	int tempReg = _allocX86reg(-1, X86TYPE_TEMP, 0, 0);
	//if (t1reg == -1) {SysPrintf("FPU: DIV Allocation Error!\n");}
	if (tempReg == -1) {SysPrintf("FPU: DIV Allocation Error!\n"); tempReg = EAX;}

	AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagI|FPUflagD)); // Clear I and D flags

	/*--- Check for divide by zero ---*/
	SSE_XORPS_XMM_to_XMM(t1reg, t1reg);
	SSE_CMPEQSS_XMM_to_XMM(t1reg, regt);
	SSE_MOVMSKPS_XMM_to_R32(tempReg, t1reg);
	AND32ItoR(tempReg, 1);  //Check sign (if regt == zero, sign will be set)
	ajmp32 = JZ32(0); //Skip if not set

		/*--- Check for 0/0 ---*/
		SSE_XORPS_XMM_to_XMM(t1reg, t1reg);
		SSE_CMPEQSS_XMM_to_XMM(t1reg, regd);
		SSE_MOVMSKPS_XMM_to_R32(tempReg, t1reg);
		AND32ItoR(tempReg, 1);  //Check sign (if regd == zero, sign will be set)
		pjmp1 = JZ8(0); //Skip if not set
			OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagI|FPUflagSI); // Set I and SI flags ( 0/0 )
			pjmp2 = JMP8(0);
		x86SetJ8(pjmp1); //x/0 but not 0/0
			OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagD|FPUflagSD); // Set D and SD flags ( x/0 )
		x86SetJ8(pjmp2);

		/*--- Make regd +/- Maximum ---*/
		SSE_XORPS_XMM_to_XMM(regd, regt); // Make regd Positive or Negative
		SSE_ANDPS_M128_to_XMM(regd, (uptr)&s_neg[0]); // Get the sign bit
		SSE_ORPS_M128_to_XMM(regd, (uptr)&g_maxvals[0]); // regd = +/- Maximum
		//SSE_MOVSS_M32_to_XMM(regd, (uptr)&g_maxvals[0]);
		bjmp32 = JMP32(0);

	x86SetJ32(ajmp32);

	/*--- Normal Divide ---*/
	if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(regt); }
	SSE_DIVSS_XMM_to_XMM(regd, regt);

	ClampValues(regd);
	x86SetJ32(bjmp32);

	_freeXMMreg(t1reg);
	_freeX86reg(tempReg);
}

void recDIVhelper2(int regd, int regt) // Doesn't sets flags
{
	if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(regt); }
	SSE_DIVSS_XMM_to_XMM(regd, regt);
	ClampValues(regd);
}

void recDIV_S_xmm(int info)
{
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
    //if (t0reg == -1) {SysPrintf("FPU: DIV Allocation Error!\n");}

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			//SysPrintf("FPU: DIV case 1\n");
			if (EEREC_D != EEREC_S) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
			if (CHECK_FPU_EXTRA_FLAGS) recDIVhelper1(EEREC_D, t0reg);
			else recDIVhelper2(EEREC_D, t0reg);
			break;
		case PROCESS_EE_T:
			//SysPrintf("FPU: DIV case 2\n");
			if (EEREC_D == EEREC_T) {
				SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_T);
				SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_FLAGS) recDIVhelper1(EEREC_D, t0reg);
				else recDIVhelper2(EEREC_D, t0reg);
			}
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_FLAGS) recDIVhelper1(EEREC_D, EEREC_T);
				else recDIVhelper2(EEREC_D, EEREC_T);
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			//SysPrintf("FPU: DIV case 3\n");
			if (EEREC_D == EEREC_T) {
				SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_T);
				SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				if (CHECK_FPU_EXTRA_FLAGS) recDIVhelper1(EEREC_D, t0reg);
				else recDIVhelper2(EEREC_D, t0reg);
			}
			else {
				if (EEREC_D != EEREC_S) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
				if (CHECK_FPU_EXTRA_FLAGS) recDIVhelper1(EEREC_D, EEREC_T);
				else recDIVhelper2(EEREC_D, EEREC_T);
			}
			break;
		default:
			//SysPrintf("FPU: DIV case 4\n");
			SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
			SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
			if (CHECK_FPU_EXTRA_FLAGS) recDIVhelper1(EEREC_D, t0reg);
			else recDIVhelper2(EEREC_D, t0reg);
			break;
	}
	_freeXMMreg(t0reg);
}

FPURECOMPILE_CONSTCODE(DIV_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MADD XMM
//------------------------------------------------------------------
void recMADDtemp(int info, int regd)
{	
	int t1reg;
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
	
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if(regd == EEREC_S) {
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(regd, t0reg);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}
			} 
			else if (regd == EEREC_ACC){
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_S); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(t0reg, EEREC_S);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_ADDSS_XMM_to_XMM(regd, t0reg);
			} 
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_S); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}
			}
			break;
		case PROCESS_EE_T:	
			if(regd == EEREC_T) {
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(regd, t0reg);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}
			} 
			else if (regd == EEREC_ACC){
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_T); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(t0reg, EEREC_T);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_ADDSS_XMM_to_XMM(regd, t0reg);
			} 
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(regd); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_ACC); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if(regd == EEREC_S) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}
			} 
			else if(regd == EEREC_T) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_S); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}
			} 
			else if(regd == EEREC_ACC) {
				SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_S);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(t0reg); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(t0reg, EEREC_T);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_ADDSS_XMM_to_XMM(regd, t0reg);
			} 
			else {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}			
			}
			break;
		default:
			if(regd == EEREC_ACC){
				t1reg = _allocTempXMMreg(XMMT_FPS, -1);
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				SSE_MOVSS_M32_to_XMM(t1reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(t0reg); fpuFloat(t1reg); }
				SSE_MULSS_XMM_to_XMM(t0reg, t1reg);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				_freeXMMreg(t1reg);
			} 
			else
			{
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(regd, t0reg);
				if (info & PROCESS_EE_ACC) {
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_ACC); }
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else {
					SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC);
					if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
					SSE_ADDSS_XMM_to_XMM(regd, t0reg);
				}
			}
			break;
	}

     ClampValues(regd);
	 _freeXMMreg(t0reg);
}

void recMADD_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMADDtemp(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(MADD_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMADDA_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMADDtemp(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(MADDA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MAX / MIN XMM
//------------------------------------------------------------------
void recMAX_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    recCommutativeOp(info, EEREC_D, 2);
}

FPURECOMPILE_CONSTCODE(MAX_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recMIN_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    recCommutativeOp(info, EEREC_D, 3);
}

FPURECOMPILE_CONSTCODE(MIN_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MFC1 XMM
//------------------------------------------------------------------
void recMFC1(void) {
	int regt, regs;
	if ( ! _Rt_ ) return;

	_eeOnWriteReg(_Rt_, 1);

	regs = _checkXMMreg(XMMTYPE_FPREG, _Fs_, MODE_READ);
	if( regs >= 0 ) {
		_deleteGPRtoXMMreg(_Rt_, 2);

#ifdef __x86_64__
        regt = _allocCheckGPRtoX86(g_pCurInstInfo, _Rt_, MODE_WRITE);
		
		if( regt >= 0 ) {

			if(EEINST_ISLIVE1(_Rt_)) {
                SSE2_MOVD_XMM_to_R(RAX, regs);
                // sign extend
                CDQE();
                MOV64RtoR(regt, RAX);
            }
            else {
                SSE2_MOVD_XMM_to_R(regt, regs);
                EEINST_RESETHASLIVE1(_Rt_);
            }
		}
#else
		regt = _allocCheckGPRtoMMX(g_pCurInstInfo, _Rt_, MODE_WRITE);
		
		if( regt >= 0 ) {
			SSE2_MOVDQ2Q_XMM_to_MM(regt, regs);

			if(EEINST_ISLIVE1(_Rt_)) _signExtendGPRtoMMX(regt, _Rt_, 0);
			else EEINST_RESETHASLIVE1(_Rt_);
		}
#endif
		else {
			if(EEINST_ISLIVE1(_Rt_)) {
				_signExtendXMMtoM((uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], regs, 0);
			}
			else {
				EEINST_RESETHASLIVE1(_Rt_);
				SSE_MOVSS_XMM_to_M32((uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], regs);
			}
		}
	}
#ifndef __x86_64__
	else if( (regs = _checkMMXreg(MMX_FPU+_Fs_, MODE_READ)) >= 0 ) {
		// convert to mmx reg
		mmxregs[regs].reg = MMX_GPR+_Rt_;
		mmxregs[regs].mode |= MODE_READ|MODE_WRITE;
		_signExtendGPRtoMMX(regs, _Rt_, 0);
	}
#endif
	else {
		regt = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
	
		if( regt >= 0 ) {
			if( xmmregs[regt].mode & MODE_WRITE ) {
				SSE_MOVHPS_XMM_to_M64((uptr)&cpuRegs.GPR.r[_Rt_].UL[2], regt);
			}
			xmmregs[regt].inuse = 0;
		}
#ifdef __x86_64__
        else if( (regt = _allocCheckGPRtoX86(g_pCurInstInfo, _Rt_, MODE_WRITE)) >= 0 ) {

            if(EEINST_ISLIVE1(_Rt_)) {
                MOV32MtoR( RAX, (uptr)&fpuRegs.fpr[ _Fs_ ].UL );
                CDQE();
                MOV64RtoR(regt, RAX);
            }
            else {
                MOV32MtoR( regt, (uptr)&fpuRegs.fpr[ _Fs_ ].UL );
                EEINST_RESETHASLIVE1(_Rt_);
            }
        }
        else
#endif
        {

            _deleteEEreg(_Rt_, 0);
            MOV32MtoR( EAX, (uptr)&fpuRegs.fpr[ _Fs_ ].UL );
            
            if(EEINST_ISLIVE1(_Rt_)) {
#ifdef __x86_64__
                CDQE();
                MOV64RtoM((uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], RAX);
#else
                CDQ( );
                MOV32RtoM( (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX );
                MOV32RtoM( (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 1 ], EDX );
#endif
            }
            else {
                EEINST_RESETHASLIVE1(_Rt_);
                MOV32RtoM( (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ], EAX );
            }
		}
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MOV XMM
//------------------------------------------------------------------
void recMOV_S_xmm(int info)
{
	if( info & PROCESS_EE_S ) {
		if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
	}
	else SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
}

FPURECOMPILE_CONSTCODE(MOV_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MSUB XMM
//------------------------------------------------------------------
void recMSUBtemp(int info, int regd)
{
int t1reg;
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
	
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if(regd == EEREC_S) {
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(regd, t0reg);
				if (info & PROCESS_EE_ACC) { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); }
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);
			} 
			else if (regd == EEREC_ACC){
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_S); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(t0reg, EEREC_S);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(regd, t0reg);
			} 
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_S); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
				if (info & PROCESS_EE_ACC) { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); }
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);
			}
			break;
		case PROCESS_EE_T:	
			if(regd == EEREC_T) {
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(regd, t0reg);
				if (info & PROCESS_EE_ACC) { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); }
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);
			} 
			else if (regd == EEREC_ACC){
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(EEREC_T); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(t0reg, EEREC_T);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(regd, t0reg);
			} 
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if (info & PROCESS_EE_ACC) { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); }
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			if(regd == EEREC_S) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if (info & PROCESS_EE_ACC) { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); }
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);
			} 
			else if(regd == EEREC_T) {
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_S); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
				if (info & PROCESS_EE_ACC) { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); }
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);
			} 
			else if(regd == EEREC_ACC) {
				SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_S);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(t0reg); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(t0reg, EEREC_T);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(regd, t0reg);
			} 
			else {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(EEREC_T); }
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if (info & PROCESS_EE_ACC) { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); }
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);	
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);
			}
			break;
		default:
			if(regd == EEREC_ACC){
				t1reg = _allocTempXMMreg(XMMT_FPS, -1);
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				SSE_MOVSS_M32_to_XMM(t1reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(t0reg); fpuFloat(t1reg); }
				SSE_MULSS_XMM_to_XMM(t0reg, t1reg);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(regd, t0reg);
				_freeXMMreg(t1reg);
			} 
			else
			{
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_MULSS_XMM_to_XMM(regd, t0reg);
				if (info & PROCESS_EE_ACC)  { SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_ACC); } 
				else { SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.ACC); }
				if (CHECK_FPU_EXTRA_OVERFLOW) { fpuFloat(regd); fpuFloat(t0reg); }
				SSE_SUBSS_XMM_to_XMM(t0reg, regd);
				SSE_MOVSS_XMM_to_XMM(regd, t0reg);	
			}
			break;
	}

     ClampValues(regd);
	 _freeXMMreg(t0reg);

}

void recMSUB_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMSUBtemp(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(MSUB_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMSUBA_S_xmm(int info)
{
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	recMSUBtemp(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(MSUBA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// MTC1 XMM
//------------------------------------------------------------------
void recMTC1(void)
{
	if( GPR_IS_CONST1(_Rt_) ) {
		_deleteFPtoXMMreg(_Fs_, 0);
		MOV32ItoM((uptr)&fpuRegs.fpr[ _Fs_ ].UL, g_cpuConstRegs[_Rt_].UL[0]);
	}
	else {
		int mmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
		if( mmreg >= 0 ) {
			if( g_pCurInstInfo->regs[_Rt_] & EEINST_LASTUSE ) {
				// transfer the reg directly
				_deleteGPRtoXMMreg(_Rt_, 2);
				_deleteFPtoXMMreg(_Fs_, 2);
				_allocFPtoXMMreg(mmreg, _Fs_, MODE_WRITE);
			}
			else {
				int mmreg2 = _allocCheckFPUtoXMM(g_pCurInstInfo, _Fs_, MODE_WRITE);
				if( mmreg2 >= 0 ) SSE_MOVSS_XMM_to_XMM(mmreg2, mmreg);
				else SSE_MOVSS_XMM_to_M32((uptr)&fpuRegs.fpr[ _Fs_ ].UL, mmreg);
			}
		}
#ifndef __x86_64__
		else if( (mmreg = _checkMMXreg(MMX_GPR+_Rt_, MODE_READ)) >= 0 ) {

			if( cpucaps.hasStreamingSIMD2Extensions ) {
				int mmreg2 = _allocCheckFPUtoXMM(g_pCurInstInfo, _Fs_, MODE_WRITE);
				if( mmreg2 >= 0 ) {
					SetMMXstate();
					SSE2_MOVQ2DQ_MM_to_XMM(mmreg2, mmreg);
				}
				else {
					SetMMXstate();
					MOVDMMXtoM((uptr)&fpuRegs.fpr[ _Fs_ ].UL, mmreg);
				}
			}
			else {
				_deleteFPtoXMMreg(_Fs_, 0);
				SetMMXstate();
				MOVDMMXtoM((uptr)&fpuRegs.fpr[ _Fs_ ].UL, mmreg);
			}
		}
#endif
		else {
			int mmreg2 = _allocCheckFPUtoXMM(g_pCurInstInfo, _Fs_, MODE_WRITE);

			if( mmreg2 >= 0 ) SSE_MOVSS_M32_to_XMM(mmreg2, (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]);
			else {
				MOV32MtoR(EAX, (uptr)&cpuRegs.GPR.r[ _Rt_ ].UL[ 0 ]);
				MOV32RtoM((uptr)&fpuRegs.fpr[ _Fs_ ].UL, EAX);
			}
		}
	}
}
//------------------------------------------------------------------


//------------------------------------------------------------------
// MUL XMM
//------------------------------------------------------------------
void recMUL_S_xmm(int info)
{			
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
    ClampValues(recCommutativeOp(info, EEREC_D, 1)); 
}

FPURECOMPILE_CONSTCODE(MUL_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recMULA_S_xmm(int info) 
{ 
	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	ClampValues(recCommutativeOp(info, EEREC_ACC, 1));
}

FPURECOMPILE_CONSTCODE(MULA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// NEG XMM
//------------------------------------------------------------------
void recNEG_S_xmm(int info) {
	if( info & PROCESS_EE_S ) {
		if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
	}
	else SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);

	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags
	SSE_XORPS_M128_to_XMM(EEREC_D, (uptr)&s_neg[0]);
	ClampValues(EEREC_D);
}

FPURECOMPILE_CONSTCODE(NEG_S, XMMINFO_WRITED|XMMINFO_READS);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SUB XMM
//------------------------------------------------------------------
void recSUBhelper(int regd, int regt)
{
	if (CHECK_FPU_EXTRA_OVERFLOW /*&& !CHECK_FPUCLAMPHACK*/) { fpuFloat(regd); fpuFloat(regt); }
	SSE_SUBSS_XMM_to_XMM(regd, regt);
}

void recSUBop(int info, int regd)
{
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
    //if (t0reg == -1) {SysPrintf("FPU: SUB Allocation Error!\n");}

	//AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagO|FPUflagU)); // Clear O and U flags

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			//SysPrintf("FPU: SUB case 1\n");
			if (regd != EEREC_S) SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
			SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
			recSUBhelper(regd, t0reg);
			break;
		case PROCESS_EE_T:
			//SysPrintf("FPU: SUB case 2\n");
			if (regd == EEREC_T) {
				SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_T);
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				recSUBhelper(regd, t0reg);
			}
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				recSUBhelper(regd, EEREC_T);
			}
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			//SysPrintf("FPU: SUB case 3\n");
			if (regd == EEREC_T) {
				SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_T);
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				recSUBhelper(regd, t0reg);
			}
			else {
				if (regd != EEREC_S) SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				recSUBhelper(regd, EEREC_T);
			}
			break;
		default:
			SysPrintf("FPU: SUB case 4\n");
			SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
			SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
			recSUBhelper(regd, t0reg);
			break;
	}

	ClampValues2(regd);
	_freeXMMreg(t0reg);
}

void recSUB_S_xmm(int info)
{
	recSUBop(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(SUB_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);


void recSUBA_S_xmm(int info) 
{ 
	recSUBop(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(SUBA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// SQRT XMM
//------------------------------------------------------------------
void recSQRT_S_xmm(int info)
{
	u8* pjmp;
	int tempReg = _allocX86reg(-1, X86TYPE_TEMP, 0, 0);
	if (tempReg == -1) {SysPrintf("FPU: SQRT Allocation Error!\n"); tempReg = EAX;}
	//SysPrintf("FPU: SQRT\n");

	if( info & PROCESS_EE_T ) { 
		if ( EEREC_D != EEREC_T ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_T); 
	}
	else SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Ft_]);

	if (CHECK_FPU_EXTRA_FLAGS) {
		AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagI|FPUflagD)); // Clear I and D flags

		/*--- Check for negative SQRT ---*/
		SSE_MOVMSKPS_XMM_to_R32(tempReg, EEREC_D);
		AND32ItoR(tempReg, 1);  //Check sign
		pjmp = JZ8(0); //Skip if none are
			OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagI|FPUflagSI); // Set I and SI flags
			SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&s_pos[0]); // Make EEREC_D Positive
		x86SetJ8(pjmp);
	}
	else SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&s_pos[0]); // Make EEREC_D Positive
	
	if (CHECK_FPU_OVERFLOW) SSE_MINSS_M32_to_XMM(EEREC_D, (uptr)&g_maxvals[0]);// Only need to do positive clamp, since EEREC_D is positive
	SSE_SQRTSS_XMM_to_XMM(EEREC_D, EEREC_D);
	if (CHECK_FPU_EXTRA_OVERFLOW) ClampValues(EEREC_D); // Shouldn't need to clamp again since SQRT of a number will always be smaller than the original number, doing it just incase :/

	_freeX86reg(tempReg);
}

FPURECOMPILE_CONSTCODE(SQRT_S, XMMINFO_WRITED|XMMINFO_READT);
//------------------------------------------------------------------


//------------------------------------------------------------------
// RSQRT XMM
//------------------------------------------------------------------
void recRSQRThelper1(int regd, int t0reg) // Preforms the RSQRT function when regd <- Fs and t0reg <- Ft (Sets correct flags)
{
	u8 *pjmp1, *pjmp2;
	u32 *pjmp32;
	int t1reg = _allocTempXMMreg(XMMT_FPS, -1);
	int tempReg = _allocX86reg(-1, X86TYPE_TEMP, 0, 0);
	//if (t1reg == -1) {SysPrintf("FPU: RSQRT Allocation Error!\n");}
	if (tempReg == -1) {SysPrintf("FPU: RSQRT Allocation Error!\n"); tempReg = EAX;}

	AND32ItoM((uptr)&fpuRegs.fprc[31], ~(FPUflagI|FPUflagD)); // Clear I and D flags

	/*--- Check for zero ---*/
	SSE_XORPS_XMM_to_XMM(t1reg, t1reg);
	SSE_CMPEQSS_XMM_to_XMM(t1reg, t0reg);
	SSE_MOVMSKPS_XMM_to_R32(tempReg, t1reg);
	AND32ItoR(tempReg, 1);  //Check sign (if t0reg == zero, sign will be set)
	pjmp1 = JZ8(0); //Skip if not set
		OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagD|FPUflagSD); // Set D and SD flags
		SSE_XORPS_XMM_to_XMM(regd, t0reg); // Make regd Positive or Negative
		SSE_ANDPS_M128_to_XMM(regd, (uptr)&s_neg[0]); // Get the sign bit
		SSE_ORPS_M128_to_XMM(regd, (uptr)&g_maxvals[0]); // regd = +/- Maximum
		pjmp32 = JMP32(0);
	x86SetJ8(pjmp1);

	/*--- Check for negative SQRT ---*/
	SSE_MOVMSKPS_XMM_to_R32(tempReg, t0reg);
	AND32ItoR(tempReg, 1);  //Check sign
	pjmp2 = JZ8(0); //Skip if not set
		OR32ItoM((uptr)&fpuRegs.fprc[31], FPUflagI|FPUflagSI); // Set I and SI flags
		SSE_ANDPS_M128_to_XMM(t0reg, (uptr)&s_pos[0]); // Make t0reg Positive
	x86SetJ8(pjmp2);

	if (CHECK_FPU_EXTRA_OVERFLOW) {
		SSE_MINSS_M32_to_XMM(t0reg, (uptr)&g_maxvals[0]); // Only need to do positive clamp, since t0reg is positive
		fpuFloat(regd);
	}

	SSE_SQRTSS_XMM_to_XMM(t0reg, t0reg);
	SSE_DIVSS_XMM_to_XMM(regd, t0reg);

	ClampValues(regd);
	x86SetJ32(pjmp32);

	_freeXMMreg(t1reg);
	_freeX86reg(tempReg);
}

void recRSQRThelper2(int regd, int t0reg) // Preforms the RSQRT function when regd <- Fs and t0reg <- Ft (Doesn't set flags)
{
	SSE_ANDPS_M128_to_XMM(t0reg, (uptr)&s_pos[0]); // Make t0reg Positive
	if (CHECK_FPU_EXTRA_OVERFLOW) {
		SSE_MINSS_M32_to_XMM(t0reg, (uptr)&g_maxvals[0]); // Only need to do positive clamp, since t0reg is positive
		fpuFloat(regd);
	}
	SSE_SQRTSS_XMM_to_XMM(t0reg, t0reg);
	SSE_DIVSS_XMM_to_XMM(regd, t0reg);
	ClampValues(regd);
}

void recRSQRT_S_xmm(int info)
{
	int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
	//if (t0reg == -1) {SysPrintf("FPU: RSQRT Allocation Error!\n");}
	//SysPrintf("FPU: RSQRT\n");

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			//SysPrintf("FPU: RSQRT case 1\n");
			if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
			if (CHECK_FPU_EXTRA_FLAGS) recRSQRThelper1(EEREC_D, t0reg);
			else recRSQRThelper2(EEREC_D, t0reg);
			break;
		case PROCESS_EE_T:	
			//SysPrintf("FPU: RSQRT case 2\n");
			SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_T);
			SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
			if (CHECK_FPU_EXTRA_FLAGS) recRSQRThelper1(EEREC_D, t0reg);
			else recRSQRThelper2(EEREC_D, t0reg);
			break;
		case (PROCESS_EE_S|PROCESS_EE_T):
			//SysPrintf("FPU: RSQRT case 3\n");
			SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_T);		
			if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
			if (CHECK_FPU_EXTRA_FLAGS) recRSQRThelper1(EEREC_D, t0reg);
			else recRSQRThelper2(EEREC_D, t0reg);
			break;
		default:
			//SysPrintf("FPU: RSQRT case 4\n");
			SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
			SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);		
			if (CHECK_FPU_EXTRA_FLAGS) recRSQRThelper1(EEREC_D, t0reg);
			else recRSQRThelper2(EEREC_D, t0reg);
			break;
	}
	_freeXMMreg(t0reg);
}

FPURECOMPILE_CONSTCODE(RSQRT_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

#endif // FPU_RECOMPILE

#endif // PCSX2_NORECBUILD
