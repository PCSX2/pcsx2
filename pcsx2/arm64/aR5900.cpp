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

#include "R5900.h"

#include "common/Assertions.h"
#include "common/Console.h"

static void recReserve()
{
	// TODO(Phase 1.3): allocate the EE code cache via HostSys and the constant pool.
}

static void recShutdown()
{
	// TODO(Phase 1.3): release the code cache.
}

static void recResetEE()
{
	// TODO(Phase 1): reset code cache / block map / const-prop state.
}

static void recStep()
{
	// Debugger single-step. Recompilers fall back to the interpreter for this.
}

static void recExecute()
{
	pxFailRel("ARM64 EE recompiler is not implemented yet (skeleton only).");
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
