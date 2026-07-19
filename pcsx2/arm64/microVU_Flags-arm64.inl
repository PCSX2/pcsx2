// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// mVUupdateFlags() - ARM64 NEON flag extraction
//------------------------------------------------------------------
// Uses NEON CMLT/FCMEQ + lane extraction in place of x86 MOVMSKPS.

#define AND_XYZW ((_XYZW_SS && modXYZW) ? (1) : (mFLAG.doFlag ? (_X_Y_Z_W) : (flipMask[_X_Y_Z_W])))
#define ADD_XYZW ((_XYZW_SS && modXYZW) ? (_X ? 3 : (_Y ? 2 : (_Z ? 1 : 0))) : 0)
#define SHIFT_XYZW(gprReg) \
	do { \
		if (_XYZW_SS && modXYZW && !_W) \
			armAsm->Lsl(gprReg, gprReg, ADD_XYZW); \
	} while (0)

static void mVUupdateFlags(mV, const a64::VRegister& reg,
	const a64::VRegister& regT1in = a64::NoVReg,
	const a64::VRegister& regT2in = a64::NoVReg,
	bool modXYZW = true)
{
	const a64::Register& mReg = gprT1;
	const a64::Register& sReg = getFlagReg(sFLAG.write);
	static const u16 flipMask[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

	if (!sFLAG.doFlag && !mFLAG.doFlag)
		return;

	// Allocate temp NEON reg if not provided
	bool regT1b = regT1in.IsNone();
	a64::VRegister regT1 = regT1b ? mVU.regAlloc->allocReg() : regT1in;

	// The x86 path shuffles WZYX→XYZW via PSHUFD 0x1B when updating MAC flag (not in
	// single-scalar mode) so MOVMSKPS produces [W,Z,Y,X] in bits [0:3],
	// matching PS2 MAC flag order. ARM64 achieves the same bit layout
	// by passing reverse=true to armEmitPackLaneBits, which picks the
	// {8,4,2,1} weight vector so lane3→bit0, lane0→bit3.
	const bool macPath = mFLAG.doFlag && !(_XYZW_SS && modXYZW);

	if (sFLAG.doFlag)
	{
		mVUallocSFLAGa(sReg, sFLAG.lastWrite);
		if (sFLAG.doNonSticky)
			armAsm->And(sReg.W(), sReg.W(), 0xfffc00ffu);
	}

	//--------- Extract sign bits (negative lanes) → bits [7:4] ---------

	armAsm->Cmlt(regT1.V4S(), reg.V4S(), 0); // All-1s where negative
	armEmitPackLaneBits(mReg.W(), regT1, RQSCRATCH3, macPath);

	armAsm->And(mReg.W(), mReg.W(), AND_XYZW);
	armAsm->Lsl(mReg.W(), mReg.W(), 4);

	//--------- Extract zero bits (zero lanes) → bits [3:0] ---------

	armAsm->Fcmeq(regT1.V4S(), reg.V4S(), 0.0);
	armEmitPackLaneBits(gprT2, regT1, RQSCRATCH3, macPath);

	armAsm->And(gprT2, gprT2, AND_XYZW);
	armAsm->Orr(mReg.W(), mReg.W(), gprT2);

	//--------- Overflow flags (VUOverflowHack gamefix only) ---------
	// Port of x86 microVU_Upper.inl CHECK_VUOVERFLOWHACK block. We can't
	// distinguish a genuine FLT_MAX result from a saturated overflow without
	// soft-float, so this stays a per-game gamefix (Superman Returns). Detect any
	// lane that reached the saturation boundary (|result| >= FLT_MAX, which also
	// catches Inf/NaN — matching x86's CMPNLT.PS, since for sign-stripped IEEE
	// floats the integer ordering equals the float ordering and Inf/NaN sit above
	// FLT_MAX). Sets STATUS O+S (0x820000) and, when emitting the MAC flag, ORs
	// the per-lane overflow bits in at the O nibble (<<12).
	if (sFLAG.doFlag && CHECK_VUOVERFLOWHACK)
	{
		armAsm->Fabs(regT1.V4S(), reg.V4S());                     // strip sign
		armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.maxvals[0])); // FLT_MAX per lane
		armAsm->Cmge(regT1.V4S(), regT1.V4S(), RQSCRATCH3.V4S()); // all-1s where |x| >= FLT_MAX
		armEmitPackLaneBits(gprT2, regT1, RQSCRATCH3, macPath);
		armAsm->And(gprT2, gprT2, AND_XYZW);

		a64::Label noOverflow;
		armAsm->Cbz(gprT2, &noOverflow);
		armAsm->Orr(sReg.W(), sReg.W(), 0x820000);
		if (mFLAG.doFlag)
		{
			armAsm->Lsl(gprT2, gprT2, 12); // into the MAC O nibble
			armAsm->Orr(mReg.W(), mReg.W(), gprT2);
		}
		armAsm->Bind(&noOverflow);
	}

	//--------- Write back flags ---------

	if (mFLAG.doFlag)
	{
		SHIFT_XYZW(mReg.W());
		mVUallocMFLAGb(mVU, mReg, mFLAG.write);
	}

	if (sFLAG.doFlag)
	{
		armAsm->And(a64::w12, mReg.W(), 0xFF);
		armAsm->Orr(sReg.W(), sReg.W(), a64::w12);
		if (sFLAG.doNonSticky)
		{
			armAsm->Lsl(a64::w12, a64::w12, 8);
			armAsm->Orr(sReg.W(), sReg.W(), a64::w12);
		}
	}

	if (regT1b)
		mVU.regAlloc->clearNeeded(regT1);
}

//------------------------------------------------------------------
// Flag Cycling — ARM64 NEON implementation of microVU_Flags.inl logic
//------------------------------------------------------------------
// Pure-logic analysis and NEON codegen for pipeline flag instance
// tracking. Status flags live in four callee-saved GPRs (gprF0-F3);
// MAC/clip flags live as 4x 32-bit lanes in mVU.macFlag/clipFlag.

static int findFlagInst(int* fFlag, int cycles)
{
	int j = 0, jValue = -1;
	for (int i = 0; i < 4; i++)
	{
		if ((fFlag[i] <= cycles) && (fFlag[i] > jValue))
		{
			j = i;
			jValue = fFlag[i];
		}
	}
	return j;
}

// Setup last 4 instances of Status/Mac/Clip flags (for accurate block linking).
// Returns number of distinct flag instances.
static int sortFlag(int* fFlag, int* bFlag, int cycles)
{
	int lFlag = -5;
	int x = 0;
	for (int i = 0; i < 4; i++)
	{
		bFlag[i] = findFlagInst(fFlag, cycles);
		if (lFlag != bFlag[i])
			x++;
		lFlag = bFlag[i];
		cycles++;
	}
	return x;
}

// Retained for parity with x86 microVU_Flags.inl::sortFullFlag; the arm64 flag
// path does not currently call it (hence [[maybe_unused]]).
[[maybe_unused]] static void sortFullFlag(int* fFlag, int* bFlag)
{
	int m = std::max(std::max(fFlag[0], fFlag[1]), std::max(fFlag[2], fFlag[3]));
	for (int i = 0; i < 4; i++)
	{
		int t = 3 - (m - fFlag[i]);
		bFlag[i] = (t < 0) ? 0 : t + 1;
	}
}

// Optimizes out unneeded status flag updates (safely done when there is an FSSET opcode).
static __fi void mVUstatusFlagOp(mV)
{
	int curPC = iPC;
	int i = mVUcount;
	bool runLoop = true;

	if (sFLAG.doFlag)
	{
		sFLAG.doNonSticky = true;
	}
	else
	{
		for (; i > 0; i--)
		{
			incPC2(-2);
			if (sFLAG.doNonSticky)
			{
				runLoop = false;
				break;
			}
			else if (sFLAG.doFlag)
			{
				sFLAG.doNonSticky = true;
				break;
			}
		}
	}
	if (runLoop)
	{
		for (; i > 0; i--)
		{
			incPC2(-2);

			if (sFLAG.doNonSticky)
				break;

			sFLAG.doFlag = false;
		}
	}
	iPC = curPC;
	DevCon.WriteLn(Color_Green, "microVU%d: FSSET Optimization", getIndex);
}

#define sFlagCond (sFLAG.doFlag || mVUlow.isFSSET || mVUinfo.doDivFlag)
#define sHackCond (mVUsFlagHack && !sFLAG.doNonSticky)

// Note: Flag handling is 'very' complex; requires full knowledge of microVU recs.
static __fi void mVUsetFlags(mV, microFlagCycles& mFC)
{
	int endPC = iPC;
	u32 aCount = 0; // Amount of instructions needed to get valid mac flag instances for block linking

	// Ensure last ~4+ instructions update mac/status flags (if next block's first 4 read them)
	for (int i = mVUcount; i > 0; i--, aCount++)
	{
		if (sFLAG.doFlag)
		{
			if (__Mac)
				mFLAG.doFlag = true;

			if (__Status)
				sFLAG.doNonSticky = true;

			if (aCount >= 3)
				break;
		}
		incPC2(-2);
	}

	// Status/Mac Flags setup
	int xS = 0, xM = 0, xC = 0;

	for (int i = 0; i < 4; i++)
	{
		mFC.xStatus[i] = i;
		mFC.xMac   [i] = i;
		mFC.xClip  [i] = i;
	}

	if (!(mVUpBlock->pState.needExactMatch & 1))
	{
		xS = (mVUpBlock->pState.flagInfo >> 2) & 3;
		mFC.xStatus[0] = -1;
		mFC.xStatus[1] = -1;
		mFC.xStatus[2] = -1;
		mFC.xStatus[3] = -1;
		mFC.xStatus[(xS - 1) & 3] = 0;
	}

	if (!(mVUpBlock->pState.needExactMatch & 2))
	{
		mFC.xMac[0] = -1;
		mFC.xMac[1] = -1;
		mFC.xMac[2] = -1;
		mFC.xMac[3] = -1;
	}

	if (!(mVUpBlock->pState.needExactMatch & 4))
	{
		xC = (mVUpBlock->pState.flagInfo >> 6) & 3;
		mFC.xClip[0] = -1;
		mFC.xClip[1] = -1;
		mFC.xClip[2] = -1;
		mFC.xClip[3] = -1;
		mFC.xClip[(xC - 1) & 3] = 0;
	}

	mFC.cycles = 0;
	u32 xCount = mVUcount;
	iPC = mVUstartPC;
	for (mVUcount = 0; mVUcount < xCount; mVUcount++)
	{
		if (mVUlow.isFSSET && !noFlagOpts)
		{
			if (__Status)
			{
				if ((xCount - mVUcount) > aCount)
					mVUstatusFlagOp(mVU);
			}
			else
				mVUstatusFlagOp(mVU);
		}
		mFC.cycles += mVUstall;

		sFLAG.read = doSFlagInsts ? findFlagInst(mFC.xStatus, mFC.cycles) : 0;
		mFLAG.read = doMFlagInsts ? findFlagInst(mFC.xMac,    mFC.cycles) : 0;
		cFLAG.read = doCFlagInsts ? findFlagInst(mFC.xClip,   mFC.cycles) : 0;

		sFLAG.write = doSFlagInsts ? xS : 0;
		mFLAG.write = doMFlagInsts ? xM : 0;
		cFLAG.write = doCFlagInsts ? xC : 0;

		sFLAG.lastWrite = doSFlagInsts ? (xS - 1) & 3 : 0;
		mFLAG.lastWrite = doMFlagInsts ? (xM - 1) & 3 : 0;
		cFLAG.lastWrite = doCFlagInsts ? (xC - 1) & 3 : 0;

		if (sHackCond)
			sFLAG.doFlag = false;

		if (sFLAG.doFlag)
		{
			if (noFlagOpts)
			{
				sFLAG.doNonSticky = true;
				mFLAG.doFlag = true;
			}
		}

		if (sFlagCond)
		{
			mFC.xStatus[xS] = mFC.cycles + 4;
			xS = (xS + 1) & 3;
		}

		if (mFLAG.doFlag)
		{
			mFC.xMac[xM] = mFC.cycles + 4;
			xM = (xM + 1) & 3;
		}

		if (cFLAG.doFlag)
		{
			mFC.xClip[xC] = mFC.cycles + 4;
			xC = (xC + 1) & 3;
		}

		mFC.cycles++;
		incPC2(2);
	}

	mVUregs.flagInfo |= ((__Status) ? 0 : (xS << 2));
	mVUregs.flagInfo |= (xM << 4);
	mVUregs.flagInfo |= ((__Clip)   ? 0 : (xC << 6));
	iPC = endPC;
}

#define getFlagReg2(x) ((bStatus[0] == x) ? getFlagReg(x) : gprT1)
#define getFlagReg3(x) ((gFlag == x) ? gprT1 : getFlagReg(x))
#define getFlagReg4(x) ((gFlag == x) ? gprT1 : gprT2)

// Emit NEON lane-shuffle equivalent to x86's SHUF.PS xmm, xmm, pattern.
// bFlag[i] names the source lane that should end up in dest lane i.
// Clobbers one temp NEON register.
static __fi void mVUshuffleFlagVec(a64::VRegister vec, const int* bFlag, a64::VRegister tmp)
{
	// If already identity, nothing to do.
	if (bFlag[0] == 0 && bFlag[1] == 1 && bFlag[2] == 2 && bFlag[3] == 3)
		return;
	// Copy source so we can read lanes before overwriting them.
	armAsm->Mov(tmp.V16B(), vec.V16B());
	for (int i = 0; i < 4; i++)
	{
		if (bFlag[i] != i)
			armAsm->Ins(vec.V4S(), i, tmp.V4S(), bFlag[i]);
	}
}

// Recompiles code for proper flags on block linkings (equivalent to x86's mVUsetupFlags).
static __fi void mVUsetupFlags(mV, microFlagCycles& mFC)
{
	if (mVUregs.flagInfo & 1)
	{
		if (mVUregs.needExactMatch)
			DevCon.Error("mVU ERROR!!!");
	}

	if (doSFlagInsts && __Status)
	{
		int bStatus[4];
		int sortRegs = sortFlag(mFC.xStatus, bStatus, mFC.cycles);
		// Skip register self-moves. vixl does NOT elide Mov(Wd, Wd)
		// (kDontDiscardForSameWReg) — it emits a real ORR because the move
		// clears bits 63:32 of the X reg. getFlagReg(i) is gprF[i], so
		// Mov(gprFi, getFlagReg(bStatus[i])) is a no-op exactly when
		// bStatus[i]==i (identity ring phase / all-same-instance link). The
		// temp regs (gprT1-3) never alias gprF0-3, so guarding every emit on
		// dst!=src is correct in all four permutation branches and elides the
		// dead ORRs the old code emitted per block link.
		const auto movF = [](const a64::Register& d, const a64::Register& s) {
			if (d.GetCode() != s.GetCode())
				armAsm->Mov(d, s);
		};
		if (sortRegs == 1)
		{
			movF(gprF0, getFlagReg(bStatus[0]));
			movF(gprF1, getFlagReg(bStatus[1]));
			movF(gprF2, getFlagReg(bStatus[2]));
			movF(gprF3, getFlagReg(bStatus[3]));
		}
		else if (sortRegs == 2)
		{
			movF(gprT1, getFlagReg (bStatus[3]));
			movF(gprF0, getFlagReg (bStatus[0]));
			movF(gprF1, getFlagReg2(bStatus[1]));
			movF(gprF2, getFlagReg2(bStatus[2]));
			movF(gprF3, gprT1);
		}
		else if (sortRegs == 3)
		{
			int gFlag = (bStatus[0] == bStatus[1]) ? bStatus[2] : bStatus[1];
			movF(gprT1, getFlagReg (gFlag));
			movF(gprT2, getFlagReg (bStatus[3]));
			movF(gprF0, getFlagReg (bStatus[0]));
			movF(gprF1, getFlagReg3(bStatus[1]));
			movF(gprF2, getFlagReg4(bStatus[2]));
			movF(gprF3, gprT2);
		}
		else
		{
			// All four are distinct — need an extra temp. Use gprT3 (w11) which
			// is scratch in the ABI (not in VI pool).
			movF(gprT1, getFlagReg(bStatus[0]));
			movF(gprT2, getFlagReg(bStatus[1]));
			movF(gprT3, getFlagReg(bStatus[2]));
			movF(gprF3, getFlagReg(bStatus[3]));
			movF(gprF0, gprT1);
			movF(gprF1, gprT2);
			movF(gprF2, gprT3);
		}
	}

	if (doMFlagInsts && __Mac)
	{
		int bMac[4];
		sortFlag(mFC.xMac, bMac, mFC.cycles);
		armAsm->Ldr(qmmT1, a64::MemOperand(gprMVUFlag));
		mVUshuffleFlagVec(qmmT1, bMac, qmmT2);
		armAsm->Str(qmmT1, a64::MemOperand(gprMVUFlag));
	}

	if (doCFlagInsts && __Clip)
	{
		int bClip[4];
		sortFlag(mFC.xClip, bClip, mFC.cycles);
		armAsm->Ldr(qmmT2, a64::MemOperand(gprMVUFlag, 16));
		mVUshuffleFlagVec(qmmT2, bClip, qmmT1);
		armAsm->Str(qmmT2, a64::MemOperand(gprMVUFlag, 16));
	}
}
