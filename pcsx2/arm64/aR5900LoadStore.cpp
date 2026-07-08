// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — slow-path load/store codegen.
//
// This is the ARM64 counterpart to the inline-vtlb portion of
// pcsx2/x86/ix86-32/recVTLB.cpp, but for bring-up it takes the simplest correct
// route: every guest memory access is emitted as a direct call to the C++
// vtlb_memRead / vtlb_memWrite helpers — exactly the path the interpreter uses.
// That makes these helpers correct by construction (interpreter is ground truth)
// and avoids needing the register allocator / indirect dispatchers yet.
//
// The fastmem path (direct access through a pinned 4GB base with SIGSEGV
// backpatching via vtlb_DynBackpatchLoadStore) was never wired up; the vtlb
// vmap path through REVTLBPTR is the fast path (see aR5900.h).

#include "aR5900.h"

#include "Memory.h"
#include "R5900.h"
#include "vtlb.h"

#include "common/Assertions.h"

#include <cstddef>

namespace a64 = vixl::aarch64;

// The effective-address codegen assumes guest GPRs are laid out as 16-byte
// GPR_reg slots starting at the base of cpuRegs (so GPR[n].UL[0] is at n*16).
static_assert(sizeof(GPR_reg) == 16, "GPR_reg must be 128 bits for EE_GPR_OFFSET");
static_assert(offsetof(cpuRegisters, GPR) == 0, "GPR must be the first member of cpuRegs");

// ------------------------------------------------------------------------
void armEmitVtlbRead(u32 bits, bool sign, const a64::Register& dst, const a64::Register& addr)
{
	// 32-bit guest address goes in the first argument register.
	if (!addr.W().Is(RWARG1))
		armAsm->Mov(RWARG1, addr.W());

	const void* fn;
	switch (bits)
	{
		case 8:  fn = reinterpret_cast<const void*>(&vtlb_memRead<mem8_t>);  break;
		case 16: fn = reinterpret_cast<const void*>(&vtlb_memRead<mem16_t>); break;
		case 32: fn = reinterpret_cast<const void*>(&vtlb_memRead<mem32_t>); break;
		case 64: fn = reinterpret_cast<const void*>(&vtlb_memRead<mem64_t>); break;
		jNO_DEFAULT
	}
	armEmitCall(fn);

	// Extend the returned value into the full 64-bit destination, matching the
	// interpreter's load semantics. The C ABI leaves the high bits of a sub-word
	// return undefined, so the extension here is mandatory, not an optimisation.
	switch (bits)
	{
		case 8:  sign ? armAsm->Sxtb(dst.X(), RWRET) : armAsm->Uxtb(dst.W(), RWRET); break;
		case 16: sign ? armAsm->Sxth(dst.X(), RWRET) : armAsm->Uxth(dst.W(), RWRET); break;
		case 32: sign ? armAsm->Sxtw(dst.X(), RWRET) : armAsm->Mov(dst.W(), RWRET); break;
		case 64: if (!dst.X().Is(RXRET)) armAsm->Mov(dst.X(), RXRET); break;
		jNO_DEFAULT
	}
}

// ------------------------------------------------------------------------
void armEmitVtlbWrite(u32 bits, const a64::Register& addr, const a64::Register& data)
{
	const void* fn;
	switch (bits)
	{
		case 8:  fn = reinterpret_cast<const void*>(&vtlb_memWrite<mem8_t>);  break;
		case 16: fn = reinterpret_cast<const void*>(&vtlb_memWrite<mem16_t>); break;
		case 32: fn = reinterpret_cast<const void*>(&vtlb_memWrite<mem32_t>); break;
		case 64: fn = reinterpret_cast<const void*>(&vtlb_memWrite<mem64_t>); break;
		jNO_DEFAULT
	}

	// vtlb_memWrite<T>(u32 addr, T data): addr -> arg1, data -> arg2. Stage the
	// value through the scratch reg first so addr/data may live in any registers
	// (including each other's arg reg) without an aliasing hazard.
	if (bits == 64)
	{
		armAsm->Mov(RXVIXLSCRATCH, data.X());
		if (!addr.W().Is(RWARG1))
			armAsm->Mov(RWARG1, addr.W());
		armAsm->Mov(RXARG2, RXVIXLSCRATCH);
	}
	else
	{
		armAsm->Mov(RWVIXLSCRATCH, data.W());
		if (!addr.W().Is(RWARG1))
			armAsm->Mov(RWARG1, addr.W());
		armAsm->Mov(RWARG2, RWVIXLSCRATCH);
	}
	armEmitCall(fn);
}

// ------------------------------------------------------------------------
void armEmitVtlbReadQuad(const a64::VRegister& dst, const a64::Register& addr)
{
	if (!addr.W().Is(RWARG1))
		armAsm->Mov(RWARG1, addr.W());

	// vtlb_memRead128 returns r128 (uint32x4_t) in q0.
	armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead128));

	if (!dst.Q().Is(RQRET))
		armAsm->Mov(dst.Q(), RQRET);
}

// ------------------------------------------------------------------------
void armEmitVtlbWriteQuad(const a64::Register& addr, const a64::VRegister& data)
{
	// vtlb_memWrite128(u32 mem, r128 value): mem -> w0, value (uint32x4_t) -> q0.
	// Set the vector arg first; it can't alias the GPR address argument.
	if (!data.Q().Is(RQRET))
		armAsm->Mov(RQRET, data.Q());
	if (!addr.W().Is(RWARG1))
		armAsm->Mov(RWARG1, addr.W());

	armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite128));
}

// ========================================================================
//  EE GPR load/store opcode generators (Phase 2.3)
// ========================================================================

// ------------------------------------------------------------------------
void armEmitEffectiveAddr(const a64::Register& dst, u32 rs, s32 imm)
{
	// addr = GPR[rs].UL[0] + imm. GPR[0] is hardwired to zero, so for rs==0 the
	// address is just the (sign-extended) immediate.
	if (rs == 0)
	{
		armAsm->Mov(dst.W(), imm);
		return;
	}

	armAsm->Ldr(dst.W(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	if (imm != 0)
		armAsm->Add(dst.W(), dst.W(), imm); // MacroAssembler materializes any s16 imm
}

// ------------------------------------------------------------------------
void armEmitLoadGpr(u32 bits, bool sign, u32 rt, u32 rs, s32 imm)
{
	// Effective address into the read helper's first argument register.
	armEmitEffectiveAddr(RWARG1, rs, imm);

	// Perform the load even when rt==0 (the access can have I/O side effects);
	// the extended 64-bit result lands in RXRET. Use it as the scratch dst.
	armEmitVtlbRead(bits, sign, RXRET, RWARG1);

	if (rt == 0)
		return;

	// Write the full 64-bit (sign/zero-extended) result to GPR[rt].UD[0]. The
	// upper doubleword (UD[1]) is left untouched, matching the interpreter — EE
	// scalar loads only define the low 64 bits of the 128-bit register.
	armAsm->Str(RXRET, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
void armEmitStoreGpr(u32 bits, u32 rt, u32 rs, s32 imm)
{
	// Load the value to store first (GPR[rt], low `bits` bits) into the write
	// helper's data argument. GPR[0] reads as zero straight from cpuRegs, so no
	// special case is needed for rt==0.
	if (bits == 64)
		armAsm->Ldr(RXARG2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	else
		armAsm->Ldr(RWARG2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));

	// Effective address into the write helper's first argument register.
	armEmitEffectiveAddr(RWARG1, rs, imm);

	armEmitVtlbWrite(bits, RWARG1, (bits == 64) ? RXARG2 : RWARG2);
}

// ------------------------------------------------------------------------
void armEmitLoadQuad(u32 rt, u32 rs, s32 imm)
{
	// Effective address into the read helper's first argument register, then
	// force 16-byte alignment (the EE silently aligns 128-bit accesses, matching
	// the x86 `xAND(arg1regd, ~0x0F)` in recLoadQuad).
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x0F);

	// Read the full 128-bit quadword into a vector scratch (the call inside
	// ReadQuad clobbers v0-v7/v16-v31, so the Mov to RQSCRATCH happens after it).
	armEmitVtlbReadQuad(RQSCRATCH, RWARG1);

	if (rt == 0)
		return;

	// Quad loads define the entire 128-bit register (both doublewords).
	armAsm->Str(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// ------------------------------------------------------------------------
void armEmitStoreQuad(u32 rt, u32 rs, s32 imm)
{
	// Load the full 128-bit GPR[rt] into a vector scratch (GPR[0] reads as zero
	// straight from cpuRegs, so rt==0 needs no special case). WriteQuad moves it
	// to q0 before its call, so the scratch only needs to live until then.
	armAsm->Ldr(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));

	// Effective address into the write helper's first argument register, 16-byte
	// aligned to match EE 128-bit store semantics.
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x0F);

	armEmitVtlbWriteQuad(RWARG1, RQSCRATCH);
}

// ========================================================================
//  Unaligned load/store (LWL/LWR/SWL/SWR, LDL/LDR/SDL/SDR)
// ========================================================================
// Bit-exact ports of the interpreter's mask/shift tables (R5900OpcodeImpl.cpp),
// computed at runtime from the low address bits instead of via lookup tables:
//
//   shift_bits = (addr & 3) << 3            (word forms;  0/8/16/24)
//   shift_bits = (addr & 7) << 3            (doubleword;  0..56)
//
//   LWL_MASK[s] = 0x00ffffff >> shift_bits
//   LWR_MASK[s] = ~(0xffffffff >> shift_bits)
//   SWL_MASK[s] = low32(0xffffffffull << (shift_bits + 8))   (X-shift: s=3 -> 0)
//   SWR_MASK[s] = ~(0xffffffff << shift_bits)
//   LDL_MASK[s] = 0x00ffffffffffffff >> shift_bits
//   LDR_MASK[s] = ~(0xffffffffffffffff >> shift_bits)
//   SDL_MASK[s] = 0xffffffffffffff00 << shift_bits           (s=7 shifts to 0)
//   SDR_MASK[s] = ~(0xffffffffffffffff << shift_bits)
//
// The vtlb call clobbers all caller-saved registers, so the effective address is
// recomputed afterwards from GPR[rs] in memory — safe because nothing in the read
// (or in this op before the final write) modifies GPR memory. Scratch: w9-w15.

// addr -> w9 (recomputed post-call), shift_bits -> w10.
static void emitUnalignedShift(u32 rs, s32 imm, u32 addr_mask)
{
	armEmitEffectiveAddr(a64::w9, rs, imm);
	armAsm->And(a64::w10, a64::w9, addr_mask);
	armAsm->Lsl(a64::w10, a64::w10, 3);
}

void armEmitLWL(u32 rt, u32 rs, s32 imm)
{
	// mem = memRead32(addr & ~3)  (the access happens even for rt==0).
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x03);
	armEmitVtlbRead(32, /*sign*/ false, RXRET, RWARG1);
	if (rt == 0)
		return;

	emitUnalignedShift(rs, imm, 3);
	// rt = (s32)((rt & (0x00ffffff >> sh)) | (mem << (24 - sh)))
	armAsm->Mov(a64::w11, 0x00ffffff);
	armAsm->Lsr(a64::w11, a64::w11, a64::w10);
	armAsm->Mov(a64::w12, 24);
	armAsm->Sub(a64::w12, a64::w12, a64::w10);
	armAsm->Lsl(a64::w13, RWRET, a64::w12);
	armAsm->Ldr(a64::w14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->And(a64::w14, a64::w14, a64::w11);
	armAsm->Orr(a64::w14, a64::w14, a64::w13);
	armAsm->Sxtw(a64::x14, a64::w14); // SD[0] = (s32)merged
	armAsm->Str(a64::x14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

void armEmitLWR(u32 rt, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x03);
	armEmitVtlbRead(32, /*sign*/ false, RXRET, RWARG1);
	if (rt == 0)
		return;

	emitUnalignedShift(rs, imm, 3);
	// merged = (rt & ~(0xffffffff >> sh)) | (mem >> sh)
	armAsm->Lsr(a64::w13, RWRET, a64::w10);
	armAsm->Mvn(a64::w11, a64::wzr); // 0xffffffff
	armAsm->Lsr(a64::w11, a64::w11, a64::w10);
	armAsm->Mvn(a64::w11, a64::w11);
	armAsm->Ldr(a64::x14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->And(a64::w15, a64::w14, a64::w11);
	armAsm->Orr(a64::w15, a64::w15, a64::w13);

	// shift==0 loads the whole word and sign-extends into SD[0]; any other shift
	// writes only UL[0] and preserves the upper half of UD[0] (interpreter LWR).
	a64::Label partial, done;
	armAsm->Cbnz(a64::w10, &partial);
	armAsm->Sxtw(a64::x15, a64::w15);
	armAsm->Str(a64::x15, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->B(&done);
	armAsm->Bind(&partial);
	armAsm->Bfi(a64::x14, a64::x15, 0, 32); // UD[0][31:0] = merged, [63:32] kept
	armAsm->Str(a64::x14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Bind(&done);
}

void armEmitSWL(u32 rt, u32 rs, s32 imm)
{
	// mem = memRead32(addr & ~3)
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x03);
	armEmitVtlbRead(32, /*sign*/ false, RXRET, RWARG1);

	emitUnalignedShift(rs, imm, 3);
	// value = (rt >> (24 - sh)) | (mem & low32(0xffffffffull << (sh + 8)))
	armAsm->Ldr(a64::w13, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Mov(a64::w12, 24);
	armAsm->Sub(a64::w12, a64::w12, a64::w10);
	armAsm->Lsr(a64::w13, a64::w13, a64::w12);
	armAsm->Mvn(a64::w11, a64::wzr);            // x11 = 0x00000000ffffffff
	armAsm->Add(a64::w12, a64::w10, 8);
	armAsm->Lsl(a64::x11, a64::x11, a64::x12);  // X shift: sh=24 -> mask bits leave low32
	armAsm->And(a64::w11, a64::w11, RWRET);
	armAsm->Orr(a64::w13, a64::w13, a64::w11);

	armAsm->And(RWARG1, a64::w9, ~0x03);
	armEmitVtlbWrite(32, RWARG1, a64::w13);
}

void armEmitSWR(u32 rt, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x03);
	armEmitVtlbRead(32, /*sign*/ false, RXRET, RWARG1);

	emitUnalignedShift(rs, imm, 3);
	// value = (rt << sh) | (mem & ~(0xffffffff << sh))
	armAsm->Ldr(a64::w13, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsl(a64::w13, a64::w13, a64::w10);
	armAsm->Mvn(a64::w11, a64::wzr);
	armAsm->Lsl(a64::w11, a64::w11, a64::w10);
	armAsm->Bic(a64::w11, RWRET, a64::w11); // mem & ~(0xffffffff << sh)
	armAsm->Orr(a64::w13, a64::w13, a64::w11);

	armAsm->And(RWARG1, a64::w9, ~0x03);
	armEmitVtlbWrite(32, RWARG1, a64::w13);
}

void armEmitLDL(u32 rt, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x07);
	armEmitVtlbRead(64, /*sign*/ false, RXRET, RWARG1);
	if (rt == 0)
		return;

	emitUnalignedShift(rs, imm, 7);
	// rt = (rt & (0x00ffffffffffffff >> sh)) | (mem << (56 - sh))
	armAsm->Mov(a64::x11, 0x00ffffffffffffffULL);
	armAsm->Lsr(a64::x11, a64::x11, a64::x10);
	armAsm->Mov(a64::w12, 56);
	armAsm->Sub(a64::w12, a64::w12, a64::w10);
	armAsm->Lsl(a64::x13, RXRET, a64::x12);
	armAsm->Ldr(a64::x14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->And(a64::x14, a64::x14, a64::x11);
	armAsm->Orr(a64::x14, a64::x14, a64::x13);
	armAsm->Str(a64::x14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

void armEmitLDR(u32 rt, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x07);
	armEmitVtlbRead(64, /*sign*/ false, RXRET, RWARG1);
	if (rt == 0)
		return;

	emitUnalignedShift(rs, imm, 7);
	// rt = (rt & ~(0xffffffffffffffff >> sh)) | (mem >> sh)
	armAsm->Lsr(a64::x13, RXRET, a64::x10);
	armAsm->Mvn(a64::x11, a64::xzr);
	armAsm->Lsr(a64::x11, a64::x11, a64::x10);
	armAsm->Mvn(a64::x11, a64::x11);
	armAsm->Ldr(a64::x14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->And(a64::x14, a64::x14, a64::x11);
	armAsm->Orr(a64::x14, a64::x14, a64::x13);
	armAsm->Str(a64::x14, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

void armEmitSDL(u32 rt, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x07);
	armEmitVtlbRead(64, /*sign*/ false, RXRET, RWARG1);

	emitUnalignedShift(rs, imm, 7);
	// value = (rt >> (56 - sh)) | (mem & (0xffffffffffffff00 << sh))
	armAsm->Ldr(a64::x13, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Mov(a64::w12, 56);
	armAsm->Sub(a64::w12, a64::w12, a64::w10);
	armAsm->Lsr(a64::x13, a64::x13, a64::x12);
	armAsm->Mov(a64::x11, 0xffffffffffffff00ULL);
	armAsm->Lsl(a64::x11, a64::x11, a64::x10); // sh=56 -> all mask bits shift out -> 0
	armAsm->And(a64::x11, a64::x11, RXRET);
	armAsm->Orr(a64::x13, a64::x13, a64::x11);

	armAsm->And(RWARG1, a64::w9, ~0x07);
	armEmitVtlbWrite(64, RWARG1, a64::x13);
}

void armEmitSDR(u32 rt, u32 rs, s32 imm)
{
	armEmitEffectiveAddr(RWARG1, rs, imm);
	armAsm->And(RWARG1, RWARG1, ~0x07);
	armEmitVtlbRead(64, /*sign*/ false, RXRET, RWARG1);

	emitUnalignedShift(rs, imm, 7);
	// value = (rt << sh) | (mem & ~(0xffffffffffffffff << sh))
	armAsm->Ldr(a64::x13, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Lsl(a64::x13, a64::x13, a64::x10);
	armAsm->Mvn(a64::x11, a64::xzr);
	armAsm->Lsl(a64::x11, a64::x11, a64::x10);
	armAsm->Bic(a64::x11, RXRET, a64::x11);
	armAsm->Orr(a64::x13, a64::x13, a64::x11);

	armAsm->And(RWARG1, a64::w9, ~0x07);
	armEmitVtlbWrite(64, RWARG1, a64::x13);
}
