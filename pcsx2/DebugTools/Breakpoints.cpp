/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "Breakpoints.h"
#include "SymbolMap.h"
#include "MIPSAnalyst.h"
#include <cstdio>
#include "../R5900.h"
#include "../System.h"

std::vector<BreakPoint> CBreakPoints::breakPoints_;
u32 CBreakPoints::breakSkipFirstAt_ = 0;
u64 CBreakPoints::breakSkipFirstTicks_ = 0;
std::vector<MemCheck> CBreakPoints::memChecks_;
std::vector<MemCheck *> CBreakPoints::cleanupMemChecks_;
bool CBreakPoints::breakpointTriggered_ = false;

// called from the dynarec
u32 __fastcall standardizeBreakpointAddress(u32 addr)
{
	if (addr >= 0xFFFF8000)
		return addr;

	if (addr >= 0xBFC00000 && addr <= 0xBFFFFFFF)
		addr &= 0x1FFFFFFF;

	addr &= 0x7FFFFFFF;
	
	if ((addr >> 28) == 2 || (addr >> 28) == 3)
		addr &= ~(0xF << 28);
	
	return addr;
}

MemCheck::MemCheck() :
	start(0),
	end(0),
	cond(MEMCHECK_READWRITE),
	result(MEMCHECK_BOTH),
	lastPC(0),
	lastAddr(0),
	lastSize(0)
{
	numHits = 0;
}

void MemCheck::Log(u32 addr, bool write, int size, u32 pc)
{
}

void MemCheck::Action(u32 addr, bool write, int size, u32 pc)
{
	int mask = write ? MEMCHECK_WRITE : MEMCHECK_READ;
	if (cond & mask)
	{
		++numHits;

		Log(addr, write, size, pc);
		if (result & MEMCHECK_BREAK)
		{
		//	Core_EnableStepping(true);
		//	host->SetDebugMode(true);
		}
	}
}

void MemCheck::JitBefore(u32 addr, bool write, int size, u32 pc)
{
	int mask = MEMCHECK_WRITE | MEMCHECK_WRITE_ONCHANGE;
	if (write && (cond & mask) == mask)
	{
		lastAddr = addr;
		lastPC = pc;
		lastSize = size;

		// We have to break to find out if it changed.
		//Core_EnableStepping(true);
	}
	else
	{
		lastAddr = 0;
		Action(addr, write, size, pc);
	}
}

void MemCheck::JitCleanup()
{
	if (lastAddr == 0 || lastPC == 0)
		return;
	/*
	// Here's the tricky part: would this have changed memory?
	// Note that it did not actually get written.
	bool changed = MIPSAnalyst::OpWouldChangeMemory(lastPC, lastAddr);
	if (changed)
	{
		++numHits;
		Log(lastAddr, true, lastSize, lastPC);
	}

	// Resume if it should not have gone to stepping, or if it did not change.
	if ((!(result & MEMCHECK_BREAK) || !changed) && coreState == CORE_STEPPING)
	{
		CBreakPoints::SetSkipFirst(lastPC);
		Core_EnableStepping(false);
	}
	else
		host->SetDebugMode(true);*/
}

size_t CBreakPoints::FindBreakpoint(u32 addr, bool matchTemp, bool temp)
{
	addr = standardizeBreakpointAddress(addr);

	for (size_t i = 0; i < breakPoints_.size(); ++i)
	{
		u32 cmp = standardizeBreakpointAddress(breakPoints_[i].addr);
		if (cmp == addr && (!matchTemp || breakPoints_[i].temporary == temp))
			return i;
	}

	return INVALID_BREAKPOINT;
}

size_t CBreakPoints::FindMemCheck(u32 start, u32 end)
{
	start = standardizeBreakpointAddress(start);
	end = standardizeBreakpointAddress(end);

	for (size_t i = 0; i < memChecks_.size(); ++i)
	{
		u32 cmpStart = standardizeBreakpointAddress(memChecks_[i].start);
		u32 cmpEnd = standardizeBreakpointAddress(memChecks_[i].end);
		if (cmpStart == start && cmpEnd == end)
			return i;
	}

	return INVALID_MEMCHECK;
}

bool CBreakPoints::IsAddressBreakPoint(u32 addr)
{
	size_t bp = FindBreakpoint(addr);
	return bp != INVALID_BREAKPOINT && breakPoints_[bp].enabled;
}

bool CBreakPoints::IsAddressBreakPoint(u32 addr, bool* enabled)
{
	size_t bp = FindBreakpoint(addr);
	if (bp == INVALID_BREAKPOINT) return false;
	if (enabled != NULL) *enabled = breakPoints_[bp].enabled;
	return true;
}

bool CBreakPoints::IsTempBreakPoint(u32 addr)
{
	size_t bp = FindBreakpoint(addr, true, true);
	return bp != INVALID_BREAKPOINT;
}

void CBreakPoints::AddBreakPoint(u32 addr, bool temp)
{
	size_t bp = FindBreakpoint(addr, true, temp);
	if (bp == INVALID_BREAKPOINT)
	{
		BreakPoint pt;
		pt.enabled = true;
		pt.temporary = temp;
		pt.addr = addr;

		breakPoints_.push_back(pt);
		Update(addr);
	}
	else if (!breakPoints_[bp].enabled)
	{
		breakPoints_[bp].enabled = true;
		breakPoints_[bp].hasCond = false;
		Update(addr);
	}
}

void CBreakPoints::RemoveBreakPoint(u32 addr)
{
	size_t bp = FindBreakpoint(addr);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_.erase(breakPoints_.begin() + bp);

		// Check again, there might've been an overlapping temp breakpoint.
		bp = FindBreakpoint(addr);
		if (bp != INVALID_BREAKPOINT)
			breakPoints_.erase(breakPoints_.begin() + bp);

		Update(addr);
	}
}

void CBreakPoints::ChangeBreakPoint(u32 addr, bool status)
{
	size_t bp = FindBreakpoint(addr);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].enabled = status;
		Update(addr);
	}
}

void CBreakPoints::ClearAllBreakPoints()
{
	if (!breakPoints_.empty())
	{
		breakPoints_.clear();
		Update();
	}
}

void CBreakPoints::ClearTemporaryBreakPoints()
{
	if (breakPoints_.empty())
		return;

	for (int i = (int)breakPoints_.size()-1; i >= 0; --i)
	{
		if (breakPoints_[i].temporary)
		{
			Update(breakPoints_[i].addr);
			breakPoints_.erase(breakPoints_.begin() + i);
		}
	}
}

void CBreakPoints::ChangeBreakPointAddCond(u32 addr, const BreakPointCond &cond)
{
	size_t bp = FindBreakpoint(addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].hasCond = true;
		breakPoints_[bp].cond = cond;
		Update();
	}
}

void CBreakPoints::ChangeBreakPointRemoveCond(u32 addr)
{
	size_t bp = FindBreakpoint(addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].hasCond = false;
		Update();
	}
}

BreakPointCond *CBreakPoints::GetBreakPointCondition(u32 addr)
{
	size_t bp = FindBreakpoint(addr, true, false);
	if (bp != INVALID_BREAKPOINT && breakPoints_[bp].hasCond)
		return &breakPoints_[bp].cond;
	return NULL;
}

void CBreakPoints::AddMemCheck(u32 start, u32 end, MemCheckCondition cond, MemCheckResult result)
{
	// This will ruin any pending memchecks.
	cleanupMemChecks_.clear();

	size_t mc = FindMemCheck(start, end);
	if (mc == INVALID_MEMCHECK)
	{
		MemCheck check;
		check.start = start;
		check.end = end;
		check.cond = cond;
		check.result = result;

		memChecks_.push_back(check);
		Update();
	}
	else
	{
		memChecks_[mc].cond = (MemCheckCondition)(memChecks_[mc].cond | cond);
		memChecks_[mc].result = (MemCheckResult)(memChecks_[mc].result | result);
		Update();
	}
}

void CBreakPoints::RemoveMemCheck(u32 start, u32 end)
{
	// This will ruin any pending memchecks.
	cleanupMemChecks_.clear();

	size_t mc = FindMemCheck(start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_.erase(memChecks_.begin() + mc);
		Update();
	}
}

void CBreakPoints::ChangeMemCheck(u32 start, u32 end, MemCheckCondition cond, MemCheckResult result)
{
	size_t mc = FindMemCheck(start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].cond = cond;
		memChecks_[mc].result = result;
		Update();
	}
}

void CBreakPoints::ClearAllMemChecks()
{
	// This will ruin any pending memchecks.
	cleanupMemChecks_.clear();

	if (!memChecks_.empty())
	{
		memChecks_.clear();
		Update();
	}
}

void CBreakPoints::SetSkipFirst(u32 pc)
{
	breakSkipFirstAt_ = standardizeBreakpointAddress(pc);
	breakSkipFirstTicks_ = r5900Debug.getCycles();
}

u32 CBreakPoints::CheckSkipFirst(u32 cmpPc)
{
	cmpPc = standardizeBreakpointAddress(cmpPc);
	u32 pc = breakSkipFirstAt_;
	if (breakSkipFirstTicks_ == r5900Debug.getCycles())
		return pc;
	return 0;
}

const std::vector<MemCheck> CBreakPoints::GetMemCheckRanges()
{
	std::vector<MemCheck> ranges = memChecks_;
	for (auto it = memChecks_.begin(), end = memChecks_.end(); it != end; ++it)
	{
		MemCheck check = *it;
		// Toggle the cached part of the address.
		check.start ^= 0x40000000;
		if (check.end != 0)
			check.end ^= 0x40000000;
		ranges.push_back(check);
	}

	return ranges;
}

const std::vector<MemCheck> CBreakPoints::GetMemChecks()
{
	return memChecks_;
}

const std::vector<BreakPoint> CBreakPoints::GetBreakpoints()
{
	return breakPoints_;
}

// including them earlier causes some ambiguities
#include "App.h"
#include "Debugger/DisassemblyDialog.h"

void CBreakPoints::Update(u32 addr)
{
	bool resume = false;
	if (r5900Debug.isCpuPaused() == false)
	{
		r5900Debug.pauseCpu();
		resume = true;
	}
	
//	if (addr != 0)
//		Cpu->Clear(addr-4,8);
//	else
		SysClearExecutionCache();
	
	if (resume)
		r5900Debug.resumeCpu();
	auto disassembly_window = wxGetApp().GetDisassemblyPtr();
	if (disassembly_window) // make sure that valid pointer is recieved to prevent potential NULL dereference.
		disassembly_window->update();
}
