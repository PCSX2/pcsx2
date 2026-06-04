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
//  Minimal block compile loop (Phase 1.4)
// --------------------------------------------------------------------------------------
// Compile a single block at the rolling emit cursor and return its entry point.
// This is the smallest possible exercise of the production emission lifecycle
// (armSetAsmPtr -> armStartBlock -> emit -> armEndBlock) against the real EE code
// cache + constant pool. The body is a placeholder: a couple of NOPs followed by a
// RET back to the caller. Later phases replace the NOPs with the ARM64 translation
// of 1-2 MIPS ops read from cpuRegs.pc, and the bare RET with a tail-jump into the
// dispatcher (block LUT + linking + event tests land in Phase 4).
static u8* recCompileBlock()
{
	// Whole-cache reset once we run past the code region into the constant-pool tail.
	if (recPtr >= recPtrEnd)
		recResetEE();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);

	u8* const entry = armStartBlock();

	// Placeholder block body. TODO(Phase 2/3): translate guest ops here.
	armAsm->Nop();
	armAsm->Nop();
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
