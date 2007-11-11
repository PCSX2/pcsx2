/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2005  Pcsx2 Team
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
// stop compiling if NORECBUILD build (only for Visual Studio)
#if !(defined(_MSC_VER) && defined(PCSX2_NORECBUILD))

#include "Common.h"
#include "InterTables.h"
#include "ix86/ix86.h"
#include "iR5900.h"
#include "iFPU.h"

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

/*********************************************************
*   COP1 opcodes                                         *
*                                                        *
*********************************************************/

#define _Ft_ _Rt_
#define _Fs_ _Rd_
#define _Fd_ _Sa_

extern PCSX2_ALIGNED16_DECL(u32 g_minvals[4]);
extern PCSX2_ALIGNED16_DECL(u32 g_maxvals[4]);

////////////////////////////////////////////////////
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

////////////////////////////////////////////////////
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

////////////////////////////////////////////////////
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

////////////////////////////////////////////////////
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

////////////////////////////////////////////////////
void recCOP1_BC1()
{
	recCP1BC1[_Rt_]();
}

static u32 _mxcsr = 0x7F80;
static u32 _mxcsrs;
static u32 fpucw = 0x007f;
static u32 fpucws = 0;

////////////////////////////////////////////////////
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

////////////////////////////////////////////////////
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

////////////////////////////////////////////////////
void recCOP1_S( void ) 
{
#ifndef __x86_64__
	if( !EE_FPU_REGCACHING || !cpucaps.hasStreamingSIMD2Extensions) {
		_freeMMXreg(6);
		_freeMMXreg(7);
	}
#endif
	recCP1S[ _Funct_ ]( );
}

////////////////////////////////////////////////////
void recCOP1_W( void ) 
{
#ifndef __x86_64__
    if( !EE_FPU_REGCACHING ) {
		_freeMMXreg(6);
		_freeMMXreg(7);
	}
#endif
    recCP1W[ _Funct_ ]( );
}

#ifndef FPU_RECOMPILE


REC_FPUFUNC(ADD_S);
REC_FPUFUNC(SUB_S);
REC_FPUFUNC(MUL_S);
REC_FPUFUNC(DIV_S);
REC_FPUFUNC(SQRT_S);
REC_FPUFUNC(RSQRT_S);
REC_FPUFUNC(ABS_S);
REC_FPUFUNC(MOV_S);
REC_FPUFUNC(NEG_S);
REC_FPUFUNC(ADDA_S);
REC_FPUFUNC(SUBA_S);
REC_FPUFUNC(MULA_S);
REC_FPUFUNC(MADD_S);
REC_FPUFUNC(MSUB_S);
REC_FPUFUNC(MADDA_S);
REC_FPUFUNC(MSUBA_S);
REC_FPUFUNC(CVT_S);
REC_FPUFUNC(CVT_W);
REC_FPUFUNC(MIN_S);
REC_FPUFUNC(MAX_S);
REC_FPUBRANCH(BC1F);
REC_FPUBRANCH(BC1T);
REC_FPUBRANCH(BC1FL);
REC_FPUBRANCH(BC1TL);
REC_FPUFUNC(C_F);
REC_FPUFUNC(C_EQ);
REC_FPUFUNC(C_LE);
REC_FPUFUNC(C_LT);

#else

// define the FPU ops using the x86 FPU. x86-64 doesn't use FPU
#ifndef __x86_64__

void SetQFromStack(u32 mem)
{
	write16(0xe5d9);
	FNSTSWtoAX();
	write8(0x9e);
	j8Ptr[0] = JAE8(0); // jnc
	
	// sign bit is in bit 9 of EAX
	FSTP(0); // pop
	AND32ItoR(EAX, 0x200);
	SHL32ItoR(EAX, 22);
	OR32MtoR(EAX, (uptr)&g_maxvals[0]);
	MOV32RtoM(mem, EAX);
	j8Ptr[1] = JMP8(0);

	x86SetJ8(j8Ptr[0]);
	// just pop
	FSTP32(mem);
	x86SetJ8(j8Ptr[1]);
}

void recC_EQ_(int info)
{
    SetFPUstate();

	FLD32( (uptr)&fpuRegs.fpr[_Fs_].f);
	FCOMP32( (uptr)&fpuRegs.fpr[_Ft_].f);
	FNSTSWtoAX( );
	TEST32ItoR( EAX, 0x00004000 );
	j8Ptr[ 0 ] = JE8( 0 );
	OR32ItoM( (uptr)&fpuRegs.fprc[ 31 ], 0x00800000 );
	j8Ptr[ 1 ] = JMP8( 0 );

	x86SetJ8( j8Ptr[ 0 ] );
	AND32ItoM( (uptr)&fpuRegs.fprc[ 31 ], ~0x00800000 );

	x86SetJ8( j8Ptr[ 1 ] );	   
}

void recC_LT_(int info)
{
    SetFPUstate();

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FCOMP32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FNSTSWtoAX( );
	TEST32ItoR( EAX, 0x00000100 );
	j8Ptr[ 0 ] = JE8( 0 );
	OR32ItoM( (uptr)&fpuRegs.fprc[ 31 ], 0x00800000 );
	j8Ptr[ 1 ] = JMP8( 0 );

	x86SetJ8( j8Ptr[ 0 ] );
	AND32ItoM( (uptr)&fpuRegs.fprc[ 31 ], ~0x00800000 );

	x86SetJ8( j8Ptr[1] );
}

void recC_LE_(int info) 
{
    SetFPUstate();

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FCOMP32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FNSTSWtoAX( );
	TEST32ItoR( EAX, 0x00004100 );
	j8Ptr[ 0 ] = JE8( 0 );
	OR32ItoM( (uptr)&fpuRegs.fprc[ 31 ], 0x00800000 );
	j8Ptr[ 1 ] = JMP8( 0 );

	x86SetJ8( j8Ptr[ 0 ] );
	AND32ItoM( (uptr)&fpuRegs.fprc[ 31 ], ~0x00800000 );

	x86SetJ8( j8Ptr[ 1 ] );
}

void recADD_S_(int info) {
    SetFPUstate();

	SaveCW(1);
	FLD32(  (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FADD32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recSUB_S_(int info)
{
    SetFPUstate();
	SaveCW(1);

	FLD32(  (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FSUB32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recMUL_S_(int info)
{
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FMUL32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recDIV_S_(int info)
{
    SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FDIV32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	SetQFromStack( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recSQRT_S_(int info)
{
    SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSQRT( );
	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recABS_S_(int info)
{
	MOV32MtoR( EAX, (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	AND32ItoR( EAX, 0x7fffffff );
	MOV32RtoM( (uptr)&fpuRegs.fpr[ _Fd_ ].f, EAX );
}

void recMOV_S_(int info)
{
	MOV32MtoR( EAX, (uptr)&fpuRegs.fpr[ _Fs_ ].UL );
	MOV32RtoM( (uptr)&fpuRegs.fpr[ _Fd_ ].UL, EAX );
}

void recNEG_S_(int info)
{
	MOV32MtoR( EAX,(uptr)&fpuRegs.fpr[ _Fs_ ].f );
	XOR32ItoR( EAX, 0x80000000 );
	MOV32RtoM( (uptr)&fpuRegs.fpr[ _Fd_ ].f, EAX );
}

void recRSQRT_S_(int info)
{
	static u32 tmp;

	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSQRT( );
	FSTP32( (uptr)&tmp );

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FDIV32( (uptr)&tmp );
	SetQFromStack( (uptr)&fpuRegs.fpr[ _Fd_ ].f );

//	FLD32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
//	FSQRT( );
//	FSTP32( (uptr)&tmp );
//
//	MOV32MtoR( EAX, (uptr)&tmp );
//	OR32RtoR( EAX, EAX );
//	j8Ptr[ 0 ] = JE8( 0 );				
//	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
//	FDIV32( (uptr)&tmp );
//	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
//	x86SetJ8( j8Ptr[ 0 ] );
}

void recADDA_S_(int info) {
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FADD32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSTP32( (uptr)&fpuRegs.ACC.f );
}

void recSUBA_S_(int info)
{
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FSUB32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSTP32( (uptr)&fpuRegs.ACC.f );
}

void recMULA_S_(int info) {
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FMUL32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSTP32( (uptr)&fpuRegs.ACC.f );
}

void recMADD_S_(int info)
{
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FMUL32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FADD32( (uptr)&fpuRegs.ACC.f );
	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recMADDA_S_(int info)
{
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FMUL32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FADD32( (uptr)&fpuRegs.ACC.f );
	FSTP32( (uptr)&fpuRegs.ACC.f );
}

void recMSUB_S_(int info)
{
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.ACC.f );
	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FMUL32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSUBP( );
	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recMSUBA_S_(int info)
{
	SetFPUstate();
	SaveCW(1);

	FLD32( (uptr)&fpuRegs.ACC.f );
	FLD32( (uptr)&fpuRegs.fpr[ _Fs_ ].f );
	FMUL32( (uptr)&fpuRegs.fpr[ _Ft_ ].f );
	FSUBP( );
	FSTP32( (uptr)&fpuRegs.ACC.f );
}

void recCVT_S_(int info)
{
	SetFPUstate();
	FILD32( (uptr)&fpuRegs.fpr[ _Fs_ ].UL );
	FSTP32( (uptr)&fpuRegs.fpr[ _Fd_ ].f );
}

void recMAX_S_(int info)
{
	MOV32ItoM( (uptr)&cpuRegs.code, cpuRegs.code );
	MOV32ItoM( (uptr)&cpuRegs.pc, pc );
	iFlushCall(FLUSH_NODESTROY);
	CALLFunc( (u32)MAX_S );   
}

void recMIN_S_(int info) {
	MOV32ItoM( (uptr)&cpuRegs.code, cpuRegs.code ); 
	MOV32ItoM( (uptr)&cpuRegs.pc, pc );
	iFlushCall(FLUSH_NODESTROY);
	CALLFunc( (u32)MIN_S );
}

#endif

////////////////////////////////////////////////////
void recC_EQ_xmm(int info)
{
	// assumes that inputs are valid
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S: SSE_UCOMISS_M32_to_XMM(EEREC_S, (uptr)&fpuRegs.fpr[_Ft_]); break;
		case PROCESS_EE_T: SSE_UCOMISS_M32_to_XMM(EEREC_T, (uptr)&fpuRegs.fpr[_Fs_]); break;
		default: SSE_UCOMISS_XMM_to_XMM(EEREC_S, EEREC_T); break;
	}

	//write8(0x9f); // lahf
	//TEST16ItoR(EAX, 0x4400);
	j8Ptr[0] = JZ8(0);
	AND32ItoM( (uptr)&fpuRegs.fprc[31], ~0x00800000 );
	j8Ptr[1] = JMP8(0);
	x86SetJ8(j8Ptr[0]);
	OR32ItoM((uptr)&fpuRegs.fprc[31], 0x00800000);
	x86SetJ8(j8Ptr[1]);
}

FPURECOMPILE_CONSTCODE(C_EQ, XMMINFO_READS|XMMINFO_READT);

////////////////////////////////////////////////////
void recC_F()
{
	AND32ItoM( (uptr)&fpuRegs.fprc[31], ~0x00800000 );
}

////////////////////////////////////////////////////
void recC_LT_xmm(int info)
{
	// assumes that inputs are valid
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S: SSE_UCOMISS_M32_to_XMM(EEREC_S, (uptr)&fpuRegs.fpr[_Ft_]); break;
		case PROCESS_EE_T: SSE_UCOMISS_M32_to_XMM(EEREC_T, (uptr)&fpuRegs.fpr[_Fs_]); break;
		default: SSE_UCOMISS_XMM_to_XMM(EEREC_S, EEREC_T); break;
	}

	//write8(0x9f); // lahf
	//TEST16ItoR(EAX, 0x4400);
	if( info & PROCESS_EE_S ) {
		j8Ptr[0] = JB8(0);
		AND32ItoM( (uptr)&fpuRegs.fprc[31], ~0x00800000 );
		j8Ptr[1] = JMP8(0);
		x86SetJ8(j8Ptr[0]);
		OR32ItoM((uptr)&fpuRegs.fprc[31], 0x00800000);
		x86SetJ8(j8Ptr[1]);
	}
	else {
		j8Ptr[0] = JBE8(0);
		OR32ItoM((uptr)&fpuRegs.fprc[31], 0x00800000);
		j8Ptr[1] = JMP8(0);
		x86SetJ8(j8Ptr[0]);
		AND32ItoM( (uptr)&fpuRegs.fprc[31], ~0x00800000 );
		x86SetJ8(j8Ptr[1]);
	}
}

FPURECOMPILE_CONSTCODE(C_LT, XMMINFO_READS|XMMINFO_READT);

////////////////////////////////////////////////////
void recC_LE_xmm(int info )
{
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S: SSE_UCOMISS_M32_to_XMM(EEREC_S, (uptr)&fpuRegs.fpr[_Ft_]); break;
		case PROCESS_EE_T: SSE_UCOMISS_M32_to_XMM(EEREC_T, (uptr)&fpuRegs.fpr[_Fs_]); break;
		default: SSE_UCOMISS_XMM_to_XMM(EEREC_S, EEREC_T); break;
	}

	//write8(0x9f); // lahf
	//TEST16ItoR(EAX, 0x4400);
	if( info & PROCESS_EE_S ) {
		j8Ptr[0] = JBE8(0);
		AND32ItoM( (uptr)&fpuRegs.fprc[31], ~0x00800000 );
		j8Ptr[1] = JMP8(0);
		x86SetJ8(j8Ptr[0]);
		OR32ItoM((uptr)&fpuRegs.fprc[31], 0x00800000);
		x86SetJ8(j8Ptr[1]);
	}
	else {
		j8Ptr[0] = JB8(0);
		OR32ItoM((uptr)&fpuRegs.fprc[31], 0x00800000);
		j8Ptr[1] = JMP8(0);
		x86SetJ8(j8Ptr[0]);
		AND32ItoM( (uptr)&fpuRegs.fprc[31], ~0x00800000 );
		x86SetJ8(j8Ptr[1]);
	}
}

FPURECOMPILE_CONSTCODE(C_LE, XMMINFO_READS|XMMINFO_READT);

////////////////////////////////////////////////////
static void (*recComOpXMM_to_XMM[] )(x86SSERegType, x86SSERegType) = {
	SSE_ADDSS_XMM_to_XMM, SSE_MULSS_XMM_to_XMM, SSE_MAXSS_XMM_to_XMM, SSE_MINSS_XMM_to_XMM };

static void (*recComOpM32_to_XMM[] )(x86SSERegType, uptr) = {
	SSE_ADDSS_M32_to_XMM, SSE_MULSS_M32_to_XMM, SSE_MAXSS_M32_to_XMM, SSE_MINSS_M32_to_XMM };

void recCommutativeOp(int info, int regd, int op) {
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if (regd == EEREC_S) recComOpM32_to_XMM[op](regd, (uptr)&fpuRegs.fpr[_Ft_]);
			else {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				recComOpM32_to_XMM[op](regd, (uptr)&fpuRegs.fpr[_Ft_]);
			}
			break;
		case PROCESS_EE_T:
			if (regd == EEREC_T) recComOpM32_to_XMM[op](regd, (uptr)&fpuRegs.fpr[_Fs_]);
			else {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_T);
				recComOpM32_to_XMM[op](regd, (uptr)&fpuRegs.fpr[_Fs_]);
			}
			break;
		default:
			if (regd == EEREC_S) {
				recComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			else if (regd == EEREC_T) {
				recComOpXMM_to_XMM[op](regd, EEREC_S);
			}
			else {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				recComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			break;
	}
}

static void (*recNonComOpXMM_to_XMM[] )(x86SSERegType, x86SSERegType) = {
	SSE_SUBSS_XMM_to_XMM, SSE_DIVSS_XMM_to_XMM };

static void (*recNonComOpM32_to_XMM[] )(x86SSERegType, uptr) = {
	SSE_SUBSS_M32_to_XMM, SSE_DIVSS_M32_to_XMM };

int recNonCommutativeOp(int info, int regd, int op)
{
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if (regd != EEREC_S) SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
			recNonComOpM32_to_XMM[op](regd, (uptr)&fpuRegs.fpr[_Ft_]);
			break;
		case PROCESS_EE_T:
			if (regd == EEREC_T) {
				int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
				SSE_MOVSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Fs_]);
				recNonComOpXMM_to_XMM[op](t0reg, EEREC_T);

				// swap regs
				xmmregs[t0reg] = xmmregs[regd];
				xmmregs[regd].inuse = 0;
				return t0reg;
			}
			else {
				SSE_MOVSS_M32_to_XMM(regd, (uptr)&fpuRegs.fpr[_Fs_]);
				recNonComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			break;
		default:
			if (regd == EEREC_S) {
				recNonComOpXMM_to_XMM[op](regd, EEREC_T);
			} else
			if (regd == EEREC_T) {
				int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
				SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_S);
				recNonComOpXMM_to_XMM[op](t0reg, regd);

				// swap regs
				xmmregs[t0reg] = xmmregs[regd];
				xmmregs[regd].inuse = 0;
				return t0reg;
			}
			else {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				recNonComOpXMM_to_XMM[op](regd, EEREC_T);
			}
			break;
	}

	return regd;
}

void recADD_S_xmm(int info) {
	recCommutativeOp(info, EEREC_D, 0);
}

FPURECOMPILE_CONSTCODE(ADD_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

////////////////////////////////////////////////////
void recSUB_S_xmm(int info)
{
	recNonCommutativeOp(info, EEREC_D, 0);
}

FPURECOMPILE_CONSTCODE(SUB_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

////////////////////////////////////////////////////
void recMUL_S_xmm(int info)
{
	recCommutativeOp(info, EEREC_D, 1);
}

FPURECOMPILE_CONSTCODE(MUL_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

////////////////////////////////////////////////////
void recDIV_S_xmm(int info)
{
    int t0reg = _allocTempXMMreg(XMMT_FPS, -1);				
    int regd = recNonCommutativeOp(info, EEREC_D, 1);
//	SSE_MAXSS_M32_to_XMM(regd, (uptr)&g_minvals[0]);
//	SSE_MINSS_M32_to_XMM(regd, (uptr)&g_maxvals[0]);

    // katamari needs this
    SSE_XORPS_XMM_to_XMM(t0reg, t0reg);
	SSE_CMPORDSS_XMM_to_XMM(t0reg, regd);
	SSE_ANDPS_XMM_to_XMM(regd, t0reg);

    _freeXMMreg(t0reg);
	
//	_freeXMMreg(EEREC_D);
//	MOV32MtoR(EAX, (uptr)&fpuRegs.fpr[_Fd_]);
//	AND32ItoR(EAX, 0x7f800000);
//	CMP32ItoR(EAX, 0x7f800000);
//	j8Ptr[0] = JNE8(0);
//	MOV32ItoM((uptr)&fpuRegs.fpr[_Fd_], 0);
//	x86SetJ8(j8Ptr[0]);
}

FPURECOMPILE_CONSTCODE(DIV_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

static u32 PCSX2_ALIGNED16(s_neg[4]) = { 0x80000000, 0, 0, 0 };
static u32 PCSX2_ALIGNED16(s_pos[4]) = { 0x7fffffff, 0, 0, 0 };

void recSQRT_S_xmm(int info)
{
	if( info & PROCESS_EE_T ) {
		if( CHECK_FORCEABS ) {
			if( EEREC_D == EEREC_T ) SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&s_pos[0]);
			else {
				SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&s_pos[0]);
				SSE_ANDPS_XMM_to_XMM(EEREC_D, EEREC_T);
			}

			SSE_SQRTSS_XMM_to_XMM(EEREC_D, EEREC_D);
		}
		else {
			SSE_SQRTSS_XMM_to_XMM(EEREC_D, EEREC_T);
		}
	}
	else {
		if( CHECK_FORCEABS ) {
			SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Ft_]);
			SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&s_pos[0]);

			SSE_SQRTSS_XMM_to_XMM(EEREC_D, EEREC_D);
		}
		else {
			SSE_SQRTSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Ft_]);
		}
	}
}

FPURECOMPILE_CONSTCODE(SQRT_S, XMMINFO_WRITED|XMMINFO_READT);

void recABS_S_xmm(int info)
{
	if( info & PROCESS_EE_S ) {
		if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
		SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&s_pos[0]);
	}
	else {
		if( _Fs_ == _Fd_ ) {
			AND32ItoM((uptr)&fpuRegs.fpr[_Fs_], 0x7fffffff);
			SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
			xmmregs[EEREC_D].mode &= ~MODE_WRITE;
		}
		else {
			SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
			SSE_ANDPS_M128_to_XMM(EEREC_D, (uptr)&s_pos[0]);
		}
	}
}

FPURECOMPILE_CONSTCODE(ABS_S, XMMINFO_WRITED|XMMINFO_READS);

void recMOV_S_xmm(int info)
{
	if( info & PROCESS_EE_S ) {
		if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
	}
	else {
		SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
	}
}

FPURECOMPILE_CONSTCODE(MOV_S, XMMINFO_WRITED|XMMINFO_READS);

void recNEG_S_xmm(int info) {
	if( info & PROCESS_EE_S ) {
		if( EEREC_D != EEREC_S ) SSE_MOVSS_XMM_to_XMM(EEREC_D, EEREC_S);
	}
	else {
		SSE_MOVSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
	}

	SSE_XORPS_M128_to_XMM(EEREC_D, (uptr)&s_neg[0]);
}

FPURECOMPILE_CONSTCODE(NEG_S, XMMINFO_WRITED|XMMINFO_READS);

void recRSQRT_S_xmm(int info)
{
	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
			if( EEREC_D == EEREC_S ) {
				int t0reg = _allocTempXMMreg(XMMT_FPS, -1);
				SSE_RSQRTSS_M32_to_XMM(t0reg, (uptr)&fpuRegs.fpr[_Ft_]);
				SSE_MULSS_XMM_to_XMM(EEREC_D, t0reg);
				_freeXMMreg(t0reg);
			}
			else {
				SSE_RSQRTSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Ft_]);
				SSE_MULSS_XMM_to_XMM(EEREC_D, EEREC_S);
			}

			break;
		case PROCESS_EE_T:
			SSE_RSQRTSS_XMM_to_XMM(EEREC_D, EEREC_T);
			SSE_MULSS_M32_to_XMM(EEREC_D, (uptr)&fpuRegs.fpr[_Fs_]);
			break;
		default:
			if( EEREC_D == EEREC_S ) {
				int t0reg;
				if( g_pCurInstInfo->regs[_Ft_]&EEINST_LASTUSE ) {
					_freeXMMreg(EEREC_T);
					t0reg = EEREC_T;
				}
				else {
					t0reg = _allocTempXMMreg(XMMT_FPS, -1);
					_freeXMMreg(t0reg);
				}

				SSE_RSQRTSS_XMM_to_XMM(t0reg, EEREC_T);
				SSE_MULSS_XMM_to_XMM(EEREC_D, t0reg);
			}
			else {
				SSE_RSQRTSS_XMM_to_XMM(EEREC_D, EEREC_T);
				SSE_MULSS_XMM_to_XMM(EEREC_D, EEREC_S);
			}
			break;
	}

	SSE_MAXSS_M32_to_XMM(EEREC_D, (uptr)&g_minvals[0]);
	SSE_MINSS_M32_to_XMM(EEREC_D, (uptr)&g_maxvals[0]);
}

FPURECOMPILE_CONSTCODE(RSQRT_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recADDA_S_xmm(int info)
{
	recCommutativeOp(info, EEREC_ACC, 0);
}

FPURECOMPILE_CONSTCODE(ADDA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);

void recSUBA_S_xmm(int info) {
	recNonCommutativeOp(info, EEREC_ACC, 0);
}

FPURECOMPILE_CONSTCODE(SUBA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);

void recMULA_S_xmm(int info) {
	recCommutativeOp(info, EEREC_ACC, 1);
}

FPURECOMPILE_CONSTCODE(MULA_S, XMMINFO_WRITEACC|XMMINFO_READS|XMMINFO_READT);

void recMADDtemp(int info, int regd)
{
	int vreg;
	u32 mreg;

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
		case PROCESS_EE_T:
			vreg = (info&PROCESS_EE_S)?EEREC_S:EEREC_T;
			mreg = (info&PROCESS_EE_S)?(uptr)&fpuRegs.fpr[_Ft_]:(uptr)&fpuRegs.fpr[_Fs_];

			if (regd == vreg) {
				SSE_MULSS_M32_to_XMM(regd, mreg);
				if( info & PROCESS_EE_ACC ) SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				else SSE_ADDSS_M32_to_XMM(regd, (uptr)&fpuRegs.ACC);
			}
			else if( (info&PROCESS_EE_ACC) && regd == EEREC_ACC ) {
				int t0reg;
				if( g_pCurInstInfo->regs[(info&PROCESS_EE_S)?_Fs_:_Ft_] & EEINST_LASTUSE ) {
					_freeXMMreg(vreg);
					t0reg = vreg;
					SSE_MULSS_M32_to_XMM(t0reg, mreg);
				}
				else {
					t0reg = _allocTempXMMreg(XMMT_FPS, -1);
					SSE_MOVSS_XMM_to_XMM(t0reg, vreg);
					SSE_MULSS_M32_to_XMM(vreg, mreg);
					
					// swap regs for better timing
					xmmregs[t0reg] = xmmregs[vreg];
					xmmregs[vreg].inuse = 0;

					t0reg = vreg;
				}

				SSE_ADDSS_XMM_to_XMM(regd, t0reg);
			}
			else {
				SSE_MOVSS_M32_to_XMM(regd, mreg);
				SSE_MULSS_XMM_to_XMM(regd, vreg);
				if( info & PROCESS_EE_ACC ) SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				else SSE_ADDSS_M32_to_XMM(regd, (uptr)&fpuRegs.ACC);
			}
			break;
		default:
			if (regd == EEREC_S) {
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if( info & PROCESS_EE_ACC ) {
					assert( regd != EEREC_ACC );
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else SSE_ADDSS_M32_to_XMM(regd, (uptr)&fpuRegs.ACC);
			}
			else if (regd == EEREC_T) {
				SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
				if( info & PROCESS_EE_ACC ) {
					assert( regd != EEREC_ACC );
					SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				}
				else SSE_ADDSS_M32_to_XMM(regd, (uptr)&fpuRegs.ACC);
			}
			else if( (info&PROCESS_EE_ACC) && regd == EEREC_ACC ) {
				int t0reg ;
				if( g_pCurInstInfo->regs[_Fs_] & EEINST_LASTUSE ) {
					_freeXMMreg(EEREC_S);
					t0reg = EEREC_S;
					SSE_MULSS_XMM_to_XMM(t0reg, EEREC_T);
				}
				else if( g_pCurInstInfo->regs[_Ft_] & EEINST_LASTUSE ) {
					_freeXMMreg(EEREC_T);
					t0reg = EEREC_T;
					SSE_MULSS_XMM_to_XMM(t0reg, EEREC_S);
				}
				else {
					t0reg = _allocTempXMMreg(XMMT_FPS, -1);
					SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_S);
					SSE_MULSS_XMM_to_XMM(EEREC_S, EEREC_T);
					
					// swap regs for better timing
					xmmregs[t0reg] = xmmregs[EEREC_S];
					xmmregs[EEREC_S].inuse = 0;

					t0reg = EEREC_S;
				}

				SSE_ADDSS_XMM_to_XMM(regd, t0reg);
			}
			else {
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				if( info & PROCESS_EE_ACC ) SSE_ADDSS_XMM_to_XMM(regd, EEREC_ACC);
				else SSE_ADDSS_M32_to_XMM(regd, (uptr)&fpuRegs.ACC);
			}
			break;
	}
}

void recMADD_S_xmm(int info)
{
	recMADDtemp(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(MADD_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMADDA_S_xmm(int info)
{
	recMADDtemp(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(MADDA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMSUBtemp(int info, int regd)
{
	int vreg;
	u32 mreg;

	if( !(info&PROCESS_EE_ACC)) {
		int regacc = _allocFPACCtoXMMreg(-1, MODE_WRITE|MODE_READ);
		info |= PROCESS_EE_SET_ACC(regacc)|PROCESS_EE_ACC;
	}

	switch(info & (PROCESS_EE_S|PROCESS_EE_T) ) {
		case PROCESS_EE_S:
		case PROCESS_EE_T:
			vreg = (info&PROCESS_EE_S)?EEREC_S:EEREC_T;
			mreg = (info&PROCESS_EE_S)?(uptr)&fpuRegs.fpr[_Ft_]:(uptr)&fpuRegs.fpr[_Fs_];

			if (regd == vreg) {
				assert( regd != EEREC_ACC );
				_freeXMMreg(EEREC_ACC);
				SSE_MULSS_M32_to_XMM(regd, mreg);
				SSE_SUBSS_XMM_to_XMM(EEREC_ACC, regd);
				xmmregs[EEREC_ACC] = xmmregs[regd];
				xmmregs[regd].inuse = 0;
			}
			else if( regd == EEREC_ACC ) {
				int t0reg;
				if( g_pCurInstInfo->regs[(info&PROCESS_EE_S)?_Fs_:_Ft_] & EEINST_LASTUSE ) {
					_freeXMMreg(vreg);
					t0reg = vreg;
					SSE_MULSS_M32_to_XMM(t0reg, mreg);
				}
				else {
					t0reg = _allocTempXMMreg(XMMT_FPS, -1);
					SSE_MOVSS_XMM_to_XMM(t0reg, vreg);
					SSE_MULSS_M32_to_XMM(vreg, mreg);
					
					// swap regs for better timing
					xmmregs[t0reg] = xmmregs[vreg];
					xmmregs[vreg].inuse = 0;

					t0reg = vreg;
				}

				SSE_SUBSS_XMM_to_XMM(regd, t0reg);
			}
			else {
				if( regd != EEREC_ACC ) {
					_freeXMMreg(EEREC_ACC);
					xmmregs[EEREC_ACC] = xmmregs[regd];
					xmmregs[regd].inuse = 0;
				}
				SSE_MOVSS_M32_to_XMM(regd, mreg);
				SSE_MULSS_XMM_to_XMM(regd, vreg);
				SSE_SUBSS_XMM_to_XMM(EEREC_ACC, regd);
			}

			break;
		default:
			if (regd == EEREC_S) {
				assert( regd != EEREC_ACC );
				_freeXMMreg(EEREC_ACC);
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				SSE_SUBSS_XMM_to_XMM(EEREC_ACC, regd);
				xmmregs[EEREC_ACC] = xmmregs[regd];
				xmmregs[regd].inuse = 0;
			}
			else if (regd == EEREC_T) {
				assert( regd != EEREC_ACC );
				_freeXMMreg(EEREC_ACC);
				SSE_MULSS_XMM_to_XMM(regd, EEREC_S);
				SSE_SUBSS_XMM_to_XMM(EEREC_ACC, regd);
				xmmregs[EEREC_ACC] = xmmregs[regd];
				xmmregs[regd].inuse = 0;
			}
			else if( regd == EEREC_ACC ) {
				int t0reg ;
				if( g_pCurInstInfo->regs[_Fs_] & EEINST_LASTUSE ) {
					_freeXMMreg(EEREC_S);
					t0reg = EEREC_S;
					SSE_MULSS_XMM_to_XMM(t0reg, EEREC_T);
				}
				else if( g_pCurInstInfo->regs[_Ft_] & EEINST_LASTUSE ) {
					_freeXMMreg(EEREC_T);
					t0reg = EEREC_T;
					SSE_MULSS_XMM_to_XMM(t0reg, EEREC_S);
				}
				else {
					t0reg = _allocTempXMMreg(XMMT_FPS, -1);
					SSE_MOVSS_XMM_to_XMM(t0reg, EEREC_S);
					SSE_MULSS_XMM_to_XMM(EEREC_S, EEREC_T);
					
					// swap regs for better timing
					xmmregs[t0reg] = xmmregs[EEREC_S];
					xmmregs[EEREC_S].inuse = 0;

					t0reg = EEREC_S;
				}

				SSE_SUBSS_XMM_to_XMM(regd, t0reg);
			}
			else {
				if( regd != EEREC_ACC ) {
					_freeXMMreg(EEREC_ACC);
					xmmregs[EEREC_ACC] = xmmregs[regd];
					xmmregs[regd].inuse = 0;
				}
				SSE_MOVSS_XMM_to_XMM(regd, EEREC_S);
				SSE_MULSS_XMM_to_XMM(regd, EEREC_T);
				SSE_SUBSS_XMM_to_XMM(EEREC_ACC, regd);
			}
			break;
	}
}

void recMSUB_S_xmm(int info)
{
	recMSUBtemp(info, EEREC_D);
}

FPURECOMPILE_CONSTCODE(MSUB_S, XMMINFO_WRITED|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

void recMSUBA_S_xmm(int info)
{
	recMSUBtemp(info, EEREC_ACC);
}

FPURECOMPILE_CONSTCODE(MSUBA_S, XMMINFO_WRITEACC|XMMINFO_READACC|XMMINFO_READS|XMMINFO_READT);

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

////////////////////////////////////////////////////
void recCVT_W() 
{
	if( cpucaps.hasStreamingSIMDExtensions ) {
		int t0reg;
		int regs = _checkXMMreg(XMMTYPE_FPREG, _Fs_, MODE_READ);

		if( regs >= 0 ) {
			t0reg = _allocTempXMMreg(XMMT_FPS, -1);
			_freeXMMreg(t0reg);
			SSE_XORPS_XMM_to_XMM(t0reg, t0reg);
			SSE_CVTTSS2SI_XMM_to_R32(EAX, regs);
		}
		else SSE_CVTTSS2SI_M32_to_R32(EAX, (uptr)&fpuRegs.fpr[ _Fs_ ]);

		_deleteFPtoXMMreg(_Fd_, 2);

		CMP32ItoR(EAX, 0x80000000);
		j8Ptr[0] = JNE8(0);

		// need to detect if reg is positive
		if( regs >= 0 ) {
			SSE_UCOMISS_XMM_to_XMM(regs, t0reg);
			j8Ptr[2] = JB8(0);
		}
		else {
			TEST32ItoM((uptr)&fpuRegs.fpr[_Fs_], 0x80000000);
			j8Ptr[2] = JNZ8(0);
		}

		MOV32ItoM((uptr)&fpuRegs.fpr[_Fd_], 0x7fffffff);
		j8Ptr[1] = JMP8(0);

		x86SetJ8( j8Ptr[0] );
		x86SetJ8( j8Ptr[2] );
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

void recMAX_S_xmm(int info)
{
	recCommutativeOp(info, EEREC_D, 2);
}

FPURECOMPILE_CONSTCODE(MAX_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

void recMIN_S_xmm(int info)
{
	recCommutativeOp(info, EEREC_D, 3);
}

FPURECOMPILE_CONSTCODE(MIN_S, XMMINFO_WRITED|XMMINFO_READS|XMMINFO_READT);

////////////////////////////////////////////////////
void recBC1F( void ) {
	u32 branchTo = (s32)_Imm_ * 4 + pc;
	
	_eeFlushAllUnused();
	MOV32MtoR(EAX, (uptr)&fpuRegs.fprc[31]);
	TEST32ItoR(EAX, 0x00800000);
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
	TEST32ItoR(EAX, 0x00800000);
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

////////////////////////////////////////////////////
void recBC1FL( void ) {
	u32 branchTo = _Imm_ * 4 + pc;

	_eeFlushAllUnused();
	MOV32MtoR(EAX, (uptr)&fpuRegs.fprc[31]);
	TEST32ItoR(EAX, 0x00800000);
	j32Ptr[0] = JNZ32(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);

	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}

////////////////////////////////////////////////////
void recBC1TL( void ) {
	u32 branchTo = _Imm_ * 4 + pc;

	_eeFlushAllUnused();
	MOV32MtoR(EAX, (uptr)&fpuRegs.fprc[31]);
	TEST32ItoR(EAX, 0x00800000);
	j32Ptr[0] = JZ32(0);

	SaveBranchState();
	recompileNextInstruction(1);
	SetBranchImm(branchTo);
	x86SetJ32(j32Ptr[0]);

	LoadBranchState();
	SetBranchImm(pc);
}

#endif

#endif // PCSX2_NORECBUILD
