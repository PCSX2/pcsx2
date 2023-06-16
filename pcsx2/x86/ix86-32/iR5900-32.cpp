/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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
#include "CDVD/CDVD.h"
#include "DebugTools/Breakpoints.h"
#include "Elfheader.h"
#include "GS.h"
#include "Memory.h"
#include "Patch.h"
#include "R3000A.h"
#include "R5900OpcodeTables.h"
#include "VMManager.h"
#include "VirtualMemory.h"
#include "vtlb.h"
#include "x86/BaseblockEx.h"
#include "x86/iR5900.h"
#include "x86/iR5900Analysis.h"

#include "common/AlignedMalloc.h"
#include "common/FastJmp.h"
#include "common/Perf.h"

// Only for MOVQ workaround.
#include "common/emitter/internal.h"

//#define DUMP_BLOCKS 1
//#define TRACE_BLOCKS 1

#ifdef DUMP_BLOCKS
#include "Zydis/Zydis.h"
#include "Zycore/Format.h"
#include "Zycore/Status.h"
#endif

#ifdef TRACE_BLOCKS
#include <zlib.h>
#endif

using namespace x86Emitter;
using namespace R5900;

static bool eeRecNeedsReset = false;
static bool eeCpuExecuting = false;
static bool eeRecExitRequested = false;
static bool g_resetEeScalingStats = false;

#define PC_GETBLOCK(x) PC_GETBLOCK_(x, recLUT)

u32 maxrecmem = 0;
alignas(16) static uptr recLUT[_64kb];
alignas(16) static u32 hwLUT[_64kb];

static __fi u32 HWADDR(u32 mem) { return hwLUT[mem >> 16] + mem; }

u32 s_nBlockCycles = 0; // cycles of current block recompiling
bool s_nBlockInterlocked = false; // Block is VU0 interlocked
u32 pc; // recompiler pc
int g_branch; // set for branch

alignas(16) GPR_reg64 g_cpuConstRegs[32] = {};
u32 g_cpuHasConstReg = 0, g_cpuFlushedConstReg = 0;
bool g_cpuFlushedPC, g_cpuFlushedCode, g_recompilingDelaySlot, g_maySignalException;

eeProfiler EE::Profiler;

////////////////////////////////////////////////////////////////
// Static Private Variables - R5900 Dynarec

#define X86

static RecompiledCodeReserve* recMem = NULL;
static u8* recRAMCopy = NULL;
static u8* recLutReserve_RAM = NULL;
static const size_t recLutSize = (Ps2MemSize::MainRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) * wordsize / 4;

static BASEBLOCK* recRAM = NULL; // and the ptr to the blocks here
static BASEBLOCK* recROM = NULL; // and here
static BASEBLOCK* recROM1 = NULL; // also here
static BASEBLOCK* recROM2 = NULL; // also here

static BaseBlocks recBlocks;
static u8* recPtr = NULL;
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

static void iBranchTest(u32 newpc = 0xffffffff);
static void ClearRecLUT(BASEBLOCK* base, int count);
static u32 scaleblockcycles();
static void recExitExecution();

#ifdef TRACE_BLOCKS
static void pauseAAA()
{
	fprintf(stderr, "\nPaused\n");
	fflush(stdout);
	fflush(stderr);
#ifdef _MSC_VER
	__debugbreak();
#else
	sleep(1);
#endif
}
#endif

#ifdef DUMP_BLOCKS
static ZydisFormatterFunc s_old_print_address;

static ZyanStatus ZydisFormatterPrintAddressAbsolute(const ZydisFormatter* formatter,
	ZydisFormatterBuffer* buffer, ZydisFormatterContext* context)
{
	ZyanU64 address;
	ZYAN_CHECK(ZydisCalcAbsoluteAddress(context->instruction, context->operand,
		context->runtime_address, &address));

	char buf[128];
	u32 len = 0;

#define A(x) ((u64)(x))

	if (address >= A(eeMem->Main) && address < A(eeMem->Scratch))
	{
		len = snprintf(buf, sizeof(buf), "eeMem+0x%08X", static_cast<u32>(address - A(eeMem->Main)));
	}
	else if (address >= A(eeMem->Scratch) && address < A(eeMem->ROM))
	{
		len = snprintf(buf, sizeof(buf), "eeScratchpad+0x%08X", static_cast<u32>(address - A(eeMem->Scratch)));
	}
	else if (address >= A(&cpuRegs.GPR) && address < A(&cpuRegs.HI))
	{
		const u32 offset = static_cast<u32>(address - A(&cpuRegs)) % 16u;
		if (offset != 0)
			len = snprintf(buf, sizeof(buf), "cpuRegs.GPR.%s+%u", GPR_REG[static_cast<u32>(address - A(&cpuRegs)) / 16u], offset);
		else
			len = snprintf(buf, sizeof(buf), "cpuRegs.GPR.%s", GPR_REG[static_cast<u32>(address - A(&cpuRegs)) / 16u]);
	}
	else if (address >= A(&cpuRegs.HI) && address < A(&cpuRegs.CP0))
	{
		const u32 offset = static_cast<u32>(address - A(&cpuRegs.HI)) % 16u;
		if (offset != 0)
			len = snprintf(buf, sizeof(buf), "cpuRegs.%s+%u", (address >= A(&cpuRegs.LO) ? "LO" : "HI"), offset);
		else
			len = snprintf(buf, sizeof(buf), "cpuRegs.%s", (address >= A(&cpuRegs.LO) ? "LO" : "HI"));
	}
	else if (address == A(&cpuRegs.pc))
	{
		len = snprintf(buf, sizeof(buf), "cpuRegs.pc");
	}
	else if (address == A(&cpuRegs.cycle))
	{
		len = snprintf(buf, sizeof(buf), "cpuRegs.cycle");
	}
	else if (address == A(&cpuRegs.nextEventCycle))
	{
		len = snprintf(buf, sizeof(buf), "cpuRegs.nextEventCycle");
	}
	else if (address >= A(fpuRegs.fpr) && address < A(fpuRegs.fprc))
	{
		len = snprintf(buf, sizeof(buf), "fpuRegs.f%02u", static_cast<u32>(address - A(fpuRegs.fpr)) / 4u);
	}
	else if (address >= A(&VU0.VF[0]) && address < A(&VU0.VI[0]))
	{
		const u32 offset = static_cast<u32>(address - A(&VU0.VF[0])) % 16u;
		if (offset != 0)
			len = snprintf(buf, sizeof(buf), "VU0.VF[%02u]+%u", static_cast<u32>(address - A(&VU0.VF[0])) / 16u, offset);
		else
			len = snprintf(buf, sizeof(buf), "VU0.VF[%02u]", static_cast<u32>(address - A(&VU0.VF[0])) / 16u);
	}
	else if (address >= A(&VU0.VI[0]) && address < A(&VU0.ACC))
	{
		const u32 offset = static_cast<u32>(address - A(&VU0.VI[0])) % 16u;
		const u32 vi = static_cast<u32>(address - A(&VU0.VI[0])) / 16u;
		if (offset != 0)
			len = snprintf(buf, sizeof(buf), "VU0.%s+%u", COP2_REG_CTL[vi], offset);
		else
			len = snprintf(buf, sizeof(buf), "VU0.%s", COP2_REG_CTL[vi]);
	}
	else if (address >= A(&VU0.ACC) && address < A(&VU0.q))
	{
		const u32 offset = static_cast<u32>(address - A(&VU0.ACC));
		if (offset != 0)
			len = snprintf(buf, sizeof(buf), "VU0.ACC+%u", offset);
		else
			len = snprintf(buf, sizeof(buf), "VU0.ACC");
	}
	else if (address >= A(&VU0.q) && address < A(&VU0.idx))
	{
		const u32 offset = static_cast<u32>(address - A(&VU0.q)) % 16u;
		const char* reg = (address >= A(&VU0.p)) ? "p" : "q";
		if (offset != 0)
			len = snprintf(buf, sizeof(buf), "VU0.%s+%u", reg, offset);
		else
			len = snprintf(buf, sizeof(buf), "VU0.%s", reg);
	}

#undef A

	if (len > 0)
	{
		ZYAN_CHECK(ZydisFormatterBufferAppend(buffer, ZYDIS_TOKEN_SYMBOL));
		ZyanString* string;
		ZYAN_CHECK(ZydisFormatterBufferGetString(buffer, &string));
		return ZyanStringAppendFormat(string, "&%s", buf);
	}

	return s_old_print_address(formatter, buffer, context);
}
#endif

void _eeFlushAllDirty()
{
	_flushXMMregs();
	_flushX86regs();

	// flush constants, do them all at once for slightly better codegen
	_flushConstRegs();
}

void _eeMoveGPRtoR(const xRegister32& to, int fromgpr, bool allow_preload)
{
	if (fromgpr == 0)
		xXOR(to, to);
	else if (GPR_IS_CONST1(fromgpr))
		xMOV(to, g_cpuConstRegs[fromgpr].UL[0]);
	else
	{
		int x86reg = _checkX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
		int xmmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ);

		if (allow_preload && x86reg < 0 && xmmreg < 0)
		{
			if (EEINST_XMMUSEDTEST(fromgpr))
				xmmreg = _allocGPRtoXMMreg(fromgpr, MODE_READ);
			else if (EEINST_USEDTEST(fromgpr))
				x86reg = _allocX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
		}

		if (x86reg >= 0)
			xMOV(to, xRegister32(x86reg));
		else if (xmmreg >= 0)
			xMOVD(to, xRegisterSSE(xmmreg));
		else
			xMOV(to, ptr[&cpuRegs.GPR.r[fromgpr].UL[0]]);
	}
}

void _eeMoveGPRtoR(const xRegister64& to, int fromgpr, bool allow_preload)
{
	if (fromgpr == 0)
		xXOR(xRegister32(to), xRegister32(to));
	else if (GPR_IS_CONST1(fromgpr))
		xMOV64(to, g_cpuConstRegs[fromgpr].UD[0]);
	else
	{
		int x86reg = _checkX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
		int xmmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ);

		if (allow_preload && x86reg < 0 && xmmreg < 0)
		{
			if (EEINST_XMMUSEDTEST(fromgpr))
				xmmreg = _allocGPRtoXMMreg(fromgpr, MODE_READ);
			else if (EEINST_USEDTEST(fromgpr))
				x86reg = _allocX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
		}

		if (x86reg >= 0)
			xMOV(to, xRegister64(x86reg));
		else if (xmmreg >= 0)
			xMOVD(to, xRegisterSSE(xmmreg));
		else
			xMOV(to, ptr32[&cpuRegs.GPR.r[fromgpr].UD[0]]);
	}
}

void _eeMoveGPRtoM(uptr to, int fromgpr)
{
	if (GPR_IS_CONST1(fromgpr))
		xMOV(ptr32[(u32*)(to)], g_cpuConstRegs[fromgpr].UL[0]);
	else
	{
		int x86reg = _checkX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
		int xmmreg = _checkXMMreg(XMMTYPE_GPRREG, fromgpr, MODE_READ);

		if (x86reg < 0 && xmmreg < 0)
		{
			if (EEINST_XMMUSEDTEST(fromgpr))
				xmmreg = _allocGPRtoXMMreg(fromgpr, MODE_READ);
			else if (EEINST_USEDTEST(fromgpr))
				x86reg = _allocX86reg(X86TYPE_GPR, fromgpr, MODE_READ);
		}

		if (x86reg >= 0)
		{
			xMOV(ptr32[(void*)(to)], xRegister32(x86reg));
		}
		else if (xmmreg >= 0)
		{
			xMOVSS(ptr32[(void*)(to)], xRegisterSSE(xmmreg));
		}
		else
		{
			xMOV(eax, ptr32[&cpuRegs.GPR.r[fromgpr].UL[0]]);
			xMOV(ptr32[(void*)(to)], eax);
		}
	}
}

// Use this to call into interpreter functions that require an immediate branchtest
// to be done afterward (anything that throws an exception or enables interrupts, etc).
void recBranchCall(void (*func)())
{
	// In order to make sure a branch test is performed, the nextBranchCycle is set
	// to the current cpu cycle.

	xMOV(eax, ptr[&cpuRegs.cycle]);
	xMOV(ptr[&cpuRegs.nextEventCycle], eax);

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

static DynGenFunc* DispatcherEvent = NULL;
static DynGenFunc* DispatcherReg = NULL;
static DynGenFunc* JITCompile = NULL;
static DynGenFunc* JITCompileInBlock = NULL;
static DynGenFunc* EnterRecompiledCode = NULL;
static DynGenFunc* ExitRecompiledCode = NULL;
static DynGenFunc* DispatchBlockDiscard = NULL;
static DynGenFunc* DispatchPageReset = NULL;

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
		xScopedStackFrame frame(true, true);
#else
		xScopedStackFrame frame(false, true);
#endif

		if (CHECK_FASTMEM)
			xMOV(RFASTMEMBASE, ptrNative[&vtlb_private::vtlbdata.fastmem_base]);

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
	DispatcherReg = _DynGen_DispatcherReg();

	JITCompile = _DynGen_JITCompile();
	JITCompileInBlock = _DynGen_JITCompileInBlock();
	EnterRecompiledCode = _DynGen_EnterRecompiledCode();
	DispatchBlockDiscard = _DynGen_DispatchBlockDiscard();
	DispatchPageReset = _DynGen_DispatchPageReset();

	HostSys::MemProtectStatic(eeRecDispatchers, PageAccess_ExecOnly());

	recBlocks.SetJITCompile(JITCompile);

	Perf::any.Register((void*)eeRecDispatchers, 4096, "EE Dispatcher");
}


//////////////////////////////////////////////////////////////////////////////////////////
//

static __ri void ClearRecLUT(BASEBLOCK* base, int memsize)
{
	for (int i = 0; i < memsize / (int)sizeof(uptr); i++)
		base[i].SetFnptr((uptr)JITCompile);
}

static void recReserve()
{
	if (recMem)
		return;

	recMem = new RecompiledCodeReserve("R5900 Recompiler Cache");
	recMem->Assign(GetVmMemory().CodeMemory(), HostMemoryMap::EErecOffset, 64 * _1mb);
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
	recRAM = basepos;
	basepos += (Ps2MemSize::MainRam / 4);
	recROM = basepos;
	basepos += (Ps2MemSize::Rom / 4);
	recROM1 = basepos;
	basepos += (Ps2MemSize::Rom1 / 4);
	recROM2 = basepos;
	basepos += (Ps2MemSize::Rom2 / 4);

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

	for (int i = 0x1e00; i < 0x1e40; i++)
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
		if (!s_pInstCache)
			pxFailRel("Failed to allocate R5900-32 InstCache array");
	}

	// No errors.. Proceed with initialization:

	_DynGen_Dispatchers();
}

alignas(16) static u16 manual_page[Ps2MemSize::MainRam >> 12];
alignas(16) static u8 manual_counter[Ps2MemSize::MainRam >> 12];

////////////////////////////////////////////////////
static void recResetRaw()
{
	Console.WriteLn(Color_StrongBlack, "EE/iR5900-32 Recompiler Reset");

	EE::Profiler.Reset();

	recAlloc();

	recMem->Reset();
	ClearRecLUT((BASEBLOCK*)recLutReserve_RAM, recLutSize);
	memset(recRAMCopy, 0, Ps2MemSize::MainRam);

	maxrecmem = 0;

	if (s_pInstCache)
		memset(s_pInstCache, 0, sizeof(EEINST) * s_nInstCacheSize);

	recBlocks.Reset();
	mmap_ResetBlockTracking();
	vtlb_ClearLoadStoreInfo();

	x86SetPtr(*recMem);

	recPtr = *recMem;

	g_branch = 0;
	g_resetEeScalingStats = true;
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
}

void recStep()
{
}

static fastjmp_buf m_SetJmp_StateCheck;

static void recExitExecution()
{
	fastjmp_jmp(&m_SetJmp_StateCheck, 1);
}

static void recSafeExitExecution()
{
	// If we're currently processing events, we can't safely jump out of the recompiler here, because we'll
	// leave things in an inconsistent state. So instead, we flag it for exiting once cpuEventTest() returns.
	// Exiting in the middle of a rec block with the registers unsaved would be a bad idea too..
	eeRecExitRequested = true;

	// Force an event test at the end of this block.
	if (!eeEventTestIsActive)
	{
		// EE is running.
		cpuRegs.nextEventCycle = 0;
	}
	else
	{
		// IOP might be running, so break out if so.
		if (psxRegs.iopCycleEE > 0)
		{
			psxRegs.iopBreak += psxRegs.iopCycleEE; // record the number of cycles the IOP didn't run.
			psxRegs.iopCycleEE = 0;
		}
	}
}

static void recResetEE()
{
	if (eeCpuExecuting)
	{
		// get outta here as soon as we can
		eeRecNeedsReset = true;
		recSafeExitExecution();
		return;
	}

	recResetRaw();
}

static void recCancelInstruction()
{
	pxFailRel("recCancelInstruction() called, this should never happen!");
}

static void recExecute()
{
	// Reset before we try to execute any code, if there's one pending.
	// We need to do this here, because if we reset while we're executing, it sets the "needs reset"
	// flag, which triggers a JIT exit (the fastjmp_set below), and eventually loops back here.
	if (eeRecNeedsReset)
	{
		eeRecNeedsReset = false;
		recResetRaw();
	}

	// setjmp will save the register context and will return 0
	// A call to longjmp will restore the context (included the eip/rip)
	// but will return the longjmp 2nd parameter (here 1)
	if (!fastjmp_set(&m_SetJmp_StateCheck))
	{
		eeCpuExecuting = true;

		// Important! Most of the console logging and such has cancel points in it.  This is great
		// in Windows, where SEH lets us safely kill a thread from anywhere we want.  This is bad
		// in Linux, which cannot have a C++ exception cross the recompiler.  Hence the changing
		// of the cancelstate here!

		EnterRecompiledCode();

		// Generally unreachable code here ...
	}

	eeCpuExecuting = false;

	EE::Profiler.Print();
}

////////////////////////////////////////////////////
void R5900::Dynarec::OpcodeImpl::recSYSCALL()
{
	EE::Profiler.EmitOp(eeOpcode::SYSCALL);

	recCall(R5900::Interpreter::OpcodeImpl::SYSCALL);
	g_branch = 2; // Indirect branch with event check.
}

////////////////////////////////////////////////////
void R5900::Dynarec::OpcodeImpl::recBREAK()
{
	EE::Profiler.EmitOp(eeOpcode::BREAK);

	recCall(R5900::Interpreter::OpcodeImpl::BREAK);
	g_branch = 2; // Indirect branch with event check.
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

	while ((pexblock = recBlocks[blockidx]))
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

	for (int i = 0; (pexblock = recBlocks[i]); i++)
	{
		if (s_pCurBlock == PC_GETBLOCK(pexblock->startpc))
			continue;
		u32 blockend = pexblock->startpc + pexblock->size * 4;
		if ((pexblock->startpc >= addr && pexblock->startpc < addr + size * 4) || (pexblock->startpc < addr && blockend > addr))
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
		const bool swap = EmuConfig.Gamefixes.GoemonTlbHack ? false : TrySwapDelaySlot(reg, 0, 0, true);
		if (!swap)
		{
			const int wbreg = _allocX86reg(X86TYPE_PCWRITEBACK, 0, MODE_WRITE | MODE_CALLEESAVED);
			_eeMoveGPRtoR(xRegister32(wbreg), reg);

			if (EmuConfig.Gamefixes.GoemonTlbHack)
			{
				xMOV(ecx, xRegister32(wbreg));
				vtlb_DynV2P();
				xMOV(xRegister32(wbreg), eax);
			}

			recompileNextInstruction(true, false);

			// the next instruction may have flushed the register.. so reload it if so.
			if (x86regs[wbreg].inuse && x86regs[wbreg].type == X86TYPE_PCWRITEBACK)
			{
				xMOV(ptr[&cpuRegs.pc], xRegister32(wbreg));
				x86regs[wbreg].inuse = 0;
			}
			else
			{
				xMOV(eax, ptr[&cpuRegs.pcWriteback]);
				xMOV(ptr[&cpuRegs.pc], eax);
			}
		}
		else
		{
			if (GPR_IS_DIRTY_CONST(reg) || _hasX86reg(X86TYPE_GPR, reg, 0))
			{
				const int x86reg = _allocX86reg(X86TYPE_GPR, reg, MODE_READ);
				xMOV(ptr32[&cpuRegs.pc], xRegister32(x86reg));
			}
			else
			{
				_eeMoveGPRtoM((uptr)&cpuRegs.pc, reg);
			}
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

u8* recBeginThunk()
{
	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= (recMem->GetPtrEnd() - _64kb))
		eeRecNeedsReset = true;

	xSetPtr(recPtr);
	recPtr = xGetAlignedCallTarget();

	x86Ptr = recPtr;
	return recPtr;
}

u8* recEndThunk()
{
	u8* block_end = x86Ptr;

	pxAssert(block_end < recMem->GetPtrEnd());
	recPtr = block_end;
	return block_end;
}

bool TrySwapDelaySlot(u32 rs, u32 rt, u32 rd, bool allow_loadstore)
{
#if 1
	if (g_recompilingDelaySlot)
		return false;

	const u32 opcode_encoded = *(u32*)PSM(pc);
	if (opcode_encoded == 0)
	{
		recompileNextInstruction(true, true);
		return true;
	}

	//std::string disasm;
	//disR5900Fasm(disasm, opcode_encoded, pc, false);

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
			// fprintf(stderr, "SWAPPING coprocessor load delay slot (block %08X) %08X %s\n", s_pCurBlockEx->startpc, pc, disasm.c_str());
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
					// fprintf(stderr, "Swapping FPU delay slot (block %08X) %08X %s\n", s_pCurBlockEx->startpc, pc, disasm.c_str());
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

			// fprintf(stderr, "Swapping COP2 delay slot (block %08X) %08X %s\n", s_pCurBlockEx->startpc, pc, disasm.c_str());
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

	// fprintf(stderr, "Swapping delay slot %08X %s\n", pc, disasm.c_str());
	recompileNextInstruction(true, true);
	return true;

is_unsafe:
	// fprintf(stderr, "NOT SWAPPING delay slot %08X %s\n", pc, disasm.c_str());
	return false;
#else
	return false;
#endif
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
	for (u32 i = 0; i < iREGCNT_GPR; i++)
	{
		if (!x86regs[i].inuse)
			continue;

		if (xRegisterBase::IsCallerSaved(i) ||
			((flushtype & FLUSH_FREE_VU0) && x86regs[i].type == X86TYPE_VIREG) ||
			((flushtype & FLUSH_FREE_NONTEMP_X86) && x86regs[i].type != X86TYPE_TEMP) ||
			((flushtype & FLUSH_FREE_TEMP_X86) && x86regs[i].type == X86TYPE_TEMP))
		{
			_freeX86reg(i);
		}
	}

	for (u32 i = 0; i < iREGCNT_XMM; i++)
	{
		if (!xmmregs[i].inuse)
			continue;

		if (xRegisterSSE::IsCallerSaved(i) ||
			(flushtype & FLUSH_FREE_XMM) ||
			((flushtype & FLUSH_FREE_VU0) && xmmregs[i].type == XMMTYPE_VFREG))
		{
			_freeXMMreg(i);
		}
	}

	if (flushtype & FLUSH_ALL_X86)
		_flushX86regs();

	if (flushtype & FLUSH_FLUSH_XMM)
		_flushXMMregs();

	if (flushtype & FLUSH_CONSTANT_REGS)
		_flushConstRegs();

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

#if 0
	if ((flushtype == FLUSH_CAUSE) && !g_maySignalException)
	{
		if (g_recompilingDelaySlot)
			xOR(ptr32[&cpuRegs.CP0.n.Cause], 1 << 31); // BD
		g_maySignalException = true;
	}
#endif
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
		xMOV(eax, ptr32[&cpuRegs.nextEventCycle]);
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
		xSUB(eax, ptr[&cpuRegs.nextEventCycle]);

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
	VMManager::SetPaused(true);
	recExitExecution();
}

void dynarecMemcheck()
{
	u32 pc = cpuRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_EE, pc) != 0)
		return;

	CBreakPoints::SetBreakpointTriggered(true);
	VMManager::SetPaused(true);
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

	auto checks = CBreakPoints::GetMemChecks(BREAKPOINT_EE);
	for (size_t i = 0; i < checks.size(); i++)
	{
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
			// Preserve ecx (address) and edx (address+size) because we aren't breaking
			// out of this loops iteration and dynarecMemLogcheck will clobber them
			// Also keep 16 byte stack alignment
			if(!(checks[i].result & MEMCHECK_BREAK))
			{
				xPUSH(eax); xPUSH(ebx); xPUSH(ecx); xPUSH(edx);
				xFastCall((void*)dynarecMemLogcheck, ecx, edx);
				xPOP(edx); xPOP(ecx); xPOP(ebx); xPOP(eax);
			}
			else
			{
				xFastCall((void*)dynarecMemLogcheck, ecx, edx);
			}
		}
		if (checks[i].result & MEMCHECK_BREAK)
		{
			// Don't need to preserve edx and ecx, we don't return
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

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot)
{
	u32 i;
	int count;

	if (EmuConfig.EnablePatches)
		Patch::ApplyDynamicPatches(pc);

	// add breakpoint
	if (!delayslot)
	{
		encodeBreakpoint();
		encodeMemcheck();
	}
	else
	{
#ifdef DUMP_BLOCKS
		std::string disasm;
		disR5900Fasm(disasm, *(u32*)PSM(pc), pc, false);
		fprintf(stderr, "Compiling delay slot %08X %s\n", pc, disasm.c_str());
#endif

		_clearNeededX86regs();
		_clearNeededXMMregs();
	}

	s_pCode = (int*)PSM(pc);
	pxAssert(s_pCode);

#if 0
	// acts as a tag for delimiting recompiled instructions when viewing x86 disasm.
	if (IsDevBuild)
		xNOP();
	if (IsDebugBuild)
		xMOV(eax, pc);
#endif

	const int old_code = cpuRegs.code;
	EEINST* old_inst_info = g_pCurInstInfo;

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

	// pc might be past s_nEndBlock if the last instruction in the block is a DI.
	if (pc <= s_nEndBlock)
	{
		for (i = 0; i < iREGCNT_GPR; ++i)
		{
			if (x86regs[i].inuse)
			{
				count = _recIsRegReadOrWritten(g_pCurInstInfo, (s_nEndBlock - pc) / 4 + 1, x86regs[i].type, x86regs[i].reg);
				if (count > 0)
					x86regs[i].counter = 1000 - count;
				else
					x86regs[i].counter = 0;
			}
		}

		for (i = 0; i < iREGCNT_XMM; ++i)
		{
			if (xmmregs[i].inuse)
			{
				count = _recIsRegReadOrWritten(g_pCurInstInfo, (s_nEndBlock - pc) / 4 + 1, xmmregs[i].type, xmmregs[i].reg);
				if (count > 0)
					xmmregs[i].counter = 1000 - count;
				else
					xmmregs[i].counter = 0;
			}
		}
	}

	if (g_pCurInstInfo->info & EEINST_COP2_FLUSH_VU0_REGISTERS)
	{
		RALOG("Flushing cop2 registers\n");
		_flushCOP2regs();
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
			case 0:
				switch (_Funct_)
				{
					case 8: // jr
					case 9: // jalr
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
		opcode.recompile();
	}

	if (!swapped_delay_slot)
	{
		_clearNeededX86regs();
		_clearNeededXMMregs();
	}
	_validateRegs();

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
					Console.Warning("Possible incorrect Q value used in COP2. If the game is broken, please report to http://github.com/pcsx2/pcsx2.");
					for (u32 i = s_pCurBlockEx->startpc; i < s_nEndBlock; i += 4)
					{
						std::string disasm = "";
						disR5900Fasm(disasm, memRead32(i), i, false);
						Console.Warning("%x %s%08X %s", i, i == pc - 4 ? "*" : i == p ? "=" :
                                                                                        " ",
							memRead32(i), disasm.c_str());
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
					if ((_Rd_ == 16 && s & 1) || (_Rd_ == 17 && s & 2) || (_Rd_ == 18 && s & 4))
					{
						std::string disasm;
						Console.Warning("Possible old value used in COP2 code. If the game is broken, please report to http://github.com/pcsx2/pcsx2.");
						for (u32 i = s_pCurBlockEx->startpc; i < s_nEndBlock; i += 4)
						{
							disasm = "";
							disR5900Fasm(disasm, memRead32(i), i, false);
							Console.Warning("%x %s%08X %s", i, i == pc - 4 ? "*" : i == p ? "=" :
                                                                                            " ",
								memRead32(i), disasm.c_str());
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

	if (swapped_delay_slot)
	{
		cpuRegs.code = old_code;
		g_pCurInstInfo = old_inst_info;
	}
}

// (Called from recompiled code)]
// This function is called from the recompiler prior to starting execution of *every* recompiled block.
// Calling of this function can be enabled or disabled through the use of EmuConfig.Recompiler.PreBlockChecks
#ifdef TRACE_BLOCKS
static void PreBlockCheck(u32 blockpc)
{
#if 0
	static FILE* fp = nullptr;
	static bool fp_opened = false;
	if (!fp_opened && cpuRegs.cycle >= 0)
	{
		fp = std::fopen("C:\\Dumps\\comp\\reglog.txt", "wb");
		fp_opened = true;
	}
	if (fp)
	{
		u32 hash = crc32(0, (Bytef*)&cpuRegs, offsetof(cpuRegisters, pc));
		u32 hashf = crc32(0, (Bytef*)&fpuRegs, sizeof(fpuRegisters));
		u32 hashi = crc32(0, (Bytef*)&VU0, offsetof(VURegs, idx));

#if 1
		std::fprintf(fp, "%08X (%u; %08X; %08X; %08X):", cpuRegs.pc, cpuRegs.cycle, hash, hashf, hashi);
		for (int i = 0; i < 34; i++)
		{
			std::fprintf(fp, " %s: %08X%08X%08X%08X", R3000A::disRNameGPR[i], cpuRegs.GPR.r[i].UL[3], cpuRegs.GPR.r[i].UL[2], cpuRegs.GPR.r[i].UL[1], cpuRegs.GPR.r[i].UL[0]);
		}
#if 1
		std::fprintf(fp, "\nFPR: CR: %08X ACC: %08X", fpuRegs.fprc[31], fpuRegs.ACC.UL);
		for (int i = 0; i < 32; i++)
			std::fprintf(fp, " %08X", fpuRegs.fpr[i].UL);
#endif
#if 1
		std::fprintf(fp, "\nVF: ");
		for (int i = 0; i < 32; i++)
			std::fprintf(fp, " %u: %08X %08X %08X %08X", i, VU0.VF[i].UL[0], VU0.VF[i].UL[1], VU0.VF[i].UL[2], VU0.VF[i].UL[3]);
		std::fprintf(fp, "\nVI: ");
		for (int i = 0; i < 32; i++)
			std::fprintf(fp, " %u: %08X", i, VU0.VI[i].UL);
		std::fprintf(fp, "\nACC: %08X %08X %08X %08X Q: %08X P: %08X", VU0.ACC.UL[0], VU0.ACC.UL[1], VU0.ACC.UL[2], VU0.ACC.UL[3], VU0.q.UL, VU0.p.UL);
		std::fprintf(fp, " MAC %08X %08X %08X %08X", VU0.micro_macflags[3], VU0.micro_macflags[2], VU0.micro_macflags[1], VU0.micro_macflags[0]);
		std::fprintf(fp, " CLIP %08X %08X %08X %08X", VU0.micro_clipflags[3], VU0.micro_clipflags[2], VU0.micro_clipflags[1], VU0.micro_clipflags[0]);
		std::fprintf(fp, " STATUS %08X %08X %08X %08X", VU0.micro_statusflags[3], VU0.micro_statusflags[2], VU0.micro_statusflags[1], VU0.micro_statusflags[0]);
#endif
		std::fprintf(fp, "\n");
#else
		std::fprintf(fp, "%08X (%u): %08X %08X %08X\n", cpuRegs.pc, cpuRegs.cycle, hash, hashf, hashi);
#endif
		// std::fflush(fp);
	}
#endif
#if 0
	if (cpuRegs.cycle == 0)
		pauseAAA();
#endif
}
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
	u32 inpage_sz = size * 4;

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
static bool skipMPEG_By_Pattern(u32 sPC)
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

static bool recSkipTimeoutLoop(s32 reg, bool is_timeout_loop)
{
	if (!EmuConfig.Speedhacks.WaitLoop || !is_timeout_loop)
		return false;

	DevCon.WriteLn("[EE] Skipping timeout loop at 0x%08X -> 0x%08X", s_pCurBlockEx->startpc, s_nEndBlock);

	// basically, if the time it takes the loop to run is shorter than the
	// time to the next event, then we want to skip ahead to the event, but
	// update v0 to reflect how long the loop would have run for.

	// if (cycle >= nextEventCycle) { jump to dispatcher, we're running late }
	// new_cycles = min(v0 * 8, nextEventCycle)
	// new_v0 = (new_cycles - cycles) / 8
	// if new_v0 > 0 { jump to dispatcher because loop exited early }
	// else new_v0 is 0, so exit loop

	xMOV(ebx, ptr32[&cpuRegs.cycle]); // ebx = cycle
	xMOV(ecx, ptr32[&cpuRegs.nextEventCycle]); // ecx = nextEventCycle
	xCMP(ebx, ecx);
	//xJAE((void*)DispatcherEvent); // jump to dispatcher if event immediately

	// TODO: In the case where nextEventCycle < cycle because it's overflowed, tack 8
	// cycles onto the event count, so hopefully it'll wrap around. This is pretty
	// gross, but until we switch to 64-bit counters, not many better options.
	xForwardJB8 not_dispatcher;
	xADD(ebx, 8);
	xMOV(ptr32[&cpuRegs.cycle], ebx);
	xJMP((void*)DispatcherEvent);
	not_dispatcher.SetTarget();

	xMOV(edx, ptr32[&cpuRegs.GPR.r[reg].UL[0]]); // eax = v0
	xLEA(rax, ptrNative[rdx * 8 + rbx]); // edx = v0 * 8 + cycle
	xCMP(rcx, rax);
	xCMOVB(rax, rcx); // eax = new_cycles = min(v8 * 8, nextEventCycle)
	xMOV(ptr32[&cpuRegs.cycle], eax); // writeback new_cycles
	xSUB(eax, ebx); // new_cycles -= cycle
	xSHR(eax, 3); // compute new v0 value
	xSUB(edx, eax); // v0 -= cycle_diff
	xMOV(ptr32[&cpuRegs.GPR.r[reg].UL[0]], edx); // write back new value of v0
	xJNZ((void*)DispatcherEvent); // jump to dispatcher if new v0 is not zero (i.e. an event)
	xMOV(ptr32[&cpuRegs.pc], s_nEndBlock); // otherwise end of loop
	recBlocks.Link(HWADDR(s_nEndBlock), xJcc32());

	g_branch = 1;
	pc = s_nEndBlock;

	return true;
}

static void recRecompile(const u32 startpc)
{
	u32 i = 0;
	u32 willbranch3 = 0;

	pxAssert(startpc);

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= (recMem->GetPtrEnd() - _64kb))
		eeRecNeedsReset = true;

	if (HWADDR(startpc) == VMManager::Internal::GetCurrentELFEntryPoint())
		VMManager::Internal::EntryPointCompilingOnCPUThread();

	if (eeRecNeedsReset)
	{
		eeRecNeedsReset = false;
		recResetRaw();
	}

	xSetPtr(recPtr);
	recPtr = xGetAlignedCallTarget();

	s_pCurBlock = PC_GETBLOCK(startpc);

	pxAssert(s_pCurBlock->GetFnptr() == (uptr)JITCompile || s_pCurBlock->GetFnptr() == (uptr)JITCompileInBlock);

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
		if (VMManager::Internal::IsFastBootInProgress())
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
	}

	if (g_eeloadExec && HWADDR(startpc) == HWADDR(g_eeloadExec))
		xFastCall((void*)eeloadHook2);

	g_branch = 0;

	// reset recomp state variables
	s_nBlockCycles = 0;
	s_nBlockInterlocked = false;
	pc = startpc;
	g_cpuHasConstReg = g_cpuFlushedConstReg = 1;
	pxAssert(g_cpuConstRegs[0].UD[0] == 0);

	_initX86regs();
	_initXMMregs();

#ifdef TRACE_BLOCKS
	xFastCall((void*)PreBlockCheck, pc);
#endif

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

	// Timeout loop speedhack.
	// God of War 2 and other games (e.g. NFS series) have these timeout loops which just spin for a few thousand
	// iterations, usually after kicking something which results in an IRQ, but instead of cancelling the loop,
	// they just let it finish anyway. Such loops look like:
	//
	//   00186D6C addiu  v0,v0, -0x1
	//   00186D70 nop
	//   00186D74 nop
	//   00186D78 nop
	//   00186D7C nop
	//   00186D80 bne    v0, zero, ->$0x00186D6C
	//   00186D84 nop
	//
	// Skipping them entirely seems to have no negative effects, but we skip cycles based on the incoming value
	// if the register being decremented, which appears to vary. So far I haven't seen any which increment instead
	// of decrementing, so we'll limit the test to that to be safe.
	//
	s32 timeout_reg = -1;
	bool is_timeout_loop = true;

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

		if (is_timeout_loop)
		{
			if ((cpuRegs.code >> 26) == 8 || (cpuRegs.code >> 26) == 9)
			{
				// addi/addiu
				if (timeout_reg >= 0 || _Rs_ != _Rt_ || _Imm_ >= 0)
					is_timeout_loop = false;
				else
					timeout_reg = _Rs_;
			}
			else if ((cpuRegs.code >> 26) == 5)
			{
				// bne
				if (timeout_reg != _Rs_ || _Rt_ != 0 || memRead32(i + 4) != 0)
					is_timeout_loop = false;
			}
			else if (cpuRegs.code != 0)
			{
				is_timeout_loop = false;
			}
		}

		switch (cpuRegs.code >> 26)
		{
			case 0: // special
				if (_Funct_ == 8 || _Funct_ == 9) // JR, JALR
				{
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				else if (_Funct_ == 12 || _Funct_ == 13) // SYSCALL, BREAK
				{
					s_nEndBlock = i + 4; // No delay slot.
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
				s_branchTo = (_InstrucTarget_ << 2) | ((i + 4) & 0xf0000000);
				s_nEndBlock = i + 8;
				goto StartRecomp;

			// branches
			case 4:
			case 5:
			case 6:
			case 7:
			case 20:
			case 21:
			case 22:
			case 23:
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
		is_timeout_loop = false;
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
			recBackpropBSC(cpuRegs.code, pcur - 1, pcur);
			pcur--;

			has_cop2_instructions |= (_Opcode_ == 022 || _Opcode_ == 066 || _Opcode_ == 076);
		}
	}

	// eventually we'll want to have a vector of passes or something.
	if (has_cop2_instructions)
	{
		COP2MicroFinishPass().Run(startpc, s_nEndBlock, s_pInstCache + 1);

		if (EmuConfig.Speedhacks.vuFlagHack)
			COP2FlagHackPass().Run(startpc, s_nEndBlock, s_pInstCache + 1);
	}

#ifdef DUMP_BLOCKS
	ZydisDecoder disas_decoder;
	ZydisDecoderInit(&disas_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);

	ZydisFormatter disas_formatter;
	ZydisFormatterInit(&disas_formatter, ZYDIS_FORMATTER_STYLE_INTEL);

	s_old_print_address = (ZydisFormatterFunc)&ZydisFormatterPrintAddressAbsolute;
	ZydisFormatterSetHook(&disas_formatter, ZYDIS_FORMATTER_FUNC_PRINT_ADDRESS_ABS, (const void**)&s_old_print_address);

	ZydisDecodedInstruction disas_instruction;
#if 0
	const bool dump_block = (startpc == 0x00000000);
#elif 1
	const bool dump_block = true;
#else
	const bool dump_block = false;
#endif
#endif

	// Detect and handle self-modified code
	memory_protect_recompiled_code(startpc, (s_nEndBlock - startpc) >> 2);

	// Skip Recompilation if sceMpegIsEnd Pattern detected
	bool doRecompilation = !skipMPEG_By_Pattern(startpc) && !recSkipTimeoutLoop(timeout_reg, is_timeout_loop);

	if (doRecompilation)
	{
		// Finally: Generate x86 recompiled code!
		g_pCurInstInfo = s_pInstCache;
		while (!g_branch && pc < s_nEndBlock)
		{
#ifdef DUMP_BLOCKS
			if (dump_block)
			{
				std::string disasm;
				disR5900Fasm(disasm, *(u32*)PSM(pc), pc, false);
				fprintf(stderr, "Compiling %08X %s\n", pc, disasm.c_str());

				const u8* instStart = x86Ptr;
				recompileNextInstruction(false, false);

				const u8* instPtr = instStart;
				ZyanUSize instLength = static_cast<ZyanUSize>(x86Ptr - instStart);
				while (ZYAN_SUCCESS(ZydisDecoderDecodeBuffer(&disas_decoder, instPtr, instLength, &disas_instruction)))
				{
					char buffer[256];
					if (ZYAN_SUCCESS(ZydisFormatterFormatInstruction(&disas_formatter, &disas_instruction, buffer, sizeof(buffer), (ZyanU64)instPtr)))
						std::fprintf(stderr, "    %016" PRIX64 "    %s\n", (u64)instPtr, buffer);

					instPtr += disas_instruction.length;
					instLength -= disas_instruction.length;
				}
			}
			else
			{
				recompileNextInstruction(false, false);
			}
#else
			recompileNextInstruction(false, false); // For the love of recursion, batman!
#endif
		}
	}

	pxAssert((pc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (pc - startpc) >> 2;

	if (HWADDR(pc) <= Ps2MemSize::MainRam)
	{
		BASEBLOCKEX* oldBlock;
		int i;

		i = recBlocks.LastIndex(HWADDR(pc) - 4);
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

	s_pCurBlockEx->x86size = static_cast<u32>(xGetPtr() - recPtr);

#if 0
	// Example: Dump both x86/EE code
	if (startpc == 0x456630) {
		iDumpBlock(s_pCurBlockEx->startpc, s_pCurBlockEx->size*4, s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size);
	}
#endif
	Perf::ee.RegisterPC((void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = xGetPtr();

	pxAssert((g_cpuHasConstReg & g_cpuFlushedConstReg) == g_cpuHasConstReg);

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
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
