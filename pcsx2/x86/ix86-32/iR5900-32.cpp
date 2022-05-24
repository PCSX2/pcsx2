/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include "Common.h"
#include "Memory.h"
#include "R3000A.h"

#include "R5900Exceptions.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"
#include "iR5900Analysis.h"
#include "BaseblockEx.h"
#include "System/RecTypes.h"

#include "vtlb.h"
#include "Dump.h"

#ifndef PCSX2_CORE
#include "gui/SysThreads.h"
#include <pthread.h>
#else
#include "VMManager.h"
#endif
#include "GS.h"
#include "CDVD/CDVD.h"
#include "Elfheader.h"

#include "DebugTools/Breakpoints.h"
#include "Patch.h"

#include "common/AlignedMalloc.h"
#include "common/FastJmp.h"
#include "common/MemsetFast.inl"
#include "common/Perf.h"


using namespace x86Emitter;
using namespace R5900;

static std::atomic<bool> eeRecIsReset(false);
static std::atomic<bool> eeRecNeedsReset(false);
static bool eeCpuExecuting = false;
static bool eeRecExitRequested = false;
static bool g_resetEeScalingStats = false;
#ifndef PCSX2_CORE
static int g_patchesNeedRedo = 0;
#endif

#define PC_GETBLOCK(x) PC_GETBLOCK_(x, recLUT)

u32 maxrecmem = 0;
alignas(16) static uptr recLUT[_64kb];
alignas(16) static u32 hwLUT[_64kb];

static __fi u32 HWADDR(u32 mem) { return hwLUT[mem >> 16] + mem; }

u32 s_nBlockCycles = 0; // cycles of current block recompiling
bool s_nBlockInterlocked = false; // Block is VU0 interlocked
u32 pc;       // recompiler pc
int g_branch; // set for branch

alignas(16) GPR_reg64 g_cpuConstRegs[32] = {0};
u32 g_cpuHasConstReg = 0, g_cpuFlushedConstReg = 0;
bool g_cpuFlushedPC, g_cpuFlushedCode, g_recompilingDelaySlot, g_maySignalException;

eeProfiler EE::Profiler;

////////////////////////////////////////////////////////////////
// Static Private Variables - R5900 Dynarec

#define X86
static const int RECCONSTBUF_SIZE = 16384 * 2; // 64 bit consts in 32 bit units

static RecompiledCodeReserve* recMem = NULL;
static u8* recRAMCopy = NULL;
static u8* recLutReserve_RAM = NULL;
static const size_t recLutSize = (Ps2MemSize::MainRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) * wordsize / 4;

static uptr m_ConfiguredCacheReserve = 64;

alignas(16) static u32 recConstBuf[RECCONSTBUF_SIZE]; // 64-bit pseudo-immediates
static BASEBLOCK* recRAM  = NULL; // and the ptr to the blocks here
static BASEBLOCK* recROM  = NULL; // and here
static BASEBLOCK* recROM1 = NULL; // also here
static BASEBLOCK* recROM2 = NULL; // also here

static BaseBlocks recBlocks;
static u8* recPtr = NULL;
static u32* recConstBufPtr = NULL;
EEINST* s_pInstCache = NULL;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = NULL;
static BASEBLOCKEX* s_pCurBlockEx = NULL;
u32 s_nEndBlock = 0; // what pc the current block ends
u32 s_branchTo;
static bool s_nBlockFF;

// save states for branches
GPR_reg64 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static EEINST* s_psaveInstInfo = NULL;

static u32 s_savenBlockCycles = 0;

#ifdef PCSX2_DEBUG
static u32 dumplog = 0;
#else
#define dumplog 0
#endif

static void iBranchTest(u32 newpc = 0xffffffff);
static void ClearRecLUT(BASEBLOCK* base, int count);
static u32 scaleblockcycles();
static void recExitExecution();

void _eeFlushAllUnused()
{
	u32 i;
	for (i = 0; i < 34; ++i)
	{
		if (pc < s_nEndBlock)
		{
			if ((g_pCurInstInfo[1].regs[i] & EEINST_USED))
				continue;
		}
		else if ((g_pCurInstInfo[0].regs[i] & EEINST_USED))
			continue;

		if (i < 32 && GPR_IS_CONST1(i))
			_flushConstReg(i);
		else
			_deleteGPRtoXMMreg(i, 1);
	}

	//TODO when used info is done for FPU and VU0
	for (i = 0; i < iREGCNT_XMM; ++i)
	{
		if (xmmregs[i].inuse && xmmregs[i].type != XMMTYPE_GPRREG)
			_freeXMMreg(i);
	}
}

u32* _eeGetConstReg(int reg)
{
	pxAssert(GPR_IS_CONST1(reg));

	if (g_cpuFlushedConstReg & (1 << reg))
		return &cpuRegs.GPR.r[reg].UL[0];

	// if written in the future, don't flush
	if (_recIsRegWritten(g_pCurInstInfo + 1, (s_nEndBlock - pc) / 4, XMMTYPE_GPRREG, reg))
		return recGetImm64(g_cpuConstRegs[reg].UL[1], g_cpuConstRegs[reg].UL[0]);

	_flushConstReg(reg);
	return &cpuRegs.GPR.r[reg].UL[0];
}

void _eeMoveGPRtoR(const xRegister32& to, int fromgpr)
{
	if (fromgpr == 0)
		xXOR(to, to); // zero register should use xor, thanks --air
	else if (GPR_IS_CONST1(fromgpr))
		xMOV(to, g_cpuConstRegs[fromgpr].UL[0]);
	else
	{
		int mmreg;

		if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0 && (xmmregs[mmreg].mode & MODE_WRITE))
		{
			xMOVD(to, xRegisterSSE(mmreg));
		}
		else
		{
			xMOV(to, ptr[&cpuRegs.GPR.r[fromgpr].UL[0]]);
		}
	}
}

void _eeMoveGPRtoM(uptr to, int fromgpr)
{
	if (GPR_IS_CONST1(fromgpr))
		xMOV(ptr32[(u32*)(to)], g_cpuConstRegs[fromgpr].UL[0]);
	else
	{
		int mmreg;

		if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0)
		{
			xMOVSS(ptr[(void*)(to)], xRegisterSSE(mmreg));
		}
		else
		{
			xMOV(eax, ptr[&cpuRegs.GPR.r[fromgpr].UL[0]]);
			xMOV(ptr[(void*)(to)], eax);
		}
	}
}

void _eeMoveGPRtoRm(x86IntRegType to, int fromgpr)
{
	if (GPR_IS_CONST1(fromgpr))
		xMOV(ptr32[xAddressReg(to)], g_cpuConstRegs[fromgpr].UL[0]);
	else
	{
		int mmreg;

		if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ)) >= 0)
		{
			xMOVSS(ptr[xAddressReg(to)], xRegisterSSE(mmreg));
		}
		else
		{
			xMOV(eax, ptr[&cpuRegs.GPR.r[fromgpr].UL[0]]);
			xMOV(ptr[xAddressReg(to)], eax);
		}
	}
}

void _signExtendToMem(void* mem)
{
	xCDQE();
	xMOV(ptr64[mem], rax);
}

void eeSignExtendTo(int gpr, bool onlyupper)
{
	if (onlyupper)
	{
		xCDQ();
		xMOV(ptr32[&cpuRegs.GPR.r[gpr].UL[1]], edx);
	}
	else
	{
		_signExtendToMem(&cpuRegs.GPR.r[gpr].UD[0]);
	}
}

int _flushXMMunused()
{
	u32 i;
	for (i = 0; i < iREGCNT_XMM; i++)
	{
		if (!xmmregs[i].inuse || xmmregs[i].needed || !(xmmregs[i].mode & MODE_WRITE))
			continue;

		if (xmmregs[i].type == XMMTYPE_GPRREG)
		{
			//if( !(g_pCurInstInfo->regs[xmmregs[i].reg]&EEINST_USED) ) {
			if (!_recIsRegWritten(g_pCurInstInfo + 1, (s_nEndBlock - pc) / 4, XMMTYPE_GPRREG, xmmregs[i].reg))
			{
				_freeXMMreg(i);
				xmmregs[i].inuse = 1;
				return 1;
			}
		}
	}

	return 0;
}

int _flushUnusedConstReg()
{
	int i;
	for (i = 1; i < 32; ++i)
	{
		if ((g_cpuHasConstReg & (1 << i)) && !(g_cpuFlushedConstReg & (1 << i)) &&
			!_recIsRegWritten(g_pCurInstInfo + 1, (s_nEndBlock - pc) / 4, XMMTYPE_GPRREG, i))
		{

			// check if will be written in the future
			xMOV(ptr32[&cpuRegs.GPR.r[i].UL[0]], g_cpuConstRegs[i].UL[0]);
			xMOV(ptr32[&cpuRegs.GPR.r[i].UL[1]], g_cpuConstRegs[i].UL[1]);
			g_cpuFlushedConstReg |= 1 << i;
			return 1;
		}
	}

	return 0;
}

// Some of the generated MMX code needs 64-bit immediates but x86 doesn't
// provide this.  One of the reasons we are probably better off not doing
// MMX register allocation for the EE.
u32* recGetImm64(u32 hi, u32 lo)
{
	u32* imm64; // returned pointer
	static u32* imm64_cache[509];
	int cacheidx = lo % (sizeof imm64_cache / sizeof *imm64_cache);

	imm64 = imm64_cache[cacheidx];
	if (imm64 && imm64[0] == lo && imm64[1] == hi)
		return imm64;

	if (recConstBufPtr >= recConstBuf + RECCONSTBUF_SIZE)
	{
		Console.WriteLn("EErec const buffer filled; Resetting...");
		throw Exception::ExitCpuExecute();

		/*for (u32 *p = recConstBuf; p < recConstBuf + RECCONSTBUF_SIZE; p += 2)
		{
			if (p[0] == lo && p[1] == hi) {
				imm64_cache[cacheidx] = p;
				return p;
			}
		}

		return recConstBuf;*/
	}

	imm64 = recConstBufPtr;
	recConstBufPtr += 2;
	imm64_cache[cacheidx] = imm64;

	imm64[0] = lo;
	imm64[1] = hi;

	//Console.Warning("Consts allocated: %d of %u", (recConstBufPtr - recConstBuf) / 2, count);

	return imm64;
}

// Use this to call into interpreter functions that require an immediate branchtest
// to be done afterward (anything that throws an exception or enables interrupts, etc).
void recBranchCall(void (*func)())
{
	// In order to make sure a branch test is performed, the nextBranchCycle is set
	// to the current cpu cycle.

	xMOV(eax, ptr[&cpuRegs.cycle]);
	xMOV(ptr[&g_nextEventCycle], eax);

	recCall(func);
	g_branch = 2;
}

void recCall(void (*func)())
{
	iFlushCall(FLUSH_INTERPRETER);
	xFastCall((void*)func);
}

// =====================================================================================================
//  R5900 Dispatchers
// =====================================================================================================

static void recRecompile(const u32 startpc);
static void dyna_block_discard(u32 start, u32 sz);
static void dyna_page_reset(u32 start, u32 sz);

// Recompiled code buffer for EE recompiler dispatchers!
alignas(__pagesize) static u8 eeRecDispatchers[__pagesize];

typedef void DynGenFunc();

static DynGenFunc* DispatcherEvent      = NULL;
static DynGenFunc* DispatcherReg        = NULL;
static DynGenFunc* JITCompile           = NULL;
static DynGenFunc* JITCompileInBlock    = NULL;
static DynGenFunc* EnterRecompiledCode  = NULL;
static DynGenFunc* ExitRecompiledCode   = NULL;
static DynGenFunc* DispatchBlockDiscard = NULL;
static DynGenFunc* DispatchPageReset    = NULL;

static void recEventTest()
{
	_cpuEventTest_Shared();

	if (eeRecExitRequested)
	{
		eeRecExitRequested = false;
		recExitExecution();
	}
}

// The address for all cleared blocks.  It recompiles the current pc and then
// dispatches to the recompiled block address.
static DynGenFunc* _DynGen_JITCompile()
{
	pxAssertMsg(DispatcherReg != NULL, "Please compile the DispatcherReg subroutine *before* JITComple.  Thanks.");

	u8* retval = xGetAlignedCallTarget();

	xFastCall((void*)recRecompile, ptr32[&cpuRegs.pc]);

	// C equivalent:
	// u32 addr = cpuRegs.pc;
	// void(**base)() = (void(**)())recLUT[addr >> 16];
	// base[addr >> 2]();
	xMOV(eax, ptr[&cpuRegs.pc]);
	xMOV(ebx, eax);
	xSHR(eax, 16);
	xMOV(rcx, ptrNative[xComplexAddress(rcx, recLUT, rax * wordsize)]);
	xJMP(ptrNative[rbx * (wordsize / 4) + rcx]);

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_JITCompileInBlock()
{
	u8* retval = xGetAlignedCallTarget();
	xJMP((void*)JITCompile);
	return (DynGenFunc*)retval;
}

// called when jumping to variable pc address
static DynGenFunc* _DynGen_DispatcherReg()
{
	u8* retval = xGetPtr(); // fallthrough target, can't align it!

	// C equivalent:
	// u32 addr = cpuRegs.pc;
	// void(**base)() = (void(**)())recLUT[addr >> 16];
	// base[addr >> 2]();
	xMOV(eax, ptr[&cpuRegs.pc]);
	xMOV(ebx, eax);
	xSHR(eax, 16);
	xMOV(rcx, ptrNative[xComplexAddress(rcx, recLUT, rax * wordsize)]);
	xJMP(ptrNative[rbx * (wordsize / 4) + rcx]);

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_DispatcherEvent()
{
	u8* retval = xGetPtr();

	xFastCall((void*)recEventTest);

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_EnterRecompiledCode()
{
	pxAssertDev(DispatcherReg != NULL, "Dynamically generated dispatchers are required prior to generating EnterRecompiledCode!");

	u8* retval = xGetAlignedCallTarget();

	{ // Properly scope the frame prologue/epilogue
#ifdef ENABLE_VTUNE
		xScopedStackFrame frame(true);
#else
		xScopedStackFrame frame(IsDevBuild);
#endif

		xJMP((void*)DispatcherReg);

		// Save an exit point
		ExitRecompiledCode = (DynGenFunc*)xGetPtr();
	}

	xRET();

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_DispatchBlockDiscard()
{
	u8* retval = xGetPtr();
	xFastCall((void*)dyna_block_discard);
	xJMP((void*)ExitRecompiledCode);
	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_DispatchPageReset()
{
	u8* retval = xGetPtr();
	xFastCall((void*)dyna_page_reset);
	xJMP((void*)ExitRecompiledCode);
	return (DynGenFunc*)retval;
}

static void _DynGen_Dispatchers()
{
	// In case init gets called multiple times:
	HostSys::MemProtectStatic(eeRecDispatchers, PageAccess_ReadWrite());

	// clear the buffer to 0xcc (easier debugging).
	memset(eeRecDispatchers, 0xcc, __pagesize);

	xSetPtr(eeRecDispatchers);

	// Place the EventTest and DispatcherReg stuff at the top, because they get called the
	// most and stand to benefit from strong alignment and direct referencing.
	DispatcherEvent = _DynGen_DispatcherEvent();
	DispatcherReg   = _DynGen_DispatcherReg();

	JITCompile           = _DynGen_JITCompile();
	JITCompileInBlock    = _DynGen_JITCompileInBlock();
	EnterRecompiledCode  = _DynGen_EnterRecompiledCode();
	DispatchBlockDiscard = _DynGen_DispatchBlockDiscard();
	DispatchPageReset    = _DynGen_DispatchPageReset();

	HostSys::MemProtectStatic(eeRecDispatchers, PageAccess_ExecOnly());

	recBlocks.SetJITCompile(JITCompile);

	Perf::any.map((uptr)&eeRecDispatchers, 4096, "EE Dispatcher");
}


//////////////////////////////////////////////////////////////////////////////////////////
//

static __ri void ClearRecLUT(BASEBLOCK* base, int memsize)
{
	for (int i = 0; i < memsize / (int)sizeof(uptr); i++)
		base[i].SetFnptr((uptr)JITCompile);
}


static void recThrowHardwareDeficiency(const char* extFail)
{
	throw Exception::HardwareDeficiency()
		.SetDiagMsg(fmt::format("R5900-32 recompiler init failed: {} is not available.", extFail))
		.SetUserMsg(fmt::format("{} Extensions not found.  The R5900-32 recompiler requires a host CPU with SSE2 extensions.", extFail));
}

static void recReserveCache()
{
	if (!recMem)
		recMem = new RecompiledCodeReserve("R5900-32 Recompiler Cache", _16mb);
	recMem->SetProfilerName("EErec");

	while (!recMem->IsOk())
	{
		if (recMem->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::EErecOffset, m_ConfiguredCacheReserve * _1mb) != NULL)
			break;

		// If it failed, then try again (if possible):
		if (m_ConfiguredCacheReserve < 16)
			break;
		m_ConfiguredCacheReserve /= 2;
	}

	recMem->ThrowIfNotOk();
}

static void recReserve()
{
	// Hardware Requirements Check...

	if (!x86caps.hasStreamingSIMD4Extensions)
		recThrowHardwareDeficiency("SSE4");

	recReserveCache();
}

static void recAlloc()
{
	if (!recRAMCopy)
	{
		recRAMCopy = (u8*)_aligned_malloc(Ps2MemSize::MainRam, 4096);
	}

	if (!recRAM)
	{
		recLutReserve_RAM = (u8*)_aligned_malloc(recLutSize, 4096);
	}

	BASEBLOCK* basepos = (BASEBLOCK*)recLutReserve_RAM;
	recRAM  = basepos; basepos += (Ps2MemSize::MainRam / 4);
	recROM  = basepos; basepos += (Ps2MemSize::Rom / 4);
	recROM1 = basepos; basepos += (Ps2MemSize::Rom1 / 4);
	recROM2 = basepos; basepos += (Ps2MemSize::Rom2 / 4);

	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(recLUT, 0, 0, 0, i, 0);

	for (int i = 0x0000; i < (int)(Ps2MemSize::MainRam / 0x10000); i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x0000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x2000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x3000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0x8000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xa000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xb000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xc000, i, i);
		recLUT_SetPage(recLUT, hwLUT, recRAM, 0xd000, i, i);
	}

	for (int i = 0x1fc0; i < 0x2000; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(recLUT, hwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	for (int i = 0x1e00; i < 0x1e04; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recLUT, hwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e48; i++)
	{
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(recLUT, hwLUT, recROM2, 0xa000, i, i - 0x1e40);
	}

	if (s_pInstCache == NULL)
	{
		s_nInstCacheSize = 128;
		s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
	}

	if (s_pInstCache == NULL)
		throw Exception::OutOfMemory("R5900-32 InstCache");

	// No errors.. Proceed with initialization:

	_DynGen_Dispatchers();
}

alignas(16) static u16 manual_page[Ps2MemSize::MainRam >> 12];
alignas(16) static u8 manual_counter[Ps2MemSize::MainRam >> 12];

////////////////////////////////////////////////////
static void recResetRaw()
{
	Perf::ee.reset();

	EE::Profiler.Reset();

	recAlloc();

	eeRecNeedsReset = false;
	if (eeRecIsReset.exchange(true))
		return;

	Console.WriteLn(Color_StrongBlack, "EE/iR5900-32 Recompiler Reset");

	recMem->Reset();
	ClearRecLUT((BASEBLOCK*)recLutReserve_RAM, recLutSize);
	memset(recRAMCopy, 0, Ps2MemSize::MainRam);

	maxrecmem = 0;

	memset(recConstBuf, 0, RECCONSTBUF_SIZE * sizeof(*recConstBuf));

	if (s_pInstCache)
		memset(s_pInstCache, 0, sizeof(EEINST) * s_nInstCacheSize);

	recBlocks.Reset();
	mmap_ResetBlockTracking();

	x86SetPtr(*recMem);

	recPtr = *recMem;
	recConstBufPtr = recConstBuf;

	g_branch = 0;
	g_resetEeScalingStats = true;
#ifndef PCSX2_CORE
	g_patchesNeedRedo = 1;
#endif
}

static void recShutdown()
{
	safe_delete(recMem);
	safe_aligned_free(recRAMCopy);
	safe_aligned_free(recLutReserve_RAM);

	recBlocks.Reset();

	recRAM = recROM = recROM1 = recROM2 = NULL;

	safe_free(s_pInstCache);
	s_nInstCacheSize = 0;

	// FIXME Warning thread unsafe
	Perf::dump();
}

static void recResetEE()
{
	if (eeCpuExecuting)
	{
		// get outta here as soon as we can
		eeRecNeedsReset = true;
		eeRecExitRequested = true;
		return;
	}

	recResetRaw();
}

void recStep()
{
}

static fastjmp_buf m_SetJmp_StateCheck;
static std::unique_ptr<BaseR5900Exception> m_cpuException;
static ScopedExcept m_Exception;

static void recExitExecution()
{
	// Without SEH we'll need to hop to a safehouse point outside the scope of recompiled
	// code.  C++ exceptions can't cross the mighty chasm in the stackframe that the recompiler
	// creates.  However, the longjump is slow so we only want to do one when absolutely
	// necessary:

	fastjmp_jmp(&m_SetJmp_StateCheck, 1);
}

static void recSafeExitExecution()
{
	// If we're currently processing events, we can't safely jump out of the recompiler here, because we'll
	// leave things in an inconsistent state. So instead, we flag it for exiting once cpuEventTest() returns.
	if (eeEventTestIsActive)
		eeRecExitRequested = true;
	else
		recExitExecution();
}

static void recExecute()
{
	// Reset before we try to execute any code, if there's one pending.
	// We need to do this here, because if we reset while we're executing, it sets the "needs reset"
	// flag, which triggers a JIT exit (the fastjmp_set below), and eventually loops back here.
	eeRecIsReset.store(false);
	if (eeRecNeedsReset.load())
		recResetRaw();

	m_cpuException = nullptr;
	m_Exception    = nullptr;

	// setjmp will save the register context and will return 0
	// A call to longjmp will restore the context (included the eip/rip)
	// but will return the longjmp 2nd parameter (here 1)
#ifndef PCSX2_CORE
	int oldstate;
#endif
	if (!fastjmp_set(&m_SetJmp_StateCheck))
	{
		eeCpuExecuting = true;

		// Important! Most of the console logging and such has cancel points in it.  This is great
		// in Windows, where SEH lets us safely kill a thread from anywhere we want.  This is bad
		// in Linux, which cannot have a C++ exception cross the recompiler.  Hence the changing
		// of the cancelstate here!

#ifndef PCSX2_CORE
		pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &oldstate);
#endif
		EnterRecompiledCode();

		// Generally unreachable code here ...
	}
	else
	{
#ifndef PCSX2_CORE
		pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
#endif
	}

	eeCpuExecuting = false;

	if (m_cpuException)
		m_cpuException->Rethrow();
	if (m_Exception)
		m_Exception->Rethrow();

	// FIXME Warning thread unsafe
	Perf::dump();

	EE::Profiler.Print();
}

////////////////////////////////////////////////////
void R5900::Dynarec::OpcodeImpl::recSYSCALL()
{
	EE::Profiler.EmitOp(eeOpcode::SYSCALL);

	recCall(R5900::Interpreter::OpcodeImpl::SYSCALL);

	xCMP(ptr32[&cpuRegs.pc], pc);
	j8Ptr[0] = JE8(0);
	xADD(ptr32[&cpuRegs.cycle], scaleblockcycles());
	// Note: technically the address is 0x8000_0180 (or 0x180)
	// (if CPU is booted)
	xJMP((void*)DispatcherReg);
	x86SetJ8(j8Ptr[0]);
	//g_branch = 2;
}

////////////////////////////////////////////////////
void R5900::Dynarec::OpcodeImpl::recBREAK()
{
	EE::Profiler.EmitOp(eeOpcode::BREAK);

	recCall(R5900::Interpreter::OpcodeImpl::BREAK);

	xCMP(ptr32[&cpuRegs.pc], pc);
	j8Ptr[0] = JE8(0);
	xADD(ptr32[&cpuRegs.cycle], scaleblockcycles());
	xJMP((void*)DispatcherEvent);
	x86SetJ8(j8Ptr[0]);
	//g_branch = 2;
}

// Size is in dwords (4 bytes)
void recClear(u32 addr, u32 size)
{
	if ((addr) >= maxrecmem || !(recLUT[(addr) >> 16] + (addr & ~0xFFFFUL)))
		return;
	addr = HWADDR(addr);

	int blockidx = recBlocks.LastIndex(addr + size * 4 - 4);

	if (blockidx == -1)
		return;

	u32 lowerextent = (u32)-1, upperextent = 0, ceiling = (u32)-1;

	BASEBLOCKEX* pexblock = recBlocks[blockidx + 1];
	if (pexblock)
		ceiling = pexblock->startpc;

	int toRemoveLast = blockidx;

	while (pexblock = recBlocks[blockidx])
	{
		u32 blockstart = pexblock->startpc;
		u32 blockend = pexblock->startpc + pexblock->size * 4;
		BASEBLOCK* pblock = PC_GETBLOCK(blockstart);

		if (pblock == s_pCurBlock)
		{
			if (toRemoveLast != blockidx)
			{
				recBlocks.Remove((blockidx + 1), toRemoveLast);
			}
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
		// This might end up inside a block that doesn't contain the clearing range,
		// so set it to recompile now.  This will become JITCompile if we clear it.
		pblock->SetFnptr((uptr)JITCompileInBlock);

		blockidx--;
	}

	if (toRemoveLast != blockidx)
	{
		recBlocks.Remove((blockidx + 1), toRemoveLast);
	}

	upperextent = std::min(upperextent, ceiling);

	for (int i = 0; pexblock = recBlocks[i]; i++)
	{
		if (s_pCurBlock == PC_GETBLOCK(pexblock->startpc))
			continue;
		u32 blockend = pexblock->startpc + pexblock->size * 4;
		if (pexblock->startpc >= addr && pexblock->startpc < addr + size * 4
		 || pexblock->startpc < addr && blockend > addr)
		{
			if (!IsDevBuild)
				Console.Error("[EE] Impossible block clearing failure");
			else
				pxFailDev("[EE] Impossible block clearing failure");
		}
	}

	if (upperextent > lowerextent)
		ClearRecLUT(PC_GETBLOCK(lowerextent), upperextent - lowerextent);
}


static int* s_pCode;

void SetBranchReg(u32 reg)
{
	g_branch = 1;

	if (reg != 0xffffffff)
	{
//		if (GPR_IS_CONST1(reg))
//			xMOV(ptr32[&cpuRegs.pc], g_cpuConstRegs[reg].UL[0]);
//		else
//		{
//			int mmreg;
//
//			if ((mmreg = _checkXMMreg(XMMTYPE_GPRREG, reg, MODE_READ)) >= 0)
//			{
//				xMOVSS(ptr[&cpuRegs.pc], xRegisterSSE(mmreg));
//			}
//			else
//			{
//				xMOV(eax, ptr[(void*)((int)&cpuRegs.GPR.r[reg].UL[0])]);
//				xMOV(ptr[&cpuRegs.pc], eax);
//			}
//		}
		_allocX86reg(calleeSavedReg2d, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
		_eeMoveGPRtoR(calleeSavedReg2d, reg);

		if (EmuConfig.Gamefixes.GoemonTlbHack)
		{
			xMOV(ecx, calleeSavedReg2d);
			vtlb_DynV2P();
			xMOV(calleeSavedReg2d, eax);
		}

		recompileNextInstruction(1);

		if (x86regs[calleeSavedReg2d.GetId()].inuse)
		{
			pxAssert(x86regs[calleeSavedReg2d.GetId()].type == X86TYPE_PCWRITEBACK);
			xMOV(ptr[&cpuRegs.pc], calleeSavedReg2d);
			x86regs[calleeSavedReg2d.GetId()].inuse = 0;
		}
		else
		{
			xMOV(eax, ptr[&g_recWriteback]);
			xMOV(ptr[&cpuRegs.pc], eax);
		}
	}

//	xCMP(ptr32[&cpuRegs.pc], 0);
//	j8Ptr[5] = JNE8(0);
//	xFastCall((void*)(uptr)tempfn);
//	x86SetJ8(j8Ptr[5]);

	iFlushCall(FLUSH_EVERYTHING);

	iBranchTest();
}

void SetBranchImm(u32 imm)
{
	g_branch = 1;

	pxAssert(imm);

	// end the current block
	iFlushCall(FLUSH_EVERYTHING);
	xMOV(ptr32[&cpuRegs.pc], imm);
	iBranchTest(imm);
}

void SaveBranchState()
{
	s_savenBlockCycles = s_nBlockCycles;
	memcpy(s_saveConstRegs, g_cpuConstRegs, sizeof(g_cpuConstRegs));
	s_saveHasConstReg = g_cpuHasConstReg;
	s_saveFlushedConstReg = g_cpuFlushedConstReg;
	s_psaveInstInfo = g_pCurInstInfo;

	memcpy(s_saveXMMregs, xmmregs, sizeof(xmmregs));
}

void LoadBranchState()
{
	s_nBlockCycles = s_savenBlockCycles;

	memcpy(g_cpuConstRegs, s_saveConstRegs, sizeof(g_cpuConstRegs));
	g_cpuHasConstReg = s_saveHasConstReg;
	g_cpuFlushedConstReg = s_saveFlushedConstReg;
	g_pCurInstInfo = s_psaveInstInfo;

	memcpy(xmmregs, s_saveXMMregs, sizeof(xmmregs));
}

void iFlushCall(int flushtype)
{
	// Free registers that are not saved across function calls (x86-32 ABI):
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(edx);

	if ((flushtype & FLUSH_PC) && !g_cpuFlushedPC)
	{
		xMOV(ptr32[&cpuRegs.pc], pc);
		g_cpuFlushedPC = true;
	}

	if ((flushtype & FLUSH_CODE) && !g_cpuFlushedCode)
	{
		xMOV(ptr32[&cpuRegs.code], cpuRegs.code);
		g_cpuFlushedCode = true;
	}

	if ((flushtype == FLUSH_CAUSE) && !g_maySignalException)
	{
		if (g_recompilingDelaySlot)
			xOR(ptr32[&cpuRegs.CP0.n.Cause], 1 << 31); // BD
		g_maySignalException = true;
	}

	if (flushtype & FLUSH_FREE_XMM)
		_freeXMMregs();
	else if (flushtype & FLUSH_FLUSH_XMM)
		_flushXMMregs();

	if (flushtype & FLUSH_CACHED_REGS)
		_flushConstRegs();
}

// Note: scaleblockcycles() scales s_nBlockCycles respective to the EECycleRate value for manipulating the cycles of current block recompiling.
// s_nBlockCycles is 3 bit fixed point.  Divide by 8 when done!
// Scaling blocks under 40 cycles seems to produce countless problem, so let's try to avoid them.

#define DEFAULT_SCALED_BLOCKS() (s_nBlockCycles >> 3)

static u32 scaleblockcycles_calculation()
{
	bool lowcycles = (s_nBlockCycles <= 40);
	s8 cyclerate = EmuConfig.Speedhacks.EECycleRate;
	u32 scale_cycles = 0;

	if (cyclerate == 0 || lowcycles || cyclerate < -99 || cyclerate > 3)
		scale_cycles = DEFAULT_SCALED_BLOCKS();

	else if (cyclerate > 1)
		scale_cycles = s_nBlockCycles >> (2 + cyclerate);

	else if (cyclerate == 1)
		scale_cycles = DEFAULT_SCALED_BLOCKS() / 1.3f; // Adds a mild 30% increase in clockspeed for value 1.

	else if (cyclerate == -1) // the mildest value which is also used by the "balanced" preset.
		// These values were manually tuned to yield mild speedup with high compatibility
		scale_cycles = (s_nBlockCycles <= 80 || s_nBlockCycles > 168 ? 5 : 7) * s_nBlockCycles / 32;

	else
		scale_cycles = ((5 + (-2 * (cyclerate + 1))) * s_nBlockCycles) >> 5;

	// Ensure block cycle count is never less than 1.
	return (scale_cycles < 1) ? 1 : scale_cycles;
}

static u32 scaleblockcycles()
{
	u32 scaled = scaleblockcycles_calculation();

#if 0 // Enable this to get some runtime statistics about the scaling result in practice
	static u32 scaled_overall = 0, unscaled_overall = 0;
	if (g_resetEeScalingStats)
	{
		scaled_overall = unscaled_overall = 0;
		g_resetEeScalingStats = false;
	}
	u32 unscaled = DEFAULT_SCALED_BLOCKS();
	if (!unscaled) unscaled = 1;

	scaled_overall += scaled;
	unscaled_overall += unscaled;
	float ratio = static_cast<float>(unscaled_overall) / scaled_overall;

	DevCon.WriteLn(L"Unscaled overall: %d,  scaled overall: %d,  relative EE clock speed: %d %%",
	               unscaled_overall, scaled_overall, static_cast<int>(100 * ratio));
#endif

	return scaled;
}
u32 scaleblockcycles_clear()
{
	u32 scaled = scaleblockcycles_calculation();

#if 0 // Enable this to get some runtime statistics about the scaling result in practice
	static u32 scaled_overall = 0, unscaled_overall = 0;
	if (g_resetEeScalingStats)
	{
		scaled_overall = unscaled_overall = 0;
		g_resetEeScalingStats = false;
	}
	u32 unscaled = DEFAULT_SCALED_BLOCKS();
	if (!unscaled) unscaled = 1;

	scaled_overall += scaled;
	unscaled_overall += unscaled;
	float ratio = static_cast<float>(unscaled_overall) / scaled_overall;

	DevCon.WriteLn(L"Unscaled overall: %d,  scaled overall: %d,  relative EE clock speed: %d %%",
		unscaled_overall, scaled_overall, static_cast<int>(100 * ratio));
#endif
	s8 cyclerate = EmuConfig.Speedhacks.EECycleRate;

	if (cyclerate > 1)
	{
		s_nBlockCycles &= (0x1 << (cyclerate + 2)) - 1;
	}
	else
	{
		s_nBlockCycles &= 0x7;
	}

	return scaled;
}

// Generates dynarec code for Event tests followed by a block dispatch (branch).
// Parameters:
//   newpc - address to jump to at the end of the block.  If newpc == 0xffffffff then
//   the jump is assumed to be to a register (dynamic).  For any other value the
//   jump is assumed to be static, in which case the block will be "hardlinked" after
//   the first time it's dispatched.
//
//   noDispatch - When set true, then jump to Dispatcher.  Used by the recs
//   for blocks which perform exception checks without branching (it's enabled by
//   setting "g_branch = 2";
static void iBranchTest(u32 newpc)
{
	// Check the Event scheduler if our "cycle target" has been reached.
	// Equiv code to:
	//    cpuRegs.cycle += blockcycles;
	//    if( cpuRegs.cycle > g_nextEventCycle ) { DoEvents(); }

	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && newpc == s_branchTo)
	{
		xMOV(eax, ptr32[&g_nextEventCycle]);
		xADD(ptr32[&cpuRegs.cycle], scaleblockcycles());
		xCMP(eax, ptr32[&cpuRegs.cycle]);
		xCMOVS(eax, ptr32[&cpuRegs.cycle]);
		xMOV(ptr32[&cpuRegs.cycle], eax);

		xJMP((void*)DispatcherEvent);
	}
	else
	{
		xMOV(eax, ptr[&cpuRegs.cycle]);
		xADD(eax, scaleblockcycles());
		xMOV(ptr[&cpuRegs.cycle], eax); // update cycles
		xSUB(eax, ptr[&g_nextEventCycle]);

		if (newpc == 0xffffffff)
			xJS(DispatcherReg);
		else
			recBlocks.Link(HWADDR(newpc), xJcc32(Jcc_Signed));

		xJMP((void*)DispatcherEvent);
	}
}

// opcode 'code' modifies:
// 1: status
// 2: MAC
// 4: clip
int cop2flags(u32 code)
{
	if (code >> 26 != 022)
		return 0; // not COP2
	if ((code >> 25 & 1) == 0)
		return 0; // a branch or transfer instruction

	switch (code >> 2 & 15)
	{
		case 15:
			switch (code >> 6 & 0x1f)
			{
				case 4: // ITOF*
				case 5: // FTOI*
				case 12: // MOVE MR32
				case 13: // LQI SQI LQD SQD
				case 15: // MTIR MFIR ILWR ISWR
				case 16: // RNEXT RGET RINIT RXOR
					return 0;
				case 7: // MULAq, ABS, MULAi, CLIP
					if ((code & 3) == 1) // ABS
						return 0;
					if ((code & 3) == 3) // CLIP
						return 4;
					return 3;
				case 11: // SUBA, MSUBA, OPMULA, NOP
					if ((code & 3) == 3) // NOP
						return 0;
					return 3;
				case 14: // DIV, SQRT, RSQRT, WAITQ
					if ((code & 3) == 3) // WAITQ
						return 0;
					return 1; // but different timing, ugh
				default:
					break;
			}
			break;
		case 4: // MAXbc
		case 5: // MINbc
		case 12: // IADD, ISUB, IADDI
		case 13: // IAND, IOR
		case 14: // VCALLMS, VCALLMSR
			return 0;
		case 7:
			if ((code & 1) == 1) // MAXi, MINIi
				return 0;
			return 3;
		case 10:
			if ((code & 3) == 3) // MAX
				return 0;
			return 3;
		case 11:
			if ((code & 3) == 3) // MINI
				return 0;
			return 3;
		default:
			break;
	}
	return 3;
}

int COP2DivUnitTimings(u32 code)
{
	// Note: Cycles are off by 1 since the check ignores the actual op, so they are off by 1
	switch (code & 0x3FF)
	{
		case 0x3BC: // DIV
		case 0x3BD: // SQRT
			return 6;
		case 0x3BE: // RSQRT
			return 12;
		default:
			return 0; // Used mainly for WAITQ
	}
}

bool COP2IsQOP(u32 code)
{
	if (_Opcode_ != 022) // Not COP2 operation
		return false;

	if ((code & 0x3f) == 0x20) // VADDq
		return true;
	if ((code & 0x3f) == 0x21) // VMADDq
		return true;
	if ((code & 0x3f) == 0x24) // VSUBq
		return true;
	if ((code & 0x3f) == 0x25) // VMSUBq
		return true;
	if ((code & 0x3f) == 0x1C) // VMULq
		return true;
	if ((code & 0x7FF) == 0x1FC) // VMULAq
		return true;
	if ((code & 0x7FF) == 0x23C) // VADDAq
		return true;
	if ((code & 0x7FF) == 0x23D) // VMADDAq
		return true;
	if ((code & 0x7FF) == 0x27C) // VSUBAq
		return true;
	if ((code & 0x7FF) == 0x27D) // VMSUBAq
		return true;

	return false;
}


void dynarecCheckBreakpoint()
{
	u32 pc = cpuRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_EE, pc) != 0)
		return;

	int bpFlags = isBreakpointNeeded(pc);
	bool hit = false;
	//check breakpoint at current pc
	if (bpFlags & 1)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_EE, pc);
		if (cond == NULL || cond->Evaluate())
		{
			hit = true;
		}
	}
	//check breakpoint in delay slot
	if (bpFlags & 2)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_EE, pc + 4);
		if (cond == NULL || cond->Evaluate())
			hit = true;
	}

	if (!hit)
		return;

	CBreakPoints::SetBreakpointTriggered(true);
#ifndef PCSX2_CORE
	GetCoreThread().PauseSelfDebug();
#endif
	recExitExecution();
}

void dynarecMemcheck()
{
	u32 pc = cpuRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_EE, pc) != 0)
		return;

	CBreakPoints::SetBreakpointTriggered(true);
#ifndef PCSX2_CORE
	GetCoreThread().PauseSelfDebug();
#endif
	recExitExecution();
}

void dynarecMemLogcheck(u32 start, bool store)
{
	if (store)
		DevCon.WriteLn("Hit store breakpoint @0x%x", start);
	else
		DevCon.WriteLn("Hit load breakpoint @0x%x", start);
}

void recMemcheck(u32 op, u32 bits, bool store)
{
	iFlushCall(FLUSH_EVERYTHING | FLUSH_PC);

	// compute accessed address
	_eeMoveGPRtoR(ecx, (op >> 21) & 0x1F);
	if ((s16)op != 0)
		xADD(ecx, (s16)op);
	if (bits == 128)
		xAND(ecx, ~0x0F);

	xFastCall((void*)standardizeBreakpointAddress, ecx);
	xMOV(ecx, eax);
	xMOV(edx, eax);
	xADD(edx, bits / 8);

	// ecx = access address
	// edx = access address+size

	auto checks = CBreakPoints::GetMemChecks();
	for (size_t i = 0; i < checks.size(); i++)
	{
		if (checks[i].cpu != BREAKPOINT_EE)
			continue;
		if (checks[i].result == 0)
			continue;
		if ((checks[i].cond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((checks[i].cond & MEMCHECK_READ) == 0 && !store)
			continue;

		// logic: memAddress < bpEnd && bpStart < memAddress+memSize

		xMOV(eax, standardizeBreakpointAddress(checks[i].end));
		xCMP(ecx, eax); // address < end
		xForwardJGE8 next1; // if address >= end then goto next1

		xMOV(eax, standardizeBreakpointAddress(checks[i].start));
		xCMP(eax, edx); // start < address+size
		xForwardJGE8 next2; // if start >= address+size then goto next2

		// hit the breakpoint
		if (checks[i].result & MEMCHECK_LOG)
		{
			xMOV(edx, store);
			xFastCall((void*)dynarecMemLogcheck, ecx, edx);
		}
		if (checks[i].result & MEMCHECK_BREAK)
		{
			xFastCall((void*)dynarecMemcheck);
		}

		next1.SetTarget();
		next2.SetTarget();
	}
}

void encodeBreakpoint()
{
	if (isBreakpointNeeded(pc) != 0)
	{
		iFlushCall(FLUSH_EVERYTHING | FLUSH_PC);
		xFastCall((void*)dynarecCheckBreakpoint);
	}
}

void encodeMemcheck()
{
	int needed = isMemcheckNeeded(pc);
	if (needed == 0)
		return;

	u32 op = memRead32(needed == 2 ? pc + 4 : pc);
	const OPCODE& opcode = GetInstruction(op);

	bool store = (opcode.flags & IS_STORE) != 0;
	switch (opcode.flags & MEMTYPE_MASK)
	{
		case MEMTYPE_BYTE:
			recMemcheck(op, 8, store);
			break;
		case MEMTYPE_HALF:
			recMemcheck(op, 16, store);
			break;
		case MEMTYPE_WORD:
			recMemcheck(op, 32, store);
			break;
		case MEMTYPE_DWORD:
			recMemcheck(op, 64, store);
			break;
		case MEMTYPE_QWORD:
			recMemcheck(op, 128, store);
			break;
	}
}

void recompileNextInstruction(int delayslot)
{
	u32 i;
	int count;

	// add breakpoint
	if (!delayslot)
	{
		encodeBreakpoint();
		encodeMemcheck();
	}

	s_pCode = (int*)PSM(pc);
	pxAssert(s_pCode);

	// acts as a tag for delimiting recompiled instructions when viewing x86 disasm.
	if (IsDevBuild)
		xNOP();
	if (IsDebugBuild)
		xMOV(eax, pc);

	cpuRegs.code = *(int*)s_pCode;

	if (!delayslot)
	{
		pc += 4;
		g_cpuFlushedPC = false;
		g_cpuFlushedCode = false;
	}
	else
	{
		// increment after recompiling so that pc points to the branch during recompilation
		g_recompilingDelaySlot = true;
	}

	g_pCurInstInfo++;

	for (i = 0; i < iREGCNT_XMM; ++i)
	{
		if (xmmregs[i].inuse)
		{
			count = _recIsRegWritten(g_pCurInstInfo, (s_nEndBlock - pc) / 4 + 1, xmmregs[i].type, xmmregs[i].reg);
			if (count > 0)
				xmmregs[i].counter = 1000 - count;
			else
				xmmregs[i].counter = 0;
		}
	}

	const OPCODE& opcode = GetCurrentInstruction();

	//pxAssert( !(g_pCurInstInfo->info & EEINSTINFO_NOREC) );
	//Console.Warning("opcode name = %s, it's cycles = %d\n",opcode.Name,opcode.cycles);
	// if this instruction is a jump or a branch, exit right away
	if (delayslot)
	{
		bool check_branch_delay = false;
		switch (_Opcode_)
		{
			case 1:
				switch (_Rt_)
				{
					case 0: case 1: case 2: case 3: case 0x10: case 0x11: case 0x12: case 0x13:
						check_branch_delay = true;
				}
				break;

			case 2: case 3: case 4: case 5: case 6: case 7: case 0x14: case 0x15: case 0x16: case 0x17:
				check_branch_delay = true;
		}
		// Check for branch in delay slot, new code by FlatOut.
		// Gregory tested this in 2017 using the ps2autotests suite and remarked "So far we return 1 (even with this PR), and the HW 2.
		// Original PR and discussion at https://github.com/PCSX2/pcsx2/pull/1783 so we don't forget this information.
		if (check_branch_delay)
		{
			DevCon.Warning("Branch %x in delay slot!", cpuRegs.code);
			_clearNeededX86regs();
			_clearNeededXMMregs();
			pc += 4;
			g_cpuFlushedPC = false;
			g_cpuFlushedCode = false;
			if (g_maySignalException)
				xAND(ptr32[&cpuRegs.CP0.n.Cause], ~(1 << 31)); // BD

			g_recompilingDelaySlot = false;
			return;
		}
	}
	// Check for NOP
	if (cpuRegs.code == 0x00000000)
	{
		// Note: Tests on a ps2 suggested more like 5 cycles for a NOP. But there's many factors in this..
		s_nBlockCycles += 9 * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
	}
	else
	{
		//If the COP0 DIE bit is disabled, cycles should be doubled.
		s_nBlockCycles += opcode.cycles * (2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1));
		try
		{
			opcode.recompile();
		}
		catch (Exception::FailedToAllocateRegister&)
		{
			// Fall back to the interpreter
			recCall(opcode.interpret);
#if 0
			// TODO: Free register ?
			//	_freeXMMregs();
#endif
		}
	}

	if (!delayslot && (_getNumXMMwrite() > 2))
		_flushXMMunused();

	//CHECK_XMMCHANGED();
	_clearNeededX86regs();
	_clearNeededXMMregs();

//	_freeXMMregs();
//	_flushCachedRegs();
//	g_cpuHasConstReg = 1;

	if (delayslot)
	{
		pc += 4;
		g_cpuFlushedPC = false;
		g_cpuFlushedCode = false;
		if (g_maySignalException)
			xAND(ptr32[&cpuRegs.CP0.n.Cause], ~(1 << 31)); // BD
		g_recompilingDelaySlot = false;
	}

	g_maySignalException = false;

	// Stalls normally occur as necessary on the R5900, but when using COP2 (VU0 macro mode),
	// there are some exceptions to this.  We probably don't even know all of them.
	// We emulate the R5900 as if it was fully interlocked (which is mostly true), and
	// in fact we don't have good enough cycle counting to do otherwise.  So for now,
	// we'll try to identify problematic code in games create patches.
	// Look ahead is probably the most reasonable way to do this given the deficiency
	// of our cycle counting.  Real cycle counting is complicated and will have to wait.
	// Instead of counting the cycles I'm going to count instructions.  There are a lot of
	// classes of instructions which use different resources and specific restrictions on
	// coissuing but this is just for printing a warning so I'll simplify.
	// Even when simplified this is not simple and it is very very wrong.

	// CFC2 flag register after arithmetic operation: 5 cycles
	// CTC2 flag register after arithmetic operation... um.  TODO.
	// CFC2 address register after increment/decrement load/store: 5 cycles TODO
	// CTC2 CMSAR0, VCALLMSR CMSAR0: 3 cycles but I want to do some tests.
	// Don't even want to think about DIV, SQRT, RSQRT now.

	if (_Opcode_ == 022) // COP2
	{
		if ((cpuRegs.code >> 25 & 1) == 1 && (cpuRegs.code >> 2 & 0x1ff) == 0xdf) // [LS]Q[DI]
			; // TODO
		else if (_Rs_ == 6) // CTC2
			; // TODO
		else if ((cpuRegs.code & 0x7FC) == 0x3BC) // DIV/RSQRT/SQRT/WAITQ
		{
			int cycles = COP2DivUnitTimings(cpuRegs.code);
			for (u32 p = pc; cycles > 0 && p < s_nEndBlock; p += 4, cycles--)
			{
				cpuRegs.code = memRead32(p);

				if ((_Opcode_ == 022) && (cpuRegs.code & 0x7FC) == 0x3BC) // WaitQ or another DIV op hit (stalled), we're safe
					break;

				else if (COP2IsQOP(cpuRegs.code))
				{
					std::string disasm;
					Console.Warning("Possible incorrect Q value used in COP2. If the game is broken, please report to http://github.com/pcsx2/pcsx2.");
					for (u32 i = s_pCurBlockEx->startpc; i < s_nEndBlock; i += 4)
					{
						disasm = "";
						disR5900Fasm(disasm, memRead32(i), i, false);
						Console.Warning("%x %s%08X %s", i, i == pc - 4 ? "*" : i == p ? "=" : " ", memRead32(i), disasm.c_str());
					}
					break;
				}
			}
		}
		else
		{
			int s = cop2flags(cpuRegs.code);
			int all_count = 0, cop2o_count = 0, cop2m_count = 0;
			for (u32 p = pc; s != 0 && p < s_nEndBlock && all_count < 10 && cop2m_count < 5 && cop2o_count < 4; p += 4)
			{
				// I am so sorry.
				cpuRegs.code = memRead32(p);
				if (_Opcode_ == 022 && _Rs_ == 2) // CFC2
					// rd is fs
					if (_Rd_ == 16 && s & 1 || _Rd_ == 17 && s & 2 || _Rd_ == 18 && s & 4)
					{
						std::string disasm;
						Console.Warning("Possible old value used in COP2 code. If the game is broken, please report to http://github.com/pcsx2/pcsx2.");
						for (u32 i = s_pCurBlockEx->startpc; i < s_nEndBlock; i += 4)
						{
							disasm = "";
							disR5900Fasm(disasm, memRead32(i), i,false);
							Console.Warning("%x %s%08X %s", i, i == pc - 4 ? "*" : i == p ? "=" : " ", memRead32(i), disasm.c_str());
						}
						break;
					}
				s &= ~cop2flags(cpuRegs.code);
				all_count++;
				if (_Opcode_ == 022 && _Rs_ == 8) // COP2 branch, handled incorrectly like most things
					;
				else if (_Opcode_ == 022 && (cpuRegs.code >> 25 & 1) == 0)
					cop2m_count++;
				else if (_Opcode_ == 022)
					cop2o_count++;
			}
		}
	}
	cpuRegs.code = *s_pCode;

	if (!delayslot && (xGetPtr() - recPtr > 0x1000))
		s_nEndBlock = pc;
}

// (Called from recompiled code)]
// This function is called from the recompiler prior to starting execution of *every* recompiled block.
// Calling of this function can be enabled or disabled through the use of EmuConfig.Recompiler.PreBlockChecks
static void PreBlockCheck(u32 blockpc)
{
	/*static int lastrec = 0;
	static int curcount = 0;
	const int skip = 0;

    if( blockpc != 0x81fc0 ) {//&& lastrec != g_lastpc ) {
		curcount++;

		if( curcount > skip ) {
			iDumpRegisters(blockpc, 1);
			curcount = 0;
		}

		lastrec = blockpc;
	}*/
}

#ifdef PCSX2_DEBUG
// Array of cpuRegs.pc block addresses to dump.  USeful for selectively dumping potential
// problem blocks, and seeing what the MIPS code equates to.
static u32 s_recblocks[] = {0};
#endif

// Called when a block under manual protection fails it's pre-execution integrity check.
// (meaning the actual code area has been modified -- ie dynamic modules being loaded or,
//  less likely, self-modifying code)
void dyna_block_discard(u32 start, u32 sz)
{
	eeRecPerfLog.Write(Color_StrongGray, "Clearing Manual Block @ 0x%08X  [size=%d]", start, sz * 4);
	recClear(start, sz);
}

// called when a page under manual protection has been run enough times to be a candidate
// for being reset under the faster vtlb write protection.  All blocks in the page are cleared
// and the block is re-assigned for write protection.
void dyna_page_reset(u32 start, u32 sz)
{
	recClear(start & ~0xfffUL, 0x400);
	manual_counter[start >> 12]++;
	mmap_MarkCountedRamPage(start);
}

static void memory_protect_recompiled_code(u32 startpc, u32 size)
{
	u32 inpage_ptr = HWADDR(startpc);
	u32 inpage_sz  = size * 4;

	// The kernel context register is stored @ 0x800010C0-0x80001300
	// The EENULL thread context register is stored @ 0x81000-....
	bool contains_thread_stack = ((startpc >> 12) == 0x81) || ((startpc >> 12) == 0x80001);

	// note: blocks are guaranteed to reside within the confines of a single page.
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
			xMOV(arg1regd, inpage_ptr);
			xMOV(arg2regd, inpage_sz / 4);
			//xMOV( eax, startpc );		// uncomment this to access startpc (as eax) in dyna_block_discard

			u32 lpc = inpage_ptr;
			u32 stg = inpage_sz;

			while (stg > 0)
			{
				xCMP(ptr32[PSM(lpc)], *(u32*)PSM(lpc));
				xJNE(DispatchBlockDiscard);

				stg -= 4;
				lpc += 4;
			}

			// Tweakpoint!  3 is a 'magic' number representing the number of times a counted block
			// is re-protected before the recompiler gives up and sets it up as an uncounted (permanent)
			// manual block.  Higher thresholds result in more recompilations for blocks that share code
			// and data on the same page.  Side effects of a lower threshold: over extended gameplay
			// with several map changes, a game's overall performance could degrade.

			// (ideally, perhaps, manual_counter should be reset to 0 every few minutes?)

			if (!contains_thread_stack && manual_counter[inpage_ptr >> 12] <= 3)
			{
				// Counted blocks add a weighted (by block size) value into manual_page each time they're
				// run.  If the block gets run a lot, it resets and re-protects itself in the hope
				// that whatever forced it to be manually-checked before was a 1-time deal.

				// Counted blocks have a secondary threshold check in manual_counter, which forces a block
				// to 'uncounted' mode if it's recompiled several times.  This protects against excessive
				// recompilation of blocks that reside on the same codepage as data.

				// fixme? Currently this algo is kinda dumb and results in the forced recompilation of a
				// lot of blocks before it decides to mark a 'busy' page as uncounted.  There might be
				// be a more clever approach that could streamline this process, by doing a first-pass
				// test using the vtlb memory protection (without recompilation!) to reprotect a counted
				// block.  But unless a new algo is relatively simple in implementation, it's probably
				// not worth the effort (tests show that we have lots of recompiler memory to spare, and
				// that the current amount of recompilation is fairly cheap).

				xADD(ptr16[&manual_page[inpage_ptr >> 12]], size);
				xJC(DispatchPageReset);

				// note: clearcnt is measured per-page, not per-block!
				ConsoleColorScope cs(Color_Gray);
				eeRecPerfLog.Write("Manual block @ %08X : size =%3d  page/offs = 0x%05X/0x%03X  inpgsz = %d  clearcnt = %d",
					startpc, size, inpage_ptr >> 12, inpage_ptr & 0xfff, inpage_sz, manual_counter[inpage_ptr >> 12]);
			}
			else
			{
				eeRecPerfLog.Write("Uncounted Manual block @ 0x%08X : size =%3d page/offs = 0x%05X/0x%03X  inpgsz = %d",
					startpc, size, inpage_ptr >> 12, inpage_ptr & 0xfff, inpage_sz);
			}
			break;
	}
}

// Skip MPEG Game-Fix
bool skipMPEG_By_Pattern(u32 sPC)
{

	if (!CHECK_SKIPMPEGHACK)
		return 0;

	// sceMpegIsEnd: lw reg, 0x40(a0); jr ra; lw v0, 0(reg)
	if ((s_nEndBlock == sPC + 12) && (memRead32(sPC + 4) == 0x03e00008))
	{
		u32 code = memRead32(sPC);
		u32 p1 = 0x8c800040;
		u32 p2 = 0x8c020000 | (code & 0x1f0000) << 5;
		if ((code & 0xffe0ffff) != p1)
			return 0;
		if (memRead32(sPC + 8) != p2)
			return 0;
		xMOV(ptr32[&cpuRegs.GPR.n.v0.UL[0]], 1);
		xMOV(ptr32[&cpuRegs.GPR.n.v0.UL[1]], 0);
		xMOV(eax, ptr32[&cpuRegs.GPR.n.ra.UL[0]]);
		xMOV(ptr32[&cpuRegs.pc], eax);
		iBranchTest();
		g_branch = 1;
		pc = s_nEndBlock;
		Console.WriteLn(Color_StrongGreen, "sceMpegIsEnd pattern found! Recompiling skip video fix...");
		return 1;
	}
	return 0;
}

#ifndef PCSX2_CORE
// defined at AppCoreThread.cpp but unclean and should not be public. We're the only
// consumers of it, so it's declared only here.
void LoadAllPatchesAndStuff(const Pcsx2Config&);
static void doPlace0Patches()
{
	LoadAllPatchesAndStuff(EmuConfig);
	ApplyLoadedPatches(PPT_ONCE_ON_LOAD);
}
#endif

static void recRecompile(const u32 startpc)
{
	u32 i = 0;
	u32 willbranch3 = 0;
	u32 usecop2;

#ifdef PCSX2_DEBUG
	if (dumplog & 4)
		iDumpRegisters(startpc, 0);
#endif

	pxAssert(startpc);

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= (recMem->GetPtrEnd() - _64kb))
	{
		eeRecNeedsReset = true;
	}
	else if ((recConstBufPtr - recConstBuf) >= RECCONSTBUF_SIZE - 64)
	{
		Console.WriteLn("EE recompiler stack reset");
		eeRecNeedsReset = true;
	}

	if (eeRecNeedsReset)
		recResetRaw();

	xSetPtr(recPtr);
	recPtr = xGetAlignedCallTarget();

	if (0x8000d618 == startpc)
		DbgCon.WriteLn("Compiling block @ 0x%08x", startpc);

	s_pCurBlock = PC_GETBLOCK(startpc);

	pxAssert(s_pCurBlock->GetFnptr() == (uptr)JITCompile
	      || s_pCurBlock->GetFnptr() == (uptr)JITCompileInBlock);

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));
	pxAssert(!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc));

	s_pCurBlockEx = recBlocks.New(HWADDR(startpc), (uptr)recPtr);

	pxAssert(s_pCurBlockEx);

	if (HWADDR(startpc) == EELOAD_START)
	{
		// The EELOAD _start function is the same across all BIOS versions
		u32 mainjump = memRead32(EELOAD_START + 0x9c);
		if (mainjump >> 26 == 3) // JAL
			g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | (mainjump << 2 & 0x0fffffffU);
	}

	if (g_eeloadMain && HWADDR(startpc) == HWADDR(g_eeloadMain))
	{
		xFastCall((void*)eeloadHook);
		if (g_SkipBiosHack)
		{
			// There are four known versions of EELOAD, identifiable by the location of the 'jal' to the EELOAD function which
			// calls ExecPS2(). The function itself is at the same address in all BIOSs after v1.00-v1.10.
			u32 typeAexecjump = memRead32(EELOAD_START + 0x470); // v1.00, v1.01?, v1.10?
			u32 typeBexecjump = memRead32(EELOAD_START + 0x5B0); // v1.20, v1.50, v1.60 (3000x models)
			u32 typeCexecjump = memRead32(EELOAD_START + 0x618); // v1.60 (3900x models)
			u32 typeDexecjump = memRead32(EELOAD_START + 0x600); // v1.70, v1.90, v2.00, v2.20, v2.30
			if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3)) // JAL to 0x822B8
				g_eeloadExec = EELOAD_START + 0x2B8;
			else if (typeAexecjump >> 26 == 3) // JAL to 0x82170
				g_eeloadExec = EELOAD_START + 0x170;
			else // There might be other types of EELOAD, because these models' BIOSs have not been examined: 18000, 3500x, 3700x, 5500x, and 7900x. However, all BIOS versions have been examined except for v1.01 and v1.10.
				Console.WriteLn("recRecompile: Could not enable launch arguments for fast boot mode; unidentified BIOS version! Please report this to the PCSX2 developers.");
		}

#ifndef PCSX2_CORE
		// On fast/full boot this will have a crc of 0x0. But when the game/elf itself is
		// recompiled (below - ElfEntry && g_GameLoading), then the crc would be from the elf.
		// g_patchesNeedRedo is set on rec reset, and this is the only consumer.
		// Also makes sure that patches from the previous elf/game are not applied on boot.
		if (g_patchesNeedRedo)
			doPlace0Patches();
		g_patchesNeedRedo = 0;
#endif
	}

	if (g_eeloadExec && HWADDR(startpc) == HWADDR(g_eeloadExec))
		xFastCall((void*)eeloadHook2);

	// this is the only way patches get applied, doesn't depend on a hack
	if (g_GameLoading && HWADDR(startpc) == ElfEntry)
	{
		Console.WriteLn("Elf entry point @ 0x%08x about to get recompiled. Load patches first.", startpc);
		xFastCall((void*)eeGameStarting);

#ifndef PCSX2_CORE
		// Apply patch as soon as possible. Normally it is done in
		// eeGameStarting but first block is already compiled.
		doPlace0Patches();
#else
		VMManager::Internal::EntryPointCompilingOnCPUThread();
#endif
	}

	g_branch = 0;

	// reset recomp state variables
	s_nBlockCycles = 0;
	s_nBlockInterlocked = false;
	pc = startpc;
	g_cpuHasConstReg = g_cpuFlushedConstReg = 1;
	pxAssert(g_cpuConstRegs[0].UD[0] == 0);

	_initX86regs();
	_initXMMregs();

	if (EmuConfig.Cpu.Recompiler.PreBlockCheckEE)
	{
		// per-block dump checks, for debugging purposes.
		// [TODO] : These must be enabled from the GUI or INI to be used, otherwise the
		// code that calls PreBlockCheck will not be generated.

		xFastCall((void*)PreBlockCheck, pc);
	}

	if (EmuConfig.Gamefixes.GoemonTlbHack)
	{
		if (pc == 0x33ad48 || pc == 0x35060c)
		{
			// 0x33ad48 and 0x35060c are the return address of the function (0x356250) that populate the TLB cache
			xFastCall((void*)GoemonPreloadTlb);
		}
		else if (pc == 0x3563b8)
		{
			// Game will unmap some virtual addresses. If a constant address were hardcoded in the block, we would be in a bad situation.
			eeRecNeedsReset = true;
			// 0x3563b8 is the start address of the function that invalidate entry in TLB cache
			xFastCall((void*)GoemonUnloadTlb, ptr32[&cpuRegs.GPR.n.a0.UL[0]]);
		}
	}

	// go until the next branch
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

	// compile breakpoints as individual blocks
	int n1 = isBreakpointNeeded(i);
	int n2 = isMemcheckNeeded(i);
	int n = std::max<int>(n1, n2);
	if (n != 0)
	{
		s_nEndBlock = i + n * 4;
		goto StartRecomp;
	}

	while (1)
	{
		BASEBLOCK* pblock = PC_GETBLOCK(i);

		// stop before breakpoints
		if (isBreakpointNeeded(i) != 0 || isMemcheckNeeded(i) != 0)
		{
			s_nEndBlock = i;
			break;
		}

		if (i != startpc) // Block size truncation checks.
		{
			if ((i & 0xffc) == 0x0) // breaks blocks at 4k page boundaries
			{
				willbranch3 = 1;
				s_nEndBlock = i;

				eeRecPerfLog.Write("Pagesplit @ %08X : size=%d insts", startpc, (i - startpc) / 4);
				break;
			}

			if (pblock->GetFnptr() != (uptr)JITCompile && pblock->GetFnptr() != (uptr)JITCompileInBlock)
			{
				willbranch3 = 1;
				s_nEndBlock = i;
				break;
			}
		}

		//HUH ? PSM ? whut ? THIS IS VIRTUAL ACCESS GOD DAMMIT
		cpuRegs.code = *(int*)PSM(i);

		switch (cpuRegs.code >> 26)
		{
			case 0: // special
				if (_Funct_ == 8 || _Funct_ == 9) // JR, JALR
				{
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;

			case 1: // regimm

				if (_Rt_ < 4 || (_Rt_ >= 16 && _Rt_ < 20))
				{
					// branches
					s_branchTo = _Imm_ * 4 + i + 4;
					if (s_branchTo > startpc && s_branchTo < i)
						s_nEndBlock = s_branchTo;
					else
						s_nEndBlock = i + 8;

					goto StartRecomp;
				}
				break;

			case 2: // J
			case 3: // JAL
				s_branchTo = _InstrucTarget_ << 2 | (i + 4) & 0xf0000000;
				s_nEndBlock = i + 8;
				goto StartRecomp;

			// branches
			case 4: case 5: case 6: case 7:
			case 20: case 21: case 22: case 23:
				s_branchTo = _Imm_ * 4 + i + 4;
				if (s_branchTo > startpc && s_branchTo < i)
					s_nEndBlock = s_branchTo;
				else
					s_nEndBlock = i + 8;

				goto StartRecomp;

			case 16: // cp0
				if (_Rs_ == 16)
				{
					if (_Funct_ == 24) // eret
					{
						s_nEndBlock = i + 4;
						goto StartRecomp;
					}
				}
				// Fall through!
				// COP0's branch opcodes line up with COP1 and COP2's

			case 17: // cp1
			case 18: // cp2
				if (_Rs_ == 8)
				{
					// BC1F, BC1T, BC1FL, BC1TL
					// BC2F, BC2T, BC2FL, BC2TL
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

	// The idea here is that as long as a loop doesn't write to a register it's already read
	// (excepting registers initialised with constants or memory loads) or use any instructions
	// which alter the machine state apart from registers, it will do the same thing on every
	// iteration.
	// TODO: special handling for counting loops.  God of war wastes time in a loop which just
	// counts to some large number and does nothing else, many other games use a counter as a
	// timeout on a register read.  AFAICS the only way to optimise this for non-const cases
	// without a significant loss in cycle accuracy is with a division, but games would probably
	// be happy with time wasting loops completing in 0 cycles and timeouts waiting forever.
	s_nBlockFF = false;
	if (s_branchTo == startpc)
	{
		s_nBlockFF = true;

		u32 reads = 0, loads = 1;

		for (i = startpc; i < s_nEndBlock; i += 4)
		{
			if (i == s_nEndBlock - 8)
				continue;
			cpuRegs.code = *(u32*)PSM(i);
			// nop
			if (cpuRegs.code == 0)
				continue;
			// cache, sync
			else if (_Opcode_ == 057 || _Opcode_ == 0 && _Funct_ == 017)
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

	// rec info //
	bool has_cop2_instructions = false;
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

		for (i = s_nEndBlock; i > startpc; i -= 4)
		{
			cpuRegs.code = *(int*)PSM(i - 4);
			pcur[-1] = pcur[0];
			pcur--;

			has_cop2_instructions |= (_Opcode_ == 022);
		}
	}

	// eventually we'll want to have a vector of passes or something.
	if (has_cop2_instructions && EmuConfig.Speedhacks.vuFlagHack)
	{
		COP2FlagHackPass fhpass;
		fhpass.Run(startpc, s_nEndBlock, s_pInstCache + 1);
	}

	// analyze instructions //
	{
		usecop2 = 0;
		g_pCurInstInfo = s_pInstCache;

		for (i = startpc; i < s_nEndBlock; i += 4)
		{
			g_pCurInstInfo++;
			cpuRegs.code = *(u32*)PSM(i);

			// cop2 //
			if (g_pCurInstInfo->info & EEINSTINFO_COP2)
			{

				if (!usecop2)
				{
					// init
					usecop2 = 1;
				}

				VU0.code = cpuRegs.code;
				continue;
			}
		}
		// This *is* important because g_pCurInstInfo is checked a bit later on and
		// if it's not equal to s_pInstCache it handles recompilation differently.
		// ... but the empty if() conditional inside the for loop is still amusing. >_<
		if (usecop2)
		{
			// add necessary mac writebacks
			g_pCurInstInfo = s_pInstCache;

			for (i = startpc; i < s_nEndBlock - 4; i += 4)
			{
				g_pCurInstInfo++;

				if (g_pCurInstInfo->info & EEINSTINFO_COP2)
				{
				}
			}
		}
	}

#ifdef PCSX2_DEBUG
	// dump code
	for (u32 recblock : s_recblocks)
	{
		if (startpc == recblock)
		{
			iDumpBlock(startpc, recPtr);
		}
	}

	if (dumplog & 1)
		iDumpBlock(startpc, recPtr);
#endif

	// Detect and handle self-modified code
	memory_protect_recompiled_code(startpc, (s_nEndBlock - startpc) >> 2);

	// Skip Recompilation if sceMpegIsEnd Pattern detected
	bool doRecompilation = !skipMPEG_By_Pattern(startpc);

	if (doRecompilation)
	{
		// Finally: Generate x86 recompiled code!
		g_pCurInstInfo = s_pInstCache;
		while (!g_branch && pc < s_nEndBlock)
		{
			recompileNextInstruction(0); // For the love of recursion, batman!
		}
	}

#ifdef PCSX2_DEBUG
	if (dumplog & 1)
		iDumpBlock(startpc, recPtr);
#endif

	pxAssert((pc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (pc - startpc) >> 2;

	if (HWADDR(pc) <= Ps2MemSize::MainRam)
	{
		BASEBLOCKEX* oldBlock;
		int i;

		i = recBlocks.LastIndex(HWADDR(pc) - 4);
		while (oldBlock = recBlocks[i--])
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
				pxAssert(s_pCurBlockEx->startpc == HWADDR(startpc));
				break;
			}
		}

		memcpy(&recRAMCopy[HWADDR(startpc) / 4], PSM(startpc), pc - startpc);
	}

	s_pCurBlock->SetFnptr((uptr)recPtr);

	for (i = 1; i < (u32)s_pCurBlockEx->size; i++)
	{
		if ((uptr)JITCompile == s_pCurBlock[i].GetFnptr())
			s_pCurBlock[i].SetFnptr((uptr)JITCompileInBlock);
	}

	if (!(pc & 0x10000000))
		maxrecmem = std::max((pc & ~0xa0000000), maxrecmem);

	if (g_branch == 2)
	{
		// Branch type 2 - This is how I "think" this works (air):
		// Performs a branch/event test but does not actually "break" the block.
		// This allows exceptions to be raised, and is thus sufficient for
		// certain types of things like SYSCALL, EI, etc.  but it is not sufficient
		// for actual branching instructions.

		iFlushCall(FLUSH_EVERYTHING);
		iBranchTest();
	}
	else
	{
		if (g_branch)
			pxAssert(!willbranch3);

		if (willbranch3 || !g_branch)
		{

			iFlushCall(FLUSH_EVERYTHING);

			// Split Block concatenation mode.
			// This code is run when blocks are split either to keep block sizes manageable
			// or because we're crossing a 4k page protection boundary in ps2 mem.  The latter
			// case can result in very short blocks which should not issue branch tests for
			// performance reasons.

			int numinsts = (pc - startpc) / 4;
			if (numinsts > 6)
				SetBranchImm(pc);
			else
			{
				xMOV(ptr32[&cpuRegs.pc], pc);
				xADD(ptr32[&cpuRegs.cycle], scaleblockcycles());
				recBlocks.Link(HWADDR(pc), xJcc32());
			}
		}
	}

	pxAssert(xGetPtr() < recMem->GetPtrEnd());
	pxAssert(recConstBufPtr < recConstBuf + RECCONSTBUF_SIZE);

	pxAssert(xGetPtr() - recPtr < _64kb);
	s_pCurBlockEx->x86size = xGetPtr() - recPtr;

#if 0
	// Example: Dump both x86/EE code
	if (startpc == 0x456630) {
		iDumpBlock(s_pCurBlockEx->startpc, s_pCurBlockEx->size*4, s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size);
	}
#endif
	Perf::ee.map(s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = xGetPtr();

	pxAssert((g_cpuHasConstReg & g_cpuFlushedConstReg) == g_cpuHasConstReg);

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

// The only *safe* way to throw exceptions from the context of recompiled code.
// The exception is cached and the recompiler is exited safely using either an
// SEH unwind (MSW) or setjmp/longjmp (GCC).
static void recThrowException(const BaseR5900Exception& ex)
{
	if (!eeCpuExecuting)
		ex.Rethrow();
	m_cpuException = std::unique_ptr<BaseR5900Exception>(ex.Clone());
	recExitExecution();
}

static void recThrowException(const BaseException& ex)
{
	if (!eeCpuExecuting)
		ex.Rethrow();
	m_Exception = ScopedExcept(ex.Clone());
	recExitExecution();
}

static void recSetCacheReserve(uint reserveInMegs)
{
	m_ConfiguredCacheReserve = reserveInMegs;
}

static uint recGetCacheReserve()
{
	return m_ConfiguredCacheReserve;
}

R5900cpu recCpu =
{
	recReserve,
	recShutdown,

	recResetEE,
	recStep,
	recExecute,

	recSafeExitExecution,
	recThrowException,
	recThrowException,
	recClear,

	recGetCacheReserve,
	recSetCacheReserve,
};
