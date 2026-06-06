// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU recompiler — branch / program-exit emission (Phase 7, task 7.7).
//
// VIXL port of pcsx2/x86/microVU_Branch.inl. This slice brings up the two
// *program-exit* emitters first, because they make NO opcode-table calls
// (mVUopU/mVUopL) and so compile standalone ahead of the Tables/Compile big-bang:
//
//   * getLastFlagInst          — pure analysis (which flag instance to flush)
//   * mVU{0,1}clearlpStateJIT  — C thunks called from emitted code
//   * mVUendProgram            — end-of-block: save P/Q + the 4 flag instances back
//                                to VURegs, then jump to mVU.exitFunct
//   * mVUDTendProgram          — same, for the D/T-bit early-exit path
//
// The branch *drivers* that DO need the opcode tables + mVUcompile
// (mVUsetupBranch / normBranch{,Compile} / normJump{,Compile} / condBranch) come
// over with the Tables + Compile slice and are NOT in this file yet.
//
// x86 -> VIXL emit translations used here:
//   xMOVSS(ptr32[a], xmmPQ)   -> Str(mVU_xmmPQ.S(), [a])          (store P/Q lane0)
//   xPSHUF.D(xmmPQ,xmmPQ,imm) -> mVUshufflePS(mVU_xmmPQ, …, imm)  (lane permute)
//   xMOVAPS(xmm, ptr128[a])   -> Ldr(xmm.Q(), [a]) / Str          (128-bit move)
//   xMOVDZX(xmm, ptr32[a])    -> Ldr(xmm.S(), [a])                (32-bit, zero hi)
//   xMOVDZX(xmm, gpr)         -> Fmov(xmm.S(), gpr)               (W->S, zero hi)
//   xSHUF.PS(xmm,xmm,0)       -> Dup(xmm.V4S(), xmm.V4S(), 0)     (lane0 broadcast)
//   xMOV(ptr32[a], gpr/imm)   -> Str / (Mov tmp,imm + Str)
//   xAND(ptr32[a], imm)       -> Ldr + And + Str
//   xJMP(mVU.exitFunct)       -> armEmitJmp(mVU.exitFunct)
//   xFastCall((void*)fn)      -> armEmitCall(fn)
// Absolute &mVU.regs()… / &VU0.VI[…] addresses are materialized with
// armMoveAddressToReg (clobbers x16/x17), exactly like the flag/alloc helpers.

// mVUincCycles is defined later in aVU.cpp (after this .inl is included), so it
// needs a forward declaration here — same as x86 microVU_Branch.inl.
extern void mVUincCycles(microVU& mVU, int x);

// The small absolute-address memory-access helpers (mvuStr32/mvuLdr32/mvuStrImm32/
// mvuStrSS/mvuLdrSS/mvuLdrQ/mvuStrQ/mvuMemAndImm32) moved to aVU_Misc.inl so the
// Tables/Flags slices (included earlier) can share them.

//------------------------------------------------------------------
// XGKICK stubs (real GIF-kick path is task 7.5b)
//------------------------------------------------------------------
// microVU is unselected on ARM64 (VMManager pins CpuIntVU0/1), so these are never
// executed yet; they exist only so mVUendProgram/mVUDTendProgram link and compile.
static void mVU_XGKICK_DELAY(mV) { (void)mVU; /* TODO 7.5b: run pending XGKick */ }
static void mVU_XGKICK_SYNC(mV, bool flush) { (void)mVU; (void)flush; /* TODO 7.5b */ }

//------------------------------------------------------------------
// C thunks called from emitted code
//------------------------------------------------------------------
static void mVUTBit()
{
	u32 old = vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUTBit, std::memory_order_release);
	if (old & VU_Thread::InterruptFlagVUTBit)
		DevCon.Warning("Old TBit not registered");
}

static void mVUEBit()
{
	vu1Thread.mtvuInterrupts.fetch_or(VU_Thread::InterruptFlagVUEBit, std::memory_order_release);
}

void mVU0clearlpStateJIT() { if (!microVU0.prog.cleared) std::memset(&microVU0.prog.lpState, 0, sizeof(microVU0.prog.lpState)); }
void mVU1clearlpStateJIT() { if (!microVU1.prog.cleared) std::memset(&microVU1.prog.lpState, 0, sizeof(microVU1.prog.lpState)); }

//------------------------------------------------------------------
// Flag-instance selection (pure analysis)
//------------------------------------------------------------------
__fi int getLastFlagInst(microRegInfo& pState, int* xFlag, int flagType, int isEbit)
{
	if (isEbit)
		return findFlagInst(xFlag, 0x7fffffff);
	if (pState.needExactMatch & (1 << flagType))
		return 3;
	return (((pState.flagInfo >> (2 * flagType + 2)) & 3) - 1) & 3;
}

//------------------------------------------------------------------
// Program exit (D/T-bit early-exit path)
//------------------------------------------------------------------
void mVUDTendProgram(mV, microFlagCycles* mFC, int isEbit)
{
	int fStatus = getLastFlagInst(mVUpBlock->pState, mFC->xStatus, 0, isEbit);
	int fMac    = getLastFlagInst(mVUpBlock->pState, mFC->xMac, 1, isEbit);
	int fClip   = getLastFlagInst(mVUpBlock->pState, mFC->xClip, 2, isEbit);
	int qInst   = 0;
	int pInst   = 0;
	microBlock stateBackup;
	memcpy(&stateBackup, &mVUregs, sizeof(mVUregs)); // backup the state, it's about to get screwed with.

	mVU.regAlloc->TDwritebackAll(); // Writing back ok, invalidating early kills the rec, so don't do it :P

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
		// Run any pending XGKick, providing we've got to it.
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
			armEmitCall(reinterpret_cast<const void*>(mVU0clearlpStateJIT));
		else
			armEmitCall(reinterpret_cast<const void*>(mVU1clearlpStateJIT));
	}

	// Save P/Q Regs
	if (qInst)
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xe1);
	mvuStrSS(&mVU.regs().VI[REG_Q].UL, mVU_xmmPQ);
	mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xe1);
	mvuStrSS(&mVU.regs().pending_q, mVU_xmmPQ);
	mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xe1);

	if (isVU1)
	{
		if (pInst)
			mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xb4); // Swap Pending/Active P
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xC6); // 3 0 1 2
		mvuStrSS(&mVU.regs().VI[REG_P].UL, mVU_xmmPQ);
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0x87); // 0 2 1 3
		mvuStrSS(&mVU.regs().pending_p, mVU_xmmPQ);
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0x27); // 3 2 1 0
	}

	// Save MAC, Status and CLIP Flag Instances
	mVUallocSFLAGc(gprT1, gprT2, fStatus);
	mvuStr32(&mVU.regs().VI[REG_STATUS_FLAG].UL, gprT1);
	mVUallocMFLAGa(mVU, gprT1, fMac);
	mVUallocCFLAGa(mVU, gprT2, fClip);
	mvuStr32(&mVU.regs().VI[REG_MAC_FLAG].UL, gprT1);
	mvuStr32(&mVU.regs().VI[REG_CLIP_FLAG].UL, gprT2);

	if (!isEbit) // Backup flag instances
	{
		mvuLdrQ(xmmT1, &mVU.macFlag[0]);
		mvuStrQ(&mVU.regs().micro_macflags, xmmT1);
		mvuLdrQ(xmmT1, &mVU.clipFlag[0]);
		mvuStrQ(&mVU.regs().micro_clipflags, xmmT1);

		mvuStr32(&mVU.regs().micro_statusflags[0], gprF0);
		mvuStr32(&mVU.regs().micro_statusflags[1], gprF1);
		mvuStr32(&mVU.regs().micro_statusflags[2], gprF2);
		mvuStr32(&mVU.regs().micro_statusflags[3], gprF3);
	}
	else // Flush flag instances
	{
		mvuLdrSS(xmmT1, &mVU.regs().VI[REG_CLIP_FLAG].UL);
		armAsm->Dup(xmmT1.V4S(), xmmT1.V4S(), 0);
		mvuStrQ(&mVU.regs().micro_clipflags, xmmT1);

		mvuLdrSS(xmmT1, &mVU.regs().VI[REG_MAC_FLAG].UL);
		armAsm->Dup(xmmT1.V4S(), xmmT1.V4S(), 0);
		mvuStrQ(&mVU.regs().micro_macflags, xmmT1);

		armAsm->Fmov(xmmT1.S(), getFlagReg(fStatus));
		armAsm->Dup(xmmT1.V4S(), xmmT1.V4S(), 0);
		mvuStrQ(&mVU.regs().micro_statusflags, xmmT1);
	}

	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		mvuStrImm32(&mVU.regs().nextBlockCycles, 0, gprT1);

	mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);

	if (isEbit) // Clear 'is busy' Flags
	{
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemAndImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? ~0x100u : ~0x001u), gprT1); // VBS0/VBS1 flag
		}
	}

	if (isEbit != 2) // Save PC, and Jump to Exit Point
	{
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUTBit));
		armEmitJmp(mVU.exitFunct);
	}

	memcpy(&mVUregs, &stateBackup, sizeof(mVUregs)); // Restore the state for the rest of the recompile
}

//------------------------------------------------------------------
// Program exit (normal / E-bit end)
//------------------------------------------------------------------
void mVUendProgram(mV, microFlagCycles* mFC, int isEbit)
{
	int fStatus = getLastFlagInst(mVUpBlock->pState, mFC->xStatus, 0, isEbit && isEbit != 3);
	int fMac    = getLastFlagInst(mVUpBlock->pState, mFC->xMac, 1, isEbit && isEbit != 3);
	int fClip   = getLastFlagInst(mVUpBlock->pState, mFC->xClip, 2, isEbit && isEbit != 3);
	int qInst   = 0;
	int pInst   = 0;
	microBlock stateBackup;
	memcpy(&stateBackup, &mVUregs, sizeof(mVUregs)); // backup the state, it's about to get screwed with.
	if (!isEbit || isEbit == 3)
		mVU.regAlloc->TDwritebackAll(); // Writing back ok, invalidating early kills the rec, so don't do it :P
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
		if (!isVU1)
			armEmitCall(reinterpret_cast<const void*>(mVU0clearlpStateJIT));
		else
			armEmitCall(reinterpret_cast<const void*>(mVU1clearlpStateJIT));
	}

	// Save P/Q Regs
	if (qInst)
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xe1);
	mvuStrSS(&mVU.regs().VI[REG_Q].UL, mVU_xmmPQ);
	mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xe1);
	mvuStrSS(&mVU.regs().pending_q, mVU_xmmPQ);
	mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xe1);

	if (isVU1)
	{
		if (pInst)
			mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xb4); // Swap Pending/Active P
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0xC6); // 3 0 1 2
		mvuStrSS(&mVU.regs().VI[REG_P].UL, mVU_xmmPQ);
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0x87); // 0 2 1 3
		mvuStrSS(&mVU.regs().pending_p, mVU_xmmPQ);
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, 0x27); // 3 2 1 0
	}

	// Save MAC, Status and CLIP Flag Instances
	mVUallocSFLAGc(gprT1, gprT2, fStatus);
	mvuStr32(&mVU.regs().VI[REG_STATUS_FLAG].UL, gprT1);
	mVUallocMFLAGa(mVU, gprT1, fMac);
	mVUallocCFLAGa(mVU, gprT2, fClip);
	mvuStr32(&mVU.regs().VI[REG_MAC_FLAG].UL, gprT1);
	mvuStr32(&mVU.regs().VI[REG_CLIP_FLAG].UL, gprT2);

	if (!isEbit || isEbit == 3) // Backup flag instances
	{
		mvuLdrQ(xmmT1, &mVU.macFlag[0]);
		mvuStrQ(&mVU.regs().micro_macflags, xmmT1);
		mvuLdrQ(xmmT1, &mVU.clipFlag[0]);
		mvuStrQ(&mVU.regs().micro_clipflags, xmmT1);

		mvuStr32(&mVU.regs().micro_statusflags[0], gprF0);
		mvuStr32(&mVU.regs().micro_statusflags[1], gprF1);
		mvuStr32(&mVU.regs().micro_statusflags[2], gprF2);
		mvuStr32(&mVU.regs().micro_statusflags[3], gprF3);
	}
	else // Flush flag instances
	{
		mvuLdrSS(xmmT1, &mVU.regs().VI[REG_CLIP_FLAG].UL);
		armAsm->Dup(xmmT1.V4S(), xmmT1.V4S(), 0);
		mvuStrQ(&mVU.regs().micro_clipflags, xmmT1);

		mvuLdrSS(xmmT1, &mVU.regs().VI[REG_MAC_FLAG].UL);
		armAsm->Dup(xmmT1.V4S(), xmmT1.V4S(), 0);
		mvuStrQ(&mVU.regs().micro_macflags, xmmT1);

		armAsm->Fmov(xmmT1.S(), getFlagReg(fStatus));
		armAsm->Dup(xmmT1.V4S(), xmmT1.V4S(), 0);
		mvuStrQ(&mVU.regs().micro_statusflags, xmmT1);
	}

	mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);

	if ((isEbit && isEbit != 3)) // Clear 'is busy' Flags
	{
		if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
			mvuStrImm32(&mVU.regs().nextBlockCycles, 0, gprT1);
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemAndImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? ~0x100u : ~0x001u), gprT1); // VBS0/VBS1 flag
		}
	}
	else if (isEbit)
	{
		if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
			mvuStrImm32(&mVU.regs().nextBlockCycles, 0, gprT1);
	}

	if (isEbit != 2 && isEbit != 3) // Save PC, and Jump to Exit Point
	{
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUEBit));
		armEmitJmp(mVU.exitFunct);
	}
	memcpy(&mVUregs, &stateBackup, sizeof(mVUregs)); // Restore the state for the rest of the recompile
}

//------------------------------------------------------------------
// Block-linking setup (flag instances + P/Q reset)
//------------------------------------------------------------------
// Recompiles code for proper flags and Q/P regs on block linkings. Table-
// independent: only needs mVUsetupFlags (ported) + a PQ lane permute, so it comes
// over now; its callers (normBranch/normJump/condBranch) arrive with the Compile
// slice. x86 xPSHUF.D(xmmPQ, xmmPQ, shufflePQ) -> mVUshufflePS.
void mVUsetupBranch(mV, microFlagCycles& mFC)
{
	mVU.regAlloc->flushAll(); // Flush Allocated Regs
	mVUsetupFlags(mVU, mFC);  // Shuffle Flag Instances

	// Shuffle P/Q regs since every block starts at instance #0
	if (mVU.p || mVU.q)
		mVUshufflePS(mVU_xmmPQ, mVU_xmmPQ, shufflePQ);
	mVU.p = 0, mVU.q = 0;
}

//------------------------------------------------------------------
// Branch drivers (the cross-referencing core)
//------------------------------------------------------------------
// These emit the block-exit / block-linking code for every branch/jump type.
// They mutually recurse with mVUcompile (defined later in aVU_Compile.inl), so it
// is forward-declared here exactly like x86 microVU_Branch.inl. mVUcompileJIT (the
// runtime JR/JALR thunk) is likewise forward-declared to take its address.
extern void* mVUcompile(microVU& mVU, u32 startPC, uptr pState);
extern void* mVUblockFetch(microVU& mVU, u32 startPC, uptr pState);
template <int vuIndex> void* mVUcompileJIT(u32 startPC, uptr ptr);

// Small file-local helpers for the absolute-addr ops the drivers need.
static inline void mvuLdrh16(const a64::Register& wreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldrh(wreg.W(), a64::MemOperand(RSCRATCHADDR));
}
// xTEST(ptr32[addr], imm) + xForwardJump32(Jcc_Zero): load, test, branch-if-zero.
static inline void mvuTestMemBranchZero(const void* addr, u32 imm, a64::Label& tgt, const a64::Register& tmp)
{
	mvuLdr32(tmp, addr);
	armAsm->Tst(tmp.W(), imm);
	armAsm->B(&tgt, a64::eq); // ZF set == (val & imm) == 0
}

void normBranchCompile(microVU& mVU, u32 branchPC)
{
	microBlock* pBlock;
	blockCreate(branchPC / 8);
	pBlock = mVUblocks[branchPC / 8]->search(mVU, (microRegInfo*)&mVUregs);
	if (pBlock)
		armEmitJmp(pBlock->codeStart);
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
		mvuLdr32(RWARG1, &mVU.evilBranch); // startPC (arg1)
		mvuLdr32(gprT1, &mVU.evilevilBranch);
		mvuStr32(&mVU.evilBranch, gprT1);
	}
	else
		mvuLdr32(RWARG1, &mVU.branch);
	if (doJumpCaching)
		armMoveAddressToReg(RXARG2, mVUpBlock);
	else
		armMoveAddressToReg(RXARG2, &mVUpBlock->pStateEnd);

	if (mVUup.eBit && isEvilJump) // E-bit EvilJump
	{
		// Xtreme G 3 does 2 conditional jumps, the first contains an E Bit on the first instruction
		// So if it is taken, you need to end the program, else you get infinite loops.
		mVUendProgram(mVU, &mFC, 2);
		mvuStr32(&mVU.regs().VI[REG_TPC].UL, RWARG1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUEBit));
		armEmitJmp(mVU.exitFunct);
	}

	if (!mVU.index)
		armEmitCall(reinterpret_cast<const void*>(&mVUcompileJIT<0>)); // (u32 startPC, uptr pState)
	else
		armEmitCall(reinterpret_cast<const void*>(&mVUcompileJIT<1>));

	mVUrestoreRegs(mVU);
	armAsm->Br(RXRET); // Jump to rec-code address (returned in x0)
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
		mvuTestMemBranchZero((mVU.index && THREAD_VU1) ? (const void*)&vu1Thread.vuFBRST : (const void*)&VU0.VI[REG_FBRST].UL,
			(isVU1 ? 0x400 : 0x4), eJMP, gprT1);
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x200 : 0x2), gprT1);
			mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
		}
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
		mvuTestMemBranchZero((mVU.index && THREAD_VU1) ? (const void*)&vu1Thread.vuFBRST : (const void*)&VU0.VI[REG_FBRST].UL,
			(isVU1 ? 0x800 : 0x8), eJMP, gprT1);
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x400 : 0x4), gprT1);
			mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
		}
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
		armMoveAddressToReg(RXARG1, &mVUpBlock->pStateEnd);
		armEmitCall(mVU.copyPLState);

		mVUsetupBranch(mVU, mFC);
		mVUendProgram(mVU, &mFC, 3);
		iPC = branchAddr(mVU) / 4;
		mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUEBit));
		armEmitJmp(mVU.exitFunct);
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
		a64::Label eJMP;
		mvuTestMemBranchZero((mVU.index && THREAD_VU1) ? (const void*)&vu1Thread.vuFBRST : (const void*)&VU0.VI[REG_FBRST].UL,
			(isVU1 ? 0x800 : 0x8), eJMP, gprT1);
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x400 : 0x4), gprT1);
			mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		mvuLdrh16(gprT1, &mVU.branch);
		armAsm->Cmp(gprT1.W(), 0);
		a64::Label tJMP;
		armAsm->B(&tJMP, a64::InvertCondition(JMPcc));
			incPC(4); // Set PC to First instruction of Non-Taken Side
			mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
			if (mVU.index && THREAD_VU1)
				armEmitCall(reinterpret_cast<const void*>(mVUTBit));
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&tJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUTBit));
		armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&eJMP);
		iPC = tempPC;
	}
	if (mVUup.dBit && doDBitHandling)
	{
		u32 tempPC = iPC;
		a64::Label eJMP;
		mvuTestMemBranchZero((mVU.index && THREAD_VU1) ? (const void*)&vu1Thread.vuFBRST : (const void*)&VU0.VI[REG_FBRST].UL,
			(isVU1 ? 0x400 : 0x4), eJMP, gprT1);
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x200 : 0x2), gprT1);
			mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		mvuLdrh16(gprT1, &mVU.branch);
		armAsm->Cmp(gprT1.W(), 0);
		a64::Label dJMP;
		armAsm->B(&dJMP, a64::InvertCondition(JMPcc));
			incPC(4); // Set PC to First instruction of Non-Taken Side
			mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&dJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
		armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&eJMP);
		iPC = tempPC;
	}
	if (mVUup.mBit)
	{
		u32 tempPC = iPC;

		memcpy(&mVUpBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));
		armMoveAddressToReg(RXARG1, &mVUpBlock->pStateEnd);
		armEmitCall(mVU.copyPLState);

		mVUendProgram(mVU, &mFC, 3);
		mvuLdrh16(gprT1, &mVU.branch);
		armAsm->Cmp(gprT1.W(), 0);
		a64::Label dJMP;
		armAsm->B(&dJMP, JMPcc);
		incPC(4); // Set PC to First instruction of Non-Taken Side
		mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUEBit));
		armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&dJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr
		iPC = branchAddr(mVU) / 4;
		mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUEBit));
		armEmitJmp(mVU.exitFunct);
		iPC = tempPC;
	}
	if (mVUup.eBit) // Conditional Branch With E-Bit Set
	{
		if (mVUlow.evilBranch)
			DevCon.Warning("End on evil branch! - Not implemented! - If game broken report to PCSX2 Team");

		mVUendProgram(mVU, &mFC, 2);
		mvuLdrh16(gprT1, &mVU.branch);
		armAsm->Cmp(gprT1.W(), 0);

		incPC(3);
		a64::Label eJMP;
		armAsm->B(&eJMP, JMPcc);
			incPC(1); // Set PC to First instruction of Non-Taken Side
			mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
			if (mVU.index && THREAD_VU1)
				armEmitCall(reinterpret_cast<const void*>(mVUEBit));
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&eJMP);
		incPC(-4); // Go Back to Branch Opcode to get branchAddr

		iPC = branchAddr(mVU) / 4;
		mvuStrImm32(&mVU.regs().VI[REG_TPC].UL, xPC, gprT1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUEBit));
		armEmitJmp(mVU.exitFunct);
		return;
	}
	else // Normal Conditional Branch
	{
		mvuLdrh16(gprT1, &mVU.branch);
		armAsm->Cmp(gprT1.W(), 0);

		incPC(3);
		microBlock* bBlock;
		incPC2(1); // Check if Branch Non-Taken Side has already been recompiled
		blockCreate(iPC / 2);
		bBlock = mVUblocks[iPC / 2]->search(mVU, (microRegInfo*)&mVUregs);
		incPC2(-1);
		if (bBlock) // Branch non-taken has already been compiled
		{
			armEmitCondBranch(a64::InvertCondition(JMPcc), bBlock->codeStart);
			incPC(-3); // Go back to branch opcode (to get branch imm addr)
			normBranchCompile(mVU, branchAddr(mVU));
		}
		else
		{
			a64::Label takenLabel;
			armAsm->B(&takenLabel, JMPcc);
			u32 bPC = iPC; // mVUcompile can modify iPC, mVUpBlock, and mVUregs so back them up

			microRegInfo regBackup;
			memcpy(&regBackup, &mVUregs, sizeof(microRegInfo));

			incPC2(1); // Get PC for branch not-taken
			mVUcompile(mVU, xPC, (uptr)&mVUregs);

			iPC = bPC;
			incPC(-3); // Go back to branch opcode (to get branch imm addr)
			armAsm->Bind(&takenLabel);
			u8* const beforeFetch = armGetCurrentCodePointer();
			void* jumpAddr = mVUblockFetch(mVU, branchAddr(mVU), (uptr)&regBackup);
			// If mVUblockFetch found an existing block (emitted nothing), bridge to
			// it; if it compiled inline, the block starts right here (fall through).
			if (jumpAddr != beforeFetch)
				armEmitJmp(jumpAddr);
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
		mvuTestMemBranchZero(THREAD_VU1 ? (const void*)&vu1Thread.vuFBRST : (const void*)&VU0.VI[REG_FBRST].UL,
			(isVU1 ? 0x400 : 0x4), eJMP, gprT1);
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x200 : 0x2), gprT1);
			mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		mvuLdr32(gprT1, &mVU.branch);
		mvuStr32(&mVU.regs().VI[REG_TPC].UL, gprT1);
		armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&eJMP);
	}
	if (mVUup.tBit)
	{
		// Flush register cache early to avoid double flush on both paths
		mVU.regAlloc->flushAll(false);

		a64::Label eJMP;
		mvuTestMemBranchZero((mVU.index && THREAD_VU1) ? (const void*)&vu1Thread.vuFBRST : (const void*)&VU0.VI[REG_FBRST].UL,
			(isVU1 ? 0x800 : 0x8), eJMP, gprT1);
		if (!mVU.index || !THREAD_VU1)
		{
			mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x400 : 0x4), gprT1);
			mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
		}
		mVUDTendProgram(mVU, &mFC, 2);
		mvuLdr32(gprT1, &mVU.branch);
		mvuStr32(&mVU.regs().VI[REG_TPC].UL, gprT1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUTBit));
		armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&eJMP);
	}
	if (mVUup.eBit) // E-bit Jump
	{
		mVUendProgram(mVU, &mFC, 2);
		mvuLdr32(gprT1, &mVU.branch);
		mvuStr32(&mVU.regs().VI[REG_TPC].UL, gprT1);
		if (mVU.index && THREAD_VU1)
			armEmitCall(reinterpret_cast<const void*>(mVUEBit));
		armEmitJmp(mVU.exitFunct);
	}
	else
	{
		normJumpCompile(mVU, mFC, false);
	}
}
