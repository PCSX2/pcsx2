// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "arm64/AsmHelpers.h"
#include "VUmicro.h"

// ARM64 Register Allocator
// Mirrors the x86 allocator in x86/iCore.h but adapted for ARM64 register conventions.

//#define RALOG(...) fprintf(stderr, __VA_ARGS__)
#define RALOG(...)

////////////////////////////////////////////////////////////////////////////////
// Shared Register Allocation Flags (same as x86 — shared with instruction codegen)

#define MODE_READ        1
#define MODE_WRITE       2
#define MODE_CALLEESAVED  0x20
#define MODE_COP2 0x40

#define PROCESS_EE_XMM 0x02

#define PROCESS_EE_S 0x04
#define PROCESS_EE_T 0x08
#define PROCESS_EE_D 0x10

#define PROCESS_EE_LO         0x40
#define PROCESS_EE_HI         0x80
#define PROCESS_EE_ACC        0x40

// Extract host register index from info bitmask.
// ARM64 needs 5 bits per field (registers 0-28), unlike x86 which uses 4 bits (0-15).
// Fields are packed into a 32-bit info word with 5-bit register indices.
//
// NOTE: EEREC_LO, EEREC_HI and EEREC_ACC intentionally decode the SAME field
// (bits 23..27). Five distinct 5-bit fields plus the presence flags overflow the
// 32-bit word, so LO/HI/ACC share one slot. This is only sound because no op needs
// two of them live simultaneously through the allocator: integer MULT/DIV/MADD and
// PMFHL load LO/HI directly from memory (bypassing the allocator), and ACC is used
// exclusively by FPU ops (never alongside LO/HI). eeRecompileCodeXMM asserts the one
// dangerous combination (LO+HI); an op needing both must bypass the allocator too.
#define EEREC_S    (((info) >>  8) & 0x1f)
#define EEREC_T    (((info) >> 13) & 0x1f)
#define EEREC_D    (((info) >> 18) & 0x1f)
#define EEREC_LO   (((info) >> 23) & 0x1f)
#define EEREC_HI   (((info) >> 23) & 0x1f)
#define EEREC_ACC  (((info) >> 23) & 0x1f)

#define PROCESS_EE_SET_S(reg)   (((reg) <<  8) | PROCESS_EE_S)
#define PROCESS_EE_SET_T(reg)   (((reg) << 13) | PROCESS_EE_T)
#define PROCESS_EE_SET_D(reg)   (((reg) << 18) | PROCESS_EE_D)
#define PROCESS_EE_SET_LO(reg)  (((reg) << 23) | PROCESS_EE_LO)
#define PROCESS_EE_SET_HI(reg)  (((reg) << 23) | PROCESS_EE_HI)
#define PROCESS_EE_SET_ACC(reg) (((reg) << 23) | PROCESS_EE_ACC)

#define PROCESS_CONSTS 1
#define PROCESS_CONSTT 2

////////////////////////////////////////////////////////////////////////////////
// NEON (128-bit) Register Allocation — equivalent to XMM on x86

enum xmminfo : u16
{
	XMMINFO_READLO = 0x001,
	XMMINFO_READHI = 0x002,
	XMMINFO_WRITELO = 0x004,
	XMMINFO_WRITEHI = 0x008,
	XMMINFO_WRITED = 0x010,
	XMMINFO_READD = 0x020,
	XMMINFO_READS = 0x040,
	XMMINFO_READT = 0x080,
	XMMINFO_READACC = 0x200,
	XMMINFO_WRITEACC = 0x400,
	XMMINFO_WRITET = 0x800,

	XMMINFO_64BITOP = 0x1000,
	XMMINFO_FORCEREGS = 0x2000,
	XMMINFO_FORCEREGT = 0x4000,
	XMMINFO_NORENAME = 0x8000
};

////////////////////////////////////////////////////////////////////////////////
// ARM64 GPR Register Allocation

// Total number of ARM64 GPR registers we track (x0-x30 = 31)
static constexpr int NUM_ARM_GPR_REGS = 31;

// Total number of ARM64 NEON registers we track (q0-q28, excluding q29-q31 scratch)
static constexpr int NUM_ARM_NEON_REGS = 29;

enum arm64gprtype : u8
{
	ARM64TYPE_TEMP = 0,
	ARM64TYPE_GPR = 1,       // EE GPR (lower 64 bits)
	ARM64TYPE_FPRC = 2,      // FPU control register
	ARM64TYPE_VIREG = 3,     // VU integer register
	ARM64TYPE_PCWRITEBACK = 4,
	ARM64TYPE_PSX = 5,       // IOP GPR
	ARM64TYPE_PSX_PCWRITEBACK = 6
};

struct _arm64gprregs
{
	u8 inuse;
	s8 reg;       // guest register index
	u8 mode;      // MODE_READ / MODE_WRITE
	u8 needed;    // pinned for current instruction
	u8 type;      // ARM64TYPE_*
	u8 looppin;   // SL-1 loop-resident entry: LRU eviction avoids it (preference,
	              // not a bar — seams and explicit deletes still free it; the
	              // back-edge reconcile restores the mapping)
	u16 counter;  // LRU counter
	u32 extra;    // extra info (e.g., IOP constant regs)
};

// NEON register types — same values as XMM types for compatibility
#define NEONTYPE_TEMP   0
#define NEONTYPE_GPRREG 1  // EE GPR (full 128 bits)
#define NEONTYPE_FPREG  6  // FPU register
#define NEONTYPE_FPACC  7  // FPU accumulator
#define NEONTYPE_VFREG  8  // VU VF register

// Callee-saved NEON range available to the allocator: q10-q15 (q8/q9 hold
// the pinned FPU clamp constants and are excluded from the pool entirely).
// AAPCS64 preserves only the LOWER 64 bits of v8-v15 across C calls, so
// full-128-bit classes (NEONTYPE_GPRREG quads, VFREG) can never be retained
// across a seam — but 32-bit FPR-class slots (FPREG/FPACC, lane 0 only) can
// (GE-15; iFlushCall's retention loop keys off this range).
static constexpr u32 NEON_CALLEE_SAVED_START = 10;
static constexpr u32 NEON_CALLEE_SAVED_END = 16; // exclusive

// x86 type aliases — used by shared analysis code (iR5900Analysis.cpp)
#define XMMTYPE_TEMP    NEONTYPE_TEMP
#define XMMTYPE_GPRREG  NEONTYPE_GPRREG
#define XMMTYPE_FPREG   NEONTYPE_FPREG
#define XMMTYPE_VFREG   NEONTYPE_VFREG
#define X86TYPE_VIREG   ARM64TYPE_VIREG
#define X86TYPE_GPR     ARM64TYPE_GPR

// Register index aliases for analysis (same values as x86)
#define XMMGPR_LO  NEONGPR_LO   // 33
#define XMMGPR_HI  NEONGPR_HI   // 32
#define XMMFPU_ACC NEONFPU_ACC   // 32

#define NEONGPR_LO  33
#define NEONGPR_HI  32
#define NEONFPU_ACC 32

enum : int
{
	DELETE_REG_FREE = 0,
	DELETE_REG_FLUSH = 1,
	DELETE_REG_FLUSH_AND_FREE = 2,
	DELETE_REG_FREE_NO_WRITEBACK = 3
};

struct _arm64neonregs
{
	u8 inuse;
	s8 reg;       // guest register index
	u8 type;      // NEONTYPE_*
	u8 mode;      // MODE_READ / MODE_WRITE
	u8 needed;    // pinned for current instruction
	u16 counter;  // LRU counter
};

////////////////////////////////////////////////////////////////////////////////
// ARM64 GPR allocator functions

extern _arm64gprregs arm64gprs[NUM_ARM_GPR_REGS], s_saveArm64GPRregs[NUM_ARM_GPR_REGS];

bool _isAllocatableArm64GPR(int armreg);
void _initArm64GPRregs();
int _getFreeArm64GPR(int mode, u32 pool);
int _allocArm64GPR(int type, int reg, int mode);
int _checkArm64GPR(int type, int reg, int mode);
bool _hasArm64GPR(int type, int reg, int required_mode = 0);
void _addNeededArm64GPR(int type, int reg);
void _clearNeededArm64GPRregs();
void _freeArm64GPR(int armreg);
void _freeArm64GPRWithoutWriteback(int armreg);
void _freeArm64GPRregs();
void _flushArm64GPRregs();
void _flushConstRegs(bool delete_const);
void _flushConstReg(int reg);
void _validateRegs();
void _writebackArm64GPR(int armreg);

void mVUFreeCOP2GPR(int hostreg);
bool mVUIsReservedCOP2(int hostreg);

////////////////////////////////////////////////////////////////////////////////
// ARM64 NEON allocator functions

extern _arm64neonregs arm64neon[NUM_ARM_NEON_REGS], s_saveArm64NEONregs[NUM_ARM_NEON_REGS];

void _initArm64NEONregs();
int _getFreeArm64NEON(u32 maxreg = NUM_ARM_NEON_REGS);
int _allocTempNEONreg();
int _allocFPtoNEONreg(int fpreg, int mode);
int _allocGPRtoNEONreg(int gprreg, int mode);
int _allocFPACCtoNEONreg(int mode);
void _reallocateNEONreg(int neonreg, int newtype, int newreg, int newmode, bool writeback = true);
int _checkNEONreg(int type, int reg, int mode);
bool _hasNEONreg(int type, int reg, int required_mode = 0);
void _addNeededFPtoNEONreg(int fpreg);
void _addNeededFPACCtoNEONreg();
void _addNeededGPRtoArm64GPR(int gprreg);
void _addNeededPSXtoArm64GPR(int gprreg);
void _addNeededGPRtoNEONreg(int gprreg);
void _clearNeededNEONregs();
void _deleteGPRtoArm64GPR(int reg, int flush);
void _deletePSXtoArm64GPR(int reg, int flush);
void _deleteGPRtoNEONreg(int reg, int flush);
void _deleteFPtoNEONreg(int reg, int flush);
void _freeNEONreg(int neonreg);
void _freeNEONregWithoutWriteback(int neonreg);
void _freeNEONregs();
void _writebackNEONreg(int neonreg);
int _allocVFtoNEONreg(int vfreg, int mode);
void mVUFreeCOP2NEONreg(int hostreg);
void _flushCOP2regs();
void _flushNEONreg(int neonreg);
void _flushNEONregs();

////////////////////////////////////////////////////////////////////////////////
// Instruction Info — architecture-independent, shared with x86
// (EEINST, liveness analysis, etc.)

#define EEINST_LIVE     1
#define EEINST_LASTUSE   8
#define EEINST_XMM    0x20  // keep name for compat — means "will use NEON/128-bit"
#define EEINST_USED   0x40

#define EEINST_COP2_DENORMALIZE_STATUS_FLAG 0x100
#define EEINST_COP2_NORMALIZE_STATUS_FLAG 0x200
#define EEINST_COP2_STATUS_FLAG 0x400
#define EEINST_COP2_MAC_FLAG 0x800
#define EEINST_COP2_CLIP_FLAG 0x1000
#define EEINST_COP2_SYNC_VU0 0x2000
#define EEINST_COP2_FINISH_VU0 0x4000
#define EEINST_COP2_FLUSH_VU0_REGISTERS 0x8000

struct EEINST
{
	u16 info;
	u8 regs[34];    // HI=32, LO=33
	u8 fpuregs[33]; // ACC=32
	u8 vfregs[34];  // ACC=32, I=33
	u8 viregs[16];

	u8 writeType[3], writeReg[3];
	u8 readType[4], readReg[4];
};

extern EEINST* g_pCurInstInfo;
extern void _recClearInst(EEINST* pinst);
extern u32 _recIsRegReadOrWritten(EEINST* pinst, int size, u8 xmmtype, u8 reg);
extern void _recFillRegister(EEINST& pinst, int type, int reg, int write);

#define EE_WRITE_DEAD_VALUES 1

static __fi bool EEINST_USEDTEST(u32 reg)
{
	return (g_pCurInstInfo->regs[reg] & (EEINST_USED | EEINST_LASTUSE)) == EEINST_USED;
}

static __fi bool EEINST_XMMUSEDTEST(u32 reg)
{
	return (g_pCurInstInfo->regs[reg] & (EEINST_USED | EEINST_XMM | EEINST_LASTUSE)) == (EEINST_USED | EEINST_XMM);
}

static __fi bool EEINST_VFUSEDTEST(u32 reg)
{
	return (g_pCurInstInfo->vfregs[reg] & (EEINST_USED | EEINST_LASTUSE)) == EEINST_USED;
}

static __fi bool EEINST_VIUSEDTEST(u32 reg)
{
	return (g_pCurInstInfo->viregs[reg] & (EEINST_USED | EEINST_LASTUSE)) == EEINST_USED;
}

static __fi bool EEINST_LIVETEST(u32 reg)
{
	return EE_WRITE_DEAD_VALUES || ((g_pCurInstInfo->regs[reg] & EEINST_LIVE) != 0);
}

static __fi bool EEINST_RENAMETEST(u32 reg)
{
	return (reg == 0 || !EEINST_USEDTEST(reg) || !EEINST_LIVETEST(reg));
}

static __fi bool FPUINST_ISLIVE(u32 reg)   { return !!(g_pCurInstInfo->fpuregs[reg] & EEINST_LIVE); }
static __fi bool FPUINST_LASTUSE(u32 reg)  { return !!(g_pCurInstInfo->fpuregs[reg] & EEINST_LASTUSE); }

static __fi bool FPUINST_USEDTEST(u32 reg)
{
	return (g_pCurInstInfo->fpuregs[reg] & (EEINST_USED | EEINST_LASTUSE)) == EEINST_USED;
}

static __fi bool FPUINST_LIVETEST(u32 reg)
{
	return EE_WRITE_DEAD_VALUES || FPUINST_ISLIVE(reg);
}

static __fi bool FPUINST_RENAMETEST(u32 reg)
{
	return (!EEINST_USEDTEST(reg) || !EEINST_LIVETEST(reg));
}

extern u16 g_arm64AllocCounter;
extern u16 g_neonAllocCounter;

// Allocates only if later instructions use this register
int _allocIfUsedGPRtoArm64(int gprreg, int mode);
int _allocIfUsedVItoArm64(int vireg, int mode);
int _allocIfUsedGPRtoNEON(int gprreg, int mode);
int _allocIfUsedFPUtoNEON(int fpureg, int mode);

////////////////////////////////////////////////////////////////////////////////
// Flush call parameters — same values as x86 for compatibility

#define FLUSH_NONE             0x000
#define FLUSH_CONSTANT_REGS    0x001
#define FLUSH_FLUSH_XMM        0x002  // flush NEON regs (keep name for compat)
#define FLUSH_FREE_XMM         0x004  // flush + free NEON regs
#define FLUSH_ALL_X86          0x020  // flush ARM64 GPRs (keep name for compat)
#define FLUSH_FREE_TEMP_X86    0x040  // flush + free temp ARM64 GPRs
#define FLUSH_FREE_NONTEMP_X86 0x080  // free non-temp ARM64 GPRs
#define FLUSH_FREE_VU0         0x100
#define FLUSH_PC               0x200
#define FLUSH_CODE             0x800

#define FLUSH_EVERYTHING   0x1ff
#define FLUSH_INTERPRETER  0xfff
#define FLUSH_FULLVTLB 0x000
#define FLUSH_NODESTROY (FLUSH_CONSTANT_REGS | FLUSH_FLUSH_XMM | FLUSH_ALL_X86)
