// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0

#include "arm64/AsmHelpers.h"
#include "arm64/iR5900-arm64.h"
#include "common/Console.h"
#include "common/HostSys.h"
#include "Memory.h"
#include "MTVU.h"
#include "SaveState.h"
#include "vtlb.h"

#include "common/Assertions.h"

namespace a64 = vixl::aarch64;

using namespace vtlb_private;

void vtlb_DynBackpatchLoadStore(uptr code_address, u32 code_size, u32 guest_pc, u32 guest_addr,
	u32 gpr_bitmask, u32 fpr_bitmask, u8 address_register, u8 data_register,
	u8 size_in_bits, bool is_signed, bool is_load, bool is_fpr)
{
#if 0
	DevCon.WriteLn("Backpatching %s at %p[%u] (pc %08X vaddr %08X): GPR %08X FPR %08X Addr %u Data %u Size %u Flags %02X %02X",
		is_load ? "load" : "store", (void*)code_address, code_size, guest_pc, guest_addr,
		gpr_bitmask, fpr_bitmask, address_register, data_register, size_in_bits, is_signed, is_load);
#endif

	u8* thunk = recBeginThunk();

	// Collect caller-saved GPRs that need saving.
	// Callee-saved (x19+) are preserved by the C call and don't need saving.
	// For loads into a GPR, skip the data register (result goes there).
	static constexpr u32 MAX_SAVE_GPRS = 16;
	u8 gprs_to_save[MAX_SAVE_GPRS];
	u32 num_gprs = 0;

	for (u32 i = 0; i < 19; i++)
	{
		if (!(gpr_bitmask & (1u << i)))
			continue;
		// Skip scratch/reserved: x8 (RWSCRATCH), x16 (VIXL), x17 (RSCRATCHADDR), x18 (platform)
		if (i == 8 || i >= 16)
			continue;
		// For loads into GPR, skip the data register
		if (is_load && !is_fpr && i == data_register)
			continue;
		pxAssert(num_gprs < MAX_SAVE_GPRS);
		gprs_to_save[num_gprs++] = static_cast<u8>(i);
	}

	// Collect NEON regs that need saving.
	// q8-q15 lower 64 bits are callee-saved, but the JIT uses full 128-bit, so save all live ones.
	static constexpr u32 MAX_SAVE_FPRS = 32;
	u8 fprs_to_save[MAX_SAVE_FPRS];
	u32 num_fprs = 0;

	for (u32 i = 0; i < 32; i++)
	{
		if (!(fpr_bitmask & (1u << i)))
			continue;
		// For loads into FPR, skip the data register
		if (is_load && is_fpr && i == data_register)
			continue;
		pxAssert(num_fprs < MAX_SAVE_FPRS);
		fprs_to_save[num_fprs++] = static_cast<u8>(i);
	}

	// Calculate stack size (must be 16-byte aligned)
	const u32 gpr_save_bytes = num_gprs * 8;
	const u32 fpr_save_bytes = num_fprs * 16;
	const u32 stack_size = (gpr_save_bytes + fpr_save_bytes + 15u) & ~15u;

	if (stack_size > 0)
		armAsm->Sub(a64::sp, a64::sp, stack_size);

	// Save GPRs to stack
	u32 offset = 0;
	for (u32 i = 0; i < num_gprs; i++)
	{
		armAsm->Str(a64::XRegister(gprs_to_save[i]), a64::MemOperand(a64::sp, offset));
		offset += 8;
	}

	// Save NEON regs to stack
	for (u32 i = 0; i < num_fprs; i++)
	{
		armAsm->Str(a64::QRegister(fprs_to_save[i]), a64::MemOperand(a64::sp, offset));
		offset += 16;
	}

	// At this point, all host registers still have their original JIT values
	// (STR only reads, doesn't modify the source register).

	// Flush cpuRegs.pc and cpuRegs.code for exception handling.
	// The fastmem path skips iFlushCall, so these may be stale.
	// If the vtlb handler triggers a TLB miss or other exception,
	// cpuTlbMiss reads cpuRegs.pc to set EPC.
	armAsm->Mov(RWSCRATCH, guest_pc);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));

	armAsm->Mov(RWSCRATCH, *(u32*)PSM(guest_pc));
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.code));

	// Set up arguments for the vtlb handler call.

	// 128-bit fastmem path. Always uses q0 (data_register == 0); the JIT
	// callers in recVTLB-arm64.cpp materialize the load result / store
	// value into q0 and never allocate q0 to a guest register at the
	// fastmem emit point. vtlb_memRead128 returns r128 in q0 per AAPCS64;
	// vtlb_memWrite128 takes value in q0.
	if (size_in_bits == 128)
	{
		pxAssertRel(is_fpr && data_register == 0,
			"128-bit fastmem backpatch must target q0");

		if (address_register != 9)
			armAsm->Mov(a64::w9, armWRegister(address_register));

		armAsm->Lsr(a64::w8, a64::w9, VTLB_PAGE_BITS);
		armMoveAddressToReg(RSCRATCHADDR, vtlb_private::vtlbdata.vmap);
		armAsm->Ldr(a64::x8, a64::MemOperand(RSCRATCHADDR, a64::x8, a64::LSL, 3));
		armAsm->Add(a64::x0, a64::x8, a64::Operand(a64::w9, a64::UXTW));

		a64::Label slow_path, done;
		armAsm->Tbnz(a64::x0, 63, &slow_path);

		if (is_load)
			armAsm->Ldr(a64::q0, a64::MemOperand(a64::x0));
		else
			armAsm->Str(a64::q0, a64::MemOperand(a64::x0));
		armAsm->B(&done);

		armAsm->Bind(&slow_path);
		armAsm->Mov(a64::w0, a64::w9);
		// Spill/reload RECCYCLE around the vtlb handler call — the slow path
		// dispatches to MMIO handlers (hwRead*/hwWrite*) which read/write
		// cpuRegs.cycle (timer regs, IntCHackCheck, etc.). Without this,
		// the handler sees a stale cycle value, which can mis-schedule
		// events and cause cascading mid-block timing bugs. Matches the
		// pattern at recVTLB-arm64.cpp:112+120.
		armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
		if (is_load)
			armEmitCall((void*)vtlb_memRead128);
		else
			armEmitCall((void*)vtlb_memWrite128);
		armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

		armAsm->Bind(&done);
	}
	else if (is_load)
	{
		// Load backpatch: emit inline VTLB read code (same as vtlbSoftmemRead).
		if (address_register != 9)
			armAsm->Mov(a64::w9, armWRegister(address_register));

		// Inline VTLB lookup
		armAsm->Lsr(a64::w8, a64::w9, VTLB_PAGE_BITS);
		armMoveAddressToReg(RSCRATCHADDR, vtlb_private::vtlbdata.vmap);
		armAsm->Ldr(a64::x8, a64::MemOperand(RSCRATCHADDR, a64::x8, a64::LSL, 3));
		armAsm->Add(a64::x0, a64::x8, a64::Operand(a64::w9, a64::UXTW));

		a64::Label slow_path, done;
		armAsm->Tbnz(a64::x0, 63, &slow_path);

		// Fast path: direct memory read via resolved host pointer
		switch (size_in_bits)
		{
			case 8:
				if (is_signed)
					armAsm->Ldrsb(a64::x0, a64::MemOperand(a64::x0));
				else
					armAsm->Ldrb(a64::w0, a64::MemOperand(a64::x0));
				break;
			case 16:
				if (is_signed)
					armAsm->Ldrsh(a64::x0, a64::MemOperand(a64::x0));
				else
					armAsm->Ldrh(a64::w0, a64::MemOperand(a64::x0));
				break;
			case 32:
				if (is_signed)
					armAsm->Ldrsw(a64::x0, a64::MemOperand(a64::x0));
				else
					armAsm->Ldr(a64::w0, a64::MemOperand(a64::x0));
				break;
			case 64:
				armAsm->Ldr(a64::x0, a64::MemOperand(a64::x0));
				break;
			default: pxFailRel("Unsupported load size in backpatch"); break;
		}
		armAsm->B(&done);

		// Slow path: call vtlb_memRead handler
		armAsm->Bind(&slow_path);
		armAsm->Mov(a64::w0, a64::w9);
		// Spill/reload RECCYCLE — see 128-bit slow_path above for rationale.
		armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
		switch (size_in_bits)
		{
			case 8:  armEmitCall((void*)vtlb_memRead<mem8_t>);  break;
			case 16: armEmitCall((void*)vtlb_memRead<mem16_t>); break;
			case 32: armEmitCall((void*)vtlb_memRead<mem32_t>); break;
			case 64: armEmitCall((void*)vtlb_memRead<mem64_t>); break;
			default: break;
		}
		armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
		// Extend the handler return into x0 for the 64-bit cpuRegs.GPR store.
		// AAPCS64 leaves the upper bits of x0 unspecified for sub-word returns,
		// so UNSIGNED sub-64-bit loads must Uxtw too — otherwise the garbage
		// upper 32 bits leak into the 64-bit EE GPR (LWU/LBU/LHU faulting to an
		// MMIO/handler page). The fast inline path zero-extends via Ldrb/Ldrh/
		// Ldr w0; this mirrors that, and the const-paddr shortcut in
		// recVTLB-arm64.cpp which handles the identical hazard.
		if (size_in_bits < 64)
		{
			if (is_signed)
			{
				if (size_in_bits == 8)
					armAsm->Sxtb(a64::x0, a64::w0);
				else if (size_in_bits == 16)
					armAsm->Sxth(a64::x0, a64::w0);
				else if (size_in_bits == 32)
					armAsm->Sxtw(a64::x0, a64::w0);
			}
			else
			{
				armAsm->Uxtw(a64::x0, a64::w0);
			}
		}

		armAsm->Bind(&done);

		// Move result to data register
		if (!is_fpr)
		{
			if (data_register != 0)
				armAsm->Mov(armXRegister(data_register), a64::x0);
		}
		else
		{
			armAsm->Fmov(a64::SRegister(data_register), a64::w0);
		}
	}
	else
	{
		// Store backpatch: emit inline VTLB write code (same as vtlbSoftmemWrite),
		// emitting the inline VTLB lookup + store rather than calling vtlb_memWrite.
		// Move address to w9, value to w10 (standard scratch for inline VTLB).
		if (address_register != 9)
			armAsm->Mov(a64::w9, armWRegister(address_register));
		if (data_register != 10)
		{
			if (size_in_bits <= 32)
				armAsm->Mov(a64::w10, armWRegister(data_register));
			else
				armAsm->Mov(a64::x10, armXRegister(data_register));
		}

		// Inline VTLB lookup: vmap[addr >> PAGE_BITS] → ppf
		armAsm->Lsr(a64::w8, a64::w9, VTLB_PAGE_BITS);
		armMoveAddressToReg(RSCRATCHADDR, vtlb_private::vtlbdata.vmap);
		armAsm->Ldr(a64::x8, a64::MemOperand(RSCRATCHADDR, a64::x8, a64::LSL, 3));
		armAsm->Add(a64::x0, a64::x8, a64::Operand(a64::w9, a64::UXTW));

		a64::Label slow_path, done;
		armAsm->Tbnz(a64::x0, 63, &slow_path);

		// Fast path: direct memory write via resolved host pointer
		switch (size_in_bits)
		{
			case 8:  armAsm->Strb(a64::w10, a64::MemOperand(a64::x0)); break;
			case 16: armAsm->Strh(a64::w10, a64::MemOperand(a64::x0)); break;
			case 32: armAsm->Str(a64::w10, a64::MemOperand(a64::x0)); break;
			case 64: armAsm->Str(a64::x10, a64::MemOperand(a64::x0)); break;
			default: pxFailRel("Unsupported store size in backpatch"); break;
		}
		armAsm->B(&done);

		// Slow path: call vtlb_memWrite handler
		armAsm->Bind(&slow_path);
		armAsm->Mov(a64::w0, a64::w9);
		if (size_in_bits <= 32)
			armAsm->Mov(a64::w1, a64::w10);
		else
			armAsm->Mov(a64::x1, a64::x10);

		// Spill/reload RECCYCLE — see 128-bit slow_path above for rationale.
		armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
		switch (size_in_bits)
		{
			case 8:  armEmitCall((void*)vtlb_memWrite<mem8_t>);  break;
			case 16: armEmitCall((void*)vtlb_memWrite<mem16_t>); break;
			case 32: armEmitCall((void*)vtlb_memWrite<mem32_t>); break;
			case 64: armEmitCall((void*)vtlb_memWrite<mem64_t>); break;
			default: pxFailRel("Unsupported store size in backpatch"); break;
		}
		armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

		armAsm->Bind(&done);
	}

	// Restore GPRs from stack
	offset = 0;
	for (u32 i = 0; i < num_gprs; i++)
	{
		armAsm->Ldr(a64::XRegister(gprs_to_save[i]), a64::MemOperand(a64::sp, offset));
		offset += 8;
	}

	// Restore NEON regs from stack
	for (u32 i = 0; i < num_fprs; i++)
	{
		armAsm->Ldr(a64::QRegister(fprs_to_save[i]), a64::MemOperand(a64::sp, offset));
		offset += 16;
	}

	if (stack_size > 0)
		armAsm->Add(a64::sp, a64::sp, stack_size);

	// Branch back to the instruction after the faulting load/store
	armEmitJmp((void*)(code_address + code_size));

	u8* thunk_end = recEndThunk();

	// Flush instruction cache for the ENTIRE thunk.
	// ARM64 icache is not coherent with dcache — without this, the CPU may
	// execute stale instructions from previously compiled code at the thunk's
	// address, causing SIGILL or corruption.
	HostSys::FlushInstructionCache(thunk, static_cast<u32>(thunk_end - thunk));

	// Patch the faulting instruction with a B (branch) to the thunk.
	// ARM64 B instruction: 0x14000000 | imm26, where imm26 = byte_offset / 4
	const s64 branch_offset = static_cast<s64>(thunk - reinterpret_cast<u8*>(code_address));
	pxAssert((branch_offset & 3) == 0);
	const s64 branch_imm26 = branch_offset >> 2;
	pxAssertRel(branch_imm26 >= -0x2000000 && branch_imm26 <= 0x1FFFFFF,
		"Backpatch thunk too far from faulting instruction for B instruction");

	HostSys::BeginCodeWrite();
	u32* patch_ptr = reinterpret_cast<u32*>(code_address);
	*patch_ptr = 0x14000000u | (static_cast<u32>(branch_imm26) & 0x03FFFFFFu);
	HostSys::EndCodeWrite();

	// Flush icache at the patch point too.
	HostSys::FlushInstructionCache(reinterpret_cast<void*>(code_address), 4);
}

// vuJITFreeze() is defined in microVU-arm64.cpp
