// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — integer arithmetic codegen (Phase 3.2).
//
// Generates ARM64 for the MIPS I-type and R-type integer arithmetic opcodes:
//   I-type: ADDI/ADDIU/SLTI/SLTIU/ANDI/ORI/XORI/LUI/DADDI/DADDIU   (Phase 3.1)
//   R-type: ADD/ADDU/SUB/SUBU/SLT/SLTU/AND/OR/XOR/NOR/DADD/DADDU/DSUB/DSUBU
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

// ------------------------------------------------------------------------
// R-type register-register arithmetic (FUNCT field in bits [5:0]).
// Format: OP rd, rs, rt
// ------------------------------------------------------------------------

// Second scratch register for R-type ops that need two source values.
static const a64::Register RSCRATCH2 = RXVIXLSCRATCH;
static const a64::Register RSCRATCH2W = RXVIXLSCRATCH.W();

// ------------------------------------------------------------------------
// ADD / ADDU  (funct 0x20 / 0x21)
// Rd = (s32)(GPR[rs].UL[0] + GPR[rt].UL[0])
// The x86 JIT skips the 32-bit overflow trap for ADD; ADDU is identical.
// ------------------------------------------------------------------------
void armEmitADD(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		// rs + rs in 32-bit, then sign-extend
		armAsm->Add(RSCRATCHW, RSCRATCHW, RSCRATCHW);
	}
	else
	{
		armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Add(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	}
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

void armEmitADDU(u32 rd, u32 rs, u32 rt)
{
	armEmitADD(rd, rs, rt); // identical in the JIT
}

// ------------------------------------------------------------------------
// DADD / DADDU  (funct 0x2C / 0x2D)
// Rd = GPR[rs].UD[0] + GPR[rt].UD[0]
// ------------------------------------------------------------------------
void armEmitDADD(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		armAsm->Add(RSCRATCH, RSCRATCH, RSCRATCH);
	}
	else
	{
		armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Add(RSCRATCH, RSCRATCH, RSCRATCH2);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

void armEmitDADDU(u32 rd, u32 rs, u32 rt)
{
	armEmitDADD(rd, rs, rt); // identical in the JIT
}

// ------------------------------------------------------------------------
// SUB / SUBU  (funct 0x22 / 0x23)
// Rd = (s32)(GPR[rs].UL[0] - GPR[rt].UL[0])
// ------------------------------------------------------------------------
void armEmitSUB(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		armAsm->Mov(RSCRATCHW, 0);
	}
	else
	{
		armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Sub(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	}
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

void armEmitSUBU(u32 rd, u32 rs, u32 rt)
{
	armEmitSUB(rd, rs, rt); // identical in the JIT
}

// ------------------------------------------------------------------------
// DSUB / DSUBU  (funct 0x2E / 0x2F)
// Rd = GPR[rs].UD[0] - GPR[rt].UD[0]
// ------------------------------------------------------------------------
void armEmitDSUB(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		armAsm->Mov(RSCRATCH, 0);
	}
	else
	{
		armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Sub(RSCRATCH, RSCRATCH, RSCRATCH2);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

void armEmitDSUBU(u32 rd, u32 rs, u32 rt)
{
	armEmitDSUB(rd, rs, rt); // identical in the JIT
}

// ------------------------------------------------------------------------
// AND  (funct 0x24)
// Rd = GPR[rs].UD[0] & GPR[rt].UD[0]
// ------------------------------------------------------------------------
void armEmitAND(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		// rs & rs == rs; RSCRATCH already holds the value.
	}
	else
	{
		armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->And(RSCRATCH, RSCRATCH, RSCRATCH2);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// OR  (funct 0x25)
// Rd = GPR[rs].UD[0] | GPR[rt].UD[0]
// ------------------------------------------------------------------------
void armEmitOR(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		// rs | rs == rs
	}
	else
	{
		armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Orr(RSCRATCH, RSCRATCH, RSCRATCH2);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// XOR  (funct 0x26)
// Rd = GPR[rs].UD[0] ^ GPR[rt].UD[0]
// ------------------------------------------------------------------------
void armEmitXOR(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		armAsm->Mov(RSCRATCH, 0);
	}
	else
	{
		armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Eor(RSCRATCH, RSCRATCH, RSCRATCH2);
	}
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// NOR  (funct 0x27)
// Rd = ~(GPR[rs].UD[0] | GPR[rt].UD[0])
// ------------------------------------------------------------------------
void armEmitNOR(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (rs == rt)
	{
		// ~(rs | rs) == ~rs
		armAsm->Orr(RSCRATCH, RSCRATCH, RSCRATCH);
	}
	else
	{
		armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Orr(RSCRATCH, RSCRATCH, RSCRATCH2);
	}
	armAsm->Mvn(RSCRATCH, RSCRATCH);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// SLT  (funct 0x2A)
// Rd = (GPR[rs].SD[0] < GPR[rt].SD[0]) ? 1 : 0
// ------------------------------------------------------------------------
void armEmitSLT(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Cmp(RSCRATCH, RSCRATCH2);
	armAsm->Cset(RSCRATCH, a64::lt);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// SLTU  (funct 0x2B)
// Rd = (GPR[rs].UD[0] < GPR[rt].UD[0]) ? 1 : 0
// ------------------------------------------------------------------------
void armEmitSLTU(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Cmp(RSCRATCH, RSCRATCH2);
	armAsm->Cset(RSCRATCH, a64::lo);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// Shift operations (Phase 3.3)
// ------------------------------------------------------------------------
//
// MIPS shift amounts:
//   - 32-bit shifts: amount masked to 5 bits (0-31). ARM64 variable shifts on
//     W-registers natively use the low 5 bits of the amount reg.
//   - 64-bit shifts: amount masked to 6 bits (0-63). ARM64 variable shifts on
//     X-registers natively use the low 6 bits of the amount reg.
//
// All 32-bit results (SLL/SRL/SRA/SLLV/SRLV/SRAV) are sign-extended to 64,
// matching MIPS semantics and the x86 JIT (xMOVSX(xRegister64, xRegister32)).

// ------------------------------------------------------------------------
// SLL  (funct 0x00)
// Rd = (s32)(GPR[rt].UL[0] << sa)
// ------------------------------------------------------------------------
void armEmitSLL(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsl(RSCRATCHW, RSCRATCHW, sa);
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// SRL  (funct 0x02)
// Rd = (s32)(GPR[rt].UL[0] >> sa)
// ------------------------------------------------------------------------
void armEmitSRL(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsr(RSCRATCHW, RSCRATCHW, sa);
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// SRA  (funct 0x03)
// Rd = (s32)(GPR[rt].SL[0] >> sa)
// ------------------------------------------------------------------------
void armEmitSRA(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Asr(RSCRATCHW, RSCRATCHW, sa);
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// SLLV  (funct 0x04)
// Rd = (s32)(GPR[rt].UL[0] << (GPR[rs].UL[0] & 0x1f))
// ------------------------------------------------------------------------
void armEmitSLLV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Lsl(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// SRLV  (funct 0x06)
// Rd = (s32)(GPR[rt].UL[0] >> (GPR[rs].UL[0] & 0x1f))
// ------------------------------------------------------------------------
void armEmitSRLV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Lsr(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// SRAV  (funct 0x07)
// Rd = (s32)(GPR[rt].SL[0] >> (GPR[rs].UL[0] & 0x1f))
// ------------------------------------------------------------------------
void armEmitSRAV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Asr(RSCRATCHW, RSCRATCHW, RSCRATCH2W);
	armAsm->Sxtw(RSCRATCH, RSCRATCHW);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSLLV  (funct 0x14)
// Rd = GPR[rt].UD[0] << (GPR[rs].UL[0] & 0x3f)
// ------------------------------------------------------------------------
void armEmitDSLLV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Lsl(RSCRATCH, RSCRATCH, RSCRATCH2);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSRLV  (funct 0x16)
// Rd = GPR[rt].UD[0] >> (GPR[rs].UL[0] & 0x3f)
// ------------------------------------------------------------------------
void armEmitDSRLV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Lsr(RSCRATCH, RSCRATCH, RSCRATCH2);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSRAV  (funct 0x17)
// Rd = (s64)(GPR[rt].SD[0] >> (GPR[rs].UL[0] & 0x3f))
// ------------------------------------------------------------------------
void armEmitDSRAV(u32 rd, u32 rt, u32 rs)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(RSCRATCH2W, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Asr(RSCRATCH, RSCRATCH, RSCRATCH2);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSLL  (funct 0x38)
// Rd = GPR[rt].UD[0] << sa
// ------------------------------------------------------------------------
void armEmitDSLL(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsl(RSCRATCH, RSCRATCH, sa);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSRL  (funct 0x3A)
// Rd = GPR[rt].UD[0] >> sa
// ------------------------------------------------------------------------
void armEmitDSRL(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsr(RSCRATCH, RSCRATCH, sa);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSRA  (funct 0x3B)
// Rd = (s64)(GPR[rt].SD[0] >> sa)
// ------------------------------------------------------------------------
void armEmitDSRA(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Asr(RSCRATCH, RSCRATCH, sa);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSLL32  (funct 0x3C)
// Rd = GPR[rt].UD[0] << (sa + 32)
// ------------------------------------------------------------------------
void armEmitDSLL32(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsl(RSCRATCH, RSCRATCH, sa + 32);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSRL32  (funct 0x3E)
// Rd = GPR[rt].UD[0] >> (sa + 32)
// ------------------------------------------------------------------------
void armEmitDSRL32(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsr(RSCRATCH, RSCRATCH, sa + 32);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// DSRA32  (funct 0x3F)
// Rd = (s64)(GPR[rt].SD[0] >> (sa + 32))
// ------------------------------------------------------------------------
void armEmitDSRA32(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Asr(RSCRATCH, RSCRATCH, sa + 32);
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// Phase 3.4 — Move operations
// ------------------------------------------------------------------------
// Offset helpers for HI/LO registers in cpuRegs (they follow GPR[31]).
static constexpr u32 EE_HI_OFFSET() { return 32 * 16u; }
static constexpr u32 EE_LO_OFFSET() { return 33 * 16u; }

// ------------------------------------------------------------------------
// MOVZ  (funct 0x0A)
// Rd = (GPR[rt].UD[0] == 0) ? GPR[rs].UD[0] : GPR[rd].UD[0]
// ------------------------------------------------------------------------
void armEmitMOVZ(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	// If rs == rd, the move is a no-op regardless of the condition.
	if (rs == rd)
		return;

	// Load Rt and test if zero.
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Cmp(RSCRATCH, 0);

	// Load Rd (destination current value) into scratch2.
	armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	// Load Rs into scratch.
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));

	// Conditional select: if Rt == 0 (eq), use Rs; otherwise keep Rd.
	armAsm->Csel(RSCRATCH2, RSCRATCH, RSCRATCH2, a64::eq);

	armAsm->Str(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// MOVN  (funct 0x0B)
// Rd = (GPR[rt].UD[0] != 0) ? GPR[rs].UD[0] : GPR[rd].UD[0]
// ------------------------------------------------------------------------
void armEmitMOVN(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	// If rs == rd, the move is a no-op regardless of the condition.
	if (rs == rd)
		return;

	// Load Rt and test if zero.
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Cmp(RSCRATCH, 0);

	// Load Rd (destination current value) into scratch2.
	armAsm->Ldr(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	// Load Rs into scratch.
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));

	// Conditional select: if Rt != 0 (ne), use Rs; otherwise keep Rd.
	armAsm->Csel(RSCRATCH2, RSCRATCH, RSCRATCH2, a64::ne);

	armAsm->Str(RSCRATCH2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// MFHI  (funct 0x10)
// Rd = HI
// ------------------------------------------------------------------------
void armEmitMFHI(u32 rd)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET()));
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// MTHI  (funct 0x11)
// HI = Rs
// ------------------------------------------------------------------------
void armEmitMTHI(u32 rs)
{
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET()));
}

// ------------------------------------------------------------------------
// MFLO  (funct 0x12)
// Rd = LO
// ------------------------------------------------------------------------
void armEmitMFLO(u32 rd)
{
	if (rd == 0)
		return;

	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET()));
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

// ------------------------------------------------------------------------
// MTLO  (funct 0x13)
// LO = Rs
// ------------------------------------------------------------------------
void armEmitMTLO(u32 rs)
{
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET()));
}


