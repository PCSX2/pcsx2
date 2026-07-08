// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
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
#include "IopDma.h"
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

// Emit psxRegs.code = op; then call the IOP interpreter handler for `op` (psxBSC, the
// R3000A primary-opcode dispatch table). Used for delay-slot ops we have no native
// generator for. The handler reads its operands from psxRegs.code and does NOT advance
// pc or charge a cycle (execI does that in the interpreter), so it is safe in a delay
// slot. RESTATEPTR(x19) is callee-saved across the call. Must not be used for a branch
// op (psxBSC for a branch would call doBranch → nested delay slot + pc write); the block
// compiler bails such cases to the interpreter.
static void recEmitInterpInline(u32 op)
{
	armAsm->Mov(RWARG1, op);
	armAsm->Str(RWARG1, a64::MemOperand(RESTATEPTR, IOP_CODE_OFFSET));
	armEmitCall(reinterpret_cast<const void*>(psxBSC[op >> 26]));
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

// --- Load / store ---------------------------------------------------------------------
// IOP memory access goes through the iopMemRead/Write8/16/32 C++ helpers (no vtlb fastmem
// on the IOP). Each generator computes the effective address GPR[rs] + (s16)imm into
// RWARG1 and calls the helper; loads land the result in RWRET (w0). The helper call
// clobbers all caller-saved regs (incl. x16/x17 scratch), but RESTATEPTR=x19 is callee-
// saved and survives, and each generator reads its inputs from psxRegs fresh, so no
// cross-call state is held in scratch.
//
// The IOP ignores load-delay-slots (as does the x86 IOP rec), so writing GPR[rt] right
// after the load — and compiling the following op natively — is correct.

// Effective address GPR[rs] + (s16)imm into dst.W (mirrors armEmitEffectiveAddr).
static void iopEmitEffectiveAddr(const a64::Register& dst, u32 rs, s32 imm)
{
	if (rs == 0)
	{
		armAsm->Mov(dst.W(), imm);
		return;
	}
	armAsm->Ldr(dst.W(), iopGpr(rs));
	if (imm != 0)
		armAsm->Add(dst.W(), dst.W(), imm); // MacroAssembler materializes any s16 imm
}

// LB/LBU/LH/LHU/LW. The read is performed even when rt==0 (the access can have I/O side
// effects); only the GPR write is suppressed, matching psxLB..psxLW.
static void iopEmitLoad(u32 bits, bool sign, u32 rt, u32 rs, s32 imm)
{
	iopEmitEffectiveAddr(RWARG1, rs, imm);

	const void* fn = (bits == 8)  ? reinterpret_cast<const void*>(&iopMemRead8) :
	                 (bits == 16) ? reinterpret_cast<const void*>(&iopMemRead16) :
	                                reinterpret_cast<const void*>(&iopMemRead32);
	armEmitCall(fn);

	if (rt == 0)
		return;

	switch (bits)
	{
		case 8:  sign ? armAsm->Sxtb(RWRET, RWRET) : armAsm->Uxtb(RWRET, RWRET); break;
		case 16: sign ? armAsm->Sxth(RWRET, RWRET) : armAsm->Uxth(RWRET, RWRET); break;
		case 32: break; // full 32-bit result already in RWRET
	}
	armAsm->Str(RWRET, iopGpr(rt));
}

// SB/SH/SW. GPR[0] reads as zero straight from psxRegs, so rt==0 needs no special case.
static void iopEmitStore(u32 bits, u32 rt, u32 rs, s32 imm)
{
	armAsm->Ldr(RWARG2, iopGpr(rt)); // value to store (low `bits` bits used by the helper)
	iopEmitEffectiveAddr(RWARG1, rs, imm);

	const void* fn = (bits == 8)  ? reinterpret_cast<const void*>(&iopMemWrite8) :
	                 (bits == 16) ? reinterpret_cast<const void*>(&iopMemWrite16) :
	                                reinterpret_cast<const void*>(&iopMemWrite32);
	armEmitCall(fn);
}

// --- Unaligned load / store (LWL/LWR/SWL/SWR) -----------------------------------------
// Read-modify-(write) with a runtime byte shift = (EA & 3) << 3 and runtime masks, mirroring
// psxLWL/psxLWR/psxSWL/psxSWR exactly. The aligned word at (EA & ~3) is read with
// iopMemRead32; stores then merge GPR[rt] with it and iopMemWrite32 it back.
//
// Only `mem` (the iopMemRead32 result, in RWRET) has to survive forward across the merge;
// the address and shift are recomputed from psxRegs after the call (GPR[rs] is unchanged by
// the access, so the recompute is identical). Working regs after the call: RWRET(w0),
// RWARG2..4 (w1-w3) and RSCRATCHW(x17) — all caller-saved/scratch, no live x16/x19 state.
// Between the SWL/SWR read and write calls no other call intervenes, so those regs are free.

// LWL (0x22): Rt = (Rt & (0x00ffffff >> shift)) | (mem << (24 - shift)).
static void iopEmitLWL(u32 rt, u32 rs, s32 imm)
{
	iopEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~3);
	armEmitCall(reinterpret_cast<const void*>(&iopMemRead32)); // RWRET = mem
	if (rt == 0)
		return;

	iopEmitEffectiveAddr(RWARG2, rs, imm);
	armAsm->And(RWARG2, RWARG2, 3);
	armAsm->Lsl(RWARG2, RWARG2, 3); // RWARG2 = shift

	armAsm->Mov(RWARG3, 24);
	armAsm->Sub(RWARG3, RWARG3, RWARG2);   // 24 - shift
	armAsm->Lsl(RWRET, RWRET, RWARG3);     // mem << (24 - shift)

	armAsm->Ldr(RWARG4, iopGpr(rt));
	armAsm->Mov(RSCRATCHW, 0x00ffffffu);
	armAsm->Lsr(RSCRATCHW, RSCRATCHW, RWARG2); // 0x00ffffff >> shift
	armAsm->And(RWARG4, RWARG4, RSCRATCHW);
	armAsm->Orr(RWARG4, RWARG4, RWRET);
	armAsm->Str(RWARG4, iopGpr(rt));
}

// LWR (0x26): Rt = (Rt & (0xffffff00 << (24 - shift))) | (mem >> shift).
static void iopEmitLWR(u32 rt, u32 rs, s32 imm)
{
	iopEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~3);
	armEmitCall(reinterpret_cast<const void*>(&iopMemRead32)); // RWRET = mem
	if (rt == 0)
		return;

	iopEmitEffectiveAddr(RWARG2, rs, imm);
	armAsm->And(RWARG2, RWARG2, 3);
	armAsm->Lsl(RWARG2, RWARG2, 3); // RWARG2 = shift

	armAsm->Lsr(RWRET, RWRET, RWARG2);     // mem >> shift

	armAsm->Mov(RWARG3, 24);
	armAsm->Sub(RWARG3, RWARG3, RWARG2);   // 24 - shift
	armAsm->Mov(RSCRATCHW, 0xffffff00u);
	armAsm->Lsl(RSCRATCHW, RSCRATCHW, RWARG3); // 0xffffff00 << (24 - shift)

	armAsm->Ldr(RWARG4, iopGpr(rt));
	armAsm->And(RWARG4, RWARG4, RSCRATCHW);
	armAsm->Orr(RWARG4, RWARG4, RWRET);
	armAsm->Str(RWARG4, iopGpr(rt));
}

// SWL (0x2A): mem[EA&~3] = (Rt >> (24 - shift)) | (mem & (0xffffff00 << shift)).
static void iopEmitSWL(u32 rt, u32 rs, s32 imm)
{
	iopEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~3);
	armEmitCall(reinterpret_cast<const void*>(&iopMemRead32)); // RWRET = mem

	iopEmitEffectiveAddr(RWARG2, rs, imm);
	armAsm->And(RWARG2, RWARG2, 3);
	armAsm->Lsl(RWARG2, RWARG2, 3); // RWARG2 = shift

	armAsm->Mov(RWARG3, 0xffffff00u);
	armAsm->Lsl(RWARG3, RWARG3, RWARG2);   // 0xffffff00 << shift
	armAsm->And(RWRET, RWRET, RWARG3);     // mem & mask

	armAsm->Mov(RWARG4, 24);
	armAsm->Sub(RWARG4, RWARG4, RWARG2);   // 24 - shift
	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Lsr(RSCRATCHW, RSCRATCHW, RWARG4); // Rt >> (24 - shift)

	armAsm->Orr(RWARG2, RSCRATCHW, RWRET); // value -> RWARG2

	iopEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~3);
	armEmitCall(reinterpret_cast<const void*>(&iopMemWrite32));
}

// SWR (0x2E): mem[EA&~3] = (Rt << shift) | (mem & (0x00ffffff >> (24 - shift))).
static void iopEmitSWR(u32 rt, u32 rs, s32 imm)
{
	iopEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~3);
	armEmitCall(reinterpret_cast<const void*>(&iopMemRead32)); // RWRET = mem

	iopEmitEffectiveAddr(RWARG2, rs, imm);
	armAsm->And(RWARG2, RWARG2, 3);
	armAsm->Lsl(RWARG2, RWARG2, 3); // RWARG2 = shift

	armAsm->Mov(RWARG4, 24);
	armAsm->Sub(RWARG4, RWARG4, RWARG2);   // 24 - shift
	armAsm->Mov(RWARG3, 0x00ffffffu);
	armAsm->Lsr(RWARG3, RWARG3, RWARG4);   // 0x00ffffff >> (24 - shift)
	armAsm->And(RWRET, RWRET, RWARG3);     // mem & mask

	armAsm->Ldr(RSCRATCHW, iopGpr(rt));
	armAsm->Lsl(RSCRATCHW, RSCRATCHW, RWARG2); // Rt << shift

	armAsm->Orr(RWARG2, RSCRATCHW, RWRET); // value -> RWARG2

	iopEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~3);
	armEmitCall(reinterpret_cast<const void*>(&iopMemWrite32));
}

// --------------------------------------------------------------------------------------
//  Branch / jump generators (Phase 6.3)
// --------------------------------------------------------------------------------------
// 32-bit counterparts of the EE branch generators (aR5900Branch.cpp). Each emits only the
// control-flow effect — the psxRegs.pc write and, for the linking forms, the GPR[31]/GPR[rd]
// return-address write. They do NOT compile the delay slot or end the block; the block
// compiler compiles the delay slot after and RETs to the dispatcher loop, which re-reads
// psxRegs.pc. Writing pc before the delay slot is safe (no IOP delay-slot op writes pc) and
// required for JR/JALR (the target is GPR[rs] as it was *before* the delay slot).
//
// Target/fallthrough/link constants mirror the interpreter macros with _PC_ == branchpc+4:
//   J/JAL  target = (instr_index << 2) | ((branchpc+4) & 0xF0000000)
//   branch target = (branchpc+4) + (s16(imm) << 2)
//   fallthrough / link = branchpc + 8
// The R3000A doBranch a0-override (target 0xbfc4a000) and ClearIrxModules (target 0x890) are
// interpreter-only quirks the x86 IOP rec also skips (psxSetBranchReg/Imm don't replicate
// them), so the native generators omit them too. The psxJ IRX-import magic IS handled — by
// bailing J-with-magic-delay-slot to the interpreter in the block compiler.

static void iopWritePcReg(const a64::Register& src_w)
{
	armAsm->Str(src_w, a64::MemOperand(RESTATEPTR, IOP_PC_OFFSET));
}

static void iopWritePcImm(u32 pc)
{
	armAsm->Mov(RSCRATCHW, pc);
	iopWritePcReg(RSCRATCHW);
}

// GPR[reg] = linkpc (32-bit).
static void iopWriteLink(u32 reg, u32 linkpc)
{
	armAsm->Mov(RSCRATCHW, linkpc);
	armAsm->Str(RSCRATCHW, iopGpr(reg));
}

// psxRegs.pc = cond ? target : fallthrough, given a preceding Cmp set the flags. Both
// constants are Mov'd straight into their dest regs (x17 = fallthrough, x16 = target), so
// neither Mov needs a VIXL temp and the Cmp flags survive into the Csel.
static void iopSelectPc(u32 target, u32 fallthrough, a64::Condition cond)
{
	armAsm->Mov(RSCRATCHW, fallthrough);
	armAsm->Mov(RSCRATCH2W, target);
	armAsm->Csel(RSCRATCHW, RSCRATCH2W, RSCRATCHW, cond);
	iopWritePcReg(RSCRATCHW);
}

// Compare signed 32-bit GPR[rs] against zero and select pc.
static void iopBranchZero(u32 rs, u32 target, u32 fallthrough, a64::Condition cond)
{
	armAsm->Ldr(RSCRATCHW, iopGpr(rs));
	armAsm->Cmp(RSCRATCHW, 0);
	iopSelectPc(target, fallthrough, cond);
}

// Emit the control-flow effect of the branch/jump at branchpc. Returns true if handled
// (always, for the ops recIsHandledBranch accepts).
static bool recEmitIopBranch(u32 op, u32 branchpc)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 funct = op & 0x3f;

	const u32 delaypc = branchpc + 4;
	const u32 jtarget = ((op & 0x03ffffff) << 2) | (delaypc & 0xf0000000u);
	const u32 btarget = delaypc + (static_cast<u32>(static_cast<s32>(static_cast<s16>(op))) << 2);
	const u32 fallthrough = branchpc + 8;
	const u32 linkpc = branchpc + 8;

	switch (opcode)
	{
		case 0x02: iopWritePcImm(jtarget); return true;                       // J
		case 0x03: iopWriteLink(31, linkpc); iopWritePcImm(jtarget); return true; // JAL

		case 0x04: // BEQ
			armAsm->Ldr(RSCRATCHW, iopGpr(rs));
			armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
			armAsm->Cmp(RSCRATCHW, RSCRATCH2W);
			iopSelectPc(btarget, fallthrough, a64::eq);
			return true;
		case 0x05: // BNE
			armAsm->Ldr(RSCRATCHW, iopGpr(rs));
			armAsm->Ldr(RSCRATCH2W, iopGpr(rt));
			armAsm->Cmp(RSCRATCHW, RSCRATCH2W);
			iopSelectPc(btarget, fallthrough, a64::ne);
			return true;
		case 0x06: iopBranchZero(rs, btarget, fallthrough, a64::le); return true; // BLEZ (Rs <= 0)
		case 0x07: iopBranchZero(rs, btarget, fallthrough, a64::gt); return true; // BGTZ (Rs >  0)

		case 0x00: // SPECIAL: JR / JALR (target = GPR[rs] read before the delay slot)
			if (funct == 0x08) // JR
			{
				armAsm->Ldr(RSCRATCHW, iopGpr(rs));
				iopWritePcReg(RSCRATCHW);
				return true;
			}
			if (funct == 0x09) // JALR
			{
				armAsm->Ldr(RSCRATCHW, iopGpr(rs));
				iopWritePcReg(RSCRATCHW);
				if (rd != 0)
					iopWriteLink(rd, linkpc);
				return true;
			}
			return false;

		case 0x01: // REGIMM: BLTZ / BGEZ / BLTZAL / BGEZAL (rt selector)
			switch (rt)
			{
				case 0x00: iopBranchZero(rs, btarget, fallthrough, a64::lt); return true; // BLTZ
				case 0x01: iopBranchZero(rs, btarget, fallthrough, a64::ge); return true; // BGEZ
				case 0x10: iopWriteLink(31, linkpc); iopBranchZero(rs, btarget, fallthrough, a64::lt); return true; // BLTZAL
				case 0x11: iopWriteLink(31, linkpc); iopBranchZero(rs, btarget, fallthrough, a64::ge); return true; // BGEZAL
				default: return false;
			}

		default: return false;
	}
}

// Is `op` a control-flow op recEmitIopBranch can emit?
static bool recIsHandledBranch(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 funct = op & 0x3f;
	const u32 rt = (op >> 16) & 0x1f;
	switch (opcode)
	{
		case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
			return true;
		case 0x00:
			return funct == 0x08 || funct == 0x09;
		case 0x01:
			return rt == 0x00 || rt == 0x01 || rt == 0x10 || rt == 0x11;
		default:
			return false;
	}
}

// Decode a single instruction into the open block. Returns true if a native generator
// handled it, false to fall back to single-stepping it through the interpreter. Control-
// flow ops are handled separately (recEmitIopBranch); coprocessor ops return false for now
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

		// Loads (0x20..0x26) / stores (0x28..0x2E), aligned + unaligned.
		case 0x20: iopEmitLoad(8, true, rt, rs, imm); return true;   // LB
		case 0x21: iopEmitLoad(16, true, rt, rs, imm); return true;  // LH
		case 0x22: iopEmitLWL(rt, rs, imm); return true;             // LWL
		case 0x23: iopEmitLoad(32, false, rt, rs, imm); return true; // LW
		case 0x24: iopEmitLoad(8, false, rt, rs, imm); return true;  // LBU
		case 0x25: iopEmitLoad(16, false, rt, rs, imm); return true; // LHU
		case 0x26: iopEmitLWR(rt, rs, imm); return true;             // LWR
		case 0x28: iopEmitStore(8, rt, rs, imm); return true;   // SB
		case 0x29: iopEmitStore(16, rt, rs, imm); return true;  // SH
		case 0x2A: iopEmitSWL(rt, rs, imm); return true;        // SWL
		case 0x2B: iopEmitStore(32, rt, rs, imm); return true;  // SW
		case 0x2E: iopEmitSWR(rt, rs, imm); return true;        // SWR

		// Coprocessor ops — all straight-line (the IOP interpreter has no COP0/COP2
		// branches; those table slots are psxNULL). Inline the interpreter handler and keep
		// the block intact instead of breaking + single-stepping, mirroring the EE Phase
		// 5.1/5.3 trick and the x86 IOP rec (REC_GTE_FUNC / rpsxCP0 — flush + call handler,
		// no block break). None write psxRegs.pc.
		case 0x10: // COP0: MFC0/CFC0/MTC0/CTC0/RFE (psxCOP0 dispatches on rs)
			recEmitInterpInline(op);
			if (rs == 0x10) // RFE: raise any pending IOP interrupts (matches x86 rpsxRFE)
				armEmitCall(reinterpret_cast<const void*>(&iopTestIntc));
			return true;
		case 0x12: // COP2: GTE ops + MFC2/CFC2/MTC2/CTC2 (psxCOP2 dispatches on funct)
			recEmitInterpInline(op);
			return true;
		case 0x32: // LWC2 — GTE quad load
		case 0x3A: // SWC2 — GTE quad store
			recEmitInterpInline(op);
			return true;

		default: return false;
	}
}

// Compile one straight-line or delay-slot instruction: native generator if we have one,
// otherwise an inline interpreter call. Must not be a branch (the caller guarantees this
// for delay slots).
static void recEmitOp(u32 op)
{
	if (!recTranslateOp(op))
		recEmitInterpInline(op);
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
			const u32 delay_op = iopMemRead32(pc + 4);

			// Bail to the interpreter when we can't safely compile the branch+delay pair:
			//  - J (0x02) whose delay slot is the IRX-import magic `addiu $0,$0,index`
			//    (delay_op >> 16 == 0x2400) — psxJ runs irxImportExec, which the native
			//    generator does not replicate.
			//  - a delay slot that is itself a branch/jump — inline-interping it would nest
			//    doBranch (a second delay slot + pc write). Illegal MIPS, but guard anyway.
			const bool magic_j = (op >> 26) == 0x02 && (delay_op >> 16) == 0x2400;
			if (magic_j || recIsHandledBranch(delay_op))
			{
				if (compiled == 0)
				{
					armEmitCall(reinterpret_cast<const void*>(iopExecuteOneInst));
					interp_step = true;
				}
				else
				{
					recEmitWritePc(pc); // next dispatch single-steps the branch
				}
				break;
			}

			// Emit the branch effect (writes psxRegs.pc + any link), then the delay slot.
			// The block ends here; the dispatcher re-reads psxRegs.pc for the next block.
			recEmitIopBranch(op, pc);
			recEmitOp(delay_op);
			block_cycles += 2; // branch + delay slot, 1 cycle each (R3000A is 1 cycle/op)
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
