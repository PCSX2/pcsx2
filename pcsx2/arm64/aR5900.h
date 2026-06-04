// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 EE (R5900) recompiler — public interface + register-allocation contract.
//
// This is the ARM64 counterpart to pcsx2/x86/iR5900.h. It is intentionally small
// for now (Phase 1 skeleton): it declares the recompiler entry points and pins
// down the persistent host-register assignments so that every later phase
// (load/store, arithmetic, branches, ...) uses the same map. See
// arm64-port/CONVENTIONS.md §2 for the rationale.

#include "R5900.h"

#include "arm64/AsmHelpers.h"

#include <cstddef>

// --------------------------------------------------------------------------------------
//  Persistent (callee-saved) host registers for hot EE state
// --------------------------------------------------------------------------------------
// Chosen from the AAPCS64 callee-saved GPR set (x19-x28) so they survive C ABI
// calls into the interpreter/helpers without extra save/restore. Reserve them
// here once; do not reuse them as scratch inside generators.
//
//   x19 = &cpuRegs              (base for all guest GPR / PC / HI/LO accesses)
//   x20 = fastmem (4GB) base    (vtlb fast path; analogous to x86 RFASTMEMBASE/rbp)
//   x21 = vtlb table base       (assigned for real in Phase 2)
//
#define RESTATEPTR vixl::aarch64::x19
#define REFASTMEMBASE vixl::aarch64::x20
#define REVTLBPTR vixl::aarch64::x21

// --------------------------------------------------------------------------------------
//  Slow-path EE memory access codegen (Phase 2)
// --------------------------------------------------------------------------------------
// Emit a call to the C++ vtlb_memRead / vtlb_memWrite helpers — semantically
// identical to the interpreter's load/store ops (same virtual-memory path, so
// these are correct by construction). The fastmem fast path (direct access via
// REFASTMEMBASE + SIGSEGV backpatch through vtlb_DynBackpatchLoadStore) is Phase 2.2.
//
//   bits  : 8 / 16 / 32 / 64 (use the *Quad helpers for 128-bit).
//   sign  : sign-extend (true) vs zero-extend (false) the loaded value into the GPR.
//   addr  : 32-bit guest address (the W view of the register is used).
//   dst   : destination guest GPR; result is extended to the full 64-bit X view.
//   data  : value to store (X view for 64-bit, W view otherwise; Q view for quad).
//
// These clobber the AAPCS64 caller-saved set (x0-x17, v0-v7, v16-v31) and LR. The
// persistent state regs (x19-x21) are callee-saved and survive. Once the EE rec
// has a register allocator (Phase 3) it must flush live caller-saved guest state
// before invoking these.
void armEmitVtlbRead(u32 bits, bool sign, const vixl::aarch64::Register& dst, const vixl::aarch64::Register& addr);
void armEmitVtlbWrite(u32 bits, const vixl::aarch64::Register& addr, const vixl::aarch64::Register& data);
void armEmitVtlbReadQuad(const vixl::aarch64::VRegister& dst, const vixl::aarch64::Register& addr);
void armEmitVtlbWriteQuad(const vixl::aarch64::Register& addr, const vixl::aarch64::VRegister& data);

// --------------------------------------------------------------------------------------
//  EE GPR load/store opcode generators (Phase 2.3)
// --------------------------------------------------------------------------------------
// The first vertical slice that turns decoded MIPS load/store ops into ARM64 that
// reads/writes guest memory. These take the simple, non-allocating path: every
// guest GPR is read from / written back to cpuRegs in memory (via RESTATEPTR =
// &cpuRegs) around the access — there is no register allocator yet (Phase 3). The
// access itself goes through the slow-path armEmitVtlbRead/Write helpers above.
//
// Fields are passed explicitly (decoded from cpuRegs.code by the caller) so the
// generators are independent of any opcode-table wiring and are unit-testable.
//
//   armEmitEffectiveAddr: dst.W() = GPR[rs].UL[0] + imm  (the EE address mode).
//   armEmitLoadGpr:  GPR[rt] = sign/zero-extend(mem[addr]); skips write-back for rt==0.
//   armEmitStoreGpr: mem[addr] = GPR[rt] (low `bits` bits).
//
// bits = 8/16/32/64; imm is the sign-extended 16-bit MIPS immediate.

// Byte offset of guest GPR `n`'s low word within cpuRegs (GPR is the first member;
// each GPR_reg is 128 bits wide).
static constexpr u32 EE_GPR_OFFSET(u32 n) { return n * 16u; }

// Byte offset of cpuRegs.pc (the next-PC field the branch/jump generators write).
static constexpr u32 EE_PC_OFFSET = static_cast<u32>(offsetof(cpuRegisters, pc));

// Byte offset of cpuRegs.code (the current-instruction field the interpreter reads).
static constexpr u32 EE_CODE_OFFSET = static_cast<u32>(offsetof(cpuRegisters, code));

void armEmitEffectiveAddr(const vixl::aarch64::Register& dst, u32 rs, s32 imm);
void armEmitLoadGpr(u32 bits, bool sign, u32 rt, u32 rs, s32 imm);
void armEmitStoreGpr(u32 bits, u32 rt, u32 rs, s32 imm);

// --------------------------------------------------------------------------------------
//  FPU (COP1) register-file offsets
// --------------------------------------------------------------------------------------
// fpuRegs lives in the same cpuRegistersPack as cpuRegs, with cpuRegs as the first
// (offset-0) member. RESTATEPTR is set to &cpuRegs, which is therefore also the base
// of the pack — so the whole FPU register file is reachable at a fixed offset from
// RESTATEPTR, exactly like the GPRs. fpr[]/ACC are 32-bit FPRreg slots; fprc[] is the
// 32-entry control-register array (fprc[31] = FCR31, the flags/status word).
static_assert(offsetof(cpuRegistersPack, cpuRegs) == 0, "cpuRegs must be first in the pack");
static constexpr u32 EE_FPU_BASE = static_cast<u32>(offsetof(cpuRegistersPack, fpuRegs));
static constexpr u32 EE_FPR_OFFSET(u32 n)
{
	return EE_FPU_BASE + static_cast<u32>(offsetof(fpuRegisters, fpr)) + n * static_cast<u32>(sizeof(FPRreg));
}
static constexpr u32 EE_FPRC_OFFSET(u32 n)
{
	return EE_FPU_BASE + static_cast<u32>(offsetof(fpuRegisters, fprc)) + n * static_cast<u32>(sizeof(u32));
}
// Byte offset of the FPU accumulator (ACC) — the destination of the ADDA/SUBA/MULA/
// MADDA/MSUBA ACC ops.
static constexpr u32 EE_ACC_OFFSET = EE_FPU_BASE + static_cast<u32>(offsetof(fpuRegisters, ACC));

// --------------------------------------------------------------------------------------
//  FPU (COP1) exact-semantics opcode generators (Phase 5.2a)
// --------------------------------------------------------------------------------------
// The subset of COP1 that is pure bit/integer movement (no EE-specific float
// arithmetic quirks), so it is bit-exact against the interpreter on ARM64. The
// arithmetic ops (ADD_S/SUB_S/MUL_S/DIV_S/SQRT_S/compares/ACC ops/BC1) need the
// EE's non-IEEE rounding+clamp behaviour and stay on the interpreter for now.
//
//   MFC1 : GPR[rt].SD[0] = (s32)fpr[fs].UL    (sign-extend into 64-bit GPR; rt==0 skip)
//   MTC1 : fpr[fs].UL    = GPR[rt].UL[0]
//   CFC1 : GPR[rt].SD[0] = (fs==31) ? (s32)fprc[31] : (fs==0) ? 0x2E00 : 0   (rt==0 skip)
//   CTC1 : if (fs==31) fprc[31] = GPR[rt].UL[0]    (other fs are ignored)
//   MOV_S: fpr[fd].UL = fpr[fs].UL
//   ABS_S: fpr[fd].UL = fpr[fs].UL & 0x7fffffff;  clear FCR31 O|U flags
//   NEG_S: fpr[fd].UL = fpr[fs].UL ^ 0x80000000;  clear FCR31 O|U flags
//   LWC1 : fpr[ft].UL = mem32[GPR[rs].UL[0] + imm]
//   SWC1 : mem32[GPR[rs].UL[0] + imm] = fpr[ft].UL
void armEmitMFC1(u32 rt, u32 fs);
void armEmitMTC1(u32 fs, u32 rt);
void armEmitCFC1(u32 rt, u32 fs);
void armEmitCTC1(u32 fs, u32 rt);
void armEmitMOV_S(u32 fd, u32 fs);
void armEmitABS_S(u32 fd, u32 fs);
void armEmitNEG_S(u32 fd, u32 fs);
void armEmitLWC1(u32 ft, u32 rs, s32 imm);
void armEmitSWC1(u32 ft, u32 rs, s32 imm);

// --------------------------------------------------------------------------------------
//  FPU (COP1) float arithmetic opcode generators (Phase 5.2b)
// --------------------------------------------------------------------------------------
// These reproduce the EE FPU's *non-IEEE* float behaviour, mirroring the interpreter
// (pcsx2/FPU.cpp — the project's ground truth and the current fallback):
//   - inputs run through fpuDouble(): denormals/zeros flush to signed zero, inf/NaN
//     clamp to signed fmax (0x7f7fffff);
//   - the single-precision result runs through checkOverflow (inf -> signed fmax) and
//     checkUnderflow (denormal -> signed zero), updating the FCR31 (fprc[31]) flags.
// The arithmetic itself is host single-precision NEON, which is bit-identical to the
// interpreter's `float OP float` (both IEEE round-to-nearest-even).
//
//   ADD_S/SUB_S/MUL_S : fpr[fd] = clamp(fpuDouble(fpr[fs]) OP fpuDouble(fpr[ft]))
//   DIV_S             : fpr[fd] = clamp(fpuDouble(fpr[fs]) / fpuDouble(fpr[ft]))
//   SQRT_S            : fpr[fd] = sqrt(abs(fpuDouble(fpr[ft]))) with signed-zero/invalid handling
//   RSQRT_S           : fpr[fd] = clamp(fpuDouble(fpr[fs]) / sqrt(abs(fpuDouble(fpr[ft]))))
//   ADDA_S/SUBA_S/MULA_S : ACC   = clamp(fpuDouble(fpr[fs]) OP fpuDouble(fpr[ft]))
void armEmitADD_S(u32 fd, u32 fs, u32 ft);
void armEmitSUB_S(u32 fd, u32 fs, u32 ft);
void armEmitMUL_S(u32 fd, u32 fs, u32 ft);
void armEmitDIV_S(u32 fd, u32 fs, u32 ft);
void armEmitSQRT_S(u32 fd, u32 ft);
void armEmitRSQRT_S(u32 fd, u32 fs, u32 ft);
void armEmitADDA_S(u32 fs, u32 ft);
void armEmitSUBA_S(u32 fs, u32 ft);
void armEmitMULA_S(u32 fs, u32 ft);

// 128-bit LQ/SQ: the effective address is forced to 16-byte alignment and the
// whole 128-bit GPR is loaded/stored via the Quad vtlb helpers (NEON q access).
void armEmitLoadQuad(u32 rt, u32 rs, s32 imm);
void armEmitStoreQuad(u32 rt, u32 rs, s32 imm);

// --------------------------------------------------------------------------------------
//  EE immediate arithmetic opcode generators (Phase 3.1)
// --------------------------------------------------------------------------------------
// Format: OP rt, rs, immediate (I-type). imm is sign-extended (_Imm_) for ADDI/… and
// zero-extended (_ImmU_) for ANDI/ORI/XORI. $zero writes are discarded.
//
// ADDI/ADDIU  : Rt = s32(GPR[rs].UL[0] + imm)   (add as 32-bit, sign-extend)
// DADDI/DADDIU: Rt = GPR[rs].UD[0] + (s64)imm   (64-bit add, no overflow check)
// SLTI        : Rt = (GPR[rs].SD[0] <  imm) ? 1 : 0
// SLTIU       : Rt = (GPR[rs].UD[0] < (u64)(s64)imm) ? 1 : 0
// ANDI        : Rt = GPR[rs].UD[0] & (u64)imm_u
// ORI         : Rt = GPR[rs].UD[0] | (u64)imm_u
// XORI        : Rt = GPR[rs].UD[0] ^ (u64)imm_u
// LUI         : Rt = s32(imm << 16)

void armEmitADDI(u32 rt, u32 rs, s32 imm);
void armEmitADDIU(u32 rt, u32 rs, s32 imm);
void armEmitDADDI(u32 rt, u32 rs, s32 imm);
void armEmitDADDIU(u32 rt, u32 rs, s32 imm);
void armEmitSLTI(u32 rt, u32 rs, s32 imm);
void armEmitSLTIU(u32 rt, u32 rs, s32 imm);
void armEmitANDI(u32 rt, u32 rs, u32 imm_u);
void armEmitORI(u32 rt, u32 rs, u32 imm_u);
void armEmitXORI(u32 rt, u32 rs, u32 imm_u);
void armEmitLUI(u32 rt, u32 imm);

// --------------------------------------------------------------------------------------
//  EE register-register arithmetic opcode generators (Phase 3.2)
// --------------------------------------------------------------------------------------
// Format: OP rd, rs, rt (R-type). All operations are on the low 64-bit GPR word;
// 32-bit results are sign-extended to 64. $zero writes are discarded.
void armEmitADD(u32 rd, u32 rs, u32 rt);
void armEmitADDU(u32 rd, u32 rs, u32 rt);
void armEmitDADD(u32 rd, u32 rs, u32 rt);
void armEmitDADDU(u32 rd, u32 rs, u32 rt);
void armEmitSUB(u32 rd, u32 rs, u32 rt);
void armEmitSUBU(u32 rd, u32 rs, u32 rt);
void armEmitDSUB(u32 rd, u32 rs, u32 rt);
void armEmitDSUBU(u32 rd, u32 rs, u32 rt);
void armEmitAND(u32 rd, u32 rs, u32 rt);
void armEmitOR(u32 rd, u32 rs, u32 rt);
void armEmitXOR(u32 rd, u32 rs, u32 rt);
void armEmitNOR(u32 rd, u32 rs, u32 rt);
void armEmitSLT(u32 rd, u32 rs, u32 rt);
void armEmitSLTU(u32 rd, u32 rs, u32 rt);

// --------------------------------------------------------------------------------------
//  EE shift opcode generators (Phase 3.3)
// --------------------------------------------------------------------------------------
// Format: OP rd, rt, sa (immediate shifts) or OP rd, rt, rs (variable shifts).
// 32-bit results are sign-extended to 64. $zero writes are discarded.
//
//   sa: 5-bit shift amount from the MIPS `sa` field (bits 10:6). DSLL32/DSRL32/DSRA32
//       add 32 to the effective amount.
//
//   rs: variable shift amount is read from GPR[rs].UL[0] (low word). The value is
//       masked by the shift width by ARM64 hardware (5 bits for W-reg, 6 for X-reg),
//       matching MIPS semantics.
void armEmitSLL(u32 rd, u32 rt, u32 sa);
void armEmitSRL(u32 rd, u32 rt, u32 sa);
void armEmitSRA(u32 rd, u32 rt, u32 sa);
void armEmitSLLV(u32 rd, u32 rt, u32 rs);
void armEmitSRLV(u32 rd, u32 rt, u32 rs);
void armEmitSRAV(u32 rd, u32 rt, u32 rs);
void armEmitDSLLV(u32 rd, u32 rt, u32 rs);
void armEmitDSRLV(u32 rd, u32 rt, u32 rs);
void armEmitDSRAV(u32 rd, u32 rt, u32 rs);
void armEmitDSLL(u32 rd, u32 rt, u32 sa);
void armEmitDSRL(u32 rd, u32 rt, u32 sa);
void armEmitDSRA(u32 rd, u32 rt, u32 sa);
void armEmitDSLL32(u32 rd, u32 rt, u32 sa);
void armEmitDSRL32(u32 rd, u32 rt, u32 sa);
void armEmitDSRA32(u32 rd, u32 rt, u32 sa);

// --------------------------------------------------------------------------------------
//  EE move opcode generators (Phase 3.4)
// --------------------------------------------------------------------------------------
// Format: OP rd, rs, rt (R-type conditional moves) or OP rd, fs (HI/LO moves).
// $zero writes are discarded.
//
//   MOVZ: Rd = (Rt == 0) ? Rs : Rd   (conditional move if Rt equals zero)
//   MOVN: Rd = (Rt != 0) ? Rs : Rd   (conditional move if Rt non-zero)
//   MFHI: Rd = HI
//   MTHI: HI = Rs
//   MFLO: Rd = LO
//   MTLO: LO = Rs
void armEmitMOVZ(u32 rd, u32 rs, u32 rt);
void armEmitMOVN(u32 rd, u32 rs, u32 rt);
void armEmitMFHI(u32 rd);
void armEmitMTHI(u32 rs);
void armEmitMFLO(u32 rd);
void armEmitMTLO(u32 rs);

// --------------------------------------------------------------------------------------
//  EE multiply/divide opcode generators (Phase 3.5)
// --------------------------------------------------------------------------------------
// Format: OP rd, rs, rt (R-type). Results written to the HI/LO special registers
// (the R5900 has no DMULT/DDIV — those are not EE instructions and are omitted).
//
//   MULT    : HI/LO = (s64)(GPR[rs].SL[0] * GPR[rt].SL[0]); if rd!=0 GPR[rd]=LO
//   MULTU   : HI/LO = (u64)(GPR[rs].UL[0] * GPR[rt].UL[0]); if rd!=0 GPR[rd]=LO
//   DIV     : LO = quotient, HI = remainder (signed 32-bit, with overflow/div0 handling)
//   DIVU    : LO = quotient, HI = remainder (unsigned 32-bit, with div0 handling)
//
// The "1" variants (MMI group) run on the second multiplier pipeline: results go
// to the upper doubleword HI1/LO1 (HI.SD[1]/LO.SD[1]) and Rd reads LO.UD[1].

void armEmitMULT(u32 rd, u32 rs, u32 rt);
void armEmitMULTU(u32 rd, u32 rs, u32 rt);
void armEmitDIV(u32 rs, u32 rt);
void armEmitDIVU(u32 rs, u32 rt);

// MMI second-pipeline variants (HI1/LO1).
void armEmitMULT1(u32 rd, u32 rs, u32 rt);
void armEmitMULTU1(u32 rd, u32 rs, u32 rt);
void armEmitDIV1(u32 rs, u32 rt);
void armEmitDIVU1(u32 rs, u32 rt);

// --------------------------------------------------------------------------------------
//  EE jump opcode generators (Phase 4.1)
// --------------------------------------------------------------------------------------
// These emit ONLY the control-flow effect — the next-PC write (cpuRegs.pc) and, for
// the linking forms, the GPR[31]/GPR[rd] return-address write. The block compiler is
// responsible for compiling the delay-slot instruction and for terminating the block
// (RET back to the dispatcher loop, which re-reads cpuRegs.pc). Register-target jumps
// (JR/JALR) read GPR[rs] *before* the delay slot is compiled, so the generator must be
// invoked before the delay slot; the value lands in cpuRegs.pc immediately (the delay
// slot never touches pc, so the early write is safe).
//
// All address arguments are absolute and precomputed by the caller relative to the
// delay slot (= branchpc + 4):
//   target : J/JAL  -> (instr_index << 2) | ((branchpc + 4) & 0xF0000000)
//   linkpc : JAL/JALR return address = branchpc + 8 (zero-extended into GPR.UD[0]).
//
//   J    : cpuRegs.pc = target
//   JAL  : GPR[31].UD[0] = linkpc; cpuRegs.pc = target
//   JR   : cpuRegs.pc = GPR[rs].UL[0]
//   JALR : cpuRegs.pc = GPR[rs].UL[0]; if (rd) GPR[rd].UD[0] = linkpc  (rs read first)
void armEmitJ(u32 target);
void armEmitJAL(u32 target, u32 linkpc);
void armEmitJR(u32 rs);
void armEmitJALR(u32 rd, u32 rs, u32 linkpc);

// --------------------------------------------------------------------------------------
//  EE conditional branch opcode generators (Phase 4.2)
// --------------------------------------------------------------------------------------
// Same contract as the jumps: emit only the next-PC selection (and, for the *AL forms,
// the unconditional GPR[31] link). The condition is evaluated on the source GPR(s) read
// here, then cpuRegs.pc is set to `target` (branch taken) or `fallthrough` (not taken):
//   target      = (branchpc + 4) + (s16(imm) << 2)
//   fallthrough = branchpc + 8   (the instruction after the delay slot)
//
// Comparisons match the interpreter: BEQ/BNE compare the full 64-bit GPR[rs]/GPR[rt];
// the single-operand forms compare signed 64-bit GPR[rs] against zero. The *AL forms
// write the link *before* reading rs (matching the interpreter's _SetLink ordering).
void armEmitBEQ(u32 rs, u32 rt, u32 target, u32 fallthrough);
void armEmitBNE(u32 rs, u32 rt, u32 target, u32 fallthrough);
void armEmitBLTZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBGEZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBLEZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBGTZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBLTZAL(u32 rs, u32 target, u32 fallthrough, u32 linkpc);
void armEmitBGEZAL(u32 rs, u32 target, u32 fallthrough, u32 linkpc);
