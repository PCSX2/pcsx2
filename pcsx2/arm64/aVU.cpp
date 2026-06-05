// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 microVU recompiler (Phase 7).
//
// Task 7.2a landed the arch-neutral data structures in aVU.h; task 7.2b adds the
// host register allocator (microRegAlloc) in aVU_IR.h. This translation unit
// currently validates that those structures compile and have the expected layout
// on ARM64, and instantiates the allocator so its codegen is exercised by the
// build. The real recompiler shell — program/block cache management and the
// recMicroVU0/1 providers — is task 7.2c. Until then this file is intentionally
// minimal.

#include "arm64/aVU.h"
#include "arm64/aVU_IR.h"

// Layout sanity checks for the ported pipeline-state key / IR structs. These
// mirror the invariants the x86 microVU relies on (the 96-byte microRegInfo is
// compared as six 128-bit vectors by the generated compareStateF).
static_assert(sizeof(microRegInfo) == 96, "microRegInfo must stay 96 bytes (host pipeline-state compare)");
static_assert(alignof(microRegInfo) == 16, "microRegInfo must stay 16-byte aligned");
static_assert(alignof(microBlock) == 16, "microBlock must stay 16-byte aligned");

// Force the register allocator's emission paths to be compiled. microRegAlloc is
// not yet wired into a provider (that is task 7.2c/7.2d), so without an explicit
// use the member-function bodies would never be instantiated and any VIXL
// emission error in them would go undetected. This function is never called.
[[maybe_unused]] static void mVUallocCompileCheck()
{
	microRegAlloc ra(0);
	ra.reset();
	(void)ra.allocReg(1, 2, 0xf);
	(void)ra.allocReg(0, 1, 0x4);
	(void)ra.allocReg(33, -1, 0x8);
	(void)ra.allocReg(32, 32, 0x6, false);
	(void)ra.allocGPR(1, 2, true);
	(void)ra.allocGPR(-1, 3);
	ra.moveVIToGPR(armWRegister(0), 1, true);
	ra.flushAll();
	ra.flushCallerSavedRegisters(true);
	ra.TDwritebackAll();
	(void)ra.checkVFClamp(0);
	(void)ra.hasRegVF(1);
	(void)ra.hasRegVI(1);
	(void)ra.getFreeXmmCount();
	(void)ra.getFreeGPRCount();
}
