// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 port of x86/microVU_Misc.h.
// The instruction field macros, pass macros and optimization options are identical to the x86
// version. What differs is the host register model and the emission helpers at the bottom.

#include "arm64/AsmHelpers.h"

namespace a64 = vixl::aarch64;

struct microVU;

//------------------------------------------------------------------
// Host registers
//------------------------------------------------------------------

// A NEON register used by microVU. Id is the register code (q0..q31), -1 when empty.
struct xmm
{
	int Id;

	constexpr xmm() : Id(-1) {}
	constexpr explicit xmm(int id) : Id(id) {}

	__fi bool IsEmpty() const { return Id < 0; }
	__fi bool operator==(const xmm& rhs) const { return Id == rhs.Id; }
	__fi bool operator!=(const xmm& rhs) const { return Id != rhs.Id; }

	__fi a64::VRegister Q() const { return a64::VRegister(Id, 128); }
	__fi a64::VRegister V4S() const { return a64::VRegister(Id, 128, 4); }
	__fi a64::VRegister V2D() const { return a64::VRegister(Id, 128, 2); }
	__fi a64::VRegister V8H() const { return a64::VRegister(Id, 128, 8); }
	__fi a64::VRegister V16B() const { return a64::VRegister(Id, 128, 16); }
	__fi a64::VRegister S() const { return a64::VRegister(Id, 32); }
	__fi a64::VRegister D() const { return a64::VRegister(Id, 64); }

	static const xmm& GetInstance(int i);
};

// A general purpose register used by microVU (32-bit view by default).
struct x32
{
	int Id;

	constexpr x32() : Id(-1) {}
	constexpr explicit x32(int id) : Id(id) {}

	__fi bool IsEmpty() const { return Id < 0; }
	__fi int GetId() const { return Id; }
	__fi bool operator==(const x32& rhs) const { return Id == rhs.Id; }
	__fi bool operator!=(const x32& rhs) const { return Id != rhs.Id; }

	__fi a64::Register W() const { return a64::Register(Id, 32); }
	__fi a64::Register X() const { return a64::Register(Id, 64); }

	static const x32& GetInstance(int i);
};

static constexpr xmm xEmptyReg;

// Temps used by the x86 code by name.
static constexpr xmm xmmT1(0);
static constexpr xmm xmmT2(1);
static constexpr xmm xmmT3(2);
static constexpr xmm xmmT4(3);
static constexpr xmm xmmT5(4);
static constexpr xmm xmmT6(5);
static constexpr xmm xmmT7(6);
static constexpr xmm xmmPQ(28); // Holds the Value and Backup Values of P and Q regs

// q29-q31 are scratch registers for emission helpers, never allocated.
#define mVUQScratch1 a64::q30
#define mVUQScratch2 a64::q31
#define mVUQScratch3 a64::q29
#define mVUVScratch1(fmt) a64::VRegister(30, 128, fmt)
#define mVUVScratch2(fmt) a64::VRegister(31, 128, fmt)
#define mVUVScratch3(fmt) a64::VRegister(29, 128, fmt)

static constexpr int mVUXmmTotal = 28; // q0..q27 are allocatable

static constexpr x32 gprT1(0); // Temp Reg (also first argument/return register)
static constexpr x32 gprT2(1); // Temp Reg (also second argument register)
static constexpr x32 gprF0(22); // Status Flag 0
static constexpr x32 gprF1(23); // Status Flag 1
static constexpr x32 gprF2(24); // Status Flag 2
static constexpr x32 gprF3(25); // Status Flag 3

#define gprT1q a64::x0
#define gprT2q a64::x1

// Pinned registers (callee-saved).
#define RMVUREGS a64::x19 // &mVU.regs()
#define RMVU a64::x20 // &mVU
#define RMVUMEM a64::x21 // mVU.regs().Mem
#define RMVUCONST a64::x26 // &s_mVUconsts

static constexpr int mVUGprTotal = 32;

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
	u32   T2      [4] = __four(0xbeaaa61c);
	u32   T3      [4] = __four(0x3e4c40a6);
	u32   T4      [4] = __four(0xbe0e6c63);
	u32   T5      [4] = __four(0x3dc577df);
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

	// ARM64 only: constants for helpers that emulate SSE instructions.
	u32   sse4_minvals_ss[4] = {0xff7fffff, 0xffffffff, 0xffffffff, 0xffffffff};
	u32   sse4_maxvals_ss[4] = {0x7f7fffff, 0x7fffffff, 0x7fffffff, 0x7fffffff};
	u32   compvals_max[4] = __four(0x7f7fffff);
	u32   compvals_abs[4] = __four(0x7fffffff);
	u32   maskbits[4] = {1, 2, 4, 8}; // lane i -> bit i (movmskps)
	u32   maskbits_rev[4] = {8, 4, 2, 1}; // lane i -> bit 3 - i (movmskps after a wzyx flip)
	u32   clipplus[4] = {0x01, 0x04, 0x10, 0x40};
	u32   clipminus[4] = {0x02, 0x08, 0x20, 0x80};
	u32   int_min[4] = __four(0x80000000);
	u32   int_max[4] = __four(0x7fffffff);
	u32   add_ss_mask[4] = {0x80000000, 0xffffffff, 0xffffffff, 0xffffffff};
#undef __four
};

alignas(32) static constexpr struct mVU_Globals mVUglob;

#define mVUconst(name) static_cast<s64>(offsetof(mVU_Globals, name))

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
// Helper Macros
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

// Misc Macros...
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

// Fetches the PC and instruction opcode relative to the current PC.  Used to rewind and
// fast-forward the IR state while calculating VU pipeline conditions (branches, writebacks, etc)
#define incPC(x)  { iPC = ((iPC + (x)) & mVU.progMemMask); mVU.code = curI; }
#define incPC2(x) { iPC = ((iPC + (x)) & mVU.progMemMask); }

// Flag Info (Set if next-block's first 4 ops will read current-block's flags)
#define __Status (mVUregs.needExactMatch & 1)
#define __Mac    (mVUregs.needExactMatch & 2)
#define __Clip   (mVUregs.needExactMatch & 4)

// Pass 3 Helper Macros (Used for program logging)
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

// Program Logging...
#ifdef mVUlogProg
	#define mVUlog      ((isVU1) ? __mVULog<1> : __mVULog<0>)
	#define mVUdumpProg __mVUdumpProgram
#else
	#define mVUlog(...)      if (0) {}
	#define mVUdumpProg(...) if (0) {}
#endif

//------------------------------------------------------------------
// Optimization / Debug Options
//------------------------------------------------------------------

// These are the same as x86, see x86/microVU_Misc.h for descriptions.
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

// Status Flag Speed Hack
#define CHECK_VU_FLAGHACK (EmuConfig.Speedhacks.vuFlagHack)

//------------------------------------------------------------------
// Address helpers
//------------------------------------------------------------------

// A memory operand relative to a base register, with an additional constant offset.
// Used for VU memory accesses where the base is either RMVUMEM or a computed address.
struct mVUAddr
{
	a64::Register base;
	s64 offset;

	mVUAddr(const a64::Register& b, s64 o = 0) : base(b), offset(o) {}
	mVUAddr operator+(s64 o) const { return mVUAddr(base, offset + o); }
	a64::MemOperand mem(s64 extra = 0) const { return a64::MemOperand(base, offset + extra); }
};

extern void mVUmergeRegs(const xmm& dest, const xmm& src, int xyzw, bool modXYZW = false);
extern void mVUsaveReg(const xmm& reg, const mVUAddr& ptr, int xyzw, bool modXYZW);
extern void mVUloadReg(const xmm& reg, const mVUAddr& ptr, int xyzw);
