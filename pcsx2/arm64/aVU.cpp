// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 microVU recompiler — recompiler shell (Phase 7, task 7.2c).
//
// This is the ARM64 counterpart to pcsx2/x86/microVU.cpp. It holds the
// arch-neutral recompiler housekeeping: the microVU0/1 + CpuMicroVU0/1 globals,
// program/block-cache management (mVUinit/mVUreset/mVUclose/mVUclear + the
// mVUcreateProg/mVUcacheProg/mVUcmpProg/mVUsearchProg cache search) and the
// recMicroVU0/1 provider methods.
//
// What is NOT here yet (deferred, see the stub block at the bottom):
//   * the dispatcher + helper-thunk codegen (mVUdispatcherAB/CD,
//     mVUGenerateWaitMTVU/CopyPipelineState/CompareState) — task 7.2d.
//   * the block compiler entry points (mVUblockFetch/mVUentryGet) — later
//     Phase 7 tasks (microVU_Compile port).
// These are forward-declared and stubbed with pxFailRel so this TU links. The
// microVU rec stays *unselected* on ARM64 (VMManager pins CpuIntVU0/1), so none
// of those stubs are ever reached — they only exist so the management code above
// compiles and the microVU0/1 globals (whose unique_ptr<microRegAlloc> dtor is
// now instantiable, the allocator being complete in aVU_IR.h) can be defined.

#include "arm64/aVU.h"
#include "arm64/aVU_IR.h"
#include "arm64/aVU_Misc.h" // arch-neutral macro layer (task 7.3)

#include "VUmicro.h"
#include "Memory.h"
#include "Dmac.h"
#include "SaveState.h"

#include "common/AlignedMalloc.h"

#include <algorithm>
#include <vector>

// microVU rec contexts (x86 defines these in microVU.h; on ARM64 they live here,
// where microRegAlloc is a complete type so microVU's unique_ptr dtor compiles).
alignas(16) microVU microVU0;
alignas(16) microVU microVU1;

// Whole-program comparison on search (matches the x86 default — off). Cloned here
// rather than pulled from the x86-coupled microVU_Misc.h. doConstProp lives in aVU.h.
static constexpr bool doWholeProgCompare = false;

// recCall function-pointer types (x86: microVU.h). startFunct/startFunctXG point at
// the dispatcher code emitted by 7.2d; the provider Execute paths call through these.
typedef void (*mVUrecCall)(u32, u32);
typedef void (*mVUrecCallXG)(void);

// Program logging is compiled out by default (x86: microVU_Misc.h mVUlogProg).
#define mVUdumpProg(...) if (0) {}

// --- dispatcher + helper-thunk codegen (task 7.2d) ----------------------------
// These emit into the already-open armAsm (mVUgenerateDispatchers opens it).
static void mVUdispatcherAB(microVU& mVU);
static void mVUdispatcherCD(microVU& mVU);
static void mVUGenerateWaitMTVU(microVU& mVU);
static void mVUGenerateCopyPipelineState(microVU& mVU);
static void mVUGenerateCompareState(microVU& mVU);
static void mVUgenerateDispatchers(microVU& mVU);

// The C helper the waitMTVU thunk calls (x86: microVU_Misc.inl mVUwaitMTVU).
static void mVUwaitMTVU();

// The dispatcher (mVUdispatcherAB) calls these; defined later in this TU.
static void* mVUexecuteVU0(u32 startPC, u32 cycles);
static void* mVUexecuteVU1(u32 startPC, u32 cycles);
static void  mVUcleanUpVU0();
static void  mVUcleanUpVU1();

// Block compiler entry points (later Phase 7 tasks):
extern void* mVUblockFetch(microVU& mVU, u32 startPC, uptr pState);
static void* mVUentryGet(microVU& mVU, microBlockManager* block, u32 startPC, uptr pState);

// Forward decls for the private cache helpers (mirrors microVU.cpp ordering).
static void mVUdeleteProg(microVU& mVU, microProgram*& prog);
static microProgram* mVUcreateProg(microVU& mVU, int startPC);
static void mVUcacheProg(microVU& mVU, microProgram& prog);

// Pass-1 analysis (task 7.3) — arch-neutral, operates on the IR, no emitter calls.
// Currently compiled but not yet driven (the per-op pass1 handlers + tables are
// task 7.5+); mVUanalyzeCompileCheck below odr-uses every entry point so the bodies
// are type-checked and codegen'd now.
#include "arm64/aVU_Analyze.inl"

// Pass-2 flag allocators (task 7.4/7.5) — the emit-coupled getFlagReg + the
// Status/Mac/Clip flag normalize/denormalize helpers. These DO make VIXL calls
// (the first emit-backend slice); mVUallocFlagCheck below odr-uses them so the
// bodies are compiled now even though their drivers (mVUsetupFlags/Branch/Lower)
// are later 7.5 slices.
#include "arm64/aVU_Alloc.inl"

// Operand/result clamp helpers (task 7.5) — VIXL min/max range + sign-overflow
// clamps used by every VU FMAC arithmetic op. Emit-coupled; mVUclampCheck below
// odr-uses them so the bodies compile ahead of the Upper/Lower opcode handlers.
#include "arm64/aVU_Clamp.inl"

// Misc emit helpers (task 7.5) — the VU address-transform helper mVUaddrFix.
// Emit-coupled; mVUmiscCheck below odr-uses it so the body compiles ahead of the
// Lower load/store handlers.
#include "arm64/aVU_Misc.inl"

// Opcode dispatch tables (Tables/Compile big-bang) — minimal mVUopU/mVUopL
// (NOP + B/BAL wired, every other slot mVUunknown). Included BEFORE aVU_Flags.inl
// because the flag read-scan (_mVUflagPass) calls mVUopU/mVUopL. mVUtablesCheck
// below odr-uses the dispatchers so the bodies compile now.
#include "arm64/aVU_Tables.inl"

// Status/Mac/Clip flag pipeline (task 7.5/7.6) — flag-instance analysis +
// mVUdivSet/mVUsetupFlags emit. The opcode-table-dependent flag read-scan
// (_mVUflagPass/mVUsetFlagInfo) is deferred to the Branch/Compile slice.
// mVUflagCheck below odr-uses the emit helpers so their VIXL bodies compile now.
#include "arm64/aVU_Flags.inl"

// Branch / program-exit emission (task 7.7) — the two program-exit emitters
// (mVUendProgram/mVUDTendProgram) + getLastFlagInst + the E/T-bit & lpState C
// thunks. They make NO opcode-table calls, so they compile standalone ahead of
// the Tables/Compile big-bang. mVUbranchCheck below odr-uses the emitters.
#include "arm64/aVU_Branch.inl"

// Emit-coupled compile-driver helpers (Tables/Compile big-bang) — the
// per-instruction executors, D/T-bit & cycle-test early exits, register
// preloader, and debug/bad-op emitters. The cross-referencing core (mVUcompile +
// the branch drivers) lands in the next slice; mVUcompileEmitCheck below odr-uses
// the static helpers here so their VIXL bodies compile now.
#include "arm64/aVU_Compile.inl"

//------------------------------------------------------------------
// Pass-1 pipeline / cycle / range helpers (task 7.3 part 2)
//------------------------------------------------------------------
// ARM64 clone of the *arch-neutral* helpers in pcsx2/x86/microVU_Compile.inl:
// program-range setup, per-instruction cycle/pipeline accounting, and the
// branch/E-bit/bad-op pass-1 bookkeeping. Like the analysis pass these operate
// purely on the IR (microOp/microIR/microRegInfo) + the program cache and make
// ZERO emitter calls, so they port near-verbatim onto the macro layer already in
// aVU_Misc.h. They are *driven* by the compile driver (mVUcompile) which is the
// emit-coupled task 7.4/7.5; until then mVUcompileHelpersCheck (below) odr-uses
// the non-inline ones so their bodies are compiled now.
//
// The emit-coupled neighbours in microVU_Compile.inl (doUpperOp/doLowerOp/
// doSwapOp/doIbit/mVUexecuteInstruction, mVUtestCycles, mVUDoDBit/mVUDoTBit,
// mvuPreloadRegisters, handleBadOp, mVUdebugPrintBlocks, and mVUcompile itself)
// are deliberately NOT ported here — they make VIXL/regAlloc calls and come over
// with the 7.4 compile driver.

// Used by mVUsetupRange — re-cache the program if the guest micro memory changed.
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
// Cycles / Pipeline State
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

//------------------------------------------------------------------
// First-pass initialization (emitter-free)
//------------------------------------------------------------------
// These set up the IR state at the start of mVUcompile's first pass. They make
// no emitter calls (memset/memcpy + block-manager add only), so they port
// verbatim from microVU_Compile.inl onto the cloned IR. mVUinitFirstPass takes
// the start-of-block host code pointer (x86 `x86Ptr` -> ARM64
// armGetCurrentCodePointer()) and stashes it in mVUblock.codeStart (renamed from
// x86ptrStart per the 7.2a struct rename). The emit-coupled compile driver that
// drives these (mVUcompile + pass-2) is the rest of task 7.4.

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
	mVUblock.codeStart = thisPtr;
	mVUpBlock = mVUblocks[mVUstartPC / 2]->add(mVU, &mVUblock); // Add this block to block manager
	mVUregs.needExactMatch = (mVUpBlock->pState.blockType) ? 7 : 0; // ToDo: Fix 1-Op block flag linking (MGS2:Demo/Sly Cooper)
	mVUregs.blockType = 0;
	mVUregs.viBackUp  = 0;
	mVUregs.flagInfo  = 0;
	mVUsFlagHack = CHECK_VU_FLAGHACK;
	mVUinitConstValues(mVU);
}

//------------------------------------------------------------------
// Micro VU - Main Functions
//------------------------------------------------------------------

// Only run this once per VU! ;)
void mVUinit(microVU& mVU, uint vuIndex)
{
	std::memset(&mVU.prog, 0, sizeof(mVU.prog));

	mVU.index        =  vuIndex;
	mVU.cop2         =  0;
	mVU.vuMemSize    = (mVU.index ? 0x4000 : 0x1000);
	mVU.microMemSize = (mVU.index ? 0x4000 : 0x1000);
	mVU.progSize     = (mVU.index ? 0x4000 : 0x1000) / 4;
	mVU.progMemMask  =  mVU.progSize-1;
	mVU.cache        = vuIndex ? SysMemory::GetVU1Rec() : SysMemory::GetVU0Rec();
	mVU.prog.codeEnd = (vuIndex ? SysMemory::GetVU1RecEnd() : SysMemory::GetVU0RecEnd()) - (mVUcacheSafeZone * _1mb);

	mVU.regAlloc.reset(new microRegAlloc(mVU.index));
}

// Resets Rec Data
void mVUreset(microVU& mVU, bool resetReserve)
{
	if (THREAD_VU1)
	{
		DevCon.Warning("mVU Reset");
		// If MTVU is toggled on during gameplay we need to flush the running VU1 program, else it gets in a mess
		if (VU0.VI[REG_VPU_STAT].UL & 0x100)
		{
			CpuVU1->Execute(vu1RunCycles);
		}
		VU0.VI[REG_VPU_STAT].UL &= ~0x100;
	}

	// Point the VIXL emitter at mVU.cache and emit the dispatchers + helper thunks
	// at the start of the cache (x86: xSetPtr(mVU.cache) + the five generate calls).
	// Also sets mVU.prog.codeStart / codePtr just past the emitted dispatchers.
	mVUgenerateDispatchers(mVU);

	mVU.regs().nextBlockCycles = 0;
	memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));
	mVU.profiler.Reset(mVU.index);

	// Program Variables
	mVU.prog.cleared  =  1;
	mVU.prog.isSame   = -1;
	mVU.prog.cur      = NULL;
	mVU.prog.total    =  0;
	mVU.prog.curFrame =  0;

	// (codeStart / codePtr were set by mVUgenerateDispatchers, just past the
	// emitted dispatchers — the x86 rec does this with xGetAlignedCallTarget().)

	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		if (!mVU.prog.prog[i])
		{
			mVU.prog.prog[i] = new std::deque<microProgram*>();
			continue;
		}
		for (auto it = mVU.prog.prog[i]->begin(); it != mVU.prog.prog[i]->end(); ++it)
		{
			mVUdeleteProg(mVU, it[0]);
		}
		mVU.prog.prog[i]->clear();
		mVU.prog.quick[i].block = NULL;
		mVU.prog.quick[i].prog = NULL;
	}
}

// Free Allocated Resources
void mVUclose(microVU& mVU)
{
	// Delete Programs and Block Managers
	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		if (!mVU.prog.prog[i])
			continue;
		for (auto it = mVU.prog.prog[i]->begin(); it != mVU.prog.prog[i]->end(); ++it)
		{
			mVUdeleteProg(mVU, it[0]);
		}
		safe_delete(mVU.prog.prog[i]);
	}
}

// Clears Block Data in specified range
void mVUclear(microVU& mVU, u32 addr, u32 size)
{
	if (!mVU.prog.cleared)
	{
		mVU.prog.cleared = 1; // Next execution searches/creates a new microprogram
		std::memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState)); // Clear pipeline state
		for (u32 i = 0; i < (mVU.progSize / 2); i++)
		{
			mVU.prog.quick[i].block = NULL; // Clear current quick-reference block
			mVU.prog.quick[i].prog = NULL; // Clear current quick-reference prog
		}
	}
}

//------------------------------------------------------------------
// Micro VU - Private Functions
//------------------------------------------------------------------

// Deletes a program
static void mVUdeleteProg(microVU& mVU, microProgram*& prog)
{
	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		safe_delete(prog->block[i]);
	}
	safe_delete(prog->ranges);
	safe_aligned_free(prog);
}

// Creates a new Micro Program
static microProgram* mVUcreateProg(microVU& mVU, int startPC)
{
	microProgram* prog = (microProgram*)_aligned_malloc(sizeof(microProgram), 64);
	memset(prog, 0, sizeof(microProgram));
	prog->idx = mVU.prog.total++;
	prog->ranges = new std::deque<microRange>();
	prog->startPC = startPC;
	if (doWholeProgCompare)
		mVUcacheProg(mVU, *prog); // Cache Micro Program
	double cacheSize = (double)((uptr)mVU.prog.codeEnd - (uptr)mVU.prog.codeStart);
	double cacheUsed = ((double)((uptr)mVU.prog.codePtr - (uptr)mVU.prog.codeStart)) / (double)_1mb;
	double cachePerc = ((double)((uptr)mVU.prog.codePtr - (uptr)mVU.prog.codeStart)) / cacheSize * 100;
	ConsoleColors c = mVU.index ? Color_Orange : Color_Magenta;
	DevCon.WriteLn(c, "microVU%d: Cached Prog = [%03d] [PC=%04x] [List=%02d] (Cache=%3.3f%%) [%3.1fmb]",
		mVU.index, prog->idx, startPC * 8, mVU.prog.prog[startPC]->size() + 1, cachePerc, cacheUsed);
	return prog;
}

// Caches Micro Program
static void mVUcacheProg(microVU& mVU, microProgram& prog)
{
	if (!doWholeProgCompare)
	{
		const microRange& range = (*mVU.prog.cur->ranges)[0];
		auto cmpOffset = [&](void* x) { return (u8*)x + range.start; };
		memcpy(cmpOffset(prog.data), cmpOffset(mVU.regs().Micro), (range.end - range.start));
	}
	else
	{
		if (!mVU.index)
			memcpy(prog.data, mVU.regs().Micro, 0x1000);
		else
			memcpy(prog.data, mVU.regs().Micro, 0x4000);
	}
	mVUdumpProg(mVU, prog);
}

// Generate Hash for partial program based on compiled ranges...
static u64 mVUrangesHash(microVU& mVU, microProgram& prog)
{
	union
	{
		u64 v64;
		u32 v32[2];
	} hash = {0};

	std::deque<microRange>::const_iterator it(prog.ranges->begin());
	for (; it != prog.ranges->end(); ++it)
	{
		if ((it[0].start < 0) || (it[0].end < 0))
		{
			DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU.index, it[0].start, it[0].end);
		}
		for (int i = it[0].start / 4; i < it[0].end / 4; i++)
		{
			hash.v32[0] -= prog.data[i];
			hash.v32[1] ^= prog.data[i];
		}
	}
	return hash.v64;
}

// Prints the ratio of unique programs to total programs
[[maybe_unused]] static void mVUprintUniqueRatio(microVU& mVU)
{
	std::vector<u64> v;
	for (u32 pc = 0; pc < mProgSize / 2; pc++)
	{
		microProgramList* list = mVU.prog.prog[pc];
		if (!list)
			continue;
		for (auto it = list->begin(); it != list->end(); ++it)
		{
			v.push_back(mVUrangesHash(mVU, *it[0]));
		}
	}
	u32 total = v.size();
	std::sort(v.begin(), v.end());
	v.erase(std::unique(v.begin(), v.end()), v.end());
	if (!total)
		return;
	DevCon.WriteLn("%d / %d [%3.1f%%]", v.size(), total, 100. - (double)v.size() / (double)total * 100.);
}

// Compare Cached microProgram to mVU.regs().Micro
static bool mVUcmpProg(microVU& mVU, microProgram& prog)
{
	if (doWholeProgCompare)
	{
		if (memcmp((u8*)prog.data, mVU.regs().Micro, mVU.microMemSize))
			return false;
	}
	else
	{
		for (const auto& range : *prog.ranges)
		{
#if defined(PCSX2_DEVBUILD) || defined(_DEBUG)
			if ((range.start < 0) || (range.end < 0))
				DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU.index, range.start, range.end);
#endif
			auto cmpOffset = [&](void* x) { return (u8*)x + range.start; };

			if (memcmp(cmpOffset(prog.data), cmpOffset(mVU.regs().Micro), (range.end - range.start)))
				return false;
		}
	}
	mVU.prog.cleared = 0;
	mVU.prog.cur = &prog;
	mVU.prog.isSame = doWholeProgCompare ? 1 : -1;
	return true;
}

// Searches for Cached Micro Program and sets prog.cur to it (returns entry-point to program)
template <int vuIndex>
void* mVUsearchProg(u32 startPC, uptr pState)
{
	microVU& mVU = (vuIndex ? microVU1 : microVU0);
	microProgramQuick& quick = mVU.prog.quick[mVU.regs().start_pc / 8];
	microProgramList*  list  = mVU.prog.prog [mVU.regs().start_pc / 8];

	if (!quick.prog) // If null, we need to search for new program
	{
		for (auto it = list->begin(); it != list->end(); ++it)
		{
			bool b = mVUcmpProg(mVU, *it[0]);

			if (b)
			{
				quick.block = it[0]->block[startPC / 8];
				quick.prog  = it[0];
				list->erase(it);
				list->push_front(quick.prog);

				// Sanity check, in case for some reason the program compilation aborted half way through (JALR for example)
				if (quick.block == nullptr)
				{
					void* entryPoint = mVUblockFetch(mVU, startPC, pState);
					return entryPoint;
				}
				return mVUentryGet(mVU, quick.block, startPC, pState);
			}
		}

		// If cleared and program not found, make a new program instance
		mVU.prog.cleared = 0;
		mVU.prog.isSame  = 1;
		mVU.prog.cur     = mVUcreateProg(mVU, mVU.regs().start_pc/8);
		void* entryPoint = mVUblockFetch(mVU,  startPC, pState);
		quick.block      = mVU.prog.cur->block[startPC/8];
		quick.prog       = mVU.prog.cur;
		list->push_front(mVU.prog.cur);
		//mVUprintUniqueRatio(mVU);
		return entryPoint;
	}

	// If list.quick, then we've already found and recompiled the program ;)
	mVU.prog.isSame = -1;
	mVU.prog.cur = quick.prog;
	// Because the VU's can now run in sections and not whole programs at once
	// we need to set the current block so it gets the right program back
	quick.block = mVU.prog.cur->block[startPC / 8];

	// Sanity check, in case for some reason the program compilation aborted half way through
	if (quick.block == nullptr)
	{
		void* entryPoint = mVUblockFetch(mVU, startPC, pState);
		return entryPoint;
	}
	return mVUentryGet(mVU, quick.block, startPC, pState);
}

// Force both template instantiations to compile (the x86 rec instantiates these
// via mVUexecute<0/1>, which is task 7.2d; keep them compiled until then).
template void* mVUsearchProg<0>(u32 startPC, uptr pState);
template void* mVUsearchProg<1>(u32 startPC, uptr pState);

//------------------------------------------------------------------
// recMicroVU0 / recMicroVU1
//------------------------------------------------------------------

recMicroVU0 CpuMicroVU0;
recMicroVU1 CpuMicroVU1;

recMicroVU0::recMicroVU0() { m_Idx = 0; IsInterpreter = false; }
recMicroVU1::recMicroVU1() { m_Idx = 1; IsInterpreter = false; }

void recMicroVU0::Reserve()
{
	mVUinit(microVU0, 0);
}
void recMicroVU1::Reserve()
{
	mVUinit(microVU1, 1);
	vu1Thread.Open();
}

void recMicroVU0::Shutdown()
{
	mVUclose(microVU0);
}
void recMicroVU1::Shutdown()
{
	if (vu1Thread.IsOpen())
		vu1Thread.WaitVU();
	mVUclose(microVU1);
}

void recMicroVU0::Reset()
{
	mVUreset(microVU0, true);
}

void recMicroVU0::Step()
{
}

void recMicroVU1::Reset()
{
	vu1Thread.WaitVU();
	vu1Thread.Get_MTVUChanges();
	mVUreset(microVU1, true);
}

void recMicroVU0::SetStartPC(u32 startPC)
{
	VU0.start_pc = startPC;
}

void recMicroVU0::Execute(u32 cycles)
{
	VU0.flags &= ~VUFLAG_MFLAGSET;

	if (!(VU0.VI[REG_VPU_STAT].UL & 1))
		return;
	VU0.VI[REG_TPC].UL <<= 3;

	((mVUrecCall)microVU0.startFunct)(VU0.VI[REG_TPC].UL, cycles);
	VU0.VI[REG_TPC].UL >>= 3;
	if (microVU0.regs().flags & 0x4)
	{
		microVU0.regs().flags &= ~0x4;
		hwIntcIrq(6);
	}
}

void recMicroVU1::SetStartPC(u32 startPC)
{
	VU1.start_pc = startPC;
}

void recMicroVU1::Step()
{
}

void recMicroVU1::Execute(u32 cycles)
{
	if (!THREAD_VU1)
	{
		if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
			return;
	}
	VU1.VI[REG_TPC].UL <<= 3;
	((mVUrecCall)microVU1.startFunct)(VU1.VI[REG_TPC].UL, cycles);
	VU1.VI[REG_TPC].UL >>= 3;
	if (microVU1.regs().flags & 0x4 && !THREAD_VU1)
	{
		microVU1.regs().flags &= ~0x4;
		hwIntcIrq(7);
	}
}

void recMicroVU0::Clear(u32 addr, u32 size)
{
	mVUclear(microVU0, addr, size);
}
void recMicroVU1::Clear(u32 addr, u32 size)
{
	mVUclear(microVU1, addr, size);
}

void recMicroVU1::ResumeXGkick()
{
	if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
		return;
	((mVUrecCallXG)microVU1.startFunctXG)();
}

bool SaveStateBase::vuJITFreeze()
{
	if (IsSaving())
		vu1Thread.WaitVU();

	Freeze(microVU0.prog.lpState);
	Freeze(microVU1.prog.lpState);
	return IsOkay();
}

//------------------------------------------------------------------
// Dispatcher / helper-thunk code generation (task 7.2d)
//------------------------------------------------------------------
// ARM64 port of microVU_Execute.inl 23-315. This pins the final ARM64 microVU
// ABI. All five generators below emit into the armAsm opened by
// mVUgenerateDispatchers (one MacroAssembler session for the whole dispatcher
// block, finalised once). microVU is still unselected on ARM64 (VMManager pins
// CpuIntVU0/1), so the emitted code is not executed yet — but it compiles for
// real and pins the register/entry contract every later Phase-7 task builds on.
//
// Entry contract (mVUdispatcherAB = mVU.startFunct), called from C as
//   ((void(*)(u32 startPC, u32 cycles))startFunct)(startPC, cycles):
//   * startPC -> w0 (RWARG1), cycles -> w1 (RWARG2);
//   * call mVUexecuteVU0/1(startPC, cycles); it returns the block entry in x0;
//   * load the VU FPCR (if it differs from the EE's), set RVUSTATE = &vuRegs[i],
//     load the PQ NEON reg + mac/clip flag instances + the 4 status-flag GPRs
//     (gprF0-3 = w23-w26);
//   * br x0 (into the compiled block); the block exits by jumping to
//     mVU.exitFunct, which restores the EE FPCR and runs mVUcleanUpVU0/1.

// Whether the dispatcher needs to swap FPCR on entry/exit (x86: mvuNeedsFPCRUpdate).
static bool mvuNeedsFPCRUpdate(microVU& mVU)
{
	// Always update on the VU1 (MTVU) thread.
	if (mVU.index == 1 && THREAD_VU1)
		return true;

	// Otherwise only when the VU's FPCR differs from the EE's.
	return EmuConfig.Cpu.FPUFPCR.bitmask !=
		(mVU.index == 0 ? EmuConfig.Cpu.VU0FPCR.bitmask : EmuConfig.Cpu.VU1FPCR.bitmask);
}

// Emit: load the u64 FPCR bitmask at `bitmaskPtr` and write it to the host FPCR.
// Uses x16/x17 scratch only (never the block-entry x0 the AB dispatcher holds).
static void mVUemitSetHostFPCR(const void* bitmaskPtr)
{
	armMoveAddressToReg(RSCRATCHADDR, bitmaskPtr);          // x17 = &bitmask
	armAsm->Ldr(RXVIXLSCRATCH, a64::MemOperand(RSCRATCHADDR)); // x16 = bitmask
	armAsm->Msr(a64::FPCR, RXVIXLSCRATCH);
}

// Generates the code for entering/exiting recompiled blocks.
static void mVUdispatcherAB(microVU& mVU)
{
	mVU.startFunct = armGetCurrentCodePointer();

	// Save callee-saved GPRs (x19-x28, fp/lr) and the low halves of v8-v15.
	armBeginStackFrame(true);

	// Args (startPC, cycles) are already in w0/w1; mVUexecute returns entry in x0.
	armEmitCall(reinterpret_cast<const void*>(isVU1 ? mVUexecuteVU1 : mVUexecuteVU0));

	// Load VU's FPCR state.
	if (mvuNeedsFPCRUpdate(mVU))
		mVUemitSetHostFPCR(isVU1 ? &EmuConfig.Cpu.VU1FPCR.bitmask : &EmuConfig.Cpu.VU0FPCR.bitmask);

	// All VF/VI/flag loads address off RVUSTATE = &vuRegs[index].
	armMoveAddressToReg(RVUSTATE, &::vuRegs[mVU.index]);

	// Build the PQ latency register (v24). Final lane layout matches the x86 rec:
	//   VU0: [Q, pending_q, P, P]   VU1: [Q, pending_q, P, pending_p]
	// (NEON V4S lane0 = lowest 32 bits = X, same order as xmm.) v25 is a free
	// scratch (not in the VF pool v0-v23, not PQ, not VIXL scratch v31).
	const a64::VRegister vtmp = a64::VRegister(25, 128);
	armAsm->Ldr(mVU_xmmPQ.S(), a64::MemOperand(RVUSTATE, mVUoffVI(REG_Q)));      // lane0 = Q
	armAsm->Ldr(vtmp.S(),      a64::MemOperand(RVUSTATE, offsetof(VURegs, pending_q)));
	armAsm->Ins(mVU_xmmPQ.V4S(), 1, vtmp.V4S(), 0);                              // lane1 = pending_q
	armAsm->Ldr(vtmp.S(),      a64::MemOperand(RVUSTATE, mVUoffVI(REG_P)));      // vtmp = P
	armAsm->Ins(mVU_xmmPQ.V4S(), 2, vtmp.V4S(), 0);                             // lane2 = P
	if (isVU1)
	{
		armAsm->Ldr(vtmp.S(), a64::MemOperand(RVUSTATE, offsetof(VURegs, pending_p)));
		armAsm->Ins(mVU_xmmPQ.V4S(), 3, vtmp.V4S(), 0);                         // lane3 = pending_p
	}
	else
	{
		armAsm->Ins(mVU_xmmPQ.V4S(), 3, vtmp.V4S(), 0);                         // lane3 = P (vtmp still = P)
	}

	// Copy the mac/clip flag instances into the microVU's working storage.
	armAsm->Ldr(vtmp.Q(), a64::MemOperand(RVUSTATE, offsetof(VURegs, micro_macflags)));
	armMoveAddressToReg(RSCRATCHADDR, &mVU.macFlag[0]);
	armAsm->Str(vtmp.Q(), a64::MemOperand(RSCRATCHADDR));
	armAsm->Ldr(vtmp.Q(), a64::MemOperand(RVUSTATE, offsetof(VURegs, micro_clipflags)));
	armMoveAddressToReg(RSCRATCHADDR, &mVU.clipFlag[0]);
	armAsm->Str(vtmp.Q(), a64::MemOperand(RSCRATCHADDR));

	// Load the 4 status-flag instances into gprF0-gprF3 (w23-w26).
	for (int i = 0; i < 4; i++)
		armAsm->Ldr(armWRegister(mVU_F0 + i), a64::MemOperand(RVUSTATE, offsetof(VURegs, micro_statusflags) + i * 4));

	// Jump to the recompiled code block (entry still in x0).
	armAsm->Br(RXRET);

	// --- exit path: the block jumps here when done ---
	mVU.exitFunct = armGetCurrentCodePointer();

	// Restore the EE's FPCR state.
	if (mvuNeedsFPCRUpdate(mVU))
		mVUemitSetHostFPCR(&EmuConfig.Cpu.FPUFPCR.bitmask);

	armEmitCall(reinterpret_cast<const void*>(isVU1 ? mVUcleanUpVU1 : mVUcleanUpVU0));

	armEndStackFrame(true);
	armAsm->Ret();
}

// Generates the code for resuming/exiting xgkick.
static void mVUdispatcherCD(microVU& mVU)
{
	mVU.startFunctXG = armGetCurrentCodePointer();

	armBeginStackFrame(true);

	// Load VU's FPCR state.
	if (mvuNeedsFPCRUpdate(mVU))
		mVUemitSetHostFPCR(isVU1 ? &EmuConfig.Cpu.VU1FPCR.bitmask : &EmuConfig.Cpu.VU0FPCR.bitmask);

	// mVUrestoreRegs(): reload the PQ reg from its XGKICK backup slot.
	armMoveAddressToReg(RSCRATCHADDR, &mVU.vecBackup[mVU_xmmPQ.GetCode()][0]);
	armAsm->Ldr(mVU_xmmPQ.Q(), a64::MemOperand(RSCRATCHADDR));

	armMoveAddressToReg(RVUSTATE, &::vuRegs[mVU.index]);
	for (int i = 0; i < 4; i++)
		armAsm->Ldr(armWRegister(mVU_F0 + i), a64::MemOperand(RVUSTATE, offsetof(VURegs, micro_statusflags) + i * 4));

	// Jump to the recompiled code position to resume xgkick.
	armMoveAddressToReg(RSCRATCHADDR, &mVU.resumePtrXG);
	armAsm->Ldr(RSCRATCHADDR, a64::MemOperand(RSCRATCHADDR));
	armAsm->Br(RSCRATCHADDR);

	// --- exit path ---
	mVU.exitFunctXG = armGetCurrentCodePointer();

	// Back up the status flags (the other regs were backed up on xgkick).
	armMoveAddressToReg(RVUSTATE, &::vuRegs[mVU.index]);
	for (int i = 0; i < 4; i++)
		armAsm->Str(armWRegister(mVU_F0 + i), a64::MemOperand(RVUSTATE, offsetof(VURegs, micro_statusflags) + i * 4));

	// Restore the EE's FPCR state.
	if (mvuNeedsFPCRUpdate(mVU))
		mVUemitSetHostFPCR(&EmuConfig.Cpu.FPUFPCR.bitmask);

	armEndStackFrame(true);
	armAsm->Ret();
}

// The C helper called by the waitMTVU thunk (x86: microVU_Misc.inl mVUwaitMTVU).
static void mVUwaitMTVU()
{
	if (IsDevBuild)
		DevCon.WriteLn("microVU: Waiting on VU1 thread to access VU1 regs!");
	vu1Thread.WaitVU();
}

// A transparent helper-thunk: the register allocator does not know this call
// happens, so it must preserve every caller-saved host reg that can hold live VU
// state — the VF/PQ NEON pool (v0-v24) and the caller-saved GPRs that can hold VI
// values (x0-x15). gprF0-3 (w23-w26) and callee-saved VI regs survive the inner C
// call automatically. Only reached under MTVU (VU1 thread).
static void mVUGenerateWaitMTVU(microVU& mVU)
{
	mVU.waitMTVU = armGetCurrentCodePointer();

	constexpr int kGprSave = 16; // x0..x15
	constexpr int kVecSave = 25; // v0..v24 (VF pool + PQ)
	constexpr int gprBytes = kGprSave * 8;
	constexpr int vecBytes = kVecSave * 16;
	constexpr int frame = gprBytes + ((vecBytes + 15) & ~15); // 16-aligned

	armAsm->Sub(a64::sp, a64::sp, frame);
	for (int i = 0; i < kGprSave; i += 2)
		armAsm->Stp(armXRegister(i), armXRegister(i + 1), a64::MemOperand(a64::sp, i * 8));
	int voff = gprBytes;
	for (int i = 0; i < kVecSave - 1; i += 2, voff += 32)
		armAsm->Stp(armQRegister(i), armQRegister(i + 1), a64::MemOperand(a64::sp, voff));
	armAsm->Str(armQRegister(kVecSave - 1), a64::MemOperand(a64::sp, voff));

	armEmitCall(reinterpret_cast<const void*>(mVUwaitMTVU));

	voff = gprBytes;
	for (int i = 0; i < kVecSave - 1; i += 2, voff += 32)
		armAsm->Ldp(armQRegister(i), armQRegister(i + 1), a64::MemOperand(a64::sp, voff));
	armAsm->Ldr(armQRegister(kVecSave - 1), a64::MemOperand(a64::sp, voff));
	for (int i = 0; i < kGprSave; i += 2)
		armAsm->Ldp(armXRegister(i), armXRegister(i + 1), a64::MemOperand(a64::sp, i * 8));
	armAsm->Add(a64::sp, a64::sp, frame);
	armAsm->Ret();
}

// Copies the 96-byte pipeline state from [x0] (source pState, supplied by the
// caller) into mVU.prog.lpState (x86: mVUGenerateCopyPipelineState).
static void mVUGenerateCopyPipelineState(microVU& mVU)
{
	mVU.copyPLState = armGetCurrentCodePointer();

	armMoveAddressToReg(RXARG2, &mVU.prog.lpState); // x1 = dest
	for (int i = 0; i < 6; i++)
	{
		const a64::VRegister v = a64::VRegister(i, 128);
		armAsm->Ldr(v.Q(), a64::MemOperand(RXARG1, i * 16));
		armAsm->Str(v.Q(), a64::MemOperand(RXARG2, i * 16));
	}
	armAsm->Ret();
}

// Custom optimised block-search comparator: compares the two 96-byte
// microRegInfo blocks at x0 (lhs) and x1 (rhs); returns w0 = 0 iff fully equal
// (x86: mVUGenerateCompareState; the search treats 0 as a match).
static void mVUGenerateCompareState(microVU& mVU)
{
	mVU.compareStateF = armGetCurrentCodePointer();

	// 6 x 128-bit lanewise equality compares (0xffffffff per equal 32-bit lane),
	// AND-reduced into v0.
	auto cmp16 = [&](int off, int dst, int t0, int t1) {
		const a64::VRegister a = a64::VRegister(t0, 128), b = a64::VRegister(t1, 128);
		armAsm->Ldr(a.Q(), a64::MemOperand(RXARG1, off));
		armAsm->Ldr(b.Q(), a64::MemOperand(RXARG2, off));
		armAsm->Cmeq(a64::VRegister(dst, 128).V4S(), a.V4S(), b.V4S());
	};
	cmp16(0x00, 0, 0, 1);
	cmp16(0x10, 2, 2, 3);
	cmp16(0x20, 4, 4, 5);
	cmp16(0x30, 6, 6, 7);
	armAsm->And(a64::VRegister(0, 128).V16B(), a64::VRegister(0, 128).V16B(), a64::VRegister(2, 128).V16B());
	armAsm->And(a64::VRegister(4, 128).V16B(), a64::VRegister(4, 128).V16B(), a64::VRegister(6, 128).V16B());
	cmp16(0x40, 2, 2, 3);
	cmp16(0x50, 6, 6, 7);
	armAsm->And(a64::VRegister(2, 128).V16B(), a64::VRegister(2, 128).V16B(), a64::VRegister(6, 128).V16B());
	armAsm->And(a64::VRegister(0, 128).V16B(), a64::VRegister(0, 128).V16B(), a64::VRegister(4, 128).V16B());
	armAsm->And(a64::VRegister(0, 128).V16B(), a64::VRegister(0, 128).V16B(), a64::VRegister(2, 128).V16B());

	// v0 is all-ones iff every byte matched. Reduce: min byte == 0xff iff equal.
	armAsm->Uminv(a64::VRegister(1, 128).B(), a64::VRegister(0, 128).V16B());
	armAsm->Umov(RWRET, a64::VRegister(1, 128).V16B(), 0);
	armAsm->Eor(RWRET, RWRET, 0xff); // 0 iff equal
	armAsm->Ret();
}

// Emit the dispatchers + helper thunks at the start of the VU's code cache, then
// set codeStart/codePtr just past them (x86: xSetPtr(mVU.cache) + the five
// generate calls + xGetAlignedCallTarget in mVUreset). No constant pool: the
// dispatchers materialise addresses via adrp/mov and call C helpers via mov+blr.
static void mVUgenerateDispatchers(microVU& mVU)
{
	u8* const cacheEnd = (mVU.index ? SysMemory::GetVU1RecEnd() : SysMemory::GetVU0RecEnd());
	armSetAsmPtr(mVU.cache, static_cast<size_t>(cacheEnd - mVU.cache), nullptr);
	armStartBlock();

	mVUdispatcherAB(mVU);
	mVUdispatcherCD(mVU);
	mVUGenerateWaitMTVU(mVU);
	mVUGenerateCopyPipelineState(mVU);
	mVUGenerateCompareState(mVU);

	u8* const end = armEndBlock();
	mVU.prog.codeStart = mVU.prog.codePtr =
		reinterpret_cast<u8*>((reinterpret_cast<uintptr_t>(end) + 15u) & ~uintptr_t(15));
}

//------------------------------------------------------------------
// Execution / cleanup (microVU_Execute.inl 318-381)
//------------------------------------------------------------------

// Executes for the given number of cycles: finds (compiling if needed) the
// microprogram for startPC and returns its host entry point.
template <int vuIndex>
static void* mVUexecute(u32 startPC, u32 cycles)
{
	microVU& mVU = (vuIndex ? microVU1 : microVU0);
	const u32 vuLimit = vuIndex ? 0x3ff8 : 0xff8;
	if (startPC > vuLimit + 7)
		DevCon.Warning("microVU%x Warning: startPC = 0x%x, cycles = 0x%x", vuIndex, startPC, cycles);

	mVU.cycles = cycles;
	mVU.totalCycles = cycles;

	// x86 repositions its single global emit cursor here (xSetTextPtr/xSetPtr to
	// mVU.prog.x86ptr). On ARM64 the per-block emit session (armStartBlock/
	// armEndBlock, with the icache flush that must precede execution) is owned by
	// the block compiler (mVUblockFetch, a later Phase-7 task); mVUexecute only
	// drives the program search.
	return mVUsearchProg<vuIndex>(startPC & vuLimit, (uptr)&mVU.prog.lpState);
}

// Post-execution bookkeeping: cache-limit reset + cycle accounting.
template <int vuIndex>
static void mVUcleanUp()
{
	microVU& mVU = (vuIndex ? microVU1 : microVU0);

	// The block compiler advanced mVU.prog.codePtr as it emitted; if it ran past
	// the cache limit, reset the program cache (x86 checks xGetPtr() here).
	if ((mVU.prog.codePtr < mVU.prog.codeStart) || (mVU.prog.codePtr >= mVU.prog.codeEnd))
	{
		Console.WriteLn(vuIndex ? Color_Orange : Color_Magenta, "microVU%d: Program cache limit reached.", mVU.index);
		mVUreset(mVU, false);
	}

	mVU.cycles = mVU.totalCycles - std::max(0, mVU.cycles);
	mVU.regs().cycle += mVU.cycles;

	if (!vuIndex || !THREAD_VU1)
	{
		u32 cycles_passed = std::min(mVU.cycles, 3000) * EmuConfig.Speedhacks.EECycleSkip;
		if (cycles_passed > 0)
		{
			s64 vu0_offset = VU0.cycle - cpuRegs.cycle;
			cpuRegs.cycle += cycles_passed;

			// VU0 needs to stay in sync with the CPU otherwise things get messy.
			// So we need to adjust when VU1 skips cycles also.
			if (!vuIndex)
				VU0.cycle = cpuRegs.cycle + vu0_offset;
			else
				VU0.cycle += cycles_passed;
		}
	}
	mVU.profiler.Print();
}

// Caller wrappers (x86: microVU_Execute.inl 378-381) — the dispatcher branches
// to these by address.
static void* mVUexecuteVU0(u32 startPC, u32 cycles) { return mVUexecute<0>(startPC, cycles); }
static void* mVUexecuteVU1(u32 startPC, u32 cycles) { return mVUexecute<1>(startPC, cycles); }
static void  mVUcleanUpVU0() { mVUcleanUp<0>(); }
static void  mVUcleanUpVU1() { mVUcleanUp<1>(); }

//------------------------------------------------------------------
// Not-yet-ported block compiler (temporary stubs — later Phase 7 tasks)
//------------------------------------------------------------------
// microVU is unselected on ARM64 (VMManager pins CpuIntVU0/1), so these are
// never reached; the pxFailRel is a loud guard if that ever changes before the
// block compiler is ported. They exist so the cache-management code links.

void* mVUblockFetch(microVU&, u32, uptr)
{
	pxFailRel("ARM64 mVUblockFetch not ported (Phase 7 block compiler)");
	return nullptr;
}
static void* mVUentryGet(microVU&, microBlockManager*, u32, uptr)
{
	pxFailRel("ARM64 mVUentryGet not ported (Phase 7 block compiler)");
	return nullptr;
}

//------------------------------------------------------------------
// Build-time validation
//------------------------------------------------------------------

// Layout sanity checks for the ported pipeline-state key / IR structs. These
// mirror the invariants the x86 microVU relies on (the 96-byte microRegInfo is
// compared as six 128-bit vectors by the generated compareStateF).
static_assert(sizeof(microRegInfo) == 96, "microRegInfo must stay 96 bytes (host pipeline-state compare)");
static_assert(alignof(microRegInfo) == 16, "microRegInfo must stay 16-byte aligned");
static_assert(alignof(microBlock) == 16, "microBlock must stay 16-byte aligned");

// Force the register allocator's emission paths to be compiled. microRegAlloc's
// emission members (allocReg/flushAll/...) are not reached by the cache-management
// code above (the block compiler/dispatcher call them — tasks 7.2d onward), so
// without an explicit use their VIXL bodies would never be instantiated and any
// emission error would go undetected. This function is never called.
[[maybe_unused]] static void mVUallocCompileCheck()
{
	microRegAlloc ra(0);
	ra.reset();
	(void)ra.allocReg(1, 2, 0xf);
	(void)ra.allocReg(0, 1, 0x4);
	(void)ra.allocReg(33, -1, 0x8);
	(void)ra.allocReg(32, 32, 0x6, false);
	(void)ra.allocGPR(1, 2, true);
	(void)ra.allocGPR(-1, 3);
	ra.moveVIToGPR(armWRegister(0), 1, true);
	ra.flushAll();
	ra.flushCallerSavedRegisters(true);
	ra.TDwritebackAll();
	(void)ra.checkVFClamp(0);
	(void)ra.hasRegVF(1);
	(void)ra.hasRegVI(1);
	(void)ra.getFreeXmmCount();
	(void)ra.getFreeGPRCount();
}

// Force the pass-1 analysis bodies (aVU_Analyze.inl) to be compiled. They are not
// reached by any live code yet (the per-op pass1 handlers that call them are task
// 7.5+), so without an explicit odr-use these inline functions would be parsed but
// their full instantiation/codegen — and any porting error — could go undetected.
// Never called: it dereferences mVU.prog.cur (null here), it exists only to compile.
[[maybe_unused]] static void mVUanalyzeCompileCheck()
{
	microVU& mVU = microVU0;
	mVUanalyzeFMAC1(mVU, 1, 2, 3);
	mVUanalyzeFMAC2(mVU, 1, 2);
	mVUanalyzeFMAC3(mVU, 1, 2, 3);
	mVUanalyzeFMAC4(mVU, 1, 2);
	mVUanalyzeIALU1(mVU, 1, 2, 3);
	mVUanalyzeIALU2(mVU, 1, 2);
	mVUanalyzeIADDI(mVU, 1, 2, (s16)3);
	mVUanalyzeMR32(mVU, 1, 2);
	mVUanalyzeFDIV(mVU, 1, 0, 2, 1, 3);
	mVUanalyzeEFU1(mVU, 1, 0, 3);
	mVUanalyzeEFU2(mVU, 1, 3);
	mVUanalyzeMFP(mVU, 1);
	mVUanalyzeMOVE(mVU, 1, 2);
	mVUanalyzeLQ(mVU, 1, 2, true);
	mVUanalyzeSQ(mVU, 1, 2, true);
	mVUanalyzeR1(mVU, 1, 0);
	mVUanalyzeR2(mVU, 1, true);
	mVUanalyzeSflag(mVU, 1);
	mVUanalyzeFSSET(mVU);
	mVUanalyzeMflag(mVU, 1, 2);
	mVUanalyzeCflag(mVU, 1);
	mVUanalyzeXGkick(mVU, 1, 2);
	mVUanalyzeCondBranch1(mVU, 1);
	mVUanalyzeCondBranch2(mVU, 1, 2);
	mVUanalyzeNormBranch(mVU, 1, true);
	mVUanalyzeJump(mVU, 1, 2, true);
}

// Force the pass-1 pipeline/cycle/range helpers (above) to be compiled. The
// non-inline ones (mVUsetupRange/mVUoptimizePipeState/mVUincCycles/cmpVFregs/
// mVUsetCycles) have external linkage so they codegen regardless, but their
// driver (mVUcompile) is task 7.4; until it lands this odr-use is the only thing
// proving the bodies + the inline helpers compile. Never called: it touches
// mVU.prog.cur (null here), it exists only to compile.
[[maybe_unused]] static void mVUcompileHelpersCheck()
{
	microVU& mVU = microVU0;
	mVUcheckIsSame(mVU);
	mVUsetupRange(mVU, 0, true);
	mVUcheckBadOp(mVU);
	branchWarning(mVU);
	int branch = 0;
	eBitPass1(mVU, branch);
	eBitWarning(mVU);
	mVUoptimizePipeState(mVU);
	mVUincCycles(mVU, 1);
	mVUsetCycles(mVU);
	microVFreg vfa{}, vfb{};
	bool xVar = false;
	cmpVFregs(vfa, vfb, xVar);
	(void)optimizeReg(0);
	(void)calcCycles(0, 0);
	(void)tCycles(0, 0);
	incP(mVU);
	incQ(mVU);
	startLoop(mVU);
	mVUinitConstValues(mVU);
	mVUinitFirstPass(mVU, (uptr)&mVU.prog.lpState, nullptr);
}

// Force the Pass-2 flag allocators (aVU_Alloc.inl) to be compiled. These are the
// first emit-backend helpers that actually call VIXL; their drivers
// (mVUsetupFlags/mVUdivSet, the Branch/Lower opcode handlers) are later 7.5
// slices, so without an explicit odr-use their VIXL bodies — and any emission
// error — would never be instantiated. Never called: it emits into no open
// assembler, it exists only to type-check + codegen the bodies.
[[maybe_unused]] static void mVUallocFlagCheck()
{
	microVU& mVU = microVU0;
	(void)getFlagReg(0);
	setBitSFLAG(gprT1, gprT2, 0x0f00, 0x0001);
	setBitFSEQ(gprT1, 0x0f00);
	mVUallocSFLAGa(gprT1, 1);
	mVUallocSFLAGb(gprT1, 1);
	mVUallocSFLAGc(gprT1, gprT2, 1);
	mVUallocSFLAGd(&mVU.divFlag, gprT1, gprT2, getFlagReg(0));
	mVUallocMFLAGa(mVU, gprT1, 1);
	mVUallocMFLAGb(mVU, gprT1, 1);
	mVUallocCFLAGa(mVU, gprT1, 1);
	mVUallocCFLAGb(mVU, gprT1, 1);

	const a64::VRegister t = a64::VRegister(0, 128);
	getPreg(mVU, t);       // uses mVUinfo.readP (mVU in scope)
	getQreg(t, 0);
	writeQreg(t, 1);
}

// Force the clamp helpers (aVU_Clamp.inl) to be compiled. Same rationale as
// mVUallocFlagCheck: they emit VIXL but their callers (the FMAC arithmetic
// opcode handlers) are a later 7.5 slice, so without an explicit odr-use their
// bodies — and any emission error — would never be instantiated. Never called.
[[maybe_unused]] static void mVUclampCheck()
{
	microVU& mVU = microVU0;
	const a64::VRegister reg = a64::VRegister(1, 128);
	const a64::VRegister tmp = a64::VRegister(2, 128);
	mVUclamp1(mVU, reg, tmp, 0xf);
	mVUclamp2(mVU, reg, tmp, 0x8);
	mVUclamp3(mVU, reg, tmp, 0xf);
	mVUclamp4(mVU, reg, tmp, 0x8);
}

// Force the misc emit helpers (aVU_Misc.inl) to be compiled. Never called.
[[maybe_unused]] static void mVUmiscCheck()
{
	microVU& mVU = microVU0;
	mVUaddrFix(mVU, a64::x9, a64::x10);
}

// Force the emit-coupled compile helpers (aVU_Compile.inl) to be compiled. They
// emit VIXL but have no live caller until mVUcompile lands, so without this
// odr-use their bodies (and any emission error) would never be instantiated.
// Never called: it runs against a null mVU.prog.cur; it exists only to compile.
[[maybe_unused]] static void mVUcompileEmitCheck()
{
	microVU& mVU = microVU0;
	microFlagCycles mFC{};
	mVUexecuteInstruction(mVU);
	doUpperOp(mVU);
	doLowerOp(mVU);
	doSwapOp(mVU);
	doIbit(mVU);
	flushRegs(mVU);
	handleBadOp(mVU, 0);
	mVUdebugPrintBlocks(mVU, false);
	mVUDoDBit(mVU, &mFC);
	mVUDoTBit(mVU, &mFC);
	mVUtestCycles(mVU, mFC);
	microFlagCycles mFCb{};
	mVUSaveFlags(mVU, mFC, mFCb);
	mvuPreloadRegisters(mVU, 0);
}

// Force the opcode dispatch tables (aVU_Tables.inl) to be compiled. The minimal
// mVUopU/mVUopL dispatchers + the NOP/B/BAL handlers have no live caller until the
// flag read-scan + mVUcompile slices, so without this odr-use the static bodies
// (and the table arrays) would be dropped. Never called: it would run pass-1
// analysis against a null mVU.prog.cur; it exists only to compile.
[[maybe_unused]] static void mVUtablesCheck()
{
	microVU& mVU = microVU0;
	mVUopU(mVU, 0);
	mVUopL(mVU, 0);
}

// Force the flag pipeline (aVU_Flags.inl) to be compiled. The analysis functions
// (findFlagInst/sortFlag/sortFullFlag/mVUstatusFlagOp/mVUsetFlags) have external
// linkage and codegen regardless, but the emit helpers (mVUdivSet/mVUsetupFlags)
// are __fi and have no live caller until the Branch/Compile slice — without this
// odr-use their VIXL bodies, and any emission error, would never be instantiated.
// Never called: it touches mVU.prog.cur (null here), it exists only to compile.
[[maybe_unused]] static void mVUflagCheck()
{
	microVU& mVU = microVU0;
	microFlagCycles mFC{};
	mVUdivSet(mVU);
	mVUsetupFlags(mVU, mFC);
	mVUsetFlags(mVU, mFC);
	mVUstatusFlagOp(mVU);
	(void)findFlagInst(mFC.xStatus, 0);
	int bFlag[4];
	(void)sortFlag(mFC.xStatus, bFlag, 0);
	sortFullFlag(mFC.xStatus, bFlag);
}

// Force the branch/program-exit emitters (aVU_Branch.inl) to be compiled. The two
// mVUendProgram/mVUDTendProgram emitters are the only non-static, never-yet-called
// emit functions in this slice; odr-use them so their VIXL bodies (and any
// emission error) are instantiated now. Never called.
[[maybe_unused]] static void mVUbranchCheck()
{
	microVU& mVU = microVU0;
	microFlagCycles mFC{};
	(void)getLastFlagInst(mVUregs, mFC.xStatus, 0, 0);
	mVUendProgram(mVU, &mFC, 1);
	mVUDTendProgram(mVU, &mFC, 1);
	mVUsetupBranch(mVU, mFC);
}
