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

// Micro VU recompiler! - author: cottonvibes(@gmail.com)

#include "PrecompiledHeader.h"
#include "microVU.h"

#include "common/AlignedMalloc.h"
#include "common/Perf.h"

//------------------------------------------------------------------
// Micro VU - Main Functions
//------------------------------------------------------------------
alignas(__pagesize) static u8 vu0_RecDispatchers[mVUdispCacheSize];
alignas(__pagesize) static u8 vu1_RecDispatchers[mVUdispCacheSize];

static __fi void mVUthrowHardwareDeficiency(const char* extFail, int vuIndex)
{
	throw Exception::HardwareDeficiency()
		.SetDiagMsg(fmt::format("microVU{} recompiler init failed: %s is not available.", vuIndex, extFail))
		.SetUserMsg(fmt::format("{} Extensions not found.  microVU requires a host CPU with SSE4 extensions.", extFail));
}

void mVUreserveCache(microVU& mVU)
{

	mVU.cache_reserve = new RecompiledCodeReserve(fmt::format("Micro VU{} Recompiler Cache", mVU.index), _16mb);
	mVU.cache_reserve->SetProfilerName(fmt::format("mVU{}rec", mVU.index));

	mVU.cache = mVU.index
		? (u8*)mVU.cache_reserve->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::mVU1recOffset, mVU.cacheSize * _1mb)
		: (u8*)mVU.cache_reserve->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::mVU0recOffset, mVU.cacheSize * _1mb);

	mVU.cache_reserve->ThrowIfNotOk();
}

// Only run this once per VU! ;)
void mVUinit(microVU& mVU, uint vuIndex)
{

	if (!x86caps.hasStreamingSIMD4Extensions)
		mVUthrowHardwareDeficiency("SSE4", vuIndex);

	memzero(mVU.prog);

	mVU.index        =  vuIndex;
	mVU.cop2         =  0;
	mVU.vuMemSize    = (mVU.index ? 0x4000 : 0x1000);
	mVU.microMemSize = (mVU.index ? 0x4000 : 0x1000);
	mVU.progSize     = (mVU.index ? 0x4000 : 0x1000) / 4;
	mVU.progMemMask  =  mVU.progSize-1;
	mVU.cacheSize    =  mVUcacheReserve;
	mVU.cache        = NULL;
	mVU.dispCache    = NULL;
	mVU.startFunct   = NULL;
	mVU.exitFunct    = NULL;

	mVUreserveCache(mVU);

	if (vuIndex)
		mVU.dispCache = vu1_RecDispatchers;
	else
		mVU.dispCache = vu0_RecDispatchers;

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
	// Restore reserve to uncommitted state
	if (resetReserve)
		mVU.cache_reserve->Reset();

	HostSys::MemProtect(mVU.dispCache, mVUdispCacheSize, PageAccess_ReadWrite());
	memset(mVU.dispCache, 0xcc, mVUdispCacheSize);

	x86SetPtr(mVU.dispCache);
	mVUdispatcherAB(mVU);
	mVUdispatcherCD(mVU);
	mVUemitSearch();

	mVU.regs().nextBlockCycles = 0;
	memset(&mVU.prog.lpState, 0, sizeof(mVU.prog.lpState));
	mVU.profiler.Reset(mVU.index);

	// Program Variables
	mVU.prog.cleared  =  1;
	mVU.prog.isSame   = -1;
	mVU.prog.cur      = NULL;
	mVU.prog.total    =  0;
	mVU.prog.curFrame =  0;

	// Setup Dynarec Cache Limits for Each Program
	u8* z = mVU.cache;
	mVU.prog.x86start = z;
	mVU.prog.x86ptr   = z;
	mVU.prog.x86end   = z + ((mVU.cacheSize - mVUcacheSafeZone) * _1mb);

	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		if (!mVU.prog.prog[i])
		{
			mVU.prog.prog[i] = new std::deque<microProgram*>();
			continue;
		}
		std::deque<microProgram*>::iterator it(mVU.prog.prog[i]->begin());
		for (; it != mVU.prog.prog[i]->end(); ++it)
		{
			mVUdeleteProg(mVU, it[0]);
		}
		mVU.prog.prog[i]->clear();
		mVU.prog.quick[i].block = NULL;
		mVU.prog.quick[i].prog = NULL;
	}

	HostSys::MemProtect(mVU.dispCache, mVUdispCacheSize, PageAccess_ExecOnly());

	if (mVU.index)
		Perf::any.map((uptr)&mVU.dispCache, mVUdispCacheSize, "mVU1 Dispatcher");
	else
		Perf::any.map((uptr)&mVU.dispCache, mVUdispCacheSize, "mVU0 Dispatcher");
}

// Free Allocated Resources
void mVUclose(microVU& mVU)
{

	safe_delete(mVU.cache_reserve);

	// Delete Programs and Block Managers
	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		if (!mVU.prog.prog[i])
			continue;
		std::deque<microProgram*>::iterator it(mVU.prog.prog[i]->begin());
		for (; it != mVU.prog.prog[i]->end(); ++it)
		{
			mVUdeleteProg(mVU, it[0]);
		}
		safe_delete(mVU.prog.prog[i]);
	}
}

// Clears Block Data in specified range
__fi void mVUclear(mV, u32 addr, u32 size)
{
	if (!mVU.prog.cleared)
	{
		mVU.prog.cleared = 1; // Next execution searches/creates a new microprogram
		memzero(mVU.prog.lpState); // Clear pipeline state
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
__ri void mVUdeleteProg(microVU& mVU, microProgram*& prog)
{
	for (u32 i = 0; i < (mVU.progSize / 2); i++)
	{
		safe_delete(prog->block[i]);
	}
	safe_delete(prog->ranges);
	safe_aligned_free(prog);
}

// Creates a new Micro Program
__ri microProgram* mVUcreateProg(microVU& mVU, int startPC)
{
	microProgram* prog = (microProgram*)_aligned_malloc(sizeof(microProgram), 64);
	memset(prog, 0, sizeof(microProgram));
	prog->idx = mVU.prog.total++;
	prog->ranges = new std::deque<microRange>();
	prog->startPC = startPC;
	mVUcacheProg(mVU, *prog); // Cache Micro Program
	double cacheSize = (double)((uptr)mVU.prog.x86end - (uptr)mVU.prog.x86start);
	double cacheUsed = ((double)((uptr)mVU.prog.x86ptr - (uptr)mVU.prog.x86start)) / (double)_1mb;
	double cachePerc = ((double)((uptr)mVU.prog.x86ptr - (uptr)mVU.prog.x86start)) / cacheSize * 100;
	ConsoleColors c = mVU.index ? Color_Orange : Color_Magenta;
	DevCon.WriteLn(c, "microVU%d: Cached Prog = [%03d] [PC=%04x] [List=%02d] (Cache=%3.3f%%) [%3.1fmb]",
		mVU.index, prog->idx, startPC * 8, mVU.prog.prog[startPC]->size() + 1, cachePerc, cacheUsed);
	return prog;
}

// Caches Micro Program
__ri void mVUcacheProg(microVU& mVU, microProgram& prog)
{
	if (!mVU.index)
		memcpy(prog.data, mVU.regs().Micro, 0x1000);
	else
		memcpy(prog.data, mVU.regs().Micro, 0x4000);
	mVUdumpProg(mVU, prog);
}

// Generate Hash for partial program based on compiled ranges...
u64 mVUrangesHash(microVU& mVU, microProgram& prog)
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
void mVUprintUniqueRatio(microVU& mVU)
{
	std::vector<u64> v;
	for (u32 pc = 0; pc < mProgSize / 2; pc++)
	{
		microProgramList* list = mVU.prog.prog[pc];
		if (!list)
			continue;
		std::deque<microProgram*>::iterator it(list->begin());
		for (; it != list->end(); ++it)
		{
			v.push_back(mVUrangesHash(mVU, *it[0]));
		}
	}
	u32 total = v.size();
	sortVector(v);
	makeUnique(v);
	if (!total)
		return;
	DevCon.WriteLn("%d / %d [%3.1f%%]", v.size(), total, 100. - (double)v.size() / (double)total * 100.);
}

// Compare Cached microProgram to mVU.regs().Micro
__fi bool mVUcmpProg(microVU& mVU, microProgram& prog, const bool cmpWholeProg)
{
	if (cmpWholeProg)
	{
		if (memcmp((u8*)prog.data, mVU.regs().Micro, mVU.microMemSize))
			return false;
	}
	else
	{
		for (const auto& range : *prog.ranges)
		{
			auto cmpOffset = [&](void* x) { return (u8*)x + range.start; };
			if ((range.start < 0) || (range.end < 0))
				DevCon.Error("microVU%d: Negative Range![%d][%d]", mVU.index, range.start, range.end);
			if (memcmp(cmpOffset(prog.data), cmpOffset(mVU.regs().Micro), (range.end - range.start)))
				return false;
		}
	}
	mVU.prog.cleared = 0;
	mVU.prog.cur = &prog;
	mVU.prog.isSame = cmpWholeProg ? 1 : -1;
	return true;
}

// Searches for Cached Micro Program and sets prog.cur to it (returns entry-point to program)
_mVUt __fi void* mVUsearchProg(u32 startPC, uptr pState)
{
	microVU& mVU = mVUx;
	microProgramQuick& quick = mVU.prog.quick[mVU.regs().start_pc / 8];
	microProgramList*  list  = mVU.prog.prog [mVU.regs().start_pc / 8];

	if (!quick.prog) // If null, we need to search for new program
	{
		std::deque<microProgram*>::iterator it(list->begin());
		for (; it != list->end(); ++it)
		{
			bool b = mVUcmpProg(mVU, *it[0], 0);

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

//------------------------------------------------------------------
// recMicroVU0 / recMicroVU1
//------------------------------------------------------------------
recMicroVU0::recMicroVU0() { m_Idx = 0; IsInterpreter = false; }
recMicroVU1::recMicroVU1() { m_Idx = 1; IsInterpreter = false; }

void recMicroVU0::Reserve()
{
	if (m_Reserved.exchange(1) == 0)
		mVUinit(microVU0, 0);
}
void recMicroVU1::Reserve()
{
	if (m_Reserved.exchange(1) == 0)
	{
		mVUinit(microVU1, 1);
		vu1Thread.Open();
	}
}

void recMicroVU0::Shutdown() noexcept
{
	if (m_Reserved.exchange(0) == 1)
		mVUclose(microVU0);
}
void recMicroVU1::Shutdown() noexcept
{
	if (m_Reserved.exchange(0) == 1)
	{
		vu1Thread.WaitVU();
		mVUclose(microVU1);
	}
}

void recMicroVU0::Reset()
{
	if (!pxAssertDev(m_Reserved, "MicroVU0 CPU Provider has not been reserved prior to reset!"))
		return;
	mVUreset(microVU0, true);
}
void recMicroVU1::Reset()
{
	if (!pxAssertDev(m_Reserved, "MicroVU1 CPU Provider has not been reserved prior to reset!"))
		return;
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
	pxAssert(m_Reserved); // please allocate me first! :|

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

void recMicroVU1::Execute(u32 cycles)
{
	pxAssert(m_Reserved); // please allocate me first! :|

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
	pxAssert(m_Reserved); // please allocate me first! :|
	mVUclear(microVU0, addr, size);
}
void recMicroVU1::Clear(u32 addr, u32 size)
{
	pxAssert(m_Reserved); // please allocate me first! :|
	mVUclear(microVU1, addr, size);
}

uint recMicroVU0::GetCacheReserve() const
{
	return microVU0.cacheSize;
}
uint recMicroVU1::GetCacheReserve() const
{
	return microVU1.cacheSize;
}

void recMicroVU0::SetCacheReserve(uint reserveInMegs) const
{
	DevCon.WriteLn("microVU0: Changing cache size [%dmb]", reserveInMegs);
	microVU0.cacheSize = std::min(reserveInMegs, mVUcacheReserve);
	safe_delete(microVU0.cache_reserve); // I assume this unmaps the memory
	mVUreserveCache(microVU0); // Need rec-reset after this
}
void recMicroVU1::SetCacheReserve(uint reserveInMegs) const
{
	DevCon.WriteLn("microVU1: Changing cache size [%dmb]", reserveInMegs);
	microVU1.cacheSize = std::min(reserveInMegs, mVUcacheReserve);
	safe_delete(microVU1.cache_reserve); // I assume this unmaps the memory
	mVUreserveCache(microVU1); // Need rec-reset after this
}

void recMicroVU1::ResumeXGkick()
{
	pxAssert(m_Reserved); // please allocate me first! :|

	if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
		return;
	((mVUrecCallXG)microVU1.startFunctXG)();
}

void SaveStateBase::vuJITFreeze()
{
	if (IsSaving())
		vu1Thread.WaitVU();

	Freeze(microVU0.prog.lpState);
	Freeze(microVU1.prog.lpState);
}
