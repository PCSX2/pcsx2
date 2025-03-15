// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Messages Called at Execution Time...
//------------------------------------------------------------------

static inline void mVUbadOp0  (u32 prog, u32 pc) { Console.Error("microVU0 Warning: Exiting... Block contains an illegal opcode. [%04x] [%03d]", pc, prog); }
static inline void mVUbadOp1  (u32 prog, u32 pc) { Console.Error("microVU1 Warning: Exiting... Block contains an illegal opcode. [%04x] [%03d]", pc, prog); }
static inline void mVUwarning0(u32 prog, u32 pc) { Console.Error("microVU0 Warning: Exiting from Possible Infinite Loop [%04x] [%03d]", pc, prog); }
static inline void mVUwarning1(u32 prog, u32 pc) { Console.Error("microVU1 Warning: Exiting from Possible Infinite Loop [%04x] [%03d]", pc, prog); }
static inline void mVUprintPC1(u32 pc) { Console.WriteLn("Block Start PC = 0x%04x", pc); }
static inline void mVUprintPC2(u32 pc) { Console.WriteLn("Block End PC   = 0x%04x", pc); }

//------------------------------------------------------------------
// Program Range Checking and Setting up Ranges
//------------------------------------------------------------------

// Used by mVUsetupRange
__fi void mVUcheckIsSame(mV)
{
	if (mVU.prog.isSame == -1)
	{
		mVU.prog.isSame = !memcmp((u8*)mVUcurProg.data, mVU.regs().Micro, mVU.microMemSize);
	}
	if (mVU.prog.isSame == 0)
	{
		mVUcacheProg(mVU, *mVU.prog.cur);
		mVU.prog.isSame = 1;
	}
}

// Sets up microProgram PC ranges based on whats been recompiled
void mVUsetupRange(microVU& mVU, s32 pc, bool isStartPC)
{
	std::deque<microRange>*& ranges = mVUcurProg.ranges;
	if (pc > (s64)mVU.microMemSize)
	{
		Console.Error("microVU%d: PC outside of VU memory PC=0x%04x", mVU.index, pc);
		pxFail("microVU: PC out of VU memory");
	}

	// The PC handling will prewrap the PC so we need to set the end PC to the end of the micro memory, but only if it wraps, no more.
	const s32 cur_pc = (!isStartPC && mVUrange.start > pc && pc == 0) ? mVU.microMemSize : pc;

	if (isStartPC) // Check if startPC is already within a block we've recompiled
	{
		std::deque<microRange>::const_iterator it(ranges->begin());
		for (; it != ranges->end(); ++it)
		{
			if ((cur_pc >= it[0].start) && (cur_pc <= it[0].end))
			{
				if (it[0].start != it[0].end)
				{
					microRange mRange = {it[0].start, it[0].end};
					ranges->erase(it);
					ranges->push_front(mRange);
					return; // new start PC is inside the range of another range
				}
			}
		}
	}
	else if (mVUrange.end >= cur_pc)
	{
		// existing range covers more area than current PC so no need to process it
		return;
	}
	
	if (doWholeProgCompare)
		mVUcheckIsSame(mVU);

	if (isStartPC)
	{
		microRange mRange = {cur_pc, -1};
		ranges->push_front(mRange);
		return;
	}

	if (mVUrange.start <= cur_pc)
	{
		mVUrange.end = cur_pc;
		s32 rStart = mVUrange.start;
		s32 rEnd = mVUrange.end;
		for (auto it = ranges->begin() + 1; it != ranges->end();)
		{
			if (((it->start >= rStart) && (it->start <= rEnd)) || ((it->end >= rStart) && (it->end <= rEnd))) // Starts after this prog but starts before the end of current prog
			{
				mVUrange.start = rStart = std::min(it->start, rStart); // Choose the earlier start
				mVUrange.end = rEnd = std::max(it->end, rEnd);
				it = ranges->erase(it);
			}
			else
				it++;
		}
	}
	else
	{
		mVUrange.end = mVU.microMemSize;
		DevCon.WriteLn(Color_Green, "microVU%d: Prog Range Wrap [%04x] [%04x] PC %x", mVU.index, mVUrange.start, mVUrange.end, cur_pc);
		microRange mRange = {0, cur_pc };
		ranges->push_front(mRange);
	}

	if(!doWholeProgCompare)
		mVUcacheProg(mVU, *mVU.prog.cur);
}

//------------------------------------------------------------------
// Execute VU Opcode/Instruction (Upper and Lower)
//------------------------------------------------------------------

__ri void doUpperOp(mV)
{
	mVUopU(mVU, 1);
	mVUdivSet(mVU);
}
__ri void doLowerOp(mV)
{
	incPC(-1);
	mVUopL(mVU, 1);
	incPC(1);
}
__ri void flushRegs(mV)
{
	if (!doRegAlloc)
		mVU.regAlloc->flushAll();
}

void doIbit(mV)
{
	if (mVUup.iBit)
	{
		incPC(-1);
		mVU.regAlloc->clearRegVF(33);
		if (EmuConfig.Gamefixes.IbitHack)
		{
			xMOV(gprT1, ptr32[&curI]);
			xMOV(ptr32[&mVU.getVI(REG_I)], gprT1);
		}
		else
		{
			u32 tempI;
			if (CHECK_VU_OVERFLOW(mVU.index) && ((curI & 0x7fffffff) >= 0x7f800000))
			{
				DevCon.WriteLn(Color_Green, "microVU%d: Clamping I Reg", mVU.index);
				tempI = (0x80000000 & curI) | 0x7f7fffff; // Clamp I Reg
			}
			else
				tempI = curI;

			xMOV(ptr32[&mVU.getVI(REG_I)], tempI);
		}
		incPC(1);
	}
}

void doSwapOp(mV)
{
	if (mVUinfo.backupVF && !mVUlow.noWriteVF)
	{
		DevCon.WriteLn(Color_Green, "microVU%d: Backing Up VF Reg [%04x]", getIndex, xPC);

		// Allocate t1 first for better chance of reg-alloc
		const xmm& t1 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg);
		const xmm& t2 = mVU.regAlloc->allocReg();
		xMOVAPS(t2, t1); // Backup VF reg
		mVU.regAlloc->clearNeeded(t1);

		mVUopL(mVU, 1);

		const xmm& t3 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg, mVUlow.VF_write.reg, 0xf, 0);
		xXOR.PS(t2, t3); // Swap new and old values of the register
		xXOR.PS(t3, t2); // Uses xor swap trick...
		xXOR.PS(t2, t3);
		mVU.regAlloc->clearNeeded(t3);

		incPC(1);
		doUpperOp(mVU);

		const xmm& t4 = mVU.regAlloc->allocReg(-1, mVUlow.VF_write.reg, 0xf);
		xMOVAPS(t4, t2);
		mVU.regAlloc->clearNeeded(t4);
		mVU.regAlloc->clearNeeded(t2);
	}
	else
	{
		mVUopL(mVU, 1);
		incPC(1);
		flushRegs(mVU);
		doUpperOp(mVU);
	}
}

void mVUexecuteInstruction(mV)
{
	if (mVUlow.isNOP)
	{
		incPC(1);
		doUpperOp(mVU);
		flushRegs(mVU);
		doIbit(mVU);
	}
	else if (!mVUinfo.swapOps)
	{
		incPC(1);
		doUpperOp(mVU);
		flushRegs(mVU);
		doLowerOp(mVU);
	}
	else
	{
		doSwapOp(mVU);
	}

	flushRegs(mVU);
}

//------------------------------------------------------------------
// Warnings / Errors / Illegal Instructions
//------------------------------------------------------------------

// If 1st op in block is a bad opcode, then don't compile rest of block (Dawn of Mana Level 2)
__fi void mVUcheckBadOp(mV)
{

	// The BIOS writes upper and lower NOPs in reversed slots (bug)
	//So to prevent spamming we ignore these, however its possible the real VU will bomb out if
	//this happens, so we will bomb out without warning.
	if (mVUinfo.isBadOp && mVU.code != 0x8000033c)
	{

		mVUinfo.isEOB = true;
		DevCon.Warning("microVU Warning: Block contains an illegal opcode...");
	}
}

// Prints msg when exiting block early if 1st op was a bad opcode (Dawn of Mana Level 2)
// #ifdef PCSX2_DEVBUILD because starting with SVN R5586 we get log spam in releases (Shadow Hearts battles)
__fi void handleBadOp(mV, int count)
{
#ifdef PCSX2_DEVBUILD
	if (mVUinfo.isBadOp)
	{
		mVUbackupRegs(mVU, true);
		if (!isVU1) xFastCall(mVUbadOp0, mVU.prog.cur->idx, xPC);
		else        xFastCall(mVUbadOp1, mVU.prog.cur->idx, xPC);
		mVUrestoreRegs(mVU, true);
	}
#endif
}

__ri void branchWarning(mV)
{
	incPC(-2);
	if (mVUup.eBit && mVUbranch)
	{
		incPC(2);
		DevCon.Warning("microVU%d Warning: Branch in E-bit delay slot! [%04x]", mVU.index, xPC);
		mVUlow.isNOP = true;
	}
	else
		incPC(2);

	if (mVUinfo.isBdelay && !mVUlow.evilBranch) // Check if VI Reg Written to on Branch Delay Slot Instruction
	{
		if (mVUlow.VI_write.reg && mVUlow.VI_write.used && !mVUlow.readFlags)
		{
			mVUlow.backupVI = true;
			mVUregs.viBackUp = mVUlow.VI_write.reg;
		}
	}
}

__fi void eBitPass1(mV, int& branch)
{
	if (mVUregs.blockType != 1)
	{
		branch = 1;
		mVUup.eBit = true;
	}
}

__ri void eBitWarning(mV)
{
	if (mVUpBlock->pState.blockType == 1)
		Console.Error("microVU%d Warning: Branch, E-bit, Branch! [%04x]",  mVU.index, xPC);
	if (mVUpBlock->pState.blockType == 2)
		DevCon.Warning("microVU%d Warning: Branch, Branch, Branch! [%04x]", mVU.index, xPC);
	incPC(2);
	if (curI & _Ebit_)
	{
		DevCon.Warning("microVU%d: E-bit in Branch delay slot! [%04x]", mVU.index, xPC);
		mVUregs.blockType = 1;
	}
	incPC(-2);
}

//------------------------------------------------------------------
// Cycles / Pipeline State / Early Exit from Execution
//------------------------------------------------------------------
__fi u8 optimizeReg(u8 rState) { return (rState == 1) ? 0 : rState; }
__fi u8 calcCycles(u8 reg, u8 x) { return ((reg > x) ? (reg - x) : 0); }
__fi u8 tCycles(u8 dest, u8 src) { return std::max(dest, src); }
__fi void incP(mV) { mVU.p ^= 1; }
__fi void incQ(mV) { mVU.q ^= 1; }

// Optimizes the End Pipeline State Removing Unnecessary Info
// If the cycles remaining is just '1', we don't have to transfer it to the next block
// because mVU automatically decrements this number at the start of its loop,
// so essentially '1' will be the same as '0'...
void mVUoptimizePipeState(mV)
{
	for (int i = 0; i < 32; i++)
	{
		mVUregs.VF[i].x = optimizeReg(mVUregs.VF[i].x);
		mVUregs.VF[i].y = optimizeReg(mVUregs.VF[i].y);
		mVUregs.VF[i].z = optimizeReg(mVUregs.VF[i].z);
		mVUregs.VF[i].w = optimizeReg(mVUregs.VF[i].w);
	}
	for (int i = 0; i < 16; i++)
	{
		mVUregs.VI[i] = optimizeReg(mVUregs.VI[i]);
	}
	if (mVUregs.q) { mVUregs.q = optimizeReg(mVUregs.q); if (!mVUregs.q) { incQ(mVU); } }
	if (mVUregs.p) { mVUregs.p = optimizeReg(mVUregs.p); if (!mVUregs.p) { incP(mVU); } }
	mVUregs.r = 0; // There are no stalls on the R-reg, so its Safe to discard info
}

void mVUincCycles(mV, int x)
{
	mVUcycles += x;
	// VF[0] is a constant value (0.0 0.0 0.0 1.0)
	for (int z = 31; z > 0; z--)
	{
		mVUregs.VF[z].x = calcCycles(mVUregs.VF[z].x, x);
		mVUregs.VF[z].y = calcCycles(mVUregs.VF[z].y, x);
		mVUregs.VF[z].z = calcCycles(mVUregs.VF[z].z, x);
		mVUregs.VF[z].w = calcCycles(mVUregs.VF[z].w, x);
	}
	// VI[0] is a constant value (0)
	for (int z = 15; z > 0; z--)
	{
		mVUregs.VI[z] = calcCycles(mVUregs.VI[z], x);
	}
	if (mVUregs.q)
	{
		if (mVUregs.q > 4)
		{
			mVUregs.q = calcCycles(mVUregs.q, x);
			if (mVUregs.q <= 4)
			{
				mVUinfo.doDivFlag = 1;
			}
		}
		else
		{
			mVUregs.q = calcCycles(mVUregs.q, x);
		}
		if (!mVUregs.q)
			incQ(mVU);
	}
	if (mVUregs.p)
	{
		mVUregs.p = calcCycles(mVUregs.p, x);
		if (!mVUregs.p || mVUregsTemp.p)
			incP(mVU);
	}
	if (mVUregs.xgkick)
	{
		mVUregs.xgkick = calcCycles(mVUregs.xgkick, x);
		if (!mVUregs.xgkick)
		{
			mVUinfo.doXGKICK = 1;
			mVUinfo.XGKICKPC = xPC;
		}
	}
	mVUregs.r = calcCycles(mVUregs.r, x);
}

// Helps check if upper/lower ops read/write to same regs...
void cmpVFregs(microVFreg& VFreg1, microVFreg& VFreg2, bool& xVar)
{
	if (VFreg1.reg == VFreg2.reg)
	{
		if ((VFreg1.x && VFreg2.x) || (VFreg1.y && VFreg2.y)
		 || (VFreg1.z && VFreg2.z) || (VFreg1.w && VFreg2.w))
		{
			xVar = 1;
		}
	}
}

void mVUsetCycles(mV)
{
	mVUincCycles(mVU, mVUstall);
	// If upper Op && lower Op write to same VF reg:
	if ((mVUregsTemp.VFreg[0] == mVUregsTemp.VFreg[1]) && mVUregsTemp.VFreg[0])
	{
		if (mVUregsTemp.r || mVUregsTemp.VI)
			mVUlow.noWriteVF = true;
		else
			mVUlow.isNOP = true; // If lower Op doesn't modify anything else, then make it a NOP
	}
	// If lower op reads a VF reg that upper Op writes to:
	if ((mVUlow.VF_read[0].reg || mVUlow.VF_read[1].reg) && mVUup.VF_write.reg)
	{
		cmpVFregs(mVUup.VF_write, mVUlow.VF_read[0], mVUinfo.swapOps);
		cmpVFregs(mVUup.VF_write, mVUlow.VF_read[1], mVUinfo.swapOps);
	}
	// If above case is true, and upper op reads a VF reg that lower Op Writes to:
	if (mVUinfo.swapOps && ((mVUup.VF_read[0].reg || mVUup.VF_read[1].reg) && mVUlow.VF_write.reg))
	{
		cmpVFregs(mVUlow.VF_write, mVUup.VF_read[0], mVUinfo.backupVF);
		cmpVFregs(mVUlow.VF_write, mVUup.VF_read[1], mVUinfo.backupVF);
	}

	mVUregs.VF[mVUregsTemp.VFreg[0]].x = tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].x, mVUregsTemp.VF[0].x);
	mVUregs.VF[mVUregsTemp.VFreg[0]].y = tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].y, mVUregsTemp.VF[0].y);
	mVUregs.VF[mVUregsTemp.VFreg[0]].z = tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].z, mVUregsTemp.VF[0].z);
	mVUregs.VF[mVUregsTemp.VFreg[0]].w = tCycles(mVUregs.VF[mVUregsTemp.VFreg[0]].w, mVUregsTemp.VF[0].w);

	mVUregs.VF[mVUregsTemp.VFreg[1]].x = tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].x, mVUregsTemp.VF[1].x);
	mVUregs.VF[mVUregsTemp.VFreg[1]].y = tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].y, mVUregsTemp.VF[1].y);
	mVUregs.VF[mVUregsTemp.VFreg[1]].z = tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].z, mVUregsTemp.VF[1].z);
	mVUregs.VF[mVUregsTemp.VFreg[1]].w = tCycles(mVUregs.VF[mVUregsTemp.VFreg[1]].w, mVUregsTemp.VF[1].w);

	mVUregs.VI[mVUregsTemp.VIreg] = tCycles(mVUregs.VI[mVUregsTemp.VIreg], mVUregsTemp.VI);
	mVUregs.q                     = tCycles(mVUregs.q,                     mVUregsTemp.q);
	mVUregs.p                     = tCycles(mVUregs.p,                     mVUregsTemp.p);
	mVUregs.r                     = tCycles(mVUregs.r,                     mVUregsTemp.r);
	mVUregs.xgkick                = tCycles(mVUregs.xgkick,                mVUregsTemp.xgkick);
}

// Prints Start/End PC of blocks executed, for debugging...
void mVUdebugPrintBlocks(microVU& mVU, bool isEndPC)
{
	if (mVUdebugNow)
	{
		mVUbackupRegs(mVU, true);
		if (isEndPC) xFastCall(mVUprintPC2, xPC);
		else         xFastCall(mVUprintPC1, xPC);
		mVUrestoreRegs(mVU, true);
	}
}

// Test cycles to see if we need to exit-early...
void mVUtestCycles(microVU& mVU, microFlagCycles& mFC)
{
	iPC = mVUstartPC;

	// If the VUSyncHack is on, we want the VU to run behind, to avoid conditions where the VU is sped up.
	if (isVU0 && EmuConfig.Speedhacks.EECycleRate != 0 && (!EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Speedhacks.EECycleRate < 0))
	{
		switch (std::min(static_cast<int>(EmuConfig.Speedhacks.EECycleRate), static_cast<int>(mVUcycles)))
		{
			case -3: // 50%
				mVUcycles *= 2.0f;
				break;
			case -2: // 60%
				mVUcycles *= 1.6666667f;
				break;
			case -1: // 75%
				mVUcycles *= 1.3333333f;
				break;
			case 1: // 130%
				mVUcycles /= 1.3f;
				break;
			case 2: // 180%
				mVUcycles /= 1.8f;
				break;
			case 3: // 300%
				mVUcycles /= 3.0f;
				break;
			default:
				break;
		}
	}
	xMOV(eax, ptr32[&mVU.cycles]);
	if (EmuConfig.Gamefixes.VUSyncHack)
		xSUB(eax, mVUcycles); // Running behind, make sure we have time to run the block
	else
		xSUB(eax, 1); // Running ahead, make sure cycles left are above 0

	xForwardJNS32 skip;

	xLoadFarAddr(rax, &mVUpBlock->pState);
	xCALL((void*)mVU.copyPLState);

	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		xMOV(ptr32[&mVU.regs().nextBlockCycles], mVUcycles);
	mVUendProgram(mVU, &mFC, 0);

	skip.SetTarget();

	xSUB(ptr32[&mVU.cycles], mVUcycles);
}

//------------------------------------------------------------------
// Initializing
//------------------------------------------------------------------

// This gets run at the start of every loop of mVU's first pass
__fi void startLoop(mV)
{
	if (curI & _Mbit_ && isVU0)
		DevCon.WriteLn(Color_Green, "microVU%d: M-bit set! PC = %x", getIndex, xPC);
	if (curI & _Dbit_)
		DevCon.WriteLn(Color_Green, "microVU%d: D-bit set! PC = %x", getIndex, xPC);
	if (curI & _Tbit_)
		DevCon.WriteLn(Color_Green, "microVU%d: T-bit set! PC = %x", getIndex, xPC);
	std::memset(&mVUinfo, 0, sizeof(mVUinfo));
	std::memset(&mVUregsTemp, 0, sizeof(mVUregsTemp));
}

// Initialize VI Constants (vi15 propagates through blocks)
__fi void mVUinitConstValues(microVU& mVU)
{
	for (int i = 0; i < 16; i++)
	{
		mVUconstReg[i].isValid  = 0;
		mVUconstReg[i].regValue = 0;
	}
	mVUconstReg[15].isValid = mVUregs.vi15v;
	mVUconstReg[15].regValue = mVUregs.vi15v ? mVUregs.vi15 : 0;
}

// Initialize Variables
__fi void mVUinitFirstPass(microVU& mVU, uptr pState, u8* thisPtr)
{
	mVUstartPC = iPC; // Block Start PC
	mVUbranch  = 0;   // Branch Type
	mVUcount   = 0;   // Number of instructions ran
	mVUcycles  = 0;   // Skips "M" phase, and starts counting cycles at "T" stage
	mVU.p      = 0;   // All blocks start at p index #0
	mVU.q      = 0;   // All blocks start at q index #0
	if ((uptr)&mVUregs != pState) // Loads up Pipeline State Info
	{
		memcpy((u8*)&mVUregs, (u8*)pState, sizeof(microRegInfo));
	}
	if (((uptr)&mVU.prog.lpState != pState))
	{
		memcpy((u8*)&mVU.prog.lpState, (u8*)pState, sizeof(microRegInfo));
	}
	mVUblock.x86ptrStart = thisPtr;
	mVUpBlock = mVUblocks[mVUstartPC / 2]->add(mVU, &mVUblock); // Add this block to block manager
	mVUregs.needExactMatch = (mVUpBlock->pState.blockType) ? 7 : 0; // ToDo: Fix 1-Op block flag linking (MGS2:Demo/Sly Cooper)
	mVUregs.blockType = 0;
	mVUregs.viBackUp  = 0;
	mVUregs.flagInfo  = 0;
	mVUsFlagHack = CHECK_VU_FLAGHACK;
	mVUinitConstValues(mVU);
}

//------------------------------------------------------------------
// Recompiler
//------------------------------------------------------------------

void mVUDoDBit(microVU& mVU, microFlagCycles* mFC)
{
	if (mVU.index && THREAD_VU1)
		xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x400 : 0x4));
	else
		xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x400 : 0x4));
	xForwardJump32 eJMP(Jcc_Zero);
	if (!isVU1 || !THREAD_VU1)
	{
		xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x200 : 0x2));
		xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
	}
	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);
	eJMP.SetTarget();
}

void mVUDoTBit(microVU& mVU, microFlagCycles* mFC)
{
	if (mVU.index && THREAD_VU1)
		xTEST(ptr32[&vu1Thread.vuFBRST], (isVU1 ? 0x800 : 0x8));
	else
		xTEST(ptr32[&VU0.VI[REG_FBRST].UL], (isVU1 ? 0x800 : 0x8));
	xForwardJump32 eJMP(Jcc_Zero);
	if (!isVU1 || !THREAD_VU1)
	{
		xOR(ptr32[&VU0.VI[REG_VPU_STAT].UL], (isVU1 ? 0x400 : 0x4));
		xOR(ptr32[&mVU.regs().flags], VUFLAG_INTCINTERRUPT);
	}
	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);

	eJMP.SetTarget();
}

void mVUSaveFlags(microVU& mVU, microFlagCycles& mFC, microFlagCycles& mFCBackup)
{
	memcpy(&mFCBackup, &mFC, sizeof(microFlagCycles));
	mVUsetFlags(mVU, mFCBackup); // Sets Up Flag instances
}

static void mvuPreloadRegisters(microVU& mVU, u32 endCount)
{
	static constexpr const int REQUIRED_FREE_XMMS = 3; // some space for temps
	static constexpr const int REQUIRED_FREE_GPRS = 1; // some space for temps

	u32 vfs_loaded = 0;
	u32 vis_loaded = 0;

	for (int reg = 0; reg < mVU.regAlloc->getXmmCount(); reg++)
	{
		const int vf = mVU.regAlloc->getRegVF(reg);
		if (vf >= 0)
			vfs_loaded |= (1u << vf);
	}

	for (int reg = 0; reg < mVU.regAlloc->getGPRCount(); reg++)
	{
		const int vi = mVU.regAlloc->getRegVI(reg);
		if (vi >= 0)
			vis_loaded |= (1u << vi);
	}

	const u32 orig_pc = iPC;
	const u32 orig_code = mVU.code;
	int free_regs = mVU.regAlloc->getFreeXmmCount();
	int free_gprs = mVU.regAlloc->getFreeGPRCount();

	auto preloadVF = [&mVU, &vfs_loaded, &free_regs](u8 reg)
	{
		if (free_regs <= REQUIRED_FREE_XMMS || reg == 0 || (vfs_loaded & (1u << reg)) != 0)
			return;

		mVU.regAlloc->clearNeeded(mVU.regAlloc->allocReg(reg));
		vfs_loaded |= (1u << reg);
		free_regs--;
	};

	auto preloadVI = [&mVU, &vis_loaded, &free_gprs](u8 reg)
	{
		if (free_gprs <= REQUIRED_FREE_GPRS || reg == 0 || (vis_loaded & (1u << reg)) != 0)
			return;

		mVU.regAlloc->clearNeeded(mVU.regAlloc->allocGPR(reg));
		vis_loaded |= (1u << reg);
		free_gprs--;
	};

	auto canPreload = [&free_regs, &free_gprs]() {
		return (free_regs >= REQUIRED_FREE_XMMS || free_gprs >= REQUIRED_FREE_GPRS);
	};

	for (u32 x = 0; x < endCount && canPreload(); x++)
	{
		incPC(1);

		const microOp* info = &mVUinfo;
		if (info->doXGKICK)
			break;

		for (u32 i = 0; i < 2; i++)
		{
			preloadVF(info->uOp.VF_read[i].reg);
			preloadVF(info->lOp.VF_read[i].reg);
			if (info->lOp.VI_read[i].used)
				preloadVI(info->lOp.VI_read[i].reg);
		}

		const microVFreg& uvfr = info->uOp.VF_write;
		if (uvfr.reg != 0 && (!uvfr.x || !uvfr.y || !uvfr.z || !uvfr.w))
		{
			// not writing entire vector
			preloadVF(uvfr.reg);
		}

		const microVFreg& lvfr = info->lOp.VF_write;
		if (lvfr.reg != 0 && (!lvfr.x || !lvfr.y || !lvfr.z || !lvfr.w))
		{
			// not writing entire vector
			preloadVF(lvfr.reg);
		}

		if (info->lOp.branch)
			break;
	}

	iPC = orig_pc;
	mVU.code = orig_code;
}

void* mVUcompile(microVU& mVU, u32 startPC, uptr pState)
{
	microFlagCycles mFC;
	u8* thisPtr = x86Ptr;
	const u32 endCount = (((microRegInfo*)pState)->blockType) ? 1 : (mVU.microMemSize / 8);

	// First Pass
	iPC = startPC / 4;
	mVUsetupRange(mVU, startPC, 1); // Setup Program Bounds/Range
	mVU.regAlloc->reset(false);          // Reset regAlloc
	mVUinitFirstPass(mVU, pState, thisPtr);
	mVUbranch = 0;
	for (int branch = 0; mVUcount < endCount;)
	{
		incPC(1);
		startLoop(mVU);
		mVUincCycles(mVU, 1);
		mVUopU(mVU, 0);
		mVUcheckBadOp(mVU);
		if (curI & _Ebit_)
		{
			eBitPass1(mVU, branch);
			// VU0 end of program MAC results can be read by COP2, so best to make sure the last instance is valid
			// Needed for State of Emergency 2 and Driving Emotion Type-S
			if (isVU0)
				mVUregs.needExactMatch |= 7;
		}

		if ((curI & _Mbit_) && isVU0)
		{
			if (xPC > 0)
			{
				incPC(-2);
				if (!(curI & _Mbit_)) //If the last instruction was also M-Bit we don't need to sync again
				{
					incPC(2);
					mVUup.mBit = true;
				}
				else
					incPC(2);
			}
			else
				mVUup.mBit = true;
		}

		if (curI & _Ibit_)
		{
			mVUlow.isNOP = true;
			mVUup.iBit = true;
			if (EmuConfig.Gamefixes.IbitHack)
			{
				mVUsetupRange(mVU, xPC, false);
				if (branch < 2)
					mVUsetupRange(mVU, xPC + 4, true);
			}
		}
		else
		{
			incPC(-1);
			if (EmuConfig.Gamefixes.IbitHack)
			{
				// Ignore IADDI, IADDIU and ISUBU, ILW, ISW, LQ, SQ.
				// Be warned, this is a little risky as we could be ignoring subtle differences in the operations.
				// 2 is too much, 1 is too little, so it gets 2. It's a hack anyways...
				const u32 upper = (mVU.code >> 25);
				if (upper == 0x1 || upper == 0x0 || upper == 0x4 || upper == 0x5 || upper == 0x8 || upper == 0x9 || (upper == 0x40 && (mVU.code & 0x3F) == 0x32))
				{
					incPC(1);
					mVUsetupRange(mVU, xPC, false);
					if (branch < 2)
						mVUsetupRange(mVU, xPC + 2, true);
					incPC(-1);
				}
			}
			mVUopL(mVU, 0);
			incPC(1);
		}
		if (curI & _Dbit_)
		{
			mVUup.dBit = true;
		}
		if (curI & _Tbit_)
		{
			mVUup.tBit = true;
		}
		mVUsetCycles(mVU);
		// Update XGKick information
		if (!mVUlow.isKick)
		{
			mVUregs.xgkickcycles += 1 + mVUstall;
			if (mVUlow.isMemWrite)
			{
				mVUlow.kickcycles = mVUregs.xgkickcycles;
				mVUregs.xgkickcycles = 0;
			}
		}
		else
		{
			// XGKick command counts as one cycle for the transfer.
			// Can be tested with Resident Evil: Outbreak, Kingdom Hearts, CART Fury.
			mVUregs.xgkickcycles = 1;
			mVUlow.kickcycles = 0;
		}

		mVUinfo.readQ = mVU.q;
		mVUinfo.writeQ = !mVU.q;
		mVUinfo.readP = mVU.p && isVU1;
		mVUinfo.writeP = !mVU.p && isVU1;
		mVUcount++;

		if (branch >= 2)
		{
			mVUinfo.isEOB = true;

			if (branch == 3)
			{
				mVUinfo.isBdelay = true;
			}

			branchWarning(mVU);
			if (mVUregs.xgkickcycles)
			{
				mVUlow.kickcycles = mVUregs.xgkickcycles;
				mVUregs.xgkickcycles = 0;
			}
			break;
		}
		else if (branch == 1)
		{
			branch = 2;
		}

		if (mVUbranch)
		{
			mVUsetFlagInfo(mVU);
			eBitWarning(mVU);
			branch = 3;
			mVUbranch = 0;
		}

		if (mVUup.mBit && !branch && !mVUup.eBit)
		{
			mVUregs.needExactMatch |= 7;
			if (mVUregs.xgkickcycles)
			{
				mVUlow.kickcycles = mVUregs.xgkickcycles;
				mVUregs.xgkickcycles = 0;
			}
			break;
		}

		if (mVUinfo.isEOB)
		{
			if (mVUregs.xgkickcycles)
			{
				mVUlow.kickcycles = mVUregs.xgkickcycles;
				mVUregs.xgkickcycles = 0;
			}
			break;
		}

		incPC(1);
	}

	// Fix up vi15 const info for propagation through blocks
	mVUregs.vi15 = (doConstProp && mVUconstReg[15].isValid) ? (u16)mVUconstReg[15].regValue : 0;
	mVUregs.vi15v = (doConstProp && mVUconstReg[15].isValid) ? 1 : 0;
	mVUsetFlags(mVU, mFC);           // Sets Up Flag instances
	mVUoptimizePipeState(mVU);       // Optimize the End Pipeline State for nicer Block Linking
	mVUdebugPrintBlocks(mVU, false); // Prints Start/End PC of blocks executed, for debugging...
	mVUtestCycles(mVU, mFC);         // Update VU Cycles and Exit Early if Necessary

	// Second Pass
	iPC = mVUstartPC;
	setCode();
	mVUbranch = 0;
	u32 x = 0;

	mvuPreloadRegisters(mVU, endCount);

	for (; x < endCount; x++)
	{
#if 0
		if (mVU.index == 1 && (x == 0 || true))
		{
			mVU.regAlloc->flushAll(false);
			mVUbackupRegs(mVU, true);
			xFastCall(DumpVUState, mVU.index, (xPC) | ((x == 0) ? 0x80000000 : 0));
			mVUrestoreRegs(mVU, true);
			//if (xPC == 0x1358) __debugbreak();
		}
#endif

		if (mVUinfo.isEOB)
		{
			handleBadOp(mVU, x);
			x = 0xffff;
		} // handleBadOp currently just prints a warning
		if (mVUup.mBit)
		{
			xOR(ptr32[&mVU.regs().flags], VUFLAG_MFLAGSET);
		}

		if (isVU1 && mVUlow.kickcycles && CHECK_XGKICKHACK)
		{
			mVU_XGKICK_SYNC(mVU, false);
		}

		mVUexecuteInstruction(mVU);
		if (!mVUinfo.isBdelay && !mVUlow.branch) //T/D Bit on branch is handled after the branch, branch delay slots are executed.
		{
			if (mVUup.tBit)
			{
				mVUDoTBit(mVU, &mFC);
			}
			else if (mVUup.dBit && doDBitHandling)
			{
				mVUDoDBit(mVU, &mFC);
			}
			else if (mVUup.mBit && !mVUup.eBit && !mVUinfo.isEOB)
			{
				// Need to make sure the flags are exact, Gungrave does FCAND with Mbit, then directly after FMAND with M-bit
				// Also call setupBranch to sort flag instances

				mVUsetupBranch(mVU, mFC);
				// Make sure we save the current state so it can come back to it
				u32* cpS = (u32*)&mVUregs;
				u32* lpS = (u32*)&mVU.prog.lpState;
				for (size_t i = 0; i < (sizeof(microRegInfo) - 4) / 4; i++, lpS++, cpS++)
				{
					xMOV(ptr32[lpS], cpS[0]);
				}
				incPC(2);
				mVUsetupRange(mVU, xPC, false);
				if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
					xMOV(ptr32[&mVU.regs().nextBlockCycles], 0);
				mVUendProgram(mVU, &mFC, 0);
				normBranchCompile(mVU, xPC);
				incPC(-2);
				goto perf_and_return;
			}
		}

		if (mVUinfo.doXGKICK)
		{
			mVU_XGKICK_DELAY(mVU);
		}

		if (isEvilBlock)
		{
			mVUsetupRange(mVU, xPC + 8, false);
			normJumpCompile(mVU, mFC, true);
			goto perf_and_return;
		}
		else if (!mVUinfo.isBdelay)
		{
			// Handle range wrapping
			if ((xPC + 8) == mVU.microMemSize)
			{
				mVUsetupRange(mVU, xPC + 8, false);
				mVUsetupRange(mVU, 0, 1);
			}
			incPC(1);
		}
		else
		{
			incPC(1);
			mVUsetupRange(mVU, xPC, false);
			mVUdebugPrintBlocks(mVU, true);
			incPC(-4); // Go back to branch opcode

			switch (mVUlow.branch)
			{
				case 1: // B/BAL
				case 2:
					normBranch(mVU, mFC);
					goto perf_and_return;
				case 9: // JR/JALR
				case 10:
					normJump(mVU, mFC);
					goto perf_and_return;
				case 3: // IBEQ
					condBranch(mVU, mFC, Jcc_Equal);
					goto perf_and_return;
				case 4: // IBGEZ
					condBranch(mVU, mFC, Jcc_GreaterOrEqual);
					goto perf_and_return;
				case 5: // IBGTZ
					condBranch(mVU, mFC, Jcc_Greater);
					goto perf_and_return;
				case 6: // IBLEQ
					condBranch(mVU, mFC, Jcc_LessOrEqual);
					goto perf_and_return;
				case 7: // IBLTZ
					condBranch(mVU, mFC, Jcc_Less);
					goto perf_and_return;
				case 8: // IBNEQ
					condBranch(mVU, mFC, Jcc_NotEqual);
					goto perf_and_return;
			}
		}
	}
	if ((x == endCount) && (x != 1))
	{
		Console.Error("microVU%d: Possible infinite compiling loop!", mVU.index);
	}

	// E-bit End
	mVUsetupRange(mVU, xPC, false);
	mVUendProgram(mVU, &mFC, 1);

perf_and_return:

	if (mVU.regs().start_pc == startPC)
	{
		if (mVU.index)
			Perf::vu1.RegisterPC(thisPtr, static_cast<u32>(x86Ptr - thisPtr), startPC);
		else
			Perf::vu0.RegisterPC(thisPtr, static_cast<u32>(x86Ptr - thisPtr), startPC);
	}

	return thisPtr;
}

// Returns the entry point of the block (compiles it if not found)
__fi void* mVUentryGet(microVU& mVU, microBlockManager* block, u32 startPC, uptr pState)
{
	microBlock* pBlock = block->search(mVU, (microRegInfo*)pState);
	if (pBlock)
		return pBlock->x86ptrStart;
	else
		return mVUcompile(mVU, startPC, pState);
}

// Search for Existing Compiled Block (if found, return x86ptr; else, compile and return x86ptr)
__fi void* mVUblockFetch(microVU& mVU, u32 startPC, uptr pState)
{

	pxAssert((startPC & 7) == 0);
	pxAssert(startPC <= mVU.microMemSize - 8);
	startPC &= mVU.microMemSize - 8;

	blockCreate(startPC / 8);
	return mVUentryGet(mVU, mVUblocks[startPC / 8], startPC, pState);
}

// mVUcompileJIT() - Called By JR/JALR during execution
_mVUt void* mVUcompileJIT(u32 startPC, uptr ptr)
{
	if (doJumpAsSameProgram) // Treat jump as part of same microProgram
	{
		if (doJumpCaching) // When doJumpCaching, ptr is a microBlock pointer
		{
			microVU& mVU = mVUx;
			microBlock* pBlock = (microBlock*)ptr;
			microJumpCache& jc = pBlock->jumpCache[startPC / 8];
			if (jc.prog && jc.prog == mVU.prog.quick[startPC / 8].prog)
				return jc.x86ptrStart;
			void* v = mVUblockFetch(mVUx, startPC, (uptr)&pBlock->pStateEnd);
			jc.prog = mVU.prog.quick[startPC / 8].prog;
			jc.x86ptrStart = v;
			return v;
		}
		return mVUblockFetch(mVUx, startPC, ptr);
	}
	mVUx.regs().start_pc = startPC;
	if (doJumpCaching) // When doJumpCaching, ptr is a microBlock pointer
	{
		microVU& mVU = mVUx;
		microBlock* pBlock = (microBlock*)ptr;
		microJumpCache& jc = pBlock->jumpCache[startPC / 8];
		if (jc.prog && jc.prog == mVU.prog.quick[startPC / 8].prog)
			return jc.x86ptrStart;
		void* v = mVUsearchProg<vuIndex>(startPC, (uptr)&pBlock->pStateEnd);
		jc.prog = mVU.prog.quick[startPC / 8].prog;
		jc.x86ptrStart = v;
		return v;
	}
	else // When !doJumpCaching, pBlock param is really a microRegInfo pointer
	{
		return mVUsearchProg<vuIndex>(startPC, ptr); // Find and set correct program
	}
}
