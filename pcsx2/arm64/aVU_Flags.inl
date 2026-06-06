// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU recompiler — Status/Mac/Clip flag pipeline (Phase 7, task 7.5/7.6).
//
// ARM64 counterpart to pcsx2/x86/microVU_Flags.inl. Two kinds of code live here:
//   * Pure *analysis* (emitter-free): findFlagInst/sortFlag/sortFullFlag/
//     mVUstatusFlagOp/mVUsetFlags — these compute which of the 4 live flag
//     instances each instruction reads/writes for accurate block linking. They
//     operate only on the IR (microFlagCycles / mVUinfo) so they port verbatim.
//   * Emit helpers (VIXL): mVUdivSet (FDIV flag merge into Status) and
//     mVUsetupFlags (shuffle the 4 Status instances into gprF0-3 + permute the
//     mac/clip flag arrays for the next block). x86 emit translations:
//       - xMOV(gpr, gpr)         -> armAsm->Mov  (W regs; same-reg movs are no-ops)
//       - xAND/xOR(gpr, imm/mem) -> And/Orr (+ armMoveAddressToReg+Ldr for memory)
//       - xMOVAPS(xmm, ptr128)   -> Ldr/Str of the .Q() view via armMoveAddressToReg
//       - xSHUF.PS(xmm,xmm,imm)  -> mVUshufflePS (aVU_IR.h; copy+Ins lane permute)
//
// DEFERRED to the Branch/Compile slice (they call mVUopU/mVUopL, i.e. the opcode
// dispatch tables, which don't exist until the Upper/Lower emit task): the flag
// read-scan _mVUflagPass / mVUflagPass / mVUsetFlagInfo and the shortBranch macro.

// Sets FDIV Flags at the proper time
__fi void mVUdivSet(mV)
{
	if (mVUinfo.doDivFlag)
	{
		if (!sFLAG.doFlag)
			armAsm->Mov(getFlagReg(sFLAG.write), getFlagReg(sFLAG.lastWrite));
		armAsm->And(getFlagReg(sFLAG.write), getFlagReg(sFLAG.write), 0xfff3ffff);
		// xOR(getFlagReg(sFLAG.write), ptr32[&mVU.divFlag]);
		armMoveAddressToReg(RSCRATCHADDR, &mVU.divFlag);
		armAsm->Ldr(gprT1, a64::MemOperand(RSCRATCHADDR));
		armAsm->Orr(getFlagReg(sFLAG.write), getFlagReg(sFLAG.write), gprT1);
	}
}

// Optimizes out unneeded status flag updates
// This can safely be done when there is an FSSET opcode
__fi void mVUstatusFlagOp(mV)
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

int findFlagInst(int* fFlag, int cycles)
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

// Setup Last 4 instances of Status/Mac/Clip flags (needed for accurate block linking)
int sortFlag(int* fFlag, int* bFlag, int cycles)
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
	return x; // Returns the number of Valid Flag Instances
}

void sortFullFlag(int* fFlag, int* bFlag)
{
	int m = std::max(std::max(fFlag[0], fFlag[1]), std::max(fFlag[2], fFlag[3]));
	for (int i = 0; i < 4; i++)
	{
		int t = 3 - (m - fFlag[i]);
		bFlag[i] = (t < 0) ? 0 : t + 1;
	}
}

#define sFlagCond (sFLAG.doFlag || mVUlow.isFSSET || mVUinfo.doDivFlag)
#define sHackCond (mVUsFlagHack && !sFLAG.doNonSticky)

// Note: Flag handling is 'very' complex, it requires full knowledge of how microVU recs work, so don't touch!
__fi void mVUsetFlags(mV, microFlagCycles& mFC)
{
	int endPC = iPC;
	u32 aCount = 0; // Amount of instructions needed to get valid mac flag instances for block linking
	//bool writeProtect = false;

	// Ensure last ~4+ instructions update mac/status flags (if next block's first 4 instructions will read them)
	for (int i = mVUcount; i > 0; i--, aCount++)
	{
		if (sFLAG.doFlag)
		{

			if (__Mac)
			{
				mFLAG.doFlag = true;
				//writeProtect = true;
			}

			if (__Status)
			{
				sFLAG.doNonSticky = true;
				//writeProtect = true;
			}

			if (aCount >= 3)
			{
				break;
			}
		}
		incPC2(-2);
	}

	// Status/Mac Flags Setup Code
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
	u32 xCount = mVUcount; // Backup count
	iPC = mVUstartPC;
	for (mVUcount = 0; mVUcount < xCount; mVUcount++)
	{
		if (mVUlow.isFSSET && !noFlagOpts)
		{
			if (__Status) // Don't Optimize out on the last ~4+ instructions
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
		{
			sFLAG.doFlag = false;
		}

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
	mVUregs.flagInfo |= /*((__Mac||1) ? 0 :*/ (xM << 4)/*)*/; //TODO: Optimise this? Might help with number of blocks.
	mVUregs.flagInfo |= ((__Clip)   ? 0 : (xC << 6));
	iPC = endPC;
}

#define getFlagReg2(x) ((bStatus[0] == x) ? getFlagReg(x) : gprT1)
#define getFlagReg3(x) ((gFlag == x) ? gprT1 : getFlagReg(x))
#define getFlagReg4(x) ((gFlag == x) ? gprT1 : gprT2)
#define shuffleMac     ((bMac[3] << 6) | (bMac[2] << 4) | (bMac[1] << 2) | bMac[0])
#define shuffleClip    ((bClip[3] << 6) | (bClip[2] << 4) | (bClip[1] << 2) | bClip[0])

// Recompiles Code for Proper Flags on Block Linkings
__fi void mVUsetupFlags(mV, microFlagCycles& mFC)
{

	if (mVUregs.flagInfo & 1)
	{
		if (mVUregs.needExactMatch)
			DevCon.Error("mVU ERROR!!!");
	}

	const bool pf = false; // Print Flag Info
	if (pf)
		DevCon.WriteLn("mVU%d - [#%d][sPC=%04x][bPC=%04x][mVUBranch=%d][branch=%d]",
			mVU.index, mVU.prog.cur->idx, mVUstartPC / 2 * 8, xPC, mVUbranch, mVUlow.branch);

	if (doSFlagInsts && __Status)
	{
		if (pf)
			DevCon.WriteLn("mVU%d - Status Flag", mVU.index);
		int bStatus[4];
		int sortRegs = sortFlag(mFC.xStatus, bStatus, mFC.cycles);
		// DevCon::Status("sortRegs = %d", params sortRegs);
		// Note: Emitter will optimize out mov(reg1, reg1) cases...
		if (sortRegs == 1)
		{
			armAsm->Mov(gprF0, getFlagReg(bStatus[0]));
			armAsm->Mov(gprF1, getFlagReg(bStatus[1]));
			armAsm->Mov(gprF2, getFlagReg(bStatus[2]));
			armAsm->Mov(gprF3, getFlagReg(bStatus[3]));
		}
		else if (sortRegs == 2)
		{
			armAsm->Mov(gprT1, getFlagReg (bStatus[3]));
			armAsm->Mov(gprF0, getFlagReg (bStatus[0]));
			armAsm->Mov(gprF1, getFlagReg2(bStatus[1]));
			armAsm->Mov(gprF2, getFlagReg2(bStatus[2]));
			armAsm->Mov(gprF3, gprT1);
		}
		else if (sortRegs == 3)
		{
			int gFlag = (bStatus[0] == bStatus[1]) ? bStatus[2] : bStatus[1];
			armAsm->Mov(gprT1, getFlagReg (gFlag));
			armAsm->Mov(gprT2, getFlagReg (bStatus[3]));
			armAsm->Mov(gprF0, getFlagReg (bStatus[0]));
			armAsm->Mov(gprF1, getFlagReg3(bStatus[1]));
			armAsm->Mov(gprF2, getFlagReg4(bStatus[2]));
			armAsm->Mov(gprF3, gprT2);
		}
		else
		{
			const a64::Register temp3 = mVU.regAlloc->allocGPR();
			armAsm->Mov(gprT1, getFlagReg(bStatus[0]));
			armAsm->Mov(gprT2, getFlagReg(bStatus[1]));
			armAsm->Mov(temp3, getFlagReg(bStatus[2]));
			armAsm->Mov(gprF3, getFlagReg(bStatus[3]));
			armAsm->Mov(gprF0, gprT1);
			armAsm->Mov(gprF1, gprT2);
			armAsm->Mov(gprF2, temp3);
			mVU.regAlloc->clearNeeded(temp3);
		}
	}

	if (doMFlagInsts && __Mac)
	{
		if (pf)
			DevCon.WriteLn("mVU%d - Mac Flag", mVU.index);
		int bMac[4];
		sortFlag(mFC.xMac, bMac, mFC.cycles);
		armMoveAddressToReg(RSCRATCHADDR, &mVU.macFlag[0]);
		armAsm->Ldr(xmmT1.Q(), a64::MemOperand(RSCRATCHADDR));
		mVUshufflePS(xmmT1, xmmT1, shuffleMac);
		armAsm->Str(xmmT1.Q(), a64::MemOperand(RSCRATCHADDR));
	}

	if (doCFlagInsts && __Clip)
	{
		if (pf)
			DevCon.WriteLn("mVU%d - Clip Flag", mVU.index);
		int bClip[4];
		sortFlag(mFC.xClip, bClip, mFC.cycles);
		armMoveAddressToReg(RSCRATCHADDR, &mVU.clipFlag[0]);
		armAsm->Ldr(xmmT2.Q(), a64::MemOperand(RSCRATCHADDR));
		mVUshufflePS(xmmT2, xmmT2, shuffleClip);
		armAsm->Str(xmmT2.Q(), a64::MemOperand(RSCRATCHADDR));
	}
}
