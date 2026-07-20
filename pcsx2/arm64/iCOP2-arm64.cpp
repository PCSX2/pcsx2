// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 native COP2 (VU0 macro mode) codegen using NEON.
// Memory-based: loads VF regs from VU0.VF[], computes with NEON, stores back.
// MAC/status flags are updated via C helper calls for correctness.
// No VU register allocator — each instruction is self-contained.

#include "arm64/iR5900-arm64.h"

#include "VUmicro.h" // CpuVU0 — VE-08 thin sync helpers

#include "common/Assertions.h"

namespace a64 = vixl::aarch64;

// ========================================================================
//  COP2 instruction field decoding (VU encoding within EE instruction)
// ========================================================================
// VU fields reuse EE instruction bit positions:
//   _Ft_ = bits 20-16 (same as _Rt_)
//   _Fs_ = bits 15-11 (same as _Rd_)
//   _Fd_ = bits 10-6  (same as _Sa_)
//   dest = bits 24-21  (XYZW write mask)

#define _Ft_cop2  _Rt_
#define _Fs_cop2  _Rd_
#define _Fd_cop2  _Sa_

#define _X_cop2  ((cpuRegs.code >> 24) & 0x1)
#define _Y_cop2  ((cpuRegs.code >> 23) & 0x1)
#define _Z_cop2  ((cpuRegs.code >> 22) & 0x1)
#define _W_cop2  ((cpuRegs.code >> 21) & 0x1)
#define _XYZW_cop2 ((cpuRegs.code >> 21) & 0xF)

// Broadcast field for bc variants (bits 1-0 of function code)
#define _bc_cop2 (cpuRegs.code & 0x3)

// Fsf/Ftf fields for scalar source selection
#define _Fsf_cop2 ((cpuRegs.code >> 21) & 0x3)
#define _Ftf_cop2 ((cpuRegs.code >> 23) & 0x3)

// ========================================================================
//  NEON scratch register assignments for COP2
// ========================================================================
// q30 (RQSCRATCH)  = fs operand / result
// q31 (RQSCRATCH2) = ft operand
// q29 (RQSCRATCH3) = dest mask / ACC / temp

// ========================================================================
//  Dest field mask table — 16 entries for each XYZW combination
// ========================================================================
// Each entry is a 128-bit mask: lane = 0xFFFFFFFF if written, 0 if not.
// XYZW is 4 bits: X=bit3, Y=bit2, Z=bit1, W=bit0
// Lane order in NEON: [0]=x, [1]=y, [2]=z, [3]=w
alignas(16) static const u32 s_cop2DestMasks[16][4] = {
	{0x00000000, 0x00000000, 0x00000000, 0x00000000}, // 0000
	{0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF}, // 000W
	{0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000}, // 00Z0
	{0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF}, // 00ZW
	{0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000}, // 0Y00
	{0x00000000, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF}, // 0Y0W
	{0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000}, // 0YZ0
	{0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}, // 0YZW
	{0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000}, // X000
	{0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF}, // X00W
	{0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0x00000000}, // X0Z0
	{0xFFFFFFFF, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF}, // X0ZW
	{0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000}, // XY00
	{0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0xFFFFFFFF}, // XY0W
	{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000}, // XYZ0
	{0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}, // XYZW
};

// ========================================================================
//  VF register load/store helpers
// ========================================================================

// ========================================================================
//  EP-2b: compile-time VF/ACC residency cache for hand-rolled COP2 macro ops
// ========================================================================
// The hand-rolled macro FMAC bodies used to round-trip every operand and
// result through VU0 memory (2 q-loads + 1-2 q-stores per op). Consecutive
// COP2 macro ops in a block reuse a small set of VF registers plus ACC, so a
// tiny compile-time cache keeps them resident in q16..q20 and the op bodies
// compute 3-operand NEON straight from the cache registers.
//
// Register choice: q16-q26 have no fixed user in EE-block emission context
// (q0-q7 = allocator temp/FPR first-fit + vtlb data + mVU macro window,
// q8/q9 = pinned FPU clamp constants, q10-q15 = allocator GPR-quad/FPR homes,
// q27/q28 = VOPMULA/VCLIP + flag-body scratch, q29-q31 = per-op scratch).
// They are caller-saved and NOT preserved by the fastmem fault thunk (which
// only saves allocator-tracked regs), so the cache must never survive any op
// that can reach a C call or a fastmem access — which the seam policy below
// guarantees. The one structural overlap is the EE allocator's FPR/FPACC/TEMP
// *fallback* range (first-fit reaches q16+ only when q0-q15 minus q8/q9 are
// all live): cop2VfCacheClaimSlot() evicts any allocator residency from the
// claimed host reg before use.
//
// Seam policy (who kills the cache):
//  - recompileNextInstruction flushes the cache for every op that is NOT a
//    cache-aware hand-rolled COP2 op (cop2OpPreservesVfCache) — covers the
//    mVU-reuse wrappers, transfers, VDIV-family raw lane reads, VCALLMS,
//    LQC2/SQC2, MMI/FPU, branches, everything unknown.
//  - iFlushCall flushes (covers the conditional-VU0-sync C calls inside
//    whitelisted ops — emitted BEFORE the runtime Tbz, so the writebacks sit
//    on the unconditional path — and every block tail, which all run
//    iFlushCall(FLUSH_EVERYTHING)).
//  - SetBranchImm/SetBranchImmCall/SetBranchReg snapshot the compile-time
//    state around their body and restore it: each branch fork's tail emits
//    its own writebacks (the values stay register-resident along both
//    runtime paths), and the sibling fork re-emits its own.
//  - SaveBranchState/LoadBranchState snapshot/restore the state so a
//    per-fork delay-slot COP2 op can't leak residency into the other fork.
//
// Dirty values are written back on eviction, flush, and fork tails. VF0 is
// never written by the dest-mask paths (hardware read-only), so its slot is
// always clean and the frequent VF0 operand loads become cache hits.

// Cop2VfCacheState is declared in iR5900-arm64.h (branch emitters snapshot it).
static constexpr int kCop2VfCacheSlots = 5;
static constexpr int kCop2VfCacheFirstQ = 16; // q16..q20
static constexpr int kCop2VfCacheACC = 32;    // pseudo VF index for ACC
static_assert(std::size(Cop2VfCacheState{}.slot) == kCop2VfCacheSlots);

static Cop2VfCacheState s_cop2VfCache;

static a64::VRegister cop2VfSlotReg(int slot)
{
	return a64::VRegister(kCop2VfCacheFirstQ + slot, 128);
}

static a64::MemOperand cop2VfHome(int vf)
{
	return (vf == kCop2VfCacheACC) ? armVU0Mem(&VU0.ACC) : armVU0Mem(&VU0.VF[vf]);
}

void cop2VfCacheReset()
{
	for (auto& s : s_cop2VfCache.slot)
	{
		s.vf = -1;
		s.dirty = false;
		s.lastUse = 0;
	}
	s_cop2VfCache.tick = 0;
}

Cop2VfCacheState cop2VfCacheGetState()
{
	return s_cop2VfCache;
}

void cop2VfCacheSetState(const Cop2VfCacheState& state)
{
	s_cop2VfCache = state;
}

// Emit writebacks for dirty slots WITHOUT touching compile-time state.
// Used by flush; fork tails get the same effect via GetState/SetState
// around the destructive flush inside iFlushCall.
static void cop2VfCacheEmitWritebacks()
{
	for (int i = 0; i < kCop2VfCacheSlots; i++)
	{
		if (s_cop2VfCache.slot[i].vf >= 0 && s_cop2VfCache.slot[i].dirty)
			armAsm->Str(cop2VfSlotReg(i), cop2VfHome(s_cop2VfCache.slot[i].vf));
	}
}

void cop2VfCacheFlush()
{
	cop2VfCacheEmitWritebacks();
	for (auto& s : s_cop2VfCache.slot)
	{
		s.vf = -1;
		s.dirty = false;
	}
}

static int cop2VfCacheLookup(int vf)
{
	for (int i = 0; i < kCop2VfCacheSlots; i++)
	{
		if (s_cop2VfCache.slot[i].vf == vf)
		{
			s_cop2VfCache.slot[i].lastUse = ++s_cop2VfCache.tick;
			return i;
		}
	}
	return -1;
}

// Claim a slot for `vf`: reuse its existing slot, else evict the LRU victim
// (emitting the victim's writeback if dirty) and detach any EE-allocator
// residency from the host reg (the FPR/FPACC/TEMP fallback range overlaps —
// see the header comment). fill=true loads the current memory value into the
// slot; full-overwrite writers pass fill=false.
static int cop2VfCacheClaimSlot(int vf, bool fill)
{
	int slot = cop2VfCacheLookup(vf);
	if (slot < 0)
	{
		slot = 0;
		for (int i = 1; i < kCop2VfCacheSlots; i++)
		{
			if (s_cop2VfCache.slot[i].vf < 0)
			{
				slot = i;
				break;
			}
			if (s_cop2VfCache.slot[slot].vf >= 0 &&
				s_cop2VfCache.slot[i].lastUse < s_cop2VfCache.slot[slot].lastUse)
				slot = i;
		}

		if (s_cop2VfCache.slot[slot].vf >= 0 && s_cop2VfCache.slot[slot].dirty)
			armAsm->Str(cop2VfSlotReg(slot), cop2VfHome(s_cop2VfCache.slot[slot].vf));

		// Structural guard: evict any allocator residency from the host reg
		// (writes back a live FPR/temp if the fallback path ever placed one
		// here; a no-op in practice).
		_freeNEONreg(kCop2VfCacheFirstQ + slot);

		s_cop2VfCache.slot[slot].vf = static_cast<s8>(vf);
		s_cop2VfCache.slot[slot].dirty = false;
		s_cop2VfCache.slot[slot].lastUse = ++s_cop2VfCache.tick;
		if (fill)
			armAsm->Ldr(cop2VfSlotReg(slot), cop2VfHome(vf));
	}
	return slot;
}

// Fetch VF[vf] (or ACC via kCop2VfCacheACC) as a READ-ONLY operand register.
// Cache hit: the resident slot reg, no emission. Miss: allocates a slot and
// loads it (same 1-insn cost as the old direct Ldr; later uses are free).
// Callers must NEVER write the returned register — compute 3-operand into
// scratch instead.
static a64::VRegister cop2GetVF(int vf)
{
	return cop2VfSlotReg(cop2VfCacheClaimSlot(vf, true));
}

static a64::VRegister cop2GetACC()
{
	return cop2GetVF(kCop2VfCacheACC);
}

// Copy VF[vf] into `qreg` for bodies that must mutate the value in place
// (saturated FTOI, VMOVE): cache hit costs a Mov instead of a Ldr; a miss
// loads memory directly WITHOUT claiming a slot, so the cost never exceeds
// the old direct load.
static void cop2LoadVFViaCache(const a64::VRegister& qreg, int vf)
{
	const int slot = cop2VfCacheLookup(vf);
	if (slot >= 0)
	{
		if (qreg.GetCode() != cop2VfSlotReg(slot).GetCode())
			armAsm->Mov(qreg.V16B(), cop2VfSlotReg(slot).V16B());
	}
	else
		armAsm->Ldr(qreg, cop2VfHome(vf));
}

// ========================================================================
//  Dest field masking
// ========================================================================

// Apply dest mask: merge 'result' in RQSCRATCH into VU0.VF[fdReg], writing
// only the lanes selected by `xyzw`. The variants without an explicit `xyzw`
// read it from the instruction (_XYZW_cop2); VOPMSUB / VOPMULA force xyzw=0xE
// since PS2 hardware always writes XYZ regardless of the encoded dest field.
// Map a single-bit dest mask to its vector lane / VF.UL index:
// bit3=x→lane0, bit2=y→lane1, bit1=z→lane2, bit0=w→lane3.
static __fi int cop2SingleLaneFromMask(int xyzw)
{
	switch (xyzw)
	{
		case 0x8: return 0; // x
		case 0x4: return 1; // y
		case 0x2: return 2; // z
		case 0x1: return 3; // w
		default:  return -1;
	}
}

// Store one 32-bit lane of `result` into base[lane]. Lane 0 is a plain Str
// of the S view (imm-offset addressing works). Lanes 1-3 use ST1 {Vt.S}[i],
// which — like LD1R (see armLd1rVU0) — silently drops an immediate offset
// outside Debug builds, so the address is materialized with a single ADD
// (VURegs fields are within imm12 of RVU0).
static void cop2StoreSingleLane(const a64::VRegister& result, const void* base, int lane)
{
	if (lane == 0)
	{
		armAsm->Str(result.S(), armVU0Mem(base));
		return;
	}
	const ptrdiff_t off = reinterpret_cast<const u8*>(base) - reinterpret_cast<const u8*>(&VU0) + lane * 4;
	armAsm->Add(RSCRATCHADDR, RVU0, off);
	armAsm->St1(result.V4S(), lane, a64::MemOperand(RSCRATCHADDR));
}

// Pick the register an op should compute its result into: for a full-mask
// write to a non-zero fd, that is fd's cache slot itself — claimed no-fill up
// front so the arithmetic lands in place and the dest-mask step emits NOTHING
// (a singleton write costs exactly the old direct store, paid at the next
// seam's writeback). Everything else computes into RQSCRATCH.
// Invariant: call this AFTER fetching the op's cache operands; the ≤4 distinct
// claims per op (fs, ft, ACC, fd) never evict each other with 5 slots.
// S4-4: used only by the tail-less ops (VMOVE/VMR32/VABS/ITOF) — the FMAC
// family computes into RQSCRATCH so the shared tail stubs see a fixed ABI.
static a64::VRegister cop2ResultReg(int fdReg, int xyzw)
{
	if (xyzw == 0xF && fdReg != 0)
		return cop2VfSlotReg(cop2VfCacheClaimSlot(fdReg, /*fill=*/false));
	return RQSCRATCH;
}

// Write the (clamped) result in `result` into VF[fd] under the dest mask,
// cache-aware: the value lands in (or merges into) fd's cache slot instead of
// memory, marked dirty for writeback at the next seam. When `result` came
// from cop2ResultReg it already IS the slot — the full-mask path then only
// marks dirty. Single-lane writes to an UNCACHED fd stay write-through —
// allocating (fill load + Ins) would cost more than today's 1-2 insn lane
// store.
static void cop2ApplyDestMaskExplicit(int fdReg, int xyzw,
	const a64::VRegister& result = RQSCRATCH)
{
	if (xyzw == 0 || fdReg == 0)
		return; // VF0 is hardware read-only; all dest-mask writes drop

	if (xyzw == 0xF)
	{
		const int slot = cop2VfCacheClaimSlot(fdReg, /*fill=*/false);
		if (result.GetCode() != cop2VfSlotReg(slot).GetCode())
			armAsm->Mov(cop2VfSlotReg(slot).V16B(), result.V16B());
		s_cop2VfCache.slot[slot].dirty = true;
		return;
	}

	// Single-lane fast path: one lane insert into a cached fd, or the 1-2 insn
	// direct lane store when uncached.
	const int lane = cop2SingleLaneFromMask(xyzw);
	if (lane >= 0)
	{
		const int slot = cop2VfCacheLookup(fdReg);
		if (slot >= 0)
		{
			armAsm->Ins(cop2VfSlotReg(slot).V4S(), lane, result.V4S(), lane);
			s_cop2VfCache.slot[slot].dirty = true;
			return;
		}
		cop2StoreSingleLane(result, &VU0.VF[fdReg], lane);
		return;
	}

	// Partial mask: when fd is RESIDENT, merge into the cached value and keep
	// the merge in the slot (hit: 3 insns vs the old 4, plus dirty
	// coalescing). When fd is NOT cached, stay write-through — allocating
	// here costs an extra insn (fill + merge + Mov + deferred store) over the
	// old load/merge/store shape.
	const int slot = cop2VfCacheLookup(fdReg);
	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&_cpuRegistersPack.cop2Rec.destMasks[xyzw]));
	if (slot >= 0)
	{
		armAsm->Bsl(RQSCRATCH2.V16B(), result.V16B(), cop2VfSlotReg(slot).V16B());
		armAsm->Mov(cop2VfSlotReg(slot).V16B(), RQSCRATCH2.V16B());
		s_cop2VfCache.slot[slot].dirty = true;
		return;
	}
	armAsm->Ldr(RQSCRATCH3, armVU0Mem(&VU0.VF[fdReg]));
	armAsm->Bsl(RQSCRATCH2.V16B(), result.V16B(), RQSCRATCH3.V16B());
	armAsm->Str(RQSCRATCH2, armVU0Mem(&VU0.VF[fdReg]));
}

static void cop2ApplyDestMask(int fdReg)
{
	cop2ApplyDestMaskExplicit(fdReg, _XYZW_cop2);
}

static void cop2ApplyDestMaskACCExplicit(const a64::VRegister& result, int xyzw)
{
	if (xyzw == 0)
		return;

	if (xyzw == 0xF)
	{
		const int slot = cop2VfCacheClaimSlot(kCop2VfCacheACC, /*fill=*/false);
		if (result.GetCode() != cop2VfSlotReg(slot).GetCode())
			armAsm->Mov(cop2VfSlotReg(slot).V16B(), result.V16B());
		s_cop2VfCache.slot[slot].dirty = true;
		return;
	}

	// Single-lane fast path — mirrors cop2ApplyDestMaskExplicit.
	const int lane = cop2SingleLaneFromMask(xyzw);
	if (lane >= 0)
	{
		const int slot = cop2VfCacheLookup(kCop2VfCacheACC);
		if (slot >= 0)
		{
			armAsm->Ins(cop2VfSlotReg(slot).V4S(), lane, result.V4S(), lane);
			s_cop2VfCache.slot[slot].dirty = true;
			return;
		}
		cop2StoreSingleLane(result, &VU0.ACC, lane);
		return;
	}

	// Partial mask — same resident-only policy as the VF variant above.
	const int slot = cop2VfCacheLookup(kCop2VfCacheACC);
	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&_cpuRegistersPack.cop2Rec.destMasks[xyzw]));
	if (slot >= 0)
	{
		armAsm->Bsl(RQSCRATCH2.V16B(), result.V16B(), cop2VfSlotReg(slot).V16B());
		armAsm->Mov(cop2VfSlotReg(slot).V16B(), RQSCRATCH2.V16B());
		s_cop2VfCache.slot[slot].dirty = true;
		return;
	}
	armAsm->Ldr(RQSCRATCH3, armVU0Mem(&VU0.ACC));
	armAsm->Bsl(RQSCRATCH2.V16B(), result.V16B(), RQSCRATCH3.V16B());
	armAsm->Str(RQSCRATCH2, armVU0Mem(&VU0.ACC));
}

static void cop2ApplyDestMaskACC(const a64::VRegister& result)
{
	cop2ApplyDestMaskACCExplicit(result, _XYZW_cop2);
}

// NOTE: MAC/status flag updates are deferred — VU0.macflag/statusflag are not
// updated here. Most games don't read COP2 flags. When flag support is needed,
// emit a C call to update flags per-instruction. The interpreter fallback ops
// (DIV, CLIP, etc.) still update flags correctly.

// COP2 accesses VU0 memory, not cpuRegs GPRs — no EE register flush needed.

// ========================================================================
//  PS2 VU float clamping
// ========================================================================
// PS2 VU has no infinities — overflow clamps to ±FLT_MAX (0x7f7fffff).
// NEON FPCR has FZ=1 (denormals flushed to zero), so only post-op clamping is needed.
// FMINNM/FMAXNM match x86 MINPS/MAXPS semantics: NaN → non-NaN operand.

alignas(16) static const u32 s_cop2MaxFloat[4] = {0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff};

// VCLIP positive per-lane clip-bit weights ([+x@bit0, +y@bit2, +z@bit4]; lane w
// unused). The negative weights ([-x@bit1, -y@bit3, -z@bit5]) are these << 1, so
// only one constant is needed. After Cmgt the positive/negative masks are
// weighted per lane and a horizontal Addv collapses them into the 6-bit field
// (the +/- bits per axis are mutually exclusive and the lane contributions
// occupy disjoint bit ranges, so the add never carries between bits).
alignas(16) static const u32 s_cop2ClipWeightPos[4] = {0x01, 0x04, 0x10, 0x00};

// The COP2 emitters reach the constants above — plus the denormalized
// status-flag scratch — through _cpuRegistersPack.cop2Rec with single
// [RSTATE, #imm] accesses (see EeCop2RecState, R5900.h) instead of a 3-insn
// absolute-address materialization per use. Q-form LDR needs a 16-aligned
// offset; the whole block must sit inside the 32-bit unsigned-imm12 window.
static_assert(offsetof(cpuRegistersPack, cop2Rec) % 16 == 0);
static_assert(offsetof(cpuRegistersPack, cop2Rec) + sizeof(EeCop2RecState) <= 16380,
	"EeCop2RecState must stay within W-imm12 reach of RSTATE");

// (Re)write the pack copies of the COP2 rec constants. Called from
// recResetRaw, so the harnesses that reset the rec before compiling are
// covered too. minFloat is the pre-negated clamp lower bound: the clamp
// emitters spend a 1-insn load where they used to spend an Fneg.
void cop2RecWritePackConstants()
{
	EeCop2RecState& st = _cpuRegistersPack.cop2Rec;
	memcpy(st.maxFloat, s_cop2MaxFloat, sizeof(st.maxFloat));
	for (int i = 0; i < 4; i++)
		st.minFloat[i] = s_cop2MaxFloat[i] | 0x80000000u;
	memcpy(st.destMasks, s_cop2DestMasks, sizeof(st.destMasks));
	memcpy(st.clipWeightPos, s_cop2ClipWeightPos, sizeof(st.clipWeightPos));
	st.denormStatusFlag = 0;
	static const u32 macWeightsRev[4] = {8, 4, 2, 1};
	memcpy(st.macPackWeightsRev, macWeightsRev, sizeof(st.macPackWeightsRev));
}

// Clamp `qreg` to [-FLT_MAX, +FLT_MAX] (removes infinities and NaNs) using a
// single scratch register (the bound is reloaded between the two clamps).
// FMINNM/FMAXNM match x86 MINPS/MAXPS semantics: NaN → non-NaN operand.
// One-tmp on purpose: the S4-4 stubs built from this must leave the OTHER
// q-scratch regs untouched (a broadcast Ft parks in q31 across the Fs
// operand-clamp call). Emitted only inside the DynGen stub bodies below —
// per-site clamps are a bl to those stubs, never inline.
static void cop2ClampRegOneTmp(const a64::VRegister& qreg, const a64::VRegister& tmp)
{
	armAsm->Ldr(tmp, armCpuRegMem(&_cpuRegistersPack.cop2Rec.maxFloat));
	armAsm->Fminnm(qreg.V4S(), qreg.V4S(), tmp.V4S()); // clamp to +FLT_MAX
	armAsm->Ldr(tmp, armCpuRegMem(&_cpuRegistersPack.cop2Rec.minFloat));
	armAsm->Fmaxnm(qreg.V4S(), qreg.V4S(), tmp.V4S()); // clamp to -FLT_MAX
}

// ========================================================================
//  PS2 VU integer-comparison MAX/MINI
// ========================================================================
// PS2 VMAX/VMINI use signed integer comparison on float bit patterns,
// NOT IEEE FMAX/FMIN. This handles NaN and negative values correctly:
//   fp_max(a,b) = both_neg ? min_s32(a,b) : max_s32(a,b)
// Implemented as: selection = CMGT(a,b) XOR both_neg_mask, then BSL.
//
// Operands `a` and `b` are READ-ONLY (cache regs or scratch copies) and must
// not alias RQSCRATCH/RQSCRATCH3.
// Result: RQSCRATCH = fp_max(a, b) or fp_min(a, b)
// Clobbers: RQSCRATCH, RQSCRATCH3; a and b preserved (which also removes the
// old reload-of-a before the BSL — the selector no longer destroys it).

static void cop2EmitIntegerMax(const a64::VRegister& a, const a64::VRegister& b)
{
	armAsm->And(RQSCRATCH3.V16B(), a.V16B(), b.V16B());                  // both_neg test
	armAsm->Sshr(RQSCRATCH3.V4S(), RQSCRATCH3.V4S(), 31);                // broadcast sign → mask
	armAsm->Cmgt(RQSCRATCH.V4S(), a.V4S(), b.V4S());                     // a > b (signed int)
	armAsm->Eor(RQSCRATCH.V16B(), RQSCRATCH.V16B(), RQSCRATCH3.V16B());  // selection = CMGT XOR both_neg
	armAsm->Bsl(RQSCRATCH.V16B(), a.V16B(), b.V16B());                   // sel ? a : b
}

static void cop2EmitIntegerMin(const a64::VRegister& a, const a64::VRegister& b)
{
	// Same as max but BSL operands swapped: sel ? b : a
	armAsm->And(RQSCRATCH3.V16B(), a.V16B(), b.V16B());
	armAsm->Sshr(RQSCRATCH3.V4S(), RQSCRATCH3.V4S(), 31);
	armAsm->Cmgt(RQSCRATCH.V4S(), a.V4S(), b.V4S());
	armAsm->Eor(RQSCRATCH.V16B(), RQSCRATCH.V16B(), RQSCRATCH3.V16B());
	armAsm->Bsl(RQSCRATCH.V16B(), b.V16B(), a.V16B());                   // sel ? b : a
}

// ========================================================================
//  MAC/Status flag update infrastructure
// ========================================================================
// Implements mVUupdateFlags + mVUallocSFLAGc/d semantics.
// The status flag is stored in a "denormalized" format during macro mode:
//   Bits 0-3:   Zero sticky per lane (ZS)
//   Bits 4-7:   Sign sticky per lane (SS)
//   Bits 8-11:  Zero current per lane (Z)
//   Bits 12-15: Sign current per lane (S)
//   Bits 16+:   D/I/O/U flags (from divide ops)
//
// The "normalized" format in VU0.VI[REG_STATUS_FLAG] has:
//   Bit 0: Z (any current zero), Bit 1: S (any current sign)
//   Bit 6: ZS (any sticky zero), Bit 7: SS (any sticky sign)
//   Bits 2-5,8+: D/I/O/U flags

// Runtime storage for the denormalized status flag during macro ops is
// _cpuRegistersPack.cop2Rec.denormStatusFlag — in the pack so the emitters
// reach it with a single [RSTATE, #imm] access. Plain shared slot (not
// thread_local): COP2/VU0 macro mode runs only on the EE thread (VU0 is
// lockstep with the EE; MTVU offloads VU1 only), so one instance is correct.

// Status-flag liveness for the hand-rolled COP2 macro path (bc3729c93). With
// vuFlagHack on, the per-op status RMW (the S4-4 flag-tail stub's
// denorm-scratch update) is emitted only when the status output is actually
// consumed by a later CFC2; with the hack off, or when analysis info is
// missing, always.
static bool cop2StatusFlagLive()
{
	// CHECK_VU_FLAGHACK (microVU_Misc-arm64.h) expands to this; inlined here to
	// avoid pulling a microVU header into the COP2 codegen TU.
	return !EmuConfig.Speedhacks.vuFlagHack || !g_pCurInstInfo || (g_pCurInstInfo->info & EEINST_COP2_STATUS_FLAG);
}

// EP-4 lazy-normalization chain gates, mirroring x86 setupMacroOp/endMacroOp
// (microVU_Macro.inl): with vuFlagHack on, COP2FlagHackPass marks the FIRST
// status-writing op of each chain EEINST_COP2_DENORMALIZE_STATUS_FLAG and the
// LAST status write before a consumer (CFC2/CTC2 of STATUS, CTC2 of FBRST,
// VCALLMS, SB/SH/SW, block end — CommitStatusFlag covers all of these)
// EEINST_COP2_NORMALIZE_STATUS_FLAG. Between the two marks the denormalized
// value persists in cop2Rec.denormStatusFlag — the memory-slot equivalent of
// x86's gprF0 persistence — and VU0.VI[REG_STATUS_FLAG] is STALE. With the
// hack off (or no analysis info) both gates are always-true, which degrades to
// the per-op denormalize/normalize lockstep.
static bool cop2StatusDenormAtSetup()
{
	return !EmuConfig.Speedhacks.vuFlagHack || !g_pCurInstInfo || (g_pCurInstInfo->info & EEINST_COP2_DENORMALIZE_STATUS_FLAG);
}

static bool cop2StatusNormAtEnd()
{
	return !EmuConfig.Speedhacks.vuFlagHack || !g_pCurInstInfo || (g_pCurInstInfo->info & EEINST_COP2_NORMALIZE_STATUS_FLAG);
}

// Emit code to denormalize status flag from VU0.VI[REG_STATUS_FLAG]
// into the cop2Rec.denormStatusFlag scratch (mVUallocSFLAGd).
// Denormalized = ((norm >> 3) & 0x18) | ((norm << 11) & 0x1800) | ((norm << 14) & 0x3cf0000)
static void cop2EmitDenormalizeStatusFlag()
{
	// Load normalized status flag
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));

	// tmp2 = norm
	const a64::Register tmp1 = a64::w1;
	const a64::Register tmp2 = a64::w2;
	armAsm->Mov(tmp2, RWSCRATCH);

	// reg = (norm >> 3) & 0x18
	armAsm->Lsr(RWSCRATCH, tmp2, 3);
	armAsm->And(RWSCRATCH, RWSCRATCH, 0x18);

	// tmp1 = (norm << 11) & 0x1800
	armAsm->Lsl(tmp1, tmp2, 11);
	armAsm->And(tmp1, tmp1, 0x1800);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, tmp1);

	// tmp2 = (norm << 14) & 0x3cf0000
	armAsm->Lsl(tmp2, tmp2, 14);
	armAsm->Mov(a64::w3, 0x3cf0000);
	armAsm->And(tmp2, tmp2, a64::w3);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, tmp2);

	// Store denormalized flag
	armAsm->Str(RWSCRATCH, armCpuRegMem(&_cpuRegistersPack.cop2Rec.denormStatusFlag));
}

// Emit code to normalize status flag from the cop2Rec.denormStatusFlag
// scratch back to VU0.VI[REG_STATUS_FLAG] — a full port of x86
// mVUallocSFLAGc, and a FULL REPLACE of VI: every normalized field derives
// from the denormalized value. Current Z/S come from denorm bits 8-15, sticky
// ZS/SS from denorm bits 0-7 (which the per-op RMWs accumulate into), and the
// whole D/I/O/U block (norm bits 2-5 current + 8-11 sticky) rides denorm bits
// 16+ shifted down by 14. Nothing is read from VI: under EP-4 lazy
// normalization the denorm scratch is the authoritative status between the
// chain's denormalize and this normalize, and VI is stale — an RMW against it
// (the pre-EP-4 shape) would resurrect values older than the chain, and would
// lose an intermediate live op's sticky contribution (pinned by
// EeVu0Cop2MacroLazyStatus.ChainStickyAccumulatesAcrossLiveOps).
//
// Interp-divergence note (EP-4): denorm bits 18-19 (current I/D) survive the
// FMAC RMW's 0xfffc00ff clear, so a DIV-unit result stays visible in the
// CURRENT field across later FMACs — matching x86, diverging from the
// interpreter's SYNCMSFLAGS (which preserves only 0xFC0, clearing current
// D/I/O/U on every macro FMAC). x86 JIT is the flag oracle per the standing
// rule; pinned by EeVu0Cop2MacroLazyStatus.DivCurrentDIBitsSurviveFmac.
// Current U/O (denorm bits 16-17) ARE cleared by every FMAC RMW, so they
// normalize back to 0 exactly as the interpreter's 0xFC0 preserve implies —
// the flag-tail stubs still never COMPUTE U/O (that gap stays latent, as
// before: no game in the corpus reads U/O after a COP2 macro FMAC).
static void cop2EmitNormalizeStatusFlag()
{
	// Load the denormalized flag. When the preceding flag-RMW just stored it,
	// this is a same-address str->ldr the store buffer forwards — the old
	// compile-time skip token (s_cop2DenormInScratch) was retired with the
	// S4-4 stubs, which always reload so the stub body is context-free.
	armAsm->Ldr(RWSCRATCH, armCpuRegMem(&_cpuRegistersPack.cop2Rec.denormStatusFlag));

	const a64::Register result = a64::w1;

	// Z bit (norm bit 0): any current zero lane (denorm bits 8-11)
	armAsm->Tst(RWSCRATCH, 0x0f00);
	armAsm->Cset(result, a64::ne);

	// S bit (norm bit 1): any current sign lane (denorm bits 12-15)
	armAsm->Tst(RWSCRATCH, 0xf000);
	armAsm->Cset(a64::w2, a64::ne);
	armAsm->Orr(result, result, a64::Operand(a64::w2, a64::LSL, 1));

	// ZS bit (norm bit 6): any sticky zero lane (denorm bits 0-3)
	armAsm->Tst(RWSCRATCH, 0x000f);
	armAsm->Cset(a64::w2, a64::ne);
	armAsm->Orr(result, result, a64::Operand(a64::w2, a64::LSL, 6));

	// SS bit (norm bit 7): any sticky sign lane (denorm bits 4-7)
	armAsm->Tst(RWSCRATCH, 0x00f0);
	armAsm->Cset(a64::w2, a64::ne);
	armAsm->Orr(result, result, a64::Operand(a64::w2, a64::LSL, 7));

	// D/I/O/U current + sticky: denorm bits 16-27 -> norm bits 2-13 (the
	// meaningful ones land in norm 2-5 and 8-11).
	armAsm->And(a64::w2, RWSCRATCH, 0xffff0000);
	armAsm->Orr(result, result, a64::Operand(a64::w2, a64::LSR, 14));

	armAsm->Str(result, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));
}


// MAC-flag liveness, same contract as cop2StatusFlagLive(): with vuFlagHack on,
// COP2FlagHackPass marks only the MAC writes a later CFC2 can observe — the
// last write before a CFC2 of REG_MAC_FLAG, and the last write in the block
// (CommitAllFlags). Unlike status, MAC is a plain overwrite (no sticky bits),
// so skipping an intermediate write is exact: only the surviving write's value
// is architecturally observable.
static bool cop2MacFlagLive()
{
	return !EmuConfig.Speedhacks.vuFlagHack || !g_pCurInstInfo || (g_pCurInstInfo->info & EEINST_COP2_MAC_FLAG);
}

// =========================================================================
//  S4-4: shared DynGen FMAC-tail stubs (clamp + flag update + status chain)
// =========================================================================
// The result clamp (4 insns), the MAC/status flag extraction (~20 insns) and
// the status-chain denormalize/normalize bodies (~12/16 insns) used to be
// re-emitted inline at EVERY hand-rolled COP2 macro arithmetic site. They
// were the top three repeated instruction shapes in the UYA hot-block corpus
// (S4 icache ledger: the ±FLT_MAX clamp alone appeared 1353x across the 121
// hot EE blocks) — the "hot-core emission density" residual vs AetherSX2.
// Both references emit this family once and BL to it (neither/LRPS2:
// emitSharedVUBody + shared flag pack, ee/Dispatcher.cpp; AetherSX2: shared
// mVUmacro stub family). This is that shape, on the S4-2 DynGen pattern.
// Per-site cost is now Mov-mask + BL (flags live) or a bare BL (clamp only).
//
// Contract (all stubs): callable only from EE recompiled code — the pinned
// bases (RSTATE, RVU0) must be live. Reached by BL (clobbers x30); preserve
// the VF cache (q16-q20), v8/v9, every pin and allocator-tracked register.
// Per-stub register effects:
//   ClampScratch    in/out q30 (clamped);       clobbers q29
//   ClampScratch2   in/out q31 (clamped);       clobbers q29
//   flag tails      in q30 = raw result, w1 = xyzw dest mask;
//                   out q30 clamped;            clobbers q27-q29, w2, w3, w8
//   Denorm          out w8 = denormalized status (also stored to the
//                   cop2Rec scratch);           clobbers w1-w3
//   Norm            clobbers w1, w2, w8
enum : int
{
	kCop2TailClampScratch, // clamp RQSCRATCH in place (flag-dead results, Fs operand copies)
	kCop2TailClampScratch2, // clamp RQSCRATCH2 in place (broadcast Ft operand)
	kCop2TailMacStatus, // clamp + MAC store + denorm-status RMW
	kCop2TailMacOnly, // clamp + MAC store
	kCop2TailStatusOnly, // clamp + denorm-status RMW
	kCop2TailDenorm, // denormalize VI[STATUS] -> cop2Rec scratch (chain start)
	kCop2TailNorm, // normalize cop2Rec scratch -> VI[STATUS] (chain end)
	kCop2TailStubCount
};
static const u8* s_cop2TailStubs[kCop2TailStubCount];

static const u8* cop2DynGenClampStub(const a64::VRegister& reg)
{
	const u8* start = armGetCurrentCodePointer();
	cop2ClampRegOneTmp(reg, RQSCRATCH3);
	armAsm->Ret();
	return start;
}

// Flag-tail stub body: clamp the result, then the mVUupdateFlags-equivalent
// MAC/status extraction on the CLAMPED value (same order as the old inline
// pair cop2ClampResultReg + cop2EmitFlagUpdate). The xyzw dest mask arrives
// in w1 at run time — one stub serves all 16 masks; lanes outside the mask
// have their flag bits cleared. Status feeds the cop2Rec.denormStatusFlag
// scratch (EP-4 lazy-normalization chain; protocol documented at
// endMacroOp_arm64). The macFlag value is computed even in the status-only
// variant — the status Z/S bits derive from the same lane extraction.
static const u8* cop2DynGenOneFlagTailStub(bool macLive, bool statusLive)
{
	const u8* start = armGetCurrentCodePointer();

	const a64::VRegister vWeights = a64::VRegister(27, 128);
	const a64::VRegister vSign = a64::VRegister(28, 128);
	const a64::Register signBits = RWARG3; // sign pack, then the MAC flag value
	const a64::Register zeroBits = RWARG4;

	cop2ClampRegOneTmp(RQSCRATCH, RQSCRATCH3);

	// Per-lane sign/zero masks from the clamped result (all-1s / 0 lanes).
	armAsm->Cmlt(vSign.V4S(), RQSCRATCH.V4S(), 0);
	armAsm->Fcmeq(RQSCRATCH3.V4S(), RQSCRATCH.V4S(), 0);

	// Pack both lane masks into GPRs in PS2 MAC flag order (bit0=W..bit3=X,
	// the reverse of NEON lane order — {8,4,2,1} weights). One weight-vector
	// load feeds both packs (the old per-site armEmitPackLaneBits pair loaded
	// it twice from the literal pool).
	armAsm->Ldr(vWeights, armCpuRegMem(&_cpuRegistersPack.cop2Rec.macPackWeightsRev));
	armAsm->And(vSign.V16B(), vSign.V16B(), vWeights.V16B());
	armAsm->Addv(a64::VRegister(vSign.GetCode(), 32), vSign.V4S());
	armAsm->Umov(signBits, vSign.V4S(), 0);
	armAsm->And(RQSCRATCH3.V16B(), RQSCRATCH3.V16B(), vWeights.V16B());
	armAsm->Addv(a64::VRegister(RQSCRATCH3.GetCode(), 32), RQSCRATCH3.V4S());
	armAsm->Umov(zeroBits, RQSCRATCH3.V4S(), 0);

	// Mask to the written lanes and build MAC = (sign << 4) | zero.
	armAsm->And(signBits, signBits, RWARG2);
	armAsm->And(zeroBits, zeroBits, RWARG2);
	armAsm->Lsl(signBits, signBits, 4);
	armAsm->Orr(signBits, signBits, zeroBits);

	if (macLive)
		armAsm->Str(signBits, armVU0Mem(&VU0.VI[REG_MAC_FLAG]));

	if (statusLive)
	{
		// Denorm-scratch RMW: clear current Z/S (bits 8-15) AND current U/O
		// (16-17), preserving current I/D (18-19, owned by the DIV-unit ops)
		// and every sticky bit — x86 mVUupdateFlags' doNonSticky clear.
		armAsm->Ldr(RWSCRATCH, armCpuRegMem(&_cpuRegistersPack.cop2Rec.denormStatusFlag));
		armAsm->And(RWSCRATCH, RWSCRATCH, 0xfffc00ff);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, signBits); // sticky bits 0-7 accumulate
		armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(signBits, a64::LSL, 8)); // current bits 8-15
		armAsm->Str(RWSCRATCH, armCpuRegMem(&_cpuRegistersPack.cop2Rec.denormStatusFlag));
	}

	armAsm->Ret();
	return start;
}

// Status-chain stubs: the denormalize/normalize bodies behind a Ret. The
// denorm stub returns with the denormalized value still live in RWSCRATCH
// (w8) — cop2EmitSyncFDiv's chain-start path consumes it directly.
static const u8* cop2DynGenStatusChainStub(bool denorm)
{
	const u8* start = armGetCurrentCodePointer();
	if (denorm)
		cop2EmitDenormalizeStatusFlag();
	else
		cop2EmitNormalizeStatusFlag();
	armAsm->Ret();
	return start;
}

void cop2DynGenTailStubs()
{
	s_cop2TailStubs[kCop2TailClampScratch] = cop2DynGenClampStub(RQSCRATCH);
	s_cop2TailStubs[kCop2TailClampScratch2] = cop2DynGenClampStub(RQSCRATCH2);
	s_cop2TailStubs[kCop2TailMacStatus] = cop2DynGenOneFlagTailStub(true, true);
	s_cop2TailStubs[kCop2TailMacOnly] = cop2DynGenOneFlagTailStub(true, false);
	s_cop2TailStubs[kCop2TailStatusOnly] = cop2DynGenOneFlagTailStub(false, true);
	s_cop2TailStubs[kCop2TailDenorm] = cop2DynGenStatusChainStub(true);
	s_cop2TailStubs[kCop2TailNorm] = cop2DynGenStatusChainStub(false);
}

// Emit a clamped COPY of `src` (a live VF-cache register — never mutated)
// into RQSCRATCH: the operand pre-clamp for the MUL/MADD-family cFs sites.
static void cop2EmitClampScratchCopy(const a64::VRegister& src)
{
	if (src.GetCode() != RQSCRATCH.GetCode())
		armAsm->Mov(RQSCRATCH.V16B(), src.V16B());
	armEmitCall(s_cop2TailStubs[kCop2TailClampScratch]);
}

// Emit the shared FMAC result tail: ±FLT_MAX clamp plus the flag-liveness-
// gated MAC/status update, via the DynGen stubs. The op's result MUST be in
// RQSCRATCH and is left there (clamped) for the dest-mask apply. flagXyzw is
// the dest mask used for FLAG lane masking — VOPMULA/VOPMSUB force 0xE
// regardless of the encoded field. With vuFlagHack on, an op whose flags are
// never consumed pays only the clamp BL.
static void cop2EmitResultTail(int flagXyzw)
{
	const bool statusLive = cop2StatusFlagLive();
	const bool macLive = cop2MacFlagLive();

	if (flagXyzw == 0 || (!statusLive && !macLive))
	{
		armEmitCall(s_cop2TailStubs[kCop2TailClampScratch]);
		return;
	}

	armAsm->Mov(RWARG2, flagXyzw);
	armEmitCall(s_cop2TailStubs[statusLive ? (macLive ? kCop2TailMacStatus : kCop2TailStatusOnly)
	                                       : kCop2TailMacOnly]);
}

// ========================================================================
//  COP2 Macro Mode Setup/Teardown
// ========================================================================
// ARM64 setupMacroOp/endMacroOp (see microVU_Macro.inl for the x86 version).
// Mode flags: 0x01=read Q, 0x02=write Q, 0x10=update status/MAC flags.

// cop2EmitConditionalSync is declared in iR5900-arm64.h (callable from
// recVTLB-arm64.cpp for LQC2/SQC2); definition is later in this file.

void setupMacroOp_arm64(int mode)
{
	// VU0 sync is gated on EEINST analysis (EEINST_COP2_SYNC_VU0 / FINISH_VU0).
	// In the common case where the analysis says no sync is needed, this emits
	// zero instructions (per-op recXXX gates sync via COP2_Interlock /
	// mVUSyncVU0 / mVUFinishVU0).
	cop2EmitConditionalSync(false, _vu0FinishMicro);

	if (mode & 0x10) // Status/MAC flags will be updated
	{
		// EP-4 lazy normalization: denormalize VU0's status into the
		// cop2Rec.denormStatusFlag scratch only at a chain START —
		// EEINST_COP2_DENORMALIZE_STATUS_FLAG, the x86 "first denormalizer"
		// marker (setupMacroOp, microVU_Macro.inl). Ops later in the chain
		// emit NOTHING here: the scratch is a memory slot that persists
		// across ops (x86 needs a VI-backup store in endMacroOp to park its
		// gprF0; we get persistence for free), and its dead-op protocol is
		// unchanged — a status-dead op skips its RMW, which is exact because
		// the slot simply carries the previous live value forward. The
		// matching normalize is gated on the chain-END mark in
		// endMacroOp_arm64; see cop2StatusDenormAtSetup for the seam list.
		if (cop2StatusDenormAtSetup())
			armEmitCall(s_cop2TailStubs[kCop2TailDenorm]);
	}

	if (mode & 0x01) // Q register will be read — load into RQSCRATCH3
	{
		// Q is loaded per-instruction by the Q-variant ops (ADDq etc.)
		// No global load needed here — the Q-variant ops load Q inline.
	}

	// microVU0 state setup so mVU-reuse wrappers (REC_COP2_mVU0_ARM64) can
	// drive mVU_LQI/SQI/MFIR/MTIR/... directly from macro-mode dispatch.
	// Hand-rolled arithmetic ops (recCOP2_VADDx etc.) don't read this state,
	// so unconditional setup is a cheap no-op cost for them.
	mVUmacroSetupCOP2State(mode, g_pCurInstInfo ? g_pCurInstInfo->info : 0u);
}

void endMacroOp_arm64(int mode)
{
	if (mode & 0x02) // Q register was written
	{
		// DIV/SQRT/RSQRT write Q inline — no global store needed here.
	}

	if (mode & 0x10) // Status/MAC flags were updated
	{
		// EP-4 lazy normalization: write VI[REG_STATUS_FLAG] back only at the
		// chain END — EEINST_COP2_NORMALIZE_STATUS_FLAG, which COP2FlagHackPass
		// places on the last status write before every consumer seam (CFC2/CTC2
		// of STATUS, VCALLMS, SB/SH/SW, block end via CommitAllFlags), so VI is
		// architecturally current whenever anything outside the chain can read
		// it. Mid-chain ops emit nothing here; VI stays stale and the denorm
		// scratch is authoritative (see cop2EmitNormalizeStatusFlag). With
		// vuFlagHack off both gates are always-true — per-op lockstep, the
		// pre-EP-4 shape.
		if (cop2StatusNormAtEnd())
			armEmitCall(s_cop2TailStubs[kCop2TailNorm]);
	}

	// microVU0 state teardown — flushPartialForCOP2 + cop2=0 + regAlloc reset.
	mVUmacroEndCOP2State();
}

// Macro for COP2 arithmetic ops that go through the setup/teardown pipeline.
// opFunc emits the actual NEON arithmetic + flag update.
#define REC_COP2_ARM64(f, mode) \
	void recCOP2_V##f() \
	{ \
		setupMacroOp_arm64(mode); \
		cop2Op_##f(); \
		endMacroOp_arm64(mode); \
	}

// ========================================================================
//  COP2 Transfer ops: QMFC2, QMTC2, CFC2, CTC2
// ========================================================================
// These move data between EE GPRs and VU0 registers.
// VU0 sync is conditional on VU0 actually running (VPU_STAT bit 0).
// Sync is skipped in the common case where VU0 micro isn't executing.

extern void _vu0FinishMicro();
extern void _vu0WaitMicro();

// VE-08: thin sync helpers for the rec-emitted COP2 sync sites below.
// Non-static so the recompiler tests can pin the contract.
//
// The emitted site (cop2EmitConditionalSync) has already
//   (a) checked VPU_STAT bit 0 (Tbz — kills the VU0-idle calls before they
//       get here; the generic _vu0run re-check is dropped), and
//   (b) flushed the absolute cpuRegs.cycle (armFlushCycleDelta).
// EE rec is running by construction — only rec-emitted code reaches these —
// so _vu0run's interp-only intUpdateCPUCycles probe is dead here too.
//
// Shape mirrors AetherSX2's unified vuSync(cpu, interlocked): a bare delta
// clamp + CpuVU0->Execute in tail position, no frame, no EmuConfig load
// (SD865 locked-60: aether's vuSync runs 0.22 Mcyc/f where our generic
// _vu0run specialization paid 0.60 for the same payload).
//
// The dispatch DECISIONS are bit-identical to _vu0run's sync path — same
// >= 0 gate, same 16-cycle floor. (Aether additionally skips delta == 0;
// that shifts VU0 run-ahead timing and moved the UYA stepdiff signature
// off the known-benign 0x0013d208 timer block, so it was dropped —
// wrapper thinning only, no timing change.) Pinned by EeVu0SyncThin.*.

// Exact catch-up (interlocked COP2 ops) — vu0Sync minus the wrapper.
void vu0SyncThin()
{
	const s32 runCycles = static_cast<s32>(static_cast<s64>(cpuRegs.cycle - VU0.cycle));
	if (runCycles >= 0)
		CpuVU0->Execute(runCycles);
}

// Non-interlocked catch-up with the 16-cycle run-ahead floor (mirrors
// _vu0run / upstream CalculateMinRunCycles — overshooting the EE is fine
// here; the next sync sees a negative delta and no-ops).
void vu0SyncRunAheadThin()
{
	const s32 runCycles = static_cast<s32>(static_cast<s64>(cpuRegs.cycle - VU0.cycle));
	if (runCycles >= 0)
		CpuVU0->Execute(runCycles < 16 ? 16 : runCycles);
}

// SL-2: seam preparation for the conditional VU0 sync below — the retain
// variant of iFlushCall(FLUSH_FREE_XMM | FLUSH_FREE_VU0) these sites used to
// pay. The C call sits behind the runtime VPU_STAT check (VU0 idle in the
// steady state), so evicting the whole caller-saved allocator on the
// UNCONDITIONAL path threw away residency the common path never had to lose:
//
//  - GPR/FPRC entries (incl. the loop-resident pins and block-resident
//    FCR31): KEPT mapped with NO writeback (S4-2). The shared sync stub
//    raw-preserves the caller-saved pool registers around the C calls, so
//    the values survive both paths in-register. Sound for the same reason
//    the old writeback-keep + reload was: the VU0-sync callees
//    (vu0SyncThin/RunAheadThin/_vu0FinishMicro/_vu0WaitMicro →
//    CpuVU0->Execute) have no path that reads OR writes EE GPRs or fprc —
//    stale canonical memory during the call is unobservable.
//  - VIREG entries are freed WITH writeback: VU0 execution writes VU0.VI, so
//    a retained VI mirror would go stale across the call.
//  - TEMP / PCWRITEBACK entries are freed (transient, no reloadable home).
//  - NEON: same free policy as FLUSH_FREE_XMM — 128-bit classes can't ride a
//    C call and the macro body that follows wants the file to itself. The VF
//    compile cache (q16-q20) dies at any C seam.
static void cop2FlushForConditionalSync()
{
	cop2VfCacheFlush();

	for (int i = 0; i < NUM_ARM_NEON_REGS; i++)
	{
		if (arm64neon[i].inuse)
			_freeNEONreg(i);
	}

	for (int i = 0; i < NUM_ARM_GPR_REGS; i++)
	{
		if (!arm64gprs[i].inuse || armIsCalleeSavedRegister(i))
			continue;
		if (arm64gprs[i].type == ARM64TYPE_GPR || arm64gprs[i].type == ARM64TYPE_FPRC)
			continue; // retained — the sync stub raw-preserves the pool regs
		_freeArm64GPR(i); // VIREG (writeback) / TEMP / PCWRITEBACK
	}
}

// =========================================================================
//  S4-2: shared DynGen VU0-sync stubs
// =========================================================================
// The seam body (VPU_STAT gate + cycle flush/reload + pin flush/reload +
// the C calls) used to be re-emitted inline at EVERY analysis-marked COP2
// site — 15-25 insns each, the fattest per-site byte carrier in COP2-dense
// hot blocks (S4 icache ledger). AetherSX2 4248 emits it ONCE per recResetEE
// and BLs to it from a 3-insn site (mVUmacroEmitCOP2_0/1 → the 0x2a506d8
// stub family); this is that shape. Per-site cost is now Add-cycles + BL.
//
// Contract (site side): emitted only after cop2FlushForConditionalSync(),
// with retained GPR/FPRC entries still mapped in the caller-saved pool regs.
// Clobbers x8 and x16/x17 (like any BL); preserves everything else on both
// paths. Relies on the EE-block pinned bases (RSTATE, RECCYCLE, x24=&VU0)
// being live — callable only from EE recompiled code.
//
// Fast path (VPU_STAT bit 0 clear — VU0 idle): Ldr + Tbnz + Ret.
// Sync path: raw-save LR + the caller-saved EE int-allocator pool regs
// (x4-x7/x14/x15 — where retained GPR/FPRC values live), publish the
// absolute cycle, flush the lazy-dirty caller-saved pins, run the sync
// callee(s), re-derive the cycle delta, reload pins, restore, Ret.

enum : int
{
	kCop2SyncStubSyncFinish, // vu0SyncThin + _vu0FinishMicro (interlocked op)
	kCop2SyncStubSyncWait, // vu0SyncThin + _vu0WaitMicro (interlocked QMTC2/CTC2)
	kCop2SyncStubSyncExact, // vu0SyncThin (non-interlock sync in an interlocked block)
	kCop2SyncStubSyncRunAhead, // vu0SyncRunAheadThin (non-interlock sync)
	kCop2SyncStubFinish, // _vu0FinishMicro (finish-only)
	kCop2SyncStubCount
};
static const u8* s_cop2SyncStubs[kCop2SyncStubCount];

static const u8* cop2DynGenOneSyncStub(void (*syncFn)(), void (*finishFn)())
{
	const u8* start = armGetCurrentCodePointer();

	a64::Label doSync;
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_VPU_STAT]));
	armAsm->Tbnz(RWSCRATCH, 0, &doSync);
	armAsm->Ret();

	armAsm->Bind(&doSync);
	armAsm->Stp(a64::x4, a64::x5, a64::MemOperand(a64::sp, -64, a64::PreIndex));
	armAsm->Stp(a64::x6, a64::x7, a64::MemOperand(a64::sp, 16));
	armAsm->Stp(a64::x14, a64::x15, a64::MemOperand(a64::sp, 32));
	armAsm->Str(a64::x30, a64::MemOperand(a64::sp, 48));

	// Publish the absolute cycle before the sync — the callees read
	// cpuRegs.cycle to determine how many VU0 micro cycles to run — and
	// flush the lazy-dirty caller-saved pins before the first call clobbers
	// them (pairs with the reloads below).
	armFlushCycleDelta();
	armFlushEEClobberedPins();

	if (syncFn)
		armEmitCall((void*)syncFn);
	if (finishFn)
		armEmitCall((void*)finishFn);

	// Re-derive the cycle delta (the callees advance cpuRegs.cycle and can
	// reschedule nextEventCycle) and restore the caller-saved pins. The
	// callees write VU state, not EE GPRs.
	armReloadCycleDelta();
	armReloadEEClobberedPins();

	armAsm->Ldr(a64::x30, a64::MemOperand(a64::sp, 48));
	armAsm->Ldp(a64::x14, a64::x15, a64::MemOperand(a64::sp, 32));
	armAsm->Ldp(a64::x6, a64::x7, a64::MemOperand(a64::sp, 16));
	armAsm->Ldp(a64::x4, a64::x5, a64::MemOperand(a64::sp, 64, a64::PostIndex));
	armAsm->Ret();

	return start;
}

void cop2DynGenSyncStubs()
{
	s_cop2SyncStubs[kCop2SyncStubSyncFinish] = cop2DynGenOneSyncStub(vu0SyncThin, _vu0FinishMicro);
	s_cop2SyncStubs[kCop2SyncStubSyncWait] = cop2DynGenOneSyncStub(vu0SyncThin, _vu0WaitMicro);
	s_cop2SyncStubs[kCop2SyncStubSyncExact] = cop2DynGenOneSyncStub(vu0SyncThin, nullptr);
	s_cop2SyncStubs[kCop2SyncStubSyncRunAhead] = cop2DynGenOneSyncStub(vu0SyncRunAheadThin, nullptr);
	s_cop2SyncStubs[kCop2SyncStubFinish] = cop2DynGenOneSyncStub(nullptr, _vu0FinishMicro);
}

// Emit conditional VU0 sync: uses EEINST analysis flags when available,
// falls back to runtime VPU_STAT check otherwise.
// Implements the COP2_Interlock + mVUSyncVU0/mVUFinishVU0 sync protocol.
void cop2EmitConditionalSync(bool interlock, void (*finishFunc)())
{
	// Handle interlock (bit 0 set): COP2_Interlock pattern
	if (interlock)
	{
		// An interlocked COP2 op anywhere in the block means VU0 timing must be
		// exact: forbid the non-interlock run-ahead for the rest of the block so
		// a later sync can't overshoot the cycle this interlock waits on. Mirrors
		// upstream's block-level s_nBlockInterlocked (set in COP2_Interlock).
		s_nBlockInterlocked = true;

		// Interlock requires sync — check if analysis says VU0 could be running
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
		{
			// SL-2 retain seam (was iFlushCall(FLUSH_FREE_XMM|FLUSH_FREE_VU0)):
			// keep GPR/FPRC mapped, free NEON/VI/temps — see the helper.
			cop2FlushForConditionalSync();

			// Apply block cycles to RECCYCLE (the pinned cycle delta).
			const u32 cycles = scaleblockcycles_clear();
			if (cycles != 0)
				armAsm->Add(RECCYCLE, RECCYCLE, cycles);

			int stub = kCop2SyncStubSyncExact;
			if (finishFunc == &_vu0FinishMicro)
				stub = kCop2SyncStubSyncFinish;
			else if (finishFunc == &_vu0WaitMicro)
				stub = kCop2SyncStubSyncWait;
			else
				pxAssert(!finishFunc);
			armEmitCall(s_cop2SyncStubs[stub]);
		}
		// else: analysis says no VU0 program between COP2 ops, safe to skip
		return;
	}

	// Non-interlock: check analysis flags for sync/finish
	const bool needsSync = (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0) != 0;
	const bool needsFinish = (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0) != 0;

	if (!needsSync && !needsFinish)
		return; // Analysis says no sync needed

	// SL-2 retain seam — see the interlock branch above.
	cop2FlushForConditionalSync();

	const u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	if (needsSync)
	{
		// Non-interlocked catch-up: run a 16-cycle minimum to amortize the mVU
		// dispatch envelope over small blocks (6dc5087cb). If the block also
		// contains an interlocked op, fall back to the exact sync.
		armEmitCall(s_cop2SyncStubs[s_nBlockInterlocked ? kCop2SyncStubSyncExact : kCop2SyncStubSyncRunAhead]);
	}
	else
	{
		armEmitCall(s_cop2SyncStubs[kCop2SyncStubFinish]);
	}
}

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

// QMFC2: cpuRegs.GPR[rt] = VU0.VF[fs] (128-bit copy, VF → EE GPR)
//
// S4-1: allocator-routed, no block-wide flush (x86 recQMFC2 / AetherSX2
// recQMFC2 shape — aether's transfer ops emit no flush at all in the
// no-sync path; the only remaining flush point is the analysis-gated sync
// seam inside cop2EmitConditionalSync). The old unconditional
// iFlushCall(FLUSH_EVERYTHING) here was ~half the emitted bytes of every
// COP2-heavy physics block (S4b: 23% of the #1 UYA block was q-class
// GPR<->memory round-trips these seams forced).
void recCOP2_QMFC2()
{
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0FinishMicro);

	if (_Rt_ == 0) return;

	if (EEINST_USEDTEST(_Rt_))
	{
		// rt is read again later: claim a NEON quad MODE_WRITE (frees any
		// scalar slot / const without a pointless writeback — the full 128
		// bits are overwritten) and load VF straight into it. The value
		// stays q-resident for following MMI/QMTC2 consumers.
		const int qd = _allocGPRtoNEONreg(_Rt_, MODE_WRITE);
		armAsm->Ldr(armQRegister(qd), armVU0Mem(&VU0.VF[_Rd_]));
	}
	else
	{
		// Dead-after dest: store straight to the canonical image instead of
		// occupying a quad slot (mirrors x86 _allocIfUsedGPRtoXMM's miss path).
		_deleteEEreg128(_Rt_);
		armAsm->Ldr(RQSCRATCH, armVU0Mem(&VU0.VF[_Rd_]));
		armStoreEEGPRQuad(RQSCRATCH, _Rt_);
	}
}

// QMTC2: VU0.VF[fs] = cpuRegs.GPR[rt] (128-bit copy, EE GPR → VF)
//
// S4-1: no block-wide flush (see recCOP2_QMFC2). Source policy mirrors x86
// recQMTC2: force a quad FILL only when the newest rt lives where a raw
// memory read can't see it (dirty const / dirty scalar slot — the fill path
// materializes the const or Ins-merges the slot); otherwise serve from an
// already-resident quad (an MMI result costs zero extra loads), and on a
// clean miss read memory + merge the lazy pin WITHOUT claiming a slot —
// a fresh alloc for a once-read source costs more (deferred writeback +
// eviction pressure in q10-q15) than the 2-3-insn memory shape. Measured:
// unconditional alloc grew several UYA physics blocks up to +120 B.
void recCOP2_QMTC2()
{
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0WaitMicro);

	if (_Rd_ == 0) return; // VF[0] is read-only

	int qs;
	if (GPR_IS_DIRTY_CONST(_Rt_) || _hasArm64GPR(ARM64TYPE_GPR, _Rt_, MODE_WRITE))
		qs = _allocGPRtoNEONreg(_Rt_, MODE_READ);
	else
		qs = _checkNEONreg(NEONTYPE_GPRREG, _Rt_, MODE_READ);
	if (qs >= 0)
	{
		armAsm->Str(armQRegister(qs), armVU0Mem(&VU0.VF[_Rd_]));
		return;
	}

	if (_Rt_ == 0)
	{
		armAsm->Movi(RQSCRATCH.V2D(), 0);
	}
	else
	{
		// Covers the clean-const case too: a non-dirty const is by definition
		// already flushed, so canonical memory is current for the lower 64
		// (and the upper 64 only ever live in memory).
		armAsm->Ldr(RQSCRATCH, armCpuRegMem(&cpuRegs.GPR.r[_Rt_]));
		armMergeEEResidentIntoQuad(RQSCRATCH, _Rt_); // lazy-dirty pin merge
	}
	armAsm->Str(RQSCRATCH, armVU0Mem(&VU0.VF[_Rd_]));
}

// CFC2: cpuRegs.GPR[rt] = sign_extend_32_to_64(VU0.VI[fs])
//
// S4-1: no block-wide flush (see recCOP2_QMFC2). The general path was
// already allocator-coherent via the dest helpers; only the REG_R partial
// write needs an explicit per-register flush.
void recCOP2_CFC2()
{
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0FinishMicro);

	if (_Rt_ == 0) return;

	if (_Rd_ == REG_R)
	{
		// REG_R: mask to 23 bits, write only UL[0]. This is a PARTIAL lower-64
		// write (UL[1] untouched), which the full-width dest helper can't
		// model; flush rt's residency with writeback (the untouched UL[1] /
		// UD[1] bytes must be current in memory) so the raw pin-aware store
		// merges into current bytes. NOTE: preserving UL[1] is the interp
		// contract our tests pin — x86 recCFC2 zero-extends the full 64 bits
		// here instead, a known upstream divergence we deliberately don't copy.
		_deleteEEreg(_Rt_, 1);
		armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_R]));
		armAsm->And(RWSCRATCH, RWSCRATCH, 0x7FFFFF);
		armStoreEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
	}
	else
	{
		// General VI: rt = sign_extend_32_to_64(VI[fs]). _eeGetGPRDestReg
		// kills const/NEON residency (NEON with writeback, so UD[1] stays
		// current) and resolves pin/resident-slot/memory.
		armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[_Rd_]));
		const a64::Register dst = _eeGetGPRDestReg(_Rt_, RXSCRATCH);
		armAsm->Sxtw(dst, RWSCRATCH);
		_eeStoreGPRDestReg(_Rt_, dst);
	}
}

// CTC2: cpuRegs.GPR[rt] → VU0.VI[fs] (with special-case registers)
// _Fs_ is known at compile time, so dispatch happens at compile time.
// FBRST and CMSAR1 fall back to interpreter (complex side effects).
// CTC2() is in global namespace (VU0.cpp), referenced via ::CTC2.

void recCOP2_CTC2()
{
	const int fs = _Rd_; // _Fs_ in VU encoding = _Rd_ in EE encoding

	if (fs == 0) return; // VI[0] is read-only

	// Read-only registers — no-op
	if (fs == REG_MAC_FLAG || fs == REG_TPC || fs == REG_VPU_STAT)
		return;

	// FBRST and CMSAR1 have complex side effects — use interpreter
	if (fs == REG_FBRST || fs == REG_CMSAR1)
	{
		recCall(::CTC2);
		return;
	}

	// For all other cases: conditional sync, then inline write. S4-1: no
	// block-wide flush — the body reads rt coherently via _eeMoveGPRtoR
	// (const/scalar/quad/pin aware) and writes only VU0 state; its raw w1/w2/
	// w3/w9 scratch is outside the EE allocator pool by the GE-M2 carve-out.
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0WaitMicro);

	// Load source value from cpuRegs.GPR[rt].UL[0]
	if (GPR_IS_CONST1(_Rt_))
	{
		armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	}
	else
	{
		// Coherent move into RWSCRATCH (pin mirror / resident slot / memory);
		// the per-fs masking below is RWSCRATCH-based.
		_eeMoveGPRtoR(RWSCRATCH, _Rt_);
	}

	if (fs == REG_R)
	{
		// REG_R: (value & 0x7FFFFF) | 0x3F800000
		armAsm->And(RWSCRATCH, RWSCRATCH, 0x7FFFFF);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x3F800000);
		armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.VI[REG_R]));
	}
	else if (fs == REG_CLIP_FLAG)
	{
		// REG_CLIP_FLAG: write to both clipflag and VI
		armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.clipflag));
		armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.VI[REG_CLIP_FLAG]));
	}
	else if (fs == REG_STATUS_FLAG)
	{
		// STATUS_FLAG: take only the 0xFC0 field from the GPR, preserve the
		// low-6 sticky bits in VI[STATUS], then denormalize the result
		// (mVUallocSFLAGd) and broadcast it into all four lanes of
		// micro_statusflags — microVU reads that array for flag sync, so a raw
		// 32-bit overwrite of VI[STATUS] alone leaves it stale and corrupts VU
		// flag state. RWSCRATCH = GPR[_Rt_].UL[0] here (== 0 for _Rt_==0, so
		// the RMW degrades to STATUS &= 0x3F).
		armAsm->And(RWSCRATCH, RWSCRATCH, 0xFC0); // masked field from GPR

		armAsm->Ldr(RWARG2, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));
		armAsm->And(RWARG2, RWARG2, 0x3F);        // preserve sticky bits 0-5
		armAsm->Orr(RWARG2, RWARG2, RWSCRATCH);   // RWARG2 = new normalized STATUS
		armAsm->Str(RWARG2, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));

		// Denormalize the new STATUS (in RWARG2) into RWSCRATCH:
		//   denorm = ((s>>3)&0x18) | ((s<<11)&0x1800) | ((s<<14)&0x3cf0000)
		armAsm->Lsr(RWSCRATCH, RWARG2, 3);
		armAsm->And(RWSCRATCH, RWSCRATCH, 0x18);
		armAsm->Lsl(a64::w2, RWARG2, 11);
		armAsm->And(a64::w2, a64::w2, 0x1800);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::w2);
		armAsm->Lsl(a64::w3, RWARG2, 14);
		armAsm->Mov(a64::w9, 0x3cf0000); // not a valid logical-imm; materialize (w9: reserved scratch — w4 is allocatable)
		armAsm->And(a64::w3, a64::w3, a64::w9);
		armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::w3);

		// Broadcast the denormalized value into all 4 lanes of micro_statusflags.
		armAsm->Dup(RQSCRATCH.V4S(), RWSCRATCH);
		armAsm->Str(RQSCRATCH, armVU0Mem(&VU0.micro_statusflags));
	}
	else if (fs < REG_STATUS_FLAG)
	{
		// Integer VIs (1-15) are physically 16-bit; the micro JIT reads/writes
		// them as 16-bit, so a 32-bit store would leave stale upper bits that a
		// later CFC2 (.UL) reads back. Store only the low 16 bits, matching x86
		// recCTC2 (upstream a7af3cd48). NOTE: this is a deliberate, hardware-
		// correct JIT-vs-interp divergence — the shared interp CTC2 stores the
		// full 32 bits.
		armAsm->Strh(RWSCRATCH, armVU0Mem(&VU0.VI[fs]));
	}
	else
	{
		// Control VIs (>= REG_STATUS_FLAG) reaching the default: full 32-bit.
		armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.VI[fs]));
	}
}

// ========================================================================
//  COP2 Integer ops: IADD, ISUB, IADDI, IAND, IOR
// ========================================================================
// 16-bit VI register operations. VU field encoding:
//   _Id_ = _Sa_ & 0xF (destination VI), _Is_ = _Rd_ & 0xF, _It_ = _Rt_ & 0xF

#define _Id_cop2 (_Sa_ & 0xF)
#define _Is_cop2 (_Rd_ & 0xF)
#define _It_cop2 (_Rt_ & 0xF)

// IADD: VI[id] = VI[is] + VI[it]
void recCOP2_VIADD()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Id_cop2 == 0) return;

	armAsm->Ldrsh(RWSCRATCH, armVU0Mem(&VU0.VI[_Is_cop2]));
	armAsm->Ldrsh(RWARG2, armVU0Mem(&VU0.VI[_It_cop2]));
	armAsm->Add(RWSCRATCH, RWSCRATCH, RWARG2);
	armAsm->Strh(RWSCRATCH, armVU0Mem(&VU0.VI[_Id_cop2]));
}

// ISUB: VI[id] = VI[is] - VI[it]
void recCOP2_VISUB()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Id_cop2 == 0) return;

	armAsm->Ldrsh(RWSCRATCH, armVU0Mem(&VU0.VI[_Is_cop2]));
	armAsm->Ldrsh(RWARG2, armVU0Mem(&VU0.VI[_It_cop2]));
	armAsm->Sub(RWSCRATCH, RWSCRATCH, RWARG2);
	armAsm->Strh(RWSCRATCH, armVU0Mem(&VU0.VI[_Id_cop2]));
}

// IADDI: VI[it] = VI[is] + sign_ext_5bit_imm
void recCOP2_VIADDI()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_It_cop2 == 0) return;

	// 5-bit immediate at bits 10-6, sign-extended
	s16 imm = ((_Sa_ & 0x1F));
	imm = ((imm & 0x10) ? (s16)(0xFFF0 | imm) : imm);

	armAsm->Ldrsh(RWSCRATCH, armVU0Mem(&VU0.VI[_Is_cop2]));
	armAsm->Add(RWSCRATCH, RWSCRATCH, imm);
	armAsm->Strh(RWSCRATCH, armVU0Mem(&VU0.VI[_It_cop2]));
}

// IAND: VI[id] = VI[is] & VI[it]
void recCOP2_VIAND()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Id_cop2 == 0) return;

	armAsm->Ldrh(RWSCRATCH, armVU0Mem(&VU0.VI[_Is_cop2]));
	armAsm->Ldrh(RWARG2, armVU0Mem(&VU0.VI[_It_cop2]));
	armAsm->And(RWSCRATCH, RWSCRATCH, RWARG2);
	armAsm->Strh(RWSCRATCH, armVU0Mem(&VU0.VI[_Id_cop2]));
}

// IOR: VI[id] = VI[is] | VI[it]
void recCOP2_VIOR()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Id_cop2 == 0) return;

	armAsm->Ldrh(RWSCRATCH, armVU0Mem(&VU0.VI[_Is_cop2]));
	armAsm->Ldrh(RWARG2, armVU0Mem(&VU0.VI[_It_cop2]));
	armAsm->Orr(RWSCRATCH, RWSCRATCH, RWARG2);
	armAsm->Strh(RWSCRATCH, armVU0Mem(&VU0.VI[_Id_cop2]));
}

// ========================================================================
//  SIMPLE template: VMOVE, VMR32, VNOP, VWAITQ, VABS
// ========================================================================

// VMOVE: VF[ft] = VF[fs] (masked by dest)
void recCOP2_VMOVE()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return; // VF0 is read-only

	const int xyzw = _XYZW_cop2;
	if (xyzw == 0) return;

	// VMOVE fx,fx moves a value onto itself — architecturally a no-op for any
	// dest mask. It must ALSO bail here for cache correctness: cop2ResultReg
	// claims ft no-fill, and with fs == ft the ViaCache lookup below would hit
	// that just-claimed (uninitialized) slot and skip the load.
	if (_Fs_cop2 == _Ft_cop2) return;

	const a64::VRegister src = cop2GetVF(_Fs_cop2);
	const a64::VRegister rd = cop2ResultReg(_Ft_cop2, _XYZW_cop2);
	if (rd.GetCode() != src.GetCode())
		armAsm->Mov(rd.V16B(), src.V16B());
	cop2ApplyDestMaskExplicit(_Ft_cop2, _XYZW_cop2, rd);
}

// VMR32: rotate VF[fs] lanes right by one, store to VF[ft] (masked)
// x=y, y=z, z=w, w=x (rotate left in element order)
void recCOP2_VMR32()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	const int xyzw = _XYZW_cop2;
	if (xyzw == 0) return;

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister rd = cop2ResultReg(_Ft_cop2, _XYZW_cop2);
	// EXT rotates: target lane order is [y,z,w,x] from [x,y,z,w]
	// That's a left rotation by 1 lane = EXT #4 (4 bytes)
	armAsm->Ext(rd.V16B(), fs.V16B(), fs.V16B(), 4);
	cop2ApplyDestMaskExplicit(_Ft_cop2, _XYZW_cop2, rd);
}

// VNOP: no operation. Still consumes the analysis sync mark: x86 syncs every
// COP2-CO special op in the recCOP2_SPEC1 dispatch wrapper, and the
// COP2MicroFinishPass clears its pending state when it places the mark — an
// op that drops it leaves the rest of the block unsynced.
void recCOP2_VNOP()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
}

// VWAITQ: wait for Q register (no-op in macro mode; sync mark as VNOP)
void recCOP2_VWAITQ()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
}

// VABS: VF[ft] = abs(VF[fs]) (masked)
void recCOP2_VABS()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	const int xyzw = _XYZW_cop2;
	if (xyzw == 0) return;

	const a64::VRegister src = cop2GetVF(_Fs_cop2);
	const a64::VRegister rd = cop2ResultReg(_Ft_cop2, _XYZW_cop2);
	armAsm->Fabs(rd.V4S(), src.V4S());
	cop2ApplyDestMaskExplicit(_Ft_cop2, _XYZW_cop2, rd);
}

// ========================================================================
//  VEC_ARITH template: VADD, VSUB, VMUL
//  Pattern: VF[fd] = VF[fs] OP VF[ft] (masked by dest)
// ========================================================================

// S4-4 note (applies to every FMAC body below): the arithmetic computes into
// RQSCRATCH and the clamp/flag tail is a BL to the shared stubs
// (cop2EmitResultTail). Full-mask writes then pay one Mov into the fd cache
// slot at the dest-mask apply — the pre-S4-4 compute-in-slot trick
// (cop2ResultReg) is not worth its cost here: keeping it would force the
// tail stub to handle a variable result register. cop2ResultReg stays in use
// for the tail-less ops (VMOVE/VMR32/VABS/ITOF).

void recCOP2_VADD()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	armAsm->Fadd(RQSCRATCH.V4S(), fs.V4S(), ft.V4S());
	cop2EmitResultTail(_XYZW_cop2);
	cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VSUB()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	armAsm->Fsub(RQSCRATCH.V4S(), fs.V4S(), ft.V4S());
	cop2EmitResultTail(_XYZW_cop2);
	cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VMUL()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), ft.V4S());
	cop2EmitResultTail(_XYZW_cop2);
	cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VMAX()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Fd_cop2 == 0) return;

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	cop2EmitIntegerMax(fs, ft);
	cop2ApplyDestMask(_Fd_cop2);
}

void recCOP2_VMINI()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Fd_cop2 == 0) return;

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	cop2EmitIntegerMin(fs, ft);
	cop2ApplyDestMask(_Fd_cop2);
}

// ========================================================================
//  Broadcast helpers for _BC variants
// ========================================================================

// Load VF[ft] and broadcast lane 'bc' (0=x, 1=y, 2=z, 3=w) to all lanes
static void cop2LoadBroadcast(const a64::VRegister& qreg, int vfReg, int bc)
{
	armAsm->Dup(qreg.V4S(), cop2GetVF(vfReg).V4S(), bc);
}

// ========================================================================
//  ADD_BC / SUB_BC / MUL_BC template
//  Pattern: VF[fd] = VF[fs] OP VF[ft].bc (broadcast one lane)
// ========================================================================

// Helper macro for broadcast binary ops (with input/output clamping + flags).
// mulClamp=true pre-clamps the FMAC operands per mVU_MULx cFs/cFt (MUL family);
// ADD/SUB pass false (ADD clampType=0; SUB's input clamp is a separate concern).
#define COP2_BC_OP(name, neonOp, bc, mulClamp) \
	void recCOP2_V##name() \
	{ \
		if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return; \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		a64::VRegister mulA = fs; \
		if (mulClamp) \
		{ \
			cop2EmitClampScratchCopy(fs); \
			mulA = RQSCRATCH; \
			if (_XYZW_cop2 == 0xf) \
				armEmitCall(s_cop2TailStubs[kCop2TailClampScratch2]); \
		} \
		armAsm->neonOp(RQSCRATCH.V4S(), mulA.V4S(), RQSCRATCH2.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2); \
		endMacroOp_arm64(0x110); \
	}

// ADDx/y/z/w
COP2_BC_OP(ADDx, Fadd, 0, false)
COP2_BC_OP(ADDy, Fadd, 1, false)
COP2_BC_OP(ADDz, Fadd, 2, false)
COP2_BC_OP(ADDw, Fadd, 3, false)

// SUBx/y/z/w
COP2_BC_OP(SUBx, Fsub, 0, false)
COP2_BC_OP(SUBy, Fsub, 1, false)
COP2_BC_OP(SUBz, Fsub, 2, false)
COP2_BC_OP(SUBw, Fsub, 3, false)

// MULx/y/z/w — pre-clamp Fs (and Ft on full mask) per mVU_MULx cFs/cFt spec
COP2_BC_OP(MULx, Fmul, 0, true)
COP2_BC_OP(MULy, Fmul, 1, true)
COP2_BC_OP(MULz, Fmul, 2, true)
COP2_BC_OP(MULw, Fmul, 3, true)

// MAXx/y/z/w — PS2 integer comparison, not IEEE FMAX
#define COP2_BC_MAX(name, bc) \
	void recCOP2_V##name() \
	{ \
		cop2EmitConditionalSync(false, _vu0FinishMicro); \
		if (_Fd_cop2 == 0) return; \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		cop2EmitIntegerMax(fs, RQSCRATCH2); \
		cop2ApplyDestMask(_Fd_cop2); \
	}

// MINIx/y/z/w — PS2 integer comparison, not IEEE FMIN
#define COP2_BC_MINI(name, bc) \
	void recCOP2_V##name() \
	{ \
		cop2EmitConditionalSync(false, _vu0FinishMicro); \
		if (_Fd_cop2 == 0) return; \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		cop2EmitIntegerMin(fs, RQSCRATCH2); \
		cop2ApplyDestMask(_Fd_cop2); \
	}

COP2_BC_MAX(MAXx, 0)
COP2_BC_MAX(MAXy, 1)
COP2_BC_MAX(MAXz, 2)
COP2_BC_MAX(MAXw, 3)

COP2_BC_MINI(MINIx, 0)
COP2_BC_MINI(MINIy, 1)
COP2_BC_MINI(MINIz, 2)
COP2_BC_MINI(MINIw, 3)

// MAXi/MINIi — broadcast I register, PS2 integer comparison
void recCOP2_VMAXi()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Fd_cop2 == 0) return;

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]);
	cop2EmitIntegerMax(fs, RQSCRATCH2);
	cop2ApplyDestMask(_Fd_cop2);
}

void recCOP2_VMINIi()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Fd_cop2 == 0) return;

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]);
	cop2EmitIntegerMin(fs, RQSCRATCH2);
	cop2ApplyDestMask(_Fd_cop2);
}

// ========================================================================
//  ADDq/SUBq/MULq — broadcast Q register
// ========================================================================

#define COP2_Q_OP(name, neonOp) \
	void recCOP2_V##name() \
	{ \
		if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return; \
		setupMacroOp_arm64(0x111); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->neonOp(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2); \
		endMacroOp_arm64(0x111); \
	}

COP2_Q_OP(ADDq, Fadd)
COP2_Q_OP(SUBq, Fsub)
COP2_Q_OP(MULq, Fmul)

// ADDi/SUBi/MULi — broadcast I register
#define COP2_I_OP(name, neonOp) \
	void recCOP2_V##name() \
	{ \
		if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return; \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->neonOp(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2); \
		endMacroOp_arm64(0x110); \
	}

COP2_I_OP(ADDi, Fadd)
COP2_I_OP(SUBi, Fsub)
COP2_I_OP(MULi, Fmul)

// ========================================================================
//  MADD/MSUB variants: VF[fd] = ACC ± VF[fs] * VF[ft]
// ========================================================================

// MADD/MSUB use separate FMUL+FADD/FSUB (not FMLA/FMLS) to match PS2 VU
// intermediate rounding. PS2 rounds the multiply result before adding to ACC.

void recCOP2_VMADD()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), ft.V4S());
	const a64::VRegister acc = cop2GetACC();
	armAsm->Fadd(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S());
	cop2EmitResultTail(_XYZW_cop2);
	cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VMSUB()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), ft.V4S());
	const a64::VRegister acc = cop2GetACC();
	armAsm->Fsub(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S());
	cop2EmitResultTail(_XYZW_cop2);
	cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2);

	endMacroOp_arm64(0x110);
}

// MADD/MSUB broadcast variants: separate FMUL + FADD/FSUB.
//
// MADDx/y/z/w pre-clamp Fs before the multiply (clampFs=true): mVU_MADDx passes
// cFs, and the interpreter routes Fs through vuDouble, so an Inf/NaN Fs against
// a zero broadcast Ft must become FLT_MAX*0 = 0 rather than Inf*0 = NaN folded
// to +/-FLT_MAX by the result clamp. MSUBx/y/z/w use mVU_FMACd (clampType=0,
// no cFs) — that Fs divergence is shared/by-design, so MSUB keeps clampFs=false.
// The MADDw extras (cACC|cFt) are a separate concern.
#define COP2_MADD_BC(name, addOp, bc, clampFs) \
	void recCOP2_V##name() \
	{ \
		if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return; \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		a64::VRegister mulA = fs; \
		if (clampFs) \
		{ \
			cop2EmitClampScratchCopy(fs); \
			mulA = RQSCRATCH; \
		} \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		armAsm->Fmul(RQSCRATCH.V4S(), mulA.V4S(), RQSCRATCH2.V4S()); \
		const a64::VRegister acc = cop2GetACC(); \
		armAsm->addOp(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2); \
		endMacroOp_arm64(0x110); \
	}

COP2_MADD_BC(MADDx, Fadd, 0, true)
COP2_MADD_BC(MADDy, Fadd, 1, true)
COP2_MADD_BC(MADDz, Fadd, 2, true)
COP2_MADD_BC(MADDw, Fadd, 3, true)

COP2_MADD_BC(MSUBx, Fsub, 0, false)
COP2_MADD_BC(MSUBy, Fsub, 1, false)
COP2_MADD_BC(MSUBz, Fsub, 2, false)
COP2_MADD_BC(MSUBw, Fsub, 3, false)

// MADDq/MSUBq — broadcast Q
#define COP2_MADD_Q(name, addOp) \
	void recCOP2_V##name() \
	{ \
		if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return; \
		setupMacroOp_arm64(0x111); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		const a64::VRegister acc = cop2GetACC(); \
		armAsm->addOp(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2); \
		endMacroOp_arm64(0x111); \
	}

COP2_MADD_Q(MADDq, Fadd)
COP2_MADD_Q(MSUBq, Fsub)

// MADDi/MSUBi — broadcast I
#define COP2_MADD_I(name, addOp) \
	void recCOP2_V##name() \
	{ \
		if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return; \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		const a64::VRegister acc = cop2GetACC(); \
		armAsm->addOp(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2); \
		endMacroOp_arm64(0x110); \
	}

COP2_MADD_I(MADDi, Fadd)
COP2_MADD_I(MSUBi, Fsub)

// OPMSUB: VF[fd].xyz = ACC.xyz - VF[fs].yzx * VF[ft].zxy (cross product subtract)
// PS2 always writes XYZ only, ignoring the instruction's dest field.
void recCOP2_VOPMSUB()
{
	if (_Fd_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);
	const a64::VRegister acc = cop2GetACC();

	// Build fs.yzx: EXT #4 gives [y,z,w,x], fix lane 2 (w→x)
	a64::VRegister fsRot = a64::VRegister(28, 128);
	armAsm->Ext(fsRot.V16B(), fs.V16B(), fs.V16B(), 4);  // [y,z,w,x]
	armAsm->Ins(fsRot.V4S(), 2, fs.V4S(), 0);            // [y,z,x,x]

	// Build ft.zxy
	a64::VRegister ftRot = a64::VRegister(27, 128);
	armAsm->Ext(ftRot.V16B(), ft.V16B(), ft.V16B(), 8);  // [z,w,x,y]
	armAsm->Ins(ftRot.V4S(), 1, ft.V4S(), 0);            // [z,x,x,y]
	armAsm->Ins(ftRot.V4S(), 2, ft.V4S(), 1);            // [z,x,y,y]

	// ACC - fs.yzx * ft.zxy (separate FMUL+FSUB for PS2 rounding)
	armAsm->Fmul(RQSCRATCH.V4S(), fsRot.V4S(), ftRot.V4S());
	armAsm->Fsub(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S());
	// OPMSUB always updates XYZ flags only (0xE), W MAC flag cleared.
	// PS2 hardware ignores the W bit of the instruction's dest field —
	// only XYZ are ever written. Force the mask to XYZ regardless of encoding.
	cop2EmitResultTail(0xE);
	cop2ApplyDestMaskExplicit(_Fd_cop2, _XYZW_cop2 & 0xE);

	endMacroOp_arm64(0x110);
}

// ========================================================================
//  Accumulator write variants (xxxA): result goes to ACC instead of VF[fd]
// ========================================================================

// VADDA/VSUBA/VMULA: ACC = VF[fs] OP VF[ft]
#define COP2_ACCUM_OP(name, neonOp) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		const a64::VRegister ft = cop2GetVF(_Ft_cop2); \
		armAsm->neonOp(RQSCRATCH.V4S(), fs.V4S(), ft.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x110); \
	}

COP2_ACCUM_OP(ADDA, Fadd)
COP2_ACCUM_OP(SUBA, Fsub)
COP2_ACCUM_OP(MULA, Fmul)

// Broadcast accumulator variants: ACC = VF[fs] OP VF[ft].bc
// mulClamp=true pre-clamps the FMAC operands (cFs every mask + cFt on the full
// mask) per mVU_MULAx cFs/cFt; ADD/SUB pass false (clampType=0).
#define COP2_ACCUM_BC(name, neonOp, bc, mulClamp) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		a64::VRegister mulA = fs; \
		if (mulClamp) \
		{ \
			cop2EmitClampScratchCopy(fs); \
			mulA = RQSCRATCH; \
			if (_XYZW_cop2 == 0xf) \
				armEmitCall(s_cop2TailStubs[kCop2TailClampScratch2]); \
		} \
		armAsm->neonOp(RQSCRATCH.V4S(), mulA.V4S(), RQSCRATCH2.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x110); \
	}

// ADDAx/y/z/w
COP2_ACCUM_BC(ADDAx, Fadd, 0, false)
COP2_ACCUM_BC(ADDAy, Fadd, 1, false)
COP2_ACCUM_BC(ADDAz, Fadd, 2, false)
COP2_ACCUM_BC(ADDAw, Fadd, 3, false)

// SUBAx/y/z/w
COP2_ACCUM_BC(SUBAx, Fsub, 0, false)
COP2_ACCUM_BC(SUBAy, Fsub, 1, false)
COP2_ACCUM_BC(SUBAz, Fsub, 2, false)
COP2_ACCUM_BC(SUBAw, Fsub, 3, false)

// MULAx/y/z/w — pre-clamp Fs (and Ft on full mask) before the multiply per
// mVU_MULAx: `(_XYZW_PS)?(cFs|cFt):cFs` (TOTA, DoM). cFs catches an Inf/NaN
// Fs against a zero broadcast (Inf*0 = NaN -> result-clamped ±FLT_MAX instead
// of the interpreter's vuDouble(Fs)-clamped 0). MULAw uses the same path to
// ensure the always-on cFs is applied.
COP2_ACCUM_BC(MULAx, Fmul, 0, true)
COP2_ACCUM_BC(MULAy, Fmul, 1, true)
COP2_ACCUM_BC(MULAz, Fmul, 2, true)
COP2_ACCUM_BC(MULAw, Fmul, 3, true)

// ACCUMq variants
#define COP2_ACCUM_Q(name, neonOp) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x111); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->neonOp(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x111); \
	}

COP2_ACCUM_Q(ADDAq, Fadd)
COP2_ACCUM_Q(SUBAq, Fsub)
COP2_ACCUM_Q(MULAq, Fmul)

// ACCUMi variants
#define COP2_ACCUM_I(name, neonOp) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->neonOp(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x110); \
	}

COP2_ACCUM_I(ADDAi, Fadd)
COP2_ACCUM_I(SUBAi, Fsub)
COP2_ACCUM_I(MULAi, Fmul)

// MADDA/MSUBA variants: ACC = ACC ± VF[fs] * VF[ft]
// Separate FMUL+FADD/FSUB for PS2 intermediate rounding.
#define COP2_MADDA_OP(name, addOp) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		const a64::VRegister ft = cop2GetVF(_Ft_cop2); \
		armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), ft.V4S()); \
		const a64::VRegister acc = cop2GetACC(); \
		armAsm->addOp(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x110); \
	}

COP2_MADDA_OP(MADDA, Fadd)
COP2_MADDA_OP(MSUBA, Fsub)

// MADDA/MSUBA broadcast variants: ACC = ACC ± VF[fs] * VF[ft].bc
#define COP2_MADDA_BC(name, addOp, bc) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		const a64::VRegister acc = cop2GetACC(); \
		armAsm->addOp(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x110); \
	}

COP2_MADDA_BC(MADDAx, Fadd, 0)
COP2_MADDA_BC(MADDAy, Fadd, 1)
COP2_MADDA_BC(MADDAz, Fadd, 2)
COP2_MADDA_BC(MADDAw, Fadd, 3)

COP2_MADDA_BC(MSUBAx, Fsub, 0)
COP2_MADDA_BC(MSUBAy, Fsub, 1)
COP2_MADDA_BC(MSUBAz, Fsub, 2)
COP2_MADDA_BC(MSUBAw, Fsub, 3)

// MADDAq/MSUBAq
#define COP2_MADDA_Q(name, addOp) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x111); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		const a64::VRegister acc = cop2GetACC(); \
		armAsm->addOp(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x111); \
	}

COP2_MADDA_Q(MADDAq, Fadd)
COP2_MADDA_Q(MSUBAq, Fsub)

// MADDAi/MSUBAi
#define COP2_MADDA_I(name, addOp) \
	void recCOP2_V##name() \
	{ \
		setupMacroOp_arm64(0x110); \
		const a64::VRegister fs = cop2GetVF(_Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->Fmul(RQSCRATCH.V4S(), fs.V4S(), RQSCRATCH2.V4S()); \
		const a64::VRegister acc = cop2GetACC(); \
		armAsm->addOp(RQSCRATCH.V4S(), acc.V4S(), RQSCRATCH.V4S()); \
		cop2EmitResultTail(_XYZW_cop2); \
		cop2ApplyDestMaskACC(RQSCRATCH); \
		endMacroOp_arm64(0x110); \
	}

COP2_MADDA_I(MADDAi, Fadd)
COP2_MADDA_I(MSUBAi, Fsub)

// OPMULA: ACC.xyz = VF[fs].yzx * VF[ft].zxy (cross product to accumulator)
// PS2 always writes XYZ only, ignoring the instruction's dest field.
void recCOP2_VOPMULA()
{
	setupMacroOp_arm64(0x110);
	const a64::VRegister fs = cop2GetVF(_Fs_cop2);
	const a64::VRegister ft = cop2GetVF(_Ft_cop2);

	// Build fs.yzx: EXT #4 gives [y,z,w,x], fix lane 2 (w→x)
	a64::VRegister fsRot = a64::VRegister(28, 128);
	armAsm->Ext(fsRot.V16B(), fs.V16B(), fs.V16B(), 4);  // [y,z,w,x]
	armAsm->Ins(fsRot.V4S(), 2, fs.V4S(), 0);            // [y,z,x,x]

	// Build ft.zxy: EXT #8 gives [z,w,x,y], fix lanes 1,2
	a64::VRegister ftRot = a64::VRegister(27, 128);
	armAsm->Ext(ftRot.V16B(), ft.V16B(), ft.V16B(), 8);  // [z,w,x,y]
	armAsm->Ins(ftRot.V4S(), 1, ft.V4S(), 0);            // [z,x,x,y]
	armAsm->Ins(ftRot.V4S(), 2, ft.V4S(), 1);            // [z,x,y,y]

	armAsm->Fmul(RQSCRATCH.V4S(), fsRot.V4S(), ftRot.V4S());
	// OPMULA always updates XYZ flags only (0xE), W MAC flag cleared.
	// PS2 hardware writes ACC.xyz only; ACC.w is preserved regardless of mask.
	cop2EmitResultTail(0xE);

	cop2ApplyDestMaskACCExplicit(RQSCRATCH, _XYZW_cop2 & 0xE);
	endMacroOp_arm64(0x110);
}

// ========================================================================
//  Conversion ops: ITOF0/4/12/15, FTOI0/4/12/15
// ========================================================================

void recCOP2_VITOF0()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	const a64::VRegister src = cop2GetVF(_Fs_cop2);
	const a64::VRegister rd = cop2ResultReg(_Ft_cop2, _XYZW_cop2);
	armAsm->Scvtf(rd.V4S(), src.V4S());
	cop2ApplyDestMaskExplicit(_Ft_cop2, _XYZW_cop2, rd);
}

void recCOP2_VITOF4()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	const a64::VRegister src = cop2GetVF(_Fs_cop2);
	const a64::VRegister rd = cop2ResultReg(_Ft_cop2, _XYZW_cop2);
	armAsm->Scvtf(rd.V4S(), src.V4S(), 4);
	cop2ApplyDestMaskExplicit(_Ft_cop2, _XYZW_cop2, rd);
}

void recCOP2_VITOF12()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	const a64::VRegister src = cop2GetVF(_Fs_cop2);
	const a64::VRegister rd = cop2ResultReg(_Ft_cop2, _XYZW_cop2);
	armAsm->Scvtf(rd.V4S(), src.V4S(), 12);
	cop2ApplyDestMaskExplicit(_Ft_cop2, _XYZW_cop2, rd);
}

void recCOP2_VITOF15()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	const a64::VRegister src = cop2GetVF(_Fs_cop2);
	const a64::VRegister rd = cop2ResultReg(_Ft_cop2, _XYZW_cop2);
	armAsm->Scvtf(rd.V4S(), src.V4S(), 15);
	cop2ApplyDestMaskExplicit(_Ft_cop2, _XYZW_cop2, rd);
}

// Float→signed-int convert (Fcvtzs) with NaN saturation, for COP2 macro-mode
// VFTOIx. ARM64 NEON Fcvtzs returns 0 for a NaN input, but the PS2 — like
// mVU_FTOIx (microVU_Upper-arm64.inl) and the interpreter — saturates NaN to a
// sign-based INT_MAX/INT_MIN. Finite overflow and ±Inf already saturate
// correctly in Fcvtzs; only NaN lanes need the fixup. Source lanes are in
// RQSCRATCH and the converted+saturated result is left there; `fbits` is the
// fixed-point fraction (0/4/12/15). Uses RQSCRATCH2/RQSCRATCH3 as temps.
//
// Uses the same sign-based BIF pattern as mVU_FTOIx, but materializes the
// 0x7FFFFFFF constant with MVNI (NOT(0x80<<24)) instead of loading
// mVUglob.absclip, since the COP2 macro path does not set up the mVUglob base
// register.
static void cop2EmitFtoiSaturated(int fbits)
{
	// Build the saturation value and NaN mask from the source float BEFORE the
	// convert clobbers RQSCRATCH.
	armAsm->Sshr(RQSCRATCH2.V4S(), RQSCRATCH.V4S(), 31);    // 0xffffffff if sign set
	armAsm->Mvni(RQSCRATCH3.V4S(), 0x80, a64::LSL, 24);     // 0x7fffffff (INT_MAX) per lane
	armAsm->Eor(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), RQSCRATCH3.V16B()); // +NaN→0x7fffffff, -NaN→0x80000000
	armAsm->Fcmeq(RQSCRATCH3.V4S(), RQSCRATCH.V4S(), RQSCRATCH.V4S());    // 0xffffffff where NOT NaN

	if (fbits)
		armAsm->Fcvtzs(RQSCRATCH.V4S(), RQSCRATCH.V4S(), fbits);
	else
		armAsm->Fcvtzs(RQSCRATCH.V4S(), RQSCRATCH.V4S());

	// NaN lanes (notNan==0): replace Fcvtzs's 0 with the saturation value.
	// BIF: dst bit <- src bit where mask bit is 0.
	armAsm->Bif(RQSCRATCH.V16B(), RQSCRATCH2.V16B(), RQSCRATCH3.V16B());
}

void recCOP2_VFTOI0()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVFViaCache(RQSCRATCH, _Fs_cop2);
	cop2EmitFtoiSaturated(0);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VFTOI4()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVFViaCache(RQSCRATCH, _Fs_cop2);
	cop2EmitFtoiSaturated(4);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VFTOI12()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVFViaCache(RQSCRATCH, _Fs_cop2);
	cop2EmitFtoiSaturated(12);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VFTOI15()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVFViaCache(RQSCRATCH, _Fs_cop2);
	cop2EmitFtoiSaturated(15);
	cop2ApplyDestMask(_Ft_cop2);
}

// ========================================================================
//  Division ops: VDIV, VSQRT, VRSQRT
// ========================================================================
// These are scalar operations on single VF lanes, writing to the Q register.
// In macro mode, the result is immediately available (no pipeline delay).
// After computing Q, sync: copy to VI[REG_Q] and update D/I status flags.
// Complex edge cases (div-by-zero, negative sqrt) are handled with branches.

// Emit SYNCFDIV: copy VU0.q to VU0.VI[REG_Q] and fold the DIV-unit D/I
// results (VU0.statusflag bits 4-5) into the status chain.
//
// EP-4: the D/I update goes through the DENORMALIZED scratch, not VI — the
// DIV-family ops are unconditional status writers (COP2FlagHackPass forces
// their EEINST_COP2_STATUS_FLAG), so they carry the chain's denormalize/
// normalize marks like any FMAC and this site is the op's "flag update".
// Denorm-space RMW mirrors x86 mVU_DIV's cop2 path (microVU_Lower.inl:
// gprF &= ~0xc0000 clears CURRENT I/D only; gprF |= divFlag, where
// divI/divD = 0x1040000/0x2080000 set current+sticky together). Sticky D/I
// therefore ACCUMULATE across divides — x86 shape, diverging from interp's
// SYNCFDIV (0x3CF preserve rebuilds sticky from current alone); pinned by
// EeVu0Cop2MacroLazyStatus.DivStickyAccumulatesAcrossDivs.
static void cop2EmitSyncFDiv()
{
	// Copy q to VI[REG_Q]
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.q));
	armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.VI[REG_Q]));

	// Chain start on this op: seed the scratch from VI via the S4-4 stub
	// (which returns with the value still live in RWSCRATCH). Mid-chain:
	// reload the persistent scratch.
	if (cop2StatusDenormAtSetup())
		armEmitCall(s_cop2TailStubs[kCop2TailDenorm]);
	else
		armAsm->Ldr(RWSCRATCH, armCpuRegMem(&_cpuRegistersPack.cop2Rec.denormStatusFlag));

	// scratch = (scratch & ~0xc0000) | (DI << 14) | (DI << 20)
	// DI = statusflag & 0x30 (norm-position current D/I): <<14 lands the
	// current bits at denorm 18-19, <<20 the sticky at denorm 24-25.
	armAsm->And(RWSCRATCH, RWSCRATCH, ~0xc0000u);
	armAsm->Ldr(a64::w1, armVU0Mem(&VU0.statusflag));
	armAsm->And(a64::w1, a64::w1, 0x30);
	armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(a64::w1, a64::LSL, 14));
	armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(a64::w1, a64::LSL, 20));
	armAsm->Str(RWSCRATCH, armCpuRegMem(&_cpuRegistersPack.cop2Rec.denormStatusFlag));

	// Chain end on this op: write VI back (the stub reloads the just-stored
	// scratch — store-forwarded). Mid-chain: VI stays stale, scratch is
	// authoritative.
	if (cop2StatusNormAtEnd())
		armEmitCall(s_cop2TailStubs[kCop2TailNorm]);
}

// VDIV: Q = VF[fs].fsf / VF[ft].ftf
void recCOP2_VDIV()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);

	const int fsf = _Fsf_cop2;
	const int ftf = _Ftf_cop2;

	// Clear D/I flags
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
	armAsm->Mov(RWARG1, 0x30); armAsm->Bic(RWSCRATCH, RWSCRATCH, RWARG1); // clear D/I bits
	armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));

	// Load fs scalar and ft scalar
	armAsm->Ldr(RSSCRATCH, armVU0Mem(&VU0.VF[_Fs_cop2].UL[fsf]));  // s30 = fs[fsf]
	armAsm->Ldr(RSSCRATCH2, armVU0Mem(&VU0.VF[_Ft_cop2].UL[ftf])); // s31 = ft[ftf]

	// Check ft == 0
	a64::Label ftNonZero, done;
	armAsm->Fcmp(RSSCRATCH2, 0.0);
	armAsm->B(a64::ne, &ftNonZero);

	// ft == 0: set D/I flags, Q = ±FLT_MAX based on sign XOR
	{
		// Check if fs == 0 too → invalid (D flag = 0x10), else divide-by-zero (I flag = 0x20)
		armAsm->Fcmp(RSSCRATCH, 0.0);
		armAsm->Mov(a64::w1, 0x10); // invalid (0/0)
		armAsm->Mov(a64::w2, 0x20); // div-by-zero
		armAsm->Csel(a64::w1, a64::w1, a64::w2, a64::eq);

		armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
		armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::w1);
		armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));

		// Q = sign(fs) XOR sign(ft) ? -FLT_MAX : +FLT_MAX
		armAsm->Ldr(a64::w1, armVU0Mem(&VU0.VF[_Fs_cop2].UL[fsf]));
		armAsm->Ldr(a64::w2, armVU0Mem(&VU0.VF[_Ft_cop2].UL[ftf]));
		armAsm->Eor(a64::w1, a64::w1, a64::w2);
		armAsm->Mov(a64::w2, 0x7F7FFFFF); // +FLT_MAX
		armAsm->Mov(a64::w3, 0xFF7FFFFF); // -FLT_MAX (encoded as two MOVs by vixl)
		armAsm->Tst(a64::w1, 0x80000000);
		armAsm->Csel(RWSCRATCH, a64::w3, a64::w2, a64::ne);

		armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.q));
	}
	armAsm->B(&done);

	// ft != 0: Q = fs / ft, then clamp
	armAsm->Bind(&ftNonZero);
	{
		armAsm->Fdiv(RSSCRATCH, RSSCRATCH, RSSCRATCH2);
		// Clamp result against ±FLT_MAX held in callee-saved s8/s9.
		armAsm->Fminnm(RSSCRATCH, RSSCRATCH, a64::s8);
		armAsm->Fmaxnm(RSSCRATCH, RSSCRATCH, a64::s9);
		armAsm->Str(RSSCRATCH, armVU0Mem(&VU0.q));
	}

	armAsm->Bind(&done);
	cop2EmitSyncFDiv();
}

// VSQRT: Q = sqrt(|VF[ft].ftf|)
void recCOP2_VSQRT()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);

	const int ftf = _Ftf_cop2;

	// Clear D/I flags
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
	armAsm->Mov(RWARG1, 0x30); armAsm->Bic(RWSCRATCH, RWSCRATCH, RWARG1); // clear D/I bits
	armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));

	// Load ft scalar
	armAsm->Ldr(RSSCRATCH, armVU0Mem(&VU0.VF[_Ft_cop2].UL[ftf]));

	// If ft < 0, set invalid flag (D flag = 0x10)
	a64::Label notNeg;
	armAsm->Fcmp(RSSCRATCH, 0.0);
	armAsm->B(a64::ge, &notNeg);
	{
		armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
		armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x10);
		armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));
	}
	armAsm->Bind(&notNeg);

	// Q = sqrt(|ft|)
	armAsm->Fabs(RSSCRATCH, RSSCRATCH);
	armAsm->Fsqrt(RSSCRATCH, RSSCRATCH);

	// Clamp against ±FLT_MAX held in callee-saved s8/s9.
	armAsm->Fminnm(RSSCRATCH, RSSCRATCH, a64::s8);
	armAsm->Fmaxnm(RSSCRATCH, RSSCRATCH, a64::s9);

	armAsm->Str(RSSCRATCH, armVU0Mem(&VU0.q));

	cop2EmitSyncFDiv();
}

// VRSQRT: Q = VF[fs].fsf / sqrt(|VF[ft].ftf|)
void recCOP2_VRSQRT()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);

	const int fsf = _Fsf_cop2;
	const int ftf = _Ftf_cop2;

	// Clear D/I flags
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
	armAsm->Mov(RWARG1, 0x30); armAsm->Bic(RWSCRATCH, RWSCRATCH, RWARG1); // clear D/I bits
	armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));

	// Load ft scalar
	armAsm->Ldr(RSSCRATCH2, armVU0Mem(&VU0.VF[_Ft_cop2].UL[ftf])); // s31 = ft[ftf]

	// Load fs scalar
	armAsm->Ldr(RSSCRATCH, armVU0Mem(&VU0.VF[_Fs_cop2].UL[fsf]));  // s30 = fs[fsf]

	// Check ft == 0 → div-by-zero
	a64::Label ftNonZero, done;
	armAsm->Fcmp(RSSCRATCH2, 0.0);
	armAsm->B(a64::ne, &ftNonZero);

	// ft == 0: set div-by-zero flag (0x20), Q based on signs
	{
		armAsm->Fcmp(RSSCRATCH, 0.0);

		// fs == 0: set invalid flag too (0x10), Q = ±0
		a64::Label fsNonZero;
		armAsm->B(a64::ne, &fsNonZero);
		{
			// D/I flags: 0x30 (both invalid and div-by-zero)
			armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
			armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x30);
			armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));

			// Q = sign(fs) XOR sign(ft) ? -0 : +0
			armAsm->Ldr(a64::w1, armVU0Mem(&VU0.VF[_Fs_cop2].UL[fsf]));
			armAsm->Ldr(a64::w2, armVU0Mem(&VU0.VF[_Ft_cop2].UL[ftf]));
			armAsm->Eor(a64::w1, a64::w1, a64::w2);
			armAsm->And(RWSCRATCH, a64::w1, 0x80000000); // just sign bit, or 0
			armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.q));
			armAsm->B(&done);
		}

		// fs != 0: Q = ±FLT_MAX
		armAsm->Bind(&fsNonZero);
		{
			armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
			armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x20);
			armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));

			armAsm->Ldr(a64::w1, armVU0Mem(&VU0.VF[_Fs_cop2].UL[fsf]));
			armAsm->Ldr(a64::w2, armVU0Mem(&VU0.VF[_Ft_cop2].UL[ftf]));
			armAsm->Eor(a64::w1, a64::w1, a64::w2);
			armAsm->Mov(a64::w2, 0x7F7FFFFF);
			armAsm->Mov(a64::w3, 0xFF7FFFFF);
			armAsm->Tst(a64::w1, 0x80000000);
			armAsm->Csel(RWSCRATCH, a64::w3, a64::w2, a64::ne);
			armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.q));
			armAsm->B(&done);
		}
	}

	// ft != 0: normal path
	armAsm->Bind(&ftNonZero);
	{
		// If ft < 0, set invalid flag
		a64::Label notNeg;
		armAsm->Fcmp(RSSCRATCH2, 0.0);
		armAsm->B(a64::ge, &notNeg);
		{
			armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.statusflag));
			armAsm->Orr(RWSCRATCH, RWSCRATCH, 0x10);
			armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.statusflag));
		}
		armAsm->Bind(&notNeg);

		// Q = fs / sqrt(|ft|)
		armAsm->Fabs(RSSCRATCH2, RSSCRATCH2);
		armAsm->Fsqrt(RSSCRATCH2, RSSCRATCH2);
		armAsm->Fdiv(RSSCRATCH, RSSCRATCH, RSSCRATCH2);

		// Clamp against ±FLT_MAX held in callee-saved s8/s9.
		armAsm->Fminnm(RSSCRATCH, RSSCRATCH, a64::s8);
		armAsm->Fmaxnm(RSSCRATCH, RSSCRATCH, a64::s9);

		armAsm->Str(RSSCRATCH, armVU0Mem(&VU0.q));
	}

	armAsm->Bind(&done);
	cop2EmitSyncFDiv();
}

// ========================================================================
//  CLIP: 6-plane frustum clip test
// ========================================================================
// Compares VF[fs].xyz against ±|VF[ft].w| using signed integer comparison.
// Result: 6 bits shifted into clipflag history (24-bit rolling window).
// Bit layout per test: bit0=+x, bit1=-x, bit2=+y, bit3=-y, bit4=+z, bit5=-z

void recCOP2_VCLIP()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);

	// Load ft.w as integer, compute |ft.w| with denormal handling
	// If denormal (exponent == 0), use 0x007fffff instead
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VF[_Ft_cop2].UL[3])); // w lane

	// value = (raw & 0x7f800000) ? (raw & 0x7fffffff) : 0x007fffff
	armAsm->Mov(a64::w1, RWSCRATCH);
	armAsm->And(a64::w2, a64::w1, 0x7F800000); // exponent field
	armAsm->And(a64::w1, a64::w1, 0x7FFFFFFF); // |raw| = clear sign
	armAsm->Mov(a64::w3, 0x007FFFFF);           // denormal replacement
	armAsm->Cmp(a64::w2, 0);
	armAsm->Csel(a64::w1, a64::w1, a64::w3, a64::ne); // w1 = clip value

	// Shift clipflag left by 6. Accumulates in w9 across the NEON stretch
	// below (w9 is reserved scratch; w4 is allocatable and may be live in
	// the surrounding EE block).
	armAsm->Ldr(a64::w9, armVU0Mem(&VU0.clipflag));
	armAsm->Lsl(a64::w9, a64::w9, 6);

	// Load fs = [x,y,z,w] as integers for the lane comparisons.
	armAsm->Ldr(RQSCRATCH, armVU0Mem(&VU0.VF[_Fs_cop2])); // q30 = [x,y,z,w]

	// Vectorized signed-integer clip test (matches the interp's
	// (s32)(fs.lane ^ {0,0x80000000}) > value exactly — Cmgt is SCMGT). The
	// scalar 6× UMOV/CMP/CSET loop collapses to two NEON compares plus a
	// weighted horizontal add.
	//   pos = (s32)fs           > value   → +x,+y,+z lanes
	//   neg = (s32)(fs^signbit) > value   → -x,-y,-z lanes
	armAsm->Dup(RQSCRATCH3.V4S(), a64::w1);                 // q29 = [value × 4]
	armAsm->Movi(RQSCRATCH2.V4S(), 0x80, a64::LSL, 24);    // q31 = [0x80000000 × 4]
	armAsm->Eor(RQSCRATCH2.V16B(), RQSCRATCH.V16B(), RQSCRATCH2.V16B()); // q31 = fs ^ sign
	a64::VRegister posMask = a64::VRegister(28, 128);
	armAsm->Cmgt(posMask.V4S(), RQSCRATCH.V4S(), RQSCRATCH3.V4S());      // pos mask
	armAsm->Cmgt(RQSCRATCH2.V4S(), RQSCRATCH2.V4S(), RQSCRATCH3.V4S());  // neg mask

	// Weight each lane by its clip bit and fold to a 6-bit field. The negative
	// weights are the positive ones << 1 ([1,4,16,0] -> [2,8,32,0]), so a single
	// constant load plus a Shl covers both. +/- per axis are mutually exclusive
	// and the weights are disjoint bits, so Add+Addv = OR (no carries).
	a64::VRegister weight = a64::VRegister(27, 128);
	armAsm->Ldr(weight, armCpuRegMem(&_cpuRegistersPack.cop2Rec.clipWeightPos)); // [1,4,16,0]
	armAsm->And(posMask.V16B(), posMask.V16B(), weight.V16B());
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), weight.V16B());
	armAsm->Shl(RQSCRATCH2.V4S(), RQSCRATCH2.V4S(), 1);     // neg weights = pos << 1
	armAsm->Add(posMask.V4S(), posMask.V4S(), RQSCRATCH2.V4S());
	armAsm->Addv(posMask.S(), posMask.V4S());               // sum lanes → scalar
	armAsm->Umov(a64::w2, posMask.V4S(), 0);                // 6-bit clip field

	// Merge into clipflag and mask to 24 bits
	armAsm->Orr(a64::w9, a64::w9, a64::w2);
	armAsm->And(a64::w9, a64::w9, 0xFFFFFF);

	// Store clipflag and sync to VI[REG_CLIP_FLAG]
	armAsm->Str(a64::w9, armVU0Mem(&VU0.clipflag));
	armAsm->Str(a64::w9, armVU0Mem(&VU0.VI[REG_CLIP_FLAG]));

	// Broadcast the new clipflag into all 4 lanes of micro_clipflags. A
	// subsequent VU0 microprogram loads its clip-flag instances directly from
	// the VURegs::micro_clipflags field in the mVU Execute prologue — without
	// this they would be stale (pre-VCLIP). RQSCRATCH is free here (its earlier
	// fs load is consumed).
	armAsm->Dup(RQSCRATCH.V4S(), a64::w9);
	armAsm->Str(RQSCRATCH, armVU0Mem(&VU0.micro_clipflags));
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900

// ========================================================================
//  cop2flags — determines which control flags a COP2 instruction writes.
//  Used by the analysis pass (iR5900Analysis.cpp) for flag optimization.
//  Returns: 0=none, 1=status, 2=MAC, 3=both, 4=clip
//  Architecture-independent — identical to x86 version.
// ========================================================================

int cop2flags(u32 code)
{
	if (code >> 26 != 022)
		return 0; // not COP2
	if ((code >> 25 & 1) == 0)
		return 0; // a branch or transfer instruction

	switch (code >> 2 & 15)
	{
		case 15:
			switch (code >> 6 & 0x1f)
			{
				case 4: // ITOF*
				case 5: // FTOI*
				case 12: // MOVE MR32
				case 13: // LQI SQI LQD SQD
				case 15: // MTIR MFIR ILWR ISWR
				case 16: // RNEXT RGET RINIT RXOR
					return 0;
				case 7: // MULAq, ABS, MULAi, CLIP
					if ((code & 3) == 1) // ABS
						return 0;
					if ((code & 3) == 3) // CLIP
						return 4;
					return 3;
				case 11: // SUBA, MSUBA, OPMULA, NOP
					if ((code & 3) == 3) // NOP
						return 0;
					return 3;
				case 14: // DIV, SQRT, RSQRT, WAITQ
					if ((code & 3) == 3) // WAITQ
						return 0;
					return 1;
				default:
					break;
			}
			break;
		case 4: // MAXbc
		case 5: // MINbc
		case 12: // IADD, ISUB, IADDI
		case 13: // IAND, IOR
		case 14: // VCALLMS, VCALLMSR
			return 0;
		case 7:
			if ((code & 1) == 1) // MAXi, MINIi
				return 0;
			return 3;
		case 10:
			if ((code & 3) == 3) // MAX
				return 0;
			return 3;
		case 11:
			if ((code & 3) == 3) // MINI
				return 0;
			return 3;
		default:
			break;
	}
	return 3;
}

// ========================================================================
//  EP-2b classifier: which ops preserve the VF residency cache
// ========================================================================
// recompileNextInstruction flushes the cache for any op this returns false
// for — the safe default for every emitter that doesn't know about the
// cache. TRUE only for the hand-rolled COP2 macro ops above, whose VF/ACC
// traffic goes through cop2GetVF/cop2ApplyDestMask* (plus the VI-only and
// no-op members of the same dispatch group). Deliberately FALSE: the
// transfers (QMFC2/QMTC2/CFC2/CTC2 — raw VF access / micro kick), BC2
// branches, VCALLMS/VCALLMSR, the mVU-reuse wrappers (LQI/SQI/LQD/SQD/
// MTIR/MFIR/ILWR/ISWR/R* — VF via microVU0.regAlloc), the DIV family and
// VCLIP (raw scalar lane reads), and every unknown-op hole. Table layout:
// recCOP2t / recCOP2SPECIAL1t / recCOP2SPECIAL2t (iR5900Misc-arm64.cpp).
bool cop2OpPreservesVfCache(u32 code)
{
	if ((code >> 26) != 022)
		return false;
	if (!((code >> 21) & 0x10))
		return false; // QMFC2/CFC2/QMTC2/CTC2/BC2/holes

	const u32 funct = code & 0x3F;
	if (funct < 0x3C)
	{
		// SPECIAL1: 0x00-0x2F = FMAC arithmetic incl. VOPMSUB (all
		// cache-aware); 0x30-0x35 = VI-only ALU (no VF traffic) minus the
		// 0x33 hole; 0x38/0x39 = VCALLMS/VCALLMSR (run micro).
		if (funct <= 0x2F)
			return true;
		switch (funct)
		{
			case 0x30: case 0x31: case 0x32: case 0x34: case 0x35:
				return true;
			default:
				return false;
		}
	}

	// SPECIAL2: 0x00-0x33 = A-family FMACs, ITOF/FTOI, VABS, VMULA*,
	// VOPMULA, VNOP, VMOVE, VMR32 — cache-aware — except VCLIP (0x1F, raw
	// lane reads) and the 0x2B/0x32/0x33 holes. Everything above 0x33 is
	// mVU-reuse, DIV-family, or holes.
	const u32 idx = (code & 0x3) | ((code >> 4) & 0x7C);
	if (idx <= 0x33)
		return idx != 0x1F && idx != 0x2B && idx != 0x32 && idx != 0x33;
	return false;
}
