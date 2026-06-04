// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — FPU (COP1) exact-semantics opcode generators
// (Phase 5.2a).
//
// This file implements the COP1 instructions that are pure bit/integer movement:
// register transfers (MFC1/MTC1/CFC1/CTC1), the bit-twiddling "arithmetic" ops
// (MOV_S/ABS_S/NEG_S), and the FPU load/store ops (LWC1/SWC1). None of these
// involve the EE FPU's non-IEEE float rounding/clamping, so the ARM64 codegen is
// bit-exact against the interpreter (the ground truth — see pcsx2/FPU.cpp).
//
// The genuine float arithmetic (ADD_S/SUB_S/MUL_S/DIV_S/SQRT_S/RSQRT_S, the ACC
// ops, the C.*.S compares, and the BC1* branches) needs the EE's denormal-flush /
// infinity-clamp / overflow-underflow behaviour (fpuDouble + checkOverflow/
// checkUnderflow). Those remain interpreter fallbacks until a later increment.

#include "aR5900.h"

#include "R5900.h"

#include "common/Assertions.h"

#include <cstddef>

namespace a64 = vixl::aarch64;

// FCR31 (fprc[31]) flag bits — see pcsx2/FPU.cpp.
static constexpr u32 FPUflagO = 0x00008000; // overflow
static constexpr u32 FPUflagU = 0x00004000; // underflow

// Clear the given FCR31 cause flags (read-modify-write fprc[31]). Used by the
// MOV-family ops that the interpreter documents as clearing O|U every execution.
static void emitClearFCR31Flags(u32 flags)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->And(RSCRATCHADDR.W(), RSCRATCHADDR.W(), ~flags);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
}

// ------------------------------------------------------------------------
// MFC1: move FPR -> GPR. The interpreter sign-extends the 32-bit FPR into the
// low 64-bit doubleword of the GPR (GPR[rt].SD[0] = (s32)fpr[fs]); the upper
// doubleword is left untouched, matching the EE's scalar-write semantics.
void armEmitMFC1(u32 rt, u32 fs)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Sxtw(RSCRATCHADDR, RSCRATCHADDR.W());
	armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
// MTC1: move GPR -> FPR (low word only). GPR[0] reads as zero straight from the
// register file, so rt==0 needs no special case.
void armEmitMTC1(u32 fs, u32 rt)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
}

// ------------------------------------------------------------------------
// CFC1: move FPU control register -> GPR. `fs` is a compile-time constant, so the
// interpreter's three-way select collapses to a single emitted path. fprc[31] is
// sign-extended; the other defined values (0x2E00 for fs==0, else 0) are constants.
void armEmitCFC1(u32 rt, u32 fs)
{
	if (rt == 0)
		return;

	if (fs == 31)
	{
		armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
		armAsm->Sxtw(RSCRATCHADDR, RSCRATCHADDR.W());
		armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	}
	else if (fs == 0)
	{
		armAsm->Mov(RSCRATCHADDR, 0x2E00);
		armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	}
	else
	{
		armAsm->Str(a64::xzr, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	}
}

// ------------------------------------------------------------------------
// CTC1: move GPR -> FPU control register. The interpreter only honours writes to
// fprc[31]; writes to any other control register are ignored, so for a compile-time
// fs != 31 this generator emits nothing.
void armEmitCTC1(u32 fs, u32 rt)
{
	if (fs != 31)
		return;

	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
}

// ------------------------------------------------------------------------
// MOV_S: fpr[fd] = fpr[fs] (pure 32-bit copy, no flags touched).
void armEmitMOV_S(u32 fd, u32 fs)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
}

// ------------------------------------------------------------------------
// ABS_S: fpr[fd] = fpr[fs] with the sign bit cleared; clears the O|U cause flags.
void armEmitABS_S(u32 fd, u32 fs)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->And(RSCRATCHADDR.W(), RSCRATCHADDR.W(), 0x7fffffff);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	emitClearFCR31Flags(FPUflagO | FPUflagU);
}

// ------------------------------------------------------------------------
// NEG_S: fpr[fd] = fpr[fs] with the sign bit flipped; clears the O|U cause flags.
void armEmitNEG_S(u32 fd, u32 fs)
{
	armAsm->Ldr(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fs)));
	armAsm->Eor(RSCRATCHADDR.W(), RSCRATCHADDR.W(), 0x80000000);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(fd)));
	emitClearFCR31Flags(FPUflagO | FPUflagU);
}

// ------------------------------------------------------------------------
// LWC1: fpr[ft].UL = mem32[GPR[rs] + imm]. Routed through the same slow-path vtlb
// helper as the GPR loads; the 32-bit result is written to the FPR's low word.
// (FPR0 is a real register, so there is no rt==0 discard as there is for GPRs.)
void armEmitLWC1(u32 ft, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armEmitVtlbRead(32, /*sign*/ false, RXRET, RWARG1);
	armAsm->Str(RWRET, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
}

// ------------------------------------------------------------------------
// SWC1: mem32[GPR[rs] + imm] = fpr[ft].UL.
void armEmitSWC1(u32 ft, u32 rs, s32 imm)
{
	armAsm->Ldr(RWARG2, a64::MemOperand(RESTATEPTR, EE_FPR_OFFSET(ft)));
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armEmitVtlbWrite(32, RWARG1, RWARG2);
}
