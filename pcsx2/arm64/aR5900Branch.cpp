// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — branch/jump codegen (Phase 4.1 / 4.2).
//
// These generators emit only the control-flow effect of a branch or jump: the
// next-PC write into cpuRegs.pc and, for the linking forms, the return-address
// write into a GPR. They do NOT compile the delay-slot instruction or terminate
// the block — that is the block compiler's job (it compiles the delay slot after
// invoking the generator, then RETs back to the dispatcher loop, which re-reads
// cpuRegs.pc to find the next block).
//
// Why writing cpuRegs.pc *before* the delay slot is safe: no EE delay-slot
// instruction writes cpuRegs.pc, so the early write survives unchanged. For the
// register-target forms (JR/JALR) this is also *required* for correctness — the
// jump target must be the value of GPR[rs] as it was before the delay slot, which
// may overwrite rs. Reading rs into pc here captures it at the right time.
//
// No register allocator yet — sources are read from / results written to cpuRegs
// in memory via RESTATEPTR. The only scratch used is RSCRATCHADDR (x17); the
// immediates materialized here (Mov of a 32-bit constant) go straight into the
// destination register, so VIXL never needs RXVIXLSCRATCH (x16) as a temp.

#include "aR5900.h"

#include "R5900.h"

namespace a64 = vixl::aarch64;

// Scratch register (caller-saved; clobbered freely by these generators).
static const a64::Register RSCRATCH = RSCRATCHADDR;
static const a64::Register RSCRATCHW = RSCRATCHADDR.W();

// Store a 32-bit value into cpuRegs.pc.
static void emitWritePcReg(const a64::Register& src_w)
{
	armAsm->Str(src_w, a64::MemOperand(RESTATEPTR, EE_PC_OFFSET));
}

// cpuRegs.pc = imm
static void emitWritePcImm(u32 pc)
{
	armAsm->Mov(RSCRATCHW, pc);
	emitWritePcReg(RSCRATCHW);
}

// GPR[reg].UD[0] = linkpc (zero-extended 32->64; upper 64 bits of the 128-bit reg
// are left untouched, matching the x86 JIT / interpreter _SetLink).
static void emitWriteLink(u32 reg, u32 linkpc)
{
	armAsm->Mov(RSCRATCHW, linkpc);                                          // X upper 32 bits zeroed
	armAsm->Str(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(reg))); // store 64-bit => UD[0]
}

// ------------------------------------------------------------------------
// J / JAL  (primary opcodes 0x02 / 0x03) — immediate (region) target.
// ------------------------------------------------------------------------
void armEmitJ(u32 target)
{
	emitWritePcImm(target);
}

void armEmitJAL(u32 target, u32 linkpc)
{
	emitWriteLink(31, linkpc);
	emitWritePcImm(target);
}

// ------------------------------------------------------------------------
// JR / JALR  (SPECIAL funct 0x08 / 0x09) — register target.
// The target is GPR[rs].UL[0] read *before* the delay slot.
// ------------------------------------------------------------------------
void armEmitJR(u32 rs)
{
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	emitWritePcReg(RSCRATCHW);
}

void armEmitJALR(u32 rd, u32 rs, u32 linkpc)
{
	// Read rs and commit the target first, so that rd==rs (link overwriting the
	// target source) still jumps to the original GPR[rs].
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	emitWritePcReg(RSCRATCHW);
	if (rd != 0)
		emitWriteLink(rd, linkpc);
}

// ------------------------------------------------------------------------
// Conditional branches (Phase 4.2).
// ------------------------------------------------------------------------
// Given that a preceding Cmp has set the condition flags, write
//   cpuRegs.pc = cond ? target : fallthrough.
// Both constants are materialized straight into their destination registers
// (x17 = fallthrough, x16 = target), so neither Mov needs a VIXL temp and the
// flags from the Cmp survive into the Csel.
static void emitSelectPc(u32 target, u32 fallthrough, a64::Condition cond)
{
	armAsm->Mov(RSCRATCHW, fallthrough);
	armAsm->Mov(RXVIXLSCRATCH.W(), target);
	armAsm->Csel(RSCRATCHW, RXVIXLSCRATCH.W(), RSCRATCHW, cond);
	emitWritePcReg(RSCRATCHW);
}

void armEmitBEQ(u32 rs, u32 rt, u32 target, u32 fallthrough)
{
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));     // GPR[rs].UD[0]
	armAsm->Ldr(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt))); // GPR[rt].UD[0]
	armAsm->Cmp(RSCRATCH, RXVIXLSCRATCH);
	emitSelectPc(target, fallthrough, a64::eq);
}

void armEmitBNE(u32 rs, u32 rt, u32 target, u32 fallthrough)
{
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Cmp(RSCRATCH, RXVIXLSCRATCH);
	emitSelectPc(target, fallthrough, a64::ne);
}

// Single-operand forms compare signed 64-bit GPR[rs] against zero.
static void emitBranchZero(u32 rs, u32 target, u32 fallthrough, a64::Condition cond)
{
	armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Cmp(RSCRATCH, 0);
	emitSelectPc(target, fallthrough, cond);
}

void armEmitBLTZ(u32 rs, u32 target, u32 fallthrough)
{
	emitBranchZero(rs, target, fallthrough, a64::lt); // rs <  0
}

void armEmitBGEZ(u32 rs, u32 target, u32 fallthrough)
{
	emitBranchZero(rs, target, fallthrough, a64::ge); // rs >= 0
}

void armEmitBLEZ(u32 rs, u32 target, u32 fallthrough)
{
	emitBranchZero(rs, target, fallthrough, a64::le); // rs <= 0
}

void armEmitBGTZ(u32 rs, u32 target, u32 fallthrough)
{
	emitBranchZero(rs, target, fallthrough, a64::gt); // rs >  0
}

// *AL forms: the link is written unconditionally and *before* rs is read, matching
// the interpreter's _SetLink ordering (so a degenerate rs==31 compares the link).
void armEmitBLTZAL(u32 rs, u32 target, u32 fallthrough, u32 linkpc)
{
	emitWriteLink(31, linkpc);
	emitBranchZero(rs, target, fallthrough, a64::lt);
}

void armEmitBGEZAL(u32 rs, u32 target, u32 fallthrough, u32 linkpc)
{
	emitWriteLink(31, linkpc);
	emitBranchZero(rs, target, fallthrough, a64::ge);
}

// ------------------------------------------------------------------------
// COP1 conditional branches BC1F/BC1T (opcode 0x11, rs==0x08, rt 0x00/0x01).
// Branch on the FCR31 C (condition) bit set by the C.* compares. The likely
// forms BC1FL/BC1TL (rt 0x02/0x03) nullify the delay slot and stay on the
// interpreter fallback, matching the other likely branches.
static constexpr u32 FPUflagC = 0x00800000;

void armEmitBC1F(u32 target, u32 fallthrough)
{
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->Tst(RSCRATCHW, FPUflagC);
	emitSelectPc(target, fallthrough, a64::eq); // C == 0 -> branch
}

void armEmitBC1T(u32 target, u32 fallthrough)
{
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
	armAsm->Tst(RSCRATCHW, FPUflagC);
	emitSelectPc(target, fallthrough, a64::ne); // C != 0 -> branch
}
