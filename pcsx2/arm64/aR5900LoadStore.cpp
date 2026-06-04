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
#include "vtlb.h"

#include "common/Assertions.h"

namespace a64 = vixl::aarch64;

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
