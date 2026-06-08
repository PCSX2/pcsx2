// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — multiply/divide codegen (Phase 3.5).
//
// Generates ARM64 for the R5900 multiply and divide opcodes:
//   MULT/MULTU    — 32×32→64-bit multiply (HI/LO; also Rd=LO when rd!=0)
//   DIV/DIVU      — 32-bit divide (quotient in LO, remainder in HI)
//   MULT1/MULTU1  — second-pipeline multiply (HI1/LO1; MMI group)
//   DIV1/DIVU1    — second-pipeline divide  (HI1/LO1; MMI group)
//
// Semantics are matched 1:1 against the interpreter (R5900OpcodeImpl.cpp /
// MMI.cpp). Note the R5900 has NO DMULT/DMULTU/DDIV/DDIVU — those are not EE
// instructions (they trap as reserved), so they are intentionally absent.
//
// No register allocator yet — every source GPR is read from cpuRegs in memory
// (via RESTATEPTR = &cpuRegs) and the HI/LO results are written straight back.
// Because the source GPRs are never modified, we freely reload them instead of
// keeping more than two values live.
//
// Register discipline (see arm64-port/CONVENTIONS.md + AsmHelpers): only x17
// (RSCRATCHADDR) is removed from VIXL's scratch list in armStartBlock, so it is
// the safe manual scratch. x16 (RXVIXLSCRATCH) doubles as VIXL's macro temp —
// it must never hold a live value across a macro that materialises an immediate.
// This code avoids that entirely: the only immediates used are encodable
// (cmp #0, mov #1, mov #-1), so no temp is ever allocated.

#include "aR5900.h"

#include "R5900.h"

#include <cstddef>

namespace a64 = vixl::aarch64;

// Two scratch registers. RSCRATCH (x17) is the safe manual scratch; RSCRATCH2
// (x16) is used only as a plain operand register for reg-reg ALU ops here.
static const a64::Register RSCRATCH = RSCRATCHADDR;
static const a64::Register RSCRATCHW = RSCRATCHADDR.W();
static const a64::Register RSCRATCH2 = RXVIXLSCRATCH;
static const a64::Register RSCRATCH2W = RXVIXLSCRATCH.W();

// HI/LO live at GPR indices 32/33 (each GPR_reg is 128 bits). The "pipeline 1"
// results (MULT1/DIV1 family) target the upper doubleword, i.e. +8 bytes.
static constexpr u32 EE_HI_OFFSET = 32u * 16u;       // HI.UD[0]  (512)
static constexpr u32 EE_LO_OFFSET = 33u * 16u;       // LO.UD[0]  (528)
static constexpr u32 EE_HI1_OFFSET = EE_HI_OFFSET + 8u; // HI.UD[1] (520)
static constexpr u32 EE_LO1_OFFSET = EE_LO_OFFSET + 8u; // LO.UD[1] (536)

// ------------------------------------------------------------------------
// Shared 32×32→64 multiply.
//   LO = (s32)(product & 0xffffffff)   (sign-extended to 64, even for MULTU)
//   HI = (s32)(product >> 32)          (sign-extended to 64)
//   if rd != 0: GPR[rd].UD[0] = LO     (R5900 3-operand form)
// ------------------------------------------------------------------------
static void emitMult(bool sign, u32 rd, u32 rs, u32 rt, u32 lo_off, u32 hi_off)
{
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));

	// Full 64-bit product in RSCRATCH (signed or unsigned widening multiply).
	if (sign)
		armAsm->Smull(RSCRATCH, RSCRATCHW, RSCRATCH2W);
	else
		armAsm->Umull(RSCRATCH, RSCRATCHW, RSCRATCH2W);

	// LO = sign-extended low 32 bits of the product.
	armAsm->Sxtw(RSCRATCH2, RSCRATCHW);
	armAsm->Str(RSCRATCH2, a64::MemOperand(RESTATEPTR, lo_off));
	if (rd != 0)
		armAsm->Str(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	// HI = sign-extended high 32 bits. asr #32 leaves bits 63:32 in 31:0 and
	// sign-extends from bit 63 (== bit 31 of the high word), matching (s32).
	armAsm->Asr(RSCRATCH, RSCRATCH, 32);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, hi_off));
}

// ------------------------------------------------------------------------
// Shared signed 32-bit divide.
//   LO = rs / rt, HI = rs % rt (both sign-extended to 64).
// ARM SDIV reproduces the EE's overflow quirk for free: 0x80000000 / -1 yields
// 0x80000000, and the remainder works out to 0. Only the divide-by-zero case
// needs a fixup: LO = (rs < 0) ? 1 : -1, HI = rs (HI already equals rs there,
// since SDIV yields 0 so remainder = rs - 0 = rs).
// ------------------------------------------------------------------------
static void emitDivS(u32 rs, u32 rt, u32 lo_off, u32 hi_off)
{
	a64::Label done;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));  // dividend
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt))); // divisor

	armAsm->Sdiv(RSCRATCHW, RSCRATCHW, RSCRATCH2W);  // RSCRATCHW = quotient
	armAsm->Mul(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);  // RSCRATCH2W = quotient * divisor

	// LO = sign-extended quotient (free up RSCRATCH afterwards).
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, lo_off));

	// HI = sign-extended remainder = dividend - quotient*divisor.
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Sub(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);
	armAsm->Sxtw(RSCRATCH2, RSCRATCH2W);
	armAsm->Str(RSCRATCH2, a64::MemOperand(RESTATEPTR, hi_off));

	// Divide-by-zero fixup for LO (HI is already correct: == dividend).
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Cmp(RSCRATCH2W, 0);
	armAsm->B(a64::ne, &done);
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs))); // dividend
	armAsm->Cmp(RSCRATCHW, 0);
	armAsm->Mov(RSCRATCH2W, 1);
	armAsm->Csneg(RSCRATCH2W, RSCRATCH2W, RSCRATCH2W, a64::lt); // (dividend<0) ? 1 : -1
	armAsm->Sxtw(RSCRATCH2, RSCRATCH2W);
	armAsm->Str(RSCRATCH2, a64::MemOperand(RESTATEPTR, lo_off));
	armAsm->Bind(&done);
}

// ------------------------------------------------------------------------
// Shared unsigned 32-bit divide.
//   LO = (s32)(rs / rt), HI = (s32)(rs % rt)  (note: sign-extended to 64).
// Divide-by-zero: LO = -1 (full 64-bit), HI = rs (sign-extended).
// ------------------------------------------------------------------------
static void emitDivU(u32 rs, u32 rt, u32 lo_off, u32 hi_off)
{
	a64::Label done;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));  // dividend
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt))); // divisor

	armAsm->Udiv(RSCRATCHW, RSCRATCHW, RSCRATCH2W);  // RSCRATCHW = quotient (÷0 -> 0)
	armAsm->Mul(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);  // RSCRATCH2W = quotient * divisor

	armAsm->Sxtw(RSCRATCH, RSCRATCHW);               // LO = (s32)quotient
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, lo_off));

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Sub(RSCRATCH2W, RSCRATCHW, RSCRATCH2W);  // remainder
	armAsm->Sxtw(RSCRATCH2, RSCRATCH2W);             // HI = (s32)remainder
	armAsm->Str(RSCRATCH2, a64::MemOperand(RESTATEPTR, hi_off));

	// Divide-by-zero fixup for LO (HI already correct: == sign-extended dividend).
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Cmp(RSCRATCH2W, 0);
	armAsm->B(a64::ne, &done);
	armAsm->Mov(RSCRATCH, 0xFFFFFFFFFFFFFFFFull);    // LO = -1 (encodable: all ones)
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, lo_off));
	armAsm->Bind(&done);
}

// ------------------------------------------------------------------------
// SPECIAL group: MULT/MULTU (funct 0x18/0x19), DIV/DIVU (0x1A/0x1B).
// ------------------------------------------------------------------------
void armEmitMULT(u32 rd, u32 rs, u32 rt) { emitMult(true, rd, rs, rt, EE_LO_OFFSET, EE_HI_OFFSET); }
void armEmitMULTU(u32 rd, u32 rs, u32 rt) { emitMult(false, rd, rs, rt, EE_LO_OFFSET, EE_HI_OFFSET); }
void armEmitDIV(u32 rs, u32 rt) { emitDivS(rs, rt, EE_LO_OFFSET, EE_HI_OFFSET); }
void armEmitDIVU(u32 rs, u32 rt) { emitDivU(rs, rt, EE_LO_OFFSET, EE_HI_OFFSET); }

// ------------------------------------------------------------------------
// MMI group: MULT1/MULTU1 (funct 0x18/0x19), DIV1/DIVU1 (0x1A/0x1B).
// Identical arithmetic, but results target the upper doubleword HI1/LO1, and
// the optional Rd write reads LO.UD[1] (== the value we store to LO1).
// ------------------------------------------------------------------------
void armEmitMULT1(u32 rd, u32 rs, u32 rt) { emitMult(true, rd, rs, rt, EE_LO1_OFFSET, EE_HI1_OFFSET); }
void armEmitMULTU1(u32 rd, u32 rs, u32 rt) { emitMult(false, rd, rs, rt, EE_LO1_OFFSET, EE_HI1_OFFSET); }
void armEmitDIV1(u32 rs, u32 rt) { emitDivS(rs, rt, EE_LO1_OFFSET, EE_HI1_OFFSET); }
void armEmitDIVU1(u32 rs, u32 rt) { emitDivU(rs, rt, EE_LO1_OFFSET, EE_HI1_OFFSET); }
