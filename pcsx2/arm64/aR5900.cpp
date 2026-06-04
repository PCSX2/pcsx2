// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — skeleton (Phase 1).
//
// ARM64 counterpart to pcsx2/x86/ix86-32/iR5900.cpp. At this stage every entry
// point is a stub: the recompiler is *defined* and links, providing the recCpu
// provider so VMManager can be wired to call Reserve/Reset/Shutdown on ARM64,
// but no guest code is actually compiled yet (recExecute fails loudly if reached).
// Real codegen lands incrementally in later phases (vtlb fastmem -> EE int ->
// branches -> coprocessors). The interpreter remains ground truth and the active
// provider until this is functional.

#include "arm64/aR5900.h"

#include "Memory.h"
#include "R5900.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Pcsx2Defs.h"

// --------------------------------------------------------------------------------------
//  EE code-cache layout (Phase 1.3)
// --------------------------------------------------------------------------------------
// The EE recompiler region is pre-reserved by SysMemory (HostMemoryMap::EErec*, 64 MB).
// We do NOT allocate it ourselves; we just carve it:
//
//   [ GetEERec() ............................ recPtrEnd )   emitted block code
//   [ recPtrEnd ............................. GetEERecEnd() )   ArmConstantPool
//
// The constant pool holds far-jump trampolines and 64/128-bit literals that VIXL
// loads PC-relative (see ArmConstantPool in AsmHelpers). x86 has no such pool (it
// inlines immediates), so this tail carve-out is ARM64-specific. recPtr is the
// rolling emit cursor; block compilation (Phase 1.4) advances it and resets the
// whole cache when it runs past recPtrEnd.

// Space reserved at the tail of the EE rec region for the constant pool.
static constexpr u32 EE_CONSTPOOL_SIZE = static_cast<u32>(_1mb);

static u8* recPtr = nullptr;    // rolling emit cursor (start of next block)
static u8* recPtrEnd = nullptr; // end of the code region / start of the constant pool

static ArmConstantPool s_const_pool;

static void recReserve()
{
	recPtr = SysMemory::GetEERec();
	recPtrEnd = SysMemory::GetEERecEnd() - EE_CONSTPOOL_SIZE;

	s_const_pool.Init(recPtrEnd, EE_CONSTPOOL_SIZE);
}

static void recShutdown()
{
	s_const_pool.Destroy();

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static void recResetEE()
{
	// Rewind the emit cursor and drop all cached trampolines/literals. Block map
	// and dispatcher regeneration land in Phase 1.4; const-prop state in Phase 3.6.
	recPtr = SysMemory::GetEERec();
	s_const_pool.Reset();
}

static void recStep()
{
	// Debugger single-step. Recompilers fall back to the interpreter for this.
}

// --------------------------------------------------------------------------------------
//  Single-instruction decode + dispatch (Phase 2.3)
// --------------------------------------------------------------------------------------
// MIPS primary opcodes we can translate so far. Everything else falls back to a
// NOP placeholder for now (interpreter remains the active provider — see below).
enum : u32
{
	OP_LW = 0x23,
	OP_SW = 0x2b,
};

// Translate a single guest instruction (cpuRegs.code) into the open block. Returns
// true if a real generator handled it, false if it fell through to a placeholder.
// Decodes the MIPS fields explicitly and hands them to the Phase 2.3 load/store
// generators (which read/write guest GPRs through RESTATEPTR and route memory
// access via the slow-path vtlb helpers).
static bool recTranslateOp(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const s32 imm = static_cast<s16>(op);

	switch (opcode)
	{
		case OP_LW: armEmitLoadGpr(32, true, rt, rs, imm); return true;
		case OP_SW: armEmitStoreGpr(32, rt, rs, imm); return true;
		default: return false;
	}
}

// --------------------------------------------------------------------------------------
//  Minimal block compile loop (Phase 1.4, extended in 2.3)
// --------------------------------------------------------------------------------------
// Compile a single guest instruction at cpuRegs.pc into a block and return its
// entry point. This exercises the production emission lifecycle (armSetAsmPtr ->
// armStartBlock -> emit -> armEndBlock) against the real EE code cache, now with a
// real MIPS decode + dispatch for LW/SW instead of a fixed NOP body.
//
// This path is still INERT in normal operation: the interpreter stays the active
// Cpu provider (recExecute is never entered — Phase 4 adds the enter-trampoline
// that pins RESTATEPTR=&cpuRegs, the block LUT, PC/cycle management, and event
// tests). It is groundwork validated by compile-clean + the Arm64EmitEE unit tests
// that exercise the generators directly; full guest-memory round-trip validation
// is Phase 2.4.
static u8* recCompileBlock()
{
	// Whole-cache reset once we run past the code region into the constant-pool tail.
	if (recPtr >= recPtrEnd)
		recResetEE();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);

	u8* const entry = armStartBlock();

	const u32 op = memRead32(cpuRegs.pc);
	cpuRegs.code = op;
	if (!recTranslateOp(op))
		armAsm->Nop(); // unhandled opcode placeholder (most ops, until later phases)

	armAsm->Ret();

	recPtr = armEndBlock();
	return entry;
}

static void recExecute()
{
	// Phase 1.4 proof-of-life: compile one trivial block through the real emitter,
	// enter it, and return. There is no execution loop / dispatcher yet, so this
	// runs exactly one (empty) block. The interpreter remains the active Cpu
	// provider (recCpu is not yet selected in VMManager — Phase 1.5), so this path
	// is not hit in normal operation; it exists to validate emit + enter + return
	// end-to-end on the EE code cache before real codegen lands.
	u8* const entry = recCompileBlock();
	reinterpret_cast<void (*)()>(entry)();
}

static void recSafeExitExecution()
{
	// TODO(Phase 4): signal the dispatcher loop to exit at a safe point.
}

static void recCancelInstruction()
{
	pxFailRel("recCancelInstruction() called, this should never happen!");
}

static void recClear(u32 addr, u32 size)
{
	// TODO(Phase 4.5): invalidate compiled blocks covering [addr, addr+size).
}

R5900cpu recCpu = {
	recReserve,
	recShutdown,

	recResetEE,
	recStep,
	recExecute,

	recSafeExitExecution,
	recCancelInstruction,
	recClear};
