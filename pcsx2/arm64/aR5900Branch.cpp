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
