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

extern void mVUincCycles(microVU& mVU, int x);
extern void* mVUcompile(microVU& mVU, u32 startPC, uptr pState);
__fi int getLastFlagInst(microRegInfo& pState, int* xFlag, int flagType, int isEbit)
{
	if (isEbit)
		return findFlagInst(xFlag, 0x7fffffff);
	if (pState.needExactMatch & (1 << flagType))
		return 3;
	return (((pState.flagInfo >> (2 * flagType + 2)) & 3) - 1) & 3;
}

void mVU0clearlpStateJIT() { if (!microVU0.prog.cleared) memzero(microVU0.prog.lpState); }
void mVU1clearlpStateJIT() { if (!microVU1.prog.cleared) memzero(microVU1.prog.lpState); }

void mVUDTendProgram(mV, microFlagCycles* mFC, int isEbit)
{

	int fStatus = getLastFlagInst(mVUpBlock->pState, mFC->xStatus, 0, isEbit);
	int fMac    = getLastFlagInst(mVUpBlock->pState, mFC->xMac, 1, isEbit);
	int fClip   = getLastFlagInst(mVUpBlock->pState, mFC->xClip, 2, isEbit);
	int qInst   = 0;
	int pInst   = 0;
	microBlock stateBackup;
	memcpy(&stateBackup, &mVUregs, sizeof(mVUregs)); //backup the state, it's about to get screwed with.

	mVU.regAlloc->TDwritebackAll(); //Writing back ok, invalidating early kills the rec, so don't do it :P

	if (isEbit)
	{
		mVUincCycles(mVU, 100); // Ensures Valid P/Q instances (And sets all cycle data to 0)
		mVUcycles -= 100;
		qInst = mVU.q;
		pInst = mVU.p;
		mVUregs.xgkickcycles = 0;
		if (mVUinfo.doDivFlag)
		{
			sFLAG.doFlag = true;
			sFLAG.write = fStatus;
			mVUdivSet(mVU);
		}
		//Run any pending XGKick, providing we've got to it.
		if (mVUinfo.doXGKICK && xPC >= mVUinfo.XGKICKPC)
		{
			mVU_XGKICK_DELAY(mVU);
		}
		if (isVU1 && CHECK_XGKICKHACK)
		{
			mVUlow.kickcycles = 99;
			mVU_XGKICK_SYNC(mVU, true);
		}
		if (!isVU1)
			xFastCall((void*)mVU0clearlpStateJIT);
		else
			xFastCall((void*)mVU1clearlpStateJIT);
	}

	// Save P/Q Regs
	if (qInst)
		xPSHUF.D(xmmPQ, xmmPQ, 0xe1);
	xMOVSS(ptr32[&mVU.regs().VI[REG_Q].UL], xmmPQ);
	xPSHUF.D(xmmPQ, xmmPQ, 0xe1);
	xMOVSS(ptr32[&mVU.regs().pending_q], xmmPQ);
	xPSHUF.D(xmmPQ, xmmPQ, 0xe1);

	if (isVU1)
	{
		if (pInst)
			xPSHUF.D(xmmPQ, xmmPQ, 0xb4); // Swap Pending/Active P
		xPSHUF.D(xmmPQ, xmmPQ, 0xC6); // 3 0 1 2
		xMOVSS(ptr32[&mVU.regs().VI[REG_P].UL], xmmPQ);
		xPSHUF.D(xmmPQ, xmmPQ, 0x87); // 0 2 1 3
		xMOVSS(ptr32[&mVU.regs().pending_p], xmmPQ);
		xPSHUF.D(xmmPQ, xmmPQ, 0x27); // 3 2 1 0
	}

	// Save MAC, Status and CLIP Flag Instances
	mVUallocSFLAGc(gprT1, gprT2, fStatus);
	xMOV(ptr32[&mVU.regs().VI[REG_STATUS_FLAG].UL], gprT1);
	mVUallocMFLAGa(mVU, gprT1, fMac);
	mVUallocCFLAGa(mVU, gprT2, fClip);
	xMOV(ptr32[&mVU.regs().VI[REG_MAC_FLAG].UL], gprT1);
	xMOV(ptr32[&mVU.regs().VI[REG_CLIP_FLAG].UL], gprT2);

	if (!isEbit) // Backup flag instances
	{
		xMOVAPS(xmmT1, ptr128[mVU.macFlag]);
		xMOVAPS(ptr128[&mVU.regs().micro_macflags], xmmT1);
		xMOVAPS(xmmT1, ptr128[mVU.clipFlag]);
		xMOVAPS(ptr128[&mVU.regs().micro_clipflags], xmmT1);

		xMOV(ptr32[&mVU.regs().micro_statusflags[0]], gprF0);
		xMOV(ptr32[&mVU.regs().micro_statusflags[1]], gprF1);
		xMOV(ptr32[&mVU.regs().micro_statusflags[2]], gprF2);
		xMOV(ptr32[&mVU.regs().micro_statusflags[3]], gprF3);
	}
	else // Flush flag instances
	{
		xMOVDZX(xmmT1, ptr32[&mVU.regs().VI[REG_CLIP_FLAG].UL]);
		xSHUF.PS(xmmT1, xmmT1, 0);
		xMOVAPS(ptr128[&mVU.regs().micro_clipflags], xmmT1);

		xMOVDZX(xmmT1, ptr32[&mVU.regs().VI[REG_MAC_FLAG].UL]);
		xSHUF.PS(xmmT1, xmmT1, 0);
		xMOVAPS(ptr128[&mVU.regs().micro_macflags], xmmT1);

		xMOVDZX(xmmT1, getFlagReg(fStatus));
		xSHUF.PS(xmmT1, xmmT1, 0);
		xMOVAPS(ptr128[&mVU.regs().micro_statusflags], xmmT1);
	}

	xMOV(ptr32[&mVU.regs().nextBlockCycles], 0);


	xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);

	if (isEbit) // Clear 'is busy' Flags
	{
		if (!mVU.index || !THREAD_VU1)
		{
			xAND(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? ~0x100 : ~0x001)); // VBS0/VBS1 flag
		}
	}

	if (isEbit != 2) // Save PC, and Jump to Exit Point
	{
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUTBit);
		xJMP(mVU.exitFunct);
	}

	memcpy(&mVUregs, &stateBackup, sizeof(mVUregs)); //Restore the state for the rest of the recompile
}

void mVUendProgram(mV, microFlagCycles* mFC, int isEbit)
{

	int fStatus = getLastFlagInst(mVUpBlock->pState, mFC->xStatus, 0, isEbit && isEbit != 3);
	int fMac    = getLastFlagInst(mVUpBlock->pState, mFC->xMac, 1, isEbit && isEbit != 3);
	int fClip   = getLastFlagInst(mVUpBlock->pState, mFC->xClip, 2, isEbit && isEbit != 3);
	int qInst   = 0;
	int pInst   = 0;
	microBlock stateBackup;
	memcpy(&stateBackup, &mVUregs, sizeof(mVUregs)); //backup the state, it's about to get screwed with.
	if (!isEbit || isEbit == 3)
		mVU.regAlloc->TDwritebackAll(); //Writing back ok, invalidating early kills the rec, so don't do it :P
	else
		mVU.regAlloc->flushAll();

	if (isEbit && isEbit != 3)
	{
		memzero(mVUinfo);
		memzero(mVUregsTemp);
		mVUincCycles(mVU, 100); // Ensures Valid P/Q instances (And sets all cycle data to 0)
		mVUcycles -= 100;
		qInst = mVU.q;
		pInst = mVU.p;
		mVUregs.xgkickcycles = 0;
		if (mVUinfo.doDivFlag)
		{
			sFLAG.doFlag = true;
			sFLAG.write = fStatus;
			mVUdivSet(mVU);
		}
		if (mVUinfo.doXGKICK)
		{
			mVU_XGKICK_DELAY(mVU);
		}
		if (isVU1 && CHECK_XGKICKHACK)
		{
			mVUlow.kickcycles = 99;
			mVU_XGKICK_SYNC(mVU, true);
		}
		if (!isVU1)
			xFastCall((void*)mVU0clearlpStateJIT);
		else
			xFastCall((void*)mVU1clearlpStateJIT);
	}

	// Save P/Q Regs
	if (qInst)
		xPSHUF.D(xmmPQ, xmmPQ, 0xe1);
	xMOVSS(ptr32[&mVU.regs().VI[REG_Q].UL], xmmPQ);
	xPSHUF.D(xmmPQ, xmmPQ, 0xe1);
	xMOVSS(ptr32[&mVU.regs().pending_q], xmmPQ);
	xPSHUF.D(xmmPQ, xmmPQ, 0xe1);

	if (isVU1)
	{
		if (pInst)
			xPSHUF.D(xmmPQ, xmmPQ, 0xb4); // Swap Pending/Active P
		xPSHUF.D(xmmPQ, xmmPQ, 0xC6); // 3 0 1 2
		xMOVSS(ptr32[&mVU.regs().VI[REG_P].UL], xmmPQ);
		xPSHUF.D(xmmPQ, xmmPQ, 0x87); // 0 2 1 3
		xMOVSS(ptr32[&mVU.regs().pending_p], xmmPQ);
		xPSHUF.D(xmmPQ, xmmPQ, 0x27); // 3 2 1 0
	}

	// Save MAC, Status and CLIP Flag Instances
	mVUallocSFLAGc(gprT1, gprT2, fStatus);
	xMOV(ptr32[&mVU.regs().VI[REG_STATUS_FLAG].UL], gprT1);
	mVUallocMFLAGa(mVU, gprT1, fMac);
	mVUallocCFLAGa(mVU, gprT2, fClip);
	xMOV(ptr32[&mVU.regs().VI[REG_MAC_FLAG].UL], gprT1);
	xMOV(ptr32[&mVU.regs().VI[REG_CLIP_FLAG].UL], gprT2);

	if (!isEbit || isEbit == 3) // Backup flag instances
	{
		xMOVAPS(xmmT1, ptr128[mVU.macFlag]);
		xMOVAPS(ptr128[&mVU.regs().micro_macflags], xmmT1);
		xMOVAPS(xmmT1, ptr128[mVU.clipFlag]);
		xMOVAPS(ptr128[&mVU.regs().micro_clipflags], xmmT1);

		xMOV(ptr32[&mVU.regs().micro_statusflags[0]], gprF0);
		xMOV(ptr32[&mVU.regs().micro_statusflags[1]], gprF1);
		xMOV(ptr32[&mVU.regs().micro_statusflags[2]], gprF2);
		xMOV(ptr32[&mVU.regs().micro_statusflags[3]], gprF3);
	}
	else // Flush flag instances
	{
		xMOVDZX(xmmT1, ptr32[&mVU.regs().VI[REG_CLIP_FLAG].UL]);
		xSHUF.PS(xmmT1, xmmT1, 0);
		xMOVAPS(ptr128[&mVU.regs().micro_clipflags], xmmT1);

		xMOVDZX(xmmT1, ptr32[&mVU.regs().VI[REG_MAC_FLAG].UL]);
		xSHUF.PS(xmmT1, xmmT1, 0);
		xMOVAPS(ptr128[&mVU.regs().micro_macflags], xmmT1);

		xMOVDZX(xmmT1, getFlagReg(fStatus));
		xSHUF.PS(xmmT1, xmmT1, 0);
		xMOVAPS(ptr128[&mVU.regs().micro_statusflags], xmmT1);
	}

	xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);

	if ((isEbit && isEbit != 3)) // Clear 'is busy' Flags
	{
		xMOV(ptr32[&mVU.regs().nextBlockCycles], 0);
		if (!mVU.index || !THREAD_VU1)
		{
			xAND(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? ~0x100 : ~0x001)); // VBS0/VBS1 flag
		}
	}
	else if(isEbit)
	{
		xMOV(ptr32[&mVU.regs().nextBlockCycles], 0);
	}

	if (isEbit != 2 && isEbit != 3) // Save PC, and Jump to Exit Point
	{
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUEBit);
		xJMP(mVU.exitFunct);
	}
	memcpy(&mVUregs, &stateBackup, sizeof(mVUregs)); //Restore the state for the rest of the recompile
}

// Recompiles Code for Proper Flags and Q/P regs on Block Linkings
void mVUsetupBranch(mV, microFlagCycles& mFC)
{
	mVU.regAlloc->flushAll(); // Flush Allocated Regs
	mVUsetupFlags(mVU, mFC);  // Shuffle Flag Instances

	// Shuffle P/Q regs since every block starts at instance #0
	if (mVU.p || mVU.q)
		xPSHUF.D(xmmPQ, xmmPQ, shufflePQ);
	mVU.p = 0, mVU.q = 0;
}

void normBranchCompile(microVU& mVU, u32 branchPC)
{
	microBlock* pBlock;
	blockCreate(branchPC / 8);
	pBlock = mVUblocks[branchPC / 8]->search((microRegInfo*)&mVUregs);
	if (pBlock)
		xJMP(pBlock->x86ptrStart);
	else
		mVUcompile(mVU, branchPC, (uptr)&mVUregs);
}

void normJumpCompile(mV, microFlagCycles& mFC, bool isEvilJump)
{
	memcpy(&mVUpBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));
	mVUsetupBranch(mVU, mFC);
	mVUbackupRegs(mVU);

	if (!mVUpBlock->jumpCache) // Create the jump cache for this block
	{
		mVUpBlock->jumpCache = new microJumpCache[mProgSize / 2];
	}

	if (isEvilJump)
	{
		xMOV(arg1regd, ptr32[&mVU.evilBranch]);
		xMOV(gprT1, ptr32[&mVU.evilevilBranch]);
		xMOV(ptr32[&mVU.evilBranch], gprT1);
	}
	else
		xMOV(arg1regd, ptr32[&mVU.branch]);
	if (doJumpCaching)
		xLoadFarAddr(arg2reg, mVUpBlock);
	else
		xLoadFarAddr(arg2reg, &mVUpBlock->pStateEnd);

	if (mVUup.eBit && isEvilJump) // E-bit EvilJump
	{
		//Xtreme G 3 does 2 conditional jumps, the first contains an E Bit on the first instruction
		//So if it is taken, you need to end the program, else you get infinite loops.
		mVUendProgram(mVU, &mFC, 2);
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], arg1regd);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUEBit);
		xJMP(mVU.exitFunct);
	}

	if (!mVU.index)
		xFastCall((void*)(void (*)())mVUcompileJIT<0>, arg1reg, arg2reg); //(u32 startPC, uptr pState)
	else
		xFastCall((void*)(void (*)())mVUcompileJIT<1>, arg1reg, arg2reg);

	mVUrestoreRegs(mVU);
	xJMP(gprT1q); // Jump to rec-code address
}

void normBranch(mV, microFlagCycles& mFC)
{

	// E-bit or T-Bit or D-Bit Branch
	if (mVUup.dBit && doDBitHandling)
	{
		u32 tempPC = iPC;
		if (mVU.index && THREAD_VU1)
			xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x400 : 0x4));
		else
			xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x400 : 0x4));
		xForwardJump32 eJMP(Jcc_Zero);
		if (!mVU.index || !THREAD_VU1)
		{
			xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x200 : 0x2));
			xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
		}
		iPC = branchAddr(mVU) / 4;
		mVUDTendProgram(mVU, &mFC, 1);
		eJMP.SetTarget();
		iPC = tempPC;
	}
	if (mVUup.tBit)
	{
		u32 tempPC = iPC;
		if (mVU.index && THREAD_VU1)
			xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x800 : 0x8));
		else
			xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x800 : 0x8));
		xForwardJump32 eJMP(Jcc_Zero);
		if (!mVU.index || !THREAD_VU1)
		{
			xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x400 : 0x4));
			xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
		}
		iPC = branchAddr(mVU) / 4;
		mVUDTendProgram(mVU, &mFC, 1);
		eJMP.SetTarget();
		iPC = tempPC;
	}
	if (mVUup.mBit)
	{
		DevCon.Warning("M-Bit on normal branch, report if broken");
		u32 tempPC = iPC;
		u32* cpS = (u32*)&mVUregs;
		u32* lpS = (u32*)&mVU.prog.lpState;
		for (size_t i = 0; i < (sizeof(microRegInfo) - 4) / 4; i++, lpS++, cpS++)
		{
			xMOV(ptr32[lpS], cpS[0]);
		}
		mVUsetupBranch(mVU, mFC);
		mVUendProgram(mVU, &mFC, 3);
		iPC = branchAddr(mVU) / 4;
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUEBit);
		xJMP(mVU.exitFunct);
		iPC = tempPC;
	}
	if (mVUup.eBit)
	{
		if (mVUlow.badBranch)
			DevCon.Warning("End on evil Unconditional branch! - Not implemented! - If game broken report to PCSX2 Team");

		iPC = branchAddr(mVU) / 4;
		mVUendProgram(mVU, &mFC, 1);
		return;
	}

	// Normal Branch
	mVUsetupBranch(mVU, mFC);
	normBranchCompile(mVU, branchAddr(mVU));
}

void condBranch(mV, microFlagCycles& mFC, int JMPcc)
{
	mVUsetupBranch(mVU, mFC);

	if (mVUup.tBit)
	{
		DevCon.Warning("T-Bit on branch, please report if broken");
		u32 tempPC = iPC;
		if (mVU.index && THREAD_VU1)
			xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x800 : 0x8));
		else
			xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x800 : 0x8));
		xForwardJump32 eJMP(Jcc_Zero);
		if (!mVU.index || !THREAD_VU1)
		{
			xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x400 : 0x4));
			xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		xCMP(ptr16[&mVU.branch], 0);
		xForwardJump32 tJMP(xInvertCond((JccComparisonType)JMPcc));
			incPC(4); // Set PC to First instruction of Non-Taken Side
			xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
			if (mVU.index && THREAD_VU1)
				xFastCall((void*)mVUTBit);
			xJMP(mVU.exitFunct);
		tJMP.SetTarget();
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUTBit);
		xJMP(mVU.exitFunct);
		eJMP.SetTarget();
		iPC = tempPC;
	}
	if (mVUup.dBit && doDBitHandling)
	{
		u32 tempPC = iPC;
		if (mVU.index  && THREAD_VU1)
			xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x400 : 0x4));
		else
			xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x400 : 0x4));
		xForwardJump32 eJMP(Jcc_Zero);
		if (!mVU.index || !THREAD_VU1)
		{
			xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x200 : 0x2));
			xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		xCMP(ptr16[&mVU.branch], 0);
		xForwardJump32 dJMP(xInvertCond((JccComparisonType)JMPcc));
			incPC(4); // Set PC to First instruction of Non-Taken Side
			xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
			xJMP(mVU.exitFunct);
		dJMP.SetTarget();
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
		xJMP(mVU.exitFunct);
		eJMP.SetTarget();
		iPC = tempPC;
	}
	if (mVUup.mBit)
	{
		u32 tempPC = iPC;
		u32* cpS = (u32*)&mVUregs;
		u32* lpS = (u32*)&mVU.prog.lpState;
		for (size_t i = 0; i < (sizeof(microRegInfo) - 4) / 4; i++, lpS++, cpS++)
		{
			xMOV(ptr32[lpS], cpS[0]);
		}
		mVUendProgram(mVU, &mFC, 3);
		xCMP(ptr16[&mVU.branch], 0);
		xForwardJump32 dJMP((JccComparisonType)JMPcc);
		incPC(4); // Set PC to First instruction of Non-Taken Side
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUEBit);
		xJMP(mVU.exitFunct);
		dJMP.SetTarget();
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUEBit);
		xJMP(mVU.exitFunct);
		iPC = tempPC;
	}
	if (mVUup.eBit) // Conditional Branch With E-Bit Set
	{
		if (mVUlow.evilBranch)
			DevCon.Warning("End on evil branch! - Not implemented! - If game broken report to PCSX2 Team");

		mVUendProgram(mVU, &mFC, 2);
		xCMP(ptr16[&mVU.branch], 0);

		incPC(3);
		xForwardJump32 eJMP(((JccComparisonType)JMPcc));
			incPC(1); // Set PC to First instruction of Non-Taken Side
			xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
			if (mVU.index && THREAD_VU1)
				xFastCall((void*)mVUEBit);
			xJMP(mVU.exitFunct);
		eJMP.SetTarget();
		incPC(-4); // Go Back to Branch Opcode to get branchAddr

		iPC = branchAddr(mVU) / 4;
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], xPC);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUEBit);
		xJMP(mVU.exitFunct);
		return;
	}
	else // Normal Conditional Branch
	{
		xCMP(ptr16[&mVU.branch], 0);

		incPC(3);
		microBlock* bBlock;
		incPC2(1); // Check if Branch Non-Taken Side has already been recompiled
		blockCreate(iPC / 2);
		bBlock = mVUblocks[iPC / 2]->search((microRegInfo*)&mVUregs);
		incPC2(-1);
		if (bBlock) // Branch non-taken has already been compiled
		{
			xJcc(xInvertCond((JccComparisonType)JMPcc), bBlock->x86ptrStart);
			incPC(-3); // Go back to branch opcode (to get branch imm addr)
			normBranchCompile(mVU, branchAddr(mVU));
		}
		else
		{
			s32* ajmp = xJcc32((JccComparisonType)JMPcc);
			u32 bPC = iPC; // mVUcompile can modify iPC, mVUpBlock, and mVUregs so back them up
			microBlock* pBlock = mVUpBlock;
			memcpy(&pBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));

			incPC2(1); // Get PC for branch not-taken
			mVUcompile(mVU, xPC, (uptr)&mVUregs);

			iPC = bPC;
			incPC(-3); // Go back to branch opcode (to get branch imm addr)
			uptr jumpAddr = (uptr)mVUblockFetch(mVU, branchAddr(mVU), (uptr)&pBlock->pStateEnd);
			*ajmp = (jumpAddr - ((uptr)ajmp + 4));
		}
	}
}

void normJump(mV, microFlagCycles& mFC)
{
	if (mVUup.mBit)
	{
		DevCon.Warning("M-Bit on Jump! Please report if broken");
	}
	if (mVUlow.constJump.isValid) // Jump Address is Constant
	{
		if (mVUup.eBit) // E-bit Jump
		{
			iPC = (mVUlow.constJump.regValue * 2) & (mVU.progMemMask);
			mVUendProgram(mVU, &mFC, 1);
			return;
		}
		int jumpAddr = (mVUlow.constJump.regValue * 8) & (mVU.microMemSize - 8);
		mVUsetupBranch(mVU, mFC);
		normBranchCompile(mVU, jumpAddr);
		return;
	}
	if (mVUup.dBit && doDBitHandling)
	{
		if (THREAD_VU1)
			xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x400 : 0x4));
		else
			xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x400 : 0x4));
		xForwardJump32 eJMP(Jcc_Zero);
		if (!mVU.index || !THREAD_VU1)
		{
			xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x200 : 0x2));
			xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		xMOV(gprT1, ptr32[&mVU.branch]);
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], gprT1);
		xJMP(mVU.exitFunct);
		eJMP.SetTarget();
	}
	if (mVUup.tBit)
	{
		if (mVU.index && THREAD_VU1)
			xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x800 : 0x8));
		else
			xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x800 : 0x8));
		xForwardJump32 eJMP(Jcc_Zero);
		if (!mVU.index || !THREAD_VU1)
		{
			xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x400 : 0x4));
			xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		xMOV(gprT1, ptr32[&mVU.branch]);
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], gprT1);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUTBit);
		xJMP(mVU.exitFunct);
		eJMP.SetTarget();
	}
	if (mVUup.eBit) // E-bit Jump
	{
		mVUendProgram(mVU, &mFC, 2);
		xMOV(gprT1, ptr32[&mVU.branch]);
		xMOV(ptr32[&mVU.regs().VI[REG_TPC].UL], gprT1);
		if (mVU.index && THREAD_VU1)
			xFastCall((void*)mVUEBit);
		xJMP(mVU.exitFunct);
	}
	else
	{
		normJumpCompile(mVU, mFC, false);
	}
}
