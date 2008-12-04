/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <stdlib.h>
#include <string.h>

#include "Common.h"
#include "Memory.h"
#include "Hw.h"
#include "DebugTools/Debug.h"
#include "R3000A.h"
#include "VUmicro.h"
#include "GS.h"

#include "Paths.h"

static int inter;

PCSX2_ALIGNED16(cpuRegisters cpuRegs);
PCSX2_ALIGNED16(fpuRegisters fpuRegs);
PCSX2_ALIGNED16(tlbs tlb[48]);
PCSX2_ALIGNED16(GPR_reg64 g_cpuConstRegs[32]) = {0};

u32 g_cpuHasConstReg = 0, g_cpuFlushedConstReg = 0;
R5900cpu *Cpu;

s32 EEsCycle;		// used to sync the IOP to the EE
u32 EEoCycle;
u32 bExecBIOS = 0; // set if the BIOS has already been executed

extern u32 dwSaveVersion;

int cpuInit()
{
	int ret;

	SysPrintf("PCSX2 v" PCSX2_VERSION " save ver: %x\n", dwSaveVersion);
    SysPrintf("EE pc offset: 0x%x, PSX pc offset: 0x%x\n", (u32)&cpuRegs.pc - (u32)&cpuRegs, (u32)&psxRegs.pc - (u32)&psxRegs);

	InitFPUOps();

	cpuRegs.constzero = 0;
#ifdef PCSX2_NORECBUILD
	Cpu = &intCpu;
#else
	cpudetectInit();
	Cpu = CHECK_EEREC ? &recCpu : &intCpu;
#endif

	ret = Cpu->Init();
	if (ret == -1 && CHECK_EEREC) {
		SysMessage(_("Error initializing Recompiler, switching to Interpreter"));
		Config.Options &= ~(PCSX2_EEREC|PCSX2_VU1REC|PCSX2_VU0REC);
		Cpu = &intCpu;
		ret = Cpu->Init();
	}

#ifdef PCSX2_VIRTUAL_MEM
	if (memInit() == -1) {
		PROCESS_INFORMATION pi;
		STARTUPINFO si;
		char strdir[g_MaxPath], strexe[g_MaxPath];
		if( MessageBox(NULL, "Failed to allocate enough physical memory to run pcsx2. Try closing\n"
			"down background programs, restarting windows, or buying more memory.\n\n"
			"Launch TLB version of pcsx2 (pcsx2t.exe)?", "Memory Allocation Error", MB_YESNO) == IDYES ) {

			GetCurrentDirectory(ARRAYSIZE(strdir), strdir);
			//_snprintf(strexe, ARRAYSIZE(strexe), "%s\\pcsx2t.exe", strdir);
			CombinePaths( strexe, strdir, "pcsx2t.exe" );
			memset(&si, 0, sizeof(si));

			if( !CreateProcess(strexe, "", NULL, NULL, FALSE, DETACHED_PROCESS|CREATE_NEW_PROCESS_GROUP, NULL, strdir, &si, &pi)) {
				_snprintf(strdir, ARRAYSIZE(strexe), "Failed to launch %s\n", strexe);
				MessageBox(NULL, strdir, "Failure", MB_OK);
			}
			else {
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
		}

		return -1;
	}
#endif
	if (hwInit() == -1) return -1;
	if (vu0Init() == -1) return -1;
	if (vu1Init() == -1) return -1;
#ifndef PCSX2_VIRTUAL_MEM
	if (memInit() == -1) return -1;
#endif

#ifdef PCSX2_DEVBUILD
	Log = 0;
#endif

	return ret;
}

void cpuReset()
{
	Cpu->Reset();

	memReset();

	memset(&cpuRegs, 0, sizeof(cpuRegs));
	memset(&fpuRegs, 0, sizeof(fpuRegs));
	memset(&tlb, 0, sizeof(tlb));

	cpuRegs.pc = 0xbfc00000; ///set pc reg to stack 
	cpuRegs.CP0.n.Config = 0x440;
	cpuRegs.CP0.n.Status.val = 0x70400004; //0x10900000 <-- wrong; // COP0 enabled | BEV = 1 | TS = 1
	cpuRegs.CP0.n.PRid   = 0x00002e20; // PRevID = Revision ID, same as R5900
	fpuRegs.fprc[0]   = 0x00002e00; // fpu Revision..
	fpuRegs.fprc[31]  = 0x01000001; // fpu Status/Control

	g_nextBranchCycle = cpuRegs.cycle + 2;
	EEsCycle = 0;
	EEoCycle = cpuRegs.cycle;

	vu0Reset();
    vu1Reset();  
	hwReset();
	vif0Reset();
    vif1Reset();
	rcntInit();
	psxReset();
}

void cpuShutdown()
{
	gsShutdown();	// shut down the GS first because it's running a thread.

	hwShutdown();
//	biosShutdown();
	psxShutdown();
	vu0Shutdown();
	vu1Shutdown();
	memShutdown();
	disR5900FreeSyms();

	Cpu->Shutdown();
}

void cpuException(u32 code, u32 bd) {
	u32 offset;
	cpuRegs.CP0.n.Cause = code & 0xffff;

	if(cpuRegs.CP0.n.Status.b.ERL == 0){ //Error Level 0-1
		if(((code & 0x7C) >= 0x8) && ((code & 0x7C) <= 0xC)) offset = 0x0; //TLB Refill
		else if ((code & 0x7C) == 0x0) offset = 0x200; //Interrupt
		else	offset = 0x180; // Everything else
		

		if (cpuRegs.CP0.n.Status.b.EXL == 0) {
			cpuRegs.CP0.n.Status.b.EXL = 1;
			if (bd) {
				SysPrintf("branch delay!!\n");
				cpuRegs.CP0.n.EPC = cpuRegs.pc - 4;
				cpuRegs.CP0.n.Cause |= 0x80000000;
			} else {
				cpuRegs.CP0.n.EPC = cpuRegs.pc;
				cpuRegs.CP0.n.Cause &= ~0x80000000;
			}
		} else {
			offset = 0x180; //Overrride the cause		
			SysPrintf("cpuException: Status.EXL = 1 cause %x\n", code);
		}
		if (cpuRegs.CP0.n.Status.b.BEV == 0) {
			cpuRegs.pc = 0x80000000 + offset;
		} else {
			cpuRegs.pc = 0xBFC00200 + offset;
		}
	} else { //Error Level 2
		SysPrintf("FIX ME: Level 2 cpuException\n");
		if((code & 0x38000) <= 0x8000 ) { //Reset / NMI
			cpuRegs.pc = 0xBFC00000;
			SysPrintf("Reset request\n");
			UpdateCP0Status();
			return;
		} else if((code & 0x38000) == 0x10000) offset = 0x80; //Performance Counter
		else if((code & 0x38000) == 0x18000)  offset = 0x100; //Debug
		else SysPrintf("Unknown Level 2 Exception!! Cause %x\n", code);

		if (cpuRegs.CP0.n.Status.b.EXL == 0) {
			cpuRegs.CP0.n.Status.b.EXL = 1;
			if (bd) {
				SysPrintf("branch delay!!\n");
				cpuRegs.CP0.n.EPC = cpuRegs.pc - 4;
				cpuRegs.CP0.n.Cause |= 0x80000000;
			} else {
				cpuRegs.CP0.n.EPC = cpuRegs.pc;
				cpuRegs.CP0.n.Cause &= ~0x80000000;
			}
		} else {
			offset = 0x180; //Overrride the cause		
			SysPrintf("cpuException: Status.EXL = 1 cause %x\n", code);
		}
		if (cpuRegs.CP0.n.Status.b.DEV == 0) {
			cpuRegs.pc = 0x80000000 + offset;
		} else {
			cpuRegs.pc = 0xBFC00200 + offset;
		}
	}
	UpdateCP0Status();
}

void cpuTlbMiss(u32 addr, u32 bd, u32 excode) {
    SysPrintf("cpuTlbMiss %x, %x, addr: %x, status=%x, code=%x\n", cpuRegs.pc, cpuRegs.cycle, addr, cpuRegs.CP0.n.Status.val, excode);
	if (bd) {
		SysPrintf("branch delay!!\n");
	}

    assert(0); // temporary

	cpuRegs.CP0.n.BadVAddr = addr;
	cpuRegs.CP0.n.Context &= 0xFF80000F;
	cpuRegs.CP0.n.Context |= (addr >> 9) & 0x007FFFF0;
	cpuRegs.CP0.n.EntryHi = (addr & 0xFFFFE000) | (cpuRegs.CP0.n.EntryHi & 0x1FFF);

	cpuRegs.CP0.n.Cause = excode;
	if (!(cpuRegs.CP0.n.Status.val & 0x2)) { // EXL bit
		cpuRegs.CP0.n.EPC = cpuRegs.pc - 4;
	}

	if ((cpuRegs.CP0.n.Status.val & 0x1) == 0) {
		cpuRegs.pc = 0x80000000;
	} else {
		cpuRegs.pc = 0x80000180;
	}

	cpuRegs.CP0.n.Status.b.EXL = 1;
	UpdateCP0Status();
//	Log=1; varLog|= 0x40000000;
}

void cpuTlbMissR(u32 addr, u32 bd) {
	cpuTlbMiss(addr, bd, EXC_CODE_TLBL);
}

void cpuTlbMissW(u32 addr, u32 bd) {
	cpuTlbMiss(addr, bd, EXC_CODE_TLBS);
}

void JumpCheckSym(u32 addr, u32 pc) {
#if 0
//	if (addr == 0x80051770) { SysPrintf("Log!: %s\n", PSM(cpuRegs.GPR.n.a0.UL[0])); Log=1; varLog|= 0x40000000; }
	if (addr == 0x8002f150) { SysPrintf("printk: %s\n", PSM(cpuRegs.GPR.n.a0.UL[0])); }
	if (addr == 0x8002aba0) return;
	if (addr == 0x8002f450) return;
	if (addr == 0x800dd520) return;
//	if (addr == 0x80049300) SysPrintf("register_blkdev: %x\n", cpuRegs.GPR.n.a0.UL[0]);
	if (addr == 0x8013cb70) { SysPrintf("change_root: %x\n", cpuRegs.GPR.n.a0.UL[0]); }
//	if (addr == 0x8013d1e8) { SysPrintf("Log!\n"); Log++; if (Log==2) exit(0); varLog|= 0x40000000; }
//	if (addr == 0x00234e88) { SysPrintf("StoreImage\n"); Log=1; /*psMu32(0x234e88) = 0x03e00008; psMu32(0x234e8c) = 0;*/ }
#endif
/*	if ((pc >= 0x00131D50 &&
		 pc <  0x00132454) ||
		(pc >= 0x00786a90 &&
		 pc <  0x00786ac8))*/
	/*if (varLog & 0x40000000) {
		char *str;
		char *strf;

		str = disR5900GetSym(addr);
		if (str != NULL) {
			strf = disR5900GetUpperSym(pc);
			if (strf) {
				SysPrintf("Func %8.8x: %s (called by %8.8x: %s)\n", addr, str, pc, strf);
			} else {
				SysPrintf("Func %8.8x: %s (called by %x)\n", addr, str, pc);
			}
			if (!strcmp(str, "printf")) { SysPrintf("%s\n", (char*)PSM(cpuRegs.GPR.n.a0.UL[0])); }
			if (!strcmp(str, "printk")) { SysPrintf("%s\n", (char*)PSM(cpuRegs.GPR.n.a0.UL[0])); }
		}
	}*/
}

void JumpCheckSymRet(u32 addr) {
	/*if (varLog & 0x40000000) {
		char *str;
		str = disR5900GetUpperSym(addr);
		if (str != NULL) {
			SysPrintf("Return       : %s, v0=%8.8x\n", str, cpuRegs.GPR.n.v0.UL[0]);
		}
	}*/
}

__inline void _cpuTestMissingINTC() {
	if (cpuRegs.CP0.n.Status.val & 0x400 &&
		psHu32(INTC_STAT) & psHu32(INTC_MASK)) {
		if ((cpuRegs.interrupt & (1 << 30)) == 0) {
			SysPrintf("*PCSX2*: Error, missing INTC Interrupt\n");
		}
	}
}

__inline void _cpuTestMissingDMAC() {
	if (cpuRegs.CP0.n.Status.val & 0x800 &&
		(psHu16(0xe012) & psHu16(0xe010) || 
		 psHu16(0xe010) & 0x8000)) {
		if ((cpuRegs.interrupt & (1 << 31)) == 0) {
			SysPrintf("*PCSX2*: Error, missing DMAC Interrupt\n");
		}
	}
}

void cpuTestMissingHwInts() {
	if ((cpuRegs.CP0.n.Status.val & 0x10007) == 0x10001) {
		_cpuTestMissingINTC();
		_cpuTestMissingDMAC();
//		_cpuTestTIMR();
	}
}

// sets a branch test to occur some time from an arbitrary starting point.
__forceinline int cpuSetNextBranch( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't blow up
	// if startCycle is greater than our next branch cycle.

	if( (int)(g_nextBranchCycle - startCycle) > delta )
	{
		g_nextBranchCycle = startCycle + delta;
		return 1;
	}
	return 0;
}

// sets a branch to occur some time from the current cycle
__forceinline int cpuSetNextBranchDelta( s32 delta )
{
	return cpuSetNextBranch( cpuRegs.cycle, delta );
}

// tests the cpu cycle agaisnt the given start and delta values.
// Returns true if the delta time has passed.
__forceinline int cpuTestCycle( u32 startCycle, s32 delta )
{
	// typecast the conditional to signed so that things don't explode
	// if the startCycle is ahead of our current cpu cycle.

	return (int)(cpuRegs.cycle - startCycle) >= delta;
}

// tells the EE to run the branch test the next time it gets a chance.
__forceinline void cpuSetBranch()
{
	g_nextBranchCycle = cpuRegs.cycle;
}

static __forceinline void TESTINT( u8 n, void (*callback)() )
{
	if( !(cpuRegs.interrupt & (1 << n)) ) return;

	if( cpuTestCycle( cpuRegs.sCycle[n], cpuRegs.eCycle[n] ) )
		callback();
	else
		cpuSetNextBranch( cpuRegs.sCycle[n], cpuRegs.eCycle[n] );
}

static __forceinline void _cpuTestInterrupts()
{
	/* These are 'pcsx2 interrupts', they handle asynchronous stuff
	   that depends on the cycle timings */

	TESTINT(1, vif1Interrupt);
	TESTINT(2, gsInterrupt);
	TESTINT(5, EEsif0Interrupt);
	TESTINT(6, EEsif1Interrupt);

	// Profile-guided Optimization (sorta)
	// The following ints are rarely called.  Encasing them in a conditional
	// as follows helps speed up most games.

	if( cpuRegs.interrupt & ( 1 | (3 << 3) | (3<<8) | (3<<10)) )
	{
		TESTINT(0, vif0Interrupt);
#ifndef USE_IPU_SPEEDHACK
		TESTINT(3, ipu0Interrupt);
		TESTINT(4, ipu1Interrupt);
#endif
		TESTINT(8, SPRFROMinterrupt);
		TESTINT(9, SPRTOinterrupt);

		TESTINT(10, vifMFIFOInterrupt);
		TESTINT(11, gifMFIFOInterrupt);
	}

	if ((cpuRegs.CP0.n.Status.val & 0x10007) != 0x10001) return;
	TESTINT(30, intcInterrupt);
	TESTINT(31, dmacInterrupt);
}

u32 s_iLastCOP0Cycle = 0;
u32 s_iLastPERFCycle[2] = {0,0};

static __forceinline void _cpuTestTIMR()
{
	cpuRegs.CP0.n.Count += cpuRegs.cycle-s_iLastCOP0Cycle;
	s_iLastCOP0Cycle = cpuRegs.cycle;

	if ( (cpuRegs.CP0.n.Status.val & 0x8000) &&
		cpuRegs.CP0.n.Count >= cpuRegs.CP0.n.Compare && cpuRegs.CP0.n.Count < cpuRegs.CP0.n.Compare+1000 ) {
		SysPrintf("timr intr: %x, %x\n", cpuRegs.CP0.n.Count, cpuRegs.CP0.n.Compare);
		cpuException(0x808000, cpuRegs.branch);
	}
}

static __forceinline void _cpuTestPERF()
{
	// fixme - The interpreter and recompiler both re-calculate these values
	// whenever they are read, so updating them at regular intervals *should be*
	// merely a common courtesy.  But when I set them up to be called less
	// frequently some games would crash.  I'd like to figure out why someday. [Air]

	if((cpuRegs.PERF.n.pccr & 0x800003E0) == 0x80000020) {
		cpuRegs.PERF.n.pcr0 += cpuRegs.cycle-s_iLastPERFCycle[0];
		s_iLastPERFCycle[0] = cpuRegs.cycle;
	}
	if((cpuRegs.PERF.n.pccr & 0x800F8000) == 0x80008000) {
		cpuRegs.PERF.n.pcr1 += cpuRegs.cycle-s_iLastPERFCycle[1];
		s_iLastPERFCycle[1] = cpuRegs.cycle;
	}
}

// Maximum wait between branches.  Lower values provide a tighter synchronization between
// the EE and the IOP, but incur more execution overhead.
#define EE_WAIT_CYCLE 512		// 2048 is still unstable due to COP0

// if cpuRegs.cycle is greater than this cycle, should check cpuBranchTest for updates
u32 g_nextBranchCycle = 0;

// Shared portion of the branch test, called from both the Interpreter
// and the recompiler.  (moved here to help alleviate redundant code)
static __forceinline void _cpuBranchTest_Shared()
{
	g_nextBranchCycle = cpuRegs.cycle + EE_WAIT_CYCLE;

	EEsCycle += cpuRegs.cycle - EEoCycle;
	EEoCycle = cpuRegs.cycle;

	iopBranchAction = ( EEsCycle > 0 );

	// ---- Counters -------------

	rcntUpdate_hScanline();

	if( cpuTestCycle( nextsCounter, nextCounter ) )
	{
		rcntUpdate();
	}

	_cpuTestPERF();
	_cpuTestTIMR();

	//#ifdef CPU_LOG
	//	cpuTestMissingHwInts();
	//#endif

	// ---- Interrupts -------------

	if( cpuRegs.interrupt )
		_cpuTestInterrupts();

	// ---- IOP -------------
	// * It's important to run a psxBranchTest before calling ExecuteBlock. This
	//   is because the IOP does not always perform branch tests before returning
	//   (during the prev branch) and also so it can act on the state the EE has
	//   given it before executing any code.
	//
	// * The IOP cannot always be run.  If we run IOP code every time through the
	//   cpuBranchTest, the IOP generally starts to run way ahead of the EE.
	//
	// * However! The IOP should be run during certain important events: vsync/hsync
	//   events and IOP interrupts / exceptions -- even if it's already getting
	//   a little ahead of the EE.  the iopBranchAction global will flag true if
	//   something like that happens.

	psxBranchTest();

	if( iopBranchAction )
	{
		//if( EEsCycle < -500 )
		//	SysPrintf( " IOP ahead by: %d\n", -EEsCycle );

		psxCpu->ExecuteBlock();
	}

	// The IOP cound be running ahead/behind of us, so adjust the iop's next branch by its
	// relative position to the EE (via EEsCycle)
	cpuSetNextBranchDelta( ((g_psxNextBranchCycle-psxRegs.cycle)*8) - EEsCycle );

	// Apply the hsync counter's nextCycle
	cpuSetNextBranch( counters[4].sCycle, counters[4].CycleT );

	// Apply other counter nextCycles
	cpuSetNextBranch( nextsCounter, nextCounter );
}

#ifndef PCSX2_NORECBUILD
#ifdef PCSX2_DEVBUILD
extern u8 g_globalXMMSaved;
X86_32CODE(extern u8 g_globalMMXSaved;)
#endif
#endif

void cpuBranchTest()
{
#ifndef PCSX2_NORECBUILD
#ifdef PCSX2_DEVBUILD
    // dont' remove this check unless doing an official release
    if( g_globalXMMSaved X86_32CODE(|| g_globalMMXSaved) )
        SysPrintf("frozen regs have not been restored!!!\n");
	assert( !g_globalXMMSaved X86_32CODE(&& !g_globalMMXSaved) );
#endif
	g_EEFreezeRegs = 0;
#endif

	// Perform counters, ints, and IOP updates:
	_cpuBranchTest_Shared();

	// ---- VU0 -------------

	if (VU0.VI[REG_VPU_STAT].UL & 0x1)
	{
		FreezeXMMRegs(1);
		Cpu->ExecuteVU0Block();
		FreezeXMMRegs(0);
	}

#ifndef PCSX2_NORECBUILD
#ifdef PCSX2_DEVBUILD
	assert( !g_globalXMMSaved X86_32CODE(&& !g_globalMMXSaved) );
#endif
	g_EEFreezeRegs = 1;
#endif
}

__forceinline void CPU_INT( u32 n, s32 ecycle)
{
	cpuRegs.interrupt|= 1 << n;
	cpuRegs.sCycle[n] = cpuRegs.cycle;
	cpuRegs.eCycle[n] = ecycle;

	// Interrupt is happening soon: make sure both EE and IOP are aware.

	if( ecycle <= 28 && psxCycleEE > 0 )
	{
		// If running in the IOP, force it to break immediately into the EE.
		// the EE's branch test is due to run.

		psxBreak += psxCycleEE;		// record the number of cycles the IOP didn't run.
		psxCycleEE = 0;
	}

	cpuSetNextBranchDelta( cpuRegs.eCycle[n] );
}

__forceinline void cpuTestINTCInts() {
	if( (cpuRegs.CP0.n.Status.val & 0x10407) != 0x10401 ) return;
	if( (psHu32(INTC_STAT) & psHu32(INTC_MASK)) == 0 ) return;
	if( cpuRegs.interrupt & (1 << 30) ) return;

	// fixme: The counters code throws INT30's alot, and most of the time they're
	// "late" already, so firing them immediately instead of after the next branch
	// (in which case they'll be really late) would be a lot better in theory.
	// However, setting this to zero for everything breaks games, so if it's done
	// it needs to be done for counters only.

	CPU_INT(30,4);
}

__forceinline void cpuTestDMACInts() {
	if ((cpuRegs.CP0.n.Status.val & 0x10807) != 0x10801) return;
	if ( cpuRegs.interrupt & (1 << 31) ) return;

	if ( ( (psHu16(0xe012) & psHu16(0xe010)) == 0) && 
		 ( (psHu16(0xe010) & 0x8000) == 0) ) return;

	CPU_INT(31, 4);
}

__forceinline void cpuTestTIMRInts() {
	if ((cpuRegs.CP0.n.Status.val & 0x10007) == 0x10001) {
		_cpuTestPERF();
		_cpuTestTIMR();
	}
}

void cpuTestHwInts() {
	cpuTestINTCInts();
	cpuTestDMACInts();
	cpuTestTIMRInts();
}

void cpuExecuteBios()
{
	// filter CPU options
	if( CHECK_EEREC ) Config.Options |= PCSX2_COP2REC;
	else Config.Options &= ~PCSX2_COP2REC;

	// remove frame skipping if GS doesn't support it
	switch(CHECK_FRAMELIMIT) {
		case PCSX2_FRAMELIMIT_SKIP:
		case PCSX2_FRAMELIMIT_VUSKIP:
			if( GSsetFrameSkip == NULL )
			{
				Config.Options &= ~PCSX2_FRAMELIMIT_MASK;
				SysPrintf("Notice: Disabling frameskip -- GS plugin does not support it.\n");
			}
			break;
	}

	// Set the video mode to user's default request:
	gsSetVideoRegionType( Config.PsxType & 1 );

	SysPrintf("* PCSX2 *: ExecuteBios\n");

	bExecBIOS = TRUE;
	while (cpuRegs.pc != 0x00200008 &&
		   cpuRegs.pc != 0x00100008) {
		g_nextBranchCycle = cpuRegs.cycle;
		Cpu->ExecuteBlock();
	}

	bExecBIOS = FALSE;
//    {
//        FILE* f = fopen("eebios.bin", "wb");
//        fwrite(PSM(0x80000000), 0x100000, 1, f);
//        fclose(f);
//        exit(0);

//        f = fopen("iopbios.bin", "wb");
//        fwrite(PS2MEM_PSX, 0x80000, 1, f);
//        fclose(f);
//    }

//	REC_CLEARM(0x00200008);
//	REC_CLEARM(0x00100008);
//	REC_CLEARM(cpuRegs.pc);
	if( CHECK_EEREC ) Cpu->Reset();

	SysPrintf("* PCSX2 *: ExecuteBios Complete\n");
	GSprintf(5, "PCSX2 v" PCSX2_VERSION "\nExecuteBios Complete\n");
}

void cpuRestartCPU()
{
#ifdef PCSX2_NORECBUILD
	Cpu = &intCpu;
#else
	Cpu = CHECK_EEREC ? &recCpu : &intCpu;
#endif

	// restart vus
	if (Cpu->Init() == -1) {
		SysClose();
		exit(1);
	}

	vu0Init();
	vu1Init();
	Cpu->Reset();
	psxRestartCPU();
}

// for interpreter only
void IntcpuBranchTest()
{
#ifndef PCSX2_NORECBUILD
	g_EEFreezeRegs = 0;
#endif

	// Perform counters, ints, and IOP updates:
	_cpuBranchTest_Shared();

	if (VU0.VI[REG_VPU_STAT].UL & 0x1) {
		Cpu->ExecuteVU0Block();
	}
	if (VU0.VI[REG_VPU_STAT].UL & 0x100) {
		Cpu->ExecuteVU1Block();
	}

#ifndef PCSX2_NORECBUILD
	g_EEFreezeRegs = 1;
#endif
}
