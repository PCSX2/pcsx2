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
// The fast path (direct access through REFASTMEMBASE with SIGSEGV backpatching
// via vtlb_DynBackpatchLoadStore) is Phase 2.2 — see arm64/RecStubs.cpp.

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
