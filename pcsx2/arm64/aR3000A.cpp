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

// --------------------------------------------------------------------------------------
//  Native integer generators (Phase 6.3)
// --------------------------------------------------------------------------------------
// 32-bit MIPS-I integer ops — a strict subset of the EE arith generators
// (pcsx2/arm64/aR5900Arith.cpp + aR5900MultDiv.cpp), but with 32-bit GPRs (no
// sign-extend-to-64) and 32-bit HI/LO. Semantics mirror R3000AOpcodeTables.cpp exactly.
//
// No register allocator yet: each source GPR is read from psxRegs (via RESTATEPTR =
// &psxRegs), the result computed in a scratch W-reg, and stored straight back. GPR[0]
// ($zero) writes are discarded, like the interpreter.
//
// Scratch discipline (see aR5900MultDiv.cpp): x17 (RSCRATCHADDR) is the only register
// removed from VIXL's scratch list by armStartBlock, so it is the safe manual scratch;
// x16 (RXVIXLSCRATCH) doubles as VIXL's macro temp, usable as a plain operand reg only
// when no live value must survive a macro that materialises an immediate.
static const a64::Register RSCRATCH = RSCRATCHADDR;
static const a64::Register RSCRATCHW = RSCRATCHADDR.W();
static const a64::Register RSCRATCH2W = RXVIXLSCRATCH.W();

static __fi a64::MemOperand iopGpr(u32 n) { return a64::MemOperand(RESTATEPTR, IOP_GPR_OFFSET(n)); }
static __fi a64::MemOperand iopHi() { return a64::MemOperand(RESTATEPTR, IOP_HI_OFFSET); }
static __fi a64::MemOperand iopLo() { return a64::MemOperand(RESTATEPTR, IOP_LO_OFFSET); }

// --- I-type immediate ops -------------------------------------------------------------

// ADDI/ADDIU (0x08/0x09): Rt = Rs + (s16)imm. The interpreter never traps overflow, so
// these are identical in the JIT.
static void iopADDI(u32 rt, u32 rs, s32 imm)
{
	if (rt == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	if (imm != 0)
		armAsm->Add(RSCRATCHW, RSCRATCHW, imm);
	armAsm->Str(RSCRATCHW, iopGpr(rt));
}

// SLTI (0x0A): Rt = (s32)Rs < (s32)imm
static void iopSLTI(u32 rt, u32 rs, s32 imm)
{
	if (rt == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Cmp(RSCRATCHW, imm);
	armAsm->Cset(RSCRATCHW, a64::lt);
	armAsm->Str(RSCRATCHW, iopGpr(rt));
}

// SLTIU (0x0B): Rt = (u32)Rs < (u32)(s32)imm. The signed-immediate cmp gives the right
// unsigned result because cmp/cmn set carry identically for the wrapped operand.
static void iopSLTIU(u32 rt, u32 rs, s32 imm)
{
	if (rt == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Cmp(RSCRATCHW, imm);
	armAsm->Cset(RSCRATCHW, a64::lo);
	armAsm->Str(RSCRATCHW, iopGpr(rt));
}

// ANDI/ORI/XORI (0x0C/0x0D/0x0E): Rt = Rs op (u16)imm (zero-extended). The 16-bit value
// is not always a valid ARM64 logical immediate, so materialize it into x16 first.
static void iopANDI(u32 rt, u32 rs, u32 immu)
{
	if (rt == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	if (immu == 0)
	{
		armAsm->Mov(RSCRATCHW, 0);
	}
	else
	{
		armAsm->Mov(RSCRATCH2W, immu);
		armAsm->And(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	}
	armAsm->Str(RSCRATCHW, iopGpr(rt));
}

static void iopORI(u32 rt, u32 rs, u32 immu)
{
	if (rt == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	if (immu != 0)
	{
		armAsm->Mov(RSCRATCH2W, immu);
		armAsm->Orr(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	}
	armAsm->Str(RSCRATCHW, iopGpr(rt));
}

static void iopXORI(u32 rt, u32 rs, u32 immu)
{
	if (rt == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	if (immu != 0)
	{
		armAsm->Mov(RSCRATCH2W, immu);
		armAsm->Eor(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	}
	armAsm->Str(RSCRATCHW, iopGpr(rt));
}

// LUI (0x0F): Rt = imm << 16
static void iopLUI(u32 rt, u32 imm)
{
	if (rt == 0)
		return;
	armAsm->Mov(RSCRATCHW, imm << 16);
	armAsm->Str(RSCRATCHW, iopGpr(rt));
}

// --- R-type reg-reg ALU ops -----------------------------------------------------------

// ADD/ADDU (0x20/0x21): Rd = Rs + Rt (no overflow trap in the JIT).
static void iopADD(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Add(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SUB/SUBU (0x22/0x23): Rd = Rs - Rt.
static void iopSUB(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Sub(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// AND (0x24).
static void iopAND(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->And(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// OR (0x25).
static void iopOR(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Orr(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// XOR (0x26).
static void iopXOR(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Eor(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// NOR (0x27): Rd = ~(Rs | Rt).
static void iopNOR(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Orr(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Mvn(RSCRATCHW, RSCRATCHW);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SLT (0x2A): Rd = (s32)Rs < (s32)Rt.
static void iopSLT(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Cmp(RSCRATCHW, RSCRATCH2W);
	armAsm->Cset(RSCRATCHW, a64::lt);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SLTU (0x2B): Rd = (u32)Rs < (u32)Rt.
static void iopSLTU(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Cmp(RSCRATCHW, RSCRATCH2W);
	armAsm->Cset(RSCRATCHW, a64::lo);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// --- Shifts ---------------------------------------------------------------------------
// 32-bit: amount masked to 5 bits. ARM64 W-reg variable shifts use the low 5 bits of the
// amount reg natively, matching MIPS.

// SLL (0x00).
static void iopSLL(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Lsl(RSCRATCHW, RSCRATCHW, sa);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SRL (0x02).
static void iopSRL(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Lsr(RSCRATCHW, RSCRATCHW, sa);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SRA (0x03).
static void iopSRA(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Asr(RSCRATCHW, RSCRATCHW, sa);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SLLV (0x04): Rd = Rt << (Rs & 31).
static void iopSLLV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rs));
	armAsm->Lsl(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SRLV (0x06): Rd = Rt >> (Rs & 31) (logical).
static void iopSRLV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rs));
	armAsm->Lsr(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// SRAV (0x07): Rd = Rt >> (Rs & 31) (arithmetic).
static void iopSRAV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rs));
	armAsm->Asr(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

// --- HI/LO moves ----------------------------------------------------------------------

static void iopMFHI(u32 rd)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopHi());
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

static void iopMFLO(u32 rd)
{
	if (rd == 0)
		return;
	armAsm->Ldr(RSCRATCHW, iopLo());
	armAsm->Str(RSCRATCHW, iopGpr(rd));
}

static void iopMTHI(u32 rs)
{
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Str(RSCRATCHW, iopHi());
}

static void iopMTLO(u32 rs)
{
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Str(RSCRATCHW, iopLo());
}

// --- Multiply / divide ----------------------------------------------------------------
// HI:LO = Rs * Rt (32x32->64). Unlike the R5900, the R3000A MULT/MULTU do NOT write Rd.

static void iopMult(bool sign, u32 rs, u32 rt)
{
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	if (sign)
		armAsm->Smull(RSCRATCH, RSCRATCHW, RSCRATCH2W);
	else
		armAsm->Umull(RSCRATCH, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopLo()); // LO = low 32 bits of the product
	armAsm->Lsr(RSCRATCH, RSCRATCH, 32);
	armAsm->Str(RSCRATCHW, iopHi()); // HI = high 32 bits
}

// DIV (0x1A): signed. LO = Rs/Rt, HI = Rs%Rt. ARM SDIV reproduces the x86 INT_MIN/-1
// overflow quirk for free (quotient 0x80000000, remainder 0). Only ÷0 needs a fixup:
//   ÷0: LO = (s32)Rs < 0 ? 1 : 0xFFFFFFFF, HI = Rs (already correct — SDIV ÷0 yields 0,
//       so remainder = Rs - 0 = Rs).
static void iopDIV(u32 rs, u32 rt)
{
	a64::Label done;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));  // dividend
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt)); // divisor
	armAsm->Sdiv(RSCRATCHW, RSCRATCHW, RSCRATCH2W);  // quotient
	armAsm->Mul(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);  // quotient * divisor
	armAsm->Str(RSCRATCHW, iopLo());                 // LO = quotient
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Sub(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);  // remainder
	armAsm->Str(RSCRATCH2W, iopHi());                // HI = remainder

	// ÷0 fixup for LO (HI is already == dividend, which is correct for ÷0).
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Cmp(RSCRATCH2W, 0);
	armAsm->B(a64::ne, &done);
	armAsm->Ldr(RSCRATCHW, iopGpr(rs)); // dividend
	armAsm->Cmp(RSCRATCHW, 0);
	armAsm->Mov(RSCRATCH2W, 1);
	armAsm->Csneg(RSCRATCH2W, RSCRATCH2W, RSCRATCH2W, a64::lt); // (dividend<0) ? 1 : -1
	armAsm->Str(RSCRATCH2W, iopLo());
	armAsm->Bind(&done);
}

// DIVU (0x1B): unsigned. ÷0: LO = 0xFFFFFFFF, HI = Rs.
static void iopDIVU(u32 rs, u32 rt)
{
	a64::Label done;
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Udiv(RSCRATCHW, RSCRATCHW, RSCRATCH2W);  // quotient (÷0 -> 0)
	armAsm->Mul(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCHW, iopLo());
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Sub(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);
	armAsm->Str(RSCRATCH2W, iopHi());

	// ÷0 fixup for LO (HI already == dividend).
	armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
	armAsm->Cmp(RSCRATCH2W, 0);
	armAsm->B(a64::ne, &done);
	armAsm->Mov(RSCRATCHW, 0xFFFFFFFFu);
	armAsm->Str(RSCRATCHW, iopLo());
	armAsm->Bind(&done);
}

// Decode a single instruction into the open block. Returns true if a native generator
// handled it, false to fall back to single-stepping it through the interpreter. Control-
// flow ops (J/JR/branches/SYSCALL), loads/stores and coprocessor ops return false for now
// — they end the native run and are interpreted (the interpreter handles delay slots).
static bool recTranslateOp(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 sa = (op >> 6) & 0x1f;
	const u32 funct = op & 0x3f;
	const s32 imm = static_cast<s16>(op);

	switch (opcode)
	{
		// SPECIAL — R-type ops dispatched on funct.
		case 0x00:
			switch (funct)
			{
				case 0x00: iopSLL(rd, rt, sa); return true;
				case 0x02: iopSRL(rd, rt, sa); return true;
				case 0x03: iopSRA(rd, rt, sa); return true;
				case 0x04: iopSLLV(rd, rt, rs); return true;
				case 0x06: iopSRLV(rd, rt, rs); return true;
				case 0x07: iopSRAV(rd, rt, rs); return true;
				case 0x10: iopMFHI(rd); return true;
				case 0x11: iopMTHI(rs); return true;
				case 0x12: iopMFLO(rd); return true;
				case 0x13: iopMTLO(rs); return true;
				case 0x18: iopMult(true, rs, rt); return true;  // MULT
				case 0x19: iopMult(false, rs, rt); return true; // MULTU
				case 0x1A: iopDIV(rs, rt); return true;
				case 0x1B: iopDIVU(rs, rt); return true;
				case 0x20: iopADD(rd, rs, rt); return true;
				case 0x21: iopADD(rd, rs, rt); return true; // ADDU
				case 0x22: iopSUB(rd, rs, rt); return true;
				case 0x23: iopSUB(rd, rs, rt); return true; // SUBU
				case 0x24: iopAND(rd, rs, rt); return true;
				case 0x25: iopOR(rd, rs, rt); return true;
				case 0x26: iopXOR(rd, rs, rt); return true;
				case 0x27: iopNOR(rd, rs, rt); return true;
				case 0x2A: iopSLT(rd, rs, rt); return true;
				case 0x2B: iopSLTU(rd, rs, rt); return true;
				// JR/JALR (0x08/0x09), SYSCALL/BREAK (0x0C/0x0D), MTHI/etc handled above.
				default: return false;
			}

		// I-type immediate ops.
		case 0x08: iopADDI(rt, rs, imm); return true;
		case 0x09: iopADDI(rt, rs, imm); return true; // ADDIU
		case 0x0A: iopSLTI(rt, rs, imm); return true;
		case 0x0B: iopSLTIU(rt, rs, imm); return true;
		case 0x0C: iopANDI(rt, rs, static_cast<u16>(op)); return true;
		case 0x0D: iopORI(rt, rs, static_cast<u16>(op)); return true;
		case 0x0E: iopXORI(rt, rs, static_cast<u16>(op)); return true;
		case 0x0F: iopLUI(rt, static_cast<u16>(op)); return true;

		default: return false;
	}
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
