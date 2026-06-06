// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU — emit-coupled compile-driver helpers (Phase 7, Tables/Compile
// big-bang). VIXL port of the emit half of pcsx2/x86/microVU_Compile.inl.
//
// The arch-neutral helpers from microVU_Compile.inl (mVUsetupRange/mVUincCycles/
// mVUsetCycles/mVUoptimizePipeState/cmpVFregs/startLoop/mVUinitFirstPass/
// mVUcheckBadOp/branchWarning/eBitPass1/eBitWarning/mVUcheckIsSame) are already in
// aVU.cpp (tasks 7.3/7.4). This file holds the pieces that emit VIXL:
//   * the per-instruction executors (doUpperOp/doLowerOp/doSwapOp/doIbit/
//     mVUexecuteInstruction);
//   * D/T-bit early-exit emitters (mVUDoDBit/mVUDoTBit);
//   * cycle-test early exit (mVUtestCycles);
//   * the register preloader (mvuPreloadRegisters) + debug/bad-op emitters.
//
// mVUcompile itself + the block entry points (mVUentryGet/mVUblockFetch/
// mVUcompileJIT) and the branch drivers (aVU_Branch.inl) are the cross-referencing
// core and come in the next slice. Until then mVUcompileEmitCheck (in aVU.cpp)
// odr-uses the static helpers here so their VIXL bodies compile.

//------------------------------------------------------------------
// Messages Called at Execution Time...
//------------------------------------------------------------------
static inline void mVUbadOp0  (u32 prog, u32 pc) { Console.Error("microVU0 Warning: Exiting... Block contains an illegal opcode. [%04x] [%03d]", pc, prog); }
static inline void mVUbadOp1  (u32 prog, u32 pc) { Console.Error("microVU1 Warning: Exiting... Block contains an illegal opcode. [%04x] [%03d]", pc, prog); }
static inline void mVUprintPC1(u32 pc) { Console.WriteLn("Block Start PC = 0x%04x", pc); }
static inline void mVUprintPC2(u32 pc) { Console.WriteLn("Block End PC   = 0x%04x", pc); }

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
			mvuLdr32(gprT1, &curI);
			mvuStr32(&mVU.getVI(REG_I), gprT1);
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

			mvuStrImm32(&mVU.getVI(REG_I), tempI, gprT1);
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
		const a64::VRegister t1 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg);
		const a64::VRegister t2 = mVU.regAlloc->allocReg();
		armAsm->Mov(t2.V16B(), t1.V16B()); // Backup VF reg
		mVU.regAlloc->clearNeeded(t1);

		mVUopL(mVU, 1);

		const a64::VRegister t3 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg, mVUlow.VF_write.reg, 0xf, false);
		armAsm->Eor(t2.V16B(), t2.V16B(), t3.V16B()); // Swap new and old values of the register
		armAsm->Eor(t3.V16B(), t3.V16B(), t2.V16B()); // Uses xor swap trick...
		armAsm->Eor(t2.V16B(), t2.V16B(), t3.V16B());
		mVU.regAlloc->clearNeeded(t3);

		incPC(1);
		doUpperOp(mVU);

		const a64::VRegister t4 = mVU.regAlloc->allocReg(-1, mVUlow.VF_write.reg, 0xf);
		armAsm->Mov(t4.V16B(), t2.V16B());
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

// Prints msg when exiting block early if 1st op was a bad opcode (Dawn of Mana Level 2)
__fi void handleBadOp(mV, int count)
{
#ifdef PCSX2_DEVBUILD
	if (mVUinfo.isBadOp)
	{
		mVUbackupRegs(mVU, true);
		armAsm->Mov(RWARG1, mVU.prog.cur->idx);
		armAsm->Mov(RWARG2, xPC);
		armEmitCall(reinterpret_cast<const void*>(isVU1 ? mVUbadOp1 : mVUbadOp0));
		mVUrestoreRegs(mVU, true);
	}
#endif
	(void)count;
}

// Prints Start/End PC of blocks executed, for debugging...
void mVUdebugPrintBlocks(microVU& mVU, bool isEndPC)
{
	if (mVUdebugNow)
	{
		mVUbackupRegs(mVU, true);
		armAsm->Mov(RWARG1, xPC);
		armEmitCall(reinterpret_cast<const void*>(isEndPC ? mVUprintPC2 : mVUprintPC1));
		mVUrestoreRegs(mVU, true);
	}
}

//------------------------------------------------------------------
// Cycle test / D-bit / T-bit early exits
//------------------------------------------------------------------

void mVUDoDBit(microVU& mVU, microFlagCycles* mFC)
{
	a64::Label eJMP;
	if (mVU.index && THREAD_VU1)
		mvuLdr32(gprT1, &vu1Thread.vuFBRST);
	else
		mvuLdr32(gprT1, &VU0.VI[REG_FBRST].UL);
	armAsm->Tst(gprT1, (isVU1 ? 0x400 : 0x4));
	armAsm->B(&eJMP, a64::eq);
	if (!isVU1 || !THREAD_VU1)
	{
		mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x200 : 0x2), gprT1);
		mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
	}
	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);
	armAsm->Bind(&eJMP);
}

void mVUDoTBit(microVU& mVU, microFlagCycles* mFC)
{
	a64::Label eJMP;
	if (mVU.index && THREAD_VU1)
		mvuLdr32(gprT1, &vu1Thread.vuFBRST);
	else
		mvuLdr32(gprT1, &VU0.VI[REG_FBRST].UL);
	armAsm->Tst(gprT1, (isVU1 ? 0x800 : 0x8));
	armAsm->B(&eJMP, a64::eq);
	if (!isVU1 || !THREAD_VU1)
	{
		mvuMemOrImm32(&VU0.VI[REG_VPU_STAT].UL, (isVU1 ? 0x400 : 0x4), gprT1);
		mvuMemOrImm32(&mVU.regs().flags, VUFLAG_INTCINTERRUPT, gprT1);
	}
	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);

	armAsm->Bind(&eJMP);
}

void mVUSaveFlags(microVU& mVU, microFlagCycles& mFC, microFlagCycles& mFCBackup)
{
	memcpy(&mFCBackup, &mFC, sizeof(microFlagCycles));
	mVUsetFlags(mVU, mFCBackup); // Sets Up Flag instances
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

	mvuLdr32(gprT1, &mVU.cycles);
	if (EmuConfig.Gamefixes.VUSyncHack)
		armAsm->Subs(gprT1.W(), gprT1.W(), mVUcycles); // Running behind, make sure we have time to run the block
	else
		armAsm->Subs(gprT1.W(), gprT1.W(), 1); // Running ahead, make sure cycles left are above 0

	a64::Label skip;
	armAsm->B(&skip, a64::pl); // jump if result >= 0 (x86 xForwardJNS32)

	armMoveAddressToReg(RXARG1, &mVUpBlock->pState);
	armEmitCall(mVU.copyPLState);

	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		mvuStrImm32(&mVU.regs().nextBlockCycles, mVUcycles, gprT1);
	mVUendProgram(mVU, &mFC, 0);

	armAsm->Bind(&skip);

	// xSUB(ptr32[&mVU.cycles], mVUcycles)
	armMoveAddressToReg(RSCRATCHADDR, &mVU.cycles);
	armAsm->Ldr(gprT1.W(), a64::MemOperand(RSCRATCHADDR));
	armAsm->Sub(gprT1.W(), gprT1.W(), mVUcycles);
	armAsm->Str(gprT1.W(), a64::MemOperand(RSCRATCHADDR));
}

//------------------------------------------------------------------
// Register preloader (front-loads VF/VI regs likely used in the block)
//------------------------------------------------------------------
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
