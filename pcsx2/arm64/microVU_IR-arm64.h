// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "microVU_Divtrace.h"

//#define MVURALOG(...) fprintf(stderr, __VA_ARGS__)
#define MVURALOG(...)

//------------------------------------------------------------------
// ARM64 NEON/GPR Register Maps
//------------------------------------------------------------------

struct microMapNEON
{
	int  VFreg;    // VF Reg Number Stored (-1=Temp, 0=VF0, 1-31=VF, 32=ACC, 33=I reg)
	int  xyzw;     // xyzw to write back (0 = clean/fully cached, nonzero = dirty partial)
	int  count;    // LRU counter
	bool isNeeded; // Locked for current instruction
	bool isZero;   // Loaded from VF0, no clamping needed
};

struct microMapGPR
{
	int  VIreg;     // VI reg number (-1=unused, 0-15=VI regs)
	int  count;     // LRU counter
	bool isNeeded;  // Locked for current instruction
	bool dirty;     // Modified, needs writeback
	bool isZeroExtended; // 16-bit value zero-extended to 32-bit
	bool usable;    // Available for allocation (not reserved)
};

//------------------------------------------------------------------
// ARM64 Register Pools
//------------------------------------------------------------------

// NEON allocatable: Q0-Q27 (28 registers). Q28=PQ, Q29-Q31=scratch.
static const int neonAllocTotal = 28;

// GPR allocatable for VI: x14-x15 (2) + x26-x28 (3) = 5.
// x0-x7=args/scratch, x8=RXSCRATCH, x9-x13=scratch, x16-x17=vixl,
// x18=platform, x19=gprVUState, x20-x23=gprF0-F3, x24=gprMVUFlag,
// x25=gprMVUglob, x29=fp, x30=lr, sp=stack.
static const int gprAllocCount = 32; // Total GPR IDs (unusable ones are marked in the map)

//------------------------------------------------------------------
// ARM64 microRegAlloc
//------------------------------------------------------------------

class microRegAlloc
{
protected:
	std::array<microMapNEON, neonAllocTotal> neonMap;
	std::array<microMapGPR, gprAllocCount>   gprMap;

	int counter;
	int index; // VU0 or VU1

	VURegs& regs() const { return ::vuRegs[index]; }

	// Load I register (immediate) into NEON reg
	__ri void loadIreg(const a64::VRegister& reg, int xyzw)
	{
		// If REG_I is cached in a VI GPR slot, transfer via GPR→NEON to
		// pick up any pending writes that haven't been flushed. Matches
		// x86 loadIreg.
		for (int i = 0; i < gprAllocCount; i++)
		{
			if (gprMap[i].usable && gprMap[i].VIreg == REG_I)
			{
				// MOV into lane 0, zero upper 96 bits (equivalent to
				// x86 xMOVDZX — 32-bit reg into xmm with zero-extension).
				armAsm->Fmov(a64::SRegister(reg.GetCode()), armWRegister(i));
				if (!_XYZWss(xyzw))
					armAsm->Dup(reg.V4S(), reg.V4S(), 0);
				return;
			}
		}

		armAsm->Ldr(a64::VRegister(reg.GetCode(), 32),
			mVUstateMem(offsetof(VURegs, VI) + REG_I * sizeof(REG_VI)));
		if (!_XYZWss(xyzw))
			armAsm->Dup(reg.V4S(), reg.V4S(), 0); // Broadcast to all lanes
	}

	// Find least-recently-used NEON reg (recursive, for eviction)
	int findFreeNeonRec(int startIdx)
	{
		for (int i = startIdx; i < neonAllocTotal; i++)
		{
			if (!neonMap[i].isNeeded)
			{
				int x = findFreeNeonRec(i + 1);
				if (x == -1)
					return i;
				return (neonMap[i].count < neonMap[x].count) ? i : x;
			}
		}
		return -1;
	}

	int findFreeNeon(int vfreg)
	{
		// Prefer unoccupied temp regs
		for (int i = 0; i < neonAllocTotal; i++)
		{
			if (!neonMap[i].isNeeded && neonMap[i].VFreg < 0)
				return i;
		}
		// Evict LRU
		int x = findFreeNeonRec(0);
		pxAssertMsg(x >= 0, "microVU NEON register allocation failure!");
		return x;
	}

	int findFreeGPRRec(int startIdx)
	{
		for (int i = startIdx; i < gprAllocCount; i++)
		{
			if (gprMap[i].usable && !gprMap[i].isNeeded)
			{
				int x = findFreeGPRRec(i + 1);
				if (x == -1)
					return i;
				return (gprMap[i].count < gprMap[x].count) ? i : x;
			}
		}
		return -1;
	}

	int findFreeGPR(int vireg)
	{
		for (int i = 0; i < gprAllocCount; i++)
		{
			if (gprMap[i].usable && !gprMap[i].isNeeded && gprMap[i].VIreg < 0)
				return i;
		}
		int x = findFreeGPRRec(0);
		pxAssertMsg(x >= 0, "microVU GPR register allocation failure!");
		return x;
	}

	// Write back a dirty NEON reg to VF memory.
	//
	// Matches x86 writeBackReg semantics:
	//   - Full-vector writes (xyzw == 0xF): cache is still valid after the
	//     store, mark clean (xyzw=0). Later reads reuse the reg.
	//   - Partial writes (xyzw in {0x1..0xE}): the register may be SS-shuffled
	//     (single-lane case from allocReg's SS path put Y/Z/W in lane 0), or
	//     have natural layout but stale lanes. Either way the register's
	//     contents no longer reliably match memory, so invalidate the cache
	//     entry entirely after the store. Subsequent reads will re-load from
	//     the fresh memory values.
	//
	// Pass modXYZW=true to mVUsaveReg so it writes lane 0 (the SS-shuffled
	// value) for single-lane partial writes, matching x86 writeBackReg.
	void writeBackNeon(int regIdx, bool clearEntry = false)
	{
		microMapNEON& entry = neonMap[regIdx];
		if (entry.VFreg > 0 && entry.xyzw != 0) // VFreg 0 is read-only
		{
			const a64::VRegister qreg = armQRegister(regIdx);
			const int64_t off = (entry.VFreg == 32) ? offsetof(VURegs, ACC) :
			                    (entry.VFreg == 33) ? offsetof(VURegs, VI) + REG_I * sizeof(REG_VI) :
			                                          offsetof(VURegs, VF) + entry.VFreg * sizeof(VECTOR);
			if (entry.xyzw == 0xF)
			{
				armAsm->Str(qreg, mVUstateMem(off));
				if (clearEntry)
					clearNeon(regIdx);
				else
					entry.xyzw = 0; // Cache still valid, memory matches.
				return;
			}
			// Partial write — must invalidate after store regardless of
			// clearEntry, otherwise later cached reads pick up a reg with
			// stale / shuffled lanes and skip the re-load.
			mVUsaveReg(qreg, gprVUState, off, entry.xyzw, true);
			clearNeon(regIdx);
			return;
		}
		if (clearEntry)
			clearNeon(regIdx);
	}

	// Write back a dirty GPR to VI memory
	void writeBackGPR(int regIdx, bool clearEntry = false)
	{
		microMapGPR& entry = gprMap[regIdx];
		if (entry.VIreg > 0 && entry.dirty)
		{
			// Store 16-bit value (VI regs are 16-bit in memory)
			const a64::Register wreg = armWRegister(regIdx);
			armAsm->Strh(wreg, mVUstateMem(offsetof(VURegs, VI) + entry.VIreg * sizeof(REG_VI)));
			entry.dirty = false;
		}
		if (clearEntry)
			clearGPR(regIdx);
	}

	void clearNeon(int regIdx)
	{
		neonMap[regIdx].VFreg = -1;
		neonMap[regIdx].xyzw = 0;
		neonMap[regIdx].count = 0;
		neonMap[regIdx].isNeeded = false;
		neonMap[regIdx].isZero = false;
	}

	void clearGPR(int regIdx)
	{
		gprMap[regIdx].VIreg = -1;
		gprMap[regIdx].count = 0;
		gprMap[regIdx].isNeeded = false;
		gprMap[regIdx].dirty = false;
		gprMap[regIdx].isZeroExtended = false;
	}

public:
	microRegAlloc(int _index)
	{
		index = _index;

		// Mark usable GPRs for VI allocation.
		// CRITICAL: gprT1(w9), gprT2(w10), gprT3(w11) are scratch registers
		// used by codegen (address computation, temps). They MUST NOT be in
		// the VI allocation pool or they'll be clobbered.
		// Also exclude w12-w13 which are used as scratch in flag extraction.
		gprMap.fill({-1, 0, false, false, false, false});
		for (int i = 14; i <= 15; i++)
			gprMap[i].usable = true;   // x14-x15: caller-saved, safe for VI
		for (int i = 26; i <= 28; i++)
			gprMap[i].usable = true;   // x26-x28: callee-saved, VI cache
		// x24 = gprMVUFlag (pinned to &mVU.macFlag[0]),
		// x25 = gprMVUglob (pinned to &mVUglob).
		// Both pinned by mVUdispatcherAB; excluded from VI allocation.

		reset(false);
	}

	// cop2mode is unused here: x86 toggles fastmem-base/text-pointer GPR
	// usability per cop2 mode, but the arm64 allocator pins those base
	// registers outside the allocatable set, so the mode does not change the
	// reset.
	void reset(bool cop2mode = false)
	{
		for (int i = 0; i < neonAllocTotal; i++)
			clearNeon(i);
		for (int i = 0; i < gprAllocCount; i++)
		{
			if (gprMap[i].usable)
				clearGPR(i);
		}
		counter = 0;
	}

	//------------------------------------------------------------------
	// VF Register Allocation (NEON Q registers)
	//------------------------------------------------------------------

	// Emit the NEON equivalent of x86's PSHUF.D(dst, src, imm) used in the
	// clone-write path for single-scalar VF ops. Moves src's lane `srcLane`
	// into dst's lane 0. Other dst lanes are don't-care (SS ops only read
	// lane 0). dst and src may alias.
	__fi void emitSSShuffle(const a64::VRegister& dst, const a64::VRegister& src, int srcLane)
	{
		// Compare physical register numbers, not the parameter-reference
		// addresses: dst/src bind to distinct caller locals even when they name
		// the same NEON register (the clone path), so &dst != &src would emit a
		// redundant self-Mov. GetCode() detects the true self-alias.
		if (srcLane == 0)
		{
			if (dst.GetCode() != src.GetCode())
				armAsm->Mov(dst.V16B(), src.V16B());
			return;
		}
		if (dst.GetCode() != src.GetCode())
			armAsm->Mov(dst.V16B(), src.V16B());
		armAsm->Ext(dst.V16B(), dst.V16B(), dst.V16B(), srcLane * 4);
	}

	// Allocate a NEON register for VF access. Ported from x86 allocReg —
	// preserves cache-validity and clone-write SS-shuffle semantics.
	//
	// vfLoadReg: VF to load from (-1=temp, 0-31=VF, 32=ACC, 33=I)
	// vfWriteReg: VF to write to (-1=none/read-only, 0-31, 32=ACC, 33=I)
	// xyzw: components touched (mask: X=8, Y=4, Z=2, W=1; 0=full-read;
	//       0xF=full-write). For SS writes a single bit is set.
	// cloneWrite: if true, clone cached reg so caller's write doesn't
	//             stomp the cached value.
	const a64::VRegister allocReg(int vfLoadReg = -1, int vfWriteReg = -1, int xyzw = 0, bool cloneWrite = true)
	{
		counter++;

		// Search for cached copy.
		if (vfLoadReg >= 0)
		{
			for (int i = 0; i < neonAllocTotal; i++)
			{
				microMapNEON& mapI = neonMap[i];
				// Cache is only valid when:
				//   - Reg was not modified (xyzw == 0), OR
				//   - Reg had ALL vectors modified (xyzw == 0xF) and it's not VF0.
				// Partial-dirty caches are NOT reused; the partial writes live
				// in-register but memory still reflects the pre-partial value.
				if (mapI.VFreg == vfLoadReg
					&& (!mapI.xyzw
					    || (mapI.VFreg != 0 && mapI.xyzw == 0xF)))
				{
					int z = i;
					if (vfWriteReg >= 0)
					{
						const a64::VRegister qmmI = armQRegister(i);
						if (cloneWrite)
						{
							z = findFreeNeon(vfWriteReg);
							const a64::VRegister qmmZ = armQRegister(z);
							writeBackNeon(z);

							if (xyzw == 4)
								emitSSShuffle(qmmZ, qmmI, 1); // Y to lane 0
							else if (xyzw == 2)
								emitSSShuffle(qmmZ, qmmI, 2); // Z to lane 0
							else if (xyzw == 1)
								emitSSShuffle(qmmZ, qmmI, 3); // W to lane 0
							else if (z != i)
								armAsm->Mov(qmmZ.V16B(), qmmI.V16B());

							mapI.count = counter; // Reg i was used, so update counter.
						}
						else
						{
							if ((vfLoadReg != vfWriteReg) || (xyzw != 0xF))
								writeBackNeon(i);

							if (xyzw == 4)
								emitSSShuffle(qmmI, qmmI, 1);
							else if (xyzw == 2)
								emitSSShuffle(qmmI, qmmI, 2);
							else if (xyzw == 1)
								emitSSShuffle(qmmI, qmmI, 3);
						}
						neonMap[z].VFreg = vfWriteReg;
						neonMap[z].xyzw  = xyzw;
						neonMap[z].isZero = (vfLoadReg == 0);
					}
					neonMap[z].count = counter;
					neonMap[z].isNeeded = true;
					return armQRegister(z);
				}
			}
		}

		// Not cached — allocate a fresh slot.
		int x = findFreeNeon(vfWriteReg >= 0 ? vfWriteReg : vfLoadReg);
		const a64::VRegister qmmX = armQRegister(x);
		writeBackNeon(x);

		if (vfWriteReg >= 0) // Reg will be modified (partial reg loading allowed)
		{
			if ((vfLoadReg == 0) && !(xyzw & 1))
			{
				// Writing to a partial fresh slot based on VF0 with X masked out.
				// x86 issues PXOR to zero the reg; lane 0 won't be read by the op.
				armAsm->Eor(qmmX.V16B(), qmmX.V16B(), qmmX.V16B());
			}
			else if (vfLoadReg == 33) // I register
			{
				loadIreg(qmmX, xyzw);
			}
			else if (vfLoadReg == 32)
			{
				mVUloadReg(qmmX, gprVUState, offsetof(VURegs, ACC), xyzw);
			}
			else if (vfLoadReg >= 0)
			{
				mVUloadReg(qmmX, gprVUState, offsetof(VURegs, VF) + vfLoadReg * sizeof(VECTOR), xyzw);
			}

			neonMap[x].VFreg = vfWriteReg;
			neonMap[x].xyzw  = xyzw;
		}
		else // Reg will not be modified (always load the full reg so it can be cached)
		{
			if (vfLoadReg == 33)
			{
				loadIreg(qmmX, 0xF);
			}
			else if (vfLoadReg == 32)
			{
				armAsm->Ldr(qmmX, mVUstateMem(offsetof(VURegs, ACC)));
			}
			else if (vfLoadReg >= 0)
			{
				armAsm->Ldr(qmmX, mVUstateMem(offsetof(VURegs, VF) + vfLoadReg * sizeof(VECTOR)));
			}

			neonMap[x].VFreg = vfLoadReg;
			neonMap[x].xyzw  = 0;
		}
		neonMap[x].isZero    = (vfLoadReg == 0);
		neonMap[x].count     = counter;
		neonMap[x].isNeeded  = true;
		return qmmX;
	}

	//------------------------------------------------------------------
	// VI Register Allocation (ARM64 W registers)
	//------------------------------------------------------------------

	// Flush and un-cache any existing GPR slot that claims VIreg == targetVI.
	// Ported from the x86 allocGPR duplicate-flush path. If the slot is still
	// in use by an active allocGPR, unbind it (clear VIreg/dirty/isZeroExtended) while
	// leaving isNeeded intact so findFreeGPR won't hand the physical reg to
	// another allocation. Otherwise fully clear the slot.
	void unbindAnyVIAllocations(int targetVI, bool& backup)
	{
		if (targetVI < 0)
			return;
		for (int i = 0; i < gprAllocCount; i++)
		{
			microMapGPR& mapI = gprMap[i];
			if (!mapI.usable || mapI.VIreg != targetVI)
				continue;

			if (backup)
			{
				writeVIBackup(armWRegister(i));
				backup = false;
			}

			if (mapI.isNeeded)
			{
				// Still held by a live allocation — flush to memory but keep
				// the physical reg bound to its caller.
				writeBackGPR(i, false);
				mapI.VIreg = -1;
				mapI.dirty = false;
				mapI.isZeroExtended = false;
			}
			else
			{
				// No one is using this slot — fully release it.
				writeBackGPR(i, false);
				clearGPR(i);
			}

			// Invariant: only one slot can be bound to a given VIreg.
			for (int j = i + 1; j < gprAllocCount; j++)
				pxAssert(gprMap[j].VIreg != targetVI);
			break;
		}
	}

	const a64::Register allocGPR(int viLoadReg = -1, int viWriteReg = -1, bool backup = false, bool zext_if_dirty = false)
	{
		counter++;

		// Writing zero? Return a zeroed register
		if (viWriteReg == 0)
		{
			int idx = findFreeGPR(0);
			writeBackGPR(idx);
			clearGPR(idx);
			armAsm->Mov(armWRegister(idx), 0);
			gprMap[idx].VIreg = 0;
			gprMap[idx].isNeeded = true;
			gprMap[idx].isZeroExtended = true;
			return armWRegister(idx);
		}

		// Search for cached copy
		if (viLoadReg >= 0)
		{
			for (int i = 0; i < gprAllocCount; i++)
			{
				if (!gprMap[i].usable)
					continue;
				if (gprMap[i].VIreg == viLoadReg)
				{
					// Bump count on the Is cache slot before anything can
					// steal it via findFreeGPR (matches x86 ordering).
					gprMap[i].count = counter;

					if (viWriteReg >= 0)
					{
						if (viLoadReg != viWriteReg)
						{
							// Clone-write: allocate a NEW slot for viWriteReg,
							// copy the Is value into it, leave the Is cache
							// entry untouched. Matches x86 allocGPR.
							unbindAnyVIAllocations(viWriteReg, backup);
							int x = findFreeGPR(viWriteReg);
							writeBackGPR(x);

							if (backup && gprMap[x].VIreg != viWriteReg)
							{
								armAsm->Ldrh(armWRegister(x),
									mVUstateMem(offsetof(VURegs, VI) + viWriteReg * sizeof(REG_VI)));
								writeVIBackup(armWRegister(x));
								backup = false;
							}

							armAsm->Mov(armWRegister(x).W(), armWRegister(i).W());

							gprMap[x].isZeroExtended = zext_if_dirty;
							// Swap so `i` names the new slot — matches x86's
							// std::swap(x, i) trick. The Is slot (now named x)
							// intentionally does NOT get isNeeded=true; only
							// the write slot the caller will clearNeeded.
							std::swap(x, i);
						}
						else
						{
							// In-place read-modify-write: no longer zext.
							gprMap[i].isZeroExtended = false;
						}

						gprMap[i].VIreg = viWriteReg;
						gprMap[i].dirty = true;
					}
					else if (zext_if_dirty && !gprMap[i].isZeroExtended)
					{
						// Mirror x86 allocGPR's zero-extend path. Caller (e.g. mVU_ISW)
						// needs 32-bit-clean storage of a 16-bit VI value. If the
						// cached slot was last written by IADDIU/IADD/IOR/etc., its
						// top 16 bits are dirty and an unmasked Str(W) would push
						// garbage into the GIFtag's PRIM/NREG bits.
						armAsm->Uxth(armWRegister(i), armWRegister(i));
						gprMap[i].isZeroExtended = true;
					}

					gprMap[i].isNeeded = true;

					if (backup)
						writeVIBackup(armWRegister(i));
					return armWRegister(i);
				}
			}
		}

		// Not cached — allocate new. Flush any duplicate binding of viWriteReg first.
		if (viWriteReg >= 0)
			unbindAnyVIAllocations(viWriteReg, backup);

		int idx = findFreeGPR(viWriteReg >= 0 ? viWriteReg : viLoadReg);
		writeBackGPR(idx);

		gprMap[idx].count = counter;
		gprMap[idx].isNeeded = true;

		if (viLoadReg >= 0 && viLoadReg != 0)
		{
			// Load VI from memory (16-bit zero-extended)
			armAsm->Ldrh(armWRegister(idx),
				mVUstateMem(offsetof(VURegs, VI) + viLoadReg * sizeof(REG_VI)));
			gprMap[idx].isZeroExtended = true;
		}
		else if (viLoadReg == 0)
		{
			armAsm->Mov(armWRegister(idx), 0);
			gprMap[idx].isZeroExtended = true;
		}

		gprMap[idx].VIreg = (viWriteReg >= 0) ? viWriteReg : ((viLoadReg >= 0) ? viLoadReg : -1);
		gprMap[idx].dirty = (viWriteReg >= 0);

		if (backup)
		{
			// viWriteReg wasn't already in any GPR slot (so unbindAny didn't
			// back it up). For write-only allocations (viLoadReg < 0, e.g.
			// MTIR), the freshly-allocated `idx` register is uninitialised at
			// this point — backing it up would store garbage to mVU.VIbackup
			// and the following IBxxx branch would read garbage.
			//
			// Load viWriteReg's CURRENT (pre-write) value from VI memory first,
			// then back it up. Mirrors x86 allocGPR.
			if (viLoadReg < 0 && viWriteReg > 0)
			{
				armAsm->Ldrh(armWRegister(idx),
					mVUstateMem(offsetof(VURegs, VI) + viWriteReg * sizeof(REG_VI)));
			}
			writeVIBackup(armWRegister(idx));
		}

		return armWRegister(idx);
	}

	//------------------------------------------------------------------
	// Clear / Flush
	//------------------------------------------------------------------

	// Mark a NEON slot as no-longer-needed after the op that allocated it is
	// done. Matches x86 clearNeeded: when the cleared
	// slot was written to (xyzw != 0), we must either merge the partial
	// write into another cached copy of the same VFreg, or flush it to
	// memory — otherwise the partial write is stuck in a cache slot that
	// subsequent cache searches will skip (xyzw mismatch in the cache-
	// validity check in allocReg), causing fresh-loads to read stale memory
	// and lose the write entirely.
	//
	// Full writes (xyzw == 0xF) invalidate other cached copies (they hold
	// the complete current state).
	// Partial writes (xyzw in {0x1..0xE}) try to merge into another copy;
	// if no other copy exists, writeback flushes to memory.
	void clearNeeded(const a64::VRegister& reg)
	{
		const int idx = reg.GetCode();
		if (idx >= neonAllocTotal)
			return;

		microMapNEON& clear = neonMap[idx];
		clear.isNeeded = false;

		if (!clear.xyzw) // Read-only slot, nothing to do.
			return;

		if (clear.VFreg <= 0) // Temp or VF0: just drop the slot.
		{
			clearNeon(idx);
			return;
		}

		// Modified VFreg: handle merge / invalidate of other cached copies.
		int mergeState = 0; // 0: full-write, invalidate others
		                    // 1: partial-write, haven't merged yet
		                    // 2: partial-write, merged into another slot
		if (clear.xyzw < 0xF)
			mergeState = 1;

		for (int i = 0; i < neonAllocTotal; i++)
		{
			if (i == idx)
				continue;
			microMapNEON& mapI = neonMap[i];
			if (mapI.VFreg != clear.VFreg)
				continue;

			if (mergeState == 1)
			{
				// First other cached copy found — merge our partial write
				// into it. The merged reg now holds the complete state, so
				// mark it as fully valid (xyzw=0xF). We'll invalidate our
				// own slot below.
				mVUmergeRegs(armQRegister(i), reg, clear.xyzw, /*modXYZW=*/true);
				mapI.xyzw  = 0xF;
				mapI.count = counter;
				mergeState = 2;
			}
			else
			{
				// Full-write path OR we already merged into another slot:
				// invalidate this copy (our slot, or the merged copy, now
				// holds the authoritative state).
				clearNeon(i);
			}
		}

		if (mergeState == 2)
		{
			// Partial write was merged into another slot — this slot is no longer needed.
			clearNeon(idx);
		}
		else if (mergeState == 1)
		{
			// No other cached copy to merge into — flush to memory. The
			// writeBack invalidates the cache entry for partial writes.
			writeBackNeon(idx);
		}
		// else mergeState == 0 (full write): cache is still valid after
		//      other copies were invalidated. Keep it cached; writeback
		//      to memory happens at eviction or flushAll time.
	}

	void clearNeeded(const a64::Register& reg)
	{
		const int idx = reg.GetCode();
		if (idx < gprAllocCount && gprMap[idx].usable)
			gprMap[idx].isNeeded = false;
	}

	void writeBackReg(const a64::VRegister& reg, bool invalidate = true)
	{
		const int idx = reg.GetCode();
		if (idx < neonAllocTotal)
			writeBackNeon(idx, invalidate);
	}

	void writeBackReg(const a64::Register& reg, bool clearDirty = true)
	{
		const int idx = reg.GetCode();
		if (idx < gprAllocCount && gprMap[idx].usable)
			writeBackGPR(idx, clearDirty);
	}

	void flushAll(bool clearState = true)
	{
		for (int i = 0; i < neonAllocTotal; i++)
		{
			writeBackNeon(i, clearState);
		}
		for (int i = 0; i < gprAllocCount; i++)
		{
			if (gprMap[i].usable)
				writeBackGPR(i, clearState);
		}
	}

	// Snapshot allocator state for vudivtrace meta records. Captured at JIT
	// compile time, BEFORE the divtrace flushAll, so the report shows what
	// the allocator was holding at the moment of each microvu instruction.
	mvu_divtrace::AllocSnapshot snapshotMaps() const
	{
		mvu_divtrace::AllocSnapshot s{};
		static_assert(neonAllocTotal == mvu_divtrace::kNeonSlots,
			"divtrace neon slot count mismatch");
		static_assert(gprAllocCount == mvu_divtrace::kGprSlots,
			"divtrace gpr slot count mismatch");
		for (int i = 0; i < neonAllocTotal; i++)
		{
			s.neon[i] = {neonMap[i].VFreg, neonMap[i].xyzw, neonMap[i].count,
				neonMap[i].isNeeded, neonMap[i].isZero};
		}
		for (int i = 0; i < gprAllocCount; i++)
		{
			s.gpr[i] = {gprMap[i].VIreg, gprMap[i].count, gprMap[i].isNeeded,
				gprMap[i].dirty, gprMap[i].isZeroExtended, gprMap[i].usable};
		}
		return s;
	}

	void flushCallerSavedRegisters(bool clearNeededFlag = false)
	{
		// Flush NEON caller-saved: Q0-Q7, Q16-Q27
		for (int i = 0; i < 8; i++)
			writeBackNeon(i, true);
		for (int i = 16; i < neonAllocTotal; i++)
			writeBackNeon(i, true);

		// Flush GPR caller-saved: x9-x15
		for (int i = 9; i <= 15; i++)
		{
			if (gprMap[i].usable)
				writeBackGPR(i, true);
		}
	}

	void flushPartialForCOP2()
	{
		// For COP2 transition: write back dirty regs, keep clean caches
		for (int i = 0; i < neonAllocTotal; i++)
		{
			if (neonMap[i].VFreg < 0) // Temp
				clearNeon(i);
			else if (neonMap[i].xyzw != 0 && neonMap[i].xyzw != 0xF) // Partial dirty
				writeBackNeon(i, true);
		}
	}

	//------------------------------------------------------------------
	// Query
	//------------------------------------------------------------------

	bool checkCachedReg(int regId) const
	{
		return (regId < neonAllocTotal && neonMap[regId].VFreg >= 0);
	}

	bool checkCachedGPR(int regId) const
	{
		return (regId < gprAllocCount && gprMap[regId].usable &&
		        (gprMap[regId].VIreg >= 0 || gprMap[regId].isNeeded));
	}

	bool hasRegVF(int vfreg) const
	{
		for (int i = 0; i < neonAllocTotal; i++)
			if (neonMap[i].VFreg == vfreg) return true;
		return false;
	}

	bool hasRegVI(int vireg) const
	{
		for (int i = 0; i < gprAllocCount; i++)
			if (gprMap[i].usable && gprMap[i].VIreg == vireg) return true;
		return false;
	}

	// Helpers used by mvuPreloadRegisters. Match the x86 register-count
	// accessors.
	int getNeonCount() const { return neonAllocTotal + 1; }

	int getFreeNeonCount() const
	{
		int count = 0;
		for (int i = 0; i < neonAllocTotal; i++)
		{
			if (!neonMap[i].isNeeded && neonMap[i].VFreg < 0)
				count++;
		}
		return count;
	}

	int getRegVF(int i) const
	{
		return (i < neonAllocTotal) ? neonMap[i].VFreg : -1;
	}

	int getGPRCount() const { return gprAllocCount; }

	int getFreeGPRCount() const
	{
		int count = 0;
		for (int i = 0; i < gprAllocCount; i++)
		{
			if (gprMap[i].usable && !gprMap[i].isNeeded && gprMap[i].VIreg < 0)
				count++;
		}
		return count;
	}

	int getRegVI(int i) const
	{
		return (i < gprAllocCount && gprMap[i].usable) ? gprMap[i].VIreg : -1;
	}

	// Move VI value into a specific GPR (for address computation etc.)
	void moveVIToGPR(const a64::Register& dstReg, int vi, bool signext = false)
	{
		// Check if cached
		for (int i = 0; i < gprAllocCount; i++)
		{
			if (gprMap[i].usable && gprMap[i].VIreg == vi)
			{
				if (signext)
					armAsm->Sxth(dstReg.W(), armWRegister(i));
				else if (static_cast<int>(dstReg.GetCode()) != i)
					armAsm->Mov(dstReg.W(), armWRegister(i));
				return;
			}
		}

		// Not cached — load from memory
		const a64::MemOperand src = mVUstateMem(offsetof(VURegs, VI) + vi * sizeof(REG_VI));
		if (signext)
			armAsm->Ldrsh(dstReg.W(), src);
		else
			armAsm->Ldrh(dstReg.W(), src);
	}

	// Defined out-of-line after microVU struct is complete
	void writeVIBackup(const a64::Register& reg);

	// Check if a VF register needs clamping (skip VF0 and I-reg)
	bool checkVFClamp(int regId) const
	{
		if (regId >= neonAllocTotal)
			return true;
		if ((neonMap[regId].VFreg == 33 && !EmuConfig.Gamefixes.IbitHack) || neonMap[regId].isZero)
			return false;
		return true;
	}

	// COP2 stubs
	void clearRegCOP2(int hostreg) {}
	void clearGPRCOP2(int hostreg) {}
};
