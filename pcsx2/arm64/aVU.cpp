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

// --- not-yet-ported codegen / compile layer (defined as stubs below) ---------
// Dispatcher + helper-thunk generators (task 7.2d):
static void mVUdispatcherAB(microVU& mVU);
static void mVUdispatcherCD(microVU& mVU);
static void mVUGenerateWaitMTVU(microVU& mVU);
static void mVUGenerateCopyPipelineState(microVU& mVU);
static void mVUGenerateCompareState(microVU& mVU);
// Block compiler entry points (later Phase 7 tasks):
extern void* mVUblockFetch(microVU& mVU, u32 startPC, uptr pState);
static void* mVUentryGet(microVU& mVU, microBlockManager* block, u32 startPC, uptr pState);

// Forward decls for the private cache helpers (mirrors microVU.cpp ordering).
static void mVUdeleteProg(microVU& mVU, microProgram*& prog);
static microProgram* mVUcreateProg(microVU& mVU, int startPC);
static void mVUcacheProg(microVU& mVU, microProgram& prog);

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

	// TODO(7.2d): point the VIXL emitter at mVU.cache (the x86 rec does
	// xSetTextPtr(mVU.textPtr()) / xSetPtr(mVU.cache) here) and emit the
	// dispatchers + helper thunks. These generators are stubs until 7.2d.
	mVUdispatcherAB(mVU);
	mVUdispatcherCD(mVU);
	mVUGenerateWaitMTVU(mVU);
	mVUGenerateCopyPipelineState(mVU);
	mVUGenerateCompareState(mVU);

	mVU.regs().nextBlockCycles = 0;
	memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));
	mVU.profiler.Reset(mVU.index);

	// Program Variables
	mVU.prog.cleared  =  1;
	mVU.prog.isSame   = -1;
	mVU.prog.cur      = NULL;
	mVU.prog.total    =  0;
	mVU.prog.curFrame =  0;

	// Setup Dynarec Cache Limits for Each Program. The x86 rec sets codeStart to
	// xGetAlignedCallTarget() (i.e. just past the dispatchers it emitted above);
	// 7.2d will set it precisely once the dispatchers actually emit code.
	mVU.prog.codeStart = mVU.cache;
	mVU.prog.codePtr   = mVU.prog.codeStart;

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
// Not-yet-ported codegen / compile layer (temporary stubs)
//------------------------------------------------------------------
// These belong to task 7.2d (dispatcher generation) and the later block-compiler
// port. microVU is unselected on ARM64 (VMManager pins CpuIntVU0/1), so none of
// these run yet — the pxFailRel is a loud guard if that ever changes before they
// are implemented. They exist so the cache-management code above links.

static void mVUdispatcherAB(microVU&)            { pxFailRel("ARM64 mVUdispatcherAB not ported (Phase 7.2d)"); }
static void mVUdispatcherCD(microVU&)            { pxFailRel("ARM64 mVUdispatcherCD not ported (Phase 7.2d)"); }
static void mVUGenerateWaitMTVU(microVU&)        { pxFailRel("ARM64 mVUGenerateWaitMTVU not ported (Phase 7.2d)"); }
static void mVUGenerateCopyPipelineState(microVU&) { pxFailRel("ARM64 mVUGenerateCopyPipelineState not ported (Phase 7.2d)"); }
static void mVUGenerateCompareState(microVU&)    { pxFailRel("ARM64 mVUGenerateCompareState not ported (Phase 7.2d)"); }

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
