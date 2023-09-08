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
#ifndef _R3000A_SUPERREC_
#define _R3000A_SUPERREC_

#include "common/emitter/x86emitter.h"
#include "R3000A.h"
#include "iCore.h"

// Cycle penalties for particularly slow instructions.
static const int psxInstCycles_Mult = 7;
static const int psxInstCycles_Div = 40;

// Currently unused (iop mod incomplete)
static const int psxInstCycles_Peephole_Store = 0;
static const int psxInstCycles_Store = 0;
static const int psxInstCycles_Load = 0;

// to be consistent with EE
#define PSX_HI XMMGPR_HI
#define PSX_LO XMMGPR_LO

extern uptr psxRecLUT[];

void _psxFlushConstReg(int reg);
void _psxFlushConstRegs();

void _psxDeleteReg(int reg, int flush);
void _psxFlushCall(int flushtype);
void _psxFlushAllDirty();

void _psxOnWriteReg(int reg);

void _psxMoveGPRtoR(const x86Emitter::xRegister32& to, int fromgpr);
void _psxMoveGPRtoM(uptr to, int fromgpr);

extern u32 psxpc; // recompiler pc
extern int psxbranch; // set for branch
extern u32 g_iopCyclePenalty;

void psxSaveBranchState();
void psxLoadBranchState();

extern void psxSetBranchReg(u32 reg);
extern void psxSetBranchImm(u32 imm);
extern void psxRecompileNextInstruction(bool delayslot, bool swapped_delayslot);

////////////////////////////////////////////////////////////////////
// IOP Constant Propagation Defines, Vars, and API - From here down!

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
int psxTryRenameReg(int to, int from, int fromx86, int other, int xmminfo);

//
// non mmx/xmm version, slower
//
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

#endif
