// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "AsmHelpers.h"

namespace a64 = vixl::aarch64;

struct microVU;

//------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------

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
	// Sign-preserving operand clamp bounds for mVUclamp2 (SMIN/UMIN on the
	// integer representation), mirroring x86's sse4_maxvals/sse4_minvals
	// (x86/microVU_Clamp.inl): row 0 = single-lane (SS) — real bound in lane
	// 0, sentinel no-op bounds (INT_MAX for SMIN, UINT_MAX for UMIN) in
	// lanes 1-3 so anything parked there survives; row 1 = all-lane (PS).
	// Appended at the end of the struct so existing [x25, #imm] offsets in
	// emitted code keep their values.
	u32 signMaxvals[2][4] = {{0x7f7fffff, 0x7fffffff, 0x7fffffff, 0x7fffffff},
	                         {0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff}};
	u32 signMinvals[2][4] = {{0xff7fffff, 0xffffffff, 0xffffffff, 0xffffffff},
	                         {0xff7fffff, 0xff7fffff, 0xff7fffff, 0xff7fffff}};
#undef __four
};

alignas(32) static constexpr struct mVU_Globals mVUglob;

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
// Opcode Decoding Macros (platform-independent)
//------------------------------------------------------------------

#define _Ft_ ((mVU.code >> 16) & 0x1F)
#define _Fs_ ((mVU.code >> 11) & 0x1F)
#define _Fd_ ((mVU.code >>  6) & 0x1F)

#define _It_ ((mVU.code >> 16) & 0xF)
#define _Is_ ((mVU.code >> 11) & 0xF)
#define _Id_ ((mVU.code >>  6) & 0xF)

#define _X ((mVU.code >> 24) & 0x1)
#define _Y ((mVU.code >> 23) & 0x1)
#define _Z ((mVU.code >> 22) & 0x1)
#define _W ((mVU.code >> 21) & 0x1)

#define _cX ((cpuRegs.code >> 24) & 0x1)
#define _cY ((cpuRegs.code >> 23) & 0x1)
#define _cZ ((cpuRegs.code >> 22) & 0x1)
#define _cW ((cpuRegs.code >> 21) & 0x1)

#define _X_Y_Z_W   (((mVU.code >> 21) & 0xF))
#define _cX_Y_Z_W   (((cpuRegs.code >> 21) & 0xF))
#define _cXYZW_SS  (_cX + _cY + _cZ + _cW == 1)
#define _cXYZW_SS2  (_cXYZW_SS && (_cX_Y_Z_W != 8))

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
// ARM64 Register Definitions
//------------------------------------------------------------------

// NEON scratch registers (matching existing AsmHelpers.h: Q29-Q31)
// qmmT1-qmmT7: allocatable VF cache / temp registers
#define qmmT1  a64::q0
#define qmmT2  a64::q1
#define qmmT3  a64::q2
#define qmmT4  a64::q3
#define qmmT5  a64::q4
#define qmmT6  a64::q5
#define qmmT7  a64::q6

// P/Q packed register (replaces x86 xmmPQ=xmm15)
#define qmmPQ  a64::q28

// GPR scratch registers
#define gprT1  a64::w9
#define gprT2  a64::w10
#define gprT1q a64::x9
#define gprT2q a64::x10
#define gprT3  a64::w11
#define gprT3q a64::x11

// Status flag instance registers (callee-saved)
#define gprF0  a64::w20
#define gprF1  a64::w21
#define gprF2  a64::w22
#define gprF3  a64::w23

// VU state base pointer (callee-saved). Pinned at mVUdispatcherAB entry to
// `&mVU.regs()` (= &vuRegs[mVU.index], a static address per-VU). Stays live
// across all blocks of a single dispatch, including across armEmitCall to C
// helpers (callee-saved per AAPCS). Lets emit reach any VURegs field with a
// 1-insn `Ldr/Str dst, [gprVUState, #imm12]` instead of the 3-insn
// movz/movk/movk + Ldr that armLoadPtr emits for absolute addresses (data
// globals at 0xaaab_xxxx_xxxx live ~64TB from the JIT cache at 0xfffe_xxxx,
// so adrp+add never reaches and we always fall through to the 3-insn path).
#define gprVUState a64::x19

// MemOperand anchored at gprVUState — `mVU.regs()` field accessor.
// `off` is the byte offset within `VURegs`. Caller is responsible for
// matching the encoding's imm12 reach (Q=64K, X=32K, W=16K, H=8K).
__fi static a64::MemOperand mVUstateMem(int64_t off)
{
	return a64::MemOperand(gprVUState, off);
}

// mVU shadow-flag base pointer (callee-saved). Pinned at mVUdispatcherAB
// entry to `&mVU.macFlag[0]`. The `microVU` struct lays out
// `statFlag[4]/macFlag[4]/clipFlag[4]/neonCTemp[4]/neonBackup[32][4]` as
// consecutive 16-byte-aligned arrays, so a single pin reaches all of them:
//
//   &mVU.statFlag[0]      = pin - 16
//   &mVU.macFlag[0]       = pin
//   &mVU.clipFlag[0]      = pin + 16
//   &mVU.neonCTemp[0]     = pin + 32
//   &mVU.neonBackup[N][0] = pin + 48 + N*16   (N=0..31, max +544)
//
// Every flag-touching FMAC reaches these globals with a single
// `Ldr/Str reg, [gprMVUFlag, #imm]` rather than the 3-insn
// movz/movk/movk + ldr sequence. The address is constant per-VU
// (microVU0/microVU1 are static globals) so the pin is set once at
// dispatch entry and survives all C-call paths (callee-saved per AAPCS).
#define gprMVUFlag a64::x24

// MemOperand anchored at gprMVUFlag. `off` is the byte offset from
// &mVU.macFlag[0] (signed; see layout above). Caller is responsible for
// matching the encoding's imm12 reach.
__fi static a64::MemOperand mVUmacFlagMem(int instance)
{
	return a64::MemOperand(gprMVUFlag, instance * 4);
}
__fi static a64::MemOperand mVUclipFlagMem(int instance)
{
	return a64::MemOperand(gprMVUFlag, 16 + instance * 4);
}
__fi static a64::MemOperand mVUneonBackupMem(int neonReg)
{
	return a64::MemOperand(gprMVUFlag, 48 + neonReg * 16);
}

// mVUglob constants base pointer (callee-saved). Pinned at mVUdispatcherAB
// entry to `&mVUglob`. The mVU_Globals struct (~512 bytes of compile-time
// float constants — clamp limits, FTOI/ITOF scale factors, Taylor-series
// coefficients) is laid out as 16-byte-aligned u32[4]/float[4] arrays;
// every entry reachable via [gprMVUglob, #imm12] rather than the
// 3-insn movz/movk/movk + ldr sequence per constant load.
#define gprMVUglob a64::x25

// Return a MemOperand for a field within mVUglob, computed at JIT-emit
// time from the absolute pointer. Lets call sites keep writing
// `&mVUglob.X` (which the compiler folds to a constant offset) while the
// emit goes through the pinned base.
__fi static a64::MemOperand mVUglobMem(const void* addr)
{
	const u8* base = reinterpret_cast<const u8*>(&mVUglob);
	const u8* p    = reinterpret_cast<const u8*>(addr);
	return a64::MemOperand(gprMVUglob, p - base);
}

//------------------------------------------------------------------
// Function/Template Macros (platform-independent)
//------------------------------------------------------------------

#define mP microVU& mVU, int recPass
#define mV microVU& mVU
#define mF int recPass
#define mX mVU, recPass

typedef void Fntype_mVUrecInst(microVU& mVU, int recPass);
typedef Fntype_mVUrecInst* Fnptr_mVUrecInst;

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
// IR/Pipeline Macros (platform-independent)
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
#define curI         ((u32*)mVU.regs().Micro)[iPC]
#define setCode()    { mVU.code = curI; }
#define bSaveAddr    (((xPC + 16) & (mVU.microMemSize-8)) / 8)
#define shufflePQ    (((mVU.p) ? 0xb0 : 0xe0) | ((mVU.q) ? 0x01 : 0x04))
#define Rmem         &mVU.regs().VI[REG_R].UL
#define aWrap(x, m)  ((x > m) ? 0 : x)
#define shuffleSS(x) ((x == 1) ? (0x27) : ((x == 2) ? (0xc6) : ((x == 4) ? (0xe1) : (0xe4))))
#define clampE       CHECK_VU_EXTRA_OVERFLOW(mVU.index)
#define varPrint(x)  DevCon.WriteLn(#x " = %d", (int)x)
#define islowerOP    ((iPC & 1) == 0)

#define blockCreate(addr) \
	{ \
		if (!mVUblocks[addr]) \
			mVUblocks[addr] = new microBlockManager(); \
	}

#define incPC(x)  { iPC = ((iPC + (x)) & mVU.progMemMask); mVU.code = curI; }
#define incPC2(x) { iPC = ((iPC + (x)) & mVU.progMemMask); }

// Flag Info
#define __Status (mVUregs.needExactMatch & 1)
#define __Mac    (mVUregs.needExactMatch & 2)
#define __Clip   (mVUregs.needExactMatch & 4)

// Pass 3 Helper Macros (logging)
#define _Fsf_String ((_Fsf_ == 3) ? "w" : ((_Fsf_ == 2) ? "z" : ((_Fsf_ == 1) ? "y" : "x")))
#define _Ftf_String ((_Ftf_ == 3) ? "w" : ((_Ftf_ == 2) ? "z" : ((_Ftf_ == 1) ? "y" : "x")))
#define xyzwStr(x, s) (_X_Y_Z_W == x) ? s:
#define _XYZW_String (xyzwStr(1, "w") (xyzwStr(2, "z") (xyzwStr(3, "zw") (xyzwStr(4, "y") (xyzwStr(5, "yw") (xyzwStr(6, "yz") (xyzwStr(7, "yzw") (xyzwStr(8, "x") (xyzwStr(9, "xw") (xyzwStr(10, "xz") (xyzwStr(11, "xzw") (xyzwStr(12, "xy") (xyzwStr(13, "xyw") (xyzwStr(14, "xyz") "xyzw"))))))))))))))
#define _BC_String   (_bc_x ? "x" : (_bc_y ? "y" : (_bc_z ? "z" : "w")))
#define mVUlogFtFs() { mVUlog(".%s vf%02d, vf%02d", _XYZW_String, _Ft_, _Fs_); }
#define mVUlogFd()   { mVUlog(".%s vf%02d, vf%02d", _XYZW_String, _Fd_, _Fs_); }
#define mVUlogACC()  { mVUlog(".%s ACC, vf%02d", _XYZW_String, _Fs_); }
#define mVUlogFt()   { mVUlog(", vf%02d", _Ft_); }
#define mVUlogBC()   { mVUlog(", vf%02d%s", _Ft_, _BC_String); }
#define mVUlogI()    { mVUlog(", I"); }
#define mVUlogQ()    { mVUlog(", Q"); }
#define mVUlogCLIP() { mVUlog("w.xyz vf%02d, vf%02dw", _Fs_, _Ft_); }

#ifdef mVUlogProg
	#define mVUlog      ((isVU1) ? __mVULog<1> : __mVULog<0>)
	#define mVUdumpProg __mVUdumpProgram
#else
	#define mVUlog(...)      if (0) {}
	#define mVUdumpProg(...) if (0) {}
#endif

//------------------------------------------------------------------
// Optimization / Debug Options (same as x86)
//------------------------------------------------------------------

static constexpr bool doRegAlloc = true;
static constexpr bool noFlagOpts = false;
static constexpr bool doSFlagInsts = true;
static constexpr bool doMFlagInsts = true;
static constexpr bool doCFlagInsts = true;
static constexpr bool doBranchInDelaySlot = true;
static constexpr bool doConstProp = false;
static constexpr bool doJumpCaching = true;
static constexpr bool doJumpAsSameProgram = false;
static constexpr bool doDBitHandling = false;
static constexpr bool doWholeProgCompare = false;
// Keep the IBcc condition value live in a pool temp from the branch op to
// condBranch's tail Cmp, skipping the Ldrsh reload of mVU.branch (WS-C1
// analog from the EE-SRA campaign). The memory store stays authoritative.
static constexpr bool doBranchCondCarry = true;

// Status Flag Speed Hack
#define CHECK_VU_FLAGHACK (EmuConfig.Speedhacks.vuFlagHack)
