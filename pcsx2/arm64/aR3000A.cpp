// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 IOP (R3000A) recompiler — Phase 6.
//
// ARM64 counterpart to pcsx2/x86/iR3000A.cpp. The R3000A is plain 32-bit MIPS-I, so
// this is a strict subset of the EE recompiler (pcsx2/arm64/aR5900.cpp): 32-bit GPRs,
// no 128-bit / FPU state, and memory access via the iopMem* helpers (no vtlb fastmem).
//
// Execution model (bring-up): a C++ dispatcher loop (recExecuteBlock) drives the IOP
// timeslice — it looks up the host block for psxRegs.pc in the recLUT (compiling on
// miss), calls it, then charges the consumed cycles against the EE timeslice and runs
// the IOP event test, exactly mirroring the interpreter's intExecuteBlock. Each block
// has its own prologue/epilogue (saves x19+LR, pins RESTATEPTR=&psxRegs, RETs) and
// ends by writing psxRegs.pc. Any opcode the rec cannot yet compile is single-stepped
// through the interpreter via iopExecuteOneInst (so the rec is correct from day one;
// native opcode generators are added incrementally and only improve speed). This
// mirrors the EE rec's own Phase 4.3 bring-up before recLUT host-code chaining.

#include "arm64/aR3000A.h"

#include "Config.h"
#include "IopBios.h"
#include "IopHw.h"
#include "IopMem.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900OpcodeTables.h"
#include "VMManager.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Pcsx2Defs.h"

#include <vector>

namespace a64 = vixl::aarch64;

// --------------------------------------------------------------------------------------
//  IOP code-cache layout
// --------------------------------------------------------------------------------------
// The IOP rec region is pre-reserved by SysMemory (HostMemoryMap::IOPrec*). As with the
// EE rec we carve a tail for the ArmConstantPool (far-jump trampolines + literals VIXL
// loads PC-relative) and roll a cursor through the rest.
static constexpr u32 IOP_CONSTPOOL_SIZE = static_cast<u32>(_1mb);
static constexpr u32 RECOMPILE_HEADROOM = static_cast<u32>(_1mb);
static constexpr u32 MAX_BLOCK_INSTS = 256;

static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;
static ArmConstantPool s_const_pool;

// --------------------------------------------------------------------------------------
//  recLUT block-lookup table (mirrors aR5900.cpp / x86 iR3000A.cpp)
// --------------------------------------------------------------------------------------
// Two-level guest-PC -> host-block lookup. recLUT[pc>>16] holds a per-64 KB-page base,
// pre-biased so that *(uptr*)(recLUT[pc>>16] + pc*2) is the one host-pointer slot for
// that 4-byte guest word. A slot holds: a compiled block entry, 0 (needs compile), or
// IOP_UNMAPPED (guest page not backed by RAM/ROM — should never be executed).
alignas(16) static uptr recLUT[0x10000];

static std::vector<uptr> recLutReserve;
static std::vector<uptr> recLutUnmapped;
static size_t recLutEntries = 0;
static uptr* recRAM = nullptr;
static uptr* recROM = nullptr;
static uptr* recROM1 = nullptr;
static uptr* recROM2 = nullptr;

static constexpr uptr IOP_UNMAPPED = 1;

static __fi uptr* recPtrToBlock(u32 pc)
{
	return reinterpret_cast<uptr*>(recLUT[pc >> 16] + pc * (sizeof(uptr) / 4));
}

static bool iopRecExecuting = false;
static bool iopRecNeedsReset = false;

static void recResetRaw();
static void recRecompile(u32 startpc);

// Associate one 64 KB guest page with the slot array `mapbase`, biased so recPtrToBlock
// lands at the right element. Direct port of x86 recLUT_SetPage / aR5900.cpp.
static void recLUT_SetPage(uptr* mapbase, uint pagebase, uint pageidx, uint mappage)
{
	const uint page = pagebase + pageidx;
	pxAssert(page < 0x10000);
	recLUT[page] = reinterpret_cast<uptr>(&mapbase[(static_cast<s32>(mappage) - static_cast<s32>(page)) << 14]);
}

static void recReserveLUT()
{
	recLutEntries = (Ps2MemSize::ExposedIopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4;
	recLutReserve.assign(recLutEntries, 0);
	recLutUnmapped.assign(_64kb / 4, IOP_UNMAPPED);

	uptr* basepos = recLutReserve.data();
	recRAM = basepos;
	basepos += (Ps2MemSize::ExposedIopRam / 4);
	recROM = basepos;
	basepos += (Ps2MemSize::Rom / 4);
	recROM1 = basepos;
	basepos += (Ps2MemSize::Rom1 / 4);
	recROM2 = basepos;
	basepos += (Ps2MemSize::Rom2 / 4);

	uptr* const unmapped = recLutUnmapped.data();
	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(unmapped, i, 0, 0);

	// IOP RAM is mirrored at segments 0x0000 / 0x8000 / 0xa000; 0x80 pages (8 MB worth)
	// masked down to the actual RAM size so extra-RAM and 2 MB configs both alias right.
	for (int i = 0; i < 0x80; i++)
	{
		const u32 mask = (Ps2MemSize::ExposedIopRam / _64kb) - 1;
		recLUT_SetPage(recRAM, 0x0000, i, i & mask);
		recLUT_SetPage(recRAM, 0x8000, i, i & mask);
		recLUT_SetPage(recRAM, 0xa000, i, i & mask);
	}

	for (int i = 0x1fc0; i < 0x2000; i++)
	{
		recLUT_SetPage(recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(recROM, 0xa000, i, i - 0x1fc0);
	}

	for (int i = 0x1e00; i < 0x1e40; i++)
	{
		recLUT_SetPage(recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e48; i++)
	{
		recLUT_SetPage(recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(recROM2, 0xa000, i, i - 0x1e40);
	}
}

// Reset every mapped slot to 0 (needs compile). Unmapped slots keep IOP_UNMAPPED.
static void recClearLUT()
{
	for (uptr& slot : recLutReserve)
		slot = 0;
	for (uptr& slot : recLutUnmapped)
		slot = IOP_UNMAPPED;
}

static void recReserve()
{
	recPtr = SysMemory::GetIOPRec();
	recPtrEnd = SysMemory::GetIOPRecEnd() - IOP_CONSTPOOL_SIZE;
	s_const_pool.Init(recPtrEnd, IOP_CONSTPOOL_SIZE);
	recReserveLUT();
}

static void recShutdown()
{
	s_const_pool.Destroy();
	recLutReserve.clear();
	recLutReserve.shrink_to_fit();
	recLutUnmapped.clear();
	recLutUnmapped.shrink_to_fit();
	recRAM = recROM = recROM1 = recROM2 = nullptr;
	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static void recResetRaw()
{
	recPtr = SysMemory::GetIOPRec();
	s_const_pool.Reset();
	recClearLUT();
	iopRecNeedsReset = false;
}

static void recResetIOP()
{
	if (iopRecExecuting)
	{
		// Defer: don't rewind the cache under a running block. The recExecuteBlock loop
		// performs the reset between block calls.
		iopRecNeedsReset = true;
		return;
	}
	recResetRaw();
}

// --------------------------------------------------------------------------------------
//  Emit helpers
// --------------------------------------------------------------------------------------

// Emit psxRegs.code = op; then call the interpreter handler for `op`. (Currently unused
// by the all-interpreter skeleton; kept for the inline-fallback path used once native
// generators land — e.g. straight-line COP2/GTE ops.)
[[maybe_unused]] static void recEmitInterpInline(u32 op)
{
	armAsm->Mov(RWARG1, op);
	armAsm->Str(RWARG1, a64::MemOperand(RESTATEPTR, IOP_CODE_OFFSET));
	armEmitCall(reinterpret_cast<const void*>(R5900::GetInstruction(op).interpret));
}

// psxRegs.pc = imm (block fallthrough / resume target).
static void recEmitWritePc(u32 pc)
{
	armAsm->Mov(RWARG1, pc);
	armAsm->Str(RWARG1, a64::MemOperand(RESTATEPTR, IOP_PC_OFFSET));
}

// Decode a single instruction into the open block. Returns true if a native generator
// handled it. Bring-up: nothing is compiled natively yet, so always false → the block
// compiler single-steps it through the interpreter. Native generators are wired in here
// in later commits (integer → load/store → branches).
static bool recTranslateOp(u32 /*op*/)
{
	return false;
}

// Is `op` a control-flow op we have a native branch generator for? (None yet.)
static bool recIsHandledBranch(u32 /*op*/)
{
	return false;
}

// --------------------------------------------------------------------------------------
//  Block compiler
// --------------------------------------------------------------------------------------
// Compile a straight-line run starting at startpc into one self-contained host block and
// install its entry in the recLUT slot for startpc. The block:
//   - has a prologue (save x19+LR, pin RESTATEPTR=&psxRegs) and epilogue (restore, RET);
//   - emits native ops we can codegen, charging 1 guest cycle each (R3000A is 1 cycle/op);
//   - if the FIRST op is un-compilable, emits a one-shot interpreter single-step block
//     (iopExecuteOneInst handles its own pc/delay/cycle), then RETs;
//   - otherwise ends at the next un-compilable op / a branch / the length cap, writing
//     psxRegs.pc and charging the accumulated block cycles to psxRegs.cycle.
static void recRecompile(u32 startpc)
{
	if (recPtr >= recPtrEnd - RECOMPILE_HEADROOM)
		iopRecNeedsReset = true;
	if (iopRecNeedsReset)
		recResetRaw();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);
	u8* const entry = armStartBlock();

	// Prologue: save callee-saved x19 + LR, pin RESTATEPTR = &psxRegs.
	armAsm->Stp(RESTATEPTR, a64::lr, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armMoveAddressToReg(RESTATEPTR, &psxRegs);

	u32 pc = startpc;
	u32 block_cycles = 0;
	u32 compiled = 0;
	bool interp_step = false;

	for (;;)
	{
		const u32 op = iopMemRead32(pc);

		if (recIsHandledBranch(op))
		{
			// (No native branch generators yet — recIsHandledBranch is always false.)
			break;
		}

		if (recTranslateOp(op))
		{
			block_cycles++;
			pc += 4;
			if (++compiled >= MAX_BLOCK_INSTS)
			{
				recEmitWritePc(pc);
				break;
			}
			continue;
		}

		// Un-compilable op.
		if (compiled == 0)
		{
			// One-shot interpreter single-step block. iopExecuteOneInst advances pc,
			// charges its own cycle(s) and (for branches) runs the delay slot + event
			// test. No block cycles to charge here.
			armEmitCall(reinterpret_cast<const void*>(iopExecuteOneInst));
			interp_step = true;
			break;
		}

		// End the block; the next dispatch single-steps this op.
		recEmitWritePc(pc);
		break;
	}

	// Charge native block cycles to psxRegs.cycle (interpreter blocks charge their own).
	if (!interp_step && block_cycles != 0)
	{
		armAsm->Ldr(RXARG1, a64::MemOperand(RESTATEPTR, IOP_CYCLE_OFFSET));
		armAsm->Add(RXARG1, RXARG1, block_cycles);
		armAsm->Str(RXARG1, a64::MemOperand(RESTATEPTR, IOP_CYCLE_OFFSET));
	}

	// Epilogue: restore x19 + LR, return to the dispatcher loop.
	armAsm->Ldp(RESTATEPTR, a64::lr, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ret();

	recPtr = armEndBlock();

	*recPtrToBlock(startpc) = reinterpret_cast<uptr>(entry);
}

// --------------------------------------------------------------------------------------
//  Timeslice accounting (mirrors R3000AInterpreter.cpp intExecuteBlock)
// --------------------------------------------------------------------------------------
// Convert `cycles` of IOP clock consumed into the EE timeslice debit. Default PS2 mode
// is a flat ×8; PS1 mode (HW_ICFG bit 3) uses the gcd ratio with a fractional carry.
static __fi void iopAddEECycles(u32 cycles)
{
	if (!(psxHu32(HW_ICFG) & (1 << 3))) [[likely]]
	{
		psxRegs.iopCycleEE -= cycles * 8;
		return;
	}

	const u32 cnum = 1280; // PS2CLK / gcd
	const u32 cdenom = 147; // PSXCLK / gcd
	const u32 t = (cnum * cycles) + psxRegs.iopCycleEECarry;
	psxRegs.iopCycleEE -= t / cdenom;
	psxRegs.iopCycleEECarry = t % cdenom;
}

static s32 recExecuteBlock(s32 eeCycles)
{
	psxRegs.iopBreak = 0;
	psxRegs.iopCycleEE = eeCycles;

	iopRecExecuting = true;

	while (psxRegs.iopCycleEE > 0)
	{
		if (iopRecNeedsReset)
			recResetRaw();

		// HLE BIOS entry (only when HW_ICFG bit 3 enables it), matching intExecuteBlock.
		if ((psxHu32(HW_ICFG) & 8) &&
			((psxRegs.pc & 0x1fffffffU) == 0xa0 || (psxRegs.pc & 0x1fffffffU) == 0xb0 ||
				(psxRegs.pc & 0x1fffffffU) == 0xc0))
		{
			psxBiosCall();
		}

		const u64 startCycle = psxRegs.cycle;

		uptr fn = *recPtrToBlock(psxRegs.pc);
		if (fn == IOP_UNMAPPED) [[unlikely]]
		{
			Console.Error("ARM64 IOP rec: execute on unmapped page (PC=0x%08x)", psxRegs.pc);
			break;
		}
		if (fn == 0)
		{
			recRecompile(psxRegs.pc);
			fn = *recPtrToBlock(psxRegs.pc);
		}

		reinterpret_cast<void (*)()>(fn)();

		iopAddEECycles(static_cast<u32>(psxRegs.cycle - startCycle));

		if (static_cast<s64>(psxRegs.cycle - psxRegs.iopNextEventCycle) >= 0)
			iopEventTest();
	}

	iopRecExecuting = false;

	return psxRegs.iopBreak + psxRegs.iopCycleEE;
}

// --------------------------------------------------------------------------------------
//  Invalidation
// --------------------------------------------------------------------------------------
// Targeted: reset only the recLUT slots covering [Addr, Addr + Size*4) back to 0 so the
// next dispatch recompiles them. `Size` is in instructions (4-byte words), matching the
// x86 recClearIOP contract. Orphaned host code is reclaimed at the next full reset.
static void recClearIOP(u32 Addr, u32 Size)
{
	const u32 end = Addr + Size * 4;
	for (u32 pc = Addr & ~3u; pc < end; pc += 4)
	{
		uptr* const slot = recPtrToBlock(pc);
		if (*slot != IOP_UNMAPPED)
			*slot = 0;
	}
}

R3000Acpu psxRec = {
	recReserve,
	recResetIOP,
	recExecuteBlock,
	recClearIOP,
	recShutdown};
