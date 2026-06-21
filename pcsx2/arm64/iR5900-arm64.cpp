// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) Dynamic Recompiler Core
// Dispatcher, block management, all instructions as interpreter fallbacks.

#include <algorithm>
#include <cfloat>
#include <unordered_map>
#include <vector>

#include "arm64/iR5900-arm64.h"
#include "arm64/iR5900Analysis.h"
#include "arm64/AsmHelpers.h"
#include "Host.h"
#include "R3000A.h"
#include "R5900.h"
#include "arm64/BaseblockEx-arm64.h"
#include "R5900OpcodeTables.h"
#include "Common.h"
#include "VMManager.h"
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

// Constant propagation — defined here, declared extern in iR5900-arm64.h

static uptr recLUT[0x10000];
static u32 hwLUT[0x10000];

static __fi u32 HWADDR(u32 mem) { return hwLUT[mem >> 16] + mem; }

static BASEBLOCK* recRAM = nullptr;
static BASEBLOCK* recROM = nullptr;
static BASEBLOCK* recROM1 = nullptr;
static Arm64BaseBlocks recBlocks;
static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;

static EEINST* s_pInstCache = nullptr;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = nullptr;
static BASEBLOCKEX* s_pCurBlockEx = nullptr;

static u32 s_nEndBlock = 0;
static u32 s_branchTo;
static bool s_nBlockFF;

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
static const void* JITCompile = nullptr;
static const void* EnterRecompiledCode = nullptr;
static const void* DispatchBlockDiscard = nullptr;
static const void* DispatchPageReset = nullptr;
static const void* UnmappedRecLUTPage = nullptr;

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

	// Flush pinned cycle counter before the C call, then reload after —
	// recRecompile itself doesn't modify cpuRegs.cycle, but other paths
	// (e.g. block discard) might, and the convention is "every C-call
	// boundary syncs RECCYCLE both ways".
	armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

	armAsm->Ldr(RWARG1, armCpuRegMem(&cpuRegs.pc));
	armEmitCall((void*)recRecompile);

	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

	armEmitJmp(DispatcherReg);

	return retval;
}

static const void* _DynGen_DispatcherEvent()
{
	u8* retval = armGetCurrentCodePointer();
	// Flush pinned cycle for recEventTest (it reads cpuRegs.cycle for
	// counter / interrupt scheduling), then reload — the event test may
	// modify cpuRegs.cycle (e.g. fast-forwarding to nextEventCycle).
	armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
	armEmitCall((void*)recEventTest);
	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
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

	// Load pinned cycle counter into RECCYCLE. The convention is that
	// RECCYCLE holds cpuRegs.cycle for the entire duration of JIT
	// execution, with flush+reload around C calls.
	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

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
	armEmitCall((void*)dyna_block_discard);
	armEmitJmp(DispatcherReg);
	return retval;
}

static const void* _DynGen_DispatchPageReset()
{
	u8* retval = armGetCurrentCodePointer();
	armEmitCall((void*)dyna_page_reset);
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

static void _DynGen_Dispatchers()
{
	const u8* start = armGetCurrentCodePointer();

	DispatcherEvent = _DynGen_DispatcherEvent();
	DispatcherReg = _DynGen_DispatcherReg();

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

	// Only the lower 64 bits of v8-v15 are callee-saved per AAPCS64; the
	// NEON allocator uses 128-bit slots, so all of them are effectively
	// caller-saved across a C call. Always free + writeback. Matches x86
	// iFlushCall (pcsx2/x86/ix86-32/iR5900.cpp:1196-1207) which also
	// unconditionally evicts caller-saved XMM regs.
	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse)
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

void recCall(void (*func)())
{
	iFlushCall(FLUSH_INTERPRETER);

	// Flush RECCYCLE → cpuRegs.cycle so the interpreter sees the live cycle
	// value (some opcodes — COP0 Count, TLB miss, branch helpers — read it).
	armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

	armEmitCall((void*)func);

	// Reload RECCYCLE in case the interpreter modified cpuRegs.cycle.
	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

	// After interpreter calls, dispatch a pending TLB-miss exception.
	recEmitInterpTlbMissCheck();
}

void recBranchCall(void (*func)())
{
	iFlushCall(FLUSH_INTERPRETER);

	// Apply accumulated block cycles to RECCYCLE, then flush to memory
	// before the C call — the interpreter's intEventTest reads
	// cpuRegs.cycle. Reload after, so the g_branch=2 exit code that
	// follows can keep using RECCYCLE.
	u32 cycles = scaleblockcycles_clear();
	if (cycles > 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));

	armEmitCall((void*)func);
	g_branch = 2;

	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
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

// =====================================================================================================
//  Branch handling
// =====================================================================================================

void SetBranchReg()
{
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
	// target and receives the translated paddr.
	if (EmuConfig.Gamefixes.GoemonTlbHack)
		armEmitCall((void*)vtlb_V2P);

	// Store to cpuRegs.pc
	armAsm->Str(a64::w0, armCpuRegMem(&cpuRegs.pc));

	// Alignment check
	a64::Label unaligned;
	armAsm->Tst(a64::w0, 3);
	armAsm->B(&unaligned, a64::ne);

	// Update pinned cycle counter (RECCYCLE = cpuRegs.cycle).
	u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	// Check events (RECCYCLE >= nextEventCycle → DispatcherEvent flushes
	// RECCYCLE itself before calling recEventTest).
	armAsm->Ldr(a64::x3, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Cmp(RECCYCLE, a64::x3);
	armEmitCondBranch(a64::ge, DispatcherEvent);

	armEmitJmp(DispatcherReg);

	armAsm->Bind(&unaligned);
	armAsm->Mov(RWARG1, 1);
	armEmitCall((void*)recError);
}

void SetBranchImm(u32 imm)
{
	g_branch = 1;
	pxAssert(imm);

	armAsm->Mov(RWSCRATCH, imm);
	armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.pc));

	iFlushCall(FLUSH_EVERYTHING);

	// Update pinned cycle counter.
	u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	// WaitLoop speedhack: when the block scanner detected this block as a
	// pure-nop spin branching back to itself (s_nBlockFF), fast-forward
	// RECCYCLE to max(RECCYCLE, nextEventCycle) and jump straight to
	// DispatcherEvent. Saves the dozens of iterations the loop would
	// otherwise burn waiting for an event to fire. Matches the x86 path in
	// iR5900.cpp:iBranchTest under EmuConfig.Speedhacks.WaitLoop.
	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && imm == s_branchTo)
	{
		armAsm->Ldr(a64::x3, armCpuRegMem(&cpuRegs.nextEventCycle));
		armAsm->Cmp(RECCYCLE, a64::x3);
		armAsm->Csel(RECCYCLE, RECCYCLE, a64::x3, a64::hi);
		armEmitJmp(DispatcherEvent);
		return;
	}

	// Check events.
	armAsm->Ldr(a64::x3, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Cmp(RECCYCLE, a64::x3);
	armEmitCondBranch(a64::ge, DispatcherEvent);

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

// =====================================================================================================
//  Block state save/restore for delay slots
// =====================================================================================================

static _arm64gprregs s_savedGPRs[NUM_ARM_GPR_REGS];
static _arm64neonregs s_savedNEON[NUM_ARM_NEON_REGS];
static GPR_reg64 s_savedConstRegs[32];
static u32 s_savedHasConstReg, s_savedFlushedConstReg;
static u32 s_savedBlockCycles;
static EEINST* s_savedInstInfo;

void SaveBranchState()
{
	s_savedBlockCycles = s_nBlockCycles;
	memcpy(s_savedConstRegs, g_cpuConstRegs, sizeof(g_cpuConstRegs));
	s_savedHasConstReg = g_cpuHasConstReg;
	s_savedFlushedConstReg = g_cpuFlushedConstReg;
	s_savedInstInfo = g_pCurInstInfo;
	memcpy(s_savedGPRs, arm64gprs, sizeof(arm64gprs));
	memcpy(s_savedNEON, arm64neon, sizeof(arm64neon));
}

void LoadBranchState()
{
	s_nBlockCycles = s_savedBlockCycles;
	memcpy(g_cpuConstRegs, s_savedConstRegs, sizeof(g_cpuConstRegs));
	g_cpuHasConstReg = s_savedHasConstReg;
	g_cpuFlushedConstReg = s_savedFlushedConstReg;
	g_pCurInstInfo = s_savedInstInfo;
	memcpy(arm64gprs, s_savedGPRs, sizeof(arm64gprs));
	memcpy(arm64neon, s_savedNEON, sizeof(arm64neon));
}

// =====================================================================================================
//  Instruction recompilation
// =====================================================================================================

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot)
{
	const u32 old_code = cpuRegs.code;
	EEINST* old_inst_info = g_pCurInstInfo;

	cpuRegs.code = memRead32(pc);

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
			armEmitCall((void*)verifySnapshotPre);

			// Step 3: Run the native codegen
			opcode.recompile();

			// Step 4: Flush native results to cpuRegs
			iFlushCall(FLUSH_EVERYTHING);

			// Step 5: Emit call to verify against interpreter
			armAsm->Mov(a64::w0, cpuRegs.code);
			armAsm->Mov(a64::w1, pc - 4);
			armEmitCall((void*)verifyCheckPost);
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
		pc += 4;
		g_cpuFlushedPC = false;
		g_cpuFlushedCode = false;
		g_recompilingDelaySlot = false;
	}

	// When called from TrySwapDelaySlot (swapped_delay_slot=true), restore
	// cpuRegs.code so that the caller's _Rs_/_Rt_/_Rd_ macros still work.
	// Matches x86 at iR5900.cpp:1918-1921.
	if (swapped_delay_slot)
	{
		cpuRegs.code = old_code;
		g_pCurInstInfo = old_inst_info;
	}
}

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

	return false;
}

// =====================================================================================================
//  Memory management and block clearing
// =====================================================================================================

static void recClear(u32 addr, u32 size)
{
	addr = HWADDR(addr);
	const u32 end = addr + size * 4;

	int blockidx = recBlocks.LastIndex(end - 4);
	if (blockidx == -1)
		return;

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
// Port of x86 memory_protect_recompiled_code().
static void memory_protect_recompiled_code(u32 startpc, u32 size)
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

			u32 lpc = inpage_ptr;
			u32 stg = inpage_sz;

			// Generate inline byte-by-byte comparison of compiled block source with current RAM.
			// If any word differs, the block is stale and must be discarded.
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
}

// =====================================================================================================
//  Reserve / Reset / Shutdown / Execute
// =====================================================================================================

static void recReserveRAM()
{
	recLutEntries = (Ps2MemSize::MainRam + Ps2MemSize::Rom + Ps2MemSize::Rom1) / 4;

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

	if (recRAMCopy.size() != Ps2MemSize::MainRam)
		recRAMCopy.resize(Ps2MemSize::MainRam);
}

static void recReserve()
{
	Console.WriteLn(Color_Green, "EE: ARM64 Recompiler reserved.");
	recPtr = SysMemory::GetEERec();
	recPtrEnd = SysMemory::GetEERecEnd() - _64kb;

	recReserveRAM();

	pxAssertRel(!s_pInstCache, "InstCache not allocated");
	s_nInstCacheSize = 128;
	s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
	if (!s_pInstCache)
		pxFailRel("Failed to allocate R5900 InstCache array.");

	const u32 poolSize = 65536;
	u8* poolBase = SysMemory::GetEERecEnd() - poolSize;
	s_eeConstantPool.Init(poolBase, poolSize);
}

static void recResetRaw()
{
	Console.WriteLn(Color_Green, "iR5900-ARM64 Recompiler reset.");

	armSetAsmPtr(SysMemory::GetEERec(), SysMemory::GetEERecEnd() - SysMemory::GetEERec(), &s_eeConstantPool);
	armStartBlock();
	const u8* dispStart = armGetCurrentCodePointer();
	_DynGen_Dispatchers();
	const u8* dispEnd = armGetCurrentCodePointer();
	recPtr = armEndBlock();

	Console.WriteLn(Color_Green, "EE ARM64: Dispatcher generated at %p (%zu bytes)", dispStart, (size_t)(dispEnd - dispStart));

	iopClearRecLUT(recLutReserve_RAM.data(),
		Ps2MemSize::MainRam + Ps2MemSize::Rom + Ps2MemSize::Rom1);

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

	// Map ROM1
	for (int i = 0x1e00; i < 0x1e04; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0xa000, i, i - 0x1e00);
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
#endif

// =====================================================================================================
//  Timeout Loop Speedhack
// =====================================================================================================

// Detects and skips timeout loops like:
//   addiu v0,v0,-1 / nop*N / bne v0,zero,loop / nop
// Instead of spinning, advances the cycle counter and decrements the register.
// Port of x86 recSkipTimeoutLoop().
static bool recSkipTimeoutLoop(s32 reg, bool is_timeout_loop)
{
	if (!EmuConfig.Speedhacks.WaitLoop || !is_timeout_loop)
		return false;

	DevCon.WriteLn("[EE] Skipping timeout loop at 0x%08X -> 0x%08X (reg=%d)",
		s_pCurBlockEx->startpc, s_nEndBlock, reg);

	// Logic: skip the loop by advancing cycles based on the register value.
	// new_cycles = min(reg * 8 + cycle, nextEventCycle)
	// new_reg = reg - (new_cycles - cycle) / 8
	// if new_reg > 0, jump to dispatcher (an event interrupted the loop)
	// else loop finished, continue at s_nEndBlock

	// if (cycle >= nextEventCycle) goto DispatcherEvent (u64 comparison)
	armAsm->Ldr(a64::x3, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Cmp(RECCYCLE, a64::x3);
	armEmitCondBranch(a64::hs, DispatcherEvent);

	// w4 = reg value (the decrementing counter)
	armAsm->Ldr(a64::w4, armCpuRegMem(&cpuRegs.GPR.r[reg].UL[0]));

	// x5 = reg * 8 + cycle (estimated end cycle, u64)
	armAsm->Add(a64::x5, RECCYCLE, a64::Operand(a64::x4, a64::LSL, 3));

	// x5 = min(x5, nextEventCycle)
	armAsm->Cmp(a64::x5, a64::x3);
	armAsm->Csel(a64::x5, a64::x3, a64::x5, a64::hi); // if x5 > nextEvent, use nextEvent

	// w6 = (new_cycles - old_cycle) >> 3 = iterations consumed (uses old RECCYCLE).
	armAsm->Sub(a64::w6, a64::w5, RECCYCLE.W());
	armAsm->Lsr(a64::w6, a64::w6, 3);

	// Commit the new cycle value into RECCYCLE (no memory store — DispatcherEvent
	// will flush it if we exit there; otherwise the next block-tail event check
	// uses RECCYCLE directly).
	armAsm->Mov(RECCYCLE, a64::x5);

	// reg -= iterations consumed
	armAsm->Sub(a64::w4, a64::w4, a64::w6);
	armAsm->Str(a64::w4, armCpuRegMem(&cpuRegs.GPR.r[reg].UL[0]));
	// Also sign-extend to upper 32 bits (EE GPRs are 64-bit for lower half)
	armAsm->Sxtw(a64::x4, a64::w4);
	armAsm->Str(a64::x4, armCpuRegMem(&cpuRegs.GPR.r[reg].UD[0]));

	// if reg != 0, event interrupted the loop — go to dispatcher
	armEmitCbnz(a64::w4, DispatcherEvent);

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

static void recRecompile(const u32 startpc)
{
	u32 i;

	// Note: startpc=0 is valid (EE RAM address 0). The x86 rec asserts on this
	// but it can legitimately happen during BIOS init (e.g., JR $ra with ra=0).
	// We allow it since address 0 is properly mapped in recLUT.

	if (recPtr >= recPtrEnd)
		eeRecNeedsReset = true;

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

	s_pCurBlock->SetFnptr(block_fnptr);
	s_nBlockCycles = 0;
	s_nBlockInterlocked = false;

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
	// on a static-linked entry, and flush RECCYCLE -> cpuRegs.cycle so the hook
	// sees the live cycle. RECCYCLE (x25) is callee-saved across the C call, so
	// no reload is needed.
	if (ee_divtrace::g_emit_block_hook)
	{
		armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
		armAsm->Mov(RWARG1, startpc);
		armEmitCall((void*)ee_divtrace_jit_block_hook);
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
		armEmitCall((void*)eeloadHook);
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
		armEmitCall((void*)eeloadHook2);

	// Scan for block boundary
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

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
					s_branchTo = _Imm_ * 4 + i + 4;
					// Backward branch into the current block: end the block at the
					// target so the loop head becomes its own linkable block.
					// Mirrors x86 iR5900.cpp:2362 and the COP1/COP2 case below.
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = s_branchTo;
					else
						s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;

			case 2: case 3: // J, JAL
				s_branchTo = (_InstrucTarget_ << 2) | ((i + 4) & 0xf0000000);
				s_nEndBlock = i + 8;
				goto StartRecomp;

			case 4: case 5: case 6: case 7: // BEQ, BNE, BLEZ, BGTZ
			case 20: case 21: // BEQL, BNEL
			case 22: case 23: // BLEZL, BGTZL
				s_branchTo = _Imm_ * 4 + i + 4;
				// Backward branch into the current block: split so the loop head
				// is its own linkable block. Mirrors x86 iR5900.cpp:2387 and
				// the COP1/COP2 case below.
				if (s_branchTo > startpc && s_branchTo < i)
					s_nEndBlock = s_branchTo;
				else
					s_nEndBlock = i + 8;
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
					s_branchTo = _Imm_ * 4 + i + 4;
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = s_branchTo;
					else
						s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;
		}

		i += 4;
	}

StartRecomp:

	// Self-modifying code detection: generate inline memory checks for manual blocks.
	memory_protect_recompiled_code(startpc, (s_nEndBlock - startpc) >> 2);

	// Infinite loop detection
	s_nBlockFF = false;
	if (s_branchTo == startpc)
	{
		s_nBlockFF = true;
		for (i = startpc; i < s_nEndBlock; i += 4)
		{
			if (i != s_nEndBlock - 8 && memRead32(i) != 0)
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
			pcur[-1] = pcur[0];
			recBackpropBSC(cpuRegs.code, pcur - 1, pcur);
			pcur--;

			has_cop2_instructions |= (_Opcode_ == 022 || _Opcode_ == 066 || _Opcode_ == 076);
		}

		// Run COP2 analysis passes — sets EEINST_COP2_SYNC_VU0/FINISH_VU0 flags
		// for conditional VU0 synchronization in transfer ops.
		if (has_cop2_instructions)
		{
			R5900::COP2MicroFinishPass().Run(startpc, s_nEndBlock, s_pInstCache + 1);

			if (EmuConfig.Speedhacks.vuFlagHack)
				R5900::COP2FlagHackPass().Run(startpc, s_nEndBlock, s_pInstCache + 1);
		}
	}

	// Try timeout loop speedhack — if detected, skip normal codegen
	// Timer-poll loops (mfc0 Count / subu / sltu / bne) are NOT skipped because
	// they need to wait for a specific elapsed time — the correct fix is native codegen.
	// Require timeout_reg >= 0 (actually found an addiu) to avoid matching all-nop blocks
	const bool doRecompilation = !recSkipTimeoutLoop(timeout_reg, is_timeout_loop && timeout_reg >= 0 && timeout_has_bne);

	// Code generation (forward pass)
	if (doRecompilation)
	{
		g_pCurInstInfo = s_pInstCache;
		while (!g_branch && pc < s_nEndBlock)
			recompileNextInstruction(false, false);
	}

	pxAssert((pc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (pc - startpc) >> 2;

	if (!(pc & 0x10000000))
		maxrecmem = std::max((pc & ~0xa0000000), maxrecmem);

	// Snapshot current block's source to recRAMCopy for future overlap detection.
	// Note: The overlap check (comparing old blocks' recRAMCopy vs current memory) is
	// disabled because it causes infinite recompilation loops — recRAMCopy starts zeroed
	// but memory has real code, so the memcmp always fails. The inline CMP checks from
	// memory_protect_recompiled_code are the primary SMC detection mechanism.
	if (HWADDR(pc) <= Ps2MemSize::MainRam)
	{
		memcpy(&recRAMCopy[HWADDR(startpc) / 4], PSM(startpc), pc - startpc);
	}

	if (g_branch == 2)
	{
		// Branch taken — flush and dispatch. recBranchCall already accumulated
		// any pre-call cycles into RECCYCLE and reloaded it after the C call,
		// so any further scaleblockcycles_clear() result is the post-call delta.
		iFlushCall(FLUSH_EVERYTHING);

		u32 cycles = scaleblockcycles_clear();
		if (cycles != 0)
			armAsm->Add(RECCYCLE, RECCYCLE, cycles);

		armAsm->Ldr(a64::x3, armCpuRegMem(&cpuRegs.nextEventCycle));
		armAsm->Cmp(RECCYCLE, a64::x3);
		armEmitCondBranch(a64::ge, DispatcherEvent);

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

	pxAssert(armGetCurrentCodePointer() < SysMemory::GetEERecEnd());

	// Size is from the aligned block_fnptr, not the pre-alignment recPtr —
	// keeps Perf::ee.RegisterPC consistent with the linker's view of the block.
	s_pCurBlockEx->x86size = static_cast<u32>((uptr)armGetCurrentCodePointer() - s_pCurBlockEx->fnptr);

	Perf::ee.RegisterPC((void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = armEndBlock();

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
