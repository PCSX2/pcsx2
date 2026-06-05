// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU Micromode Lower instructions
//------------------------------------------------------------------

//------------------------------------------------------------------
// DIV/SQRT/RSQRT
//------------------------------------------------------------------

// Test if Vector is +/- Zero
static __fi void testZero(const xmm& xmmReg, const xmm& xmmTemp, const x32& gprTemp)
{
//	xXOR.PS(xmmTemp, xmmTemp);
    armAsm->Eor(xmmTemp.V16B(), xmmTemp.V16B(), xmmTemp.V16B());
//	xCMPEQ.SS(xmmTemp, xmmReg);
    armAsm->Fcmeq(xmmTemp.S(), xmmTemp.S(), xmmReg.S());
//	xPTEST(xmmTemp, xmmTemp);
    armAsm->Fcmp(xmmTemp.S(), 0.0);
}

// Test if Vector is Negative (Set Flags and Makes Positive)
static __fi void testNeg(mV, const xmm& xmmReg, const x32& gprTemp)
{
//	xMOVMSKPS(gprTemp, xmmReg);
    armMOVMSKPS(gprTemp, xmmReg);
//	xTEST(gprTemp, 1);
    armAsm->Tst(gprTemp, 1);
//	xForwardJZ8 skip;
    a64::Label skip;
    armAsm->B(&skip, a64::Condition::eq);
//		xMOV(ptr32[&mVU.divFlag], divI);
        armStorePtr(divI, PTR_MVU(microVU[mVU.index].divFlag));
//		xAND.PS(xmmReg, ptr128[mVUglob.absclip]);
        armAsm->And(xmmReg.V16B(), xmmReg.V16B(), armLoadPtrV(PTR_CPU(mVUglob.absclip)).V16B());
//	skip.SetTarget();
    armBind(&skip);
}

mVUop(mVU_DIV)
{
	pass1 { mVUanalyzeFDIV(mVU, _Fs_, _Fsf_, _Ft_, _Ftf_, 7); }
	pass2
	{
		xmm Ft;
		if (_Ftf_) Ft = mVU.regAlloc->allocReg(_Ft_, 0, (1 << (3 - _Ftf_)));
		else       Ft = mVU.regAlloc->allocReg(_Ft_);
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const xmm& t1 = mVU.regAlloc->allocReg();

		testZero(Ft, t1, gprT1); // Test if Ft is zero
//		xForwardJZ8 cjmp; // Skip if not zero
        a64::Label cjmp;
        armAsm->B(&cjmp, a64::Condition::eq);

			testZero(Fs, t1, gprT1); // Test if Fs is zero
//			xForwardJZ8 ajmp;
            a64::Label ajmp;
            armAsm->B(&ajmp, a64::Condition::eq);
//				xMOV(ptr32[&mVU.divFlag], divI); // Set invalid flag (0/0)
                armStorePtr(divI, PTR_MVU(microVU[mVU.index].divFlag));
//				xForwardJump8 bjmp;
                a64::Label bjmp;
                armAsm->B(&bjmp);
//			ajmp.SetTarget();
            armBind(&ajmp);
//				xMOV(ptr32[&mVU.divFlag], divD); // Zero divide (only when not 0/0)
                armStorePtr(divD, PTR_MVU(microVU[mVU.index].divFlag));
//			bjmp.SetTarget();
            armBind(&bjmp);

//			xXOR.PS(Fs, Ft);
            armAsm->Eor(Fs.V16B(), Fs.V16B(), Ft.V16B());
//			xAND.PS(Fs, ptr128[mVUglob.signbit]);
            armAsm->And(Fs.V16B(), Fs.V16B(), armLoadPtrV(PTR_CPU(mVUglob.signbit)).V16B());
//			xOR.PS (Fs, ptr128[mVUglob.maxvals]); // If division by zero, then xmmFs = +/- fmax
            armAsm->Orr(Fs.V16B(), Fs.V16B(), armLoadPtrV(PTR_CPU(mVUglob.maxvals)).V16B());

//			xForwardJump8 djmp;
            a64::Label djmp;
            armAsm->B(&djmp);
//		cjmp.SetTarget();
        armBind(&cjmp);
//			xMOV(ptr32[&mVU.divFlag], 0); // Clear I/D flags
            armStorePtr(0, PTR_MVU(microVU[mVU.index].divFlag));
			SSE_DIVSS(mVU, Fs, Ft);
			mVUclamp1(mVU, Fs, t1, 8, true);
//		djmp.SetTarget();
        armBind(&djmp);

		writeQreg(Fs, mVUinfo.writeQ);

		if (mVU.cop2)
		{
//			xAND(gprF0, ~0xc0000);
            armAsm->And(gprF0, gprF0, ~0xc0000);
//			xOR(gprF0, ptr32[&mVU.divFlag]);
            armAsm->Orr(gprF0, gprF0, armLoadPtr(PTR_MVU(microVU[mVU.index].divFlag)));
		}

		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(t1);
		mVU.profiler.EmitOp(opDIV);
	}
	pass3 { mVUlog("DIV Q, vf%02d%s, vf%02d%s", _Fs_, _Fsf_String, _Ft_, _Ftf_String); }
}

mVUop(mVU_SQRT)
{
	pass1 { mVUanalyzeFDIV(mVU, 0, 0, _Ft_, _Ftf_, 7); }
	pass2
	{
		const xmm& Ft = mVU.regAlloc->allocReg(_Ft_, 0, (1 << (3 - _Ftf_)));

//		xMOV(ptr32[&mVU.divFlag], 0); // Clear I/D flags
        armStorePtr(0, PTR_MVU(microVU[mVU.index].divFlag));
		testNeg(mVU, Ft, gprT1); // Check for negative sqrt

		if (CHECK_VU_OVERFLOW(mVU.index)) { // Clamp infinities (only need to do positive clamp since xmmFt is positive)
//            xMIN.SS(Ft, ptr32[mVUglob.maxvals]);
            armAsm->Umin(Ft.V4S(), Ft.V4S(), armLoadPtrV(PTR_CPU(mVUglob.maxvals)).V4S());
        }
//		xSQRT.SS(Ft, Ft);
        armAsm->Fsqrt(Ft.S(), Ft.S());
		writeQreg(Ft, mVUinfo.writeQ);

		if (mVU.cop2)
		{
//			xAND(gprF0, ~0xc0000);
            armAsm->And(gprF0, gprF0, ~0xc0000);
//			xOR(gprF0, ptr32[&mVU.divFlag]);
            armAsm->Orr(gprF0, gprF0, armLoadPtr(PTR_MVU(microVU[mVU.index].divFlag)));
		}

		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opSQRT);
	}
	pass3 { mVUlog("SQRT Q, vf%02d%s", _Ft_, _Ftf_String); }
}

mVUop(mVU_RSQRT)
{
	pass1 { mVUanalyzeFDIV(mVU, _Fs_, _Fsf_, _Ft_, _Ftf_, 13); }
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const xmm& Ft = mVU.regAlloc->allocReg(_Ft_, 0, (1 << (3 - _Ftf_)));
		const xmm& t1 = mVU.regAlloc->allocReg();

//		xMOV(ptr32[&mVU.divFlag], 0); // Clear I/D flags
        armStorePtr(0, PTR_MVU(microVU[mVU.index].divFlag));
		testNeg(mVU, Ft, gprT1); // Check for negative sqrt

//		xSQRT.SS(Ft, Ft);
        armAsm->Fsqrt(Ft.S(), Ft.S());
		testZero(Ft, t1, gprT1); // Test if Ft is zero
//		xForwardJZ8 ajmp; // Skip if not zero
        a64::Label ajmp;
        armAsm->B(&ajmp, a64::Condition::eq);

			testZero(Fs, t1, gprT1); // Test if Fs is zero
//			xForwardJZ8 bjmp; // Skip if none are
            a64::Label bjmp;
            armAsm->B(&bjmp, a64::Condition::eq);
//				xMOV(ptr32[&mVU.divFlag], divI); // Set invalid flag (0/0)
                armStorePtr(divI, PTR_MVU(microVU[mVU.index].divFlag));
//				xForwardJump8 cjmp;
                a64::Label cjmp;
                armAsm->B(&cjmp);
//			bjmp.SetTarget();
            armBind(&bjmp);
//				xMOV(ptr32[&mVU.divFlag], divD); // Zero divide flag (only when not 0/0)
                armStorePtr(divD, PTR_MVU(microVU[mVU.index].divFlag));
//			cjmp.SetTarget();
            armBind(&cjmp);

//			xAND.PS(Fs, ptr128[mVUglob.signbit]);
            armAsm->And(Fs.V16B(), Fs.V16B(), armLoadPtrV(PTR_CPU(mVUglob.signbit)).V16B());
//			xOR.PS(Fs, ptr128[mVUglob.maxvals]); // xmmFs = +/-Max
            armAsm->Orr(Fs.V16B(), Fs.V16B(), armLoadPtrV(PTR_CPU(mVUglob.maxvals)).V16B());

//			xForwardJump8 djmp;
            a64::Label djmp;
            armAsm->B(&djmp);
//		ajmp.SetTarget();
        armBind(&ajmp);
			SSE_DIVSS(mVU, Fs, Ft);
			mVUclamp1(mVU, Fs, t1, 8, true);
//		djmp.SetTarget();
        armBind(&djmp);

		writeQreg(Fs, mVUinfo.writeQ);

		if (mVU.cop2)
		{
//			xAND(gprF0, ~0xc0000);
            armAsm->And(gprF0, gprF0, ~0xc0000);
//			xOR(gprF0, ptr32[&mVU.divFlag]);
            armAsm->Orr(gprF0, gprF0, armLoadPtr(PTR_MVU(microVU[mVU.index].divFlag)));
		}

		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(t1);
		mVU.profiler.EmitOp(opRSQRT);
	}
	pass3 { mVUlog("RSQRT Q, vf%02d%s, vf%02d%s", _Fs_, _Fsf_String, _Ft_, _Ftf_String); }
}

//------------------------------------------------------------------
// EATAN/EEXP/ELENG/ERCPR/ERLENG/ERSADD/ERSQRT/ESADD/ESIN/ESQRT/ESUM
//------------------------------------------------------------------

#define EATANhelper(addr) \
	{ \
		SSE_MULSS(mVU, t2, Fs); \
		SSE_MULSS(mVU, t2, Fs); \
		armAsm->Mov(t1, t2);    \
		armAsm->Fmul(t1.S(), t1.S(), armLoadPtrV(addr).S()); \
		SSE_ADDSS(mVU, PQ, t1); \
	}

// ToDo: Can Be Optimized Further? (takes approximately (~115 cycles + mem access time) on a c2d)
static __fi void mVU_EATAN_(mV, const xmm& PQ, const xmm& Fs, const xmm& t1, const xmm& t2)
{
//	xMOVSS(PQ, Fs);
    armAsm->Mov(PQ.S(), 0, Fs.S(), 0);
//	xMUL.SS(PQ, ptr32[mVUglob.T1]);
    armAsm->Fmul(PQ.S(), PQ.S(), armLoadPtrV(PTR_CPU(mVUglob.T1)).S());
//	xMOVAPS(t2, Fs);
    armAsm->Mov(t2.Q(), Fs.Q());
	EATANhelper(PTR_CPU(mVUglob.T2));
	EATANhelper(PTR_CPU(mVUglob.T3));
	EATANhelper(PTR_CPU(mVUglob.T4));
	EATANhelper(PTR_CPU(mVUglob.T5));
	EATANhelper(PTR_CPU(mVUglob.T6));
	EATANhelper(PTR_CPU(mVUglob.T7));
	EATANhelper(PTR_CPU(mVUglob.T8));
//	xADD.SS(PQ, ptr32[mVUglob.Pi4]);
    armAsm->Fadd(PQ.S(), PQ.S(), armLoadPtrV(PTR_CPU(mVUglob.Pi4)).S());
//	xPSHUF.D(PQ, PQ, mVUinfo.writeP ? 0x27 : 0xC6);
    armPSHUFD(PQ, PQ, mVUinfo.writeP ? 0x27 : 0xC6);
}

mVUop(mVU_EATAN)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU1(mVU, _Fs_, _Fsf_, 54);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const xmm& t1 = mVU.regAlloc->allocReg();
		const xmm& t2 = mVU.regAlloc->allocReg();
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xMOVSS (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xSUB.SS(Fs,    ptr32[mVUglob.one]);
        armAsm->Fsub(Fs.S(), Fs.S(), armLoadPtrV(PTR_CPU(mVUglob.one)).S());
//		xADD.SS(xmmPQ, ptr32[mVUglob.one]);
        armAsm->Fadd(xmmPQ.S(), xmmPQ.S(), armLoadPtrV(PTR_CPU(mVUglob.one)).S());
		SSE_DIVSS(mVU, Fs, xmmPQ);
		mVU_EATAN_(mVU, xmmPQ, Fs, t1, t2);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEATAN);
	}
	pass3 { mVUlog("EATAN P"); }
}

mVUop(mVU_EATANxy)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU2(mVU, _Fs_, 54);
	}
	pass2
	{
		const xmm& t1 = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const xmm& Fs = mVU.regAlloc->allocReg();
		const xmm& t2 = mVU.regAlloc->allocReg();
//		xPSHUF.D(Fs, t1, 0x01);
        armPSHUFD(Fs, t1, 0x01);
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xMOVSS  (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
		SSE_SUBSS (mVU, Fs, t1); // y-x, not y-1? ><
		SSE_ADDSS (mVU, t1, xmmPQ);
		SSE_DIVSS (mVU, Fs, t1);
		mVU_EATAN_(mVU, xmmPQ, Fs, t1, t2);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEATANxy);
	}
	pass3 { mVUlog("EATANxy P"); }
}

mVUop(mVU_EATANxz)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU2(mVU, _Fs_, 54);
	}
	pass2
	{
		const xmm& t1 = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const xmm& Fs = mVU.regAlloc->allocReg();
		const xmm& t2 = mVU.regAlloc->allocReg();
//		xPSHUF.D(Fs, t1, 0x02);
        armPSHUFD(Fs, t1, 0x02);
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xMOVSS  (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
		SSE_SUBSS (mVU, Fs, t1);
		SSE_ADDSS (mVU, t1, xmmPQ);
		SSE_DIVSS (mVU, Fs, t1);
		mVU_EATAN_(mVU, xmmPQ, Fs, t1, t2);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEATANxz);
	}
	pass3 { mVUlog("EATANxz P"); }
}

#define eexpHelper(addr) \
	{ \
		SSE_MULSS(mVU, t2, Fs); \
		armAsm->Mov(t1, t2); \
		armAsm->Fmul(t1.S(), t1.S(), armLoadPtrV(addr).S()); \
		SSE_ADDSS(mVU, xmmPQ, t1); \
	}

mVUop(mVU_EEXP)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU1(mVU, _Fs_, _Fsf_, 44);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const xmm& t1 = mVU.regAlloc->allocReg();
		const xmm& t2 = mVU.regAlloc->allocReg();
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xMOVSS  (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xMUL.SS (xmmPQ, ptr32[mVUglob.E1]);
        armAsm->Fmul(xmmPQ.S(), xmmPQ.S(), armLoadPtrV(PTR_CPU(mVUglob.E1)).S());
//		xADD.SS (xmmPQ, ptr32[mVUglob.one]);
        armAsm->Fadd(xmmPQ.S(), xmmPQ.S(), armLoadPtrV(PTR_CPU(mVUglob.one)).S());
//		xMOVAPS(t1, Fs);
        armAsm->Mov(t1.Q(), Fs.Q());
		SSE_MULSS(mVU, t1, Fs);
//		xMOVAPS(t2, t1);
        armAsm->Mov(t2.Q(), t1.Q());
//		xMUL.SS(t1, ptr32[mVUglob.E2]);
        armAsm->Fmul(t1.S(), t1.S(), armLoadPtrV(PTR_CPU(mVUglob.E2)).S());
		SSE_ADDSS(mVU, xmmPQ, t1);
		eexpHelper(PTR_CPU(mVUglob.E3));
		eexpHelper(PTR_CPU(mVUglob.E4));
		eexpHelper(PTR_CPU(mVUglob.E5));
		SSE_MULSS(mVU, t2, Fs);
//		xMUL.SS(t2, ptr32[mVUglob.E6]);
        armAsm->Fmul(t2.S(), t2.S(), armLoadPtrV(PTR_CPU(mVUglob.E6)).S());
		SSE_ADDSS(mVU, xmmPQ, t2);
		SSE_MULSS(mVU, xmmPQ, xmmPQ);
		SSE_MULSS(mVU, xmmPQ, xmmPQ);
//		xMOVSSZX(t2, ptr32[mVUglob.one]);
        armAsm->Ldr(t2, PTR_CPU(mVUglob.one));
		SSE_DIVSS(mVU, t2, xmmPQ);
//		xMOVSS(xmmPQ, t2);
        armAsm->Mov(xmmPQ.S(), 0, t2.S(), 0);
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEEXP);
	}
	pass3 { mVUlog("EEXP P"); }
}

// sumXYZ(): PQ.x = x ^ 2 + y ^ 2 + z ^ 2
static __fi void mVU_sumXYZ(mV, const xmm& PQ, const xmm& Fs)
{
//	xDP.PS(Fs, Fs, 0x71);
//	xMOVSS(PQ, Fs);

    armAsm->Fmul(PQ.V4S(), Fs.V4S(), Fs.V4S());
    armAsm->Ins(PQ.V4S(), 3, a64::wzr);

    armAsm->Faddp(PQ.V4S(), PQ.V4S(), PQ.V4S());
    armAsm->Faddp(PQ.S(), PQ.V2S());

    armAsm->Fmov(EAX, PQ.S());
    armAsm->Fmov(PQ.S(), EAX);
}

mVUop(mVU_ELENG)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU2(mVU, _Fs_, 18);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
//		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU_sumXYZ(mVU, RQSCRATCH, Fs);
//		xSQRT.SS       (xmmPQ, xmmPQ);
        armAsm->Fsqrt(RQSCRATCH.S(), RQSCRATCH.S());
        armAsm->Mov(xmmPQ.S(), 0, RQSCRATCH.S(), 0);
//		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opELENG);
	}
	pass3 { mVUlog("ELENG P"); }
}

mVUop(mVU_ERCPR)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU1(mVU, _Fs_, _Fsf_, 12);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
//		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xMOVSS        (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xMOVSSZX      (Fs, ptr32[mVUglob.one]);
        armAsm->Ldr(Fs, PTR_CPU(mVUglob.one));
		SSE_DIVSS(mVU, Fs, xmmPQ);
//		xMOVSS        (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opERCPR);
	}
	pass3 { mVUlog("ERCPR P"); }
}

mVUop(mVU_ERLENG)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU2(mVU, _Fs_, 24);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
//		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU_sumXYZ(mVU, RQSCRATCH, Fs);
//		xSQRT.SS       (xmmPQ, xmmPQ);
        armAsm->Fsqrt(RQSCRATCH.S(), RQSCRATCH.S());
        armAsm->Mov(xmmPQ.S(), 0, RQSCRATCH.S(), 0);
//		xMOVSSZX       (Fs, ptr32[mVUglob.one]);
        armAsm->Ldr(Fs, PTR_CPU(mVUglob.one));
		SSE_DIVSS (mVU, Fs, xmmPQ);
//		xMOVSS         (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opERLENG);
	}
	pass3 { mVUlog("ERLENG P"); }
}

mVUop(mVU_ERSADD)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU2(mVU, _Fs_, 18);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
//		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU_sumXYZ(mVU, xmmPQ, Fs);
//		xMOVSSZX       (Fs, ptr32[mVUglob.one]);
        armAsm->Ldr(Fs, PTR_CPU(mVUglob.one));
		SSE_DIVSS (mVU, Fs, xmmPQ);
//		xMOVSS         (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opERSADD);
	}
	pass3 { mVUlog("ERSADD P"); }
}

mVUop(mVU_ERSQRT)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU1(mVU, _Fs_, _Fsf_, 18);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
//		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xAND.PS       (Fs, ptr128[mVUglob.absclip]);
        armAsm->And(Fs.V16B(), Fs.V16B(), armLoadPtrV(PTR_CPU(mVUglob.absclip)).V16B());
//		xSQRT.SS      (xmmPQ, Fs);
		armAsm->Fsqrt(RQSCRATCH.S(), Fs.S());
		armAsm->Mov(xmmPQ.S(), 0, RQSCRATCH.S(), 0);
//		xMOVSSZX      (Fs, ptr32[mVUglob.one]);
        armAsm->Ldr(Fs, PTR_CPU(mVUglob.one));
		SSE_DIVSS(mVU, Fs, xmmPQ);
//		xMOVSS        (xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opERSQRT);
	}
	pass3 { mVUlog("ERSQRT P"); }
}

mVUop(mVU_ESADD)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU2(mVU, _Fs_, 11);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU_sumXYZ(mVU, xmmPQ, Fs);
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opESADD);
	}
	pass3 { mVUlog("ESADD P"); }
}

mVUop(mVU_ESIN)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU1(mVU, _Fs_, _Fsf_, 29);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const xmm& t1 = mVU.regAlloc->allocReg();
		const xmm& t2 = mVU.regAlloc->allocReg();
//		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xMOVSS        (xmmPQ, Fs); // pq = X
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
		SSE_MULSS(mVU, Fs, Fs);    // fs = X^2
//		xMOVAPS       (t1, Fs);    // t1 = X^2
        armAsm->Mov(t1, Fs);
		SSE_MULSS(mVU, Fs, xmmPQ); // fs = X^3
//		xMOVAPS       (t2, Fs);    // t2 = X^3
        armAsm->Mov(t2, Fs);
//		xMUL.SS       (Fs, ptr32[mVUglob.S2]); // fs = s2 * X^3
        armAsm->Fmul(Fs.S(), Fs.S(), armLoadPtrV(PTR_CPU(mVUglob.S2)).S());
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3

		SSE_MULSS(mVU, t2, t1);    // t2 = X^3 * X^2
//		xMOVAPS       (Fs, t2);    // fs = X^5
        armAsm->Mov(Fs, t2);
//		xMUL.SS       (Fs, ptr32[mVUglob.S3]); // ps = s3 * X^5
        armAsm->Fmul(Fs.S(), Fs.S(), armLoadPtrV(PTR_CPU(mVUglob.S3)).S());
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3 + s3 * X^5

		SSE_MULSS(mVU, t2, t1);    // t2 = X^5 * X^2
//		xMOVAPS       (Fs, t2);    // fs = X^7
        armAsm->Mov(Fs, t2);
//		xMUL.SS       (Fs, ptr32[mVUglob.S4]); // fs = s4 * X^7
        armAsm->Fmul(Fs.S(), Fs.S(), armLoadPtrV(PTR_CPU(mVUglob.S4)).S());
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3 + s3 * X^5 + s4 * X^7

		SSE_MULSS(mVU, t2, t1);    // t2 = X^7 * X^2
//		xMUL.SS       (t2, ptr32[mVUglob.S5]); // t2 = s5 * X^9
        armAsm->Fmul(t2.S(), t2.S(), armLoadPtrV(PTR_CPU(mVUglob.S5)).S());
		SSE_ADDSS(mVU, xmmPQ, t2); // pq = X + s2 * X^3 + s3 * X^5 + s4 * X^7 + s5 * X^9
//		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opESIN);
	}
	pass3 { mVUlog("ESIN P"); }
}

mVUop(mVU_ESQRT)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU1(mVU, _Fs_, _Fsf_, 12);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xAND.PS (Fs, ptr128[mVUglob.absclip]);
        armAsm->And(Fs.V16B(), Fs.V16B(), armLoadPtrV(PTR_CPU(mVUglob.absclip)).V16B());
//		xSQRT.SS(xmmPQ, Fs);
		armAsm->Fsqrt(RQSCRATCH.S(), Fs.S());
		armAsm->Mov(xmmPQ.S(), 0, RQSCRATCH.S(), 0);
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opESQRT);
	}
	pass3 { mVUlog("ESQRT P"); }
}

mVUop(mVU_ESUM)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeEFU2(mVU, _Fs_, 12);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		const xmm& t1 = mVU.regAlloc->allocReg();
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
//		xPSHUF.D(t1, Fs, 0x1b);
        armPSHUFD(t1, Fs, 0x1b);
		SSE_ADDPS(mVU, Fs, t1);
//		xPSHUF.D(t1, Fs, 0x01);
        armPSHUFD(t1, Fs, 0x01);
		SSE_ADDSS(mVU, Fs, t1);
//		xMOVSS(xmmPQ, Fs);
        armAsm->Mov(xmmPQ.S(), 0, Fs.S(), 0);
//		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
        armPSHUFD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.profiler.EmitOp(opESUM);
	}
	pass3 { mVUlog("ESUM P"); }
}

//------------------------------------------------------------------
// FCAND/FCEQ/FCGET/FCOR/FCSET
//------------------------------------------------------------------

mVUop(mVU_FCAND)
{
	pass1 { mVUanalyzeCflag(mVU, 1); }
	pass2
	{
		const a64::Register& dst = mVU.regAlloc->allocGPR(-1, 1, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, dst, cFLAG.read);
//		xAND(dst, _Imm24_);
        armAsm->And(dst, dst, _Imm24_);
//		xADD(dst, 0xffffff);
        armAsm->Add(dst, dst, 0xffffff);
//		xSHR(dst, 24);
        armAsm->Lsr(dst, dst, 24);
		mVU.regAlloc->clearNeeded(dst);
		mVU.profiler.EmitOp(opFCAND);
	}
	pass3 { mVUlog("FCAND vi01, $%x", _Imm24_); }
	pass4 { mVUregs.needExactMatch |= 4; }
}

mVUop(mVU_FCEQ)
{
	pass1 { mVUanalyzeCflag(mVU, 1); }
	pass2
	{
		const a64::Register& dst = mVU.regAlloc->allocGPR(-1, 1, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, dst, cFLAG.read);
//		xXOR(dst, _Imm24_);
        armAsm->Eor(dst, dst, _Imm24_);
//		xSUB(dst, 1);
        armAsm->Sub(dst, dst, 1);
//		xSHR(dst, 31);
        armAsm->Lsr(dst, dst, 31);
		mVU.regAlloc->clearNeeded(dst);
		mVU.profiler.EmitOp(opFCEQ);
	}
	pass3 { mVUlog("FCEQ vi01, $%x", _Imm24_); }
	pass4 { mVUregs.needExactMatch |= 4; }
}

mVUop(mVU_FCGET)
{
	pass1 { mVUanalyzeCflag(mVU, _It_); }
	pass2
	{
		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, regT, cFLAG.read);
//		xAND(regT, 0xfff);
        armAsm->And(regT, regT, 0xfff);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opFCGET);
	}
	pass3 { mVUlog("FCGET vi%02d", _Ft_); }
	pass4 { mVUregs.needExactMatch |= 4; }
}

mVUop(mVU_FCOR)
{
	pass1 { mVUanalyzeCflag(mVU, 1); }
	pass2
	{
		const a64::Register& dst = mVU.regAlloc->allocGPR(-1, 1, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, dst, cFLAG.read);
//		xOR(dst, _Imm24_);
        armAsm->Orr(dst, dst, _Imm24_);
//		xADD(dst, 1);  // If 24 1's will make 25th bit 1, else 0
        armAsm->Add(dst, dst, 1);
//		xSHR(dst, 24); // Get the 25th bit (also clears the rest of the garbage in the reg)
        armAsm->Lsr(dst, dst, 24);
		mVU.regAlloc->clearNeeded(dst);
		mVU.profiler.EmitOp(opFCOR);
	}
	pass3 { mVUlog("FCOR vi01, $%x", _Imm24_); }
	pass4 { mVUregs.needExactMatch |= 4; }
}

mVUop(mVU_FCSET)
{
	pass1 { cFLAG.doFlag = true; }
	pass2
	{
//		xMOV(gprT1, _Imm24_);
        armAsm->Mov(gprT1, _Imm24_);
		mVUallocCFLAGb(mVU, gprT1, cFLAG.write);
		mVU.profiler.EmitOp(opFCSET);
	}
	pass3 { mVUlog("FCSET $%x", _Imm24_); }
}

//------------------------------------------------------------------
// FMAND/FMEQ/FMOR
//------------------------------------------------------------------

mVUop(mVU_FMAND)
{
	pass1 { mVUanalyzeMflag(mVU, _Is_, _It_); }
	pass2
	{
		mVUallocMFLAGa(mVU, gprT1, mFLAG.read);
		const a64::Register& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
//		xAND(regT, gprT1);
        armAsm->And(regT, regT, gprT1);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opFMAND);
	}
	pass3 { mVUlog("FMAND vi%02d, vi%02d", _Ft_, _Fs_); }
	pass4 { mVUregs.needExactMatch |= 2; }
}

mVUop(mVU_FMEQ)
{
	pass1 { mVUanalyzeMflag(mVU, _Is_, _It_); }
	pass2
	{
		mVUallocMFLAGa(mVU, gprT1, mFLAG.read);
		const a64::Register& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
//		xXOR(regT, gprT1);
        armAsm->Eor(regT, regT, gprT1);
//		xSUB(regT, 1);
        armAsm->Sub(regT, regT, 1);
//		xSHR(regT, 31);
        armAsm->Lsr(regT, regT, 31);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opFMEQ);
	}
	pass3 { mVUlog("FMEQ vi%02d, vi%02d", _Ft_, _Fs_); }
	pass4 { mVUregs.needExactMatch |= 2; }
}

mVUop(mVU_FMOR)
{
	pass1 { mVUanalyzeMflag(mVU, _Is_, _It_); }
	pass2
	{
		mVUallocMFLAGa(mVU, gprT1, mFLAG.read);
		const a64::Register& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
//		xOR(regT, gprT1);
        armAsm->Orr(regT, regT, gprT1);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opFMOR);
	}
	pass3 { mVUlog("FMOR vi%02d, vi%02d", _Ft_, _Fs_); }
	pass4 { mVUregs.needExactMatch |= 2; }
}

//------------------------------------------------------------------
// FSAND/FSEQ/FSOR/FSSET
//------------------------------------------------------------------

mVUop(mVU_FSAND)
{
	pass1 { mVUanalyzeSflag(mVU, _It_); }
	pass2
	{
		if (_Imm12_ & 0x0c30) DevCon.WriteLn(Color_Green, "mVU_FSAND: Checking I/D/IS/DS Flags");
		if (_Imm12_ & 0x030c) DevCon.WriteLn(Color_Green, "mVU_FSAND: Checking U/O/US/OS Flags");
		const a64::Register& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGc(reg, gprT1, sFLAG.read);
//		xAND(reg, _Imm12_);
        armAsm->And(reg, reg, _Imm12_);
		mVU.regAlloc->clearNeeded(reg);
		mVU.profiler.EmitOp(opFSAND);
	}
	pass3 { mVUlog("FSAND vi%02d, $%x", _Ft_, _Imm12_); }
	pass4 { mVUregs.needExactMatch |= 1; }
}

mVUop(mVU_FSOR)
{
	pass1 { mVUanalyzeSflag(mVU, _It_); }
	pass2
	{
		const a64::Register& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGc(reg, gprT2, sFLAG.read);
//		xOR(reg, _Imm12_);
        armAsm->Orr(reg, reg, _Imm12_);
		mVU.regAlloc->clearNeeded(reg);
		mVU.profiler.EmitOp(opFSOR);
	}
	pass3 { mVUlog("FSOR vi%02d, $%x", _Ft_, _Imm12_); }
	pass4 { mVUregs.needExactMatch |= 1; }
}

mVUop(mVU_FSEQ)
{
	pass1 { mVUanalyzeSflag(mVU, _It_); }
	pass2
	{
		int imm = 0;
		if (_Imm12_ & 0x0c30) DevCon.WriteLn(Color_Green, "mVU_FSEQ: Checking I/D/IS/DS Flags");
		if (_Imm12_ & 0x030c) DevCon.WriteLn(Color_Green, "mVU_FSEQ: Checking U/O/US/OS Flags");
		if (_Imm12_ & 0x0001) imm |= 0x0000f00; // Z
		if (_Imm12_ & 0x0002) imm |= 0x000f000; // S
		if (_Imm12_ & 0x0004) imm |= 0x0010000; // U
		if (_Imm12_ & 0x0008) imm |= 0x0020000; // O
		if (_Imm12_ & 0x0010) imm |= 0x0040000; // I
		if (_Imm12_ & 0x0020) imm |= 0x0080000; // D
		if (_Imm12_ & 0x0040) imm |= 0x000000f; // ZS
		if (_Imm12_ & 0x0080) imm |= 0x00000f0; // SS
		if (_Imm12_ & 0x0100) imm |= 0x0400000; // US
		if (_Imm12_ & 0x0200) imm |= 0x0800000; // OS
		if (_Imm12_ & 0x0400) imm |= 0x1000000; // IS
		if (_Imm12_ & 0x0800) imm |= 0x2000000; // DS

		const a64::Register& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGa(reg, sFLAG.read);
		setBitFSEQ(reg, 0x0f00); // Z  bit
		setBitFSEQ(reg, 0xf000); // S  bit
		setBitFSEQ(reg, 0x000f); // ZS bit
		setBitFSEQ(reg, 0x00f0); // SS bit
//		xXOR(reg, imm);
        armAsm->Eor(reg, reg, imm);
//		xSUB(reg, 1);
        armAsm->Sub(reg, reg, 1);
//		xSHR(reg, 31);
        armAsm->Lsr(reg, reg, 31);
		mVU.regAlloc->clearNeeded(reg);
		mVU.profiler.EmitOp(opFSEQ);
	}
	pass3 { mVUlog("FSEQ vi%02d, $%x", _Ft_, _Imm12_); }
	pass4 { mVUregs.needExactMatch |= 1; }
}

mVUop(mVU_FSSET)
{
	pass1 { mVUanalyzeFSSET(mVU); }
	pass2
	{
		int imm = 0;
		if (_Imm12_ & 0x0040) imm |= 0x000000f; // ZS
		if (_Imm12_ & 0x0080) imm |= 0x00000f0; // SS
		if (_Imm12_ & 0x0100) imm |= 0x0400000; // US
		if (_Imm12_ & 0x0200) imm |= 0x0800000; // OS
		if (_Imm12_ & 0x0400) imm |= 0x1000000; // IS
		if (_Imm12_ & 0x0800) imm |= 0x2000000; // DS
		if (!(sFLAG.doFlag || mVUinfo.doDivFlag))
		{
			mVUallocSFLAGa(getFlagReg(sFLAG.write), sFLAG.lastWrite); // Get Prev Status Flag
		}
//		xAND(getFlagReg(sFLAG.write), 0xfff00); // Keep Non-Sticky Bits
        a64::Register reg32 = getFlagReg(sFLAG.write);
        armAsm->And(reg32, reg32, 0xfff00);
		if (imm) {
//            xOR(getFlagReg(sFLAG.write), imm);
            armAsm->Orr(reg32, reg32, imm);
        }
		mVU.profiler.EmitOp(opFSSET);
	}
	pass3 { mVUlog("FSSET $%x", _Imm12_); }
}

//------------------------------------------------------------------
// IADD/IADDI/IADDIU/IAND/IOR/ISUB/ISUBIU
//------------------------------------------------------------------

mVUop(mVU_IADD)
{
	pass1 { mVUanalyzeIALU1(mVU, _Id_, _Is_, _It_); }
	pass2
	{
		if (_Is_ == 0 || _It_ == 0)
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_ ? _Is_ : _It_, -1);
			const a64::Register& regD = mVU.regAlloc->allocGPR(-1, _Id_, mVUlow.backupVI);
//			xMOV(regD, regS);
            armAsm->Mov(regD, regS);
			mVU.regAlloc->clearNeeded(regD);
			mVU.regAlloc->clearNeeded(regS);
		}
		else
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1);
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
//			xADD(regS, regT);
            armAsm->Add(regS, regS, regT);
			mVU.regAlloc->clearNeeded(regS);
			mVU.regAlloc->clearNeeded(regT);
		}
		mVU.profiler.EmitOp(opIADD);
	}
	pass3 { mVUlog("IADD vi%02d, vi%02d, vi%02d", _Fd_, _Fs_, _Ft_); }
}

mVUop(mVU_IADDI)
{
	pass1 { mVUanalyzeIADDI(mVU, _Is_, _It_, _Imm5_); }
	pass2
	{
		if (_Is_ == 0)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm5_ != 0) {
//                    xMOV(regT, _Imm5_);
                    armAsm->Mov(regT, _Imm5_);
                }
                else {
//                    xXOR(regT, regT);
                    armAsm->Eor(regT, regT, regT);
                }
			}
			else
			{
//				xMOV(regT, ptr32[&curI]);
                armAsm->Ldr(regT, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xSHL(regT, 21);
                armAsm->Lsl(regT, regT, 21);
//				xSAR(regT, 27);
                armAsm->Asr(regT, regT, 27);
			}
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm5_ != 0) {
//                    xADD(regS, _Imm5_);
                    armAsm->Add(regS, regS, _Imm5_);
                }
			}
			else
			{
//				xMOV(gprT1, ptr32[&curI]);
                armAsm->Ldr(gprT1, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xSHL(gprT1, 21);
                armAsm->Lsl(gprT1, gprT1, 21);
//				xSAR(gprT1, 27);
                armAsm->Asr(gprT1, gprT1, 27);

//				xADD(regS, gprT1);
                armAsm->Add(regS, regS, gprT1);
			}
			mVU.regAlloc->clearNeeded(regS);
		}
		mVU.profiler.EmitOp(opIADDI);
	}
	pass3 { mVUlog("IADDI vi%02d, vi%02d, %d", _Ft_, _Fs_, _Imm5_); }
}

mVUop(mVU_IADDIU)
{
	pass1 { mVUanalyzeIADDI(mVU, _Is_, _It_, _Imm15_); }
	pass2
	{
		if (_Is_ == 0)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm15_ != 0) {
//                    xMOV(regT, _Imm15_);
                    armAsm->Mov(regT, _Imm15_);
                }
                else {
//                    xXOR(regT, regT);
                    armAsm->Eor(regT, regT, regT);
                }
			}
			else
			{
//				xMOV(regT, ptr32[&curI]);
                armAsm->Ldr(regT, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xMOV(gprT1, regT);
                armAsm->Mov(gprT1, regT);
//				xSHR(gprT1, 10);
                armAsm->Lsr(gprT1, gprT1, 10);
//				xAND(gprT1, 0x7800);
                armAsm->And(gprT1, gprT1, 0x7800);
//				xAND(regT, 0x7FF);
                armAsm->And(regT, regT, 0x7FF);
//				xOR(regT, gprT1);
                armAsm->Orr(regT, regT, gprT1);
			}
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm15_ != 0) {
//                    xADD(regS, _Imm15_);
                    armAsm->Add(regS, regS, _Imm15_);
                }
			}
			else
			{
//				xMOV(gprT1, ptr32[&curI]);
                armAsm->Ldr(gprT1, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xMOV(gprT2, gprT1);
                armAsm->Mov(gprT2, gprT1);
//				xSHR(gprT2, 10);
                armAsm->Lsr(gprT2, gprT2, 10);
//				xAND(gprT2, 0x7800);
                armAsm->And(gprT2, gprT2, 0x7800);
//				xAND(gprT1, 0x7FF);
                armAsm->And(gprT1, gprT1, 0x7FF);
//				xOR(gprT1, gprT2);
                armAsm->Orr(gprT1, gprT1, gprT2);

//				xADD(regS, gprT1);
                armAsm->Add(regS, regS, gprT1);
			}
			mVU.regAlloc->clearNeeded(regS);
		}
		mVU.profiler.EmitOp(opIADDIU);
	}
	pass3 { mVUlog("IADDIU vi%02d, vi%02d, %d", _Ft_, _Fs_, _Imm15_); }
}

mVUop(mVU_IAND)
{
	pass1 { mVUanalyzeIALU1(mVU, _Id_, _Is_, _It_); }
	pass2
	{
		const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1);
		const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
        if (_It_ != _Is_) {
//            xAND(regS, regT);
            armAsm->And(regS, regS, regT);
        }
		mVU.regAlloc->clearNeeded(regS);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opIAND);
	}
	pass3 { mVUlog("IAND vi%02d, vi%02d, vi%02d", _Fd_, _Fs_, _Ft_); }
}

mVUop(mVU_IOR)
{
	pass1 { mVUanalyzeIALU1(mVU, _Id_, _Is_, _It_); }
	pass2
	{
		const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1);
		const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
        if (_It_ != _Is_) {
//            xOR(regS, regT);
            armAsm->Orr(regS, regS, regT);
        }
		mVU.regAlloc->clearNeeded(regS);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opIOR);
	}
	pass3 { mVUlog("IOR vi%02d, vi%02d, vi%02d", _Fd_, _Fs_, _Ft_); }
}

mVUop(mVU_ISUB)
{
	pass1 { mVUanalyzeIALU1(mVU, _Id_, _Is_, _It_); }
	pass2
	{
		if (_It_ != _Is_)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1);
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
//			xSUB(regS, regT);
            armAsm->Sub(regS, regS, regT);
			mVU.regAlloc->clearNeeded(regS);
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const a64::Register& regD = mVU.regAlloc->allocGPR(-1, _Id_, mVUlow.backupVI);
//			xXOR(regD, regD);
            armAsm->Eor(regD, regD, regD);
			mVU.regAlloc->clearNeeded(regD);
		}
		mVU.profiler.EmitOp(opISUB);
	}
	pass3 { mVUlog("ISUB vi%02d, vi%02d, vi%02d", _Fd_, _Fs_, _Ft_); }
}

mVUop(mVU_ISUBIU)
{
	pass1 { mVUanalyzeIALU2(mVU, _Is_, _It_); }
	pass2
	{
		const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
		if (!EmuConfig.Gamefixes.IbitHack)
		{
            if (_Imm15_ != 0) {
//                xSUB(regS, _Imm15_);
                armAsm->Sub(regS, regS, _Imm15_);
            }
		}
		else
		{
//			xMOV(gprT1, ptr32[&curI]);
            armAsm->Ldr(gprT1, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//			xMOV(gprT2, gprT1);
            armAsm->Mov(gprT2, gprT1);
//			xSHR(gprT2, 10);
            armAsm->Lsr(gprT2, gprT2, 10);
//			xAND(gprT2, 0x7800);
            armAsm->And(gprT2, gprT2, 0x7800);
//			xAND(gprT1, 0x7FF);
            armAsm->And(gprT1, gprT1, 0x7FF);
//			xOR(gprT1, gprT2);
            armAsm->Orr(gprT1, gprT1, gprT2);

//			xSUB(regS, gprT1);
            armAsm->Sub(regS, regS, gprT1);
		}
		mVU.regAlloc->clearNeeded(regS);
		mVU.profiler.EmitOp(opISUBIU);
	}
	pass3 { mVUlog("ISUBIU vi%02d, vi%02d, %d", _Ft_, _Fs_, _Imm15_); }
}

//------------------------------------------------------------------
// MFIR/MFP/MOVE/MR32/MTIR
//------------------------------------------------------------------

mVUop(mVU_MFIR)
{
	pass1
	{
		if (!_Ft_)
		{
			mVUlow.isNOP = true;
		}
		analyzeVIreg1(mVU, _Is_, mVUlow.VI_read[0]);
		analyzeReg2  (mVU, _Ft_, mVUlow.VF_write, 1);
	}
	pass2
	{
		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		if (_Is_ != 0)
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, -1);
//			xMOVSX(xRegister32(regS), xRegister16(regS));
            armAsm->Sxth(regS, regS);
			// TODO: Broadcast instead
//			xMOVDZX(Ft, regS);
            armAsm->Fmov(Ft.S(), regS);
			if (!_XYZW_SS)
				mVUunpack_xyzw(Ft, Ft, 0);
			mVU.regAlloc->clearNeeded(regS);
		}
		else
		{
//			xPXOR(Ft, Ft);
            armAsm->Eor(Ft.V16B(), Ft.V16B(), Ft.V16B());
		}
		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opMFIR);
	}
	pass3 { mVUlog("MFIR.%s vf%02d, vi%02d", _XYZW_String, _Ft_, _Fs_); }
}

mVUop(mVU_MFP)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeMFP(mVU, _Ft_);
	}
	pass2
	{
		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		getPreg(mVU, Ft);
		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opMFP);
	}
	pass3 { mVUlog("MFP.%s vf%02d, P", _XYZW_String, _Ft_); }
}

mVUop(mVU_MOVE)
{
	pass1 { mVUanalyzeMOVE(mVU, _Fs_, _Ft_); }
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opMOVE);
	}
	pass3 { mVUlog("MOVE.%s vf%02d, vf%02d", _XYZW_String, _Ft_, _Fs_); }
}

mVUop(mVU_MR32)
{
	pass1 { mVUanalyzeMR32(mVU, _Fs_, _Ft_); }
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_);
		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		if (_XYZW_SS) {
            mVUunpack_xyzw(Ft, Fs, (_X ? 1 : (_Y ? 2 : (_Z ? 3 : 0))));
        }
		else {
//            xPSHUF.D(Ft, Fs, 0x39);
            armPSHUFD(Ft, Fs, 0x39);
        }
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opMR32);
	}
	pass3 { mVUlog("MR32.%s vf%02d, vf%02d", _XYZW_String, _Ft_, _Fs_); }
}

mVUop(mVU_MTIR)
{
	pass1
	{
		if (!_It_)
			mVUlow.isNOP = true;

		analyzeReg5(mVU, _Fs_, _Fsf_, mVUlow.VF_read[0]);
		analyzeVIreg2(mVU, _It_, mVUlow.VI_write, 1);
	}
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//		xMOVD(regT, Fs);
        armAsm->Fmov(regT, Fs.S());
		mVU.regAlloc->clearNeeded(regT);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opMTIR);
	}
	pass3 { mVUlog("MTIR vi%02d, vf%02d%s", _Ft_, _Fs_, _Fsf_String); }
}

//------------------------------------------------------------------
// ILW/ILWR
//------------------------------------------------------------------

mVUop(mVU_ILW)
{
	pass1
	{
		if (!_It_)
			mVUlow.isNOP = true;

		analyzeVIreg1(mVU, _Is_, mVUlow.VI_read[0]);
		analyzeVIreg2(mVU, _It_, mVUlow.VI_write, 4);
	}
	pass2
	{
//		void* ptr = mVU.regs().Mem + offsetSS;
        std::optional<a64::MemOperand> optaddr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, offsetSS));
		if (!optaddr.has_value())
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm11_ != 0) {
//                    xADD(gprT1, _Imm11_);
                    armAsm->Add(gprT1, gprT1, _Imm11_);
                }
			}
			else
			{
//				xMOV(gprT2, ptr32[&curI]);
                armAsm->Ldr(gprT2, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xSHL(gprT2, 21);
                armAsm->Lsl(gprT2, gprT2, 21);
//				xSAR(gprT2, 21);
                armAsm->Asr(gprT2, gprT2, 21);

//				xADD(gprT1, gprT2);
                armAsm->Add(gprT1, gprT1, gprT2);
			}
			mVUaddrFix(mVU, gprT1q);
		}

		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//		xMOVZX(regT, ptr16[optaddr.has_value() ? optaddr.value() : xComplexAddress(gprT2q, ptr, gprT1q)]);
        if(optaddr.has_value()) {
            armAsm->Ldrh(regT, optaddr.value());
        } else {
            armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(gprT2q, gprT2q, offsetSS);
            armAsm->Add(gprT2q, gprT2q, gprT1q);
            armAsm->Ldrh(regT, a64::MemOperand(gprT2q)); // test= mVU_ILW
        }
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opILW);
	}
	pass3 { mVUlog("ILW.%s vi%02d, vi%02d + %d", _XYZW_String, _Ft_, _Fs_, _Imm11_); }
}

mVUop(mVU_ILWR)
{
	pass1
	{
		if (!_It_)
			mVUlow.isNOP = true;

		analyzeVIreg1(mVU, _Is_, mVUlow.VI_read[0]);
		analyzeVIreg2(mVU, _It_, mVUlow.VI_write, 4);
	}
	pass2
	{
//		void* ptr = mVU.regs().Mem + offsetSS;
		if (_Is_)
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			mVUaddrFix (mVU, gprT1q);

			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//			xMOVZX(regT, ptr16[xComplexAddress(gprT2q, ptr, gprT1q)]);
            armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(gprT2q, gprT2q, offsetSS);
            armAsm->Add(gprT2q, gprT2q, gprT1q);
            armAsm->Ldrh(regT, a64::MemOperand(gprT2q));
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//			xMOVZX(regT, ptr16[ptr]);
            armAsm->Ldr(REX, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(REX, REX, offsetSS);
            armAsm->Ldrh(regT, a64::MemOperand(REX));
			mVU.regAlloc->clearNeeded(regT);
		}
		mVU.profiler.EmitOp(opILWR);
	}
	pass3 { mVUlog("ILWR.%s vi%02d, vi%02d", _XYZW_String, _Ft_, _Fs_); }
}

//------------------------------------------------------------------
// ISW/ISWR
//------------------------------------------------------------------

mVUop(mVU_ISW)
{
	pass1
	{
		mVUlow.isMemWrite = true;
		analyzeVIreg1(mVU, _Is_, mVUlow.VI_read[0]);
		analyzeVIreg1(mVU, _It_, mVUlow.VI_read[1]);
	}
	pass2
	{
		std::optional<a64::MemOperand> optaddr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, 0));
		if (!optaddr.has_value())
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm11_ != 0) {
//                    xADD(gprT1, _Imm11_);
                    armAsm->Add(gprT1, gprT1, _Imm11_);
                }
			}
			else
			{
//				xMOV(gprT2, ptr32[&curI]);
                armAsm->Ldr(gprT2, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xSHL(gprT2, 21);
                armAsm->Lsl(gprT2, gprT2, 21);
//				xSAR(gprT2, 21);
                armAsm->Asr(gprT2, gprT2, 21);

//				xADD(gprT1, gprT2);
                armAsm->Add(gprT1, gprT1, gprT2);
			}
			mVUaddrFix(mVU, gprT1q);
		}

		// If regT is dirty, the high bits might not be zero.
		const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1, false, true);
//		const xAddressVoid ptr(optaddr.has_value() ? optaddr.value() : xComplexAddress(gprT2q, mVU.regs().Mem, gprT1q));
        a64::MemOperand ptr;
        if(optaddr.has_value()) {
            ptr = optaddr.value();
        } else {
            armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(gprT2q, gprT2q, gprT1q);
            ptr = a64::MemOperand(gprT2q);
        }
        
//		if (_X) xMOV(ptr32[ptr], regT);
        if (_X) {
            armAsm->Str(regT, ptr);
        }
        
//		if (_Y) xMOV(ptr32[ptr + 4], regT);
        if (_Y) {
            armGetMemOperandInRegister(RXVIXLSCRATCH, ptr, 4);
            armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
        }
        
//		if (_Z) xMOV(ptr32[ptr + 8], regT);
        if (_Z) {
            armGetMemOperandInRegister(RXVIXLSCRATCH, ptr, 8);
            armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
        }
        
//		if (_W) xMOV(ptr32[ptr + 12], regT);
        if (_W) {
            armGetMemOperandInRegister(RXVIXLSCRATCH, ptr, 12);
            armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
        }
        
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opISW);
	}
	pass3 { mVUlog("ISW.%s vi%02d, vi%02d + %d", _XYZW_String, _Ft_, _Fs_, _Imm11_); }
}

mVUop(mVU_ISWR)
{
	pass1
	{
		mVUlow.isMemWrite = true;
		analyzeVIreg1(mVU, _Is_, mVUlow.VI_read[0]);
		analyzeVIreg1(mVU, _It_, mVUlow.VI_read[1]);
	}
	pass2
	{
		void* base = mVU.regs().Mem;
//		xAddressReg is = xEmptyReg;
        a64::Register is = a64::NoReg;
		if (_Is_)
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			mVUaddrFix(mVU, gprT1q);
			is = gprT1q;
		}
		const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1, false, true);
		if (!is.IsNone() && (sptr)base != (s32)(sptr)base)
		{
			int register_offset = -1;
			auto writeBackAt = [&](int offset) {
				if (register_offset == -1)
				{
//					xLEA(gprT2q, ptr[(void*)((sptr)base + offset)]);
                    armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
                    armAsm->Add(gprT2q, gprT2q, offset);
					register_offset = offset;
				}
//				xMOV(ptr32[gprT2q + is + (offset - register_offset)], regT);
                armAsm->Add(gprT2q, gprT2q, is);
                armAsm->Add(gprT2q, gprT2q, (offset - register_offset));
                armAsm->Str(regT, a64::MemOperand(gprT2q));
			};
			if (_X) writeBackAt(0);
			if (_Y) writeBackAt(4);
			if (_Z) writeBackAt(8);
			if (_W) writeBackAt(12);
		}
		else if (is.IsNone())
		{
//            auto base_ = armMemOperandPtr((void*)((uptr)base));
            auto base_mop = PTR_CPU(vuRegs[mVU.index].Mem);

//			if (_X) xMOV(ptr32[(void*)((uptr)base)], regT);
            if (_X) {
                armAsm->Str(regT, base_mop);
            }

//			if (_Y) xMOV(ptr32[(void*)((uptr)base + 4)], regT);
            if (_Y) {
                armGetMemOperandInRegister(RXVIXLSCRATCH, base_mop, 4);
                armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
            }

//			if (_Z) xMOV(ptr32[(void*)((uptr)base + 8)], regT);
            if (_Z) {
                armGetMemOperandInRegister(RXVIXLSCRATCH, base_mop, 8);
                armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
            }

//			if (_W) xMOV(ptr32[(void*)((uptr)base + 12)], regT);
            if (_W) {
                armGetMemOperandInRegister(RXVIXLSCRATCH, base_mop, 12);
                armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
            }
		}
		else
		{
            armAsm->Ldr(REX, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(REX, REX, is);
            auto base_is = a64::MemOperand(REX);

//			if (_X) xMOV(ptr32[base + is], regT);
            if (_X) {
                armAsm->Str(regT, base_is);
            }

//			if (_Y) xMOV(ptr32[base + is + 4], regT);
            if (_Y) {
                armGetMemOperandInRegister(RXVIXLSCRATCH, base_is, 4);
                armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
            }

//			if (_Z) xMOV(ptr32[base + is + 8], regT);
            if (_Z) {
                armGetMemOperandInRegister(RXVIXLSCRATCH, base_is, 8);
                armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
            }

//			if (_W) xMOV(ptr32[base + is + 12], regT);
            if (_W) {
                armGetMemOperandInRegister(RXVIXLSCRATCH, base_is, 12);
                armAsm->Str(regT, a64::MemOperand(RXVIXLSCRATCH));
            }
		}
		mVU.regAlloc->clearNeeded(regT);

		mVU.profiler.EmitOp(opISWR);
	}
	pass3 { mVUlog("ISWR.%s vi%02d, vi%02d", _XYZW_String, _Ft_, _Fs_); }
}

//------------------------------------------------------------------
// LQ/LQD/LQI
//------------------------------------------------------------------

mVUop(mVU_LQ)
{
	pass1 { mVUanalyzeLQ(mVU, _Ft_, _Is_, false); }
	pass2
	{
		const std::optional<a64::MemOperand> optaddr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, 0));
		if (!optaddr.has_value())
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm11_ != 0) {
//                    xADD(gprT1, _Imm11_);
                    armAsm->Add(gprT1, gprT1, _Imm11_);
                }
			}
			else
			{
//				xMOV(gprT2, ptr32[&curI]);
                armAsm->Ldr(gprT2, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xSHL(gprT2, 21);
                armAsm->Lsl(gprT2, gprT2, 21);
//				xSAR(gprT2, 21);
                armAsm->Asr(gprT2, gprT2, 21);

//				xADD(gprT1, gprT2);
                armAsm->Add(gprT1, gprT1, gprT2);
			}
			mVUaddrFix(mVU, gprT1q);
		}

		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
//		mVUloadReg(Ft, optaddr.has_value() ? optaddr.value() : xComplexAddress(gprT2q, mVU.regs().Mem, gprT1q), _X_Y_Z_W);
        if(optaddr.has_value()) {
            mVUloadReg(Ft, optaddr.value(), _X_Y_Z_W);
        } else {
            armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(gprT2q, gprT2q, gprT1q);
            mVUloadReg(Ft, a64::MemOperand(gprT2q), _X_Y_Z_W);
        }
		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opLQ);
	}
	pass3 { mVUlog("LQ.%s vf%02d, vi%02d + %d", _XYZW_String, _Ft_, _Fs_, _Imm11_); }
}

mVUop(mVU_LQD)
{
	pass1 { mVUanalyzeLQ(mVU, _Ft_, _Is_, true); }
	pass2
	{
		void* ptr = mVU.regs().Mem;
//		xAddressReg is = xEmptyReg;
        a64::Register is = a64::NoReg;
		if (_Is_ || isVU0) // Access VU1 regs mem-map in !_Is_ case
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Is_, mVUlow.backupVI);
//			xDEC(regS);
            armAsm->Sub(regS, regS, 1);
//			xMOVSX(gprT1, xRegister16(regS)); // TODO: Confirm
            armAsm->Sxth(gprT1, regS);
			mVU.regAlloc->clearNeeded(regS);
			mVUaddrFix(mVU, gprT1q);
			is = gprT1q;
		}
		else
		{
			ptr = (void*)((sptr)ptr + (0xffff & (mVU.microMemSize - 8)));
		}
		if (!mVUlow.noWriteVF)
		{
			const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
			if (is.IsNone())
			{
//				mVUloadReg(Ft, xAddressVoid(ptr), _X_Y_Z_W);
                mVUloadReg(Ft, PTR_CPU(vuRegs[mVU.index].Mem), _X_Y_Z_W);
			}
			else
			{
//				mVUloadReg(Ft, xComplexAddress(gprT2q, ptr, is), _X_Y_Z_W);
                armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
                armAsm->Add(gprT2q, gprT2q, is);
                mVUloadReg(Ft, a64::MemOperand(gprT2q), _X_Y_Z_W);
			}
			mVU.regAlloc->clearNeeded(Ft);
		}
		mVU.profiler.EmitOp(opLQD);
	}
	pass3 { mVUlog("LQD.%s vf%02d, --vi%02d", _XYZW_String, _Ft_, _Is_); }
}

mVUop(mVU_LQI)
{
	pass1 { mVUanalyzeLQ(mVU, _Ft_, _Is_, true); }
	pass2
	{
//		void* ptr = mVU.regs().Mem;
//		xAddressReg is = xEmptyReg;
        a64::Register is = a64::NoReg;
		if (_Is_)
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Is_, mVUlow.backupVI);
//			xMOVSX(gprT1, xRegister16(regS)); // TODO: Confirm
            armAsm->Sxth(gprT1, regS);
//			xINC(regS);
            armAsm->Add(regS, regS, 1);
			mVU.regAlloc->clearNeeded(regS);
			mVUaddrFix(mVU, gprT1q);
			is = gprT1q;
		}
		if (!mVUlow.noWriteVF)
		{
			const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
            if (is.IsNone()) {
//                mVUloadReg(Ft, xAddressVoid(ptr), _X_Y_Z_W);
                mVUloadReg(Ft, PTR_CPU(vuRegs[mVU.index].Mem), _X_Y_Z_W);
            }
            else {
//                mVUloadReg(Ft, xComplexAddress(gprT2q, ptr, is), _X_Y_Z_W);
                armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
                armAsm->Add(gprT2q, gprT2q, is);
                mVUloadReg(Ft, a64::MemOperand(gprT2q), _X_Y_Z_W);
            }
			mVU.regAlloc->clearNeeded(Ft);
		}
		mVU.profiler.EmitOp(opLQI);
	}
	pass3 { mVUlog("LQI.%s vf%02d, vi%02d++", _XYZW_String, _Ft_, _Fs_); }
}

//------------------------------------------------------------------
// SQ/SQD/SQI
//------------------------------------------------------------------

mVUop(mVU_SQ)
{
	pass1 { mVUanalyzeSQ(mVU, _Fs_, _It_, false); }
	pass2
	{
		const std::optional<a64::MemOperand> optptr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _It_, _Imm11_, 0));
		if (!optptr.has_value())
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _It_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
                if (_Imm11_ != 0) {
//                    xADD(gprT1, _Imm11_);
                    armAsm->Add(gprT1, gprT1, _Imm11_);
                }
			}
			else
			{
//				xMOV(gprT2, ptr32[&curI]);
                armAsm->Ldr(gprT2, armOffsetMemOperand(PTR_CPU(vuRegs[mVU.index].Micro), mVU.prog.IRinfo.curPC));
//				xSHL(gprT2, 21);
                armAsm->Lsl(gprT2, gprT2, 21);
//				xSAR(gprT2, 21);
                armAsm->Asr(gprT2, gprT2, 21);

//				xADD(gprT1, gprT2);
                armAsm->Add(gprT1, gprT1, gprT2);
			}
			mVUaddrFix(mVU, gprT1q);
		}

		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _XYZW_PS ? -1 : 0, _X_Y_Z_W);
//		mVUsaveReg(Fs, optptr.has_value() ? optptr.value() : xComplexAddress(gprT2q, mVU.regs().Mem, gprT1q), _X_Y_Z_W, 1);
        if(optptr.has_value()) {
            mVUsaveReg(Fs, optptr.value(), _X_Y_Z_W, 1);
        } else {
            armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(gprT2q, gprT2q, gprT1q);
            mVUsaveReg(Fs, a64::MemOperand(gprT2q), _X_Y_Z_W, 1);
        }
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opSQ);
	}
	pass3 { mVUlog("SQ.%s vf%02d, vi%02d + %d", _XYZW_String, _Fs_, _Ft_, _Imm11_); }
}

mVUop(mVU_SQD)
{
	pass1 { mVUanalyzeSQ(mVU, _Fs_, _It_, true); }
	pass2
	{
		void* ptr = mVU.regs().Mem;
//		xAddressReg it = xEmptyReg;
        a64::Register it = a64::NoReg;
		if (_It_ || isVU0) // Access VU1 regs mem-map in !_It_ case
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, _It_, mVUlow.backupVI);
//			xDEC(regT);
            armAsm->Sub(regT, regT, 1);
//			xMOVZX(gprT1, xRegister16(regT));
            armAsm->Uxth(gprT1, regT);
			mVU.regAlloc->clearNeeded(regT);
			mVUaddrFix(mVU, gprT1q);
			it = gprT1q;
		}
		else
		{
			ptr = (void*)((sptr)ptr + (0xffff & (mVU.microMemSize - 8)));
		}
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _XYZW_PS ? -1 : 0, _X_Y_Z_W);
        if (it.IsNone()) {
//            mVUsaveReg(Fs, xAddressVoid(ptr), _X_Y_Z_W, 1);
            mVUsaveReg(Fs, PTR_CPU(vuRegs[mVU.index].Mem), _X_Y_Z_W, 1);
        }
        else {
//            mVUsaveReg(Fs, xComplexAddress(gprT2q, ptr, it), _X_Y_Z_W, 1);
            armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(gprT2q, gprT2q, it);
            mVUsaveReg(Fs, a64::MemOperand(gprT2q), _X_Y_Z_W, 1);
        }
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opSQD);
	}
	pass3 { mVUlog("SQD.%s vf%02d, --vi%02d", _XYZW_String, _Fs_, _Ft_); }
}

mVUop(mVU_SQI)
{
	pass1 { mVUanalyzeSQ(mVU, _Fs_, _It_, true); }
	pass2
	{
//		void* ptr = mVU.regs().Mem;
		if (_It_)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, _It_, mVUlow.backupVI);
//			xMOVZX(gprT1, xRegister16(regT));
            armAsm->Uxth(gprT1, regT);
//			xINC(regT);
            armAsm->Add(regT, regT, 1);
			mVU.regAlloc->clearNeeded(regT);
			mVUaddrFix(mVU, gprT1q);
		}
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _XYZW_PS ? -1 : 0, _X_Y_Z_W);
        if (_It_) {
//            mVUsaveReg(Fs, xComplexAddress(gprT2q, ptr, gprT1q), _X_Y_Z_W, 1);
            armAsm->Ldr(gprT2q, PTR_CPU(vuRegs[mVU.index].Mem));
            armAsm->Add(gprT2q, gprT2q, gprT1q);
            mVUsaveReg(Fs, a64::MemOperand(gprT2q), _X_Y_Z_W, 1);
        }
        else {
//            mVUsaveReg(Fs, xAddressVoid(ptr), _X_Y_Z_W, 1);
            mVUsaveReg(Fs, PTR_CPU(vuRegs[mVU.index].Mem), _X_Y_Z_W, 1);
        }
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opSQI);
	}
	pass3 { mVUlog("SQI.%s vf%02d, vi%02d++", _XYZW_String, _Fs_, _Ft_); }
}

//------------------------------------------------------------------
// RINIT/RGET/RNEXT/RXOR
//------------------------------------------------------------------

mVUop(mVU_RINIT)
{
	pass1 { mVUanalyzeR1(mVU, _Fs_, _Fsf_); }
	pass2
	{
		if (_Fs_ || (_Fsf_ == 3))
		{
			const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
//			xMOVD(gprT1, Fs);
            armAsm->Fmov(gprT1, Fs.S());
//			xAND(gprT1, 0x007fffff);
            armAsm->And(gprT1, gprT1, 0x007fffff);
//			xOR (gprT1, 0x3f800000);
            armAsm->Orr(gprT1, gprT1, 0x3f800000);
//			xMOV(ptr32[Rmem], gprT1);
            armAsm->Str(gprT1, PTR_CPU(vuRegs[mVU.index].VI[REG_R].UL));
			mVU.regAlloc->clearNeeded(Fs);
		}
        else {
//            xMOV(ptr32[Rmem], 0x3f800000);
            armStorePtr(0x3f800000, PTR_CPU(vuRegs[mVU.index].VI[REG_R].UL));
        }
		mVU.profiler.EmitOp(opRINIT);
	}
	pass3 { mVUlog("RINIT R, vf%02d%s", _Fs_, _Fsf_String); }
}

static __fi void mVU_RGET_(mV, const x32& Rreg)
{
	if (!mVUlow.noWriteVF)
	{
		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
//		xMOVDZX(Ft, Rreg);
        armAsm->Fmov(Ft.S(), Rreg);
		if (!_XYZW_SS)
			mVUunpack_xyzw(Ft, Ft, 0);
		mVU.regAlloc->clearNeeded(Ft);
	}
}

mVUop(mVU_RGET)
{
	pass1 { mVUanalyzeR2(mVU, _Ft_, true); }
	pass2
	{
//		xMOV(gprT1, ptr32[Rmem]);
        armAsm->Ldr(gprT1, PTR_CPU(vuRegs[mVU.index].VI[REG_R].UL));
		mVU_RGET_(mVU, gprT1);
		mVU.profiler.EmitOp(opRGET);
	}
	pass3 { mVUlog("RGET.%s vf%02d, R", _XYZW_String, _Ft_); }
}

mVUop(mVU_RNEXT)
{
	pass1 { mVUanalyzeR2(mVU, _Ft_, false); }
	pass2
	{
		// algorithm from www.project-fao.org
		const a64::Register& temp3 = mVU.regAlloc->allocGPR();
//		xMOV(temp3, ptr32[Rmem]);
        armAsm->Ldr(temp3, PTR_CPU(vuRegs[mVU.index].VI[REG_R].UL));
//		xMOV(gprT1, temp3);
        armAsm->Mov(gprT1, temp3);
//		xSHR(gprT1, 4);
        armAsm->Lsr(gprT1, gprT1, 4);
//		xAND(gprT1, 1);
        armAsm->And(gprT1, gprT1, 1);

//		xMOV(gprT2, temp3);
        armAsm->Mov(gprT2, temp3);
//		xSHR(gprT2, 22);
        armAsm->Lsr(gprT2, gprT2, 22);
//		xAND(gprT2, 1);
        armAsm->And(gprT2, gprT2, 1);

//		xSHL(temp3, 1);
        armAsm->Lsl(temp3, temp3, 1);
//		xXOR(gprT1, gprT2);
        armAsm->Eor(gprT1, gprT1, gprT2);
//		xXOR(temp3, gprT1);
        armAsm->Eor(temp3, temp3, gprT1);
//		xAND(temp3, 0x007fffff);
        armAsm->And(temp3, temp3, 0x007fffff);
//		xOR (temp3, 0x3f800000);
        armAsm->Orr(temp3, temp3, 0x3f800000);
//		xMOV(ptr32[Rmem], temp3);
        armAsm->Str(temp3, PTR_CPU(vuRegs[mVU.index].VI[REG_R].UL));
		mVU_RGET_(mVU, temp3);
		mVU.regAlloc->clearNeeded(temp3);
		mVU.profiler.EmitOp(opRNEXT);
	}
	pass3 { mVUlog("RNEXT.%s vf%02d, R", _XYZW_String, _Ft_); }
}

mVUop(mVU_RXOR)
{
	pass1 { mVUanalyzeR1(mVU, _Fs_, _Fsf_); }
	pass2
	{
		if (_Fs_ || (_Fsf_ == 3))
		{
			const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
//			xMOVD(gprT1, Fs);
            armAsm->Fmov(gprT1, Fs.S());
//			xAND(gprT1, 0x7fffff);
            armAsm->And(gprT1, gprT1, 0x7fffff);
//			xXOR(ptr32[Rmem], gprT1);
            armEor(PTR_CPU(vuRegs[mVU.index].VI[REG_R].UL), gprT1);
			mVU.regAlloc->clearNeeded(Fs);
		}
		mVU.profiler.EmitOp(opRXOR);
	}
	pass3 { mVUlog("RXOR R, vf%02d%s", _Fs_, _Fsf_String); }
}

//------------------------------------------------------------------
// WaitP/WaitQ
//------------------------------------------------------------------

mVUop(mVU_WAITP)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUstall = std::max(mVUstall, (u8)((mVUregs.p) ? (mVUregs.p - 1) : 0));
	}
	pass2 { mVU.profiler.EmitOp(opWAITP); }
	pass3 { mVUlog("WAITP"); }
}

mVUop(mVU_WAITQ)
{
	pass1 { mVUstall = std::max(mVUstall, mVUregs.q); }
	pass2 { mVU.profiler.EmitOp(opWAITQ); }
	pass3 { mVUlog("WAITQ"); }
}

//------------------------------------------------------------------
// XTOP/XITOP
//------------------------------------------------------------------

mVUop(mVU_XTOP)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}

		if (!_It_)
			mVUlow.isNOP = true;

		analyzeVIreg2(mVU, _It_, mVUlow.VI_write, 1);
	}
	pass2
	{
		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//		xMOVZX(regT, ptr16[&mVU.getVifRegs().top]);
        if (mVU.index && THREAD_VU1) {
            armAsm->Ldrh(regT, PTR_MVU(vu1Thread.vifRegs.top));
        } else {
            armAsm->Ldrh(regT, PTR_CPU(vifRegs[mVU.index].top));
        }
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opXTOP);
	}
	pass3 { mVUlog("XTOP vi%02d", _Ft_); }
}

mVUop(mVU_XITOP)
{
	pass1
	{
		if (!_It_)
			mVUlow.isNOP = true;

		analyzeVIreg2(mVU, _It_, mVUlow.VI_write, 1);
	}
	pass2
	{
		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//		xMOVZX(regT, ptr16[&mVU.getVifRegs().itop]);
        if (mVU.index && THREAD_VU1) {
            armAsm->Ldrh(regT, PTR_MVU(vu1Thread.vifRegs.itop));
        } else {
            armAsm->Ldrh(regT, PTR_CPU(vifRegs[mVU.index].itop));
        }
//		xAND(regT, isVU1 ? 0x3ff : 0xff);
        armAsm->And(regT, regT, isVU1 ? 0x3ff : 0xff);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opXITOP);
	}
	pass3 { mVUlog("XITOP vi%02d", _Ft_); }
}

//------------------------------------------------------------------
// XGkick
//------------------------------------------------------------------

void mVU_XGKICK_(u32 addr)
{
	addr = (addr & 0x3ff) * 16;
	u32 diff = 0x4000 - addr;
	u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, g_cpuRegistersPack.vuRegs[1].Mem, addr, ~0u, true);

	if (size > diff)
	{
		//DevCon.WriteLn(Color_Green, "microVU1: XGkick Wrap!");
		gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&g_cpuRegistersPack.vuRegs[1].Mem[addr], diff, true);
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &g_cpuRegistersPack.vuRegs[1].Mem[0], size - diff, true);
	}
	else
	{
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &g_cpuRegistersPack.vuRegs[1].Mem[addr], size, true);
	}
}

void _vuXGKICKTransfermVU(bool flush)
{
	while (VU1.xgkickenable && (flush || VU1.xgkickcyclecount >= 2))
	{
		u32 transfersize = 0;

		if (VU1.xgkicksizeremaining == 0)
		{
			//VUM_LOG("XGKICK reading new tag from %x", VU1.xgkickaddr);
			u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, g_cpuRegistersPack.vuRegs[1].Mem, VU1.xgkickaddr, ~0u, flush);
			VU1.xgkicksizeremaining = size & 0xFFFF;
			VU1.xgkickendpacket = size >> 31;
			VU1.xgkickdiff = 0x4000 - VU1.xgkickaddr;

			if (VU1.xgkicksizeremaining == 0)
			{
				//VUM_LOG("Invalid GS packet size returned, cancelling XGKick");
				VU1.xgkickenable = false;
				break;
			}
			//else
				//VUM_LOG("XGKICK New tag size %d bytes EOP %d", VU1.xgkicksizeremaining, VU1.xgkickendpacket);
		}

		if (!flush)
		{
			transfersize = std::min(VU1.xgkicksizeremaining, VU1.xgkickcyclecount * 8);
			transfersize = std::min(transfersize, VU1.xgkickdiff);
		}
		else
		{
			transfersize = VU1.xgkicksizeremaining;
			transfersize = std::min(transfersize, VU1.xgkickdiff);
		}

		//VUM_LOG("XGKICK Transferring %x bytes from %x size %x", transfersize * 0x10, VU1.xgkickaddr, VU1.xgkicksizeremaining);

		// Would be "nicer" to do the copy until it's all up, however this really screws up PATH3 masking stuff
		// So lets just do it the other way :)
		if (THREAD_VU1)
		{
			if (transfersize < VU1.xgkicksizeremaining)
				gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&VU1.Mem[VU1.xgkickaddr], transfersize, true);
			else
				gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &g_cpuRegistersPack.vuRegs[1].Mem[VU1.xgkickaddr], transfersize, true);
		}
		else
		{
			gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &g_cpuRegistersPack.vuRegs[1].Mem[VU1.xgkickaddr], transfersize, true);
		}

		if (flush)
			VU1.cycle += transfersize / 8;

		VU1.xgkickcyclecount -= transfersize / 8;

		VU1.xgkickaddr = (VU1.xgkickaddr + transfersize) & 0x3FFF;
		VU1.xgkicksizeremaining -= transfersize;
		VU1.xgkickdiff = 0x4000 - VU1.xgkickaddr;

		if (VU1.xgkickendpacket && !VU1.xgkicksizeremaining)
		//	VUM_LOG("XGKICK next addr %x left size %x", VU1.xgkickaddr, VU1.xgkicksizeremaining);
		//else
		{
			//VUM_LOG("XGKICK transfer finished");
			VU1.xgkickenable = false;
			// Check if VIF is waiting for the GIF to not be busy
		}
	}
	//VUM_LOG("XGKick run complete Enabled %d", VU1.xgkickenable);
}

static __fi void mVU_XGKICK_SYNC(mV, bool flush)
{
	mVU.regAlloc->flushCallerSavedRegisters();

	// Add the single cycle remainder after this instruction, some games do the store
	// on the second instruction after the kick and that needs to go through first
	// but that's VERY close..
//	xTEST(ptr32[&VU1.xgkickenable], 0x1);
    armAsm->Tst(armLoadPtr(PTR_CPU(vuRegs[1].xgkickenable)), 0x1);
//	xForwardJZ32 skipxgkick;
    a64::Label skipxgkick;
    armAsm->B(&skipxgkick, a64::Condition::eq);
//	xADD(ptr32[&VU1.xgkickcyclecount], mVUlow.kickcycles-1);
    armAdd(PTR_CPU(vuRegs[1].xgkickcyclecount), mVUlow.kickcycles-1);
//	xCMP(ptr32[&VU1.xgkickcyclecount], 2);
    armAsm->Cmp(armLoadPtr(PTR_CPU(vuRegs[1].xgkickcyclecount)), 2);
//	xForwardJL32 needcycles;
    a64::Label needcycles;
    armAsm->B(&needcycles, a64::Condition::lt);
	mVUbackupRegs(mVU, true, true);
//	xFastCall(_vuXGKICKTransfermVU, flush);
    armAsm->Mov(EAX, flush);
    armEmitCall(reinterpret_cast<void*>(_vuXGKICKTransfermVU));
	mVUrestoreRegs(mVU, true, true);
//	needcycles.SetTarget();
    armBind(&needcycles);
//	xADD(ptr32[&VU1.xgkickcyclecount], 1);
    armAdd(PTR_CPU(vuRegs[1].xgkickcyclecount), 1);
//	skipxgkick.SetTarget();
    armBind(&skipxgkick);
}

static __fi void mVU_XGKICK_DELAY(mV)
{
	mVU.regAlloc->flushCallerSavedRegisters();

	mVUbackupRegs(mVU, true, true);
#if 0 // XGkick Break - ToDo: Change "SomeGifPathValue" to w/e needs to be tested
	xTEST (ptr32[&SomeGifPathValue], 1); // If '1', breaks execution
	xMOV  (ptr32[&mVU.resumePtrXG], (uptr)xGetPtr() + 10 + 6);
	xJcc32(Jcc_NotZero, (uptr)mVU.exitFunctXG - ((uptr)xGetPtr()+6));
#endif
//	xFastCall(mVU_XGKICK_, ptr32[&mVU.VIxgkick]);
    armAsm->Ldr(EAX, PTR_MVU(microVU[mVU.index].VIxgkick));
    armEmitCall(reinterpret_cast<void*>(mVU_XGKICK_));
	mVUrestoreRegs(mVU, true, true);
}

mVUop(mVU_XGKICK)
{
	pass1
	{
		if (isVU0)
		{
			mVUlow.isNOP = true;
			return;
		}
		mVUanalyzeXGkick(mVU, _Is_, 1);
	}
		pass2
	{
		if (CHECK_XGKICKHACK)
		{
			mVUlow.kickcycles = 99;
			mVU_XGKICK_SYNC(mVU, true);
			mVUlow.kickcycles = 0;
		}
		if (mVUinfo.doXGKICK) // check for XGkick Transfer
		{
			mVU_XGKICK_DELAY(mVU);
			mVUinfo.doXGKICK = false;
		}

		const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, -1);
		if (!CHECK_XGKICKHACK)
		{
//			xMOV(ptr32[&mVU.VIxgkick], regS);
            armAsm->Str(regS, PTR_MVU(microVU[mVU.index].VIxgkick));
		}
		else
		{
//			xMOV(ptr32[&VU1.xgkickenable], 1);
            armStorePtr(1, PTR_CPU(vuRegs[1].xgkickenable));
//			xMOV(ptr32[&VU1.xgkickendpacket], 0);
            armStorePtr(0, PTR_CPU(vuRegs[1].xgkickendpacket));
//			xMOV(ptr32[&VU1.xgkicksizeremaining], 0);
            armStorePtr(0, PTR_CPU(vuRegs[1].xgkicksizeremaining));
//			xMOV(ptr32[&VU1.xgkickcyclecount], 0);
            armStorePtr(0, PTR_CPU(vuRegs[1].xgkickcyclecount));
//			xMOV(gprT2, ptr32[&mVU.totalCycles]);
            armAsm->Ldr(gprT2, PTR_MVU(microVU[mVU.index].totalCycles));
//			xSUB(gprT2, ptr32[&mVU.cycles]);
            armAsm->Sub(gprT2, gprT2, armLoadPtr(PTR_MVU(microVU[mVU.index].cycles)));
//			xADD(gprT2, ptr32[&VU1.cycle]);
            armAsm->Add(gprT2, gprT2, armLoadPtr(PTR_CPU(vuRegs[1].cycle)));
//			xMOV(ptr32[&VU1.xgkicklastcycle], gprT2);
            armAsm->Str(gprT2, PTR_CPU(vuRegs[1].xgkicklastcycle));
//			xMOV(gprT1, regS);
            armAsm->Mov(gprT1, regS);
//			xAND(gprT1, 0x3FF);
            armAsm->And(gprT1, gprT1, 0x3FF);
//			xSHL(gprT1, 4);
            armAsm->Lsl(gprT1, gprT1, 4);
//			xMOV(ptr32[&VU1.xgkickaddr], gprT1);
            armAsm->Str(gprT1, PTR_CPU(vuRegs[1].xgkickaddr));
		}
		mVU.regAlloc->clearNeeded(regS);
		mVU.profiler.EmitOp(opXGKICK);
	}
	pass3 { mVUlog("XGKICK vi%02d", _Fs_); }
}

//------------------------------------------------------------------
// Branches/Jumps
//------------------------------------------------------------------

void setBranchA(mP, int x, int _x_)
{
	bool isBranchDelaySlot = false;

	incPC(-2);
	if (mVUlow.branch)
		isBranchDelaySlot = true;
	incPC(2);

	pass1
	{
		if (_Imm11_ == 1 && !_x_ && !isBranchDelaySlot)
		{
			DevCon.WriteLn(Color_Green, "microVU%d: Branch Optimization", mVU.index);
			mVUlow.isNOP = true;
			return;
		}
		mVUbranch     = x;
		mVUlow.branch = x;
	}
	pass2 { if (_Imm11_ == 1 && !_x_ && !isBranchDelaySlot) { return; } mVUbranch = x; }
	pass3 { mVUbranch = x; }
	pass4 { if (_Imm11_ == 1 && !_x_ && !isBranchDelaySlot) { return; } mVUbranch = x; }
}

void condEvilBranch(mV, a64::Condition JMPcc)
{
	if (mVUlow.badBranch)
	{
//		xMOV(ptr32[&mVU.branch], gprT1);
        armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
//		xMOV(ptr32[&mVU.badBranch], branchAddr(mVU));
        armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].badBranch));

//		xCMP(gprT1b, 0);
        armAsm->Cmp(gprT1b, 0);
//		xForwardJump8 cJMP((JccComparisonType)JMPcc);
        a64::Label cJMP;
        armAsm->B(&cJMP, JMPcc);
			incPC(4); // Branch Not Taken Addr
//			xMOV(ptr32[&mVU.badBranch], xPC);
            armStorePtr(xPC, PTR_MVU(microVU[mVU.index].badBranch));
			incPC(-4);
//		cJMP.SetTarget();
        armBind(&cJMP);
		return;
	}
	if (isEvilBlock)
	{
//		xMOV(ptr32[&mVU.evilevilBranch], branchAddr(mVU));
        armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].evilevilBranch));
//		xCMP(gprT1b, 0);
        armAsm->Cmp(gprT1b, 0);
//		xForwardJump8 cJMP((JccComparisonType)JMPcc);
        a64::Label cJMP;
        armAsm->B(&cJMP, JMPcc);
//		xMOV(gprT1, ptr32[&mVU.evilBranch]); // Branch Not Taken
        armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].evilBranch));
//		xADD(gprT1, 8); // We have already executed 1 instruction from the original branch
        armAsm->Add(gprT1, gprT1, 8);
//		xMOV(ptr32[&mVU.evilevilBranch], gprT1);
        armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].evilevilBranch));
//		cJMP.SetTarget();
        armBind(&cJMP);
	}
	else
	{
//		xMOV(ptr32[&mVU.evilBranch], branchAddr(mVU));
        armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].evilBranch));
//		xCMP(gprT1b, 0);
        armAsm->Cmp(gprT1b, 0);
//		xForwardJump8 cJMP((JccComparisonType)JMPcc);
        a64::Label cJMP;
        armAsm->B(&cJMP, JMPcc);
//		xMOV(gprT1, ptr32[&mVU.badBranch]); // Branch Not Taken
        armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].badBranch));
//		xADD(gprT1, 8); // We have already executed 1 instruction from the original branch
        armAsm->Add(gprT1, gprT1, 8);
//		xMOV(ptr32[&mVU.evilBranch], gprT1);
        armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].evilBranch));
//		cJMP.SetTarget();
        armBind(&cJMP);
		incPC(-2);
		if (mVUlow.branch >= 9)
			DevCon.Warning("Conditional in JALR/JR delay slot - If game broken report to PCSX2 Team");
		incPC(2);
	}
}

mVUop(mVU_B)
{
	setBranchA(mX, 1, 0);
	pass1 { mVUanalyzeNormBranch(mVU, 0, false); }
	pass2
	{
        if (mVUlow.badBranch)  {
//            xMOV(ptr32[&mVU.badBranch],  branchAddr(mVU));
            armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].badBranch));
        }
        if (mVUlow.evilBranch) {
            if(isEvilBlock) {
//                xMOV(ptr32[&mVU.evilevilBranch], branchAddr(mVU));
                armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].evilevilBranch));
            }
            else {
//                xMOV(ptr32[&mVU.evilBranch], branchAddr(mVU));
                armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].evilBranch));
            }
        }
		mVU.profiler.EmitOp(opB);
	}
	pass3 { mVUlog("B [<a href=\"#addr%04x\">%04x</a>]", branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_BAL)
{
	setBranchA(mX, 2, _It_);
	pass1 { mVUanalyzeNormBranch(mVU, _It_, true); }
	pass2
	{
		if (!mVUlow.evilBranch)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//			xMOV(regT, bSaveAddr);
            armAsm->Mov(regT, bSaveAddr);
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			incPC(-2);
			DevCon.Warning("Linking BAL from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
			incPC(2);

			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
            if (isEvilBlock) {
//                xMOV(regT, ptr32[&mVU.evilBranch]);
                armAsm->Ldr(regT, PTR_MVU(microVU[mVU.index].evilBranch));
            }
            else {
//                xMOV(regT, ptr32[&mVU.badBranch]);
                armAsm->Ldr(regT, PTR_MVU(microVU[mVU.index].badBranch));
            }

//			xADD(regT, 8);
            armAsm->Add(regT, regT, 8);
//			xSHR(regT, 3);
            armAsm->Lsr(regT, regT, 3);
			mVU.regAlloc->clearNeeded(regT);
		}

        if (mVUlow.badBranch)  {
//            xMOV(ptr32[&mVU.badBranch],  branchAddr(mVU));
            armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].badBranch));
        }
        if (mVUlow.evilBranch) {
            if (isEvilBlock) {
//                xMOV(ptr32[&mVU.evilevilBranch], branchAddr(mVU));
                armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].evilevilBranch));
            }
            else {
//                xMOV(ptr32[&mVU.evilBranch], branchAddr(mVU));
                armStorePtr(branchAddr(mVU), PTR_MVU(microVU[mVU.index].evilBranch));
            }
        }
		mVU.profiler.EmitOp(opBAL);
	}
	pass3 { mVUlog("BAL vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBEQ)
{
	setBranchA(mX, 3, 0);
	pass1 { mVUanalyzeCondBranch2(mVU, _Is_, _It_); }
	pass2
	{
        if (mVUlow.memReadIs) {
//            xMOV(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].VIbackup));
        }
        else {
            mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
        }

        if (mVUlow.memReadIt) {
//            xXOR(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Eor(gprT1, gprT1, armLoadPtr(PTR_MVU(microVU[mVU.index].VIbackup)));
        }
        else
        {
            const a64::Register& regT = mVU.regAlloc->allocGPR(_It_);
//			xXOR(gprT1, regT);
            armAsm->Eor(gprT1, gprT1, regT);
            mVU.regAlloc->clearNeeded(regT);
        }

        if (!(isBadOrEvil)) {
//            xMOV(ptr32[&mVU.branch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
        }
        else {
//            condEvilBranch(mVU, Jcc_Equal);
            condEvilBranch(mVU, a64::Condition::eq);
        }
		mVU.profiler.EmitOp(opIBEQ);
	}
	pass3 { mVUlog("IBEQ vi%02d, vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBGEZ)
{
	setBranchA(mX, 4, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2
	{
        if (mVUlow.memReadIs) {
//            xMOV(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].VIbackup));
        }
        else {
            mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
        }
        if (!(isBadOrEvil)) {
//            xMOV(ptr32[&mVU.branch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
        }
        else {
//            condEvilBranch(mVU, Jcc_GreaterOrEqual);
            condEvilBranch(mVU, a64::Condition::ge);
        }
		mVU.profiler.EmitOp(opIBGEZ);
	}
	pass3 { mVUlog("IBGEZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBGTZ)
{
	setBranchA(mX, 5, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2
	{
        if (mVUlow.memReadIs) {
//            xMOV(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].VIbackup));
        }
        else {
            mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
        }
        if (!(isBadOrEvil)) {
//            xMOV(ptr32[&mVU.branch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
        }
        else {
//            condEvilBranch(mVU, Jcc_Greater);
            condEvilBranch(mVU, a64::Condition::gt);
        }
		mVU.profiler.EmitOp(opIBGTZ);
	}
	pass3 { mVUlog("IBGTZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBLEZ)
{
	setBranchA(mX, 6, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2
	{
        if (mVUlow.memReadIs) {
//            xMOV(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].VIbackup));
        }
        else {
            mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
        }
        if (!(isBadOrEvil)) {
//            xMOV(ptr32[&mVU.branch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
        }
        else {
//            condEvilBranch(mVU, Jcc_LessOrEqual);
            condEvilBranch(mVU, a64::Condition::le);
        }
		mVU.profiler.EmitOp(opIBLEZ);
	}
	pass3 { mVUlog("IBLEZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBLTZ)
{
	setBranchA(mX, 7, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2
	{
        if (mVUlow.memReadIs) {
//            xMOV(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].VIbackup));
        }
        else {
            mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
        }
        if (!(isBadOrEvil)) {
//            xMOV(ptr32[&mVU.branch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
        }
        else {
//            condEvilBranch(mVU, Jcc_Less);
            condEvilBranch(mVU, a64::Condition::lt);
        }
		mVU.profiler.EmitOp(opIBLTZ);
	}
	pass3 { mVUlog("IBLTZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBNE)
{
	setBranchA(mX, 8, 0);
	pass1 { mVUanalyzeCondBranch2(mVU, _Is_, _It_); }
	pass2
	{
        if (mVUlow.memReadIs) {
//            xMOV(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Ldr(gprT1, PTR_MVU(microVU[mVU.index].VIbackup));
        }
        else {
            mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
        }

        if (mVUlow.memReadIt) {
//            xXOR(gprT1, ptr32[&mVU.VIbackup]);
            armAsm->Eor(gprT1, gprT1, armLoadPtr(PTR_MVU(microVU[mVU.index].VIbackup)));
        }
        else
        {
            const a64::Register& regT = mVU.regAlloc->allocGPR(_It_);
//			xXOR(gprT1, regT);
            armAsm->Eor(gprT1, gprT1, regT);
            mVU.regAlloc->clearNeeded(regT);
        }

        if (!(isBadOrEvil)) {
//            xMOV(ptr32[&mVU.branch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
        }
        else {
//            condEvilBranch(mVU, Jcc_NotEqual);
            condEvilBranch(mVU, a64::Condition::ne);
        }
		mVU.profiler.EmitOp(opIBNE);
	}
	pass3 { mVUlog("IBNE vi%02d, vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

void normJumpPass2(mV)
{
	if (!mVUlow.constJump.isValid || mVUlow.evilBranch)
	{
		mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
//		xSHL(gprT1, 3);
        armAsm->Lsl(gprT1, gprT1, 3);
//		xAND(gprT1, mVU.microMemSize - 8);
        armAsm->And(gprT1, gprT1, mVU.microMemSize - 8);

		if (!mVUlow.evilBranch)
		{
//			xMOV(ptr32[&mVU.branch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].branch));
		}
		else
		{
            if(isEvilBlock) {
//                xMOV(ptr32[&mVU.evilevilBranch], gprT1);
                armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].evilevilBranch));
            }
            else {
//                xMOV(ptr32[&mVU.evilBranch], gprT1);
                armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].evilBranch));
            }
		}
		//If delay slot is conditional, it uses badBranch to go to its target
		if (mVUlow.badBranch)
		{
//			xMOV(ptr32[&mVU.badBranch], gprT1);
            armAsm->Str(gprT1, PTR_MVU(microVU[mVU.index].badBranch));
		}
	}
}

mVUop(mVU_JR)
{
	mVUbranch = 9;
	pass1 { mVUanalyzeJump(mVU, _Is_, 0, false); }
	pass2
	{
		normJumpPass2(mVU);
		mVU.profiler.EmitOp(opJR);
	}
	pass3 { mVUlog("JR [vi%02d]", _Fs_); }
}

mVUop(mVU_JALR)
{
	mVUbranch = 10;
	pass1 { mVUanalyzeJump(mVU, _Is_, _It_, 1); }
	pass2
	{
		normJumpPass2(mVU);
		if (!mVUlow.evilBranch)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
//			xMOV(regT, bSaveAddr);
            armAsm->Mov(regT, bSaveAddr);
			mVU.regAlloc->clearNeeded(regT);
		}
		if (mVUlow.evilBranch)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (isEvilBlock)
			{
//				xMOV(regT, ptr32[&mVU.evilBranch]);
                armAsm->Ldr(regT, PTR_MVU(microVU[mVU.index].evilBranch));
//				xADD(regT, 8);
                armAsm->Add(regT, regT, 8);
//				xSHR(regT, 3);
                armAsm->Lsr(regT, regT, 3);
			}
			else
			{
				incPC(-2);
				DevCon.Warning("Linking JALR from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
				incPC(2);

//				xMOV(regT, ptr32[&mVU.badBranch]);
                armAsm->Ldr(regT, PTR_MVU(microVU[mVU.index].badBranch));
//				xADD(regT, 8);
                armAsm->Add(regT, regT, 8);
//				xSHR(regT, 3);
                armAsm->Lsr(regT, regT, 3);
			}
			mVU.regAlloc->clearNeeded(regT);
		}

		mVU.profiler.EmitOp(opJALR);
	}
	pass3 { mVUlog("JALR vi%02d, [vi%02d]", _Ft_, _Fs_); }
}
