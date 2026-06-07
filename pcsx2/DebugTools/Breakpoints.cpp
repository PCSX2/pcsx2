// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Breakpoints.h"
#include "SymbolGuardian.h"
#include "MIPSAnalyst.h"
#include <cstdio>
#include "R5900.h"
#include "R3000A.h"
#include "common/Console.h"

std::vector<BreakPoint> CBreakPoints::breakPoints_;
u32 CBreakPoints::breakSkipFirstAtEE_ = 0;
u32 CBreakPoints::breakSkipFirstAtIop_ = 0;
bool CBreakPoints::pendingClearSkipFirstAtEE_ = false;
bool CBreakPoints::pendingClearSkipFirstAtIop_ = false;
std::vector<MemCheck> CBreakPoints::memChecks_;
std::vector<MemCheck*> CBreakPoints::cleanupMemChecks_;
bool CBreakPoints::breakpointTriggered_ = false;
BreakPointCpu CBreakPoints::breakpointTriggeredCpu_;
bool CBreakPoints::corePaused = false;

// called from the dynarec
u32 standardizeBreakpointAddress(u32 addr)
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

// Parses a format string. Calls onLiteral for strings of literal characters and calls
// onExpression for escaped expressions. An escaped expression is an expression that appears
// between curly braces (e.g. {v0}). Can interpret literal curly braces by double escaping,
// e.g. "{{v0}}" would call onLiteral on "{v0}" rather than onExpression. Ideally this is
// meant to be used how we use it in EvaluateInstrumentationLogFormat, building a literal
// string out of the literal segments and evaluating the expressions for the expression statements.
// Returns false if the brackets are unbalanced (e.g. "{{}" contains an unbalanced set of brackets).
template <typename LiteralFn, typename ExpressionFn>
static bool ParseInstrumentationLogFormat(const std::string& format, std::string& error, LiteralFn onLiteral, ExpressionFn onExpression)
{
	std::string literal;
	for (size_t i = 0; i < format.size(); i++)
	{
		const char c = format[i];
		if (c == '{')
		{
			if (i + 1 < format.size() && format[i + 1] == '{')
			{
				literal += '{';
				i++;
				continue;
			}

			const size_t close = format.find('}', i + 1);
			if (close == std::string::npos)
			{
				error = "Unmatched '{' in log format.";
				return false;
			}

			if (!literal.empty())
			{
				onLiteral(literal);
				literal.clear();
			}

			onExpression(format.substr(i + 1, close - (i + 1)));
			i = close;
		}
		else if (c == '}')
		{
			if (i + 1 < format.size() && format[i + 1] == '}')
			{
				literal += '}';
				i++;
				continue;
			}

			error = "Unmatched '}' in log format.";
			return false;
		}
		else
		{
			literal += c;
		}
	}

	if (!literal.empty())
		onLiteral(literal);

	return true;
}

// Example of evaluated strings:
// format: "hit at PC={pc} ra={ra} a0={a0} a1={a1} v0={v0} sp={sp} mem=[{sp}]={{[{sp}]}} lit={{braces}}"
// out: "hit at PC=0x438AB4 ra=0x438AAC a0=0x0 a1=0x0 v0=0x0 sp=0x1FFFA70 mem=[0x1FFFA70]={[0x1FFFA70]} lit={braces}"
std::string EvaluateInstrumentationLogFormat(DebugInterface& debug, const std::string& format)
{
	std::string out;
	std::string error;
	ParseInstrumentationLogFormat(
		format,
		error,
		[&out](const std::string& literal) { out += literal; },
		[&out, &debug](const std::string& expression) {
			PostfixExpression expr;
			std::string err;
			u64 value;
			if (debug.initExpression(expression.c_str(), expr, err) && debug.parseExpression(expr, value, err))
			{
				char buffer[32];
				std::snprintf(buffer, sizeof(buffer), "0x%llX", static_cast<unsigned long long>(value));
				out += buffer;
			}
			else
			{
				out += "<err>";
			}
		});
	return out;
}

bool ValidateInstrumentationLogFormat(DebugInterface& debug, const std::string& format, std::string& error)
{
	bool areExpressionsValid = true;
	const bool areBracketsBalanced = ParseInstrumentationLogFormat(
		format,
		error,
		[](const std::string&) {},
		[&areExpressionsValid, &error, &debug](const std::string& expression) {
			if (!areExpressionsValid)
				return;
			PostfixExpression expr;
			u64 value;
			if (!debug.initExpression(expression.c_str(), expr, error) ||
				!debug.parseExpression(expr, value, error))
				areExpressionsValid = false;
		});
	return areBracketsBalanced && areExpressionsValid;
}

// Logs an instrumentation message to the console for the breakpoint/memcheck that was just hit.
static void LogInstrumentation(BreakPointCpu cpu, const std::string& logFormat)
{
	if (logFormat.empty())
		return;

	DebugInterface& debug = (cpu == BREAKPOINT_IOP) ? static_cast<DebugInterface&>(r3000Debug)
													: static_cast<DebugInterface&>(r5900Debug);
	const ConsoleColors color = (cpu == BREAKPOINT_IOP) ? Color_Yellow : Color_Cyan;
	Console.WriteLn(color, "%s", EvaluateInstrumentationLogFormat(debug, logFormat).c_str());
}

MemCheck::MemCheck()
	: start(0)
	, end(0)
	, hasCond(false)
	, memCond(MEMCHECK_READWRITE)
	, result(MEMCHECK_BOTH)
	, cpu(BREAKPOINT_EE)
	, maxHits(0)
	, hitsSinceEnabled(0)
	, totalHits(0)
	, instrumentationEnabled(false)
	, continueOnHit(false)
	, lastPC(0)
	, lastAddr(0)
	, lastSize(0)
{
}

void MemCheck::Log(u32 addr, bool write, int size, u32 pc)
{
}

void MemCheck::Action(u32 addr, bool write, int size, u32 pc)
{
	int mask = write ? MEMCHECK_WRITE : MEMCHECK_READ;
	if (memCond & mask)
	{
		++totalHits;

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
	if (write && (memCond & mask) == mask)
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

size_t CBreakPoints::FindBreakpoint(BreakPointCpu cpu, u32 addr, bool matchTemp, bool temp)
{
	if (cpu == BREAKPOINT_EE)
		addr = standardizeBreakpointAddress(addr);

	for (size_t i = 0; i < breakPoints_.size(); ++i)
	{
		u32 cmp = cpu == BREAKPOINT_EE ? standardizeBreakpointAddress(breakPoints_[i].addr) : breakPoints_[i].addr;
		if (cpu == breakPoints_[i].cpu && cmp == addr && (!matchTemp || breakPoints_[i].temporary == temp))
			return i;
	}

	return INVALID_BREAKPOINT;
}

size_t CBreakPoints::FindMemCheck(BreakPointCpu cpu, u32 start, u32 end)
{
	if (cpu == BREAKPOINT_EE)
	{
		start = standardizeBreakpointAddress(start);
		end = standardizeBreakpointAddress(end);
	}

	for (size_t i = 0; i < memChecks_.size(); ++i)
	{
		u32 cmpStart = cpu == BREAKPOINT_EE ? standardizeBreakpointAddress(memChecks_[i].start) : memChecks_[i].start;
		u32 cmpEnd = cpu == BREAKPOINT_EE ? standardizeBreakpointAddress(memChecks_[i].end) : memChecks_[i].end;
		if (memChecks_[i].cpu == cpu && cmpStart == start && cmpEnd == end)
			return i;
	}

	return INVALID_MEMCHECK;
}

bool CBreakPoints::IsAddressBreakPoint(BreakPointCpu cpu, u32 addr)
{
	size_t bp = FindBreakpoint(cpu, addr);
	if (bp != INVALID_BREAKPOINT && breakPoints_[bp].enabled)
		return true;
	// Check again for overlapping temp breakpoint
	bp = FindBreakpoint(cpu, addr, true, true);
	return bp != INVALID_BREAKPOINT && breakPoints_[bp].enabled;
}

bool CBreakPoints::IsAddressBreakPoint(BreakPointCpu cpu, u32 addr, bool* enabled)
{
	const size_t bp = FindBreakpoint(cpu, addr);
	if (bp == INVALID_BREAKPOINT)
		return false;
	if (enabled != NULL)
		*enabled = breakPoints_[bp].enabled;
	return true;
}

bool CBreakPoints::IsTempBreakPoint(BreakPointCpu cpu, u32 addr)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, true);
	return bp != INVALID_BREAKPOINT;
}

bool CBreakPoints::IsSteppingBreakPoint(BreakPointCpu cpu, u32 addr)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, true);
	return bp != INVALID_BREAKPOINT && breakPoints_[bp].stepping;
}

void CBreakPoints::AddBreakPoint(BreakPointCpu cpu, u32 addr, bool temp, bool enabled, bool stepping)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, temp);
	if (bp == INVALID_BREAKPOINT)
	{
		BreakPoint pt;
		pt.enabled = enabled;
		pt.temporary = temp;
		pt.stepping = stepping;
		pt.addr = addr;
		pt.cpu = cpu;

		breakPoints_.push_back(pt);
		Update(cpu, addr);
	}
	else if (!breakPoints_[bp].enabled)
	{
		breakPoints_[bp].enabled = true;
		breakPoints_[bp].hasCond = false;
		Update(cpu, addr);
	}
}

void CBreakPoints::RemoveBreakPoint(BreakPointCpu cpu, u32 addr)
{
	size_t bp = FindBreakpoint(cpu, addr);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_.erase(breakPoints_.begin() + bp);

		// Check again, there might've been an overlapping temp breakpoint.
		bp = FindBreakpoint(cpu, addr);
		if (bp != INVALID_BREAKPOINT)
			breakPoints_.erase(breakPoints_.begin() + bp);

		Update(cpu, addr);
	}
}

void CBreakPoints::ChangeBreakPoint(BreakPointCpu cpu, u32 addr, bool status)
{
	const size_t bp = FindBreakpoint(cpu, addr);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].enabled = status;
		breakPoints_[bp].hitsSinceEnabled = 0;
		Update(cpu, addr);
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

	for (int i = (int)breakPoints_.size() - 1; i >= 0; --i)
	{
		if (breakPoints_[i].temporary)
		{
			Update(breakPoints_[i].cpu, breakPoints_[i].addr);
			breakPoints_.erase(breakPoints_.begin() + i);
		}
	}
}

void CBreakPoints::ChangeBreakPointAddCond(BreakPointCpu cpu, u32 addr, const BreakPointCond& cond)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].hasCond = true;
		breakPoints_[bp].cond = cond;
		Update();
	}
}

void CBreakPoints::ChangeBreakPointRemoveCond(BreakPointCpu cpu, u32 addr)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].hasCond = false;
		Update();
	}
}

BreakPointCond* CBreakPoints::GetBreakPointCondition(BreakPointCpu cpu, u32 addr)
{
	size_t bp = FindBreakpoint(cpu, addr, true, true);
	//temp breakpoints are unconditional
	if (bp != INVALID_BREAKPOINT)
		return NULL;

	bp = FindBreakpoint(cpu, addr, true, false);
	if (bp != INVALID_BREAKPOINT && breakPoints_[bp].hasCond)
		return &breakPoints_[bp].cond;
	return NULL;
}

void CBreakPoints::ChangeBreakPointDescription(BreakPointCpu cpu, u32 addr, const std::string& description)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].description = description;
		Update();
	}
}

void CBreakPoints::ChangeBreakPointMaxHits(BreakPointCpu cpu, u32 addr, u32 maxHits)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].maxHits = maxHits;
		Update();
	}
}

void CBreakPoints::ChangeBreakPointTotalHits(BreakPointCpu cpu, u32 addr, u32 totalHits)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].totalHits = totalHits;
		Update();
	}
}

void CBreakPoints::ChangeBreakPointInstrumentation(BreakPointCpu cpu, u32 addr, bool enabled, const std::string& logFormat, bool continueOnHit)
{
	const size_t bp = FindBreakpoint(cpu, addr, true, false);
	if (bp != INVALID_BREAKPOINT)
	{
		breakPoints_[bp].instrumentationEnabled = enabled;
		breakPoints_[bp].logFormat = logFormat;
		breakPoints_[bp].continueOnHit = continueOnHit;
		Update();
	}
}

bool CBreakPoints::HandleBreakpointHit(BreakPointCpu cpu, u32 addr)
{

	if (IsTempBreakPoint(cpu, addr))
		return true;

	const size_t breakpointIndex = FindBreakpoint(cpu, addr, true, false);
	if (breakpointIndex == INVALID_BREAKPOINT)
		return false;

	BreakPoint& breakpoint = breakPoints_[breakpointIndex];
	breakpoint.hitsSinceEnabled++;
	breakpoint.totalHits++;

	const bool instrumentationEnabled = breakpoint.instrumentationEnabled;
	const std::string logFormat = breakpoint.logFormat;
	const bool continueOnHit = breakpoint.continueOnHit;

	if (instrumentationEnabled)
		LogInstrumentation(cpu, logFormat);

	if(breakpoint.maxHits > 0 && breakpoint.hitsSinceEnabled >= breakpoint.maxHits)
	{
		ChangeBreakPoint(cpu, addr, false);
	}

	if (instrumentationEnabled && continueOnHit)
		return false;

	return true;
}

bool CBreakPoints::HandleMemCheckHit(BreakPointCpu cpu, u32 start, u32 end)
{
	const size_t memCheckIndex = FindMemCheck(cpu, start, end);
	if (memCheckIndex == INVALID_MEMCHECK)
		return false;

	MemCheck& memCheck = memChecks_[memCheckIndex];
	memCheck.hitsSinceEnabled++;
	memCheck.totalHits++;

	const bool instrumentationEnabled = memCheck.instrumentationEnabled;
	const std::string logFormat = memCheck.logFormat;
	const bool continueOnHit = memCheck.continueOnHit;

	if (instrumentationEnabled)
		LogInstrumentation(cpu, logFormat);

	if(memCheck.maxHits > 0 && memCheck.hitsSinceEnabled >= memCheck.maxHits)
	{
		ChangeMemCheck(cpu, start, end, memCheck.memCond, MemCheckResult(memCheck.result & ~MEMCHECK_BREAK));
	}

	if (instrumentationEnabled && continueOnHit)
		return false;

	return true;
}

void CBreakPoints::AddMemCheck(BreakPointCpu cpu, u32 start, u32 end, MemCheckCondition cond, MemCheckResult result)
{
	// This will ruin any pending memchecks.
	cleanupMemChecks_.clear();

	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc == INVALID_MEMCHECK)
	{
		MemCheck check;
		check.start = start;
		check.end = end;
		check.memCond = cond;
		check.result = result;
		check.cpu = cpu;

		memChecks_.push_back(check);
		Update(cpu);
	}
	else
	{
		memChecks_[mc].memCond = (MemCheckCondition)(memChecks_[mc].memCond | cond);
		memChecks_[mc].result = (MemCheckResult)(memChecks_[mc].result | result);
		Update(cpu);
	}
}

void CBreakPoints::RemoveMemCheck(BreakPointCpu cpu, u32 start, u32 end)
{
	// This will ruin any pending memchecks.
	cleanupMemChecks_.clear();

	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_.erase(memChecks_.begin() + mc);
		Update(cpu);
	}
}

void CBreakPoints::ChangeMemCheck(BreakPointCpu cpu, u32 start, u32 end, MemCheckCondition cond, MemCheckResult result)
{
	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].memCond = cond;
		memChecks_[mc].result = result;
		memChecks_[mc].hitsSinceEnabled = 0;
		Update(cpu);
	}
}

void CBreakPoints::ChangeMemCheckRemoveCond(BreakPointCpu cpu, u32 start, u32 end)
{
	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].hasCond = false;
		Update(cpu);
	}
}

void CBreakPoints::ChangeMemCheckAddCond(BreakPointCpu cpu, u32 start, u32 end, const BreakPointCond& cond)
{
	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].hasCond = true;
		memChecks_[mc].cond = cond;
		Update(cpu);
	}
}

void CBreakPoints::ChangeMemCheckDescription(BreakPointCpu cpu, u32 start, u32 end, const std::string& description)
{
	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].description = description;
		Update(cpu);
	}
}

void CBreakPoints::ChangeMemCheckMaxHits(BreakPointCpu cpu, u32 start, u32 end, u32 maxHits)
{
	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].maxHits = maxHits;
		Update(cpu);
	}
}

void CBreakPoints::ChangeMemCheckTotalHits(BreakPointCpu cpu, u32 start, u32 end, u32 totalHits)
{
	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].totalHits = totalHits;
		Update(cpu);
	}
}

void CBreakPoints::ChangeMemCheckInstrumentation(BreakPointCpu cpu, u32 start, u32 end, bool enabled, const std::string& logFormat, bool continueOnHit)
{
	const size_t mc = FindMemCheck(cpu, start, end);
	if (mc != INVALID_MEMCHECK)
	{
		memChecks_[mc].instrumentationEnabled = enabled;
		memChecks_[mc].logFormat = logFormat;
		memChecks_[mc].continueOnHit = continueOnHit;
		Update(cpu);
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

void CBreakPoints::SetSkipFirst(BreakPointCpu cpu, u32 pc)
{
	if (cpu == BREAKPOINT_EE)
	{
		breakSkipFirstAtEE_ = standardizeBreakpointAddress(pc);
		pendingClearSkipFirstAtEE_ = false;
	}
	else if (cpu == BREAKPOINT_IOP)
	{
		breakSkipFirstAtIop_ = pc;
		pendingClearSkipFirstAtIop_ = false;
	}
}

u32 CBreakPoints::CheckSkipFirst(BreakPointCpu cpu, u32 cmpPc)
{
	if (cpu == BREAKPOINT_EE && breakSkipFirstAtEE_ == r5900Debug.getPC())
		return breakSkipFirstAtEE_;
	else if (cpu == BREAKPOINT_IOP && breakSkipFirstAtIop_ == r3000Debug.getPC())
		return breakSkipFirstAtIop_;
	return 0;
}

void CBreakPoints::ClearSkipFirst(BreakPointCpu cpu)
{
	if((cpu & BREAKPOINT_EE) != 0)
		pendingClearSkipFirstAtEE_ = true;
	else if ((cpu & BREAKPOINT_IOP) != 0)
		pendingClearSkipFirstAtIop_ = true;
	
	if(cpu == BREAKPOINT_IOP_AND_EE)
		CommitClearSkipFirst(BREAKPOINT_IOP_AND_EE);
}

void CBreakPoints::CommitClearSkipFirst(BreakPointCpu cpu)
{
	if((cpu & BREAKPOINT_EE) != 0 && pendingClearSkipFirstAtEE_)
	{
		pendingClearSkipFirstAtEE_ = false;
		breakSkipFirstAtEE_ = 0;
	}
	else if ((cpu & BREAKPOINT_IOP) != 0 && pendingClearSkipFirstAtIop_)
	{
		pendingClearSkipFirstAtIop_ = true;
		breakSkipFirstAtIop_ = 0;
	}
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

const std::vector<MemCheck> CBreakPoints::GetMemChecks(BreakPointCpu cpu)
{
	std::vector<MemCheck> memChecks;
	std::copy_if(memChecks_.begin(), memChecks_.end(), std::back_inserter(memChecks), [cpu](MemCheck& mc) { return mc.cpu == cpu; });
	return memChecks;
}

const std::vector<BreakPoint> CBreakPoints::GetBreakpoints(BreakPointCpu cpu, bool includeTemp)
{
	std::vector<BreakPoint> breakPoints;

	std::copy_if(breakPoints_.begin(), breakPoints_.end(), std::back_inserter(breakPoints), [cpu, includeTemp](BreakPoint& bp) {
		if (bp.cpu == cpu)
		{
			if (includeTemp)
				return true;
			else
				return bp.temporary == false;
		}
		else
		{
			return false;
		}
	});

	if (includeTemp)
		return breakPoints_;

	return breakPoints;
}

void CBreakPoints::Update(BreakPointCpu cpu, u32 addr)
{
	bool resume = false;
	if (!r5900Debug.isCpuPaused())
	{
		corePaused = true; // This will be set to false in whatever handles the VM pause event
		r5900Debug.pauseCpu();
		resume = true;
	}

	if (cpu & BREAKPOINT_EE)
	{
		Cpu->Reset();
	}

	if (cpu & BREAKPOINT_IOP)
	{
		psxCpu->Reset();
	}

	if (resume)
		r5900Debug.resumeCpu();
}
