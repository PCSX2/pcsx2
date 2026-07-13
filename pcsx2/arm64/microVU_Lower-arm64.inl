// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU - ARM64 Lower Instructions (Full Codegen)
//------------------------------------------------------------------
// pass1: Platform-independent analysis from microVU_Analyze.inl
// pass2: ARM64 NEON/GPR codegen
// pass3: Logging
// pass4: Flag exact-match hints where needed
//------------------------------------------------------------------

//------------------------------------------------------------------
// DIV/SQRT/RSQRT
//------------------------------------------------------------------

mVUop(mVU_DIV)
{
	pass1 { mVUanalyzeFDIV(mVU, _Fs_, _Fsf_, _Ft_, _Ftf_, 7); }
	pass2
	{
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(_Ft_, 0, (1 << (3 - _Ftf_)));
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::VRegister& t1 = mVU.regAlloc->allocReg();
		const a64::SRegister sFt(Ft.GetCode());
		const a64::SRegister sFs(Fs.GetCode());

		// Test if Ft is zero (NaN takes the not-zero branch — matches Fcmeq path).
		armAsm->Fcmp(sFt, 0.0);
		a64::Label ftNotZero, divDone;
		armAsm->B(&ftNotZero, a64::ne); // Skip if Ft != 0

		// Ft is zero -- check Fs
		armAsm->Fcmp(sFs, 0.0);
		a64::Label fsNotZero;
		armAsm->B(&fsNotZero, a64::ne); // Skip if Fs != 0

		// 0/0 => Invalid
		armAsm->Mov(gprT1.W(), divI);
		mVUstrField(mVU, gprT1, &mVU.divFlag);
		a64::Label afterDivFlag;
		armAsm->B(&afterDivFlag);

		armAsm->Bind(&fsNotZero);
		// Non-zero / 0 => Divide by zero
		armAsm->Mov(gprT1.W(), divD);
		mVUstrField(mVU, gprT1, &mVU.divFlag);

		armAsm->Bind(&afterDivFlag);
		// Result = +/- fmax: sign(Fs) XOR sign(Ft), magnitude = fmax
		armAsm->Eor(Fs.V16B(), Fs.V16B(), Ft.V16B());
		armAsm->Ldr(t1, mVUglobMem(&mVUglob.signbit[0]));
		armAsm->And(Fs.V16B(), Fs.V16B(), t1.V16B());
		armAsm->Ldr(t1, mVUglobMem(&mVUglob.maxvals[0]));
		armAsm->Orr(Fs.V16B(), Fs.V16B(), t1.V16B());
		a64::Label skipNormalDiv;
		armAsm->B(&skipNormalDiv);

		armAsm->Bind(&ftNotZero);
		// Normal division
		armAsm->Mov(gprT1.W(), 0);
		mVUstrField(mVU, gprT1, &mVU.divFlag);
		armAsm->Fdiv(sFs, sFs, sFt);
		mVUclamp1(mVU, Fs, t1, 8, true);

		armAsm->Bind(&skipNormalDiv);

		writeQreg(Fs, mVUinfo.writeQ);

		if (mVU.cop2)
		{
			armAsm->Bic(gprF0, gprF0, 0xc0000);
			mVUldrField(mVU, gprT1, &mVU.divFlag);
			armAsm->Orr(gprF0, gprF0, gprT1.W());
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
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(_Ft_, 0, (1 << (3 - _Ftf_)));
		const a64::SRegister sFt(Ft.GetCode());

		// Clear divFlag
		armAsm->Mov(gprT1.W(), 0);
		mVUstrField(mVU, gprT1, &mVU.divFlag);

		// Check for negative: if sign bit set, set I flag and make positive
		armAsm->Umov(gprT1.W(), Ft.V4S(), 0);
		armAsm->Tst(gprT1.W(), 0x80000000u);
		a64::Label notNeg;
		armAsm->B(&notNeg, a64::eq);
		armAsm->Mov(gprT1.W(), divI);
		mVUstrField(mVU, gprT1, &mVU.divFlag);
		armAsm->Ldr(RQSCRATCH, mVUglobMem(&mVUglob.absclip[0]));
		armAsm->And(Ft.V16B(), Ft.V16B(), RQSCRATCH.V16B());
		armAsm->Bind(&notNeg);

		// Clamp infinity. Fminnm (number-preserving) so positive-NaN inputs
		// clamp to +maxfloat instead of propagating into Fsqrt — matches
		// mVUclamp1's semantics (see microVU_Clamp-arm64.inl).
		if (CHECK_VU_OVERFLOW(mVU.index))
		{
			armAsm->Ldr(RQSCRATCH, mVUglobMem(&mVUglob.maxvals[0]));
			armAsm->Fminnm(sFt, sFt, a64::SRegister(RQSCRATCH.GetCode()));
		}

		armAsm->Fsqrt(sFt, sFt);
		writeQreg(Ft, mVUinfo.writeQ);

		if (mVU.cop2)
		{
			armAsm->Bic(gprF0, gprF0, 0xc0000);
			mVUldrField(mVU, gprT1, &mVU.divFlag);
			armAsm->Orr(gprF0, gprF0, gprT1.W());
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(_Ft_, 0, (1 << (3 - _Ftf_)));
		const a64::VRegister& t1 = mVU.regAlloc->allocReg();
		const a64::SRegister sFs(Fs.GetCode());
		const a64::SRegister sFt(Ft.GetCode());

		// Clear divFlag
		armAsm->Mov(gprT1.W(), 0);
		mVUstrField(mVU, gprT1, &mVU.divFlag);

		// Check for negative Ft: if sign bit set, set I flag and make positive
		armAsm->Umov(gprT1.W(), Ft.V4S(), 0);
		armAsm->Tst(gprT1.W(), 0x80000000u);
		a64::Label notNeg;
		armAsm->B(&notNeg, a64::eq);
		armAsm->Mov(gprT1.W(), divI);
		mVUstrField(mVU, gprT1, &mVU.divFlag);
		armAsm->Ldr(RQSCRATCH, mVUglobMem(&mVUglob.absclip[0]));
		armAsm->And(Ft.V16B(), Ft.V16B(), RQSCRATCH.V16B());
		armAsm->Bind(&notNeg);

		armAsm->Fsqrt(sFt, sFt);

		// Test if sqrt(Ft) is zero (NaN takes the not-zero branch — matches Fcmeq path).
		armAsm->Fcmp(sFt, 0.0);
		a64::Label sqrtNotZero, rsqrtDone;
		armAsm->B(&sqrtNotZero, a64::ne);

		// sqrt(Ft) is zero -- check Fs
		armAsm->Fcmp(sFs, 0.0);
		a64::Label fsNotZero2;
		armAsm->B(&fsNotZero2, a64::ne);

		// 0/0 => Invalid
		armAsm->Mov(gprT1.W(), divI);
		mVUstrField(mVU, gprT1, &mVU.divFlag);
		a64::Label afterFlag2;
		armAsm->B(&afterFlag2);

		armAsm->Bind(&fsNotZero2);
		armAsm->Mov(gprT1.W(), divD);
		mVUstrField(mVU, gprT1, &mVU.divFlag);

		armAsm->Bind(&afterFlag2);
		// Result = sign(Fs) | fmax
		armAsm->Ldr(t1, mVUglobMem(&mVUglob.signbit[0]));
		armAsm->And(Fs.V16B(), Fs.V16B(), t1.V16B());
		armAsm->Ldr(t1, mVUglobMem(&mVUglob.maxvals[0]));
		armAsm->Orr(Fs.V16B(), Fs.V16B(), t1.V16B());
		armAsm->B(&rsqrtDone);

		armAsm->Bind(&sqrtNotZero);
		armAsm->Fdiv(sFs, sFs, sFt);
		mVUclamp1(mVU, Fs, t1, 8, true);

		armAsm->Bind(&rsqrtDone);
		writeQreg(Fs, mVUinfo.writeQ);

		if (mVU.cop2)
		{
			armAsm->Bic(gprF0, gprF0, 0xc0000);
			mVUldrField(mVU, gprT1, &mVU.divFlag);
			armAsm->Orr(gprF0, gprF0, gprT1.W());
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
// EFU ops are VU1-only extended math. Ported from x86 microVU_Lower.inl.
// qmmPQ layout: [0]=Q, [1]=pending_q, [2]=P, [3]=pending_p.
//
// x86 uses PSHUFD to bring the P-write lane to position 0 for xMUL.SS/xADD.SS
// (which preserve upper lanes), then flips back. On ARM64, scalar FP ops
// (FMUL Sd,Sn,Sm etc.) ZERO the upper 96 bits of the destination V register.
// So we can't accumulate directly into qmmPQ — we'd destroy Q/pending_q/pending_p.
// Instead we accumulate in a scratch VRegister (`pq`) and Ins into qmmPQ at
// the end. Target lane is 2 (P) when writeP=false, 3 (pending_p) when true.

// NEON equivalent of SSE_DIVSS (scalar divide with clamping)
static __fi void NEON_DIVSS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	mVUclamp3(mVU, to, RQSCRATCH3, 0x8);
	mVUclamp3(mVU, from, RQSCRATCH3, 0x8);
	armAsm->Fdiv(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()),
		a64::SRegister(from.GetCode()));
	mVUclamp4(mVU, to, RQSCRATCH3, 0x8);
}

// Scalar lane-0 multiply by 32-bit float constant at addr (raw xMUL.SS equiv, no clamping).
// Uses RQSCRATCH internally so `to` may alias any caller-owned scratch.
static __fi void mVUmulSSConst(const a64::VRegister& to, const void* addr)
{
	armAsm->Ldr(a64::SRegister(RQSCRATCH.GetCode()), mVUglobMem(addr));
	armAsm->Fmul(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()),
		a64::SRegister(RQSCRATCH.GetCode()));
}

// Scalar lane-0 add of 32-bit float constant at addr (raw xADD.SS equiv, no clamping).
static __fi void mVUaddSSConst(const a64::VRegister& to, const void* addr)
{
	armAsm->Ldr(a64::SRegister(RQSCRATCH.GetCode()), mVUglobMem(addr));
	armAsm->Fadd(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()),
		a64::SRegister(RQSCRATCH.GetCode()));
}

// Scalar lane-0 subtract 32-bit float constant at addr (raw xSUB.SS equiv, no clamping).
static __fi void mVUsubSSConst(const a64::VRegister& to, const void* addr)
{
	armAsm->Ldr(a64::SRegister(RQSCRATCH.GetCode()), mVUglobMem(addr));
	armAsm->Fsub(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()),
		a64::SRegister(RQSCRATCH.GetCode()));
}

// xMOVAPS reg,reg — full 128-bit copy.
static __fi void mVUmovAPSReg(const a64::VRegister& dst, const a64::VRegister& src)
{
	armAsm->Mov(dst.V16B(), src.V16B());
}

// Copy src lane 0 into qmmPQ at the P-write target lane (2 or 3).
static __fi void mVUwritePQresult(const a64::VRegister& src, bool writeP)
{
	const int targetLane = writeP ? 3 : 2;
	armAsm->Ins(qmmPQ.V4S(), targetLane, src.V4S(), 0);
}

// sumXYZ: dst[0] = Fs.x*Fs.x + Fs.y*Fs.y + Fs.z*Fs.z. Trashes Fs.
// Matches x86 DPPS 0x71 + MOVSS semantics. AArch64 NEON has no FADDV
// across-vector reduction for floats, so fall back to two FADDP passes
// (the standard pattern). Skip the final Ins when dst == Fs (single
// caller, ESUM, passes the same register for both).
static __fi void mVU_sumXYZ_arm(const a64::VRegister& dst, const a64::VRegister& Fs)
{
	armAsm->Fmul(Fs.V4S(), Fs.V4S(), Fs.V4S());
	armAsm->Ins(Fs.V4S(), 3, a64::wzr); // zero W lane
	armAsm->Faddp(Fs.V4S(), Fs.V4S(), Fs.V4S());
	armAsm->Faddp(Fs.V4S(), Fs.V4S(), Fs.V4S());
	if (dst.GetCode() != Fs.GetCode())
		armAsm->Ins(dst.V4S(), 0, Fs.V4S(), 0);
}

// EATAN polynomial helper: pq[0] += Fs^(2*n+1) * T_n (Taylor-like series).
// Matches the x86 EATANhelper macro.
// All scalar math operates on lane 0 of `pq` (a scratch, NOT qmmPQ).
#define EATANhelper_arm(addr) \
	do { \
		NEON_MULSS(mVU, t2, Fs); \
		NEON_MULSS(mVU, t2, Fs); \
		mVUmovAPSReg(t1, t2); \
		mVUmulSSConst(t1, (addr)); \
		NEON_ADDSS(mVU, pq, t1); \
	} while (0)

static __fi void mVU_EATAN_arm(mV, const a64::VRegister& pq, const a64::VRegister& Fs,
	const a64::VRegister& t1, const a64::VRegister& t2)
{
	armAsm->Ins(pq.V4S(), 0, Fs.V4S(), 0);
	mVUmulSSConst(pq, &mVUglob.T1[0]);
	mVUmovAPSReg(t2, Fs);
	EATANhelper_arm(&mVUglob.T2[0]);
	EATANhelper_arm(&mVUglob.T3[0]);
	EATANhelper_arm(&mVUglob.T4[0]);
	EATANhelper_arm(&mVUglob.T5[0]);
	EATANhelper_arm(&mVUglob.T6[0]);
	EATANhelper_arm(&mVUglob.T7[0]);
	EATANhelper_arm(&mVUglob.T8[0]);
	mVUaddSSConst(pq, &mVUglob.Pi4[0]);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::VRegister& pq = mVU.regAlloc->allocReg(); // scratch P accumulator
		const a64::VRegister& t1 = mVU.regAlloc->allocReg();
		const a64::VRegister& t2 = mVU.regAlloc->allocReg();
		// pq[0] = Fs[0] + 1; Fs[0] -= 1; Fs = (Fs-1)/(Fs+1)
		armAsm->Ins(pq.V4S(), 0, Fs.V4S(), 0);
		mVUsubSSConst(Fs, &mVUglob.one[0]);
		mVUaddSSConst(pq, &mVUglob.one[0]);
		NEON_DIVSS(mVU, Fs, pq);
		mVU_EATAN_arm(mVU, pq, Fs, t1, t2);
		mVUwritePQresult(pq, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& t1 = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const a64::VRegister& Fs = mVU.regAlloc->allocReg();
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		const a64::VRegister& t2 = mVU.regAlloc->allocReg();
		// x86: PSHUFD(Fs, t1, 0x01) broadcasts t1.y — only lane 0 is read later.
		armAsm->Ins(Fs.V4S(), 0, t1.V4S(), 1);
		armAsm->Ins(pq.V4S(), 0, Fs.V4S(), 0);      // pq[0] = Fs[0]   (= VF.y)
		NEON_SUBSS(mVU, Fs, t1);                    // Fs[0] = y - x
		NEON_ADDSS(mVU, t1, pq);                    // t1[0] = x + y
		NEON_DIVSS(mVU, Fs, t1);                    // Fs[0] = (y-x)/(y+x)
		mVU_EATAN_arm(mVU, pq, Fs, t1, t2);
		mVUwritePQresult(pq, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& t1 = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const a64::VRegister& Fs = mVU.regAlloc->allocReg();
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		const a64::VRegister& t2 = mVU.regAlloc->allocReg();
		armAsm->Ins(Fs.V4S(), 0, t1.V4S(), 2);      // Fs[0] = VF.z
		armAsm->Ins(pq.V4S(), 0, Fs.V4S(), 0);
		NEON_SUBSS(mVU, Fs, t1);                    // z - x
		NEON_ADDSS(mVU, t1, pq);                    // z + x
		NEON_DIVSS(mVU, Fs, t1);
		mVU_EATAN_arm(mVU, pq, Fs, t1, t2);
		mVUwritePQresult(pq, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEATANxz);
	}
	pass3 { mVUlog("EATANxz P"); }
}

// EEXP polynomial helper: pq[0] += (Fs^n) * E_n.
// Matches the x86 eexpHelper macro.
#define eexpHelper_arm(addr) \
	do { \
		NEON_MULSS(mVU, t2, Fs); \
		mVUmovAPSReg(t1, t2); \
		mVUmulSSConst(t1, (addr)); \
		NEON_ADDSS(mVU, pq, t1); \
	} while (0)

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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		const a64::VRegister& t1 = mVU.regAlloc->allocReg();
		const a64::VRegister& t2 = mVU.regAlloc->allocReg();
		armAsm->Ins(pq.V4S(), 0, Fs.V4S(), 0);   // pq = Fs
		mVUmulSSConst(pq, &mVUglob.E1[0]);   // pq *= E1
		mVUaddSSConst(pq, &mVUglob.one[0]);  // pq += 1
		mVUmovAPSReg(t1, Fs);
		NEON_MULSS(mVU, t1, Fs);             // t1 = Fs^2
		mVUmovAPSReg(t2, t1);                // t2 = Fs^2
		mVUmulSSConst(t1, &mVUglob.E2[0]);
		NEON_ADDSS(mVU, pq, t1);
		eexpHelper_arm(&mVUglob.E3[0]);
		eexpHelper_arm(&mVUglob.E4[0]);
		eexpHelper_arm(&mVUglob.E5[0]);
		NEON_MULSS(mVU, t2, Fs);
		mVUmulSSConst(t2, &mVUglob.E6[0]);
		NEON_ADDSS(mVU, pq, t2);
		NEON_MULSS(mVU, pq, pq);
		NEON_MULSS(mVU, pq, pq);
		// pq[0] = 1 / pq[0]^4
		armAsm->Ldr(a64::SRegister(t2.GetCode()), mVUglobMem(&mVUglob.one[0]));
		NEON_DIVSS(mVU, t2, pq);
		mVUwritePQresult(t2, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEEXP);
	}
	pass3 { mVUlog("EEXP P"); }
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		mVU_sumXYZ_arm(pq, Fs);
		armAsm->Fsqrt(a64::SRegister(pq.GetCode()), a64::SRegister(pq.GetCode()));
		mVUwritePQresult(pq, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		// Fs is reused after pq is filled (Fs[0] := 1.0, then 1/pq[0]). Guard
		// the bound-vs-scratch allocator invariant in debug builds.
		pxAssert(Fs.GetCode() != pq.GetCode());
		armAsm->Ins(pq.V4S(), 0, Fs.V4S(), 0);               // pq[0] = Fs[0]
		armAsm->Ldr(a64::SRegister(Fs.GetCode()), mVUglobMem(&mVUglob.one[0])); // Fs[0] = 1.0
		NEON_DIVSS(mVU, Fs, pq);                              // Fs[0] = 1 / pq[0]
		mVUwritePQresult(Fs, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		// pq is filled from Fs (sumXYZ) then Fs[0] := 1.0 — Fs and pq must
		// be different NEON registers. mVU_sumXYZ_arm squares Fs in-place.
		pxAssert(Fs.GetCode() != pq.GetCode());
		mVU_sumXYZ_arm(pq, Fs);
		armAsm->Fsqrt(a64::SRegister(pq.GetCode()), a64::SRegister(pq.GetCode()));
		armAsm->Ldr(a64::SRegister(Fs.GetCode()), mVUglobMem(&mVUglob.one[0]));
		NEON_DIVSS(mVU, Fs, pq);
		mVUwritePQresult(Fs, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		pxAssert(Fs.GetCode() != pq.GetCode());
		mVU_sumXYZ_arm(pq, Fs);
		armAsm->Ldr(a64::SRegister(Fs.GetCode()), mVUglobMem(&mVUglob.one[0]));
		NEON_DIVSS(mVU, Fs, pq);
		mVUwritePQresult(Fs, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		pxAssert(Fs.GetCode() != pq.GetCode());
		armAsm->Ldr(RQSCRATCH, mVUglobMem(&mVUglob.absclip[0]));
		armAsm->And(Fs.V16B(), Fs.V16B(), RQSCRATCH.V16B());
		armAsm->Fsqrt(a64::SRegister(pq.GetCode()), a64::SRegister(Fs.GetCode()));
		armAsm->Ldr(a64::SRegister(Fs.GetCode()), mVUglobMem(&mVUglob.one[0]));
		NEON_DIVSS(mVU, Fs, pq);
		mVUwritePQresult(Fs, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		mVU_sumXYZ_arm(Fs, Fs);
		mVUwritePQresult(Fs, mVUinfo.writeP);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::VRegister& pq = mVU.regAlloc->allocReg();
		const a64::VRegister& t1 = mVU.regAlloc->allocReg();
		const a64::VRegister& t2 = mVU.regAlloc->allocReg();
		// pq = X
		armAsm->Ins(pq.V4S(), 0, Fs.V4S(), 0);
		NEON_MULSS(mVU, Fs, Fs);                 // Fs = X^2
		mVUmovAPSReg(t1, Fs);                    // t1 = X^2
		NEON_MULSS(mVU, Fs, pq);                 // Fs = X^3
		mVUmovAPSReg(t2, Fs);                    // t2 = X^3
		mVUmulSSConst(Fs, &mVUglob.S2[0]);       // Fs = s2 * X^3
		NEON_ADDSS(mVU, pq, Fs);                 // pq = X + s2*X^3

		NEON_MULSS(mVU, t2, t1);                 // t2 = X^5
		mVUmovAPSReg(Fs, t2);
		mVUmulSSConst(Fs, &mVUglob.S3[0]);       // Fs = s3*X^5
		NEON_ADDSS(mVU, pq, Fs);

		NEON_MULSS(mVU, t2, t1);                 // t2 = X^7
		mVUmovAPSReg(Fs, t2);
		mVUmulSSConst(Fs, &mVUglob.S4[0]);       // Fs = s4*X^7
		NEON_ADDSS(mVU, pq, Fs);

		NEON_MULSS(mVU, t2, t1);                 // t2 = X^9
		mVUmulSSConst(t2, &mVUglob.S5[0]);       // t2 = s5*X^9
		NEON_ADDSS(mVU, pq, t2);

		mVUwritePQresult(pq, mVUinfo.writeP);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(pq);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		armAsm->Ldr(RQSCRATCH, mVUglobMem(&mVUglob.absclip[0]));
		armAsm->And(Fs.V16B(), Fs.V16B(), RQSCRATCH.V16B());
		armAsm->Fsqrt(a64::SRegister(Fs.GetCode()), a64::SRegister(Fs.GetCode()));
		mVUwritePQresult(Fs, mVUinfo.writeP);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		const a64::VRegister& t1 = mVU.regAlloc->allocReg();
		// x86: PSHUFD(t1, Fs, 0x1b) reverses lanes: t1 = [Fs[3], Fs[2], Fs[1], Fs[0]]
		armAsm->Rev64(t1.V4S(), Fs.V4S());               // t1 = [Fs[1], Fs[0], Fs[3], Fs[2]]
		armAsm->Ext(t1.V16B(), t1.V16B(), t1.V16B(), 8); // rotate: [Fs[3], Fs[2], Fs[1], Fs[0]]
		NEON_ADDPS(mVU, Fs, t1);                         // Fs = Fs + reverse(Fs) — lane 0 holds (x+w)
		// x86: PSHUFD(t1, Fs, 0x01) — only lane 0 used: t1[0] = Fs[1] (= y+z)
		armAsm->Ins(t1.V4S(), 0, Fs.V4S(), 1);
		NEON_ADDSS(mVU, Fs, t1);                         // Fs[0] = x+y+z+w
		mVUwritePQresult(Fs, mVUinfo.writeP);
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
		// vi01 = ((clip & imm24) != 0) ? 1 : 0.  Use Tst+Cset.
		if (_Imm24_)
		{
			armAsm->Tst(dst.W(), _Imm24_);
			armAsm->Cset(dst.W(), a64::ne);
		}
		else
		{
			armAsm->Mov(dst.W(), 0);
		}
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
		armAsm->Mov(gprT1.W(), _Imm24_);
		armAsm->Eor(dst.W(), dst.W(), gprT1.W());
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
		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
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
		const a64::Register& dst = mVU.regAlloc->allocGPR(-1, 1, mVUlow.backupVI);
		mVUallocCFLAGa(mVU, dst, cFLAG.read);
		armAsm->Mov(gprT1.W(), _Imm24_);
		armAsm->Orr(dst.W(), dst.W(), gprT1.W());
		armAsm->Add(dst.W(), dst.W(), 1);
		armAsm->Lsr(dst.W(), dst.W(), 24);
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
		const a64::Register& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
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
		const a64::Register& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
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
		const a64::Register& regT = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
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
		const a64::Register& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGc(reg, gprT1, sFLAG.read);
		if (_Imm12_)
		{
			armAsm->Mov(gprT1.W(), _Imm12_);
			armAsm->And(reg.W(), reg.W(), gprT1.W());
		}
		else
		{
			armAsm->Mov(reg.W(), 0);
		}
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
		if (_Imm12_)
		{
			armAsm->Mov(gprT1.W(), _Imm12_);
			armAsm->Orr(reg.W(), reg.W(), gprT1.W());
		}
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
		if (_Imm12_ & 0x0c30) DevCon.WriteLn(Color_Green, "mVU_FSEQ: Checking I/D/IS/DS Flags");
		if (_Imm12_ & 0x030c) DevCon.WriteLn(Color_Green, "mVU_FSEQ: Checking U/O/US/OS Flags");

		// Use mVUallocSFLAGc which normalizes the flag, then compare with immediate
		const a64::Register& reg = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		mVUallocSFLAGc(reg, gprT1, sFLAG.read);
		if (_Imm12_)
		{
			armAsm->Mov(gprT1.W(), _Imm12_);
			armAsm->Eor(reg.W(), reg.W(), gprT1.W());
		}
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
		// Build denormalized sticky bits from _Imm12_
		int imm = 0;
		if (_Imm12_ & 0x0040) imm |= 0x000000f; // ZS
		if (_Imm12_ & 0x0080) imm |= 0x00000f0; // SS
		if (_Imm12_ & 0x0100) imm |= 0x0400000; // US
		if (_Imm12_ & 0x0200) imm |= 0x0800000; // OS
		if (_Imm12_ & 0x0400) imm |= 0x1000000; // IS
		if (_Imm12_ & 0x0800) imm |= 0x2000000; // DS

		if (!(sFLAG.doFlag || mVUinfo.doDivFlag))
		{
			mVUallocSFLAGa(getFlagReg(sFLAG.write), sFLAG.lastWrite);
		}
		armAsm->Mov(gprT1.W(), 0xfff00u);
		armAsm->And(getFlagReg(sFLAG.write), getFlagReg(sFLAG.write), gprT1.W());
		if (imm)
		{
			armAsm->Mov(gprT1.W(), imm);
			armAsm->Orr(getFlagReg(sFLAG.write), getFlagReg(sFLAG.write), gprT1.W());
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
			armAsm->Mov(regD.W(), regS.W());
			mVU.regAlloc->clearNeeded(regD);
			mVU.regAlloc->clearNeeded(regS);
		}
		else
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1);
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
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
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm5_ != 0)
					armAsm->Mov(regT.W(), (u32)(s32)_Imm5_);
				else
					armAsm->Mov(regT.W(), 0);
			}
			else
			{
				// IbitHack: reconstruct signed Imm5 from the live opcode word at
				// runtime. _Imm5_ takes bit 10 of curI as sign and bits 9:6 as
				// magnitude (matching the _Imm5_ macro), so emit the
				// sbfx+and+bfxil idiom.
				armLoadPtr(gprT1, &curI);
				armAsm->Sbfx(regT.W(), gprT1.W(), 10, 1);
				armAsm->And(regT.W(), regT.W(), 0xfff0);
				armAsm->Bfxil(regT.W(), gprT1.W(), 6, 4);
			}
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm5_ != 0)
				{
					s16 imm = _Imm5_;
					if (imm >= 0)
						armAsm->Add(regS.W(), regS.W(), (u32)imm);
					else
						armAsm->Sub(regS.W(), regS.W(), (u32)(-imm));
				}
			}
			else
			{
				armLoadPtr(gprT1, &curI);
				armAsm->Sbfx(gprT2.W(), gprT1.W(), 10, 1);
				armAsm->And(gprT2.W(), gprT2.W(), 0xfff0);
				armAsm->Bfxil(gprT2.W(), gprT1.W(), 6, 4);
				armAsm->Add(regS.W(), regS.W(), gprT2.W());
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
				if (_Imm15_ != 0)
					armAsm->Mov(regT.W(), _Imm15_);
				else
					armAsm->Mov(regT.W(), 0);
			}
			else
			{
				// IbitHack: the game patches the I-bit immediate field in micro
				// memory between block runs without invalidating the JIT cache, so
				// the JIT must reconstruct Imm15 from the live opcode word.
				// Imm15 = ((curI >> 21) & 0xf) << 11 | (curI & 0x7ff), expressed
				// via ubfx+and+bfxil.
				armLoadPtr(gprT1, &curI);
				armAsm->Ubfx(regT.W(), gprT1.W(), 10, 22);
				armAsm->And(regT.W(), regT.W(), 0x7800);
				armAsm->Bfxil(regT.W(), gprT1.W(), 0, 11);
			}
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _It_, mVUlow.backupVI);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm15_ != 0)
				{
					armAsm->Mov(gprT1.W(), _Imm15_);
					armAsm->Add(regS.W(), regS.W(), gprT1.W());
				}
			}
			else
			{
				armLoadPtr(gprT1, &curI);
				armAsm->Ubfx(gprT2.W(), gprT1.W(), 10, 22);
				armAsm->And(gprT2.W(), gprT2.W(), 0x7800);
				armAsm->Bfxil(gprT2.W(), gprT1.W(), 0, 11);
				armAsm->Add(regS.W(), regS.W(), gprT2.W());
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
		const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1);
		const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
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
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1);
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Id_, mVUlow.backupVI);
			armAsm->Sub(regS.W(), regS.W(), regT.W());
			mVU.regAlloc->clearNeeded(regS);
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			const a64::Register& regD = mVU.regAlloc->allocGPR(-1, _Id_, mVUlow.backupVI);
			armAsm->Mov(regD.W(), 0);
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
			if (_Imm15_ != 0)
			{
				armAsm->Mov(gprT1.W(), _Imm15_);
				armAsm->Sub(regS.W(), regS.W(), gprT1.W());
			}
		}
		else
		{
			armLoadPtr(gprT1, &curI);
			armAsm->Ubfx(gprT2.W(), gprT1.W(), 10, 22);
			armAsm->And(gprT2.W(), gprT2.W(), 0x7800);
			armAsm->Bfxil(gprT2.W(), gprT1.W(), 0, 11);
			armAsm->Sub(regS.W(), regS.W(), gprT2.W());
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
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		if (_Is_ != 0)
		{
			// Load VI[Is] sign-extended to 32-bit, then broadcast
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_, true); // sign-extend
			armAsm->Dup(Ft.V4S(), gprT1.W());
		}
		else
		{
			armAsm->Movi(Ft.V16B(), 0);
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
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_);
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		if (_XYZW_SS)
		{
			// Single component: pick the rotated lane
			// MR32 rotates XYZW -> YZWX
			// If writing X, read Y (lane 1); Y->Z (lane 2); Z->W (lane 3); W->X (lane 0)
			mVUunpack_xyzw(Ft, Fs, (_X ? 1 : (_Y ? 2 : (_Z ? 3 : 0))));
		}
		else
		{
			// EXT #4 rotates left by 1 lane: [X,Y,Z,W] -> [Y,Z,W,X]
			armAsm->Ext(Ft.V16B(), Fs.V16B(), Fs.V16B(), 4);
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
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		armAsm->Umov(regT.W(), Fs.V4S(), 0);
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
		// Compute address: (VI[Is] + Imm11) wrapped, then byte offset
		if (!mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, offsetSS, gprT1q))
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm11_ != 0)
				{
					s32 imm = _Imm11_;
					if (imm >= 0)
						armAsm->Add(gprT1.W(), gprT1.W(), (u32)imm);
					else
						armAsm->Sub(gprT1.W(), gprT1.W(), (u32)(-imm));
				}
			}
			else
			{
				// IbitHack: reconstruct signed Imm11 from the live opcode word at
				// runtime via sbfx+bfxil.
				armLoadPtr(RWSCRATCH, &curI);
				armAsm->Sbfx(gprT2.W(), RWSCRATCH, 10, 1);
				armAsm->Bfxil(gprT2.W(), RWSCRATCH, 0, 10);
				armAsm->Add(gprT1.W(), gprT1.W(), gprT2.W());
			}
			mVUaddrFix(mVU, gprT1);

			// Add lane offset for the selected component
			armAsm->Add(gprT1.W(), gprT1.W(), offsetSS);

			// Add VU memory base
			armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
			armAsm->Add(gprT1q, gprT2q, gprT1q.X());
		}

		// Load 16-bit value from memory
		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		armAsm->Ldrh(regT.W(), a64::MemOperand(gprT1q));
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
			mVUaddrFix(mVU, gprT1);
		}
		else
		{
			armAsm->Mov(gprT1.W(), 0);
		}

		armAsm->Add(gprT1.W(), gprT1.W(), offsetSS);
		armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
		armAsm->Add(gprT1q, gprT2q, gprT1q.X());

		const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
		armAsm->Ldrh(regT.W(), a64::MemOperand(gprT1q));
		mVU.regAlloc->clearNeeded(regT);
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
		// Compute address
		if (!mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, 0, gprT1q))
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm11_ != 0)
				{
					s32 imm = _Imm11_;
					if (imm >= 0)
						armAsm->Add(gprT1.W(), gprT1.W(), (u32)imm);
					else
						armAsm->Sub(gprT1.W(), gprT1.W(), (u32)(-imm));
				}
			}
			else
			{
				// IbitHack: reconstruct signed Imm11 from the live opcode word at runtime.
				armLoadPtr(RWSCRATCH, &curI);
				armAsm->Sbfx(gprT2.W(), RWSCRATCH, 10, 1);
				armAsm->Bfxil(gprT2.W(), RWSCRATCH, 0, 10);
				armAsm->Add(gprT1.W(), gprT1.W(), gprT2.W());
			}
			mVUaddrFix(mVU, gprT1);

			armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
			armAsm->Add(gprT1q, gprT2q, gprT1q.X());
		}

		// Load VI[It] value (zero-extended to 32-bit) and store to selected lanes
		const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1, false, true);
		if (_X) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 0));
		if (_Y) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 4));
		if (_Z) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 8));
		if (_W) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 12));
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
			mVUaddrFix(mVU, gprT1);
		}
		else
		{
			armAsm->Mov(gprT1.W(), 0);
		}

		armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
		armAsm->Add(gprT1q, gprT2q, gprT1q.X());

		const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, -1, false, true);
		if (_X) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 0));
		if (_Y) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 4));
		if (_Z) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 8));
		if (_W) armAsm->Str(regT.W(), a64::MemOperand(gprT1q, 12));
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
		// Compute address: (VI[Is] + Imm11) wrapped
		if (!mVUoptimizeConstantAddr(mVU, _Is_, _Imm11_, 0, gprT1q))
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm11_ != 0)
				{
					s32 imm = _Imm11_;
					if (imm >= 0)
						armAsm->Add(gprT1.W(), gprT1.W(), (u32)imm);
					else
						armAsm->Sub(gprT1.W(), gprT1.W(), (u32)(-imm));
				}
			}
			else
			{
				// IbitHack: reconstruct signed Imm11 from the live opcode word at runtime.
				armLoadPtr(RWSCRATCH, &curI);
				armAsm->Sbfx(gprT2.W(), RWSCRATCH, 10, 1);
				armAsm->Bfxil(gprT2.W(), RWSCRATCH, 0, 10);
				armAsm->Add(gprT1.W(), gprT1.W(), gprT2.W());
			}
			mVUaddrFix(mVU, gprT1);
			armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
			armAsm->Add(gprT1q, gprT2q, gprT1q.X());
		}

		const a64::VRegister& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		mVUloadMem(Ft, gprT1q, _X_Y_Z_W);
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
		if (_Is_ || isVU0)
		{
			// Pre-decrement VI[Is]
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Is_, mVUlow.backupVI);
			armAsm->Sub(regS.W(), regS.W(), 1);
			armAsm->Sxth(gprT1.W(), regS.W());
			mVU.regAlloc->clearNeeded(regS);
			mVUaddrFix(mVU, gprT1);
		}
		else
		{
			// _Is_ == 0 and !isVU0: use fixed address (end of micro mem - 8)
			armAsm->Mov(gprT1.W(), 0xffff & (mVU.microMemSize - 8));
		}

		armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
		armAsm->Add(gprT1q, gprT2q, gprT1q.X());

		if (!mVUlow.noWriteVF)
		{
			const a64::VRegister& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
			mVUloadMem(Ft, gprT1q, _X_Y_Z_W);
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
			// Post-increment: read current value, then increment
			const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, _Is_, mVUlow.backupVI);
			armAsm->Sxth(gprT1.W(), regS.W());
			armAsm->Add(regS.W(), regS.W(), 1);
			mVU.regAlloc->clearNeeded(regS);
			mVUaddrFix(mVU, gprT1);
		}
		else
		{
			armAsm->Mov(gprT1.W(), 0);
		}

		armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
		armAsm->Add(gprT1q, gprT2q, gprT1q.X());

		if (!mVUlow.noWriteVF)
		{
			const a64::VRegister& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
			mVUloadMem(Ft, gprT1q, _X_Y_Z_W);
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
		// Compute address from VI[It] + Imm11
		if (!mVUoptimizeConstantAddr(mVU, _It_, _Imm11_, 0, gprT1q))
		{
			mVU.regAlloc->moveVIToGPR(gprT1, _It_);
			if (!EmuConfig.Gamefixes.IbitHack)
			{
				if (_Imm11_ != 0)
				{
					s32 imm = _Imm11_;
					if (imm >= 0)
						armAsm->Add(gprT1.W(), gprT1.W(), (u32)imm);
					else
						armAsm->Sub(gprT1.W(), gprT1.W(), (u32)(-imm));
				}
			}
			else
			{
				// IbitHack: reconstruct signed Imm11 from the live opcode word at runtime.
				armLoadPtr(RWSCRATCH, &curI);
				armAsm->Sbfx(gprT2.W(), RWSCRATCH, 10, 1);
				armAsm->Bfxil(gprT2.W(), RWSCRATCH, 0, 10);
				armAsm->Add(gprT1.W(), gprT1.W(), gprT2.W());
			}
			mVUaddrFix(mVU, gprT1);
			armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
			armAsm->Add(gprT1q, gprT2q, gprT1q.X());
		}

		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, -1, _X_Y_Z_W);
		if (_X_Y_Z_W == 0xf)
		{
			armAsm->Str(Fs, a64::MemOperand(gprT1q));
		}
		else
		{
			// Partial store: load existing, merge, store
			armAsm->Ldr(RQSCRATCH, a64::MemOperand(gprT1q));
			mVUmergeRegs(RQSCRATCH, Fs, _X_Y_Z_W, false);
			armAsm->Str(RQSCRATCH, a64::MemOperand(gprT1q));
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
		if (_It_ || isVU0)
		{
			// Pre-decrement VI[It]
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, _It_, mVUlow.backupVI);
			armAsm->Sub(regT.W(), regT.W(), 1);
			armAsm->Uxth(gprT1.W(), regT.W());
			mVU.regAlloc->clearNeeded(regT);
			mVUaddrFix(mVU, gprT1);
		}
		else
		{
			armAsm->Mov(gprT1.W(), 0xffff & (mVU.microMemSize - 8));
		}

		armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
		armAsm->Add(gprT1q, gprT2q, gprT1q.X());

		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, -1, _X_Y_Z_W);
		if (_X_Y_Z_W == 0xf)
		{
			armAsm->Str(Fs, a64::MemOperand(gprT1q));
		}
		else
		{
			armAsm->Ldr(RQSCRATCH, a64::MemOperand(gprT1q));
			mVUmergeRegs(RQSCRATCH, Fs, _X_Y_Z_W, false);
			armAsm->Str(RQSCRATCH, a64::MemOperand(gprT1q));
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
		if (_It_)
		{
			// Post-increment: read current, then increment
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_, _It_, mVUlow.backupVI);
			armAsm->Uxth(gprT1.W(), regT.W());
			armAsm->Add(regT.W(), regT.W(), 1);
			mVU.regAlloc->clearNeeded(regT);
			mVUaddrFix(mVU, gprT1);
		}
		else
		{
			armAsm->Mov(gprT1.W(), 0);
		}

		armAsm->Ldr(gprT2q, mVUstateMem(offsetof(VURegs, Mem)));
		armAsm->Add(gprT1q, gprT2q, gprT1q.X());

		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, -1, _X_Y_Z_W);
		if (_X_Y_Z_W == 0xf)
		{
			armAsm->Str(Fs, a64::MemOperand(gprT1q));
		}
		else
		{
			armAsm->Ldr(RQSCRATCH, a64::MemOperand(gprT1q));
			mVUmergeRegs(RQSCRATCH, Fs, _X_Y_Z_W, false);
			armAsm->Str(RQSCRATCH, a64::MemOperand(gprT1q));
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
			const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
			armAsm->Umov(gprT1.W(), Fs.V4S(), 0);
			armAsm->And(gprT1.W(), gprT1.W(), 0x007fffff);
			armAsm->Orr(gprT1.W(), gprT1.W(), 0x3f800000);
			armStorePtr(gprT1, Rmem);
			mVU.regAlloc->clearNeeded(Fs);
		}
		else
		{
			armAsm->Mov(gprT1.W(), 0x3f800000);
			armStorePtr(gprT1, Rmem);
		}
		mVU.profiler.EmitOp(opRINIT);
	}
	pass3 { mVUlog("RINIT R, vf%02d%s", _Fs_, _Fsf_String); }
}

static __fi void mVU_RGET_(mV, const a64::Register& Rreg)
{
	if (!mVUlow.noWriteVF)
	{
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		armAsm->Dup(Ft.V4S(), Rreg.W());
		mVU.regAlloc->clearNeeded(Ft);
	}
}

mVUop(mVU_RGET)
{
	pass1 { mVUanalyzeR2(mVU, _Ft_, true); }
	pass2
	{
		armLoadPtr(gprT1, Rmem);
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
		// LFSR step: new = (R << 1) with bit 0 = R[4] XOR R[22], then mantissa
		// mask + IEEE single 1.0-exponent. AArch64 fuses bit4/bit22 extract via
		// EOR-with-shift: bit 4 of (R EOR (R LSR 18)) is exactly R[4] XOR R[22].
		const a64::Register& temp3 = mVU.regAlloc->allocGPR();
		armLoadPtr(temp3, Rmem);
		armAsm->Eor(gprT1.W(), temp3.W(), a64::Operand(temp3.W(), a64::LSR, 18));
		armAsm->Lsl(temp3.W(), temp3.W(), 1);
		armAsm->Bfxil(temp3.W(), gprT1.W(), 4, 1);
		armAsm->And(temp3.W(), temp3.W(), 0x007fffff);
		armAsm->Orr(temp3.W(), temp3.W(), 0x3f800000);
		armStorePtr(temp3, Rmem);
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
			const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, (1 << (3 - _Fsf_)));
			armAsm->Umov(gprT1.W(), Fs.V4S(), 0);
			armAsm->And(gprT1.W(), gprT1.W(), 0x7fffff);
			armLoadPtr(gprT2, Rmem);
			armAsm->Eor(gprT2.W(), gprT2.W(), gprT1.W());
			armStorePtr(gprT2, Rmem);
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
		armMoveAddressToReg(a64::x8, &mVU.getVifRegs().top);
		armAsm->Ldrh(regT.W(), a64::MemOperand(a64::x8));
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
		armMoveAddressToReg(a64::x8, &mVU.getVifRegs().itop);
		armAsm->Ldrh(regT.W(), a64::MemOperand(a64::x8));
		armAsm->And(regT.W(), regT.W(), isVU1 ? 0x3ff : 0xff);
		mVU.regAlloc->clearNeeded(regT);
		mVU.profiler.EmitOp(opXITOP);
	}
	pass3 { mVUlog("XITOP vi%02d", _Ft_); }
}

//------------------------------------------------------------------
// XGkick
//------------------------------------------------------------------

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
		if (mVUinfo.doXGKICK)
		{
			mVU_XGKICK_DELAY(mVU);
			mVUinfo.doXGKICK = false;
		}

		const a64::Register& regS = mVU.regAlloc->allocGPR(_Is_, -1);
		if (!CHECK_XGKICKHACK)
		{
			mVUstrField(mVU, regS, &mVU.VIxgkick);
		}
		else
		{
			// Gamefix XgKickHack — set up VU1 xgkick state registers so
			// _vuXGKICKTransfermVU's loop can run cycle-by-cycle.
			// Mirrors the x86 mVU_XGKICK XgKickHack path.
			// XGKICK is a VU1-only op, so gprVUState (x19) == &VU1 here and
			// every VU1.* field is a single [x19, #off] access.
			armAsm->Mov(a64::w9, 1);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, xgkickenable)));

			armAsm->Str(a64::wzr, mVUstateMem(offsetof(VURegs, xgkickendpacket)));
			armAsm->Str(a64::wzr, mVUstateMem(offsetof(VURegs, xgkicksizeremaining)));
			armAsm->Str(a64::wzr, mVUstateMem(offsetof(VURegs, xgkickcyclecount)));

			// xgkicklastcycle = totalCycles - cycles + VU1.cycle
			armAsm->Ldr(gprT2.W(), mVUfieldMem(mVU, &mVU.totalCycles));
			armAsm->Ldr(a64::w9, mVUfieldMem(mVU, &mVU.cycles));
			armAsm->Sub(gprT2.W(), gprT2.W(), a64::w9);
			// VU1.cycle is u64 (upstream f0723a6ec widened it). Read the full
			// value to match x86's ptr64. The xgkicklastcycle store below is
			// 32-bit (matching x86 ptr32), so only the low 32 bits are retained.
			armAsm->Ldr(a64::x9, mVUstateMem(offsetof(VURegs, cycle)));
			armAsm->Add(gprT2q, gprT2q, a64::x9);
			armAsm->Str(gprT2.W(), mVUstateMem(offsetof(VURegs, xgkicklastcycle)));

			// xgkickaddr = (regS & 0x3FF) << 4
			armAsm->Mov(gprT1.W(), regS);
			armAsm->And(gprT1.W(), gprT1.W(), 0x3FF);
			armAsm->Lsl(gprT1.W(), gprT1.W(), 4);
			armMoveAddressToReg(a64::x8, &VU1.xgkickaddr);
			armAsm->Str(gprT1.W(), a64::MemOperand(a64::x8));
		}
		mVU.regAlloc->clearNeeded(regS);
		mVU.profiler.EmitOp(opXGKICK);
	}
	pass3 { mVUlog("XGKICK vi%02d", _Fs_); }
}

//------------------------------------------------------------------
// Branches/Jumps
//------------------------------------------------------------------

// Branch setup helper -- runs across all passes to set branch metadata.
// Mirrors x86/microVU_Lower.inl setBranchA.
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

// Conditional branch sitting in another branch's delay slot ("bad"/"evil"
// branch). The condition value is in gprT1 (sign-extended for the signed
// compares, so the 32-bit Cmp matches x86's 16-bit signed xCMP). Conditionally
// selects the taken/not-taken continuation address into mVU.badBranch /
// evilBranch / evilevilBranch — port of x86 microVU_Lower.inl condEvilBranch.
// JMPcc is the "branch taken" condition. Dropping this (pre-fix arm64 state)
// left the continuation targets stale: MGS2's VU0 collision solver
// (FMAND;IBNE;IBNE) took the retry path forever and hung the game.
void condEvilBranch(mV, a64::Condition JMPcc)
{
	if (mVUlow.badBranch)
	{
		mVUstrField(mVU, gprT1, &mVU.branch);
		armAsm->Mov(gprT2.W(), branchAddr(mVU));
		mVUstrField(mVU, gprT2, &mVU.badBranch);

		armAsm->Cmp(gprT1.W(), 0);
		a64::Label cJMP;
		armAsm->B(&cJMP, JMPcc);
		{
			incPC(4); // Branch Not Taken Addr
			armAsm->Mov(gprT2.W(), xPC);
			mVUstrField(mVU, gprT2, &mVU.badBranch);
			incPC(-4);
		}
		armAsm->Bind(&cJMP);
		return;
	}
	if (isEvilBlock)
	{
		armAsm->Mov(gprT2.W(), branchAddr(mVU));
		mVUstrField(mVU, gprT2, &mVU.evilevilBranch);
		armAsm->Cmp(gprT1.W(), 0);
		a64::Label cJMP;
		armAsm->B(&cJMP, JMPcc);
		{
			mVUldrField(mVU, gprT2, &mVU.evilBranch); // Branch Not Taken
			armAsm->Add(gprT2.W(), gprT2.W(), 8); // We have already executed 1 instruction from the original branch
			mVUstrField(mVU, gprT2, &mVU.evilevilBranch);
		}
		armAsm->Bind(&cJMP);
	}
	else
	{
		armAsm->Mov(gprT2.W(), branchAddr(mVU));
		mVUstrField(mVU, gprT2, &mVU.evilBranch);
		armAsm->Cmp(gprT1.W(), 0);
		a64::Label cJMP;
		armAsm->B(&cJMP, JMPcc);
		{
			mVUldrField(mVU, gprT2, &mVU.badBranch); // Branch Not Taken
			armAsm->Add(gprT2.W(), gprT2.W(), 8); // We have already executed 1 instruction from the original branch
			mVUstrField(mVU, gprT2, &mVU.evilBranch);
		}
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
		if (mVUlow.badBranch)  { armAsm->Mov(gprT1.W(), branchAddr(mVU)); mVUstrField(mVU, gprT1, &mVU.badBranch); }
		if (mVUlow.evilBranch) {
			armAsm->Mov(gprT1.W(), branchAddr(mVU));
			if (isEvilBlock) mVUstrField(mVU, gprT1, &mVU.evilevilBranch);
			else             mVUstrField(mVU, gprT1, &mVU.evilBranch);
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
			armAsm->Mov(regT.W(), bSaveAddr);
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			incPC(-2);
			DevCon.Warning("Linking BAL from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
			incPC(2);

			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (isEvilBlock)
				mVUldrField(mVU, regT, &mVU.evilBranch);
			else
				mVUldrField(mVU, regT, &mVU.badBranch);

			armAsm->Add(regT.W(), regT.W(), 8);
			armAsm->Lsr(regT.W(), regT.W(), 3);
			mVU.regAlloc->clearNeeded(regT);
		}

		if (mVUlow.badBranch)  { armAsm->Mov(gprT1.W(), branchAddr(mVU)); mVUstrField(mVU, gprT1, &mVU.badBranch); }
		if (mVUlow.evilBranch) {
			armAsm->Mov(gprT1.W(), branchAddr(mVU));
			if (isEvilBlock) mVUstrField(mVU, gprT1, &mVU.evilevilBranch);
			else             mVUstrField(mVU, gprT1, &mVU.evilBranch);
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
		if (mVUlow.memReadIs)
			mVUldrField(mVU, gprT1, &mVU.VIbackup);
		else
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);

		if (mVUlow.memReadIt)
		{
			mVUldrField(mVU, gprT2, &mVU.VIbackup);
			armAsm->Eor(gprT1.W(), gprT1.W(), gprT2.W());
		}
		else
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_);
			armAsm->Eor(gprT1.W(), gprT1.W(), regT.W());
			mVU.regAlloc->clearNeeded(regT);
		}

		if (!(isBadOrEvil))
			mVUstrField(mVU, gprT1, &mVU.branch);
		else
			condEvilBranch(mVU, a64::eq);
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
		if (mVUlow.memReadIs)
			mVUldrField(mVU, gprT1, &mVU.VIbackup);
		else
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_, true); // sign-extend for comparison
		if (!(isBadOrEvil))
			mVUstrField(mVU, gprT1, &mVU.branch);
		else
			condEvilBranch(mVU, a64::ge);
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
		if (mVUlow.memReadIs)
			mVUldrField(mVU, gprT1, &mVU.VIbackup);
		else
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_, true);
		if (!(isBadOrEvil))
			mVUstrField(mVU, gprT1, &mVU.branch);
		else
			condEvilBranch(mVU, a64::gt);
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
		if (mVUlow.memReadIs)
			mVUldrField(mVU, gprT1, &mVU.VIbackup);
		else
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_, true);
		if (!(isBadOrEvil))
			mVUstrField(mVU, gprT1, &mVU.branch);
		else
			condEvilBranch(mVU, a64::le);
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
		if (mVUlow.memReadIs)
			mVUldrField(mVU, gprT1, &mVU.VIbackup);
		else
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_, true);
		if (!(isBadOrEvil))
			mVUstrField(mVU, gprT1, &mVU.branch);
		else
			condEvilBranch(mVU, a64::lt);
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
		if (mVUlow.memReadIs)
			mVUldrField(mVU, gprT1, &mVU.VIbackup);
		else
			mVU.regAlloc->moveVIToGPR(gprT1, _Is_);

		if (mVUlow.memReadIt)
		{
			mVUldrField(mVU, gprT2, &mVU.VIbackup);
			armAsm->Eor(gprT1.W(), gprT1.W(), gprT2.W());
		}
		else
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(_It_);
			armAsm->Eor(gprT1.W(), gprT1.W(), regT.W());
			mVU.regAlloc->clearNeeded(regT);
		}

		if (!(isBadOrEvil))
			mVUstrField(mVU, gprT1, &mVU.branch);
		else
			condEvilBranch(mVU, a64::ne);
		mVU.profiler.EmitOp(opIBNE);
	}
	pass3 { mVUlog("IBNE vi%02d, vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

static void normJumpPass2(mV)
{
	if (!mVUlow.constJump.isValid || mVUlow.evilBranch)
	{
		mVU.regAlloc->moveVIToGPR(gprT1, _Is_);
		armAsm->Lsl(gprT1.W(), gprT1.W(), 3);
		armAsm->And(gprT1.W(), gprT1.W(), mVU.microMemSize - 8);

		if (!mVUlow.evilBranch)
		{
			mVUstrField(mVU, gprT1, &mVU.branch);
		}
		else
		{
			if (isEvilBlock)
				mVUstrField(mVU, gprT1, &mVU.evilevilBranch);
			else
				mVUstrField(mVU, gprT1, &mVU.evilBranch);
		}

		if (mVUlow.badBranch)
		{
			mVUstrField(mVU, gprT1, &mVU.badBranch);
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
			armAsm->Mov(regT.W(), bSaveAddr);
			mVU.regAlloc->clearNeeded(regT);
		}
		if (mVUlow.evilBranch)
		{
			const a64::Register& regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (isEvilBlock)
			{
				mVUldrField(mVU, regT, &mVU.evilBranch);
				armAsm->Add(regT.W(), regT.W(), 8);
				armAsm->Lsr(regT.W(), regT.W(), 3);
			}
			else
			{
				incPC(-2);
				DevCon.Warning("Linking JALR from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
				incPC(2);

				mVUldrField(mVU, regT, &mVU.badBranch);
				armAsm->Add(regT.W(), regT.W(), 8);
				armAsm->Lsr(regT.W(), regT.W(), 3);
			}
			mVU.regAlloc->clearNeeded(regT);
		}

		mVU.profiler.EmitOp(opJALR);
	}
	pass3 { mVUlog("JALR vi%02d, [vi%02d]", _Ft_, _Fs_); }
}
