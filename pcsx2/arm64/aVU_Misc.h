// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU — arch-neutral macro layer (Phase 7, task 7.3).
//
// This is the parallel clone of the *arch-neutral* part of pcsx2/x86/microVU_Misc.h:
// the instruction-field extractors, the IR-state accessor macros (mVUregs / iPC /
// incPC / mVUup / mVUlow / sFLAG / ...), and the optimization-option constexprs that
// the analysis pass (aVU_Analyze.inl) and the compile driver operate on.
//
// Deliberately DROPPED vs the x86 header (all x86emitter-coupled):
//   * `using namespace x86Emitter;` + the xmm/x32 typedefs;
//   * the host-register name macros (xmmT1.., gprT1.., gprF0..) — the ARM64 register
//     map lives in aVU_IR.h (NEON v0-v23 for VF, w-regs for VI, mVU_F0..3=w23-26);
//   * the x86 shuffle-immediate helpers (shufflePQ/shuffleSS) — NEON does lane moves
//     differently (task 7.5);
//   * the `extern mVUmergeRegs/mVUsaveReg/mVUloadReg` decls — the ARM64 NEON versions
//     are members/helpers in aVU_IR.h.
//
// `doConstProp` already lives in aVU.h and `doWholeProgCompare` in aVU.cpp, so they
// are NOT redefined here (see the options block below).

#include "aVU.h"
#include "Config.h"

//------------------------------------------------------------------
// Bit flags in the VU instruction word + branch-name table
//------------------------------------------------------------------

static const uint _Ibit_ = 1 << 31;
static const uint _Ebit_ = 1 << 30;
static const uint _Mbit_ = 1 << 29;
static const uint _Dbit_ = 1 << 28;
static const uint _Tbit_ = 1 << 27;

static const uint divI = 0x1040000;
static const uint divD = 0x2080000;

static const char branchSTR[16][8] = {
	"None",  "B",     "BAL",   "IBEQ",
	"IBGEZ", "IBGTZ", "IBLEZ", "IBLTZ",
	"IBNE",  "JR",    "JALR",  "N/A",
	"N/A",   "N/A",   "N/A",   "N/A"
};

//------------------------------------------------------------------
// Emit-constant table (x86: microVU_Misc.h mVU_Globals / mVUglob)
//------------------------------------------------------------------
// Arch-neutral constant data consumed by the VIXL emit layer (clamp range
// bounds, FTOI/ITOF scales, the polynomial coefficients for the EFU estimates).
// Loaded into NEON regs via armMoveAddressToReg + Ldr(.Q()). `static constexpr`
// gives each including TU its own copy, exactly like x86.

struct mVU_Globals
{
#define __four(val) { val, val, val, val }
	u32   absclip [4] = __four(0x7fffffff);
	u32   signbit [4] = __four(0x80000000);
	u32   minvals [4] = __four(0xff7fffff);
	u32   maxvals [4] = __four(0x7f7fffff);
	u32   exponent[4] = __four(0x7f800000);
	u32   one     [4] = __four(0x3f800000);
	u32   Pi4     [4] = __four(0x3f490fdb);
	u32   T1      [4] = __four(0x3f7ffff5);
	u32   T5      [4] = __four(0xbeaaa61c);
	u32   T2      [4] = __four(0x3e4c40a6);
	u32   T3      [4] = __four(0xbe0e6c63);
	u32   T4      [4] = __four(0x3dc577df);
	u32   T6      [4] = __four(0xbd6501c4);
	u32   T7      [4] = __four(0x3cb31652);
	u32   T8      [4] = __four(0xbb84d7e7);
	u32   S2      [4] = __four(0xbe2aaaa4);
	u32   S3      [4] = __four(0x3c08873e);
	u32   S4      [4] = __four(0xb94fb21f);
	u32   S5      [4] = __four(0x362e9c14);
	u32   E1      [4] = __four(0x3e7fffa8);
	u32   E2      [4] = __four(0x3d0007f4);
	u32   E3      [4] = __four(0x3b29d3ff);
	u32   E4      [4] = __four(0x3933e553);
	u32   E5      [4] = __four(0x36b63510);
	u32   E6      [4] = __four(0x353961ac);
	u32   I32MAXF [4] = __four(0x4effffff);
	float FTOI_4  [4] = __four(16.0);
	float FTOI_12 [4] = __four(4096.0);
	float FTOI_15 [4] = __four(32768.0);
	float ITOF_4  [4] = __four(0.0625f);
	float ITOF_12 [4] = __four(0.000244140625);
	float ITOF_15 [4] = __four(0.000030517578125);
#undef __four
};

alignas(32) static constexpr struct mVU_Globals mVUglob;

//------------------------------------------------------------------
// Instruction-field extractor macros (operate on mVU.code)
//------------------------------------------------------------------

#define _Ft_ ((mVU.code >> 16) & 0x1F) // The ft part of the instruction register
#define _Fs_ ((mVU.code >> 11) & 0x1F) // The fs part of the instruction register
#define _Fd_ ((mVU.code >>  6) & 0x1F) // The fd part of the instruction register

#define _It_ ((mVU.code >> 16) & 0xF)  // The it part of the instruction register
#define _Is_ ((mVU.code >> 11) & 0xF)  // The is part of the instruction register
#define _Id_ ((mVU.code >>  6) & 0xF)  // The id part of the instruction register

#define _X ((mVU.code >> 24) & 0x1)
#define _Y ((mVU.code >> 23) & 0x1)
#define _Z ((mVU.code >> 22) & 0x1)
#define _W ((mVU.code >> 21) & 0x1)

#define _X_Y_Z_W   (((mVU.code >> 21) & 0xF))
#define _XYZW_SS   (_X + _Y + _Z + _W == 1)
#define _XYZW_SS2  (_XYZW_SS && (_X_Y_Z_W != 8))
#define _XYZW_PS   (_X_Y_Z_W == 0xf)
#define _XYZWss(x) ((x == 8) || (x == 4) || (x == 2) || (x == 1))

#define _bc_   (mVU.code & 0x3)
#define _bc_x ((mVU.code & 0x3) == 0)
#define _bc_y ((mVU.code & 0x3) == 1)
#define _bc_z ((mVU.code & 0x3) == 2)
#define _bc_w ((mVU.code & 0x3) == 3)

#define _Fsf_ ((mVU.code >> 21) & 0x03)
#define _Ftf_ ((mVU.code >> 23) & 0x03)

#define _Imm5_  ((s16) (((mVU.code & 0x400) ? 0xfff0 : 0) | ((mVU.code >> 6) & 0xf)))
#define _Imm11_ ((s32)  ((mVU.code & 0x400) ? (0xfffffc00 |  (mVU.code & 0x3ff)) : (mVU.code & 0x3ff)))
#define _Imm12_ ((u32)((((mVU.code >> 21) & 0x1) << 11)   |  (mVU.code & 0x7ff)))
#define _Imm15_ ((u32) (((mVU.code >> 10) & 0x7800)       |  (mVU.code & 0x7ff)))
#define _Imm24_ ((u32)   (mVU.code & 0xffffff))

#define isCOP2      (mVU.cop2 != 0)
#define isVU1       (mVU.index != 0)
#define isVU0       (mVU.index == 0)
#define getIndex    (isVU1 ? 1 : 0)
#define getVUmem(x) (((isVU1) ? (x & 0x3ff) : ((x >= 0x400) ? (x & 0x43f) : (x & 0xff))) * 16)
#define offsetSS    ((_X) ? (0) : ((_Y) ? (4) : ((_Z) ? 8 : 12)))
#define offsetReg   ((_X) ? (0) : ((_Y) ? (1) : ((_Z) ? 2 :  3)))

//------------------------------------------------------------------
// Recompiler-pass function-signature macros
//------------------------------------------------------------------

// Function Params
#define mP microVU& mVU, int recPass
#define mV microVU& mVU
#define mF int recPass
#define mX mVU, recPass

typedef void Fntype_mVUrecInst(microVU& mVU, int recPass);
typedef Fntype_mVUrecInst* Fnptr_mVUrecInst;

// Function/Template Stuff
#define mVUx (vuIndex ? microVU1 : microVU0)
#define mVUop(opName) static void opName(mP)
#define _mVUt template <int vuIndex>

// Define Passes
#define pass1 if (recPass == 0) // Analyze
#define pass2 if (recPass == 1) // Recompile
#define pass3 if (recPass == 2) // Logging
#define pass4 if (recPass == 3) // Flag stuff

// Upper Opcode Cases
#define opCase1 if (opCase == 1) // Normal Opcodes
#define opCase2 if (opCase == 2) // BC Opcodes
#define opCase3 if (opCase == 3) // I  Opcodes
#define opCase4 if (opCase == 4) // Q  Opcodes

//------------------------------------------------------------------
// IR-state accessor macros
//------------------------------------------------------------------

#define mVUcurProg   mVU.prog.cur[0]
#define mVUblocks    mVU.prog.cur->block
#define mVUir        mVU.prog.IRinfo
#define mVUbranch    mVU.prog.IRinfo.branch
#define mVUcycles    mVU.prog.IRinfo.cycles
#define mVUcount     mVU.prog.IRinfo.count
#define mVUpBlock    mVU.prog.IRinfo.pBlock
#define mVUblock     mVU.prog.IRinfo.block
#define mVUregs      mVU.prog.IRinfo.block.pState
#define mVUregsTemp  mVU.prog.IRinfo.regsTemp
#define iPC          mVU.prog.IRinfo.curPC
#define mVUsFlagHack mVU.prog.IRinfo.sFlagHack
#define mVUconstReg  mVU.prog.IRinfo.constReg
#define mVUstartPC   mVU.prog.IRinfo.startPC
#define mVUinfo      mVU.prog.IRinfo.info[iPC / 2]
#define mVUstall     mVUinfo.stall
#define mVUup        mVUinfo.uOp
#define mVUlow       mVUinfo.lOp
#define sFLAG        mVUinfo.sFlag
#define mFLAG        mVUinfo.mFlag
#define cFLAG        mVUinfo.cFlag
#define mVUrange     (mVUcurProg.ranges[0])[0]
#define isEvilBlock  (mVUpBlock->pState.blockType == 2)
#define isBadOrEvil  (mVUlow.badBranch || mVUlow.evilBranch)
#define isConditional (mVUlow.branch > 2 && mVUlow.branch < 9)
#define xPC          ((iPC / 2) * 8)
#define curI         ((u32*)mVU.regs().Micro)[iPC] //mVUcurProg.data[iPC]
#define setCode()    { mVU.code = curI; }
#define bSaveAddr    (((xPC + 16) & (mVU.microMemSize-8)) / 8)
#define Rmem         &mVU.regs().VI[REG_R].UL
#define aWrap(x, m)  ((x > m) ? 0 : x)
#define clampE       CHECK_VU_EXTRA_OVERFLOW(mVU.index)
#define varPrint(x)  DevCon.WriteLn(#x " = %d", (int)x)
#define islowerOP    ((iPC & 1) == 0)

#define blockCreate(addr) \
	{ \
		if (!mVUblocks[addr]) \
			mVUblocks[addr] = new microBlockManager(); \
	}

// Fetches the PC and instruction opcode relative to the current PC.  Used to rewind and
// fast-forward the IR state while calculating VU pipeline conditions (branches, writebacks, etc)
#define incPC(x)  { iPC = ((iPC + (x)) & mVU.progMemMask); mVU.code = curI; }
#define incPC2(x) { iPC = ((iPC + (x)) & mVU.progMemMask); }

// Flag Info (Set if next-block's first 4 ops will read current-block's flags)
#define __Status (mVUregs.needExactMatch & 1)
#define __Mac    (mVUregs.needExactMatch & 2)
#define __Clip   (mVUregs.needExactMatch & 4)

// Program logging is compiled out (x86: microVU_Misc.h mVUlogProg path).
#define mVUlog(...)      if (0) {}

//------------------------------------------------------------------
// Optimization / Debug Options
//------------------------------------------------------------------
// NOTE: doConstProp lives in aVU.h and doWholeProgCompare in aVU.cpp (each only has
// one consumer today); they are intentionally not duplicated here.

// Reg Alloc — false flushes every 32-bit instruction (debug only).
static constexpr bool doRegAlloc = true;

// No Flag Optimizations — true forces mVU to always update Mac/Status flags (debug).
static constexpr bool noFlagOpts = false;

// Multiple Flag Instances (the correct VU behavior — up to 4 live instances each).
static constexpr bool doSFlagInsts = true;
static constexpr bool doMFlagInsts = true;
static constexpr bool doCFlagInsts = true;

// Branch in Branch Delay Slots — emulate evil-branches (true) vs treat 2nd as NOP.
static constexpr bool doBranchInDelaySlot = true;

// Indirect Jump Caching — remember entry points for previously jumped-to addresses.
static constexpr bool doJumpCaching = true;

// Indirect Jumps as part of same cached microProgram (must disable doJumpCaching if on).
static constexpr bool doJumpAsSameProgram = false;

// D-Bit handling in micro programs (debug only; released games use the T-Bit).
static constexpr bool doDBitHandling = false;

//------------------------------------------------------------------
// Speed Hacks
//------------------------------------------------------------------

// Status Flag Speed Hack — only update the Status Flag on blocks that read it.
#define CHECK_VU_FLAGHACK (EmuConfig.Speedhacks.vuFlagHack)
