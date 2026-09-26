// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 R3000A (IOP) recompiler.
//
// This is a port of the x86 IOP recompiler (x86/iR3000A.cpp), with a simpler design:
//  - Guest GPRs live in psxRegs and are accessed relative to a pinned base register (x19).
//    There is no host register cache across instructions, only constant propagation.
//  - Block layout, cycle accounting, event testing and block linking follow the x86 recompiler,
//    so timing should match it.
//  - Rarely used/complex instructions (LWL/LWR/SWL/SWR, DIV/DIVU, GTE) call the interpreter.
//
// Needs proper testing across a wide range of games; compare against the interpreter when in doubt.

// R3000A.h must come first, so that the instruction field macros (_Rs_ etc.) refer to psxRegs, not cpuRegs.
#include "R3000A.h"

#include "arm64/AsmHelpers.h"
#include "x86/BaseblockEx.h"

#include "Common.h"
#include "Config.h"
#include "Host.h"
#include "IopBios.h"
#include "IopDma.h"
#include "IopHw.h"
#include "IopMem.h"
#include "Memory.h"
#include "R5900OpcodeTables.h"
#include "VMManager.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/SymbolGuardian.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/HeapArray.h"
#include "common/Perf.h"

#include <algorithm>
#include <cstddef>
#include <string>

namespace a64 = vixl::aarch64;

// Pinned registers. Both are callee-saved, so they survive calls into C code.
#define RPSXREGS a64::x19 // &psxRegs
#define RWBRANCH a64::w21 // branch condition/target, held across the delay slot

// Temporaries, never live across a call.
#define RWTEMP1 a64::w8
#define RWTEMP2 a64::w9
#define RWTEMP3 a64::w10
#define RWTEMP4 a64::w11
#define RWTEMP5 a64::w12
#define RXTEMP1 a64::x8
#define RXTEMP2 a64::x9
#define RXTEMP3 a64::x10
#define RXTEMP4 a64::x11

// Cycle penalties for particularly slow instructions (same as x86).
static constexpr u32 psxInstCycles_Mult = 7;
static constexpr u32 psxInstCycles_Div = 40;

static constexpr u32 PSX_HI = 32;
static constexpr u32 PSX_LO = 33;

extern void psxNULL();
extern u32 g_psxMaxRecMem;
u32 g_psxMaxRecMem = 0;

uptr psxRecLUT[0x10000];
u32 psxhwLUT[0x10000];

static __fi u32 HWADDR(u32 mem) { return psxhwLUT[mem >> 16] + mem; }

static BASEBLOCK* recRAM = nullptr;
static BASEBLOCK* recROM = nullptr;
static BASEBLOCK* recROM1 = nullptr;
static BASEBLOCK* recROM2 = nullptr;
static BaseBlocks recBlocks;
static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;

static u32 psxpc; // recompiler pc
static int psxbranch; // set for branch
static u32 g_iopCyclePenalty;

static BASEBLOCK* s_pCurBlock = nullptr;
static BASEBLOCKEX* s_pCurBlockEx = nullptr;

static u32 s_nEndBlock = 0; // what psxpc the current block ends
static u32 s_branchTo;
static bool s_nBlockFF;

static u32 s_psxBlockCycles = 0; // cycles of current block recompiling

// Constant propagation state.
static u32 s_constRegs[32];
static u32 s_hasConstReg = 0;
static u32 s_flushedConstReg = 0;

#define PSX_GETBLOCK(x) PC_GETBLOCK_(x, psxRecLUT)

#define PSXREC_CLEARM(mem) \
	(((mem) < g_psxMaxRecMem && (psxRecLUT[(mem) >> 16] + (mem))) ? \
			psxRecClearMem(mem) : \
			4)

static void iopRecRecompile(u32 startpc);
static void iopClearRecLUT(BASEBLOCK* base, int count);
static void iopRecError(int err);
static void psxRecompileNextInstruction(bool delayslot);

static const void* iopDispatcherReg = nullptr;
static const void* iopJITCompile = nullptr;
static const void* iopEnterRecompiledCode = nullptr;
static const void* iopExitRecompiledCode = nullptr;
static const void* iopUnmappedRecLUTPage = nullptr;

//////////////////////////////////////////////////////////////////////////
// Register access helpers
//////////////////////////////////////////////////////////////////////////

#define PSXREG_MEM(field) a64::MemOperand(RPSXREGS, static_cast<s64>(offsetof(psxRegisters, field)))

static __fi a64::MemOperand PSXGPR(u32 reg)
{
	return a64::MemOperand(RPSXREGS, static_cast<s64>(offsetof(psxRegisters, GPR) + reg * sizeof(u32)));
}

static __fi a64::MemOperand PSXCP0(u32 reg)
{
	return a64::MemOperand(RPSXREGS, static_cast<s64>(offsetof(psxRegisters, CP0) + reg * sizeof(u32)));
}

static __fi bool PSX_IS_CONST1(u32 reg)
{
	return (reg < 32 && (s_hasConstReg & (1u << reg)));
}

static __fi bool PSX_IS_CONST2(u32 reg1, u32 reg2)
{
	return PSX_IS_CONST1(reg1) && PSX_IS_CONST1(reg2);
}

static __fi void PSX_SET_CONST(u32 reg, u32 value)
{
	if (reg == 0 || reg >= 32)
		return;

	s_constRegs[reg] = value;
	s_hasConstReg |= (1u << reg);
	s_flushedConstReg &= ~(1u << reg);
}

static __fi void PSX_DEL_CONST(u32 reg)
{
	if (reg == 0 || reg >= 32)
		return;

	s_hasConstReg &= ~(1u << reg);
}

static void _psxFlushConstRegs()
{
	for (u32 i = 1; i < 32; i++)
	{
		const u32 bit = (1u << i);
		if (!(s_hasConstReg & bit) || (s_flushedConstReg & bit))
			continue;

		if (s_constRegs[i] == 0)
		{
			armAsm->Str(a64::wzr, PSXGPR(i));
		}
		else
		{
			armAsm->Mov(RWTEMP1, s_constRegs[i]);
			armAsm->Str(RWTEMP1, PSXGPR(i));
		}

		s_flushedConstReg |= bit;
	}
}

// Loads a guest GPR into a host register.
static void _psxMoveGPRtoR(const a64::Register& to, u32 fromgpr)
{
	if (fromgpr == 0)
		armAsm->Mov(to, 0);
	else if (PSX_IS_CONST1(fromgpr))
		armAsm->Mov(to, s_constRegs[fromgpr]);
	else
		armAsm->Ldr(to, PSXGPR(fromgpr));
}

// Stores a host register to a guest GPR, and forgets any constant for it.
static void _psxMoveRtoGPR(u32 togpr, const a64::Register& from)
{
	if (togpr == 0)
		return;

	armAsm->Str(from, PSXGPR(togpr));
	PSX_DEL_CONST(togpr);
}

static void _psxStorePC(u32 pc)
{
	armAsm->Mov(RWTEMP1, pc);
	armAsm->Str(RWTEMP1, PSXREG_MEM(pc));
}

static void _psxStoreCode(u32 code)
{
	armAsm->Mov(RWTEMP1, code);
	armAsm->Str(RWTEMP1, PSXREG_MEM(code));
}

//////////////////////////////////////////////////////////////////////////
// Dispatchers
//////////////////////////////////////////////////////////////////////////

// Looks up the block for psxRegs.pc and jumps to it.
static void _DynGen_EmitDispatch()
{
	armAsm->Ldr(a64::w0, PSXREG_MEM(pc));
	armAsm->Lsr(a64::w1, a64::w0, 16);
	armMoveAddressToReg(a64::x2, psxRecLUT);
	armAsm->Ldr(a64::x1, a64::MemOperand(a64::x2, a64::x1, a64::LSL, 3));
	// BASEBLOCK is 8 bytes and indexed by pc / 4, so the offset is pc * 2.
	armAsm->Add(a64::x1, a64::x1, a64::Operand(a64::x0, a64::LSL, 1));
	armAsm->Ldr(a64::x1, a64::MemOperand(a64::x1));
	armAsm->Br(a64::x1);
}

static const void* _DynGen_DispatcherReg()
{
	armAlignAsmPtr();
	const void* retval = armGetCurrentCodePointer();
	_DynGen_EmitDispatch();
	return retval;
}

// The address for all cleared blocks. It recompiles the current pc and then
// dispatches to the recompiled block address.
static const void* _DynGen_JITCompile()
{
	pxAssertMsg(iopDispatcherReg != nullptr, "Please compile the DispatcherReg subroutine *before* JITCompile.");

	const void* retval = armGetCurrentCodePointer();
	armAsm->Ldr(a64::w0, PSXREG_MEM(pc));
	armEmitCall(reinterpret_cast<const void*>(iopRecRecompile));
	_DynGen_EmitDispatch();
	return retval;
}

static const void* _DynGen_EnterRecompiledCode()
{
	const void* retval = armGetCurrentCodePointer();

	armBeginStackFrame(false);
	armMoveAddressToReg(RPSXREGS, &psxRegs);
	armEmitJmp(iopDispatcherReg);

	iopExitRecompiledCode = armGetCurrentCodePointer();
	armEndStackFrame(false);
	armAsm->Ret();

	return retval;
}

static const void* _DynGen_UnmappedRecLUTPage()
{
	const void* retval = armGetCurrentCodePointer();

	armAsm->Mov(a64::w0, 0);
	armEmitCall(reinterpret_cast<const void*>(iopRecError));

	// Ideally iopRecError should not return, but it might if the EE rec's
	// ExitExecution deferred stopping until later
	armEmitJmp(iopExitRecompiledCode);

	return retval;
}

static void _DynGen_Dispatchers()
{
	const u8* start = armGetCurrentCodePointer();

	iopDispatcherReg = _DynGen_DispatcherReg();
	iopJITCompile = _DynGen_JITCompile();
	iopEnterRecompiledCode = _DynGen_EnterRecompiledCode();
	iopUnmappedRecLUTPage = _DynGen_UnmappedRecLUTPage();

	recBlocks.SetJITCompile(iopJITCompile);

	Perf::any.Register(start, armGetCurrentCodePointer() - start, "IOP Dispatcher");
}

static void iopRecError(int err)
{
	switch (err)
	{
		case 0:
			Host::ReportErrorAsync("R3000A Exception", fmt::format("Jump to unmapped recLUT page (PC: 0x{:08x})", psxRegs.pc));
			break;
		case 1:
			Host::ReportErrorAsync("R3000A Exception", fmt::format("Jump to unaligned address (PC: 0x{:08x})", psxRegs.pc));
			break;
	}

	VMManager::SetPaused(true);
	Cpu->ExitExecution();
}

//////////////////////////////////////////////////////////////////////////
// Cycle counting and branch tests
//////////////////////////////////////////////////////////////////////////

static constexpr u32 DYNAMIC_CYCLES = 0xFFFFFFFFu;

// Subtracts IOP cycles from iopCycleEE, converted to EE cycles. Flags are set from the new iopCycleEE value.
// When blockCycles is DYNAMIC_CYCLES, the IOP cycle delta is taken from RWTEMP2.
static void iPsxAddEECycles(u32 blockCycles)
{
	if (!(psxHu32(HW_ICFG) & (1 << 3))) [[likely]]
	{
		armAsm->Ldr(RWTEMP3, PSXREG_MEM(iopCycleEE));
		if (blockCycles != DYNAMIC_CYCLES)
			armAsm->Subs(RWTEMP3, RWTEMP3, blockCycles * 8);
		else
			armAsm->Subs(RWTEMP3, RWTEMP3, a64::Operand(RWTEMP2, a64::LSL, 3));
		armAsm->Str(RWTEMP3, PSXREG_MEM(iopCycleEE));
		return;
	}

	// F = gcd(PS2CLK, PSXCLK) = 230400
	const u32 cnum = 1280; // PS2CLK / F
	const u32 cdenom = 147; // PSXCLK / F

	if (blockCycles != DYNAMIC_CYCLES)
	{
		armAsm->Mov(RWTEMP3, blockCycles * cnum);
	}
	else
	{
		armAsm->Mov(RWTEMP4, cnum);
		armAsm->Mul(RWTEMP3, RWTEMP2, RWTEMP4);
	}

	armAsm->Ldr(RWTEMP4, PSXREG_MEM(iopCycleEECarry));
	armAsm->Add(RWTEMP3, RWTEMP3, RWTEMP4);
	armAsm->Mov(RWTEMP4, cdenom);
	armAsm->Udiv(RWTEMP5, RWTEMP3, RWTEMP4);
	armAsm->Msub(RWTEMP4, RWTEMP5, RWTEMP4, RWTEMP3);
	armAsm->Str(RWTEMP4, PSXREG_MEM(iopCycleEECarry));
	armAsm->Ldr(RWTEMP3, PSXREG_MEM(iopCycleEE));
	armAsm->Subs(RWTEMP3, RWTEMP3, RWTEMP5);
	armAsm->Str(RWTEMP3, PSXREG_MEM(iopCycleEE));
}

// Adds the block's cycles without testing for the end of the timeslice.
static void iPsxAddBlockCycles()
{
	armAsm->Ldr(RXTEMP1, PSXREG_MEM(cycle));
	armAsm->Add(RXTEMP1, RXTEMP1, s_psxBlockCycles);
	armAsm->Str(RXTEMP1, PSXREG_MEM(cycle));
	iPsxAddEECycles(s_psxBlockCycles);
}

static void iPsxBranchTest(u32 newpc)
{
	const u32 blockCycles = s_psxBlockCycles;

	if (EmuConfig.Speedhacks.WaitLoop && s_nBlockFF && newpc == s_branchTo)
	{
		// Skip ahead to the next event, or the end of the timeslice, whichever comes first.
		armAsm->Ldr(RXTEMP1, PSXREG_MEM(cycle));
		armAsm->Ldrsw(RXTEMP2, PSXREG_MEM(iopCycleEE));
		armAsm->Add(RXTEMP2, RXTEMP2, 7);
		armAsm->Asr(RXTEMP2, RXTEMP2, 3);
		armAsm->Add(RXTEMP2, RXTEMP1, RXTEMP2);
		armAsm->Ldr(RXTEMP3, PSXREG_MEM(iopNextEventCycle));
		armAsm->Cmp(RXTEMP2, RXTEMP3);
		armAsm->Csel(RXTEMP2, RXTEMP3, RXTEMP2, a64::pl);
		armAsm->Str(RXTEMP2, PSXREG_MEM(cycle));
		armAsm->Sub(RXTEMP2, RXTEMP2, RXTEMP1);
		iPsxAddEECycles(DYNAMIC_CYCLES);
		armEmitCondBranch(a64::le, iopExitRecompiledCode);

		armEmitCall(reinterpret_cast<const void*>(iopEventTest));

		if (newpc != 0xffffffff)
		{
			armAsm->Ldr(RWTEMP1, PSXREG_MEM(pc));
			armAsm->Cmp(RWTEMP1, newpc);
			armEmitCondBranch(a64::ne, iopDispatcherReg);
		}
	}
	else
	{
		armAsm->Ldr(RXTEMP1, PSXREG_MEM(cycle));
		armAsm->Add(RXTEMP1, RXTEMP1, blockCycles);
		armAsm->Str(RXTEMP1, PSXREG_MEM(cycle));

		// jump if iopCycleEE <= 0  (iop's timeslice timed out, so time to return control to the EE)
		iPsxAddEECycles(blockCycles);
		armEmitCondBranch(a64::le, iopExitRecompiledCode);

		// check if an event is pending
		a64::Label nointerruptpending;
		armAsm->Ldr(RXTEMP2, PSXREG_MEM(iopNextEventCycle));
		armAsm->Cmp(RXTEMP1, RXTEMP2);
		armAsm->B(&nointerruptpending, a64::mi);

		armEmitCall(reinterpret_cast<const void*>(iopEventTest));

		if (newpc != 0xffffffff)
		{
			armAsm->Ldr(RWTEMP1, PSXREG_MEM(pc));
			armAsm->Cmp(RWTEMP1, newpc);
			armEmitCondBranch(a64::ne, iopDispatcherReg);
		}

		armAsm->Bind(&nointerruptpending);
	}
}

// Emits a B to the block at pc, which gets patched when that block is (re)compiled or cleared.
static void psxEmitBlockLink(u32 pc)
{
	a64::SingleEmissionCheckScope guard(armAsm);
	u32* branch_ptr = reinterpret_cast<u32*>(armGetCurrentCodePointer());
	armAsm->b(static_cast<int64_t>(0)); // placeholder, patched by Link()
	recBlocks.Link(HWADDR(pc), branch_ptr);
}

static void psxSetBranchImm(u32 imm)
{
	psxbranch = 1;
	pxAssert(imm);

	// end the current block
	_psxStorePC(imm);
	_psxFlushConstRegs();
	iPsxBranchTest(imm);

	psxEmitBlockLink(imm);
}

// Target is in RWBRANCH.
static void psxSetBranchReg()
{
	psxbranch = 1;

	armAsm->Str(RWBRANCH, PSXREG_MEM(pc));
	_psxFlushConstRegs();

	a64::Label unaligned;
	armAsm->Tst(RWBRANCH, 3);
	armAsm->B(&unaligned, a64::ne);

	iPsxBranchTest(0xffffffff);
	armEmitJmp(iopDispatcherReg);

	armAsm->Bind(&unaligned);
	armAsm->Mov(a64::w0, 1);
	armEmitCall(reinterpret_cast<const void*>(iopRecError));
	// Ideally iopRecError should not return, but it might if the EE rec's
	// ExitExecution deferred stopping until later
	armEmitJmp(iopExitRecompiledCode);
}

//////////////////////////////////////////////////////////////////////////
// Instruction helpers
//////////////////////////////////////////////////////////////////////////

// Calls an interpreter implementation. The interpreter reads/writes psxRegs directly, so
// constants are flushed first and any register it may write is no longer constant afterwards.
static void rpsxInterpret(void (*func)())
{
	_psxFlushConstRegs();
	_psxStoreCode(psxRegs.code);
	armEmitCall(reinterpret_cast<const void*>(func));
	PSX_DEL_CONST(_Rt_);
	PSX_DEL_CONST(_Rd_);
}

static void psxRecompileIrxImport()
{
	const u32 import_table = R3000A::irxImportTableAddr(psxpc - 4);
	const u16 index = psxRegs.code & 0xffff;
	if (!import_table)
		return;

	const std::string libname = iopMemReadString(import_table + 12, 8);

	irxHLE hle = R3000A::irxImportHLE(libname, index);
#ifdef PCSX2_DEVBUILD
	const irxDEBUG debug = R3000A::irxImportDebug(libname, index);
	const char* funcname = R3000A::irxImportFuncname(libname, index);
#else
	const irxDEBUG debug = 0;
	const char* funcname = nullptr;
#endif

	if (!hle && !debug && (!TraceActive(IOP.Bios) || !funcname))
		return;

	_psxStoreCode(psxRegs.code);
	_psxStorePC(psxpc);
	_psxFlushConstRegs();

	if (TraceActive(IOP.Bios))
	{
		armAsm->Mov(a64::w0, import_table);
		armAsm->Mov(a64::w1, index);
		armAsm->Mov(a64::x2, reinterpret_cast<uptr>(funcname));
		armEmitCall(reinterpret_cast<const void*>(R3000A::irxImportLog_rec));
	}

	if (debug)
		armEmitCall(reinterpret_cast<const void*>(debug));

	if (hle)
	{
		armEmitCall(reinterpret_cast<const void*>(hle));
		armEmitCbnz(a64::w0, iopDispatcherReg);
	}
}

//////////////////////////////////////////////////////////////////////////
// ALU
//////////////////////////////////////////////////////////////////////////

enum class ImmOp
{
	ADDIU,
	SLTI,
	SLTIU,
	ANDI,
	ORI,
	XORI,
};

// rt = rs op imm16
static void rpsxImmOp(ImmOp op)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 simm = static_cast<u32>(static_cast<s32>(_Imm_));
	const u32 zimm = _ImmU_;

	if (rt == 0)
	{
		// check for iop module import table magic
		if (psxRegs.code >> 16 == 0x2400)
			psxRecompileIrxImport();
		return;
	}

	if (PSX_IS_CONST1(rs))
	{
		const u32 s = s_constRegs[rs];
		u32 result = 0;
		switch (op)
		{
			case ImmOp::ADDIU: result = s + simm; break;
			case ImmOp::SLTI: result = static_cast<s32>(s) < static_cast<s32>(simm); break;
			case ImmOp::SLTIU: result = s < simm; break;
			case ImmOp::ANDI: result = s & zimm; break;
			case ImmOp::ORI: result = s | zimm; break;
			case ImmOp::XORI: result = s ^ zimm; break;
		}
		PSX_SET_CONST(rt, result);
		return;
	}

	_psxMoveGPRtoR(RWTEMP2, rs);
	switch (op)
	{
		case ImmOp::ADDIU:
			armAsm->Add(RWTEMP2, RWTEMP2, simm);
			break;
		case ImmOp::SLTI:
			armAsm->Cmp(RWTEMP2, a64::Operand(static_cast<s64>(static_cast<s32>(simm))));
			armAsm->Cset(RWTEMP2, a64::lt);
			break;
		case ImmOp::SLTIU:
			armAsm->Cmp(RWTEMP2, a64::Operand(static_cast<u64>(simm)));
			armAsm->Cset(RWTEMP2, a64::lo);
			break;
		case ImmOp::ANDI:
			armAsm->And(RWTEMP2, RWTEMP2, zimm);
			break;
		case ImmOp::ORI:
			armAsm->Orr(RWTEMP2, RWTEMP2, zimm);
			break;
		case ImmOp::XORI:
			armAsm->Eor(RWTEMP2, RWTEMP2, zimm);
			break;
	}
	_psxMoveRtoGPR(rt, RWTEMP2);
}

static void rpsxLUI()
{
	if (!_Rt_)
		return;

	PSX_SET_CONST(_Rt_, psxRegs.code << 16);
}

enum class RegOp
{
	ADDU,
	SUBU,
	AND,
	OR,
	XOR,
	NOR,
	SLT,
	SLTU,
};

// rd = rs op rt
static void rpsxRegOp(RegOp op)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	if (rd == 0)
		return;

	if (PSX_IS_CONST2(rs, rt))
	{
		const u32 s = s_constRegs[rs];
		const u32 t = s_constRegs[rt];
		u32 result = 0;
		switch (op)
		{
			case RegOp::ADDU: result = s + t; break;
			case RegOp::SUBU: result = s - t; break;
			case RegOp::AND: result = s & t; break;
			case RegOp::OR: result = s | t; break;
			case RegOp::XOR: result = s ^ t; break;
			case RegOp::NOR: result = ~(s | t); break;
			case RegOp::SLT: result = static_cast<s32>(s) < static_cast<s32>(t); break;
			case RegOp::SLTU: result = s < t; break;
		}
		PSX_SET_CONST(rd, result);
		return;
	}

	_psxMoveGPRtoR(RWTEMP2, rs);
	_psxMoveGPRtoR(RWTEMP3, rt);
	switch (op)
	{
		case RegOp::ADDU:
			armAsm->Add(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
		case RegOp::SUBU:
			armAsm->Sub(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
		case RegOp::AND:
			armAsm->And(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
		case RegOp::OR:
			armAsm->Orr(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
		case RegOp::XOR:
			armAsm->Eor(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
		case RegOp::NOR:
			armAsm->Orr(RWTEMP2, RWTEMP2, RWTEMP3);
			armAsm->Mvn(RWTEMP2, RWTEMP2);
			break;
		case RegOp::SLT:
			armAsm->Cmp(RWTEMP2, RWTEMP3);
			armAsm->Cset(RWTEMP2, a64::lt);
			break;
		case RegOp::SLTU:
			armAsm->Cmp(RWTEMP2, RWTEMP3);
			armAsm->Cset(RWTEMP2, a64::lo);
			break;
	}
	_psxMoveRtoGPR(rd, RWTEMP2);
}

enum class ShiftOp
{
	SLL,
	SRL,
	SRA,
};

// rd = rt op sa
static void rpsxShiftImm(ShiftOp op)
{
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	const u32 sa = _Sa_;
	if (rd == 0)
		return;

	if (PSX_IS_CONST1(rt))
	{
		const u32 t = s_constRegs[rt];
		u32 result = 0;
		switch (op)
		{
			case ShiftOp::SLL: result = t << sa; break;
			case ShiftOp::SRL: result = t >> sa; break;
			case ShiftOp::SRA: result = static_cast<u32>(static_cast<s32>(t) >> sa); break;
		}
		PSX_SET_CONST(rd, result);
		return;
	}

	_psxMoveGPRtoR(RWTEMP2, rt);
	switch (op)
	{
		case ShiftOp::SLL:
			armAsm->Lsl(RWTEMP2, RWTEMP2, sa);
			break;
		case ShiftOp::SRL:
			armAsm->Lsr(RWTEMP2, RWTEMP2, sa);
			break;
		case ShiftOp::SRA:
			armAsm->Asr(RWTEMP2, RWTEMP2, sa);
			break;
	}
	_psxMoveRtoGPR(rd, RWTEMP2);
}

// rd = rt op rs
static void rpsxShiftVar(ShiftOp op)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	if (rd == 0)
		return;

	if (PSX_IS_CONST2(rs, rt))
	{
		// Shift amounts are masked to 5 bits, same as the host (and x86).
		const u32 t = s_constRegs[rt];
		const u32 sa = s_constRegs[rs] & 0x1f;
		u32 result = 0;
		switch (op)
		{
			case ShiftOp::SLL: result = t << sa; break;
			case ShiftOp::SRL: result = t >> sa; break;
			case ShiftOp::SRA: result = static_cast<u32>(static_cast<s32>(t) >> sa); break;
		}
		PSX_SET_CONST(rd, result);
		return;
	}

	_psxMoveGPRtoR(RWTEMP2, rt);
	_psxMoveGPRtoR(RWTEMP3, rs);
	switch (op)
	{
		case ShiftOp::SLL:
			armAsm->Lsl(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
		case ShiftOp::SRL:
			armAsm->Lsr(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
		case ShiftOp::SRA:
			armAsm->Asr(RWTEMP2, RWTEMP2, RWTEMP3);
			break;
	}
	_psxMoveRtoGPR(rd, RWTEMP2);
}

static void rpsxMFHILO(u32 reg)
{
	if (!_Rd_)
		return;

	armAsm->Ldr(RWTEMP2, PSXGPR(reg));
	_psxMoveRtoGPR(_Rd_, RWTEMP2);
}

static void rpsxMTHILO(u32 reg)
{
	_psxMoveGPRtoR(RWTEMP2, _Rs_);
	armAsm->Str(RWTEMP2, PSXGPR(reg));
}

static void rpsxMULT(bool is_signed)
{
	const u32 rs = _Rs_;
	const u32 rt = _Rt_;

	if (PSX_IS_CONST2(rs, rt))
	{
		const u64 res = is_signed ?
							static_cast<u64>(static_cast<s64>(static_cast<s32>(s_constRegs[rs])) * static_cast<s64>(static_cast<s32>(s_constRegs[rt]))) :
							(static_cast<u64>(s_constRegs[rs]) * static_cast<u64>(s_constRegs[rt]));
		armAsm->Mov(RXTEMP2, res);
	}
	else
	{
		_psxMoveGPRtoR(RWTEMP2, rs);
		_psxMoveGPRtoR(RWTEMP3, rt);
		if (is_signed)
			armAsm->Smull(RXTEMP2, RWTEMP2, RWTEMP3);
		else
			armAsm->Umull(RXTEMP2, RWTEMP2, RWTEMP3);
	}

	// LO is r[33] and HI is r[32].
	armAsm->Str(RWTEMP2, PSXGPR(PSX_LO));
	armAsm->Lsr(RXTEMP2, RXTEMP2, 32);
	armAsm->Str(RWTEMP2, PSXGPR(PSX_HI));

	g_iopCyclePenalty = psxInstCycles_Mult;
}

static void rpsxDIV(bool is_signed)
{
	rpsxInterpret(psxSPC[is_signed ? 26 : 27]);
	g_iopCyclePenalty = psxInstCycles_Div;
}

//////////////////////////////////////////////////////////////////////////
// Loads and stores
//////////////////////////////////////////////////////////////////////////

// w0 = rs + imm
static void rpsxCalcAddressOperand()
{
	const u32 rs = _Rs_;
	if (PSX_IS_CONST1(rs))
	{
		armAsm->Mov(a64::w0, s_constRegs[rs] + static_cast<u32>(static_cast<s32>(_Imm_)));
	}
	else
	{
		_psxMoveGPRtoR(a64::w0, rs);
		if (_Imm_ != 0)
			armAsm->Add(a64::w0, a64::w0, static_cast<u32>(static_cast<s32>(_Imm_)));
	}
}

static void rpsxLoad(int size, bool sign)
{
	const u32 rt = _Rt_;
	rpsxCalcAddressOperand();

	if (rt != 0)
		PSX_DEL_CONST(rt);

	a64::Label is_hw_read, done;
	armAsm->Tst(a64::w0, 0x10000000);
	armAsm->B(&is_hw_read, a64::ne);

	if (rt != 0)
	{
		// read from IOP RAM directly
		armAsm->And(a64::w1, a64::w0, Ps2MemSize::ExposedIopRam - 1);
		armMoveAddressToReg(a64::x2, iopMem->Main);
		const a64::MemOperand mem(a64::x2, a64::x1);
		switch (size)
		{
			case 8:
				sign ? armAsm->Ldrsb(RWTEMP2, mem) : armAsm->Ldrb(RWTEMP2, mem);
				break;
			case 16:
				sign ? armAsm->Ldrsh(RWTEMP2, mem) : armAsm->Ldrh(RWTEMP2, mem);
				break;
			case 32:
				armAsm->Ldr(RWTEMP2, mem);
				break;
				jNO_DEFAULT
		}
		armAsm->B(&done);
	}
	else
	{
		// dummy read, only needed for side effects of hardware registers.
		armAsm->B(&done);
	}

	armAsm->Bind(&is_hw_read);
	switch (size)
	{
		case 8:
			armEmitCall(reinterpret_cast<const void*>(iopMemRead8));
			if (rt != 0)
				sign ? armAsm->Sxtb(RWTEMP2, a64::w0) : armAsm->Uxtb(RWTEMP2, a64::w0);
			break;
		case 16:
			armEmitCall(reinterpret_cast<const void*>(iopMemRead16));
			if (rt != 0)
				sign ? armAsm->Sxth(RWTEMP2, a64::w0) : armAsm->Uxth(RWTEMP2, a64::w0);
			break;
		case 32:
			armEmitCall(reinterpret_cast<const void*>(iopMemRead32));
			if (rt != 0)
				armAsm->Mov(RWTEMP2, a64::w0);
			break;
			jNO_DEFAULT
	}

	armAsm->Bind(&done);
	if (rt != 0)
		_psxMoveRtoGPR(rt, RWTEMP2);
}

static void rpsxStore(int size)
{
	rpsxCalcAddressOperand();
	_psxMoveGPRtoR(a64::w1, _Rt_);

	// Apple's ABI requires the caller to extend narrow arguments.
	switch (size)
	{
		case 8:
			armAsm->Uxtb(a64::w1, a64::w1);
			armEmitCall(reinterpret_cast<const void*>(iopMemWrite8));
			break;
		case 16:
			armAsm->Uxth(a64::w1, a64::w1);
			armEmitCall(reinterpret_cast<const void*>(iopMemWrite16));
			break;
		case 32:
			armEmitCall(reinterpret_cast<const void*>(iopMemWrite32));
			break;
			jNO_DEFAULT
	}
}

//////////////////////////////////////////////////////////////////////////
// Jumps and branches
//////////////////////////////////////////////////////////////////////////

static void rpsxJ()
{
	// j target
	const u32 newpc = _InstrucTarget_ * 4 + (psxpc & 0xf0000000);
	psxRecompileNextInstruction(true);
	psxSetBranchImm(newpc);
}

static void rpsxJAL()
{
	const u32 newpc = (_InstrucTarget_ << 2) + (psxpc & 0xf0000000);
	PSX_SET_CONST(31, psxpc + 4);

	psxRecompileNextInstruction(true);
	psxSetBranchImm(newpc);
}

static void rpsxJR()
{
	_psxMoveGPRtoR(RWBRANCH, _Rs_);
	psxRecompileNextInstruction(true);
	psxSetBranchReg();
}

static void rpsxJALR()
{
	// Read the target before the link, in case rd == rs.
	_psxMoveGPRtoR(RWBRANCH, _Rs_);
	if (_Rd_)
		PSX_SET_CONST(_Rd_, psxpc + 4);

	psxRecompileNextInstruction(true);
	psxSetBranchReg();
}

enum class BranchCond
{
	EQ,
	NE,
	LEZ,
	GTZ,
	LTZ,
	GEZ,
};

static bool psxEvalBranchCond(BranchCond cond, u32 s, u32 t)
{
	switch (cond)
	{
		case BranchCond::EQ: return (s == t);
		case BranchCond::NE: return (s != t);
		case BranchCond::LEZ: return (static_cast<s32>(s) <= 0);
		case BranchCond::GTZ: return (static_cast<s32>(s) > 0);
		case BranchCond::LTZ: return (static_cast<s32>(s) < 0);
		case BranchCond::GEZ: return (static_cast<s32>(s) >= 0);
		jNO_DEFAULT
	}
}

static void rpsxBranch(BranchCond cond, bool link)
{
	const u32 rs = _Rs_;
	const u32 rt = (cond == BranchCond::EQ || cond == BranchCond::NE) ? _Rt_ : 0;
	const u32 branchTo = static_cast<u32>(static_cast<s32>(_Imm_) * 4) + psxpc;

	// The interpreter (and x86 rec) sets the link register before evaluating the condition.
	if (link)
		PSX_SET_CONST(31, psxpc + 4);

	const bool known = (cond == BranchCond::EQ || cond == BranchCond::NE) ?
						   (rs == rt || PSX_IS_CONST2(rs, rt)) :
						   PSX_IS_CONST1(rs);
	if (known)
	{
		const u32 s = PSX_IS_CONST1(rs) ? s_constRegs[rs] : 0;
		const u32 t = PSX_IS_CONST1(rt) ? s_constRegs[rt] : 0;
		const bool taken = (rs == rt && (cond == BranchCond::EQ || cond == BranchCond::NE)) ?
							   (cond == BranchCond::EQ) :
							   psxEvalBranchCond(cond, s, t);
		const u32 target = taken ? branchTo : (psxpc + 4);

		psxRecompileNextInstruction(true);
		psxSetBranchImm(target);
		return;
	}

	_psxFlushConstRegs();

	// Evaluate the condition before the delay slot, which may overwrite the source registers.
	_psxMoveGPRtoR(RWTEMP2, rs);
	switch (cond)
	{
		case BranchCond::EQ:
		case BranchCond::NE:
			_psxMoveGPRtoR(RWTEMP3, rt);
			armAsm->Cmp(RWTEMP2, RWTEMP3);
			armAsm->Cset(RWBRANCH, (cond == BranchCond::EQ) ? a64::eq : a64::ne);
			break;
		case BranchCond::LEZ:
			armAsm->Cmp(RWTEMP2, 0);
			armAsm->Cset(RWBRANCH, a64::le);
			break;
		case BranchCond::GTZ:
			armAsm->Cmp(RWTEMP2, 0);
			armAsm->Cset(RWBRANCH, a64::gt);
			break;
		case BranchCond::LTZ:
			armAsm->Cmp(RWTEMP2, 0);
			armAsm->Cset(RWBRANCH, a64::lt);
			break;
		case BranchCond::GEZ:
			armAsm->Cmp(RWTEMP2, 0);
			armAsm->Cset(RWBRANCH, a64::ge);
			break;
	}

	psxRecompileNextInstruction(true);

	// Both exits must flush the same constants, so restore the flush state before the second.
	const u32 saved_flushed = s_flushedConstReg;

	a64::Label not_taken;
	armAsm->Cbz(RWBRANCH, &not_taken);
	psxSetBranchImm(branchTo);

	armAsm->Bind(&not_taken);
	s_flushedConstReg = saved_flushed;
	psxSetBranchImm(psxpc);
}

//////////////////////////////////////////////////////////////////////////
// Exceptions
//////////////////////////////////////////////////////////////////////////

static void rpsxException(u32 code)
{
	_psxStoreCode(psxRegs.code);
	_psxStorePC(psxpc - 4);
	_psxFlushConstRegs();

	armAsm->Mov(a64::w0, code);
	armAsm->Mov(a64::w1, static_cast<u32>(psxbranch == 1));
	armEmitCall(reinterpret_cast<const void*>(psxException));

	a64::Label no_exception;
	armAsm->Ldr(RWTEMP1, PSXREG_MEM(pc));
	armAsm->Cmp(RWTEMP1, psxpc - 4);
	armAsm->B(&no_exception, a64::eq);

	iPsxAddBlockCycles();
	armEmitJmp(iopDispatcherReg);

	// jump target for skipping blockCycle updates
	armAsm->Bind(&no_exception);
}

//////////////////////////////////////////////////////////////////////////
// COP0
//////////////////////////////////////////////////////////////////////////

static void rpsxMFC0()
{
	// Rt = Cop0->Rd
	if (!_Rt_)
		return;

	armAsm->Ldr(RWTEMP2, PSXCP0(_Rd_));
	_psxMoveRtoGPR(_Rt_, RWTEMP2);
}

static void rpsxMTC0()
{
	// Cop0->Rd = Rt
	_psxMoveGPRtoR(RWTEMP2, _Rt_);
	armAsm->Str(RWTEMP2, PSXCP0(_Rd_));
}

static void rpsxRFE()
{
	armAsm->Ldr(RWTEMP2, PSXCP0(12)); // Status
	armAsm->And(RWTEMP3, RWTEMP2, 0x3c);
	armAsm->And(RWTEMP2, RWTEMP2, 0xfffffff0);
	armAsm->Orr(RWTEMP2, RWTEMP2, a64::Operand(RWTEMP3, a64::LSR, 2));
	armAsm->Str(RWTEMP2, PSXCP0(12));

	// Test the IOP's INTC status, so that any pending ints get raised.
	_psxFlushConstRegs();
	armEmitCall(reinterpret_cast<const void*>(&iopTestIntc));
}

//////////////////////////////////////////////////////////////////////////
// Decoder
//////////////////////////////////////////////////////////////////////////

static void rpsxNULL()
{
	Console.WriteLn("psxUNK: %8.8x", psxRegs.code);
}

static void rpsxSPECIAL()
{
	switch (_Funct_)
	{
		case 0: rpsxShiftImm(ShiftOp::SLL); break;
		case 2: rpsxShiftImm(ShiftOp::SRL); break;
		case 3: rpsxShiftImm(ShiftOp::SRA); break;
		case 4: rpsxShiftVar(ShiftOp::SLL); break;
		case 6: rpsxShiftVar(ShiftOp::SRL); break;
		case 7: rpsxShiftVar(ShiftOp::SRA); break;
		case 8: rpsxJR(); break;
		case 9: rpsxJALR(); break;
		case 12: rpsxException(0x20); break; // SYSCALL
		case 13: rpsxException(0x24); break; // BREAK
		case 16: rpsxMFHILO(PSX_HI); break;
		case 17: rpsxMTHILO(PSX_HI); break;
		case 18: rpsxMFHILO(PSX_LO); break;
		case 19: rpsxMTHILO(PSX_LO); break;
		case 24: rpsxMULT(true); break;
		case 25: rpsxMULT(false); break;
		case 26: rpsxDIV(true); break;
		case 27: rpsxDIV(false); break;
		case 32: rpsxRegOp(RegOp::ADDU); break; // ADD, no overflow exception (same as x86)
		case 33: rpsxRegOp(RegOp::ADDU); break;
		case 34: rpsxRegOp(RegOp::SUBU); break; // SUB, no overflow exception (same as x86)
		case 35: rpsxRegOp(RegOp::SUBU); break;
		case 36: rpsxRegOp(RegOp::AND); break;
		case 37: rpsxRegOp(RegOp::OR); break;
		case 38: rpsxRegOp(RegOp::XOR); break;
		case 39: rpsxRegOp(RegOp::NOR); break;
		case 42: rpsxRegOp(RegOp::SLT); break;
		case 43: rpsxRegOp(RegOp::SLTU); break;
		default: rpsxNULL(); break;
	}
}

static void rpsxREGIMM()
{
	switch (_Rt_)
	{
		case 0: rpsxBranch(BranchCond::LTZ, false); break;
		case 1: rpsxBranch(BranchCond::GEZ, false); break;
		case 16: rpsxBranch(BranchCond::LTZ, true); break;
		case 17: rpsxBranch(BranchCond::GEZ, true); break;
		default: rpsxNULL(); break;
	}
}

static void rpsxCOP0()
{
	switch (_Rs_)
	{
		case 0: // MFC0
		case 2: // CFC0
			rpsxMFC0();
			break;
		case 4: // MTC0
		case 6: // CTC0
			rpsxMTC0();
			break;
		case 16:
			rpsxRFE();
			break;
		default:
			rpsxNULL();
			break;
	}
}

static void rpsxCOP2()
{
	// GTE is handled by the interpreter, same as x86.
	if (psxCP2[_Funct_] == psxNULL)
		rpsxNULL();
	else
		rpsxInterpret(psxCP2[_Funct_]);
}

static void rpsxCompileInstruction()
{
	switch (psxRegs.code >> 26)
	{
		case 0: rpsxSPECIAL(); break;
		case 1: rpsxREGIMM(); break;
		case 2: rpsxJ(); break;
		case 3: rpsxJAL(); break;
		case 4: rpsxBranch(BranchCond::EQ, false); break;
		case 5: rpsxBranch(BranchCond::NE, false); break;
		case 6: rpsxBranch(BranchCond::LEZ, false); break;
		case 7: rpsxBranch(BranchCond::GTZ, false); break;
		case 8: rpsxImmOp(ImmOp::ADDIU); break; // ADDI, no overflow exception (same as x86)
		case 9: rpsxImmOp(ImmOp::ADDIU); break;
		case 10: rpsxImmOp(ImmOp::SLTI); break;
		case 11: rpsxImmOp(ImmOp::SLTIU); break;
		case 12: rpsxImmOp(ImmOp::ANDI); break;
		case 13: rpsxImmOp(ImmOp::ORI); break;
		case 14: rpsxImmOp(ImmOp::XORI); break;
		case 15: rpsxLUI(); break;
		case 16: rpsxCOP0(); break;
		case 18: rpsxCOP2(); break;
		case 32: rpsxLoad(8, true); break;
		case 33: rpsxLoad(16, true); break;
		case 34: rpsxInterpret(psxBSC[34]); break; // LWL
		case 35: rpsxLoad(32, false); break;
		case 36: rpsxLoad(8, false); break;
		case 37: rpsxLoad(16, false); break;
		case 38: rpsxInterpret(psxBSC[38]); break; // LWR
		case 40: rpsxStore(8); break;
		case 41: rpsxStore(16); break;
		case 42: rpsxInterpret(psxBSC[42]); break; // SWL
		case 43: rpsxStore(32); break;
		case 46: rpsxInterpret(psxBSC[46]); break; // SWR
		case 50: rpsxInterpret(psxBSC[50]); break; // LWC2
		case 58: rpsxInterpret(psxBSC[58]); break; // SWC2
		default: rpsxNULL(); break;
	}
}

//////////////////////////////////////////////////////////////////////////
// Debugger support
//////////////////////////////////////////////////////////////////////////

static bool psxDynarecCheckBreakpoint()
{
	u32 pc = psxRegs.pc;
	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_IOP, pc) == pc)
	{
		CBreakPoints::ClearSkipFirst(BREAKPOINT_IOP);
		return false;
	}

	int bpFlags = psxIsBreakpointNeeded(pc);
	bool hit = false;
	//check breakpoint at current pc
	if (bpFlags & 1)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_IOP, pc);
		if (cond == NULL || cond->Evaluate())
		{
			if (CBreakPoints::HandleBreakpointHit(BREAKPOINT_IOP, pc))
				hit = true;
		}
	}
	//check breakpoint in delay slot
	if (bpFlags & 2)
	{
		auto cond = CBreakPoints::GetBreakPointCondition(BREAKPOINT_IOP, pc + 4);
		if (cond == NULL || cond->Evaluate())
			if (CBreakPoints::HandleBreakpointHit(BREAKPOINT_IOP, pc + 4))
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

static bool psxDynarecMemcheck(size_t i)
{
	const u32 pc = psxRegs.pc;
	const auto mc = CBreakPoints::GetMemChecks(BREAKPOINT_IOP)[i];

	if (CBreakPoints::CheckSkipFirst(BREAKPOINT_IOP, pc) == pc)
	{
		CBreakPoints::ClearSkipFirst(BREAKPOINT_IOP);
		return false;
	}
	if (mc.hasCond)
	{
		if (!mc.cond.Evaluate())
			return false;
	}

	if (!CBreakPoints::HandleMemCheckHit(BREAKPOINT_IOP, mc.start, mc.end))
		return false;

	CBreakPoints::SetBreakpointTriggered(true, BREAKPOINT_IOP);
	VMManager::SetPaused(true);

	// Exit the EE too.
	Cpu->ExitExecution();
	return true;
}

static void psxRecMemcheck(u32 op, u32 bits, bool store)
{
	_psxStorePC(psxpc);
	_psxFlushConstRegs();

	const auto checks = CBreakPoints::GetMemChecks(BREAKPOINT_IOP);
	for (size_t i = 0; i < checks.size(); i++)
	{
		if (checks[i].result == 0)
			continue;
		if ((checks[i].memCond & MEMCHECK_WRITE) == 0 && store)
			continue;
		if ((checks[i].memCond & MEMCHECK_READ) == 0 && !store)
			continue;

		// Recompute the address each time, the call below clobbers the temporaries.
		// w9 = access address, w10 = access address + size
		armAsm->Ldr(RWTEMP2, PSXGPR((op >> 21) & 0x1F));
		if (static_cast<s16>(op) != 0)
			armAsm->Add(RWTEMP2, RWTEMP2, static_cast<u32>(static_cast<s32>(static_cast<s16>(op))));
		armAsm->Add(RWTEMP3, RWTEMP2, bits / 8);

		// logic: memAddress < bpEnd && bpStart < memAddress+memSize
		a64::Label next;
		armAsm->Mov(RWTEMP4, checks[i].end);
		armAsm->Cmp(RWTEMP2, RWTEMP4);
		armAsm->B(&next, a64::ge);
		armAsm->Mov(RWTEMP4, checks[i].start);
		armAsm->Cmp(RWTEMP4, RWTEMP3);
		armAsm->B(&next, a64::ge);

		// hit the breakpoint
		if (checks[i].result & MEMCHECK_BREAK)
		{
			armAsm->Mov(a64::x0, i);
			armEmitCall(reinterpret_cast<const void*>(psxDynarecMemcheck));
			armAsm->Tst(a64::w0, 0xff);
			armEmitCondBranch(a64::ne, iopExitRecompiledCode);
		}

		armAsm->Bind(&next);
	}
}

static bool psxEncodeBreakpoint()
{
	if (psxIsBreakpointNeeded(psxpc) != 0)
	{
		_psxStorePC(psxpc);
		_psxFlushConstRegs();
		armEmitCall(reinterpret_cast<const void*>(psxDynarecCheckBreakpoint));
		armAsm->Tst(a64::w0, 0xff);
		armEmitCondBranch(a64::ne, iopExitRecompiledCode);
		return true;
	}

	return false;
}

static bool psxEncodeMemcheck()
{
	int needed = psxIsMemcheckNeeded(psxpc);
	if (needed == 0)
		return false;

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
	return true;
}

static void psxRecompileNextInstruction(bool delayslot)
{
	if (!delayslot)
	{
		if (psxEncodeBreakpoint() || psxEncodeMemcheck())
		{
			armAsm->Mov(a64::w0, static_cast<u32>(BREAKPOINT_IOP));
			armEmitCall(reinterpret_cast<const void*>(&CBreakPoints::CommitClearSkipFirst));
		}
	}

	psxRegs.code = iopMemRead32(psxpc);
	s_psxBlockCycles++;
	psxpc += 4;

	g_iopCyclePenalty = 0;
	rpsxCompileInstruction();
	s_psxBlockCycles += g_iopCyclePenalty;
}

//////////////////////////////////////////////////////////////////////////
// Block management
//////////////////////////////////////////////////////////////////////////

static DynamicHeapArray<BASEBLOCK, 4096> recLutReserve;
static DynamicHeapArray<BASEBLOCK, 4096> recLutUnmapped;
static size_t recLutEntries;
static bool extraRam = false;

static void recReserveRAM()
{
	// Goal: Allocate BASEBLOCKs for every possible branch target in IOP memory.
	// Any 4-byte aligned address makes a valid branch target as per MIPS design (all instructions are
	// always 4 bytes long).

	recLutEntries =
		((Ps2MemSize::ExposedIopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4);

	if (recLutReserve.size() != recLutEntries)
		recLutReserve.resize(recLutEntries);

	recLutUnmapped.resize(_64kb / 4);

	BASEBLOCK* curpos = recLutReserve.data();
	recRAM = curpos;
	curpos += (Ps2MemSize::ExposedIopRam / 4);
	recROM = curpos;
	curpos += (Ps2MemSize::Rom / 4);
	recROM1 = curpos;
	curpos += (Ps2MemSize::Rom1 / 4);
	recROM2 = curpos;
	curpos += (Ps2MemSize::Rom2 / 4);
}

static void recReserve()
{
	recPtr = SysMemory::GetIOPRec();
	recPtrEnd = SysMemory::GetIOPRecEnd() - _64kb;

	recReserveRAM();
}

static void recResetIOP()
{
	DevCon.WriteLn("iR3000A ARM64 Recompiler reset.");

	if (CHECK_EXTRAMEM != extraRam)
	{
		recReserveRAM();
		extraRam = !extraRam;
	}

	armSetAsmPtr(SysMemory::GetIOPRec(), SysMemory::GetIOPRecEnd() - SysMemory::GetIOPRec(), nullptr);
	armStartBlock();
	_DynGen_Dispatchers();
	recPtr = armEndBlock();

	iopClearRecLUT(reinterpret_cast<BASEBLOCK*>(recLutReserve.data()),
		Ps2MemSize::ExposedIopRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2);

	BASEBLOCK* unmapped = recLutUnmapped.data();

	for (int i = 0; i < 0x10000; i++)
	{
		recLUT_SetPage(psxRecLUT, psxhwLUT, unmapped, i, 0, 0);
	}

	for (int i = 0; i < _64kb / 4; i++)
	{
		unmapped[i].SetFnptr((uptr)iopUnmappedRecLUTPage);
	}

	// IOP knows 64k pages, hence for the 0x10000's

	// The bottom 2 bits of PC are always zero, so we <<14 to "compress"
	// the pc indexer into it's lower common denominator.

	// We're only mapping 20 pages here in 4 places.
	// 0x80 comes from : (Ps2MemSize::IopRam / _64kb) * 4

	for (int i = 0; i < 0x80; i++)
	{
		u32 mask = (Ps2MemSize::ExposedIopRam / _64kb) - 1;

		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x0000, i, i & mask);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0x8000, i, i & mask);
		recLUT_SetPage(psxRecLUT, psxhwLUT, recRAM, 0xa000, i, i & mask);
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

	recBlocks.Reset();
	g_psxMaxRecMem = 0;

	psxbranch = 0;
}

static void recShutdown()
{
	recLutReserve.deallocate();

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static void iopClearRecLUT(BASEBLOCK* base, int count)
{
	for (int i = 0; i < count / 4; i++)
		base[i].SetFnptr((uptr)iopJITCompile);
}

static __noinline s32 recExecuteBlock(s32 eeCycles)
{
	psxRegs.iopBreak = 0;
	psxRegs.iopCycleEE = eeCycles;

	reinterpret_cast<void (*)()>(const_cast<void*>(iopEnterRecompiledCode))();

	return psxRegs.iopBreak + psxRegs.iopCycleEE;
}

// Returns the offset to the next instruction after any cleared memory
static __fi u32 psxRecClearMem(u32 pc)
{
	BASEBLOCK* pblock;

	pblock = PSX_GETBLOCK(pc);
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

	iopClearRecLUT(PSX_GETBLOCK(lowerextent), upperextent - lowerextent);

	return upperextent - pc;
}

static __fi void recClearIOP(u32 Addr, u32 Size)
{
	u32 pc = Addr;
	while (pc < Addr + Size * 4)
		pc += PSXREC_CLEARM(pc);
}

static void iopRecRecompile(const u32 startpc)
{
	u32 i;
	u32 link_next_block = 0;

	// When upgrading the IOP, there are two resets, the second of which is a 'fake' reset
	// This second 'reset' involves UDNL calling SYSMEM and LOADCORE directly, resetting LOADCORE's modules
	// This detects when SYSMEM is called and clears the modules then
	if (startpc == 0x890)
	{
		DevCon.WriteLn(Color_Gray, "R3000 Debugger: Branch to 0x890 (SYSMEM). Clearing modules.");
		R3000SymbolGuardian.ClearIrxModules();
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

	// Override the memory size argument to IOPBOOT
	if (startpc == 0xbfc4a000)
	{
		// Don't bother emitting assembly for this, it'll only run once
		// per boot so this is fine
		psxRegs.GPR.n.a0 = Ps2MemSize::ExposedIopRam >> 20;
	}

	pxAssert(startpc);

	// if recPtr reached the mem limit reset whole mem
	if (recPtr >= recPtrEnd)
	{
		recResetIOP();
	}

	// Other recompilers share the thread-local assembler state, so always set our pointer.
	armSetAsmPtr(recPtr, SysMemory::GetIOPRecEnd() - recPtr, nullptr);
	recPtr = armStartBlock();

	s_pCurBlock = PSX_GETBLOCK(startpc);

	pxAssert(s_pCurBlock->GetFnptr() == (uptr)iopJITCompile);

	s_pCurBlockEx = recBlocks.Get(HWADDR(startpc));

	if (!s_pCurBlockEx || s_pCurBlockEx->startpc != HWADDR(startpc))
		s_pCurBlockEx = recBlocks.New(HWADDR(startpc), (uptr)recPtr);

	psxbranch = 0;

	s_pCurBlock->SetFnptr((uptr)recPtr);
	s_psxBlockCycles = 0;

	// reset recomp state variables
	psxpc = startpc;
	s_constRegs[0] = 0;
	s_hasConstReg = s_flushedConstReg = 1;

	if ((psxHu32(HW_ICFG) & 8) && (HWADDR(startpc) == 0xa0 || HWADDR(startpc) == 0xb0 || HWADDR(startpc) == 0xc0))
	{
		armEmitCall(reinterpret_cast<const void*>(psxBiosCall));
		armAsm->Tst(a64::w0, 0xff);
		armEmitCondBranch(a64::ne, iopDispatcherReg);
	}

	// go until the next branch
	i = startpc;
	s_nEndBlock = 0xffffffff;
	s_branchTo = -1;

	while (1)
	{
		BASEBLOCK* pblock = PSX_GETBLOCK(i);
		if (i != startpc && pblock->GetFnptr() != (uptr)iopJITCompile)
		{
			// The next instruction is already compiled, end here and link to it.
			link_next_block = 1;
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

	while (!psxbranch && psxpc < s_nEndBlock)
	{
		psxRecompileNextInstruction(false);
	}

	pxAssert((psxpc - startpc) >> 2 <= 0xffff);
	s_pCurBlockEx->size = (psxpc - startpc) >> 2;

	if (!(psxpc & 0x10000000))
		g_psxMaxRecMem = std::max((psxpc & ~0xa0000000), g_psxMaxRecMem);

	if (psxbranch)
	{
		pxAssert(!link_next_block);
	}
	else
	{
		iPsxAddBlockCycles();

		pxAssert(psxpc == s_nEndBlock);
		_psxFlushConstRegs();
		_psxStorePC(psxpc);
		psxEmitBlockLink(s_nEndBlock);
		psxbranch = 3;
	}

	u8* block_end = armEndBlock();
	pxAssert(block_end < SysMemory::GetIOPRecEnd());
	pxAssert(block_end - recPtr < _64kb);
	s_pCurBlockEx->x86size = static_cast<u32>(block_end - recPtr);

	Perf::iop.RegisterPC((void*)s_pCurBlockEx->fnptr, s_pCurBlockEx->x86size, s_pCurBlockEx->startpc);

	recPtr = block_end;

	pxAssert((s_hasConstReg & s_flushedConstReg) == s_hasConstReg);

	s_pCurBlock = nullptr;
	s_pCurBlockEx = nullptr;
}

R3000Acpu psxRec = {
	recReserve,
	recResetIOP,
	recExecuteBlock,
	recClearIOP,
	recShutdown,
};
