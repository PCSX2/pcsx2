// SPDX-FileCopyrightText: 2012 PPSSPP Project, 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-2.0+

#pragma once

#include <algorithm>
#include <iterator>
#include <vector>

#include "DebugInterface.h"
#include "common/Pcsx2Types.h"

struct BreakPointCond
{
	DebugInterface *debug;
	PostfixExpression expression;
	char expressionString[128];

	BreakPointCond() : debug(NULL)
	{
		expressionString[0] = '\0';
	}

	u32 Evaluate()
	{
		u64 result;
		if (!debug->parseExpression(expression,result) || result == 0) return 0;
		return 1;
	}
};

struct BreakPoint
{
	BreakPoint() : addr(0), enabled(false), temporary(false), hasCond(false)
	{}

	u32	addr;
	bool enabled;
	bool temporary;

	bool hasCond;
	BreakPointCond cond;
	BreakPointCpu cpu;

	bool operator == (const BreakPoint &other) const {
		return addr == other.addr;
	}
	bool operator < (const BreakPoint &other) const {
		return addr < other.addr;
	}
};

enum MemCheckCondition
{
	MEMCHECK_READ = 0x01,
	MEMCHECK_WRITE = 0x02,
	MEMCHECK_WRITE_ONCHANGE = 0x04,

	MEMCHECK_READWRITE = 0x03,
	MEMCHECK_INVALID = 0x08, // Invalid condition, used by the CSV parser to know if the line is for a memcheck
};

enum MemCheckResult
{
	MEMCHECK_IGNORE = 0x00,
	MEMCHECK_LOG = 0x01,
	MEMCHECK_BREAK = 0x02,

	MEMCHECK_BOTH = 0x03,
};

struct MemCheck
{
	MemCheck();
	u32 start;
	u32 end;

	MemCheckCondition cond;
	MemCheckResult result;
	BreakPointCpu cpu;

	u32 numHits;

	u32 lastPC;
	u32 lastAddr;
	int lastSize;

	void Action(u32 addr, bool write, int size, u32 pc);
	void JitBefore(u32 addr, bool write, int size, u32 pc);
	void JitCleanup();

	void Log(u32 addr, bool write, int size, u32 pc);

	bool operator == (const MemCheck &other) const {
		return start == other.start && end == other.end;
	}
};

// BreakPoints cannot overlap, only one is allowed per address.
// MemChecks can overlap, as long as their ends are different.
// WARNING: MemChecks are not used in the interpreter or HLE currently.
class CBreakPoints
{
public:
	static const size_t INVALID_BREAKPOINT = -1;
	static const size_t INVALID_MEMCHECK = -1;

	static bool IsAddressBreakPoint(BreakPointCpu cpu, u32 addr);
	static bool IsAddressBreakPoint(BreakPointCpu cpu, u32 addr, bool* enabled);
	static bool IsTempBreakPoint(BreakPointCpu cpu, u32 addr);
	static void AddBreakPoint(BreakPointCpu cpu, u32 addr, bool temp = false, bool enabled = true);
	static void RemoveBreakPoint(BreakPointCpu cpu, u32 addr);
	static void ChangeBreakPoint(BreakPointCpu cpu, u32 addr, bool enable);
	static void ClearAllBreakPoints();
	static void ClearTemporaryBreakPoints();

	// Makes a copy.  Temporary breakpoints can't have conditions.
	static void ChangeBreakPointAddCond(BreakPointCpu cpu, u32 addr, const BreakPointCond &cond);
	static void ChangeBreakPointRemoveCond(BreakPointCpu cpu, u32 addr);
	static BreakPointCond *GetBreakPointCondition(BreakPointCpu cpu, u32 addr);

	static void AddMemCheck(BreakPointCpu cpu, u32 start, u32 end, MemCheckCondition cond, MemCheckResult result);
	static void RemoveMemCheck(BreakPointCpu cpu, u32 start, u32 end);
	static void ChangeMemCheck(BreakPointCpu cpu, u32 start, u32 end, MemCheckCondition cond, MemCheckResult result);
	static void ClearAllMemChecks();

	static void SetSkipFirst(BreakPointCpu cpu, u32 pc);
	static u32 CheckSkipFirst(BreakPointCpu cpu, u32 pc);
	static void ClearSkipFirst();

	// Includes uncached addresses.
	static const std::vector<MemCheck> GetMemCheckRanges();

	static const std::vector<MemCheck> GetMemChecks(BreakPointCpu cpu);
	static const std::vector<BreakPoint> GetBreakpoints(BreakPointCpu cpu, bool includeTemp);
	// Returns count of all non-temporary breakpoints
	static size_t GetNumBreakpoints()
	{ 
		return std::count_if(breakPoints_.begin(), breakPoints_.end(), [](BreakPoint& bp) { return !bp.temporary; });
	}
	static size_t GetNumMemchecks() { return memChecks_.size(); }

	static void Update(BreakPointCpu cpu = BREAKPOINT_IOP_AND_EE, u32 addr = 0);

	static void SetBreakpointTriggered(bool triggered, BreakPointCpu cpu = BreakPointCpu::BREAKPOINT_IOP_AND_EE) { breakpointTriggered_ = triggered; breakpointTriggeredCpu_ = cpu; };
	static bool GetBreakpointTriggered() { return breakpointTriggered_; };
	static BreakPointCpu GetBreakpointTriggeredCpu() { return breakpointTriggeredCpu_; };

	static bool GetCorePaused() { return corePaused; };
	static void SetCorePaused(bool b) { corePaused = b; };

private:
	static size_t FindBreakpoint(BreakPointCpu cpu, u32 addr, bool matchTemp = false, bool temp = false);
	// Finds exactly, not using a range check.
	static size_t FindMemCheck(BreakPointCpu cpu, u32 start, u32 end);

	static std::vector<BreakPoint> breakPoints_;
	static u32 breakSkipFirstAtEE_;
	static u64 breakSkipFirstTicksEE_;
	static u32 breakSkipFirstAtIop_;
	static u64 breakSkipFirstTicksIop_;

	static bool breakpointTriggered_;
	static BreakPointCpu breakpointTriggeredCpu_;
	static bool corePaused;

	static std::vector<MemCheck> memChecks_;
	static std::vector<MemCheck *> cleanupMemChecks_;
};


// called from the dynarec
u32 standardizeBreakpointAddress(u32 addr);
