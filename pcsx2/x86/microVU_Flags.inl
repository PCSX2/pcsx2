/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

// Sets FDIV Flags at the proper time
__fi void mVUdivSet(mV)
{
	if (mVUinfo.doDivFlag)
	{
		if (!sFLAG.doFlag)
			xMOV(getFlagReg(sFLAG.write), getFlagReg(sFLAG.lastWrite));
		xAND(getFlagReg(sFLAG.write), 0xfff3ffff);
		xOR(getFlagReg(sFLAG.write), ptr32[&mVU.divFlag]);
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
			xMOV(gprF0, getFlagReg(bStatus[0]));
			xMOV(gprF1, getFlagReg(bStatus[1]));
			xMOV(gprF2, getFlagReg(bStatus[2]));
			xMOV(gprF3, getFlagReg(bStatus[3]));
		}
		else if (sortRegs == 2)
		{
			xMOV(gprT1, getFlagReg (bStatus[3]));
			xMOV(gprF0, getFlagReg (bStatus[0]));
			xMOV(gprF1, getFlagReg2(bStatus[1]));
			xMOV(gprF2, getFlagReg2(bStatus[2]));
			xMOV(gprF3, gprT1);
		}
		else if (sortRegs == 3)
		{
			int gFlag = (bStatus[0] == bStatus[1]) ? bStatus[2] : bStatus[1];
			xMOV(gprT1, getFlagReg (gFlag));
			xMOV(gprT2, getFlagReg (bStatus[3]));
			xMOV(gprF0, getFlagReg (bStatus[0]));
			xMOV(gprF1, getFlagReg3(bStatus[1]));
			xMOV(gprF2, getFlagReg4(bStatus[2]));
			xMOV(gprF3, gprT2);
		}
		else
		{
			xMOV(gprT1, getFlagReg(bStatus[0]));
			xMOV(gprT2, getFlagReg(bStatus[1]));
			xMOV(gprT3, getFlagReg(bStatus[2]));
			xMOV(gprF3, getFlagReg(bStatus[3]));
			xMOV(gprF0, gprT1);
			xMOV(gprF1, gprT2);
			xMOV(gprF2, gprT3);
		}
	}

	if (doMFlagInsts && __Mac)
	{
		if (pf)
			DevCon.WriteLn("mVU%d - Mac Flag", mVU.index);
		int bMac[4];
		sortFlag(mFC.xMac, bMac, mFC.cycles);
		xMOVAPS(xmmT1, ptr128[mVU.macFlag]);
		xSHUF.PS(xmmT1, xmmT1, shuffleMac);
		xMOVAPS(ptr128[mVU.macFlag], xmmT1);
	}

	if (doCFlagInsts && __Clip)
	{
		if (pf)
			DevCon.WriteLn("mVU%d - Clip Flag", mVU.index);
		int bClip[4];
		sortFlag(mFC.xClip, bClip, mFC.cycles);
		xMOVAPS(xmmT2, ptr128[mVU.clipFlag]);
		xSHUF.PS(xmmT2, xmmT2, shuffleClip);
		xMOVAPS(ptr128[mVU.clipFlag], xmmT2);
	}
}

#define shortBranch() \
	{ \
		if ((branch == 3) || (branch == 4)) /*Branches*/ \
		{ \
			_mVUflagPass(mVU, aBranchAddr, sCount + found, found, v); \
			if (branch == 3) /*Non-conditional Branch*/ \
				break; \
			branch = 0; \
		} \
		else if (branch == 5) /*JR/JARL*/ \
		{ \
			if (sCount + found < 4) \
				mVUregs.needExactMatch |= 7; \
			break; \
		} \
		else /*E-Bit End*/ \
			break; \
	}

// Scan through instructions and check if flags are read (FSxxx, FMxxx, FCxxx opcodes)
void _mVUflagPass(mV, u32 startPC, u32 sCount, u32 found, std::vector<u32>& v)
{

	for (u32 i = 0; i < v.size(); i++)
	{
		if (v[i] == startPC)
			return; // Prevent infinite recursion
	}
	v.push_back(startPC);

	int oldPC = iPC;
	int oldBranch = mVUbranch;
	int aBranchAddr = 0;
	iPC = startPC / 4;
	mVUbranch = 0;
	for (int branch = 0; sCount < 4; sCount += found)
	{
		mVUregs.needExactMatch &= 7;
		incPC(1);
		mVUopU(mVU, 3);
		found |= (mVUregs.needExactMatch & 8) >> 3;
		mVUregs.needExactMatch &= 7;
		if (curI & _Ebit_)
		{
			branch = 1;
		}
		if (curI & _Tbit_)
		{
			branch = 6;
		}
		if ((curI & _Dbit_) && doDBitHandling)
		{
			branch = 6;
		}
		if (!(curI & _Ibit_))
		{
			incPC(-1);
			mVUopL(mVU, 3);
			incPC(1);
		}

		if (branch >= 2)
		{
			shortBranch();
		}
		else if (branch == 1)
		{
			branch = 2;
		}
		if (mVUbranch)
		{
			branch = ((mVUbranch > 8) ? (5) : ((mVUbranch < 3) ? 3 : 4));
			incPC(-1);
			aBranchAddr = branchAddr(mVU);
			incPC(1);
			mVUbranch = 0;
		}
		incPC(1);
		if ((mVUregs.needExactMatch & 7) == 7)
			break;
	}
	iPC = oldPC;
	mVUbranch = oldBranch;
	mVUregs.needExactMatch &= 7;
	setCode();
}

void mVUflagPass(mV, u32 startPC, u32 sCount = 0, u32 found = 0)
{
	std::vector<u32> v;
	_mVUflagPass(mVU, startPC, sCount, found, v);
}

// Checks if the first ~4 instructions of a block will read flags
void mVUsetFlagInfo(mV)
{
	if (noFlagOpts)
	{
		mVUregs.needExactMatch = 0x7;
		mVUregs.flagInfo = 0x0;
		return;
	}
	if (mVUbranch <= 2) // B/BAL
	{
		incPC(-1);
		mVUflagPass(mVU, branchAddr(mVU));
		incPC(1);

		mVUregs.needExactMatch &= 0x7;
	}
	else if (mVUbranch <= 8) // Conditional Branch
	{
		incPC(-1); // Branch Taken
		mVUflagPass(mVU, branchAddr(mVU));
		int backupFlagInfo = mVUregs.needExactMatch;
		mVUregs.needExactMatch = 0;

		incPC(4); // Branch Not Taken
		mVUflagPass(mVU, xPC);
		incPC(-3);

		mVUregs.needExactMatch |= backupFlagInfo;
		mVUregs.needExactMatch &= 0x7;
	}
	else // JR/JALR
	{
		if (!doConstProp || !mVUlow.constJump.isValid)
		{
			mVUregs.needExactMatch |= 0x7;
		}
		else
		{
			mVUflagPass(mVU, (mVUlow.constJump.regValue * 8) & (mVU.microMemSize - 8));
		}
		mVUregs.needExactMatch &= 0x7;
	}
}
