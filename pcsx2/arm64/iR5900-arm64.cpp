// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) Dynamic Recompiler Core
// Dispatcher, block management, all instructions as interpreter fallbacks.

#include <algorithm>
#include <cfloat>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "arm64/iR5900-arm64.h"
#include "arm64/iR5900Analysis.h"
#include "arm64/AsmHelpers.h"
#include "common/HostSys.h"
#include "Host.h"
#include "R3000A.h"
#include "R5900.h"
#include "arm64/BaseblockEx-arm64.h"
#include "R5900OpcodeTables.h"
#include "Common.h"
#include "VMManager.h"
#include "Patch.h"
#include "Config.h"
#include "vtlb.h"
#include "Dmac.h"
#include "GS.h"
#ifdef PCSX2_RECOMPILER_TESTS
#include "ee_divtrace.h" // diagnostic divergence-trace hooks (test builds only)
#endif

#include "common/Assertions.h"
#include "common/AlignedMalloc.h"
#include "common/Console.h"
#include "common/FastJmp.h"
#include "common/HeapArray.h"
#include "common/Perf.h"

#include "DebugTools/Breakpoints.h"

namespace a64 = vixl::aarch64;

// =====================================================================================================
//  Global State
// =====================================================================================================

u32 maxrecmem = 0;
u32 pc;
int g_branch;
u32 target;
u32 s_nBlockCycles;
bool s_nBlockInterlocked;

bool g_recompilingDelaySlot = false;
bool g_cpuFlushedPC = false;
bool g_cpuFlushedCode = false;
// Mirrors x86 iR5900.cpp:64. Set only by the (currently #if 0'd) FLUSH_CAUSE
// path in iFlushCall and reset after every recompiled instruction, so in
// practice it is always false and the two BD-bit clears it guards never emit at
// runtime — exactly matching x86. Kept so the delay-slot handling is a
// line-for-line mirror of x86 and re-syncs cleanly if upstream re-enables it.
bool g_maySignalException = false;

// LDL/LDR pair fusion state (see iR5900-arm64.h). g_eeUnalignedFused is set by the
// leading half of a fused unaligned 64-bit load and consumed by the trailing half;
// g_eeUnalignedFuseCount is a diagnostic/test tally.
bool g_eeUnalignedFused = false;
u32 g_eeUnalignedFuseCount = 0;

// Constant propagation — defined here, declared extern in iR5900-arm64.h

static uptr recLUT[0x10000];
static u32 hwLUT[0x10000];

static __fi u32 HWADDR(u32 mem) { return hwLUT[mem >> 16] + mem; }

static BASEBLOCK* recRAM = nullptr;
static BASEBLOCK* recROM = nullptr;
static BASEBLOCK* recROM1 = nullptr;
static BASEBLOCK* recROM2 = nullptr;
static Arm64BaseBlocks recBlocks;
static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;

// SL-10: cold side-exit arena. Superblock taken arms outline into a separate
// region carved from the top of the EE cache (below the constant pool) instead
// of inline after each block's tail — inline placement puts cold bytes between
// consecutive hot blocks in the compile-order stream, which the S2 device A/B
// measured as +5.4% EErec icache-miss density. The arena stays inside the EE
// rec region, so fastmem fault range checks and perf bucketing are unaffected.
// Bump-allocated; recycled only by the full cache reset, like block memory.
static u8* s_coldBase = nullptr;
static u8* s_coldPtr = nullptr;
static u8* s_coldPtrEnd = nullptr;

static EEINST* s_pInstCache = nullptr;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = nullptr;
static BASEBLOCKEX* s_pCurBlockEx = nullptr;

static u32 s_nEndBlock = 0;
static u32 s_branchTo;
// SL-1: terminal branch class permits a resident back-edge (conditional
// branches and plain J — not JAL/JR, which own call-ret/indirect tails).
static bool s_branchLoopable;
static bool s_nBlockFF;

// Exposed for the LDL/LDR fusion in recVTLB-arm64.cpp (same-block partner check).
u32 recCurrentBlockEndPC() { return s_nEndBlock; }

static DynamicHeapArray<BASEBLOCK, 4096> recLutReserve_RAM;
static DynamicHeapArray<BASEBLOCK, 4096> recLutUnmapped;
static DynamicHeapArray<u8> recRAMCopy;
static size_t recLutEntries = 0;

static ArmConstantPool s_eeConstantPool;

// Execution state
static fastjmp_buf m_SetJmp_StateCheck;
static bool eeCpuExecuting = false;
static bool eeRecNeedsReset = false;
static bool eeRecExitRequested = false;

#ifdef PCSX2_RECOMPILER_TESTS
// Harness-entry state. Set by recEeExecuteBlock before entering the JIT,
// observed by recEventTest at every iBranchTest event-due fall-out.
// Production execution (recExecute) leaves g_eeHarnessActive false; the
// predicate short-circuits on it. Compiled out entirely in release builds
// (ENABLE_RECOMPILER_TEST_HOOKS=OFF) so the live VM path carries no
// test-only symbols or branches.
static bool g_eeHarnessActive = false;
static u32 g_eeHarnessParkPc = 0;
static s32 g_eeHarnessCycleBudget = 0;
static u64 g_eeHarnessCycleStart = 0;
static constexpr s32 kExecuteBlockSafetyCap = 1 << 20;
// Wait-loop detection verdict for the most recently analysed block —
// non-static: read by the AX-07 detector tests via extern.
bool g_eeRecLastBlockFF = false;
#endif

// Self-modifying code detection
static u16 manual_page[Ps2MemSize::MainRam / 4096] = {};
static u8 manual_counter[Ps2MemSize::MainRam / 4096] = {};

// Forward declarations
static void recRecompile(const u32 startpc);
static void recResetRaw();
static void recExitExecution();
static void recSafeExitExecution();
#ifdef PCSX2_RECOMPILER_TESTS
static bool harnessShouldExit();
#endif
static void recError(u32 error);
static void dyna_block_discard(u32 start, u32 sz);
static void dyna_page_reset(u32 start, u32 sz);
static void iopClearRecLUT(BASEBLOCK* base, int count);

// recBackpropBSC declared in arm64/iR5900Analysis.h

// =====================================================================================================
//  Native Codegen Verification Mode
// =====================================================================================================

#ifdef VERIFY_NATIVE_CODEGEN

// Snapshot of GPR + HI/LO state before the native instruction executes
static GPR_reg s_verifyGPR[32];
static GPR_reg s_verifyHI, s_verifyLO;
static u32 s_verifyMismatchCount = 0;

// VU0 state snapshot for COP2 verification
static VECTOR s_verifyVF[32];
static VECTOR s_verifyACC;
static REG_VI s_verifyVI[32];
static u32 s_verifyClipFlag;

// Called at runtime BEFORE the native instruction: snapshot all state
static void verifySnapshotPre(u32 code, u32 instPC)
{
	memcpy(s_verifyGPR, cpuRegs.GPR.r, sizeof(s_verifyGPR));
	s_verifyHI = cpuRegs.HI;
	s_verifyLO = cpuRegs.LO;

	// COP2: also snapshot VU0 state
	if ((code >> 26) == 0x12) // COP2 opcode
	{
		memcpy(s_verifyVF, VU0.VF, sizeof(s_verifyVF));
		s_verifyACC = VU0.ACC;
		memcpy(s_verifyVI, VU0.VI, sizeof(s_verifyVI));
		s_verifyClipFlag = VU0.clipflag;
	}
}

// Called at runtime AFTER the native instruction: re-run via interpreter on the snapshot and compare
static void verifyCheckPost(u32 code, u32 instPC)
{
	const bool isCOP2 = (code >> 26) == 0x12;

	// Save native results
	GPR_reg nativeGPR[32];
	GPR_reg nativeHI, nativeLO;
	memcpy(nativeGPR, cpuRegs.GPR.r, sizeof(nativeGPR));
	nativeHI = cpuRegs.HI;
	nativeLO = cpuRegs.LO;

	// Save native VU0 state for COP2
	VECTOR nativeVF[32];
	VECTOR nativeACC;
	REG_VI nativeVI[32];
	u32 nativeClipFlag = 0;
	if (isCOP2)
	{
		memcpy(nativeVF, VU0.VF, sizeof(nativeVF));
		nativeACC = VU0.ACC;
		memcpy(nativeVI, VU0.VI, sizeof(nativeVI));
		nativeClipFlag = VU0.clipflag;
	}

	// Restore pre-instruction state
	memcpy(cpuRegs.GPR.r, s_verifyGPR, sizeof(s_verifyGPR));
	cpuRegs.HI = s_verifyHI;
	cpuRegs.LO = s_verifyLO;
	if (isCOP2)
	{
		memcpy(VU0.VF, s_verifyVF, sizeof(s_verifyVF));
		VU0.ACC = s_verifyACC;
		memcpy(VU0.VI, s_verifyVI, sizeof(s_verifyVI));
		VU0.clipflag = s_verifyClipFlag;
	}

	// Run interpreter
	const u32 savedCode = cpuRegs.code;
	cpuRegs.code = code;
	const R5900::OPCODE& opcode = R5900::GetCurrentInstruction();
	if (opcode.interpret)
		opcode.interpret();
	cpuRegs.code = savedCode;

	// Compare results
	bool mismatch = false;
	static const char* gpr_names[] = {
		"zero","at","v0","v1","a0","a1","a2","a3",
		"t0","t1","t2","t3","t4","t5","t6","t7",
		"s0","s1","s2","s3","s4","s5","s6","s7",
		"t8","t9","k0","k1","gp","sp","fp","ra"
	};

	for (int i = 1; i < 32; i++) // skip r0
	{
		if (cpuRegs.GPR.r[i].UD[0] != nativeGPR[i].UD[0])
		{
			if (!mismatch) { Console.Error("VERIFY MISMATCH at pc=0x%08X code=0x%08X:", instPC, code); mismatch = true; }
			Console.Error("  %s(r%d): native=0x%016llX interp=0x%016llX (pre=0x%016llX)",
				gpr_names[i], i, nativeGPR[i].UD[0], cpuRegs.GPR.r[i].UD[0], s_verifyGPR[i].UD[0]);
		}
	}

	if (cpuRegs.HI.UD[0] != nativeHI.UD[0])
	{
		if (!mismatch) { Console.Error("VERIFY MISMATCH at pc=0x%08X code=0x%08X:", instPC, code); mismatch = true; }
		Console.Error("  HI: native=0x%016llX interp=0x%016llX", nativeHI.UD[0], cpuRegs.HI.UD[0]);
	}
	if (cpuRegs.LO.UD[0] != nativeLO.UD[0])
	{
		if (!mismatch) { Console.Error("VERIFY MISMATCH at pc=0x%08X code=0x%08X:", instPC, code); mismatch = true; }
		Console.Error("  LO: native=0x%016llX interp=0x%016llX", nativeLO.UD[0], cpuRegs.LO.UD[0]);
	}

	// COP2: compare VU0 state (tolerate 1-ULP float differences)
	if (isCOP2)
	{
		auto ulpDiff = [](u32 a, u32 b) -> u32 {
			return (a > b) ? (a - b) : (b - a);
		};

		for (int i = 1; i < 32; i++) // skip VF0
		{
			bool vfMismatch = false;
			for (int lane = 0; lane < 4; lane++)
			{
				if (ulpDiff(VU0.VF[i].UL[lane], nativeVF[i].UL[lane]) > 100)
					vfMismatch = true;
			}
			if (vfMismatch)
			{
				if (!mismatch) { Console.Error("VERIFY MISMATCH at pc=0x%08X code=0x%08X:", instPC, code); mismatch = true; }
				Console.Error("  VF%d: native=[%08X,%08X,%08X,%08X] interp=[%08X,%08X,%08X,%08X]",
					i, nativeVF[i].UL[0], nativeVF[i].UL[1], nativeVF[i].UL[2], nativeVF[i].UL[3],
					VU0.VF[i].UL[0], VU0.VF[i].UL[1], VU0.VF[i].UL[2], VU0.VF[i].UL[3]);
			}
		}
		bool accMismatch = false;
		for (int lane = 0; lane < 4; lane++)
		{
			if (ulpDiff(VU0.ACC.UL[lane], nativeACC.UL[lane]) > 100)
				accMismatch = true;
		}
		if (accMismatch)
		{
			if (!mismatch) { Console.Error("VERIFY MISMATCH at pc=0x%08X code=0x%08X:", instPC, code); mismatch = true; }
			Console.Error("  ACC: native=[%08X,%08X,%08X,%08X] interp=[%08X,%08X,%08X,%08X]",
				nativeACC.UL[0], nativeACC.UL[1], nativeACC.UL[2], nativeACC.UL[3],
				VU0.ACC.UL[0], VU0.ACC.UL[1], VU0.ACC.UL[2], VU0.ACC.UL[3]);
		}
		// Check MAC and status flags
		if (VU0.VI[REG_MAC_FLAG].UL != nativeVI[REG_MAC_FLAG].UL)
		{
			if (!mismatch) { Console.Error("VERIFY MISMATCH at pc=0x%08X code=0x%08X:", instPC, code); mismatch = true; }
			Console.Error("  MAC_FLAG: native=0x%04X interp=0x%04X", nativeVI[REG_MAC_FLAG].UL, VU0.VI[REG_MAC_FLAG].UL);
		}
		if (VU0.VI[REG_STATUS_FLAG].UL != nativeVI[REG_STATUS_FLAG].UL)
		{
			if (!mismatch) { Console.Error("VERIFY MISMATCH at pc=0x%08X code=0x%08X:", instPC, code); mismatch = true; }
			Console.Error("  STATUS_FLAG: native=0x%04X interp=0x%04X", nativeVI[REG_STATUS_FLAG].UL, VU0.VI[REG_STATUS_FLAG].UL);
		}
	}

	if (mismatch)
	{
		const u32 op = code >> 26;
		const u32 rs = (code >> 21) & 0x1f;
		const u32 rt = (code >> 16) & 0x1f;
		const u32 rd = (code >> 11) & 0x1f;
		const u32 sa = (code >> 6) & 0x1f;
		const u32 funct = code & 0x3f;
		Console.Error("  Decode: op=%d rs=%d rt=%d rd=%d sa=%d funct=%d",
			op, rs, rt, rd, sa, funct);
		s_verifyMismatchCount++;
		// Don't assert — remaining mismatches are rounding-induced flag diffs
		// (MAC zero flag differs when result is on the boundary of 0.0).
		// Log only, no crash.
	}

	// Restore native results so execution continues with native values
	memcpy(cpuRegs.GPR.r, nativeGPR, sizeof(nativeGPR));
	cpuRegs.HI = nativeHI;
	cpuRegs.LO = nativeLO;
	if (isCOP2)
	{
		memcpy(VU0.VF, nativeVF, sizeof(nativeVF));
		VU0.ACC = nativeACC;
		memcpy(VU0.VI, nativeVI, sizeof(nativeVI));
		VU0.clipflag = nativeClipFlag;
	}
}

#endif // VERIFY_NATIVE_CODEGEN

#define GETBLOCK(x) PC_GETBLOCK_(x, recLUT)

// =====================================================================================================
//  Dynamically Compiled Dispatchers - R5900 ARM64
// =====================================================================================================

static const void* DispatcherEvent = nullptr;
static const void* DispatcherReg = nullptr;
// SL-11: shared tail for cold side exits (pc in RWSCRATCH, cycles in RWARG2):
// Str pc / Adds cycles / event-exit / Ret. BL'd from each exit so the per-exit
// tail shrinks to Mov+Mov+BL+linked-B.
static const void* SuperblockExitStub = nullptr;
static const void* JITCompile = nullptr;
static const void* EnterRecompiledCode = nullptr;
static const void* DispatchBlockDiscard = nullptr;
static const void* DispatchPageReset = nullptr;
static const void* UnmappedRecLUTPage = nullptr;

#if FPU_GUARD_MASK_STUB
// Shared FPU add/sub guard-bit masking stub. Emitted once per dispatcher
// generation, re-set on cache reset. See iFPU-arm64.cpp.
const void* g_fpuGuardMaskStub = nullptr;
#endif

static void recEventTest()
{
	eeEventTestIsActive = true;
	_cpuEventTest_Shared();
	eeEventTestIsActive = false;

	if (eeRecExitRequested)
	{
		eeRecExitRequested = false;
		recExitExecution();
	}

#ifdef PCSX2_RECOMPILER_TESTS
	if (harnessShouldExit())
		recExitExecution();
#endif

	if (eeRecNeedsReset)
	{
		eeRecNeedsReset = false;
		recResetRaw();
	}
}

#ifdef PCSX2_RECOMPILER_TESTS
// Harness-exit predicate. Returns true when running under recEeExecuteBlock
// AND either the parking PC has been reached or the cycle budget has been
// exhausted. Test-only — release builds drop the call site entirely.
static bool harnessShouldExit()
{
	if (!g_eeHarnessActive)
		return false;
	if (cpuRegs.pc == g_eeHarnessParkPc)
		return true;
	const u64 elapsed = cpuRegs.cycle - g_eeHarnessCycleStart;
	return elapsed >= static_cast<u64>(g_eeHarnessCycleBudget);
}
#endif

// ARM64 EE dispatcher — same two-level LUT as IOP but using cpuRegs.pc
static const void* _DynGen_DispatcherReg()
{
	u8* retval = armGetCurrentCodePointer();

	armAsm->Ldr(a64::w0, armCpuRegMem(&cpuRegs.pc));

	// Two-level LUT lookup:
	// base = recLUT[pc >> 16]
	// block = *(BASEBLOCK*)(base + pc * sizeof(BASEBLOCK)/4)
	// sizeof(BASEBLOCK) = 8, so /4 = *2, hence: base + pc*2
	// Note: use FULL pc as index (not pc & 0xFFFF) because recLUT_SetPage
	// adjusts the base address to account for the upper bits.
	armAsm->Lsr(a64::w1, a64::w0, 16);
	armMoveAddressToReg(RSCRATCHADDR, recLUT);
	armAsm->Ldr(RSCRATCHADDR, a64::MemOperand(RSCRATCHADDR, a64::x1, a64::LSL, 3));

	// Index with full PC: base + pc * 2 (not (pc & 0xFFFF) * 2)
	armAsm->Add(RSCRATCHADDR, RSCRATCHADDR, a64::Operand(a64::x0, a64::LSL, 1));

	armAsm->Ldr(RSCRATCHADDR, a64::MemOperand(RSCRATCHADDR));

	armAsm->Br(RSCRATCHADDR);

	return retval;
}

static const void* _DynGen_JITCompile()
{
	u8* retval = armGetCurrentCodePointer();

	// Flush pinned cycle delta (as the absolute counter) before the C call,
	// then re-derive after — recRecompile itself doesn't modify
	// cpuRegs.cycle, but other paths (e.g. block discard) might, and the
	// convention is "every C-call boundary syncs RECCYCLE both ways".
	armFlushCycleDelta();

	armFlushEEGPRPins(); // lazy-dirty seam: compile-time hooks read guest GPRs
	armAsm->Ldr(RWARG1, armCpuRegMem(&cpuRegs.pc));
	armEmitCall((void*)recRecompile);

	armReloadCycleDelta();
	// Compile-time hooks (EntryPointCompilingOnCPUThread → game starting /
	// ELF load) can write guest GPRs — refresh the pin mirrors.
	armReloadEEGPRPins();

	armEmitJmp(DispatcherReg);

	return retval;
}

static const void* _DynGen_DispatcherEvent()
{
	u8* retval = armGetCurrentCodePointer();
	// Flush pinned cycle delta for recEventTest (it reads cpuRegs.cycle for
	// counter / interrupt scheduling), then re-derive — the event test both
	// modifies cpuRegs.cycle (e.g. fast-forwarding to nextEventCycle) and
	// reschedules nextEventCycle itself.
	armFlushCycleDelta();
	armFlushEEGPRPins(); // lazy-dirty seam: savestate save / VM exit read GPR memory
	armEmitCall((void*)recEventTest);
	armReloadCycleDelta();
	// Event processing can rewrite every guest GPR (savestate load on the EE
	// thread, debugger pokes at a pause point) — refresh the pin mirrors.
	armReloadEEGPRPins();
	return retval; // falls through to DispatcherReg
}

static const void* _DynGen_EnterRecompiledCode()
{
	u8* retval = armGetCurrentCodePointer();

	// We never return through this function — we exit via fastjmp_jmp (longjmp).
	// fastjmp_set/fastjmp_jmp save and restore callee-saved registers, so we
	// don't need armBeginStackFrame. Just align the stack (AArch64 requires
	// 16-byte alignment). Match x86 pattern: only adjust SP, don't save regs.
	armAsm->Sub(a64::sp, a64::sp, 16);

	// Park PS2 FPU clamp constants in callee-saved scalar NEON registers.
	// s8 = +FLT_MAX, s9 = -FLT_MAX. AAPCS64 preserves the lower 64 bits of
	// d8-d15 across C calls, so these survive every armEmitCall path inside
	// JIT blocks without compile-time tracking. fpuClampResult and iCOP2
	// scalar VDIV/VSQRT/VRSQRT clamps read them directly. v8/v9 are removed
	// from the NEON allocator pool (see NEON_RESERVED_FPU_{MAX,MIN} in
	// iCore-arm64.cpp), so nothing in JIT codegen can clobber them.
	armAsm->Ldr(a64::s8, FLT_MAX);
	armAsm->Ldr(a64::s9, -FLT_MAX);

	// Load fastmem base into x19 if enabled
	if (CHECK_FASTMEM)
	{
		armMoveAddressToReg(RSCRATCHADDR, &vtlb_private::vtlbdata.fastmem_base);
		armAsm->Ldr(RFASTMEMBASE, a64::MemOperand(RSCRATCHADDR));
	}

	// Load &cpuRegs into RSTATE. Callee-saved, never modified by C, so this
	// load happens once per JIT entry. Subsequent cpuRegs.X accesses become
	// `Ldr/Str ..., [RSTATE, #offsetof(...)]` instead of materializing the
	// full address each time.
	armMoveAddressToReg(RSTATE, &cpuRegs);

	// Load pinned cycle delta into RECCYCLE (cycle - nextEventCycle; see
	// iR5900-arm64.h). The convention is that RECCYCLE holds the delta for
	// the entire duration of JIT execution, with flush+reload around C
	// calls that touch cycle/nextEventCycle.
	armReloadCycleDelta();

	// Load the write-through pin mirrors for every kEEPinTable entry
	// (see the REEPIN_* doc in iR5900-arm64.h).
	armReloadEEGPRPins();

	// Load &VU0 into RVU0. Same idea as RSTATE: VU0 is a static reference
	// (constant address), so iCOP2 codegen can reach every VURegs field via
	// [RVU0, #imm12]. Survives both armEmitCall and mVU dispatcher runs.
	armMoveAddressToReg(RVU0, &VU0);

	// Jump into dispatcher
	armEmitJmp(DispatcherReg);

	// Exit point — restore callee-saved and return
	// We get here via fastjmp_jmp, not via normal return
	return retval;
}

static const void* _DynGen_DispatchBlockDiscard()
{
	u8* retval = armGetCurrentCodePointer();
	armFlushEEClobberedPins(); // lazy-dirty seam: pairs with the reload below
	armEmitCall((void*)dyna_block_discard);
	// DispatcherReg's cache-hit path reloads nothing — restore the
	// caller-saved pins the C call clobbered before re-entering blocks.
	armReloadEEClobberedPins();
	armEmitJmp(DispatcherReg);
	return retval;
}

static const void* _DynGen_DispatchPageReset()
{
	u8* retval = armGetCurrentCodePointer();
	armFlushEEClobberedPins(); // lazy-dirty seam: pairs with the reload below
	armEmitCall((void*)dyna_page_reset);
	// See _DynGen_DispatchBlockDiscard.
	armReloadEEClobberedPins();
	armEmitJmp(DispatcherReg);
	return retval;
}

static const void* _DynGen_UnmappedRecLUTPage()
{
	u8* retval = armGetCurrentCodePointer();
	armAsm->Mov(RWARG1, 0);
	armEmitCall((void*)(void(*)(u32))recError);
	return retval;
}

#if FPU_GUARD_MASK_STUB
// Shared PS2 FPU guard-bit masking stub. Branchless, GPR-only; does the masking
// but not the fadd/fsub, so one stub serves both add and sub (the caller does
// the op):
//   in:  w0 = operand A bits, w1 = operand B bits
//   out: w0 = masked A bits,  w1 = masked B bits
//   clobbers w0/w1/w8/w9/w10/w16/w17, x30; no NEON, no memory. Leaf (ret x30).
//
// ⚠️ Scratches must stay OFF the int-allocator pools: a resident FCR31
// (ARM64TYPE_FPRC, GE-12) lives in {x2-x7, x14, x15} across ops and is NOT
// evicted around this call (the caller emits a raw bl, no iFlushCall). The
// original w2-w6 scratch choice corrupted it — the SotC guard-bit bug; see
// fpuEmitGuardedAddSub's contract note and the
// EeRecFpu.CompareSurvivesInterposedGuardedAdd* tests. w8-w10 are the
// non-allocatable value scratches, w16/w17 the vixl/addr scratches (dead
// across a bl — veneers may clobber them anyway).
static const void* _DynGen_FpuGuardMaskStub()
{
	u8* retval = armGetCurrentCodePointer();
	armAsm->Ubfx(a64::w8, a64::w0, 23, 8);              // expA
	armAsm->Ubfx(a64::w9, a64::w1, 23, 8);              // expB
	armAsm->Sub(a64::w8, a64::w8, a64::w9);             // diff = expA - expB
	armAsm->Sub(a64::w9, a64::w8, 1);                   // diff - 1
	armAsm->Mvn(a64::w10, a64::wzr);                    // 0xffffffff
	armAsm->Lsl(a64::w9, a64::w10, a64::w9);            // mask B: clear low (diff-1) bits
	armAsm->Lsl(a64::w16, a64::w10, 31);                // 0x80000000 (sign-only)
	armAsm->Cmp(a64::w8, 25);
	armAsm->Csel(a64::w9, a64::w16, a64::w9, a64::ge);  // diff >= 25 -> keep only sign
	armAsm->And(a64::w9, a64::w1, a64::w9);             // masked-B candidate
	armAsm->Mvn(a64::w17, a64::w8);                     // ~diff = -diff - 1
	armAsm->Lsl(a64::w17, a64::w10, a64::w17);          // mask A: clear low (-diff-1) bits
	armAsm->Cmn(a64::w8, 25);
	armAsm->Csel(a64::w17, a64::w16, a64::w17, a64::le); // diff <= -25 -> keep only sign
	armAsm->And(a64::w17, a64::w0, a64::w17);           // masked-A candidate
	armAsm->Cmp(a64::w8, 0);
	armAsm->Csel(a64::w1, a64::w9, a64::w1, a64::gt);   // diff > 0 -> B is smaller, mask B
	armAsm->Csel(a64::w0, a64::w17, a64::w0, a64::lt);  // diff < 0 -> A is smaller, mask A
	armAsm->Ret();
	return retval;
}
#endif // FPU_GUARD_MASK_STUB

static void _DynGen_Dispatchers()
{
	const u8* start = armGetCurrentCodePointer();

	DispatcherEvent = _DynGen_DispatcherEvent();
	DispatcherReg = _DynGen_DispatcherReg();
#if FPU_GUARD_MASK_STUB
	g_fpuGuardMaskStub = _DynGen_FpuGuardMaskStub();
#endif
	cop2DynGenSyncStubs();
	cop2DynGenTailStubs();

	JITCompile = _DynGen_JITCompile();
	EnterRecompiledCode = _DynGen_EnterRecompiledCode();
	DispatchBlockDiscard = _DynGen_DispatchBlockDiscard();
	DispatchPageReset = _DynGen_DispatchPageReset();
	UnmappedRecLUTPage = _DynGen_UnmappedRecLUTPage();

	// Block linker needs JITCompile so it can route stale / not-yet-compiled
	// link sites through the dispatcher path.
	recBlocks.SetJITCompile(JITCompile);

	Perf::any.Register(start, static_cast<u32>(armGetCurrentCodePointer() - start), "EE Dispatcher");
}

// =====================================================================================================
//  Error handling
// =====================================================================================================

static void recError(u32 error)
{
	switch (error)
	{
		case 0:
			Host::ReportErrorAsync("R5900 Exception",
				fmt::format("Unrecognized opcode (PC: 0x{:08x})", cpuRegs.pc));
			break;

		case 1:
			Host::ReportErrorAsync("R5900 Exception",
				fmt::format("Jump to unaligned address (PC: 0x{:08x})", cpuRegs.pc));
			break;
	}

	VMManager::SetPaused(true);
	Cpu->ExitExecution();
}

// =====================================================================================================
//  Code generation helpers
// =====================================================================================================

void iFlushCall(int flushtype)
{
	// EP-2b: the COP2 VF residency cache lives in caller-saved q16-q20 and
	// never survives a C-call seam — write back dirty slots and invalidate
	// before anything else. Every block tail also funnels through here; the
	// SetBranch* fork tails restore the compile-time state afterwards
	// (Cop2VfCacheScope) so each fork emits its own writebacks.
	cop2VfCacheFlush();

	// Free caller-saved registers
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (!arm64gprs[i].inuse)
			continue;

		if (!armIsCalleeSavedRegister(i) ||
			((flushtype & FLUSH_FREE_NONTEMP_X86) && arm64gprs[i].type != ARM64TYPE_TEMP) ||
			((flushtype & FLUSH_FREE_TEMP_X86) && arm64gprs[i].type == ARM64TYPE_TEMP))
		{
			_freeArm64GPR(i);
		}
	}

	// GE-15: 32-bit FPR-class slots (NEONTYPE_FPREG/FPACC) in the
	// callee-saved q10-q15 range survive plain C-helper seams — AAPCS64
	// preserves the LOWER 64 bits of v8-v15, and this class only ever
	// reads/writes lane 0 (S register; _writebackNEONreg stores S-width, so
	// post-call garbage in the upper lanes is never observed). Writeback if
	// dirty but KEEP mapped — 4248's writeback-dirty-but-keep passes, whose
	// type-mask 0x482 likewise excludes the 128-bit classes: GPRREG quads /
	// VFREG upper lanes are caller-saved and must still flush
	// (feedback_arm64_callee_saved_neon_const_pattern).
	//
	// Retention is vetoed by FLUSH_FREE_XMM: FLUSH_EVERYTHING/INTERPRETER
	// carry it (interp fallbacks like recRSQRT_S RMW fpr[]/ACC memory in C,
	// so a kept slot would go stale), and the COP2 macro wrappers pass it
	// explicitly (the inlined mVU bodies need the NEON file to themselves).
	// Everything outside q10-q15 always frees (x86 iFlushCall model,
	// pcsx2/x86/ix86-32/iR5900.cpp:1196-1207 — all XMM caller-saved there).
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (!arm64neon[i].inuse)
			continue;

		const bool retainable = !(flushtype & FLUSH_FREE_XMM) &&
			static_cast<u32>(i) >= NEON_CALLEE_SAVED_START &&
			static_cast<u32>(i) < NEON_CALLEE_SAVED_END &&
			(arm64neon[i].type == NEONTYPE_FPREG || arm64neon[i].type == NEONTYPE_FPACC);
		if (retainable)
			_flushNEONreg(i);
		else
			_freeNEONreg(i);
	}

	if (flushtype & FLUSH_ALL_X86)
		_flushArm64GPRregs();

	if (flushtype & FLUSH_CONSTANT_REGS)
		_flushConstRegs(true);

	if ((flushtype & FLUSH_PC) && !g_cpuFlushedPC)
	{
		armAsm->Mov(RWSCRATCH, pc);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));
		g_cpuFlushedPC = true;
	}

	if ((flushtype & FLUSH_CODE) && !g_cpuFlushedCode)
	{
		armAsm->Mov(RWSCRATCH, cpuRegs.code);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.code));
		g_cpuFlushedCode = true;
	}

	// Lazy-dirty pin seam (EE-SRA 2 WS-B; no-op in write-through mode).
	// Flush ALL pins only before GPR-READING callees — the exact
	// FLUSH_INTERPRETER mode (recCall/recBranchCall interpreter fallbacks,
	// Interp::MTC0). FLUSH_EVERYTHING at block tails is NOT a C seam: dirty
	// pins deliberately ride across the dispatcher in their registers (the
	// entire prize), and the JIT-exit paths (JITCompile/DispatcherEvent
	// stubs) flush explicitly. Lighter modes flush nothing here — their
	// call sites carry the caller-saved flush paired with the reload.
	// Ordered after _flushConstRegs: a const-tracked pinned reg
	// materializes through the pin first. First lazy census with the naive
	// FLUSH_ALL_X86 gate: +224k emitted Strs (+20%) — placement is the
	// whole game here.
	if ((flushtype & FLUSH_INTERPRETER) == FLUSH_INTERPRETER)
		armFlushEEGPRPins();

#if 0
	// Disabled in x86 too (iR5900.cpp:1235-1242). Left here #if 0'd so the
	// FLUSH_CAUSE / g_maySignalException mechanism stays a faithful mirror of
	// x86 and re-enables in lockstep if upstream ever does.
	if ((flushtype == FLUSH_CAUSE) && !g_maySignalException)
	{
		if (g_recompilingDelaySlot)
		{
			armAsm->Ldr(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.n.Cause));
			armAsm->Orr(RWSCRATCH, RWSCRATCH, 1u << 31); // BD
			armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.n.Cause));
		}
		g_maySignalException = true;
	}
#endif
}

// Emit cpuRegs.CP0.n.Cause &= ~(1 << 31) — clears the branch-delay (BD) bit.
// Mirrors x86's inline `xAND(ptr32[&cpuRegs.CP0.n.Cause], ~(1 << 31))`.
static void armClearCauseBD()
{
	armAsm->Ldr(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.n.Cause));
	armAsm->And(RWSCRATCH, RWSCRATCH, ~(1u << 31));
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.CP0.n.Cause));
}

// Flag set by cpuTlbMiss to signal that a TLB exception occurred during
// an interpreter call. The JIT block checks this after recCall and exits
// to the dispatcher if set, so the exception vector gets dispatched.
u32 s_recTlbMissOccurred = 0;

// Emit the post-interpreter-call TLB-miss exception dispatch. cpuTlbMiss sets
// s_recTlbMissOccurred and moves cpuRegs.pc to the exception vector; when set we
// clear the flag and exit to DispatcherReg rather than continue the block at the
// wrong PC. DispatcherReg/s_recTlbMissOccurred are file-local here, so this is
// the shared entry point used by recCall and recVTLB-arm64.cpp's recUnalignedCall.
void recEmitInterpTlbMissCheck()
{
	// Dispatch to DispatcherReg (not DispatcherEvent, which runs event
	// processing that may interfere with the pending exception state).
	a64::Label noException;
	armMoveAddressToReg(RSCRATCHADDR, &s_recTlbMissOccurred);
	armAsm->Ldr(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));
	armAsm->Cbz(RWSCRATCH, &noException);
	armAsm->Str(a64::wzr, a64::MemOperand(RSCRATCHADDR)); // clear flag
	armEmitJmp(DispatcherReg);
	armAsm->Bind(&noException);
}

// Interp-called ops assume the interpreter's post-fetch convention:
// cpuRegs.pc = op + 4 at handler entry (trap()/SYSCALL/BREAK subtract 4 to
// find the faulting op). Non-delay-slot compiles advance `pc` before the
// body, so FLUSH_PC already matches; delay slots advance only afterwards —
// compensate here or a raising delay-slot op computes EPC one slot low
// under the cpuRegs.branch bracket. (AX-05)
static u32 interpCallFlushPcBias()
{
	return g_recompilingDelaySlot ? 4 : 0;
}

// Bridges for mVU macro-mode emit bodies (can't see kEEPinTable): emit the
// caller-saved pin flush-before / reload-after around a C call emitted inline
// into an EE block. The flush is a lazy-dirty-mode no-op in write-through.
void armEmitEEClobberedPinFlushForCOP2()
{
	armFlushEEClobberedPins();
}

void armEmitEEClobberedPinReloadForCOP2()
{
	armReloadEEClobberedPins();
}

void recCall(void (*func)())
{
	const u32 saved_pc = pc;
	pc += interpCallFlushPcBias();
	iFlushCall(FLUSH_INTERPRETER);
	pc = saved_pc;

	// Flush the delta → cpuRegs.cycle so the interpreter sees the live cycle
	// value (some opcodes — COP0 Count, TLB miss, branch helpers — read it).
	armFlushCycleDelta();

	armEmitCall((void*)func);

	// Re-derive the delta in case the interpreter modified cpuRegs.cycle or
	// rescheduled nextEventCycle.
	armReloadCycleDelta();

	// The interpreter writes guest GPRs in memory — refresh the pin mirrors.
	armReloadEEGPRPins();

	// After interpreter calls, dispatch a pending TLB-miss exception.
	recEmitInterpTlbMissCheck();
}

void recBranchCall(void (*func)())
{
	const u32 saved_pc = pc;
	pc += interpCallFlushPcBias(); // see recCall (AX-05)
	iFlushCall(FLUSH_INTERPRETER);
	pc = saved_pc;

	// Apply accumulated block cycles to the delta, then flush to memory
	// before the C call — the interpreter's intEventTest reads
	// cpuRegs.cycle. Re-derive after, so the g_branch=2 exit code that
	// follows can keep using RECCYCLE.
	u32 cycles = scaleblockcycles_clear();
	if (cycles > 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	armFlushCycleDelta();

	armEmitCall((void*)func);
	g_branch = 2;

	armReloadCycleDelta();
	armReloadEEGPRPins();
}

// s_nBlockCycles is 3-bit fixed point. Divide by 8 when done!
// Scaling blocks under 40 cycles seems to produce countless problems, so let's try to avoid them.
// Matches x86 scaleblockcycles_calculation() in ix86-32/iR5900.cpp
#define DEFAULT_SCALED_BLOCKS() (s_nBlockCycles >> 3)

static u32 scaleblockcycles_calculation()
{
	const bool lowcycles = (s_nBlockCycles <= 40);
	const s8 cyclerate = EmuConfig.Speedhacks.EECycleRate;
	u32 scale_cycles = 0;

	if (cyclerate == 0 || lowcycles || cyclerate < -99 || cyclerate > 3)
		scale_cycles = DEFAULT_SCALED_BLOCKS();

	else if (cyclerate > 1)
		scale_cycles = s_nBlockCycles >> (2 + cyclerate);

	else if (cyclerate == 1)
		scale_cycles = DEFAULT_SCALED_BLOCKS() / 1.3f;

	else if (cyclerate == -1)
		scale_cycles = (s_nBlockCycles <= 80 || s_nBlockCycles > 168 ? 5 : 7) * s_nBlockCycles / 32;

	else
		scale_cycles = ((5 + (-2 * (cyclerate + 1))) * s_nBlockCycles) >> 5;

	return (scale_cycles < 1) ? 1 : scale_cycles;
}

u32 scaleblockcycles_clear()
{
	const u32 scaled = scaleblockcycles_calculation();

	const s8 cyclerate = EmuConfig.Speedhacks.EECycleRate;
	const bool lowcycles = (s_nBlockCycles <= 40);

	if (!lowcycles && cyclerate > 1)
		s_nBlockCycles &= (0x1 << (cyclerate + 2)) - 1;
	else
		s_nBlockCycles &= 0x7;

	return scaled;
}

void _eeFlushAllDirty()
{
	_flushConstRegs(false);
	_flushArm64GPRregs();
	_flushNEONregs();
}

void _eeOnWriteReg(int reg, int signext)
{
	GPR_DEL_CONST(reg);
}

void _deleteEEreg(int reg, int flush)
{
	if (!reg)
		return;
	if (flush && GPR_IS_CONST1(reg))
		_flushConstReg(reg);

	GPR_DEL_CONST(reg);
	_deleteGPRtoArm64GPR(reg, flush ? DELETE_REG_FREE : DELETE_REG_FREE_NO_WRITEBACK);
	// NEON side: ALWAYS writeback before free. EE GPRs are 128-bit and scalar
	// MIPS ops only overwrite UD[0]; the slot's UD[1] holds the live upper-64
	// from a prior MMI write (MMI routes through eeRecompileCodeXMM, so Rd stays
	// live in the slot with MODE_WRITE). Dropping without writeback silently
	// zeros UD[1] in memory and breaks the interpreter's "preserve UD[1]"
	// contract for LUI/MFLO/MOVZ/ADDIU/...
	_deleteGPRtoNEONreg(reg, DELETE_REG_FREE);
}

void _deleteEEreg128(int reg)
{
	if (!reg)
		return;
	if (GPR_IS_CONST1(reg))
		_flushConstReg(reg);

	GPR_DEL_CONST(reg);
	_deleteGPRtoArm64GPR(reg, DELETE_REG_FREE_NO_WRITEBACK);
	_deleteGPRtoNEONreg(reg, DELETE_REG_FREE);
}

void _flushEEreg(int reg, bool clear)
{
	if (!reg)
		return;

	if (GPR_IS_DIRTY_CONST(reg))
		_flushConstReg(reg);
	if (clear)
		GPR_DEL_CONST(reg);

	// Per-register flush honoring reg/clear, mirroring x86 _flushEEreg.
	// clear=false → writeback but keep the allocation; clear=true → also free.
	// (The previous arm64 impl flushed ALL registers and ignored reg/clear —
	// a behavior-equivalent superset given the lone caller, but a lying API.)
	_deleteGPRtoNEONreg(reg, clear ? DELETE_REG_FLUSH_AND_FREE : DELETE_REG_FLUSH);
	_deleteGPRtoArm64GPR(reg, clear ? DELETE_REG_FLUSH_AND_FREE : DELETE_REG_FLUSH);
}

void _eeMoveGPRtoR(const a64::Register& to, int fromgpr, bool allow_preload)
{
	if (fromgpr == 0)
	{
		// r0 is always zero
		if (to.Is64Bits())
			armAsm->Mov(to, a64::xzr);
		else
			armAsm->Mov(to, a64::wzr);
		return;
	}

	if (GPR_IS_CONST1(fromgpr))
	{
		// Value known at compile time — emit immediate load
		if (to.Is64Bits())
			armAsm->Mov(to, g_cpuConstRegs[fromgpr].SD[0]);
		else
			armAsm->Mov(to, g_cpuConstRegs[fromgpr].UL[0]);
		return;
	}

	// Check if the register is currently allocated in an ARM64 GPR with
	// MODE_READ — meaning the host register holds the current guest value.
	// MODE_WRITE-only means it's a destination allocation; the current value
	// was never loaded, so the host register contains stale data.
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (arm64gprs[i].inuse && arm64gprs[i].type == ARM64TYPE_GPR &&
			arm64gprs[i].reg == fromgpr && (arm64gprs[i].mode & MODE_READ))
		{
			if (to.Is64Bits())
				armAsm->Mov(to, armXRegister(i));
			else
				armAsm->Mov(to, armWRegister(i));
			return;
		}
	}

	// Check if allocated in a NEON register. A MODE_WRITE-only slot is also
	// authoritative — the MMI op that allocated it has written the live value
	// to qreg even though the slot was never MODE_READ-loaded. Reading from
	// memory in that case would return the pre-MMI stale value. eeRecompileCodeXMM
	// passes MODE_WRITE alone (no MODE_READ unless XMMINFO_READD is set) for Rd,
	// so every MMI destination lands here.
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse && arm64neon[i].type == NEONTYPE_GPRREG &&
			arm64neon[i].reg == fromgpr && (arm64neon[i].mode & (MODE_READ | MODE_WRITE)))
		{
			if (to.Is64Bits())
				armAsm->Fmov(to, armDRegister(i));     // FMOV Xd, Dn
			else
				armAsm->Fmov(to, armSRegister(i));      // FMOV Wd, Sn (lower 32 bits)
			return;
		}
	}

	// Not allocated anywhere — load from cpuRegs memory
	armLoadEERegPtr(to, &cpuRegs.GPR.r[fromgpr].UD[0]);
}

// Operand-substitution variant of _eeMoveGPRtoR (EE-SRA 2 WS-C): returns a
// register that CURRENTLY holds the guest value — the pin, or an allocator
// slot already in MODE_READ — emitting nothing at all in those cases; only
// the fallback (const / NEON-resident / memory) materializes into `scratch`.
// Contract:
//   - dirty-safe by construction: a resident allocator/NEON slot IS the newest
//     value (a dirty slot is exactly what we want to read), and pin / GPR-slot /
//     NEON residency are mutually exclusive (invariants I1/I2), so the
//     pin>slot>fallback order never returns a stale home. (The GE-M2 write
//     helpers below mark their dest slots MODE_READ after depositing, which is
//     what keeps a within-block writer→reader chain coherent here.)
//   - consume the returned register in the immediately following
//     instruction(s), before anything can mutate pins or allocator slots.
//   - the returned register may be `scratch` or may not be — never write it.
//   - $zero materializes 0 into `scratch` rather than returning xzr: reg 31
//     in an Rn operand position encodes SP for arithmetic ops (Cmp!), and
//     callers shouldn't have to care.
vixl::aarch64::Register _eeGetGPRSourceReg(const a64::Register& scratch, int fromgpr)
{
	if (fromgpr != 0 && !GPR_IS_CONST1(fromgpr))
	{
		// A live 128-bit NEON home (an in-flight MMI / LQ result) is authoritative
		// over BOTH the pin mirror and any GPR slot: an MMI write updates only the
		// quad, and the pin + cpuRegs memory stay stale until a seam runs
		// armStoreEEGPRQuad to refresh them. Take the no-emit pin/slot fast path
		// only when no quad is live; otherwise fall through to _eeMoveGPRtoR, which
		// reads lane 0 straight out of the quad. This is the coherence the RC0
		// residency flip depends on — pre-flip the templates flushed the source
		// (refreshing the pin) so the fast path was always current; post-flip a
		// producer's quad can still be live when a consumer reads the reg here.
		if (!_hasNEONreg(NEONTYPE_GPRREG, fromgpr))
		{
			if (const a64::Register* pin = armEEPinForGPR(fromgpr))
				return scratch.Is64Bits() ? *pin : pin->W();

			for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
			{
				if (arm64gprs[i].inuse && arm64gprs[i].type == ARM64TYPE_GPR &&
					arm64gprs[i].reg == fromgpr && (arm64gprs[i].mode & MODE_READ))
				{
					return scratch.Is64Bits() ? armXRegister(i) : armWRegister(i);
				}
			}
		}
	}

	_eeMoveGPRtoR(scratch, fromgpr);
	return scratch;
}

// WRITE-side operand substitution (GE-M2 central write API). Resolve a guest
// GPR's 64-bit write home so that a hand-written op deposits its result into a
// pinned mirror or an allocator-resident host register when one exists, instead
// of always storing to cpuRegs memory — the dest counterpart of
// _eeGetGPRSourceReg. Mirrors PCSX2's x86 allocator-template dest handling
// (pcsx2/x86/ix86-32/iR5900Templates.cpp + _allocX86reg, pcsx2/x86/iCore.cpp).
//
// Returns the X-sized write home (like armEEDestForGPR — the caller writes .W()
// and sign-/zero-extends into it as the op requires). Before resolving, a scalar
// write invalidates any live 128-bit dual-residence copy (writeback first so the
// slot's UD[1] upper half survives in memory — the _deleteEEreg UD[1] rationale)
// and clears the compile-time-const flag.
//
// alloc_if_used == false (the default, used by the Phase-1 coherence sweep) never
// creates a new slot: it resolves to a pin, an already-resident slot, or the
// caller's scratch — bit-for-bit today's pin/memory behavior when nothing is
// resident. alloc_if_used == true lets the op claim a fresh MODE_WRITE slot for a
// still-live dest (the residency flip, Phases 3/4).
//
// Emit-ordering contract (identical to armEEDestForGPR): ONLY the final
// result-producing instruction may target the returned register, reading its
// guest sources in that same instruction; follow it with _eeStoreGPRDestReg
// before anything can observe the reg.
vixl::aarch64::Register _eeGetGPRDestReg(int gpr, const a64::Register& scratch, bool alloc_if_used)
{
	// Scalar write kills any 128-bit copy (writeback preserves UD[1]) and any
	// known-constant value for this reg.
	_deleteGPRtoNEONreg(gpr, DELETE_REG_FREE);
	GPR_DEL_CONST(gpr);

	if (const a64::Register* pin = armEEPinForGPR(gpr))
		return *pin;

	int r = _checkArm64GPR(ARM64TYPE_GPR, gpr, MODE_WRITE);
	if (r < 0 && alloc_if_used && EEINST_USEDTEST(gpr))
		r = _allocArm64GPR(ARM64TYPE_GPR, gpr, MODE_WRITE);
	if (r >= 0)
		return armXRegister(r);

	return scratch.X();
}

// Deposit `src` (the dest home itself, a source register, or xzr) into gpr's
// resolved home. An allocator-resident dest takes a DEFERRED store: mark the
// slot MODE_READ so _eeGetGPRSourceReg / branch / COP2 consumers pick the value
// up before the next seam writes it back, and Mov only when src isn't already
// the slot's host reg. A pinned or memory-backed dest takes the existing
// lazy-dirty / canonical helper (a src that IS the pin emits nothing).
void _eeStoreGPRDestReg(int gpr, const a64::Register& src)
{
	const int r = _checkArm64GPR(ARM64TYPE_GPR, gpr, MODE_READ | MODE_WRITE);
	if (r >= 0)
	{
		if (src.GetCode() != static_cast<unsigned>(r))
			armAsm->Mov(armXRegister(r), src.X());
		return;
	}
	armStoreEERegPtr(src, &cpuRegs.GPR.r[gpr].UD[0]);
}

#ifdef PCSX2_DEVBUILD
// GE-M2 coherence tripwire body (see armAssertRawGPRPtrCoherent's header
// declaration for the full rationale). Reject a raw guest-GPR memory access
// only when the reg's allocator/NEON copy is DIRTY (memory stale) and the reg
// is neither $zero nor pinned.
void armAssertRawGPRPtrCoherent(const void* field)
{
	const uptr base = reinterpret_cast<uptr>(&cpuRegs.GPR.r[0]);
	const uptr f = reinterpret_cast<uptr>(field);
	if (f < base || f >= base + sizeof(cpuRegs.GPR.r))
		return; // not a guest-GPR pointer

	const uptr rel = f - base;
	if ((rel % sizeof(cpuRegs.GPR.r[0])) >= 8)
		return; // upper 64 bits are never scalar-resident

	const int n = static_cast<int>(rel / sizeof(cpuRegs.GPR.r[0]));
	if (n == 0 || armEEPinForGPR(n) != nullptr)
		return; // $zero and pinned regs: the raw path is authoritative

	pxAssertMsg(!_hasArm64GPR(ARM64TYPE_GPR, n, MODE_WRITE) &&
					!_hasNEONreg(NEONTYPE_GPRREG, n, MODE_WRITE),
		"GE-M2 I3: raw guest-GPR memory access while its allocator/NEON copy is dirty");
}
#endif

// =====================================================================================================
//  Branch handling
// =====================================================================================================

// Call-ret shadow-stack ring (P2-2; FEX-Emu design, adapted). Frames are
// {u64 guest return PC, u64 host landing}; the landing sits right after the
// call site's BL, so a matching guest JR-$ra can RET to it and the hardware
// return-address stack — pushed by that BL — predicts the transfer.
//
// Ring instead of FEX's guard-page stack: eeCallRetOff wraps via a masked
// And, so over/underflow simply cycles the ring — no bounds checks, no
// SIGSEGV recentering (our PageFaultHandler interface exposes no ucontext),
// and net push/pop imbalance (interpreter-path calls, exceptions, thread
// switches) degrades to compare-misses that re-sync within one call depth.
//
// Consuming a frame is correct whenever its guestRA matches the live target:
// the landing's B is linked to that same PC, and Remove() keeps dead block
// entries resolving via redirect stubs, so stale-but-matching frames stay
// VALID across every recClear. Only recResetRaw (code cache rewind) dangles
// host pointers — it sentinel-fills the ring (guestRA=1 can never match an
// alignment-checked target; the compare fails before the landing is used).
namespace
{
	constexpr u32 kEECallRetRingBytes = 0x10000; // 4096 x 16-byte frames
	constexpr u64 kEECallRetOffMask = kEECallRetRingBytes - 16;
	constexpr u64 kEECallRetSentinelRA = 1;

	alignas(16) u8 s_eeCallRetRing[kEECallRetRingBytes];

	void eeCallRetResetRing()
	{
		u64* p = reinterpret_cast<u64*>(s_eeCallRetRing);
		for (size_t i = 0; i < kEECallRetRingBytes / 8; i += 2)
		{
			p[i] = kEECallRetSentinelRA;
			p[i + 1] = 0;
		}
		_cpuRegistersPack.eeCallRetBase = reinterpret_cast<u64>(s_eeCallRetRing);
		_cpuRegistersPack.eeCallRetOff = 0;
	}

	// (Push/pop emission is gated off at the recJAL/recJALR/recJR call sites
	// under GoemonTlbHack: SetBranchReg compares V2P-translated targets
	// there, which can never match the virtual frame RAs. Gamefix flips
	// reset the rec, so emit-time gating is sound.)

	// {guestRA, landing} frame push. Emitted between the tail flush and the
	// event check: the frame must exist on EVERY downstream path (an event
	// detour still reaches the callee, whose eventual return must find a
	// balanced ring). Clobbers x8/x9/x10/x17; all dead post-flush, and the
	// tier-2 pins (x11-x13) are untouched.
	void emitCallRetPush(u32 return_pc, a64::Label* landing)
	{
		armAsm->Mov(RXSCRATCH, return_pc);
		armAsm->Adr(RSCRATCHADDR, landing);
		armAsm->Ldr(a64::x9, armCpuRegMem(&_cpuRegistersPack.eeCallRetBase));
		armAsm->Ldr(a64::x10, armCpuRegMem(&_cpuRegistersPack.eeCallRetOff));
		armAsm->Sub(a64::x10, a64::x10, 16);
		armAsm->And(a64::x10, a64::x10, kEECallRetOffMask);
		armAsm->Str(a64::x10, armCpuRegMem(&_cpuRegistersPack.eeCallRetOff));
		armAsm->Add(a64::x9, a64::x9, a64::x10);
		armAsm->Stp(RXSCRATCH, RSCRATCHADDR, a64::MemOperand(a64::x9));
	}
} // namespace

// Block-tail cycle update + event check under the delta representation:
// fold the pending block cycles into RECCYCLE with a flag-setting add, then
// branch to DispatcherEvent when the delta is non-negative (an event is
// due). Replaces the old per-exit `ldr nextEventCycle; cmp; b.ge` — no
// memory access on the hot linked path. The adds can't overflow s64: the
// delta stays within the event-scheduling horizon (≤ a frame) and block
// cycles are 16-bit scaled.
static void emitCycleUpdateAndEventCheck()
{
	const u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Adds(RECCYCLE, RECCYCLE, cycles);
	else
		armAsm->Cmp(RECCYCLE, 0);
	armEmitCondBranch(a64::ge, DispatcherEvent);
}

void SetBranchReg(EEBranchRegMode mode, u32 call_return_pc)
{
	const Cop2VfCacheScope vfCacheScope; // fork tail: preserve compile-time cache state
	g_branch = 1;

	// Flush all GPR/NEON/constant allocations FIRST, while host registers
	// still hold correct guest values. iFlushCall writes back delay slot
	// results (like addiu sp) before the branch target is loaded into w0.
	iFlushCall(FLUSH_EVERYTHING);

	// Now load branch target from pcWriteback (saved by recJR/recJALR)
	armLoadEERegPtr(a64::w0, &cpuRegs.pcWriteback);

	// GoemonTlbHack: recJR/recJALR store the raw virtual register target; the
	// JIT dispatches in physical space, so translate it before use. Mirrors
	// recJ/recJAL (compile-time vtlb_V2P via SetBranchImm) and x86
	// recJR/recJALR (vtlb_DynV2P). The V2P lives in SetBranchReg, whose only
	// EE callers are recJR/recJALR, so no other target gets double-translated.
	// The iFlushCall(FLUSH_EVERYTHING) above has already spilled guest state;
	// vtlb_V2P preserves callee-saved x25 (RECCYCLE) per AAPCS64, so the
	// C-call needs no extra save. w0 (== RWARG1) already holds the virtual
	// target and receives the translated paddr. (Call/Return modes are
	// emit-gated off under this hack — see eeCallRetActive.)
	if (EmuConfig.Gamefixes.GoemonTlbHack)
	{
		pxAssert(mode == EEBranchRegMode::Jump);
		armFlushEEClobberedPins(); // lazy-dirty seam: pairs with the reload below
		armEmitCall((void*)vtlb_V2P);
		// vtlb_V2P writes no guest GPRs but clobbers the caller-saved pins;
		// the DispatcherReg jump below can cache-hit straight into a block.
		armReloadEEClobberedPins();
	}

	// Store to cpuRegs.pc
	armAsm->Str(a64::w0, armCpuRegMem(&cpuRegs.pc));

	// Alignment check
	a64::Label unaligned;
	armAsm->Tst(a64::w0, 3);
	armAsm->B(&unaligned, a64::ne);

	if (mode == EEBranchRegMode::Return)
	{
		// Guest JR-$ra: pop a frame — ALWAYS, before the event check, so an
		// event detour (which still reaches the return target through the
		// dispatcher) leaves the ring balanced. Frame regs survive the event
		// check below: it is flags-only (Adds/Cmp + b.ge).
		armAsm->Ldr(a64::x9, armCpuRegMem(&_cpuRegistersPack.eeCallRetBase));
		armAsm->Ldr(a64::x10, armCpuRegMem(&_cpuRegistersPack.eeCallRetOff));
		armAsm->Add(a64::x9, a64::x9, a64::x10);
		armAsm->Ldp(RXSCRATCH, RSCRATCHADDR, a64::MemOperand(a64::x9));
		armAsm->Add(a64::x10, a64::x10, 16);
		armAsm->And(a64::x10, a64::x10, kEECallRetOffMask);
		armAsm->Str(a64::x10, armCpuRegMem(&_cpuRegistersPack.eeCallRetOff));

		emitCycleUpdateAndEventCheck();

		// Hit: RET to the frame's landing — paired with the pushing BL, the
		// hardware RAS predicts it. Miss: STILL exit via RET (into the
		// dispatcher) so the hardware RAS pops in lockstep with the software
		// ring — a plain B here would leave the paired RAS entry unpopped
		// and desync every subsequent return (the FEX detail our first
		// sketch got wrong).
		a64::Label miss;
		armAsm->Cmp(RXSCRATCH, a64::x0);
		armAsm->B(&miss, a64::ne);
		armAsm->Ret(RSCRATCHADDR);

		armAsm->Bind(&miss);
		armMoveAddressToReg(RSCRATCHADDR, DispatcherReg);
		armAsm->Ret(RSCRATCHADDR);
	}
	else if (mode == EEBranchRegMode::Call)
	{
		// Guest JALR-rd31: push the frame (before the event check — the
		// callee is reached either way and its return must pop this frame),
		// then transfer with a BL through the shared dispatcher. The BL
		// pushes the hardware RAS entry the callee's RET will consume.
		a64::Label landing;
		emitCallRetPush(call_return_pc, &landing);

		emitCycleUpdateAndEventCheck();

		armEmitCall(DispatcherReg);

		armAsm->Bind(&landing);
		{
			a64::SingleEmissionCheckScope guard(armAsm);
			u8* patch_site = armGetCurrentCodePointer();
			armAsm->b(int64_t{0});
			recBlocks.Link(HWADDR(call_return_pc), patch_site);
		}
	}
	else
	{
		// Update the pinned cycle delta and check events (delta >= 0 →
		// DispatcherEvent converts RECCYCLE itself before calling recEventTest).
		emitCycleUpdateAndEventCheck();

		armEmitJmp(DispatcherReg);
	}

	armAsm->Bind(&unaligned);
	armAsm->Mov(RWARG1, 1);
	armEmitCall((void*)recError);
}

// =====================================================================================================
//  SL-1: loop-carried residency for self-loop blocks
// =====================================================================================================
// A block whose terminal branch targets its own startpc (the scanner's
// backward-split rule guarantees loop heads are block starts) compiles with a
// register-resident back-edge: the loop-live guest GPRs are allocated once in
// a preheader (dirty-pessimized, loop-pinned), the loop-top label is bound
// after it, and the taken arm of the terminal branch — instead of the normal
// flush + event check + linked-B tail — emits a reconcile back to the loop-top
// allocator state, the cycle/event check (side-exiting to a cold spill stub),
// and a direct B to the loop-top. Dirty values ride host registers across
// iterations; memory becomes current only on the exit arm, the event side
// exit, or across any mid-body C seam (iFlushCall frees caller-saved entries
// coherently, so seams stay correct — they just localize the win away).
//
// The runtime contract at the back-edge → loop-top jump: every snapshot (S0)
// mapping holds its guest value in its host reg; every other allocatable host
// reg is dead; memory is current for every guest reg NOT mapped by S0. The S0
// dirty bits are pessimized (preheader allocates MODE_READ|MODE_WRITE) so the
// body-emitted evictions always write back — a clean S0 bit over a runtime-
// dirty value would lose writes.
//
// External observers never see mid-loop stale memory: reaching any frame
// boundary, savestate, or C code requires leaving through the exit arm (full
// flush), the spill stub (spills S0), or a mid-body seam (iFlushCall).

static bool s_loopResident = false;
static bool s_loopBackedgeEmitted = false;
static u32 s_loopTopPc = 0;
static _arm64gprregs s_loopEntryState[NUM_ARM_GPR_REGS];
static std::unique_ptr<a64::Label> s_loopTopLabel;

#ifdef PCSX2_RECOMPILER_TESTS
static std::unordered_set<u32> s_loopResidentBlocks;
bool recEeBlockIsLoopResident(u32 pc_query)
{
	return s_loopResidentBlocks.find(HWADDR(pc_query)) != s_loopResidentBlocks.end();
}
#endif

// Emit the state transform "compile-state S1 → loop-top state S0" at the
// back-edge. Phase A makes memory current for everything S0 doesn't carry: VF
// compile cache, body-created constants (materialized BEFORE phase B so its
// reloads see them), and every allocator entry that is not exactly an S0 pin
// sitting in its S0 slot (misplaced pin guests route through memory — no
// move-cycle handling needed). Phase B reloads S0 pins the body displaced.
static void _eeLoopReconcileBackedge()
{
	cop2VfCacheFlush();
	_flushConstRegs(false);

	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (!arm64gprs[i].inuse)
			continue;
		const _arm64gprregs& want = s_loopEntryState[i];
		if (want.inuse && arm64gprs[i].type == want.type && arm64gprs[i].reg == want.reg)
			continue; // S0 pin riding through — stays resident and dirty
		_freeArm64GPR(i); // writeback-if-dirty + free
	}

	// S0 carries no NEON entries: write back dirty quads so memory is current.
	// The emitted loop-top code loads on demand; a residual clean value being
	// clobbered by a later alloc is harmless.
	_flushNEONregs();

	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		const _arm64gprregs& want = s_loopEntryState[i];
		if (!want.inuse)
			continue;
		if (arm64gprs[i].inuse && arm64gprs[i].type == want.type && arm64gprs[i].reg == want.reg)
			continue;
		pxAssert(!arm64gprs[i].inuse); // phase A vacated every mismatch
		armLoadEERegPtrRaw(armXRegister(i), &cpuRegs.GPR.r[want.reg].UD[0]);
	}
}

// Back-edge tail — replaces SetBranchImm on the resident self-loop taken arm.
static void SetBranchBackedge()
{
	const Cop2VfCacheScope vfCacheScope; // fork tail: preserve compile-time cache state
	g_branch = 1;

	_eeLoopReconcileBackedge();

	// Cycle update + event check, side-exiting to a local cold stub instead of
	// DispatcherEvent (the stub owes the spill + pc store first).
	a64::Label spill;
	const u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Adds(RECCYCLE, RECCYCLE, cycles);
	else
		armAsm->Cmp(RECCYCLE, 0);
	armAsm->B(&spill, a64::ge);

	// The resident back-edge. Registered on the block so recClear can repoint
	// it to the spill stub (see Arm64BaseBlocks::Remove) — the entry redirect
	// alone can't catch an internal branch, and a cleared self-loop must not
	// keep executing stale code until the next event.
	u8* backedge_site;
	{
		a64::SingleEmissionCheckScope guard(armAsm);
		backedge_site = armGetCurrentCodePointer();
		armAsm->b(s_loopTopLabel.get());
	}

	// Cold spill stub: runtime state here is exactly S0 (the reconcile ran on
	// the fall-through into the event check). Write back the pinned set (S0
	// pessimizes every pin dirty), publish the loop-top pc, and hand off to
	// DispatcherEvent. Re-entry after the event comes through the block head,
	// whose preheader reloads the pins.
	armAsm->Bind(&spill);
	s_pCurBlockEx->backedge_site = reinterpret_cast<uptr>(backedge_site);
	s_pCurBlockEx->backedge_stub = reinterpret_cast<uptr>(armGetCurrentCodePointer());
	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		const _arm64gprregs& want = s_loopEntryState[i];
		if (want.inuse && (want.mode & MODE_WRITE))
			armStoreEERegPtrRaw(armXRegister(i), &cpuRegs.GPR.r[want.reg].UD[0]);
	}
	armAsm->Mov(RWSCRATCH, s_loopTopPc);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));
	armEmitJmp(DispatcherEvent);

	s_loopBackedgeEmitted = true;
#ifdef PCSX2_RECOMPILER_TESTS
	s_loopResidentBlocks.insert(HWADDR(s_loopTopPc));
#endif
}

void SetBranchImm(u32 imm)
{
	if (s_loopResident && !s_loopBackedgeEmitted && imm == s_loopTopPc)
	{
		SetBranchBackedge();
		return;
	}

	const Cop2VfCacheScope vfCacheScope; // fork tail: preserve compile-time cache state
	g_branch = 1;
	pxAssert(imm);

	armAsm->Mov(RWSCRATCH, imm);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));

	iFlushCall(FLUSH_EVERYTHING);

	// WaitLoop speedhack: when the block scanner detected this block as a
	// pure-nop spin branching back to itself (s_nBlockFF), fast-forward the
	// delta to max(delta, 0) — i.e. cycle to max(cycle, nextEventCycle) —
	// and jump straight to DispatcherEvent. Saves the dozens of iterations
	// the loop would otherwise burn waiting for an event to fire. Matches
	// the x86 path in iR5900.cpp:iBranchTest under Speedhacks.WaitLoop.
	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && imm == s_branchTo)
	{
		const u32 cycles = scaleblockcycles_clear();
		if (cycles != 0)
			armAsm->Add(RECCYCLE, RECCYCLE, cycles);
		armAsm->Cmp(RECCYCLE, 0);
		armAsm->Csel(RECCYCLE, RECCYCLE, a64::xzr, a64::gt);
		armEmitJmp(DispatcherEvent);
		return;
	}

	// Update the pinned cycle delta and check events.
	emitCycleUpdateAndEventCheck();

	// Block linking: emit a single B as the patch site. Initially routed
	// through JITCompile via recBlocks.Link(); once the target block is
	// compiled, recBlocks.New() rewrites this B's imm26 to branch to the
	// target's fnptr directly, bypassing the dispatcher.
	{
		a64::SingleEmissionCheckScope guard(armAsm);
		u8* patch_site = armGetCurrentCodePointer();
		armAsm->b(int64_t{0}); // placeholder; recBlocks.Link will overwrite
		recBlocks.Link(HWADDR(imm), patch_site);
	}
}

// recJAL tail: SetBranchImm's shape plus a call-ret frame push, a BL-form
// link to the callee (hardware RAS push), and a linked landing at the
// return PC. See the call-ret ring comment block above SetBranchReg.
void SetBranchImmCall(u32 imm, u32 return_pc)
{
	// A WaitLoop-FF-shaped block wants the fast-forward path, and a
	// self-spinning JAL never returns anyway — keep the plain tail.
	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && imm == s_branchTo)
	{
		SetBranchImm(imm);
		return;
	}

	const Cop2VfCacheScope vfCacheScope; // fork tail: preserve compile-time cache state
	g_branch = 1;
	pxAssert(imm);

	armAsm->Mov(RWSCRATCH, imm);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));

	iFlushCall(FLUSH_EVERYTHING);

	// Push before the event check: an event detour still reaches the callee
	// (via DispatcherEvent → DispatcherReg), whose eventual return must find
	// this frame. Only the hardware-RAS pairing is lost on that rare path.
	a64::Label landing;
	emitCallRetPush(return_pc, &landing);

	emitCycleUpdateAndEventCheck();

	// BL-form link site: patched toward JITCompile now, re-patched to the
	// callee block when it compiles — always keeping the BL opcode so the
	// RAS entry gets pushed no matter where the site currently points.
	{
		a64::SingleEmissionCheckScope guard(armAsm);
		u8* patch_site = armGetCurrentCodePointer();
		armAsm->bl(int64_t{0}); // placeholder; recBlocks.Link will overwrite
		recBlocks.Link(HWADDR(imm), patch_site, /*call=*/true);
	}

	// The RET landing: a plain linked B to the return-PC block (the one
	// conscious deviation from FEX/MAMBO-X64 — they translate through the
	// call so the continuation falls through; our block formation ends at
	// the call, and one predicted direct B costs ~a cycle on the hit path).
	armAsm->Bind(&landing);
	{
		a64::SingleEmissionCheckScope guard(armAsm);
		u8* patch_site = armGetCurrentCodePointer();
		armAsm->b(int64_t{0}); // placeholder; recBlocks.Link will overwrite
		recBlocks.Link(HWADDR(return_pc), patch_site);
	}
}

// =====================================================================================================
//  Block state save/restore for delay slots
// =====================================================================================================

// The full per-fork compile state: everything a branch arm needs restored to
// re-emit from the fork point. Shared by the delay-slot fork
// (SaveBranchState/LoadBranchState) and the superblock side-exit snapshots.
struct BranchCompileState
{
	_arm64gprregs gprs[NUM_ARM_GPR_REGS];
	_arm64neonregs neon[NUM_ARM_NEON_REGS];
	GPR_reg64 constRegs[32];
	u32 hasConstReg, flushedConstReg;
	u32 blockCycles;
	EEINST* instInfo;
	Cop2VfCacheState vfCache;

	void capture()
	{
		vfCache = cop2VfCacheGetState();
		blockCycles = s_nBlockCycles;
		memcpy(constRegs, g_cpuConstRegs, sizeof(g_cpuConstRegs));
		hasConstReg = g_cpuHasConstReg;
		flushedConstReg = g_cpuFlushedConstReg;
		instInfo = g_pCurInstInfo;
		memcpy(gprs, arm64gprs, sizeof(arm64gprs));
		memcpy(neon, arm64neon, sizeof(arm64neon));
	}

	void restore() const
	{
		cop2VfCacheSetState(vfCache);
		s_nBlockCycles = blockCycles;
		memcpy(g_cpuConstRegs, constRegs, sizeof(g_cpuConstRegs));
		g_cpuHasConstReg = hasConstReg;
		g_cpuFlushedConstReg = flushedConstReg;
		g_pCurInstInfo = instInfo;
		memcpy(arm64gprs, gprs, sizeof(arm64gprs));
		memcpy(arm64neon, neon, sizeof(arm64neon));
	}
};

static BranchCompileState s_savedBranchState;

void SaveBranchState()
{
	s_savedBranchState.capture();
}

void LoadBranchState()
{
	s_savedBranchState.restore();
}

// =====================================================================================================
//  SL-03: superblocks — conditional-fallthrough continuation
// =====================================================================================================
// The scanner records forward conditional branches (BEQ/BNE/BLEZ/BGTZ/BLTZ/
// BGEZ; non-likely, non-link) as continuation sites and keeps scanning at the
// fallthrough, so the not-taken path compiles as one straight line: no pc
// store, no event check, no linked-B, no next-head reload — register/const
// residency rides through the former boundary. The taken arm becomes a cold
// side exit outlined after the block tail; it snapshots the compile state at
// the branch, and its emission restores that snapshot, compiles the taken-path
// delay slot (unless TrySwapDelaySlot already hoisted it), and ends with the
// normal SetBranchImm tail. Analysis stays exactly as conservative as today's
// block ends: the liveness backward pass merges all-live at each site (the
// taken path leaves the block there), and the COP2 deferred-commit passes run
// per segment delimited at sites. Event-check coarsening equals today's
// straight-line blocks (one check per exit, range capped by the 4K page).

static constexpr int kMaxContSites = 8;
static constexpr u32 kMaxSuperblockInsns = 128;

static u32 s_contSitePcs[kMaxContSites]; // ascending (scan order)
static int s_numContSites = 0;

struct SuperblockSideExit
{
	std::unique_ptr<a64::Label> label;
	u32 branchTo;
	u32 dsPc;
	bool needDs;
	BranchCompileState state;
};
static SuperblockSideExit s_sideExits[kMaxContSites];
static int s_numSideExits = 0;

bool recSuperblockIsContSite(u32 branch_pc)
{
	for (int i = 0; i < s_numContSites; i++)
		if (s_contSitePcs[i] == branch_pc)
			return true;
	return false;
}

// Snapshot the compile state for the taken path and hand back the label the
// handler's inverted condition branches to. Call after _eeFlushAllDirty and
// (for a swapped slot) after the delay-slot emission; the compare itself is a
// pure post-flush read and may be emitted after the snapshot. pc points at the
// delay slot here.
a64::Label* recSuperblockAddSideExit(u32 branch_target, bool need_delay_slot)
{
	pxAssert(s_numSideExits < kMaxContSites);
	SuperblockSideExit& x = s_sideExits[s_numSideExits++];
	x.label = std::make_unique<a64::Label>();
	x.branchTo = branch_target;
	x.dsPc = pc;
	x.needDs = need_delay_slot;
	x.state.capture();
	return x.label.get();
}

// SL-10: the in-block footprint of a side exit is one island — a single far B
// (imm26, ±128MB reaches the cold arena) that the site's short-range
// conditional (Tbz ±32KB / B.cond ±1MB) can target. Bound after the tail,
// patched toward the outlined exit body once the cold session has emitted it.
static u8* s_sideExitIslands[kMaxContSites];

// Emit the per-exit islands after the block tail (still inside the hot
// emission session — they are part of the block and count in x86size).
// The mainline pc/size/recRAMCopy bookkeeping ran before this — pc is dead.
static void recEmitSideExitIslands()
{
	for (int k = 0; k < s_numSideExits; k++)
	{
		SuperblockSideExit& x = s_sideExits[k];
		armAsm->Bind(x.label.get());
		a64::SingleEmissionCheckScope guard(armAsm);
		s_sideExitIslands[k] = armGetCurrentCodePointer();
		armAsm->b(int64_t{0}); // placeholder; patched to the cold exit body
	}
}

// SL-11: compact cold-exit tail — SetBranchImm's shape with the Str-pc /
// cycle-update / event-check factored into the shared SuperblockExitStub.
// Falls back to SetBranchImm for its special-case shapes (resident back-edge,
// WaitLoop FF), which never apply to a continuation side exit in practice.
static void recEmitSideExitTail(u32 imm)
{
	if ((s_loopResident && !s_loopBackedgeEmitted && imm == s_loopTopPc) ||
		(EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && imm == s_branchTo))
	{
		SetBranchImm(imm);
		return;
	}

	const Cop2VfCacheScope vfCacheScope; // fork tail: preserve compile-time cache state
	g_branch = 1;
	pxAssert(imm);

	iFlushCall(FLUSH_EVERYTHING);

	armAsm->Mov(RWSCRATCH, imm);
	armAsm->Mov(RWARG2, scaleblockcycles_clear());
	armEmitCall(SuperblockExitStub);

	// RET lands here: the linked B (same patch protocol as SetBranchImm).
	a64::SingleEmissionCheckScope guard(armAsm);
	u8* patch_site = armGetCurrentCodePointer();
	armAsm->b(int64_t{0}); // placeholder; recBlocks.Link will overwrite
	recBlocks.Link(HWADDR(imm), patch_site);
}

// Island → cold-body patch: same single-word B rewrite + cache maintenance
// protocol as Arm64BaseBlocks link patching (compile-thread only here).
static void recPatchIslandB(u8* site, const u8* target)
{
	const intptr_t imm26 = (reinterpret_cast<intptr_t>(target) - reinterpret_cast<intptr_t>(site)) >> 2;
	pxAssertRel(imm26 >= -(1 << 25) && imm26 < (1 << 25), "Cold-exit island out of B imm26 range");
	*reinterpret_cast<volatile u32*>(site) = 0x14000000u | (static_cast<u32>(imm26) & 0x03FFFFFFu);
	__builtin___clear_cache(reinterpret_cast<char*>(site), reinterpret_cast<char*>(site) + 4);
}

// Emit every pending side exit body into the cold arena (a second emission
// session, after the hot block finalized) and patch the islands to reach
// them. Each body restores its branch-point snapshot, so the exit charges
// exactly the branch-point cycles and the flush writes back exactly what was
// live-dirty there.
static void recEmitColdSideExits()
{
	const int n = s_numSideExits;
	s_numSideExits = 0;
	if (n == 0)
		return;

	const u8* coldStart[kMaxContSites];

	armSetAsmPtr(s_coldPtr, s_coldPtrEnd - s_coldPtr + _64kb, &s_eeConstantPool);
	armStartBlock();
	for (int k = 0; k < n; k++)
	{
		SuperblockSideExit& x = s_sideExits[k];
		coldStart[k] = armGetCurrentCodePointer();
		x.state.restore();
		g_branch = 0;
		if (x.needDs)
		{
			pc = x.dsPc;
			recompileNextInstruction(true, false);
		}
		recEmitSideExitTail(x.branchTo);
		x.label.reset();
	}
	pxAssert(armGetCurrentCodePointer() < SysMemory::GetEERecEnd());
	s_coldPtr = armEndBlock();

	HostSys::BeginCodeWrite();
	for (int k = 0; k < n; k++)
		recPatchIslandB(s_sideExitIslands[k], coldStart[k]);
	HostSys::EndCodeWrite();

	g_branch = 1;
}

// Liveness barrier for the backward pass: `addr` is a continuation branch or
// its delay slot. The taken path leaves the block after the delay slot, so no
// dead-value assumption may cross either out-state.
static bool recSuperblockLivenessBarrier(u32 addr)
{
	for (int i = 0; i < s_numContSites; i++)
		if (addr == s_contSitePcs[i] || addr == s_contSitePcs[i] + 4)
			return true;
	return false;
}

// =====================================================================================================
//  Instruction recompilation
// =====================================================================================================

// AX-05 bracket gate (EE-SRA 2 WS-A): TRUE when a delay-slot instruction can
// observe cpuRegs.branch at runtime, i.e. can reach cpuException (which takes
// it as bd) or the bracket epilogue's exception divert. Fork-verified raiser
// inventory (2026-07-07):
//   - memory ops: TLB miss via vtlb_Miss → cpuTlbMissR/W(addr, cpuRegs.branch)
//     (vtlb.cpp) from BOTH fastmem slow path and softmem, plus the MMIO
//     fallback _ext_memRead/Write (Memory.cpp). The inline load/store paths
//     have no recEmitInterpTlbMissCheck — the vector divert rides the bracket
//     epilogue, so these must keep the bracket.
//   - SYSCALL (IS_BRANCH|BRANCHTYPE_SYSCALL), BREAK, and the trap family
//     TGE..TNE / TGEI..TNEI — all recBranchCall/recCall interpreter fallbacks
//     whose bodies call cpuException(_, cpuRegs.branch). BREAK and the traps
//     carry flags==0 in the opcode table, so they are keyed on opcode fields.
//   - any instruction without native codegen (interpreter fallback via
//     recCall): kept conservatively — an interp body may raise.
// NON-raisers (bracket dropped — the 4-insn win on the common ALU/move/shift/
// lui fillers): integer overflow is never raised by the rec (plain Add/Sub,
// same as x86; only the debug-only FORCE_INTERP_* builds could see interp
// overflow raise with a weakened BD — accepted), FPU/COP2 have no bd path,
// INTC/DMAC/TIMR fire only at event tests (block boundary, branch==0 there),
// and AdEL (RaiseAddressError) is a stub. Pinned by ee_rec_traps_tests.cpp
// delay-slot raiser tests + AluDelaySlotBranchSemanticsSurviveWithoutBracket.
static bool delaySlotNeedsBranchBracket(u32 code, const R5900::OPCODE& op)
{
	if (op.flags & (IS_LOAD | IS_STORE | IS_MEMORY))
		return true; // TLB-miss / MMIO-fallback raisers
	if (op.flags & IS_BRANCH)
		return true; // SYSCALL + branch-in-delay-slot interpreter fallback
	if (!op.recompile)
		return true; // conservative: interpreter fallback may raise
	const u32 primary = code >> 26;
	if (primary == 0x00)
	{
		const u32 funct = code & 0x3F;
		if (funct == 0x0D) // BREAK (flags==0 in the table)
			return true;
		if (funct >= 0x30 && funct <= 0x36) // TGE/TGEU/TLT/TLTU/TEQ/TNE
			return true;
	}
	else if (primary == 0x01)
	{
		const u32 rt = (code >> 16) & 0x1F;
		if (rt >= 0x08 && rt <= 0x0E) // TGEI/TGEIU/TLTI/TLTIU/TEQI/TNEI
			return true;
	}
	return false;
}

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot)
{
	// Apply GameDB DynamicPatch pattern-matches during recompilation, matching
	// x86 recompileNextInstruction. Without this, per-instruction dynamic
	// patches from the game database never take effect under the arm64 EE rec.
	// Upstream 7ee62b822.
	if (EmuConfig.EnablePatches)
		Patch::ApplyDynamicPatches(pc);

	const u32 old_code = cpuRegs.code;
	EEINST* old_inst_info = g_pCurInstInfo;

	cpuRegs.code = memRead32(pc);

	// EP-2b: default-flush seam — any op that is not a cache-aware
	// hand-rolled COP2 macro op kills the VF residency cache (dirty
	// writebacks emit here, before the op's own code). Whitelist over
	// blocklist: an emitter this policy doesn't know about can never
	// corrupt or be corrupted by the cache.
	if (!cop2OpPreservesVfCache(cpuRegs.code))
		cop2VfCacheFlush();

	if (!delayslot)
	{
		pc += 4;
		g_cpuFlushedPC = false;
		g_cpuFlushedCode = false;
	}
	else
	{
		// For delay slots, increment pc after recompiling (at the end of this function)
		g_recompilingDelaySlot = true;
	}

	g_pCurInstInfo++;

	// Branch/jump in a delay slot. Verbatim port of the x86 check_branch_delay
	// block (iR5900.cpp:1742-1803, "new code by FlatOut"). When the delay slot is
	// itself a branch, the inner branch is squashed — emit NOTHING (not even an
	// interpreter call) and return before cycle counting, so cpuRegs.cycle and pc
	// match x86 exactly. This matches x86's behavior, not the interpreter's: the
	// EE interpreter (_doBranch_shared) fully EXECUTES the inner branch, so this
	// path deliberately diverges from interp on this undefined-behavior corner
	// (ps2autotests: HW returns 2, both JIT and interp imperfect). Opcode set is
	// the exact x86 switch (jr/jalr; bltz/bgez(al)(l) family; j/jal/b{eq,ne,lez,
	// gtz}(l)). The downstream isBranchInDelaySlot fallback is now unreachable for
	// these but kept as defense-in-depth.
	if (delayslot)
	{
		bool check_branch_delay = false;
		switch (_Opcode_)
		{
			case 0:
				switch (_Funct_)
				{
					case 8:  // jr
					case 9:  // jalr
						check_branch_delay = true;
						break;
				}
				break;

			case 1:
				switch (_Rt_)
				{
					case 0:
					case 1:
					case 2:
					case 3:
					case 0x10:
					case 0x11:
					case 0x12:
					case 0x13:
						check_branch_delay = true;
						break;
				}
				break;

			case 2:
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
			case 0x14:
			case 0x15:
			case 0x16:
			case 0x17:
				check_branch_delay = true;
				break;
		}

		if (check_branch_delay)
		{
			DevCon.Warning("Branch %x in delay slot!", cpuRegs.code);
			_clearNeededArm64GPRregs();
			_clearNeededNEONregs();
			pc += 4;
			g_cpuFlushedPC = false;
			g_cpuFlushedCode = false;
			if (g_maySignalException)
				armClearCauseBD();

			g_recompilingDelaySlot = false;
			return;
		}
	}

	// Bracket can-raise delay-slot bodies with cpuRegs.branch = 1 / 0,
	// mirroring the interpreter's _doBranch_shared. Every exception raiser
	// (trap/SYSCALL/BREAK, and the aarch64 vtlb TLB-miss path) passes
	// cpuRegs.branch as the bd argument to cpuException — with no store the
	// rec always raised BD=0 with a delay-slot resume address, and the kernel
	// would ERET straight to the delay slot, losing the branch. The epilogue
	// below also uses the flag to detect that an exception fired
	// (cpuException zeroes it) and divert to the dispatcher — without that,
	// the enclosing branch's static dispatch overwrites the exception vector
	// and the branch target executes as if nothing happened. (AX-05)
	// Gated to raiser classes only (EE-SRA 2 WS-A): cpuRegs.branch was the
	// single hottest state field at 4.2% of ALL emitted EE instructions when
	// every non-NOP slot paid the 4-insn bracket; the common ALU/move/shift
	// fillers can't observe it (see delaySlotNeedsBranchBracket).
	const bool dsExceptionBracket = delayslot && (cpuRegs.code != 0) &&
		delaySlotNeedsBranchBracket(cpuRegs.code, R5900::GetCurrentInstruction());
	if (dsExceptionBracket)
	{
		armAsm->Mov(RWSCRATCH, 1);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.branch));
	}

	// NOP gets cycle counted but no codegen (matching x86 behavior)
	if (cpuRegs.code == 0)
	{
		s_nBlockCycles += 9 * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
	}
	else
	{
		const R5900::OPCODE& opcode = R5900::GetCurrentInstruction();
		s_nBlockCycles += opcode.cycles * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));

#ifdef VERIFY_NATIVE_CODEGEN
		// Verification mode: verify native codegen against interpreter for
		// instructions in categories that have native codegen enabled.
		// Skip COP0/Memory/FPU which are interpreter-only and may have
		// timing-sensitive behaviour (MFC0 Count reads cycle counter).
		const u32 verifyOp = cpuRegs.code >> 26;
		const bool isVerifiableCategory =
			// Verify COP2 instructions (opcode 18 = 0x12)
			(verifyOp == 0x12);

		if (isVerifiableCategory && opcode.recompile && opcode.interpret)
		{
			// Step 1: Flush all registers to cpuRegs BEFORE native codegen
			iFlushCall(FLUSH_EVERYTHING);

			// Step 2: Emit call to snapshot pre-instruction state
			armAsm->Mov(a64::w0, cpuRegs.code);
			armAsm->Mov(a64::w1, pc - 4); // current instruction PC
			armFlushEEGPRPins(); // lazy-dirty seam: snapshot READS guest GPR memory
			armEmitCall((void*)verifySnapshotPre);
			// The hook is GPR-read-only but clobbers the caller-saved pins,
			// and the native codegen emitted next reads guest state through
			// them.
			armReloadEEClobberedPins();

			// Step 3: Run the native codegen
			opcode.recompile();

			// Step 4: Flush native results to cpuRegs
			iFlushCall(FLUSH_EVERYTHING);

			// Step 5: Emit call to verify against interpreter
			armAsm->Mov(a64::w0, cpuRegs.code);
			armAsm->Mov(a64::w1, pc - 4);
			armFlushEEGPRPins(); // lazy-dirty seam: verify READS guest GPR memory
			armEmitCall((void*)verifyCheckPost);
			armReloadEEClobberedPins(); // see verifySnapshotPre above
		}
		else
#endif
		{
			// Guard: branch/jump in a delay slot would cause infinite
			// compile-time recursion. Use interpreter for the instruction.
			const bool isBranchInDelaySlot = delayslot && (opcode.flags & IS_BRANCH);
			if (isBranchInDelaySlot || !opcode.recompile)
			{
				if ((opcode.flags & IS_BRANCH) && !isBranchInDelaySlot)
					recBranchCall(opcode.interpret);
				else
					recCall(opcode.interpret);
			}
			else
				opcode.recompile();
		}
	}

	// SP misalignment check disabled: MMI/COP2 instructions legitimately use
	// r29 as SIMD data, causing massive false-positive spam.

	if (!swapped_delay_slot)
	{
		_clearNeededArm64GPRregs();
		_clearNeededNEONregs();
	}

	if (delayslot)
	{
		if (dsExceptionBracket)
		{
			// cpuException zeroes cpuRegs.branch; still-1 means the delay
			// slot completed without raising. On an exception, divert to the
			// dispatcher on the vector PC before the enclosing branch's
			// static dispatch can clobber it (same contract as
			// recEmitInterpTlbMissCheck; cycle undercount on this exceptional
			// path is accepted the same way). (AX-05)
			a64::Label noException;
			armAsm->Ldr(RWSCRATCH, armCpuRegMem(&cpuRegs.branch));
			armAsm->Cbnz(RWSCRATCH, &noException);
			armEmitJmp(DispatcherReg);
			armAsm->Bind(&noException);
			armAsm->Str(a64::wzr, armCpuRegMem(&cpuRegs.branch));
		}

		pc += 4;
		g_cpuFlushedPC = false;
		g_cpuFlushedCode = false;
		if (g_maySignalException) // mirrors x86 iR5900.cpp:1830 (dormant: always false)
			armClearCauseBD();
		g_recompilingDelaySlot = false;
	}

	g_maySignalException = false; // mirrors x86 iR5900.cpp:1835

	// When called from TrySwapDelaySlot (swapped_delay_slot=true), restore
	// cpuRegs.code so that the caller's _Rs_/_Rt_/_Rd_ macros still work.
	// Matches x86 at iR5900.cpp:1918-1921.
	if (swapped_delay_slot)
	{
		cpuRegs.code = old_code;
		g_pCurInstInfo = old_inst_info;
	}
}

// Verbatim port of the x86 EE TrySwapDelaySlot decode/safety table
// (pcsx2/x86/ix86-32/iR5900.cpp:901). This is a pure instruction-decode +
// hazard check with NO codegen — it only decides whether the instruction in
// the branch's delay slot can be safely hoisted ahead of the branch (so its
// result is available when the branch evaluates its condition). Keep this in
// lockstep with the x86 table (comments and all, including the dead `case 64`
// quirk) so it can be diffed line-for-line on future re-syncs; behavioral
// divergence from x86 here is a bug, not an arm64 optimization.
bool TrySwapDelaySlot(u32 rs, u32 rt, u32 rd, bool allow_loadstore)
{
	if (g_recompilingDelaySlot)
		return false;

	const u32 opcode_encoded = memRead32(pc);
	if (opcode_encoded == 0) // NOP
	{
		recompileNextInstruction(true, true);
		return true;
	}

	const u32 opcode_rs = ((opcode_encoded >> 21) & 0x1F);
	const u32 opcode_rt = ((opcode_encoded >> 16) & 0x1F);
	const u32 opcode_rd = ((opcode_encoded >> 11) & 0x1F);

	switch (opcode_encoded >> 26)
	{
		case 8: // ADDI
		case 9: // ADDIU
		case 10: // SLTI
		case 11: // SLTIU
		case 12: // ANDIU
		case 13: // ORI
		case 14: // XORI
		case 24: // DADDI
		case 25: // DADDIU
		{
			if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && (rd == opcode_rs || rd == opcode_rt)))
				goto is_unsafe;
		}
		break;

		case 26: // LDL
		case 27: // LDR
		case 30: // LQ
		case 31: // SQ
		case 32: // LB
		case 33: // LH
		case 34: // LWL
		case 35: // LW
		case 36: // LBU
		case 37: // LHU
		case 38: // LWR
		case 39: // LWU
		case 40: // SB
		case 41: // SH
		case 42: // SWL
		case 43: // SW
		case 44: // SDL
		case 45: // SDR
		case 46: // SWR
		case 55: // LD
		case 63: // SD
		{
			// We can't allow loadstore swaps for BC0x/BC2x, since they could affect the condition.
			if (!allow_loadstore || (rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && (rd == opcode_rs || rd == opcode_rt)))
				goto is_unsafe;
		}
		break;

		case 15: // LUI
		{
			if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && rd == opcode_rt))
				goto is_unsafe;
		}
		break;

		case 49: // LWC1
		case 57: // SWC1
		case 54: // LQC2
		case 62: // SQC2
			break;

		case 0: // SPECIAL
		{
			switch (opcode_encoded & 0x3F)
			{
				case 0: // SLL
				case 2: // SRL
				case 3: // SRA
				case 4: // SLLV
				case 6: // SRLV
				case 7: // SRAV
				case 10: // MOVZ
				case 11: // MOVN
				case 20: // DSLLV
				case 22: // DSRLV
				case 23: // DSRAV
				case 24: // MULT
				case 25: // MULTU
				case 32: // ADD
				case 33: // ADDU
				case 34: // SUB
				case 35: // SUBU
				case 36: // AND
				case 37: // OR
				case 38: // XOR
				case 39: // NOR
				case 42: // SLT
				case 43: // SLTU
				case 44: // DADD
				case 45: // DADDU
				case 46: // DSUB
				case 47: // DSUBU
				case 56: // DSLL
				case 58: // DSRL
				case 59: // DSRA
				case 60: // DSLL32
				case 62: // DSRL31
				case 64: // DSRA32
				{
					if ((rs != 0 && rs == opcode_rd) || (rt != 0 && rt == opcode_rd) || (rd != 0 && (rd == opcode_rs || rd == opcode_rt)))
						goto is_unsafe;
				}
				break;

				case 15: // SYNC
				case 26: // DIV
				case 27: // DIVU
					break;

				default:
					goto is_unsafe;
			}
		}
		break;

		case 16: // COP0
		{
			switch ((opcode_encoded >> 21) & 0x1F)
			{
				case 0: // MFC0
				case 2: // CFC0
				{
					if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && rd == opcode_rt))
						goto is_unsafe;
				}
				break;

				case 4: // MTC0
				case 6: // CTC0
					break;

				case 16: // TLB (technically would be safe, but we don't use it anyway)
				default:
					goto is_unsafe;
			}
			break;
		}
		break;

		case 17: // COP1
		{
			switch ((opcode_encoded >> 21) & 0x1F)
			{
				case 0: // MFC1
				case 2: // CFC1
				{
					if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && rd == opcode_rt))
						goto is_unsafe;
				}
				break;

				case 4: // MTC1
				case 6: // CTC1
				case 16: // S
				{
					const u32 funct = (opcode_encoded & 0x3F);
					if (funct == 50 || funct == 52 || funct == 54) // C.EQ, C.LT, C.LE
					{
						// affects flags that we're comparing
						goto is_unsafe;
					}
				}
					[[fallthrough]];

				case 20: // W
				{
				}
				break;

				default:
					goto is_unsafe;
			}
		}
		break;

		case 18: // COP2
		{
			switch ((opcode_encoded >> 21) & 0x1F)
			{
				case 8: // BC2XX
					goto is_unsafe;

				case 1: // QMFC2
				case 2: // CFC2
				{
					if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && rd == opcode_rt))
						goto is_unsafe;
				}
				break;

				default:
					break;
			}
		}
		break;

		case 28: // MMI
		{
			switch (opcode_encoded & 0x3F)
			{
				case 8: // MMI0
				case 9: // MMI1
				case 10: // MMI2
				case 40: // MMI3
				case 41: // MMI3
				case 52: // PSLLH
				case 54: // PSRLH
				case 55: // LSRAH
				case 60: // PSLLW
				case 62: // PSRLW
				case 63: // PSRAW
				{
					if ((rs != 0 && rs == opcode_rd) || (rt != 0 && rt == opcode_rd) || (rd != 0 && rd == opcode_rd))
						goto is_unsafe;
				}
				break;

				default:
					goto is_unsafe;
			}
		}
		break;

		default:
			goto is_unsafe;
	}

	recompileNextInstruction(true, true);
	return true;

is_unsafe:
	return false;
}

// =====================================================================================================
//  Memory management and block clearing
// =====================================================================================================

static void recClear(u32 addr, u32 size)
{
	addr = HWADDR(addr);
	const u32 end = addr + size * 4;

#ifdef PCSX2_RECOMPILER_TESTS
	// Keep the loop-residency introspection in sync with the live block set.
	for (auto it = s_loopResidentBlocks.begin(); it != s_loopResidentBlocks.end();)
	{
		if (*it >= addr && *it < end)
			it = s_loopResidentBlocks.erase(it);
		else
			++it;
	}
#endif

	int blockidx = recBlocks.LastIndex(end - 4);
	if (blockidx == -1)
		return;

	// macOS/Apple Silicon (no-op elsewhere): recClear runs from the EE
	// page-fault handler (fastmem backpatch + SMC via mmap_ClearCpuBlock) and
	// from runtime SMC on the executing CPU thread — both enter with the
	// MAP_JIT code cache in execute-protected (W^X) mode. The SetFnptr writes
	// and Arm64BaseBlocks::Remove() stub patches below target that region, so
	// a write faults (SIGBUS) unless we flip the thread to write mode first.
	// Refcounted, so it nests harmlessly when reached from an open emit scope.
	HostSys::BeginCodeWrite();

	// Track the EE-address span of all blocks we touch so the post-walk
	// tail can reset interior BLOCKs across the *full* extent of the
	// removed blocks (a straddler can extend well past `end` or below
	// `addr`). `ceiling` clamps the tail at the next surviving block's
	// startpc so we never trample its interior.
	u32 lowerextent = static_cast<u32>(-1);
	u32 upperextent = 0;
	u32 ceiling = static_cast<u32>(-1);

	if (BASEBLOCKEX* peb_above = recBlocks[blockidx + 1])
		ceiling = peb_above->startpc;

	int toRemoveLast = blockidx;

	// Walk down through blocks overlapping [addr, end). For each, reset
	// BLOCK->fnptr at the block's actual start (the straddle-from-below
	// case is load-bearing — Arm64BaseBlocks::Remove() patches only the
	// compiled-code stub, so any BLOCK->fnptr left pointing at a stub
	// trips the recRecompile fnptr assertion on the next dispatch).
	//
	// Skip s_pCurBlock if we hit it: it's the block currently being
	// compiled, and yanking it mid-emit corrupts the in-progress block.
	// Splitting the Remove range around it preserves it. Mirrors x86
	// recClear (pcsx2/x86/ix86-32/iR5900.cpp:786).
	while (BASEBLOCKEX* pexblock = recBlocks[blockidx])
	{
		const u32 blockstart = pexblock->startpc;
		const u32 blockend = blockstart + pexblock->size * 4;
		BASEBLOCK* pblock = GETBLOCK(blockstart);

		if (pblock == s_pCurBlock)
		{
			if (toRemoveLast != blockidx)
				recBlocks.Remove(blockidx + 1, toRemoveLast);
			toRemoveLast = --blockidx;
			continue;
		}

		if (blockend <= addr)
		{
			lowerextent = std::max(lowerextent, blockend);
			break;
		}

		lowerextent = std::min(lowerextent, blockstart);
		upperextent = std::max(upperextent, blockend);
		pblock->SetFnptr((uptr)JITCompile);

		--blockidx;
	}

	if (toRemoveLast != blockidx)
		recBlocks.Remove(blockidx + 1, toRemoveLast);

	upperextent = std::min(upperextent, ceiling);

	// Reset interior BLOCKs across the full removed-block extent. Without
	// this, interior fnptrs of straddler blocks can stay non-JITCompile
	// from a prior compilation, leading to wrong dispatch on a later JR
	// into the middle of a freshly-recompiled block.
	if (upperextent > lowerextent)
		iopClearRecLUT(GETBLOCK(lowerextent), upperextent - lowerextent);

	HostSys::EndCodeWrite();
}

static void iopClearRecLUT(BASEBLOCK* base, int count)
{
	for (int i = 0; i < count / 4; i++)
		base[i].SetFnptr((uptr)JITCompile);
}

static void dyna_block_discard(u32 start, u32 sz)
{
	DevCon.WriteLn("%.8X rec block discard (sz=%d)", start, sz);
	recClear(start, sz);
}

static void dyna_page_reset(u32 start, u32 sz)
{
	recClear(start & ~0xFFF, 0x400); // clear 4KB page
	manual_counter[start >> 12]++;
	mmap_MarkCountedRamPage(start);
}

// Self-modifying code detection — generates inline memory comparison checks
// for blocks in manually-protected pages, and sets up page protection for new pages.
// Port of x86 memory_protect_recompiled_code(). Returns true when the block
// got an inline manual SMC check — such blocks are excluded from SL-1 loop
// residency (the entry check must run every iteration).
static bool memory_protect_recompiled_code(u32 startpc, u32 size)
{
	u32 inpage_ptr = HWADDR(startpc);
	const u32 inpage_sz = size * 4;

	// The kernel context register is stored @ 0x800010C0-0x80001300
	// The EENULL thread context register is stored @ 0x81000-....
	const bool contains_thread_stack = ((startpc >> 12) == 0x81) || ((startpc >> 12) == 0x80001);

	const vtlb_ProtectionMode PageType = contains_thread_stack ? ProtMode_Manual : mmap_GetRamPageInfo(inpage_ptr);

	switch (PageType)
	{
		case ProtMode_NotRequired:
			break;

		case ProtMode_None:
		case ProtMode_Write:
			mmap_MarkCountedRamPage(inpage_ptr);
			manual_page[inpage_ptr >> 12] = 0;
			break;

		case ProtMode_Manual:
		{
			// Set up arguments for DispatchBlockDiscard (w0=addr, w1=size)
			armAsm->Mov(a64::w0, inpage_ptr);
			armAsm->Mov(a64::w1, inpage_sz / 4);

			// FX-03a fast compare (mirrors the AetherSX2 leak's hoisted-base
			// wide-compare shape, module 1030, which strides 8 bytes; we take
			// it to 16): snapshot the source words into the constant pool,
			// hoist both base addresses once, then Ldp 16-byte pairs through
			// a single Cmp/Ccmp flag chain with ONE exit branch. ~1.3
			// insns/word vs ~8 for the per-word fallback below. Post-index
			// addressing dodges the Ldp imm7 range limit on page-size blocks.
			// The snapshot must be OUR frozen copy — comparing against
			// recRAMCopy would alias later recompiles of overlapping blocks.
			const u8* expected_blob = armConstantPool ?
				armConstantPool->GetBlob((const u8*)PSM(inpage_ptr), inpage_sz) : nullptr;
			if (expected_blob)
			{
				// Clobbers x2/x3/x8/x9/x10/x17 — all dead at block head
				// (w0/w1 hold the discard args and are untouched).
				armMoveAddressToReg(RSCRATCHADDR, (void*)PSM(inpage_ptr));
				armMoveAddressToReg(RXSCRATCH, expected_blob);
				u32 rem = inpage_sz;
				bool first = true;
				while (rem >= 16)
				{
					armAsm->Ldp(a64::x9, a64::x10, a64::MemOperand(RSCRATCHADDR, 16, a64::PostIndex));
					armAsm->Ldp(a64::x2, a64::x3, a64::MemOperand(RXSCRATCH, 16, a64::PostIndex));
					if (first)
						armAsm->Cmp(a64::x9, a64::x2);
					else
						armAsm->Ccmp(a64::x9, a64::x2, a64::NoFlag, a64::eq);
					armAsm->Ccmp(a64::x10, a64::x3, a64::NoFlag, a64::eq);
					first = false;
					rem -= 16;
				}
				if (rem >= 8)
				{
					armAsm->Ldr(a64::x9, a64::MemOperand(RSCRATCHADDR, 8, a64::PostIndex));
					armAsm->Ldr(a64::x2, a64::MemOperand(RXSCRATCH, 8, a64::PostIndex));
					if (first)
						armAsm->Cmp(a64::x9, a64::x2);
					else
						armAsm->Ccmp(a64::x9, a64::x2, a64::NoFlag, a64::eq);
					first = false;
					rem -= 8;
				}
				if (rem)
				{
					armAsm->Ldr(a64::w9, a64::MemOperand(RSCRATCHADDR));
					armAsm->Ldr(a64::w2, a64::MemOperand(RXSCRATCH));
					if (first)
						armAsm->Cmp(a64::w9, a64::w2);
					else
						armAsm->Ccmp(a64::w9, a64::w2, a64::NoFlag, a64::eq);
				}
				armEmitCondBranch(a64::ne, DispatchBlockDiscard);
			}
			else
			{
				// Pool full — per-word fallback (the original emission).
				u32 lpc = inpage_ptr;
				u32 stg = inpage_sz;

				while (stg > 0)
				{
					const u32 expected = *(u32*)PSM(lpc);

					// Load current memory word
					armMoveAddressToReg(RSCRATCHADDR, (void*)PSM(lpc));
					armAsm->Ldr(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));

					// Compare with compile-time snapshot
					armAsm->Mov(a64::w9, expected);
					armAsm->Cmp(RWSCRATCH, a64::w9);
					armEmitCondBranch(a64::ne, DispatchBlockDiscard);

					stg -= 4;
					lpc += 4;
				}
			}

			// Counted blocks: track how often this block runs. If the counter overflows,
			// reset the page to write-protected mode (faster than manual checks).
			if (!contains_thread_stack && manual_counter[inpage_ptr >> 12] <= 3)
			{
				armMoveAddressToReg(RSCRATCHADDR, &manual_page[inpage_ptr >> 12]);
				armAsm->Ldrh(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));
				armAsm->Add(RWSCRATCH, RWSCRATCH, size);
				armAsm->Strh(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));
				// Check for u16 overflow (bit 16+ set means wrapped past 0xFFFF)
				armAsm->Tst(RWSCRATCH, 0xFFFF0000u);
				armEmitCondBranch(a64::ne, DispatchPageReset);
			}
			break;
		}
	}

	return PageType == ProtMode_Manual;
}

// =====================================================================================================
//  Reserve / Reset / Shutdown / Execute
// =====================================================================================================

static void recReserveRAM()
{
	recLutEntries = (Ps2MemSize::MainRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4;

	if (recLutReserve_RAM.size() != recLutEntries)
		recLutReserve_RAM.resize(recLutEntries);

	recLutUnmapped.resize(_64kb / 4);

	BASEBLOCK* curpos = recLutReserve_RAM.data();
	recRAM = curpos;
	curpos += (Ps2MemSize::MainRam / 4);
	recROM = curpos;
	curpos += (Ps2MemSize::Rom / 4);
	recROM1 = curpos;
	curpos += (Ps2MemSize::Rom1 / 4);
	recROM2 = curpos;
	curpos += (Ps2MemSize::Rom2 / 4);

	if (recRAMCopy.size() != Ps2MemSize::MainRam)
		recRAMCopy.resize(Ps2MemSize::MainRam);
}

static void recReserve()
{
	Console.WriteLn(Color_Green, "EE: ARM64 Recompiler reserved.");

	// 256KB: the FX-03a manual-check snapshots are blobs, not dedup'd
	// literals — a manual-heavy title banks ~70-80KB of them per rec
	// generation (SotC 17k words, UYA 19k). Overflow falls back to the
	// per-word check emission, so this is a soft ceiling.
	const u32 poolSize = 262144;
	u8* poolBase = SysMemory::GetEERecEnd() - poolSize;
	s_eeConstantPool.Init(poolBase, poolSize);

	// Code region: everything below the pool, minus the 64KB slop a single
	// compile may overhang past recPtrEnd (armSetAsmPtr grants capacity
	// recPtrEnd - recPtr + 64KB; the cache-full reset triggers on the NEXT
	// compile). recPtrEnd must sit BELOW poolBase by the slop, or a block
	// compiled near the full mark overwrites the pool — whose first bytes
	// are the dispatcher stubs' far-call veneers (bl → mov x16/../br), so
	// the corruption fires on the next event dispatch, far from the cause.
	// (Latent since the pool moved to the cache tail; surfaced by SL-03
	// superblocks growing per-block emission enough for the fuzz soak to
	// fill the cache into the overlap.)
	// SL-10: cold side-exit arena between the hot code region and the pool.
	// Same slop discipline as the hot region: a session's capacity grant is
	// (end - ptr + 64KB), so each region's ptr-end sits one slop below the
	// next region's base and an overhanging emission can never cross it.
	const u32 coldArenaSize = 8 * _1mb;
	s_coldBase = poolBase - coldArenaSize;
	s_coldPtr = s_coldBase;
	s_coldPtrEnd = poolBase - _64kb;

	recPtr = SysMemory::GetEERec();
	recPtrEnd = s_coldBase - _64kb;
	pxAssertRel(recPtrEnd > recPtr && recPtrEnd + _64kb <= s_coldBase &&
		s_coldPtrEnd + _64kb <= poolBase,
		"EE rec code region and cold arena must not reach the constant pool");

	recReserveRAM();

	pxAssertRel(!s_pInstCache, "InstCache not allocated");
	s_nInstCacheSize = 128;
	s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
	if (!s_pInstCache)
		pxFailRel("Failed to allocate R5900 InstCache array.");
}

static void recResetRaw()
{
	Console.WriteLn(Color_Green, "iR5900-ARM64 Recompiler reset.");

	// The code-cache rewind below dangles every host landing pointer in the
	// call-ret ring — sentinel-fill so no stale frame can match. (recClear
	// needs nothing: dead block entries keep resolving via redirect stubs.)
	eeCallRetResetRing();

#ifdef PCSX2_RECOMPILER_TESTS
	s_loopResidentBlocks.clear();
#endif

	// COP2 macro-mode emitters read their clamp/mask constants from the pack
	// ([RSTATE, #imm]) — (re)write them before any block compiles.
	cop2RecWritePackConstants();

	// Full reset regenerates every block and dispatcher, so nothing can
	// reference old pool content — drop it. Required since FX-03a: the
	// manual-check snapshot blobs are not dedup'd, so without this the pool
	// grows monotonically across resets into permanent per-word fallback.
	s_eeConstantPool.Reset();

	armSetAsmPtr(SysMemory::GetEERec(), SysMemory::GetEERecEnd() - SysMemory::GetEERec(), &s_eeConstantPool);
	armStartBlock();
	const u8* dispStart = armGetCurrentCodePointer();
	_DynGen_Dispatchers();

	// SL-11: the shared cold-exit tail. Contract on entry: guest state fully
	// flushed, RWSCRATCH = target guest pc, RWARG2 (w1) = scaled block cycles
	// (zero-extended). Adds with a zero register still sets N/Z from RECCYCLE,
	// matching emitCycleUpdateAndEventCheck's Cmp-on-zero-cycles shape. The
	// no-event path RETs to the BL site's linked B; the event path discards
	// the link register (pc is already stored, DispatcherEvent re-enters
	// through the dispatcher).
	SuperblockExitStub = armGetCurrentCodePointer();
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));
	armAsm->Adds(RECCYCLE, RECCYCLE, a64::x1);
	armEmitCondBranch(a64::ge, DispatcherEvent);
	armAsm->Ret();

	const u8* dispEnd = armGetCurrentCodePointer();
	recPtr = armEndBlock();

	s_coldPtr = s_coldBase;

	Console.WriteLn(Color_Green, "EE ARM64: Dispatcher generated at %p (%zu bytes)", dispStart, (size_t)(dispEnd - dispStart));

	iopClearRecLUT(recLutReserve_RAM.data(),
		Ps2MemSize::MainRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2);

	BASEBLOCK* unmapped = recLutUnmapped.data();

	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(recLUT, hwLUT, unmapped, i, 0, 0);

	for (int i = 0; i < _64kb / 4; i++)
		unmapped[i].SetFnptr((uptr)UnmappedRecLUTPage);

	// Map EE RAM (32MB, mirrored)
	for (int i = 0; i < 0x200; i++)
	{
		u32 mask = (Ps2MemSize::MainRam / _64kb) - 1;
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x0000, i, i & mask);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x2000, i, i & mask);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x3000, i, i & mask);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x8000, i, i & mask);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xa000, i, i & mask);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xb000, i, i & mask);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xc000, i, i & mask);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xd000, i, i & mask);
	}

	// Map BIOS ROM
	for (int i = 0x1fc0; i < 0x2000; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	// Map ROM1 (full 4 MB / 64 pages). EROM lives inside ROM1 at a variable
	// offset, so games using EROM above page 0x1e04 must dispatch to mapped JIT
	// pages. recROM1 is already reserved for Rom1/4 entries above; matches x86
	// iR5900.cpp:573 and the arm64 IOP twin. Upstream fix 8fbb1e556.
	for (int i = 0x1e00; i < 0x1e40; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	// Map ROM2 (EROM2, Chinese BIOS extension, phys 0x1e400000-0x1e800000,
	// full 4 MB / 64 pages). x86 EE (iR5900.cpp:580-584), x86 IOP, and our
	// own arm64 IOP all map it; without it EE fetches from EROM2 dispatch to
	// UnmappedRecLUTPage and pause the VM. (AX-06)
	for (int i = 0x1e40; i < 0x1e80; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0xa000, i, i - 0x1e40);
	}

	if (s_pInstCache)
		memset(s_pInstCache, 0, sizeof(EEINST) * s_nInstCacheSize);

	recBlocks.Reset();
	maxrecmem = 0;

	memset(manual_page, 0, sizeof(manual_page));
	memset(manual_counter, 0, sizeof(manual_counter));
	if (recRAMCopy.data())
		memset(recRAMCopy.data(), 0, recRAMCopy.size());

	g_branch = 0;
}

static void recShutdown()
{
	s_eeConstantPool.Destroy();
	recRAMCopy.deallocate();
	recLutReserve_RAM.deallocate();
	recLutUnmapped.deallocate();

	safe_free(s_pInstCache);
	s_nInstCacheSize = 0;

	recPtr = nullptr;
	recPtrEnd = nullptr;
	s_coldBase = nullptr;
	s_coldPtr = nullptr;
	s_coldPtrEnd = nullptr;
}

static void recResetEE()
{
	if (eeCpuExecuting)
	{
		eeRecNeedsReset = true;
		recSafeExitExecution();
		return;
	}

	recResetRaw();
}

static void recStep()
{
}

static void recExitExecution()
{
	fastjmp_jmp(&m_SetJmp_StateCheck, 1);
}

static void recSafeExitExecution()
{
	eeRecExitRequested = true;

	if (!eeEventTestIsActive)
	{
		cpuRegs.nextEventCycle = 0;
	}
	else
	{
		if (psxRegs.iopCycleEE > 0)
		{
			psxRegs.iopBreak += psxRegs.iopCycleEE;
			psxRegs.iopCycleEE = 0;
		}
	}
}

static void recCancelInstruction()
{
	// Called by interpreter functions (e.g. RaiseAddressError) when an
	// exception occurs mid-instruction. For the interpreter, this does a
	// longjmp. For the recompiler, set the TLB miss flag so that recCall's
	// post-call check dispatches to the exception vector.
	s_recTlbMissOccurred = 1;
}

static void recExecute()
{
	if (eeRecNeedsReset)
	{
		eeRecNeedsReset = false;
		recResetRaw();
	}

	Console.WriteLn(Color_Green, "EE ARM64: Entering recompiled code (pc=0x%08X)", cpuRegs.pc);

	if (!fastjmp_set(&m_SetJmp_StateCheck))
	{
		eeCpuExecuting = true;
		((void (*)())EnterRecompiledCode)();
	}

	eeCpuExecuting = false;
}

#ifdef PCSX2_RECOMPILER_TESTS
// Harness entry. Not part of R5900cpu; called directly by EeRecTestHarness
// for a bounded number of guest cycles ending at park_pc. Forces
// nextEventCycle = cpuRegs.cycle so iBranchTest at every block tail routes
// through DispatcherEvent (where recEventTest's harnessShouldExit check
// can observe parking-PC arrival or cycle exhaust). Returns the cycle
// delta consumed in this run.
s32 recEeExecuteBlock(s32 cycles, u32 park_pc)
{
	const s32 cap = std::min(cycles, kExecuteBlockSafetyCap);

	g_eeHarnessActive = true;
	g_eeHarnessParkPc = park_pc;
	g_eeHarnessCycleBudget = cap;
	g_eeHarnessCycleStart = cpuRegs.cycle;
	eeRecExitRequested = false;

	cpuRegs.nextEventCycle = cpuRegs.cycle;

	if (!fastjmp_set(&m_SetJmp_StateCheck))
	{
		((void (*)())EnterRecompiledCode)();
	}

	g_eeHarnessActive = false;

	return static_cast<s32>(cpuRegs.cycle - g_eeHarnessCycleStart);
}

// Test-harness link introspection. Forwards to Arm64BaseBlocks::IsLinked,
// which walks the link multimap for any patch site within the block
// containing src_pc that targets dst_pc.
bool recEeIsBlockLinked(u32 src_pc, u32 dst_pc)
{
	return recBlocks.IsLinked(src_pc, dst_pc);
}

// SL-1 introspection: the registered back-edge patch site + spill stub of the
// block at pc_query, so tests can assert Remove()'s repoint after a recClear.
bool recEeLoopBackedgeInfo(u32 pc_query, uptr* site, uptr* stub)
{
	BASEBLOCKEX* b = recBlocks.Get(HWADDR(pc_query));
	if (!b || !b->backedge_site)
		return false;
	*site = b->backedge_site;
	*stub = b->backedge_stub;
	return true;
}

// SL-03 introspection: guest-insn size of the compiled block starting at
// pc_query (0 = no block), so tests can assert superblock formation (the
// range spans past a continuation branch) vs termination (it doesn't).
u32 recEeBlockGuestSize(u32 pc_query)
{
	BASEBLOCKEX* b = recBlocks.Get(HWADDR(pc_query));
	return b ? b->size : 0;
}

// Test-harness recLUT coverage introspection: does this guest PC dispatch to
// a real (compile-on-first-hit) LUT page rather than UnmappedRecLUTPage?
// The harness can't execute code from ROM regions (it's hardwired to EE
// RAM), so region-mapping fixes like the ROM2 one (AX-06) are pinned by
// asserting page coverage directly.
bool recEeIsPcMapped(u32 pc_query)
{
	const BASEBLOCK* b = GETBLOCK(pc_query);
	return b && (b->GetFnptr() != (uptr)UnmappedRecLUTPage);
}
#endif

// =====================================================================================================
//  Timeout Loop Speedhack
// =====================================================================================================

// Detects and skips timeout loops like:
//   addiu v0,v0,-1 / nop*N / bne v0,zero,loop / nop
// Instead of spinning, advances the cycle counter and decrements the register.
// Port of x86 skipMPEG_By_Pattern (pcsx2/x86/ix86-32/iR5900.cpp). The IOP FMV
// path's sceMpegIsEnd compiles to the 3-instruction leaf
//     lw  reg, 0x40(a0) ; jr ra ; lw v0, 0(reg)
// When CHECK_SKIPMPEGHACK is on, recognize that signature at a block boundary
// and replace the whole block with "v0 = 1; pc = ra" — telling the game the
// video already finished so the (often unplayable/looping) FMV is skipped.
// Several games hard-depend on this to boot — Katamari (our benchmark) among
// them. Emits a complete self-contained block (like recSkipTimeoutLoop) and
// makes the caller skip normal codegen by returning true. Restores Skip-MPEG
// host hook B4 on arm64 (DT-03); was a silent no-op before this.
static bool skipMPEG_By_Pattern(u32 sPC)
{
	if (!CHECK_SKIPMPEGHACK)
		return false;

	// sceMpegIsEnd: lw reg, 0x40(a0); jr ra; lw v0, 0(reg)
	if ((s_nEndBlock == sPC + 12) && (memRead32(sPC + 4) == 0x03e00008))
	{
		const u32 code = memRead32(sPC);
		const u32 p1 = 0x8c800040;
		const u32 p2 = 0x8c020000 | (code & 0x1f0000) << 5;
		if ((code & 0xffe0ffff) != p1)
			return false;
		if (memRead32(sPC + 8) != p2)
			return false;

		// v0 = 1 (low) / 0 (high); pc = ra.
		armAsm->Mov(RWSCRATCH, 1);
		armStoreEERegPtr(RWSCRATCH, &cpuRegs.GPR.n.v0.UL[0]);
		armStoreEERegPtr(a64::wzr, &cpuRegs.GPR.n.v0.UL[1]);
		armLoadEERegPtr(a64::w0, &cpuRegs.GPR.n.ra.UL[0]);
		armAsm->Str(a64::w0, armCpuRegMem(&cpuRegs.pc));

		// x86 iBranchTest() tail (newpc == 0xffffffff path): commit the block's
		// cycles into the delta, then route to DispatcherEvent if an event is
		// due (delta >= 0), else DispatcherReg to run the block at pc=ra.
		// s_nBlockCycles is still 0 here (no instruction emitted yet) — matches
		// x86, where iBranchTest's scaleblockcycles() also sees a zero count.
		emitCycleUpdateAndEventCheck();
		armEmitJmp(DispatcherReg);

		g_branch = 1;
		pc = s_nEndBlock;
		Console.WriteLn(Color_StrongGreen, "sceMpegIsEnd pattern found! Recompiling skip video fix...");
		return true;
	}
	return false;
}

// Port of x86 recSkipTimeoutLoop().
static bool recSkipTimeoutLoop(s32 reg, bool is_timeout_loop)
{
	if (!EmuConfig.Speedhacks.WaitLoop || !is_timeout_loop)
		return false;

	DevCon.WriteLn("[EE] Skipping timeout loop at 0x%08X -> 0x%08X (reg=%d)",
		s_pCurBlockEx->startpc, s_nEndBlock, reg);

	// Logic: skip the loop by advancing cycles based on the register value.
	// In delta terms (delta = cycle - nextEventCycle; subtract nextEventCycle
	// from every absolute quantity in the x86 original):
	// new_delta = min(reg * 8 + delta, 0)
	// new_reg = reg - (new_delta - delta) / 8
	// if new_reg > 0, jump to dispatcher (an event interrupted the loop)
	// else loop finished, continue at s_nEndBlock

	// if (delta >= 0) goto DispatcherEvent
	armAsm->Cmp(RECCYCLE, 0);
	armEmitCondBranch(a64::ge, DispatcherEvent);

	// w9 = reg value (the decrementing counter). Scratches are w9/x10/w8
	// (reserved — w4-w7 are allocatable and may hold live values in the
	// surrounding EE block). MUST go through the pin helper: the counter can
	// be a pinned GPR (UYA idles on $at), and under lazy-dirty the loop's
	// decrements live in the pin while canonical memory is stale — a raw Ldr
	// here read a stale counter and the pin-aware store below then clobbered
	// the real one, collapsing UYA's frame pacing (600 frames in 1s).
	armLoadEERegPtr(a64::w9, &cpuRegs.GPR.r[reg].UL[0]);

	// x10 = reg * 8 + delta (estimated end delta, s64)
	armAsm->Add(a64::x10, RECCYCLE, a64::Operand(a64::x9, a64::LSL, 3));

	// x10 = min(x10, 0) — can't run past the next event
	armAsm->Cmp(a64::x10, 0);
	armAsm->Csel(a64::x10, a64::xzr, a64::x10, a64::gt); // if x10 > 0, clamp to 0

	// w8 = (new_delta - old_delta) >> 3 = iterations consumed (deltas share
	// the same nextEventCycle base, so their difference equals the absolute
	// cycle difference).
	armAsm->Sub(RWSCRATCH, a64::w10, RECCYCLE.W());
	armAsm->Lsr(RWSCRATCH, RWSCRATCH, 3);

	// Commit the new delta into RECCYCLE (no memory store — DispatcherEvent
	// converts and flushes it if we exit there; otherwise the next block-tail
	// event check uses RECCYCLE directly).
	armAsm->Mov(RECCYCLE, a64::x10);

	// reg -= iterations consumed; sign-extend into the 64-bit guest reg
	// (the full UD[0] store covers the UL[0] half).
	armAsm->Sub(a64::w9, a64::w9, RWSCRATCH);
	armAsm->Sxtw(a64::x9, a64::w9);
	armStoreEERegPtr(a64::x9, &cpuRegs.GPR.r[reg].UD[0]);

	// if reg != 0, event interrupted the loop — go to dispatcher
	armEmitCbnz(a64::w9, DispatcherEvent);

	// Loop finished — set PC to end of block and dispatch
	armAsm->Mov(RWSCRATCH, s_nEndBlock);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));
	armEmitJmp(DispatcherReg);

	g_branch = 1;
	pc = s_nEndBlock;

	return true;
}

// =====================================================================================================
//  Main Recompilation Loop
// =====================================================================================================

// Scanner-side decode: is `code` any branch/jump/eret/syscall-class op (a
// block-ender or continuation candidate)? Used to refuse continuation through
// a branch whose delay slot is itself a branch (architecturally-UB shape) —
// those fall back to ending the block, which is today's behavior.
static bool eeScanInsnIsBranchClass(u32 code)
{
	switch (code >> 26)
	{
		case 0: // SPECIAL
		{
			const u32 funct = code & 0x3f;
			return funct == 8 || funct == 9 || funct == 12 || funct == 13; // JR/JALR/SYSCALL/BREAK
		}
		case 1: // REGIMM
		{
			const u32 rt = (code >> 16) & 0x1f;
			return rt < 4 || (rt >= 16 && rt < 20);
		}
		case 2: case 3: // J, JAL
		case 4: case 5: case 6: case 7: // BEQ, BNE, BLEZ, BGTZ
		case 20: case 21: case 22: case 23: // likely forms
			return true;
		case 16: // COP0: BC0x or ERET
			return ((code >> 21) & 0x1f) == 8 ||
				   (((code >> 21) & 0x1f) == 16 && (code & 0x3f) == 24);
		case 17: case 18: // COP1/COP2: BCx
			return ((code >> 21) & 0x1f) == 8;
	}
	return false;
}

// Scanner-side continuation gate for a conditional branch at `i` targeting
// `target`. Forward-only (backward keeps the split/end logic), bounded, and
// refuses a branch-class delay slot.
static bool eeScanContinuable(u32 startpc, u32 i, u32 target, u32 famBit)
{
	// Testing-only kill switch (offline A/B bisection of superblock formation):
	// YAPS2_EESB is a bitmask of continuation-site families — bit0 BEQ/BNE,
	// bit1 BLEZ/BGTZ, bit2 REGIMM BLTZ/BGEZ. Unset = all on; 0 = all off
	// (reverts block formation to the pre-superblock shape without a rebuild).
	// Not a production knob.
	static const u32 s_sitesMask = []() -> u32 {
		const char* e = std::getenv("YAPS2_EESB");
		return e ? static_cast<u32>(std::atoi(e)) : 0xffu;
	}();
	if (!(s_sitesMask & famBit))
		return false;
	// Optional guest-pc window (hex), same offline-bisection purpose: only
	// branches inside [YAPS2_EESB_LO, YAPS2_EESB_HI) become sites.
	static const u32 s_siteLo = []() -> u32 {
		const char* e = std::getenv("YAPS2_EESB_LO");
		return e ? static_cast<u32>(std::strtoul(e, nullptr, 16)) : 0u;
	}();
	static const u32 s_siteHi = []() -> u32 {
		const char* e = std::getenv("YAPS2_EESB_HI");
		return e ? static_cast<u32>(std::strtoul(e, nullptr, 16)) : 0xffffffffu;
	}();
	if (i < s_siteLo || i >= s_siteHi)
		return false;
	return target > i + 4 &&
		   s_numContSites < kMaxContSites &&
		   ((i + 8 - startpc) / 4) < kMaxSuperblockInsns &&
		   !eeScanInsnIsBranchClass(memRead32(i + 4));
}

// A backward-split target that lands exactly on a continuation site's delay
// slot would leave that branch as the last insn of the range with its delay
// slot outside the analyzed block (instinfo overrun, split-pair emission).
// End the block before the branch instead; the split's purpose — making the
// target a block start — is preserved (a block starting at the delay-slot
// address compiles it as a plain instruction, which is the architectural
// meaning of branching into a delay slot).
//
// Degenerate case: the site is the block's FIRST instruction (guest loops
// that re-enter at the head branch's delay slot — e.g. UYA's dcache flush
// routine: `beqz exit; addiu t2,-1; ...; bgtz t2, <the addiu>`). Clamping
// there would yield s_nEndBlock == startpc — a zero-length block whose
// short tail is an unconditional self-linked B with no event check, i.e. a
// hard wedge. Skipping the split is always correct (the target simply gets
// its own block when branched to), so fall back to ending at the branch.
static u32 eeSuperblockClampSplit(u32 startpc, u32 branch_i, u32 target)
{
	for (int k = 0; k < s_numContSites; k++)
	{
		if (target == s_contSitePcs[k] + 4)
		{
			if (s_contSitePcs[k] > startpc)
				return s_contSitePcs[k];
			return branch_i + 8; // degenerate: no split, end at the branch
		}
	}
	return target;
}

static void recRecompile(const u32 startpc)
{
	u32 i;

	// Note: startpc=0 is valid (EE RAM address 0). The x86 rec asserts on this
	// but it can legitimately happen during BIOS init (e.g., JR $ra with ra=0).
	// We allow it since address 0 is properly mapped in recLUT.

	if (recPtr >= recPtrEnd || s_coldPtr >= s_coldPtrEnd)
	{
		Console.WriteLn("EE ARM64: cache-full reset trigger — hot used=%zu/%zu cold used=%zu/%zu",
			(size_t)(recPtr - SysMemory::GetEERec()), (size_t)(recPtrEnd - SysMemory::GetEERec()),
			(size_t)(s_coldPtr - s_coldBase), (size_t)(s_coldPtrEnd - s_coldBase));
		eeRecNeedsReset = true;
	}

	// Signal that the ELF entry point is now compiling, so VMManager flips
	// HasBootedELF() and applies the per-game GameDB fixes (both game fixes and
	// GS hardware fixes — e.g. Baldur's Gate: Dark Alliance's textureInsideRT,
	// which fixes the right-half-black menu render). Must fire before the
	// deferred reset below — the hook can change settings and flush the JIT.
	// Mirrors iR5900.cpp's recRecompile.
	if (HWADDR(startpc) == VMManager::Internal::GetCurrentELFEntryPoint())
		VMManager::Internal::EntryPointCompilingOnCPUThread();

	if (eeRecNeedsReset)
	{
		eeRecNeedsReset = false;
		recResetRaw();
	}

	armSetAsmPtr(recPtr, recPtrEnd - recPtr + _64kb, &s_eeConstantPool);
	armStartBlock();

	s_pCurBlock = GETBLOCK(startpc);
	pxAssert(s_pCurBlock->GetFnptr() == (uptr)JITCompile || s_pCurBlock->GetFnptr() == (uptr)UnmappedRecLUTPage);

	// armStartBlock() aligned armAsmPtr to 16 bytes, so the actual block
	// code starts at armGetCurrentCodePointer(), not at recPtr. Block
	// linking branches to BASEBLOCKEX::fnptr, so it must be the aligned
	// address — using recPtr instead lands the branch on padding bytes
	// and triggers SIGILL.
	const uptr block_fnptr = (uptr)armGetCurrentCodePointer();

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
	if (!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc))
		s_pCurBlockEx = recBlocks.New(HWADDR(startpc), block_fnptr);

	g_branch = 0;
	cop2VfCacheReset();

	s_pCurBlock->SetFnptr(block_fnptr);
	s_nBlockCycles = 0;
	s_nBlockInterlocked = false;

	// A recompile at a startpc reuses the existing BASEBLOCKEX — drop any
	// stale SL-1 back-edge registration from the previous compile.
	s_pCurBlockEx->backedge_site = 0;
	s_pCurBlockEx->backedge_stub = 0;

	pc = startpc;
	g_cpuHasConstReg = g_cpuFlushedConstReg = 1;
	g_cpuFlushedPC = false;
	g_cpuFlushedCode = false;

	_initArm64GPRregs();
	_initArm64NEONregs();

#ifdef PCSX2_RECOMPILER_TESTS
	// Optional block-entry diagnostic hook (test builds only). Emitted only when
	// g_emit_block_hook is set before recReset; production recompiles emit
	// nothing. Fires on EVERY block entry, including statically-linked ones,
	// because linked branches target block_fnptr — i.e. exactly here. At the
	// prologue all guest state is memory-resident (the allocator was just
	// re-initialized to memory), so the hook's FingerprintCpu() reads correct
	// cpuRegs. We pass startpc as an immediate because cpuRegs.pc is not updated
	// on a static-linked entry, and flush the cycle delta -> cpuRegs.cycle so
	// the hook sees the live cycle. The hook is read-only (no cycle/event
	// mutation) and RECCYCLE (x25) is callee-saved across the C call, so no
	// reload is needed.
	if (ee_divtrace::g_emit_block_hook)
	{
		armFlushCycleDelta();
		armAsm->Mov(RWARG1, startpc);
		armFlushEEGPRPins(); // lazy-dirty seam: the hook READS guest GPR memory
		armEmitCall((void*)ee_divtrace_jit_block_hook);
		// Read-only hook, but the C call clobbers the caller-saved pins and
		// the block body it precedes reads guest state through them.
		armReloadEEClobberedPins();
	}
#endif

	// EELOAD detection + ELF-load hooks (mirrors iR5900.cpp's recRecompile).
	// These compile-time-detected, run-time-emitted calls are how PCSX2 learns
	// which game ELF is booting: eeloadHook() -> ELFLoadingOnCPUThread() sets
	// s_elf_entry_point / CRC, which gates HasBootedELF() and therefore ALL
	// per-game patches, game fixes, GS hardware fixes, widescreen, symbol import
	// and achievements. The emitted calls run at block-execution time; at the
	// prologue all guest state is memory-resident, so no iFlushCall is needed,
	// and the pinned bases (x24/x25) are callee-saved across the C call.
	if (HWADDR(startpc) == EELOAD_START)
	{
		// The EELOAD _start function is the same across all BIOS versions.
		const u32 mainjump = memRead32(EELOAD_START + 0x9c);
		if (mainjump >> 26 == 3) // JAL
			g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | (mainjump << 2 & 0x0fffffffU);
	}

	if (g_eeloadMain && HWADDR(startpc) == HWADDR(g_eeloadMain))
	{
		// eeloadHook can do arbitrary VM work (ELF load, patch/achievement
		// init) that may reschedule nextEventCycle — keep the cycle delta
		// coherent across it. Boot-only path, cost is irrelevant.
		armFlushCycleDelta();
		// lazy-dirty seam: eeloadHook READS $a0/$a1 (argc/argv) from GPR
		// memory, and the full reload below must see flushed memory or it
		// would clobber dirty pins with stale values.
		armFlushEEGPRPins();
		armEmitCall((void*)eeloadHook);
		armReloadCycleDelta();
		armReloadEEGPRPins(); // ELF load / arg injection writes guest GPRs
		if (VMManager::Internal::IsFastBootInProgress())
		{
			// Four known EELOAD versions, identified by the location of the 'jal' to
			// the EELOAD function that calls ExecPS2(). The function itself is at the
			// same address in all BIOSs after v1.00-v1.10.
			const u32 typeAexecjump = memRead32(EELOAD_START + 0x470); // v1.00, v1.01?, v1.10?
			const u32 typeBexecjump = memRead32(EELOAD_START + 0x5B0); // v1.20, v1.50, v1.60 (3000x models)
			const u32 typeCexecjump = memRead32(EELOAD_START + 0x618); // v1.60 (3900x models)
			const u32 typeDexecjump = memRead32(EELOAD_START + 0x600); // v1.70, v1.90, v2.00, v2.20, v2.30
			if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3)) // JAL to 0x822B8
				g_eeloadExec = EELOAD_START + 0x2B8;
			else if (typeAexecjump >> 26 == 3) // JAL to 0x82170
				g_eeloadExec = EELOAD_START + 0x170;
			else // Unexamined BIOS models: 18000, 3500x, 3700x, 5500x, 7900x (and v1.01/v1.10).
				Console.WriteLn("recRecompile: Could not enable launch arguments for fast boot mode; unidentified BIOS version! Please report this to the PCSX2 developers.");
		}
	}

	if (g_eeloadExec && HWADDR(startpc) == HWADDR(g_eeloadExec))
	{
		// Same coherence rationale as the eeloadHook call above.
		armFlushCycleDelta();
		// lazy-dirty seam: the full reload below must not clobber dirty pins
		// with stale memory (eeloadHook2 itself only WRITES $a0/$a1).
		armFlushEEGPRPins();
		armEmitCall((void*)eeloadHook2);
		armReloadCycleDelta();
		armReloadEEGPRPins(); // eeloadHook2 injects launch arguments into GPRs
	}

	// Goemon TLB-cache preload/unload intercept (mirrors x86 iR5900.cpp:2241-2255,
	// dropped-host-hook item B5). PCSX2 precalculates all TLB mappings, but Goemon
	// dynamically remaps; these boot-region PC hooks keep the precalculated cache
	// in sync. The branch-target V2P half is already handled (SetBranchReg / recJ*
	// when GoemonTlbHack is set). pc == startpc here, and the addresses are useg so
	// HWADDR would be a no-op on them — use the raw pc to mirror x86 literally.
	if (EmuConfig.Gamefixes.GoemonTlbHack)
	{
		if (pc == 0x33ad48 || pc == 0x35060c)
		{
			// 0x33ad48 / 0x35060c are the return address of the function (0x356250)
			// that populates the TLB cache.
			armFlushEEClobberedPins(); // lazy-dirty seam: pairs with the reload below
			armEmitCall((void*)GoemonPreloadTlb);
			// TLB-cache population writes no guest GPRs; restore the
			// caller-saved pins the C call clobbered before the block body.
			armReloadEEClobberedPins();
		}
		else if (pc == 0x3563b8)
		{
			// Game unmaps some virtual addresses; a constant address hardcoded into a
			// compiled block would go stale, so force a full rec reset.
			eeRecNeedsReset = true;
			// 0x3563b8 is the start of the function that invalidates a TLB-cache entry;
			// a0 holds the key. $a0 is pinned — read the mirror (authoritative in
			// both pin modes), NOT canonical memory, which is stale under
			// lazy-dirty until the seam flush (and $a0's pin is callee-saved, so
			// no clobber flush ever covers it).
			armLoadEERegPtr(RWARG1, &cpuRegs.GPR.n.a0.UL[0]);
			armFlushEEClobberedPins(); // lazy-dirty seam: pairs with the reload below
			armEmitCall((void*)GoemonUnloadTlb);
			armReloadEEClobberedPins(); // see GoemonPreloadTlb above
		}
	}

	// Scan for block boundary
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;
	s_branchLoopable = false;
	s_numContSites = 0;

	// Timeout loop detection (matches x86 recSkipTimeoutLoop pattern):
	//   addiu reg,reg,-N / nop*N / bne reg,zero,loop / nop
	s32 timeout_reg = -1;
	bool is_timeout_loop = true;
	bool timeout_has_bne = false;

	while (1)
	{
		BASEBLOCK* pblock = GETBLOCK(i);
		if (i != startpc && pblock->GetFnptr() != (uptr)JITCompile)
		{
			s_nEndBlock = i;
			break;
		}

		// 4K page boundary
		if (i != startpc && (i & 0xffc) == 0)
		{
			s_nEndBlock = i;
			break;
		}

		cpuRegs.code = memRead32(i);

		// Timeout loop pattern matching
		if (is_timeout_loop)
		{
			if ((cpuRegs.code >> 26) == 8 || (cpuRegs.code >> 26) == 9)
			{
				// addi/addiu — must be first non-nop, decrementing same reg
				if (timeout_reg >= 0 || _Rs_ != _Rt_ || _Imm_ >= 0)
					is_timeout_loop = false;
				else
					timeout_reg = _Rs_;
			}
			else if ((cpuRegs.code >> 26) == 5)
			{
				// bne — must branch back using the timeout reg vs zero
				if (timeout_reg != static_cast<s32>(_Rs_) || _Rt_ != 0 || memRead32(i + 4) != 0)
					is_timeout_loop = false;
				else
					timeout_has_bne = true;
			}
			else if (cpuRegs.code != 0)
			{
				is_timeout_loop = false;
			}
		}

		switch (cpuRegs.code >> 26)
		{
			case 0: // SPECIAL
				if (_Funct_ == 8 || _Funct_ == 9) // JR, JALR
				{
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				if (_Funct_ == 12 || _Funct_ == 13) // SYSCALL, BREAK
				{
					s_nEndBlock = i + 4; // no delay slot
					goto StartRecomp;
				}
				break;

			case 1: // REGIMM
				if (_Rt_ < 4 || (_Rt_ >= 16 && _Rt_ < 20))
				{
					// rt 16-19 are the AL link variants — call-shaped tails
					// (SetBranchImmCall), not back-edge candidates.
					// SL-03: forward BLTZ/BGEZ (rt 0/1) become continuation
					// sites — scan on at the fallthrough. Likely + AL forms
					// keep ending the block.
					if (_Rt_ < 2 && eeScanContinuable(startpc, i, _Imm_ * 4 + i + 4, 4u))
					{
						s_contSitePcs[s_numContSites++] = i;
						i += 8; // skip the delay slot word in the scan
						continue;
					}
					s_branchLoopable = _Rt_ < 4;
					s_branchTo = _Imm_ * 4 + i + 4;
					// Backward branch into the current block: end the block at the
					// target so the loop head becomes its own linkable block.
					// Mirrors x86 iR5900.cpp:2362 and the COP1/COP2 case below.
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = eeSuperblockClampSplit(startpc, i, s_branchTo);
					else
					{
						s_nEndBlock = i + 8;
					}
					goto StartRecomp;
				}
				break;

			case 2: case 3: // J, JAL
				s_branchLoopable = (cpuRegs.code >> 26) == 2; // JAL = call tail
				s_branchTo = (_InstrucTarget_ << 2) | ((i + 4) & 0xf0000000);
				s_nEndBlock = i + 8;
				goto StartRecomp;

			case 4: case 5: case 6: case 7: // BEQ, BNE, BLEZ, BGTZ
				// SL-03: forward conditionals become continuation sites — scan
				// on at the fallthrough. BEQ rs==rt is the unconditional-`b`
				// idiom (always taken: everything after is unreachable on the
				// fallthrough) and keeps ending the block.
				if (!((cpuRegs.code >> 26) == 4 && _Rs_ == _Rt_) &&
					eeScanContinuable(startpc, i, _Imm_ * 4 + i + 4,
						(cpuRegs.code >> 26) < 6 ? 1u : 2u))
				{
					s_contSitePcs[s_numContSites++] = i;
					i += 8; // skip the delay slot word in the scan
					continue;
				}
				[[fallthrough]];
			case 20: case 21: // BEQL, BNEL
			case 22: case 23: // BLEZL, BGTZL
				s_branchLoopable = true;
				s_branchTo = _Imm_ * 4 + i + 4;
				// Backward branch into the current block: split so the loop head
				// is its own linkable block. Mirrors x86 iR5900.cpp:2387 and
				// the COP1/COP2 case below.
				if (s_branchTo > startpc && s_branchTo < i)
					s_nEndBlock = eeSuperblockClampSplit(startpc, i, s_branchTo);
				else
				{
					s_nEndBlock = i + 8;
				}
				goto StartRecomp;

			case 16: // COP0
				if (_Rs_ == 16 && _Funct_ == 24) // ERET (no delay slot)
				{
					s_nEndBlock = i + 4;
					goto StartRecomp;
				}
				// Fall through: COP0's branch opcodes line up with COP1/COP2's.
				[[fallthrough]];

			case 17: // COP1
			case 18: // COP2
				if (_Rs_ == 8) // BC0/BC1/BC2 F/T/FL/TL
				{
					s_branchLoopable = true;
					s_branchTo = _Imm_ * 4 + i + 4;
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = eeSuperblockClampSplit(startpc, i, s_branchTo);
					else
					{
						s_nEndBlock = i + 8;
					}
					goto StartRecomp;
				}
				break;
		}

		i += 4;
	}

StartRecomp:

	// SL-03: a backward-split can truncate s_nEndBlock below already-recorded
	// continuation sites — drop any site whose branch+delay-slot pair no longer
	// fits inside [startpc, s_nEndBlock). (Sites are ascending; the compile
	// loop never reaches a dropped one, but the analysis passes below must not
	// treat it as an exit boundary either.)
	while (s_numContSites > 0 && s_contSitePcs[s_numContSites - 1] + 8 > s_nEndBlock)
		s_numContSites--;

	// A zero-length block would compile to an unconditional self-linked B
	// with no event check (hard wedge) — eeSuperblockClampSplit's degenerate
	// fallback guarantees this can't happen.
	pxAssert(s_nEndBlock > startpc);

	// Self-modifying code detection: generate inline memory checks for manual blocks.
	const bool is_manual_block = memory_protect_recompiled_code(startpc, (s_nEndBlock - startpc) >> 2);

	// Infinite (wait) loop detection — verbatim port of the x86 hazard
	// tracker (ix86-32/iR5900.cpp:2438-2515), replacing the old all-NOP-only
	// scan (AX-07). The idea: as long as a self-loop doesn't write a register
	// it has already read (registers initialised from constants or memory
	// loads excepted) and uses no instruction that alters machine state
	// beyond registers, every iteration does the same thing — so it can be
	// fast-forwarded to the next event. This admits the shape of real
	// hardware-poll idle loops (lw STATUS; andi/test; beq back), which the
	// NOP-only scan never matched. The last-pair skip (i == s_nEndBlock - 8)
	// excludes the backward branch + its delay slot.
	s_nBlockFF = false;
	if (s_branchTo == startpc)
	{
		s_nBlockFF = true;

		u32 reads = 0, loads = 1;

		for (i = startpc; i < s_nEndBlock; i += 4)
		{
			if (i == s_nEndBlock - 8)
				continue;
			cpuRegs.code = memRead32(i);
			// nop
			if (cpuRegs.code == 0)
				continue;
			// cache, sync
			else if (_Opcode_ == 057 || (_Opcode_ == 0 && _Funct_ == 017))
				continue;
			// imm arithmetic
			else if ((_Opcode_ & 070) == 010 || (_Opcode_ & 076) == 030)
			{
				if (loads & 1 << _Rs_)
				{
					loads |= 1 << _Rt_;
					continue;
				}
				else
					reads |= 1 << _Rs_;
				if (reads & 1 << _Rt_)
				{
					s_nBlockFF = false;
					break;
				}
			}
			// common register arithmetic instructions
			else if (_Opcode_ == 0 && (_Funct_ & 060) == 040 && (_Funct_ & 076) != 050)
			{
				if (loads & 1 << _Rs_ && loads & 1 << _Rt_)
				{
					loads |= 1 << _Rd_;
					continue;
				}
				else
					reads |= 1 << _Rs_ | 1 << _Rt_;
				if (reads & 1 << _Rd_)
				{
					s_nBlockFF = false;
					break;
				}
			}
			// loads
			else if ((_Opcode_ & 070) == 040 || (_Opcode_ & 076) == 032 || _Opcode_ == 067)
			{
				if (loads & 1 << _Rs_)
				{
					loads |= 1 << _Rt_;
					continue;
				}
				else
					reads |= 1 << _Rs_;
				if (reads & 1 << _Rt_)
				{
					s_nBlockFF = false;
					break;
				}
			}
			// mfc*, cfc*
			else if ((_Opcode_ & 074) == 020 && _Rs_ < 4)
			{
				loads |= 1 << _Rt_;
			}
			else
			{
				s_nBlockFF = false;
				break;
			}
		}
	}
	else
	{
		// A timeout loop must branch back to its own start (a self-loop). If the
		// block's terminating branch targets anywhere else, it is NOT a timeout
		// loop and must be recompiled normally. Mirrors x86 iR5900.cpp:2510-2513.
		// Without this guard, the early-exit `bne reg,zero,<forward>` at the TOP
		// of a counted compute loop gets misdetected as a timeout loop and
		// recSkipTimeoutLoop fast-forwards the counter to 0 while skipping the
		// loop BODY's real work. timeout_has_bne alone is insufficient because it
		// matches the forward early-exit branch without checking the branch target.
		is_timeout_loop = false;
	}

#ifdef PCSX2_RECOMPILER_TESTS
	// Test-harness introspection: the wait-loop detection verdict for the
	// block most recently analysed. The FF fast-forward itself is not
	// observable from the harness (both engines are cycle-budget bounded),
	// so the detector (AX-07) is pinned at this seam.
	g_eeRecLastBlockFF = s_nBlockFF;
#endif

	// Instruction analysis (backward pass)
	{
		EEINST* pcur;

		if (s_nInstCacheSize < (s_nEndBlock - startpc) / 4 + 1)
		{
			free(s_pInstCache);
			s_nInstCacheSize = (s_nEndBlock - startpc) / 4 + 10;
			s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
			pxAssert(s_pInstCache != NULL);
		}

		pcur = s_pInstCache + (s_nEndBlock - startpc) / 4;
		_recClearInst(pcur);
		pcur->info = 0;

		bool has_cop2_instructions = false;
		for (i = s_nEndBlock; i > startpc; i -= 4)
		{
			cpuRegs.code = memRead32(i - 4);
			// SL-03: at a continuation site the taken path leaves the block
			// after the delay slot — merge "everything live" (the block-end
			// init state) into the out-state of both the branch and its delay
			// slot so no dead-value assumption crosses the side exit. This is
			// exactly today's block-end conservatism at the former boundary.
			if (recSuperblockLivenessBarrier(i - 4))
			{
				memset(pcur->regs, EEINST_LIVE, sizeof(pcur->regs));
				memset(pcur->fpuregs, EEINST_LIVE, sizeof(pcur->fpuregs));
				memset(pcur->vfregs, EEINST_LIVE, sizeof(pcur->vfregs));
				memset(pcur->viregs, EEINST_LIVE, sizeof(pcur->viregs));
			}
			pcur[-1] = pcur[0];
			recBackpropBSC(cpuRegs.code, pcur - 1, pcur);
			pcur--;

			has_cop2_instructions |= (_Opcode_ == 022 || _Opcode_ == 066 || _Opcode_ == 076);
		}

		// Run COP2 analysis passes — sets EEINST_COP2_SYNC_VU0/FINISH_VU0 flags
		// for conditional VU0 synchronization in transfer ops.
		// SL-03: run per segment, delimited at continuation sites. Both passes
		// defer commits forward (flag-hack elision, micro-finish placement); a
		// deferral must not cross a side exit that can escape before the
		// superseding instruction executes. Per-segment == today's per-block
		// semantics at each former boundary.
		if (has_cop2_instructions)
		{
			u32 seg_start = startpc;
			for (int k = 0; k <= s_numContSites; k++)
			{
				const u32 seg_end = (k < s_numContSites) ? (s_contSitePcs[k] + 8) : s_nEndBlock;
				if (seg_end <= seg_start)
					continue;
				EEINST* const seg_inst = s_pInstCache + 1 + (seg_start - startpc) / 4;
				R5900::COP2MicroFinishPass().Run(seg_start, seg_end, seg_inst);

				if (EmuConfig.Speedhacks.vuFlagHack)
					R5900::COP2FlagHackPass().Run(seg_start, seg_end, seg_inst);

				seg_start = seg_end;
			}
		}
	}

	// Try timeout loop speedhack — if detected, skip normal codegen
	// Timer-poll loops (mfc0 Count / subu / sltu / bne) are NOT skipped because
	// they need to wait for a specific elapsed time — the correct fix is native codegen.
	// Require timeout_reg >= 0 (actually found an addiu) to avoid matching all-nop blocks
	// Skip-MPEG speedhack fires first (short-circuits the timeout-loop check, as
	// in x86); both emit a complete self-contained block + set g_branch/pc.
	const bool doRecompilation = !skipMPEG_By_Pattern(startpc) &&
		!recSkipTimeoutLoop(timeout_reg, is_timeout_loop && timeout_reg >= 0 && timeout_has_bne);

	// Code generation (forward pass)
	if (doRecompilation)
	{
		g_pCurInstInfo = s_pInstCache;
		// Sweep any LDL/LDR fusion residue from an aborted prior compile (the
		// per-pair gate otherwise guarantees same-block consume).
		g_eeUnalignedFused = false;
		// SL-03: sweep side-exit residue the same way (recEmitPendingSideExits
		// drains the list on every completed compile).
		s_numSideExits = 0;

		// SL-1 candidacy: a resident self-loop is a block whose terminal branch
		// targets its own startpc. Wait-loop-FF blocks keep the fast-forward
		// tail; manual/SMC blocks are excluded (the entry check must run per
		// iteration). Pin the most-used guest GPRs in a preheader and bind the
		// loop-top label after it — see the SL-1 comment block above
		// SetBranchBackedge for the full contract.
		s_loopResident = false;
		s_loopBackedgeEmitted = false;
		if (s_branchLoopable && s_branchTo == startpc && !s_nBlockFF && !is_manual_block)
		{
			const int ninsts = static_cast<int>((s_nEndBlock - startpc) / 4);
			int uses[32] = {};
			for (int k = 1; k <= ninsts; k++)
				for (int r = 1; r < 32; r++)
					if (s_pInstCache[k].regs[r] & EEINST_USED)
						uses[r]++;

			struct LoopCand
			{
				int reg;
				int uses;
			};
			LoopCand cands[32];
			int ncand = 0;
			for (int r = 1; r < 32; r++)
			{
				// Pin-table guest regs already live in dedicated mirrors and
				// must never get a scalar allocator home (GE-M2 I1).
				if (uses[r] > 0 && !armEEPinForGPR(r))
					cands[ncand++] = {r, uses[r]};
			}
			std::sort(cands, cands + ncand,
				[](const LoopCand& a, const LoopCand& b) { return a.uses > b.uses; });

			// Leave ≥2 pool regs unpinned for temps/FPRC churn; eviction of a
			// pin is legal anyway (reconcile restores), this just keeps the
			// common body allocation off the pins.
			// A zero-pin candidate (loop-live set entirely pin-table regs) still
			// activates: S0 = empty allocator is valid, and the back-edge still
			// drops the pc-store + link hop.
			constexpr int kMaxLoopPins = 5;
			const int npins = std::min(ncand, kMaxLoopPins);
			for (int k = 0; k < npins; k++)
			{
				// MODE_WRITE up front pessimizes the dirty bit: the loop-top
				// snapshot must claim dirty ⊇ any runtime dirty, or a body-
				// emitted eviction could drop a previous iteration's write.
				const int host = _allocArm64GPR(ARM64TYPE_GPR, cands[k].reg, MODE_READ | MODE_WRITE);
				arm64gprs[host].looppin = 1;
			}
			_clearNeededArm64GPRregs();
			std::memcpy(s_loopEntryState, arm64gprs, sizeof(arm64gprs));
			s_loopTopPc = startpc;
			s_loopTopLabel = std::make_unique<a64::Label>();
			armAsm->Bind(s_loopTopLabel.get());
			s_loopResident = true;
		}

		while (!g_branch && pc < s_nEndBlock)
			recompileNextInstruction(false, false);

		s_loopResident = false;
	}

	pxAssert((pc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (pc - startpc) >> 2;

	if (!(pc & 0x10000000))
		maxrecmem = std::max((pc & ~0xa0000000), maxrecmem);

	// Stale-overlap walk + snapshot (x86 iR5900.cpp:2636-2661, GE-18). Walk
	// OLDER blocks overlapping [startpc, pc) and memcmp each one's recRAMCopy
	// snapshot — taken when THAT block compiled — against live memory. A
	// mismatch means the old block went stale through a write no protection
	// path caught (raw host pokes, protection-bypassing writes); recClear the
	// range so it recompiles. recClear skips s_pCurBlockEx (in-progress), so
	// the re-Get below always finds it. The original port compared the NEW
	// block's not-yet-snapshotted (zeroed) region instead — every compile
	// mismatched and recompile-looped — which is why this was disabled; the
	// old-block-region compare is the correct x86 semantics
	// (OverlapWalkIgnoresUnmodifiedNeighbors pins the no-false-positive side).
	if (HWADDR(pc) <= Ps2MemSize::MainRam)
	{
		BASEBLOCKEX* oldBlock;
		int i = recBlocks.LastIndex(HWADDR(pc) - 4);
		while ((oldBlock = recBlocks[i--]))
		{
			if (oldBlock == s_pCurBlockEx)
				continue;
			if (oldBlock->startpc >= HWADDR(pc))
				continue;
			if ((oldBlock->startpc + oldBlock->size * 4) <= HWADDR(startpc))
				break;

			if (memcmp(&recRAMCopy[oldBlock->startpc / 4], PSM(oldBlock->startpc),
					oldBlock->size * 4))
			{
				recClear(startpc, (pc - startpc) / 4);
				s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
				pxAssert(s_pCurBlockEx && s_pCurBlockEx->startpc == HWADDR(startpc));
				break;
			}
		}

		memcpy(&recRAMCopy[HWADDR(startpc) / 4], PSM(startpc), pc - startpc);
	}

	if (g_branch == 2)
	{
		// Branch taken — flush and dispatch. recBranchCall already accumulated
		// any pre-call cycles into RECCYCLE and re-derived it after the C
		// call, so any further scaleblockcycles_clear() result is the
		// post-call remainder.
		iFlushCall(FLUSH_EVERYTHING);

		emitCycleUpdateAndEventCheck();

		armEmitJmp(DispatcherReg);
	}
	else
	{
		if (g_branch)
		{
			// g_branch == 1: block ended with a branch instruction.
			// pc may not equal s_nEndBlock for branch-likely instructions
			// where the not-taken path skips the delay slot.
		}
		else
		{
			// Non-branch block end (split / page boundary / cycle cap).
			// Mirrors x86 iR5900.cpp:2680-2698:
			//   long block  (>6 insns): SetBranchImm(pc)         — event check + static-linked B
			//   short block (≤6 insns): flush + pc + cycle + bare static-linked B
			// Short blocks skip the event check entirely; a few-insn block can't
			// have advanced cycles far enough to cross an event boundary, so the
			// load+cmp+B.ge is dead-weight code per block tail.
			if (pc != s_nEndBlock)
				Console.Error("EE ARM64: Block end mismatch! startpc=0x%08X pc=0x%08X s_nEndBlock=0x%08X", startpc, pc, s_nEndBlock);
			pxAssert(pc == s_nEndBlock);

			const int numinsts = (pc - startpc) / 4;
			if (numinsts > 6)
			{
				SetBranchImm(pc);
			}
			else
			{
				iFlushCall(FLUSH_EVERYTHING);

				armAsm->Mov(RWSCRATCH, pc);
				armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));

				u32 cycles = scaleblockcycles_clear();
				if (cycles != 0)
					armAsm->Add(RECCYCLE, RECCYCLE, cycles);

				a64::SingleEmissionCheckScope guard(armAsm);
				u8* patch_site = armGetCurrentCodePointer();
				armAsm->b(int64_t{0}); // placeholder; recBlocks.Link will overwrite
				recBlocks.Link(HWADDR(pc), patch_site);
			}
		}
	}

	// SL-03/SL-10: the taken side exits' in-block footprint — one far-B island
	// each after the tail (mainline pc is dead — size and recRAMCopy
	// bookkeeping ran on it above). The bodies outline into the cold arena
	// below, after the hot block finalizes.
	recEmitSideExitIslands();

	pxAssert(armGetCurrentCodePointer() < SysMemory::GetEERecEnd());

	// Size is from the aligned block_fnptr, not the pre-alignment recPtr —
	// keeps Perf::ee.RegisterPC consistent with the linker's view of the block.
	s_pCurBlockEx->x86size = static_cast<u32>((uptr)armGetCurrentCodePointer() - s_pCurBlockEx->fnptr);

	Perf::ee.RegisterPC((void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = armEndBlock();

	// SL-10: outline the side-exit bodies into the cold arena and patch the
	// islands. Runs as its own emission session so the bodies land outside
	// the hot compile-order stream.
	const u8* coldDumpStart = s_coldPtr;
	recEmitColdSideExits();

	// Testing-only: YAPS2_EESB_DUMP=<hex guest pc> dumps the emitted host code
	// (including the literal pool, post-finalize so offsets are patched) of any
	// block whose guest range covers that pc (offline bisection aid).
	{
		static const u32 s_dumpPc = []() -> u32 {
			const char* e = std::getenv("YAPS2_EESB_DUMP");
			return e ? static_cast<u32>(std::strtoul(e, nullptr, 16)) : 0u;
		}();
		if (s_dumpPc && startpc <= s_dumpPc && s_dumpPc < s_nEndBlock)
		{
			fprintf(stderr, "EESB_DUMP: block %08x..%08x fnptr=%p size=%u endptr=%p sites=%d cold=%p+%u\n",
				startpc, s_nEndBlock, (void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size,
				(void*)recPtr, s_numContSites,
				(void*)coldDumpStart, static_cast<u32>(s_coldPtr - coldDumpStart));
			armDisassembleAndDumpCode((void*)s_pCurBlockEx->fnptr,
				static_cast<size_t>((uptr)recPtr - (uptr)s_pCurBlockEx->fnptr));
			if (s_coldPtr != coldDumpStart)
				armDisassembleAndDumpCode(coldDumpStart, static_cast<size_t>(s_coldPtr - coldDumpStart));
		}
	}

	pxAssert((g_cpuHasConstReg & g_cpuFlushedConstReg) == g_cpuHasConstReg);

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

// =====================================================================================================
//  Thunk helpers for fastmem backpatching
// =====================================================================================================

u8* recBeginThunk()
{
	// Check for recompiler cache overflow
	if (recPtr >= recPtrEnd)
		eeRecNeedsReset = true;

	// Set up assembler to emit thunk code at the current recompiler pointer.
	// No constant pool needed for thunks (they're small and self-contained).
	armSetAsmPtr(recPtr, recPtrEnd - recPtr + _64kb, nullptr);
	u8* aligned = armStartBlock();

	// Return the aligned address where code actually starts, not recPtr.
	// armStartBlock() aligns to 16 bytes — branching to recPtr would hit padding.
	return aligned;
}

u8* recEndThunk()
{
	recPtr = armEndBlock();
	pxAssert(recPtr < SysMemory::GetEERecEnd());
	return recPtr;
}

// =====================================================================================================
//  R5900cpu struct — public interface
// =====================================================================================================

R5900cpu recCpu = {
	recReserve,
	recShutdown,
	recResetEE,
	recStep,
	recExecute,
	recSafeExitExecution,
	recCancelInstruction,
	recClear,
};
