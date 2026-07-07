// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE VTLB Dynamic Code Generation
//
// Two paths for load/store instructions:
//
// 1. Fastmem (primary): Single LDR/STR via RFASTMEMBASE (x19).
//    The fastmem area is a 4GB region mapped to PS2 memory. If the page
//    is unmapped (MMIO, etc.), the SIGSEGV handler backpatches the faulting
//    instruction with a branch to a slow-path thunk.
//
// 2. Softmem (fallback): Inline VTLB page table lookup. Used when fastmem
//    is disabled or when a PC has previously faulted (vtlb_IsFaultingPC).

// FORCE_INTERP_MEMORY is defined in iR5900-arm64.h to force interpreter fallback

#include "arm64/iR5900-arm64.h"
#include "arm64/AsmHelpers.h"
#include "vtlb.h"
#include "VU.h"
#include "Hw.h"
#include "Memory.h"
#include "common/Assertions.h"

extern void vu0Sync();

namespace a64 = vixl::aarch64;

using namespace vtlb_private;


// =====================================================================================================
//  Softmem — Inline VTLB Lookup (fallback for faulting PCs)
// =====================================================================================================

// Generates inline vtlb read code. Result in w0/x0.
//
// Algorithm:
//   vmv = vmap[addr >> VTLB_PAGE_BITS]    (page table lookup)
//   ppf = addr + vmv                       (combine with mapping)
//   if (ppf >= 0) result = *(DataType*)ppf (fast path: direct read)
//   else call vtlb_memRead(addr)           (slow path: handler dispatch)
//
// addr_reg: ARM64 W register index containing the EE virtual address
// Clobbers: w0, x0, x8, x9, x17
// Result: in w0/x0
static void vtlbSoftmemRead(int addr_wreg, u32 bits, bool sign)
{
	pxAssert(bits == 8 || bits == 16 || bits == 32 || bits == 64);

	// Save the original address in w9 (needed for slow path)
	if (addr_wreg != 9)
		armAsm->Mov(a64::w9, armWRegister(addr_wreg));

	// Page index: w8 = addr >> VTLB_PAGE_BITS
	armAsm->Lsr(a64::w8, a64::w9, VTLB_PAGE_BITS);

	// Load vmap base address into x17
	armMoveAddressToReg(RSCRATCHADDR, vtlbdata.vmap);

	// Load vmap entry: x8 = vmap[page_index] (each entry is 8 bytes = sptr)
	armAsm->Ldr(a64::x8, a64::MemOperand(RSCRATCHADDR, a64::x8, a64::LSL, 3));

	// Compute ppf: x0 = addr + vmv (use 64-bit add, addr zero-extended from w9).
	// ADDS sets N from bit 63 of the result, so B.mi handles the slow-path
	// branch on the sign bit without a separate Tbnz.
	armAsm->Adds(a64::x0, a64::x8, a64::Operand(a64::w9, a64::UXTW));

	a64::Label slow_path, done;
	armAsm->B(&slow_path, a64::mi);

	// --- Fast path: direct memory read from host pointer ppf ---
	switch (bits)
	{
		case 8:
			if (sign)
				armAsm->Ldrsb(a64::x0, a64::MemOperand(a64::x0));
			else
				armAsm->Ldrb(a64::w0, a64::MemOperand(a64::x0));
			break;
		case 16:
			if (sign)
				armAsm->Ldrsh(a64::x0, a64::MemOperand(a64::x0));
			else
				armAsm->Ldrh(a64::w0, a64::MemOperand(a64::x0));
			break;
		case 32:
			if (sign)
				armAsm->Ldrsw(a64::x0, a64::MemOperand(a64::x0));
			else
				armAsm->Ldr(a64::w0, a64::MemOperand(a64::x0));
			break;
		case 64:
			armAsm->Ldr(a64::x0, a64::MemOperand(a64::x0));
			break;
	}
	armAsm->B(&done);

	// --- Slow path: call vtlb_memRead<DataType>(addr) ---
	armAsm->Bind(&slow_path);
	armAsm->Mov(a64::w0, a64::w9); // restore original address as argument

	// Spill/reload RECCYCLE around the handler call: page-0F INTC_STAT
	// reads invoke IntCHackCheck which mutates cpuRegs.cycle. Without
	// this the JIT's pinned x25 stays stale and block-end cycle compare
	// never trips on tight INTC polls.
	armFlushCycleDelta();
	switch (bits)
	{
		case 8:  armEmitCall((void*)vtlb_memRead<mem8_t>);  break;
		case 16: armEmitCall((void*)vtlb_memRead<mem16_t>); break;
		case 32: armEmitCall((void*)vtlb_memRead<mem32_t>); break;
		case 64: armEmitCall((void*)vtlb_memRead<mem64_t>); break;
	}
	armReloadCycleDelta();
	// preserve_most spares x9-x15 but never x0-x8 — restore the rung-3
	// x4-x7 pins the call clobbered (x0 result is untouched).
	armReloadEEClobberedPins();
	// Sign-extend if needed (vtlb_memRead returns zero-extended)
	if (sign && bits == 8)
		armAsm->Sxtb(a64::x0, a64::w0);
	else if (sign && bits == 16)
		armAsm->Sxth(a64::x0, a64::w0);
	else if (sign && bits == 32)
		armAsm->Sxtw(a64::x0, a64::w0);

	armAsm->Bind(&done);
}

// Generates inline vtlb write code.
// addr_reg: W register index with EE virtual address
// value_reg: W/X register index with value to write
// Clobbers: w0, x0, w1, x1, x8, x9, x17
static void vtlbSoftmemWrite(int addr_wreg, int value_reg, u32 bits)
{
	pxAssert(bits == 8 || bits == 16 || bits == 32 || bits == 64);

	if (addr_wreg != 9)
		armAsm->Mov(a64::w9, armWRegister(addr_wreg));
	if (value_reg != 10)
	{
		if (bits <= 32)
			armAsm->Mov(a64::w10, armWRegister(value_reg));
		else
			armAsm->Mov(a64::x10, armXRegister(value_reg));
	}

	armAsm->Lsr(a64::w8, a64::w9, VTLB_PAGE_BITS);
	armMoveAddressToReg(RSCRATCHADDR, vtlbdata.vmap);
	armAsm->Ldr(a64::x8, a64::MemOperand(RSCRATCHADDR, a64::x8, a64::LSL, 3));
	// ADDS sets N from bit 63 of ppf; B.mi (=N) branches on the sign bit
	// without the separate Tbnz, saving one instruction per softmem op.
	armAsm->Adds(a64::x0, a64::x8, a64::Operand(a64::w9, a64::UXTW));

	a64::Label slow_path, done;
	armAsm->B(&slow_path, a64::mi);

	// --- Fast path: direct memory write ---
	switch (bits)
	{
		case 8:  armAsm->Strb(a64::w10, a64::MemOperand(a64::x0)); break;
		case 16: armAsm->Strh(a64::w10, a64::MemOperand(a64::x0)); break;
		case 32: armAsm->Str(a64::w10, a64::MemOperand(a64::x0)); break;
		case 64: armAsm->Str(a64::x10, a64::MemOperand(a64::x0)); break;
	}
	armAsm->B(&done);

	// --- Slow path: call vtlb_memWrite ---
	armAsm->Bind(&slow_path);
	armAsm->Mov(a64::w0, a64::w9);
	if (bits <= 32)
		armAsm->Mov(a64::w1, a64::w10);
	else
		armAsm->Mov(a64::x1, a64::x10);

	// Spill/reload RECCYCLE: write-side handlers are symmetric to reads —
	// any cycle-mutating handler reachable from MMIO must keep the JIT's
	// pinned x25 coherent. See vtlbSoftmemRead for full rationale.
	armFlushCycleDelta();
	switch (bits)
	{
		case 8:  armEmitCall((void*)vtlb_memWrite<mem8_t>);  break;
		case 16: armEmitCall((void*)vtlb_memWrite<mem16_t>); break;
		case 32: armEmitCall((void*)vtlb_memWrite<mem32_t>); break;
		case 64: armEmitCall((void*)vtlb_memWrite<mem64_t>); break;
	}
	armReloadCycleDelta();
	// x4-x7 pin restore — see vtlbSoftmemRead.
	armReloadEEClobberedPins();

	armAsm->Bind(&done);
}

// =====================================================================================================
//  Load/Store Instruction Implementations
// =====================================================================================================

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

// =====================================================================================================
//  Fastmem helpers
// =====================================================================================================

// Build bitmasks of currently allocated ARM64 GPR and NEON registers.
// The backpatch thunk uses these to save/restore live registers around
// the vtlb C call, preventing corruption of JIT register allocator state.
static void vtlbGetLiveRegisterMasks(u32& gpr_bitmask, u32& fpr_bitmask)
{
	gpr_bitmask = 0;
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse)
			gpr_bitmask |= (1u << i);
	}

	fpr_bitmask = 0;
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse)
			fpr_bitmask |= (1u << i);
	}
}

// Emit a single fastmem load instruction and register backpatch info.
// addr_wreg: W register index holding the 32-bit guest virtual address
// dest_reg: register index where the result goes (W or X based on bits)
// Result is in dest_reg after the load (or after backpatch thunk on fault).
static void vtlbFastmemRead(int addr_wreg, int dest_reg, u32 bits, bool sign)
{
	u32 gpr_bitmask, fpr_bitmask;
	vtlbGetLiveRegisterMasks(gpr_bitmask, fpr_bitmask);

	const u8* codeStart = armGetCurrentCodePointer();

	a64::MemOperand mem(RFASTMEMBASE, armWRegister(addr_wreg), a64::UXTW);
	switch (bits)
	{
		case 8:
			if (sign)
				armAsm->Ldrsb(armXRegister(dest_reg), mem);
			else
				armAsm->Ldrb(armWRegister(dest_reg), mem);
			break;
		case 16:
			if (sign)
				armAsm->Ldrsh(armXRegister(dest_reg), mem);
			else
				armAsm->Ldrh(armWRegister(dest_reg), mem);
			break;
		case 32:
			if (sign)
				armAsm->Ldrsw(armXRegister(dest_reg), mem);
			else
				armAsm->Ldr(armWRegister(dest_reg), mem);
			break;
		case 64:
			armAsm->Ldr(armXRegister(dest_reg), mem);
			break;
	}

	vtlb_AddLoadStoreInfo((uptr)codeStart, 4, pc, gpr_bitmask, fpr_bitmask,
		static_cast<u8>(addr_wreg), static_cast<u8>(dest_reg),
		static_cast<u8>(bits), sign, true, false);
}

// Emit a single fastmem store instruction and register backpatch info.
static void vtlbFastmemWrite(int addr_wreg, int value_reg, u32 bits)
{
	u32 gpr_bitmask, fpr_bitmask;
	vtlbGetLiveRegisterMasks(gpr_bitmask, fpr_bitmask);

	const u8* codeStart = armGetCurrentCodePointer();

	a64::MemOperand mem(RFASTMEMBASE, armWRegister(addr_wreg), a64::UXTW);
	switch (bits)
	{
		case 8:  armAsm->Strb(armWRegister(value_reg), mem); break;
		case 16: armAsm->Strh(armWRegister(value_reg), mem); break;
		case 32: armAsm->Str(armWRegister(value_reg), mem);  break;
		case 64: armAsm->Str(armXRegister(value_reg), mem);  break;
	}

	vtlb_AddLoadStoreInfo((uptr)codeStart, 4, pc, gpr_bitmask, fpr_bitmask,
		static_cast<u8>(addr_wreg), static_cast<u8>(value_reg),
		static_cast<u8>(bits), false, false, false);
}

// Emit a single 128-bit fastmem load (LDR Q0, [RFASTMEMBASE, w_addr, UXTW]).
// Result in q0. Mirrors x86 PCSX2's MOVAPS-via-RFASTMEMBASE pattern
// (ix86-32/recVTLB.cpp). Backpatch thunk extended in RecStubs.cpp.
static void vtlbFastmemRead128(int addr_wreg)
{
	u32 gpr_bitmask, fpr_bitmask;
	vtlbGetLiveRegisterMasks(gpr_bitmask, fpr_bitmask);

	const u8* codeStart = armGetCurrentCodePointer();
	armAsm->Ldr(a64::q0, a64::MemOperand(RFASTMEMBASE, armWRegister(addr_wreg), a64::UXTW));

	vtlb_AddLoadStoreInfo((uptr)codeStart, 4, pc, gpr_bitmask, fpr_bitmask,
		static_cast<u8>(addr_wreg), /*data_register*/ 0,
		/*size_in_bits*/ 128, /*is_signed*/ false, /*is_load*/ true, /*is_fpr*/ true);
}

// Emit a single 128-bit fastmem store (STR Q0, [RFASTMEMBASE, w_addr, UXTW]).
// Value in q0. Backpatch thunk extended in RecStubs.cpp.
static void vtlbFastmemWrite128(int addr_wreg)
{
	u32 gpr_bitmask, fpr_bitmask;
	vtlbGetLiveRegisterMasks(gpr_bitmask, fpr_bitmask);

	const u8* codeStart = armGetCurrentCodePointer();
	armAsm->Str(a64::q0, a64::MemOperand(RFASTMEMBASE, armWRegister(addr_wreg), a64::UXTW));

	vtlb_AddLoadStoreInfo((uptr)codeStart, 4, pc, gpr_bitmask, fpr_bitmask,
		static_cast<u8>(addr_wreg), /*data_register*/ 0,
		/*size_in_bits*/ 128, /*is_signed*/ false, /*is_load*/ false, /*is_fpr*/ true);
}

// =====================================================================================================
//  Address computation helpers
// =====================================================================================================

// Compute load/store address (rs + imm) into w9.
// Does NOT flush — reads from wherever the value currently lives
// (const propagation, ARM64 GPR, NEON register, or cpuRegs memory).
// For const Rs, the full address is computed at compile time.
static void recComputeAddr()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		armAsm->Mov(a64::w9, g_cpuConstRegs[_Rs_].UL[0] + _Imm_);
	}
	else
	{
		_eeMoveGPRtoR(a64::w9, _Rs_);
		if (_Imm_ != 0)
			armAsm->Add(a64::w9, a64::w9, _Imm_);
	}
}

// Prepare store value in w10/x10.
// Does NOT flush — reads from wherever the value currently lives.
static void recPrepStoreValue(u32 bits)
{
	_eeMoveGPRtoR(bits <= 32 ? a64::w10 : a64::x10, _Rt_);
}

// Store the load result to guest register Rt.
// Invalidates any existing allocation for Rt (const/GPR/NEON) and
// writes the result to cpuRegs memory. `result` defaults to x0 (the C-call /
// softmem convention); the fastmem pinned-dest path (WS-C4) loads straight
// into the pin and passes it here — armStoreEERegPtr then emits just the
// canonical Str (the mirror Mov self-skips when src IS the pin).
static void recStoreLoadResult(const a64::Register& result = a64::x0)
{
	if (_Rt_)
	{
		_deleteEEreg(_Rt_, 0);
		GPR_DEL_CONST(_Rt_);
		armStoreEERegPtr(result, &cpuRegs.GPR.r[_Rt_].UD[0]);
	}
}

// =====================================================================================================
//  Load implementations
// =====================================================================================================

// Const-paddr MMIO shortcut. When Rs is constant and the resolved page is a
// handler (MMIO), emit a direct BL to the registered handler instead of going
// through fastmem-fault → backpatch thunk → vtlb_memRead → page-table dispatch.
// Mirrors x86 vtlb_DynGenReadNonQuad_Const (ix86-32/recVTLB.cpp).
//
// Direct (RAM-backed) const-paddr loads stay on the fastmem path — a single
// LDR off RFASTMEMBASE is already optimal for those.
//
// Returns true if the shortcut emitted the load; caller should bail out.
static bool recLoadConstPaddrMMIOShortcut(u32 bits, bool sign)
{
	if (!GPR_IS_CONST1(_Rs_))
		return false;

	const u32 addr_const = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
	const auto vmv = vtlbdata.vmap[addr_const >> VTLB_PAGE_BITS];
	if (!vmv.isHandler(addr_const))
		return false;

	const u32 paddr = vmv.assumeHandlerGetPAddr(addr_const);

	iFlushCall(FLUSH_INTERPRETER);

	// INTC_STAT inline-load when the speedhack is disabled. With it on
	// (the default), fall through to a direct BL of the registered
	// hwRead32_page_0F_INTC_HACK handler.
	if (bits == 32 && !EmuConfig.Speedhacks.IntcStat && paddr == INTC_STAT)
	{
		armLoadPtr(a64::w0, &psHu32(INTC_STAT));
		if (sign)
			armAsm->Sxtw(a64::x0, a64::w0);
		recStoreLoadResult();
		return true;
	}

	int szidx = 0;
	switch (bits)
	{
		case 8:  szidx = 0; break;
		case 16: szidx = 1; break;
		case 32: szidx = 2; break;
		case 64: szidx = 3; break;
	}
	armAsm->Mov(a64::w0, paddr);
	// Spill/reload RECCYCLE around the registered handler: the const-paddr
	// MMIO shortcut targets the same handler set as vtlbSoftmemRead's slow
	// path, including page-0F INTC_STAT → IntCHackCheck which mutates
	// cpuRegs.cycle. See vtlbSoftmemRead for full rationale.
	armFlushCycleDelta();
	armEmitCall(vmv.assumeHandlerGetRaw(szidx, false));
	armReloadCycleDelta();
	// Raw registered handlers are plain AAPCS (not preserve_most like the
	// vtlb_memRead/Write dispatchers) and write no guest GPRs — restore the
	// caller-saved pins they clobbered. x0 (the handler result) is untouched.
	armReloadEEClobberedPins();

	// Extend handler return value into x0 for the 64-bit cpuRegs.GPR store.
	// AAPCS64 leaves the upper bits of x0 unspecified for sub-word returns.
	if (bits < 64)
	{
		if (sign)
		{
			switch (bits)
			{
				case 8:  armAsm->Sxtb(a64::x0, a64::w0); break;
				case 16: armAsm->Sxth(a64::x0, a64::w0); break;
				case 32: armAsm->Sxtw(a64::x0, a64::w0); break;
			}
		}
		else
		{
			armAsm->Uxtw(a64::x0, a64::w0);
		}
	}

	recStoreLoadResult();
	return true;
}

// Generic load: fastmem (primary) with softmem fallback for faulting PCs.
//
// Fastmem path: no flush — the backpatch thunk in RecStubs.cpp saves/
// restores live regs around the slow-path C call.
//
// Softmem path: FLUSH_CONSTANT_REGS only. iFlushCall already evicts all
// caller-saved GPRs + NEON unconditionally; constants must additionally
// be written back so post-call emit re-reads from cpuRegs.GPR rather than
// trusting now-stale const tracking. PC and CODE are not load-bearing —
// vtlb_memRead<T> doesn't read them, and exception handlers that fire
// from the slow path stash their own PC.
static void recLoad(u32 bits, bool sign)
{
	// Force an event test on EE counter-range reads (the EE timers at
	// 0x10000000..0x10001FFF) to improve read + interrupt syncing — namely
	// ESPN Games. Follows the upstream x86-master fix
	// (iR5900LoadStore.cpp: needs_flush → iFlushCall(FLUSH_INTERPRETER) +
	// g_branch=2). Setting g_branch=2 makes the block finalizer end the block
	// with the event-test exit (it does the FLUSH_EVERYTHING + cycle accumulate +
	// nextEventCycle dispatch itself), so no explicit iFlushCall is needed here.
	bool forceEventTest = false;
	if (GPR_IS_CONST1(_Rs_) && bits <= 32)
	{
		const u32 srcadr = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
		forceEventTest = (srcadr & 0xFFFFE000) == 0x10000000;
	}

	if (recLoadConstPaddrMMIOShortcut(bits, sign))
	{
		if (forceEventTest)
			g_branch = 2;
		return;
	}

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	unsigned addr_reg = 9;
	if (GPR_IS_CONST1(_Rs_))
	{
		iFlushCall(FLUSH_CONSTANT_REGS);
		armAsm->Mov(a64::w9, g_cpuConstRegs[_Rs_].UL[0] + _Imm_);
	}
	else if (const a64::Register* pin = armEEPinForGPR(_Rs_))
	{
		// Pinned base (EE-SRA 2 WS-C2): pins survive iFlushCall (they are not
		// allocator state), and the flush is precisely what write-backs any
		// dirty NEON dual-residence copy into the pin — so form the address
		// AFTER the flush, from the pin. _Imm_==0 skips even the Add: the pin
		// itself is the fastmem index / softmem address (the backpatch info
		// records the address register per site; RecStubs normalizes it).
		iFlushCall(FLUSH_CONSTANT_REGS);
		if (_Imm_ != 0)
			armAsm->Add(a64::w9, pin->W(), _Imm_);
		else
			addr_reg = static_cast<unsigned>(pin->GetCode());
	}
	else
	{
		_eeMoveGPRtoR(a64::w9, _Rs_);
		iFlushCall(FLUSH_CONSTANT_REGS);
		if (_Imm_ != 0)
			armAsm->Add(a64::w9, a64::w9, _Imm_);
	}

	if (useFastmem)
	{
		// Pinned dest (WS-C4): load straight into the pin — the recorded
		// data_register makes the backpatch thunk reconstruct into it on a
		// fault (after armReloadEEClobberedPins, so caller-saved pins get the
		// load result last). Kills the Mov pin<-x0 mirror refresh; the
		// canonical Str in recStoreLoadResult stays. Not done for softmem —
		// its result comes back from a C call in x0.
		const a64::Register* dpin = _Rt_ ? armEEPinForGPR(_Rt_) : nullptr;
		vtlbFastmemRead(addr_reg, dpin ? static_cast<unsigned>(dpin->GetCode()) : 0, bits, sign);
		recStoreLoadResult(dpin ? *dpin : a64::Register(a64::x0));
	}
	else
	{
		vtlbSoftmemRead(addr_reg, bits, sign);
		recStoreLoadResult();
	}

	if (forceEventTest)
		g_branch = 2;
}

#ifdef FORCE_INTERP_MEMORY
REC_FUNC(LB);  REC_FUNC(LBU); REC_FUNC(LH);  REC_FUNC(LHU);
REC_FUNC(LW);  REC_FUNC(LWU); REC_FUNC(LD);
#else
void recLB()  { recLoad(8,  true);  }
void recLBU() { recLoad(8,  false); }
void recLH()  { recLoad(16, true);  }
void recLHU() { recLoad(16, false); }
void recLW()  { recLoad(32, true);  }
void recLWU() { recLoad(32, false); }
void recLD()  { recLoad(64, false); }
#endif

// =====================================================================================================
//  Store implementations
// =====================================================================================================

// Symmetric to recLoadConstPaddrMMIOShortcut: when Rs is constant and the
// resolved page is a handler (MMIO), emit a direct BL to the registered write
// handler instead of going through fastmem-fault → backpatch thunk →
// vtlb_memWrite → page-table dispatch. Mirrors x86 vtlb_DynGenWrite_Const
// (ix86-32/recVTLB.cpp).
//
// Direct (RAM-backed) const-paddr stores stay on the fastmem path — a single
// STR off RFASTMEMBASE is already optimal for those.
//
// Returns true if the shortcut emitted the store; caller should bail out.
static bool recStoreConstPaddrMMIOShortcut(u32 bits)
{
	if (!GPR_IS_CONST1(_Rs_))
		return false;

	const u32 addr_const = g_cpuConstRegs[_Rs_].UL[0] + _Imm_;
	const auto vmv = vtlbdata.vmap[addr_const >> VTLB_PAGE_BITS];
	if (!vmv.isHandler(addr_const))
		return false;

	const u32 paddr = vmv.assumeHandlerGetPAddr(addr_const);

	iFlushCall(FLUSH_INTERPRETER);

	int szidx = 0;
	switch (bits)
	{
		case 8:  szidx = 0; break;
		case 16: szidx = 1; break;
		case 32: szidx = 2; break;
		case 64: szidx = 3; break;
	}

	// AAPCS64: w0 = paddr, w1/x1 = value. After FLUSH_INTERPRETER (0xfff)
	// all guest reg state is in memory, so armLoadEERegPtr is correct for
	// both const and non-const Rt (including Rt == 0).
	armAsm->Mov(a64::w0, paddr);
	if (bits <= 32)
		armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rt_].UL[0]);
	else
		armLoadEERegPtr(a64::x1, &cpuRegs.GPR.r[_Rt_].UD[0]);

	// RECCYCLE coherence — same rationale as recLoadConstPaddrMMIOShortcut.
	armFlushCycleDelta();
	armEmitCall(vmv.assumeHandlerGetRaw(szidx, true));
	armReloadCycleDelta();
	// Caller-saved pin restore — same rationale as the read shortcut.
	armReloadEEClobberedPins();

	return true;
}

static void recStore(u32 bits)
{
	if (recStoreConstPaddrMMIOShortcut(bits))
		return;

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	// Flush rationale: see recLoad. FLUSH_CONSTANT_REGS only.
	unsigned value_reg = 10;
	if (GPR_IS_CONST1(_Rt_))
	{
		if (bits <= 32)
			armAsm->Mov(a64::w10, g_cpuConstRegs[_Rt_].UL[0]);
		else
			armAsm->Mov(a64::x10, g_cpuConstRegs[_Rt_].UD[0]);
	}
	else if (const a64::Register* vpin = armEEPinForGPR(_Rt_))
	{
		// Pinned store value (WS-C3): the pin IS the value register — consumed
		// by the fastmem Str / softmem call after iFlushCall below, where the
		// pin is coherent. The per-site backpatch info records it; the thunk
		// and softmem normalize into w10/x10 read-only.
		value_reg = static_cast<unsigned>(vpin->GetCode());
	}
	else
	{
		_eeMoveGPRtoR(bits <= 32 ? a64::w10 : a64::x10, _Rt_);
	}

	unsigned addr_reg = 9;
	if (GPR_IS_CONST1(_Rs_))
	{
		iFlushCall(FLUSH_CONSTANT_REGS);
		armAsm->Mov(a64::w9, g_cpuConstRegs[_Rs_].UL[0] + _Imm_);
	}
	else if (const a64::Register* pin = armEEPinForGPR(_Rs_))
	{
		// Pinned base — post-flush pin read; see recLoad. (WS-C2)
		iFlushCall(FLUSH_CONSTANT_REGS);
		if (_Imm_ != 0)
			armAsm->Add(a64::w9, pin->W(), _Imm_);
		else
			addr_reg = static_cast<unsigned>(pin->GetCode());
	}
	else
	{
		_eeMoveGPRtoR(a64::w9, _Rs_);
		iFlushCall(FLUSH_CONSTANT_REGS);
		if (_Imm_ != 0)
			armAsm->Add(a64::w9, a64::w9, _Imm_);
	}

	if (useFastmem)
	{
		vtlbFastmemWrite(addr_reg, value_reg, bits);
	}
	else
	{
		vtlbSoftmemWrite(addr_reg, value_reg, bits);
	}
}

#ifdef FORCE_INTERP_MEMORY
REC_FUNC(SB); REC_FUNC(SH); REC_FUNC(SW); REC_FUNC(SD);
#else
void recSB() { recStore(8);  }
void recSH() { recStore(16); }
void recSW() { recStore(32); }
void recSD() { recStore(64); }
#endif

// =====================================================================================================
//  LQ / SQ — 128-bit quad load/store
//  addr = (rs + imm) & ~0xF (silently aligned to 16 bytes)
// =====================================================================================================

// Inline VTLB 128-bit read. Result in q0.
static void vtlbSoftmemRead128(int addr_wreg)
{
	if (addr_wreg != 9)
		armAsm->Mov(a64::w9, armWRegister(addr_wreg));

	armAsm->Lsr(a64::w8, a64::w9, VTLB_PAGE_BITS);
	armMoveAddressToReg(RSCRATCHADDR, vtlbdata.vmap);
	armAsm->Ldr(a64::x8, a64::MemOperand(RSCRATCHADDR, a64::x8, a64::LSL, 3));
	// ADDS sets N from bit 63 of ppf; B.mi (=N) branches on the sign bit
	// without the separate Tbnz, saving one instruction per softmem op.
	armAsm->Adds(a64::x0, a64::x8, a64::Operand(a64::w9, a64::UXTW));

	a64::Label slow_path, done;
	armAsm->B(&slow_path, a64::mi);

	// Fast path: LDR q0, [x0]
	armAsm->Ldr(a64::q0, a64::MemOperand(a64::x0));
	armAsm->B(&done);

	// Slow path: call vtlb_memRead128(addr) — returns r128 in q0
	armAsm->Bind(&slow_path);
	armAsm->Mov(a64::w0, a64::w9);
	// See vtlbSoftmemRead for the RECCYCLE coherence rationale.
	armFlushCycleDelta();
	armEmitCall((void*)vtlb_memRead128);
	armReloadCycleDelta();
	// x4-x7 pin restore — see vtlbSoftmemRead (q0 result untouched).
	armReloadEEClobberedPins();

	armAsm->Bind(&done);
}

// Inline VTLB 128-bit write. Value in q0.
static void vtlbSoftmemWrite128(int addr_wreg)
{
	if (addr_wreg != 9)
		armAsm->Mov(a64::w9, armWRegister(addr_wreg));

	armAsm->Lsr(a64::w8, a64::w9, VTLB_PAGE_BITS);
	armMoveAddressToReg(RSCRATCHADDR, vtlbdata.vmap);
	armAsm->Ldr(a64::x8, a64::MemOperand(RSCRATCHADDR, a64::x8, a64::LSL, 3));
	// ADDS sets N from bit 63 of ppf; B.mi (=N) branches on the sign bit
	// without the separate Tbnz, saving one instruction per softmem op.
	armAsm->Adds(a64::x0, a64::x8, a64::Operand(a64::w9, a64::UXTW));

	a64::Label slow_path, done;
	armAsm->B(&slow_path, a64::mi);

	// Fast path: STR q0, [x0]
	armAsm->Str(a64::q0, a64::MemOperand(a64::x0));
	armAsm->B(&done);

	// Slow path: call vtlb_memWrite128(addr, value)
	// addr in w0, value in q0 (ARM64 ABI: 128-bit passed in q0)
	armAsm->Bind(&slow_path);
	armAsm->Mov(a64::w0, a64::w9);
	// See vtlbSoftmemRead for the RECCYCLE coherence rationale.
	armFlushCycleDelta();
	armEmitCall((void*)vtlb_memWrite128);
	armReloadCycleDelta();
	// x4-x7 pin restore — see vtlbSoftmemRead.
	armReloadEEClobberedPins();

	armAsm->Bind(&done);
}

void recLQ()
{
	// addr = (rs + imm) & ~0xF — compute from live registers, then flush
	recComputeAddr();
	armAsm->And(a64::w9, a64::w9, (u32)~0xF);
	iFlushCall(FLUSH_CONSTANT_REGS);

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);
	if (useFastmem)
		vtlbFastmemRead128(9);
	else
		vtlbSoftmemRead128(9);

	// Store full 128-bit result to GPR[rt] via memory
	if (_Rt_)
	{
		_deleteEEreg(_Rt_, 0);
		GPR_DEL_CONST(_Rt_);
		armStoreEEGPRQuad(a64::q0, _Rt_);
	}
}

void recSQ()
{
	// Flush rationale: see recLoad. FLUSH_CONSTANT_REGS only.
	// Rt and Rs are read from memory after the flush. iFlushCall has
	// freed all NEON unconditionally (writeback-on-dirty), so any EE
	// GPR allocated in NEON is in memory. EE GPRs in arm64gprs[] are
	// not written back by FLUSH_CONSTANT_REGS — relies on allocator
	// preferring NEON for full-128-bit guest GPRs.
	iFlushCall(FLUSH_CONSTANT_REGS);
	armLoadEERegPtr(a64::q0, &cpuRegs.GPR.r[_Rt_].UQ);

	armLoadEERegPtr(a64::w9, &cpuRegs.GPR.r[_Rs_].UL[0]);
	if (_Imm_ != 0)
		armAsm->Add(a64::w9, a64::w9, _Imm_);
	armAsm->And(a64::w9, a64::w9, (u32)~0xF);

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);
	if (useFastmem)
		vtlbFastmemWrite128(9);
	else
		vtlbSoftmemWrite128(9);
}

// =====================================================================================================
//  LWC1 / SWC1 — FPU 32-bit load/store
//  LWC1: fpr[ft] = mem32(rs + imm)
//  SWC1: mem32(rs + imm) = fpr[ft]
// =====================================================================================================

void recLWC1()
{
	// On the fast path a single inline LDR off RFASTMEMBASE + backpatch,
	// no iFlushCall and no vtlb C call. The result lands in w0 (a plain GPR
	// rather than an allocated FPR host reg). Softmem stays as the
	// faulting-PC fallback.
	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	// Compute address into w9 from live registers.
	recComputeAddr();

	if (useFastmem)
	{
		vtlbFastmemRead(9, 0, 32, false);
	}
	else
	{
		iFlushCall(FLUSH_CONSTANT_REGS);
		vtlbSoftmemRead(9, 32, false);
	}

	// fpr[ft] in memory is about to be overwritten; the allocator's slot
	// (if any) is now stale and must not flush back over the write.
	_deleteFPtoNEONreg(_Rt_, DELETE_REG_FREE_NO_WRITEBACK);
	// Store to fpuRegs.fpr[ft]
	armStoreEERegPtr(a64::w0, &fpuRegs.fpr[_Rt_].UL);
}

void recSWC1()
{
	// fpr[ft] may be live in NEON with MODE_WRITE-only state; flush dirty
	// content to memory and drop the slot before reading via armLoad.
	_deleteFPtoNEONreg(_Rt_, DELETE_REG_FLUSH_AND_FREE);

	// Load FPU register value into w10
	armLoadEERegPtr(a64::w10, &fpuRegs.fpr[_Rt_].UL);

	// Inline STR off RFASTMEMBASE + backpatch on the fast path, softmem
	// fallback otherwise.
	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	// Compute address from live registers.
	recComputeAddr();

	if (useFastmem)
	{
		vtlbFastmemWrite(9, 10, 32);
	}
	else
	{
		iFlushCall(FLUSH_CONSTANT_REGS);
		vtlbSoftmemWrite(9, 10, 32);
	}
}

// =====================================================================================================
//  Unaligned load/store (LWL/LWR/LDL/LDR/SWL/SWR/SDL/SDR) — inline fastmem
//  read-modify-write codegen. Mirrors x86 master's REC_LOADS/REC_STORES paths:
//  the inline path needs only FLUSH_CONSTANT_REGS plus fastmem accesses
//  (no C call on the fast path), avoiding a full FLUSH_INTERPRETER eviction.
// =====================================================================================================

// Inline LWL/LWR codegen. Mirrors x86 recLWL/recLWR (ix86-32/iR5900LoadStore.cpp).
//
//   addr    = Rs + imm
//   shift8  = (addr & 3) * 8     // kept in a callee-saved temp across the read
//   aligned = addr & ~3
//   loaded  = mem32(aligned)
//
//   LWL: Rt = sign_ext_32_to_64( (Rt & (0xffffff   >> shift8)) | (loaded << (24 - shift8)) )
//   LWR: if shift8 == 0: Rt = sign_ext_32_to_64(loaded)
//        else           : Rt[31:0] = (Rt[31:0] & (0xffffff00 << (24 - shift8))) | (loaded >> shift8)
//                         Rt[63:32] preserved   (see interpreter LWL/LWR in R5900OpcodeImpl.cpp)
//
// Uses fastmem when available (the backpatch thunk spills the live temp around
// its slow-path C call); softmem path's slow-path C call obeys AAPCS so the
// callee-saved temp survives there too.
static void recUnalignedWord(bool is_lwl)
{
	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	// Compute Rs+imm in w9. Mirrors recLoad: load Rs first, then flush, then add imm.
	_eeMoveGPRtoR(a64::w9, _Rs_);
	iFlushCall(FLUSH_CONSTANT_REGS);
	if (_Imm_ != 0)
		armAsm->Add(a64::w9, a64::w9, _Imm_);

	// shift8 and the loaded word both live in callee-saved temps. shift8 must
	// survive vtlb's slow-path C call (fastmem backpatch thunk OR softmem slow
	// path); the loaded word must survive the Rt alloc below — under register
	// pressure _allocArm64GPR can spill a guest reg or land Rt in x0, clobbering
	// w0 between the read and the merge. Same hazard fixed in
	// recUnalignedLoadDouble; single-op tests miss it (it needs the pressure).
	const int shift8 = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
	const int memTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);

	armAsm->And(armWRegister(shift8), a64::w9, 3);
	armAsm->Lsl(armWRegister(shift8), armWRegister(shift8), 3);
	armAsm->And(a64::w9, a64::w9, ~3u);

	// 32-bit aligned read; result in w0.
	if (useFastmem)
		vtlbFastmemRead(9, 0, 32, false);
	else
		vtlbSoftmemRead(9, 32, false);

	if (!_Rt_)
	{
		_freeArm64GPR(memTemp);
		_freeArm64GPR(shift8);
		return;
	}

	armAsm->Mov(armWRegister(memTemp), a64::w0);                       // park loaded (x0 unsafe across Rt alloc)

	const int rt = _allocArm64GPR(ARM64TYPE_GPR, _Rt_, MODE_READ | MODE_WRITE);

	if (is_lwl)
	{
		// mask = 0xffffff >> shift8
		armAsm->Mov(RWSCRATCH, 0xffffff);
		armAsm->Lsr(RWSCRATCH, RWSCRATCH, armWRegister(shift8));
		armAsm->And(armWRegister(rt), armWRegister(rt), RWSCRATCH);

		// shifted_loaded = loaded << (24 - shift8); reuse RWSCRATCH as shift amount.
		armAsm->Mov(RWSCRATCH, 24);
		armAsm->Sub(RWSCRATCH, RWSCRATCH, armWRegister(shift8));
		armAsm->Lsl(armWRegister(memTemp), armWRegister(memTemp), RWSCRATCH);

		// Merge and sign-extend the 32-bit result into the 64-bit guest reg.
		armAsm->Orr(armWRegister(rt), armWRegister(rt), armWRegister(memTemp));
		armAsm->Sxtw(armXRegister(rt), armWRegister(rt));
	}
	else
	{
		a64::Label nomask, done;
		armAsm->Cbz(armWRegister(shift8), &nomask);

		// mask = 0xffffff00 << (24 - shift8); held in RSCRATCHADDR.W() since RWSCRATCH carries the shift amount.
		armAsm->Mov(RWSCRATCH, 24);
		armAsm->Sub(RWSCRATCH, RWSCRATCH, armWRegister(shift8));
		armAsm->Mov(RSCRATCHADDR.W(), 0xffffff00u);
		armAsm->Lsl(RSCRATCHADDR.W(), RSCRATCHADDR.W(), RWSCRATCH);
		armAsm->And(RWSCRATCH, armWRegister(rt), RSCRATCHADDR.W());

		armAsm->Lsr(armWRegister(memTemp), armWRegister(memTemp), armWRegister(shift8));
		armAsm->Orr(armWRegister(memTemp), armWRegister(memTemp), RWSCRATCH);

		// Per interp: when shift8 != 0, only Rt[31:0] changes; upper 32 preserved.
		armAsm->Bfi(armXRegister(rt), armXRegister(memTemp), 0, 32);
		armAsm->B(&done);

		// shift8 == 0 (aligned): straight sign-extend, full 64-bit overwrite.
		armAsm->Bind(&nomask);
		armAsm->Sxtw(armXRegister(rt), armWRegister(memTemp));

		armAsm->Bind(&done);
	}

	_freeArm64GPR(memTemp);
	_freeArm64GPR(shift8);
}

// Inline SWL/SWR codegen (32-bit unaligned store, read-modify-write). Mirrors
// x86 recSWL/recSWR (ix86-32/iR5900LoadStore.cpp) and interp R5900OpcodeImpl.cpp SWL/SWR.
//
//   addr = Rs + imm ; shift8 = (addr & 3) * 8 ; aligned = addr & ~3
//   mem  = mem32(aligned)
//   SWL: mem32(aligned) = (Rt >> (24 - shift8)) | (mem & (0xffffff00 << shift8))
//   SWR: mem32(aligned) = (Rt <<       shift8 ) | (mem & (0x00ffffff >> (24 - shift8)))
//
// aligned and shift8 live in callee-saved temps so they survive the read's
// slow-path C call (fastmem backpatch thunk OR softmem slow path). The Rt load
// and merged value never cross a call. The implementation always performs a
// full RMW (no shift8==24 full-overwrite skip): the aligned word is the same
// one written, so reading it is always safe, and the general shifts already
// collapse to "store Rt" at that alignment.
static void recUnalignedStoreWord(bool is_swl)
{
	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	_eeMoveGPRtoR(a64::w9, _Rs_);
	iFlushCall(FLUSH_CONSTANT_REGS);
	if (_Imm_ != 0)
		armAsm->Add(a64::w9, a64::w9, _Imm_);

	const int addrTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
	const int shiftTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);

	armAsm->And(armWRegister(addrTemp), a64::w9, ~3u);                 // aligned addr
	armAsm->And(armWRegister(shiftTemp), a64::w9, 3);
	armAsm->Lsl(armWRegister(shiftTemp), armWRegister(shiftTemp), 3);  // shift8

	// 32-bit aligned read; mem -> w0.
	armAsm->Mov(a64::w9, armWRegister(addrTemp));
	if (useFastmem)
		vtlbFastmemRead(9, 0, 32, false);
	else
		vtlbSoftmemRead(9, 32, false);

	// Load Rt after the read (never crosses a call). Handles Rt==0 -> 0.
	_eeMoveGPRtoR(a64::w1, _Rt_);

	if (is_swl)
	{
		armAsm->Mov(RWSCRATCH, 0xffffff00u);
		armAsm->Lsl(RWSCRATCH, RWSCRATCH, armWRegister(shiftTemp));    // 0xffffff00 << shift8
		armAsm->And(a64::w0, a64::w0, RWSCRATCH);                      // mem & mask
		armAsm->Mov(RWSCRATCH, 24);
		armAsm->Sub(RWSCRATCH, RWSCRATCH, armWRegister(shiftTemp));    // 24 - shift8
		armAsm->Lsr(a64::w1, a64::w1, RWSCRATCH);                      // Rt >> (24 - shift8)
	}
	else
	{
		armAsm->Mov(RWSCRATCH, 24);
		armAsm->Sub(RWSCRATCH, RWSCRATCH, armWRegister(shiftTemp));    // 24 - shift8
		armAsm->Mov(RSCRATCHADDR.W(), 0x00ffffffu);
		armAsm->Lsr(RSCRATCHADDR.W(), RSCRATCHADDR.W(), RWSCRATCH);    // 0x00ffffff >> (24 - shift8)
		armAsm->And(a64::w0, a64::w0, RSCRATCHADDR.W());               // mem & mask
		armAsm->Lsl(a64::w1, a64::w1, armWRegister(shiftTemp));        // Rt << shift8
	}

	armAsm->Orr(a64::w0, a64::w0, a64::w1);                            // merged -> w0

	armAsm->Mov(a64::w9, armWRegister(addrTemp));
	if (useFastmem)
		vtlbFastmemWrite(9, 0, 32);
	else
		vtlbSoftmemWrite(9, 0, 32);

	_freeArm64GPR(shiftTemp);
	_freeArm64GPR(addrTemp);
}

// Inline LDL/LDR codegen (64-bit unaligned load). Mirrors x86 recLDL/recLDR and
// interp R5900OpcodeImpl.cpp LDL/LDR.
//
//   addr = Rs + imm ; s = addr & 7 ; shift8 = s * 8 ; aligned = addr & ~7
//   mem  = mem64(aligned)
//   LDL: Rt = (Rt & (~0 >> (shift8 + 8))) | (mem << (56 - shift8))   [s==7: Rt = mem]
//   LDR: Rt = (Rt & (~0 << (64 - shift8))) | (mem >> shift8)         [s==0: Rt = mem]
//
// The degenerate alignment (LDL s==7 / LDR s==0) needs a shift by 64, which the
// AArch64 variable-shift uses mod-64 — so those map to a straight Rt = mem and
// are branched out, exactly like x86's CMOVE/skip.
static void recUnalignedLoadDouble(bool is_ldl)
{
	if (!_Rt_)
		return;

	// --- LDL/LDR pair fusion -------------------------------------------------
	// Consume: this op is the trailing half of a pair already emitted as a single
	// 64-bit load by its predecessor — emit nothing.
	if (g_eeUnalignedFused)
	{
		g_eeUnalignedFused = false;
		return;
	}

	// Lead: an unaligned 64-bit load is emitted by the game as an LDL/LDR pair on
	// the same Rt/Rs whose offsets differ by 7 — together exactly mem64(Rs + lower
	// offset), which ARM64 loads in one (unaligned) LDR x. Either order occurs
	// (PS2 is little-endian; LDR-first is the common MIPSEL idiom). pc has been
	// advanced past THIS op, so it is the partner's guest address: memRead32(pc)
	// peeks it, pc is this load's fastmem-backpatch key, and pc < block-end
	// confirms the partner is in-block (so the flag is consumed this block). Gated
	// off faulting PCs (pc here, pc+4 = the partner) so the proven single-op
	// backpatch path is taken whenever either previously faulted.
	if (!g_recompilingDelaySlot && CHECK_FASTMEM && pc < recCurrentBlockEndPC() &&
		!vtlb_IsFaultingPC(pc) && !vtlb_IsFaultingPC(pc + 4))
	{
		const u32 partner = memRead32(pc);
		const u32 partnerOp = partner >> 26;
		const u32 partnerRt = (partner >> 16) & 0x1f;
		const u32 partnerRs = (partner >> 21) & 0x1f;
		const s32 partnerImm = static_cast<s16>(partner & 0xffff);
		const u32 wantOp = is_ldl ? 0x1bu : 0x1au; // LDL(0x1A) pairs with LDR(0x1B)
		const s32 ldlImm = is_ldl ? _Imm_ : partnerImm;
		const s32 ldrImm = is_ldl ? partnerImm : _Imm_;
		if (partnerOp == wantOp && partnerRt == static_cast<u32>(_Rt_) &&
			partnerRs == static_cast<u32>(_Rs_) && (ldlImm - ldrImm) == 7)
		{
			// One unaligned 64-bit fastmem load at Rs + ldrImm into Rt. mem is
			// parked in a callee-saved temp before the Rt alloc (the alloc can
			// spill via / land Rt in x0, clobbering the loaded value — the Black
			// LDL/LDR boot-hang bug). Rt uses MODE_READ|MODE_WRITE (not WRITE-only):
			// although the fused load fully overwrites Rt, WRITE-only on a GPR dest
			// fails to reconcile a prior dual-residence (NEON) copy under register
			// pressure, so a later reader (here the SDL/SDR store half of a memcpy)
			// reads the stale copy — observed as alternating-pair zeros in the
			// MultiRegUnalignedDwordCopyBlock test. READ|WRITE forces a coherent
			// residence; the redundant old-value load is free vs the bug.
			_eeMoveGPRtoR(a64::w9, _Rs_);
			iFlushCall(FLUSH_CONSTANT_REGS);
			if (ldrImm != 0)
				armAsm->Add(a64::w9, a64::w9, ldrImm);
			const int memTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
			vtlbFastmemRead(9, 0, 64, false);                 // mem -> x0
			armAsm->Mov(armXRegister(memTemp), a64::x0);       // park (x0 unsafe across Rt alloc)
			const int rt = _allocArm64GPR(ARM64TYPE_GPR, _Rt_, MODE_READ | MODE_WRITE);
			armAsm->Mov(armXRegister(rt), armXRegister(memTemp));
			_freeArm64GPR(memTemp);
			g_eeUnalignedFused = true;
			g_eeUnalignedFuseCount++;
			return;
		}
	}
	// --- end fusion ----------------------------------------------------------

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	_eeMoveGPRtoR(a64::w9, _Rs_);
	iFlushCall(FLUSH_CONSTANT_REGS);
	if (_Imm_ != 0)
		armAsm->Add(a64::w9, a64::w9, _Imm_);

	// mem must be parked in a callee-saved temp before the Rt alloc: under
	// register pressure the alloc can spill a guest reg or land Rt in x0,
	// clobbering x0 between the fastmem read and the merge. s = addr & 7 also
	// lives in a callee-saved temp. The store path parks all its operands in
	// callee-saved temps for the same reason.
	const int sTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
	const int memTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
	armAsm->And(armWRegister(sTemp), a64::w9, 7);
	armAsm->And(a64::w9, a64::w9, ~7u);                                // aligned

	if (useFastmem)
		vtlbFastmemRead(9, 0, 64, false);                             // mem -> x0
	else
		vtlbSoftmemRead(9, 64, false);

	armAsm->Mov(armXRegister(memTemp), a64::x0);                       // park mem (x0 unsafe across Rt alloc)

	const int rt = _allocArm64GPR(ARM64TYPE_GPR, _Rt_, MODE_READ | MODE_WRITE);

	a64::Label special, done;
	armAsm->Cmp(armWRegister(sTemp), is_ldl ? 7 : 0);
	armAsm->B(&special, a64::eq);

	armAsm->Lsl(RWSCRATCH, armWRegister(sTemp), 3);                    // x8 = shift8 (<=56)

	if (is_ldl)
	{
		// value: mem << (56 - shift8)
		armAsm->Mov(RSCRATCHADDR, 56);
		armAsm->Sub(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH);           // 56 - shift8
		armAsm->Lsl(armXRegister(memTemp), armXRegister(memTemp), RSCRATCHADDR);
		// mask: Rt & (~0 >> (shift8 + 8))
		armAsm->Add(RXSCRATCH, RXSCRATCH, 8);                         // shift8 + 8
		armAsm->Mov(RSCRATCHADDR, UINT64_C(0xFFFFFFFFFFFFFFFF));
		armAsm->Lsr(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH);
		armAsm->And(armXRegister(rt), armXRegister(rt), RSCRATCHADDR);
	}
	else
	{
		// mask amount = 64 - shift8
		armAsm->Mov(RSCRATCHADDR, 64);
		armAsm->Sub(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH);           // 64 - shift8
		// value: mem >> shift8
		armAsm->Lsr(armXRegister(memTemp), armXRegister(memTemp), RXSCRATCH);
		// mask: Rt & (~0 << (64 - shift8))
		armAsm->Mov(RXSCRATCH, UINT64_C(0xFFFFFFFFFFFFFFFF));
		armAsm->Lsl(RXSCRATCH, RXSCRATCH, RSCRATCHADDR);
		armAsm->And(armXRegister(rt), armXRegister(rt), RXSCRATCH);
	}

	armAsm->Orr(armXRegister(rt), armXRegister(rt), armXRegister(memTemp));
	armAsm->B(&done);

	armAsm->Bind(&special);
	armAsm->Mov(armXRegister(rt), armXRegister(memTemp));              // Rt = mem

	armAsm->Bind(&done);
	_freeArm64GPR(memTemp);
	_freeArm64GPR(sTemp);
}

// Inline SDL/SDR codegen (64-bit unaligned store, read-modify-write). Mirrors
// x86 recSDL/recSDR and interp R5900OpcodeImpl.cpp SDL/SDR.
//
//   addr = Rs + imm ; s = addr & 7 ; shift8 = s * 8 ; aligned = addr & ~7
//   mem  = mem64(aligned)
//   SDL: mem64(aligned) = (Rt >> (56 - shift8)) | (mem & (~0 << (shift8 + 8)))  [s==7: store Rt]
//   SDR: mem64(aligned) = (Rt <<       shift8 ) | (mem & (~0 >> (64 - shift8))) [s==0: store Rt]
//
// aligned, s and Rt all live in callee-saved temps across the read. The
// degenerate alignment (SDL s==7 / SDR s==0) stores Rt whole and skips the read.
static void recUnalignedStoreDouble(bool is_sdl)
{
	// --- SDL/SDR pair fusion -------------------------------------------------
	// Consume: this op is the trailing half of a pair already emitted as a single
	// 64-bit store by its predecessor — emit nothing.
	if (g_eeUnalignedFused)
	{
		g_eeUnalignedFused = false;
		return;
	}

	// Lead: the game emits an unaligned 64-bit store as an SDL/SDR pair on the
	// same Rt/Rs whose offsets differ by 7 (the textbook MIPS idiom: SDL at D+7,
	// SDR at D) — together exactly mem64(Rs + D) = Rt, which ARM64 stores in one
	// (unaligned) STR x. Either order occurs (PS2 is little-endian; SDR-first is
	// the common MIPSEL idiom). pc has been advanced past THIS op, so it is the
	// partner's guest address. Gated off faulting PCs (pc here, pc+4 = the
	// partner) so the proven single-op backpatch path is taken whenever either
	// previously faulted. Mirrors the LDL/LDR load fusion above.
	if (!g_recompilingDelaySlot && CHECK_FASTMEM && pc < recCurrentBlockEndPC() &&
		!vtlb_IsFaultingPC(pc) && !vtlb_IsFaultingPC(pc + 4))
	{
		const u32 partner = memRead32(pc);
		const u32 partnerOp = partner >> 26;
		const u32 partnerRt = (partner >> 16) & 0x1f;
		const u32 partnerRs = (partner >> 21) & 0x1f;
		const s32 partnerImm = static_cast<s16>(partner & 0xffff);
		const u32 wantOp = is_sdl ? 0x2du : 0x2cu; // SDL(0x2C) pairs with SDR(0x2D)
		const s32 sdlImm = is_sdl ? _Imm_ : partnerImm;
		const s32 sdrImm = is_sdl ? partnerImm : _Imm_;
		if (partnerOp == wantOp && partnerRt == static_cast<u32>(_Rt_) &&
			partnerRs == static_cast<u32>(_Rs_) && (sdlImm - sdrImm) == 7)
		{
			// One unaligned 64-bit fastmem store of Rt at Rs + sdrImm. The address
			// is parked in a callee-saved temp because _eeMoveGPRtoR(Rt) clobbers w9;
			// Rt goes through valTemp into x0 (the fastmem write value reg), exactly
			// like the non-fused store's whole-Rt (special) path. _eeMoveGPRtoR
			// handles Rt==0 (store $zero).
			_eeMoveGPRtoR(a64::w9, _Rs_);
			iFlushCall(FLUSH_CONSTANT_REGS);
			if (sdrImm != 0)
				armAsm->Add(a64::w9, a64::w9, sdrImm);
			const int addrTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
			const int valTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
			armAsm->Mov(armWRegister(addrTemp), a64::w9);     // park addr
			_eeMoveGPRtoR(armXRegister(valTemp), _Rt_);        // Rt (64-bit)
			armAsm->Mov(a64::x0, armXRegister(valTemp));        // value -> x0
			armAsm->Mov(a64::w9, armWRegister(addrTemp));       // addr -> w9
			vtlbFastmemWrite(9, 0, 64);
			_freeArm64GPR(valTemp);
			_freeArm64GPR(addrTemp);
			g_eeUnalignedFused = true;
			g_eeUnalignedFuseCount++;
			return;
		}
	}
	// --- end fusion ----------------------------------------------------------

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);

	_eeMoveGPRtoR(a64::w9, _Rs_);
	iFlushCall(FLUSH_CONSTANT_REGS);
	if (_Imm_ != 0)
		armAsm->Add(a64::w9, a64::w9, _Imm_);

	const int addrTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
	const int sTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);
	const int valTemp = _allocArm64GPR(ARM64TYPE_TEMP, 0, MODE_CALLEESAVED);

	armAsm->And(armWRegister(addrTemp), a64::w9, ~7u);                // aligned
	armAsm->And(armWRegister(sTemp), a64::w9, 7);                     // s
	_eeMoveGPRtoR(armXRegister(valTemp), _Rt_);                       // Rt (64-bit), handles Rt==0

	a64::Label special, merged;
	armAsm->Cmp(armWRegister(sTemp), is_sdl ? 7 : 0);
	armAsm->B(&special, a64::eq);

	// General path: read aligned word, merge with Rt.
	armAsm->Mov(a64::w9, armWRegister(addrTemp));
	if (useFastmem)
		vtlbFastmemRead(9, 0, 64, false);                            // mem -> x0
	else
		vtlbSoftmemRead(9, 64, false);

	armAsm->Lsl(RWSCRATCH, armWRegister(sTemp), 3);                   // x8 = shift8

	if (is_sdl)
	{
		// Rt >> (56 - shift8)
		armAsm->Mov(RSCRATCHADDR, 56);
		armAsm->Sub(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH);          // 56 - shift8
		armAsm->Lsr(armXRegister(valTemp), armXRegister(valTemp), RSCRATCHADDR);
		// mem & (~0 << (shift8 + 8))
		armAsm->Add(RXSCRATCH, RXSCRATCH, 8);                        // shift8 + 8
		armAsm->Mov(RSCRATCHADDR, UINT64_C(0xFFFFFFFFFFFFFFFF));
		armAsm->Lsl(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH);
		armAsm->And(a64::x0, a64::x0, RSCRATCHADDR);
	}
	else
	{
		// Rt << shift8
		armAsm->Lsl(armXRegister(valTemp), armXRegister(valTemp), RXSCRATCH);
		// mem & (~0 >> (64 - shift8))
		armAsm->Mov(RSCRATCHADDR, 64);
		armAsm->Sub(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH);          // 64 - shift8
		armAsm->Mov(RXSCRATCH, UINT64_C(0xFFFFFFFFFFFFFFFF));
		armAsm->Lsr(RXSCRATCH, RXSCRATCH, RSCRATCHADDR);
		armAsm->And(a64::x0, a64::x0, RXSCRATCH);
	}

	armAsm->Orr(a64::x0, a64::x0, armXRegister(valTemp));             // merged -> x0
	armAsm->B(&merged);

	armAsm->Bind(&special);
	armAsm->Mov(a64::x0, armXRegister(valTemp));                      // store Rt whole

	armAsm->Bind(&merged);
	armAsm->Mov(a64::w9, armWRegister(addrTemp));
	if (useFastmem)
		vtlbFastmemWrite(9, 0, 64);
	else
		vtlbSoftmemWrite(9, 0, 64);

	_freeArm64GPR(valTemp);
	_freeArm64GPR(sTemp);
	_freeArm64GPR(addrTemp);
}

void recLWL() { recUnalignedWord(true);  }
void recLWR() { recUnalignedWord(false); }
void recLDL() { recUnalignedLoadDouble(true);  }
void recLDR() { recUnalignedLoadDouble(false); }
void recSWL() { recUnalignedStoreWord(true);  }
void recSWR() { recUnalignedStoreWord(false); }
void recSDL() { recUnalignedStoreDouble(true);  }
void recSDR() { recUnalignedStoreDouble(false); }
// =====================================================================================================
//  LQC2 / SQC2 — 128-bit COP2 (VU0) register load/store
//  LQC2: VU0.VF[ft] = mem128((rs + imm) & ~0xF)
//  SQC2: mem128((rs + imm) & ~0xF) = VU0.VF[ft]
//  Same as LQ/SQ but target is VU0.VF[ft] instead of cpuRegs.GPR[rt].
//  Requires vu0Sync() before access.
// =====================================================================================================

void recLQC2()
{
	// Sync VU0 before COP2 register access. Gated on EEINST analysis —
	// no emit at all when the analysis says no sync is needed (the common
	// case for quad-load-heavy code like vertex streaming). Mirrors x86
	// recLQC2 (mVUSyncVU0 / mVUFinishVU0 gating on EEINST_COP2_SYNC_VU0 /
	// EEINST_COP2_FINISH_VU0). The helper handles iFlushCall, RECCYCLE
	// save/reload, runtime VPU_STAT check, and cycle accounting.
	cop2EmitConditionalSync(false, _vu0FinishMicro);

	// addr = (rs + imm) & ~0xF
	recComputeAddr();
	armAsm->And(a64::w9, a64::w9, (u32)~0xF);

	// Match recLQ/recSQ: spill constant tracking before the softmem C call
	// can fire (vtlb_memRead128 may dispatch through an MMIO handler that
	// reads cpuRegs). Redundant when cop2EmitConditionalSync already
	// emitted FLUSH_INTERPRETER, harmless otherwise.
	iFlushCall(FLUSH_CONSTANT_REGS);

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);
	if (useFastmem)
		vtlbFastmemRead128(9);
	else
		vtlbSoftmemRead128(9);

	// Store 128-bit result to VU0.VF[rt] (COP2 ft field = rt field)
	if (_Rt_)
	{
		armMoveAddressToReg(RSCRATCHADDR, &VU0.VF[_Rt_].UQ);
		armAsm->Str(a64::q0, a64::MemOperand(RSCRATCHADDR));
	}
}

void recSQC2()
{
	// EEINST-gated VU0 sync — see recLQC2 above.
	cop2EmitConditionalSync(false, _vu0FinishMicro);

	// addr = (rs + imm) & ~0xF — allocator-aware (rs may still be live
	// in a host reg if the sync above emitted nothing).
	recComputeAddr();
	armAsm->And(a64::w9, a64::w9, (u32)~0xF);

	// Flush before the q0 load. iFlushCall unconditionally evicts all NEON
	// (iR5900-arm64.cpp:765-769) and writes back any dirty cache; after
	// this point q0 is allocator-detached and safe to touch directly.
	iFlushCall(FLUSH_CONSTANT_REGS);

	// Load 128-bit VU0.VF[rt] into q0 (COP2 ft field = rt field). MUST be
	// after iFlushCall: q0 may otherwise be tracked by the EE NEON allocator
	// as caching a live FPREG, in which case this load stomps q0's runtime
	// contents while leaving arm64neon[0] still marked dirty. The next
	// allocator flush would then write back the stomped value (VU0.VF[rt])
	// to the FPREG's memory slot, corrupting the FPREG.
	armLoadPtr(a64::q0, &VU0.VF[_Rt_].UQ);

	const bool useFastmem = CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);
	if (useFastmem)
		vtlbFastmemWrite128(9);
	else
		vtlbSoftmemWrite128(9);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
