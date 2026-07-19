// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "RecompilerTestEnvironment.h"
#include "MipsEncode.h"

#include "Config.h"
#include "IopCounters.h"
#include "IopHw.h"
#include "IopMem.h"
#include "Memory.h"
#include "R3000A.h"
#include "R5900.h"
#include "VMManager.h"
#include "VUmicro.h"
#if defined(_M_ARM64) || defined(__aarch64__)
#include "arm64/microVU_Persist-arm64.h"
#endif
#include "common/FPControl.h"

#include "cpuinfo.h"

#include <cstdio>
#include <cstdlib>

extern s32 psxNextDeltaCounter;
extern u64 psxNextStartCounter;
extern s32 nextDeltaCounter;
extern u64 nextStartCounter;

namespace recompiler_tests {

namespace {

bool s_ready = false;

void InstallParkingLot()
{
	// `j kParkingPc; nop` — the harness's `jr ra` sentinel target. Any
	// block the JIT compiles here is an infinite self-loop that will run
	// until the ExecuteBlock cycle budget is exhausted.
	iopMemWrite32(RecompilerTestEnvironment::kParkingPc + 0, mips::J(RecompilerTestEnvironment::kParkingPc));
	iopMemWrite32(RecompilerTestEnvironment::kParkingPc + 4, mips::NOP);

	// Same convention for EE — parking lot at the same guest address
	// (different physical memory). EE harness loops intCpu.Step() until
	// `cpuRegs.pc == kParkingPc`, so the J is never actually executed; it's
	// here so that if any path accidentally falls through, it still self-
	// loops rather than escaping into uninitialized memory.
	memWrite32(RecompilerTestEnvironment::kParkingPc + 0, mips::J(RecompilerTestEnvironment::kParkingPc));
	memWrite32(RecompilerTestEnvironment::kParkingPc + 4, mips::NOP);
}

void InstallEeExceptionVectorStubs()
{
	// EE trap-bearing opcodes (TEQ/TGE/TLT/TNE + immediate variants +
	// SYSCALL/BREAK) call cpuException, which with BEV=0 / EXL=0 lands PC
	// at 0x80000000+offset (R5900.cpp:94). Without a handler, the interp
	// step-loop walks zero-bytes-as-NOPs forever and wedges the harness.
	//
	// Both EeRecTestHarness and JitTestHarness seed ra=kParkingPc on entry
	// and traps don't clobber ra, so a `jr ra; nop` stub at each vector
	// returns cleanly to the parking lot — giving trap-taken tests a
	// well-defined post-state (architecturally-correct Cause/EPC/Status.EXL
	// + PC parked).
	//
	// Cover all five BEV=0 vectors (not just 0x80000180) so tests
	// exercising interrupt/TLB/L2 paths don't each need to re-install. BEV=1
	// mirrors at 0xBFC00200+offset are BIOS-ROM territory and stay
	// uninstalled; tests that intentionally set BEV can install their own.
	static constexpr u32 kBev0Vectors[] = {
		0x80000000, // TLB refill
		0x80000080, // Level-2 (perf counter)
		0x80000100, // Level-2 (debug)
		0x80000180, // Common (trap/syscall/break + general exceptions)
		0x80000200, // Interrupt (when Cause.IV=0; IV=1 diverts to +0x200 as well)
	};
	for (const u32 va : kBev0Vectors)
	{
		memWrite32(va + 0, mips::JR(mips::reg::ra));
		memWrite32(va + 4, mips::NOP);
	}
}

} // namespace

bool RecompilerTestEnvironment::Initialize()
{
	if (s_ready)
		return true;

	// 1. FPU default rounding mode — upstream does this in
	//    VMManager::Internal::CPUThreadInitialize before any JIT setup.
	FPControlRegister::SetCurrent(FPControlRegister::GetDefault());

	// 2. cpuinfo. Used by emitter ISA checks; harmless if already done.
	if (!cpuinfo_initialize())
		std::fprintf(stderr, "[recompiler_tests] cpuinfo_initialize() failed\n");

	// 3. VM memory regions (EE / IOP / VU RAM + all rec code buffers).
	if (!SysMemory::Allocate())
		return false;

	// 4-5. CPU providers. Reserve allocates BASEBLOCK tables and the JIT
	//      dispatcher's code buffer; Reset initializes them + emits the
	//      dispatcher prologue.
	psxRec.Reserve();
	psxInt.Reserve();
	recCpu.Reserve();
	intCpu.Reserve();
	// Persisted-JIT cache: take manual control of recording so the
	// production mVUinit/mVUreset SyncRecordingFromConfig calls don't drive
	// it from EmuConfig — the persist/abi/disk tests set recording (and the
	// on-disk cache) explicitly. Must precede the Reserve calls below, since
	// CpuMicroVU0.Reserve runs mVUinit (which calls SyncRecordingFromConfig).
#if defined(_M_ARM64) || defined(__aarch64__)
	mVUPersist::SetTestManualRecording(true);
#endif

	// microVU JITs. mVUinit allocates the per-VU regAlloc + sets cache
	// pointers; the corresponding Reset call below emits dispatchers.
	// CpuMicroVU1::Reserve also opens vu1Thread — with THREAD_VU1=false it
	// parks waiting for work that never comes; TearDown joins it via
	// CpuMicroVU1.Shutdown().
	CpuMicroVU0.Reserve();
	CpuMicroVU1.Reserve();

	// 6. SysMemory::Reset zeroes + remaps IOP LUT + EE mem + VU mem.
	SysMemory::Reset();

	// 7-8. Per-CPU reset.
	psxRec.Reset();
	psxInt.Reset();
	recCpu.Reset();
	intCpu.Reset();
	CpuMicroVU0.Reset();
	CpuMicroVU1.Reset();

	// iopMemWrite32 calls `psxCpu->Clear(addr, 1)` after every store to invalidate
	// the JIT's block cache for self-modifying code. Must be non-null before any
	// memory write. Default to the IOP interpreter; the IOP JIT harness flips
	// this to psxRec in its ctor when DiffJitVsInterp mode is active.
	//
	// Why not default to psxRec? EE branches call cpuEventTest →
	// `psxCpu->ExecuteBlock` on every branch — so using the JIT here would drag
	// IOP-rec compilation into every EE test, needlessly. The EE tests want a
	// benign IOP that just burns a few cycles, which is exactly what psxInt
	// provides.
	psxCpu = &psxInt;

	// EE: wire the interpreter as the active Cpu. memWrite* → vtlb store paths
	// eventually call `Cpu->Clear(...)` to invalidate compiled blocks; Cpu must
	// be non-null before any EE memory write. intCpu.Clear is a no-op, which is
	// fine for InterpOnly mode.
	Cpu = &intCpu;

	// VU interpreters. _cpuEventTest_Shared calls CpuVU0/CpuVU1->ExecuteBlock
	// at the end of every EE branch; these pointers default to null and would
	// segfault on first branch. CpuIntVU0 / CpuIntVU1 are always-linked
	// interpreter instances (no VU rec required). VU tests reuse these as
	// the diff baseline.
	CpuVU0 = &CpuIntVU0;
	CpuVU1 = &CpuIntVU1;

	// Pin VU-related EmuConfig flags off for determinism. THREAD_VU1 forks
	// VU1 dispatch into a separate thread (parallel-universe code paths);
	// vu1Instant short-circuits VU1 cycle accounting; XgKickHack alters
	// XGKICK cycle accumulation per-game. vuFlagHack (on by default!) lets the
	// JIT skip COP2 status-flag updates the interpreter still performs, so the
	// JIT-vs-interp diff would see legitimately-dead flag divergences. None of
	// these belong in a unit test where determinism is the contract — tests
	// that exercise the flaghack path opt in by setting it true locally and
	// reading only live values (RunJitNoDiff).
	EmuConfig.Speedhacks.vuThread = false;
	EmuConfig.Speedhacks.vu1Instant = false;
	EmuConfig.Speedhacks.vuFlagHack = false;
	EmuConfig.Gamefixes.XgKickHack = false;

	// Testing-only override: PCSX2_VU_XGKICKHACK=1 replays with the XgKickHack
	// gamefix ON, matching games whose GameDB entry forces it (e.g. Crash
	// Twinsanity). CHECK_XGKICKHACK changes the COMPILED SHAPE of every VU1
	// block containing an XGKICK and of the E-bit end sequence
	// (mVU_XGKICK_SYNC emission), so a live-vs-replay divergence can hide in
	// the hack-ON code paths that the deterministic default never compiles.
	if (const char* xgkick = std::getenv("PCSX2_VU_XGKICKHACK"))
		EmuConfig.Gamefixes.XgKickHack = std::atoi(xgkick) != 0;

	// 9. Parking lot for test programs' `jr ra` sentinel + EE exception
	//    vector `jr ra; nop` stubs that trap-bearing opcode tests return
	//    through.
	InstallParkingLot();
	InstallEeExceptionVectorStubs();

	// 10. Initialize IOP counters to a state where the IOP event test is a
	//     no-op for the duration of any single-block test. Without this,
	//     `psxTestCycle(psxNextStartCounter, psxNextDeltaCounter)` fires
	//     after 1 cycle (both values default to 0), which calls
	//     psxRcntUpdate() → DEV9 ATA::Async on an uninitialized `this`.
	//     Setting the delta to INT32_MAX makes the counter check return
	//     false until a test runs for 2^31 IOP cycles — effectively never.
	psxRcntInit();
	psxNextDeltaCounter = 0x7FFFFFFF;
	psxNextStartCounter = 0;

	// 11. Same story for the EE. On any EE branch, `intEventTest` →
	//     `_cpuEventTest_Shared` →
	//     `if (cpuTestCycle(nextStartCounter, nextDeltaCounter)) rcntUpdate()`
	//     → `rcntUpdate_vSync` → `VSyncStart` → input-poll (null deref on the
	//     uninitialized InputManager). Setting the delta to INT32_MAX prevents
	//     cpuTestCycle from ever firing during a test-length run.
	nextDeltaCounter = 0x7FFFFFFF;
	nextStartCounter = 0;

	s_ready = true;
	return true;
}

void RecompilerTestEnvironment::Shutdown()
{
	if (!s_ready)
		return;

	psxRec.Shutdown();
	psxInt.Shutdown();
	recCpu.Shutdown();
	intCpu.Shutdown();
	CpuMicroVU0.Shutdown();
	CpuMicroVU1.Shutdown(); // joins vu1Thread
	SysMemory::Release();

	s_ready = false;
}

bool RecompilerTestEnvironment::IsReady()
{
	return s_ready;
}

void RecompilerTestEnvironment::ResetVuBlockCache(int vu_index)
{
	// Reset() re-emits the dispatcher and zeroes mVU.prog.lpState, which
	// invalidates every compiled block — the cheapest way to ensure a fresh
	// test cannot inherit a compiled variant whose entry pState happens to
	// match the new test's seeded entry state.
	if (vu_index == 0)
		CpuMicroVU0.Reset();
	else
		CpuMicroVU1.Reset();
}

} // namespace recompiler_tests
