// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-License-Identifier: GPL-3.0

#include "common/Console.h"
#include "MTVU.h"
#include "SaveState.h"
#include "vtlb.h"

#include "arm64/AsmHelpers.h"
#include "arm64/aR5900.h"

#include "common/Assertions.h"

namespace a64 = vixl::aarch64;

// Host-MMU fastmem backpatch (FASTMEM F2). Called from the shared vtlb_BackpatchLoadStore
// (vtlb.cpp) when a single fastmem Ldr/Str at `code_address` faults on a handler/MMIO/
// unmapped page. We carve a thunk that redoes the access via the slow vtlb_mem* helper
// (using the recorded address/data registers), resumes at the instruction after the fault,
// then overwrite the faulting Ldr/Str with a branch to the thunk so subsequent executions
// skip straight to the slow path (no re-fault). Our cycle model is memory-based, so — unlike
// the x86 stock thunk — there is no cycle flush/reload.
//
// address_register / data_register are ARM64 register codes; they can be EE guest-GPR cache
// regs (x20/x22-x27), not just scratch, so the moves below preserve them where needed.
// RESTATEPTR(x19)/REVTLBPTR(x21)/RFASTMEMBASE(x28) are callee-saved and survive the C call.
// is_fpr is always false here: LWC1/SWC1 stage the FP value through a GPR, so their memory
// access is a plain 32-bit integer access. size_in_bits==128 is the LQ/SQ vector path.
void vtlb_DynBackpatchLoadStore(uptr code_address, u32 code_size, u32 guest_pc, u32 guest_addr, u32 gpr_bitmask, u32 fpr_bitmask, u8 address_register, u8 data_register, u8 size_in_bits, bool is_signed, bool is_load, bool is_fpr)
{
	u8* const thunk = recBeginThunk();

	if (size_in_bits == 128)
	{
		// 128-bit quad via vtlb_memRead128/vtlb_memWrite128 (r128 value in q0 = RQRET). data
		// is a V-register (banked separately from the GPR address), so no addr/data aliasing.
		if (is_load)
		{
			if (address_register != RWARG1.GetCode())
				armAsm->Mov(RWARG1, armWRegister(address_register));
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead128));
			if (data_register != RQRET.GetCode())
				armAsm->Mov(armQRegister(data_register).V16B(), RQRET.V16B());
		}
		else
		{
			if (data_register != RQRET.GetCode())
				armAsm->Mov(RQRET.V16B(), armQRegister(data_register).V16B());
			if (address_register != RWARG1.GetCode())
				armAsm->Mov(RWARG1, armWRegister(address_register));
			armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite128));
		}
	}
	else if (is_load)
	{
		// addr -> w0 (RWARG1). vtlb_memRead<T>(u32 addr) -> value in w0/x0.
		if (address_register != RWARG1.GetCode())
			armAsm->Mov(RWARG1, armWRegister(address_register));

		switch (size_in_bits)
		{
			case 8:  armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem8_t>));  break;
			case 16: armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem16_t>)); break;
			case 32: armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem32_t>)); break;
			case 64: armEmitCall(reinterpret_cast<const void*>(&vtlb_memRead<mem64_t>)); break;
		}

		// Extend the sub-word return into the FULL x0 (the C ABI leaves the high bits of a
		// sub-word return undefined) so the X-move below carries the correct 64-bit value —
		// matching the interpreter's load semantics and the direct fastmem load's extension.
		switch (size_in_bits)
		{
			case 8:  is_signed ? armAsm->Sxtb(RXRET, RWRET) : armAsm->Uxtb(RWRET, RWRET); break;
			case 16: is_signed ? armAsm->Sxth(RXRET, RWRET) : armAsm->Uxth(RWRET, RWRET); break;
			case 32: is_signed ? armAsm->Sxtw(RXRET, RWRET) : armAsm->Mov(RWRET, RWRET);  break; // W-move zeroes high 32
			case 64: break;
		}
		if (data_register != RXRET.GetCode())
			armAsm->Mov(armXRegister(data_register), RXRET);
	}
	else
	{
		// vtlb_memWrite<T>(u32 addr -> w0, T value -> x1/w1). Resolve the aliasing case
		// where addr sits in x1 and data in x0 (moving one would clobber the other).
		if (address_register == RXARG2.GetCode() && data_register == RXARG1.GetCode())
		{
			armAsm->Mov(RXVIXLSCRATCH, armXRegister(address_register)); // x16 = addr
			armAsm->Mov(RXARG2, armXRegister(data_register));           // x1  = value
			armAsm->Mov(RWARG1, RWVIXLSCRATCH);                         // x0  = addr (low 32)
		}
		else
		{
			if (data_register != RXARG2.GetCode())
				armAsm->Mov(RXARG2, armXRegister(data_register));
			if (address_register != RWARG1.GetCode())
				armAsm->Mov(RWARG1, armWRegister(address_register));
		}

		switch (size_in_bits)
		{
			case 8:  armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem8_t>));  break;
			case 16: armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem16_t>)); break;
			case 32: armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem32_t>)); break;
			case 64: armEmitCall(reinterpret_cast<const void*>(&vtlb_memWrite<mem64_t>)); break;
		}
	}

	// No cycle reload (memory-based cycle model). Resume at the instruction after the fault.
	armEmitJmp(reinterpret_cast<const void*>(code_address + code_size));
	recEndThunk();

	// Redirect the faulting single fastmem instr to the thunk (one 4-byte B in place).
	armEmitJmpPtr(reinterpret_cast<void*>(code_address), thunk, true);
}

// SaveStateBase::vuJITFreeze() now lives in arm64/aVU.cpp (Phase 7.2c), where it
// freezes the real microVU0/1 pipeline state (mVU.prog.lpState) instead of the
// 96-byte placeholder that used to be stubbed here.
