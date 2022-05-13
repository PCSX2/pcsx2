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

#pragma once

//------------------------------------------------------------------
// Dispatcher Functions
//------------------------------------------------------------------

// Generates the code for entering/exit recompiled blocks
void mVUdispatcherAB(mV)
{
	mVU.startFunct = x86Ptr;

	{
		xScopedStackFrame frame(false, true);

		// = The caller has already put the needed parameters in ecx/edx:
		if (!isVU1) xFastCall((void*)mVUexecuteVU0, arg1reg, arg2reg);
		else        xFastCall((void*)mVUexecuteVU1, arg1reg, arg2reg);

		// Load VU's MXCSR state
		xLDMXCSR(g_sseVUMXCSR);

		// Load Regs
		xMOVAPS (xmmT1, ptr128[&mVU.regs().VI[REG_P].UL]);
		xMOVAPS (xmmPQ, ptr128[&mVU.regs().VI[REG_Q].UL]);
		xMOVDZX (xmmT2, ptr32[&mVU.regs().pending_q]);
		xSHUF.PS(xmmPQ, xmmT1, 0); // wzyx = PPQQ
		//Load in other Q instance
		xPSHUF.D(xmmPQ, xmmPQ, 0xe1);
		xMOVSS(xmmPQ, xmmT2);
		xPSHUF.D(xmmPQ, xmmPQ, 0xe1);

		if (isVU1)
		{
			//Load in other P instance
			xMOVDZX(xmmT2, ptr32[&mVU.regs().pending_p]);
			xPSHUF.D(xmmPQ, xmmPQ, 0x1B);
			xMOVSS(xmmPQ, xmmT2);
			xPSHUF.D(xmmPQ, xmmPQ, 0x1B);
		}

		xMOVAPS(xmmT1, ptr128[&mVU.regs().micro_macflags]);
		xMOVAPS(ptr128[mVU.macFlag], xmmT1);


		xMOVAPS(xmmT1, ptr128[&mVU.regs().micro_clipflags]);
		xMOVAPS(ptr128[mVU.clipFlag], xmmT1);

		xMOV(gprF0, ptr32[&mVU.regs().micro_statusflags[0]]);
		xMOV(gprF1, ptr32[&mVU.regs().micro_statusflags[1]]);
		xMOV(gprF2, ptr32[&mVU.regs().micro_statusflags[2]]);
		xMOV(gprF3, ptr32[&mVU.regs().micro_statusflags[3]]);

		// Jump to Recompiled Code Block
		xJMP(rax);

		mVU.exitFunct = x86Ptr;

		// Load EE's MXCSR state
		xLDMXCSR(g_sseMXCSR);

		// = The first two DWORD or smaller arguments are passed in ECX and EDX registers;
		//              all other arguments are passed right to left.
		if (!isVU1) xFastCall((void*)mVUcleanUpVU0);
		else        xFastCall((void*)mVUcleanUpVU1);
	}

	xRET();

	pxAssertDev(xGetPtr() < (mVU.dispCache + mVUdispCacheSize),
		"microVU: Dispatcher generation exceeded reserved cache area!");
}

// Generates the code for resuming/exit xgkick
void mVUdispatcherCD(mV)
{
	mVU.startFunctXG = x86Ptr;

	{
		xScopedStackFrame frame(false, true);

		// Load VU's MXCSR state
		xLDMXCSR(g_sseVUMXCSR);

		mVUrestoreRegs(mVU);
		xMOV(gprF0, ptr32[&mVU.regs().micro_statusflags[0]]);
		xMOV(gprF1, ptr32[&mVU.regs().micro_statusflags[1]]);
		xMOV(gprF2, ptr32[&mVU.regs().micro_statusflags[2]]);
		xMOV(gprF3, ptr32[&mVU.regs().micro_statusflags[3]]);

		// Jump to Recompiled Code Block
		xJMP(ptrNative[&mVU.resumePtrXG]);

		mVU.exitFunctXG = x86Ptr;

		// Backup Status Flag (other regs were backed up on xgkick)
		xMOV(ptr32[&mVU.regs().micro_statusflags[0]], gprF0);
		xMOV(ptr32[&mVU.regs().micro_statusflags[1]], gprF1);
		xMOV(ptr32[&mVU.regs().micro_statusflags[2]], gprF2);
		xMOV(ptr32[&mVU.regs().micro_statusflags[3]], gprF3);

		// Load EE's MXCSR state
		xLDMXCSR(g_sseMXCSR);
	}

	xRET();

	pxAssertDev(xGetPtr() < (mVU.dispCache + mVUdispCacheSize),
		"microVU: Dispatcher generation exceeded reserved cache area!");
}

//------------------------------------------------------------------
// Execution Functions
//------------------------------------------------------------------

// Executes for number of cycles
_mVUt void* mVUexecute(u32 startPC, u32 cycles)
{

	microVU& mVU = mVUx;
	u32 vuLimit = vuIndex ? 0x3ff8 : 0xff8;
	if (startPC > vuLimit + 7)
	{
		DevCon.Warning("microVU%x Warning: startPC = 0x%x, cycles = 0x%x", vuIndex, startPC, cycles);
	}

	mVU.cycles = cycles;
	mVU.totalCycles = cycles;

	xSetPtr(mVU.prog.x86ptr); // Set x86ptr to where last program left off
	return mVUsearchProg<vuIndex>(startPC & vuLimit, (uptr)&mVU.prog.lpState); // Find and set correct program
}

//------------------------------------------------------------------
// Cleanup Functions
//------------------------------------------------------------------

_mVUt void mVUcleanUp()
{
	microVU& mVU = mVUx;

	mVU.prog.x86ptr = x86Ptr;

	if ((xGetPtr() < mVU.prog.x86start) || (xGetPtr() >= mVU.prog.x86end))
	{
		Console.WriteLn(vuIndex ? Color_Orange : Color_Magenta, "microVU%d: Program cache limit reached.", mVU.index);
		mVUreset(mVU, false);
	}

	mVU.cycles = mVU.totalCycles - mVU.cycles;
	mVU.regs().cycle += mVU.cycles;

	if (!vuIndex || !THREAD_VU1)
	{
		u32 cycles_passed = std::min(mVU.cycles, 3000u) * EmuConfig.Speedhacks.EECycleSkip;
		if (cycles_passed > 0)
		{
			s32 vu0_offset = VU0.cycle - cpuRegs.cycle;
			cpuRegs.cycle += cycles_passed;

			// VU0 needs to stay in sync with the CPU otherwise things get messy
			// So we need to adjust when VU1 skips cycles also
			if (!vuIndex)
				VU0.cycle = cpuRegs.cycle + vu0_offset;
			else
				VU0.cycle += cycles_passed;
		}
	}
	mVU.profiler.Print();
}

//------------------------------------------------------------------
// Caller Functions
//------------------------------------------------------------------

void* mVUexecuteVU0(u32 startPC, u32 cycles) { return mVUexecute<0>(startPC, cycles); }
void* mVUexecuteVU1(u32 startPC, u32 cycles) { return mVUexecute<1>(startPC, cycles); }
void mVUcleanUpVU0() { mVUcleanUp<0>(); }
void mVUcleanUpVU1() { mVUcleanUp<1>(); }
