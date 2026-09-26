// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU Micromode Lower instructions
//------------------------------------------------------------------

//------------------------------------------------------------------
// DIV/SQRT/RSQRT
//------------------------------------------------------------------

// Branches to not_zero if lane 0 of xmmReg is not +/- zero (NaN counts as not zero).
static __fi void testZero(const xmm& xmmReg, a64::Label* not_zero)
{
	armAsm->Fcmp(xmmReg.S(), 0.0);
	armAsm->B(not_zero, a64::ne);
}

// Test if Vector is Negative (Set Flags and Makes Positive)
static __fi void testNeg(mV, const xmm& xmmReg, const x32& gprTemp)
{
	a64::Label skip;
	armAsm->Fmov(gprTemp.W(), xmmReg.S());
	armAsm->Tbz(gprTemp.W(), 31, &skip);
		mVUstoreImm32(mVUmem(&mVU.divFlag), divI);
		armAsm->And(xmmReg.V16B(), xmmReg.V16B(), mVUloadConst(mVUconst(absclip)).V16B());
	armAsm->Bind(&skip);
}

// Sets the D/I status bits in COP2 mode.
static __fi void mVUdivCOP2Flags(mV)
{
	if (mVU.cop2)
	{
		armAsm->And(gprF0.W(), gprF0.W(), ~0xc0000);
		armAsm->Ldr(a64::w8, mVUmem(&mVU.divFlag));
		armAsm->Orr(gprF0.W(), gprF0.W(), a64::w8);
	}
}

// Fs = (Fs ^ Ft) & signbit | maxvals, i.e. +/- fmax with the sign of the division.
static __fi void mVUdivByZeroResult(const xmm& Fs, const xmm& Ft)
{
	armAsm->Eor(Fs.V16B(), Fs.V16B(), Ft.V16B());
	armAsm->And(Fs.V16B(), Fs.V16B(), mVUloadConst(mVUconst(signbit)).V16B());
	armAsm->Orr(Fs.V16B(), Fs.V16B(), mVUloadConst(mVUconst(maxvals)).V16B()); // If division by zero, then xmmFs = +/- fmax
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

		a64::Label cjmp, ajmp, bjmp, djmp;
		testZero(Ft, &cjmp); // Skip if not zero

			testZero(Fs, &ajmp); // Test if Fs is zero
				mVUstoreImm32(mVUmem(&mVU.divFlag), divI); // Set invalid flag (0/0)
				armAsm->B(&bjmp);
			armAsm->Bind(&ajmp);
				mVUstoreImm32(mVUmem(&mVU.divFlag), divD); // Zero divide (only when not 0/0)
			armAsm->Bind(&bjmp);

			mVUdivByZeroResult(Fs, Ft);

			armAsm->B(&djmp);
		armAsm->Bind(&cjmp);
			mVUstoreImm32(mVUmem(&mVU.divFlag), 0); // Clear I/D flags
			SSE_DIVSS(mVU, Fs, Ft);
			mVUclamp1(mVU, Fs, t1, 8, true);
		armAsm->Bind(&djmp);

		writeQreg(Fs, mVUinfo.writeQ);
		mVUdivCOP2Flags(mVU);

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

		mVUstoreImm32(mVUmem(&mVU.divFlag), 0); // Clear I/D flags
		testNeg(mVU, Ft, gprT1); // Check for negative sqrt

		if (CHECK_VU_OVERFLOW(mVU.index)) // Clamp infinities (only need to do positive clamp since xmmFt is positive)
		{
			armAsm->Ldr(a64::s31, a64::MemOperand(RMVUCONST, mVUconst(maxvals)));
			armAsm->Fminnm(a64::s30, Ft.S(), a64::s31);
			mVUinsLane(Ft, 0, mVUScratch1, 0);
		}
		mVUsqrtSS(Ft, Ft);
		writeQreg(Ft, mVUinfo.writeQ);
		mVUdivCOP2Flags(mVU);

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

		mVUstoreImm32(mVUmem(&mVU.divFlag), 0); // Clear I/D flags
		testNeg(mVU, Ft, gprT1); // Check for negative sqrt

		mVUsqrtSS(Ft, Ft);

		a64::Label ajmp, bjmp, cjmp, djmp;
		testZero(Ft, &ajmp); // Skip if not zero

			testZero(Fs, &bjmp); // Skip if none are
				mVUstoreImm32(mVUmem(&mVU.divFlag), divI); // Set invalid flag (0/0)
				armAsm->B(&cjmp);
			armAsm->Bind(&bjmp);
				mVUstoreImm32(mVUmem(&mVU.divFlag), divD); // Zero divide flag (only when not 0/0)
			armAsm->Bind(&cjmp);

			armAsm->And(Fs.V16B(), Fs.V16B(), mVUloadConst(mVUconst(signbit)).V16B());
			armAsm->Orr(Fs.V16B(), Fs.V16B(), mVUloadConst(mVUconst(maxvals)).V16B()); // xmmFs = +/-Max

			armAsm->B(&djmp);
		armAsm->Bind(&ajmp);
			SSE_DIVSS(mVU, Fs, Ft);
			mVUclamp1(mVU, Fs, t1, 8, true);
		armAsm->Bind(&djmp);

		writeQreg(Fs, mVUinfo.writeQ);
		mVUdivCOP2Flags(mVU);

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

// Flips xmmPQ to get a valid P instance in lane 0 (and back again).
static __fi void mVUflipPQ(mV)
{
	mVUshufD(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6);
}

static __fi void EATANhelper(microVU& mVU, const xmm& PQ, const xmm& Fs, const xmm& t1, const xmm& t2, s64 addr)
{
	SSE_MULSS(mVU, t2, Fs);
	SSE_MULSS(mVU, t2, Fs);
	mVUmovReg(t1, t2);
	mVUarithSSConst(mVUArithOp::Mul, t1, addr);
	SSE_ADDSS(mVU, PQ, t1);
}

// ToDo: Can Be Optimized Further? (takes approximately (~115 cycles + mem access time) on a c2d)
static __fi void mVU_EATAN_(mV, const xmm& PQ, const xmm& Fs, const xmm& t1, const xmm& t2)
{
	mVUinsLane(PQ, 0, Fs, 0);
	mVUarithSSConst(mVUArithOp::Mul, PQ, mVUconst(T1));
	mVUmovReg(t2, Fs);
	EATANhelper(mVU, PQ, Fs, t1, t2, mVUconst(T2));
	EATANhelper(mVU, PQ, Fs, t1, t2, mVUconst(T3));
	EATANhelper(mVU, PQ, Fs, t1, t2, mVUconst(T4));
	EATANhelper(mVU, PQ, Fs, t1, t2, mVUconst(T5));
	EATANhelper(mVU, PQ, Fs, t1, t2, mVUconst(T6));
	EATANhelper(mVU, PQ, Fs, t1, t2, mVUconst(T7));
	EATANhelper(mVU, PQ, Fs, t1, t2, mVUconst(T8));
	mVUarithSSConst(mVUArithOp::Add, PQ, mVUconst(Pi4));
	mVUflipPQ(mVU);
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVUinsLane(xmmPQ, 0, Fs, 0);
		mVUarithSSConst(mVUArithOp::Sub, Fs, mVUconst(one));
		mVUarithSSConst(mVUArithOp::Add, xmmPQ, mVUconst(one));
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
		mVUshufD(Fs, t1, 0x01);
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVUinsLane(xmmPQ, 0, Fs, 0);
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
		mVUshufD(Fs, t1, 0x02);
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVUinsLane(xmmPQ, 0, Fs, 0);
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

static __fi void eexpHelper(microVU& mVU, const xmm& Fs, const xmm& t1, const xmm& t2, s64 addr)
{
	SSE_MULSS(mVU, t2, Fs);
	mVUmovReg(t1, t2);
	mVUarithSSConst(mVUArithOp::Mul, t1, addr);
	SSE_ADDSS(mVU, xmmPQ, t1);
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVUinsLane(xmmPQ, 0, Fs, 0);
		mVUarithSSConst(mVUArithOp::Mul, xmmPQ, mVUconst(E1));
		mVUarithSSConst(mVUArithOp::Add, xmmPQ, mVUconst(one));
		mVUmovReg(t1, Fs);
		SSE_MULSS(mVU, t1, Fs);
		mVUmovReg(t2, t1);
		mVUarithSSConst(mVUArithOp::Mul, t1, mVUconst(E2));
		SSE_ADDSS(mVU, xmmPQ, t1);
		eexpHelper(mVU, Fs, t1, t2, mVUconst(E3));
		eexpHelper(mVU, Fs, t1, t2, mVUconst(E4));
		eexpHelper(mVU, Fs, t1, t2, mVUconst(E5));
		SSE_MULSS(mVU, t2, Fs);
		mVUarithSSConst(mVUArithOp::Mul, t2, mVUconst(E6));
		SSE_ADDSS(mVU, xmmPQ, t2);
		SSE_MULSS(mVU, xmmPQ, xmmPQ);
		SSE_MULSS(mVU, xmmPQ, xmmPQ);
		armAsm->Ldr(t2.S(), a64::MemOperand(RMVUCONST, mVUconst(one)));
		SSE_DIVSS(mVU, t2, xmmPQ);
		mVUinsLane(xmmPQ, 0, t2, 0);
		mVUflipPQ(mVU); // Flip back
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEEXP);
	}
	pass3 { mVUlog("EEXP P"); }
}

// sumXYZ(): PQ.x = x ^ 2 + y ^ 2 + z ^ 2
// Same summation order as dpps with mask 0x71: (x*x + y*y) + (z*z + 0)
static __fi void mVU_sumXYZ(mV, const xmm& PQ, const xmm& Fs)
{
	armAsm->Fmul(mVUVScratch1(4), Fs.V4S(), Fs.V4S());
	armAsm->Faddp(a64::s31, a64::v30.V2S());
	armAsm->Mov(a64::s30, mVUVScratch1(4), 2);
	armAsm->Fadd(a64::s30, a64::s31, a64::s30);
	mVUinsLane(PQ, 0, mVUScratch1, 0);
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		mVUsqrtSS(xmmPQ, xmmPQ);
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVUinsLane(xmmPQ, 0, Fs, 0);
		armAsm->Ldr(Fs.S(), a64::MemOperand(RMVUCONST, mVUconst(one)));
		SSE_DIVSS(mVU, Fs, xmmPQ);
		mVUinsLane(xmmPQ, 0, Fs, 0);
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		mVUsqrtSS(xmmPQ, xmmPQ);
		armAsm->Ldr(Fs.S(), a64::MemOperand(RMVUCONST, mVUconst(one)));
		SSE_DIVSS (mVU, Fs, xmmPQ);
		mVUinsLane(xmmPQ, 0, Fs, 0);
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		armAsm->Ldr(Fs.S(), a64::MemOperand(RMVUCONST, mVUconst(one)));
		SSE_DIVSS (mVU, Fs, xmmPQ);
		mVUinsLane(xmmPQ, 0, Fs, 0);
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		armAsm->And(Fs.V16B(), Fs.V16B(), mVUloadConst(mVUconst(absclip)).V16B());
		mVUsqrtSS(xmmPQ, Fs);
		armAsm->Ldr(Fs.S(), a64::MemOperand(RMVUCONST, mVUconst(one)));
		SSE_DIVSS(mVU, Fs, xmmPQ);
		mVUinsLane(xmmPQ, 0, Fs, 0);
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVUinsLane(xmmPQ, 0, Fs, 0); // pq = X
		SSE_MULSS(mVU, Fs, Fs);    // fs = X^2
		mVUmovReg(t1, Fs);         // t1 = X^2
		SSE_MULSS(mVU, Fs, xmmPQ); // fs = X^3
		mVUmovReg(t2, Fs);         // t2 = X^3
		mVUarithSSConst(mVUArithOp::Mul, Fs, mVUconst(S2)); // fs = s2 * X^3
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3

		SSE_MULSS(mVU, t2, t1);    // t2 = X^3 * X^2
		mVUarithSSConst3(mVUArithOp::Mul, Fs, t2, mVUconst(S3)); // ps = s3 * X^5
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3 + s3 * X^5

		SSE_MULSS(mVU, t2, t1);    // t2 = X^5 * X^2
		mVUarithSSConst3(mVUArithOp::Mul, Fs, t2, mVUconst(S4)); // fs = s4 * X^7
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3 + s3 * X^5 + s4 * X^7

		SSE_MULSS(mVU, t2, t1);    // t2 = X^7 * X^2
		mVUarithSSConst(mVUArithOp::Mul, t2, mVUconst(S5)); // t2 = s5 * X^9
		SSE_ADDSS(mVU, xmmPQ, t2); // pq = X + s2 * X^3 + s3 * X^5 + s4 * X^7 + s5 * X^9
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		armAsm->And(Fs.V16B(), Fs.V16B(), mVUloadConst(mVUconst(absclip)).V16B());
		mVUsqrtSS(xmmPQ, Fs);
		mVUflipPQ(mVU); // Flip back
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
		mVUflipPQ(mVU); // Flip xmmPQ to get Valid P instance
		mVUshufD(t1, Fs, 0x1b);
		SSE_ADDPS(mVU, Fs, t1);
		mVUshufD(t1, Fs, 0x01);
		SSE_ADDSS(mVU, Fs, t1);
		mVUinsLane(xmmPQ, 0, Fs, 0);
		mVUflipPQ(mVU); // Flip back
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
		const x32& dst = mVU.regAlloc->allocGPR(-1, 1, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, dst, cFLAG.read);
		armAsm->And(dst.W(), dst.W(), _Imm24_);
		armAsm->Add(dst.W(), dst.W(), 0xffffff);
		armAsm->Lsr(dst.W(), dst.W(), 24);
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
		const x32& dst = mVU.regAlloc->allocGPR(-1, 1, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, dst, cFLAG.read);
		armAsm->Eor(dst.W(), dst.W(), _Imm24_);
		armAsm->Sub(dst.W(), dst.W(), 1);
		armAsm->Lsr(dst.W(), dst.W(), 31);
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
		const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, regT, cFLAG.read);
		armAsm->And(regT.W(), regT.W(), 0xfff);
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
		const x32& dst = mVU.regAlloc->allocGPR(-1, 1, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, dst, cFLAG.read);
		armAsm->Orr(dst.W(), dst.W(), _Imm24_);
		armAsm->Add(dst.W(), dst.W(), 1);  // If 24 1's will make 25th bit 1, else 0
		armAsm->Lsr(dst.W(), dst.W(), 24); // Get the 25th bit (also clears the rest of the garbage in the reg)
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
		armAsm->Mov(gprT1.W(), _Imm24_);
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
		const x32& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
		armAsm->And(regT.W(), regT.W(), gprT1.W());
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
		const x32& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
		armAsm->Eor(regT.W(), regT.W(), gprT1.W());
		armAsm->Sub(regT.W(), regT.W(), 1);
		armAsm->Lsr(regT.W(), regT.W(), 31);
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
		const x32& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
		armAsm->Orr(regT.W(), regT.W(), gprT1.W());
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
		const x32& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGc(reg, gprT1, sFLAG.read);
		armAsm->And(reg.W(), reg.W(), _Imm12_);
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
		const x32& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGc(reg, gprT2, sFLAG.read);
		armAsm->Orr(reg.W(), reg.W(), _Imm12_);
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

		const x32& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGa(reg, sFLAG.read);
		setBitFSEQ(reg, 0x0f00); // Z  bit
		setBitFSEQ(reg, 0xf000); // S  bit
		setBitFSEQ(reg, 0x000f); // ZS bit
		setBitFSEQ(reg, 0x00f0); // SS bit
		if (imm)
			armAsm->Eor(reg.W(), reg.W(), imm);
		armAsm->Sub(reg.W(), reg.W(), 1);
		armAsm->Lsr(reg.W(), reg.W(), 31);
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
		armAsm->And(getFlagReg(sFLAG.write).W(), getFlagReg(sFLAG.write).W(), 0xfff00); // Keep Non-Sticky Bits
		if (imm)
			armAsm->Orr(getFlagReg(sFLAG.write).W(), getFlagReg(sFLAG.write).W(), imm);
		mVU.profiler.EmitOp(opFSSET);
	}
	pass3 { mVUlog("FSSET $%x", _Imm12_); }
}

//------------------------------------------------------------------
// IADD/IADDI/IADDIU/IAND/IOR/ISUB/ISUBIU
//------------------------------------------------------------------

// Loads the current instruction word (only used by the I-bit hack).
static __fi void mVUloadCurI(mV, const a64::Register& reg)
{
	armAsm->Ldr(reg, mVUmem(&curI));
}

mVUop(mVU_IADD)
{
	pass1 { mVUanalyzeIALU1(mVU, _Id_, _Is_, _It_); }
	pass2
	{
		if (_Is_ == 0 || _It_ == 0)
		{
			const x32& regS = mVU.regAlloc->allocGPR(_Is_ ? _Is_ : _It_, -1);
			const x32& regD = mVU.regAlloc->allocGPR(-1, _Id_, mVUlow.backupVI);
			armAsm->Mov(regD.W(), regS.W());
			mVU.regAlloc->clearNeeded(regD);
			mVU.regAlloc->clearNeeded(regS);
		}
		else
		{
			const x32& regT = mVU.regAlloc->allocGPR(_It_, -1);
			const x32& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
			armAsm->Add(regS.W(), regS.W(), regT.W());
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
			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				armAsm->Mov(regT.W(), static_cast<u32>(static_cast<s32>(_Imm5_)));
			}
			else
			{
				mVUloadCurI(mVU, regT.W());
				armAsm->Lsl(regT.W(), regT.W(), 21);
				armAsm->Asr(regT.W(), regT.W(), 27);
			}
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const x32& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm5_ != 0)
					armAsm->Add(regS.W(), regS.W(), static_cast<u32>(static_cast<s32>(_Imm5_)));
			}
			else
			{
				mVUloadCurI(mVU, gprT1.W());
				armAsm->Lsl(gprT1.W(), gprT1.W(), 21);
				armAsm->Asr(gprT1.W(), gprT1.W(), 27);

				armAsm->Add(regS.W(), regS.W(), gprT1.W());
			}
			mVU.regAlloc->clearNeeded(regS);
		}
		mVU.profiler.EmitOp(opIADDI);
	}
	pass3 { mVUlog("IADDI vi%02d, vi%02d, %d", _Ft_, _Fs_, _Imm5_); }
}

// Imm15 from the current instruction word (I-bit hack).
static __fi void mVUloadImm15(mV, const a64::Register& dst, const a64::Register& tmp)
{
	mVUloadCurI(mVU, dst);
	armAsm->Lsr(tmp, dst, 10);
	armAsm->And(tmp, tmp, 0x7800);
	armAsm->And(dst, dst, 0x7FF);
	armAsm->Orr(dst, dst, tmp);
}

mVUop(mVU_IADDIU)
{
	pass1 { mVUanalyzeIADDI(mVU, _Is_, _It_, _Imm15_); }
	pass2
	{
		if (_Is_ == 0)
		{
			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
				armAsm->Mov(regT.W(), _Imm15_);
			else
				mVUloadImm15(mVU, regT.W(), gprT1.W());
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const x32& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm15_ != 0)
					armAsm->Add(regS.W(), regS.W(), _Imm15_);
			}
			else
			{
				mVUloadImm15(mVU, gprT1.W(), gprT2.W());
				armAsm->Add(regS.W(), regS.W(), gprT1.W());
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
		const x32& regT = mVU.regAlloc->allocGPR(_It_, -1);
		const x32& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
		if (_It_ != _Is_)
			armAsm->And(regS.W(), regS.W(), regT.W());
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
		const x32& regT = mVU.regAlloc->allocGPR(_It_, -1);
		const x32& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
		if (_It_ != _Is_)
			armAsm->Orr(regS.W(), regS.W(), regT.W());
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
			const x32& regT = mVU.regAlloc->allocGPR(_It_, -1);
			const x32& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
			armAsm->Sub(regS.W(), regS.W(), regT.W());
			mVU.regAlloc->clearNeeded(regS);
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const x32& regD = mVU.regAlloc->allocGPR(-1, _Id_, mVUlow.backupVI);
			armAsm->Mov(regD.W(), a64::wzr);
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
		const x32& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
		if (!EmuConfig.Gamefixes.IbitHack)
		{
			if (_Imm15_ != 0)
				armAsm->Sub(regS.W(), regS.W(), _Imm15_);
		}
		else
		{
			mVUloadImm15(mVU, gprT1.W(), gprT2.W());
			armAsm->Sub(regS.W(), regS.W(), gprT1.W());
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
			const x32& regS = mVU.regAlloc->allocGPR(_Is_, -1);
			armAsm->Sxth(regS.W(), regS.W());
			if (!_XYZW_SS)
				armAsm->Dup(Ft.V4S(), regS.W());
			else
				armAsm->Fmov(Ft.S(), regS.W());
			mVU.regAlloc->clearNeeded(regS);
		}
		else
		{
			armAsm->Movi(Ft.V2D(), 0);
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
		if (_XYZW_SS)
			mVUunpack_xyzw(Ft, Fs, (_X ? 1 : (_Y ? 2 : (_Z ? 3 : 0))));
		else
			mVUshufD(Ft, Fs, 0x39);
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
		const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		armAsm->Fmov(regT.W(), Fs.S());
		mVU.regAlloc->clearNeeded(regT);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opMTIR);
	}
	pass3 { mVUlog("MTIR vi%02d, vf%02d%s", _Ft_, _Fs_, _Fsf_String); }
}

//------------------------------------------------------------------
// ILW/ILWR
//------------------------------------------------------------------

// gprT1 = VI[reg] + Imm11, fixed up to a byte offset into VU memory.
static void mVUcalcAddrImm11(mV, int vireg)
{
	mVU.regAlloc->moveVIToGPR(gprT1, vireg);
	if (!EmuConfig.Gamefixes.IbitHack)
	{
		if (_Imm11_ != 0)
			armAsm->Add(gprT1.W(), gprT1.W(), static_cast<u32>(_Imm11_));
	}
	else
	{
		mVUloadCurI(mVU, gprT2.W());
		armAsm->Sbfx(gprT2.W(), gprT2.W(), 0, 11);
		armAsm->Add(gprT1.W(), gprT1.W(), gprT2.W());
	}
	mVUaddrFix(mVU, gprT1q, gprT2q);
}

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
		std::optional<mVUAddr> optaddr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, offsetSS));
		if (!optaddr.has_value())
			mVUcalcAddrImm11(mVU, _Is_);

		const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		const mVUAddr addr = optaddr.has_value() ? optaddr.value() : mVUmemIndexed(gprT1q, offsetSS);
		armAsm->Ldrh(regT.W(), addr.mem());
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
		if (_Is_)
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			mVUaddrFix (mVU, gprT1q, gprT2q);

			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			armAsm->Ldrh(regT.W(), mVUmemIndexed(gprT1q, offsetSS).mem());
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			armAsm->Ldrh(regT.W(), mVUmemConst(offsetSS).mem());
			mVU.regAlloc->clearNeeded(regT);
		}
		mVU.profiler.EmitOp(opILWR);
	}
	pass3 { mVUlog("ILWR.%s vi%02d, vi%02d", _XYZW_String, _Ft_, _Fs_); }
}

//------------------------------------------------------------------
// ISW/ISWR
//------------------------------------------------------------------

static void mVUstoreISW(mV, const x32& regT, const mVUAddr& ptr)
{
	if (_X) armAsm->Str(regT.W(), ptr.mem(0));
	if (_Y) armAsm->Str(regT.W(), ptr.mem(4));
	if (_Z) armAsm->Str(regT.W(), ptr.mem(8));
	if (_W) armAsm->Str(regT.W(), ptr.mem(12));
}

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
		std::optional<mVUAddr> optaddr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, 0));
		if (!optaddr.has_value())
			mVUcalcAddrImm11(mVU, _Is_);

		// If regT is dirty, the high bits might not be zero.
		const x32& regT = mVU.regAlloc->allocGPR(_It_, -1, false, true);
		const mVUAddr ptr(optaddr.has_value() ? optaddr.value() : mVUmemIndexed(gprT1q, 0));
		mVUstoreISW(mVU, regT, ptr);
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
		if (_Is_)
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			mVUaddrFix(mVU, gprT1q, gprT2q);
		}
		const x32& regT = mVU.regAlloc->allocGPR(_It_, -1, false, true);
		mVUstoreISW(mVU, regT, _Is_ ? mVUmemIndexed(gprT1q, 0) : mVUmemConst(0));
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
		const std::optional<mVUAddr> optaddr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, 0));
		if (!optaddr.has_value())
			mVUcalcAddrImm11(mVU, _Is_);

		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		mVUloadReg(Ft, optaddr.has_value() ? optaddr.value() : mVUmemIndexed(gprT1q, 0), _X_Y_Z_W);
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
		s64 offset = 0;
		bool indexed = false;
		if (_Is_ || isVU0) // Access VU1 regs mem-map in !_Is_ case
		{
			const x32& regS = mVU.regAlloc->allocGPR(_Is_, _Is_, mVUlow.backupVI);
			armAsm->Sub(regS.W(), regS.W(), 1);
			armAsm->Sxth(gprT1.W(), regS.W()); // TODO: Confirm
			mVU.regAlloc->clearNeeded(regS);
			mVUaddrFix(mVU, gprT1q, gprT2q);
			indexed = true;
		}
		else
		{
			offset = (0xffff & (mVU.microMemSize - 8));
		}
		if (!mVUlow.noWriteVF)
		{
			const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
			mVUloadReg(Ft, indexed ? mVUmemIndexed(gprT1q, 0) : mVUmemConst(offset), _X_Y_Z_W);
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
		if (_Is_)
		{
			const x32& regS = mVU.regAlloc->allocGPR(_Is_, _Is_, mVUlow.backupVI);
			armAsm->Sxth(gprT1.W(), regS.W()); // TODO: Confirm
			armAsm->Add(regS.W(), regS.W(), 1);
			mVU.regAlloc->clearNeeded(regS);
			mVUaddrFix(mVU, gprT1q, gprT2q);
		}
		if (!mVUlow.noWriteVF)
		{
			const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
			mVUloadReg(Ft, _Is_ ? mVUmemIndexed(gprT1q, 0) : mVUmemConst(0), _X_Y_Z_W);
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
		const std::optional<mVUAddr> optptr(EmuConfig.Gamefixes.IbitHack ? std::nullopt : mVUoptimizeConstantAddr(mVU, _It_, _Imm11_, 0));
		if (!optptr.has_value())
			mVUcalcAddrImm11(mVU, _It_);

		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _XYZW_PS ? -1 : 0, _X_Y_Z_W);
		mVUsaveReg(Fs, optptr.has_value() ? optptr.value() : mVUmemIndexed(gprT1q, 0), _X_Y_Z_W, 1);
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
		s64 offset = 0;
		bool indexed = false;
		if (_It_ || isVU0) // Access VU1 regs mem-map in !_It_ case
		{
			const x32& regT = mVU.regAlloc->allocGPR(_It_, _It_, mVUlow.backupVI);
			armAsm->Sub(regT.W(), regT.W(), 1);
			armAsm->Uxth(gprT1.W(), regT.W());
			mVU.regAlloc->clearNeeded(regT);
			mVUaddrFix(mVU, gprT1q, gprT2q);
			indexed = true;
		}
		else
		{
			offset = (0xffff & (mVU.microMemSize - 8));
		}
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _XYZW_PS ? -1 : 0, _X_Y_Z_W);
		mVUsaveReg(Fs, indexed ? mVUmemIndexed(gprT1q, 0) : mVUmemConst(offset), _X_Y_Z_W, 1);
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
		if (_It_)
		{
			const x32& regT = mVU.regAlloc->allocGPR(_It_, _It_, mVUlow.backupVI);
			armAsm->Uxth(gprT1.W(), regT.W());
			armAsm->Add(regT.W(), regT.W(), 1);
			mVU.regAlloc->clearNeeded(regT);
			mVUaddrFix(mVU, gprT1q, gprT2q);
		}
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _XYZW_PS ? -1 : 0, _X_Y_Z_W);
		mVUsaveReg(Fs, _It_ ? mVUmemIndexed(gprT1q, 0) : mVUmemConst(0), _X_Y_Z_W, 1);
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
			armAsm->Fmov(gprT1.W(), Fs.S());
			armAsm->And(gprT1.W(), gprT1.W(), 0x007fffff);
			armAsm->Orr(gprT1.W(), gprT1.W(), 0x3f800000);
			armAsm->Str(gprT1.W(), mVUmem(Rmem));
			mVU.regAlloc->clearNeeded(Fs);
		}
		else
			mVUstoreImm32(mVUmem(Rmem), 0x3f800000);
		mVU.profiler.EmitOp(opRINIT);
	}
	pass3 { mVUlog("RINIT R, vf%02d%s", _Fs_, _Fsf_String); }
}

static __fi void mVU_RGET_(mV, const x32& Rreg)
{
	if (!mVUlow.noWriteVF)
	{
		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		if (!_XYZW_SS)
			armAsm->Dup(Ft.V4S(), Rreg.W());
		else
			armAsm->Fmov(Ft.S(), Rreg.W());
		mVU.regAlloc->clearNeeded(Ft);
	}
}

mVUop(mVU_RGET)
{
	pass1 { mVUanalyzeR2(mVU, _Ft_, true); }
	pass2
	{
		armAsm->Ldr(gprT1.W(), mVUmem(Rmem));
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
		const x32& temp3 = mVU.regAlloc->allocGPR();
		armAsm->Ldr(temp3.W(), mVUmem(Rmem));
		armAsm->Ubfx(gprT1.W(), temp3.W(), 4, 1);
		armAsm->Ubfx(gprT2.W(), temp3.W(), 22, 1);
		armAsm->Lsl(temp3.W(), temp3.W(), 1);
		armAsm->Eor(gprT1.W(), gprT1.W(), gprT2.W());
		armAsm->Eor(temp3.W(), temp3.W(), gprT1.W());
		armAsm->And(temp3.W(), temp3.W(), 0x007fffff);
		armAsm->Orr(temp3.W(), temp3.W(), 0x3f800000);
		armAsm->Str(temp3.W(), mVUmem(Rmem));
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
			armAsm->Fmov(gprT1.W(), Fs.S());
			armAsm->And(gprT1.W(), gprT1.W(), 0x7fffff);
			armAsm->Ldr(gprT2.W(), mVUmem(Rmem));
			armAsm->Eor(gprT2.W(), gprT2.W(), gprT1.W());
			armAsm->Str(gprT2.W(), mVUmem(Rmem));
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
		const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		armAsm->Ldrh(regT.W(), mVUmem(&mVU.getVifRegs().top));
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
		const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		armAsm->Ldrh(regT.W(), mVUmem(&mVU.getVifRegs().itop));
		armAsm->And(regT.W(), regT.W(), isVU1 ? 0x3ff : 0xff);
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
	u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, vuRegs[1].Mem, addr, ~0u, true);

	if (size > diff)
	{
		//DevCon.WriteLn(Color_Green, "microVU1: XGkick Wrap!");
		gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&vuRegs[1].Mem[addr], diff, true);
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[0], size - diff, true);
	}
	else
	{
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[addr], size, true);
	}
}

void _vuXGKICKTransfermVU(bool flush)
{
	while (VU1.xgkickenable && (flush || VU1.xgkickcyclecount >= 2))
	{
		u32 transfersize = 0;

		if (VU1.xgkicksizeremaining == 0)
		{
			u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, vuRegs[1].Mem, VU1.xgkickaddr, ~0u, flush);
			VU1.xgkicksizeremaining = size & 0xFFFF;
			VU1.xgkickendpacket = size >> 31;
			VU1.xgkickdiff = 0x4000 - VU1.xgkickaddr;

			if (VU1.xgkicksizeremaining == 0)
			{
				VU1.xgkickenable = false;
				break;
			}
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

		// Would be "nicer" to do the copy until it's all up, however this really screws up PATH3 masking stuff
		// So lets just do it the other way :)
		if (THREAD_VU1)
		{
			if (transfersize < VU1.xgkicksizeremaining)
				gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&VU1.Mem[VU1.xgkickaddr], transfersize, true);
			else
				gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[VU1.xgkickaddr], transfersize, true);
		}
		else
		{
			gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[VU1.xgkickaddr], transfersize, true);
		}

		if (flush)
			VU1.cycle += transfersize / 8;

		VU1.xgkickcyclecount -= transfersize / 8;

		VU1.xgkickaddr = (VU1.xgkickaddr + transfersize) & 0x3FFF;
		VU1.xgkicksizeremaining -= transfersize;
		VU1.xgkickdiff = 0x4000 - VU1.xgkickaddr;

		if (VU1.xgkickendpacket && !VU1.xgkicksizeremaining)
		{
			VU1.xgkickenable = false;
			// Check if VIF is waiting for the GIF to not be busy
		}
	}
}

static __fi void mVU_XGKICK_SYNC(mV, bool flush)
{
	mVU.regAlloc->flushCallerSavedRegisters();

	// Add the single cycle remainder after this instruction, some games do the store
	// on the second instruction after the kick and that needs to go through first
	// but that's VERY close..
	a64::Label skipxgkick, needcycles;
	armAsm->Ldr(a64::w8, mVUmem(&VU1.xgkickenable));
	armAsm->Tbz(a64::w8, 0, &skipxgkick);
	armAsm->Ldr(a64::w8, mVUmem(&VU1.xgkickcyclecount));
	armAsm->Add(a64::w8, a64::w8, static_cast<u32>(mVUlow.kickcycles - 1));
	armAsm->Str(a64::w8, mVUmem(&VU1.xgkickcyclecount));
	armAsm->Cmp(a64::w8, 2);
	armAsm->B(&needcycles, a64::lt);
	mVUbackupRegs(mVU, true, true);
	armAsm->Mov(a64::w0, static_cast<u32>(flush));
	armEmitCall(reinterpret_cast<const void*>(_vuXGKICKTransfermVU));
	mVUrestoreRegs(mVU, true, true);
	armAsm->Bind(&needcycles);
	mVUrmw32(mVUmem(&VU1.xgkickcyclecount), [](const a64::Register& r) { armAsm->Add(r, r, 1); });
	armAsm->Bind(&skipxgkick);
}

static __fi void mVU_XGKICK_DELAY(mV)
{
	mVU.regAlloc->flushCallerSavedRegisters();

	mVUbackupRegs(mVU, true, true);
	armAsm->Ldr(a64::w0, mVUmem(&mVU.VIxgkick));
	armEmitCall(reinterpret_cast<const void*>(mVU_XGKICK_));
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

		const x32& regS = mVU.regAlloc->allocGPR(_Is_, -1);
		if (!CHECK_XGKICKHACK)
		{
			armAsm->Str(regS.W(), mVUmem(&mVU.VIxgkick));
		}
		else
		{
			mVUstoreImm32(mVUmem(&VU1.xgkickenable), 1);
			mVUstoreImm32(mVUmem(&VU1.xgkickendpacket), 0);
			mVUstoreImm32(mVUmem(&VU1.xgkicksizeremaining), 0);
			mVUstoreImm32(mVUmem(&VU1.xgkickcyclecount), 0);
			armAsm->Ldr(gprT2.W(), mVUmem(&mVU.totalCycles));
			armAsm->Ldr(a64::w8, mVUmem(&mVU.cycles));
			armAsm->Sub(gprT2.W(), gprT2.W(), a64::w8);
			armAsm->Ldr(a64::w8, mVUmem(&VU1.cycle));
			armAsm->Add(gprT2.W(), gprT2.W(), a64::w8);
			armAsm->Str(gprT2.W(), mVUmem(&VU1.xgkicklastcycle));
			armAsm->And(gprT1.W(), regS.W(), 0x3FF);
			armAsm->Lsl(gprT1.W(), gprT1.W(), 4);
			armAsm->Str(gprT1.W(), mVUmem(&VU1.xgkickaddr));
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

// Branches to 'target' if the low 16 bits of gprT1 (signed) satisfy cond compared to zero.
static __fi void mVUcmpBranchValue(const a64::Register& value)
{
	armAsm->Sxth(a64::w8, value);
	armAsm->Cmp(a64::w8, 0);
}

void condEvilBranch(mV, a64::Condition JMPcc)
{
	if (mVUlow.badBranch)
	{
		armAsm->Str(gprT1.W(), mVUmem(&mVU.branch));
		mVUstoreImm32(mVUmem(&mVU.badBranch), branchAddr(mVU));

		a64::Label cJMP;
		mVUcmpBranchValue(gprT1.W());
		armAsm->B(&cJMP, JMPcc);
			incPC(4); // Branch Not Taken Addr
			mVUstoreImm32(mVUmem(&mVU.badBranch), xPC);
			incPC(-4);
		armAsm->Bind(&cJMP);
		return;
	}
	if (isEvilBlock)
	{
		mVUstoreImm32(mVUmem(&mVU.evilevilBranch), branchAddr(mVU));
		a64::Label cJMP;
		mVUcmpBranchValue(gprT1.W());
		armAsm->B(&cJMP, JMPcc);
		armAsm->Ldr(gprT1.W(), mVUmem(&mVU.evilBranch)); // Branch Not Taken
		armAsm->Add(gprT1.W(), gprT1.W(), 8); // We have already executed 1 instruction from the original branch
		armAsm->Str(gprT1.W(), mVUmem(&mVU.evilevilBranch));
		armAsm->Bind(&cJMP);
	}
	else
	{
		mVUstoreImm32(mVUmem(&mVU.evilBranch), branchAddr(mVU));
		a64::Label cJMP;
		mVUcmpBranchValue(gprT1.W());
		armAsm->B(&cJMP, JMPcc);
		armAsm->Ldr(gprT1.W(), mVUmem(&mVU.badBranch)); // Branch Not Taken
		armAsm->Add(gprT1.W(), gprT1.W(), 8); // We have already executed 1 instruction from the original branch
		armAsm->Str(gprT1.W(), mVUmem(&mVU.evilBranch));
		armAsm->Bind(&cJMP);
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
		if (mVUlow.badBranch)  { mVUstoreImm32(mVUmem(&mVU.badBranch), branchAddr(mVU)); }
		if (mVUlow.evilBranch) { if (isEvilBlock) mVUstoreImm32(mVUmem(&mVU.evilevilBranch), branchAddr(mVU)); else mVUstoreImm32(mVUmem(&mVU.evilBranch), branchAddr(mVU)); }
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
			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			armAsm->Mov(regT.W(), bSaveAddr);
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			incPC(-2);
			DevCon.Warning("Linking BAL from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
			incPC(2);

			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (isEvilBlock)
				armAsm->Ldr(regT.W(), mVUmem(&mVU.evilBranch));
			else
				armAsm->Ldr(regT.W(), mVUmem(&mVU.badBranch));

			armAsm->Add(regT.W(), regT.W(), 8);
			armAsm->Lsr(regT.W(), regT.W(), 3);
			mVU.regAlloc->clearNeeded(regT);
		}

		if (mVUlow.badBranch)  { mVUstoreImm32(mVUmem(&mVU.badBranch), branchAddr(mVU)); }
		if (mVUlow.evilBranch) { if (isEvilBlock) mVUstoreImm32(mVUmem(&mVU.evilevilBranch), branchAddr(mVU)); else mVUstoreImm32(mVUmem(&mVU.evilBranch), branchAddr(mVU)); }
		mVU.profiler.EmitOp(opBAL);
	}
	pass3 { mVUlog("BAL vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, branchAddr(mVU), branchAddr(mVU)); }
}

// Common code for the conditional branches. Computes the value to compare against zero in gprT1.
static void mVUcondBranchPass2(mV, bool twoRegs, a64::Condition cond, microOpcode op)
{
	if (mVUlow.memReadIs)
		armAsm->Ldr(gprT1.W(), mVUmem(&mVU.VIbackup));
	else
		mVU.regAlloc->moveVIToGPR(gprT1, _Is_);

	if (twoRegs)
	{
		if (mVUlow.memReadIt)
		{
			armAsm->Ldr(a64::w8, mVUmem(&mVU.VIbackup));
			armAsm->Eor(gprT1.W(), gprT1.W(), a64::w8);
		}
		else
		{
			const x32& regT = mVU.regAlloc->allocGPR(_It_);
			armAsm->Eor(gprT1.W(), gprT1.W(), regT.W());
			mVU.regAlloc->clearNeeded(regT);
		}
	}

	if (!(isBadOrEvil))
		armAsm->Str(gprT1.W(), mVUmem(&mVU.branch));
	else
		condEvilBranch(mVU, cond);
	mVU.profiler.EmitOp(op);
}

mVUop(mVU_IBEQ)
{
	setBranchA(mX, 3, 0);
	pass1 { mVUanalyzeCondBranch2(mVU, _Is_, _It_); }
	pass2 { mVUcondBranchPass2(mVU, true, a64::eq, opIBEQ); }
	pass3 { mVUlog("IBEQ vi%02d, vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBGEZ)
{
	setBranchA(mX, 4, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2 { mVUcondBranchPass2(mVU, false, a64::ge, opIBGEZ); }
	pass3 { mVUlog("IBGEZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBGTZ)
{
	setBranchA(mX, 5, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2 { mVUcondBranchPass2(mVU, false, a64::gt, opIBGTZ); }
	pass3 { mVUlog("IBGTZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBLEZ)
{
	setBranchA(mX, 6, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2 { mVUcondBranchPass2(mVU, false, a64::le, opIBLEZ); }
	pass3 { mVUlog("IBLEZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBLTZ)
{
	setBranchA(mX, 7, 0);
	pass1 { mVUanalyzeCondBranch1(mVU, _Is_); }
	pass2 { mVUcondBranchPass2(mVU, false, a64::lt, opIBLTZ); }
	pass3 { mVUlog("IBLTZ vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_IBNE)
{
	setBranchA(mX, 8, 0);
	pass1 { mVUanalyzeCondBranch2(mVU, _Is_, _It_); }
	pass2 { mVUcondBranchPass2(mVU, true, a64::ne, opIBNE); }
	pass3 { mVUlog("IBNE vi%02d, vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

void normJumpPass2(mV)
{
	if (!mVUlow.constJump.isValid || mVUlow.evilBranch)
	{
		mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
		armAsm->Lsl(gprT1.W(), gprT1.W(), 3);
		armAsm->And(gprT1.W(), gprT1.W(), mVU.microMemSize - 8);

		if (!mVUlow.evilBranch)
		{
			armAsm->Str(gprT1.W(), mVUmem(&mVU.branch));
		}
		else
		{
			if(isEvilBlock)
				armAsm->Str(gprT1.W(), mVUmem(&mVU.evilevilBranch));
			else
				armAsm->Str(gprT1.W(), mVUmem(&mVU.evilBranch));
		}
		//If delay slot is conditional, it uses badBranch to go to its target
		if (mVUlow.badBranch)
		{
			armAsm->Str(gprT1.W(), mVUmem(&mVU.badBranch));
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
			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			armAsm->Mov(regT.W(), bSaveAddr);
			mVU.regAlloc->clearNeeded(regT);
		}
		if (mVUlow.evilBranch)
		{
			const x32& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (isEvilBlock)
			{
				armAsm->Ldr(regT.W(), mVUmem(&mVU.evilBranch));
				armAsm->Add(regT.W(), regT.W(), 8);
				armAsm->Lsr(regT.W(), regT.W(), 3);
			}
			else
			{
				incPC(-2);
				DevCon.Warning("Linking JALR from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
				incPC(2);

				armAsm->Ldr(regT.W(), mVUmem(&mVU.badBranch));
				armAsm->Add(regT.W(), regT.W(), 8);
				armAsm->Lsr(regT.W(), regT.W(), 3);
			}
			mVU.regAlloc->clearNeeded(regT);
		}

		mVU.profiler.EmitOp(opJALR);
	}
	pass3 { mVUlog("JALR vi%02d, [vi%02d]", _Ft_, _Fs_); }
}
