// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — immediate arithmetic codegen (Phase 3.1).
//
// Generates ARM64 for the MIPS I immediate arithmetic opcodes:
//   ADDI/ADDIU/SLTI/SLTIU/ANDI/ORI/XORI/LUI/DADDI/DADDIU
//
// No register allocator yet — every source GPR is loaded from cpuRegs in memory
// (via RESTATEPTR = &cpuRegs), computed in a scratch register, and stored back.
// $zero writes are silently discarded, matching interpreter semantics.

#include "aR5900.h"

#include "R5900.h"

#include <cstddef>

namespace a64 = vixl::aarch64;

// Scratch register for arithmetic ops (caller-saved, not used by any helper).
static const a64::Register RSCRATCH = RSCRATCHADDR;
static const a64::Register RSCRATCHW = RSCRATCHADDR.W();

// ------------------------------------------------------------------------
// ADDI / ADDIU  (primary opcodes 0x08 / 0x09)
// Rt = (s32)(GPR[rs].UL[0] + imm)
// The x86 JIT treats ADDI identically to ADDIU (skips the overflow trap);
// we follow that model.
// ------------------------------------------------------------------------
void armEmitADDI(u32 rt, u32 rs, s32 imm)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (imm != 0)
		armAsm->Add(RSCRATCHW, RSCRATCHW, imm);
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

void armEmitADDIU(u32 rt, u32 rs, s32 imm)
{
	armEmitADDI(rt, rs, imm); // identical in the JIT
}

// ------------------------------------------------------------------------
// DADDI / DADDIU  (primary opcodes 0x18 / 0x19)
// Rt = GPR[rs].UD[0] + (s64)imm
// ------------------------------------------------------------------------
void armEmitDADDI(u32 rt, u32 rs, s32 imm)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (imm != 0)
		armAsm->Add(RSCRATCH, RSCRATCH, imm);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

void armEmitDADDIU(u32 rt, u32 rs, s32 imm)
{
	armEmitDADDI(rt, rs, imm); // identical in the JIT
}

// ------------------------------------------------------------------------
// SLTI  (primary opcode 0x0A)
// Rt = (GPR[rs].SD[0] < (s64)imm) ? 1 : 0
// ------------------------------------------------------------------------
void armEmitSLTI(u32 rt, u32 rs, s32 imm)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Cmp(RSCRATCH, imm);
	armAsm->Cset(RSCRATCH, a64::lt);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
// SLTIU  (primary opcode 0x0B)
// Rt = (GPR[rs].UD[0] < (u64)(s64)imm) ? 1 : 0
// ------------------------------------------------------------------------
void armEmitSLTIU(u32 rt, u32 rs, s32 imm)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Cmp(RSCRATCH, imm);
	armAsm->Cset(RSCRATCH, a64::lo);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
// ANDI  (primary opcode 0x0C)
// Rt = GPR[rs].UD[0] & (u64)imm_u   (imm_u is zero-extended 16-bit)
// ------------------------------------------------------------------------
void armEmitANDI(u32 rt, u32 rs, u32 imm_u)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (imm_u == 0)
	{
		armAsm->Mov(RSCRATCH, 0);
	}
	else
	{
		// Zero-extended 16-bit immediates are not always valid ARM64 logical
		// immediates; materialize into a scratch register first.
		armAsm->Mov(RXVIXLSCRATCH, imm_u);
		armAsm->And(RSCRATCH, RSCRATCH, RXVIXLSCRATCH);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
// ORI  (primary opcode 0x0D)
// Rt = GPR[rs].UD[0] | (u64)imm_u
// ------------------------------------------------------------------------
void armEmitORI(u32 rt, u32 rs, u32 imm_u)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (imm_u != 0)
	{
		armAsm->Mov(RXVIXLSCRATCH, imm_u);
		armAsm->Orr(RSCRATCH, RSCRATCH, RXVIXLSCRATCH);
	}
	// imm_u == 0: identity; RSCRATCH already holds GPR[rs]
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
// XORI  (primary opcode 0x0E)
// Rt = GPR[rs].UD[0] ^ (u64)imm_u
// ------------------------------------------------------------------------
void armEmitXORI(u32 rt, u32 rs, u32 imm_u)
{
	if (rt == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (imm_u != 0)
	{
		armAsm->Mov(RXVIXLSCRATCH, imm_u);
		armAsm->Eor(RSCRATCH, RSCRATCH, RXVIXLSCRATCH);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
// LUI  (primary opcode 0x0F)
// Rt = (s32)(imm << 16)
// The 16-bit immediate is shifted left 16 and sign-extended from 32 to 64.
// ------------------------------------------------------------------------
void armEmitLUI(u32 rt, u32 imm)
{
	if (rt == 0)
		return;

	const s32 val = static_cast<s32>(static_cast<u32>(imm) << 16);
	if (val == 0)
	{
		armAsm->Mov(RSCRATCH, 0);
	}
	else
	{
		armAsm->Mov(RSCRATCHW, val);
		armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}
