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

// recompiler reworked to add dynamic linking Jan06
// and added reg caching, const propagation, block analysis Jun06
// zerofrog(@gmail.com)


#include "PrecompiledHeader.h"

#include "iR3000A.h"
#include "R3000A.h"
#include "BaseblockEx.h"
#include "System/RecTypes.h"
#include "R5900OpcodeTables.h"
#include "IopBios.h"
#include "IopHw.h"
#include "Common.h"

#include <time.h>

#ifndef _WIN32
#include <sys/types.h>
#endif

#include "iCore.h"

#include "Config.h"

#include "common/AlignedMalloc.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/Perf.h"
#include "DebugTools/Breakpoints.h"

#ifndef PCSX2_CORE
#include "gui/SysThreads.h"
#endif

#include "fmt/core.h"

using namespace x86Emitter;

extern u32 g_iopNextEventCycle;
extern void psxBREAK();

u32 g_psxMaxRecMem = 0;
u32 s_psxrecblocks[] = {0};

uptr psxRecLUT[0x10000];
u32 psxhwLUT[0x10000];

static __fi u32 HWADDR(u32 mem) { return psxhwLUT[mem >> 16] + mem; }

static RecompiledCodeReserve* recMem = NULL;

static BASEBLOCK* recRAM  = NULL; // and the ptr to the blocks here
static BASEBLOCK* recROM  = NULL; // and here
static BASEBLOCK* recROM1 = NULL; // also here
static BASEBLOCK* recROM2 = NULL; // also here
static BaseBlocks recBlocks;
static u8* recPtr = NULL;
u32 psxpc; // recompiler psxpc
int psxbranch; // set for branch
u32 g_iopCyclePenalty;

static EEINST* s_pInstCache = NULL;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = NULL;
static BASEBLOCKEX* s_pCurBlockEx = NULL;

static u32 s_nEndBlock = 0; // what psxpc the current block ends
static u32 s_branchTo;
static bool s_nBlockFF;

static u32 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static EEINST* s_psaveInstInfo = NULL;

u32 s_psxBlockCycles = 0; // cycles of current block recompiling
static u32 s_savenBlockCycles = 0;

static void iPsxBranchTest(u32 newpc, u32 cpuBranch);
void psxRecompileNextInstruction(int delayslot);

extern void (*rpsxBSC[64])();
void rpsxpropBSC(EEINST* prev, EEINST* pinst);

static void iopClearRecLUT(BASEBLOCK* base, int count);

static u32 psxdump = 0;

#define PSX_GETBLOCK(x) PC_GETBLOCK_(x, psxRecLUT)

#define PSXREC_CLEARM(mem) \
	(((mem) < g_psxMaxRecMem && (psxRecLUT[(mem) >> 16] + (mem))) ? \
		psxRecClearMem(mem) : 4)

// =====================================================================================================
//  Dynamically Compiled Dispatchers - R3000A style
// =====================================================================================================

static void iopRecRecompile(const u32 startpc);

// Recompiled code buffer for EE recompiler dispatchers!
alignas(__pagesize) static u8 iopRecDispatchers[__pagesize];

typedef void DynGenFunc();

static DynGenFunc* iopDispatcherEvent     = NULL;
static DynGenFunc* iopDispatcherReg       = NULL;
static DynGenFunc* iopJITCompile          = NULL;
static DynGenFunc* iopJITCompileInBlock   = NULL;
static DynGenFunc* iopEnterRecompiledCode = NULL;
static DynGenFunc* iopExitRecompiledCode  = NULL;

static void recEventTest()
{
	_cpuEventTest_Shared();
}

// The address for all cleared blocks.  It recompiles the current pc and then
// dispatches to the recompiled block address.
static DynGenFunc* _DynGen_JITCompile()
{
	pxAssertMsg(iopDispatcherReg != NULL, "Please compile the DispatcherReg subroutine *before* JITComple.  Thanks.");

	u8* retval = xGetPtr();

	xFastCall((void*)iopRecRecompile, ptr32[&psxRegs.pc]);

	xMOV(eax, ptr[&psxRegs.pc]);
	xMOV(ebx, eax);
	xSHR(eax, 16);
	xMOV(rcx, ptrNative[xComplexAddress(rcx, psxRecLUT, rax * wordsize)]);
	xJMP(ptrNative[rbx * (wordsize / 4) + rcx]);

	return (DynGenFunc*)retval;
}

static DynGenFunc* _DynGen_JITCompileInBlock()
{
	u8* retval = xGetPtr();
	xJMP((void*)iopJITCompile);
	return (DynGenFunc*)retval;
}

// called when jumping to variable pc address
static DynGenFunc* _DynGen_DispatcherReg()
{
	u8* retval = xGetPtr();

	xMOV(eax, ptr[&psxRegs.pc]);
	xMOV(ebx, eax);
	xSHR(eax, 16);
	xMOV(rcx, ptrNative[xComplexAddress(rcx, psxRecLUT, rax * wordsize)]);
	xJMP(ptrNative[rbx * (wordsize / 4) + rcx]);

	return (DynGenFunc*)retval;
}

// --------------------------------------------------------------------------------------
//  EnterRecompiledCode  - dynamic compilation stub!
// --------------------------------------------------------------------------------------
static DynGenFunc* _DynGen_EnterRecompiledCode()
{
	// Optimization: The IOP never uses stack-based parameter invocation, so we can avoid
	// allocating any room on the stack for it (which is important since the IOP's entry
	// code gets invoked quite a lot).

	u8* retval = xGetPtr();

	{ // Properly scope the frame prologue/epilogue
#ifdef ENABLE_VTUNE
		xScopedStackFrame frame(true);
#else
		xScopedStackFrame frame(IsDevBuild);
#endif

		xJMP((void*)iopDispatcherReg);

		// Save an exit point
		iopExitRecompiledCode = (DynGenFunc*)xGetPtr();
	}

	xRET();

	return (DynGenFunc*)retval;
}

static void _DynGen_Dispatchers()
{
	// In case init gets called multiple times:
	HostSys::MemProtectStatic(iopRecDispatchers, PageAccess_ReadWrite());

	// clear the buffer to 0xcc (easier debugging).
	memset(iopRecDispatchers, 0xcc, __pagesize);

	xSetPtr(iopRecDispatchers);

	// Place the EventTest and DispatcherReg stuff at the top, because they get called the
	// most and stand to benefit from strong alignment and direct referencing.
	iopDispatcherEvent = (DynGenFunc*)xGetPtr();
	xFastCall((void*)recEventTest);
	iopDispatcherReg       = _DynGen_DispatcherReg();

	iopJITCompile          = _DynGen_JITCompile();
	iopJITCompileInBlock   = _DynGen_JITCompileInBlock();
	iopEnterRecompiledCode = _DynGen_EnterRecompiledCode();

	HostSys::MemProtectStatic(iopRecDispatchers, PageAccess_ExecOnly());

	recBlocks.SetJITCompile(iopJITCompile);

	Perf::any.map((uptr)&iopRecDispatchers, 4096, "IOP Dispatcher");
}

////////////////////////////////////////////////////
using namespace R3000A;

static void iIopDumpBlock(int startpc, u8* ptr)
{
	u32 i, j;
	EEINST* pcur;
	u8 used[34];
	int numused, count;

	Console.WriteLn("dump1 %x:%x, %x", startpc, psxpc, psxRegs.cycle);
	FileSystem::CreateDirectoryPath(EmuFolders::Logs.c_str(), false);

	std::string filename(Path::Combine(EmuFolders::Logs, fmt::format("psxdump{:.8X}.txt", startpc)));
	std::FILE* f = FileSystem::OpenCFile(filename.c_str(), "w");
	if (!f)
		return;

	std::fprintf(f, "Dump PSX register data: 0x%p\n\n", &psxRegs);

	for (i = startpc; i < s_nEndBlock; i += 4)
	{
		std::fprintf(f, "%s\n", disR3000AF(iopMemRead32(i), i));
	}

	// write the instruction info
	std::fprintf(f, "\n\nlive0 - %x, lastuse - %x used - %x\n", EEINST_LIVE0, EEINST_LASTUSE, EEINST_USED);

	memzero(used);
	numused = 0;
	for (i = 0; i < std::size(s_pInstCache->regs); ++i)
	{
		if (s_pInstCache->regs[i] & EEINST_USED)
		{
			used[i] = 1;
			numused++;
		}
	}

	std::fprintf(f, "       ");
	for (i = 0; i < std::size(s_pInstCache->regs); ++i)
	{
		if (used[i])
			std::fprintf(f, "%2d ", i);
	}
	std::fprintf(f, "\n");

	std::fprintf(f, "       ");
	for (i = 0; i < std::size(s_pInstCache->regs); ++i)
	{
		if (used[i])
			std::fprintf(f, "%s ", disRNameGPR[i]);
	}
	std::fprintf(f, "\n");

	pcur = s_pInstCache + 1;
	for (i = 0; i < (s_nEndBlock - startpc) / 4; ++i, ++pcur)
	{
		std::fprintf(f, "%2d: %2.2x ", i + 1, pcur->info);

		count = 1;
		for (j = 0; j < std::size(s_pInstCache->regs); j++)
		{
			if (used[j])
			{
				std::fprintf(f, "%2.2x%s", pcur->regs[j], ((count % 8) && count < numused) ? "_" : " ");
				++count;
			}
		}
		std::fprintf(f, "\n");
	}
	std::fclose(f);

#ifdef __linux__
	// dump the asm
	{
		f = std::fopen("mydump1", "wb");
		if (!f)
			return;

		std::fwrite(ptr, (uptr)x86Ptr - (uptr)ptr, 1, f);
		std::fclose(f);
	}

	int status = std::system(fmt::format("objdump -D -b binary -mi386 -M intel --no-show-raw-insn {} >> {}; rm {}",
		"mydump1", filename.c_str(), "mydump1").c_str());

	if (!WIFEXITED(status))
		Console.Error("IOP dump didn't terminate normally");
#endif
}

u8 _psxLoadWritesRs(u32 tempcode)
{
	switch (tempcode >> 26)
	{
		case 32: case 33: case 34: case 35: case 36: case 37: case 38:
			return ((tempcode >> 21) & 0x1f) == ((tempcode >> 16) & 0x1f); // rs==rt
	}
	return 0;
}

u8 _psxIsLoadStore(u32 tempcode)
{
	switch (tempcode >> 26)
	{
		case 32: case 33: case 34: case 35: case 36: case 37: case 38:
		// 4 byte stores
		case 40: case 41: case 42: case 43: case 46:
			return 1;
	}
	return 0;
}

void _psxFlushAllUnused()
{
	int i;
	for (i = 0; i < 34; ++i)
	{
		if (psxpc < s_nEndBlock)
		{
			if ((g_pCurInstInfo[1].regs[i] & EEINST_USED))
				continue;
		}
		else if ((g_pCurInstInfo[0].regs[i] & EEINST_USED))
		{
			continue;
		}

		if (i < 32 && PSX_IS_CONST1(i))
		{
			_psxFlushConstReg(i);
		}
		else
		{
			_deleteX86reg(X86TYPE_PSX, i, 1);
		}
	}
}

int _psxFlushUnusedConstReg()
{
	int i;
	for (i = 1; i < 32; ++i)
	{
		if ((g_psxHasConstReg & (1 << i)) && !(g_psxFlushedConstReg & (1 << i)) &&
			!_recIsRegWritten(g_pCurInstInfo + 1, (s_nEndBlock - psxpc) / 4, XMMTYPE_GPRREG, i))
		{

			// check if will be written in the future
			xMOV(ptr32[&psxRegs.GPR.r[i]], g_psxConstRegs[i]);
			g_psxFlushedConstReg |= 1 << i;
			return 1;
		}
	}

	return 0;
}

void _psxFlushCachedRegs()
{
	_psxFlushConstRegs();
}

void _psxFlushConstReg(int reg)
{
	if (PSX_IS_CONST1(reg) && !(g_psxFlushedConstReg & (1 << reg)))
	{
		xMOV(ptr32[&psxRegs.GPR.r[reg]], g_psxConstRegs[reg]);
		g_psxFlushedConstReg |= (1 << reg);
	}
}

void _psxFlushConstRegs()
{
	int i;

	// flush constants

	// ignore r0
	for (i = 1; i < 32; ++i)
	{
		if (g_psxHasConstReg & (1 << i))
		{

			if (!(g_psxFlushedConstReg & (1 << i)))
			{
				xMOV(ptr32[&psxRegs.GPR.r[i]], g_psxConstRegs[i]);
				g_psxFlushedConstReg |= 1 << i;
			}

			if (g_psxHasConstReg == g_psxFlushedConstReg)
				break;
		}
	}
}

void _psxDeleteReg(int reg, int flush)
{
	if (!reg)
		return;
	if (flush && PSX_IS_CONST1(reg))
	{
		_psxFlushConstReg(reg);
		return;
	}
	PSX_DEL_CONST(reg);
	_deleteX86reg(X86TYPE_PSX, reg, flush ? 0 : 2);
}

void _psxMoveGPRtoR(const xRegister32& to, int fromgpr)
{
	if (PSX_IS_CONST1(fromgpr))
		xMOV(to, g_psxConstRegs[fromgpr]);
	else
	{
		// check x86
		xMOV(to, ptr[&psxRegs.GPR.r[fromgpr]]);
	}
}

#if 0
void _psxMoveGPRtoM(uptr to, int fromgpr)
{
	if( PSX_IS_CONST1(fromgpr) )
		xMOV(ptr32[(u32*)(to)], g_psxConstRegs[fromgpr] );
	else {
		// check x86
		xMOV(eax, ptr[&psxRegs.GPR.r[ fromgpr ] ]);
		xMOV(ptr[(void*)(to)], eax);
	}
}
#endif

#if 0
void _psxMoveGPRtoRm(x86IntRegType to, int fromgpr)
{
	if( PSX_IS_CONST1(fromgpr) )
		xMOV(ptr32[xAddressReg(to)], g_psxConstRegs[fromgpr] );
	else {
		// check x86
		xMOV(eax, ptr[&psxRegs.GPR.r[ fromgpr ] ]);
		xMOV(ptr[xAddressReg(to)], eax);
	}
}
#endif

void _psxFlushCall(int flushtype)
{
	// x86-32 ABI : These registers are not preserved across calls:
	_freeX86reg(eax);
	_freeX86reg(ecx);
	_freeX86reg(edx);

	if ((flushtype & FLUSH_PC) /*&& !g_cpuFlushedPC*/)
	{
		xMOV(ptr32[&psxRegs.pc], psxpc);
		//g_cpuFlushedPC = true;
	}

	if (flushtype & FLUSH_CACHED_REGS)
		_psxFlushConstRegs();
}

void psxSaveBranchState()
{
	s_savenBlockCycles = s_psxBlockCycles;
	memcpy(s_saveConstRegs, g_psxConstRegs, sizeof(g_psxConstRegs));
	s_saveHasConstReg = g_psxHasConstReg;
	s_saveFlushedConstReg = g_psxFlushedConstReg;
	s_psaveInstInfo = g_pCurInstInfo;

	// save all regs
	memcpy(s_saveX86regs, x86regs, sizeof(x86regs));
}

void psxLoadBranchState()
{
	s_psxBlockCycles = s_savenBlockCycles;

	memcpy(g_psxConstRegs, s_saveConstRegs, sizeof(g_psxConstRegs));
	g_psxHasConstReg = s_saveHasConstReg;
	g_psxFlushedConstReg = s_saveFlushedConstReg;
	g_pCurInstInfo = s_psaveInstInfo;

	// restore all regs
	memcpy(x86regs, s_saveX86regs, sizeof(x86regs));
}

////////////////////
// Code Templates //
////////////////////

void _psxOnWriteReg(int reg)
{
	PSX_DEL_CONST(reg);
}

// rd = rs op rt
void psxRecompileCodeConst0(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode)
{
	if (!_Rd_)
		return;

	// for now, don't support xmm

	_deleteX86reg(X86TYPE_PSX, _Rs_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rt_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rd_, 0);

	if (PSX_IS_CONST2(_Rs_, _Rt_))
	{
		PSX_SET_CONST(_Rd_);
		constcode();
		return;
	}

	if (PSX_IS_CONST1(_Rs_))
	{
		constscode(0);
		PSX_DEL_CONST(_Rd_);
		return;
	}

	if (PSX_IS_CONST1(_Rt_))
	{
		consttcode(0);
		PSX_DEL_CONST(_Rd_);
		return;
	}

	noconstcode(0);
	PSX_DEL_CONST(_Rd_);
}

static void psxRecompileIrxImport()
{
	u32 import_table = irxImportTableAddr(psxpc - 4);
	u16 index = psxRegs.code & 0xffff;
	if (!import_table)
		return;

	const std::string libname = iopMemReadString(import_table + 12, 8);

	irxHLE hle = irxImportHLE(libname, index);
#ifdef PCSX2_DEVBUILD
	const irxDEBUG debug = irxImportDebug(libname, index);
	const char* funcname = irxImportFuncname(libname, index);
#else
	const irxDEBUG debug = 0;
	const char* funcname = nullptr;
#endif

	if (!hle && !debug && (!SysTraceActive(IOP.Bios) || !funcname))
		return;

	xMOV(ptr32[&psxRegs.code], psxRegs.code);
	xMOV(ptr32[&psxRegs.pc], psxpc);
	_psxFlushCall(FLUSH_NODESTROY);

	if (SysTraceActive(IOP.Bios))
	{
		xMOV64(arg3reg, (uptr)funcname);

		xFastCall((void*)irxImportLog_rec, import_table, index);
	}

	if (debug)
		xFastCall((void*)debug);

	if (hle)
	{
		xFastCall((void*)hle);
		xTEST(eax, eax);
		xJNZ(iopDispatcherReg);
	}
}

// rt = rs op imm16
void psxRecompileCodeConst1(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode)
{
	if (!_Rt_)
	{
		// check for iop module import table magic
		if (psxRegs.code >> 16 == 0x2400)
			psxRecompileIrxImport();
		return;
	}

	// for now, don't support xmm

	_deleteX86reg(X86TYPE_PSX, _Rs_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rt_, 0);

	if (PSX_IS_CONST1(_Rs_))
	{
		PSX_SET_CONST(_Rt_);
		constcode();
		return;
	}

	noconstcode(0);
	PSX_DEL_CONST(_Rt_);
}

// rd = rt op sa
void psxRecompileCodeConst2(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode)
{
	if (!_Rd_)
		return;

	// for now, don't support xmm

	_deleteX86reg(X86TYPE_PSX, _Rt_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rd_, 0);

	if (PSX_IS_CONST1(_Rt_))
	{
		PSX_SET_CONST(_Rd_);
		constcode();
		return;
	}

	noconstcode(0);
	PSX_DEL_CONST(_Rd_);
}

// rd = rt MULT rs  (SPECIAL)
void psxRecompileCodeConst3(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int LOHI)
{
	_deleteX86reg(X86TYPE_PSX, _Rs_, 1);
	_deleteX86reg(X86TYPE_PSX, _Rt_, 1);

	if (LOHI)
	{
		_deleteX86reg(X86TYPE_PSX, PSX_HI, 1);
		_deleteX86reg(X86TYPE_PSX, PSX_LO, 1);
	}

	if (PSX_IS_CONST2(_Rs_, _Rt_))
	{
		constcode();
		return;
	}

	if (PSX_IS_CONST1(_Rs_))
	{
		constscode(0);
		return;
	}

	if (PSX_IS_CONST1(_Rt_))
	{
		consttcode(0);
		return;
	}

	noconstcode(0);
}

static uptr m_ConfiguredCacheReserve = 32;
static u8* m_recBlockAlloc = NULL;

static const uint m_recBlockAllocSize =
	(((Ps2MemSize::IopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4) * sizeof(BASEBLOCK));

static void recReserveCache()
{
	if (!recMem)
		recMem = new RecompiledCodeReserve("R3000A Recompiler Cache", _8mb);
	recMem->SetProfilerName("IOPrec");

	while (!recMem->IsOk())
	{
		if (recMem->Reserve(GetVmMemory().MainMemory(), HostMemoryMap::IOPrecOffset, m_ConfiguredCacheReserve * _1mb) != NULL)
			break;

		// If it failed, then try again (if possible):
		if (m_ConfiguredCacheReserve < 4)
			break;
		m_ConfiguredCacheReserve /= 2;
	}

	recMem->ThrowIfNotOk();
}

static void recReserve()
{
	// IOP has no hardware requirements!

	recReserveCache();
}

static void recAlloc()
{
	// Goal: Allocate BASEBLOCKs for every possible branch target in IOP memory.
	// Any 4-byte aligned address makes a valid branch target as per MIPS design (all instructions are
	// always 4 bytes long).

	if (m_recBlockAlloc == NULL)
		m_recBlockAlloc = (u8*)_aligned_malloc(m_recBlockAllocSize, 4096);

	if (m_recBlockAlloc == NULL)
		throw Exception::OutOfMemory("R3000A BASEBLOCK lookup tables");

	u8* curpos = m_recBlockAlloc;
	recRAM  = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::IopRam / 4) * sizeof(BASEBLOCK);
	recROM  = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom    / 4) * sizeof(BASEBLOCK);
	recROM1 = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom1   / 4) * sizeof(BASEBLOCK);
	recROM2 = (BASEBLOCK*)curpos; curpos += (Ps2MemSize::Rom2   / 4) * sizeof(BASEBLOCK);


	if (s_pInstCache == NULL)
	{
		s_nInstCacheSize = 128;
		s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
	}

	if (s_pInstCache == NULL)
		throw Exception::OutOfMemory("R3000 InstCache.");

	_DynGen_Dispatchers();
}

void recResetIOP()
{
	DevCon.WriteLn("iR3000A Recompiler reset.");

	Perf::iop.reset();

	recAlloc();
	recMem->Reset();

	iopClearRecLUT((BASEBLOCK*)m_recBlockAlloc,
		(((Ps2MemSize::IopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4)));

	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(psxRecLUT, 0, 0, 0, i, 0);

	// IOP knows 64k pages, hence for the 0x10000's

	// The bottom 2 bits of PC are always zero, so we <<14 to "compress"
	// the pc indexer into it's lower common denominator.

	// We're only mapping 20 pages here in 4 places.
	// 0x80 comes from : (Ps2MemSize::IopRam / 0x10000) * 4

	for (int i = 0; i < 0x80; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x0000, i, i & 0x1f);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x8000, i, i & 0x1f);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0xa000, i, i & 0x1f);
	}

	for (int i = 0x1fc0; i < 0x2000; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM, 0xa000, i, i - 0x1fc0);
	}

	for (int i = 0x1e00; i < 0x1e04; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e48; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recROM2, 0xa000, i, i - 0x1e40);
	}

	if (s_pInstCache)
		memset(s_pInstCache, 0, sizeof(EEINST) * s_nInstCacheSize);

	recBlocks.Reset();
	g_psxMaxRecMem = 0;

	recPtr = *recMem;
	psxbranch = 0;
}

static void recShutdown()
{
	safe_delete(recMem);

	safe_aligned_free(m_recBlockAlloc);

	safe_free(s_pInstCache);
	s_nInstCacheSize = 0;

	// FIXME Warning thread unsafe
	Perf::dump();
}

static void iopClearRecLUT(BASEBLOCK* base, int count)
{
	for (int i = 0; i < count; i++)
		base[i].SetFnptr((uptr)iopJITCompile);
}

static __noinline s32 recExecuteBlock(s32 eeCycles)
{
	iopBreak = 0;
	iopCycleEE = eeCycles;

#ifdef PCSX2_DEVBUILD
	//if (SysTrace.SIF.IsActive())
	//	SysTrace.IOP.R3000A.Write("Switching to IOP CPU for %d cycles", eeCycles);
#endif

	// [TODO] recExecuteBlock could be replaced by a direct call to the iopEnterRecompiledCode()
	//   (by assigning its address to the psxRec structure).  But for that to happen, we need
	//   to move iopBreak/iopCycleEE update code to emitted assembly code. >_<  --air

	// Likely Disasm, as borrowed from MSVC:

	// Entry:
	// 	mov         eax,dword ptr [esp+4]
	// 	mov         dword ptr [iopBreak (0E88DCCh)],0
	// 	mov         dword ptr [iopCycleEE (832A84h)],eax

	// Exit:
	// 	mov         ecx,dword ptr [iopBreak (0E88DCCh)]
	// 	mov         edx,dword ptr [iopCycleEE (832A84h)]
	// 	lea         eax,[edx+ecx]

	iopEnterRecompiledCode();

	return iopBreak + iopCycleEE;
}

// Returns the offset to the next instruction after any cleared memory
static __fi u32 psxRecClearMem(u32 pc)
{
	BASEBLOCK* pblock;

	pblock = PSX_GETBLOCK(pc);
	// if ((u8*)iopJITCompile == pblock->GetFnptr())
	if (pblock->GetFnptr() == (uptr)iopJITCompile)
		return 4;

	pc = HWADDR(pc);

	u32 lowerextent = pc, upperextent = pc + 4;
	int blockidx = recBlocks.Index(pc);
	pxAssert(blockidx != -1);

	while (BASEBLOCKEX* pexblock = recBlocks[blockidx - 1])
	{
		if (pexblock->startpc + pexblock->size * 4 <= lowerextent)
			break;

		lowerextent = std::min(lowerextent, pexblock->startpc);
		blockidx--;
	}

	int toRemoveFirst = blockidx;

	while (BASEBLOCKEX* pexblock = recBlocks[blockidx])
	{
		if (pexblock->startpc >= upperextent)
			break;

		lowerextent = std::min(lowerextent, pexblock->startpc);
		upperextent = std::max(upperextent, pexblock->startpc + pexblock->size * 4);

		blockidx++;
	}

	if (toRemoveFirst != blockidx)
	{
		recBlocks.Remove(toRemoveFirst, (blockidx - 1));
	}

	blockidx = 0;
	while (BASEBLOCKEX* pexblock = recBlocks[blockidx++])
	{
		if (pc >= pexblock->startpc && pc < pexblock->startpc + pexblock->size * 4)
		{
			DevCon.Error("[IOP] Impossible block clearing failure");
			pxFailDev("[IOP] Impossible block clearing failure");
		}
	}

	iopClearRecLUT(PSX_GETBLOCK(lowerextent), (upperextent - lowerextent) / 4);

	return upperextent - pc;
}

static __fi void recClearIOP(u32 Addr, u32 Size)
{
	u32 pc = Addr;
	while (pc < Addr + Size * 4)
		pc += PSXREC_CLEARM(pc);
}

void psxSetBranchReg(u32 reg)
{
	psxbranch = 1;

	if (reg != 0xffffffff)
	{
		_allocX86reg(calleeSavedReg2d, X86TYPE_PCWRITEBACK, 0, MODE_WRITE);
		_psxMoveGPRtoR(calleeSavedReg2d, reg);

		psxRecompileNextInstruction(1);

		if (x86regs[calleeSavedReg2d.GetId()].inuse)
		{
			pxAssert(x86regs[calleeSavedReg2d.GetId()].type == X86TYPE_PCWRITEBACK);
			xMOV(ptr32[&psxRegs.pc], calleeSavedReg2d);
			x86regs[calleeSavedReg2d.GetId()].inuse = 0;
#ifdef PCSX2_DEBUG
			xOR(calleeSavedReg2d, calleeSavedReg2d);
#endif
		}
		else
		{
			xMOV(eax, ptr32[&g_recWriteback]);
			xMOV(ptr32[&psxRegs.pc], eax);

#ifdef PCSX2_DEBUG
			xOR(eax, eax);
#endif
		}

#ifdef PCSX2_DEBUG
		xForwardJNZ8 skipAssert;
		xWrite8(0xcc);
		skipAssert.SetTarget();
#endif
	}

	_psxFlushCall(FLUSH_EVERYTHING);
	iPsxBranchTest(0xffffffff, 1);

	JMP32((uptr)iopDispatcherReg - ((uptr)x86Ptr + 5));
}

void psxSetBranchImm(u32 imm)
{
	psxbranch = 1;
	pxAssert(imm);

	// end the current block
	xMOV(ptr32[&psxRegs.pc], imm);
	_psxFlushCall(FLUSH_EVERYTHING);
	iPsxBranchTest(imm, imm <= psxpc);

	recBlocks.Link(HWADDR(imm), xJcc32());
}

static __fi u32 psxScaleBlockCycles()
{
	return s_psxBlockCycles;
}

static void iPsxBranchTest(u32 newpc, u32 cpuBranch)
{
	u32 blockCycles = psxScaleBlockCycles();

	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && newpc == s_branchTo)
	{
		xMOV(eax, ptr32[&psxRegs.cycle]);
		xMOV(ecx, eax);
		xMOV(edx, ptr32[&iopCycleEE]);
		xADD(edx, 7);
		xSHR(edx, 3);
		xADD(eax, edx);
		xCMP(eax, ptr32[&g_iopNextEventCycle]);
		xCMOVNS(eax, ptr32[&g_iopNextEventCycle]);
		xMOV(ptr32[&psxRegs.cycle], eax);
		xSUB(eax, ecx);
		xSHL(eax, 3);
		xSUB(ptr32[&iopCycleEE], eax);
		xJLE(iopExitRecompiledCode);

		xFastCall((void*)iopEventTest);

		if (newpc != 0xffffffff)
		{
			xCMP(ptr32[&psxRegs.pc], newpc);
			xJNE(iopDispatcherReg);
		}
	}
	else
	{
		xMOV(eax, ptr32[&psxRegs.cycle]);
		xADD(eax, blockCycles);
		xMOV(ptr32[&psxRegs.cycle], eax); // update cycles

		// jump if iopCycleEE <= 0  (iop's timeslice timed out, so time to return control to the EE)
		xSUB(ptr32[&iopCycleEE], blockCycles * 8);
		xJLE(iopExitRecompiledCode);

		// check if an event is pending
		xSUB(eax, ptr32[&g_iopNextEventCycle]);
		xForwardJS<u8> nointerruptpending;

		xFastCall((void*)iopEventTest);

		if (newpc != 0xffffffff)
		{
			xCMP(ptr32[&psxRegs.pc], newpc);
			xJNE(iopDispatcherReg);
		}

		nointerruptpending.SetTarget();
	}
}

#if 0
//static const int *s_pCode;

#if !defined(_MSC_VER)
static void checkcodefn()
{
	int pctemp;

#ifdef _MSC_VER
	__asm mov pctemp, eax;
#else
    __asm__ __volatile__("movl %%eax, %[pctemp]" : [pctemp]"m="(pctemp) );
#endif
	Console.WriteLn("iop code changed! %x", pctemp);
}
#endif
#endif

void rpsxSYSCALL()
{
	xMOV(ptr32[&psxRegs.code], psxRegs.code);
	xMOV(ptr32[&psxRegs.pc], psxpc - 4);
	_psxFlushCall(FLUSH_NODESTROY);

	//xMOV( ecx, 0x20 );			// exception code
	//xMOV( edx, psxbranch==1 );	// branch delay slot?
	xFastCall((void*)psxException, 0x20, psxbranch == 1);

	xCMP(ptr32[&psxRegs.pc], psxpc - 4);
	j8Ptr[0] = JE8(0);

	xADD(ptr32[&psxRegs.cycle], psxScaleBlockCycles());
	xSUB(ptr32[&iopCycleEE], psxScaleBlockCycles() * 8);
	JMP32((uptr)iopDispatcherReg - ((uptr)x86Ptr + 5));

	// jump target for skipping blockCycle updates
	x86SetJ8(j8Ptr[0]);

	//if (!psxbranch) psxbranch = 2;
}

void rpsxBREAK()
{
	xMOV(ptr32[&psxRegs.code], psxRegs.code);
	xMOV(ptr32[&psxRegs.pc], psxpc - 4);
	_psxFlushCall(FLUSH_NODESTROY);

	//xMOV( ecx, 0x24 );			// exception code
	//xMOV( edx, psxbranch==1 );	// branch delay slot?
	xFastCall((void*)psxException, 0x24, psxbranch == 1);

	xCMP(ptr32[&psxRegs.pc], psxpc - 4);
	j8Ptr[0] = JE8(0);
	xADD(ptr32[&psxRegs.cycle], psxScaleBlockCycles());
	xSUB(ptr32[&iopCycleEE], psxScaleBlockCycles() * 8);
	JMP32((uptr)iopDispatcherReg - ((uptr)x86Ptr + 5));
	x86SetJ8(j8Ptr[0]);

	//if (!psxbranch) psxbranch = 2;
}

static bool psxDynarecCheckBreakpoint()
{
	u32 pc = psxRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_IOP, pc) == pc)
		return false;

	int bpFlags = psxIsBreakpointNeeded(pc);
	bool hit = false;
	//check breakpoint at current pc
	if (bpFlags & 1)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_IOP, pc);
		if (cond == NULL || cond->Evaluate())
		{
			hit = true;
		}
	}
	//check breakpoint in delay slot
	if (bpFlags & 2)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_IOP, pc + 4);
		if (cond == NULL || cond->Evaluate())
			hit = true;
	}

	if (!hit)
		return false;

	CBreakPoints::SetBreakpointTriggered(true);
#ifndef PCSX2_CORE
	GetCoreThread().PauseSelfDebug();
#endif

	// Exit the EE too.
	Cpu->ExitExecution();
	return true;
}

static bool psxDynarecMemcheck()
{
	u32 pc = psxRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_IOP, pc) == pc)
		return false;

	CBreakPoints::SetBreakpointTriggered(true);
#ifndef PCSX2_CORE
	GetCoreThread().PauseSelfDebug();
#endif

	// Exit the EE too.
	Cpu->ExitExecution();
	return true;
}

static void psxDynarecMemLogcheck(u32 start, bool store)
{
	if (store)
		DevCon.WriteLn("Hit store breakpoint @0x%x", start);
	else
		DevCon.WriteLn("Hit load breakpoint @0x%x", start);
}

static void psxRecMemcheck(u32 op, u32 bits, bool store)
{
	_psxFlushCall(FLUSH_EVERYTHING | FLUSH_PC);

	// compute accessed address
	_psxMoveGPRtoR(ecx, (op >> 21) & 0x1F);
	if ((s16)op != 0)
		xADD(ecx, (s16)op);

	xMOV(edx, ecx);
	xADD(edx, bits / 8);

	// ecx = access address
	// edx = access address+size

	auto checks = CBreakPoints::GetMemChecks();
	for (size_t i = 0; i < checks.size(); i++)
	{
		if (checks[i].cpu != BREAKPOINT_IOP)
			continue;
		if (checks[i].result == 0)
			continue;
		if ((checks[i].cond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((checks[i].cond & MEMCHECK_READ) == 0 && !store)
			continue;

		// logic: memAddress < bpEnd && bpStart < memAddress+memSize

		xMOV(eax, checks[i].end);
		xCMP(ecx, eax);     // address < end
		xForwardJGE8 next1; // if address >= end then goto next1

		xMOV(eax, checks[i].start);
		xCMP(eax, edx);     // start < address+size
		xForwardJGE8 next2; // if start >= address+size then goto next2

		                    // hit the breakpoint
		if (checks[i].result & MEMCHECK_LOG)
		{
			xMOV(edx, store);
			xFastCall((void*)psxDynarecMemLogcheck, ecx, edx);
		}
		if (checks[i].result & MEMCHECK_BREAK)
		{
			xFastCall((void*)psxDynarecMemcheck);
			xTEST(al, al);
			xJNZ(iopExitRecompiledCode);
		}

		next1.SetTarget();
		next2.SetTarget();
	}
}

static void psxEncodeBreakpoint()
{
	if (psxIsBreakpointNeeded(psxpc) != 0)
	{
		_psxFlushCall(FLUSH_EVERYTHING | FLUSH_PC);
		xFastCall((void*)psxDynarecCheckBreakpoint);
		xTEST(al, al);
		xJNZ(iopExitRecompiledCode);
	}
}

static void psxEncodeMemcheck()
{
	int needed = psxIsMemcheckNeeded(psxpc);
	if (needed == 0)
		return;

	u32 op = iopMemRead32(needed == 2 ? psxpc + 4 : psxpc);
	const R5900::OPCODE& opcode = R5900::GetInstruction(op);

	bool store = (opcode.flags & IS_STORE) != 0;
	switch (opcode.flags & MEMTYPE_MASK)
	{
		case MEMTYPE_BYTE:  psxRecMemcheck(op,   8, store); break;
		case MEMTYPE_HALF:  psxRecMemcheck(op,  16, store); break;
		case MEMTYPE_WORD:  psxRecMemcheck(op,  32, store); break;
		case MEMTYPE_DWORD: psxRecMemcheck(op,  64, store); break;
	}
}

void psxRecompileNextInstruction(int delayslot)
{
	// pblock isn't used elsewhere in this function.
	//BASEBLOCK* pblock = PSX_GETBLOCK(psxpc);

	// add breakpoint
	if (!delayslot)
	{
		psxEncodeBreakpoint();
		psxEncodeMemcheck();
	}

	if (IsDebugBuild)
	{
		xNOP();
		xMOV(eax, psxpc);
	}

	psxRegs.code = iopMemRead32(psxpc);
	s_psxBlockCycles++;
	psxpc += 4;

	g_pCurInstInfo++;

	g_iopCyclePenalty = 0;
	rpsxBSC[psxRegs.code >> 26]();
	s_psxBlockCycles += g_iopCyclePenalty;

	_clearNeededX86regs();
}

static void PreBlockCheck(u32 blockpc)
{
#ifdef PCSX2_DEBUG
	extern void iDumpPsxRegisters(u32 startpc, u32 temp);

	static u32 lastrec = 0;
	static int curcount = 0;
	const int skip = 0;

	//*(int*)PSXM(0x27990) = 1; // enables cdvd bios output for scph10000

	if ((psxdump & 2) && lastrec != blockpc)
	{
		curcount++;

		if (curcount > skip)
		{
			iDumpPsxRegisters(blockpc, 1);
			curcount = 0;
		}

		lastrec = blockpc;
	}
#endif
}

static void iopRecRecompile(const u32 startpc)
{
	u32 i;
	u32 willbranch3 = 0;

	// Inject IRX hack
	if (startpc == 0x1630 && EmuConfig.CurrentIRX.length() > 3)
	{
		if (iopMemRead32(0x20018) == 0x1F)
		{
			// FIXME do I need to increase the module count (0x1F -> 0x20)
			iopMemWrite32(0x20094, 0xbffc0000);
		}
	}

	if (IsDebugBuild && (psxdump & 4))
	{
		extern void iDumpPsxRegisters(u32 startpc, u32 temp);
		iDumpPsxRegisters(startpc, 0);
	}

	pxAssert(startpc);

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= (recMem->GetPtrEnd() - _64kb))
	{
		recResetIOP();
	}

	x86SetPtr(recPtr);
	x86Align(16);
	recPtr = x86Ptr;

	s_pCurBlock = PSX_GETBLOCK(startpc);

	pxAssert(s_pCurBlock->GetFnptr() == (uptr)iopJITCompile
		|| s_pCurBlock->GetFnptr() == (uptr)iopJITCompileInBlock);

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));

	if (!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc))
		s_pCurBlockEx = recBlocks.New(HWADDR(startpc), (uptr)recPtr);

	psxbranch = 0;

	s_pCurBlock->SetFnptr((uptr)x86Ptr);
	s_psxBlockCycles = 0;

	// reset recomp state variables
	psxpc = startpc;
	g_psxHasConstReg = g_psxFlushedConstReg = 1;

	_initX86regs();

	if ((psxHu32(HW_ICFG) & 8) && (HWADDR(startpc) == 0xa0 || HWADDR(startpc) == 0xb0 || HWADDR(startpc) == 0xc0))
	{
		xFastCall((void*)psxBiosCall);
		xTEST(al, al);
		xJNZ(iopDispatcherReg);
	}

	if (IsDebugBuild)
	{
		xFastCall((void*)PreBlockCheck, psxpc);
	}

	// go until the next branch
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

	while (1)
	{
		BASEBLOCK* pblock = PSX_GETBLOCK(i);
		if (i != startpc
		 && pblock->GetFnptr() != (uptr)iopJITCompile
		 && pblock->GetFnptr() != (uptr)iopJITCompileInBlock)
		{
			// branch = 3
			willbranch3 = 1;
			s_nEndBlock = i;
			break;
		}

		psxRegs.code = iopMemRead32(i);

		switch (psxRegs.code >> 26)
		{
			case 0: // special
				if (_Funct_ == 8 || _Funct_ == 9)
				{ // JR, JALR
					s_nEndBlock = i + 8;
					goto StartRecomp;
				}
				break;

			case 1: // regimm
				if (_Rt_ == 0 || _Rt_ == 1 || _Rt_ == 16 || _Rt_ == 17)
				{
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
				s_branchTo = _Imm_ * 4 + i + 4;
				if (s_branchTo > startpc && s_branchTo < i)
					s_nEndBlock = s_branchTo;
				else
					s_nEndBlock = i + 8;
				goto StartRecomp;
		}

		i += 4;
	}

StartRecomp:

	s_nBlockFF = false;
	if (s_branchTo == startpc)
	{
		s_nBlockFF = true;
		for (i = startpc; i < s_nEndBlock; i += 4)
		{
			if (i != s_nEndBlock - 8)
			{
				switch (iopMemRead32(i))
				{
					case 0: // nop
						break;
					default:
						s_nBlockFF = false;
				}
			}
		}
	}

	// rec info //
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
			psxRegs.code = iopMemRead32(i - 4);
			pcur[-1] = pcur[0];
			rpsxpropBSC(pcur - 1, pcur);
			pcur--;
		}
	}

	// dump code
	if (IsDebugBuild)
	{
		for (u32 recblock : s_psxrecblocks)
		{
			if (startpc == recblock)
			{
				iIopDumpBlock(startpc, recPtr);
			}
		}

		if ((psxdump & 1))
			iIopDumpBlock(startpc, recPtr);
	}

	g_pCurInstInfo = s_pInstCache;
	while (!psxbranch && psxpc < s_nEndBlock)
	{
		psxRecompileNextInstruction(0);
	}

	if (IsDebugBuild && (psxdump & 1))
		iIopDumpBlock(startpc, recPtr);

	pxAssert((psxpc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (psxpc - startpc) >> 2;

	for (i = 1; i < (u32)s_pCurBlockEx->size; ++i)
	{
		if (s_pCurBlock[i].GetFnptr() == (uptr)iopJITCompile)
			s_pCurBlock[i].SetFnptr((uptr)iopJITCompileInBlock);
	}

	if (!(psxpc & 0x10000000))
		g_psxMaxRecMem = std::max((psxpc & ~0xa0000000), g_psxMaxRecMem);

	if (psxbranch == 2)
	{
		_psxFlushCall(FLUSH_EVERYTHING);

		iPsxBranchTest(0xffffffff, 1);

		JMP32((uptr)iopDispatcherReg - ((uptr)x86Ptr + 5));
	}
	else
	{
		if (psxbranch)
			pxAssert(!willbranch3);
		else
		{
			xADD(ptr32[&psxRegs.cycle], psxScaleBlockCycles());
			xSUB(ptr32[&iopCycleEE], psxScaleBlockCycles() * 8);
		}

		if (willbranch3 || !psxbranch)
		{
			pxAssert(psxpc == s_nEndBlock);
			_psxFlushCall(FLUSH_EVERYTHING);
			xMOV(ptr32[&psxRegs.pc], psxpc);
			recBlocks.Link(HWADDR(s_nEndBlock), xJcc32());
			psxbranch = 3;
		}
	}

	pxAssert(xGetPtr() < recMem->GetPtrEnd());

	pxAssert(xGetPtr() - recPtr < _64kb);
	s_pCurBlockEx->x86size = xGetPtr() - recPtr;

	Perf::iop.map(s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = xGetPtr();

	pxAssert((g_psxHasConstReg & g_psxFlushedConstReg) == g_psxHasConstReg);

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

static void recSetCacheReserve(uint reserveInMegs)
{
	m_ConfiguredCacheReserve = reserveInMegs;
}

static uint recGetCacheReserve()
{
	return m_ConfiguredCacheReserve;
}

R3000Acpu psxRec = {
	recReserve,
	recResetIOP,
	recExecuteBlock,
	recClearIOP,
	recShutdown,

	recGetCacheReserve,
	recSetCacheReserve
};
