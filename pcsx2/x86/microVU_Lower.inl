/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
	xXOR.PS(xmmTemp, xmmTemp);
	xCMPEQ.SS(xmmTemp, xmmReg);
	xPTEST(xmmTemp, xmmTemp);
}

// Test if Vector is Negative (Set Flags and Makes Positive)
static __fi void testNeg(mV, const xmm& xmmReg, const x32& gprTemp)
{
	xMOVMSKPS(gprTemp, xmmReg);
	xTEST(gprTemp, 1);
	xForwardJZ8 skip;
		xMOV(ptr32[&mVU.divFlag], divI);
		xAND.PS(xmmReg, ptr128[mVUglob.absclip]);
	skip.SetTarget();
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
		xForwardJZ8 cjmp; // Skip if not zero

			testZero(Fs, t1, gprT1); // Test if Fs is zero
			xForwardJZ8 ajmp;
				xMOV(ptr32[&mVU.divFlag], divI); // Set invalid flag (0/0)
				xForwardJump8 bjmp;
			ajmp.SetTarget();
				xMOV(ptr32[&mVU.divFlag], divD); // Zero divide (only when not 0/0)
			bjmp.SetTarget();

			xXOR.PS(Fs, Ft);
			xAND.PS(Fs, ptr128[mVUglob.signbit]);
			xOR.PS (Fs, ptr128[mVUglob.maxvals]); // If division by zero, then xmmFs = +/- fmax

			xForwardJump8 djmp;
		cjmp.SetTarget();
			xMOV(ptr32[&mVU.divFlag], 0); // Clear I/D flags
			SSE_DIVSS(mVU, Fs, Ft);
			mVUclamp1(Fs, t1, 8, true);
		djmp.SetTarget();

		writeQreg(Fs, mVUinfo.writeQ);

		if (mVU.cop2)
		{
			xAND(gprF0, ~0xc0000);
			xOR(gprF0, ptr32[&mVU.divFlag]);
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

		xMOV(ptr32[&mVU.divFlag], 0); // Clear I/D flags
		testNeg(mVU, Ft, gprT1); // Check for negative sqrt

		if (CHECK_VU_OVERFLOW) // Clamp infinities (only need to do positive clamp since xmmFt is positive)
			xMIN.SS(Ft, ptr32[mVUglob.maxvals]);
		xSQRT.SS(Ft, Ft);
		writeQreg(Ft, mVUinfo.writeQ);

		if (mVU.cop2)
		{
			xAND(gprF0, ~0xc0000);
			xOR(gprF0, ptr32[&mVU.divFlag]);
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

		xMOV(ptr32[&mVU.divFlag], 0); // Clear I/D flags
		testNeg(mVU, Ft, gprT1); // Check for negative sqrt

		xSQRT.SS(Ft, Ft);
		testZero(Ft, t1, gprT1); // Test if Ft is zero
		xForwardJZ8 ajmp; // Skip if not zero

			testZero(Fs, t1, gprT1); // Test if Fs is zero
			xForwardJZ8 bjmp; // Skip if none are
				xMOV(ptr32[&mVU.divFlag], divI); // Set invalid flag (0/0)
				xForwardJump8 cjmp;
			bjmp.SetTarget();
				xMOV(ptr32[&mVU.divFlag], divD); // Zero divide flag (only when not 0/0)
			cjmp.SetTarget();

			xAND.PS(Fs, ptr128[mVUglob.signbit]);
			xOR.PS(Fs, ptr128[mVUglob.maxvals]); // xmmFs = +/-Max

			xForwardJump8 djmp;
		ajmp.SetTarget();
			SSE_DIVSS(mVU, Fs, Ft);
			mVUclamp1(Fs, t1, 8, true);
		djmp.SetTarget();

		writeQreg(Fs, mVUinfo.writeQ);

		if (mVU.cop2)
		{
			xAND(gprF0, ~0xc0000);
			xOR(gprF0, ptr32[&mVU.divFlag]);
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
		xMOVAPS(t1, t2); \
		xMUL.SS(t1, ptr32[addr]); \
		SSE_ADDSS(mVU, PQ, t1); \
	}

// ToDo: Can Be Optimized Further? (takes approximately (~115 cycles + mem access time) on a c2d)
static __fi void mVU_EATAN_(mV, const xmm& PQ, const xmm& Fs, const xmm& t1, const xmm& t2)
{
	xMOVSS(PQ, Fs);
	xMUL.SS(PQ, ptr32[mVUglob.T1]);
	xMOVAPS(t2, Fs);
	EATANhelper(mVUglob.T2);
	EATANhelper(mVUglob.T3);
	EATANhelper(mVUglob.T4);
	EATANhelper(mVUglob.T5);
	EATANhelper(mVUglob.T6);
	EATANhelper(mVUglob.T7);
	EATANhelper(mVUglob.T8);
	xADD.SS(PQ, ptr32[mVUglob.Pi4]);
	xPSHUF.D(PQ, PQ, mVUinfo.writeP ? 0x27 : 0xC6);
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
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xMOVSS (xmmPQ, Fs);
		xSUB.SS(Fs,    ptr32[mVUglob.one]);
		xADD.SS(xmmPQ, ptr32[mVUglob.one]);
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
		xPSHUF.D(Fs, t1, 0x01);
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xMOVSS  (xmmPQ, Fs);
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
		xPSHUF.D(Fs, t1, 0x02);
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xMOVSS  (xmmPQ, Fs);
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
		xMOVAPS(t1, t2); \
		xMUL.SS(t1, ptr32[addr]); \
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
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xMOVSS  (xmmPQ, Fs);
		xMUL.SS (xmmPQ, ptr32[mVUglob.E1]);
		xADD.SS (xmmPQ, ptr32[mVUglob.one]);
		xMOVAPS(t1, Fs);
		SSE_MULSS(mVU, t1, Fs);
		xMOVAPS(t2, t1);
		xMUL.SS(t1, ptr32[mVUglob.E2]);
		SSE_ADDSS(mVU, xmmPQ, t1);
		eexpHelper(&mVUglob.E3);
		eexpHelper(&mVUglob.E4);
		eexpHelper(&mVUglob.E5);
		SSE_MULSS(mVU, t2, Fs);
		xMUL.SS(t2, ptr32[mVUglob.E6]);
		SSE_ADDSS(mVU, xmmPQ, t2);
		SSE_MULSS(mVU, xmmPQ, xmmPQ);
		SSE_MULSS(mVU, xmmPQ, xmmPQ);
		xMOVSSZX(t2, ptr32[mVUglob.one]);
		SSE_DIVSS(mVU, t2, xmmPQ);
		xMOVSS(xmmPQ, t2);
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
	xDP.PS(Fs, Fs, 0x71);
	xMOVSS(PQ, Fs);
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
		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		xSQRT.SS       (xmmPQ, xmmPQ);
		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xMOVSS        (xmmPQ, Fs);
		xMOVSSZX      (Fs, ptr32[mVUglob.one]);
		SSE_DIVSS(mVU, Fs, xmmPQ);
		xMOVSS        (xmmPQ, Fs);
		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		xSQRT.SS       (xmmPQ, xmmPQ);
		xMOVSSZX       (Fs, ptr32[mVUglob.one]);
		SSE_DIVSS (mVU, Fs, xmmPQ);
		xMOVSS         (xmmPQ, Fs);
		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		xMOVSSZX       (Fs, ptr32[mVUglob.one]);
		SSE_DIVSS (mVU, Fs, xmmPQ);
		xMOVSS         (xmmPQ, Fs);
		xPSHUF.D       (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xAND.PS       (Fs, ptr128[mVUglob.absclip]);
		xSQRT.SS      (xmmPQ, Fs);
		xMOVSSZX      (Fs, ptr32[mVUglob.one]);
		SSE_DIVSS(mVU, Fs, xmmPQ);
		xMOVSS        (xmmPQ, Fs);
		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		mVU_sumXYZ(mVU, xmmPQ, Fs);
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xMOVSS        (xmmPQ, Fs); // pq = X
		SSE_MULSS(mVU, Fs, Fs);    // fs = X^2
		xMOVAPS       (t1, Fs);    // t1 = X^2
		SSE_MULSS(mVU, Fs, xmmPQ); // fs = X^3
		xMOVAPS       (t2, Fs);    // t2 = X^3
		xMUL.SS       (Fs, ptr32[mVUglob.S2]); // fs = s2 * X^3
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3

		SSE_MULSS(mVU, t2, t1);    // t2 = X^3 * X^2
		xMOVAPS       (Fs, t2);    // fs = X^5
		xMUL.SS       (Fs, ptr32[mVUglob.S3]); // ps = s3 * X^5
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3 + s3 * X^5

		SSE_MULSS(mVU, t2, t1);    // t2 = X^5 * X^2
		xMOVAPS       (Fs, t2);    // fs = X^7
		xMUL.SS       (Fs, ptr32[mVUglob.S4]); // fs = s4 * X^7
		SSE_ADDSS(mVU, xmmPQ, Fs); // pq = X + s2 * X^3 + s3 * X^5 + s4 * X^7

		SSE_MULSS(mVU, t2, t1);    // t2 = X^7 * X^2
		xMUL.SS       (t2, ptr32[mVUglob.S5]); // t2 = s5 * X^9
		SSE_ADDSS(mVU, xmmPQ, t2); // pq = X + s2 * X^3 + s3 * X^5 + s4 * X^7 + s5 * X^9
		xPSHUF.D      (xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xAND.PS (Fs, ptr128[mVUglob.absclip]);
		xSQRT.SS(xmmPQ, Fs);
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip xmmPQ to get Valid P instance
		xPSHUF.D(t1, Fs, 0x1b);
		SSE_ADDPS(mVU, Fs, t1);
		xPSHUF.D(t1, Fs, 0x01);
		SSE_ADDSS(mVU, Fs, t1);
		xMOVSS(xmmPQ, Fs);
		xPSHUF.D(xmmPQ, xmmPQ, mVUinfo.writeP ? 0x27 : 0xC6); // Flip back
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
		mVUallocCFLAGa(mVU, gprT1, cFLAG.read);
		xAND(gprT1, _Imm24_);
		xADD(gprT1, 0xffffff);
		xSHR(gprT1, 24);
		mVUallocVIb(mVU, gprT1, 1);
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
		mVUallocCFLAGa(mVU, gprT1, cFLAG.read);
		xXOR(gprT1, _Imm24_);
		xSUB(gprT1, 1);
		xSHR(gprT1, 31);
		mVUallocVIb(mVU, gprT1, 1);
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
		mVUallocCFLAGa(mVU, gprT1, cFLAG.read);
		xAND(gprT1, 0xfff);
		mVUallocVIb(mVU, gprT1, _It_);
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
		mVUallocCFLAGa(mVU, gprT1, cFLAG.read);
		xOR(gprT1, _Imm24_);
		xADD(gprT1, 1);  // If 24 1's will make 25th bit 1, else 0
		xSHR(gprT1, 24); // Get the 25th bit (also clears the rest of the garbage in the reg)
		mVUallocVIb(mVU, gprT1, 1);
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
		xMOV(gprT1, _Imm24_);
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
		mVUallocVIa(mVU, gprT2, _Is_);
		xAND(gprT1b, gprT2b);
		mVUallocVIb(mVU, gprT1, _It_);
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
		mVUallocVIa(mVU, gprT2, _Is_);
		xXOR(gprT1, gprT2);
		xSUB(gprT1, 1);
		xSHR(gprT1, 31);
		mVUallocVIb(mVU, gprT1, _It_);
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
		mVUallocVIa(mVU, gprT2, _Is_);
		xOR(gprT1b, gprT2b);
		mVUallocVIb(mVU, gprT1, _It_);
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
		mVUallocSFLAGc(gprT1, gprT2, sFLAG.read);
		xAND(gprT1, _Imm12_);
		mVUallocVIb(mVU, gprT1, _It_);
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
		mVUallocSFLAGc(gprT1, gprT2, sFLAG.read);
		xOR(gprT1, _Imm12_);
		mVUallocVIb(mVU, gprT1, _It_);
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

		mVUallocSFLAGa(gprT1, sFLAG.read);
		setBitFSEQ(gprT1, 0x0f00); // Z  bit
		setBitFSEQ(gprT1, 0xf000); // S  bit
		setBitFSEQ(gprT1, 0x000f); // ZS bit
		setBitFSEQ(gprT1, 0x00f0); // SS bit
		xXOR(gprT1, imm);
		xSUB(gprT1, 1);
		xSHR(gprT1, 31);
		mVUallocVIb(mVU, gprT1, _It_);
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
		xAND(getFlagReg(sFLAG.write), 0xfff00); // Keep Non-Sticky Bits
		if (imm)
			xOR(getFlagReg(sFLAG.write), imm);
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
		mVUallocVIa(mVU, gprT1, _Is_);
		if (_It_ != _Is_)
		{
			mVUallocVIa(mVU, gprT2, _It_);
			xADD(gprT1b, gprT2b);
		}
		else
			xADD(gprT1b, gprT1b);
		mVUallocVIb(mVU, gprT1, _Id_);
		mVU.profiler.EmitOp(opIADD);
	}
	pass3 { mVUlog("IADD vi%02d, vi%02d, vi%02d", _Fd_, _Fs_, _Ft_); }
}

mVUop(mVU_IADDI)
{
	pass1 { mVUanalyzeIADDI(mVU, _Is_, _It_, _Imm5_); }
	pass2
	{
		mVUallocVIa(mVU, gprT1, _Is_);
		if (_Imm5_ != 0)
			xADD(gprT1b, _Imm5_);
		mVUallocVIb(mVU, gprT1, _It_);
		mVU.profiler.EmitOp(opIADDI);
	}
	pass3 { mVUlog("IADDI vi%02d, vi%02d, %d", _Ft_, _Fs_, _Imm5_); }
}

mVUop(mVU_IADDIU)
{
	pass1 { mVUanalyzeIADDI(mVU, _Is_, _It_, _Imm15_); }
	pass2
	{
		mVUallocVIa(mVU, gprT1, _Is_);
		if (_Imm15_ != 0)
			xADD(gprT1b, _Imm15_);
		mVUallocVIb(mVU, gprT1, _It_);
		mVU.profiler.EmitOp(opIADDIU);
	}
	pass3 { mVUlog("IADDIU vi%02d, vi%02d, %d", _Ft_, _Fs_, _Imm15_); }
}

mVUop(mVU_IAND)
{
	pass1 { mVUanalyzeIALU1(mVU, _Id_, _Is_, _It_); }
	pass2
	{
		mVUallocVIa(mVU, gprT1, _Is_);
		if (_It_ != _Is_)
		{
			mVUallocVIa(mVU, gprT2, _It_);
			xAND(gprT1, gprT2);
		}
		mVUallocVIb(mVU, gprT1, _Id_);
		mVU.profiler.EmitOp(opIAND);
	}
	pass3 { mVUlog("IAND vi%02d, vi%02d, vi%02d", _Fd_, _Fs_, _Ft_); }
}

mVUop(mVU_IOR)
{
	pass1 { mVUanalyzeIALU1(mVU, _Id_, _Is_, _It_); }
	pass2
	{
		mVUallocVIa(mVU, gprT1, _Is_);
		if (_It_ != _Is_)
		{
			mVUallocVIa(mVU, gprT2, _It_);
			xOR(gprT1, gprT2);
		}
		mVUallocVIb(mVU, gprT1, _Id_);
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
			mVUallocVIa(mVU, gprT1, _Is_);
			mVUallocVIa(mVU, gprT2, _It_);
			xSUB(gprT1b, gprT2b);
			mVUallocVIb(mVU, gprT1, _Id_);
		}
		else
		{
			xXOR(gprT1, gprT1);
			mVUallocVIb(mVU, gprT1, _Id_);
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
		mVUallocVIa(mVU, gprT1, _Is_);
		if (_Imm15_ != 0)
			xSUB(gprT1b, _Imm15_);
		mVUallocVIb(mVU, gprT1, _It_);
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
		mVUallocVIa(mVU, gprT1, _Is_, true);
		xMOVDZX(Ft, gprT1);
		if (!_XYZW_SS)
			mVUunpack_xyzw(Ft, Ft, 0);
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
			xPSHUF.D(Ft, Fs, 0x39);
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
		xMOVD(gprT1, Fs);
		mVUallocVIb(mVU, gprT1, _It_);
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
		void* ptr = mVU.regs().Mem + offsetSS;

		mVUallocVIa(mVU, gprT2, _Is_);
		if (!_Is_)
			xXOR(gprT2, gprT2);
		if (_Imm11_ != 0)
			xADD(gprT2, _Imm11_);
		mVUaddrFix(mVU, gprT2q);
		xMOVZX(gprT1, ptr16[xComplexAddress(gprT3q, ptr, gprT2q)]);
		mVUallocVIb(mVU, gprT1, _It_);
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
		void* ptr = mVU.regs().Mem + offsetSS;
		if (_Is_)
		{
			mVUallocVIa(mVU, gprT2, _Is_);
			mVUaddrFix (mVU, gprT2q);
			xMOVZX(gprT1, ptr16[xComplexAddress(gprT3q, ptr, gprT2q)]);
		}
		else
		{
			xMOVZX(gprT1, ptr16[ptr]);
		}
		mVUallocVIb(mVU, gprT1, _It_);
		mVU.profiler.EmitOp(opILWR);
	}
	pass3 { mVUlog("ILWR.%s vi%02d, vi%02d", _XYZW_String, _Ft_, _Fs_); }
}

//------------------------------------------------------------------
// ISW/ISWR
//------------------------------------------------------------------

static void writeBackISW(microVU& mVU, void* base_ptr, xAddressReg reg)
{
	if (!reg.IsEmpty() && (sptr)base_ptr != (s32)(sptr)base_ptr)
	{
		int register_offset = -1;
		auto writeBackAt = [&](int offset) {
			if (register_offset == -1)
			{
				xLEA(gprT3q, ptr[(void*)((sptr)base_ptr + offset)]);
				register_offset = offset;
			}
			xMOV(ptr32[gprT3q + reg + (offset - register_offset)], gprT1);
		};
		if (_X) writeBackAt(0);
		if (_Y) writeBackAt(4);
		if (_Z) writeBackAt(8);
		if (_W) writeBackAt(12);
	}
	else if (reg.IsEmpty())
	{
		if (_X) xMOV(ptr32[(void*)((uptr)base_ptr     )], gprT1);
		if (_Y) xMOV(ptr32[(void*)((uptr)base_ptr +  4)], gprT1);
		if (_Z) xMOV(ptr32[(void*)((uptr)base_ptr +  8)], gprT1);
		if (_W) xMOV(ptr32[(void*)((uptr)base_ptr + 12)], gprT1);
	}
	else
	{
		if (_X) xMOV(ptr32[base_ptr+reg     ], gprT1);
		if (_Y) xMOV(ptr32[base_ptr+reg +  4], gprT1);
		if (_Z) xMOV(ptr32[base_ptr+reg +  8], gprT1);
		if (_W) xMOV(ptr32[base_ptr+reg + 12], gprT1);
	}
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
		void* ptr = mVU.regs().Mem;

		mVUallocVIa(mVU, gprT2, _Is_);
		if (!_Is_)
			xXOR(gprT2, gprT2);
		if (_Imm11_ != 0)
			xADD(gprT2, _Imm11_);
		mVUaddrFix(mVU, gprT2q);

		mVUallocVIa(mVU, gprT1, _It_);
		writeBackISW(mVU, ptr, gprT2q);
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
		void* ptr = mVU.regs().Mem;
		xAddressReg is = xEmptyReg;
		if (_Is_)
		{
			mVUallocVIa(mVU, gprT2, _Is_);
			mVUaddrFix(mVU, gprT2q);
			is = gprT2q;
		}
		mVUallocVIa(mVU, gprT1, _It_);
		writeBackISW(mVU, ptr, is);

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
		void* ptr = mVU.regs().Mem;
		mVUallocVIa(mVU, gprT2, _Is_);
		if (!_Is_)
			xXOR(gprT2, gprT2);
		if (_Imm11_ != 0)
			xADD(gprT2, _Imm11_);
		mVUaddrFix(mVU, gprT2q);

		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		mVUloadReg(Ft, xComplexAddress(gprT3q, ptr, gprT2q), _X_Y_Z_W);
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
		xAddressReg is = xEmptyReg;
		if (_Is_ || isVU0) // Access VU1 regs mem-map in !_Is_ case
		{
			mVUallocVIa(mVU, gprT2, _Is_);
			xSUB(gprT2b, 1);
			if (_Is_)
				mVUallocVIb(mVU, gprT2, _Is_);
			mVUaddrFix(mVU, gprT2q);
			is = gprT2q;
		}
		else
		{
			ptr = (void*)((sptr)ptr + (0xffff & (mVU.microMemSize - 8)));
		}
		if (!mVUlow.noWriteVF)
		{
			const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
			if (is.IsEmpty())
			{
				mVUloadReg(Ft, xAddressVoid(ptr), _X_Y_Z_W);
			}
			else
			{
				mVUloadReg(Ft, xComplexAddress(gprT3q, ptr, is), _X_Y_Z_W);
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
		void* ptr = mVU.regs().Mem;
		xAddressReg is = xEmptyReg;
		if (_Is_)
		{
			mVUallocVIa(mVU, gprT1, _Is_);
			xMOV(gprT2, gprT1);
			xADD(gprT1b, 1);
			mVUallocVIb(mVU, gprT1, _Is_);
			mVUaddrFix (mVU, gprT2q);
			is = gprT2q;
		}
		if (!mVUlow.noWriteVF)
		{
			const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
			if (is.IsEmpty())
				mVUloadReg(Ft, xAddressVoid(ptr), _X_Y_Z_W);
			else
				mVUloadReg(Ft, xComplexAddress(gprT3q, ptr, is), _X_Y_Z_W);
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
		void* ptr = mVU.regs().Mem;

		mVUallocVIa(mVU, gprT2, _It_);
		if (!_It_)
			xXOR(gprT2, gprT2);
		if (_Imm11_ != 0)
			xADD(gprT2, _Imm11_);
		mVUaddrFix(mVU, gprT2q);

		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		mVUsaveReg(Fs, xComplexAddress(gprT3q, ptr, gprT2q), _X_Y_Z_W, 1);
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
		xAddressReg it = xEmptyReg;
		if (_It_ || isVU0) // Access VU1 regs mem-map in !_It_ case
		{
			mVUallocVIa(mVU, gprT2, _It_);
			xSUB(gprT2b, 1);
			if (_It_)
				mVUallocVIb(mVU, gprT2, _It_);
			mVUaddrFix(mVU, gprT2q);
			it = gprT2q;
		}
		else
		{
			ptr = (void*)((sptr)ptr + (0xffff & (mVU.microMemSize - 8)));
		}
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		if (it.IsEmpty())
			mVUsaveReg(Fs, xAddressVoid(ptr), _X_Y_Z_W, 1);
		else
			mVUsaveReg(Fs, xComplexAddress(gprT3q, ptr, it), _X_Y_Z_W, 1);
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
		void* ptr = mVU.regs().Mem;
		if (_It_)
		{
			mVUallocVIa(mVU, gprT1, _It_);
			xMOV(gprT2, gprT1);
			xADD(gprT1b, 1);
			mVUallocVIb(mVU, gprT1, _It_);
			mVUaddrFix(mVU, gprT2q);
		}
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		if (_It_)
			mVUsaveReg(Fs, xComplexAddress(gprT3q, ptr, gprT2q), _X_Y_Z_W, 1);
		else
			mVUsaveReg(Fs, xAddressVoid(ptr), _X_Y_Z_W, 1);
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
			xMOVD(gprT1, Fs);
			xAND(gprT1, 0x007fffff);
			xOR (gprT1, 0x3f800000);
			xMOV(ptr32[Rmem], gprT1);
			mVU.regAlloc->clearNeeded(Fs);
		}
		else
			xMOV(ptr32[Rmem], 0x3f800000);
		mVU.profiler.EmitOp(opRINIT);
	}
	pass3 { mVUlog("RINIT R, vf%02d%s", _Fs_, _Fsf_String); }
}

static __fi void mVU_RGET_(mV, const x32& Rreg)
{
	if (!mVUlow.noWriteVF)
	{
		const xmm& Ft = mVU.regAlloc->allocReg(-1, _Ft_, _X_Y_Z_W);
		xMOVDZX(Ft, Rreg);
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
		xMOV(gprT1, ptr32[Rmem]);
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
		xMOV(gprT3, ptr32[Rmem]);
		xMOV(gprT1, gprT3);
		xSHR(gprT1, 4);
		xAND(gprT1, 1);

		xMOV(gprT2, gprT3);
		xSHR(gprT2, 22);
		xAND(gprT2, 1);

		xSHL(gprT3, 1);
		xXOR(gprT1, gprT2);
		xXOR(gprT3, gprT1);
		xAND(gprT3, 0x007fffff);
		xOR (gprT3, 0x3f800000);
		xMOV(ptr32[Rmem], gprT3);
		mVU_RGET_(mVU, gprT3);
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
			xMOVD(gprT1, Fs);
			xAND(gprT1, 0x7fffff);
			xXOR(ptr32[Rmem], gprT1);
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
		xMOVZX(gprT1, ptr16[&mVU.getVifRegs().top]);
		mVUallocVIb(mVU, gprT1, _It_);
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
		xMOVZX(gprT1, ptr16[&mVU.getVifRegs().itop]);
		xAND(gprT1, isVU1 ? 0x3ff : 0xff);
		mVUallocVIb(mVU, gprT1, _It_);
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
			//VUM_LOG("XGKICK reading new tag from %x", VU1.xgkickaddr);
			u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, vuRegs[1].Mem, VU1.xgkickaddr, ~0u, flush);
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
	// Add the single cycle remainder after this instruction, some games do the store
	// on the second instruction after the kick and that needs to go through first
	// but that's VERY close..
	xTEST(ptr32[&VU1.xgkickenable], 0x1);
	xForwardJZ32 skipxgkick;
	xADD(ptr32[&VU1.xgkickcyclecount], mVUlow.kickcycles-1);
	xCMP(ptr32[&VU1.xgkickcyclecount], 2);
	xForwardJL32 needcycles;
	mVUbackupRegs(mVU, true, true);
	xFastCall(_vuXGKICKTransfermVU, flush);
	mVUrestoreRegs(mVU, true, true);
	needcycles.SetTarget();
	xADD(ptr32[&VU1.xgkickcyclecount], 1);
	skipxgkick.SetTarget();
}

static __fi void mVU_XGKICK_DELAY(mV)
{
	mVUbackupRegs(mVU);
#if 0 // XGkick Break - ToDo: Change "SomeGifPathValue" to w/e needs to be tested
	xTEST (ptr32[&SomeGifPathValue], 1); // If '1', breaks execution
	xMOV  (ptr32[&mVU.resumePtrXG], (uptr)xGetPtr() + 10 + 6);
	xJcc32(Jcc_NotZero, (uptr)mVU.exitFunctXG - ((uptr)xGetPtr()+6));
#endif
	xFastCall(mVU_XGKICK_, ptr32[&mVU.VIxgkick]);
	mVUrestoreRegs(mVU);
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
		
		if (!CHECK_XGKICKHACK)
		{
			mVUallocVIa(mVU, gprT1, _Is_);
			xMOV(ptr32[&mVU.VIxgkick], gprT1);
		}
		else
		{
			xMOV(ptr32[&VU1.xgkickenable], 1);
			xMOV(ptr32[&VU1.xgkickendpacket], 0);
			xMOV(ptr32[&VU1.xgkicksizeremaining], 0);
			xMOV(ptr32[&VU1.xgkickcyclecount], 0);
			xMOV(gprT2, ptr32[&mVU.totalCycles]);
			xSUB(gprT2, ptr32[&mVU.cycles]);
			xADD(gprT2, ptr32[&VU1.cycle]);
			xMOV(ptr32[&VU1.xgkicklastcycle], gprT2);
			mVUallocVIa(mVU, gprT1, _Is_);
			xAND(gprT1, 0x3FF);
			xSHL(gprT1, 4);
			xMOV(ptr32[&VU1.xgkickaddr], gprT1);
		}
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

void condEvilBranch(mV, int JMPcc)
{
	if (mVUlow.badBranch)
	{
		xMOV(ptr32[&mVU.branch], gprT1);
		xMOV(ptr32[&mVU.badBranch], branchAddr(mVU));

		xCMP(gprT1b, 0);
		xForwardJump8 cJMP((JccComparisonType)JMPcc);
			incPC(4); // Branch Not Taken Addr
			xMOV(ptr32[&mVU.badBranch], xPC);
			incPC(-4);
		cJMP.SetTarget();
		return;
	}
	if (isEvilBlock)
	{
		xMOV(ptr32[&mVU.evilevilBranch], branchAddr(mVU));
		xCMP(gprT1b, 0);
		xForwardJump8 cJMP((JccComparisonType)JMPcc);
		xMOV(gprT1, ptr32[&mVU.evilBranch]); // Branch Not Taken
		xADD(gprT1, 8); // We have already executed 1 instruction from the original branch
		xMOV(ptr32[&mVU.evilevilBranch], gprT1);
		cJMP.SetTarget();
	}
	else
	{
		xMOV(ptr32[&mVU.evilBranch], branchAddr(mVU));
		xCMP(gprT1b, 0);
		xForwardJump8 cJMP((JccComparisonType)JMPcc);
		xMOV(gprT1, ptr32[&mVU.badBranch]); // Branch Not Taken
		xADD(gprT1, 8); // We have already executed 1 instruction from the original branch
		xMOV(ptr32[&mVU.evilBranch], gprT1);
		cJMP.SetTarget();
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
		if (mVUlow.badBranch)  { xMOV(ptr32[&mVU.badBranch],  branchAddr(mVU)); }
		if (mVUlow.evilBranch) { if(isEvilBlock) xMOV(ptr32[&mVU.evilevilBranch], branchAddr(mVU)); else xMOV(ptr32[&mVU.evilBranch], branchAddr(mVU)); }
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
			xMOV(gprT1, bSaveAddr);
			mVUallocVIb(mVU, gprT1, _It_);
		}
		else
		{
			incPC(-2);
			DevCon.Warning("Linking BAL from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
			incPC(2);
			if (isEvilBlock)
				xMOV(gprT1, ptr32[&mVU.evilBranch]);
			else
				xMOV(gprT1, ptr32[&mVU.badBranch]);

			xADD(gprT1, 8);
			xSHR(gprT1, 3);
			mVUallocVIb(mVU, gprT1, _It_);
		}

		if (mVUlow.badBranch)  { xMOV(ptr32[&mVU.badBranch],  branchAddr(mVU)); }
		if (mVUlow.evilBranch) { if (isEvilBlock) xMOV(ptr32[&mVU.evilevilBranch], branchAddr(mVU)); else xMOV(ptr32[&mVU.evilBranch], branchAddr(mVU)); }
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
			xMOV(gprT1, ptr32[&mVU.VIbackup]);
		else
			mVUallocVIa(mVU, gprT1, _Is_);

		if (mVUlow.memReadIt)
			xXOR(gprT1, ptr32[&mVU.VIbackup]);
		else
		{
			mVUallocVIa(mVU, gprT2, _It_);
			xXOR(gprT1, gprT2);
		}

		if (!(isBadOrEvil))
			xMOV(ptr32[&mVU.branch], gprT1);
		else
			condEvilBranch(mVU, Jcc_Equal);
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
			xMOV(gprT1, ptr32[&mVU.VIbackup]);
		else
			mVUallocVIa(mVU, gprT1, _Is_);
		if (!(isBadOrEvil))
			xMOV(ptr32[&mVU.branch], gprT1);
		else
			condEvilBranch(mVU, Jcc_GreaterOrEqual);
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
			xMOV(gprT1, ptr32[&mVU.VIbackup]);
		else
			mVUallocVIa(mVU, gprT1, _Is_);
		if (!(isBadOrEvil))
			xMOV(ptr32[&mVU.branch], gprT1);
		else
			condEvilBranch(mVU, Jcc_Greater);
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
			xMOV(gprT1, ptr32[&mVU.VIbackup]);
		else
			mVUallocVIa(mVU, gprT1, _Is_);
		if (!(isBadOrEvil))
			xMOV(ptr32[&mVU.branch], gprT1);
		else
			condEvilBranch(mVU, Jcc_LessOrEqual);
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
			xMOV(gprT1, ptr32[&mVU.VIbackup]);
		else
			mVUallocVIa(mVU, gprT1, _Is_);
		if (!(isBadOrEvil))
			xMOV(ptr32[&mVU.branch], gprT1);
		else
			condEvilBranch(mVU, Jcc_Less);
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
			xMOV(gprT1, ptr32[&mVU.VIbackup]);
		else
			mVUallocVIa(mVU, gprT1, _Is_);

		if (mVUlow.memReadIt)
			xXOR(gprT1, ptr32[&mVU.VIbackup]);
		else
		{
			mVUallocVIa(mVU, gprT2, _It_);
			xXOR(gprT1, gprT2);
		}

		if (!(isBadOrEvil))
			xMOV(ptr32[&mVU.branch], gprT1);
		else
			condEvilBranch(mVU, Jcc_NotEqual);
		mVU.profiler.EmitOp(opIBNE);
	}
	pass3 { mVUlog("IBNE vi%02d, vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, _Fs_, branchAddr(mVU), branchAddr(mVU)); }
}

void normJumpPass2(mV)
{
	if (!mVUlow.constJump.isValid || mVUlow.evilBranch)
	{
		mVUallocVIa(mVU, gprT1, _Is_);
		xSHL(gprT1, 3);
		xAND(gprT1, mVU.microMemSize - 8);

		if (!mVUlow.evilBranch)
		{
			xMOV(ptr32[&mVU.branch], gprT1);
		}
		else
		{
			if(isEvilBlock)
				xMOV(ptr32[&mVU.evilevilBranch], gprT1);
			else
				xMOV(ptr32[&mVU.evilBranch], gprT1);
		}
		//If delay slot is conditional, it uses badBranch to go to its target
		if (mVUlow.badBranch)
		{
			xMOV(ptr32[&mVU.badBranch], gprT1);
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
			xMOV(gprT1, bSaveAddr);
			mVUallocVIb(mVU, gprT1, _It_);
		}
		if (mVUlow.evilBranch)
		{
			if (isEvilBlock)
			{
				xMOV(gprT1, ptr32[&mVU.evilBranch]);
				xADD(gprT1, 8);
				xSHR(gprT1, 3);
				mVUallocVIb(mVU, gprT1, _It_);
			}
			else
			{
				incPC(-2);
				DevCon.Warning("Linking JALR from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
				incPC(2);

				xMOV(gprT1, ptr32[&mVU.badBranch]);
				xADD(gprT1, 8);
				xSHR(gprT1, 3);
				mVUallocVIb(mVU, gprT1, _It_);
			}
		}

		mVU.profiler.EmitOp(opJALR);
	}
	pass3 { mVUlog("JALR vi%02d, [vi%02d]", _Ft_, _Fs_); }
}
