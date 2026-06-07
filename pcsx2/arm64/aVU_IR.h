// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU recompiler — host register allocator (Phase 7, task 7.2b).
//
// This is the ARM64 counterpart to the `microRegAlloc` class in
// pcsx2/x86/microVU_IR.h (lines 226-1139). Per the Phase 7 strategy
// (PROGRESS.md / [[arm64-microvu-architecture]]) microVU is *parallel-cloned*
// into pcsx2/arm64/ rather than #included from x86/.
//
// What changed vs the x86 original:
//   * VF registers live in NEON v-regs (q view, 128-bit) instead of x86 xmm.
//   * VI registers live in ARM64 w-regs instead of x86 GPRs.
//   * The x86 emitter (absolute-address ptr[...] loads/stores) becomes VIXL
//     base+offset addressing against a designated VU-state pointer register
//     (RVUSTATE = x19 = &vuRegs[index]).
//   * The COP2 / macro-mode path (`regAllocCOP2`, `_allocVFtoXMMreg`,
//     `_allocX86reg`, the pxmmregs/x86regs interaction, updateCOP2AllocState,
//     flushPartialForCOP2, clearRegCOP2/clearGPRCOP2) is DROPPED. That path is
//     the EE-side VU0-macro allocator which is Phase 7.9; on ARM64 the macro
//     ops currently run through the Phase 5.3 inline-interp fallback. Removing
//     it makes the bookkeeping a faithful but much smaller mirror of the x86
//     allocator's "normal" (microVU-thread) path.
//
// The emission helpers (mVUloadReg/mVUsaveReg/mVUmergeRegs/loadIreg) and the
// register-map constants below are PROVISIONAL: they are exercised for real
// only once the dispatcher (7.2d) and opcode emission (7.5) land and can pin
// the final ABI. They are written to mirror the x86 lane semantics exactly.

#include "aVU.h"

#include "arm64/AsmHelpers.h"

#include <array>

namespace a64 = vixl::aarch64;

//------------------------------------------------------------------
// ARM64 microVU host register map (provisional — see header note)
//------------------------------------------------------------------
// NEON (VF):
//   v0 .. v(mVU_VF_TOTAL-1)  — the VF allocation pool (slot i == host v_i)
//   v24                      — PQ latency reg (Q/P instances), reserved
//   v29/v30/v31              — shared VIXL scratch (AsmHelpers RQSCRATCH*), reserved
//
// GPR (VI + scratch):
//   x19  RVUSTATE  — &vuRegs[index]; base for all VF/VI/ACC/I memory access
//   w9   gprT1     — emit scratch temp
//   w10  gprT2     — emit scratch temp
//   x17  RVUADDR   — transient address-calc scratch (for NEON lane stores)
//   w23..w26 gprF0..gprF3 — the 4 Status-flag instances kept live across a block
//   the remaining usable w-regs form the VI allocation pool.
static constexpr int mVU_VF_TOTAL = 24; // v0..v23 allocatable for VF
static constexpr int mVU_GPR_TOTAL = 27; // w0..w26 tracked by the allocator

static constexpr int mVU_T1 = 9;
static constexpr int mVU_T2 = 10;
static constexpr int mVU_F0 = 23, mVU_F1 = 24, mVU_F2 = 25, mVU_F3 = 26;

#define RVUSTATE a64::x19 // base ptr = &vuRegs[index]
#define RVUADDR a64::x17  // transient address scratch

// Named host registers for the emit layer (x86: microVU_Misc.h #defines gprT1..
// /gprF0..). VIXL predefined W-register objects matching the index constants
// above; getFlagReg() (aVU_Alloc.inl) indexes gprF0..gprF3.
#define gprT1 a64::w9  // emit scratch temp
#define gprT2 a64::w10 // emit scratch temp
#define gprF0 a64::w23 // Status flag instance 0
#define gprF1 a64::w24 // Status flag instance 1
#define gprF2 a64::w25 // Status flag instance 2
#define gprF3 a64::w26 // Status flag instance 3

// PQ latency NEON register.
static const a64::VRegister mVU_xmmPQ = a64::VRegister(24, 128);

// Sentinel "no register" for optional NEON params (x86: xEmptyReg). VIXL's
// default-constructed VRegister reports IsNone(); ported callers test reg.IsNone()
// where the x86 original tested reg.IsEmpty().
static const a64::VRegister xEmptyReg;

// Emit-layer NEON temp registers (x86: microVU_Misc.h xmmT1=xmm0/xmmT2=xmm1, the
// regAlloc temps). On ARM64 the VF allocation pool is v0-v23, so these instead map
// to the shared NEON scratch (q30/q31). Every flag/branch/endProgram path that uses
// them does so *after* a regAlloc flushAll(), so they never overlap a live VF reg.
#define xmmT1 RQSCRATCH  // q30
#define xmmT2 RQSCRATCH2 // q31

// x86 xSHUF.PS(dst, src, imm) — arbitrary 4-lane (32-bit) permute: out lane i takes
// src lane ((imm >> (2*i)) & 3). NEON has no immediate 4-lane shuffle, so copy src
// into a scratch (q29) and Ins each selected lane. Safe when dst == src.
static inline void mVUshufflePS(const a64::VRegister& dst, const a64::VRegister& src, u8 imm)
{
	armAsm->Mov(RQSCRATCH3.V16B(), src.V16B());
	for (int i = 0; i < 4; i++)
		armAsm->Ins(dst.V4S(), i, RQSCRATCH3.V4S(), (imm >> (2 * i)) & 3);
}

//------------------------------------------------------------------
// Byte offsets into VURegs (addressed from RVUSTATE = &vuRegs[index])
//------------------------------------------------------------------
static constexpr u32 mVUoffVF(int n) { return static_cast<u32>(offsetof(VURegs, VF) + n * sizeof(VECTOR)); }
static constexpr u32 mVUoffVI(int n) { return static_cast<u32>(offsetof(VURegs, VI) + n * sizeof(REG_VI)); }
static constexpr u32 mVUoffACC = static_cast<u32>(offsetof(VURegs, ACC));
static constexpr u32 mVUoffI = static_cast<u32>(offsetof(VURegs, VI) + REG_I * sizeof(REG_VI));

//------------------------------------------------------------------
// NEON reg load/save/merge helpers (mirror microVU_Misc.inl lane semantics)
//------------------------------------------------------------------
// xyzw is the microVU 4-bit mask: bit3(0x8)=X=lane0, bit2(0x4)=Y=lane1,
// bit1(0x2)=Z=lane2, bit0(0x1)=W=lane3. NEON V4S lane order matches xmm
// (lane0 = lowest 32 bits = X), so the mapping is one-to-one.

static inline bool mVU_isSS(int xyzw) // exactly one subvector selected
{
	return (xyzw == 8) || (xyzw == 4) || (xyzw == 2) || (xyzw == 1);
}

// Broadcast a single lane of srcreg across all 4 lanes of dstreg (x86:
// xPSHUF.D with a constant splat pattern — 0x00=XXXX, 0x55=YYYY, 0xaa=ZZZZ,
// 0xff=WWWW). The unpack index 0..3 selects lane X/Y/Z/W directly, so NEON Dup
// from that lane is a one-to-one translation.
static inline void mVUunpack_xyzw(const a64::VRegister& dstreg, const a64::VRegister& srcreg, int xyzw)
{
	armAsm->Dup(dstreg.V4S(), srcreg.V4S(), xyzw & 3);
}

// Load VF/ACC reg bytes at `off` (from RVUSTATE) into `reg`. Single-subvector
// masks load that one 32-bit lane into lane0 (zero-extending the rest, like
// xMOVSSZX); anything else loads the full 128 bits.
static inline void mVUloadReg(const a64::VRegister& reg, u32 off, int xyzw)
{
	switch (xyzw)
	{
		case 8:  armAsm->Ldr(reg.S(), a64::MemOperand(RVUSTATE, off + 0));  break; // X
		case 4:  armAsm->Ldr(reg.S(), a64::MemOperand(RVUSTATE, off + 4));  break; // Y
		case 2:  armAsm->Ldr(reg.S(), a64::MemOperand(RVUSTATE, off + 8));  break; // Z
		case 1:  armAsm->Ldr(reg.S(), a64::MemOperand(RVUSTATE, off + 12)); break; // W
		default: armAsm->Ldr(reg.Q(), a64::MemOperand(RVUSTATE, off));      break;
	}
}

// Write back the selected lanes of `reg` to VF/ACC bytes at `off`.
// modXYZW: when true the value of a single-subvector (Y/Z/W) result lives in
// lane0 (microVU produces SS results pre-shuffled into lane0); when false the
// lanes are in their natural positions. Multi-lane masks are always natural.
static inline void mVUsaveReg(const a64::VRegister& reg, u32 off, int xyzw, bool modXYZW)
{
	if (xyzw == 0xf)
	{
		armAsm->Str(reg.Q(), a64::MemOperand(RVUSTATE, off));
		return;
	}

	// Single subvector with the value already shuffled into lane0.
	if (modXYZW && (xyzw == 4 || xyzw == 2 || xyzw == 1))
	{
		const u32 coff = (xyzw == 4) ? 4u : (xyzw == 2) ? 8u : 12u;
		armAsm->Str(reg.S(), a64::MemOperand(RVUSTATE, off + coff));
		return;
	}

	// Natural-position store of each selected lane. X (lane0) uses a scalar
	// store with an immediate offset; Y/Z/W need an explicit address because
	// NEON single-lane stores can't carry an immediate offset.
	if (xyzw & 8)
		armAsm->Str(reg.S(), a64::MemOperand(RVUSTATE, off + 0));
	if (xyzw & 4)
	{
		armAsm->Add(RVUADDR, RVUSTATE, off + 4);
		armAsm->St1(reg.V4S(), 1, a64::MemOperand(RVUADDR));
	}
	if (xyzw & 2)
	{
		armAsm->Add(RVUADDR, RVUSTATE, off + 8);
		armAsm->St1(reg.V4S(), 2, a64::MemOperand(RVUADDR));
	}
	if (xyzw & 1)
	{
		armAsm->Add(RVUADDR, RVUSTATE, off + 12);
		armAsm->St1(reg.V4S(), 3, a64::MemOperand(RVUADDR));
	}
}

// Merge the selected lanes of `src` into `dest`. Same modXYZW convention.
static inline void mVUmergeRegs(const a64::VRegister& dest, const a64::VRegister& src, int xyzw, bool modXYZW)
{
	xyzw &= 0xf;
	if ((dest.GetCode() == src.GetCode()) || (xyzw == 0))
		return;

	if (xyzw == 0xf)
	{
		armAsm->Mov(dest.V16B(), src.V16B());
		return;
	}

	if (modXYZW && (xyzw == 4 || xyzw == 2 || xyzw == 1))
	{
		const int lane = (xyzw == 4) ? 1 : (xyzw == 2) ? 2 : 3;
		armAsm->Ins(dest.V4S(), lane, src.V4S(), 0);
		return;
	}

	if (xyzw & 8) armAsm->Ins(dest.V4S(), 0, src.V4S(), 0);
	if (xyzw & 4) armAsm->Ins(dest.V4S(), 1, src.V4S(), 1);
	if (xyzw & 2) armAsm->Ins(dest.V4S(), 2, src.V4S(), 2);
	if (xyzw & 1) armAsm->Ins(dest.V4S(), 3, src.V4S(), 3);
}

//------------------------------------------------------------------
// Reg Alloc maps (mirror microVU_IR.h microMapXMM/microMapGPR)
//------------------------------------------------------------------

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

class microRegAlloc
{
protected:
	static const int xmmTotal = mVU_VF_TOTAL;  // PQ register is reserved (lives at v24)
	static const int gprTotal = mVU_GPR_TOTAL;

	std::array<microMapXMM, xmmTotal> xmmMap;
	std::array<microMapGPR, gprTotal> gprMap;

	int counter; // Current allocation count
	int index;   // VU0 or VU1

	// Helper functions to get VU regs
	VURegs& regs() const { return ::vuRegs[index]; }
	__fi REG_VI& getVI(uint reg) const { return regs().VI[reg]; }
	__fi VECTOR& getVF(uint reg) const { return regs().VF[reg]; }

	// Host register objects by allocator index.
	static a64::VRegister vfReg(int i) { return a64::VRegister(i, 128); }
	static const a64::Register& gprW(int i) { return armWRegister(i); }

	__ri void loadIreg(const a64::VRegister& reg, int xyzw)
	{
		for (int i = 0; i < gprTotal; i++)
		{
			if (gprMap[i].VIreg == REG_I)
			{
				armAsm->Fmov(reg.S(), gprW(i)); // GPR32 -> lane0, zero rest (xMOVDZX)
				if (!mVU_isSS(xyzw))
					armAsm->Dup(reg.V4S(), reg.V4S(), 0);
				return;
			}
		}

		armAsm->Ldr(reg.S(), a64::MemOperand(RVUSTATE, mVUoffI));
		if (!mVU_isSS(xyzw))
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

	void writeVIBackup(const a64::Register& reg)
	{
		microVU& mVU = index ? microVU1 : microVU0;
		armMoveAddressToReg(RVUADDR, &mVU.VIbackup);
		armAsm->Str(reg.W(), a64::MemOperand(RVUADDR));
	}

public:
	microRegAlloc(int _index)
	{
		index = _index;

		// mark gpr registers as usable
		gprMap.fill({0, 0, false, false, false, false});
		for (int i = 0; i < gprTotal; i++)
		{
			if (i == mVU_T1 || i == mVU_T2 ||
				i == mVU_F0 || i == mVU_F1 || i == mVU_F2 || i == mVU_F3 ||
				i == 16 || i == 17 || i == 18 || i == 19)
			{
				continue; // scratch / reserved / RVUSTATE
			}

			gprMap[i].usable = true;
		}

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
			if (!gprMap[i].usable && (gprMap[i].VIreg < 0))
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
			writeBackReg(vfReg(i));
			if (clearState)
				clearReg(i);
		}

		for (int i = 0; i < gprTotal; i++)
		{
			writeBackReg(gprW(i), true);
			if (clearState)
				clearGPR(i);
		}
	}

	void flushCallerSavedRegisters(bool clearNeeded = false)
	{
		for (int i = 0; i < xmmTotal; i++)
		{
			if (!vfIsCallerSaved(i))
				continue;

			writeBackReg(vfReg(i));
			if (clearNeeded || !xmmMap[i].isNeeded)
				clearReg(i);
		}

		for (int i = 0; i < gprTotal; i++)
		{
			if (armIsCalleeSavedRegister(i))
				continue;

			writeBackReg(gprW(i), true);
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
					armAsm->Str(vfReg(i).S(), a64::MemOperand(RVUSTATE, mVUoffI));
				else if (mapX.VFreg == 32)
					mVUsaveReg(vfReg(i), mVUoffACC, mapX.xyzw, true);
				else
					mVUsaveReg(vfReg(i), mVUoffVF(mapX.VFreg), mapX.xyzw, true);
			}
		}

		for (int i = 0; i < gprTotal; i++)
			writeBackReg(gprW(i), false);
	}

	bool checkVFClamp(int regId)
	{
		if (regId != static_cast<int>(mVU_xmmPQ.GetCode()) && ((xmmMap[regId].VFreg == 33 && !EmuConfig.Gamefixes.IbitHack) || xmmMap[regId].isZero))
			return false;
		else
			return true;
	}

	bool checkCachedReg(int regId)
	{
		if (regId < xmmTotal)
			return xmmMap[regId].VFreg >= 0;
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

	void clearReg(const a64::VRegister& reg) { clearReg(reg.GetCode()); }
	void clearReg(int regId)
	{
		microMapXMM& clear = xmmMap[regId];
		clear = {-1, 0, 0, false, false};
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
	void writeBackReg(const a64::VRegister& reg, bool invalidateRegs = true)
	{
		microMapXMM& mapX = xmmMap[reg.GetCode()];

		if ((mapX.VFreg > 0) && mapX.xyzw) // Reg was modified and not Temp or vf0
		{
			extern bool g_mvuDiffActive;
			if (g_mvuDiffActive && index == 1 && (mapX.VFreg == 17 || mapX.VFreg == 24))
			{
				microVU& mVU = microVU1;
				DevCon.Error("WB VF%02d xyzw=%x v%d @pc=%04x", mapX.VFreg, mapX.xyzw, reg.GetCode(), (mVU.prog.IRinfo.curPC / 2) * 8);
			}
			if (mapX.VFreg == 33)
				armAsm->Str(reg.S(), a64::MemOperand(RVUSTATE, mVUoffI));
			else if (mapX.VFreg == 32)
				mVUsaveReg(reg, mVUoffACC, mapX.xyzw, true);
			else
				mVUsaveReg(reg, mVUoffVF(mapX.VFreg), mapX.xyzw, true);

			if (invalidateRegs)
			{
				for (int i = 0; i < xmmTotal; i++)
				{
					microMapXMM& mapI = xmmMap[i];

					if ((i == static_cast<int>(reg.GetCode())) || mapI.isNeeded)
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
	void clearNeeded(const a64::VRegister& reg)
	{
		const int rid = reg.GetCode();
		if ((rid < 0) || (rid >= xmmTotal)) // Sometimes mVU_xmmPQ hits this
			return;

		microMapXMM& clear = xmmMap[rid];
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
					if (i == rid)
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
							mVUmergeRegs(vfReg(i), reg, clear.xyzw, true);
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

	// vfLoadReg  = VF reg to be loaded to the NEON register
	// vfWriteReg = VF reg that the returned NEON register will be considered as
	// xyzw       = XYZW vectors that will be modified (and loaded)
	// cloneWrite = When loading a reg that will be written to, it copies it to its own NEON reg instead of overwriting the cached one...
	// Notes:
	// To load a temp reg use the default param values, vfLoadReg = -1 and vfWriteReg = -1.
	// To load a full reg which won't be modified and you want cached, specify vfLoadReg >= 0 and vfWriteReg = -1
	// To load a reg which you don't want written back or cached, specify vfLoadReg >= 0 and vfWriteReg = 0
	const a64::VRegister allocReg(int vfLoadReg = -1, int vfWriteReg = -1, int xyzw = 0, bool cloneWrite = true)
	{
		counter++;
		{
			extern bool g_mvuDiffActive;
			if (g_mvuDiffActive && index == 1 &&
				(vfLoadReg == 17 || vfLoadReg == 24 || vfWriteReg == 17 || vfWriteReg == 24))
			{
				microVU& mVU = microVU1;
				Console.WriteLn("ALLOC load=%d write=%d xyzw=%x clone=%d @pc=%04x", vfLoadReg, vfWriteReg, xyzw, (int)cloneWrite, (mVU.prog.IRinfo.curPC / 2) * 8);
			}
		}
		if (vfLoadReg >= 0) // Search For Cached Regs
		{
			for (int i = 0; i < xmmTotal; i++)
			{
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
							const a64::VRegister xmmZ = vfReg(z);
							writeBackReg(xmmZ);

							if (xyzw == 4)
								armAsm->Dup(xmmZ.V4S(), vfReg(i).V4S(), 1); // Y -> lane0
							else if (xyzw == 2)
								armAsm->Dup(xmmZ.V4S(), vfReg(i).V4S(), 2); // Z -> lane0
							else if (xyzw == 1)
								armAsm->Dup(xmmZ.V4S(), vfReg(i).V4S(), 3); // W -> lane0
							else if (z != i)
								armAsm->Mov(xmmZ.V16B(), vfReg(i).V16B());

							mapI.count = counter; // Reg i was used, so update counter
						}
						else // Don't clone reg, but shuffle to adjust for SS ops
						{
							if ((vfLoadReg != vfWriteReg) || (xyzw != 0xf))
								writeBackReg(vfReg(i));

							if (xyzw == 4)
								armAsm->Dup(vfReg(i).V4S(), vfReg(i).V4S(), 1);
							else if (xyzw == 2)
								armAsm->Dup(vfReg(i).V4S(), vfReg(i).V4S(), 2);
							else if (xyzw == 1)
								armAsm->Dup(vfReg(i).V4S(), vfReg(i).V4S(), 3);
						}
						xmmMap[z].VFreg = vfWriteReg;
						xmmMap[z].xyzw = xyzw;
						xmmMap[z].isZero = (vfLoadReg == 0);
					}
					xmmMap[z].count = counter;
					xmmMap[z].isNeeded = true;
					return vfReg(z);
				}
			}
		}
		int x = findFreeReg((vfWriteReg >= 0) ? vfWriteReg : vfLoadReg);
		const a64::VRegister xmmX = vfReg(x);
		writeBackReg(xmmX);

		if (vfWriteReg >= 0) // Reg Will Be Modified (allow partial reg loading)
		{
			if ((vfLoadReg == 0) && !(xyzw & 1))
				armAsm->Eor(xmmX.V16B(), xmmX.V16B(), xmmX.V16B());
			else if (vfLoadReg == 33)
				loadIreg(xmmX, xyzw);
			else if (vfLoadReg == 32)
				mVUloadReg(xmmX, mVUoffACC, xyzw);
			else if (vfLoadReg >= 0)
				mVUloadReg(xmmX, mVUoffVF(vfLoadReg), xyzw);

			xmmMap[x].VFreg = vfWriteReg;
			xmmMap[x].xyzw  = xyzw;
		}
		else // Reg Will Not Be Modified (always load full reg for caching)
		{
			if (vfLoadReg == 33)
				loadIreg(xmmX, 0xf);
			else if (vfLoadReg == 32)
				armAsm->Ldr(xmmX.Q(), a64::MemOperand(RVUSTATE, mVUoffACC));
			else if (vfLoadReg >= 0)
				armAsm->Ldr(xmmX.Q(), a64::MemOperand(RVUSTATE, mVUoffVF(vfLoadReg)));

			xmmMap[x].VFreg = vfLoadReg;
			xmmMap[x].xyzw  = 0;
		}
		xmmMap[x].isZero = (vfLoadReg == 0);
		xmmMap[x].count    = counter;
		xmmMap[x].isNeeded = true;
		return xmmX;
	}

	void clearGPR(const a64::Register& reg) { clearGPR(reg.GetCode()); }

	void clearGPR(int regId)
	{
		microMapGPR& clear = gprMap[regId];
		clear.VIreg = -1;
		clear.count = 0;
		clear.isNeeded = 0;
		clear.dirty = false;
		clear.isZeroExtended = false;
	}

	void writeBackReg(const a64::Register& reg, bool clearDirty)
	{
		microMapGPR& mapX = gprMap[reg.GetCode()];
		pxAssert(mapX.usable || !mapX.dirty);
		if (mapX.dirty)
		{
			pxAssert(mapX.VIreg > 0);
			if (mapX.VIreg < 16)
				armAsm->Strh(reg.W(), a64::MemOperand(RVUSTATE, mVUoffVI(mapX.VIreg)));
			if (clearDirty)
				mapX.dirty = false;
		}
	}

	void clearNeeded(const a64::Register& reg)
	{
		pxAssert(reg.GetCode() < gprTotal);
		microMapGPR& clear = gprMap[reg.GetCode()];
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
					writeVIBackup(gprW(i));
					backup = false;
				}

				// if it's needed, we just unbind the allocation and preserve it, otherwise clear
				if (mapI.isNeeded)
				{
					mapI.VIreg = -1;
					mapI.dirty = false;
					mapI.isZeroExtended = false;
				}
				else
				{
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

	const a64::Register allocGPR(int viLoadReg = -1, int viWriteReg = -1, bool backup = false, bool zext_if_dirty = false)
	{
		// TODO: When load != write, we should check whether load is used later, and if so, copy it.
		const int this_counter = counter++;
		if (viLoadReg == 0 || viWriteReg == 0)
		{
			// write zero register as temp and discard later
			if (viWriteReg == 0)
			{
				int x = findFreeGPR(-1);
				const a64::Register gprX = gprW(x);
				writeBackReg(gprX, true);
				armAsm->Mov(gprX.W(), a64::wzr);
				gprMap[x].VIreg = -1;
				gprMap[x].dirty = false;
				gprMap[x].count = this_counter;
				gprMap[x].isNeeded = true;
				gprMap[x].isZeroExtended = true;
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
							const a64::Register gprX = gprW(x);

							writeBackReg(gprX, true);

							// writeReg not cached, needs backing up
							if (backup && gprMap[x].VIreg != viWriteReg)
							{
								armAsm->Ldrh(gprX.W(), a64::MemOperand(RVUSTATE, mVUoffVI(viWriteReg)));
								writeVIBackup(gprX);
								backup = false;
							}

							if (zext_if_dirty)
								armAsm->Uxth(gprX.W(), gprW(i).W());
							else
								armAsm->Mov(gprX.W(), gprW(i).W());
							gprMap[x].isZeroExtended = zext_if_dirty;
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
						armAsm->Uxth(gprW(i).W(), gprW(i).W());
						gprMap[i].isZeroExtended = true;
					}

					gprMap[i].isNeeded = true;

					if (backup)
						writeVIBackup(gprW(i));

					return gprW(i);
				}
			}
		}

		if (viWriteReg >= 0) // Writing a new value, make sure this register isn't cached already
			unbindAnyVIAllocations(viWriteReg, backup);

		int x = findFreeGPR(viLoadReg);
		const a64::Register gprX = gprW(x);
		writeBackReg(gprX, true);

		// Special case: we need to back up the destination register, but it might not have already
		// been cached. If so, we need to load the old value from state and back it up. Otherwise,
		// it's going to get lost when we eventually write this register back.
		if (backup && viLoadReg >= 0 && viWriteReg > 0 && viLoadReg != viWriteReg)
		{
			armAsm->Ldrh(gprX.W(), a64::MemOperand(RVUSTATE, mVUoffVI(viWriteReg)));
			writeVIBackup(gprX);
			backup = false;
		}

		if (viLoadReg > 0)
			armAsm->Ldrh(gprX.W(), a64::MemOperand(RVUSTATE, mVUoffVI(viLoadReg)));
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
					armAsm->Ldrh(gprX.W(), a64::MemOperand(RVUSTATE, mVUoffVI(viWriteReg)));

				writeVIBackup(gprX);
			}
		}

		gprMap[x].count = this_counter;
		gprMap[x].isNeeded = true;

		return gprX;
	}

	void moveVIToGPR(const a64::Register& reg, int vi, bool signext = false)
	{
		pxAssert(vi >= 0);
		if (vi == 0)
		{
			armAsm->Mov(reg.W(), a64::wzr);
			return;
		}

		// TODO: Check liveness/usedness before allocating.
		const a64::Register srcreg = allocGPR(vi);
		if (signext)
			armAsm->Sxth(reg.W(), srcreg.W());
		else
			armAsm->Uxth(reg.W(), srcreg.W());
		clearNeeded(srcreg);
	}

private:
	// Every VF pool register must be treated as caller-saved: AAPCS64 only
	// preserves the *low 64 bits* of v8-v15 across a C call, but VF regs are
	// 128-bit, so even those cannot survive a call intact. This matches the x86
	// SysV behaviour (no xmm is callee-saved there either) — flushCallerSaved
	// writes back every cached VF reg before a call.
	static bool vfIsCallerSaved([[maybe_unused]] int i) { return true; }
};
