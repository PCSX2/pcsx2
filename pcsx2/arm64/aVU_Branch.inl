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

//------------------------------------------------------------------
// Small absolute-address memory-access helpers (file-local)
//------------------------------------------------------------------
// armMoveAddressToReg clobbers RSCRATCH (x16/x17); the value regs passed in are
// always w9/w10 (gprT1/gprT2) or the flag GPRs, never x16/x17.

static inline void mvuStr32(const void* addr, const a64::Register& wreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(wreg, a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuStrImm32(const void* addr, u32 imm, const a64::Register& tmp)
{
	armAsm->Mov(tmp, imm);
	mvuStr32(addr, tmp);
}

static inline void mvuStrSS(const void* addr, const a64::VRegister& vreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(vreg.S(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdrSS(const a64::VRegister& vreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(vreg.S(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdrQ(const a64::VRegister& vreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(vreg.Q(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuStrQ(const void* addr, const a64::VRegister& vreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(vreg.Q(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuMemAndImm32(const void* addr, u32 imm, const a64::Register& tmp)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(tmp, a64::MemOperand(RSCRATCHADDR));
	armAsm->And(tmp, tmp, imm);
	armAsm->Str(tmp, a64::MemOperand(RSCRATCHADDR));
}

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
