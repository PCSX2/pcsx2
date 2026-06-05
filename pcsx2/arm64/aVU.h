// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU recompiler — arch-neutral data structures (Phase 7, task 7.2a).
//
// This is the ARM64 counterpart to pcsx2/x86/microVU.h + microVU_IR.h. Per the
// Phase 7 strategy (PROGRESS.md / [[arm64-microvu-architecture]]) microVU is
// *parallel-cloned* into pcsx2/arm64/ rather than #included from x86/ — the x86
// headers fuse the arch-neutral structs together with x86emitter types and the
// x86 `microRegAlloc` in the same files, so they cannot be compiled on ARM64.
//
// What lives here (arch-neutral, near-verbatim copies of the x86 originals):
//   * the pipeline-state key `microRegInfo` and the IR structs
//     (`microBlock`, `microOp`, `microIR`, the per-op read/write/flag info)
//   * the program/block bookkeeping (`microProgram*`, `microProgManager`,
//     `microBlockManager`) and the top-level `microVU` context
//
// What is NOT here:
//   * `microRegAlloc` — the host register allocator (NEON v0-v31 for VF, ARM
//     w-regs for VI). It is arch-specific and is ported in task 7.2b
//     (aVU_IR.h). Only forward-declared below; `microVU` holds it by
//     unique_ptr, so the incomplete type is fine in this header. The microVU0/1
//     globals are *defined* in aVU.cpp where the allocator is complete.
//   * the emitter / dispatcher / opcode emission (tasks 7.2c onward).
//
// Renames vs the x86 original (documented in PROGRESS.md 7.2a):
//   microProgManager::x86ptr / x86start / x86end  -> codePtr / codeStart / codeEnd
//   microBlock::x86ptrStart                        -> codeStart
//   microJumpCache::x86ptrStart                    -> codeStart

#include "Common.h"
#include "common/AlignedMalloc.h"
#include "VU.h"
#include "MTVU.h"
#include "R5900.h"

// Arch-neutral opcode enum + (compiled-out by default) opcode profiler. This
// header has no x86 dependency — it is shared as-is rather than re-cloned.
#include "x86/microVU_Profiler.h"

#include <array>
#include <deque>
#include <memory>
#include <vector>

class microRegAlloc; // ARM64 host register allocator — see aVU_IR.h (task 7.2b)
class microBlockManager;

//------------------------------------------------------------------
// Pipeline-state key + IR structures (from x86/microVU_IR.h)
//------------------------------------------------------------------

struct regCycleInfo
{
	u8 x : 4;
	u8 y : 4;
	u8 z : 4;
	u8 w : 4;
};

// microRegInfo is carefully ordered for faster compares.  The "important" information is
// housed in a union that is accessed via 'quick32' so that several u8 fields can be compared
// using a pair of 32-bit equalities.
// vi15 is only used if microVU const-prop is enabled (it is *not* by default).  When constprop
// is disabled the vi15 field acts as additional padding that is required for 16 byte alignment
// needed by the (x86 xmm / ARM NEON q) compare.
union alignas(16) microRegInfo
{
	struct
	{
		union
		{
			struct
			{
				u8 needExactMatch; // If set, block needs an exact match of pipeline state
				u8 flagInfo;       // xC * 2 | xM * 2 | xS * 2 | 0 * 1 | fullFlag Valid * 1
				u8 q;
				u8 p;
				u8 xgkick;
				u8 viBackUp;       // VI reg number that was written to on branch-delay slot
				u8 blockType;      // 0 = Normal; 1,2 = Compile one instruction (E-bit/Branch Ending)
				u8 r;
			};
			u64 quick64[1];
			u32 quick32[2];
		};

		u32 xgkickcycles;
		u8 unused;
		u8 vi15v; // 'vi15' constant is valid
		u16 vi15; // Constant Prop Info for vi15

		struct
		{
			u8 VI[16];
			regCycleInfo VF[32];
		};
	};

	u128 full128[96 / sizeof(u128)];
	u64  full64[96 / sizeof(u64)];
	u32  full32[96 / sizeof(u32)];
};

// Note: the host pipeline-state compare (compareStateF) needs to be updated if this is changed
static_assert(sizeof(microRegInfo) == 96, "microRegInfo was not 96 bytes");

struct microProgram;
struct microJumpCache
{
	microJumpCache() : prog(nullptr), codeStart(nullptr) {}
	microProgram* prog;  // Program to which the entry point below is part of
	void* codeStart;     // Start of code (Entry point for block)
};

struct alignas(16) microBlock
{
	microRegInfo    pState;      // Detailed State of Pipeline
	microRegInfo    pStateEnd;   // Detailed State of Pipeline at End of Block (needed by JR/JALR opcodes)
	u8*             codeStart;   // Start of code (Entry point for block)
	microJumpCache* jumpCache;   // Will point to an array of entry points of size [16k/8] if block ends in JR/JALR
};

struct microTempRegInfo
{
	regCycleInfo VF[2]; // Holds cycle info for Fd, VF[0] = Upper Instruction, VF[1] = Lower Instruction
	u8 VFreg[2];        // Index of the VF reg
	u8 VI;              // Holds cycle info for Id
	u8 VIreg;           // Index of the VI reg
	u8 q;               // Holds cycle info for Q reg
	u8 p;               // Holds cycle info for P reg
	u8 r;               // Holds cycle info for R reg (Will never cause stalls, but useful to know if R is modified)
	u8 xgkick;          // Holds the cycle info for XGkick
};

struct microVFreg
{
	u8 reg; // Reg Index
	u8 x;   // X vector read/written to?
	u8 y;   // Y vector read/written to?
	u8 z;   // Z vector read/written to?
	u8 w;   // W vector read/written to?
};

struct microVIreg
{
	u8 reg;  // Reg Index
	u8 used; // Reg is Used? (Read/Written)
};

struct microConstInfo
{
	u8  isValid;  // Is the constant in regValue valid?
	u32 regValue; // Constant Value
};

struct microUpperOp
{
	bool eBit;             // Has E-bit set
	bool iBit;             // Has I-bit set
	bool mBit;             // Has M-bit set
	bool tBit;             // Has T-bit set
	bool dBit;             // Has D-bit set
	microVFreg VF_write;   // VF Vectors written to by this instruction
	microVFreg VF_read[2]; // VF Vectors read by this instruction
};

struct microLowerOp
{
	microVFreg VF_write;      // VF Vectors written to by this instruction
	microVFreg VF_read[2];    // VF Vectors read by this instruction
	microVIreg VI_write;      // VI reg written to by this instruction
	microVIreg VI_read[2];    // VI regs read by this instruction
	microConstInfo constJump; // Constant Reg Info for JR/JARL instructions
	u32  branch;     // Branch Type (0 = Not a Branch, 1 = B. 2 = BAL, 3~8 = Conditional Branches, 9 = JR, 10 = JALR)
	u32  kickcycles; // Number of xgkick cycles accumulated by this instruction
	bool badBranch;  // This instruction is a Branch who has another branch in its Delay Slot
	bool evilBranch; // This instruction is a Branch in a Branch Delay Slot (Instruction after badBranch)
	bool isNOP;      // This instruction is a NOP
	bool isFSSET;    // This instruction is a FSSET
	bool noWriteVF;  // Don't write back the result of a lower op to VF reg if upper op writes to same reg (or if VF = 0)
	bool backupVI;   // Backup VI reg to memory if modified before branch (branch uses old VI value unless opcode is ILW or ILWR)
	bool memReadIs;  // Read Is (VI reg) from memory (used by branches)
	bool memReadIt;  // Read If (VI reg) from memory (used by branches)
	bool readFlags;  // Current Instruction reads Status, Mac, or Clip flags
	bool isMemWrite; // Current Instruction writes to VU memory
	bool isKick;     // Op is a kick so don't count kick cycles
};

struct microFlagInst
{
	bool doFlag;      // Update Flag on this Instruction
	bool doNonSticky; // Update O,U,S,Z (non-sticky) bits on this Instruction (status flag only)
	u8   write;       // Points to the instance that should be written to (s-stage write)
	u8   lastWrite;   // Points to the instance that was last written to (most up-to-date flag)
	u8   read;        // Points to the instance that should be read by a lower instruction (t-stage read)
};

struct microFlagCycles
{
	int xStatus[4];
	int xMac[4];
	int xClip[4];
	int cycles;
};

struct microOp
{
	u8   stall;          // Info on how much current instruction stalled
	bool isBadOp;        // Cur Instruction is a bad opcode (not a legal instruction)
	bool isEOB;          // Cur Instruction is last instruction in block (End of Block)
	bool isBdelay;       // Cur Instruction in Branch Delay slot
	bool swapOps;        // Run Lower Instruction before Upper Instruction
	bool backupVF;       // Backup mVUlow.VF_write.reg, and restore it before the Upper Instruction is called
	bool doXGKICK;       // Do XGKICK transfer on this instruction
	u32  XGKICKPC;       // The PC in which the XGKick has taken place, so if we break early (before it) we don run it.
	bool doDivFlag;      // Transfer Div flag to Status Flag on this instruction
	int  readQ;          // Q instance for reading
	int  writeQ;         // Q instance for writing
	int  readP;          // P instance for reading
	int  writeP;         // P instance for writing
	microFlagInst sFlag; // Status Flag Instance Info
	microFlagInst mFlag; // Mac    Flag Instance Info
	microFlagInst cFlag; // Clip   Flag Instance Info
	microUpperOp  uOp;   // Upper Op Info
	microLowerOp  lOp;   // Lower Op Info
};

template <u32 pSize>
struct microIR
{
	microBlock       block;           // Block/Pipeline info
	microBlock*      pBlock;          // Pointer to a block in mVUblocks
	microTempRegInfo regsTemp;        // Temp Pipeline info (used so that new pipeline info isn't conflicting between upper and lower instructions in the same cycle)
	microOp          info[pSize / 2]; // Info for Instructions in current block
	microConstInfo   constReg[16];    // Simple Const Propagation Info for VI regs within blocks
	u8  branch;
	u32 cycles;    // Cycles for current block
	u32 count;     // Number of VU 64bit instructions ran (starts at 0 for each block)
	u32 curPC;     // Current PC
	u32 startPC;   // Start PC for Cur Block
	u32 sFlagHack; // Optimize out all Status flag updates if microProgram doesn't use Status flags
};

//------------------------------------------------------------------
// Program / block bookkeeping (from x86/microVU.h)
//------------------------------------------------------------------

struct microBlockLink
{
	microBlock block;
	microBlockLink* next;
};

struct microBlockLinkRef
{
	microBlock* pBlock;
	u64 quick;
};

struct microRange
{
	s32 start; // Start PC (The opcode the block starts at)
	s32 end;   // End PC   (The opcode the block ends with)
};

#define mProgSize (0x4000 / 4)
struct microProgram
{
	u32                data [mProgSize];     // Holds a copy of the VU microProgram
	microBlockManager* block[mProgSize / 2]; // Array of Block Managers
	std::deque<microRange>* ranges;          // The ranges of the microProgram that have already been recompiled
	u32 startPC; // Start PC of this program
	int idx;     // Program index
};

typedef std::deque<microProgram*> microProgramList;

struct microProgramQuick
{
	microBlockManager* block; // Quick reference to valid microBlockManager for current startPC
	microProgram*      prog;  // The microProgram who is the owner of 'block'
};

struct microProgManager
{
	microIR<mProgSize> IRinfo;             // IR information
	microProgramList*  prog [mProgSize/2]; // List of microPrograms indexed by startPC values
	microProgramQuick  quick[mProgSize/2]; // Quick reference to valid microPrograms for current execution
	microProgram*      cur;                // Pointer to currently running MicroProgram
	int                total;              // Total Number of valid MicroPrograms
	int                isSame;             // Current cached microProgram is Exact Same program as mVU.regs().Micro (-1 = unknown, 0 = No, 1 = Yes)
	int                cleared;            // Micro Program is Indeterminate so must be searched for (and if no matches are found then recompile a new one)
	u32                curFrame;           // Frame Counter
	u8*                codePtr;            // Pointer to program's recompilation code  (x86: x86ptr)
	u8*                codeStart;          // Start of program's rec-cache             (x86: x86start)
	u8*                codeEnd;            // Limit of program's rec-cache             (x86: x86end)
	microRegInfo       lpState;            // Pipeline state from where program left off (useful for continuing execution)
};

static const uint mVUcacheSafeZone =  3; // Safe-Zone for program recompilation (in megabytes)

// Const-prop of the vi15 register is off by default (matches the x86 rec).
static constexpr bool doConstProp = false;

struct microVU
{
	alignas(16) u32 statFlag[4]; // 4 instances of status flag (backup for xgkick)
	alignas(16) u32 macFlag [4]; // 4 instances of mac    flag (used in execution)
	alignas(16) u32 clipFlag[4]; // 4 instances of clip   flag (used in execution)
	alignas(16) u32 vecCTemp[4];      // Backup used in mVUclamp2()                  (x86: xmmCTemp)
	alignas(16) u32 vecBackup[32][4]; // Backup for host vector regs across XGKICK   (x86: xmmBackup[16][4]; sized for NEON v0-v31)

	u32 index;        // VU Index (VU0 or VU1)
	u32 cop2;         // VU is in COP2 mode?  (No/Yes)
	u32 vuMemSize;    // VU Main  Memory Size (in bytes)
	u32 microMemSize; // VU Micro Memory Size (in bytes)
	u32 progSize;     // VU Micro Memory Size (in u32's)
	u32 progMemMask;  // VU Micro Memory Size (in u32's)
	u32 cacheSize;    // VU Cache Size

	microProgManager               prog;     // Micro Program Data
	microProfiler                  profiler; // Opcode Profiler
	std::unique_ptr<microRegAlloc> regAlloc; // Reg Alloc Class
	std::FILE*                     logFile;  // Log File Pointer

	u8* cache;        // Dynarec Cache Start (where we will start writing the recompiled code to)
	u8* startFunct;   // Function Ptr to the recompiler dispatcher (start)
	u8* exitFunct;    // Function Ptr to the recompiler dispatcher (exit)
	u8* startFunctXG; // Function Ptr to the recompiler dispatcher (xgkick resume)
	u8* exitFunctXG;  // Function Ptr to the recompiler dispatcher (xgkick exit)
	u8* compareStateF;// Function Ptr to search which compares all state.
	u8* waitMTVU;     // Ptr to function to save registers/sync VU1 thread
	u8* copyPLState;  // Ptr to function to copy pipeline state into microVU
	u8* resumePtrXG;  // Ptr to recompiled code position to resume xgkick
	u32 code;         // Contains the current Instruction
	u32 divFlag;      // 1 instance of I/D flags
	u32 VIbackup;     // Holds a backup of a VI reg if modified before a branch
	u32 VIxgkick;     // Holds a backup of a VI reg used for xgkick-delays
	u32 branch;       // Holds branch compare result (IBxx) OR Holds address to Jump to (JALR/JR)
	u32 badBranch;    // For Branches in Branch Delay Slots, holds Address the first Branch went to + 8
	u32 evilBranch;   // For Branches in Branch Delay Slots, holds Address to Jump to
	u32 evilevilBranch;// For Branches in Branch Delay Slots (chained), holds Address to Jump to
	u32 p;            // Holds current P instance index
	u32 q;            // Holds current Q instance index
	u32 totalCycles;  // Total Cycles that mVU is expected to run for
	s32 cycles;       // Cycles Counter

	VURegs& regs() const { return ::vuRegs[index]; }
	void* textPtr() const { return (index && THREAD_VU1) ? (void*)&regs().VF[9] : (void*)&cpuRegs.GPR.r[9]; }

	__fi REG_VI& getVI(uint reg) const { return regs().VI[reg]; }
	__fi VECTOR& getVF(uint reg) const { return regs().VF[reg]; }
	__fi VIFregisters& getVifRegs() const
	{
		return (index && THREAD_VU1) ? vu1Thread.vifRegs : regs().GetVifRegs();
	}

	__fi u32 compareState(microRegInfo* lhs, microRegInfo* rhs) const {
		return reinterpret_cast<u32(*)(void*, void*)>(compareStateF)(lhs, rhs);
	}
};

class microBlockManager
{
private:
	microBlockLink *qBlockList, *qBlockEnd; // Quick Search
	microBlockLink *fBlockList, *fBlockEnd; // Full  Search
	std::vector<microBlockLinkRef> quickLookup;
	int qListI, fListI;

public:
	inline int getFullListCount() const { return fListI; }
	microBlockManager()
	{
		qListI = fListI = 0;
		qBlockEnd = qBlockList = nullptr;
		fBlockEnd = fBlockList = nullptr;
	}
	~microBlockManager() { reset(); }
	void reset()
	{
		for (microBlockLink* linkI = qBlockList; linkI != nullptr;)
		{
			microBlockLink* freeI = linkI;
			safe_delete_array(linkI->block.jumpCache);
			linkI = linkI->next;
			_aligned_free(freeI);
		}
		for (microBlockLink* linkI = fBlockList; linkI != nullptr;)
		{
			microBlockLink* freeI = linkI;
			safe_delete_array(linkI->block.jumpCache);
			linkI = linkI->next;
			_aligned_free(freeI);
		}
		qListI = fListI = 0;
		qBlockEnd = qBlockList = nullptr;
		fBlockEnd = fBlockList = nullptr;
		quickLookup.clear();
	};
	microBlock* add(microVU& mVU, microBlock* pBlock)
	{
		microBlock* thisBlock = search(mVU, &pBlock->pState);
		if (!thisBlock)
		{
			u8 fullCmp = pBlock->pState.needExactMatch;
			if (fullCmp)
				fListI++;
			else
				qListI++;

			microBlockLink*& blockList = fullCmp ? fBlockList : qBlockList;
			microBlockLink*& blockEnd  = fullCmp ? fBlockEnd  : qBlockEnd;
			microBlockLink*  newBlock  = (microBlockLink*)_aligned_malloc(sizeof(microBlockLink), 32);
			newBlock->block.jumpCache  = nullptr;
			newBlock->next             = nullptr;

			if (blockEnd)
			{
				blockEnd->next = newBlock;
				blockEnd       = newBlock;
			}
			else
			{
				blockEnd = blockList = newBlock;
			}

			std::memcpy(&newBlock->block, pBlock, sizeof(microBlock));
			thisBlock = &newBlock->block;

			quickLookup.push_back({&newBlock->block, pBlock->pState.quick64[0]});
		}
		return thisBlock;
	}
	__ri microBlock* search(microVU& mVU, microRegInfo* pState)
	{
		if (pState->needExactMatch) // Needs Detailed Search (Exact Match of Pipeline State)
		{
			microBlockLink* prevI = nullptr;
			for (microBlockLink* linkI = fBlockList; linkI != nullptr; prevI = linkI, linkI = linkI->next)
			{
				if (mVU.compareState(pState, &linkI->block.pState) == 0)
				{
					if (linkI != fBlockList)
					{
						prevI->next = linkI->next;
						linkI->next = fBlockList;
						fBlockList = linkI;
					}

					return &linkI->block;
				}
			}
		}
		else // Can do Simple Search (Only Matches the Important Pipeline Stuff)
		{
			const u64 quick64 = pState->quick64[0];
			for (const microBlockLinkRef& ref : quickLookup)
			{
				// if we're using the flag hack, ignore the mac flags going in to the new block too if an exact match wasn't requested.
				if (mVU.prog.IRinfo.sFlagHack)
				{
					if ((ref.quick & ~0x0C04) != (quick64 & ~0x0C04)) continue;
				}
				else if (ref.quick != quick64) continue;

				if (doConstProp && (ref.pBlock->pState.vi15 != pState->vi15))  continue;
				if (doConstProp && (ref.pBlock->pState.vi15v != pState->vi15v)) continue;
				return ref.pBlock;
			}
		}
		return nullptr;
	}
	void printInfo(int pc, bool printQuick)
	{
		int listI = printQuick ? qListI : fListI;
		if (listI < 7)
			return;
		microBlockLink* linkI = printQuick ? qBlockList : fBlockList;
		for (int i = 0; i <= listI; i++)
		{
			u32 viCRC = 0, vfCRC = 0, crc = 0, z = sizeof(microRegInfo) / 4;
			for (u32 j = 0; j < 4;  j++) viCRC -= ((u32*)linkI->block.pState.VI)[j];
			for (u32 j = 0; j < 32; j++) vfCRC -= linkI->block.pState.VF[j].x + (linkI->block.pState.VF[j].y << 8) + (linkI->block.pState.VF[j].z << 16) + (linkI->block.pState.VF[j].w << 24);
			for (u32 j = 0; j < z;  j++) crc   -= ((u32*)&linkI->block.pState)[j];
			DevCon.WriteLn(Color_Green,
				"[%04x][Block #%d][crc=%08x][q=%02d][p=%02d][xgkick=%d][vi15=%04x][vi15v=%d][viBackup=%02d]"
				"[flags=%02x][exactMatch=%x][blockType=%d][viCRC=%08x][vfCRC=%08x]",
				pc, i, crc, linkI->block.pState.q,
				linkI->block.pState.p, linkI->block.pState.xgkick, linkI->block.pState.vi15, linkI->block.pState.vi15v,
				linkI->block.pState.viBackUp, linkI->block.pState.flagInfo, linkI->block.pState.needExactMatch,
				linkI->block.pState.blockType, viCRC, vfCRC);
			linkI = linkI->next;
		}
	}
};

// microVU rec contexts (defined in aVU.cpp, where microRegAlloc is complete).
extern microVU microVU0;
extern microVU microVU1;
