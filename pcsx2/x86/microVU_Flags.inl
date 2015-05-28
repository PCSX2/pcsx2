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
__fi void mVUdivSet(mV) {
	if (mVUinfo.doDivFlag) {
		if (!sFLAG.doFlag) { xMOV(getFlagReg(sFLAG.write), getFlagReg(sFLAG.lastWrite)); }
		xAND(getFlagReg(sFLAG.write), 0xfff3ffff);
		xOR (getFlagReg(sFLAG.write), ptr32[&mVU.divFlag]);
	}
}

// Optimizes out unneeded status flag updates
// This can safely be done when there is an FSSET opcode
__fi void mVUstatusFlagOp(mV) {
	int curPC = iPC;
	int i = mVUcount;
	bool runLoop = true;

	if (sFLAG.doFlag && sFLAG.skipWrite != 1) {
		sFLAG.doNonSticky = true;
	}
	else {
		for (; i > 0; i--) {
			incPC2(-2);
			if (sFLAG.doNonSticky) {
				runLoop = false;
				break;
			}
			else if (sFLAG.doFlag && sFLAG.skipWrite != 1) {
				sFLAG.doNonSticky = true;
				break;
			}
		}
	}
	if (runLoop) {
		for (; i > 0; i--) {
			incPC2(-2);

			if (sFLAG.doNonSticky)
				break;

			sFLAG.doFlag = false;
		}
	}
	iPC = curPC;
	DevCon.WriteLn(Color_Green, "microVU%d: FSSET Optimization", getIndex);
}

int findFlagInst(int* fFlag, int cycles) {
	int j = 0, jValue = -1;
	for(int i = 0; i < 4; i++) {
		if ((fFlag[i] <= cycles) && (fFlag[i] > jValue)) {
			j = i; jValue = fFlag[i];
		}
	}
	return j;
}

// Setup Last 4 instances of Status/Mac/Clip flags (needed for accurate block linking)
int sortFlag(int* fFlag, int* bFlag, int cycles) {
	int lFlag = -5;
	int x = 0;
	for(int i = 0; i < 4; i++) {
		bFlag[i] = findFlagInst(fFlag, cycles);
		if (lFlag != bFlag[i]) { x++; }
		lFlag = bFlag[i];
		cycles++;
	}
	return x; // Returns the number of Valid Flag Instances
}

void sortFullFlag(int* fFlag, int* bFlag) {
	int m = std::max(std::max(fFlag[0], fFlag[1]), std::max(fFlag[2], fFlag[3]));
	for(int i = 0; i < 4; i++) {
		int t = 3 - (m - fFlag[i]);
		bFlag[i] = (t < 0) ? 0 : t+1;
	}
}

#define sFlagCond ((sFLAG.doFlag && sFLAG.skipWrite != 1) || mVUlow.isFSSET || mVUinfo.doDivFlag )
#define sHackCond (mVUsFlagHack && !sFLAG.doNonSticky)

// Note: Flag handling is 'very' complex, it requires full knowledge of how microVU recs work, so don't touch!
__fi void mVUsetFlags(mV, microFlagCycles& mFC) {
	int endPC  = iPC;
	u32 aCount = 0; // Amount of instructions needed to get valid mac flag instances for block linking
	bool writeProtect = false;

	// Ensure last ~4+ instructions update mac/status flags (if next block's first 4 instructions will read them)
	for(int i = mVUcount; i > 0; i--, aCount++) {
		if (sFLAG.doFlag) { 

			if (__Mac) {
				mFLAG.doFlag = true;
				writeProtect = true;
			}

			if (__Status) {
				sFLAG.doNonSticky = true;
				writeProtect = true;
			}
			if (writeProtect == true)
				
			if (aCount >= 3){
				break;
			}
		}
		incPC2(-2);
	}

	if (mVUsFlagHack){
		bool flagReadFound = false;
		bool updateCalcFound = false;
		int flagReadptr = 0;
		incPC2((aCount * 2));
		int instanceCycles = 0;

		// Go through checking flags if they need to be written back or not, ignoring the last 4ish of course
		for (int i = mVUcount; i > 0; i--) {
			if (sFLAG.doFlag) {
				if (writeProtect && (mVUcount - i) <= aCount) { //Don't actually clear any of the last ops in the block, just check for reads
					//DevCon.Warning("aCnt %d, mVUcnt %d mVUcnt-i %d", aCount, mVUcount, (mVUcount - i));
					sFLAG.skipWrite = 2;
				}

				if (flagReadFound) {
					if (instanceCycles >= 4) {
						if (mVUinfo.uOp.VF_write.reg != 0) { //Keep going because this is a mac/status calculation op only
							if (updateCalcFound == false)
								sFLAG.skipWrite = 2;
							//DevCon.Warning("Flag read mac found at %d and %d cycles, rolling back to %d", i, instanceCycles, i + (flagReadptr - 1) * 2);
							flagReadFound = false;
							updateCalcFound = false;
							instanceCycles = 0;
							//Roll back in case there was other flag reads

							incPC2((flagReadptr - 1) * 2);
							i += flagReadptr - 1;
							flagReadptr = 0;
						}
						else {
							//DevCon.Warning("Calculation found!");
							updateCalcFound = true;
							sFLAG.skipWrite = 2;
						}
					}
					//else DevCon.Warning("waiting On %d cycles, flagptr %d pos %d", instanceCycles, flagReadptr, i);
					//Else do nothing, we will come back to this one					
				}
				else {
					if (sFLAG.skipWrite != 2) { //Kill only non-write protected flag updates

						sFLAG.skipWrite = 1;
						//DevCon.Warning("ignoring at %d", i);
					}
					//else DevCon.Warning("Write protected at %d", i);
				}
			}



			if (flagReadFound) {
				if (i == 1) {
					//Nothing found by the end of the block
					flagReadFound = false;

					//Roll back in case there was other flag reads
					if (flagReadptr > 0) {
						incPC2((flagReadptr)* 2);
						i += flagReadptr - 1;
						flagReadptr = 0;
					}
				}
				else { //Relevant flag update instructions not yet found, keep checking the stalls to get the right one
					if (flagReadptr > 0){
						//DevCon.Warning("adding cycles Swap ops %d", mVUinfo.swapOps);
						instanceCycles += mVUstall;
						if (mVUstall == 0)
							instanceCycles += 1;

					}
					flagReadptr++;
				}
			}
			if (mVUlow.readFlags == 1 && !flagReadFound) {
				flagReadFound = true;
				flagReadptr = 0;
				instanceCycles = 0;
				//DevCon.Warning("Found Flag on %d Stall = %d swapops = %d", i, mVUstall, mVUinfo.swapOps);

				if (mVUinfo.swapOps)
					instanceCycles += mVUstall;
				
				instanceCycles += 1;
				//DevCon.Warning("%d cycles on flag read", instanceCycles);
				flagReadptr++;
			}
			incPC2(-2);
		}
	}
	
	// Status/Mac Flags Setup Code
	int xS = 0, xM = 0, xC = 0;
	u32 ff0=0, ff1=0, ffOn=0, fInfo=0;
	
	if (doFullFlagOpt) {
		ff0   = mVUpBlock->pState.fullFlags0;
		ff1   = mVUpBlock->pState.fullFlags1;
		ffOn  = mVUpBlock->pState.flagInfo&1;
		fInfo = mVUpBlock->pState.flagInfo;
	}

	for(int i = 0; i < 4; i++) {
		mFC.xStatus[i] = i;
		mFC.xMac   [i] = i;
		mFC.xClip  [i] = i;
	}
	if (ffOn) { // Full Flags Enabled
		xS = (fInfo >> 2) & 3;
		xM = (fInfo >> 4) & 3;
		xC = (fInfo >> 6) & 3;
		mFC.xStatus[0] = ((ff0 >> (3*0+ 0)) & 7) - 1;
		mFC.xStatus[1] = ((ff0 >> (3*1+ 0)) & 7) - 1;
		mFC.xStatus[2] = ((ff0 >> (3*2+ 0)) & 7) - 1;
		mFC.xStatus[3] = ((ff0 >> (3*3+ 0)) & 7) - 1;
		mFC.xMac   [0] = ((ff0 >> (3*0+12)) & 7) - 1;
		mFC.xMac   [1] = ((ff0 >> (3*1+12)) & 7) - 1;
		mFC.xMac   [2] = ((ff0 >> (3*2+12)) & 7) - 1;
		mFC.xMac   [3] = ((ff0 >> (3*3+12)) & 7) - 1;
		mFC.xClip  [0] = ((ff0 >> (3*0+24)) & 7) - 1;
		mFC.xClip  [1] = ((ff0 >> (3*1+24)) & 7) - 1;
		mFC.xClip  [2] = ((ff1 >> (3*0+ 0)) & 7) - 1;
		mFC.xClip  [3] = ((ff1 >> (3*1+ 0)) & 7) - 1;
	}

	if(!ffOn && !(mVUpBlock->pState.needExactMatch & 1)) {
		xS = (mVUpBlock->pState.flagInfo >> 2) & 3;
		mFC.xStatus[0] = -1; mFC.xStatus[1] = -1;
		mFC.xStatus[2] = -1; mFC.xStatus[3] = -1;
		mFC.xStatus[(xS-1)&3] = 0;
	}

	if(!ffOn && !(mVUpBlock->pState.needExactMatch & 2)) {
		//xM = (mVUpBlock->pState.flagInfo >> 4) & 3;
		mFC.xMac[0] = -1; mFC.xMac[1] = -1;
		mFC.xMac[2] = -1; mFC.xMac[3] = -1;
		//mFC.xMac[(xM-1)&3] = 0;
	}

	if(!ffOn && !(mVUpBlock->pState.needExactMatch & 4)) {
		xC = (mVUpBlock->pState.flagInfo >> 6) & 3;
		mFC.xClip[0] = -1; mFC.xClip[1] = -1;
		mFC.xClip[2] = -1; mFC.xClip[3] = -1;
		mFC.xClip[(xC-1)&3] = 0;
	}

	mFC.cycles	= 0;
	u32 xCount	= mVUcount; // Backup count
	iPC			= mVUstartPC;
	for(mVUcount = 0; mVUcount < xCount; mVUcount++) {
		if (mVUlow.isFSSET && !noFlagOpts) {
			if (__Status) { // Don't Optimize out on the last ~4+ instructions
				if ((xCount - mVUcount) > aCount) { mVUstatusFlagOp(mVU); }
			}
			else mVUstatusFlagOp(mVU);
		}
		mFC.cycles += mVUstall;

		sFLAG.read  = doSFlagInsts ? findFlagInst(mFC.xStatus, mFC.cycles) : 0;
		mFLAG.read  = doMFlagInsts ? findFlagInst(mFC.xMac,    mFC.cycles) : 0;
		cFLAG.read  = doCFlagInsts ? findFlagInst(mFC.xClip,   mFC.cycles) : 0;
		
		sFLAG.write = doSFlagInsts ? xS : 0;
		mFLAG.write = doMFlagInsts ? xM : 0;
		cFLAG.write = doCFlagInsts ? xC : 0;

		sFLAG.lastWrite = doSFlagInsts ? (xS-1) & 3 : 0;
		mFLAG.lastWrite = doMFlagInsts ? (xM-1) & 3 : 0;
		cFLAG.lastWrite = doCFlagInsts ? (xC-1) & 3 : 0;

		if (sHackCond)    {
			sFLAG.doFlag = false;
		}

		if (sFLAG.doFlag) {
			if(noFlagOpts) {
				sFLAG.doNonSticky = true;
				mFLAG.doFlag = true;
			}
		}

		if (sFlagCond) {
			mFC.xStatus[xS] = mFC.cycles + 4;
			xS = (xS+1) & 3;
		}

		if (mFLAG.doFlag) {
			mFC.xMac[xM] = mFC.cycles + 4;
			xM = (xM+1) & 3;
		}

		if (cFLAG.doFlag) {
			mFC.xClip[xC] = mFC.cycles + 4;
			xC = (xC+1) & 3;
		}

		mFC.cycles++;
		incPC2(2);
	}

	mVUregs.flagInfo |= ((__Status) ? 0 : (xS << 2));
	mVUregs.flagInfo |= ((__Mac||1) ? 0 : (xM << 4));
	mVUregs.flagInfo |= ((__Clip)   ? 0 : (xC << 6));
	iPC = endPC;

	if (doFullFlagOpt && (mVUregs.flagInfo & 1)) {
		//if (mVUregs.needExactMatch) DevCon.Error("mVU ERROR!!!");
		int bS[4], bM[4], bC[4];
		sortFullFlag(mFC.xStatus, bS);
		sortFullFlag(mFC.xMac,    bM);
		sortFullFlag(mFC.xClip,   bC);
		mVUregs.flagInfo       = (xC<<6) | (xM<<4) | (xS<<2) | 1;
		mVUregs.fullFlags0     = ((bS[3]<<9)|(bS[2]<<6)|(bS[1]<<3)|(bS[0]<<0)) << (12*0);
		mVUregs.fullFlags0    |= ((bM[3]<<9)|(bM[2]<<6)|(bM[1]<<3)|(bM[0]<<0)) << (12*1);
		mVUregs.fullFlags0    |= ((bC[1]<<3)|(bC[0]<<0)) << (12*2);
		mVUregs.fullFlags1     = ((bC[3]<<3)|(bC[2]<<0)) << (12*0);
		mVUregs.needExactMatch = 0;
		DevCon.WriteLn("MVU FULL FLAG!!!!!!!! [0x%04x][0x%08x][0x%02x]",
			   xPC, mVUregs.fullFlags0, (u32)mVUregs.fullFlags1);
	}
}

#define getFlagReg2(x)	((bStatus[0] == x) ? getFlagReg(x) : gprT1)
#define getFlagReg3(x)	((gFlag == x) ? gprT1 : getFlagReg(x))
#define getFlagReg4(x)	((gFlag == x) ? gprT1 : gprT2)
#define shuffleMac		((bMac [3]<<6)|(bMac [2]<<4)|(bMac [1]<<2)|bMac [0])
#define shuffleClip		((bClip[3]<<6)|(bClip[2]<<4)|(bClip[1]<<2)|bClip[0])

// Recompiles Code for Proper Flags on Block Linkings
__fi void mVUsetupFlags(mV, microFlagCycles& mFC) {

	if (mVUregs.flagInfo & 1) {
		if (mVUregs.needExactMatch) DevCon.Error("mVU ERROR!!!");
	}

	const bool pf = false; // Print Flag Info
	if (pf)	DevCon.WriteLn("mVU%d - [#%d][sPC=%04x][bPC=%04x][mVUBranch=%d][branch=%d]",
			mVU.index, mVU.prog.cur->idx, mVUstartPC/2*8, xPC, mVUbranch, mVUlow.branch);

	if (doSFlagInsts && __Status) {
		if (pf) DevCon.WriteLn("mVU%d - Status Flag", mVU.index);
		int bStatus[4];
		int sortRegs = sortFlag(mFC.xStatus, bStatus, mFC.cycles);
		// DevCon::Status("sortRegs = %d", params sortRegs);
		// Note: Emitter will optimize out mov(reg1, reg1) cases...
		if (sortRegs == 1) {
			xMOV(gprF0,  getFlagReg(bStatus[0]));
			xMOV(gprF1,  getFlagReg(bStatus[1]));
			xMOV(gprF2,  getFlagReg(bStatus[2]));
			xMOV(gprF3,  getFlagReg(bStatus[3]));
		}
		else if (sortRegs == 2) {
			xMOV(gprT1,	 getFlagReg (bStatus[3])); 
			xMOV(gprF0,  getFlagReg (bStatus[0]));
			xMOV(gprF1,  getFlagReg2(bStatus[1]));
			xMOV(gprF2,  getFlagReg2(bStatus[2]));
			xMOV(gprF3,  gprT1);
		}
		else if (sortRegs == 3) {
			int gFlag = (bStatus[0] == bStatus[1]) ? bStatus[2] : bStatus[1];
			xMOV(gprT1,	 getFlagReg (gFlag)); 
			xMOV(gprT2,	 getFlagReg (bStatus[3]));
			xMOV(gprF0,  getFlagReg (bStatus[0]));
			xMOV(gprF1,  getFlagReg3(bStatus[1]));
			xMOV(gprF2,  getFlagReg4(bStatus[2]));
			xMOV(gprF3,  gprT2);
		}
		else {
			xMOV(gprT1,  getFlagReg(bStatus[0])); 
			xMOV(gprT2,  getFlagReg(bStatus[1]));
			xMOV(gprT3,  getFlagReg(bStatus[2]));
			xMOV(gprF3,  getFlagReg(bStatus[3]));
			xMOV(gprF0,  gprT1);
			xMOV(gprF1,  gprT2); 
			xMOV(gprF2,  gprT3);
		}
	}
	
	if (doMFlagInsts && __Mac) {
		if (pf) DevCon.WriteLn("mVU%d - Mac Flag", mVU.index);
		int bMac[4];
		sortFlag(mFC.xMac, bMac, mFC.cycles);
		xMOVAPS(xmmT1, ptr128[mVU.macFlag]);
		xSHUF.PS(xmmT1, xmmT1, shuffleMac);
		xMOVAPS(ptr128[mVU.macFlag], xmmT1);
	}

	if (doCFlagInsts && __Clip) {
		if (pf) DevCon.WriteLn("mVU%d - Clip Flag", mVU.index);
		int bClip[4];
		sortFlag(mFC.xClip, bClip, mFC.cycles);
		xMOVAPS(xmmT2, ptr128[mVU.clipFlag]);
		xSHUF.PS(xmmT2, xmmT2, shuffleClip);
		xMOVAPS(ptr128[mVU.clipFlag], xmmT2);
	}
}

#define shortBranch() {											\
	if ((branch == 3) || (branch == 4)) { /*Branches*/			\
		_mVUflagPass(mVU, aBranchAddr, sCount+found, found, v);	\
		if (branch == 3) break;	/*Non-conditional Branch*/		\
		branch = 0;												\
	}															\
	else if (branch == 5) { /*JR/JARL*/							\
		if(sCount+found<4) {			\
			mVUregs.needExactMatch |= 7;						\
		}														\
		break;													\
	}															\
	else break; /*E-Bit End*/									\
}

// Scan through instructions and check if flags are read (FSxxx, FMxxx, FCxxx opcodes)
void _mVUflagPass(mV, u32 startPC, u32 sCount, u32 found, std::vector<u32>& v) {

	for (u32 i = 0; i < v.size(); i++) {
		if (v[i] == startPC) return; // Prevent infinite recursion
	}
	v.push_back(startPC);

	int oldPC	    = iPC;
	int oldBranch   = mVUbranch;
	int aBranchAddr = 0;
	iPC		  = startPC / 4;
	mVUbranch = 0;
	for(int branch = 0; sCount < 4; sCount += found) {
		mVUregs.needExactMatch &= 7;
		incPC(1);
		mVUopU(mVU, 3);
		found |= (mVUregs.needExactMatch&8)>>3;
		mVUregs.needExactMatch &= 7;
		if (  curI & _Ebit_  )	{ branch = 1; }
		if (  curI & _DTbit_ )	{ branch = 6; }
		if (!(curI & _Ibit_) )	{ incPC(-1); mVUopL(mVU, 3); incPC(1); }
		
		// if (mVUbranch&&(branch>=3)&&(branch<=5)) { DevCon.Error("Double Branch [%x]", xPC); mVUregs.needExactMatch |= 7; break; }
		
		if		(branch >= 2)	{ shortBranch(); }
		else if (branch == 1)	{ branch = 2; }
		if		(mVUbranch)		{ branch = ((mVUbranch>8)?(5):((mVUbranch<3)?3:4)); incPC(-1); aBranchAddr = branchAddr; incPC(1); mVUbranch = 0; }
		incPC(1);
		if ((mVUregs.needExactMatch&7)==7) break;
	}
	iPC		  = oldPC;
	mVUbranch = oldBranch;
	mVUregs.needExactMatch &= 7;
	setCode();
}

void mVUflagPass(mV, u32 startPC, u32 sCount = 0, u32 found = 0) {
	std::vector<u32> v;
	_mVUflagPass(mVU, startPC, sCount, found, v);
}

__fi void checkFFblock(mV, u32 addr, int& ffOpt) {
	if (ffOpt && doFullFlagOpt) {
		blockCreate(addr/8);
		ffOpt = mVUblocks[addr/8]->getFullListCount() <= doFullFlagOpt;
	}
}

// Checks if the first ~4 instructions of a block will read flags
void mVUsetFlagInfo(mV) {
	if (noFlagOpts) {
		mVUregs.needExactMatch  = 0x7;
		mVUregs.fullFlags0      = 0x0;
		mVUregs.fullFlags1      = 0x0;
		mVUregs.flagInfo		= 0x0;
		return;
	}
	int ffOpt = doFullFlagOpt;
	if (mVUbranch <= 2) { // B/BAL
		incPC(-1);
		mVUflagPass (mVU, branchAddr);
		checkFFblock(mVU, branchAddr, ffOpt);
		incPC(1);

		mVUregs.needExactMatch &= 0x7;
		if (mVUregs.needExactMatch && ffOpt) {
			mVUregs.flagInfo |= 1;
		}
	}
	else if (mVUbranch <= 8) { // Conditional Branch
		incPC(-1); // Branch Taken
		mVUflagPass (mVU, branchAddr);
		checkFFblock(mVU, branchAddr, ffOpt);
		int backupFlagInfo     = mVUregs.needExactMatch;
		mVUregs.needExactMatch = 0;
		
		incPC(4); // Branch Not Taken
		mVUflagPass (mVU, xPC);
		checkFFblock(mVU, xPC, ffOpt);
		incPC(-3);

		mVUregs.needExactMatch |= backupFlagInfo;
		mVUregs.needExactMatch &= 0x7;
		if (mVUregs.needExactMatch && ffOpt) {
			mVUregs.flagInfo |= 1;
		}
	}
	else { // JR/JALR
		if (!doConstProp || !mVUlow.constJump.isValid) { mVUregs.needExactMatch |= 0x7; } 
		else { mVUflagPass(mVU, (mVUlow.constJump.regValue*8)&(mVU.microMemSize-8)); }
		mVUregs.needExactMatch &= 0x7;
	}
}
