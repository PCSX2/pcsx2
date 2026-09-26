// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 port of x86/microVU_IR.h. The IR structures are identical, the register allocator
// targets NEON/AArch64 registers.
#pragma once
#include "microVU.h"
#include <array>

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
// needed by the xmm compare.
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

// Note: mVUcustomSearch needs to be updated if this is changed
static_assert(sizeof(microRegInfo) == 96, "microRegInfo was not 96 bytes");

struct microProgram;
struct microJumpCache
{
	microJumpCache() : prog(NULL), x86ptrStart(NULL) {}
	microProgram* prog; // Program to which the entry point below is part of
	void* x86ptrStart;  // Start of code (Entry point for block)
};

struct alignas(16) microBlock
{
	microRegInfo    pState;      // Detailed State of Pipeline
	microRegInfo    pStateEnd;   // Detailed State of Pipeline at End of Block (needed by JR/JALR opcodes)
	u8*             x86ptrStart; // Start of code (Entry point for block)
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
// Reg Alloc
//------------------------------------------------------------------

// ARM64 port of the x86 microRegAlloc. The allocation logic is the same, minus the
// sharing of registers with the EE recompiler (COP2 mode), which the ARM64 EE rec doesn't do.

//#define MVURALOG(...) fprintf(stderr, __VA_ARGS__)
#define MVURALOG(...)

struct microMapXMM
{
	int  VFreg;    // VF Reg Number Stored (-1 = Temp; 0 = vf0 and will not be written back; 32 = ACC; 33 = I reg)
	int  xyzw;     // xyzw to write back (0 = Don't write back anything AND cached vfReg has all vectors valid)
	int  count;    // Count of when last used
	bool isNeeded; // Is needed for current instruction
	bool isZero;   // Register was loaded from VF00 and doesn't need clamping
};

struct microMapGPR
{
	int VIreg;
	int count;
	bool isNeeded;
	bool dirty;
	bool isZeroExtended;
	bool usable;
};

// Emission helpers used by the register allocator, defined in microVU_Misc.inl.
extern void mVUmovReg(const xmm& dst, const xmm& src);
extern void mVUshufD(const xmm& dst, const xmm& src, int imm);
extern a64::MemOperand mVUptr(u32 vuIndex, const void* addr);

static __fi bool mVUIsCallerSavedGPR(int i)
{
	return (i < 19);
}

class microRegAlloc
{
protected:
	static const int xmmTotal = mVUXmmTotal; // PQ and scratch registers are reserved
	static const int gprTotal = mVUGprTotal;

	std::array<microMapXMM, xmmTotal> xmmMap;
	std::array<microMapGPR, gprTotal> gprMap;

	int counter; // Current allocation count
	int index;   // VU0 or VU1

	// Helper functions to get VU regs
	VURegs& regs() const { return ::vuRegs[index]; }
	__fi REG_VI& getVI(uint reg) const { return regs().VI[reg]; }
	__fi VECTOR& getVF(uint reg) const { return regs().VF[reg]; }

	__fi mVUAddr VFaddr(int vfreg) const
	{
		const s64 offset = (vfreg == 32) ? static_cast<s64>(offsetof(VURegs, ACC)) :
		                                   static_cast<s64>(offsetof(VURegs, VF) + vfreg * sizeof(VECTOR));
		return mVUAddr(RMVUREGS, offset);
	}

	__fi a64::MemOperand VIaddr(int vireg) const
	{
		return a64::MemOperand(RMVUREGS, static_cast<s64>(offsetof(VURegs, VI) + vireg * sizeof(REG_VI)));
	}

	__ri void loadIreg(const xmm& reg, int xyzw)
	{
		for (int i = 0; i < gprTotal; i++)
		{
			if (gprMap[i].VIreg == REG_I)
			{
				armAsm->Fmov(reg.S(), x32(i).W());
				if (!_XYZWss(xyzw))
					armAsm->Dup(reg.V4S(), reg.V4S(), 0);

				return;
			}
		}

		armAsm->Ldr(reg.S(), VIaddr(REG_I));
		if (!_XYZWss(xyzw))
			armAsm->Dup(reg.V4S(), reg.V4S(), 0);
	}

	int findFreeRegRec(int startIdx)
	{
		for (int i = startIdx; i < xmmTotal; i++)
		{
			if (!xmmMap[i].isNeeded)
			{
				int x = findFreeRegRec(i + 1);
				if (x == -1)
					return i;
				return ((xmmMap[i].count < xmmMap[x].count) ? i : x);
			}
		}
		return -1;
	}

	int findFreeReg(int vfreg)
	{
		for (int i = 0; i < xmmTotal; i++)
		{
			if (!xmmMap[i].isNeeded && (xmmMap[i].VFreg < 0))
			{
				return i; // Reg is not needed and was a temp reg
			}
		}
		int x = findFreeRegRec(0);
		pxAssertMsg(x >= 0, "microVU register allocation failure!");
		return x;
	}

	int findFreeGPRRec(int startIdx)
	{
		for (int i = startIdx; i < gprTotal; i++)
		{
			if (gprMap[i].usable && !gprMap[i].isNeeded)
			{
				int x = findFreeGPRRec(i + 1);
				if (x == -1)
					return i;
				return ((gprMap[i].count < gprMap[x].count) ? i : x);
			}
		}
		return -1;
	}

	int findFreeGPR(int vireg)
	{
		for (int i = 0; i < gprTotal; i++)
		{
			if (gprMap[i].usable && !gprMap[i].isNeeded && (gprMap[i].VIreg < 0))
			{
				return i; // Reg is not needed and was a temp reg
			}
		}
		int x = findFreeGPRRec(0);
		pxAssertMsg(x >= 0, "microVU register allocation failure!");
		return x;
	}

	void writeVIBackup(const x32& reg);

public:
	microRegAlloc(int _index)
	{
		index = _index;

		// Only mark registers which aren't reserved as usable:
		// x0/x1 are gprT1/gprT2, x8 is a helper temp, x16/x17 are scratch, x18 is the platform register,
		// x19-x26 are pinned (VU state and status flags), x29/x30 are the frame pointer and link register.
		gprMap.fill({0, 0, false, false, false, false});
		static constexpr int usable_gprs[] = {2, 3, 4, 5, 6, 7, 9, 10, 11, 12, 13, 14, 15, 27, 28};
		for (const int i : usable_gprs)
			gprMap[i].usable = true;

		reset();
	}

	// Fully resets the regalloc by clearing all cached data
	void reset()
	{
		for (int i = 0; i < xmmTotal; i++)
			clearReg(i);
		for (int i = 0; i < gprTotal; i++)
			clearGPR(i);

		counter = 0;
	}

	int getXmmCount()
	{
		return xmmTotal + 1;
	}

	int getFreeXmmCount()
	{
		int count = 0;

		for (int i = 0; i < xmmTotal; i++)
		{
			if (!xmmMap[i].isNeeded && (xmmMap[i].VFreg < 0))
				count++;
		}

		return count;
	}

	bool hasRegVF(int vfreg)
	{
		for (int i = 0; i < xmmTotal; i++)
		{
			if (xmmMap[i].VFreg == vfreg)
				return true;
		}

		return false;
	}

	int getRegVF(int i)
	{
		return (i < xmmTotal) ? xmmMap[i].VFreg : -1;
	}

	int getGPRCount()
	{
		return gprTotal;
	}

	int getFreeGPRCount()
	{
		int count = 0;

		for (int i = 0; i < gprTotal; i++)
		{
			if (gprMap[i].usable && !gprMap[i].isNeeded && (gprMap[i].VIreg < 0))
				count++;
		}

		return count;
	}

	bool hasRegVI(int vireg)
	{
		for (int i = 0; i < gprTotal; i++)
		{
			if (gprMap[i].VIreg == vireg)
				return true;
		}

		return false;
	}

	int getRegVI(int i)
	{
		return (i < gprTotal) ? gprMap[i].VIreg : -1;
	}

	// Flushes all allocated registers (i.e. writes-back to memory all modified registers).
	// If clearState is 0, then it keeps cached reg data valid
	// If clearState is 1, then it invalidates all cached reg data after write-back
	void flushAll(bool clearState = true)
	{
		for (int i = 0; i < xmmTotal; i++)
		{
			writeBackReg(xmm(i));
			if (clearState)
				clearReg(i);
		}

		for (int i = 0; i < gprTotal; i++)
		{
			writeBackReg(x32(i), true);
			if (clearState)
				clearGPR(i);
		}
	}

	// All 128-bit vector registers are caller-saved on AArch64 (only the low 64 bits of v8-v15 are preserved).
	void flushCallerSavedRegisters(bool clearNeeded = false)
	{
		for (int i = 0; i < xmmTotal; i++)
		{
			writeBackReg(xmm(i));
			if (clearNeeded || !xmmMap[i].isNeeded)
				clearReg(i);
		}

		for (int i = 0; i < gprTotal; i++)
		{
			if (!mVUIsCallerSavedGPR(i))
				continue;

			writeBackReg(x32(i), true);
			if (clearNeeded || !gprMap[i].isNeeded)
				clearGPR(i);
		}
	}

	void TDwritebackAll()
	{
		// NOTE: We don't clear state here, this happens in an optional branch

		for (int i = 0; i < xmmTotal; i++)
		{
			microMapXMM& mapX = xmmMap[i];

			if ((mapX.VFreg > 0) && mapX.xyzw) // Reg was modified and not Temp or vf0
			{
				if (mapX.VFreg == 33)
					armAsm->Str(xmm(i).S(), VIaddr(REG_I));
				else
					mVUsaveReg(xmm(i), VFaddr(mapX.VFreg), mapX.xyzw, 1); // doesn't modify the source on ARM64
			}
		}

		for (int i = 0; i < gprTotal; i++)
			writeBackReg(x32(i), false);
	}

	bool checkVFClamp(int regId)
	{
		if (regId != xmmPQ.Id && ((regId < xmmTotal) && ((xmmMap[regId].VFreg == 33 && !EmuConfig.Gamefixes.IbitHack) || xmmMap[regId].isZero)))
			return false;
		else
			return true;
	}

	bool checkCachedReg(int regId)
	{
		if (regId < xmmTotal)
			return xmmMap[regId].VFreg >= 0 || xmmMap[regId].isNeeded;
		else
			return false;
	}

	bool checkCachedGPR(int regId)
	{
		if (regId < gprTotal)
			return gprMap[regId].VIreg >= 0 || gprMap[regId].isNeeded;
		else
			return false;
	}

	void clearReg(const xmm& reg) { clearReg(reg.Id); }
	void clearReg(int regId)
	{
		if (regId < 0 || regId >= xmmTotal)
			return;

		xmmMap[regId] = {-1, 0, 0, false, false};
	}

	void clearRegVF(int VFreg)
	{
		for (int i = 0; i < xmmTotal; i++)
		{
			if (xmmMap[i].VFreg == VFreg)
				clearReg(i);
		}
	}

	// Writes back modified reg to memory.
	// If all vectors modified, then keeps the VF reg cached in the xmm register.
	// If reg was not modified, then keeps the VF reg cached in the xmm register.
	void writeBackReg(const xmm& reg, bool invalidateRegs = true)
	{
		if (reg.Id < 0 || reg.Id >= xmmTotal)
			return;

		microMapXMM& mapX = xmmMap[reg.Id];

		if ((mapX.VFreg > 0) && mapX.xyzw) // Reg was modified and not Temp or vf0
		{
			if (mapX.VFreg == 33)
				armAsm->Str(reg.S(), VIaddr(REG_I));
			else
				mVUsaveReg(reg, VFaddr(mapX.VFreg), mapX.xyzw, true);

			if (invalidateRegs)
			{
				for (int i = 0; i < xmmTotal; i++)
				{
					microMapXMM& mapI = xmmMap[i];

					if ((i == reg.Id) || mapI.isNeeded)
						continue;

					if (mapI.VFreg == mapX.VFreg)
					{
						if (mapI.xyzw && mapI.xyzw < 0xf)
							DevCon.Error("microVU Error: writeBackReg() [%d]", mapI.VFreg);
						clearReg(i); // Invalidate any Cached Regs of same vf Reg
					}
				}
			}
			if (mapX.xyzw == 0xf) // Make Cached Reg if All Vectors were Modified
			{
				mapX.count    = counter;
				mapX.xyzw     = 0;
				mapX.isNeeded = false;
				return;
			}
			clearReg(reg);
		}
		else if (mapX.xyzw) // Clear reg if modified and is VF0 or temp reg...
		{
			clearReg(reg);
		}
	}

	// Use this when done using the allocated register, it clears its "Needed" status.
	// The register that was written to, should be cleared before other registers are cleared.
	// This is to guarantee proper merging between registers... When a written-to reg is cleared,
	// it invalidates other cached registers of the same VF reg, and merges partial-vector
	// writes into them.
	void clearNeeded(const xmm& reg)
	{
		if ((reg.Id < 0) || (reg.Id >= xmmTotal)) // Sometimes xmmPQ hits this
			return;

		microMapXMM& clear = xmmMap[reg.Id];
		clear.isNeeded = false;
		if (clear.xyzw) // Reg was modified
		{
			if (clear.VFreg > 0)
			{
				int mergeRegs = 0;
				if (clear.xyzw < 0xf) // Try to merge partial writes
					mergeRegs = 1;
				for (int i = 0; i < xmmTotal; i++) // Invalidate any other read-only regs of same vfReg
				{
					if (i == reg.Id)
						continue;
					microMapXMM& mapI = xmmMap[i];
					if (mapI.VFreg == clear.VFreg)
					{
						if (mapI.xyzw && mapI.xyzw < 0xf)
						{
							DevCon.Error("microVU Error: clearNeeded() [%d]", mapI.VFreg);
						}
						if (mergeRegs == 1)
						{
							mVUmergeRegs(xmm(i), reg, clear.xyzw, true);
							mapI.xyzw  = 0xf;
							mapI.count = counter;
							mergeRegs  = 2;
						}
						else
							clearReg(i); // Clears when mergeRegs is 0 or 2
					}
				}
				if (mergeRegs == 2) // Clear Current Reg if Merged
					clearReg(reg);
				else if (mergeRegs == 1) // Write Back Partial Writes if couldn't merge
					writeBackReg(reg);
			}
			else
				clearReg(reg); // If Reg was temp or vf0, then invalidate itself
		}
	}

	// vfLoadReg  = VF reg to be loaded to the xmm register
	// vfWriteReg = VF reg that the returned xmm register will be considered as
	// xyzw       = XYZW vectors that will be modified (and loaded)
	// cloneWrite = When loading a reg that will be written to, it copies it to its own xmm reg instead of overwriting the cached one...
	// Notes:
	// To load a temp reg use the default param values, vfLoadReg = -1 and vfWriteReg = -1.
	// To load a full reg which won't be modified and you want cached, specify vfLoadReg >= 0 and vfWriteReg = -1
	// To load a reg which you don't want written back or cached, specify vfLoadReg >= 0 and vfWriteReg = 0
	const xmm& allocReg(int vfLoadReg = -1, int vfWriteReg = -1, int xyzw = 0, bool cloneWrite = true)
	{
		counter++;
		if (vfLoadReg >= 0) // Search For Cached Regs
		{
			for (int i = 0; i < xmmTotal; i++)
			{
				const xmm& xmmI = xmm::GetInstance(i);
				microMapXMM& mapI = xmmMap[i];
				if ((mapI.VFreg == vfLoadReg)
				 && (!mapI.xyzw                           // Reg Was Not Modified
				  || (mapI.VFreg && (mapI.xyzw == 0xf)))) // Reg Had All Vectors Modified and != VF0
				{
					int z = i;
					if (vfWriteReg >= 0) // Reg will be modified
					{
						if (cloneWrite) // Clone Reg so as not to use the same Cached Reg
						{
							z = findFreeReg(vfWriteReg);
							const xmm& xmmZ = xmm::GetInstance(z);
							writeBackReg(xmmZ);

							if (xyzw == 4)
								mVUshufD(xmmZ, xmmI, 1);
							else if (xyzw == 2)
								mVUshufD(xmmZ, xmmI, 2);
							else if (xyzw == 1)
								mVUshufD(xmmZ, xmmI, 3);
							else if (z != i)
								mVUmovReg(xmmZ, xmmI);

							mapI.count = counter; // Reg i was used, so update counter
						}
						else // Don't clone reg, but shuffle to adjust for SS ops
						{
							if ((vfLoadReg != vfWriteReg) || (xyzw != 0xf))
								writeBackReg(xmmI);

							if (xyzw == 4)
								mVUshufD(xmmI, xmmI, 1);
							else if (xyzw == 2)
								mVUshufD(xmmI, xmmI, 2);
							else if (xyzw == 1)
								mVUshufD(xmmI, xmmI, 3);
						}
						xmmMap[z].VFreg = vfWriteReg;
						xmmMap[z].xyzw = xyzw;
						xmmMap[z].isZero = (vfLoadReg == 0);
					}
					xmmMap[z].count = counter;
					xmmMap[z].isNeeded = true;

					return xmm::GetInstance(z);
				}
			}
		}
		int x = findFreeReg((vfWriteReg >= 0) ? vfWriteReg : vfLoadReg);
		const xmm& xmmX = xmm::GetInstance(x);
		writeBackReg(xmmX);

		if (vfWriteReg >= 0) // Reg Will Be Modified (allow partial reg loading)
		{
			if ((vfLoadReg == 0) && !(xyzw & 1))
				armAsm->Movi(xmmX.V2D(), 0);
			else if (vfLoadReg == 33)
				loadIreg(xmmX, xyzw);
			else if (vfLoadReg >= 0)
				mVUloadReg(xmmX, VFaddr(vfLoadReg), xyzw);

			xmmMap[x].VFreg = vfWriteReg;
			xmmMap[x].xyzw  = xyzw;
		}
		else // Reg Will Not Be Modified (always load full reg for caching)
		{
			if (vfLoadReg == 33)
				loadIreg(xmmX, 0xf);
			else if (vfLoadReg >= 0)
				armAsm->Ldr(xmmX.Q(), VFaddr(vfLoadReg).mem());

			xmmMap[x].VFreg = vfLoadReg;
			xmmMap[x].xyzw  = 0;
		}
		xmmMap[x].isZero = (vfLoadReg == 0);
		xmmMap[x].count    = counter;
		xmmMap[x].isNeeded = true;
		return xmmX;
	}

	void clearGPR(const x32& reg) { clearGPR(reg.GetId()); }

	void clearGPR(int regId)
	{
		microMapGPR& clear = gprMap[regId];
		clear.VIreg = -1;
		clear.count = 0;
		clear.isNeeded = 0;
		clear.dirty = false;
		clear.isZeroExtended = false;
	}

	void writeBackReg(const x32& reg, bool clearDirty)
	{
		microMapGPR& mapX = gprMap[reg.GetId()];
		pxAssert(mapX.usable || !mapX.dirty);
		if (mapX.dirty)
		{
			pxAssert(mapX.VIreg > 0);
			if (mapX.VIreg < 16)
				armAsm->Strh(reg.W(), VIaddr(mapX.VIreg));
			if (clearDirty)
				mapX.dirty = false;
		}
	}

	void clearNeeded(const x32& reg)
	{
		pxAssert(reg.GetId() < gprTotal);
		microMapGPR& clear = gprMap[reg.GetId()];
		clear.isNeeded = false;
	}

	void unbindAnyVIAllocations(int reg, bool& backup)
	{
		for (int i = 0; i < gprTotal; i++)
		{
			microMapGPR& mapI = gprMap[i];
			if (mapI.VIreg == reg)
			{
				if (backup)
				{
					writeVIBackup(x32(i));
					backup = false;
				}

				// if it's needed, we just unbind the allocation and preserve it, otherwise clear
				if (mapI.isNeeded)
				{
					MVURALOG("  unbind %d to %d for write\n", i, reg);
					mapI.VIreg = -1;
					mapI.dirty = false;
					mapI.isZeroExtended = false;
				}
				else
				{
					MVURALOG("  clear %d to %d for write\n", i, reg);
					clearGPR(i);
				}

				// shouldn't be any others...
				for (int j = i + 1; j < gprTotal; j++)
				{
					pxAssert(gprMap[j].VIreg != reg);
				}

				break;
			}
		}
	}

	const x32& allocGPR(int viLoadReg = -1, int viWriteReg = -1, bool backup = false, bool zext_if_dirty = false)
	{
		// TODO: When load != write, we should check whether load is used later, and if so, copy it.
		const int this_counter = counter++;
		if (viLoadReg == 0 || viWriteReg == 0)
		{
			// write zero register as temp and discard later
			if (viWriteReg == 0)
			{
				int x = findFreeGPR(-1);
				const x32& gprX = x32::GetInstance(x);
				writeBackReg(gprX, true);
				armAsm->Mov(gprX.W(), a64::wzr);
				gprMap[x].VIreg = -1;
				gprMap[x].dirty = false;
				gprMap[x].count = this_counter;
				gprMap[x].isNeeded = true;
				gprMap[x].isZeroExtended = true;
				MVURALOG("  alloc zero to scratch %d\n", x);
				return gprX;
			}
		}

		if (viLoadReg >= 0) // Search For Cached Regs
		{
			for (int i = 0; i < gprTotal; i++)
			{
				microMapGPR& mapI = gprMap[i];
				if (mapI.VIreg == viLoadReg)
				{
					// Do this first, there is a case where when loadReg != writeReg, the findFreeGPR can steal the loadReg
					gprMap[i].count = this_counter;

					if (viWriteReg >= 0) // Reg will be modified
					{
						if (viLoadReg != viWriteReg)
						{
							// kill any allocations of viWriteReg
							unbindAnyVIAllocations(viWriteReg, backup);

							// allocate a new register for writing to
							int x = findFreeGPR(viWriteReg);
							const x32& gprX = x32::GetInstance(x);

							writeBackReg(gprX, true);

							// writeReg not cached, needs backing up
							if (backup && gprMap[x].VIreg != viWriteReg)
							{
								armAsm->Ldrh(gprX.W(), VIaddr(viWriteReg));
								writeVIBackup(gprX);
								backup = false;
							}

							if (zext_if_dirty)
								armAsm->Uxth(gprX.W(), x32(i).W());
							else
								armAsm->Mov(gprX.W(), x32(i).W());
							gprMap[x].isZeroExtended = zext_if_dirty;
							MVURALOG("  clone write %d in %d to %d for %d\n", viLoadReg, i, x, viWriteReg);
							std::swap(x, i);
						}
						else
						{
							// writing to it, no longer zero extended
							gprMap[i].isZeroExtended = false;
						}

						gprMap[i].VIreg = viWriteReg;
						gprMap[i].dirty = true;
					}
					else if (zext_if_dirty && !gprMap[i].isZeroExtended)
					{
						armAsm->Uxth(x32(i).W(), x32(i).W());
						gprMap[i].isZeroExtended = true;
					}

					gprMap[i].isNeeded = true;

					if (backup)
						writeVIBackup(x32(i));

					MVURALOG("  returning cached in %d\n", i);
					return x32::GetInstance(i);
				}
			}
		}

		if (viWriteReg >= 0) // Writing a new value, make sure this register isn't cached already
			unbindAnyVIAllocations(viWriteReg, backup);

		int x = findFreeGPR(viLoadReg);
		const x32& gprX = x32::GetInstance(x);
		writeBackReg(gprX, true);

		// Special case: we need to back up the destination register, but it might not have already
		// been cached. If so, we need to load the old value from state and back it up. Otherwise,
		// it's going to get lost when we eventually write this register back.
		if (backup && viLoadReg >= 0 && viWriteReg > 0 && viLoadReg != viWriteReg)
		{
			armAsm->Ldrh(gprX.W(), VIaddr(viWriteReg));
			writeVIBackup(gprX);
			backup = false;
		}

		if (viLoadReg > 0)
			armAsm->Ldrh(gprX.W(), VIaddr(viLoadReg));
		else if (viLoadReg == 0)
			armAsm->Mov(gprX.W(), a64::wzr);

		gprMap[x].VIreg = viLoadReg;
		gprMap[x].isZeroExtended = true;
		if (viWriteReg >= 0)
		{
			gprMap[x].VIreg = viWriteReg;
			gprMap[x].dirty = true;
			gprMap[x].isZeroExtended = false;

			if (backup)
			{
				if (viLoadReg < 0 && viWriteReg > 0)
					armAsm->Ldrh(gprX.W(), VIaddr(viWriteReg));

				writeVIBackup(gprX);
			}
		}

		gprMap[x].count = this_counter;
		gprMap[x].isNeeded = true;

		MVURALOG("  returning new %d\n", x);
		return gprX;
	}

	void moveVIToGPR(const x32& reg, int vi, bool signext = false)
	{
		pxAssert(vi >= 0);
		if (vi == 0)
		{
			armAsm->Mov(reg.W(), a64::wzr);
			return;
		}

		// TODO: Check liveness/usedness before allocating.
		// TODO: Check whether zero-extend is needed everywhere heae. Loadstores are.
		const x32& srcreg = allocGPR(vi);
		if (signext)
			armAsm->Sxth(reg.W(), srcreg.W());
		else
			armAsm->Uxth(reg.W(), srcreg.W());
		clearNeeded(srcreg);
	}
};
