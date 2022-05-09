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

#ifndef _PCSX2_CORE_RECOMPILER_
#define _PCSX2_CORE_RECOMPILER_

#include "common/emitter/x86emitter.h"
#include "VUmicro.h"

// Namespace Note : iCore32 contains all of the Register Allocation logic, in addition to a handful
// of utility functions for emitting frequent code.

////////////////////////////////////////////////////////////////////////////////
// Shared Register allocation flags (apply to X86, XMM, MMX, etc).

#define MODE_READ        1
#define MODE_WRITE       2
#define MODE_READHALF    4 // read only low 64 bits
#define MODE_VUXY        8 // vector only has xy valid (real zw are in mem), not the same as MODE_READHALF
#define MODE_VUZ      0x10 // z only doesn't work for now
#define MODE_VUXYZ (MODE_VUZ | MODE_VUXY) // vector only has xyz valid (real w is in memory)
#define MODE_NOFLUSH  0x20 // can't flush reg to mem
#define MODE_NOFRAME  0x40 // when allocating x86regs, don't use ebp reg
#define MODE_8BITREG  0x80 // when allocating x86regs, use only eax, ecx, edx, and ebx

#define PROCESS_EE_XMM 0x02

// currently only used in FPU
#define PROCESS_EE_S 0x04 // S is valid, otherwise take from mem
#define PROCESS_EE_T 0x08 // T is valid, otherwise take from mem

// not used in VU recs
#define PROCESS_EE_MODEWRITES 0x10 // if s is a reg, set if not in cpuRegs
#define PROCESS_EE_MODEWRITET 0x20 // if t is a reg, set if not in cpuRegs
#define PROCESS_EE_LO         0x40 // lo reg is valid
#define PROCESS_EE_HI         0x80 // hi reg is valid
#define PROCESS_EE_ACC        0x40 // acc reg is valid

// used in VU recs
#define PROCESS_VU_UPDATEFLAGS 0x10
#define PROCESS_VU_COP2 0x80 // simple cop2

#define EEREC_S    (((info) >>  8) & 0xf)
#define EEREC_T    (((info) >> 12) & 0xf)
#define EEREC_D    (((info) >> 16) & 0xf)
#define EEREC_LO   (((info) >> 20) & 0xf)
#define EEREC_HI   (((info) >> 24) & 0xf)
#define EEREC_ACC  (((info) >> 20) & 0xf)
#define EEREC_TEMP (((info) >> 24) & 0xf)
#define VUREC_FMAC ((info)&0x80000000)

#define PROCESS_EE_SET_S(reg)   ((reg) <<  8)
#define PROCESS_EE_SET_T(reg)   ((reg) << 12)
#define PROCESS_EE_SET_D(reg)   ((reg) << 16)
#define PROCESS_EE_SET_LO(reg)  ((reg) << 20)
#define PROCESS_EE_SET_HI(reg)  ((reg) << 24)
#define PROCESS_EE_SET_ACC(reg) ((reg) << 20)

#define PROCESS_VU_SET_ACC(reg) PROCESS_EE_SET_ACC(reg)
#define PROCESS_VU_SET_TEMP(reg) ((reg) << 24)

#define PROCESS_VU_SET_FMAC() 0x80000000

// special info not related to above flags
#define PROCESS_CONSTS 1
#define PROCESS_CONSTT 2

////////////////////////////////////////////////////////////////////////////////
//   X86 (32-bit) Register Allocation Tools

#define X86TYPE_TEMP 0
#define X86TYPE_GPR 1
#define X86TYPE_VI 2
#define X86TYPE_MEMOFFSET 3
#define X86TYPE_VIMEMOFFSET 4
#define X86TYPE_VUQREAD 5
#define X86TYPE_VUPREAD 6
#define X86TYPE_VUQWRITE 7
#define X86TYPE_VUPWRITE 8
#define X86TYPE_PSX 9
#define X86TYPE_PCWRITEBACK 10
#define X86TYPE_VUJUMP 12 // jump from random mem (g_recWriteback)
#define X86TYPE_VITEMP 13
#define X86TYPE_FNARG 14 // function parameter, max is 4

#define X86TYPE_VU1 0x80

//#define X86_ISVI(type) ((type&~X86TYPE_VU1) == X86TYPE_VI)
static __fi int X86_ISVI(int type)
{
	return ((type & ~X86TYPE_VU1) == X86TYPE_VI);
}

struct _x86regs
{
	u8 inuse;
	u8 reg; // value of 0 - not used
	u8 mode;
	u8 needed;
	u8 type; // X86TYPE_
	u16 counter;
	u32 extra; // extra info assoc with the reg
};

extern _x86regs x86regs[iREGCNT_GPR], s_saveX86regs[iREGCNT_GPR];

uptr _x86GetAddr(int type, int reg);
void _initX86regs();
int _getFreeX86reg(int mode);
int _allocX86reg(x86Emitter::xRegister32 x86reg, int type, int reg, int mode);
void _deleteX86reg(int type, int reg, int flush);
int _checkX86reg(int type, int reg, int mode);
void _addNeededX86reg(int type, int reg);
void _clearNeededX86regs();
void _freeX86reg(const x86Emitter::xRegister32& x86reg);
void _freeX86reg(int x86reg);
void _freeX86regs();
void _flushCachedRegs();
void _flushConstRegs();
void _flushConstReg(int reg);

////////////////////////////////////////////////////////////////////////////////
//   XMM (128-bit) Register Allocation Tools

#define XMM_CONV_VU(VU) (VU == &VU1)

#define XMMTYPE_TEMP   0 // has to be 0
#define XMMTYPE_VFREG  1
#define XMMTYPE_ACC    2
#define XMMTYPE_FPREG  3
#define XMMTYPE_FPACC  4
#define XMMTYPE_GPRREG 5

// lo and hi regs
#define XMMGPR_LO  33
#define XMMGPR_HI  32
#define XMMFPU_ACC 32

struct _xmmregs
{
	u8 inuse;
	u8 reg;
	u8 type;
	u8 mode;
	u8 needed;
	u8 VU; // 0 = VU0, 1 = VU1
	u16 counter;
};

void _cop2BackupRegs();
void _cop2RestoreRegs();
void _initXMMregs();
int _getFreeXMMreg();
int _allocTempXMMreg(XMMSSEType type, int xmmreg);
int _allocFPtoXMMreg(int xmmreg, int fpreg, int mode);
int _allocGPRtoXMMreg(int xmmreg, int gprreg, int mode);
int _allocFPACCtoXMMreg(int xmmreg, int mode);
int _checkXMMreg(int type, int reg, int mode);
void _addNeededFPtoXMMreg(int fpreg);
void _addNeededFPACCtoXMMreg();
void _addNeededGPRtoXMMreg(int gprreg);
void _clearNeededXMMregs();
//void _deleteACCtoXMMreg(int vu, int flush);
void _deleteGPRtoXMMreg(int reg, int flush);
void _deleteFPtoXMMreg(int reg, int flush);
void _freeXMMreg(u32 xmmreg);
void _clearNeededCOP2Regs();
u16 _freeXMMregsCOP2();
//void _moveXMMreg(int xmmreg); // instead of freeing, moves it to a diff location
void _flushXMMregs();
u8 _hasFreeXMMreg();
void _freeXMMregs();
int _getNumXMMwrite();
void _signExtendSFtoM(uptr mem);

// returns new index of reg, lower 32 bits already in mmx
// shift is used when the data is in the top bits of the mmx reg to begin with
// a negative shift is for sign extension
int _signExtendXMMtoM(uptr to, x86SSERegType from, int candestroy); // returns true if reg destroyed

//////////////////////
// Instruction Info //
//////////////////////
// Liveness information for the noobs :)
// Let's take I instructions that read from RN register set and write to
// WN register set.
// 1/ EEINST_USED will be set in register N of instruction I1, if and only if RN or WN is used in the insruction I2 with I2 >= I1.
// In others words, it will be set on [I0, ILast] with ILast the last instruction that use the register.
// 2/ EEINST_LASTUSE will be set in register N the last instruction that use the register.
// Note: EEINST_USED will be cleared after EEINST_LASTUSE
// My guess: both variable allow to detect register that can be flushed for free
//
// 3/ EEINST_LIVE* is cleared when register is written. And set again when register is read.
// My guess: the purpose is to detect the usage hole in the flow

#define EEINST_LIVE0     1 // if var is ever used (read or write)
#define EEINST_LIVE2     4 // if cur var's next 64 bits are needed
#define EEINST_LASTUSE   8 // if var isn't written/read anymore
//#define EEINST_MMX    0x10 // removed
#define EEINST_XMM    0x20 // var will be used in xmm ops
#define EEINST_USED   0x40

#define EEINSTINFO_COP1 1
#define EEINSTINFO_COP2 2

#define EEINST_COP2_DENORMALIZE_STATUS_FLAG 0x100
#define EEINST_COP2_NORMALIZE_STATUS_FLAG 0x200
#define EEINST_COP2_STATUS_FLAG 0x400
#define EEINST_COP2_MAC_FLAG 0x800
#define EEINST_COP2_CLIP_FLAG 0x1000

struct EEINST
{
	u16 info; // extra info, if 1 inst is COP1, 2 inst is COP2. Also uses EEINST_XMM
	u8 regs[34]; // includes HI/LO (HI=32, LO=33)
	u8 fpuregs[33]; // ACC=32

	// uses XMMTYPE_ flags; if type == XMMTYPE_TEMP, not used
	u8 writeType[3], writeReg[3]; // reg written in this inst, 0 if no reg
	u8 readType[4], readReg[4];

	// valid if info & EEINSTINFO_COP2
	int cycle; // cycle of inst (at offset from block)
	_VURegsNum vuregs;
};

extern EEINST* g_pCurInstInfo; // info for the cur instruction
extern void _recClearInst(EEINST* pinst);

// returns the number of insts + 1 until written (0 if not written)
extern u32 _recIsRegWritten(EEINST* pinst, int size, u8 xmmtype, u8 reg);
// returns the number of insts + 1 until used (0 if not used)
//extern u32 _recIsRegUsed(EEINST* pinst, int size, u8 xmmtype, u8 reg);
extern void _recFillRegister(EEINST& pinst, int type, int reg, int write);

static __fi bool EEINST_ISLIVE64(u32 reg)  { return !!(g_pCurInstInfo->regs[reg] & (EEINST_LIVE0)); }
static __fi bool EEINST_ISLIVEXMM(u32 reg) { return !!(g_pCurInstInfo->regs[reg] & (EEINST_LIVE0 | EEINST_LIVE2)); }
static __fi bool EEINST_ISLIVE2(u32 reg)   { return !!(g_pCurInstInfo->regs[reg] & EEINST_LIVE2); }

static __fi bool FPUINST_ISLIVE(u32 reg)   { return !!(g_pCurInstInfo->fpuregs[reg] & EEINST_LIVE0); }
static __fi bool FPUINST_LASTUSE(u32 reg)  { return !!(g_pCurInstInfo->fpuregs[reg] & EEINST_LASTUSE); }

extern u32 g_recWriteback; // used for jumps (VUrec mess!)

extern _xmmregs xmmregs[iREGCNT_XMM], s_saveXMMregs[iREGCNT_XMM];

extern thread_local u8* j8Ptr[32];   // depreciated item.  use local u8* vars instead.
extern thread_local u32* j32Ptr[32]; // depreciated item.  use local u32* vars instead.

extern u16 g_x86AllocCounter;
extern u16 g_xmmAllocCounter;

// allocates only if later insts use XMM, otherwise checks
int _allocCheckGPRtoXMM(EEINST* pinst, int gprreg, int mode);
int _allocCheckFPUtoXMM(EEINST* pinst, int fpureg, int mode);

// allocates only if later insts use this register
int _allocCheckGPRtoX86(EEINST* pinst, int gprreg, int mode);

//////////////////////////////////////////////////////////////////////////
// iFlushCall / _psxFlushCall Parameters

// Flushing vs. Freeing, as understood by Air (I could be wrong still....)

// "Freeing" registers means that the contents of the registers are flushed to memory.
// This is good for any sort of C code function that plans to modify the actual
// registers.  When the Recs resume, they'll reload the registers with values saved
// as needed.  (similar to a "FreezeXMMRegs")

// "Flushing" means that in addition to the standard free (which is actually a flush)
// the register allocations are additionally wiped.  This should only be necessary if
// the code being called is going to modify register allocations -- ie, be doing
// some kind of recompiling of its own.

#define FLUSH_CACHED_REGS  0x001
#define FLUSH_FLUSH_XMM    0x002
#define FLUSH_FREE_XMM     0x004 // both flushes and frees
#define FLUSH_FLUSH_ALLX86 0x020 // flush x86
#define FLUSH_FREE_TEMPX86 0x040 // flush and free temporary x86 regs
#define FLUSH_FREE_ALLX86  0x080 // free all x86 regs
#define FLUSH_FREE_VU0     0x100 // free all vu0 related regs
#define FLUSH_PC           0x200 // program counter
#define FLUSH_CAUSE        0x000 // disabled for now: cause register, only the branch delay bit
#define FLUSH_CODE         0x800 // opcode for interpreter

#define FLUSH_EVERYTHING   0x1ff
//#define FLUSH_EXCEPTION		0x1ff   // will probably do this totally differently actually
#define FLUSH_INTERPRETER  0xfff
#define FLUSH_FULLVTLB FLUSH_NOCONST

// no freeing, used when callee won't destroy xmm regs
#define FLUSH_NODESTROY (FLUSH_CACHED_REGS | FLUSH_FLUSH_XMM | FLUSH_FLUSH_ALLX86)
// used when regs aren't going to be changed be callee
#define FLUSH_NOCONST (FLUSH_FREE_XMM | FLUSH_FREE_TEMPX86)

#endif
