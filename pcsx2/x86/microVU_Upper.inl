// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// mVUupdateFlags() - Updates status/mac flags
//------------------------------------------------------------------

#define AND_XYZW ((_XYZW_SS && modXYZW) ? (1) : (mFLAG.doFlag ? (_X_Y_Z_W) : (flipMask[_X_Y_Z_W])))
#define ADD_XYZW ((_XYZW_SS && modXYZW) ? (_X ? 3 : (_Y ? 2 : (_Z ? 1 : 0))) : 0)
#define SHIFT_XYZW(gprReg) \
	do { \
		if (_XYZW_SS && modXYZW && !_W) \
		{ \
			xSHL(gprReg, ADD_XYZW); \
		} \
	} while (0)


alignas(16) const u32 sse4_compvals[2][4] = {
	{0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff}, //1111
	{0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff}, //1111
};

// Note: If modXYZW is true, then it adjusts XYZW for Single Scalar operations
static void mVUupdateFlags(mV, const xmm& reg, const xmm& regT1in = xEmptyReg, const xmm& regT2in = xEmptyReg, bool modXYZW = 1)
{
	const x32& mReg = gprT1;
	const x32& sReg = getFlagReg(sFLAG.write);
	bool regT1b = regT1in.IsEmpty(), regT2b = false;
	static const u16 flipMask[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

	//SysPrintf("Status = %d; Mac = %d\n", sFLAG.doFlag, mFLAG.doFlag);
	if (!sFLAG.doFlag && !mFLAG.doFlag)
		return;

	const xmm& regT1 = regT1b ? mVU.regAlloc->allocReg() : regT1in;

	xmm regT2 = reg;
	if ((mFLAG.doFlag && !(_XYZW_SS && modXYZW)))
	{
		regT2 = regT2in;
		if (regT2.IsEmpty())
		{
			regT2 = mVU.regAlloc->allocReg();
			regT2b = true;
		}
		xPSHUF.D(regT2, reg, 0x1B); // Flip wzyx to xyzw
	}
	else
		regT2 = reg;

	if (sFLAG.doFlag)
	{
		mVUallocSFLAGa(sReg, sFLAG.lastWrite); // Get Prev Status Flag
		if (sFLAG.doNonSticky)
			xAND(sReg, 0xfffc00ff); // Clear O,U,S,Z flags
	}

	//-------------------------Check for Signed flags------------------------------

	xMOVMSKPS(mReg,  regT2); // Move the Sign Bits of the t2reg
	xXOR.PS  (regT1, regT1); // Clear regT1
	xCMPEQ.PS(regT1, regT2); // Set all F's if each vector is zero
	xMOVMSKPS(gprT2, regT1); // Used for Zero Flag Calculation

	xAND(mReg, AND_XYZW); // Grab "Is Signed" bits from the previous calculation
	xSHL(mReg, 4);

	//-------------------------Check for Zero flags------------------------------

	xAND(gprT2, AND_XYZW); // Grab "Is Zero" bits from the previous calculation
	xOR(mReg, gprT2);

	//-------------------------Overflow Flags-----------------------------------
	// We can't really do this because of the limited range of x86 and the value MIGHT genuinely be FLT_MAX (x86)
	// so this will need to remain as a gamefix for the one game that needs it (Superman Returns)
	// until some sort of soft float implementation.
	if (sFLAG.doFlag && CHECK_VUOVERFLOWHACK)
	{
		//Calculate overflow
		xMOVAPS(regT1, regT2);
		xAND.PS(regT1, ptr128[&sse4_compvals[1][0]]); // Remove sign flags (we don't care)
		xCMPNLT.PS(regT1, ptr128[&sse4_compvals[0][0]]); // Compare if T1 == FLT_MAX
		xMOVMSKPS(gprT2, regT1); // Grab sign bits  for equal results
		xAND(gprT2, AND_XYZW); // Grab "Is FLT_MAX" bits from the previous calculation
		xForwardJump32 oJMP(Jcc_Zero);

		xOR(sReg, 0x820000);
		if (mFLAG.doFlag)
		{
			xSHL(gprT2, 12); // Add the results to the MAC Flag
			xOR(mReg, gprT2);
		}

		oJMP.SetTarget();
	}

	//-------------------------Write back flags------------------------------
	if (mFLAG.doFlag)
	{
		SHIFT_XYZW(mReg); // If it was Single Scalar, move the flags in to the correct position
		mVUallocMFLAGb(mVU, mReg, mFLAG.write); // Set Mac Flag
	}
	if (sFLAG.doFlag)
	{
		xAND(mReg, 0xFF); // Ignore overflow bits, they're handled separately
		xOR(sReg, mReg);
		if (sFLAG.doNonSticky)
		{
			xSHL(mReg, 8);
			xOR(sReg, mReg);
		}
	}
	if (regT1b)
		mVU.regAlloc->clearNeeded(regT1);
	if (regT2b)
		mVU.regAlloc->clearNeeded(regT2);
}

//------------------------------------------------------------------
// Helper Macros and Functions
//------------------------------------------------------------------

static void (*const SSE_PS[])(microVU&, const xmm&, const xmm&, const xmm&, const xmm&) = {
	SSE_ADDPS, // 0
	SSE_SUBPS, // 1
	SSE_MULPS, // 2
	SSE_MAXPS, // 3
	SSE_MINPS, // 4
	SSE_ADD2PS // 5
};

static void (*const SSE_SS[])(microVU&, const xmm&, const xmm&, const xmm&, const xmm&) = {
	SSE_ADDSS, // 0
	SSE_SUBSS, // 1
	SSE_MULSS, // 2
	SSE_MAXSS, // 3
	SSE_MINSS, // 4
	SSE_ADD2SS // 5
};

enum clampModes
{
	cFt = 0x01, // Clamp Ft / I-reg / Q-reg
	cFs = 0x02, // Clamp Fs
	cACC = 0x04, // Clamp ACC
};

// Prints Opcode to MicroProgram Logs
static void mVU_printOP(microVU& mVU, int opCase, microOpcode opEnum, bool isACC)
{
	mVUlog(microOpcodeName[opEnum]);
	opCase1 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogFt(); }
	opCase2 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogBC(); }
	opCase3 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogI();  }
	opCase4 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogQ();  }
}

// Sets Up Pass1 Info for Normal, BC, I, and Q Cases
static void setupPass1(microVU& mVU, int opCase, bool isACC, bool noFlagUpdate)
{
	opCase1 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, _Ft_); }
	opCase2 { mVUanalyzeFMAC3(mVU, ((isACC) ? 0 : _Fd_), _Fs_, _Ft_); }
	opCase3 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, 0); }
	opCase4 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, 0); }

	if (noFlagUpdate) //Max/Min Ops
		sFLAG.doFlag = false;
}

// Safer to force 0 as the result for X minus X than to do actual subtraction
static bool doSafeSub(microVU& mVU, int opCase, int opType, bool isACC)
{
	opCase1
	{
		if ((opType == 1) && (_Ft_ == _Fs_) && (opCase == 1)) // Don't do this with BC's!
		{
			const xmm& Fs = mVU.regAlloc->allocReg(-1, isACC ? 32 : _Fd_, _X_Y_Z_W);
			xPXOR(Fs, Fs); // Set to Positive 0
			mVUupdateFlags(mVU, Fs);
			mVU.regAlloc->clearNeeded(Fs);
			return true;
		}
	}
	return false;
}

// Sets Up Ft Reg for Normal, BC, I, and Q Cases
static void setupFtReg(microVU& mVU, xmm& Ft, xmm& tempFt, int opCase, int clampType)
{
	opCase1
	{
		// Based on mVUclamp2 -> mVUclamp1 below.
		const bool willClamp = (clampE || ((clampType & cFt) && !clampE && (CHECK_VU_OVERFLOW(mVU.index) || CHECK_VU_SIGN_OVERFLOW(mVU.index))));

		if (_XYZW_SS2)      { Ft = mVU.regAlloc->allocReg(_Ft_, 0, _X_Y_Z_W); tempFt = Ft; }
		else if (willClamp) { Ft = mVU.regAlloc->allocReg(_Ft_, 0, 0xf);      tempFt = Ft; }
		else                { Ft = mVU.regAlloc->allocReg(_Ft_);              tempFt = xEmptyReg;  }
	}
	opCase2
	{
		tempFt = mVU.regAlloc->allocReg(_Ft_);
		Ft     = mVU.regAlloc->allocReg();
		mVUunpack_xyzw(Ft, tempFt, _bc_);
		mVU.regAlloc->clearNeeded(tempFt);
		tempFt = Ft;
	}
	opCase3
	{
		Ft = mVU.regAlloc->allocReg(33, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	opCase4
	{
		if (!clampE && _XYZW_SS && !mVUinfo.readQ)
		{
			Ft = xmmPQ;
			tempFt = xEmptyReg;
		}
		else
		{
			Ft = mVU.regAlloc->allocReg();
			tempFt = Ft;
			getQreg(Ft, mVUinfo.readQ);
		}
	}
}

// Normal FMAC Opcodes
static void mVU_FMACa(microVU& mVU, int recPass, int opCase, int opType, bool isACC, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, isACC, ((opType == 3) || (opType == 4))); }
	pass2
	{
		if (doSafeSub(mVU, opCase, opType, isACC))
			return;

		xmm Fs, Ft, ACC, tempFt;
		setupFtReg(mVU, Ft, tempFt, opCase, clampType);

		if (isACC)
		{
			Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
			ACC = mVU.regAlloc->allocReg((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, 0);
			if (_XYZW_SS2)
				xPSHUF.D(ACC, ACC, shuffleSS(_X_Y_Z_W));
		}
		else
		{
			Fs = mVU.regAlloc->allocReg(_Fs_, _Fd_, _X_Y_Z_W);
		}

		if (clampType & cFt) mVUclamp2(mVU, Ft, xEmptyReg, _X_Y_Z_W);
		if (clampType & cFs) mVUclamp2(mVU, Fs, xEmptyReg, _X_Y_Z_W);

		if (_XYZW_SS) SSE_SS[opType](mVU, Fs, Ft, xEmptyReg, xEmptyReg);
		else          SSE_PS[opType](mVU, Fs, Ft, xEmptyReg, xEmptyReg);

		if (isACC)
		{
			if (_XYZW_SS)
				xMOVSS(ACC, Fs);
			else
				mVUmergeRegs(ACC, Fs, _X_Y_Z_W);
			mVUupdateFlags(mVU, ACC, Fs, tempFt);
			if (_XYZW_SS2)
				xPSHUF.D(ACC, ACC, shuffleSS(_X_Y_Z_W));
			mVU.regAlloc->clearNeeded(ACC);
		}
		else if (opType < 3 || opType == 5) // Not Min/Max or is ADDi(5) (TODO: Reorganise this so its < 4 including ADDi)
			mVUupdateFlags(mVU, Fs, tempFt);

		mVU.regAlloc->clearNeeded(Fs); // Always Clear Written Reg First
		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, isACC); }
	pass4
	{
		if ((opType != 3) && (opType != 4))
			mVUregs.needExactMatch |= 8;
	}
}

// MADDA/MSUBA Opcodes
static void mVU_FMACb(microVU& mVU, int recPass, int opCase, int opType, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, true, false); }
	pass2
	{
		xmm Fs, Ft, ACC, tempFt;
		setupFtReg(mVU, Ft, tempFt, opCase, clampType);

		Fs = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		ACC = mVU.regAlloc->allocReg(32, 32, 0xf, false);

		if (_XYZW_SS2)
			xPSHUF.D(ACC, ACC, shuffleSS(_X_Y_Z_W));

		if (clampType & cFt) mVUclamp2(mVU, Ft, xEmptyReg, _X_Y_Z_W);
		if (clampType & cFs) mVUclamp2(mVU, Fs, xEmptyReg, _X_Y_Z_W);

		if (_XYZW_SS) SSE_SS[2](mVU, Fs, Ft, xEmptyReg, xEmptyReg);
		else          SSE_PS[2](mVU, Fs, Ft, xEmptyReg, xEmptyReg);

		if (_XYZW_SS || _X_Y_Z_W == 0xf)
		{
			if (_XYZW_SS) SSE_SS[opType](mVU, ACC, Fs, tempFt, xEmptyReg);
			else          SSE_PS[opType](mVU, ACC, Fs, tempFt, xEmptyReg);
			mVUupdateFlags(mVU, ACC, Fs, tempFt);
			if (_XYZW_SS && _X_Y_Z_W != 8)
				xPSHUF.D(ACC, ACC, shuffleSS(_X_Y_Z_W));
		}
		else
		{
			const xmm& tempACC = mVU.regAlloc->allocReg();
			xMOVAPS(tempACC, ACC);
			SSE_PS[opType](mVU, tempACC, Fs, tempFt, xEmptyReg);
			mVUmergeRegs(ACC, tempACC, _X_Y_Z_W);
			mVUupdateFlags(mVU, ACC, Fs, tempFt);
			mVU.regAlloc->clearNeeded(tempACC);
		}

		mVU.regAlloc->clearNeeded(ACC);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, true); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MADD Opcodes
static void mVU_FMACc(microVU& mVU, int recPass, int opCase, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, false, false); }
	pass2
	{
		xmm Fs, Ft, ACC, tempFt;
		setupFtReg(mVU, Ft, tempFt, opCase, clampType);

		ACC = mVU.regAlloc->allocReg(32);
		Fs = mVU.regAlloc->allocReg(_Fs_, _Fd_, _X_Y_Z_W);

		if (_XYZW_SS2)
			xPSHUF.D(ACC, ACC, shuffleSS(_X_Y_Z_W));

		if (clampType & cFt)  mVUclamp2(mVU, Ft,  xEmptyReg, _X_Y_Z_W);
		if (clampType & cFs)  mVUclamp2(mVU, Fs,  xEmptyReg, _X_Y_Z_W);
		if (clampType & cACC) mVUclamp2(mVU, ACC, xEmptyReg, _X_Y_Z_W);


		if (_XYZW_SS) { SSE_SS[2](mVU, Fs, Ft, xEmptyReg, xEmptyReg); SSE_SS[0](mVU, Fs, ACC, tempFt, xEmptyReg); }
		else          { SSE_PS[2](mVU, Fs, Ft, xEmptyReg, xEmptyReg); SSE_PS[0](mVU, Fs, ACC, tempFt, xEmptyReg); }

		if (_XYZW_SS2)
			xPSHUF.D(ACC, ACC, shuffleSS(_X_Y_Z_W));

		mVUupdateFlags(mVU, Fs, tempFt);

		mVU.regAlloc->clearNeeded(Fs); // Always Clear Written Reg First
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(ACC);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, false); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// MSUB Opcodes
static void mVU_FMACd(microVU& mVU, int recPass, int opCase, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, false, false); }
	pass2
	{
		xmm Fs, Ft, Fd, tempFt;
		setupFtReg(mVU, Ft, tempFt, opCase, clampType);

		Fs = mVU.regAlloc->allocReg(_Fs_,  0, _X_Y_Z_W);
		Fd = mVU.regAlloc->allocReg(32, _Fd_, _X_Y_Z_W);

		if (clampType & cFt)  mVUclamp2(mVU, Ft, xEmptyReg, _X_Y_Z_W);
		if (clampType & cFs)  mVUclamp2(mVU, Fs, xEmptyReg, _X_Y_Z_W);
		if (clampType & cACC) mVUclamp2(mVU, Fd, xEmptyReg, _X_Y_Z_W);

		if (_XYZW_SS) { SSE_SS[2](mVU, Fs, Ft, xEmptyReg, xEmptyReg); SSE_SS[1](mVU, Fd, Fs, tempFt, xEmptyReg); }
		else          { SSE_PS[2](mVU, Fs, Ft, xEmptyReg, xEmptyReg); SSE_PS[1](mVU, Fd, Fs, tempFt, xEmptyReg); }

		mVUupdateFlags(mVU, Fd, Fs, tempFt);

		mVU.regAlloc->clearNeeded(Fd); // Always Clear Written Reg First
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, false); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

// ABS Opcode
mVUop(mVU_ABS)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		if (!_Ft_)
			return;
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
		xAND.PS(Fs, ptr128[mVUglob.absclip]);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opABS);
	}
	pass3
	{
		mVUlog("ABS");
		mVUlogFtFs();
	}
}

// OPMULA Opcode
mVUop(mVU_OPMULA)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		const xmm& Ft = mVU.regAlloc->allocReg(_Ft_, 0, _X_Y_Z_W);
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 32, _X_Y_Z_W);

		xPSHUF.D(Fs, Fs, 0xC9); // WXZY
		xPSHUF.D(Ft, Ft, 0xD2); // WYXZ
		SSE_MULPS(mVU, Fs, Ft);
		mVU.regAlloc->clearNeeded(Ft);
		mVUupdateFlags(mVU, Fs);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opOPMULA);
	}
	pass3
	{
		mVUlog("OPMULA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// OPMSUB Opcode
mVUop(mVU_OPMSUB)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		const xmm& Ft = mVU.regAlloc->allocReg(_Ft_, 0, 0xf);
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const xmm& ACC = mVU.regAlloc->allocReg(32, _Fd_, _X_Y_Z_W);

		xPSHUF.D(Fs, Fs, 0xC9); // WXZY
		xPSHUF.D(Ft, Ft, 0xD2); // WYXZ
		SSE_MULPS(mVU, Fs,  Ft);
		SSE_SUBPS(mVU, ACC, Fs);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVUupdateFlags(mVU, ACC);
		mVU.regAlloc->clearNeeded(ACC);
		mVU.profiler.EmitOp(opOPMSUB);
	}
	pass3
	{
		mVUlog("OPMSUB");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

// FTOI0/FTIO4/FTIO12/FTIO15 Opcodes
static void mVU_FTOIx(mP, const float* addr, microOpcode opEnum)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		if (!_Ft_)
			return;
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
		const xmm& t1 = mVU.regAlloc->allocReg();
		const xmm& t2 = mVU.regAlloc->allocReg();

		// Note: For help understanding this algorithm see recVUMI_FTOI_Saturate()
		xMOVAPS(t1, Fs);
		if (addr)
			xMUL.PS(Fs, ptr128[addr]);
		xCVTTPS2DQ(Fs, Fs);
		xPXOR(t1, ptr128[mVUglob.signbit]);
		xPSRA.D(t1, 31);
		xMOVAPS(t2, Fs);
		xPCMP.EQD(t2, ptr128[mVUglob.signbit]);
		xAND.PS(t1, t2);
		xPADD.D(Fs, t1);

		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3
	{
		mVUlog(microOpcodeName[opEnum]);
		mVUlogFtFs();
	}
}

// ITOF0/ITOF4/ITOF12/ITOF15 Opcodes
static void mVU_ITOFx(mP, const float* addr, microOpcode opEnum)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		if (!_Ft_)
			return;
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));

		xCVTDQ2PS(Fs, Fs);
		if (addr)
			xMUL.PS(Fs, ptr128[addr]);
		//mVUclamp2(Fs, xmmT1, 15); // Clamp (not sure if this is needed)

		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3
	{
		mVUlog(microOpcodeName[opEnum]);
		mVUlogFtFs();
	}
}

// Clip Opcode
mVUop(mVU_CLIP)
{
	pass1 { mVUanalyzeFMAC4(mVU, _Fs_, _Ft_); }
	pass2
	{
		const xmm& Fs = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const xmm& Ft = mVU.regAlloc->allocReg(_Ft_, 0, 0x1);
		const xmm& t1 = mVU.regAlloc->allocReg();
		const xmm& t2 = mVU.regAlloc->allocReg();

		mVUunpack_xyzw(Ft, Ft, 0);
		mVUallocCFLAGa(mVU, gprT1, cFLAG.lastWrite);
		xSHL(gprT1, 6);

		xMOVAPS  (t1, ptr128[mVUglob.exponent]);
		xPAND    (t1, Fs);
		xPXOR    (t2, t2);
		xPCMP.EQD(t1, t2); // Denormal check
		xPANDN   (t1, Fs); // If denormal, set to zero, which can't be greater than any nonnegative denormal in Ft
		xPAND    (Ft, ptr128[mVUglob.absclip]);

		xMOVAPS  (Fs, ptr128[mVUglob.signbit]);
		xPXOR    (Fs, t1); // Negate
		xPCMP.GTD(t1, Ft); // +w, +z, +y, +x
		xPCMP.GTD(Fs, Ft); // -w, -z, -y, -x

		xPBLEND.W (Fs, t1, 0x55); // Squish together
		xPACK.SSWB(Fs, Fs);       // Convert u16 to u8
		xPMOVMSKB (gprT2, Fs);    // Get bitmask
		xAND      (gprT2, 0x3f);  // Mask unused stuff
		xAND      (gprT1, 0xffffff);
		xOR       (gprT1, gprT2);

		mVUallocCFLAGb(mVU, gprT1, cFLAG.write);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(t1);
		mVU.regAlloc->clearNeeded(t2);
		mVU.profiler.EmitOp(opCLIP);
	}
	pass3
	{
		mVUlog("CLIP");
		mVUlogCLIP();
	}
}

//------------------------------------------------------------------
// Micro VU Micromode Upper instructions
//------------------------------------------------------------------

mVUop(mVU_ADD)    { mVU_FMACa(mVU, recPass, 1, 0, false, opADD,    0);  }
mVUop(mVU_ADDi)   { mVU_FMACa(mVU, recPass, 3, 5, false, opADDi,   0);  }
mVUop(mVU_ADDq)   { mVU_FMACa(mVU, recPass, 4, 0, false, opADDq,   0);  }
mVUop(mVU_ADDx)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDx,   0);  }
mVUop(mVU_ADDy)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDy,   0);  }
mVUop(mVU_ADDz)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDz,   0);  }
mVUop(mVU_ADDw)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDw,   0);  }
mVUop(mVU_ADDA)   { mVU_FMACa(mVU, recPass, 1, 0, true,  opADDA,   0);  }
mVUop(mVU_ADDAi)  { mVU_FMACa(mVU, recPass, 3, 0, true,  opADDAi,  0);  }
mVUop(mVU_ADDAq)  { mVU_FMACa(mVU, recPass, 4, 0, true,  opADDAq,  0);  }
mVUop(mVU_ADDAx)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAx,  0);  }
mVUop(mVU_ADDAy)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAy,  0);  }
mVUop(mVU_ADDAz)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAz,  0);  }
mVUop(mVU_ADDAw)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAw,  0);  }
mVUop(mVU_SUB)    { mVU_FMACa(mVU, recPass, 1, 1, false, opSUB,  (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBi)   { mVU_FMACa(mVU, recPass, 3, 1, false, opSUBi, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBq)   { mVU_FMACa(mVU, recPass, 4, 1, false, opSUBq, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBx)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBx, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBy)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBy, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBz)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBz, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBw)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBw, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBA)   { mVU_FMACa(mVU, recPass, 1, 1, true,  opSUBA,   0);  }
mVUop(mVU_SUBAi)  { mVU_FMACa(mVU, recPass, 3, 1, true,  opSUBAi,  0);  }
mVUop(mVU_SUBAq)  { mVU_FMACa(mVU, recPass, 4, 1, true,  opSUBAq,  0);  }
mVUop(mVU_SUBAx)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAx,  0);  }
mVUop(mVU_SUBAy)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAy,  0);  }
mVUop(mVU_SUBAz)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAz,  0);  }
mVUop(mVU_SUBAw)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAw,  0);  }
mVUop(mVU_MUL)    { mVU_FMACa(mVU, recPass, 1, 2, false, opMUL,  (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULi)   { mVU_FMACa(mVU, recPass, 3, 2, false, opMULi, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULq)   { mVU_FMACa(mVU, recPass, 4, 2, false, opMULq, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULx)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULx, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (vu0))
mVUop(mVU_MULy)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULy, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULz)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULz, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULw)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULw, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULA)   { mVU_FMACa(mVU, recPass, 1, 2, true,  opMULA,   0);  }
mVUop(mVU_MULAi)  { mVU_FMACa(mVU, recPass, 3, 2, true,  opMULAi,  0);  }
mVUop(mVU_MULAq)  { mVU_FMACa(mVU, recPass, 4, 2, true,  opMULAq,  0);  }
mVUop(mVU_MULAx)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAx,  cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MULAy)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAy,  cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MULAz)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAz,  cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MULAw)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAw, (_XYZW_PS) ? (cFs | cFt) : cFs); } // Clamp (TOTA, DoM, ...)- Ft for Superman - Shadow Of Apokolips
mVUop(mVU_MADD)   { mVU_FMACc(mVU, recPass, 1,           opMADD,   0); }
mVUop(mVU_MADDi)  { mVU_FMACc(mVU, recPass, 3,           opMADDi,  0); }
mVUop(mVU_MADDq)  { mVU_FMACc(mVU, recPass, 4,           opMADDq,  0); }
mVUop(mVU_MADDx)  { mVU_FMACc(mVU, recPass, 2,           opMADDx,  cFs); } // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDy)  { mVU_FMACc(mVU, recPass, 2,           opMADDy,  cFs); } // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDz)  { mVU_FMACc(mVU, recPass, 2,           opMADDz,  cFs); } // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDw)  { mVU_FMACc(mVU, recPass, 2,           opMADDw, (isCOP2)?(cACC|cFt|cFs):cFs);} // Clamp (ICO (COP2), TOTA, DoM)
mVUop(mVU_MADDA)  { mVU_FMACb(mVU, recPass, 1, 0,        opMADDA,  0);  }
mVUop(mVU_MADDAi) { mVU_FMACb(mVU, recPass, 3, 0,        opMADDAi, 0);  }
mVUop(mVU_MADDAq) { mVU_FMACb(mVU, recPass, 4, 0,        opMADDAq, 0);  }
mVUop(mVU_MADDAx) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAx, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDAy) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAy, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDAz) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAz, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDAw) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAw, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MSUB)   { mVU_FMACd(mVU, recPass, 1,           opMSUB,  (isCOP2) ? cFs : 0); } // Clamp ( Superman - Shadow Of Apokolips)
mVUop(mVU_MSUBi)  { mVU_FMACd(mVU, recPass, 3,           opMSUBi,  0);  }
mVUop(mVU_MSUBq)  { mVU_FMACd(mVU, recPass, 4,           opMSUBq,  0);  }
mVUop(mVU_MSUBx)  { mVU_FMACd(mVU, recPass, 2,           opMSUBx,  0);  }
mVUop(mVU_MSUBy)  { mVU_FMACd(mVU, recPass, 2,           opMSUBy,  0);  }
mVUop(mVU_MSUBz)  { mVU_FMACd(mVU, recPass, 2,           opMSUBz,  0);  }
mVUop(mVU_MSUBw)  { mVU_FMACd(mVU, recPass, 2,           opMSUBw,  0);  }
mVUop(mVU_MSUBA)  { mVU_FMACb(mVU, recPass, 1, 1,        opMSUBA,  0);  }
mVUop(mVU_MSUBAi) { mVU_FMACb(mVU, recPass, 3, 1,        opMSUBAi, 0);  }
mVUop(mVU_MSUBAq) { mVU_FMACb(mVU, recPass, 4, 1,        opMSUBAq, 0);  }
mVUop(mVU_MSUBAx) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAx, 0);  }
mVUop(mVU_MSUBAy) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAy, 0);  }
mVUop(mVU_MSUBAz) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAz, 0);  }
mVUop(mVU_MSUBAw) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAw, 0);  }
mVUop(mVU_MAX)    { mVU_FMACa(mVU, recPass, 1, 3, false, opMAX,    0);  }
mVUop(mVU_MAXi)   { mVU_FMACa(mVU, recPass, 3, 3, false, opMAXi,   0);  }
mVUop(mVU_MAXx)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXx,   0);  }
mVUop(mVU_MAXy)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXy,   0);  }
mVUop(mVU_MAXz)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXz,   0);  }
mVUop(mVU_MAXw)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXw,   0);  }
mVUop(mVU_MINI)   { mVU_FMACa(mVU, recPass, 1, 4, false, opMINI,   0);  }
mVUop(mVU_MINIi)  { mVU_FMACa(mVU, recPass, 3, 4, false, opMINIi,  0);  }
mVUop(mVU_MINIx)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIx,  0);  }
mVUop(mVU_MINIy)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIy,  0);  }
mVUop(mVU_MINIz)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIz,  0);  }
mVUop(mVU_MINIw)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIw,  0);  }
mVUop(mVU_FTOI0)  { mVU_FTOIx(mX, NULL,                  opFTOI0);      }
mVUop(mVU_FTOI4)  { mVU_FTOIx(mX, mVUglob.FTOI_4,        opFTOI4);      }
mVUop(mVU_FTOI12) { mVU_FTOIx(mX, mVUglob.FTOI_12,       opFTOI12);     }
mVUop(mVU_FTOI15) { mVU_FTOIx(mX, mVUglob.FTOI_15,       opFTOI15);     }
mVUop(mVU_ITOF0)  { mVU_ITOFx(mX, NULL,                  opITOF0);      }
mVUop(mVU_ITOF4)  { mVU_ITOFx(mX, mVUglob.ITOF_4,        opITOF4);      }
mVUop(mVU_ITOF12) { mVU_ITOFx(mX, mVUglob.ITOF_12,       opITOF12);     }
mVUop(mVU_ITOF15) { mVU_ITOFx(mX, mVUglob.ITOF_15,       opITOF15);     }
mVUop(mVU_NOP)    { pass2 { mVU.profiler.EmitOp(opNOP); } pass3 { mVUlog("NOP"); } }
