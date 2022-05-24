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


#include "PrecompiledHeader.h"
#include "Common.h"

#include "R5900OpcodeTables.h"
#include "R5900Exceptions.h"
#ifndef PCSX2_CORE
#include "gui/SysThreads.h"
#else
#include "VMManager.h"
#endif

#include "Elfheader.h"

#include "DebugTools/Breakpoints.h"

#include <float.h>

using namespace R5900;		// for OPCODE and OpcodeImpl

extern int vu0branch, vu1branch;

static int branch2 = 0;
static u32 cpuBlockCycles = 0;		// 3 bit fixed point version of cycle count
static std::string disOut;
static bool intExitExecution = false;

static void intEventTest();

// These macros are used to assemble the repassembler functions

void intBreakpoint(bool memcheck)
{
	u32 pc = cpuRegs.pc;
 	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_EE, pc) != 0)
		return;

	if (!memcheck)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_EE, pc);
		if (cond && !cond->Evaluate())
			return;
	}

	CBreakPoints::SetBreakpointTriggered(true);
#ifndef PCSX2_CORE
	GetCoreThread().PauseSelfDebug();
#endif
	throw Exception::ExitCpuExecute();
}

void intMemcheck(u32 op, u32 bits, bool store)
{
	// compute accessed address
	u32 start = cpuRegs.GPR.r[(op >> 21) & 0x1F].UD[0];
	if ((s16)op != 0)
		start += (s16)op;
	if (bits == 128)
		start &= ~0x0F;

	start = standardizeBreakpointAddress(start);
	u32 end = start + bits/8;
	
	auto checks = CBreakPoints::GetMemChecks();
	for (size_t i = 0; i < checks.size(); i++)
	{
		auto& check = checks[i];

		if (check.cpu != BREAKPOINT_EE)
			continue;
		if (check.result == 0)
			continue;
		if ((check.cond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((check.cond & MEMCHECK_READ) == 0 && !store)
			continue;

		if (start < check.end && check.start < end)
			intBreakpoint(true);
	}
}

void intCheckMemcheck()
{
	u32 pc = cpuRegs.pc;
	int needed = isMemcheckNeeded(pc);
	if (needed == 0)
		return;

	u32 op = memRead32(needed == 2 ? pc+4 : pc);
	const OPCODE& opcode = GetInstruction(op);

	bool store = (opcode.flags & IS_STORE) != 0;
	switch (opcode.flags & MEMTYPE_MASK)
	{
	case MEMTYPE_BYTE:
		intMemcheck(op,8,store);
		break;
	case MEMTYPE_HALF:
		intMemcheck(op,16,store);
		break;
	case MEMTYPE_WORD:
		intMemcheck(op,32,store);
		break;
	case MEMTYPE_DWORD:
		intMemcheck(op,64,store);
		break;
	case MEMTYPE_QWORD:
		intMemcheck(op,128,store);
		break;
	}
}

static void execI()
{
	// execI is called for every instruction so it must remains as light as possible.
	// If you enable the next define, Interpreter will be much slower (around
	// ~4fps on 3.9GHz Haswell vs ~8fps (even 10fps on dev build))
	// Extra note: due to some cycle count issue PCSX2's internal debugger is
	// not yet usable with the interpreter
//#define EXTRA_DEBUG
#if defined(EXTRA_DEBUG) || defined(PCSX2_DEVBUILD)
	// check if any breakpoints or memchecks are triggered by this instruction
	if (isBreakpointNeeded(cpuRegs.pc))
		intBreakpoint(false);

	intCheckMemcheck();
#endif

	u32 pc = cpuRegs.pc;
	// We need to increase the pc before executing the memRead32. An exception could appears
	// and it expects the PC counter to be pre-incremented
	cpuRegs.pc += 4;

	// interprete instruction
	cpuRegs.code = memRead32( pc );

	const OPCODE& opcode = GetCurrentInstruction();
#if 0
	static long int runs = 0;
	//use this to find out what opcodes your game uses. very slow! (rama)
	runs++;
	if (runs > 1599999999){ //leave some time to startup the testgame
		if (opcode.Name[0] == 'L') { //find all opcodes beginning with "L"
			Console.WriteLn ("Load %s", opcode.Name);
		}
	}
#endif

#if 0
	static long int print_me = 0;
	// Based on cycle
	// if( cpuRegs.cycle > 0x4f24d714 )
	// Or dump from a particular PC (useful to debug handler/syscall)
	if (pc == 0x80000000) {
		print_me = 2000;
	}
	if (print_me) {
		print_me--;
		disOut.clear();
		disR5900Fasm(disOut, cpuRegs.code, pc);
		CPU_LOG( disOut.c_str() );
	}
#endif


	cpuBlockCycles += opcode.cycles;

	opcode.interpret();
}

static __fi void _doBranch_shared(u32 tar)
{
	branch2 = cpuRegs.branch = 1;
	execI();

	// branch being 0 means an exception was thrown, since only the exception
	// handler should ever clear it.

	if( cpuRegs.branch != 0 )
	{
		cpuRegs.pc = tar;
		cpuRegs.branch = 0;
	}
}

static void doBranch( u32 target )
{
	_doBranch_shared( target );
	cpuRegs.cycle += cpuBlockCycles >> 3;
	cpuBlockCycles &= (1<<3)-1;
	intEventTest();
}

void intDoBranch(u32 target)
{
	//Console.WriteLn("Interpreter Branch ");
	_doBranch_shared( target );

	if( Cpu == &intCpu )
	{
		cpuRegs.cycle += cpuBlockCycles >> 3;
		cpuBlockCycles &= (1<<3)-1;
		intEventTest();
	}
}

void intSetBranch()
{
	branch2 = /*cpuRegs.branch =*/ 1;
}

////////////////////////////////////////////////////////////////////
// R5900 Branching Instructions!
// These are the interpreter versions of the branch instructions.  Unlike other
// types of interpreter instructions which can be called safely from the recompilers,
// these instructions are not "recSafe" because they may not invoke the
// necessary branch test logic that the recs need to maintain sync with the
// cpuRegs.pc and delaySlot instruction and such.

namespace R5900 {
namespace Interpreter {
namespace OpcodeImpl {

/*********************************************************
* Jump to target                                         *
* Format:  OP target                                     *
*********************************************************/
// fixme: looking at the other branching code, shouldn't those _SetLinks in BGEZAL and such only be set
// if the condition is true? --arcum42

void J()
{
	doBranch(_JumpTarget_);
}

void JAL()
{
	// 0x3563b8 is the start address of the function that invalidate entry in TLB cache
	if (EmuConfig.Gamefixes.GoemonTlbHack) {
		if (_JumpTarget_ == 0x3563b8)
			GoemonUnloadTlb(cpuRegs.GPR.n.a0.UL[0]);
	}
	_SetLink(31);
	doBranch(_JumpTarget_);
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, rt, offset                             *
*********************************************************/

void BEQ()  // Branch if Rs == Rt
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
		intEventTest();
}

void BNE()  // Branch if Rs != Rt
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0])
		doBranch(_BranchTarget_);
	else
		intEventTest();
}

/*********************************************************
* Register branch logic                                  *
* Format:  OP rs, offset                                 *
*********************************************************/

void BGEZ()    // Branch if Rs >= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BGEZAL() // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if (cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BGTZ()    // Branch if Rs >  0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] > 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BLEZ()   // Branch if Rs <= 0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] <= 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BLTZ()    // Branch if Rs <  0
{
	if (cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
}

void BLTZAL()  // Branch if Rs <  0 and link
{
	_SetLink(31);
	if (cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
}

/*********************************************************
* Register branch logic  Likely                          *
* Format:  OP rs, offset                                 *
*********************************************************/


void BEQL()    // Branch if Rs == Rt
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] == cpuRegs.GPR.r[_Rt_].SD[0])
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BNEL()     // Branch if Rs != Rt
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] != cpuRegs.GPR.r[_Rt_].SD[0])
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLEZL()    // Branch if Rs <= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] <= 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGTZL()     // Branch if Rs >  0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] > 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLTZL()     // Branch if Rs <  0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGEZL()     // Branch if Rs >= 0
{
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BLTZALL()   // Branch if Rs <  0 and link
{
	_SetLink(31);
	if(cpuRegs.GPR.r[_Rs_].SD[0] < 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

void BGEZALL()   // Branch if Rs >= 0 and link
{
	_SetLink(31);
	if(cpuRegs.GPR.r[_Rs_].SD[0] >= 0)
	{
		doBranch(_BranchTarget_);
	}
	else
	{
		cpuRegs.pc +=4;
		intEventTest();
	}
}

/*********************************************************
* Register jump                                          *
* Format:  OP rs, rd                                     *
*********************************************************/
void JR()
{
	// 0x33ad48 and 0x35060c are the return address of the function (0x356250) that populate the TLB cache
	if (EmuConfig.Gamefixes.GoemonTlbHack) {
		u32 add = cpuRegs.GPR.r[_Rs_].UL[0];
		if (add == 0x33ad48 || add == 0x35060c)
			GoemonPreloadTlb();
	}
	doBranch(cpuRegs.GPR.r[_Rs_].UL[0]);
}

void JALR()
{
	u32 temp = cpuRegs.GPR.r[_Rs_].UL[0];

	if (_Rd_)  _SetLink(_Rd_);

	doBranch(temp);
}

} } }		// end namespace R5900::Interpreter::OpcodeImpl


// --------------------------------------------------------------------------------------
//  R5900cpu/intCpu interface (implementations)
// --------------------------------------------------------------------------------------

static void intReserve()
{
	// fixme : detect cpu for use the optimize asm code
}

static void intAlloc()
{
	// Nothing to do!
}

static void intReset()
{
	cpuRegs.branch = 0;
	branch2 = 0;
}

static void intEventTest()
{
	// Perform counters, ints, and IOP updates:
	_cpuEventTest_Shared();
	
	if (intExitExecution)
	{
		intExitExecution = false;
		throw Exception::ExitCpuExecute();
	}
}

static void intExecute()
{
	bool instruction_was_cancelled;
	enum ExecuteState {
		RESET,
		GAME_LOADING,
		GAME_RUNNING
	};
	ExecuteState state = RESET;
	do {
		instruction_was_cancelled = false;
		try {
			// The execution was splited in three parts so it is easier to
			// resume it after a cancelled instruction.
			switch (state) {
				case RESET:
					do
						execI();
					while (cpuRegs.pc != (g_eeloadMain ? g_eeloadMain : EELOAD_START));
					if (cpuRegs.pc == EELOAD_START)
					{
						// The EELOAD _start function is the same across all BIOS versions afaik
						u32 mainjump = memRead32(EELOAD_START + 0x9c);
						if (mainjump >> 26 == 3) // JAL
							g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | (mainjump << 2 & 0x0fffffffU);
					}
					else if (cpuRegs.pc == g_eeloadMain)
					{
						eeloadHook();
						if (g_SkipBiosHack)
						{
							// See comments on this code in iR5900-32.cpp's recRecompile()
							u32 typeAexecjump = memRead32(EELOAD_START + 0x470);
							u32 typeBexecjump = memRead32(EELOAD_START + 0x5B0);
							u32 typeCexecjump = memRead32(EELOAD_START + 0x618);
							u32 typeDexecjump = memRead32(EELOAD_START + 0x600);
							if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3)) // JAL to 0x822B8
								g_eeloadExec = EELOAD_START + 0x2B8;
							else if (typeAexecjump >> 26 == 3) // JAL to 0x82170
								g_eeloadExec = EELOAD_START + 0x170;
							else
								Console.WriteLn("intExecute: Could not enable launch arguments for fast boot mode; unidentified BIOS version! Please report this to the PCSX2 developers.");
						}
					}
					else if (cpuRegs.pc == g_eeloadExec)
						eeloadHook2();

					if (g_GameLoading)
						state = GAME_LOADING;
					else
						break;

				case GAME_LOADING:
					if (ElfEntry != 0xFFFFFFFF) {
						do
							execI();
						while (cpuRegs.pc != ElfEntry);
						eeGameStarting();
					}
					state = GAME_RUNNING;

				case GAME_RUNNING:
					while (true)
						execI();
			}
		}
		catch( Exception::ExitCpuExecute& ) { }
		catch( Exception::CancelInstruction& ) { instruction_was_cancelled = true; }

		// For example a tlb miss will throw an exception. Cpu must be resumed
		// to execute the handler
	} while (instruction_was_cancelled);
}

static void intSafeExitExecution()
{
	// If we're currently processing events, we can't safely jump out of the interpreter here, because we'll
	// leave things in an inconsistent state. So instead, we flag it for exiting once cpuEventTest() returns.
	if (eeEventTestIsActive)
		intExitExecution = true;
	else
		throw Exception::ExitCpuExecute();
}

static void intStep()
{
	execI();
}

static void intClear(u32 Addr, u32 Size)
{
}

static void intShutdown() {
}

static void intThrowException( const BaseR5900Exception& ex )
{
	// No tricks needed; C++ stack unwnding should suffice for MSW and GCC alike.
	ex.Rethrow();
}

static void intThrowException( const BaseException& ex )
{
	// No tricks needed; C++ stack unwnding should suffice for MSW and GCC alike.
	ex.Rethrow();
}

static void intSetCacheReserve( uint reserveInMegs )
{
}

static uint intGetCacheReserve()
{
	return 0;
}

R5900cpu intCpu =
{
	intReserve,
	intShutdown,

	intReset,
	intStep,
	intExecute,

	intSafeExitExecution,
	intThrowException,
	intThrowException,
	intClear,

	intGetCacheReserve,
	intSetCacheReserve,
};
