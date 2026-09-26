// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

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

void mVU0clearlpStateJIT() { if (!microVU0.prog.cleared) std::memset(&microVU0.prog.lpState, 0, sizeof(microVU1.prog.lpState)); }
void mVU1clearlpStateJIT() { if (!microVU1.prog.cleared) std::memset(&microVU1.prog.lpState, 0, sizeof(microVU1.prog.lpState)); }

//------------------------------------------------------------------
// Helpers
//------------------------------------------------------------------

// Copies a pipeline state into mVU.prog.lpState (replaces the x86 copyPLState routine).
static void mVUcopyPipelineState(mV, const microRegInfo* src)
{
	static_assert(sizeof(microRegInfo) == 96);
	armMoveAddressToReg(a64::x8, src);
	armAsm->Add(RSCRATCHADDR, RMVU, static_cast<s64>(offsetof(microVU, prog) + offsetof(microProgManager, lpState)));
	for (u32 i = 0; i < 3; i++)
	{
		armAsm->Ldp(mVUQScratch1, mVUQScratch2, a64::MemOperand(a64::x8, i * 32));
		armAsm->Stp(mVUQScratch1, mVUQScratch2, a64::MemOperand(RSCRATCHADDR, i * 32));
	}
}

// Branches to 'clear' if the D/T bit enable in FBRST isn't set.
static void mVUtestFBRST(mV, u32 vu1bit, u32 vu0bit, a64::Label* clear)
{
	if (mVU.index && THREAD_VU1)
		armAsm->Ldr(a64::w8, mVUmem(&vu1Thread.vuFBRST));
	else
		armAsm->Ldr(a64::w8, mVUmem(&VU0.VI[REG_FBRST].UL));
	armAsm->Tst(a64::w8, isVU1 ? vu1bit : vu0bit);
	armAsm->B(clear, a64::eq);
}

// Sets the D/T bit status and requests an interrupt (not on the MTVU thread).
static void mVUsetDTStatus(mV, u32 vu1bit, u32 vu0bit)
{
	if (!mVU.index || !THREAD_VU1)
	{
		mVUrmw32(mVUmem(&VU0.VI[REG_VPU_STAT].UL), [&](const a64::Register& r) { armAsm->Orr(r, r, isVU1 ? vu1bit : vu0bit); });
		mVUrmw32(mVUmem(&mVU.regs().flags), [](const a64::Register& r) { armAsm->Orr(r, r, VUFLAG_INTCINTERRUPT); });
	}
}

static __fi void mVUstoreTPC(mV, u32 pc)
{
	mVUstoreImm32(mVUmem(&mVU.regs().VI[REG_TPC].UL), pc);
}

static __fi void mVUjumpToExit(mV, bool tbit)
{
	if (mVU.index && THREAD_VU1)
		armEmitCall(reinterpret_cast<const void*>(tbit ? mVUTBit : mVUEBit));
	armEmitJmp(mVU.exitFunct);
}

// Compares the 16-bit branch condition value against zero.
static __fi void mVUcmpBranch(mV)
{
	armAsm->Ldrsh(a64::w8, mVUmem(&mVU.branch));
	armAsm->Cmp(a64::w8, 0);
}

// Save P/Q Regs
static void mVUsavePQ(mV, int qInst, int pInst)
{
	if (qInst)
		mVUshufD(xmmPQ, xmmPQ, 0xe1);
	armAsm->Str(xmmPQ.S(), mVUmem(&mVU.regs().VI[REG_Q].UL));
	mVUshufD(xmmPQ, xmmPQ, 0xe1);
	armAsm->Str(xmmPQ.S(), mVUmem(&mVU.regs().pending_q));
	mVUshufD(xmmPQ, xmmPQ, 0xe1);

	if (isVU1)
	{
		if (pInst)
			mVUshufD(xmmPQ, xmmPQ, 0xb4); // Swap Pending/Active P
		mVUshufD(xmmPQ, xmmPQ, 0xC6); // 3 0 1 2
		armAsm->Str(xmmPQ.S(), mVUmem(&mVU.regs().VI[REG_P].UL));
		mVUshufD(xmmPQ, xmmPQ, 0x87); // 0 2 1 3
		armAsm->Str(xmmPQ.S(), mVUmem(&mVU.regs().pending_p));
		mVUshufD(xmmPQ, xmmPQ, 0x27); // 3 2 1 0
	}
}

// Save MAC, Status and CLIP Flag Instances
static void mVUsaveFlags(mV, int fStatus, int fMac, int fClip, bool backup)
{
	mVUallocSFLAGc(gprT1, gprT2, fStatus);
	armAsm->Str(gprT1.W(), mVUmem(&mVU.regs().VI[REG_STATUS_FLAG].UL));
	mVUallocMFLAGa(mVU, gprT1, fMac);
	mVUallocCFLAGa(mVU, gprT2, fClip);
	armAsm->Str(gprT1.W(), mVUmem(&mVU.regs().VI[REG_MAC_FLAG].UL));
	armAsm->Str(gprT2.W(), mVUmem(&mVU.regs().VI[REG_CLIP_FLAG].UL));

	if (backup) // Backup flag instances
	{
		armAsm->Ldr(xmmT1.Q(), mVUmem(mVU.macFlag));
		armAsm->Str(xmmT1.Q(), mVUmem(&mVU.regs().micro_macflags));
		armAsm->Ldr(xmmT1.Q(), mVUmem(mVU.clipFlag));
		armAsm->Str(xmmT1.Q(), mVUmem(&mVU.regs().micro_clipflags));

		armAsm->Str(gprF0.W(), mVUmem(&mVU.regs().micro_statusflags[0]));
		armAsm->Str(gprF1.W(), mVUmem(&mVU.regs().micro_statusflags[1]));
		armAsm->Str(gprF2.W(), mVUmem(&mVU.regs().micro_statusflags[2]));
		armAsm->Str(gprF3.W(), mVUmem(&mVU.regs().micro_statusflags[3]));
	}
	else // Flush flag instances
	{
		armAsm->Dup(xmmT1.V4S(), gprT2.W());
		armAsm->Str(xmmT1.Q(), mVUmem(&mVU.regs().micro_clipflags));

		armAsm->Dup(xmmT1.V4S(), gprT1.W());
		armAsm->Str(xmmT1.Q(), mVUmem(&mVU.regs().micro_macflags));

		armAsm->Dup(xmmT1.V4S(), getFlagReg(fStatus).W());
		armAsm->Str(xmmT1.Q(), mVUmem(&mVU.regs().micro_statusflags));
	}
}

// Emits a placeholder B, returns its address so it can be patched later.
static u32* mVUemitPatchableBranch()
{
	a64::SingleEmissionCheckScope guard(armAsm);
	u32* ptr = reinterpret_cast<u32*>(armGetCurrentCodePointer());
	armAsm->b(static_cast<int64_t>(0));
	return ptr;
}

static void mVUpatchBranch(u32* branch, const void* target)
{
	const s64 disp = GetPCDisplacement(branch, target);
	pxAssertRel(vixl::IsInt26(disp), "microVU branch out of range");
	*branch = 0x14000000u | (static_cast<u32>(disp) & 0x03FFFFFFu);
}

//------------------------------------------------------------------
// Program endings
//------------------------------------------------------------------

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
		armEmitCall(reinterpret_cast<const void*>(isVU1 ? mVU1clearlpStateJIT : mVU0clearlpStateJIT));
	}

	mVUsavePQ(mVU, qInst, pInst);
	mVUsaveFlags(mVU, fStatus, fMac, fClip, !isEbit);

	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		mVUstoreImm32(mVUmem(&mVU.regs().nextBlockCycles), 0);

	mVUstoreTPC(mVU, xPC);

	if (isEbit) // Clear 'is busy' Flags
	{
		if (!mVU.index || !THREAD_VU1)
		{
			mVUrmw32(mVUmem(&VU0.VI[REG_VPU_STAT].UL), [&](const a64::Register& r) { armAsm->And(r, r, isVU1 ? ~0x100u : ~0x001u); }); // VBS0/VBS1 flag
		}
	}

	if (isEbit != 2) // Save PC, and Jump to Exit Point
		mVUjumpToExit(mVU, true);

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
		std::memset(&mVUinfo, 0, sizeof(mVUinfo));
		std::memset(&mVUregsTemp, 0, sizeof(mVUregsTemp));
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
		armEmitCall(reinterpret_cast<const void*>(isVU1 ? mVU1clearlpStateJIT : mVU0clearlpStateJIT));
	}

	mVUsavePQ(mVU, qInst, pInst);
	mVUsaveFlags(mVU, fStatus, fMac, fClip, !isEbit || isEbit == 3);

	mVUstoreTPC(mVU, xPC);

	if ((isEbit && isEbit != 3)) // Clear 'is busy' Flags
	{
		if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
			mVUstoreImm32(mVUmem(&mVU.regs().nextBlockCycles), 0);
		if (!mVU.index || !THREAD_VU1)
		{
			mVUrmw32(mVUmem(&VU0.VI[REG_VPU_STAT].UL), [&](const a64::Register& r) { armAsm->And(r, r, isVU1 ? ~0x100u : ~0x001u); }); // VBS0/VBS1 flag
		}
	}
	else if (isEbit)
	{
		if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
			mVUstoreImm32(mVUmem(&mVU.regs().nextBlockCycles), 0);
	}

	if (isEbit != 2 && isEbit != 3) // Save PC, and Jump to Exit Point
		mVUjumpToExit(mVU, false);
	memcpy(&mVUregs, &stateBackup, sizeof(mVUregs)); //Restore the state for the rest of the recompile
}

// Recompiles Code for Proper Flags and Q/P regs on Block Linkings
void mVUsetupBranch(mV, microFlagCycles& mFC)
{
	mVU.regAlloc->flushAll(); // Flush Allocated Regs
	mVUsetupFlags(mVU, mFC);  // Shuffle Flag Instances

	// Shuffle P/Q regs since every block starts at instance #0
	if (mVU.p || mVU.q)
		mVUshufD(xmmPQ, xmmPQ, shufflePQ);
	mVU.p = 0, mVU.q = 0;
}

void normBranchCompile(microVU& mVU, u32 branchPC)
{
	microBlock* pBlock;
	blockCreate(branchPC / 8);
	pBlock = mVUblocks[branchPC / 8]->search(mVU, (microRegInfo*)&mVUregs);
	if (pBlock)
		armEmitJmp(pBlock->x86ptrStart);
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

	// The jump target is kept in w27 (callee-saved, nothing is allocated after the flush above),
	// because mVUendProgram() below uses the argument registers.
	if (isEvilJump)
	{
		armAsm->Ldr(a64::w27, mVUmem(&mVU.evilBranch));
		armAsm->Ldr(a64::w8, mVUmem(&mVU.evilevilBranch));
		armAsm->Str(a64::w8, mVUmem(&mVU.evilBranch));
	}
	else
		armAsm->Ldr(a64::w27, mVUmem(&mVU.branch));

	if (mVUup.eBit && isEvilJump) // E-bit EvilJump
	{
		//Xtreme G 3 does 2 conditional jumps, the first contains an E Bit on the first instruction
		//So if it is taken, you need to end the program, else you get infinite loops.
		mVUendProgram(mVU, &mFC, 2);
		armAsm->Str(a64::w27, mVUmem(&mVU.regs().VI[REG_TPC].UL));
		mVUjumpToExit(mVU, false);
	}

	armAsm->Mov(a64::w0, a64::w27);
	if (doJumpCaching)
		armMoveAddressToReg(a64::x1, mVUpBlock);
	else
		armMoveAddressToReg(a64::x1, &mVUpBlock->pStateEnd);

	if (!mVU.index)
		armEmitCall(reinterpret_cast<const void*>(mVUcompileJIT<0>)); //(u32 startPC, uptr pState)
	else
		armEmitCall(reinterpret_cast<const void*>(mVUcompileJIT<1>));

	mVUrestoreRegs(mVU);
	armAsm->Br(a64::x0); // Jump to rec-code address
}

void normBranch(mV, microFlagCycles& mFC)
{
	// E-bit or T-Bit or D-Bit Branch
	if (mVUup.dBit && doDBitHandling)
	{
		// Flush register cache early to avoid double flush on both paths
		mVU.regAlloc->flushAll(false);

		u32 tempPC = iPC;
		a64::Label eJMP;
		mVUtestFBRST(mVU, 0x400, 0x4, &eJMP);
		mVUsetDTStatus(mVU, 0x200, 0x2);
		iPC = branchAddr(mVU) / 4;
		mVUDTendProgram(mVU, &mFC, 1);
		armAsm->Bind(&eJMP);
		iPC = tempPC;
	}
	if (mVUup.tBit)
	{
		// Flush register cache early to avoid double flush on both paths
		mVU.regAlloc->flushAll(false);

		u32 tempPC = iPC;
		a64::Label eJMP;
		mVUtestFBRST(mVU, 0x800, 0x8, &eJMP);
		mVUsetDTStatus(mVU, 0x400, 0x4);
		iPC = branchAddr(mVU) / 4;
		mVUDTendProgram(mVU, &mFC, 1);
		armAsm->Bind(&eJMP);
		iPC = tempPC;
	}
	if (mVUup.mBit)
	{
		DevCon.Warning("M-Bit on normal branch, report if broken");
		u32 tempPC = iPC;

		memcpy(&mVUpBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));
		mVUcopyPipelineState(mVU, &mVUpBlock->pStateEnd);

		mVUsetupBranch(mVU, mFC);
		mVUendProgram(mVU, &mFC, 3);
		iPC = branchAddr(mVU) / 4;
		mVUstoreTPC(mVU, xPC);
		mVUjumpToExit(mVU, false);
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

void condBranch(mV, microFlagCycles& mFC, a64::Condition JMPcc)
{
	mVUsetupBranch(mVU, mFC);

	if (mVUup.tBit)
	{
		DevCon.Warning("T-Bit on branch, please report if broken");
		u32 tempPC = iPC;
		a64::Label eJMP, tJMP;
		mVUtestFBRST(mVU, 0x800, 0x8, &eJMP);
		mVUsetDTStatus(mVU, 0x400, 0x4);
		mVUDTendProgram(mVU, &mFC, 2);
		mVUcmpBranch(mVU);
		armAsm->B(&tJMP, JMPcc);
			incPC(4); // Set PC to First instruction of Non-Taken Side
			mVUstoreTPC(mVU, xPC);
			mVUjumpToExit(mVU, true);
		armAsm->Bind(&tJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		mVUstoreTPC(mVU, xPC);
		mVUjumpToExit(mVU, true);
		armAsm->Bind(&eJMP);
		iPC = tempPC;
	}
	if (mVUup.dBit && doDBitHandling)
	{
		u32 tempPC = iPC;
		a64::Label eJMP, dJMP;
		mVUtestFBRST(mVU, 0x400, 0x4, &eJMP);
		mVUsetDTStatus(mVU, 0x200, 0x2);
		mVUDTendProgram(mVU, &mFC, 2);
		mVUcmpBranch(mVU);
		armAsm->B(&dJMP, JMPcc);
			incPC(4); // Set PC to First instruction of Non-Taken Side
			mVUstoreTPC(mVU, xPC);
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&dJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		mVUstoreTPC(mVU, xPC);
		armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&eJMP);
		iPC = tempPC;
	}
	if (mVUup.mBit)
	{
		u32 tempPC = iPC;

		memcpy(&mVUpBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));
		mVUcopyPipelineState(mVU, &mVUpBlock->pStateEnd);

		mVUendProgram(mVU, &mFC, 3);
		a64::Label dJMP;
		mVUcmpBranch(mVU);
		armAsm->B(&dJMP, JMPcc);
		incPC(4); // Set PC to First instruction of Non-Taken Side
		mVUstoreTPC(mVU, xPC);
		mVUjumpToExit(mVU, false);
		armAsm->Bind(&dJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		mVUstoreTPC(mVU, xPC);
		mVUjumpToExit(mVU, false);
		iPC = tempPC;
	}
	if (mVUup.eBit) // Conditional Branch With E-Bit Set
	{
		if (mVUlow.evilBranch)
			DevCon.Warning("End on evil branch! - Not implemented! - If game broken report to PCSX2 Team");

		mVUendProgram(mVU, &mFC, 2);
		mVUcmpBranch(mVU);

		incPC(3);
		a64::Label eJMP;
		armAsm->B(&eJMP, JMPcc);
			incPC(1); // Set PC to First instruction of Non-Taken Side
			mVUstoreTPC(mVU, xPC);
			mVUjumpToExit(mVU, false);
		armAsm->Bind(&eJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr

		iPC = branchAddr(mVU) / 4;
		mVUstoreTPC(mVU, xPC);
		mVUjumpToExit(mVU, false);
		return;
	}
	else // Normal Conditional Branch
	{
		mVUcmpBranch(mVU);

		incPC(3);
		microBlock* bBlock;
		incPC2(1); // Check if Branch Non-Taken Side has already been recompiled
		blockCreate(iPC / 2);
		bBlock = mVUblocks[iPC / 2]->search(mVU, (microRegInfo*)&mVUregs);
		incPC2(-1);
		if (bBlock) // Branch non-taken has already been compiled
		{
			armEmitCondBranch(a64::InvertCondition(JMPcc), bBlock->x86ptrStart);
			incPC(-3); // Go back to branch opcode (to get branch imm addr)
			normBranchCompile(mVU, branchAddr(mVU));
		}
		else
		{
			// The taken side is compiled after the not-taken side, so jump over a patchable branch.
			a64::Label notTaken;
			armAsm->B(&notTaken, a64::InvertCondition(JMPcc));
			u32* ajmp = mVUemitPatchableBranch();
			armAsm->Bind(&notTaken);
			u32 bPC = iPC; // mVUcompile can modify iPC, mVUpBlock, and mVUregs so back them up

			microRegInfo regBackup;
			memcpy(&regBackup, &mVUregs, sizeof(microRegInfo));

			incPC2(1); // Get PC for branch not-taken
			mVUcompile(mVU, xPC, (uptr)&mVUregs);

			iPC = bPC;
			incPC(-3); // Go back to branch opcode (to get branch imm addr)
			void* jumpAddr = mVUblockFetch(mVU, branchAddr(mVU), (uptr)&regBackup);
			mVUpatchBranch(ajmp, jumpAddr);
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
		// Flush register cache early to avoid double flush on both paths
		mVU.regAlloc->flushAll(false);

		a64::Label eJMP;
		mVUtestFBRST(mVU, 0x400, 0x4, &eJMP);
		mVUsetDTStatus(mVU, 0x200, 0x2);
		mVUDTendProgram(mVU, &mFC, 2);
		armAsm->Ldr(gprT1.W(), mVUmem(&mVU.branch));
		armAsm->Str(gprT1.W(), mVUmem(&mVU.regs().VI[REG_TPC].UL));
		armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&eJMP);
	}
	if (mVUup.tBit)
	{
		// Flush register cache early to avoid double flush on both paths
		mVU.regAlloc->flushAll(false);

		a64::Label eJMP;
		mVUtestFBRST(mVU, 0x800, 0x8, &eJMP);
		mVUsetDTStatus(mVU, 0x400, 0x4);
		mVUDTendProgram(mVU, &mFC, 2);
		armAsm->Ldr(gprT1.W(), mVUmem(&mVU.branch));
		armAsm->Str(gprT1.W(), mVUmem(&mVU.regs().VI[REG_TPC].UL));
		mVUjumpToExit(mVU, true);
		armAsm->Bind(&eJMP);
	}
	if (mVUup.eBit) // E-bit Jump
	{
		mVUendProgram(mVU, &mFC, 2);
		armAsm->Ldr(gprT1.W(), mVUmem(&mVU.branch));
		armAsm->Str(gprT1.W(), mVUmem(&mVU.regs().VI[REG_TPC].UL));
		mVUjumpToExit(mVU, false);
	}
	else
	{
		normJumpCompile(mVU, mFC, false);
	}
}
