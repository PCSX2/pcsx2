// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 R5900 (EE) recompiler.
//
// This follows the structure of the x86 EE recompiler (x86/ix86-32/iR5900.cpp): same block
// scanning, cycle counting, event tests, block linking and self-modifying code protection.
// Code generation is simpler:
//  - Guest GPRs live in cpuRegs and are accessed relative to a pinned base register (x19).
//    There is no host register cache across instructions, only constant propagation of the
//    low 64 bits of each GPR.
//  - Common integer, branch and load/store instructions are emitted natively. Loads and stores
//    use an inline vtlb lookup, and fall back to the vtlb handlers for I/O.
//  - Everything else (MMI, FPU arithmetic, COP2/VU0 macro mode, unaligned loads/stores, TLB
//    and system instructions) calls the interpreter implementation from the opcode tables.
//
// Needs proper testing across a wide range of games; compare against the interpreter when in doubt.

// Common.h must come first, so that the instruction field macros (_Rs_ etc.) refer to cpuRegs.
#include "Common.h"

#include "arm64/AsmHelpers.h"
#include "x86/BaseblockEx.h"

#include "COP0.h"
#include "Config.h"
#include "GS.h"
#include "Host.h"
#include "Patch.h"
#include "R5900OpcodeTables.h"
#include "VMManager.h"
#include "VUmicro.h"
#include "vtlb.h"
#include "R3000A.h"
#include "DebugTools/Breakpoints.h"

#include "common/AlignedMalloc.h"
#include "common/Assertions.h"
#include "common/FastJmp.h"
#include "common/HeapArray.h"
#include "common/Perf.h"

#include <algorithm>
#include <cstddef>
#include <optional>

namespace a64 = vixl::aarch64;

using namespace R5900;
using namespace vtlb_private;

// Pinned registers, all callee-saved so they survive calls into C code.
#define RCPUREGS a64::x19 // &cpuRegs (fpuRegs follows it in _cpuRegistersPack)
#define RVTLBMAP a64::x20 // vtlbdata.vmap
#define RWBRANCH a64::w21 // branch target, held across the delay slot
#define RXBRANCH a64::x21

// Temporaries, never live across a call.
#define RWTEMP1 a64::w8
#define RWTEMP2 a64::w9
#define RWTEMP3 a64::w10
#define RWTEMP4 a64::w11
#define RXTEMP1 a64::x8
#define RXTEMP2 a64::x9
#define RXTEMP3 a64::x10
#define RXTEMP4 a64::x11

static bool eeRecNeedsReset = false;
static bool eeCpuExecuting = false;
static bool eeRecExitRequested = false;

static u32 maxrecmem = 0;
alignas(16) static uptr recLUT[_64kb];
alignas(16) static u32 hwLUT[_64kb];

static __fi u32 HWADDR(u32 mem) { return hwLUT[mem >> 16] + mem; }

static u32 s_nBlockCycles = 0; // cycles of current block recompiling
static u32 pc; // recompiler pc
static int g_branch; // set for branch
static bool g_cpuFlushedPC;
static bool g_recompilingDelaySlot;

// Constant propagation state, for the low 64 bits of each GPR.
static u64 g_cpuConstRegs[32];
static u32 g_cpuHasConstReg = 0;
static u32 g_cpuFlushedConstReg = 0;

static DynamicHeapArray<u8, 4096> recRAMCopy;
static DynamicHeapArray<BASEBLOCK, 4096> recLutReserve_RAM;
static DynamicHeapArray<BASEBLOCK, 4096> recLutUnmapped;
static size_t recLutEntries;
static bool extraRam;

static BASEBLOCK* recRAM = nullptr;
static BASEBLOCK* recROM = nullptr;
static BASEBLOCK* recROM1 = nullptr;
static BASEBLOCK* recROM2 = nullptr;

static BaseBlocks recBlocks;
static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;

static BASEBLOCK* s_pCurBlock = nullptr;
static BASEBLOCKEX* s_pCurBlockEx = nullptr;
static u32 s_nEndBlock = 0; // what pc the current block ends
static u32 s_branchTo;
static bool s_nBlockFF;

// Saved state for compiling both sides of a branch.
static u64 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static u32 s_savenBlockCycles = 0;

alignas(16) static u16 manual_page[Ps2MemSize::TotalRam >> 12];
alignas(16) static u8 manual_counter[Ps2MemSize::TotalRam >> 12];

#define PC_GETBLOCK(x) PC_GETBLOCK_(x, recLUT)

static void recRecompile(const u32 startpc);
static void dyna_block_discard(u32 start, u32 sz);
static void dyna_page_reset(u32 start, u32 sz);
static void recError(u32 error);
static void recExitExecution();
static void recompileNextInstruction(bool delayslot);

static const void* DispatcherEvent = nullptr;
static const void* DispatcherReg = nullptr;
static const void* JITCompile = nullptr;
static const void* EnterRecompiledCode = nullptr;
static const void* DispatchBlockDiscard = nullptr;
static const void* DispatchPageReset = nullptr;
static const void* UnmappedRecLUTPage = nullptr;

//////////////////////////////////////////////////////////////////////////
// Register access helpers
//////////////////////////////////////////////////////////////////////////

#define CPUREG_MEM(field) a64::MemOperand(RCPUREGS, static_cast<s64>(offsetof(cpuRegisters, field)))
#define FPUREG_MEM(field) a64::MemOperand(RCPUREGS, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs) + offsetof(fpuRegisters, field)))

static_assert(offsetof(cpuRegistersPack, cpuRegs) == 0);

static __fi a64::MemOperand GPR_MEM(u32 reg, u32 offset = 0)
{
	return a64::MemOperand(RCPUREGS, static_cast<s64>(offsetof(cpuRegisters, GPR) + reg * sizeof(GPR_reg) + offset));
}

static __fi s64 GPR_OFFSET(u32 reg)
{
	return static_cast<s64>(offsetof(cpuRegisters, GPR) + reg * sizeof(GPR_reg));
}

static __fi a64::MemOperand CP0_MEM(u32 reg)
{
	return a64::MemOperand(RCPUREGS, static_cast<s64>(offsetof(cpuRegisters, CP0) + reg * sizeof(u32)));
}

static __fi a64::MemOperand FPR_MEM(u32 reg)
{
	return a64::MemOperand(RCPUREGS, static_cast<s64>(offsetof(cpuRegistersPack, fpuRegs) + offsetof(fpuRegisters, fpr) + reg * sizeof(FPRreg)));
}

static __fi bool GPR_IS_CONST1(u32 reg)
{
	return (reg < 32 && (g_cpuHasConstReg & (1u << reg)));
}

static __fi bool GPR_IS_CONST2(u32 reg1, u32 reg2)
{
	return GPR_IS_CONST1(reg1) && GPR_IS_CONST1(reg2);
}

static __fi void GPR_SET_CONST(u32 reg, u64 value)
{
	if (reg == 0 || reg >= 32)
		return;

	g_cpuConstRegs[reg] = value;
	g_cpuHasConstReg |= (1u << reg);
	g_cpuFlushedConstReg &= ~(1u << reg);
}

static __fi void GPR_DEL_CONST(u32 reg)
{
	if (reg == 0 || reg >= 32)
		return;

	g_cpuHasConstReg &= ~(1u << reg);
}

static void _flushConstRegs()
{
	for (u32 i = 1; i < 32; i++)
	{
		const u32 bit = (1u << i);
		if (!(g_cpuHasConstReg & bit) || (g_cpuFlushedConstReg & bit))
			continue;

		if (g_cpuConstRegs[i] == 0)
		{
			armAsm->Str(a64::xzr, GPR_MEM(i));
		}
		else
		{
			armAsm->Mov(RXTEMP1, g_cpuConstRegs[i]);
			armAsm->Str(RXTEMP1, GPR_MEM(i));
		}

		g_cpuFlushedConstReg |= bit;
	}
}

// Loads the low 64 bits of a GPR.
static void _eeMoveGPRtoR(const a64::Register& to, u32 fromgpr)
{
	if (fromgpr == 0)
		armAsm->Mov(to, 0);
	else if (GPR_IS_CONST1(fromgpr))
		armAsm->Mov(to, to.Is64Bits() ? g_cpuConstRegs[fromgpr] : static_cast<u32>(g_cpuConstRegs[fromgpr]));
	else
		armAsm->Ldr(to, GPR_MEM(fromgpr));
}

// Stores to the low 64 bits of a GPR. 32-bit registers are sign extended, like most EE ops.
static void _eeMoveRtoGPR(u32 togpr, const a64::Register& from)
{
	if (togpr == 0)
		return;

	if (from.Is32Bits())
	{
		const a64::Register xfrom = from.X();
		armAsm->Sxtw(xfrom, from);
		armAsm->Str(xfrom, GPR_MEM(togpr));
	}
	else
	{
		armAsm->Str(from, GPR_MEM(togpr));
	}
	GPR_DEL_CONST(togpr);
}

static void _eeStorePC(u32 newpc)
{
	armAsm->Mov(RWTEMP1, newpc);
	armAsm->Str(RWTEMP1, CPUREG_MEM(pc));
}

static void _eeStoreCode(u32 code)
{
	armAsm->Mov(RWTEMP1, code);
	armAsm->Str(RWTEMP1, CPUREG_MEM(code));
}

static void SaveBranchState()
{
	s_savenBlockCycles = s_nBlockCycles;
	std::memcpy(s_saveConstRegs, g_cpuConstRegs, sizeof(g_cpuConstRegs));
	s_saveHasConstReg = g_cpuHasConstReg;
	s_saveFlushedConstReg = g_cpuFlushedConstReg;
}

static void LoadBranchState()
{
	s_nBlockCycles = s_savenBlockCycles;
	std::memcpy(g_cpuConstRegs, s_saveConstRegs, sizeof(g_cpuConstRegs));
	g_cpuHasConstReg = s_saveHasConstReg;
	g_cpuFlushedConstReg = s_saveFlushedConstReg;
}

//////////////////////////////////////////////////////////////////////////
// Cycle counting
//////////////////////////////////////////////////////////////////////////

// Note: scaleblockcycles() scales s_nBlockCycles respective to the EECycleRate value for manipulating the cycles of current block recompiling.
// s_nBlockCycles is 3 bit fixed point.  Divide by 8 when done!
// Scaling blocks under 40 cycles seems to produce countless problem, so let's try to avoid them.

#define DEFAULT_SCALED_BLOCKS() (s_nBlockCycles >> 3)

static u32 scaleblockcycles_calculation()
{
	const bool lowcycles = (s_nBlockCycles <= 40);
	const s8 cyclerate = EmuConfig.Speedhacks.EECycleRate;
	u32 scale_cycles = 0;

	if (cyclerate == 0 || lowcycles || cyclerate < -99 || cyclerate > 3)
		scale_cycles = DEFAULT_SCALED_BLOCKS();

	else if (cyclerate > 1)
		scale_cycles = s_nBlockCycles >> (2 + cyclerate);

	else if (cyclerate == 1)
		scale_cycles = DEFAULT_SCALED_BLOCKS() / 1.3f; // Adds a mild 30% increase in clockspeed for value 1.

	else if (cyclerate == -1) // the mildest value.
		// These values were manually tuned to yield mild speedup with high compatibility
		scale_cycles = (s_nBlockCycles <= 80 || s_nBlockCycles > 168 ? 5 : 7) * s_nBlockCycles / 32;

	else
		scale_cycles = ((5 + (-2 * (cyclerate + 1))) * s_nBlockCycles) >> 5;

	// Ensure block cycle count is never less than 1.
	return (scale_cycles < 1) ? 1 : scale_cycles;
}

static u32 scaleblockcycles()
{
	return scaleblockcycles_calculation();
}

static u32 scaleblockcycles_clear()
{
	const u32 scaled = scaleblockcycles_calculation();

	const s8 cyclerate = EmuConfig.Speedhacks.EECycleRate;
	const bool lowcycles = (s_nBlockCycles <= 40);

	if (!lowcycles && cyclerate > 1)
		s_nBlockCycles &= (0x1 << (cyclerate + 2)) - 1;
	else
		s_nBlockCycles &= 0x7;

	return scaled;
}

// Adds the cycles executed so far in this block to cpuRegs.cycle, for code that reads it.
// Leaves the new cycle count in RXTEMP2.
static void iFlushBlockCycles()
{
	armAsm->Ldr(RXTEMP2, CPUREG_MEM(cycle));
	armAsm->Add(RXTEMP2, RXTEMP2, scaleblockcycles_clear());
	armAsm->Str(RXTEMP2, CPUREG_MEM(cycle));
}

//////////////////////////////////////////////////////////////////////////
// Dispatchers
//////////////////////////////////////////////////////////////////////////

static void recEventTest()
{
	_cpuEventTest_Shared();

	if (eeRecExitRequested)
	{
		eeRecExitRequested = false;
		recExitExecution();
	}
}

static void _DynGen_EmitDispatch()
{
	armAsm->Ldr(a64::w0, CPUREG_MEM(pc));
	armAsm->Lsr(a64::w1, a64::w0, 16);
	armMoveAddressToReg(a64::x2, recLUT);
	armAsm->Ldr(a64::x1, a64::MemOperand(a64::x2, a64::x1, a64::LSL, 3));
	// BASEBLOCK is 8 bytes and indexed by pc / 4, so the offset is pc * 2.
	armAsm->Add(a64::x1, a64::x1, a64::Operand(a64::x0, a64::LSL, 1));
	armAsm->Ldr(a64::x1, a64::MemOperand(a64::x1));
	armAsm->Br(a64::x1);
}

static void _DynGen_Dispatchers()
{
	const u8* start = armGetCurrentCodePointer();

	// Event test falls through to the register dispatcher.
	DispatcherEvent = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(recEventTest));
	DispatcherReg = armGetCurrentCodePointer();
	_DynGen_EmitDispatch();

	armAlignAsmPtr();
	JITCompile = armGetCurrentCodePointer();
	armAsm->Ldr(a64::w0, CPUREG_MEM(pc));
	armEmitCall(reinterpret_cast<const void*>(recRecompile));
	_DynGen_EmitDispatch();

	// We never return through this function, instead we fastjmp() out.
	// So we don't need to worry about preserving callee-saved registers, and the stack is already aligned.
	armAlignAsmPtr();
	EnterRecompiledCode = armGetCurrentCodePointer();
	armMoveAddressToReg(RCPUREGS, &cpuRegs);
	armMoveAddressToReg(RVTLBMAP, &vtlbdata.vmap);
	armAsm->Ldr(RVTLBMAP, a64::MemOperand(RVTLBMAP));
	armEmitJmp(DispatcherReg);

	DispatchBlockDiscard = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(dyna_block_discard));
	armEmitJmp(DispatcherReg);

	DispatchPageReset = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(dyna_page_reset));
	armEmitJmp(DispatcherReg);

	UnmappedRecLUTPage = armGetCurrentCodePointer();
	armAsm->Mov(a64::w0, 0);
	armEmitCall(reinterpret_cast<const void*>(recError));

	recBlocks.SetJITCompile(JITCompile);

	Perf::any.Register(start, static_cast<u32>(armGetCurrentCodePointer() - start), "EE Dispatcher");
}

static void recError(u32 error)
{
	switch (error)
	{
		case 0:
			Host::ReportErrorAsync("R5900 Exception", fmt::format("Jump to unmapped recLUT page (PC: 0x{:08x})", cpuRegs.pc));
			break;
		case 1:
			Host::ReportErrorAsync("R5900 Exception", fmt::format("Jump to unaligned address (PC: 0x{:08x})", cpuRegs.pc));
			break;
	}

	VMManager::SetPaused(true);
	recExitExecution();
}

//////////////////////////////////////////////////////////////////////////
// Reset / execution
//////////////////////////////////////////////////////////////////////////

static __ri void ClearRecLUT(BASEBLOCK* base, int memsize)
{
	for (int i = 0; i < memsize / 4; i++)
		base[i].SetFnptr((uptr)JITCompile);
}

static void recReserveRAM()
{
	// One entry per possible call target
	recLutEntries = (Ps2MemSize::ExposedRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4;

	if (recRAMCopy.size() != Ps2MemSize::ExposedRam)
		recRAMCopy.resize(Ps2MemSize::ExposedRam);

	if (recLutReserve_RAM.size() != recLutEntries)
		recLutReserve_RAM.resize(recLutEntries);

	// Allocate one LUT page of memory for unmapped pages to reference
	recLutUnmapped.resize(_64kb / 4);

	BASEBLOCK* basepos = recLutReserve_RAM.data();
	recRAM = basepos;
	basepos += (Ps2MemSize::ExposedRam / 4);
	recROM = basepos;
	basepos += (Ps2MemSize::Rom / 4);
	recROM1 = basepos;
	basepos += (Ps2MemSize::Rom1 / 4);
	recROM2 = basepos;
	basepos += (Ps2MemSize::Rom2 / 4);

	BASEBLOCK* unmapped = recLutUnmapped.data();
	for (int i = 0; i < 0x10000; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, unmapped, i, 0, 0);
	}

	for (int i = 0x0000; i < (int)(Ps2MemSize::ExposedRam / 0x10000); i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x0000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x2000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x3000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x8000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xa000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xb000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xc000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xd000, i, i);
	}

	for (int i = 0x1fc0; i < 0x2000; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	for (int i = 0x1e00; i < 0x1e40; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e80; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0xa000, i, i - 0x1e40);
	}
}

static void recReserve()
{
	recPtr = SysMemory::GetEERec();
	recPtrEnd = SysMemory::GetEERecEnd() - _64kb;
	recReserveRAM();
}

static void recResetRaw()
{
	Console.WriteLn(Color_StrongBlack, "EE/iR5900 ARM64 Recompiler Reset");

	if (CHECK_EXTRAMEM != extraRam)
	{
		recReserveRAM();
		extraRam = !extraRam;
	}

	armSetAsmPtr(SysMemory::GetEERec(), SysMemory::GetEERecEnd() - SysMemory::GetEERec(), nullptr);
	armStartBlock();
	_DynGen_Dispatchers();
	recPtr = armEndBlock();

	ClearRecLUT(recLutReserve_RAM.data(),
		Ps2MemSize::ExposedRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2);

	for (int i = 0; i < _64kb / 4; i++)
		recLutUnmapped.data()[i].SetFnptr((uptr)UnmappedRecLUTPage);

	recRAMCopy.fill(0);

	maxrecmem = 0;

	recBlocks.Reset();

	g_branch = 0;

	std::memset(manual_page, 0, sizeof(manual_page));
	std::memset(manual_counter, 0, sizeof(manual_counter));
}

static void recShutdown()
{
	recRAMCopy.deallocate();
	recLutReserve_RAM.deallocate();

	recBlocks.Reset();

	recRAM = recROM = recROM1 = recROM2 = nullptr;

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static void recStep()
{
}

static fastjmp_buf m_SetJmp_StateCheck;

static void recExitExecution()
{
	fastjmp_jmp(&m_SetJmp_StateCheck, 1);
}

static void recSafeExitExecution()
{
	// If we're currently processing events, we can't safely jump out of the recompiler here, because we'll
	// leave things in an inconsistent state. So instead, we flag it for exiting once cpuEventTest() returns.
	// Exiting in the middle of a rec block with the registers unsaved would be a bad idea too..
	eeRecExitRequested = true;

	// Force an event test at the end of this block.
	if (!eeEventTestIsActive)
	{
		// EE is running.
		cpuRegs.nextEventCycle = 0;
	}
	else
	{
		// IOP might be running, so break out if so.
		if (psxRegs.iopCycleEE > 0)
		{
			psxRegs.iopBreak += psxRegs.iopCycleEE; // record the number of cycles the IOP didn't run.
			psxRegs.iopCycleEE = 0;
		}
	}
}

static void recResetEE()
{
	if (eeCpuExecuting)
	{
		// get outta here as soon as we can
		eeRecNeedsReset = true;
		recSafeExitExecution();
		return;
	}

	recResetRaw();
}

static void recCancelInstruction()
{
	pxFailRel("recCancelInstruction() called, this should never happen!");
}

static void recExecute()
{
	// Reset before we try to execute any code, if there's one pending.
	// We need to do this here, because if we reset while we're executing, it sets the "needs reset"
	// flag, which triggers a JIT exit (the fastjmp_set below), and eventually loops back here.
	if (eeRecNeedsReset)
	{
		eeRecNeedsReset = false;
		recResetRaw();
	}

	// setjmp will save the register context and will return 0
	// A call to longjmp will restore the context (included the eip/rip)
	// but will return the longjmp 2nd parameter (here 1)
	if (!fastjmp_set(&m_SetJmp_StateCheck))
	{
		eeCpuExecuting = true;
		reinterpret_cast<void (*)()>(const_cast<void*>(EnterRecompiledCode))();

		// Generally unreachable code here ...
	}

	eeCpuExecuting = false;
}

// Size is in dwords (4 bytes)
static void recClear(u32 addr, u32 size)
{
	if ((addr) >= maxrecmem || !(recLUT[(addr) >> 16] + (addr & ~0xFFFFUL)))
		return;
	addr = HWADDR(addr);

	int blockidx = recBlocks.LastIndex(addr + size * 4 - 4);

	if (blockidx == -1)
		return;

	u32 lowerextent = static_cast<u32>(-1), upperextent = 0, ceiling = static_cast<u32>(-1);

	BASEBLOCKEX* pexblock = recBlocks[blockidx + 1];
	if (pexblock)
		ceiling = pexblock->startpc;

	int toRemoveLast = blockidx;

	while ((pexblock = recBlocks[blockidx]))
	{
		u32 blockstart = pexblock->startpc;
		u32 blockend = pexblock->startpc + pexblock->size * 4;
		BASEBLOCK* pblock = PC_GETBLOCK(blockstart);

		if (pblock == s_pCurBlock)
		{
			if (toRemoveLast != blockidx)
			{
				recBlocks.Remove((blockidx + 1), toRemoveLast);
			}
			toRemoveLast = --blockidx;
			continue;
		}

		if (blockend <= addr)
		{
			lowerextent = std::max(lowerextent, blockend);
			break;
		}

		lowerextent = std::min(lowerextent, blockstart);
		upperextent = std::max(upperextent, blockend);
		pblock->SetFnptr((uptr)JITCompile);

		blockidx--;
	}

	if (toRemoveLast != blockidx)
	{
		recBlocks.Remove((blockidx + 1), toRemoveLast);
	}

	upperextent = std::min(upperextent, ceiling);

	for (int i = 0; (pexblock = recBlocks[i]); i++)
	{
		if (s_pCurBlock == PC_GETBLOCK(pexblock->startpc))
			continue;
		u32 blockend = pexblock->startpc + pexblock->size * 4;
		if ((pexblock->startpc >= addr && pexblock->startpc < addr + size * 4) || (pexblock->startpc < addr && blockend > addr)) [[unlikely]]
		{
			Console.Error("[EE] Impossible block clearing failure");
			pxFail("[EE] Impossible block clearing failure");
		}
	}

	if (upperextent > lowerextent)
		ClearRecLUT(PC_GETBLOCK(lowerextent), upperextent - lowerextent);
}

//////////////////////////////////////////////////////////////////////////
// Block exits
//////////////////////////////////////////////////////////////////////////

// Emits a B to the block at pc, which gets patched when that block is (re)compiled or cleared.
static void recEmitBlockLink(u32 target_pc)
{
	a64::SingleEmissionCheckScope guard(armAsm);
	u32* branch_ptr = reinterpret_cast<u32*>(armGetCurrentCodePointer());
	armAsm->b(static_cast<int64_t>(0)); // placeholder, patched by Link()
	recBlocks.Link(HWADDR(target_pc), branch_ptr);
}

// Generates dynarec code for Event tests followed by a block dispatch (branch).
//   newpc - address to jump to at the end of the block.  If newpc == 0xffffffff then
//   the jump is assumed to be to a register (dynamic).  For any other value the
//   jump is assumed to be static, in which case the block will be "hardlinked" after
//   the first time it's dispatched.
static void iBranchTest(u32 newpc = 0xffffffff)
{
	// Check the Event scheduler if our "cycle target" has been reached.
	// Equiv code to:
	//    cpuRegs.cycle += blockcycles;
	//    if ( cpuRegs.cycle > g_nextEventCycle ) { DoEvents(); }

	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && newpc == s_branchTo)
	{
		armAsm->Ldr(RXTEMP1, CPUREG_MEM(nextEventCycle));
		armAsm->Ldr(RXTEMP2, CPUREG_MEM(cycle));
		armAsm->Add(RXTEMP2, RXTEMP2, scaleblockcycles());
		armAsm->Cmp(RXTEMP1, RXTEMP2);
		armAsm->Csel(RXTEMP1, RXTEMP2, RXTEMP1, a64::mi);
		armAsm->Str(RXTEMP1, CPUREG_MEM(cycle));

		armEmitJmp(DispatcherEvent);
	}
	else
	{
		armAsm->Ldr(RXTEMP1, CPUREG_MEM(cycle));
		armAsm->Add(RXTEMP1, RXTEMP1, scaleblockcycles());
		armAsm->Str(RXTEMP1, CPUREG_MEM(cycle)); // update cycles
		armAsm->Ldr(RXTEMP2, CPUREG_MEM(nextEventCycle));
		armAsm->Cmp(RXTEMP1, RXTEMP2);

		// Event pending: go through the event dispatcher.
		armEmitCondBranch(a64::pl, DispatcherEvent);

		if (newpc == 0xffffffff)
			armEmitJmp(DispatcherReg);
		else
			recEmitBlockLink(newpc);
	}
}

// Branch to a runtime variable target, which is in RWBRANCH.
static void SetBranchReg()
{
	g_branch = 1;

	armAsm->Str(RWBRANCH, CPUREG_MEM(pc));
	_flushConstRegs();

	// Test for jump to unaligned, only needed for register branches
	//  since unaligned targets can't be encoded with imm
	a64::Label unaligned;
	armAsm->Tst(RWBRANCH, 3);
	armAsm->B(&unaligned, a64::ne);

	iBranchTest();

	armAsm->Bind(&unaligned);
	armAsm->Mov(a64::w0, 1);
	armEmitCall(reinterpret_cast<const void*>(recError));
}

static void SetBranchImm(u32 imm)
{
	g_branch = 1;

	pxAssert(imm);

	// end the current block
	_flushConstRegs();
	_eeStorePC(imm);
	iBranchTest(imm);
}

//////////////////////////////////////////////////////////////////////////
// Interpreter fallback
//////////////////////////////////////////////////////////////////////////

// Calls the interpreter implementation of the current instruction. The interpreter reads and writes
// cpuRegs directly, so constants are flushed first and registers it may write are no longer constant.
static void recInterpret(void (*func)(), bool sync_cycles = false)
{
	_flushConstRegs();
	_eeStorePC(pc);
	_eeStoreCode(cpuRegs.code);
	g_cpuFlushedPC = true;

	// Instructions which look at cpuRegs.cycle (COP0 counters, VU0 sync) need it to be up to date.
	if (sync_cycles)
		iFlushBlockCycles();

	armEmitCall(reinterpret_cast<const void*>(func));
	GPR_DEL_CONST(_Rt_);
	GPR_DEL_CONST(_Rd_);
}

static void recInterpretCurrent(bool sync_cycles = false)
{
	recInterpret(GetCurrentInstruction().interpret, sync_cycles);
}

// Use this to call into interpreter functions that require an immediate branchtest
// to be done afterward (anything that throws an exception or enables interrupts, etc).
static void recBranchCall(void (*func)(), bool sync_cycles = false)
{
	// In order to make sure a branch test is performed, the nextBranchCycle is set
	// to the current cpu cycle.
	armAsm->Ldr(RXTEMP1, CPUREG_MEM(cycle));
	armAsm->Str(RXTEMP1, CPUREG_MEM(nextEventCycle));

	recInterpret(func, sync_cycles);
	g_branch = 2;
}

//////////////////////////////////////////////////////////////////////////
// ALU
//////////////////////////////////////////////////////////////////////////

enum class ImmOp
{
	ADDIU,
	DADDIU,
	SLTI,
	SLTIU,
	ANDI,
	ORI,
	XORI,
};

// rt = rs op imm16
static void recImmOp(ImmOp op)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const s64 simm = static_cast<s64>(_Imm_);
	const u64 zimm = _ImmU_;
	if (rt == 0)
		return;

	if (GPR_IS_CONST1(rs))
	{
		const u64 s = g_cpuConstRegs[rs];
		u64 result = 0;
		switch (op)
		{
			case ImmOp::ADDIU: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(static_cast<u32>(s) + static_cast<u32>(simm)))); break;
			case ImmOp::DADDIU: result = s + static_cast<u64>(simm); break;
			case ImmOp::SLTI: result = static_cast<s64>(s) < simm; break;
			case ImmOp::SLTIU: result = s < static_cast<u64>(simm); break;
			case ImmOp::ANDI: result = s & zimm; break;
			case ImmOp::ORI: result = s | zimm; break;
			case ImmOp::XORI: result = s ^ zimm; break;
		}
		GPR_SET_CONST(rt, result);
		return;
	}

	if (op == ImmOp::ADDIU)
	{
		_eeMoveGPRtoR(RWTEMP2, rs);
		armAsm->Add(RWTEMP2, RWTEMP2, static_cast<u32>(simm));
		_eeMoveRtoGPR(rt, RWTEMP2);
		return;
	}

	_eeMoveGPRtoR(RXTEMP2, rs);
	switch (op)
	{
		case ImmOp::DADDIU:
			armAsm->Add(RXTEMP2, RXTEMP2, simm);
			break;
		case ImmOp::SLTI:
			armAsm->Cmp(RXTEMP2, simm);
			armAsm->Cset(RXTEMP2, a64::lt);
			break;
		case ImmOp::SLTIU:
			armAsm->Cmp(RXTEMP2, a64::Operand(static_cast<u64>(simm)));
			armAsm->Cset(RXTEMP2, a64::lo);
			break;
		case ImmOp::ANDI:
			armAsm->And(RXTEMP2, RXTEMP2, zimm);
			break;
		case ImmOp::ORI:
			armAsm->Orr(RXTEMP2, RXTEMP2, zimm);
			break;
		case ImmOp::XORI:
			armAsm->Eor(RXTEMP2, RXTEMP2, zimm);
			break;
		default:
			break;
	}
	_eeMoveRtoGPR(rt, RXTEMP2);
}

static void recLUI()
{
	if (!_Rt_)
		return;

	GPR_SET_CONST(_Rt_, static_cast<u64>(static_cast<s64>(static_cast<s32>(cpuRegs.code << 16))));
}

enum class RegOp
{
	ADDU,
	SUBU,
	DADDU,
	DSUBU,
	AND,
	OR,
	XOR,
	NOR,
	SLT,
	SLTU,
};

// rd = rs op rt
static void recRegOp(RegOp op)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	if (rd == 0)
		return;

	if (GPR_IS_CONST2(rs, rt))
	{
		const u64 s = g_cpuConstRegs[rs];
		const u64 t = g_cpuConstRegs[rt];
		u64 result = 0;
		switch (op)
		{
			case RegOp::ADDU: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(static_cast<u32>(s) + static_cast<u32>(t)))); break;
			case RegOp::SUBU: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(static_cast<u32>(s) - static_cast<u32>(t)))); break;
			case RegOp::DADDU: result = s + t; break;
			case RegOp::DSUBU: result = s - t; break;
			case RegOp::AND: result = s & t; break;
			case RegOp::OR: result = s | t; break;
			case RegOp::XOR: result = s ^ t; break;
			case RegOp::NOR: result = ~(s | t); break;
			case RegOp::SLT: result = static_cast<s64>(s) < static_cast<s64>(t); break;
			case RegOp::SLTU: result = s < t; break;
		}
		GPR_SET_CONST(rd, result);
		return;
	}

	if (op == RegOp::ADDU || op == RegOp::SUBU)
	{
		_eeMoveGPRtoR(RWTEMP2, rs);
		_eeMoveGPRtoR(RWTEMP3, rt);
		if (op == RegOp::ADDU)
			armAsm->Add(RWTEMP2, RWTEMP2, RWTEMP3);
		else
			armAsm->Sub(RWTEMP2, RWTEMP2, RWTEMP3);
		_eeMoveRtoGPR(rd, RWTEMP2);
		return;
	}

	_eeMoveGPRtoR(RXTEMP2, rs);
	_eeMoveGPRtoR(RXTEMP3, rt);
	switch (op)
	{
		case RegOp::DADDU:
			armAsm->Add(RXTEMP2, RXTEMP2, RXTEMP3);
			break;
		case RegOp::DSUBU:
			armAsm->Sub(RXTEMP2, RXTEMP2, RXTEMP3);
			break;
		case RegOp::AND:
			armAsm->And(RXTEMP2, RXTEMP2, RXTEMP3);
			break;
		case RegOp::OR:
			armAsm->Orr(RXTEMP2, RXTEMP2, RXTEMP3);
			break;
		case RegOp::XOR:
			armAsm->Eor(RXTEMP2, RXTEMP2, RXTEMP3);
			break;
		case RegOp::NOR:
			armAsm->Orr(RXTEMP2, RXTEMP2, RXTEMP3);
			armAsm->Mvn(RXTEMP2, RXTEMP2);
			break;
		case RegOp::SLT:
			armAsm->Cmp(RXTEMP2, RXTEMP3);
			armAsm->Cset(RXTEMP2, a64::lt);
			break;
		case RegOp::SLTU:
			armAsm->Cmp(RXTEMP2, RXTEMP3);
			armAsm->Cset(RXTEMP2, a64::lo);
			break;
		default:
			break;
	}
	_eeMoveRtoGPR(rd, RXTEMP2);
}

enum class ShiftOp
{
	SLL,
	SRL,
	SRA,
	DSLL,
	DSRL,
	DSRA,
};

// rd = rt op (sa + extra)
static void recShiftImm(ShiftOp op, u32 extra)
{
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	const u32 sa = _Sa_ + extra;
	if (rd == 0)
		return;

	if (GPR_IS_CONST1(rt))
	{
		const u64 t = g_cpuConstRegs[rt];
		u64 result = 0;
		switch (op)
		{
			case ShiftOp::SLL: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(static_cast<u32>(t) << sa))); break;
			case ShiftOp::SRL: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(static_cast<u32>(t) >> sa))); break;
			case ShiftOp::SRA: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(t) >> sa)); break;
			case ShiftOp::DSLL: result = t << sa; break;
			case ShiftOp::DSRL: result = t >> sa; break;
			case ShiftOp::DSRA: result = static_cast<u64>(static_cast<s64>(t) >> sa); break;
		}
		GPR_SET_CONST(rd, result);
		return;
	}

	switch (op)
	{
		case ShiftOp::SLL:
		case ShiftOp::SRL:
		case ShiftOp::SRA:
			_eeMoveGPRtoR(RWTEMP2, rt);
			if (op == ShiftOp::SLL)
				armAsm->Lsl(RWTEMP2, RWTEMP2, sa);
			else if (op == ShiftOp::SRL)
				armAsm->Lsr(RWTEMP2, RWTEMP2, sa);
			else
				armAsm->Asr(RWTEMP2, RWTEMP2, sa);
			_eeMoveRtoGPR(rd, RWTEMP2);
			break;

		case ShiftOp::DSLL:
		case ShiftOp::DSRL:
		case ShiftOp::DSRA:
			_eeMoveGPRtoR(RXTEMP2, rt);
			if (op == ShiftOp::DSLL)
				armAsm->Lsl(RXTEMP2, RXTEMP2, sa);
			else if (op == ShiftOp::DSRL)
				armAsm->Lsr(RXTEMP2, RXTEMP2, sa);
			else
				armAsm->Asr(RXTEMP2, RXTEMP2, sa);
			_eeMoveRtoGPR(rd, RXTEMP2);
			break;
	}
}

// rd = rt op rs
static void recShiftVar(ShiftOp op)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	if (rd == 0)
		return;

	if (GPR_IS_CONST2(rs, rt))
	{
		const u64 t = g_cpuConstRegs[rt];
		const u32 sa32 = static_cast<u32>(g_cpuConstRegs[rs]) & 0x1f;
		const u32 sa64 = static_cast<u32>(g_cpuConstRegs[rs]) & 0x3f;
		u64 result = 0;
		switch (op)
		{
			case ShiftOp::SLL: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(static_cast<u32>(t) << sa32))); break;
			case ShiftOp::SRL: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(static_cast<u32>(t) >> sa32))); break;
			case ShiftOp::SRA: result = static_cast<u64>(static_cast<s64>(static_cast<s32>(t) >> sa32)); break;
			case ShiftOp::DSLL: result = t << sa64; break;
			case ShiftOp::DSRL: result = t >> sa64; break;
			case ShiftOp::DSRA: result = static_cast<u64>(static_cast<s64>(t) >> sa64); break;
		}
		GPR_SET_CONST(rd, result);
		return;
	}

	// The host masks the shift amount to the register width, same as the EE.
	switch (op)
	{
		case ShiftOp::SLL:
		case ShiftOp::SRL:
		case ShiftOp::SRA:
			_eeMoveGPRtoR(RWTEMP2, rt);
			_eeMoveGPRtoR(RWTEMP3, rs);
			if (op == ShiftOp::SLL)
				armAsm->Lsl(RWTEMP2, RWTEMP2, RWTEMP3);
			else if (op == ShiftOp::SRL)
				armAsm->Lsr(RWTEMP2, RWTEMP2, RWTEMP3);
			else
				armAsm->Asr(RWTEMP2, RWTEMP2, RWTEMP3);
			_eeMoveRtoGPR(rd, RWTEMP2);
			break;

		case ShiftOp::DSLL:
		case ShiftOp::DSRL:
		case ShiftOp::DSRA:
			_eeMoveGPRtoR(RXTEMP2, rt);
			_eeMoveGPRtoR(RXTEMP3, rs);
			if (op == ShiftOp::DSLL)
				armAsm->Lsl(RXTEMP2, RXTEMP2, RXTEMP3);
			else if (op == ShiftOp::DSRL)
				armAsm->Lsr(RXTEMP2, RXTEMP2, RXTEMP3);
			else
				armAsm->Asr(RXTEMP2, RXTEMP2, RXTEMP3);
			_eeMoveRtoGPR(rd, RXTEMP2);
			break;
	}
}

static void recMOVZN(bool movn)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	if (rd == 0)
		return;

	if (GPR_IS_CONST1(rt))
	{
		const bool zero = (g_cpuConstRegs[rt] == 0);
		if (zero == movn)
			return;

		if (GPR_IS_CONST1(rs))
		{
			GPR_SET_CONST(rd, g_cpuConstRegs[rs]);
		}
		else
		{
			_eeMoveGPRtoR(RXTEMP2, rs);
			_eeMoveRtoGPR(rd, RXTEMP2);
		}
		return;
	}

	// rd may be a constant which we need to keep if the move doesn't happen, so flush it.
	_flushConstRegs();
	_eeMoveGPRtoR(RXTEMP2, rs);
	_eeMoveGPRtoR(RXTEMP3, rt);
	armAsm->Ldr(RXTEMP4, GPR_MEM(rd));
	armAsm->Cmp(RXTEMP3, 0);
	armAsm->Csel(RXTEMP2, RXTEMP2, RXTEMP4, movn ? a64::ne : a64::eq);
	_eeMoveRtoGPR(rd, RXTEMP2);
}

static void recMFHILO(bool hi)
{
	if (!_Rd_)
		return;

	armAsm->Ldr(RXTEMP2, hi ? CPUREG_MEM(HI) : CPUREG_MEM(LO));
	_eeMoveRtoGPR(_Rd_, RXTEMP2);
}

static void recMTHILO(bool hi)
{
	_eeMoveGPRtoR(RXTEMP2, _Rs_);
	armAsm->Str(RXTEMP2, hi ? CPUREG_MEM(HI) : CPUREG_MEM(LO));
}

static void recMULT(bool is_signed)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;

	_eeMoveGPRtoR(RWTEMP2, rs);
	_eeMoveGPRtoR(RWTEMP3, rt);
	if (is_signed)
		armAsm->Smull(RXTEMP2, RWTEMP2, RWTEMP3);
	else
		armAsm->Umull(RXTEMP2, RWTEMP2, RWTEMP3);

	// Both halves are sign extended into 64 bits, even for the unsigned multiply.
	armAsm->Sxtw(RXTEMP3, RWTEMP2);
	armAsm->Str(RXTEMP3, CPUREG_MEM(LO));
	armAsm->Asr(RXTEMP4, RXTEMP2, 32);
	armAsm->Str(RXTEMP4, CPUREG_MEM(HI));

	// Result is written to both HI/LO and to the _Rd_ (Lo only)
	if (rd != 0)
		_eeMoveRtoGPR(rd, RXTEMP3);
}

//////////////////////////////////////////////////////////////////////////
// Loads and stores
//////////////////////////////////////////////////////////////////////////

// w0 = rs + imm
static void recCalcAddress()
{
	const u32 rs = _Rs_;
	if (GPR_IS_CONST1(rs))
	{
		armAsm->Mov(a64::w0, static_cast<u32>(g_cpuConstRegs[rs]) + static_cast<u32>(static_cast<s32>(_Imm_)));
	}
	else
	{
		_eeMoveGPRtoR(a64::w0, rs);
		if (_Imm_ != 0)
			armAsm->Add(a64::w0, a64::w0, static_cast<u32>(static_cast<s32>(_Imm_)));
	}
}

// Looks up the host pointer for the address in w0 and leaves it in x1.
// Branches to slow_path if the page is mapped to a handler.
static void recVTLBLookup(a64::Label* slow_path)
{
	armAsm->Lsr(a64::w1, a64::w0, VTLB_PAGE_BITS);
	armAsm->Ldr(a64::x1, a64::MemOperand(RVTLBMAP, a64::x1, a64::LSL, 3));
	armAsm->Adds(a64::x1, a64::x1, a64::Operand(a64::w0, a64::UXTW));
	armAsm->B(slow_path, a64::mi);
}

static void recLoad(u32 bits, bool sign)
{
	const u32 rt = _Rt_;
	bool needs_flush = false;
	if (GPR_IS_CONST1(_Rs_))
	{
		const u32 srcadr = static_cast<u32>(g_cpuConstRegs[_Rs_]) + static_cast<u32>(static_cast<s32>(_Imm_));

		// Force event test on EE counter read to improve read + interrupt syncing. Namely ESPN Games.
		if (bits <= 32 && (srcadr & 0xFFFFE000) == 0x10000000)
			needs_flush = true;
	}

	recCalcAddress();
	if (rt != 0)
		GPR_DEL_CONST(rt);

	a64::Label slow_path, done;
	recVTLBLookup(&slow_path);

	const a64::MemOperand mem(a64::x1);
	switch (bits)
	{
		case 8:
			sign ? armAsm->Ldrsb(RXTEMP2, mem) : armAsm->Ldrb(RWTEMP2, mem);
			break;
		case 16:
			sign ? armAsm->Ldrsh(RXTEMP2, mem) : armAsm->Ldrh(RWTEMP2, mem);
			break;
		case 32:
			sign ? armAsm->Ldrsw(RXTEMP2, mem) : armAsm->Ldr(RWTEMP2, mem);
			break;
		case 64:
			armAsm->Ldr(RXTEMP2, mem);
			break;
			jNO_DEFAULT
	}
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	switch (bits)
	{
		case 8:
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem8_t>));
			sign ? armAsm->Sxtb(RXTEMP2, a64::w0) : armAsm->Uxtb(RWTEMP2, a64::w0);
			break;
		case 16:
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem16_t>));
			sign ? armAsm->Sxth(RXTEMP2, a64::w0) : armAsm->Uxth(RWTEMP2, a64::w0);
			break;
		case 32:
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem32_t>));
			sign ? armAsm->Sxtw(RXTEMP2, a64::w0) : armAsm->Mov(RWTEMP2, a64::w0);
			break;
		case 64:
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem64_t>));
			armAsm->Mov(RXTEMP2, a64::x0);
			break;
			jNO_DEFAULT
	}

	armAsm->Bind(&done);
	if (rt != 0)
	{
		// 32-bit W writes clear the upper half, so zero-extended loads are already correct in x9.
		armAsm->Str(RXTEMP2, GPR_MEM(rt));
	}

	if (bits <= 32 && needs_flush)
	{
		_flushConstRegs();
		_eeStorePC(pc);
		g_cpuFlushedPC = true;
		g_branch = 2;
	}
}

static void recStore(u32 bits)
{
	recCalcAddress();

	// Load the value first, both paths need it.
	_eeMoveGPRtoR(RXTEMP2, _Rt_);

	a64::Label slow_path, done;
	recVTLBLookup(&slow_path);

	const a64::MemOperand mem(a64::x1);
	switch (bits)
	{
		case 8:
			armAsm->Strb(RWTEMP2, mem);
			break;
		case 16:
			armAsm->Strh(RWTEMP2, mem);
			break;
		case 32:
			armAsm->Str(RWTEMP2, mem);
			break;
		case 64:
			armAsm->Str(RXTEMP2, mem);
			break;
			jNO_DEFAULT
	}
	armAsm->B(&done);

	// Apple's ABI requires the caller to extend narrow arguments.
	armAsm->Bind(&slow_path);
	switch (bits)
	{
		case 8:
			armAsm->Uxtb(a64::w1, RWTEMP2);
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem8_t>));
			break;
		case 16:
			armAsm->Uxth(a64::w1, RWTEMP2);
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem16_t>));
			break;
		case 32:
			armAsm->Mov(a64::w1, RWTEMP2);
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem32_t>));
			break;
		case 64:
			armAsm->Mov(a64::x1, RXTEMP2);
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem64_t>));
			break;
			jNO_DEFAULT
	}

	armAsm->Bind(&done);
}

static void recLQ()
{
	// MIPS Note: LQ and SQ are special and "silently" align memory addresses, thus
	// an address error due to unaligned access isn't possible like it is on other loads/stores.
	const u32 rt = _Rt_;
	recCalcAddress();
	armAsm->And(a64::w0, a64::w0, ~0xFu);
	if (rt != 0)
		GPR_DEL_CONST(rt);

	a64::Label slow_path, done;
	recVTLBLookup(&slow_path);
	armAsm->Ldr(a64::q0, a64::MemOperand(a64::x1));
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead128));

	armAsm->Bind(&done);
	if (rt != 0)
		armAsm->Str(a64::q0, GPR_MEM(rt));
}

static void recSQ()
{
	recCalcAddress();
	armAsm->And(a64::w0, a64::w0, ~0xFu);

	// The upper 64 bits are never constant, so write back the lower half first.
	if (GPR_IS_CONST1(_Rt_) && _Rt_ != 0)
		_flushConstRegs();
	if (_Rt_ == 0)
		armAsm->Movi(a64::v0.V2D(), 0);
	else
		armAsm->Ldr(a64::q0, GPR_MEM(_Rt_));

	a64::Label slow_path, done;
	recVTLBLookup(&slow_path);
	armAsm->Str(a64::q0, a64::MemOperand(a64::x1));
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite128));

	armAsm->Bind(&done);
}

static void recLWC1()
{
	recCalcAddress();

	a64::Label slow_path, done;
	recVTLBLookup(&slow_path);
	armAsm->Ldr(RWTEMP2, a64::MemOperand(a64::x1));
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem32_t>));
	armAsm->Mov(RWTEMP2, a64::w0);

	armAsm->Bind(&done);
	armAsm->Str(RWTEMP2, FPR_MEM(_Rt_));
}

static void recSWC1()
{
	recCalcAddress();
	armAsm->Ldr(RWTEMP2, FPR_MEM(_Rt_));

	a64::Label slow_path, done;
	recVTLBLookup(&slow_path);
	armAsm->Str(RWTEMP2, a64::MemOperand(a64::x1));
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	armAsm->Mov(a64::w1, RWTEMP2);
	armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem32_t>));

	armAsm->Bind(&done);
}

//////////////////////////////////////////////////////////////////////////
// Jumps and branches
//////////////////////////////////////////////////////////////////////////

static u32 recJumpTarget(u32 target)
{
	return EmuConfig.Gamefixes.GoemonTlbHack ? vtlb_V2P(target) : target;
}

static void recJ()
{
	const u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	recompileNextInstruction(true);
	SetBranchImm(recJumpTarget(newpc));
}

static void recJAL()
{
	const u32 newpc = (_InstrucTarget_ << 2) + (pc & 0xf0000000);
	GPR_SET_CONST(31, pc + 4);

	recompileNextInstruction(true);
	SetBranchImm(recJumpTarget(newpc));
}

static void recLoadJumpRegister(u32 rs)
{
	_eeMoveGPRtoR(RWBRANCH, rs);
	if (EmuConfig.Gamefixes.GoemonTlbHack)
	{
		armAsm->Mov(a64::w0, RWBRANCH);
		armEmitCall(reinterpret_cast<const void*>(vtlb_V2P));
		armAsm->Mov(RWBRANCH, a64::w0);
	}
}

static void recJR()
{
	recLoadJumpRegister(_Rs_);
	recompileNextInstruction(true);
	SetBranchReg();
}

static void recJALR()
{
	const u32 newpc = pc + 4;

	// Read the target before the link, in case rd == rs.
	recLoadJumpRegister(_Rs_);
	if (_Rd_)
		GPR_SET_CONST(_Rd_, newpc);

	recompileNextInstruction(true);
	SetBranchReg();
}

enum class BranchCond
{
	EQ,
	NE,
	LEZ,
	GTZ,
	LTZ,
	GEZ,
	COP0F,
	COP0T,
	COP1F,
	COP1T,
	COP2F,
	COP2T,
};

// Emits code which branches to not_taken if the branch condition is false.
static void recEmitBranchCondition(BranchCond cond, u32 rs, u32 rt, a64::Label* not_taken)
{
	switch (cond)
	{
		case BranchCond::EQ:
		case BranchCond::NE:
			_eeMoveGPRtoR(RXTEMP2, rs);
			_eeMoveGPRtoR(RXTEMP3, rt);
			armAsm->Cmp(RXTEMP2, RXTEMP3);
			armAsm->B(not_taken, (cond == BranchCond::EQ) ? a64::ne : a64::eq);
			break;

		case BranchCond::LEZ:
			_eeMoveGPRtoR(RXTEMP2, rs);
			armAsm->Cmp(RXTEMP2, 0);
			armAsm->B(not_taken, a64::gt);
			break;

		case BranchCond::GTZ:
			_eeMoveGPRtoR(RXTEMP2, rs);
			armAsm->Cmp(RXTEMP2, 0);
			armAsm->B(not_taken, a64::le);
			break;

		case BranchCond::LTZ:
			_eeMoveGPRtoR(RXTEMP2, rs);
			armAsm->Tbz(RXTEMP2, 63, not_taken);
			break;

		case BranchCond::GEZ:
			_eeMoveGPRtoR(RXTEMP2, rs);
			armAsm->Tbnz(RXTEMP2, 63, not_taken);
			break;

		case BranchCond::COP0F:
		case BranchCond::COP0T:
		{
			// CPCOND0: ((DMAC_STAT.CIS | ~DMAC_PCR.CPC) & 0x3FF) == 0x3ff
			armMoveAddressToReg(RXTEMP4, &psHu32(DMAC_PCR));
			armAsm->Ldr(RWTEMP2, a64::MemOperand(RXTEMP4));
			armMoveAddressToReg(RXTEMP4, &psHu32(DMAC_STAT));
			armAsm->Ldr(RWTEMP3, a64::MemOperand(RXTEMP4));
			armAsm->Orn(RWTEMP2, RWTEMP3, RWTEMP2);
			armAsm->And(RWTEMP2, RWTEMP2, 0x3ff);
			armAsm->Cmp(RWTEMP2, 0x3ff);
			armAsm->B(not_taken, (cond == BranchCond::COP0F) ? a64::eq : a64::ne);
		}
		break;

		case BranchCond::COP1F:
		case BranchCond::COP1T:
			// C flag in the FPU control register 31.
			armAsm->Ldr(RWTEMP2, FPUREG_MEM(fprc[31]));
			if (cond == BranchCond::COP1F)
				armAsm->Tbnz(RWTEMP2, 23, not_taken);
			else
				armAsm->Tbz(RWTEMP2, 23, not_taken);
			break;

		case BranchCond::COP2F:
		case BranchCond::COP2T:
			// VU1 running flag in VPU_STAT.
			armMoveAddressToReg(RXTEMP4, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(RWTEMP2, a64::MemOperand(RXTEMP4));
			if (cond == BranchCond::COP2F)
				armAsm->Tbnz(RWTEMP2, 8, not_taken);
			else
				armAsm->Tbz(RWTEMP2, 8, not_taken);
			break;
	}
}

static std::optional<bool> recEvaluateConstBranch(BranchCond cond, u32 rs, u32 rt)
{
	switch (cond)
	{
		case BranchCond::EQ:
		case BranchCond::NE:
			if (rs == rt)
				return (cond == BranchCond::EQ);
			if (!GPR_IS_CONST2(rs, rt))
				return std::nullopt;
			return (cond == BranchCond::EQ) == (g_cpuConstRegs[rs] == g_cpuConstRegs[rt]);

		case BranchCond::LEZ:
		case BranchCond::GTZ:
		case BranchCond::LTZ:
		case BranchCond::GEZ:
		{
			if (!GPR_IS_CONST1(rs))
				return std::nullopt;
			const s64 s = static_cast<s64>(g_cpuConstRegs[rs]);
			switch (cond)
			{
				case BranchCond::LEZ: return (s <= 0);
				case BranchCond::GTZ: return (s > 0);
				case BranchCond::LTZ: return (s < 0);
				default: return (s >= 0);
			}
		}

		default:
			return std::nullopt;
	}
}

static void recBranch(BranchCond cond, bool likely, bool link)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 branchTo = static_cast<u32>(static_cast<s32>(_Imm_) * 4) + pc;

	// The link register is set before the condition is evaluated, same as the interpreter.
	if (link)
		GPR_SET_CONST(31, pc + 4);

	if (const std::optional<bool> taken = recEvaluateConstBranch(cond, rs, rt); taken.has_value())
	{
		if (taken.value())
		{
			recompileNextInstruction(true);
			SetBranchImm(branchTo);
		}
		else
		{
			// Likely branches skip the delay slot when not taken.
			if (!likely)
				recompileNextInstruction(true);
			SetBranchImm(likely ? (pc + 4) : pc);
		}
		return;
	}

	_flushConstRegs();

	a64::Label not_taken;
	recEmitBranchCondition(cond, rs, rt, &not_taken);

	// First up is the Branch Taken Path : Save the recompiler's state, compile the
	// DelaySlot, and issue a BranchTest insertion.  The state is reloaded below for
	// the "did not branch" path (maintains consts, register allocations, and other optimizations).
	SaveBranchState();
	recompileNextInstruction(true);
	SetBranchImm(branchTo);

	armAsm->Bind(&not_taken);

	// if it's a likely branch then we'll need to skip the delay slot here, since
	// MIPS cancels the delay slot instruction when branches aren't taken.
	LoadBranchState();
	if (!likely)
	{
		pc -= 4; // instruction rewinder for delay slot, if non-likely.
		recompileNextInstruction(true);
	}

	SetBranchImm(pc); // start a new recompiled block.
}

//////////////////////////////////////////////////////////////////////////
// COP0
//////////////////////////////////////////////////////////////////////////

static void recMFC0()
{
	if (_Rd_ == 9)
	{
		// Count is derived from the cycle counter.
		iFlushBlockCycles();
		armAsm->Ldr(RXTEMP3, CPUREG_MEM(lastCOP0Cycle));
		armAsm->Sub(RXTEMP3, RXTEMP2, RXTEMP3);
		armAsm->Ldr(RWTEMP4, CP0_MEM(9));
		armAsm->Add(RWTEMP4, RWTEMP4, RWTEMP3);
		armAsm->Str(RWTEMP4, CP0_MEM(9));
		armAsm->Str(RXTEMP2, CPUREG_MEM(lastCOP0Cycle));
		if (_Rt_)
			_eeMoveRtoGPR(_Rt_, RWTEMP4);
		return;
	}

	if (!_Rt_)
		return;

	if (_Rd_ == 25 || _Rd_ == 24)
	{
		// Performance counters and debug registers.
		recInterpretCurrent(true);
		return;
	}

	armAsm->Ldr(RWTEMP2, CP0_MEM(_Rd_));
	_eeMoveRtoGPR(_Rt_, RWTEMP2);
}

static void recMTC0()
{
	switch (_Rd_)
	{
		case 12: // Status, may enable interrupts
		case 16: // Config
		case 9: // Count
		case 25: // Performance counters
		case 24: // Debug
			recBranchCall(GetCurrentInstruction().interpret, true);
			break;

		default:
			_eeMoveGPRtoR(RWTEMP2, _Rt_);
			armAsm->Str(RWTEMP2, CP0_MEM(_Rd_));
			break;
	}
}

static void recDI()
{
	if (!g_recompilingDelaySlot)
		recompileNextInstruction(false); // DI execution is delayed by one instruction

	a64::Label disable, done;
	armAsm->Ldr(RWTEMP2, CP0_MEM(12));
	armAsm->Tst(RWTEMP2, 0x20006); // EXL | ERL | EDI
	armAsm->B(&disable, a64::ne);
	armAsm->Tst(RWTEMP2, 0x18); // KSU
	armAsm->B(&done, a64::ne);
	armAsm->Bind(&disable);
	armAsm->And(RWTEMP2, RWTEMP2, ~(u32)0x10000); // EIE
	armAsm->Str(RWTEMP2, CP0_MEM(12));
	armAsm->Bind(&done);
}

//////////////////////////////////////////////////////////////////////////
// Decoder
//////////////////////////////////////////////////////////////////////////

static void recSPECIAL()
{
	switch (_Funct_)
	{
		case 0: recShiftImm(ShiftOp::SLL, 0); break;
		case 2: recShiftImm(ShiftOp::SRL, 0); break;
		case 3: recShiftImm(ShiftOp::SRA, 0); break;
		case 4: recShiftVar(ShiftOp::SLL); break;
		case 6: recShiftVar(ShiftOp::SRL); break;
		case 7: recShiftVar(ShiftOp::SRA); break;
		case 8: recJR(); break;
		case 9: recJALR(); break;
		case 10: recMOVZN(false); break;
		case 11: recMOVZN(true); break;
		case 12: // SYSCALL
			if (GPR_IS_CONST1(3))
			{
				// If it's FlushCache or iFlushCache, we can skip it since we don't support cache in the JIT.
				if ((g_cpuConstRegs[3] & 0xFF) == 0x64 || (g_cpuConstRegs[3] & 0xFF) == 0x68)
				{
					// Emulate the amount of cycles it takes for the exception handlers to run
					// This number was found by using github.com/F0bes/flushcache-cycles
					s_nBlockCycles += 5650;
					break;
				}
			}
			recInterpretCurrent();
			g_branch = 2; // Indirect branch with event check.
			break;
		case 13: // BREAK
			recInterpretCurrent();
			g_branch = 2; // Indirect branch with event check.
			break;
		case 15: break; // SYNC
		case 16: recMFHILO(true); break;
		case 17: recMTHILO(true); break;
		case 18: recMFHILO(false); break;
		case 19: recMTHILO(false); break;
		case 20: recShiftVar(ShiftOp::DSLL); break;
		case 22: recShiftVar(ShiftOp::DSRL); break;
		case 23: recShiftVar(ShiftOp::DSRA); break;
		case 24: recMULT(true); break;
		case 25: recMULT(false); break;
		case 32: recRegOp(RegOp::ADDU); break; // ADD, overflow exceptions aren't emulated (same as x86)
		case 33: recRegOp(RegOp::ADDU); break;
		case 34: recRegOp(RegOp::SUBU); break; // SUB
		case 35: recRegOp(RegOp::SUBU); break;
		case 36: recRegOp(RegOp::AND); break;
		case 37: recRegOp(RegOp::OR); break;
		case 38: recRegOp(RegOp::XOR); break;
		case 39: recRegOp(RegOp::NOR); break;
		case 42: recRegOp(RegOp::SLT); break;
		case 43: recRegOp(RegOp::SLTU); break;
		case 44: recRegOp(RegOp::DADDU); break; // DADD
		case 45: recRegOp(RegOp::DADDU); break;
		case 46: recRegOp(RegOp::DSUBU); break; // DSUB
		case 47: recRegOp(RegOp::DSUBU); break;
		case 48: // TGE
		case 49: // TGEU
		case 50: // TLT
		case 51: // TLTU
		case 52: // TEQ
		case 54: // TNE
			// Traps may raise an exception.
			recInterpretCurrent();
			g_branch = 2;
			break;
		case 56: recShiftImm(ShiftOp::DSLL, 0); break;
		case 58: recShiftImm(ShiftOp::DSRL, 0); break;
		case 59: recShiftImm(ShiftOp::DSRA, 0); break;
		case 60: recShiftImm(ShiftOp::DSLL, 32); break;
		case 62: recShiftImm(ShiftOp::DSRL, 32); break;
		case 63: recShiftImm(ShiftOp::DSRA, 32); break;
		default: recInterpretCurrent(); break; // MFSA, MTSA, DIV, DIVU, unknown
	}
}

static void recREGIMM()
{
	switch (_Rt_)
	{
		case 0: recBranch(BranchCond::LTZ, false, false); break;
		case 1: recBranch(BranchCond::GEZ, false, false); break;
		case 2: recBranch(BranchCond::LTZ, true, false); break;
		case 3: recBranch(BranchCond::GEZ, true, false); break;
		case 16: recBranch(BranchCond::LTZ, false, true); break;
		case 17: recBranch(BranchCond::GEZ, false, true); break;
		case 18: recBranch(BranchCond::LTZ, true, true); break;
		case 19: recBranch(BranchCond::GEZ, true, true); break;
		case 8: // TGEI
		case 9: // TGEIU
		case 10: // TLTI
		case 11: // TLTIU
		case 12: // TEQI
		case 14: // TNEI
			recInterpretCurrent();
			g_branch = 2;
			break;
		default: recInterpretCurrent(); break; // MTSAB, MTSAH
	}
}

static void recCOP0()
{
	switch (_Rs_)
	{
		case 0: recMFC0(); break;
		case 4: recMTC0(); break;
		case 8: // BC0
			switch (_Rt_)
			{
				case 0: recBranch(BranchCond::COP0F, false, false); break;
				case 1: recBranch(BranchCond::COP0T, false, false); break;
				case 2: recBranch(BranchCond::COP0F, true, false); break;
				case 3: recBranch(BranchCond::COP0T, true, false); break;
				default: recInterpretCurrent(); break;
			}
			break;
		case 16: // C0
			switch (_Funct_)
			{
				case 24: recBranchCall(GetCurrentInstruction().interpret); break; // ERET
				case 56: recBranchCall(GetCurrentInstruction().interpret); break; // EI
				case 57: recDI(); break;
				default: recInterpretCurrent(); break; // TLBR, TLBWI, TLBWR, TLBP
			}
			break;
		default:
			recInterpretCurrent();
			break;
	}
}

static void recCOP1()
{
	if (_Rs_ == 8) // BC1
	{
		switch (_Rt_)
		{
			case 0: recBranch(BranchCond::COP1F, false, false); break;
			case 1: recBranch(BranchCond::COP1T, false, false); break;
			case 2: recBranch(BranchCond::COP1F, true, false); break;
			case 3: recBranch(BranchCond::COP1T, true, false); break;
			default: recInterpretCurrent(); break;
		}
		return;
	}

	recInterpretCurrent();
}

static void recCOP2()
{
	if (_Rs_ == 8) // BC2
	{
		switch (_Rt_)
		{
			case 0: recBranch(BranchCond::COP2F, false, false); break;
			case 1: recBranch(BranchCond::COP2T, false, false); break;
			case 2: recBranch(BranchCond::COP2F, true, false); break;
			case 3: recBranch(BranchCond::COP2T, true, false); break;
			default: recInterpretCurrent(true); break;
		}
		return;
	}

	// VU0 macro mode is synchronised with VU0 micro mode through cpuRegs.cycle.
	recInterpretCurrent(true);
}

static void recCompileInstruction()
{
	switch (cpuRegs.code >> 26)
	{
		case 0: recSPECIAL(); break;
		case 1: recREGIMM(); break;
		case 2: recJ(); break;
		case 3: recJAL(); break;
		case 4: recBranch(BranchCond::EQ, false, false); break;
		case 5: recBranch(BranchCond::NE, false, false); break;
		case 6: recBranch(BranchCond::LEZ, false, false); break;
		case 7: recBranch(BranchCond::GTZ, false, false); break;
		case 8: recImmOp(ImmOp::ADDIU); break; // ADDI, overflow exceptions aren't emulated (same as x86)
		case 9: recImmOp(ImmOp::ADDIU); break;
		case 10: recImmOp(ImmOp::SLTI); break;
		case 11: recImmOp(ImmOp::SLTIU); break;
		case 12: recImmOp(ImmOp::ANDI); break;
		case 13: recImmOp(ImmOp::ORI); break;
		case 14: recImmOp(ImmOp::XORI); break;
		case 15: recLUI(); break;
		case 16: recCOP0(); break;
		case 17: recCOP1(); break;
		case 18: recCOP2(); break;
		case 20: recBranch(BranchCond::EQ, true, false); break;
		case 21: recBranch(BranchCond::NE, true, false); break;
		case 22: recBranch(BranchCond::LEZ, true, false); break;
		case 23: recBranch(BranchCond::GTZ, true, false); break;
		case 24: recImmOp(ImmOp::DADDIU); break; // DADDI
		case 25: recImmOp(ImmOp::DADDIU); break;
		case 30: recLQ(); break;
		case 31: recSQ(); break;
		case 32: recLoad(8, true); break;
		case 33: recLoad(16, true); break;
		case 35: recLoad(32, true); break;
		case 36: recLoad(8, false); break;
		case 37: recLoad(16, false); break;
		case 39: recLoad(32, false); break;
		case 40: recStore(8); break;
		case 41: recStore(16); break;
		case 43: recStore(32); break;
		case 47: break; // CACHE, not emulated by the recompiler
		case 49: recLWC1(); break;
		case 51: break; // PREF
		case 54: recInterpretCurrent(true); break; // LQC2
		case 55: recLoad(64, false); break;
		case 57: recSWC1(); break;
		case 62: recInterpretCurrent(true); break; // SQC2
		case 63: recStore(64); break;
		default: recInterpretCurrent(); break; // MMI, LDL/LDR/SDL/SDR, LWL/LWR/SWL/SWR, unknown
	}
}

//////////////////////////////////////////////////////////////////////////
// Debugger support
//////////////////////////////////////////////////////////////////////////

static void dynarecCheckBreakpoint()
{
	u32 pc = cpuRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_EE, pc) != 0)
	{
		CBreakPoints::ClearSkipFirst(BREAKPOINT_EE);
		return;
	}

	const int bpFlags = isBreakpointNeeded(pc);
	bool hit = false;
	//check breakpoint at current pc
	if (bpFlags & 1)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_EE, pc);
		if (cond == NULL || cond->Evaluate())
		{
			if (CBreakPoints::HandleBreakpointHit(BREAKPOINT_EE, pc))
				hit = true;
		}
	}
	//check breakpoint in delay slot
	if (bpFlags & 2)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_EE, pc + 4);
		if (cond == NULL || cond->Evaluate())
			if (CBreakPoints::HandleBreakpointHit(BREAKPOINT_EE, pc + 4))
				hit = true;
	}

	if (!hit)
		return;

	CBreakPoints::SetBreakpointTriggered(true, BREAKPOINT_EE);
	VMManager::SetPaused(true);
	recExitExecution();
}

static void dynarecMemcheck(size_t i)
{
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_EE, cpuRegs.pc) != 0)
	{
		CBreakPoints::ClearSkipFirst(BREAKPOINT_EE);
		return;
	}

	const auto mc = CBreakPoints::GetMemChecks(BREAKPOINT_EE)[i];

	if (mc.hasCond)
	{
		if (!mc.cond.Evaluate())
			return;
	}

	if (!CBreakPoints::HandleMemCheckHit(BREAKPOINT_EE, mc.start, mc.end))
		return;

	CBreakPoints::SetBreakpointTriggered(true, BREAKPOINT_EE);
	VMManager::SetPaused(true);
	recExitExecution();
}

static void recMemcheck(u32 op, u32 bits, bool store)
{
	_flushConstRegs();
	_eeStorePC(pc);

	const auto checks = CBreakPoints::GetMemChecks(BREAKPOINT_EE);
	for (size_t i = 0; i < checks.size(); i++)
	{
		if (checks[i].result == 0)
			continue;
		if ((checks[i].memCond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((checks[i].memCond & MEMCHECK_READ) == 0 && !store)
			continue;

		// Recompute the address each time, the call below clobbers the temporaries.
		// w9 = access address, w10 = access address + size
		armAsm->Ldr(a64::w0, GPR_MEM((op >> 21) & 0x1F));
		if (static_cast<s16>(op) != 0)
			armAsm->Add(a64::w0, a64::w0, static_cast<u32>(static_cast<s32>(static_cast<s16>(op))));
		if (bits == 128)
			armAsm->And(a64::w0, a64::w0, ~0x0Fu);
		armEmitCall(reinterpret_cast<const void*>(standardizeBreakpointAddress));
		armAsm->Mov(RWTEMP2, a64::w0);
		armAsm->Add(RWTEMP3, RWTEMP2, bits / 8);

		// logic: memAddress < bpEnd && bpStart < memAddress+memSize
		a64::Label next;
		armAsm->Mov(RWTEMP4, standardizeBreakpointAddress(checks[i].end));
		armAsm->Cmp(RWTEMP2, RWTEMP4); // address < end
		armAsm->B(&next, a64::ge); // if address >= end then goto next
		armAsm->Mov(RWTEMP4, standardizeBreakpointAddress(checks[i].start));
		armAsm->Cmp(RWTEMP4, RWTEMP3); // start < address+size
		armAsm->B(&next, a64::ge); // if start >= address+size then goto next

		// hit the breakpoint
		if (checks[i].result & MEMCHECK_BREAK)
		{
			armAsm->Mov(a64::x0, i);
			armEmitCall(reinterpret_cast<const void*>(dynarecMemcheck));
		}

		armAsm->Bind(&next);
	}
}

static bool encodeBreakpoint()
{
	if (isBreakpointNeeded(pc) != 0)
	{
		_flushConstRegs();
		_eeStorePC(pc);
		armEmitCall(reinterpret_cast<const void*>(dynarecCheckBreakpoint));
		return true;
	}

	return false;
}

static bool encodeMemcheck()
{
	const int needed = isMemcheckNeeded(pc);
	if (needed == 0)
		return false;

	const u32 op = memRead32(needed == 2 ? pc + 4 : pc);
	const OPCODE& opcode = GetInstruction(op);

	const bool store = (opcode.flags & IS_STORE) != 0;
	switch (opcode.flags & MEMTYPE_MASK)
	{
		case MEMTYPE_BYTE:
			recMemcheck(op, 8, store);
			break;
		case MEMTYPE_HALF:
			recMemcheck(op, 16, store);
			break;
		case MEMTYPE_WORD:
			recMemcheck(op, 32, store);
			break;
		case MEMTYPE_DWORD:
			recMemcheck(op, 64, store);
			break;
		case MEMTYPE_QWORD:
			recMemcheck(op, 128, store);
			break;
	}

	return true;
}

static bool recIsBranchInstruction(u32 code)
{
	switch (code >> 26)
	{
		case 0:
			return ((code & 0x3F) == 8 || (code & 0x3F) == 9); // jr, jalr

		case 1:
			switch ((code >> 16) & 0x1F)
			{
				case 0:
				case 1:
				case 2:
				case 3:
				case 0x10:
				case 0x11:
				case 0x12:
				case 0x13:
					return true;
			}
			return false;

		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
			return true;

		default:
			return false;
	}
}

static void recompileNextInstruction(bool delayslot)
{
	if (EmuConfig.EnablePatches)
		Patch::ApplyDynamicPatches(pc);

	// add breakpoint
	if (!delayslot)
	{
		if (encodeBreakpoint() || encodeMemcheck())
		{
			armAsm->Mov(a64::w0, static_cast<u32>(BREAKPOINT_EE));
			armEmitCall(reinterpret_cast<const void*>(&CBreakPoints::CommitClearSkipFirst));
		}
	}

	const u32* s_pCode = static_cast<const u32*>(PSM(pc));
	pxAssert(s_pCode);

	cpuRegs.code = *s_pCode;

	if (!delayslot)
	{
		pc += 4;
		g_cpuFlushedPC = false;
	}
	else
	{
		// increment after recompiling so that pc points to the branch during recompilation
		g_recompilingDelaySlot = true;
	}

	const OPCODE& opcode = GetCurrentInstruction();

	// Check for branch in delay slot, new code by FlatOut.
	// Gregory tested this in 2017 using the ps2autotests suite and remarked "So far we return 1 (even with this PR), and the HW 2.
	// Original PR and discussion at https://github.com/PCSX2/pcsx2/pull/1783 so we don't forget this information.
	if (delayslot && recIsBranchInstruction(cpuRegs.code))
	{
		DevCon.Warning("Branch %x in delay slot!", cpuRegs.code);
		pc += 4;
		g_cpuFlushedPC = false;
		g_recompilingDelaySlot = false;
		return;
	}

	// Check for NOP
	if (cpuRegs.code == 0x00000000)
	{
		// Note: Tests on a ps2 suggested more like 5 cycles for a NOP. But there's many factors in this..
		s_nBlockCycles += 9 * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
	}
	else
	{
		//If the COP0 DIE bit is disabled, cycles should be doubled.
		s_nBlockCycles += opcode.cycles * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
		recCompileInstruction();
	}

	if (delayslot)
	{
		pc += 4;
		g_cpuFlushedPC = false;
		g_recompilingDelaySlot = false;
	}

	cpuRegs.code = *s_pCode;
}

//////////////////////////////////////////////////////////////////////////
// Self-modifying code
//////////////////////////////////////////////////////////////////////////

static void dyna_block_discard(u32 start, u32 sz)
{
	eeRecPerfLog.Write(Color_StrongGray, "Clearing Manual Block @ 0x%08X  [size=%d]", start, sz * 4);
	recClear(start, sz);
}

// called when a page under manual protection has been run enough times to be a candidate
// for being reset under the faster vtlb write protection.  All blocks in the page are cleared
// and the block is re-assigned for write protection.
static void dyna_page_reset(u32 start, u32 sz)
{
	recClear(start & ~0xfffUL, 0x400);
	manual_counter[start >> 12]++;
	mmap_MarkCountedRamPage(start);
}

static void memory_protect_recompiled_code(u32 startpc, u32 size)
{
	u32 inpage_ptr = HWADDR(startpc);
	const u32 inpage_sz = size * 4;

	// The kernel context register is stored @ 0x800010C0-0x80001300
	// The EENULL thread context register is stored @ 0x81000-....
	const bool contains_thread_stack = ((startpc >> 12) == 0x81) || ((startpc >> 12) == 0x80001);

	// note: blocks are guaranteed to reside within the confines of a single page.
	const vtlb_ProtectionMode PageType = contains_thread_stack ? ProtMode_Manual : mmap_GetRamPageInfo(inpage_ptr);

	switch (PageType)
	{
		case ProtMode_NotRequired:
			break;

		case ProtMode_None:
		case ProtMode_Write:
			mmap_MarkCountedRamPage(inpage_ptr);
			manual_page[inpage_ptr >> 12] = 0;
			break;

		case ProtMode_Manual:
		{
			// Arguments for the discard/reset dispatchers.
			armAsm->Mov(a64::w0, inpage_ptr);
			armAsm->Mov(a64::w1, inpage_sz / 4);

			u32 lpc = inpage_ptr;
			u32 stg = inpage_sz;

			armMoveAddressToReg(RXTEMP1, PSM(lpc));
			while (stg > 0)
			{
				// Compare 8 bytes at a time where possible.
				if (stg >= 8)
				{
					armAsm->Ldr(RXTEMP2, a64::MemOperand(RXTEMP1, lpc - inpage_ptr));
					armAsm->Mov(RXTEMP3, *reinterpret_cast<const u64*>(PSM(lpc)));
					armAsm->Cmp(RXTEMP2, RXTEMP3);
					armEmitCondBranch(a64::ne, DispatchBlockDiscard);
					stg -= 8;
					lpc += 8;
				}
				else
				{
					armAsm->Ldr(RWTEMP2, a64::MemOperand(RXTEMP1, lpc - inpage_ptr));
					armAsm->Mov(RWTEMP3, *reinterpret_cast<const u32*>(PSM(lpc)));
					armAsm->Cmp(RWTEMP2, RWTEMP3);
					armEmitCondBranch(a64::ne, DispatchBlockDiscard);
					stg -= 4;
					lpc += 4;
				}
			}

			// Tweakpoint!  3 is a 'magic' number representing the number of times a counted block
			// is re-protected before the recompiler gives up and sets it up as an uncounted (permanent)
			// manual block.  Higher thresholds result in more recompilations for blocks that share code
			// and data on the same page.  Side effects of a lower threshold: over extended gameplay
			// with several map changes, a game's overall performance could degrade.

			// (ideally, perhaps, manual_counter should be reset to 0 every few minutes?)

			if (!contains_thread_stack && manual_counter[inpage_ptr >> 12] <= 3)
			{
				// Counted blocks add a weighted (by block size) value into manual_page each time they're
				// run.  If the block gets run a lot, it resets and re-protects itself in the hope
				// that whatever forced it to be manually-checked before was a 1-time deal.

				// Counted blocks have a secondary threshold check in manual_counter, which forces a block
				// to 'uncounted' mode if it's recompiled several times.  This protects against excessive
				// recompilation of blocks that reside on the same codepage as data.
				armMoveAddressToReg(RXTEMP1, &manual_page[inpage_ptr >> 12]);
				armAsm->Ldrh(RWTEMP2, a64::MemOperand(RXTEMP1));
				armAsm->Add(RWTEMP2, RWTEMP2, size);
				armAsm->Strh(RWTEMP2, a64::MemOperand(RXTEMP1));
				// 16-bit carry out.
				armAsm->Tst(RWTEMP2, 0x10000);
				armEmitCondBranch(a64::ne, DispatchPageReset);

				// note: clearcnt is measured per-page, not per-block!
				eeRecPerfLog.Write("Manual block @ %08X : size =%3d  page/offs = 0x%05X/0x%03X  inpgsz = %d  clearcnt = %d",
					startpc, size, inpage_ptr >> 12, inpage_ptr & 0xfff, inpage_sz, manual_counter[inpage_ptr >> 12]);
			}
			else
			{
				eeRecPerfLog.Write("Uncounted Manual block @ 0x%08X : size =%3d page/offs = 0x%05X/0x%03X  inpgsz = %d",
					startpc, size, inpage_ptr >> 12, inpage_ptr & 0xfff, inpage_sz);
			}
		}
		break;
	}
}

// Skip MPEG Game-Fix
static bool skipMPEG_By_Pattern(u32 sPC)
{
	if (!CHECK_SKIPMPEGHACK)
		return 0;

	// sceMpegIsEnd: lw reg, 0x40(a0); jr ra; lw v0, 0(reg)
	if ((s_nEndBlock == sPC + 12) && (memRead32(sPC + 4) == 0x03e00008))
	{
		const u32 code = memRead32(sPC);
		const u32 p1 = 0x8c800040;
		const u32 p2 = 0x8c020000 | (code & 0x1f0000) << 5;
		if ((code & 0xffe0ffff) != p1)
			return 0;
		if (memRead32(sPC + 8) != p2)
			return 0;
		armAsm->Mov(RXTEMP2, 1);
		armAsm->Str(RXTEMP2, GPR_MEM(2)); // v0
		armAsm->Ldr(RWBRANCH, GPR_MEM(31)); // ra
		armAsm->Str(RWBRANCH, CPUREG_MEM(pc));
		iBranchTest();
		g_branch = 1;
		pc = s_nEndBlock;
		Console.WriteLn(Color_StrongGreen, "sceMpegIsEnd pattern found! Recompiling skip video fix...");
		return 1;
	}
	return 0;
}

static bool recSkipTimeoutLoop(s32 reg, bool is_timeout_loop)
{
	if (!EmuConfig.Speedhacks.WaitLoop || !is_timeout_loop)
		return false;

	DevCon.WriteLn("[EE] Skipping timeout loop at 0x%08X -> 0x%08X", s_pCurBlockEx->startpc, s_nEndBlock);

	// basically, if the time it takes the loop to run is shorter than the
	// time to the next event, then we want to skip ahead to the event, but
	// update v0 to reflect how long the loop would have run for.

	// if (cycle >= nextEventCycle) { jump to dispatcher, we're running late }
	// new_cycles = min(v0 * 8, nextEventCycle)
	// new_v0 = (new_cycles - cycles) / 8
	// if new_v0 > 0 { jump to dispatcher because loop exited early }
	// else new_v0 is 0, so exit loop

	armAsm->Ldr(RXTEMP1, CPUREG_MEM(cycle));
	armAsm->Ldr(RXTEMP2, CPUREG_MEM(nextEventCycle));
	armAsm->Cmp(RXTEMP1, RXTEMP2);
	armEmitCondBranch(a64::hs, DispatcherEvent); // jump to dispatcher if event immediately

	armAsm->Ldr(RWTEMP3, GPR_MEM(reg)); // x10 = v0 (zero extended)
	armAsm->Add(RXTEMP4, RXTEMP1, a64::Operand(RXTEMP3, a64::LSL, 3)); // x11 = v0 * 8 + cycle
	armAsm->Cmp(RXTEMP2, RXTEMP4);
	armAsm->Csel(RXTEMP4, RXTEMP2, RXTEMP4, a64::lo); // x11 = new_cycles = min(v0 * 8, nextEventCycle)
	armAsm->Str(RXTEMP4, CPUREG_MEM(cycle)); // writeback new_cycles
	armAsm->Sub(RXTEMP4, RXTEMP4, RXTEMP1); // new_cycles -= cycle
	armAsm->Lsr(RXTEMP4, RXTEMP4, 3); // compute new v0 value
	armAsm->Subs(RWTEMP3, RWTEMP3, RWTEMP4); // v0 -= cycle_diff
	armAsm->Str(RWTEMP3, GPR_MEM(reg)); // write back new value of v0
	armEmitCondBranch(a64::ne, DispatcherEvent); // jump to dispatcher if new v0 is not zero (i.e. an event)
	_eeStorePC(s_nEndBlock); // otherwise end of loop
	recEmitBlockLink(s_nEndBlock);

	g_branch = 1;
	pc = s_nEndBlock;

	return true;
}

//////////////////////////////////////////////////////////////////////////
// Block compilation
//////////////////////////////////////////////////////////////////////////

static void recRecompile(const u32 startpc)
{
	u32 i = 0;
	u32 willbranch3 = 0;

	pxAssert(startpc);

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= recPtrEnd)
		eeRecNeedsReset = true;

	if (HWADDR(startpc) == VMManager::Internal::GetCurrentELFEntryPoint())
		VMManager::Internal::EntryPointCompilingOnCPUThread();

	if (eeRecNeedsReset)
	{
		eeRecNeedsReset = false;
		recResetRaw();
	}

	// Other recompilers share the thread-local assembler state, so always set our pointer.
	armSetAsmPtr(recPtr, SysMemory::GetEERecEnd() - recPtr, nullptr);
	recPtr = armStartBlock();

	s_pCurBlock = PC_GETBLOCK(startpc);

	pxAssert(s_pCurBlock->GetFnptr() == (uptr)JITCompile);

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
	pxAssert(!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc));

	s_pCurBlockEx = recBlocks.New(HWADDR(startpc), (uptr)recPtr);

	pxAssert(s_pCurBlockEx);

	if (HWADDR(startpc) == EELOAD_START)
	{
		// The EELOAD _start function is the same across all BIOS versions
		const u32 mainjump = memRead32(EELOAD_START + 0x9c);
		if (mainjump >> 26 == 3) // JAL
			g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | (mainjump << 2 & 0x0fffffffU);
	}

	if (g_eeloadMain && HWADDR(startpc) == HWADDR(g_eeloadMain))
	{
		armEmitCall(reinterpret_cast<const void*>(eeloadHook));
		if (VMManager::Internal::IsFastBootInProgress())
		{
			// There are four known versions of EELOAD, identifiable by the location of the 'jal' to the EELOAD function which
			// calls ExecPS2(). The function itself is at the same address in all BIOSs after v1.00-v1.10.
			const u32 typeAexecjump = memRead32(EELOAD_START + 0x470); // v1.00, v1.01?, v1.10?
			const u32 typeBexecjump = memRead32(EELOAD_START + 0x5B0); // v1.20, v1.50, v1.60 (3000x models)
			const u32 typeCexecjump = memRead32(EELOAD_START + 0x618); // v1.60 (3900x models)
			const u32 typeDexecjump = memRead32(EELOAD_START + 0x600); // v1.70, v1.90, v2.00, v2.20, v2.30
			if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3)) // JAL to 0x822B8
				g_eeloadExec = EELOAD_START + 0x2B8;
			else if (typeAexecjump >> 26 == 3) // JAL to 0x82170
				g_eeloadExec = EELOAD_START + 0x170;
			else // There might be other types of EELOAD, because these models' BIOSs have not been examined: 18000, 3500x, 3700x, 5500x, and 7900x. However, all BIOS versions have been examined except for v1.01 and v1.10.
				Console.WriteLn("recRecompile: Could not enable launch arguments for fast boot mode; unidentified BIOS version! Please report this to the PCSX2 developers.");
		}
	}

	if (g_eeloadExec && HWADDR(startpc) == HWADDR(g_eeloadExec))
		armEmitCall(reinterpret_cast<const void*>(eeloadHook2));

	g_branch = 0;

	// reset recomp state variables
	s_nBlockCycles = 0;
	pc = startpc;
	g_cpuConstRegs[0] = 0;
	g_cpuHasConstReg = g_cpuFlushedConstReg = 1;
	g_cpuFlushedPC = false;
	g_recompilingDelaySlot = false;

	if (EmuConfig.Gamefixes.GoemonTlbHack)
	{
		if (pc == 0x33ad48 || pc == 0x35060c)
		{
			// 0x33ad48 and 0x35060c are the return address of the function (0x356250) that populate the TLB cache
			armEmitCall(reinterpret_cast<const void*>(GoemonPreloadTlb));
		}
		else if (pc == 0x3563b8)
		{
			// Game will unmap some virtual addresses. If a constant address were hardcoded in the block, we would be in a bad situation.
			eeRecNeedsReset = true;
			// 0x3563b8 is the start address of the function that invalidate entry in TLB cache
			armAsm->Ldr(a64::w0, GPR_MEM(4)); // a0
			armEmitCall(reinterpret_cast<const void*>(GoemonUnloadTlb));
		}
	}

	// go until the next branch
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

	// Timeout loop speedhack.
	// God of War 2 and other games (e.g. NFS series) have these timeout loops which just spin for a few thousand
	// iterations, usually after kicking something which results in an IRQ, but instead of cancelling the loop,
	// they just let it finish anyway. Such loops look like:
	//
	//   00186D6C addiu  v0,v0, -0x1
	//   00186D70 nop
	//   00186D74 nop
	//   00186D78 nop
	//   00186D7C nop
	//   00186D80 bne    v0, zero, ->$0x00186D6C
	//   00186D84 nop
	//
	// Skipping them entirely seems to have no negative effects, but we skip cycles based on the incoming value
	// if the register being decremented, which appears to vary. So far I haven't seen any which increment instead
	// of decrementing, so we'll limit the test to that to be safe.
	//
	s32 timeout_reg = -1;
	bool is_timeout_loop = true;

	// compile breakpoints as individual blocks
	const int n1 = isBreakpointNeeded(i);
	const int n2 = isMemcheckNeeded(i);
	const int n = std::max<int>(n1, n2);
	if (n != 0)
	{
		s_nEndBlock = i + n * 4;
		goto StartRecomp;
	}

	while (1)
	{
		BASEBLOCK* pblock = PC_GETBLOCK(i);

		// stop before breakpoints
		if (isBreakpointNeeded(i) != 0 || isMemcheckNeeded(i) != 0)
		{
			s_nEndBlock = i;
			break;
		}

		if (i != startpc) // Block size truncation checks.
		{
			if ((i & 0xffc) == 0x0) // breaks blocks at 4k page boundaries
			{
				willbranch3 = 1;
				s_nEndBlock = i;

				eeRecPerfLog.Write("Pagesplit @ %08X : size=%d insts", startpc, (i - startpc) / 4);
				break;
			}

			if (pblock->GetFnptr() != (uptr)JITCompile)
			{
				willbranch3 = 1;
				s_nEndBlock = i;
				break;
			}
		}

		cpuRegs.code = *(int*)PSM(i);

		if (is_timeout_loop)
		{
			if ((cpuRegs.code >> 26) == 8 || (cpuRegs.code >> 26) == 9)
			{
				// addi/addiu
				if (timeout_reg >= 0 || _Rs_ != _Rt_ || _Imm_ >= 0)
					is_timeout_loop = false;
				else
					timeout_reg = _Rs_;
			}
			else if ((cpuRegs.code >> 26) == 5)
			{
				// bne
				if (timeout_reg != static_cast<s32>(_Rs_) || _Rt_ != 0 || memRead32(i + 4) != 0)
					is_timeout_loop = false;
			}
			else if (cpuRegs.code != 0)
			{
				is_timeout_loop = false;
			}
		}

		switch (cpuRegs.code >> 26)
		{
			case 0: // special
				if (_Funct_ == 8 || _Funct_ == 9) // JR, JALR
				{
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				else if (_Funct_ == 12 || _Funct_ == 13) // SYSCALL, BREAK
				{
					s_nEndBlock = i + 4; // No delay slot.
					goto StartRecomp;
				}
				break;

			case 1: // regimm

				if (_Rt_ < 4 || (_Rt_ >= 16 && _Rt_ < 20))
				{
					// branches
					s_branchTo = _Imm_ * 4 + i + 4;
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = s_branchTo;
					else
						s_nEndBlock = i + 8;

					goto StartRecomp;
				}
				break;

			case 2: // J
			case 3: // JAL
				s_branchTo = (_InstrucTarget_ << 2) | ((i + 4) & 0xf0000000);
				s_nEndBlock = i + 8;
				goto StartRecomp;

			// branches
			case 4:
			case 5:
			case 6:
			case 7:
			case 20:
			case 21:
			case 22:
			case 23:
				s_branchTo = _Imm_ * 4 + i + 4;
				if (s_branchTo > startpc && s_branchTo < i)
					s_nEndBlock = s_branchTo;
				else
					s_nEndBlock = i + 8;

				goto StartRecomp;

			case 16: // cp0
				if (_Rs_ == 16)
				{
					if (_Funct_ == 24) // eret
					{
						s_nEndBlock = i + 4;
						goto StartRecomp;
					}
				}
				// Fall through!
				// COP0's branch opcodes line up with COP1 and COP2's
				[[fallthrough]];

			case 17: // cp1
			case 18: // cp2
				if (_Rs_ == 8)
				{
					// BC1F, BC1T, BC1FL, BC1TL
					// BC2F, BC2T, BC2FL, BC2TL
					s_branchTo = _Imm_ * 4 + i + 4;
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = s_branchTo;
					else
						s_nEndBlock = i + 8;

					goto StartRecomp;
				}
				break;
		}

		i += 4;
	}

StartRecomp:

	// The idea here is that as long as a loop doesn't write to a register it's already read
	// (excepting registers initialised with constants or memory loads) or use any instructions
	// which alter the machine state apart from registers, it will do the same thing on every
	// iteration.
	s_nBlockFF = false;
	if (s_branchTo == startpc)
	{
		s_nBlockFF = true;

		u32 reads = 0, loads = 1;

		for (i = startpc; i < s_nEndBlock; i += 4)
		{
			if (i == s_nEndBlock - 8)
				continue;
			cpuRegs.code = *(u32*)PSM(i);
			// nop
			if (cpuRegs.code == 0)
				continue;
			// cache, sync
			else if (_Opcode_ == 057 || (_Opcode_ == 0 && _Funct_ == 017))
				continue;
			// imm arithmetic
			else if ((_Opcode_ & 070) == 010 || (_Opcode_ & 076) == 030)
			{
				if (loads & 1 << _Rs_)
				{
					loads |= 1 << _Rt_;
					continue;
				}
				else
					reads |= 1 << _Rs_;
				if (reads & 1 << _Rt_)
				{
					s_nBlockFF = false;
					break;
				}
			}
			// common register arithmetic instructions
			else if (_Opcode_ == 0 && (_Funct_ & 060) == 040 && (_Funct_ & 076) != 050)
			{
				if (loads & 1 << _Rs_ && loads & 1 << _Rt_)
				{
					loads |= 1 << _Rd_;
					continue;
				}
				else
					reads |= 1 << _Rs_ | 1 << _Rt_;
				if (reads & 1 << _Rd_)
				{
					s_nBlockFF = false;
					break;
				}
			}
			// loads
			else if ((_Opcode_ & 070) == 040 || (_Opcode_ & 076) == 032 || _Opcode_ == 067)
			{
				if (loads & 1 << _Rs_)
				{
					loads |= 1 << _Rt_;
					continue;
				}
				else
					reads |= 1 << _Rs_;
				if (reads & 1 << _Rt_)
				{
					s_nBlockFF = false;
					break;
				}
			}
			// mfc*, cfc*
			else if ((_Opcode_ & 074) == 020 && _Rs_ < 4)
			{
				loads |= 1 << _Rt_;
			}
			else
			{
				s_nBlockFF = false;
				break;
			}
		}
	}
	else
	{
		is_timeout_loop = false;
	}

	// Detect and handle self-modified code
	memory_protect_recompiled_code(startpc, (s_nEndBlock - startpc) >> 2);

	// Skip Recompilation if sceMpegIsEnd Pattern detected
	const bool doRecompilation = !skipMPEG_By_Pattern(startpc) && !recSkipTimeoutLoop(timeout_reg, is_timeout_loop);

	if (doRecompilation)
	{
		while (!g_branch && pc < s_nEndBlock)
			recompileNextInstruction(false); // For the love of recursion, batman!
	}

	pxAssert((pc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (pc - startpc) >> 2;

	if (HWADDR(pc) <= Ps2MemSize::ExposedRam)
	{
		BASEBLOCKEX* oldBlock;
		int i;

		i = recBlocks.LastIndex(HWADDR(pc) - 4);
		while ((oldBlock = recBlocks[i--]))
		{
			if (oldBlock == s_pCurBlockEx)
				continue;
			if (oldBlock->startpc >= HWADDR(pc))
				continue;
			if ((oldBlock->startpc + oldBlock->size * 4) <= HWADDR(startpc))
				break;

			if (std::memcmp(&recRAMCopy[oldBlock->startpc], PSM(oldBlock->startpc),
					oldBlock->size * 4))
			{
				recClear(startpc, (pc - startpc) / 4);
				s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
				pxAssert(s_pCurBlockEx->startpc == HWADDR(startpc));
				break;
			}
		}

		std::memcpy(&recRAMCopy[HWADDR(startpc)], PSM(startpc), pc - startpc);
	}

	s_pCurBlock->SetFnptr((uptr)recPtr);

	if (!(pc & 0x10000000))
		maxrecmem = std::max((pc & ~0xa0000000), maxrecmem);

	if (g_branch == 2)
	{
		// Branch type 2 - This is how I "think" this works (air):
		// Performs a branch/event test but does not actually "break" the block.
		// This allows exceptions to be raised, and is thus sufficient for
		// certain types of things like SYSCALL, EI, etc.  but it is not sufficient
		// for actual branching instructions.
		_flushConstRegs();
		if (!g_cpuFlushedPC)
			_eeStorePC(pc);
		iBranchTest();
	}
	else
	{
		if (g_branch)
			pxAssert(!willbranch3);

		if (willbranch3 || !g_branch)
		{
			_flushConstRegs();

			// Split Block concatenation mode.
			// This code is run when blocks are split either to keep block sizes manageable
			// or because we're crossing a 4k page protection boundary in ps2 mem.  The latter
			// case can result in very short blocks which should not issue branch tests for
			// performance reasons.

			const int numinsts = (pc - startpc) / 4;
			if (numinsts > 6)
			{
				SetBranchImm(pc);
			}
			else
			{
				_eeStorePC(pc);
				armAsm->Ldr(RXTEMP1, CPUREG_MEM(cycle));
				armAsm->Add(RXTEMP1, RXTEMP1, scaleblockcycles());
				armAsm->Str(RXTEMP1, CPUREG_MEM(cycle));
				recEmitBlockLink(pc);
			}
		}
	}

	u8* block_end = armEndBlock();
	pxAssert(block_end < SysMemory::GetEERecEnd());
	s_pCurBlockEx->x86size = static_cast<u32>(block_end - recPtr);

	Perf::ee.RegisterPC((void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = block_end;

	pxAssert((g_cpuHasConstReg & g_cpuFlushedConstReg) == g_cpuHasConstReg);

	s_pCurBlock = nullptr;
	s_pCurBlockEx = nullptr;
}

R5900cpu recCpu = {
	recReserve,
	recShutdown,

	recResetEE,
	recStep,
	recExecute,

	recSafeExitExecution,
	recCancelInstruction,
	recClear,
};
