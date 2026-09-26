// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"

//------------------------------------------------------------------
// Dispatcher Functions
//------------------------------------------------------------------
static bool mvuNeedsFPCRUpdate(mV)
{
	// always update on the vu1 thread
	if (isVU1 && THREAD_VU1)
		return true;

	// otherwise only emit when it's different to the EE
	return EmuConfig.Cpu.FPUFPCR.bitmask != (isVU0 ? EmuConfig.Cpu.VU0FPCR.bitmask : EmuConfig.Cpu.VU1FPCR.bitmask);
}

static void mVUloadFPCR(const FPControlRegister* fpcr)
{
	armMoveAddressToReg(a64::x8, &fpcr->bitmask);
	armAsm->Ldr(a64::x8, a64::MemOperand(a64::x8));
	armAsm->Msr(a64::FPCR, a64::x8);
}

// Sets up the pinned registers. Must be done before using mVUmem().
static void mVUloadPinnedRegs(mV)
{
	armMoveAddressToReg(RMVUREGS, &mVU.regs());
	armMoveAddressToReg(RMVU, &mVU);
	armAsm->Ldr(RMVUMEM, a64::MemOperand(RMVUREGS, static_cast<s64>(offsetof(VURegs, Mem))));
	armMoveAddressToReg(RMVUCONST, &mVUglob);
}

static void mVUloadStatusFlags(mV)
{
	armAsm->Ldr(gprF0.W(), mVUmem(&mVU.regs().micro_statusflags[0]));
	armAsm->Ldr(gprF1.W(), mVUmem(&mVU.regs().micro_statusflags[1]));
	armAsm->Ldr(gprF2.W(), mVUmem(&mVU.regs().micro_statusflags[2]));
	armAsm->Ldr(gprF3.W(), mVUmem(&mVU.regs().micro_statusflags[3]));
}

// Generates the code for entering/exit recompiled blocks
void mVUdispatcherAB(mV)
{
	armAlignAsmPtr();
	mVU.startFunct = armGetCurrentCodePointer();

	armBeginStackFrame(true);

	// = The caller has already put the needed parameters in w0/w1:
	armEmitCall(reinterpret_cast<const void*>(isVU1 ? mVUexecuteVU1 : mVUexecuteVU0));
	armAsm->Mov(a64::x27, a64::x0); // Code to jump to

	// Load VU's FPCR state
	if (mvuNeedsFPCRUpdate(mVU))
		mVUloadFPCR(isVU0 ? &EmuConfig.Cpu.VU0FPCR : &EmuConfig.Cpu.VU1FPCR);

	mVUloadPinnedRegs(mVU);

	// Load Regs: xmmPQ = [Q, pending Q, P, pending P]
	armAsm->Ldr(xmmPQ.S(), mVUmem(&mVU.regs().VI[REG_Q].UL));
	armAsm->Ldr(a64::w8, mVUmem(&mVU.regs().pending_q));
	armAsm->Mov(xmmPQ.V4S(), 1, a64::w8);
	armAsm->Ldr(a64::w8, mVUmem(&mVU.regs().VI[REG_P].UL));
	armAsm->Mov(xmmPQ.V4S(), 2, a64::w8);
	if (isVU1)
	{
		//Load in other P instance
		armAsm->Ldr(a64::w8, mVUmem(&mVU.regs().pending_p));
	}
	armAsm->Mov(xmmPQ.V4S(), 3, a64::w8);

	armAsm->Ldr(xmmT1.Q(), mVUmem(&mVU.regs().micro_macflags));
	armAsm->Str(xmmT1.Q(), mVUmem(mVU.macFlag));

	armAsm->Ldr(xmmT1.Q(), mVUmem(&mVU.regs().micro_clipflags));
	armAsm->Str(xmmT1.Q(), mVUmem(mVU.clipFlag));

	mVUloadStatusFlags(mVU);

	// Jump to Recompiled Code Block
	armAsm->Br(a64::x27);

	mVU.exitFunct = armGetCurrentCodePointer();

	// Load EE's FPCR state
	if (mvuNeedsFPCRUpdate(mVU))
		mVUloadFPCR(&EmuConfig.Cpu.FPUFPCR);

	armEmitCall(reinterpret_cast<const void*>(isVU1 ? mVUcleanUpVU1 : mVUcleanUpVU0));

	armEndStackFrame(true);
	armAsm->Ret();

	Perf::any.Register(mVU.startFunct, static_cast<u32>(armGetCurrentCodePointer() - mVU.startFunct),
		mVU.index ? "VU1StartFunc" : "VU0StartFunc");
}

// Generates the code for resuming/exit xgkick
void mVUdispatcherCD(mV)
{
	armAlignAsmPtr();
	mVU.startFunctXG = armGetCurrentCodePointer();

	armBeginStackFrame(true);

	// Load VU's FPCR state
	if (mvuNeedsFPCRUpdate(mVU))
		mVUloadFPCR(isVU0 ? &EmuConfig.Cpu.VU0FPCR : &EmuConfig.Cpu.VU1FPCR);

	mVUloadPinnedRegs(mVU);
	mVUrestoreRegs(mVU);
	mVUloadStatusFlags(mVU);

	// Jump to Recompiled Code Block
	armAsm->Ldr(a64::x8, mVUmem(&mVU.resumePtrXG));
	armAsm->Br(a64::x8);

	mVU.exitFunctXG = armGetCurrentCodePointer();

	// Backup Status Flag (other regs were backed up on xgkick)
	armAsm->Str(gprF0.W(), mVUmem(&mVU.regs().micro_statusflags[0]));
	armAsm->Str(gprF1.W(), mVUmem(&mVU.regs().micro_statusflags[1]));
	armAsm->Str(gprF2.W(), mVUmem(&mVU.regs().micro_statusflags[2]));
	armAsm->Str(gprF3.W(), mVUmem(&mVU.regs().micro_statusflags[3]));

	// Load EE's FPCR state
	if (mvuNeedsFPCRUpdate(mVU))
		mVUloadFPCR(&EmuConfig.Cpu.FPUFPCR);

	armEndStackFrame(true);
	armAsm->Ret();

	Perf::any.Register(mVU.startFunctXG, static_cast<u32>(armGetCurrentCodePointer() - mVU.startFunctXG),
		mVU.index ? "VU1StartFuncXG" : "VU0StartFuncXG");
}

// Saves all caller-saved registers around the wait, since it's called in the middle of an instruction.
static void mVUGenerateWaitMTVU(mV)
{
	armAlignAsmPtr();
	mVU.waitMTVU = armGetCurrentCodePointer();

	mVUSavedRegs saved;
	for (int i = 0; i < 16; i++)
		saved.gprs[i] = true;
	saved.gprs[30] = true; // link register
	for (int i = 0; i < 32; i++)
		saved.vecs[i] = true;

	mVUpushRegs(saved);
	armEmitCall(reinterpret_cast<const void*>(mVUwaitMTVU));
	mVUpopRegs(saved);
	armAsm->Ret();

	Perf::any.Register(mVU.waitMTVU, static_cast<u32>(armGetCurrentCodePointer() - mVU.waitMTVU),
		mVU.index ? "VU1WaitMTVU" : "VU0WaitMTVU");
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

	return mVUsearchProg<vuIndex>(startPC & vuLimit, (uptr)&mVU.prog.lpState); // Find and set correct program
}

//------------------------------------------------------------------
// Cleanup Functions
//------------------------------------------------------------------

_mVUt void mVUcleanUp()
{
	microVU& mVU = mVUx;

	if ((mVU.prog.x86ptr < mVU.prog.x86start) || (mVU.prog.x86ptr >= mVU.prog.x86end))
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
