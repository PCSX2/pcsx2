// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "iR3000A.h"
#include "R3000A.h"
#include "BaseblockEx.h"
#include "R5900OpcodeTables.h"
#include "IopBios.h"
#include "IopHw.h"
#include "Common.h"
#include "VMManager.h"

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

#include "fmt/core.h"

// #define DUMP_BLOCKS 1
// #define TRACE_BLOCKS 1

#ifdef DUMP_BLOCKS
#include "Zydis/Zydis.h"
#include "Zycore/Format.h"
#include "Zycore/Status.h"
#endif

#ifdef TRACE_BLOCKS
#include <zlib.h>
#endif

using namespace x86Emitter;

extern void psxBREAK();

u32 g_psxMaxRecMem = 0;

uptr psxRecLUT[0x10000];
u32 psxhwLUT[0x10000];

static __fi u32 HWADDR(u32 mem) { return psxhwLUT[mem >> 16] + mem; }

static BASEBLOCK* recRAM = nullptr; // and the ptr to the blocks here
static BASEBLOCK* recROM = nullptr; // and here
static BASEBLOCK* recROM1 = nullptr; // also here
static BASEBLOCK* recROM2 = nullptr; // also here
static BaseBlocks recBlocks;
static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;
u32 psxpc; // recompiler psxpc
int psxbranch; // set for branch
u32 g_iopCyclePenalty;

static EEINST* s_pInstCache = nullptr;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = nullptr;
static BASEBLOCKEX* s_pCurBlockEx = nullptr;

static u32 s_nEndBlock = 0; // what psxpc the current block ends
static u32 s_branchTo;
static bool s_nBlockFF;

static u32 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static EEINST* s_psaveInstInfo = nullptr;

u32 s_psxBlockCycles = 0; // cycles of current block recompiling
static u32 s_savenBlockCycles = 0;
static bool s_recompilingDelaySlot = false;

static void iPsxBranchTest(u32 newpc, u32 cpuBranch);
void psxRecompileNextInstruction(int delayslot);

extern void (*rpsxBSC[64])();
void rpsxpropBSC(EEINST* prev, EEINST* pinst);

static void iopClearRecLUT(BASEBLOCK* base, int count);

#define PSX_GETBLOCK(x) PC_GETBLOCK_(x, psxRecLUT)

#define PSXREC_CLEARM(mem) \
	(((mem) < g_psxMaxRecMem && (psxRecLUT[(mem) >> 16] + (mem))) ? \
			psxRecClearMem(mem) : \
            4)

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

	if (address >= A(iopMem->Main) && address < A(iopMem->P))
	{
		len = snprintf(buf, sizeof(buf), "iopMem+0x%08X", static_cast<u32>(address - A(iopMem->Main)));
	}
	else if (address >= A(&psxRegs.GPR) && address < A(&psxRegs.CP0))
	{
		len = snprintf(buf, sizeof(buf), "psxRegs.GPR.%s", R3000A::disRNameGPR[static_cast<u32>(address - A(&psxRegs)) / 4u]);
	}
	else if (address == A(&psxRegs.pc))
	{
		len = snprintf(buf, sizeof(buf), "psxRegs.pc");
	}
	else if (address == A(&psxRegs.cycle))
	{
		len = snprintf(buf, sizeof(buf), "psxRegs.cycle");
	}
	else if (address == A(&g_nextEventCycle))
	{
		len = snprintf(buf, sizeof(buf), "g_nextEventCycle");
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

// =====================================================================================================
//  Dynamically Compiled Dispatchers - R3000A style
// =====================================================================================================

static void iopRecRecompile(u32 startpc);

static const void* iopDispatcherEvent = nullptr;
static const void* iopDispatcherReg = nullptr;
static const void* iopJITCompile = nullptr;
static const void* iopJITCompileInBlock = nullptr;
static const void* iopEnterRecompiledCode = nullptr;
static const void* iopExitRecompiledCode = nullptr;

static void recEventTest()
{
	_cpuEventTest_Shared();
}

// The address for all cleared blocks.  It recompiles the current pc and then
// dispatches to the recompiled block address.
static const void* _DynGen_JITCompile()
{
	pxAssertMsg(iopDispatcherReg != NULL, "Please compile the DispatcherReg subroutine *before* JITComple.  Thanks.");

	u8* retval = xGetPtr();

	xFastCall((void*)iopRecRecompile, ptr32[&psxRegs.pc]);

	xMOV(eax, ptr[&psxRegs.pc]);
	xMOV(ebx, eax);
	xSHR(eax, 16);
	xMOV(rcx, ptrNative[xComplexAddress(rcx, psxRecLUT, rax * wordsize)]);
	xJMP(ptrNative[rbx * (wordsize / 4) + rcx]);

	return retval;
}

static const void* _DynGen_JITCompileInBlock()
{
	u8* retval = xGetPtr();
	xJMP((void*)iopJITCompile);
	return retval;
}

// called when jumping to variable pc address
static const void* _DynGen_DispatcherReg()
{
	u8* retval = xGetPtr();

	xMOV(eax, ptr[&psxRegs.pc]);
	xMOV(ebx, eax);
	xSHR(eax, 16);
	xMOV(rcx, ptrNative[xComplexAddress(rcx, psxRecLUT, rax * wordsize)]);
	xJMP(ptrNative[rbx * (wordsize / 4) + rcx]);

	return retval;
}

// --------------------------------------------------------------------------------------
//  EnterRecompiledCode  - dynamic compilation stub!
// --------------------------------------------------------------------------------------
static const void* _DynGen_EnterRecompiledCode()
{
	// Optimization: The IOP never uses stack-based parameter invocation, so we can avoid
	// allocating any room on the stack for it (which is important since the IOP's entry
	// code gets invoked quite a lot).

	u8* retval = xGetPtr();

	{ // Properly scope the frame prologue/epilogue
#ifdef ENABLE_VTUNE
		xScopedStackFrame frame(true, true);
#else
		xScopedStackFrame frame(false, true);
#endif

		xJMP((void*)iopDispatcherReg);

		// Save an exit point
		iopExitRecompiledCode = xGetPtr();
	}

	xRET();

	return retval;
}

static void _DynGen_Dispatchers()
{
	const u8* start = xGetAlignedCallTarget();

	// Place the EventTest and DispatcherReg stuff at the top, because they get called the
	// most and stand to benefit from strong alignment and direct referencing.
	iopDispatcherEvent = xGetPtr();
	xFastCall((void*)recEventTest);
	iopDispatcherReg = _DynGen_DispatcherReg();

	iopJITCompile = _DynGen_JITCompile();
	iopJITCompileInBlock = _DynGen_JITCompileInBlock();
	iopEnterRecompiledCode = _DynGen_EnterRecompiledCode();

	recBlocks.SetJITCompile(iopJITCompile);

	Perf::any.Register(start, xGetPtr() - start, "IOP Dispatcher");
}

////////////////////////////////////////////////////
using namespace R3000A;

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
	// TODO: Combine flushes

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
		_psxFlushConstReg(reg);

	PSX_DEL_CONST(reg);
	_deletePSXtoX86reg(reg, flush ? DELETE_REG_FREE : DELETE_REG_FREE_NO_WRITEBACK);
}

void _psxMoveGPRtoR(const xRegister32& to, int fromgpr)
{
	if (PSX_IS_CONST1(fromgpr))
	{
		xMOV(to, g_psxConstRegs[fromgpr]);
	}
	else
	{
		const int reg = EEINST_USEDTEST(fromgpr) ? _allocX86reg(X86TYPE_PSX, fromgpr, MODE_READ) : _checkX86reg(X86TYPE_PSX, fromgpr, MODE_READ);
		if (reg >= 0)
			xMOV(to, xRegister32(reg));
		else
			xMOV(to, ptr[&psxRegs.GPR.r[fromgpr]]);
	}
}

void _psxMoveGPRtoM(uptr to, int fromgpr)
{
	if (PSX_IS_CONST1(fromgpr))
	{
		xMOV(ptr32[(u32*)(to)], g_psxConstRegs[fromgpr]);
	}
	else
	{
		const int reg = EEINST_USEDTEST(fromgpr) ? _allocX86reg(X86TYPE_PSX, fromgpr, MODE_READ) : _checkX86reg(X86TYPE_PSX, fromgpr, MODE_READ);
		if (reg >= 0)
		{
			xMOV(ptr32[(u32*)(to)], xRegister32(reg));
		}
		else
		{
			xMOV(eax, ptr[&psxRegs.GPR.r[fromgpr]]);
			xMOV(ptr32[(u32*)(to)], eax);
		}
	}
}

void _psxFlushCall(int flushtype)
{
	// Free registers that are not saved across function calls (x86-32 ABI):
	for (u32 i = 0; i < iREGCNT_GPR; i++)
	{
		if (!x86regs[i].inuse)
			continue;

		if (xRegisterBase::IsCallerSaved(i) ||
			((flushtype & FLUSH_FREE_NONTEMP_X86) && x86regs[i].type != X86TYPE_TEMP) ||
			((flushtype & FLUSH_FREE_TEMP_X86) && x86regs[i].type == X86TYPE_TEMP))
		{
			_freeX86reg(i);
		}
	}

	if (flushtype & FLUSH_ALL_X86)
		_flushX86regs();

	if (flushtype & FLUSH_CONSTANT_REGS)
		_psxFlushConstRegs();

	if ((flushtype & FLUSH_PC) /*&& !g_cpuFlushedPC*/)
	{
		xMOV(ptr32[&psxRegs.pc], psxpc);
		//g_cpuFlushedPC = true;
	}
}

void _psxFlushAllDirty()
{
	// TODO: Combine flushes
	for (u32 i = 0; i < 32; ++i)
	{
		if (PSX_IS_CONST1(i))
			_psxFlushConstReg(i);
	}

	_flushX86regs();
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

bool psxTrySwapDelaySlot(u32 rs, u32 rt, u32 rd)
{
#if 1
	if (s_recompilingDelaySlot)
		return false;

	const u32 opcode_encoded = iopMemRead32(psxpc);
	if (opcode_encoded == 0)
	{
		psxRecompileNextInstruction(true, true);
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
		case 15: // LUI
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
		case 46: // SWR
		{
			if ((rs != 0 && rs == opcode_rt) || (rt != 0 && rt == opcode_rt) || (rd != 0 && (rd == opcode_rs || rd == opcode_rt)))
				goto is_unsafe;
		}
		break;

		case 50: // LWC2
		case 58: // SWC2
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
				{
					if ((rs != 0 && rs == opcode_rd) || (rt != 0 && rt == opcode_rd) || (rd != 0 && (rd == opcode_rs || rd == opcode_rt)))
						goto is_unsafe;
				}
				break;

				case 15: // SYNC
				case 24: // MULT
				case 25: // MULTU
				case 26: // DIV
				case 27: // DIVU
					break;

				default:
					goto is_unsafe;
			}
		}
		break;

		case 16: // COP0
		case 17: // COP1
		case 18: // COP2
		case 19: // COP3
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

				default:
				{
					// swap when it's GTE
					if ((opcode_encoded >> 26) != 18)
						goto is_unsafe;
				}
				break;
			}
			break;
		}
		break;

		default:
			goto is_unsafe;
	}

	RALOG("Swapping delay slot %08X %s\n", psxpc, disR3000AF(iopMemRead32(psxpc), psxpc));
	psxRecompileNextInstruction(true, true);
	return true;

is_unsafe:
	RALOG("NOT SWAPPING delay slot %08X %s\n", psxpc, disR3000AF(iopMemRead32(psxpc), psxpc));
	return false;
#else
	return false;
#endif
}

int psxTryRenameReg(int to, int from, int fromx86, int other, int xmminfo)
{
	// can't rename when in form Rd = Rs op Rt and Rd == Rs or Rd == Rt
	if ((xmminfo & XMMINFO_NORENAME) || fromx86 < 0 || to == from || to == other || !EEINST_RENAMETEST(from))
		return -1;

	RALOG("Renaming %s to %s\n", R3000A::disRNameGPR[from], R3000A::disRNameGPR[to]);

	// flush back when it's been modified
	if (x86regs[fromx86].mode & MODE_WRITE && EEINST_LIVETEST(from))
		_writebackX86Reg(fromx86);

	// remove all references to renamed-to register
	_deletePSXtoX86reg(to, DELETE_REG_FREE_NO_WRITEBACK);
	PSX_DEL_CONST(to);

	// and do the actual rename, new register has been modified.
	x86regs[fromx86].reg = to;
	x86regs[fromx86].mode |= MODE_READ | MODE_WRITE;
	return fromx86;
}

// rd = rs op rt
void psxRecompileCodeConst0(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int xmminfo)
{
	if (!_Rd_)
		return;

	if (PSX_IS_CONST2(_Rs_, _Rt_))
	{
		_deletePSXtoX86reg(_Rd_, DELETE_REG_FREE_NO_WRITEBACK);
		PSX_SET_CONST(_Rd_);
		constcode();
		return;
	}

	// we have to put these up here, because the register allocator below will wipe out const flags
	// for the destination register when/if it switches it to write mode.
	const bool s_is_const = PSX_IS_CONST1(_Rs_);
	const bool t_is_const = PSX_IS_CONST1(_Rt_);
	const bool d_is_const = PSX_IS_CONST1(_Rd_);
	const bool s_is_used = EEINST_USEDTEST(_Rs_);
	const bool t_is_used = EEINST_USEDTEST(_Rt_);

	if (!s_is_const)
		_addNeededGPRtoX86reg(_Rs_);
	if (!t_is_const)
		_addNeededGPRtoX86reg(_Rt_);
	if (!d_is_const)
		_addNeededGPRtoX86reg(_Rd_);

	u32 info = 0;
	int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs < 0 && ((!s_is_const && s_is_used) || _Rs_ == _Rd_))
		regs = _allocX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		info |= PROCESS_EE_SET_S(regs);

	int regt = _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
	if (regt < 0 && ((!t_is_const && t_is_used) || _Rt_ == _Rd_))
		regt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
	if (regt >= 0)
		info |= PROCESS_EE_SET_T(regt);

	// If S is no longer live, swap D for S. Saves the move.
	int regd = psxTryRenameReg(_Rd_, _Rs_, regs, _Rt_, xmminfo);
	if (regd < 0)
	{
		// TODO: If not live, write direct to memory.
		regd = _allocX86reg(X86TYPE_PSX, _Rd_, MODE_WRITE);
	}
	if (regd >= 0)
		info |= PROCESS_EE_SET_D(regd);

	_validateRegs();

	if (s_is_const && regs < 0)
	{
		// This *must* go inside the if, because of when _Rs_ =  _Rd_
		PSX_DEL_CONST(_Rd_);
		constscode(info /*| PROCESS_CONSTS*/);
		return;
	}

	if (t_is_const && regt < 0)
	{
		PSX_DEL_CONST(_Rd_);
		consttcode(info /*| PROCESS_CONSTT*/);
		return;
	}

	PSX_DEL_CONST(_Rd_);
	noconstcode(info);
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
void psxRecompileCodeConst1(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode, int xmminfo)
{
	if (!_Rt_)
	{
		// check for iop module import table magic
		if (psxRegs.code >> 16 == 0x2400)
			psxRecompileIrxImport();
		return;
	}

	if (PSX_IS_CONST1(_Rs_))
	{
		_deletePSXtoX86reg(_Rt_, DELETE_REG_FREE_NO_WRITEBACK);
		PSX_SET_CONST(_Rt_);
		constcode();
		return;
	}

	_addNeededPSXtoX86reg(_Rs_);
	_addNeededPSXtoX86reg(_Rt_);

	u32 info = 0;

	const bool s_is_used = EEINST_USEDTEST(_Rs_);
	const int regs = s_is_used ? _allocX86reg(X86TYPE_PSX, _Rs_, MODE_READ) : _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		info |= PROCESS_EE_SET_S(regs);

	int regt = psxTryRenameReg(_Rt_, _Rs_, regs, 0, xmminfo);
	if (regt < 0)
	{
		regt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_WRITE);
	}
	if (regt >= 0)
		info |= PROCESS_EE_SET_T(regt);

	_validateRegs();

	PSX_DEL_CONST(_Rt_);
	noconstcode(info);
}

// rd = rt op sa
void psxRecompileCodeConst2(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode, int xmminfo)
{
	if (!_Rd_)
		return;

	if (PSX_IS_CONST1(_Rt_))
	{
		_deletePSXtoX86reg(_Rd_, DELETE_REG_FREE_NO_WRITEBACK);
		PSX_SET_CONST(_Rd_);
		constcode();
		return;
	}

	_addNeededPSXtoX86reg(_Rt_);
	_addNeededPSXtoX86reg(_Rd_);

	u32 info = 0;
	const bool s_is_used = EEINST_USEDTEST(_Rt_);
	const int regt = s_is_used ? _allocX86reg(X86TYPE_PSX, _Rt_, MODE_READ) : _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
	if (regt >= 0)
		info |= PROCESS_EE_SET_T(regt);

	int regd = psxTryRenameReg(_Rd_, _Rt_, regt, 0, xmminfo);
	if (regd < 0)
	{
		regd = _allocX86reg(X86TYPE_PSX, _Rd_, MODE_WRITE);
	}
	if (regd >= 0)
		info |= PROCESS_EE_SET_D(regd);

	_validateRegs();

	PSX_DEL_CONST(_Rd_);
	noconstcode(info);
}

// rd = rt MULT rs  (SPECIAL)
void psxRecompileCodeConst3(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int LOHI)
{
	if (PSX_IS_CONST2(_Rs_, _Rt_))
	{
		if (LOHI)
		{
			_deletePSXtoX86reg(PSX_LO, DELETE_REG_FREE_NO_WRITEBACK);
			_deletePSXtoX86reg(PSX_HI, DELETE_REG_FREE_NO_WRITEBACK);
		}

		constcode();
		return;
	}

	// we have to put these up here, because the register allocator below will wipe out const flags
	// for the destination register when/if it switches it to write mode.
	const bool s_is_const = PSX_IS_CONST1(_Rs_);
	const bool t_is_const = PSX_IS_CONST1(_Rt_);
	const bool s_is_used = EEINST_USEDTEST(_Rs_);
	const bool t_is_used = EEINST_USEDTEST(_Rt_);

	if (!s_is_const)
		_addNeededGPRtoX86reg(_Rs_);
	if (!t_is_const)
		_addNeededGPRtoX86reg(_Rt_);
	if (LOHI)
	{
		if (EEINST_LIVETEST(PSX_LO))
			_addNeededPSXtoX86reg(PSX_LO);
		if (EEINST_LIVETEST(PSX_HI))
			_addNeededPSXtoX86reg(PSX_HI);
	}

	u32 info = 0;
	int regs = _checkX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs < 0 && !s_is_const && s_is_used)
		regs = _allocX86reg(X86TYPE_PSX, _Rs_, MODE_READ);
	if (regs >= 0)
		info |= PROCESS_EE_SET_S(regs);

	// need at least one in a register
	int regt = _checkX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
	if (regs < 0 || (regt < 0 && !t_is_const && t_is_used))
		regt = _allocX86reg(X86TYPE_PSX, _Rt_, MODE_READ);
	if (regt >= 0)
		info |= PROCESS_EE_SET_T(regt);

	if (LOHI)
	{
		// going to destroy lo/hi, so invalidate if we're writing it back to state
		const bool lo_is_used = EEINST_USEDTEST(PSX_LO);
		const int reglo = lo_is_used ? _allocX86reg(X86TYPE_PSX, PSX_LO, MODE_WRITE) : -1;
		if (reglo >= 0)
			info |= PROCESS_EE_SET_LO(reglo) | PROCESS_EE_LO;
		else
			_deletePSXtoX86reg(PSX_LO, DELETE_REG_FREE_NO_WRITEBACK);

		const bool hi_is_live = EEINST_USEDTEST(PSX_HI);
		const int reghi = hi_is_live ? _allocX86reg(X86TYPE_PSX, PSX_HI, MODE_WRITE) : -1;
		if (reghi >= 0)
			info |= PROCESS_EE_SET_HI(reghi) | PROCESS_EE_HI;
		else
			_deletePSXtoX86reg(PSX_HI, DELETE_REG_FREE_NO_WRITEBACK);
	}

	_validateRegs();

	if (s_is_const && regs < 0)
	{
		// This *must* go inside the if, because of when _Rs_ =  _Rd_
		constscode(info /*| PROCESS_CONSTS*/);
		return;
	}

	if (t_is_const && regt < 0)
	{
		consttcode(info /*| PROCESS_CONSTT*/);
		return;
	}

	noconstcode(info);
}

static u8* m_recBlockAlloc = NULL;

static const uint m_recBlockAllocSize =
	(((Ps2MemSize::IopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4) * sizeof(BASEBLOCK));

static void recReserve()
{
	recPtr = SysMemory::GetIOPRec();
	recPtrEnd = SysMemory::GetIOPRecEnd() - _64kb;

	// Goal: Allocate BASEBLOCKs for every possible branch target in IOP memory.
	// Any 4-byte aligned address makes a valid branch target as per MIPS design (all instructions are
	// always 4 bytes long).

	if (!m_recBlockAlloc)
	{
		// We're on 64-bit, if these memory allocations fail, we're in real trouble.
		m_recBlockAlloc = (u8*)_aligned_malloc(m_recBlockAllocSize, 4096);
		if (!m_recBlockAlloc)
			pxFailRel("Failed to allocate R3000A BASEBLOCK lookup tables");
	}

	u8* curpos = m_recBlockAlloc;
	recRAM = (BASEBLOCK*)curpos;
	curpos += (Ps2MemSize::IopRam / 4) * sizeof(BASEBLOCK);
	recROM = (BASEBLOCK*)curpos;
	curpos += (Ps2MemSize::Rom / 4) * sizeof(BASEBLOCK);
	recROM1 = (BASEBLOCK*)curpos;
	curpos += (Ps2MemSize::Rom1 / 4) * sizeof(BASEBLOCK);
	recROM2 = (BASEBLOCK*)curpos;
	curpos += (Ps2MemSize::Rom2 / 4) * sizeof(BASEBLOCK);

	pxAssertRel(!s_pInstCache, "InstCache not allocated");
	s_nInstCacheSize = 128;
	s_pInstCache = (EEINST*)malloc(sizeof(EEINST) * s_nInstCacheSize);
	if (!s_pInstCache)
		pxFailRel("Failed to allocate R3000 InstCache array.");
}

void recResetIOP()
{
	DevCon.WriteLn("iR3000A Recompiler reset.");

	xSetPtr(SysMemory::GetIOPRec());
	_DynGen_Dispatchers();
	recPtr = xGetPtr();

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

	for (int i = 0x1e00; i < 0x1e40; i++)
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

	psxbranch = 0;
}

static void recShutdown()
{
	safe_aligned_free(m_recBlockAlloc);

	safe_free(s_pInstCache);
	s_nInstCacheSize = 0;

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static void iopClearRecLUT(BASEBLOCK* base, int count)
{
	for (int i = 0; i < count; i++)
		base[i].SetFnptr((uptr)iopJITCompile);
}

static __noinline s32 recExecuteBlock(s32 eeCycles)
{
	psxRegs.iopBreak = 0;
	psxRegs.iopCycleEE = eeCycles;

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

	((void(*)())iopEnterRecompiledCode)();

	return psxRegs.iopBreak + psxRegs.iopCycleEE;
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
		if (pc >= pexblock->startpc && pc < pexblock->startpc + pexblock->size * 4) [[unlikely]]
		{
			DevCon.Error("[IOP] Impossible block clearing failure");
			pxFail("[IOP] Impossible block clearing failure");
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
		const bool swap = psxTrySwapDelaySlot(reg, 0, 0);

		if (!swap)
		{
			const int wbreg = _allocX86reg(X86TYPE_PCWRITEBACK, 0, MODE_WRITE | MODE_CALLEESAVED);
			_psxMoveGPRtoR(xRegister32(wbreg), reg);

			psxRecompileNextInstruction(true, false);

			if (x86regs[wbreg].inuse && x86regs[wbreg].type == X86TYPE_PCWRITEBACK)
			{
				xMOV(ptr32[&psxRegs.pc], xRegister32(wbreg));
				x86regs[wbreg].inuse = 0;
			}
			else
			{
				xMOV(eax, ptr32[&psxRegs.pcWriteback]);
				xMOV(ptr32[&psxRegs.pc], eax);
			}
		}
		else
		{
			if (PSX_IS_DIRTY_CONST(reg) || _hasX86reg(X86TYPE_PSX, reg, 0))
			{
				const int x86reg = _allocX86reg(X86TYPE_PSX, reg, MODE_READ);
				xMOV(ptr32[&psxRegs.pc], xRegister32(x86reg));
			}
			else
			{
				_psxMoveGPRtoM((uptr)&psxRegs.pc, reg);
			}
		}
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
		xMOV(edx, ptr32[&psxRegs.iopCycleEE]);
		xADD(edx, 7);
		xSHR(edx, 3);
		xADD(eax, edx);
		xCMP(eax, ptr32[&psxRegs.iopNextEventCycle]);
		xCMOVNS(eax, ptr32[&psxRegs.iopNextEventCycle]);
		xMOV(ptr32[&psxRegs.cycle], eax);
		xSUB(eax, ecx);
		xSHL(eax, 3);
		xSUB(ptr32[&psxRegs.iopCycleEE], eax);
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
		xSUB(ptr32[&psxRegs.iopCycleEE], blockCycles * 8);
		xJLE(iopExitRecompiledCode);

		// check if an event is pending
		xSUB(eax, ptr32[&psxRegs.iopNextEventCycle]);
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
	xSUB(ptr32[&psxRegs.iopCycleEE], psxScaleBlockCycles() * 8);
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
	xSUB(ptr32[&psxRegs.iopCycleEE], psxScaleBlockCycles() * 8);
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

	CBreakPoints::SetBreakpointTriggered(true, BREAKPOINT_IOP);
	VMManager::SetPaused(true);

	// Exit the EE too.
	Cpu->ExitExecution();
	return true;
}

static bool psxDynarecMemcheck()
{
	u32 pc = psxRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_IOP, pc) == pc)
		return false;

	CBreakPoints::SetBreakpointTriggered(true, BREAKPOINT_IOP);
	VMManager::SetPaused(true);

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

	auto checks = CBreakPoints::GetMemChecks(BREAKPOINT_IOP);
	for (size_t i = 0; i < checks.size(); i++)
	{
		if (checks[i].result == 0)
			continue;
		if ((checks[i].cond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((checks[i].cond & MEMCHECK_READ) == 0 && !store)
			continue;

		// logic: memAddress < bpEnd && bpStart < memAddress+memSize

		xMOV(eax, checks[i].end);
		xCMP(ecx, eax); // address < end
		xForwardJGE8 next1; // if address >= end then goto next1

		xMOV(eax, checks[i].start);
		xCMP(eax, edx); // start < address+size
		xForwardJGE8 next2; // if start >= address+size then goto next2

		// hit the breakpoint
		if (checks[i].result & MEMCHECK_LOG)
		{
			xMOV(edx, store);

			// Refer to the EE recompiler for an explaination
			if(!(checks[i].result & MEMCHECK_BREAK))
			{
				xPUSH(eax); xPUSH(ebx); xPUSH(ecx); xPUSH(edx);
				xFastCall((void*)psxDynarecMemLogcheck, ecx, edx);
				xPOP(edx); xPOP(ecx); xPOP(ebx); xPOP(eax);
			}
			else
			{
				xFastCall((void*)psxDynarecMemLogcheck, ecx, edx);
			}
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
		case MEMTYPE_BYTE:
			psxRecMemcheck(op, 8, store);
			break;
		case MEMTYPE_HALF:
			psxRecMemcheck(op, 16, store);
			break;
		case MEMTYPE_WORD:
			psxRecMemcheck(op, 32, store);
			break;
		case MEMTYPE_DWORD:
			psxRecMemcheck(op, 64, store);
			break;
	}
}

void psxRecompileNextInstruction(bool delayslot, bool swapped_delayslot)
{
#ifdef DUMP_BLOCKS
	const bool dump_block = true;

	const u8* instStart = x86Ptr;
	ZydisDecoder disas_decoder;
	ZydisFormatter disas_formatter;
	ZydisDecodedInstruction disas_instruction;

	if (dump_block)
	{
		fprintf(stderr, "Compiling %s%s\n", delayslot ? "delay slot " : "", disR3000AF(iopMemRead32(psxpc), psxpc));
		if (!delayslot)
		{
			ZydisDecoderInit(&disas_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_ADDRESS_WIDTH_64);
			ZydisFormatterInit(&disas_formatter, ZYDIS_FORMATTER_STYLE_INTEL);
			s_old_print_address = (ZydisFormatterFunc)&ZydisFormatterPrintAddressAbsolute;
			ZydisFormatterSetHook(&disas_formatter, ZYDIS_FORMATTER_FUNC_PRINT_ADDRESS_ABS, (const void**)&s_old_print_address);
		}
	}
#endif

	const int old_code = psxRegs.code;
	EEINST* old_inst_info = g_pCurInstInfo;
	s_recompilingDelaySlot = delayslot;

	// add breakpoint
	if (!delayslot)
	{
		// Broken on x64
		psxEncodeBreakpoint();
		psxEncodeMemcheck();
	}
	else
	{
		_clearNeededX86regs();
	}

	psxRegs.code = iopMemRead32(psxpc);
	s_psxBlockCycles++;
	psxpc += 4;

	g_pCurInstInfo++;

	g_iopCyclePenalty = 0;
	rpsxBSC[psxRegs.code >> 26]();
	s_psxBlockCycles += g_iopCyclePenalty;

	if (!swapped_delayslot)
		_clearNeededX86regs();

	if (swapped_delayslot)
	{
		psxRegs.code = old_code;
		g_pCurInstInfo = old_inst_info;
	}

#ifdef DUMP_BLOCKS
	if (dump_block && !delayslot)
	{
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
#endif
}

#ifdef TRACE_BLOCKS
static void PreBlockCheck(u32 blockpc)
{
#if 0
	static FILE* fp = nullptr;
	static bool fp_opened = false;
	if (!fp_opened && psxRegs.cycle >= 0)
	{
		fp = std::fopen("C:\\Dumps\\comp\\ioplog.txt", "wb");
		fp_opened = true;
	}
	if (fp)
	{
		u32 hash = crc32(0, (Bytef*)&psxRegs, offsetof(psxRegisters, pc));

#if 1
		std::fprintf(fp, "%08X (%u; %08X):", psxRegs.pc, psxRegs.cycle, hash);
		for (int i = 0; i < 34; i++)
		{
			std::fprintf(fp, " %s: %08X", R3000A::disRNameGPR[i], psxRegs.GPR.r[i]);
		}
		std::fprintf(fp, "\n");
#else
		std::fprintf(fp, "%08X (%u): %08X\n", psxRegs.pc, psxRegs.cycle, hash);
#endif
		// std::fflush(fp);
	}
#endif
#if 0
	if (psxRegs.cycle == 0)
		__debugbreak();
#endif
}
#endif

static void iopRecRecompile(const u32 startpc)
{
	u32 i;
	u32 willbranch3 = 0;

	// When upgrading the IOP, there are two resets, the second of which is a 'fake' reset
	// This second 'reset' involves UDNL calling SYSMEM and LOADCORE directly, resetting LOADCORE's modules
	// This detects when SYSMEM is called and clears the modules then
	if(startpc == 0x890)
	{
		DevCon.WriteLn(Color_Gray, "[R3000 Debugger] Branch to 0x890 (SYSMEM). Clearing modules.");
		R3000SymbolMap.ClearModules();
	}

	// Inject IRX hack
	if (startpc == 0x1630 && EmuConfig.CurrentIRX.length() > 3)
	{
		if (iopMemRead32(0x20018) == 0x1F)
		{
			// FIXME do I need to increase the module count (0x1F -> 0x20)
			iopMemWrite32(0x20094, 0xbffc0000);
		}
	}

	pxAssert(startpc);

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= recPtrEnd)
	{
		recResetIOP();
	}

	xSetPtr(recPtr);
	recPtr = xGetAlignedCallTarget();

	s_pCurBlock = PSX_GETBLOCK(startpc);

	pxAssert(s_pCurBlock->GetFnptr() == (uptr)iopJITCompile || s_pCurBlock->GetFnptr() == (uptr)iopJITCompileInBlock);

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

#ifdef TRACE_BLOCKS
	xFastCall((void*)PreBlockCheck, psxpc);
#endif

	// go until the next branch
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

	while (1)
	{
		BASEBLOCK* pblock = PSX_GETBLOCK(i);
		if (i != startpc && pblock->GetFnptr() != (uptr)iopJITCompile && pblock->GetFnptr() != (uptr)iopJITCompileInBlock)
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
				s_branchTo = (_InstrucTarget_ << 2) | ((i + 4) & 0xf0000000);
				s_nEndBlock = i + 8;
				goto StartRecomp;

			// branches
			case 4:
			case 5:
			case 6:
			case 7:
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

	g_pCurInstInfo = s_pInstCache;
	while (!psxbranch && psxpc < s_nEndBlock)
	{
		psxRecompileNextInstruction(false, false);
	}

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
			xSUB(ptr32[&psxRegs.iopCycleEE], psxScaleBlockCycles() * 8);
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

	pxAssert(xGetPtr() < recPtrEnd);

	pxAssert(xGetPtr() - recPtr < _64kb);
	s_pCurBlockEx->x86size = xGetPtr() - recPtr;

	Perf::iop.RegisterPC((void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = xGetPtr();

	pxAssert((g_psxHasConstReg & g_psxFlushedConstReg) == g_psxHasConstReg);

	s_pCurBlock = NULL;
	s_pCurBlockEx = NULL;
}

R3000Acpu psxRec = {
	recReserve,
	recResetIOP,
	recExecuteBlock,
	recClearIOP,
	recShutdown,
};
