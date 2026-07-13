// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

extern void mVUincCycles(microVU& mVU, int x);
extern void* mVUcompile(microVU& mVU, u32 startPC, uptr pState);

void mVU0clearlpStateJIT() { if (!microVU0.prog.cleared) std::memset(&microVU0.prog.lpState, 0, sizeof(microVU0.prog.lpState)); }
void mVU1clearlpStateJIT() { if (!microVU1.prog.cleared) std::memset(&microVU1.prog.lpState, 0, sizeof(microVU1.prog.lpState)); }

__fi int getLastFlagInst(microRegInfo& pState, int* xFlag, int flagType, int isEbit)
{
	if (isEbit)
		return findFlagInst(xFlag, 0x7fffffff);
	if (pState.needExactMatch & (1 << flagType))
		return 3;
	return (((pState.flagInfo >> (2 * flagType + 2)) & 3) - 1) & 3;
}

//------------------------------------------------------------------
// mVUDTendProgram — D/T-bit end program variant
//------------------------------------------------------------------

void mVUDTendProgram(mV, microFlagCycles* mFC, int isEbit)
{
	int fStatus = getLastFlagInst(mVUpBlock->pState, mFC->xStatus, 0, isEbit);
	int fMac    = getLastFlagInst(mVUpBlock->pState, mFC->xMac, 1, isEbit);
	int fClip   = getLastFlagInst(mVUpBlock->pState, mFC->xClip, 2, isEbit);
	int qInst = 0, pInst = 0;
	microBlock stateBackup;
	memcpy(&stateBackup, &mVUregs, sizeof(mVUregs));

	mVU.regAlloc->flushAll();

	if (isEbit)
	{
		mVUincCycles(mVU, 100);
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
		// Run any pending XGKick providing we've reached its PC.
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
			armEmitCall((void*)mVU0clearlpStateJIT);
		else
			armEmitCall((void*)mVU1clearlpStateJIT);
	}

	// Save P/Q regs from qmmPQ. qmmPQ layout: [0]=Q, [1]=pending_q, [2]=P,
	// [3]=pending_p. Lane-0 stores go through Str-S [gprVUState, #imm12] since
	// the S form refers to the lower 32 bits of the V register. Ext-by-4 is a
	// full 4-lane left rotate (NOT an involution), so its inverse is Ext-by-12,
	// NOT another Ext-by-4 — the qInst==1 "swap back" below MUST use Ext12.
	if (qInst)
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 4);
	armAsm->Str(a64::SRegister(qmmPQ.GetCode()),
		mVUstateMem(offsetof(VURegs, VI) + REG_Q * sizeof(REG_VI)));
	if (qInst)
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 12); // Swap back (inverse of Ext4)
	else
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 4);
	armAsm->Str(a64::SRegister(qmmPQ.GetCode()),
		mVUstateMem(offsetof(VURegs, pending_q)));
	if (!qInst)
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 12); // Restore

	if (isVU1)
	{
		// pInst rotation: when set, lanes 2/3 hold (pending_p, P) instead of
		// (P, pending_p). Mirror x86 mVUendProgram by swapping the St1 lane
		// indices rather than emitting a physical lane swap.
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_P * sizeof(REG_VI));
		armAsm->St1(qmmPQ.V4S(), pInst ? 3 : 2, a64::MemOperand(a64::x8));
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, pending_p));
		armAsm->St1(qmmPQ.V4S(), pInst ? 2 : 3, a64::MemOperand(a64::x8));
	}

	// MAC/CLIP store (per-callsite — fMac/fClip vary by emit-time fInstance).
	mVUallocMFLAGa(mVU, gprT1, fMac);
	mVUallocCFLAGa(mVU, gprT2, fClip);
	armAsm->Str(gprT1, mVUstateMem(offsetof(VURegs, VI) + REG_MAC_FLAG * sizeof(REG_VI)));
	armAsm->Str(gprT2, mVUstateMem(offsetof(VURegs, VI) + REG_CLIP_FLAG * sizeof(REG_VI)));

	// SFLAGc + micro_flag tail factored into per-VU helpers; see
	// mVUGenerateEndProgramFlagsHelper.
	armAsm->Mov(gprT3, getFlagReg(fStatus));
	armEmitCall(isEbit ? mVU.endProgramFlagsB : mVU.endProgramFlagsA);

	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
	{
		armAsm->Str(a64::wzr, mVUstateMem(offsetof(VURegs, nextBlockCycles)));
	}

	armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
	armAsm->Mov(a64::w9, xPC);
	armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

	if (isEbit)
	{
		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->And(a64::w9, a64::w9, isVU1 ? ~0x100u : ~0x001u);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
		}
	}

	if (isEbit != 2)
	{
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUTBit);
		armEmitJmp(mVU.exitFunct);
	}

	memcpy(&mVUregs, &stateBackup, sizeof(mVUregs));
}

//------------------------------------------------------------------
// mVUendProgram — Main end-of-program handler
//------------------------------------------------------------------

void mVUendProgram(mV, microFlagCycles* mFC, int isEbit)
{
	int fStatus = getLastFlagInst(mVUpBlock->pState, mFC->xStatus, 0, isEbit && isEbit != 3);
	int fMac    = getLastFlagInst(mVUpBlock->pState, mFC->xMac, 1, isEbit && isEbit != 3);
	int fClip   = getLastFlagInst(mVUpBlock->pState, mFC->xClip, 2, isEbit && isEbit != 3);
	int qInst = 0, pInst = 0;
	microBlock stateBackup;
	memcpy(&stateBackup, &mVUregs, sizeof(mVUregs));

	// x86 (microVU_Branch.inl) splits this: TDwritebackAll() for the non-E-bit /
	// isEbit==3 case (write back without clearing, to preserve mappings for
	// downstream reuse) and flushAll() for the E-bit case. On arm64 nothing after
	// this flush reuses the regAlloc VF/VI mappings — the P/Q saves below go
	// through qmmPQ and the MAC/CLIP/status stores use scratch temps — so the
	// clear-vs-no-clear distinction is unobservable and one flushAll() covers
	// both. (arm64 regAlloc has no TDwritebackAll; flushAll(false) would be the
	// no-clear equivalent if a downstream consumer ever appears here.)
	mVU.regAlloc->flushAll();

	if (isEbit && isEbit != 3)
	{
		std::memset(&mVUinfo, 0, sizeof(mVUinfo));
		std::memset(&mVUregsTemp, 0, sizeof(mVUregsTemp));
		mVUincCycles(mVU, 100);
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
			armEmitCall((void*)mVU0clearlpStateJIT);
		else
			armEmitCall((void*)mVU1clearlpStateJIT);
	}

	// Save P/Q regs from qmmPQ. qmmPQ layout: [0]=Q, [1]=pending_q, [2]=P,
	// [3]=pending_p. Lane-0 stores go through Str-S [gprVUState, #imm12] since
	// the S form refers to the lower 32 bits of the V register. Ext-by-4 is a
	// full 4-lane left rotate (NOT an involution), so its inverse is Ext-by-12,
	// NOT another Ext-by-4 — the qInst==1 "swap back" below MUST use Ext12.
	if (qInst)
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 4);
	armAsm->Str(a64::SRegister(qmmPQ.GetCode()),
		mVUstateMem(offsetof(VURegs, VI) + REG_Q * sizeof(REG_VI)));
	if (qInst)
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 12);
	else
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 4);
	armAsm->Str(a64::SRegister(qmmPQ.GetCode()),
		mVUstateMem(offsetof(VURegs, pending_q)));
	if (!qInst)
		armAsm->Ext(qmmPQ.V16B(), qmmPQ.V16B(), qmmPQ.V16B(), 12);

	if (isVU1)
	{
		// pInst rotation: when set, lanes 2/3 hold (pending_p, P) instead of
		// (P, pending_p). Mirror x86 mVUendProgram by swapping the St1 lane
		// indices rather than emitting a physical lane swap.
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_P * sizeof(REG_VI));
		armAsm->St1(qmmPQ.V4S(), pInst ? 3 : 2, a64::MemOperand(a64::x8));
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, pending_p));
		armAsm->St1(qmmPQ.V4S(), pInst ? 2 : 3, a64::MemOperand(a64::x8));
	}

	// MAC/CLIP store (per-callsite — fMac/fClip vary by emit-time fInstance).
	mVUallocMFLAGa(mVU, gprT1, fMac);
	mVUallocCFLAGa(mVU, gprT2, fClip);
	armAsm->Str(gprT1, mVUstateMem(offsetof(VURegs, VI) + REG_MAC_FLAG * sizeof(REG_VI)));
	armAsm->Str(gprT2, mVUstateMem(offsetof(VURegs, VI) + REG_CLIP_FLAG * sizeof(REG_VI)));

	// SFLAGc denormalization + micro_flag backup-or-broadcast tail factored
	// into a per-VU BL-callable helper. Per-exit emit shrinks from ~20 insns
	// to 2 (Mov + Bl). See mVUGenerateEndProgramFlagsHelper in
	// pcsx2/arm64/microVU-arm64.cpp.
	armAsm->Mov(gprT3, getFlagReg(fStatus));
	armEmitCall((!isEbit || isEbit == 3) ? mVU.endProgramFlagsA : mVU.endProgramFlagsB);

	// Save TPC
	armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
	armAsm->Mov(a64::w9, xPC);
	armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

	if (isEbit && isEbit != 3)
	{
		if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		{
			armAsm->Str(a64::wzr, mVUstateMem(offsetof(VURegs, nextBlockCycles)));
		}
		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->And(a64::w9, a64::w9, isVU1 ? ~0x100u : ~0x001u);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
		}
	}
	else if (isEbit == 3)
	{
		if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		{
			armAsm->Str(a64::wzr, mVUstateMem(offsetof(VURegs, nextBlockCycles)));
		}
	}

	if (isEbit != 2 && isEbit != 3)
	{
		// lpState is established by the branch sites (normBranch/condBranch via
		// pStateEnd), the M-bit path, and the cycle early-exit before control
		// reaches here. mVUendProgram intentionally does NOT copy pipeline state
		// (matching mVUendProgram in x86 microVU_Branch.inl), so a Q/P-pipeline
		// countdown still carried by lpState is preserved for the next program.
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUEBit);
		armEmitJmp(mVU.exitFunct);
	}

	memcpy(&mVUregs, &stateBackup, sizeof(mVUregs));
}

//------------------------------------------------------------------
// Branch Setup
//------------------------------------------------------------------

void mVUsetupBranch(mV, microFlagCycles& mFC)
{
	mVU.regAlloc->flushAll();
	mVUsetupFlags(mVU, mFC);

	// Shuffle P/Q regs since every block starts at instance #0.
	// qmmPQ layout: [0]=Q, [1]=pending_q, [2]=P, [3]=pending_p.
	// When mVU.q==1 the "current" Q is in pending_q's slot — swap lanes 0/1.
	// When mVU.p==1, similarly swap lanes 2/3.
	// Matches x86 mVUsetupBranch (xPSHUF.D(xmmPQ, xmmPQ, shufflePQ)).
	if (mVU.q)
	{
		armAsm->Umov(gprT1.W(), qmmPQ.V4S(), 0);
		armAsm->Ins(qmmPQ.V4S(), 0, qmmPQ.V4S(), 1);
		armAsm->Ins(qmmPQ.V4S(), 1, gprT1.W());
	}
	if (mVU.p)
	{
		armAsm->Umov(gprT1.W(), qmmPQ.V4S(), 2);
		armAsm->Ins(qmmPQ.V4S(), 2, qmmPQ.V4S(), 3);
		armAsm->Ins(qmmPQ.V4S(), 3, gprT1.W());
	}
	mVU.p = 0;
	mVU.q = 0;
}

//------------------------------------------------------------------
// normBranchCompile — Compile/link to a block at known PC
//------------------------------------------------------------------

void normBranchCompile(microVU& mVU, u32 branchPC)
{
	microBlock* pBlock;
	blockCreate(branchPC / 8);
	pBlock = mVUblocks[branchPC / 8]->search(mVU, (microRegInfo*)&mVUregs);
	if (pBlock)
		armEmitJmp(pBlock->hostEntry);
	else
		mVUcompile(mVU, branchPC, (uptr)&mVUregs);
}

//------------------------------------------------------------------
// normJumpCompile — Compile indirect jump (JR/JALR)
//------------------------------------------------------------------

void normJumpCompile(mV, microFlagCycles& mFC, bool isEvilJump)
{
	memcpy(&mVUpBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));
	mVUsetupBranch(mVU, mFC);
	mVUbackupRegs(mVU);

	if (!mVUpBlock->jumpCache)
		mVUpBlock->jumpCache = new microJumpCache[mProgSize / 2];

	if (isEvilJump)
	{
		mVUldrField(mVU, RWARG1, &mVU.evilBranch);
		mVUldrField(mVU, gprT1, &mVU.evilevilBranch);
		mVUstrField(mVU, gprT1, &mVU.evilBranch);
	}
	else
		mVUldrField(mVU, RWARG1, &mVU.branch);

	if (doJumpCaching)
		armMoveAddressToReg(RXARG2, mVUpBlock);
	else
		armMoveAddressToReg(RXARG2, &mVUpBlock->pStateEnd);

	if (mVUup.eBit && isEvilJump)
	{
		mVUendProgram(mVU, &mFC, 2);
		armAsm->Str(RWARG1, mVUstateMem(offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI)));
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUEBit);
		armEmitJmp(mVU.exitFunct);
	}

	if (!mVU.index)
		armEmitCall((void*)(void (*)())mVUcompileJIT<0>);
	else
		armEmitCall((void*)(void (*)())mVUcompileJIT<1>);

	mVUrestoreRegs(mVU);

	// Jump to returned code address (in x0).
	// Guard against NULL: if compile failed, exit instead of crashing.
	a64::Label validBlock;
	armAsm->Cbnz(a64::x0, &validBlock);
	armEmitJmp(mVU.exitFunct);
	armAsm->Bind(&validBlock);
	armAsm->Br(a64::x0);
}

//------------------------------------------------------------------
// ARM64 MacroAssembler helpers for runtime compilation
//------------------------------------------------------------------

// Per-mVUopenCodeCache state: binds the global armAsm to the persistent
// per-VU MacroAssembler and records where its cursor sat so mVUcloseCodeCache
// can compute *this* block's size from the cursor delta. The MA itself is
// constructed once per mVUreset over the whole post-dispatcher cache range.
static thread_local ptrdiff_t s_mVUblockStartOffset = 0;

static void mVUopenCodeCache(microVU& mVU)
{
	// Nested call (same thread re-enters before its outer close): no-op.
	if (armAsm)
		return;

	pxAssert(mVU.jitAsm); // mVUreset must have built it.

	HostSys::BeginCodeWrite();
	// armAsmPtr MUST equal the MA's buffer base (= mVU.prog.x86start) for
	// armGetCurrentCodePointer() to return the real write position:
	//   armGetCurrentCodePointer() = armAsmPtr + armAsm->GetCursorOffset()
	// With the persistent MA the cursor is "bytes since x86start", so
	// armAsmPtr must be x86start (NOT x86start + cursor_at_open). Setting
	// armAsmPtr to the block-start address would double-count the cursor
	// offset and shift every armEmitJmp displacement / recorded block-entry
	// pointer by exactly s_mVUblockStartOffset bytes — landing the
	// dispatcher's Br x0 in the literal pool / alignment nops below the
	// real block.
	armAsmPtr = mVU.prog.x86start;
	// Mirrors the persistent MA's construction in mVUreset: capacity runs to
	// the physical rec-region end (mVUcacheSafeZone past the x86end reset
	// threshold) so overshooting sessions can finish emitting.
	armAsmCapacity = static_cast<size_t>(mVU.prog.x86end - mVU.prog.x86start) + (mVUcacheSafeZone * _1mb);
	armConstantPool = nullptr;
	armAsm = mVU.jitAsm.get();

	// Align this block's start to 16 bytes inside the persistent buffer.
	// With a single MA spanning the whole cache, the cursor must be walked
	// forward rather than aligning the buffer pointer. Cheap (≤ 3 nops).
	while (armAsm->GetCursorOffset() & 15)
		armAsm->Nop();
	s_mVUblockStartOffset = armAsm->GetCursorOffset();

	// Persisted-JIT recorder: everything emitted until the matching close is
	// one contiguous, relocatable-as-a-unit chunk. No-op unless recording is
	// enabled. (Nested opens returned above, so this attaches exactly once
	// per episode, on the thread that owns this VU's emission.)
	mVUPersist::BeginEpisode(mVU, armAsmPtr + s_mVUblockStartOffset);
}

static void mVUcloseCodeCache(microVU& mVU)
{
	if (!armAsm)
		return;

	// kFallThrough: emit any accumulated literal pool inline without a branch
	// over it. Subsequent blocks emit immediately after; safe because every
	// block ends with armEmitJmp / Ret, so control never falls through.
	armAsm->FinalizeCode(vixl::aarch64::MacroAssembler::kFallThrough);

	const ptrdiff_t curOffset = armAsm->GetCursorOffset();
	const u32 codeSize = static_cast<u32>(curOffset - s_mVUblockStartOffset);
	// armAsmPtr is the MA's buffer base (= x86start) so the actual block-start
	// address is x86start + s_mVUblockStartOffset, NOT armAsmPtr.
	u8* codeStart = armAsmPtr + s_mVUblockStartOffset;

	// Persisted-JIT recorder: finalize (or drop) the chunk. Runs after
	// FinalizeCode so the captured bytes include any literal pool.
	mVUPersist::EndEpisode(mVU, codeStart + codeSize);

	armAsm = nullptr; // unbind; do not delete (persistent)
	HostSys::EndCodeWrite();
	if (codeSize > 0)
	{
		HostSys::FlushInstructionCache(codeStart, codeSize);
	}

	mVU.prog.x86ptr = codeStart + codeSize;
}

//------------------------------------------------------------------
// mVUcompileJIT — Called by JR/JALR at runtime
//------------------------------------------------------------------

_mVUt void* mVUcompileJIT(u32 startPC, uptr ptr)
{
	microVU& mVU = mVUx;

	if (doJumpAsSameProgram)
	{
		if (doJumpCaching)
		{
			microBlock* pBlock = (microBlock*)ptr;
			microJumpCache& jc = pBlock->jumpCache[startPC / 8];
			if (jc.prog && jc.prog == mVU.prog.quick[startPC / 8].prog)
				return jc.hostEntry;

			mVUopenCodeCache(mVU);
			void* v = mVUblockFetch(mVU, startPC, (uptr)&pBlock->pStateEnd);
			mVUcloseCodeCache(mVU);

			jc.prog = mVU.prog.quick[startPC / 8].prog;
			jc.x86ptrStart = v;
			jc.hostEntry = v;
			return v;
		}

		mVUopenCodeCache(mVU);
		void* v = mVUblockFetch(mVU, startPC, ptr);
		mVUcloseCodeCache(mVU);
		return v;
	}

	mVU.regs().start_pc = startPC;
	if (doJumpCaching)
	{
		microBlock* pBlock = (microBlock*)ptr;
		microJumpCache& jc = pBlock->jumpCache[startPC / 8];
		if (jc.prog && jc.prog == mVU.prog.quick[startPC / 8].prog)
			return jc.hostEntry;

		mVUopenCodeCache(mVU);
		void* v = mVUsearchProg<vuIndex>(startPC, (uptr)&pBlock->pStateEnd);
		mVUcloseCodeCache(mVU);

		jc.prog = mVU.prog.quick[startPC / 8].prog;
		jc.x86ptrStart = v;
		jc.hostEntry = v;
		return v;
	}
	else
	{
		mVUopenCodeCache(mVU);
		void* v = mVUsearchProg<vuIndex>(startPC, ptr);
		mVUcloseCodeCache(mVU);
		return v;
	}
}

//------------------------------------------------------------------
// normBranch — Unconditional branch (B/BAL)
//------------------------------------------------------------------

void normBranch(mV, microFlagCycles& mFC)
{
	if (mVUup.dBit && doDBitHandling)
	{
		mVU.regAlloc->flushAll(false);
		u32 tempPC = iPC;

		a64::Label noDBit;
		armMoveAddressToReg(a64::x8, (mVU.index && THREAD_VU1) ?
			(void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Tst(a64::w9, isVU1 ? 0x400 : 0x4);
		armAsm->B(&noDBit, a64::eq);

		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x200 : 0x2);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

			armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
			armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		}
		iPC = branchAddr(mVU) / 4;
		mVUDTendProgram(mVU, &mFC, 1);

		armAsm->Bind(&noDBit);
		iPC = tempPC;
	}
	if (mVUup.tBit)
	{
		mVU.regAlloc->flushAll(false);
		u32 tempPC = iPC;

		a64::Label noTBit;
		armMoveAddressToReg(a64::x8, (mVU.index && THREAD_VU1) ?
			(void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Tst(a64::w9, isVU1 ? 0x800 : 0x8);
		armAsm->B(&noTBit, a64::eq);

		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x400 : 0x4);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

			armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
			armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		}
		iPC = branchAddr(mVU) / 4;
		mVUDTendProgram(mVU, &mFC, 1);

		armAsm->Bind(&noTBit);
		iPC = tempPC;
	}
	if (mVUup.mBit)
	{
		DevCon.Warning("M-Bit on normal branch, report if broken");
		u32 tempPC = iPC;

		memcpy(&mVUpBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));
		armMoveAddressToReg(a64::x0, &mVUpBlock->pStateEnd);
		armEmitCall(mVU.copyPLState);

		mVUsetupBranch(mVU, mFC);
		mVUendProgram(mVU, &mFC, 3);
		iPC = branchAddr(mVU) / 4;
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
		armAsm->Mov(a64::w9, xPC);
		armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUEBit);
		armEmitJmp(mVU.exitFunct);
		iPC = tempPC;
	}
	if (mVUup.eBit)
	{
		if (mVUlow.badBranch)
			DevCon.Warning("End on evil Unconditional branch! - Not implemented!");

		iPC = branchAddr(mVU) / 4;
		mVUendProgram(mVU, &mFC, 1);
		return;
	}

	// Normal unconditional branch
	mVUsetupBranch(mVU, mFC);
	normBranchCompile(mVU, branchAddr(mVU));
}

//------------------------------------------------------------------
// condBranch — Conditional branch (IBEQ/IBNE/IBGEZ/IBGTZ/IBLEZ/IBLTZ)
//------------------------------------------------------------------

void condBranch(mV, microFlagCycles& mFC, a64::Condition cond)
{
	mVUsetupBranch(mVU, mFC);

	// T-bit, D-bit, M-bit conditional branches — match x86 condBranch(). Each
	// branch bit tests the relevant FBRST flag (T/D), then if set raises INTC,
	// ends the program, and exits to either the taken or not-taken target based
	// on the branch condition.

	if (mVUup.tBit)
	{
		DevCon.Warning("T-Bit on branch, please report if broken");
		u32 tempPC = iPC;

		a64::Label noTBit;
		armMoveAddressToReg(a64::x8, (mVU.index && THREAD_VU1) ?
			(void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Tst(a64::w9, isVU1 ? 0x800 : 0x8);
		armAsm->B(&noTBit, a64::eq);

		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x400 : 0x4);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

			armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
			armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		}
		mVUDTendProgram(mVU, &mFC, 2);
		armAsm->Ldrsh(a64::w9, mVUfieldMem(mVU, &mVU.branch));
		armAsm->Cmp(a64::w9, 0);

		// DELIBERATE divergence from x86 (which wraps JMPcc in xInvertCond
		// here): the direct condition is the semantically correct one — taken
		// branch resumes at the branch target, matching the interpreter and
		// x86's own game-validated E-bit site. x86's T/D invert is an
		// unvalidated leftover of the 2015 crossed-label fix (2f20e6da6).
		// Pinned by Vu0SpecialBits.TBitOnConditionalBranch* (AX-01).
		a64::Label tJMP;
		armAsm->B(&tJMP, cond);
			// Not taken: set TPC to PC after the delay slot
			incPC(4);
			armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
			armAsm->Mov(a64::w9, xPC);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
			if (mVU.index && THREAD_VU1)
				armEmitCall((void*)mVUTBit);
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&tJMP);
		incPC(-4);
		iPC = branchAddr(mVU) / 4;
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
		armAsm->Mov(a64::w9, xPC);
		armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUTBit);
		armEmitJmp(mVU.exitFunct);

		armAsm->Bind(&noTBit);
		iPC = tempPC;
	}

	if (mVUup.dBit && doDBitHandling)
	{
		u32 tempPC = iPC;

		a64::Label noDBit;
		armMoveAddressToReg(a64::x8, (mVU.index && THREAD_VU1) ?
			(void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Tst(a64::w9, isVU1 ? 0x400 : 0x4);
		armAsm->B(&noDBit, a64::eq);

		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x200 : 0x2);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

			armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
			armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		}
		mVUDTendProgram(mVU, &mFC, 2);
		armAsm->Ldrsh(a64::w9, mVUfieldMem(mVU, &mVU.branch));
		armAsm->Cmp(a64::w9, 0);

		// Direct cond on purpose — see the T-bit stub above (AX-01).
		a64::Label dJMP;
		armAsm->B(&dJMP, cond);
			incPC(4);
			armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
			armAsm->Mov(a64::w9, xPC);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&dJMP);
		incPC(-4);
		iPC = branchAddr(mVU) / 4;
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
		armAsm->Mov(a64::w9, xPC);
		armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
		armEmitJmp(mVU.exitFunct);

		armAsm->Bind(&noDBit);
		iPC = tempPC;
	}

	if (mVUup.mBit)
	{
		u32 tempPC = iPC;

		memcpy(&mVUpBlock->pStateEnd, &mVUregs, sizeof(microRegInfo));
		armMoveAddressToReg(a64::x0, &mVUpBlock->pStateEnd);
		armEmitCall(mVU.copyPLState);

		mVUendProgram(mVU, &mFC, 3);
		armAsm->Ldrsh(a64::w9, mVUfieldMem(mVU, &mVU.branch));
		armAsm->Cmp(a64::w9, 0);

		a64::Label mJMP;
		// x86 emits ForwardJump with (JccComparisonType)JMPcc — the TAKEN
		// branch skips forward, so the inline path is NOT TAKEN. Match.
		armAsm->B(&mJMP, cond);
			incPC(4);
			armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
			armAsm->Mov(a64::w9, xPC);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
			if (mVU.index && THREAD_VU1)
				armEmitCall((void*)mVUEBit);
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&mJMP);
		incPC(-4);
		iPC = branchAddr(mVU) / 4;
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
		armAsm->Mov(a64::w9, xPC);
		armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUEBit);
		armEmitJmp(mVU.exitFunct);

		iPC = tempPC;
	}

	if (mVUup.eBit)
	{
		if (mVUlow.evilBranch)
			DevCon.Warning("End on evil branch! - Not implemented!");

		mVUendProgram(mVU, &mFC, 2);

		// Test branch condition. `mVU.branch` holds the signed comparison
		// value (from IBLEZ/IBLTZ etc.) or XOR-result (IBEQ/IBNE). The x86
		// path uses a 16-bit memory cmp (xCMP ptr16[&mVU.branch], 0) which
		// treats the value as signed s16. Match that here with Ldrsh so
		// negative VI values (e.g. 0xFFFE = -2) trigger .le/.lt correctly.
		armAsm->Ldrsh(a64::w9, mVUfieldMem(mVU, &mVU.branch));
		armAsm->Cmp(a64::w9, 0);

		a64::Label taken;
		incPC(3);
		armAsm->B(&taken, cond);
			// Not taken: set TPC to next instruction
			incPC(1);
			armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
			armAsm->Mov(a64::w9, xPC);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
			if (mVU.index && THREAD_VU1)
				armEmitCall((void*)mVUEBit);
			armEmitJmp(mVU.exitFunct);
		armAsm->Bind(&taken);
		incPC(-4);

		// Taken: set TPC to branch target
		iPC = branchAddr(mVU) / 4;
		armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI));
		armAsm->Mov(a64::w9, xPC);
		armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUEBit);
		armEmitJmp(mVU.exitFunct);
		return;
	}

	// Normal conditional branch. See E-bit path above for why Ldrsh.
	armAsm->Ldrsh(a64::w9, mVUfieldMem(mVU, &mVU.branch));
	armAsm->Cmp(a64::w9, 0);

	incPC(3);
	// Try to find cached block for not-taken path
	microBlock* bBlock;
	incPC2(1);
	blockCreate(iPC / 2);
	bBlock = mVUblocks[iPC / 2]->search(mVU, (microRegInfo*)&mVUregs);
	incPC2(-1);

	if (bBlock)
	{
		// Not-taken block exists: emit conditional jump to it
		a64::Condition invCond = a64::InvertCondition(cond);
		armEmitCondBranch(invCond, bBlock->hostEntry);
		incPC(-3);
		normBranchCompile(mVU, branchAddr(mVU));
	}
	else
	{
		// Neither path compiled yet: compile not-taken, then patch taken
		a64::Label takenLabel;
		armAsm->B(&takenLabel, cond);

		u32 bPC = iPC;
		microRegInfo regBackup;
		memcpy(&regBackup, &mVUregs, sizeof(microRegInfo));

		incPC2(1);
		mVUcompile(mVU, xPC, (uptr)&mVUregs);

		iPC = bPC;
		incPC(-3);
		void* jumpAddr = mVUblockFetch(mVU, branchAddr(mVU), (uptr)&regBackup);
		armAsm->Bind(&takenLabel);
		if (jumpAddr)
			armEmitJmp(jumpAddr);
		else
			armEmitJmp(mVU.exitFunct); // Safety: exit if compile failed
	}
}

//------------------------------------------------------------------
// normJump — Indirect jump (JR/JALR)
//------------------------------------------------------------------

void normJump(mV, microFlagCycles& mFC)
{
	if (mVUup.mBit)
		DevCon.Warning("M-Bit on Jump! Please report if broken");

	if (mVUlow.constJump.isValid)
	{
		if (mVUup.eBit)
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
		mVU.regAlloc->flushAll(false);
		a64::Label noDBit;
		armMoveAddressToReg(a64::x8, (THREAD_VU1) ?
			(void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Tst(a64::w9, isVU1 ? 0x400 : 0x4);
		armAsm->B(&noDBit, a64::eq);

		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x200 : 0x2);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

			armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
			armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		}
		mVUDTendProgram(mVU, &mFC, 2);
		mVUldrField(mVU, gprT1, &mVU.branch);
		armAsm->Str(gprT1, mVUstateMem(offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI)));
		armEmitJmp(mVU.exitFunct);

		armAsm->Bind(&noDBit);
	}
	if (mVUup.tBit)
	{
		mVU.regAlloc->flushAll(false);
		a64::Label noTBit;
		armMoveAddressToReg(a64::x8, (mVU.index && THREAD_VU1) ?
			(void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Tst(a64::w9, isVU1 ? 0x800 : 0x8);
		armAsm->B(&noTBit, a64::eq);

		if (!mVU.index || !THREAD_VU1)
		{
			armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
			armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x400 : 0x4);
			armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

			armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
			armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		}
		mVUDTendProgram(mVU, &mFC, 2);
		mVUldrField(mVU, gprT1, &mVU.branch);
		armAsm->Str(gprT1, mVUstateMem(offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI)));
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUTBit);
		armEmitJmp(mVU.exitFunct);

		armAsm->Bind(&noTBit);
	}
	if (mVUup.eBit)
	{
		mVUendProgram(mVU, &mFC, 2);
		mVUldrField(mVU, gprT1, &mVU.branch);
		armAsm->Str(gprT1, mVUstateMem(offsetof(VURegs, VI) + REG_TPC * sizeof(REG_VI)));
		if (mVU.index && THREAD_VU1)
			armEmitCall((void*)mVUEBit);
		armEmitJmp(mVU.exitFunct);
	}
	else
	{
		normJumpCompile(mVU, mFC, false);
	}
}

// Division flag transfer to status flag (ported from x86 mVUdivSet).
// If this instruction doesn't otherwise write the status flag, seed the write
// instance from lastWrite. Then clear the D/I division bits and OR in the
// latest division flags captured via mVU.divFlag.
static void mVUdivSet(mV)
{
	if (mVUinfo.doDivFlag)
	{
		const a64::Register& sReg = getFlagReg(sFLAG.write);
		if (!sFLAG.doFlag)
			armAsm->Mov(sReg, getFlagReg(sFLAG.lastWrite));
		// Clear D/I bits (18:19) before OR'ing new flags. AArch64 BIC immediate
		// accepts 0x000C0000 (two consecutive 1s, ROR 14 in 32-bit element) as
		// a valid logical-immediate, so this collapses Mov + And → single Bic.
		armAsm->Bic(sReg.W(), sReg.W(), 0x000C0000u);
		mVUldrField(mVU, gprT1, &mVU.divFlag);
		armAsm->Orr(sReg.W(), sReg.W(), gprT1);
	}
}

//------------------------------------------------------------------
// XGKICK Support
//------------------------------------------------------------------

// C helper: perform the GIF transfer (called at runtime)
void mVU_XGKICK_(u32 addr)
{
	addr = (addr & 0x3ff) * 16;
	u32 diff = 0x4000 - addr;
	u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, vuRegs[1].Mem, addr, ~0u, true);

	if (size > diff)
	{
		gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&vuRegs[1].Mem[addr], diff, true);
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[0], size - diff, true);
	}
	else
	{
		gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[addr], size, true);
	}
}

// C helper: cycle-counted XGKICK transfer
void _vuXGKICKTransfermVU(bool flush)
{
	while (VU1.xgkickenable && (flush || VU1.xgkickcyclecount >= 2))
	{
		u32 transfersize = 0;

		if (VU1.xgkicksizeremaining == 0)
		{
			u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, vuRegs[1].Mem, VU1.xgkickaddr, ~0u, flush);
			VU1.xgkicksizeremaining = size & 0xFFFF;
			VU1.xgkickendpacket = size >> 31;
			VU1.xgkickdiff = 0x4000 - VU1.xgkickaddr;

			if (VU1.xgkicksizeremaining == 0)
			{
				VU1.xgkickenable = false;
				break;
			}
		}

		if (!flush)
		{
			transfersize = std::min(VU1.xgkicksizeremaining, VU1.xgkickcyclecount * 8);
			transfersize = std::min(transfersize, VU1.xgkickdiff);
		}
		else
		{
			transfersize = VU1.xgkicksizeremaining;
			transfersize = std::min(transfersize, VU1.xgkickdiff);
		}

		if (THREAD_VU1)
		{
			if (transfersize < VU1.xgkicksizeremaining)
				gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&VU1.Mem[VU1.xgkickaddr], transfersize, true);
			else
				gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[VU1.xgkickaddr], transfersize, true);
		}
		else
		{
			gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &vuRegs[1].Mem[VU1.xgkickaddr], transfersize, true);
		}

		if (flush)
			VU1.cycle += transfersize / 8;

		VU1.xgkickcyclecount -= transfersize / 8;
		VU1.xgkickaddr = (VU1.xgkickaddr + transfersize) & 0x3FFF;
		VU1.xgkicksizeremaining -= transfersize;
		VU1.xgkickdiff = 0x4000 - VU1.xgkickaddr;

		if (VU1.xgkickendpacket && !VU1.xgkicksizeremaining)
			VU1.xgkickenable = false;
	}
}

// JIT emitter: emit code to call mVU_XGKICK_ at runtime
static __fi void mVU_XGKICK_DELAY(mV)
{
	mVU.regAlloc->flushCallerSavedRegisters();
	mVUbackupRegs(mVU, true, true);

	// Load VIxgkick value as argument and call the C helper
	mVUldrField(mVU, RWARG1, &mVU.VIxgkick);
	armEmitCall((void*)mVU_XGKICK_);

	mVUrestoreRegs(mVU, true, true);
}

// JIT emitter: emit code for cycle-counted XGKICK sync
static __fi void mVU_XGKICK_SYNC(mV, bool flush)
{
	// Full flush, NOT flushCallerSavedRegisters(): every store emitted here
	// must sit BEFORE the runtime xgkickenable / cyclecount branches below,
	// because the allocator marks the regs clean at compile time
	// unconditionally. Our GPR pool contains CALLEE-SAVED registers (unlike
	// x86's all-caller-saved pool), and a dirty callee-saved VI would
	// otherwise only be spilled by the mVUbackupRegs inside the conditional
	// path — skipped at runtime whenever no kick is pending, silently losing
	// the write (Crash Twinsanity: pc0x0's JALR link vi15 and the terminal
	// handler's vi12=0 died in w26 => VIF1 VEW deadlock; pinned by
	// Vu1Xgkick.XgKickHackSyncMustNotLoseCalleeSavedViWrites).
	mVU.regAlloc->flushAll();

	// Test if xgkickenable is set
	a64::Label skipxgkick;
	armMoveAddressToReg(a64::x8, &VU1.xgkickenable);
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
	armAsm->Tst(a64::w9, 1);
	armAsm->B(&skipxgkick, a64::eq);

	// Add kick cycles
	armMoveAddressToReg(a64::x8, &VU1.xgkickcyclecount);
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
	armAsm->Add(a64::w9, a64::w9, mVUlow.kickcycles - 1);
	armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

	// Check if enough cycles accumulated
	a64::Label needcycles;
	armAsm->Cmp(a64::w9, 2);
	armAsm->B(&needcycles, a64::lt);

	mVUbackupRegs(mVU, true, true);
	armAsm->Mov(RWARG1, flush ? 1 : 0);
	armEmitCall((void*)_vuXGKICKTransfermVU);
	mVUrestoreRegs(mVU, true, true);

	armAsm->Bind(&needcycles);

	// Add the remaining 1 cycle
	armMoveAddressToReg(a64::x8, &VU1.xgkickcyclecount);
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
	armAsm->Add(a64::w9, a64::w9, 1);
	armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

	armAsm->Bind(&skipxgkick);
}
