// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "common/FileSystem.h"
#include "common/Path.h"


//------------------------------------------------------------------
// Messages Called at Execution Time
//------------------------------------------------------------------

static inline void mVUbadOp0  (u32 prog, u32 pc) { Console.Error("microVU0 Warning: Bad opcode [%04x] [%03d]", pc, prog); }
static inline void mVUbadOp1  (u32 prog, u32 pc) { Console.Error("microVU1 Warning: Bad opcode [%04x] [%03d]", pc, prog); }

//------------------------------------------------------------------
// Program Range Checking
//------------------------------------------------------------------

__fi void mVUcheckIsSame(mV)
{
	if (mVU.prog.isSame == -1)
		mVU.prog.isSame = !memcmp((u8*)mVUcurProg.data, mVU.regs().Micro, mVU.microMemSize);
	if (mVU.prog.isSame == 0)
	{
		mVUcacheProg(mVU, *mVU.prog.cur);
		mVU.prog.isSame = 1;
	}
}

void mVUsetupRange(microVU& mVU, s32 pc, bool isStartPC)
{
	std::deque<microRange>*& ranges = mVUcurProg.ranges;
	if (pc > (s64)mVU.microMemSize)
	{
		Console.Error("microVU%d: PC outside of VU memory PC=0x%04x", mVU.index, pc);
		pxFail("microVU: PC out of VU memory");
	}

	const s32 cur_pc = (!isStartPC && mVUrange.start > pc && pc == 0) ? mVU.microMemSize : pc;

	if (isStartPC)
	{
		for (auto it = ranges->begin(); it != ranges->end(); ++it)
		{
			if ((cur_pc >= it->start) && (cur_pc <= it->end))
			{
				if (it->start != it->end)
				{
					microRange mRange = {it->start, it->end};
					ranges->erase(it);
					ranges->push_front(mRange);
					return;
				}
			}
		}
	}
	else if (mVUrange.end >= cur_pc)
		return;

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
			if (((it->start >= rStart) && (it->start <= rEnd)) ||
				((it->end >= rStart) && (it->end <= rEnd)))
			{
				mVUrange.start = rStart = std::min(it->start, rStart);
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
		microRange mRange = {0, cur_pc};
		ranges->push_front(mRange);
	}

	if (!doWholeProgCompare)
		mVUcacheProg(mVU, *mVU.prog.cur);
}

//------------------------------------------------------------------
// Pipeline State Helpers (platform-independent)
//------------------------------------------------------------------

__fi u8 optimizeReg(u8 rState) { return (rState == 1) ? 0 : rState; }
__fi u8 calcCycles(u8 reg, u8 x) { return ((reg > x) ? (reg - x) : 0); }
__fi u8 tCycles(u8 dest, u8 src) { return std::max(dest, src); }
__fi void incP(mV) { mVU.p ^= 1; }
__fi void incQ(mV) { mVU.q ^= 1; }

// Optimizes the end pipeline state — collapses cycles-remaining==1 to 0, since
// mVU's block loop auto-decrements at entry so 1 is equivalent to 0. Without
// this, pipeline-state-hashed blocks get distinct variants for every cycle
// delta, exploding the block cache. Ported verbatim from x86 microVU_Compile.inl.
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
	mVUregs.r = 0; // No stalls on R-reg — safe to discard.
}

// Advance pipeline cycles by x instructions. Ported verbatim from x86.
void mVUincCycles(mV, int x)
{
	mVUcycles += x;
	// VF[0] is a constant (0,0,0,1) — skip.
	for (int z = 31; z > 0; z--)
	{
		mVUregs.VF[z].x = calcCycles(mVUregs.VF[z].x, x);
		mVUregs.VF[z].y = calcCycles(mVUregs.VF[z].y, x);
		mVUregs.VF[z].z = calcCycles(mVUregs.VF[z].z, x);
		mVUregs.VF[z].w = calcCycles(mVUregs.VF[z].w, x);
	}
	// VI[0] is constant (0) — skip.
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
				mVUinfo.doDivFlag = 1;
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

// Helper: set xVar to 1 if VFreg1 and VFreg2 reference the same VF reg and
// any of the X/Y/Z/W components are touched by both. Ported from x86.
static __fi void cmpVFregs(microVFreg& VFreg1, microVFreg& VFreg2, bool& xVar)
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

	// If upper Op && lower Op write to same VF reg: either make Lower skip its
	// VF write (noWriteVF) or mark it a NOP entirely when Lower has no other
	// side effects.
	if ((mVUregsTemp.VFreg[0] == mVUregsTemp.VFreg[1]) && mVUregsTemp.VFreg[0])
	{
		if (mVUregsTemp.r || mVUregsTemp.VI)
			mVUlow.noWriteVF = true;
		else
			mVUlow.isNOP = true;
	}

	// If Lower reads a VF reg that Upper writes, Upper's semantic output must
	// be visible to Lower → run Lower first (swapOps).
	if ((mVUlow.VF_read[0].reg || mVUlow.VF_read[1].reg) && mVUup.VF_write.reg)
	{
		cmpVFregs(mVUup.VF_write, mVUlow.VF_read[0], mVUinfo.swapOps);
		cmpVFregs(mVUup.VF_write, mVUlow.VF_read[1], mVUinfo.swapOps);
	}

	// If swapOps is set AND Upper also reads a VF reg that Lower writes,
	// snapshot the VF reg before Lower runs so Upper sees pre-Lower
	// state (backupVF).
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

	mVUregs.VI[mVUregsTemp.VIreg]      = tCycles(mVUregs.VI[mVUregsTemp.VIreg], mVUregsTemp.VI);

	mVUregs.q      = tCycles(mVUregs.q,      mVUregsTemp.q);
	mVUregs.p      = tCycles(mVUregs.p,      mVUregsTemp.p);
	mVUregs.r      = tCycles(mVUregs.r,      mVUregsTemp.r);
	mVUregs.xgkick = tCycles(mVUregs.xgkick, mVUregsTemp.xgkick);
	memset(&mVUregsTemp, 0, sizeof(mVUregsTemp));
}

//------------------------------------------------------------------
// Flag-Pass Analysis (ported from x86 microVU_Flags.inl)
//------------------------------------------------------------------
// Scans forward through instructions to determine which pipeline flags
// (sFlag/mFlag/cFlag) the next block reads in its first ~4 instructions.
// Sets mVUregs.needExactMatch bits (1/2/4) so block lookup can require
// an exact pipeline-state match for correctness.

#define shortBranchPass() \
	{ \
		if ((branch == 3) || (branch == 4)) /* Branches */ \
		{ \
			_mVUflagPass(mVU, aBranchAddr, sCount + found, found, v); \
			if (branch == 3) /* Non-conditional Branch */ \
				break; \
			branch = 0; \
		} \
		else if (branch == 5) /* JR/JARL */ \
		{ \
			if (sCount + found < 4) \
				mVUregs.needExactMatch |= 7; \
			break; \
		} \
		else /* E-Bit End */ \
			break; \
	}

// Scan instructions at startPC and check if they read any pipeline flags.
// Uses pass4 (recPass=3) on each Upper/Lower op to accumulate needExactMatch bits.
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
			shortBranchPass();
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

// Checks if the first ~4 instructions of the successor block(s) read flags,
// and sets needExactMatch bits accordingly so block lookup requires exact state.
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

//------------------------------------------------------------------
// Cycle Test (emits code to check remaining cycles)
//------------------------------------------------------------------

// Test remaining cycles; if insufficient, save block state via copyPLState +
// mVUendProgram(0) and exit to the dispatcher. Otherwise deduct cycles and
// continue into the block. Ported from x86 microVU_Compile.inl:449.
// The copyPLState + mVUendProgram(0) on early-exit is required so that
// a cycle-timeout block has its pipeline state saved; without it, the block
// manager would see stale pState on re-entry and create a new variant.
static void mVUtestCycles(mV, microFlagCycles& mFC)
{
	iPC = mVUstartPC;

	if (isVU0 && EmuConfig.Speedhacks.EECycleRate != 0 && (!EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Speedhacks.EECycleRate < 0))
	{
		switch (std::min(static_cast<int>(EmuConfig.Speedhacks.EECycleRate), static_cast<int>(mVUcycles)))
		{
			case -3: mVUcycles *= 2.0f;       break;
			case -2: mVUcycles *= 1.6666667f; break;
			case -1: mVUcycles *= 1.3333333f; break;
			case  1: mVUcycles /= 1.3f;       break;
			case  2: mVUcycles /= 1.8f;       break;
			case  3: mVUcycles /= 3.0f;       break;
			default: break;
		}
	}

	armMoveAddressToReg(a64::x8, &mVU.cycles);
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
	if (EmuConfig.Gamefixes.VUSyncHack)
		armAsm->Subs(a64::w9, a64::w9, mVUcycles);
	else
		armAsm->Subs(a64::w9, a64::w9, 1);

	// If (cycles - check) is non-negative, there is budget — skip the early exit.
	a64::Label skip;
	armAsm->B(&skip, a64::pl); // pl = N clear = non-negative

	// Early exit path: save pipeline state then exit via mVUendProgram(0).
	armMoveAddressToReg(a64::x0, &mVUpBlock->pState);
	armEmitCall(mVU.copyPLState);
	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
	{
		armAsm->Mov(a64::w9, mVUcycles);
		armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, nextBlockCycles)));
	}
	mVUendProgram(mVU, &mFC, 0);

	armAsm->Bind(&skip);

	// Budget remains — deduct block cycles from mVU.cycles and fall through
	// into the block body. x8 still holds &mVU.cycles from the first
	// materialization above; the early-exit path that clobbers it tail-calls
	// mVUendProgram and never reaches here, so x8 is safe to reuse directly.
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
	armAsm->Sub(a64::w9, a64::w9, mVUcycles);
	armAsm->Str(a64::w9, a64::MemOperand(a64::x8));
}

//------------------------------------------------------------------
// Execute VU Instruction (Upper + Lower)
//------------------------------------------------------------------

// Pre-populate NEON/GPR caches with VF/VI registers the next few ops will
// read. Ported from pcsx2/x86/microVU_Compile.inl:603-690. Runs once at
// the start of pass 2; iterates forward through mVUinfo until caches are
// nearly full, or an XGKICK / branch is encountered. Purely an
// optimization — skips pre-loaded regs on allocReg so subsequent ops
// reuse the cached data instead of re-loading from memory.
static void mvuPreloadRegisters(microVU& mVU, u32 endCount)
{
	static constexpr const int REQUIRED_FREE_NEON = 3;
	static constexpr const int REQUIRED_FREE_GPRS = 1;

	u32 vfs_loaded = 0;
	u32 vis_loaded = 0;

	for (int reg = 0; reg < mVU.regAlloc->getNeonCount(); reg++)
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
	int free_regs = mVU.regAlloc->getFreeNeonCount();
	int free_gprs = mVU.regAlloc->getFreeGPRCount();

	auto preloadVF = [&mVU, &vfs_loaded, &free_regs](u8 reg)
	{
		if (free_regs <= REQUIRED_FREE_NEON || reg == 0 || (vfs_loaded & (1u << reg)) != 0)
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
		return (free_regs >= REQUIRED_FREE_NEON || free_gprs >= REQUIRED_FREE_GPRS);
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
			preloadVF(uvfr.reg);

		const microVFreg& lvfr = info->lOp.VF_write;
		if (lvfr.reg != 0 && (!lvfr.x || !lvfr.y || !lvfr.z || !lvfr.w))
			preloadVF(lvfr.reg);

		if (info->lOp.branch)
			break;
	}

	iPC = orig_pc;
	mVU.code = orig_code;
}

__ri void doUpperOp(mV) { mVUopU(mVU, 1); }
__ri void doLowerOp(mV) { incPC(-1); mVUopL(mVU, 1); incPC(1); }
__ri void flushRegs(mV) { if (!doRegAlloc) mVU.regAlloc->flushAll(); }

void doIbit(mV)
{
	if (mVUup.iBit)
	{
		incPC(-1);
		u32 tempI = curI;
		if (CHECK_VU_OVERFLOW(mVU.index) && ((curI & 0x7fffffff) >= 0x7f800000))
			tempI = (0x80000000 & curI) | 0x7f7fffff;

		armAsm->Mov(a64::w9, tempI);
		armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, VI) + REG_I * sizeof(REG_VI)));
		incPC(1);
	}
}

// Ported from x86 microVU_Compile.inl:doSwapOp — runs Lower before Upper, and
// when Upper reads a VF reg Lower writes, snapshots the pre-Lower VF value via
// an XOR-swap so Upper observes the original value.
static void doSwapOp(mV)
{
	if (mVUinfo.backupVF && !mVUlow.noWriteVF)
	{
		DevCon.WriteLn(Color_Green, "microVU%d: Backing Up VF Reg [%04x]", getIndex, xPC);

		// Alloc t1 = current value of Lower's VF_write reg (pre-Lower).
		const a64::VRegister t1 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg);
		const a64::VRegister t2 = mVU.regAlloc->allocReg();
		armAsm->Mov(t2.V16B(), t1.V16B()); // t2 = pre-Lower value
		mVU.regAlloc->clearNeeded(t1);

		mVUopL(mVU, 1); // Lower writes new value to VF_write

		// XOR-swap: t2 gets new value, VF_write reg (via t3) gets old value,
		// so Upper sees the pre-Lower state.
		const a64::VRegister t3 = mVU.regAlloc->allocReg(mVUlow.VF_write.reg, mVUlow.VF_write.reg, 0xf, false);
		armAsm->Eor(t2.V16B(), t2.V16B(), t3.V16B());
		armAsm->Eor(t3.V16B(), t3.V16B(), t2.V16B());
		armAsm->Eor(t2.V16B(), t2.V16B(), t3.V16B());
		mVU.regAlloc->clearNeeded(t3);

		incPC(1);
		doUpperOp(mVU); // Upper reads VF_write reg with old value

		// Write the new value (held in t2) back to VF_write reg.
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

// Runtime D-bit handler: if VU0/VU1 FBRST has the D-interrupt bit set, raise
// VPU_STAT and INTCINTERRUPT flags, end the program, otherwise fall through.
// Mirrors x86 microVU_Compile.inl:560-576.
static void mVUDoDBit(microVU& mVU, microFlagCycles* mFC)
{
	// Flush regalloc before the conditional skip — mVUDTendProgram's internal
	// flushAll emits the stores INSIDE the branch, so the silent path would
	// otherwise drop dirty regs (the lower op of the D-bit pair). Same pattern
	// as the branch-side D-bit handler in microVU_Branch-arm64.inl:540.
	mVU.regAlloc->flushAll(false);

	a64::Label noDBit;
	armMoveAddressToReg(a64::x8, (mVU.index && THREAD_VU1)
		? (void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
	armAsm->Tst(a64::w9, isVU1 ? 0x400 : 0x4);
	armAsm->B(&noDBit, a64::eq);

	if (!isVU1 || !THREAD_VU1)
	{
		armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x200 : 0x2);
		armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

		armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
		armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
	}

	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);

	armAsm->Bind(&noDBit);
}

// Runtime T-bit handler. Same pattern as mVUDoDBit but tests the T bit.
// Mirrors x86 microVU_Compile.inl:578-595.
static void mVUDoTBit(microVU& mVU, microFlagCycles* mFC)
{
	// Flush regalloc before the conditional skip — mVUDTendProgram's internal
	// flushAll emits the stores INSIDE the branch, so the silent path would
	// otherwise drop dirty regs (the lower op of the T-bit pair). Same pattern
	// as the branch-side T-bit handler in microVU_Branch-arm64.inl:569.
	mVU.regAlloc->flushAll(false);

	a64::Label noTBit;
	armMoveAddressToReg(a64::x8, (mVU.index && THREAD_VU1)
		? (void*)&vu1Thread.vuFBRST : (void*)&VU0.VI[REG_FBRST].UL);
	armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
	armAsm->Tst(a64::w9, isVU1 ? 0x800 : 0x8);
	armAsm->B(&noTBit, a64::eq);

	if (!isVU1 || !THREAD_VU1)
	{
		armMoveAddressToReg(a64::x8, &VU0.VI[REG_VPU_STAT].UL);
		armAsm->Ldr(a64::w9, a64::MemOperand(a64::x8));
		armAsm->Orr(a64::w9, a64::w9, isVU1 ? 0x400 : 0x4);
		armAsm->Str(a64::w9, a64::MemOperand(a64::x8));

		armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		armAsm->Orr(a64::w9, a64::w9, VUFLAG_INTCINTERRUPT);
		armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
	}

	incPC(1);
	mVUDTendProgram(mVU, mFC, 1);
	incPC(-1);

	armAsm->Bind(&noTBit);
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
// Init helpers
//------------------------------------------------------------------

__fi void startLoop(mV)
{
	memset(&mVUinfo, 0, sizeof(mVUinfo));
	memset(&mVUregsTemp, 0, sizeof(mVUregsTemp));
}

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

__fi void mVUinitFirstPass(mV, uptr pState, u8* thisPtr)
{
	mVUstartPC = iPC;
	mVUbranch  = 0;
	mVUcount   = 0;
	mVUcycles  = 0;
	mVU.p      = 0;
	mVU.q      = 0;

	if ((uptr)&mVUregs != pState)
		memcpy((u8*)&mVUregs, (u8*)pState, sizeof(microRegInfo));
	if ((uptr)&mVU.prog.lpState != pState)
		memcpy((u8*)&mVU.prog.lpState, (u8*)pState, sizeof(microRegInfo));

	mVUblock.x86ptrStart = thisPtr;
	// hostEntry mirrors x86ptrStart: the JIT stores its code-cache slot in both.
	// The indirection is kept so an alternate code source can repoint hostEntry
	// at a prepared block without disturbing the code-cache slot tracking.
	mVUblock.hostEntry = thisPtr;

	// Create block manager if needed, then add this block. Both conditions are
	// invariant violations — the caller (mVUcompile) unconditionally proceeds
	// into the first-pass loop and dereferences mVUpBlock, so a bare return
	// here would just defer a NULL/stale deref into UB. Fail fast instead.
	pxAssertRel(mVU.prog.cur, "microVU: mVUinitFirstPass with NULL mVU.prog.cur");
	blockCreate(mVUstartPC / 2);
	mVUpBlock = mVUblocks[mVUstartPC / 2]->add(mVU, &mVUblock);
	pxAssertRel(mVUpBlock, "microVU: mVUpBlock NULL after blockManager::add");
	// Register this block (manager copy + host entry) with the VU program-cache
	// recorder so the emitted code can be persisted and reloaded across runs.
	mVUPersist::OnBlockCompiled(mVU, mVUpBlock, thisPtr, mVUstartPC * 4);
	mVUregs.needExactMatch = (mVUpBlock->pState.blockType) ? 7 : 0;
	mVUregs.blockType = 0;
	mVUregs.viBackUp  = 0;
	mVUregs.flagInfo  = 0; // Must be cleared each compile: mVUsetFlags OR-updates
	                       // flagInfo at end of compile, so stale bits accumulate
	                       // across blocks, making every compile hash to a new
	                       // pipeline state and create a new variant.
	mVUsFlagHack = CHECK_VU_FLAGHACK;

	mVUinitConstValues(mVU);
}

__fi void mVUcheckBadOp(mV)
{
	if (mVUinfo.isBadOp && mVU.code != 0x8000033c)
	{
		mVUinfo.isEOB = true;
		DevCon.Warning("microVU Warning: Block contains an illegal opcode...");
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

__fi void branchWarning(mV)
{
	incPC(-2);
	if (mVUup.eBit && mVUbranch)
	{
		incPC(2);
		mVUlow.isNOP = true;
	}
	else
		incPC(2);

	if (mVUinfo.isBdelay && !mVUlow.evilBranch)
	{
		if (mVUlow.VI_write.reg && mVUlow.VI_write.used && !mVUlow.readFlags)
		{
			mVUlow.backupVI = true;
			mVUregs.viBackUp = mVUlow.VI_write.reg;
		}
	}
}

__ri void eBitWarning(mV)
{
	incPC(2);
	if (curI & _Ebit_)
		mVUregs.blockType = 1;
	incPC(-2);
}

void mVUdebugPrintBlocks(mV, bool isEndPC) {}

//------------------------------------------------------------------
// Main Compile Function
//------------------------------------------------------------------

void* mVUcompile(microVU& mVU, u32 startPC, uptr pState)
{
	microFlagCycles mFC;
	// armAsm is managed by mVUexecute/mVUcompileJIT — must be active here.
	pxAssert(armAsm);
	u8* thisPtr = armGetCurrentCodePointer();

	const u32 endCount = (((microRegInfo*)pState)->blockType) ? 1 : (mVU.microMemSize / 8);

	// === First Pass (Analysis) ===
	iPC = startPC / 4;
	mVUsetupRange(mVU, startPC, 1);
	mVU.regAlloc->reset(false);
	mVUinitFirstPass(mVU, pState, thisPtr);
	mVUbranch = 0;

	for (int branch = 0; mVUcount < endCount;)
	{
		incPC(1);
		startLoop(mVU);
		mVUincCycles(mVU, 1);
		mVUopU(mVU, 0); // Upper analysis
		mVUcheckBadOp(mVU);

		if (curI & _Ebit_)
		{
			eBitPass1(mVU, branch);
			// VU0 end of program MAC results can be read by COP2, so best to
			// make sure the last instance is valid. Needed for State of Emergency 2
			// and Driving Emotion Type-S (mirrors x86 microVU_Compile.inl:711-717).
			if (isVU0)
				mVUregs.needExactMatch |= 7;
		}

		// M-bit: VU0 sync point with EE. If the previous instruction was also
		// M-bit, skip — no need to re-sync. Mirrors x86 microVU_Compile.inl:720-735.
		if ((curI & _Mbit_) && isVU0)
		{
			if (xPC > 0)
			{
				incPC(-2);
				if (!(curI & _Mbit_))
				{
					incPC(2);
					mVUup.mBit = true;
				}
				else
				{
					incPC(2);
				}
			}
			else
			{
				mVUup.mBit = true;
			}
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
				// Ignore IADDI/IADDIU/ISUBU/ILW/ISW/LQ/SQ on the lower slot when
				// IbitHack is active. Matches x86 microVU_Compile.inl:751-765.
				const u32 upper = (mVU.code >> 25);
				if (upper == 0x1 || upper == 0x0 || upper == 0x4 || upper == 0x5
					|| upper == 0x8 || upper == 0x9
					|| (upper == 0x40 && (mVU.code & 0x3F) == 0x32))
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

		if (curI & _Dbit_) { mVUup.dBit = true; }
		if (curI & _Tbit_) { mVUup.tBit = true; }
		mVUsetCycles(mVU);

		if (!mVUlow.isKick)
		{
			mVUregs.xgkickcycles += 1 + mVUstall;
			if (mVUlow.isMemWrite) { mVUlow.kickcycles = mVUregs.xgkickcycles; mVUregs.xgkickcycles = 0; }
		}
		else
		{
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
				mVUinfo.isBdelay = true;
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

		if (mVUbranch) { mVUsetFlagInfo(mVU); eBitWarning(mVU); branch = 3; mVUbranch = 0; }

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

	mVUregs.vi15 = 0;
	mVUregs.vi15v = 0;
	mVUsetFlags(mVU, mFC);
	mVUoptimizePipeState(mVU);
	mVUtestCycles(mVU, mFC);

	// === Second Pass (Codegen) ===
	iPC = mVUstartPC;
	setCode();
	mVUbranch = 0;
	u32 x = 0;

	mvuPreloadRegisters(mVU, endCount);

	for (; x < endCount; x++)
	{
		if (mVUinfo.isEOB) { x = 0xffff; }

		// M-bit: signal the EE-visible M-flag so VU0 micro-mode can break/sync
		// to the EE (VU0.cpp gates the M-bit Break on VURegs.flags & MFLAGSET).
		// Mirrors x86 microVU_Compile.inl:890-893; VURegs.flags is always
		// memory-resident, so no regalloc flush is needed (same as x86's
		// direct memory xOR). Matches the VUFLAG_INTCINTERRUPT D/T-bit pattern.
		if (mVUup.mBit)
		{
			armAsm->Ldr(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
			armAsm->Orr(a64::w9, a64::w9, VUFLAG_MFLAGSET);
			armAsm->Str(a64::w9, mVUstateMem(offsetof(VURegs, flags)));
		}

		if (isVU1 && mVUlow.kickcycles && CHECK_XGKICKHACK)
			mVU_XGKICK_SYNC(mVU, false);

		mVUexecuteInstruction(mVU);

#ifdef PCSX2_RECOMPILER_TESTS
		// Per-op state-snapshot hook (test builds only). When enabled, flush all
		// dirty allocator state to vuRegs[N] memory and emit a brk whose imm16
		// encodes the op index; a test harness's SIGTRAP handler captures
		// vuRegs[N] for that index and skips the brk. Release builds emit no
		// per-op probe into the block.
		if (mvu_divtrace::g_enabled.load(std::memory_order_relaxed))
		{
			mvu_divtrace::OpMeta meta{};
			meta.op_idx     = static_cast<u16>(mvu_divtrace::g_meta.size());
			meta.microvu_pc = xPC;
			// Raw 64-bit microvu instruction (lower word + upper word) at xPC.
			std::memcpy(&meta.opcode, &mVU.regs().Micro[xPC], sizeof(u32));
			meta.host_lo = armGetCurrentCodePointer();
			meta.alloc   = mVU.regAlloc->snapshotMaps();

			mVU.regAlloc->flushAll(true);

			// Flush qmmPQ (host-resident Q/P pipeline) to vuRegs.VI[REG_Q]/[REG_P]
			// + pending_q/pending_p so vi22/vi23 are meaningful at the brk.
			// The current lane is not known at codegen-of-op-N (mVU.q is
			// the post-analyze final value, not the per-op value), so dump both:
			// VI[REG_Q] := qmmPQ[0], pending_q := qmmPQ[1]. The driver compares
			// JIT and interp Q as a multiset {VI[Q], pending_q} to tolerate the
			// JIT/interp lane-index disagreement.
			armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_Q * sizeof(REG_VI));
			armAsm->St1(qmmPQ.V4S(), 0, a64::MemOperand(a64::x8));
			armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, pending_q));
			armAsm->St1(qmmPQ.V4S(), 1, a64::MemOperand(a64::x8));
			if (isVU1)
			{
				armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, VI) + REG_P * sizeof(REG_VI));
				armAsm->St1(qmmPQ.V4S(), 2, a64::MemOperand(a64::x8));
				armAsm->Add(a64::x8, gprVUState, offsetof(VURegs, pending_p));
				armAsm->St1(qmmPQ.V4S(), 3, a64::MemOperand(a64::x8));
			}

			armAsm->Brk(meta.op_idx);
			meta.host_hi = armGetCurrentCodePointer();
			mvu_divtrace::g_meta.push_back(meta);
		}
#endif

		// T/D/M-bit per-instruction handling (excluding branch delay slots;
		// those are handled after the branch itself emits). Mirrors x86
		// microVU_Compile.inl:901-923.
		if (!mVUinfo.isBdelay && !mVUlow.branch)
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
				// Flags must be exact: Gungrave does FCAND/FMAND with M-bit
				// back-to-back. setupBranch sorts flag instances.
				mVUsetupBranch(mVU, mFC);
				// Emit a runtime snapshot of the current pipeline state into
				// mVU.prog.lpState. Matches x86's xMOV(ptr32[lpS], cpS[0])
				// loop: the values are compile-time constants (from mVUregs)
				// baked into the emitted store instructions.
				{
					const u32* cpS = reinterpret_cast<const u32*>(&mVUregs);
					const size_t nWords = (sizeof(microRegInfo) - 4) / 4;
					armMoveAddressToReg(a64::x8, &mVU.prog.lpState);
					for (size_t i = 0; i < nWords; i++)
					{
						armAsm->Mov(a64::w9, cpS[i]);
						armAsm->Str(a64::w9, a64::MemOperand(a64::x8, static_cast<s32>(i * 4)));
					}
				}
				incPC(2);
				mVUsetupRange(mVU, xPC, false);
				// VUSyncHack: clear nextBlockCycles (mVUendProgram only emits
				// this for the E-bit/isEbit!=0 paths, so the M-bit case must
				// store it explicitly). Mirrors x86 microVU_Compile.inl:926-927.
				if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
					armAsm->Str(a64::wzr, mVUstateMem(offsetof(VURegs, nextBlockCycles)));
				// endProgram + normBranchCompile run at iPC+2 so the saved TPC is
				// the continuation PC (not the already-executed M-bit instruction)
				// and the continuation block is compiled/linked into the cache.
				// incPC(-2) is deferred until after, matching x86:928-930.
				mVUendProgram(mVU, &mFC, 0);
				normBranchCompile(mVU, xPC);
				incPC(-2);
				goto perf_and_return;
			}
		}

		if (mVUinfo.doXGKICK)
			mVU_XGKICK_DELAY(mVU);

		if (isEvilBlock)
		{
			mVUsetupRange(mVU, xPC + 8, false);
			normJumpCompile(mVU, mFC, true);
			goto perf_and_return;
		}
		else if (!mVUinfo.isBdelay)
		{
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
			incPC(-4); // Go back to branch opcode

			switch (mVUlow.branch)
			{
				case 1: case 2: // B/BAL
					normBranch(mVU, mFC);
					goto perf_and_return;
				case 9: case 10: // JR/JALR
					normJump(mVU, mFC);
					goto perf_and_return;
				case 3: // IBEQ
					condBranch(mVU, mFC, a64::eq);
					goto perf_and_return;
				case 4: // IBGEZ
					condBranch(mVU, mFC, a64::ge);
					goto perf_and_return;
				case 5: // IBGTZ
					condBranch(mVU, mFC, a64::gt);
					goto perf_and_return;
				case 6: // IBLEQ
					condBranch(mVU, mFC, a64::le);
					goto perf_and_return;
				case 7: // IBLTZ
					condBranch(mVU, mFC, a64::lt);
					goto perf_and_return;
				case 8: // IBNE
					condBranch(mVU, mFC, a64::ne);
					goto perf_and_return;
			}
		}
	}

	// E-bit end
	mVUsetupRange(mVU, xPC, false);
	mVUendProgram(mVU, &mFC, 1);

perf_and_return:
	{
		u8* endPtr = armGetCurrentCodePointer();
		if (mVU.index)
			Perf::vu1.RegisterPC(thisPtr, static_cast<u32>(endPtr - thisPtr), startPC);
		else
			Perf::vu0.RegisterPC(thisPtr, static_cast<u32>(endPtr - thisPtr), startPC);
	}
	return thisPtr;
}
