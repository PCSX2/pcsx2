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
