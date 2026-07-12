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
#include "VU.h" // VU0 / REG_VPU_STAT for the COP2 (BC2) branch condition
#include "Memory.h" // eeHw — the backing store the DMAC registers alias into
#include "Hw.h"     // D0_CHCR.. HW-register address enum (Dmac.h prerequisite)
#include "Dmac.h"   // dmacRegs / DMACregisters for the COP0 (BC0) CPCOND0 condition

#include "common/Assertions.h"


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
// forms BC1FL/BC1TL (rt 0x02/0x03) are handled by armEmitBranchLikelyTest below.
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

// ------------------------------------------------------------------------
// COP2 conditional branches BC2F/BC2T (opcode 0x12, rs==0x08 (BC), rt 0x00/0x01).
// Branch on the VU0 macro-mode condition bit VBS0: VU0.VI[REG_VPU_STAT].UL & 0x100
// (CP2COND = bit 8). BC2F branches when the bit is CLEAR (CP2COND==0), BC2T when
// SET (CP2COND==1) — matching the interpreter (COP2.cpp BC2F/BC2T) and x86
// microVU_Macro.inl recBC2F/T (_setupBranchTest: TEST VPU_STAT,0x100 then
// recBC2F=JNZ32 / recBC2T=JZ32, where the jmpType skips the taken path on the
// opposite condition). This is purely a bit-test branch — x86 BC2 emits NO VU
// sync / interlock / cycle commit, so neither do we (unlike the M3 transfer ops).
// VU0.VI is global state (not RESTATEPTR-relative), so the address is materialized.
// The likely forms BC2FL/BC2TL (rt 0x02/0x03) are in armEmitBranchLikelyTest below.
static constexpr u32 VU0_VBS0 = 0x100;

void armEmitBC2F(u32 target, u32 fallthrough)
{
	armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_VPU_STAT].UL);
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RSCRATCHADDR));
	armAsm->Tst(RSCRATCHW, VU0_VBS0);
	emitSelectPc(target, fallthrough, a64::eq); // bit clear (CP2COND==0) -> branch
}

void armEmitBC2T(u32 target, u32 fallthrough)
{
	armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_VPU_STAT].UL);
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RSCRATCHADDR));
	armAsm->Tst(RSCRATCHW, VU0_VBS0);
	emitSelectPc(target, fallthrough, a64::ne); // bit set (CP2COND==1) -> branch
}

// ------------------------------------------------------------------------
// COP0 conditional branches BC0F/BC0T (opcode 0x10, rs==0x08 (BC), rt 0x00/0x01).
// Branch on CPCOND0, the DMA-ready flag the EE polls while waiting for DMA to finish:
//   CPCOND0 = (((dmacRegs.stat.CIS | ~dmacRegs.pcr.CPC) & 0x3FF) == 0x3FF)  (COP0.cpp:595)
// i.e. true once every PCR-enabled DMA channel has its STAT interrupt bit set. The OR
// form is emitted verbatim from CPCOND0() so the polarity is self-evident; CIS/CPC are
// bits[9:0] of stat/pcr, so masking the 32-bit loads with 0x3FF is exact. dmacRegs is
// global HW state (a reference into eeHw[0xE000]); materialize its base once and load
// stat+pcr by offset — the second Ldr's destination (w17) reuses the base register, so
// it must come last (the base is consumed for address-gen before being overwritten).
// BC0T branches when CPCOND0==1, BC0F when ==0, matching the interpreter (COP0.cpp
// BC0F/BC0T) and x86 iCOP0.cpp _setupBranchTest. No cycle commit / VU sync — a plain
// HW-register-test branch like BC1/BC2. The likely forms BC0FL/BC0TL (rt 0x02/0x03)
// are in armEmitBranchLikelyTest below.
static void emitCpcond0Test()
{
	armMoveAddressToReg(RSCRATCHADDR, &dmacRegs);                                              // x17 = &dmacRegs
	armAsm->Ldr(RXVIXLSCRATCH.W(), a64::MemOperand(RSCRATCHADDR, offsetof(DMACregisters, pcr)));  // w16 = PCR (CPC)
	armAsm->Ldr(RSCRATCHW, a64::MemOperand(RSCRATCHADDR, offsetof(DMACregisters, stat)));         // w17 = STAT (CIS); clobbers base last
	armAsm->Mvn(RXVIXLSCRATCH.W(), RXVIXLSCRATCH.W());                // ~CPC
	armAsm->Orr(RXVIXLSCRATCH.W(), RXVIXLSCRATCH.W(), RSCRATCHW);     // CIS | ~CPC
	armAsm->And(RXVIXLSCRATCH.W(), RXVIXLSCRATCH.W(), 0x3FF);         // & 0x3FF (bits[9:0])
	armAsm->Cmp(RXVIXLSCRATCH.W(), 0x3FF);                            // eq <=> CPCOND0 == 1
}

void armEmitBC0F(u32 target, u32 fallthrough)
{
	emitCpcond0Test();
	emitSelectPc(target, fallthrough, a64::ne); // CPCOND0 == 0 -> branch
}

void armEmitBC0T(u32 target, u32 fallthrough)
{
	emitCpcond0Test();
	emitSelectPc(target, fallthrough, a64::eq); // CPCOND0 == 1 -> branch
}

// ------------------------------------------------------------------------
// Branch-likely forms. Evaluate the condition, write
// cpuRegs.pc = taken ? target : fallthrough, and return the "taken" condition
// with the flags still live (the Mov/Csel/Str of the PC select don't touch
// flags), so the block compiler can branch around the nullified delay slot.
// Forms: 0x14 BEQL, 0x15 BNEL, 0x16 BLEZL, 0x17 BGTZL,
//        REGIMM rt 0x02 BLTZL / 0x03 BGEZL,
//        COP1 rs==0x08, rt 0x02 BC1FL / 0x03 BC1TL,
//        COP2 rs==0x08, rt 0x02 BC2FL / 0x03 BC2TL.
// ------------------------------------------------------------------------
vixl::aarch64::Condition armEmitBranchLikelyTest(u32 op, u32 target, u32 fallthrough)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;

	a64::Condition taken = a64::nv;
	switch (opcode)
	{
		case 0x14: // BEQL
		case 0x15: // BNEL
			armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
			armAsm->Ldr(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
			armAsm->Cmp(RSCRATCH, RXVIXLSCRATCH);
			taken = (opcode == 0x14) ? a64::eq : a64::ne;
			break;

		case 0x16: // BLEZL
		case 0x17: // BGTZL
			armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
			armAsm->Cmp(RSCRATCH, 0);
			taken = (opcode == 0x16) ? a64::le : a64::gt;
			break;

		case 0x01: // REGIMM: BLTZL (0x02) / BGEZL (0x03)
			pxAssert(rt == 0x02 || rt == 0x03);
			armAsm->Ldr(RSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
			armAsm->Cmp(RSCRATCH, 0);
			taken = (rt == 0x02) ? a64::lt : a64::ge;
			break;

		case 0x11: // COP1: BC1FL (rt 0x02) / BC1TL (rt 0x03)
			pxAssert(rs == 0x08 && (rt == 0x02 || rt == 0x03));
			armAsm->Ldr(RSCRATCHW, a64::MemOperand(RESTATEPTR, EE_FPRC_OFFSET(31)));
			armAsm->Tst(RSCRATCHW, FPUflagC);
			taken = (rt == 0x02) ? a64::eq : a64::ne;
			break;

		case 0x12: // COP2: BC2FL (rt 0x02) / BC2TL (rt 0x03)
			pxAssert(rs == 0x08 && (rt == 0x02 || rt == 0x03));
			armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_VPU_STAT].UL);
			armAsm->Ldr(RSCRATCHW, a64::MemOperand(RSCRATCHADDR));
			armAsm->Tst(RSCRATCHW, VU0_VBS0);
			taken = (rt == 0x02) ? a64::eq : a64::ne; // FL: bit clear; TL: bit set
			break;

		case 0x10: // COP0: BC0FL (rt 0x02) / BC0TL (rt 0x03)
			pxAssert(rs == 0x08 && (rt == 0x02 || rt == 0x03));
			emitCpcond0Test();                        // Cmp sets eq <=> CPCOND0 == 1
			taken = (rt == 0x03) ? a64::eq : a64::ne; // TL: CPCOND0==1; FL: CPCOND0==0
			break;

		default:
			pxFailRel("armEmitBranchLikelyTest: not a likely branch");
	}

	emitSelectPc(target, fallthrough, taken);
	return taken;
}

