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

void armEmitEffectiveAddr(const vixl::aarch64::Register& dst, u32 rs, s32 imm);
void armEmitLoadGpr(u32 bits, bool sign, u32 rt, u32 rs, s32 imm);
void armEmitStoreGpr(u32 bits, u32 rt, u32 rs, s32 imm);

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
