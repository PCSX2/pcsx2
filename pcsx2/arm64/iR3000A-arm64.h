// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "R3000A.h"
#include "arm64/iCore-arm64.h"
#include "arm64/AsmHelpers.h"

// x21: Pointer to psxRegs struct (callee-saved). Loaded once at IOP JIT entry
// by EnterRecompiledCode and never modified for the duration of IOP execution.
// Use armPsxRegMem() to construct psxRegs-relative MemOperands cheaply.
#define RPSXSTATE vixl::aarch64::x21

// Build a MemOperand addressing a psxRegs field via RPSXSTATE.
// Mirrors the EE armCpuRegMem pattern. ARM64 LDR with imm12 covers offsets up
// to 32760 bytes (64-bit) — easily larger than psxRegs, so a single instruction
// suffices for every reachable field.
static __fi vixl::aarch64::MemOperand armPsxRegMem(const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&psxRegs);
	return vixl::aarch64::MemOperand(RPSXSTATE, static_cast<int64_t>(off));
}

static __fi bool armIsPsxRegPtr(const void* field)
{
	const u8* base = reinterpret_cast<const u8*>(&psxRegs);
	const u8* p    = reinterpret_cast<const u8*>(field);
	return p >= base && p < base + sizeof(psxRegs);
}
static __fi void armLoadPsxRegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	if (armIsPsxRegPtr(field))
		armAsm->Ldr(reg, armPsxRegMem(field));
	else
		armLoadPtr(reg, field);
}
static __fi void armStorePsxRegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	if (armIsPsxRegPtr(field))
		armAsm->Str(reg, armPsxRegMem(field));
	else
		armStorePtr(reg, field);
}

// Cycle penalties for particularly slow IOP instructions.
static const int psxInstCycles_Mult = 7;
static const int psxInstCycles_Div = 40;

static const int psxInstCycles_Peephole_Store = 0;
static const int psxInstCycles_Store = 0;
static const int psxInstCycles_Load = 0;

// HI/LO register indices — consistent with EE naming
#define PSX_HI NEONGPR_HI
#define PSX_LO NEONGPR_LO

extern uptr psxRecLUT[];

void _psxFlushConstReg(int reg);
void _psxFlushConstRegs();

void _psxDeleteReg(int reg, int flush);
void _psxFlushCall(int flushtype);
void _psxFlushAllDirty();

void _psxOnWriteReg(int reg);

void _psxMoveGPRtoR(const vixl::aarch64::Register& to, int fromgpr);

extern u32 psxpc;       // recompiler pc
extern int psxbranch;   // set for branch
extern u32 g_iopCyclePenalty;

void psxSaveBranchState();
void psxLoadBranchState();

extern void psxSetBranchReg();
extern void psxSetBranchImm(u32 imm);
extern void psxRecompileNextInstruction(bool delayslot, bool swapped_delayslot);

////////////////////////////////////////////////////////////////////
// IOP Constant Propagation

#define PSX_IS_CONST1(reg) ((reg) < 32 && (g_psxHasConstReg & (1 << (reg))))
#define PSX_IS_CONST2(reg1, reg2) ((g_psxHasConstReg & (1 << (reg1))) && (g_psxHasConstReg & (1 << (reg2))))
#define PSX_IS_DIRTY_CONST(reg) ((reg) < 32 && (g_psxHasConstReg & (1 << (reg))) && (!(g_psxFlushedConstReg & (1 << (reg)))))
#define PSX_SET_CONST(reg) \
	{ \
		if ((reg) < 32) \
		{ \
			g_psxHasConstReg |= (1u << (reg)); \
			g_psxFlushedConstReg &= ~(1u << (reg)); \
		} \
	}

#define PSX_DEL_CONST(reg) \
	{ \
		if ((reg) < 32) \
			g_psxHasConstReg &= ~(1 << (reg)); \
	}

extern u32 g_psxConstRegs[32];
extern u32 g_psxHasConstReg, g_psxFlushedConstReg;

typedef void (*R3000AFNPTR)();
typedef void (*R3000AFNPTR_INFO)(int info);

bool psxTrySwapDelaySlot(u32 rs, u32 rt, u32 rd);

////////////////////////////////////////////////////////////////////
// Constant propagation code generation macros

// rd = rs op rt
#define PSXRECOMPILE_CONSTCODE0(fn, info) \
	void rpsx##fn(void) \
	{ \
		psxRecompileCodeConst0(rpsx##fn##_const, rpsx##fn##_consts, rpsx##fn##_constt, rpsx##fn##_, info); \
	}

// rt = rs op imm16
#define PSXRECOMPILE_CONSTCODE1(fn, info) \
	void rpsx##fn(void) \
	{ \
		psxRecompileCodeConst1(rpsx##fn##_const, rpsx##fn##_, info); \
	}

// rd = rt op sa
#define PSXRECOMPILE_CONSTCODE2(fn, info) \
	void rpsx##fn(void) \
	{ \
		psxRecompileCodeConst2(rpsx##fn##_const, rpsx##fn##_, info); \
	}

// [lo,hi] = rt op rs
#define PSXRECOMPILE_CONSTCODE3(fn, LOHI) \
	void rpsx##fn(void) \
	{ \
		psxRecompileCodeConst3(rpsx##fn##_const, rpsx##fn##_consts, rpsx##fn##_constt, rpsx##fn##_, LOHI); \
	}

#define PSXRECOMPILE_CONSTCODE3_PENALTY(fn, LOHI, cycles) \
	void rpsx##fn(void) \
	{ \
		psxRecompileCodeConst3(rpsx##fn##_const, rpsx##fn##_consts, rpsx##fn##_constt, rpsx##fn##_, LOHI); \
		g_iopCyclePenalty = cycles; \
	}

// rd = rs op rt
void psxRecompileCodeConst0(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int xmminfo);
// rt = rs op imm16
void psxRecompileCodeConst1(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode, int xmminfo);
// rd = rt op sa
void psxRecompileCodeConst2(R3000AFNPTR constcode, R3000AFNPTR_INFO noconstcode, int xmminfo);
// [lo,hi] = rt op rs
void psxRecompileCodeConst3(R3000AFNPTR constcode, R3000AFNPTR_INFO constscode, R3000AFNPTR_INFO consttcode, R3000AFNPTR_INFO noconstcode, int LOHI);
