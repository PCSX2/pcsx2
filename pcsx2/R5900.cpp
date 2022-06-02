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

#include "common/StringUtil.h"
#include "ps2/BiosTools.h"
#include "R5900.h"
#include "R3000A.h"
#include "ps2/pgif.h" // pgif init
#include "VUmicro.h"
#include "COP0.h"
#include "MTVU.h"

#ifndef PCSX2_CORE
#include "gui/SysThreads.h"
#else
#include "VMManager.h"
#endif
#include "R5900Exceptions.h"

#include "Hardware.h"
#include "IPU/IPUdma.h"

#include "Elfheader.h"
#include "CDVD/CDVD.h"
#include "USB/USB.h"
#include "Patch.h"
#include "GameDatabase.h"

#include "DebugTools/Breakpoints.h"
#include "DebugTools/MIPSAnalyst.h"
#include "DebugTools/SymbolMap.h"
#include "R5900OpcodeTables.h"

using namespace R5900;	// for R5900 disasm tools

s32 EEsCycle;		// used to sync the IOP to the EE
u32 EEoCycle;

alignas(16) cpuRegisters cpuRegs;
alignas(16) fpuRegisters fpuRegs;
alignas(16) tlbs tlb[48];
R5900cpu *Cpu = NULL;

bool g_SkipBiosHack; // set at boot if the skip bios hack is on, reset before the game has started
bool g_GameStarted; // set when we reach the game's entry point or earlier if the entry point cannot be determined
bool g_GameLoading; // EELOAD has been called to load the game

static const uint eeWaitCycles = 3072;

bool eeEventTestIsActive = false;

u32 g_eeloadMain = 0, g_eeloadExec = 0, g_osdsys_str = 0;

/* I don't know how much space for args there is in the memory block used for args in full boot mode,
but in fast boot mode, the block we use can fit at least 16 argv pointers (varies with BIOS version).
The second EELOAD call during full boot has three built-in arguments ("EELOAD rom0:PS2LOGO <ELF>"),
meaning that only the first 13 game arguments supplied by the user can be added on and passed through.
In fast boot mode, 15 arguments can fit because the only call to EELOAD is "<ELF> <<args>>". */
const int kMaxArgs = 16;
uptr g_argPtrs[kMaxArgs];
#define DEBUG_LAUNCHARG 0 // show lots of helpful console messages as the launch arguments are passed to the game

extern SysMainMemory& GetVmMemory();

void cpuReset()
{
	vu1Thread.WaitVU();
	if (GetMTGS().IsOpen())
		GetMTGS().WaitGS();		// GS better be done processing before we reset the EE, just in case.

	GetVmMemory().ResetAll();

	memzero(cpuRegs);
	memzero(fpuRegs);
	memzero(tlb);

	cpuRegs.pc				= 0xbfc00000; //set pc reg to stack
	cpuRegs.CP0.n.Config	= 0x440;
	cpuRegs.CP0.n.Status.val= 0x70400004; //0x10900000 <-- wrong; // COP0 enabled | BEV = 1 | TS = 1
	cpuRegs.CP0.n.PRid		= 0x00002e20; // PRevID = Revision ID, same as R5900
	fpuRegs.fprc[0]			= 0x00002e30; // fpu Revision..
	fpuRegs.fprc[31]		= 0x01000001; // fpu Status/Control

	g_nextEventCycle = cpuRegs.cycle + 4;
	EEsCycle = 0;
	EEoCycle = cpuRegs.cycle;

	psxReset();
	pgifInit();

	hwReset();

	extern void Deci2Reset();		// lazy, no good header for it yet.
	Deci2Reset();

	g_SkipBiosHack = EmuConfig.UseBOOT2Injection;
	AllowParams1 = !g_SkipBiosHack;
	AllowParams2 = !g_SkipBiosHack;

	ElfCRC = 0;
	DiscSerial.clear();
	ElfEntry = -1;

	// Probably not the right place, but it has to be done when the ram is actually initialized
	USBsetRAM(iopMem->Main);

	// FIXME: LastELF should be reset on media changes as well as on CPU resets, in
	// the very unlikely case that a user swaps to another media source that "looks"
	// the same (identical ELF names) but is actually different (devs actually could
	// run into this while testing minor binary hacked changes to ISO images, which
	// is why I found out about this) --air
	LastELF.clear();

	g_eeloadMain = 0, g_eeloadExec = 0, g_osdsys_str = 0;
}

__ri void cpuException(u32 code, u32 bd)
{
	bool errLevel2, checkStatus;
	u32 offset = 0;

    cpuRegs.branch = 0;		// Tells the interpreter that an exception occurred during a branch.
	cpuRegs.CP0.n.Cause = code & 0xffff;

	if(cpuRegs.CP0.n.Status.b.ERL == 0)
	{
		//Error Level 0-1
		errLevel2 = false;
		checkStatus = (cpuRegs.CP0.n.Status.b.BEV == 0); //  for TLB/general exceptions

		if (((code & 0x7C) >= 0x8) && ((code & 0x7C) <= 0xC))
			offset = 0x0; //TLB Refill
		else if ((code & 0x7C) == 0x0)
			offset = 0x200; //Interrupt
		else
			offset = 0x180; // Everything else
	}
	else
	{
		//Error Level 2
		errLevel2 = true;
		checkStatus = (cpuRegs.CP0.n.Status.b.DEV == 0); // for perf/debug exceptions

		Console.Error("*PCSX2* FIX ME: Level 2 cpuException");
		if ((code & 0x38000) <= 0x8000 )
		{
			//Reset / NMI
			cpuRegs.pc = 0xBFC00000;
			Console.Warning("Reset request");
			cpuUpdateOperationMode();
			return;
		}
		else if((code & 0x38000) == 0x10000)
			offset = 0x80; //Performance Counter
		else if((code & 0x38000) == 0x18000)
			offset = 0x100; //Debug
		else
			Console.Error("Unknown Level 2 Exception!! Cause %x", code);
	}

	if (cpuRegs.CP0.n.Status.b.EXL == 0)
	{
		cpuRegs.CP0.n.Status.b.EXL = 1;
		if (bd)
		{
			Console.Warning("branch delay!!");
			cpuRegs.CP0.n.EPC = cpuRegs.pc - 4;
			cpuRegs.CP0.n.Cause |= 0x80000000;
		}
		else
		{
			cpuRegs.CP0.n.EPC = cpuRegs.pc;
			cpuRegs.CP0.n.Cause &= ~0x80000000;
		}
	}
	else
	{
		offset = 0x180; //Override the cause
		if (errLevel2) Console.Warning("cpuException: Status.EXL = 1 cause %x", code);
	}

	if (checkStatus)
		cpuRegs.pc = 0x80000000 + offset;
	else
		cpuRegs.pc = 0xBFC00200 + offset;

	cpuUpdateOperationMode();
}

void cpuTlbMiss(u32 addr, u32 bd, u32 excode)
{
	// Avoid too much spamming on the interpreter
	if (Cpu != &intCpu || IsDebugBuild) {
		Console.Error("cpuTlbMiss pc:%x, cycl:%x, addr: %x, status=%x, code=%x",
				cpuRegs.pc, cpuRegs.cycle, addr, cpuRegs.CP0.n.Status.val, excode);
	}

	cpuRegs.CP0.n.BadVAddr = addr;
	cpuRegs.CP0.n.Context &= 0xFF80000F;
	cpuRegs.CP0.n.Context |= (addr >> 9) & 0x007FFFF0;
	cpuRegs.CP0.n.EntryHi = (addr & 0xFFFFE000) | (cpuRegs.CP0.n.EntryHi & 0x1FFF);

	cpuRegs.pc -= 4;
	cpuException(excode, bd);
}

void cpuTlbMissR(u32 addr, u32 bd) {
	cpuTlbMiss(addr, bd, EXC_CODE_TLBL);
}

void cpuTlbMissW(u32 addr, u32 bd) {
	cpuTlbMiss(addr, bd, EXC_CODE_TLBS);
}

// sets a branch test to occur some time from an arbitrary starting point.
__fi void cpuSetNextEvent( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't blow up
	// if startCycle is greater than our next branch cycle.

	if( (int)(g_nextEventCycle - startCycle) > delta )
	{
		g_nextEventCycle = startCycle + delta;
	}
}

// sets a branch to occur some time from the current cycle
__fi void cpuSetNextEventDelta( s32 delta )
{
	cpuSetNextEvent( cpuRegs.cycle, delta );
}

// tests the cpu cycle against the given start and delta values.
// Returns true if the delta time has passed.
__fi int cpuTestCycle( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't explode
	// if the startCycle is ahead of our current cpu cycle.

	return (int)(cpuRegs.cycle - startCycle) >= delta;
}

// tells the EE to run the branch test the next time it gets a chance.
__fi void cpuSetEvent()
{
	g_nextEventCycle = cpuRegs.cycle;
}

__fi void cpuClearInt( uint i )
{
	pxAssume( i < 32 );
	cpuRegs.interrupt &= ~(1 << i);
}

static __fi void TESTINT( u8 n, void (*callback)() )
{
	if( !(cpuRegs.interrupt & (1 << n)) ) return;

	if(!g_GameStarted || cpuTestCycle( cpuRegs.sCycle[n], cpuRegs.eCycle[n] ) )
	{
		cpuClearInt( n );
		callback();
	}
	else
		cpuSetNextEvent( cpuRegs.sCycle[n], cpuRegs.eCycle[n] );
}

// [TODO] move this function to Dmac.cpp, and remove most of the DMAC-related headers from
// being included into R5900.cpp.
static __fi void _cpuTestInterrupts()
{
	if (!dmacRegs.ctrl.DMAE || (psHu8(DMAC_ENABLER+2) & 1))
	{
		//Console.Write("DMAC Disabled or suspended");
		return;
	}
	/* These are 'pcsx2 interrupts', they handle asynchronous stuff
	   that depends on the cycle timings */

	TESTINT(DMAC_VIF1,		vif1Interrupt);
	TESTINT(DMAC_GIF,		gifInterrupt);
	TESTINT(DMAC_SIF0,		EEsif0Interrupt);
	TESTINT(DMAC_SIF1,		EEsif1Interrupt);
	// Profile-guided Optimization (sorta)
	// The following ints are rarely called.  Encasing them in a conditional
	// as follows helps speed up most games.

	if (cpuRegs.interrupt & ((1 << DMAC_VIF0) | (1 << DMAC_FROM_IPU) | (1 << DMAC_TO_IPU)
		| (1 << DMAC_FROM_SPR) | (1 << DMAC_TO_SPR) | (1 << DMAC_MFIFO_VIF) | (1 << DMAC_MFIFO_GIF)
		| (1 << VIF_VU0_FINISH) | (1 << VIF_VU1_FINISH) | (1 << IPU_PROCESS)))
	{
		TESTINT(DMAC_VIF0,		vif0Interrupt);

		TESTINT(DMAC_FROM_IPU,	ipu0Interrupt);
		TESTINT(DMAC_TO_IPU,	ipu1Interrupt);
		TESTINT(IPU_PROCESS,	ipuCMDProcess);

		TESTINT(DMAC_FROM_SPR,	SPRFROMinterrupt);
		TESTINT(DMAC_TO_SPR,	SPRTOinterrupt);

		TESTINT(DMAC_MFIFO_VIF, vifMFIFOInterrupt);
		TESTINT(DMAC_MFIFO_GIF, gifMFIFOInterrupt);

		TESTINT(VIF_VU0_FINISH, vif0VUFinish);
		TESTINT(VIF_VU1_FINISH, vif1VUFinish);
	}
}

static __fi void _cpuTestTIMR()
{
	cpuRegs.CP0.n.Count += cpuRegs.cycle-s_iLastCOP0Cycle;
	s_iLastCOP0Cycle = cpuRegs.cycle;

	// fixme: this looks like a hack to make up for the fact that the TIMR
	// doesn't yet have a proper mechanism for setting itself up on a nextEventCycle.
	// A proper fix would schedule the TIMR to trigger at a specific cycle anytime
	// the Count or Compare registers are modified.

	if ( (cpuRegs.CP0.n.Status.val & 0x8000) &&
		cpuRegs.CP0.n.Count >= cpuRegs.CP0.n.Compare && cpuRegs.CP0.n.Count < cpuRegs.CP0.n.Compare+1000 )
	{
		Console.WriteLn( Color_Magenta, "timr intr: %x, %x", cpuRegs.CP0.n.Count, cpuRegs.CP0.n.Compare);
		cpuException(0x808000, cpuRegs.branch);
	}
}

static __fi void _cpuTestPERF()
{
	// Perfs are updated when read by games (COP0's MFC0/MTC0 instructions), so we need
	// only update them at semi-regular intervals to keep cpuRegs.cycle from wrapping
	// around twice on us btween updates.  Hence this function is called from the cpu's
	// Counters update.

	COP0_UpdatePCCR();
}

// Checks the COP0.Status for exception enablings.
// Exception handling for certain modes is *not* currently supported, this function filters
// them out.  Exceptions while the exception handler is active (EIE), or exceptions of any
// level other than 0 are ignored here.

static bool cpuIntsEnabled(int Interrupt)
{
	bool IntType = !!(cpuRegs.CP0.n.Status.val & Interrupt); //Choose either INTC or DMAC, depending on what called it

	return IntType && cpuRegs.CP0.n.Status.b.EIE && cpuRegs.CP0.n.Status.b.IE &&
		!cpuRegs.CP0.n.Status.b.EXL && (cpuRegs.CP0.n.Status.b.ERL == 0);
}

// if cpuRegs.cycle is greater than this cycle, should check cpuEventTest for updates
u32 g_nextEventCycle = 0;

// Shared portion of the branch test, called from both the Interpreter
// and the recompiler.  (moved here to help alleviate redundant code)
__fi void _cpuEventTest_Shared()
{
	eeEventTestIsActive = true;
	g_nextEventCycle = cpuRegs.cycle + eeWaitCycles;

	// ---- INTC / DMAC (CPU-level Exceptions) -----------------
	// Done first because exceptions raised during event tests need to be postponed a few
	// cycles (fixes Grandia II [PAL], which does a spin loop on a vsync and expects to
	// be able to read the value before the exception handler clears it).

	uint mask = intcInterrupt() | dmacInterrupt();
	if (cpuIntsEnabled(mask)) cpuException(mask, cpuRegs.branch);


	// ---- Counters -------------
	// Important: the vsync counter must be the first to be checked.  It includes emulation
	// escape/suspend hooks, and it's really a good idea to suspend/resume emulation before
	// doing any actual meaningful branchtest logic.

	if( cpuTestCycle( nextsCounter, nextCounter ) )
	{
		rcntUpdate();
		_cpuTestPERF();
	}

	rcntUpdate_hScanline();

	_cpuTestTIMR();

	// ---- Interrupts -------------
	// These are basically just DMAC-related events, which also piggy-back the same bits as
	// the PS2's own DMA channel IRQs and IRQ Masks.

	// This is a BIOS hack because the coding in the BIOS is terrible but the bug is masked by Data Cache
	// where a DMA buffer is overwritten without waiting for the transfer to end, which causes the fonts to get all messed up
	// so to fix it, we run all the DMA's instantly when in the BIOS.
	// Only use the lower 17 bits of the cpuRegs.interrupt as the upper bits are for VU0/1 sync which can't be done in a tight loop
	if (!g_GameStarted && dmacRegs.ctrl.DMAE && !(psHu8(DMAC_ENABLER + 2) & 1) && (cpuRegs.interrupt & 0x1FFFF))
	{
		while(cpuRegs.interrupt & 0x1FFFF)
			_cpuTestInterrupts();
	}
	else
		_cpuTestInterrupts();

	// ---- IOP -------------
	// * It's important to run a iopEventTest before calling ExecuteBlock. This
	//   is because the IOP does not always perform branch tests before returning
	//   (during the prev branch) and also so it can act on the state the EE has
	//   given it before executing any code.
	//
	// * The IOP cannot always be run.  If we run IOP code every time through the
	//   cpuEventTest, the IOP generally starts to run way ahead of the EE.

	EEsCycle += cpuRegs.cycle - EEoCycle;
	EEoCycle = cpuRegs.cycle;

	if( EEsCycle > 0 )
		iopEventAction = true;

	iopEventTest();

	if( iopEventAction )
	{
		//if( EEsCycle < -450 )
		//	Console.WriteLn( " IOP ahead by: %d cycles", -EEsCycle );

		EEsCycle = psxCpu->ExecuteBlock( EEsCycle );

		iopEventAction = false;
	}

	// ---- VU Sync -------------
	// We're in a EventTest.  All dynarec registers are flushed
	// so there is no need to freeze registers here.
	CpuVU0->ExecuteBlock();
	CpuVU1->ExecuteBlock();

	// ---- Schedule Next Event Test --------------

	if( EEsCycle > 192 )
	{
		// EE's running way ahead of the IOP still, so we should branch quickly to give the
		// IOP extra timeslices in short order.

		cpuSetNextEventDelta( 48 );
		//Console.Warning( "EE ahead of the IOP -- Rapid Event!  %d", EEsCycle );
	}

	// The IOP could be running ahead/behind of us, so adjust the iop's next branch by its
	// relative position to the EE (via EEsCycle)
	cpuSetNextEventDelta( ((g_iopNextEventCycle-psxRegs.cycle)*8) - EEsCycle );

	// Apply the hsync counter's nextCycle
	cpuSetNextEvent( hsyncCounter.sCycle, hsyncCounter.CycleT );

	// Apply vsync and other counter nextCycles
	cpuSetNextEvent( nextsCounter, nextCounter );

	eeEventTestIsActive = false;
}

__ri void cpuTestINTCInts()
{
	// Check the COP0's Status register for general interrupt disables, and the 0x400
	// bit (which is INTC master toggle).
	if( !cpuIntsEnabled(0x400) ) return;

	if( (psHu32(INTC_STAT) & psHu32(INTC_MASK)) == 0 ) return;

	cpuSetNextEventDelta( 4 );
	if(eeEventTestIsActive && (iopCycleEE > 0))
	{
		iopBreak += iopCycleEE;		// record the number of cycles the IOP didn't run.
		iopCycleEE = 0;
	}
}

__fi void cpuTestDMACInts()
{
	// Check the COP0's Status register for general interrupt disables, and the 0x800
	// bit (which is the DMAC master toggle).
	if( !cpuIntsEnabled(0x800) ) return;

	if ( ( (psHu16(0xe012) & psHu16(0xe010)) == 0) &&
		 ( (psHu16(0xe010) & 0x8000) == 0) ) return;

	cpuSetNextEventDelta( 4 );
	if(eeEventTestIsActive && (iopCycleEE > 0))
	{
		iopBreak += iopCycleEE;		// record the number of cycles the IOP didn't run.
		iopCycleEE = 0;
	}
}

__fi void cpuTestTIMRInts() {
	if ((cpuRegs.CP0.n.Status.val & 0x10007) == 0x10001) {
		_cpuTestPERF();
		_cpuTestTIMR();
	}
}

__fi void cpuTestHwInts() {
	cpuTestINTCInts();
	cpuTestDMACInts();
	cpuTestTIMRInts();
}

__fi void CPU_INT( EE_EventType n, s32 ecycle)
{
	// EE events happen 8 cycles in the future instead of whatever was requested.
	// This can be used on games with PATH3 masking issues for example, or when
	// some FMV look bad.
	if(CHECK_EETIMINGHACK) ecycle = 8;

	cpuRegs.interrupt|= 1 << n;
	cpuRegs.sCycle[n] = cpuRegs.cycle;
	cpuRegs.eCycle[n] = ecycle;

	// Interrupt is happening soon: make sure both EE and IOP are aware.

	if( ecycle <= 28 && iopCycleEE > 0 )
	{
		// If running in the IOP, force it to break immediately into the EE.
		// the EE's branch test is due to run.

		iopBreak += iopCycleEE;		// record the number of cycles the IOP didn't run.
		iopCycleEE = 0;
	}

	cpuSetNextEventDelta( cpuRegs.eCycle[n] );
}

// Called from recompilers; define is mandatory.
void eeGameStarting()
{
	if (!g_GameStarted)
	{
		//Console.WriteLn( Color_Green, "(R5900) ELF Entry point! [addr=0x%08X]", ElfEntry );
		g_GameStarted = true;
		g_GameLoading = false;

		// GameStartingInThread may issue a reset of the cpu and/or recompilers.  Check for and
		// handle such things here:
#ifndef PCSX2_CORE
		GetCoreThread().GameStartingInThread();
		if (GetCoreThread().HasPendingStateChangeRequest())
			Cpu->ExitExecution();
#else
		VMManager::Internal::GameStartingOnCPUThread();
		if (VMManager::Internal::IsExecutionInterrupted())
			Cpu->ExitExecution();
#endif
	}
	else
	{
		Console.WriteLn( Color_Green, "(R5900) Re-executed ELF Entry point (ignored) [addr=0x%08X]", ElfEntry );
	}
}

// Count arguments, save their starting locations, and replace the space separators with null terminators so they're separate strings
int ParseArgumentString(u32 arg_block)
{
	if (!arg_block)
		return 0;

	int argc = 1; // one arg is guaranteed at least
	g_argPtrs[0] = arg_block; // first arg is right here
	bool wasSpace = false; // status of last char. scanned
	int args_len = strlen((char *)PSM(arg_block));
	for (int i = 0; i < args_len; i++)
	{
		char curchar = *(char *)PSM(arg_block + i);
		if (curchar == '\0')
			break; // should never reach this

		bool isSpace = (curchar == ' ');
		if (isSpace)
			memset(PSM(arg_block + i), 0, 1);
		else if (wasSpace) // then we're at a new arg
		{
			if (argc < kMaxArgs)
			{
				g_argPtrs[argc] = arg_block + i;
				argc++;
			}
			else
			{
				Console.WriteLn("ParseArgumentString: Discarded additional arguments beyond the maximum of %d.", kMaxArgs);
				break;
			}
		}
		wasSpace = isSpace;
	}
#if DEBUG_LAUNCHARG
	// Check our args block
	Console.WriteLn("ParseArgumentString: Saving these strings:");
	for (int a = 0; a < argc; a++)
		Console.WriteLn("%p -> '%s'.", g_argPtrs[a], (char *)PSM(g_argPtrs[a]));
#endif
	return argc;
}

// Called from recompilers; define is mandatory.
void eeloadHook()
{
#ifndef PCSX2_CORE
	const std::string elf_override(StringUtil::wxStringToUTF8String(GetCoreThread().GetElfOverride()));
#else
	const std::string& elf_override(VMManager::Internal::GetElfOverride());
#endif

	if (!elf_override.empty())
		cdvdReloadElfInfo(StringUtil::StdStringFromFormat("host:%s", elf_override.c_str()));
	else
		cdvdReloadElfInfo();

	std::string discelf;
	int disctype = GetPS2ElfName(discelf);

	std::string elfname;
	int argc = cpuRegs.GPR.n.a0.SD[0];
	if (argc) // calls to EELOAD *after* the first one during the startup process will come here
	{
#if DEBUG_LAUNCHARG
		Console.WriteLn("eeloadHook: EELOAD was called with %d arguments according to $a0 and %d according to vargs block:",
			argc, memRead32(cpuRegs.GPR.n.a1.UD[0] - 4));
		for (int a = 0; a < argc; a++)
			Console.WriteLn("argv[%d]: %p -> %p -> '%s'", a, cpuRegs.GPR.n.a1.UL[0] + (a * 4),
				memRead32(cpuRegs.GPR.n.a1.UD[0] + (a * 4)), (char *)PSM(memRead32(cpuRegs.GPR.n.a1.UD[0] + (a * 4))));
#endif
		if (argc > 1)
			elfname = (char*)PSM(memRead32(cpuRegs.GPR.n.a1.UD[0] + 4)); // argv[1] in OSDSYS's invocation "EELOAD <game ELF>"

		// This code fires if the user chooses "full boot". First the Sony Computer Entertainment screen appears. This is the result
		// of an EELOAD call that does not want to accept launch arguments (but we patch it to do so in eeloadHook2() in fast boot
		// mode). Then EELOAD is called with the argument "rom0:PS2LOGO". At this point, we do not need any additional tricks
		// because EELOAD is now ready to accept launch arguments. So in full-boot mode, we simply wait for PS2LOGO to be called,
		// then we add the desired launch arguments. PS2LOGO passes those on to the game itself as it calls EELOAD a third time.
		if (!EmuConfig.CurrentGameArgs.empty() && !strcmp(elfname.c_str(), "rom0:PS2LOGO"))
		{
			const char *argString = EmuConfig.CurrentGameArgs.c_str();
			Console.WriteLn("eeloadHook: Supplying launch argument(s) '%s' to module '%s'...", argString, elfname.c_str());

			// Join all arguments by space characters so they can be processed as one string by ParseArgumentString(), then add the
			// user's launch arguments onto the end
			u32 arg_ptr = 0;
			int arg_len = 0;
			for (int a = 0; a < argc; a++)
			{
				arg_ptr = memRead32(cpuRegs.GPR.n.a1.UD[0] + (a * 4));
				arg_len = strlen((char *)PSM(arg_ptr));
				memset(PSM(arg_ptr + arg_len), 0x20, 1);
			}
			strcpy((char *)PSM(arg_ptr + arg_len + 1), EmuConfig.CurrentGameArgs.c_str());
			u32 first_arg_ptr = memRead32(cpuRegs.GPR.n.a1.UD[0]);
#if DEBUG_LAUNCHARG
			Console.WriteLn("eeloadHook: arg block is '%s'.", (char *)PSM(first_arg_ptr));
#endif
			argc = ParseArgumentString(first_arg_ptr);

			// Write pointer to next slot in $a1
			for (int a = 0; a < argc; a++)
				memWrite32(cpuRegs.GPR.n.a1.UD[0] + (a * 4), g_argPtrs[a]);
			cpuRegs.GPR.n.a0.SD[0] = argc;
#if DEBUG_LAUNCHARG
			// Check our work
			Console.WriteLn("eeloadHook: New arguments are:");
			for (int a = 0; a < argc; a++)
				Console.WriteLn("argv[%d]: %p -> '%s'", a, memRead32(cpuRegs.GPR.n.a1.UD[0] + (a * 4)),
				(char *)PSM(memRead32(cpuRegs.GPR.n.a1.UD[0] + (a * 4))));
#endif
		}
		// else it's presumed that the invocation is "EELOAD <game ELF> <<launch args>>", coming from PS2LOGO, and we needn't do
		// anything more
	}
#if DEBUG_LAUNCHARG
	// This code fires in full/fast boot mode when EELOAD is called the first/only time. When EELOAD is not given any arguments,
	// it calls rom0:OSDSYS by default, which displays the Sony Computer Entertainment screen. OSDSYS then calls "EELOAD
	// rom0:PS2LOGO" and we end up above.
	else
		Console.WriteLn("eeloadHook: EELOAD was called with no arguments.");
#endif

	// If "fast boot" was chosen, then on EELOAD's first call we won't yet know what the game's ELF is. Find the name and write it
	// into EELOAD's memory.
	if (g_SkipBiosHack && elfname.empty())
	{
		std::string elftoload;
		if (!elf_override.empty())
		{
			elftoload = StringUtil::StdStringFromFormat("host:%s", elf_override.c_str());
		}
		else
		{
			if (disctype == 2)
				elftoload = discelf;
			else
				g_SkipBiosHack = false; // We're not fast booting, so disable it (Fixes some weirdness with the BIOS)
		}

		// When fast-booting, we insert the game's ELF name into EELOAD so that the game is called instead of the default call of
		// "rom0:OSDSYS"; any launch arguments supplied by the user will be inserted into EELOAD later by eeloadHook2()
		if (!elftoload.empty())
		{
			// Find and save location of default/fallback call "rom0:OSDSYS"; to be used later by eeloadHook2()
			for (g_osdsys_str = EELOAD_START; g_osdsys_str < EELOAD_START + EELOAD_SIZE; g_osdsys_str += 8) // strings are 64-bit aligned
			{
				if (!strcmp((char*)PSM(g_osdsys_str), "rom0:OSDSYS"))
				{
					// Overwrite OSDSYS with game's ELF name
					strcpy((char*)PSM(g_osdsys_str), elftoload.c_str());
					g_GameLoading = true;
					return;
				}
			}
		}
	}

	if (!g_GameStarted && ((disctype == 2 && elfname == discelf) || disctype == 1))
		g_GameLoading = true;
}

// Called from recompilers; define is mandatory.
// Only called if g_SkipBiosHack is true
void eeloadHook2()
{
	if (EmuConfig.CurrentGameArgs.empty())
		return;

	if (!g_osdsys_str)
	{
		Console.WriteLn("eeloadHook2: Called before \"rom0:OSDSYS\" was found by eeloadHook()!");
		return;
	}

	const char *argString = EmuConfig.CurrentGameArgs.c_str();
	Console.WriteLn("eeloadHook2: Supplying launch argument(s) '%s' to ELF '%s'.", argString, (char *)PSM(g_osdsys_str));

	// Add args string after game's ELF name that was written over "rom0:OSDSYS" by eeloadHook(). In between the ELF name and args
	// string we insert a space character so that ParseArgumentString() has one continuous string to process.
	int game_len = strlen((char *)PSM(g_osdsys_str));
	memset(PSM(g_osdsys_str + game_len), 0x20, 1);
	strcpy((char *)PSM(g_osdsys_str + game_len + 1), EmuConfig.CurrentGameArgs.c_str());
#if DEBUG_LAUNCHARG
	Console.WriteLn("eeloadHook2: arg block is '%s'.", (char *)PSM(g_osdsys_str));
#endif
	int argc = ParseArgumentString(g_osdsys_str);
	
	// Back up 4 bytes from start of args block for every arg + 4 bytes for start of argv pointer block, write pointers
	uptr block_start = g_osdsys_str - (argc * 4);
	for (int a = 0; a < argc; a++)
	{
#if DEBUG_LAUNCHARG
		Console.WriteLn("eeloadHook2: Writing address %p to location %p.", g_argPtrs[a], block_start + (a * 4));
#endif
		memWrite32(block_start + (a * 4), g_argPtrs[a]);
	}

	// Save argc and argv as incoming arguments for EELOAD function which calls ExecPS2()
#if DEBUG_LAUNCHARG
	Console.WriteLn("eeloadHook2: Saving %d and %p in $a0 and $a1.", argc, block_start);
#endif
	cpuRegs.GPR.n.a0.SD[0] = argc;
	cpuRegs.GPR.n.a1.UD[0] = block_start;
}

inline bool isBranchOrJump(u32 addr)
{
	u32 op = memRead32(addr);
	const OPCODE& opcode = GetInstruction(op);
	
	// Return false for eret & syscall as they are branch type in pcsx2 debugging tools,
	// but shouldn't have delay slot in isBreakpointNeeded/isMemcheckNeeded.
	if ((opcode.flags == (IS_BRANCH | BRANCHTYPE_SYSCALL)) || (opcode.flags == (IS_BRANCH | BRANCHTYPE_ERET)))
		return false;
		
	return (opcode.flags & IS_BRANCH) != 0;
}

// The next two functions return 0 if no breakpoint is needed,
// 1 if it's needed on the current pc, 2 if it's needed in the delay slot
// 3 if needed in both

int isBreakpointNeeded(u32 addr)
{
	int bpFlags = 0;
	if (CBreakPoints::IsAddressBreakPoint(BREAKPOINT_EE, addr))
		bpFlags += 1;

	// there may be a breakpoint in the delay slot
	if (isBranchOrJump(addr) && CBreakPoints::IsAddressBreakPoint(BREAKPOINT_EE, addr+4))
		bpFlags += 2;

	return bpFlags;
}

int isMemcheckNeeded(u32 pc)
{
	if (CBreakPoints::GetNumMemchecks() == 0)
		return 0;
	
	u32 addr = pc;
	if (isBranchOrJump(addr))
		addr += 4;

	u32 op = memRead32(addr);
	const OPCODE& opcode = GetInstruction(op);

	if (opcode.flags & IS_MEMORY)
		return addr == pc ? 1 : 2;

	return 0;
}
