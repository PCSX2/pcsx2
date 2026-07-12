// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// waitMTVU stub-shape guard.
//
// mVUGenerateWaitMTVU emits the thunk that mVUaddrFix branches to when VU0 (or
// COP2 macro mode) touches VU1's register space (`addr & 0x400`) while MTVU is
// on. Its job is to call mVUwaitMTVU -> vu1Thread.WaitVU(), synchronising the
// async VU1 worker before the read. The thunk shipped for a long time as a bare
// `Ret` (the x86 save/call/restore body — microVU_Execute.inl — was never
// ported), so under MTVU the sync silently did nothing and VU0/COP2 raced the
// VU1 thread. Same "whole x86 function never ported" class as condEvilBranch.
//
// The single-threaded VU harness pins vuThread=false, so THREAD_VU1 is always
// false and the seam that emits the call to this thunk is dead — a JIT-vs-interp
// diff can never reach it. This test instead inspects the emitted thunk directly
// and asserts it makes a call; a bare Ret (zero calls) is the bug. The thunk is
// generated once by mVUgenerateDispatchers during environment init, so IsReady()
// is sufficient — no VU program needs to run.

#include "harness/RecompilerTestEnvironment.h"

#include "common/Pcsx2Defs.h"

#include <gtest/gtest.h>

// Defined in pcsx2/arm64/microVU-arm64.cpp (PCSX2_RECOMPILER_TESTS builds only).
// Global scope — must be declared outside namespace recompiler_tests.
const u8* mVUTestProbe_WaitMTVUStub(int index);

namespace recompiler_tests {

namespace {

// AArch64 fixed-width encodings (per the ARM ARM):
//   BL  imm26 : 100101 iiii...  -> 0x94000000, fixed bits mask 0xFC000000
//   BLR Rn    : ...11111 Rn ..  -> 0xD63F0000, fixed bits mask 0xFFFFFC1F
//   RET Rn    : ...11111 Rn ..  -> 0xD65F0000, fixed bits mask 0xFFFFFC1F
constexpr bool IsBL(u32 w)  { return (w & 0xFC000000u) == 0x94000000u; }
constexpr bool IsBLR(u32 w) { return (w & 0xFFFFFC1Fu) == 0xD63F0000u; }
constexpr bool IsRet(u32 w) { return (w & 0xFFFFFC1Fu) == 0xD65F0000u; }

// Scan the thunk from its entry to the first RET and report whether any call
// (BL, or the far-call Mov+BLR form armEmitCall may pick) was emitted before it.
// The thunk is straight-line frame-save + one call + restore + ret, so the first
// RET is its terminator; a movz/movk immediate chain never aliases these opcodes.
bool StubCallsOut(const u8* stub)
{
	if (!stub)
		return false;
	const u32* insn = reinterpret_cast<const u32*>(stub);
	constexpr int kMaxScan = 256; // real thunk is ~70 insns; bound a runaway scan
	for (int i = 0; i < kMaxScan; i++)
	{
		const u32 w = insn[i];
		if (IsBL(w) || IsBLR(w))
			return true;
		if (IsRet(w))
			return false; // reached the terminator with no call — the bare-Ret bug
	}
	return false;
}

} // namespace

// A bare-Ret thunk (the historical bug) returns without ever calling
// vu1Thread.WaitVU(): under MTVU, VU0/COP2 reads of VU1 registers silently skip
// the worker-thread sync and race it. Require the thunk to actually call out.
TEST(MvuWaitMTVU, StubSyncsVu1ThreadNotBareRet)
{
	ASSERT_TRUE(RecompilerTestEnvironment::IsReady());

	// VU0's thunk is the one the seam (isVU0 && addr & 0x400) branches to.
	const u8* vu0Stub = mVUTestProbe_WaitMTVUStub(0);
	ASSERT_NE(vu0Stub, nullptr);
	EXPECT_TRUE(StubCallsOut(vu0Stub))
		<< "VU0 waitMTVU thunk emitted no BL/BLR before its Ret — the MTVU "
		   "VU0->VU1 sync (vu1Thread.WaitVU) is a no-op.";

	// The thunk is generated for both VUs; VU1's is unused by the seam but is
	// emitted by the same generator and must share the shape.
	const u8* vu1Stub = mVUTestProbe_WaitMTVUStub(1);
	ASSERT_NE(vu1Stub, nullptr);
	EXPECT_TRUE(StubCallsOut(vu1Stub))
		<< "VU1 waitMTVU thunk emitted no BL/BLR before its Ret.";
}

} // namespace recompiler_tests
