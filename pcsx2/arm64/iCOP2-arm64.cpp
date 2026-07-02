// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 native COP2 (VU0 macro mode) codegen using NEON.
// Memory-based: loads VF regs from VU0.VF[], computes with NEON, stores back.
// MAC/status flags are updated via C helper calls for correctness.
// No VU register allocator — each instruction is self-contained.

#include "arm64/iR5900-arm64.h"

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

// Load VU0.VF[reg] into NEON Q register
static void cop2LoadVF(const a64::VRegister& qreg, int vfReg)
{
	armAsm->Ldr(qreg, armVU0Mem(&VU0.VF[vfReg]));
}

// Store NEON Q register to VU0.VF[reg]
static void cop2StoreVF(const a64::VRegister& qreg, int vfReg)
{
	armAsm->Str(qreg, armVU0Mem(&VU0.VF[vfReg]));
}

// Load VU0.ACC into NEON Q register
static void cop2LoadACC(const a64::VRegister& qreg)
{
	armAsm->Ldr(qreg, armVU0Mem(&VU0.ACC));
}

// Store NEON Q register to VU0.ACC
static void cop2StoreACC(const a64::VRegister& qreg)
{
	armAsm->Str(qreg, armVU0Mem(&VU0.ACC));
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

static void cop2ApplyDestMaskExplicit(int fdReg, int xyzw)
{
	if (xyzw == 0xF)
	{
		if (fdReg != 0)
			cop2StoreVF(RQSCRATCH, fdReg);
		return;
	}

	if (xyzw == 0)
		return;

	if (fdReg == 0)
		return;

	// Single-lane fast path: one 32-bit lane store (1-2 insns) instead of
	// the ~6-insn load-VF + mask-load + BSL + store merge below. Single-lane
	// dest masks are common in macro COP2 code.
	const int lane = cop2SingleLaneFromMask(xyzw);
	if (lane >= 0)
	{
		cop2StoreSingleLane(RQSCRATCH, &VU0.VF[fdReg], lane);
		return;
	}

	cop2LoadVF(RQSCRATCH3, fdReg);

	armMoveAddressToReg(RSCRATCHADDR, &s_cop2DestMasks[xyzw]);
	armAsm->Ldr(RQSCRATCH2, a64::MemOperand(RSCRATCHADDR));

	armAsm->Bsl(RQSCRATCH2.V16B(), RQSCRATCH.V16B(), RQSCRATCH3.V16B());
	cop2StoreVF(RQSCRATCH2, fdReg);
}

static void cop2ApplyDestMask(int fdReg)
{
	cop2ApplyDestMaskExplicit(fdReg, _XYZW_cop2);
}

static void cop2ApplyDestMaskACCExplicit(const a64::VRegister& result, int xyzw)
{
	if (xyzw == 0xF)
	{
		cop2StoreACC(result);
		return;
	}

	if (xyzw == 0)
		return;

	// Single-lane fast path — mirrors cop2ApplyDestMaskExplicit; stores
	// straight from `result`, so the Mov-to-RQSCRATCH below is skipped too.
	const int lane = cop2SingleLaneFromMask(xyzw);
	if (lane >= 0)
	{
		cop2StoreSingleLane(result, &VU0.ACC, lane);
		return;
	}

	if (result.GetCode() != RQSCRATCH.GetCode())
		armAsm->Mov(RQSCRATCH.V16B(), result.V16B());

	cop2LoadACC(RQSCRATCH3);

	armMoveAddressToReg(RSCRATCHADDR, &s_cop2DestMasks[xyzw]);
	armAsm->Ldr(RQSCRATCH2, a64::MemOperand(RSCRATCHADDR));

	armAsm->Bsl(RQSCRATCH2.V16B(), RQSCRATCH.V16B(), RQSCRATCH3.V16B());
	cop2StoreACC(RQSCRATCH2);
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

// Clamp RQSCRATCH to [-FLT_MAX, +FLT_MAX] (removes infinities and NaNs)
// FMINNM/FMAXNM match x86 MINPS/MAXPS semantics: NaN → non-NaN operand.
static void cop2ClampResult()
{
	armMoveAddressToReg(RSCRATCHADDR, &s_cop2MaxFloat);
	armAsm->Ldr(RQSCRATCH2, a64::MemOperand(RSCRATCHADDR));
	armAsm->Fneg(RQSCRATCH3.V4S(), RQSCRATCH2.V4S()); // -FLT_MAX

	armAsm->Fminnm(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); // clamp to +FLT_MAX
	armAsm->Fmaxnm(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH3.V4S()); // clamp to -FLT_MAX
}

// Clamp an arbitrary operand register `qreg` to [-FLT_MAX, +FLT_MAX], using
// tmpHi/tmpLo to hold the ±FLT_MAX bounds. Input-operand variant of
// cop2ClampResult — used to pre-clamp an FMAC operand before the arithmetic
// (matching x86 mVU's cFs/cFt operand clamps) rather than clamping the result.
static void cop2ClampReg(const a64::VRegister& qreg,
	const a64::VRegister& tmpHi, const a64::VRegister& tmpLo)
{
	armMoveAddressToReg(RSCRATCHADDR, &s_cop2MaxFloat);
	armAsm->Ldr(tmpHi, a64::MemOperand(RSCRATCHADDR));
	armAsm->Fneg(tmpLo.V4S(), tmpHi.V4S());
	armAsm->Fminnm(qreg.V4S(), qreg.V4S(), tmpHi.V4S());
	armAsm->Fmaxnm(qreg.V4S(), qreg.V4S(), tmpLo.V4S());
}

// Single-temp variant of cop2ClampReg: clamps `qreg` to [-FLT_MAX, +FLT_MAX]
// using just one scratch register (it negates the +FLT_MAX bound in place
// between the two clamps). Needed when pre-clamping a broadcast FMAC operand,
// where Fs/Ft already occupy two of the three q-scratch regs and only one is
// free.
static void cop2ClampRegOneTmp(const a64::VRegister& qreg, const a64::VRegister& tmp)
{
	armMoveAddressToReg(RSCRATCHADDR, &s_cop2MaxFloat);
	armAsm->Ldr(tmp, a64::MemOperand(RSCRATCHADDR));
	armAsm->Fminnm(qreg.V4S(), qreg.V4S(), tmp.V4S()); // clamp to +FLT_MAX
	armAsm->Fneg(tmp.V4S(), tmp.V4S());                // -FLT_MAX
	armAsm->Fmaxnm(qreg.V4S(), qreg.V4S(), tmp.V4S()); // clamp to -FLT_MAX
}

// Pre-clamp the broadcast-MUL operands per mVU_FMACa: (_XYZW_PS)?(cFs|cFt):cFs.
// cFs (clamp Fs) is applied on every mask; cFt (clamp the broadcast Ft) only
// when all four lanes are active. Fs is in RQSCRATCH, the already-broadcast Ft
// in RQSCRATCH2, RQSCRATCH3 is the scratch. This catches operand overflow before
// it propagates through the multiply (TOTA, Disgaea, Ice Age on VU0), instead of
// only clamping the product afterward.
static void cop2EmitMulInputClamp()
{
	cop2ClampRegOneTmp(RQSCRATCH, RQSCRATCH3); // cFs (every mask)
	if (_XYZW_cop2 == 0xf)
		cop2ClampRegOneTmp(RQSCRATCH2, RQSCRATCH3); // cFt (full mask only)
}

// ========================================================================
//  PS2 VU integer-comparison MAX/MINI
// ========================================================================
// PS2 VMAX/VMINI use signed integer comparison on float bit patterns,
// NOT IEEE FMAX/FMIN. This handles NaN and negative values correctly:
//   fp_max(a,b) = both_neg ? min_s32(a,b) : max_s32(a,b)
// Implemented as: selection = CMGT(a,b) XOR both_neg_mask, then BSL.
//
// Expects: RQSCRATCH = a (from VF[fs]), RQSCRATCH2 = b (from VF[ft] or broadcast)
// Result: RQSCRATCH = fp_max(a, b) or fp_min(a, b)
// Clobbers: RQSCRATCH, RQSCRATCH2 preserved, RQSCRATCH3 used as scratch.

static void cop2EmitIntegerMax(int fsReg)
{
	// q30=a, q31=b, q29=scratch
	armAsm->And(RQSCRATCH3.V16B(), RQSCRATCH.V16B(), RQSCRATCH2.V16B()); // both_neg test
	armAsm->Sshr(RQSCRATCH3.V4S(), RQSCRATCH3.V4S(), 31);                // broadcast sign → mask
	armAsm->Cmgt(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());    // a > b (signed int)
	armAsm->Eor(RQSCRATCH.V16B(), RQSCRATCH.V16B(), RQSCRATCH3.V16B());  // selection = CMGT XOR both_neg
	cop2LoadVF(RQSCRATCH3, fsReg);                                         // reload a into q29
	armAsm->Bsl(RQSCRATCH.V16B(), RQSCRATCH3.V16B(), RQSCRATCH2.V16B()); // sel ? a : b
}

static void cop2EmitIntegerMin(int fsReg)
{
	// Same as max but BSL operands swapped: sel ? b : a
	armAsm->And(RQSCRATCH3.V16B(), RQSCRATCH.V16B(), RQSCRATCH2.V16B());
	armAsm->Sshr(RQSCRATCH3.V4S(), RQSCRATCH3.V4S(), 31);
	armAsm->Cmgt(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());
	armAsm->Eor(RQSCRATCH.V16B(), RQSCRATCH.V16B(), RQSCRATCH3.V16B());
	cop2LoadVF(RQSCRATCH3, fsReg);
	armAsm->Bsl(RQSCRATCH.V16B(), RQSCRATCH2.V16B(), RQSCRATCH3.V16B()); // sel ? b : a
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

// Runtime storage for denormalized status flag during macro op. Plain static
// (not thread_local): COP2/VU0 macro mode runs only on the EE thread (VU0 is
// lockstep with the EE; MTVU offloads VU1 only), and the JIT bakes this address
// in at emit time — a fixed global address is correct and avoids materializing a
// thread-local slot that only ever has one instance.
static u32 s_cop2DenormStatusFlag;

// Status-flag liveness for the hand-rolled COP2 macro path (bc3729c93). With
// vuFlagHack on, the denormalize (setup) and normalize (teardown) are emitted
// only when the status output is actually consumed by a later CFC2; with the
// hack off, or when analysis info is missing, they're always emitted. setup and
// teardown MUST call this with the same instruction in flight so they skip in
// lockstep — a denormalize without its matching normalize (or vice versa)
// corrupts VU0.VI[REG_STATUS_FLAG].
static bool cop2StatusFlagLive()
{
	// CHECK_VU_FLAGHACK (microVU_Misc-arm64.h) expands to this; inlined here to
	// avoid pulling a microVU header into the COP2 codegen TU.
	return !EmuConfig.Speedhacks.vuFlagHack || !g_pCurInstInfo || (g_pCurInstInfo->info & EEINST_COP2_STATUS_FLAG);
}

// Emit code to denormalize status flag from VU0.VI[REG_STATUS_FLAG]
// into s_cop2DenormStatusFlag (mVUallocSFLAGd).
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
	armMoveAddressToReg(RSCRATCHADDR, &s_cop2DenormStatusFlag);
	armAsm->Str(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));
}

// Emit code to normalize status flag from s_cop2DenormStatusFlag
// back to VU0.VI[REG_STATUS_FLAG] (mVUallocSFLAGc).
static void cop2EmitNormalizeStatusFlag()
{
	// Load denormalized flag
	armMoveAddressToReg(RSCRATCHADDR, &s_cop2DenormStatusFlag);
	armAsm->Ldr(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));

	const a64::Register result = a64::w1;
	armAsm->Mov(result, a64::wzr); // result = 0

	// Z bit (norm bit 0): set if any of denorm bits 8-11
	armAsm->Tst(RWSCRATCH, 0x0f00);
	armAsm->Cset(a64::w2, a64::ne);
	armAsm->Orr(result, result, a64::w2); // bit 0

	// S bit (norm bit 1): set if any of denorm bits 12-15
	armAsm->Tst(RWSCRATCH, 0xf000);
	armAsm->Cset(a64::w2, a64::ne);
	armAsm->Orr(result, result, a64::Operand(a64::w2, a64::LSL, 1)); // bit 1

	// 'result' now holds the current Z (bit0) / S (bit1). Sticky bits are not
	// derived from a separate denorm range — they accumulate from the current
	// Z/S below (preserve old sticky, OR current into both current and sticky
	// positions), matching the interpreter's statusflag-shift behavior.
	//
	//   VI[STATUS] = (VI[STATUS] & 0xFC0) | (Z/S) | ((Z/S) << 6)
	//
	// This MUST mirror the interpreter's COP2 macro oracle SYNCMSFLAGS()
	// (VUops.cpp): preserve 0xFC0 — NOT 0xFF0; the 0xFF0 mask belongs to the
	// FMAC-pipeline-flush path, not the COP2 macro path — then write the low
	// nibble into bits 0-3 and shifted into the sticky bits 6-9.
	//
	// LIMITATION: cop2EmitFlagUpdate() computes only Z (Fcmeq) and S (Cmlt) here;
	// it never sets the U (underflow, exp==0) or O (overflow, exp==255) bits that
	// interp's VU_STAT_UPDATE (VUflags.cpp) can produce. So 'result' only ever
	// holds bits 0-1, and masking it with 0x3 is exact. DO NOT widen the 0xFC0
	// preserve to 0xFF0 (or the 0x3 result-mask to 0xF) without first teaching
	// cop2EmitFlagUpdate to compute U/O — a bare widen keeps stale bits the
	// interpreter clears and regresses EeVu0Cop2Macro.VaddXyzwSumsLanes. No game
	// in the corpus reads U/O after a COP2 macro FMAC, so this gap is latent.
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));
	armAsm->And(RWSCRATCH, RWSCRATCH, 0xFC0); // preserve existing sticky (bits 6-11)
	armAsm->And(result, result, 0x3);          // keep only Z/S (bits 0-1)
	armAsm->Orr(RWSCRATCH, RWSCRATCH, result); // OR in current Z/S
	armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(result, a64::LSL, 6)); // OR shifted into sticky
	armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));
}

// Emit code to update MAC and status flags from the result in RQSCRATCH.
// Implements mVUupdateFlags behavior.
// xyzw = dest field mask (which lanes were written).
// Uses RQSCRATCH2, RQSCRATCH3 as temporaries.
static void cop2EmitFlagUpdate(int xyzw)
{
	// Flags are updated unconditionally for correctness; no liveness-based
	// skip is applied here.

	if (xyzw == 0) return;

	// Save result — flag extraction clobbers NEON scratch registers
	// Use q28 to preserve the result while extracting flags from it
	a64::VRegister savedResult = a64::VRegister(28, 128);
	armAsm->Mov(savedResult.V16B(), RQSCRATCH.V16B());

	// --- Extract sign bits from result ---
	// CMLT produces all-1s per lane if negative. armEmitPackLaneBits expects
	// all-1s/0 lanes (no Ushr needed) — AND-with-weights gives back the weight
	// when the lane is set and 0 otherwise.
	armAsm->Cmlt(RQSCRATCH2.V4S(), savedResult.V4S(), 0);

	// --- Extract zero bits ---
	// FCMEQ produces all-1s per lane if == 0.0
	armAsm->Fcmeq(RQSCRATCH3.V4S(), savedResult.V4S(), 0);

	// --- Pack 4 lane bits into GPR in PS2 MAC flag order ---
	// PS2 MAC flag: bit0=W, bit1=Z, bit2=Y, bit3=X (reverse of NEON lane order
	// [0]=x, [1]=y, [2]=z, [3]=w). reverse=true picks weight vector {8,4,2,1}.
	// RQSCRATCH (q30) is free here — savedResult lives in q28.
	const a64::Register signBits = a64::w1;
	const a64::Register zeroBits = a64::w2;
	armEmitPackLaneBits(signBits, RQSCRATCH2, RQSCRATCH, /*reverse=*/true);
	armEmitPackLaneBits(zeroBits, RQSCRATCH3, RQSCRATCH, /*reverse=*/true);

	// --- Apply XYZW dest mask ---
	// _XYZW_cop2 = X(bit3) Y(bit2) Z(bit1) W(bit0) — matches PS2 MAC order
	// Lanes not in dest mask should have their flag bits cleared.
	armAsm->And(signBits, signBits, xyzw);
	armAsm->And(zeroBits, zeroBits, xyzw);

	// --- Build MAC flag: (sign << 4) | zero ---
	const a64::Register macFlag = a64::w3;
	armAsm->Lsl(macFlag, signBits, 4);
	armAsm->Orr(macFlag, macFlag, zeroBits);

	// --- Write MAC flag to VU0.VI[REG_MAC_FLAG] ---
	armAsm->Str(macFlag, armVU0Mem(&VU0.VI[REG_MAC_FLAG]));

	// --- Update denormalized status flag ---
	// Load current denorm flag
	armMoveAddressToReg(RSCRATCHADDR, &s_cop2DenormStatusFlag);
	armAsm->Ldr(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));

	// Clear current (non-sticky) bits 8-15
	armAsm->Mov(a64::w4, 0xFF00);
	armAsm->Bic(RWSCRATCH, RWSCRATCH, a64::w4);

	// OR macFlag into sticky bits (0-7) — accumulates over time
	armAsm->Orr(RWSCRATCH, RWSCRATCH, macFlag);

	// OR (macFlag << 8) into current bits (8-15) — this instruction's result
	armAsm->Orr(RWSCRATCH, RWSCRATCH, a64::Operand(macFlag, a64::LSL, 8));

	// Store back. RSCRATCHADDR still holds &s_cop2DenormStatusFlag from the load
	// above (none of the Mov/Bic/Orr between touch it), so no reload is needed.
	armAsm->Str(RWSCRATCH, a64::MemOperand(RSCRATCHADDR));

	// Restore result to RQSCRATCH for subsequent cop2ApplyDestMask
	armAsm->Mov(RQSCRATCH.V16B(), savedResult.V16B());
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
		// Denormalize VU0's status flag into s_cop2DenormStatusFlag, but skip it
		// when the status output is dead. With vuFlagHack (on by default) the
		// shared COP2FlagHackPass tags every status write that reaches a CFC2
		// with EEINST_COP2_STATUS_FLAG — sticky reach included, since the tag is
		// applied to all writes before the next CFC2 (apc < m_cfc2_pc). A write
		// that reaches no CFC2 is dead, so the ~11-instruction denormalize plus
		// the matching normalize in endMacroOp are pure dead emitted code.
		// Mirrors upstream bc3729c93 and the EEINST_COP2_STATUS_FLAG gate already
		// used by mVUmacroSetupCOP2State for the mVU-reuse path. arm64 re-seeds
		// s_cop2DenormStatusFlag from VU0.VI[REG_STATUS_FLAG] every op (no x86
		// gprF0 persistence across ops), so EEINST_COP2_DENORMALIZE_STATUS_FLAG —
		// the x86 "first denormalizer" marker — is intentionally not in the gate.
		if (cop2StatusFlagLive())
			cop2EmitDenormalizeStatusFlag();
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
		// Normalize the status flag back to VU0.VI[REG_STATUS_FLAG], under the
		// same liveness gate as the setup denormalize (they must skip together —
		// see cop2StatusFlagLive). When status is dead the whole denormalize ->
		// update -> normalize chain is elided: the body's cop2EmitFlagUpdate
		// still scribbles s_cop2DenormStatusFlag, but that value is dead (never
		// normalized out) and the next live op re-seeds the scratch from
		// VU0.VI[REG_STATUS_FLAG], so nothing leaks. This is the dead-status
		// subset of bc3729c93; it needs no cross-op denormalized persistence
		// (which arm64 doesn't implement) because the live path still normalizes
		// to the architectural register every time.
		if (cop2StatusFlagLive())
			cop2EmitNormalizeStatusFlag();
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

extern void vu0Sync();
extern void vu0SyncRunAhead();
extern void _vu0FinishMicro();
extern void _vu0WaitMicro();

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
			// Lighter flush than FLUSH_EVERYTHING: FLUSH_FREE_XMM | FLUSH_FREE_VU0
			// skips callee-saved EE-GPR writebacks (callee-saved survives the C
			// call) while still evicting caller-saved GPR + all NEON.
			iFlushCall(FLUSH_FREE_XMM | FLUSH_FREE_VU0);

			// Apply block cycles to RECCYCLE (the pinned cycle delta).
			u32 cycles = scaleblockcycles_clear();
			if (cycles != 0)
				armAsm->Add(RECCYCLE, RECCYCLE, cycles);

			// Runtime: skip if VU0 not running
			a64::Label skipSync;
			armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_VPU_STAT]));
			armAsm->Tbz(RWSCRATCH, 0, &skipSync);

			// Flush the absolute cycle before vu0Sync — it reads
			// cpuRegs.cycle to determine how many VU0 micro cycles to run.
			// Re-derive after, since vu0Sync may advance cpuRegs.cycle and
			// reschedule nextEventCycle.
			armFlushCycleDelta();

			armEmitCall((void*)vu0Sync);
			if (finishFunc)
				armEmitCall((void*)finishFunc);

			armReloadCycleDelta();

			armAsm->Bind(&skipSync);
		}
		// else: analysis says no VU0 program between COP2 ops, safe to skip
		return;
	}

	// Non-interlock: check analysis flags for sync/finish
	const bool needsSync = (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0) != 0;
	const bool needsFinish = (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0) != 0;

	if (!needsSync && !needsFinish)
		return; // Analysis says no sync needed

	// Lighter flush — see interlock branch above for rationale.
	iFlushCall(FLUSH_FREE_XMM | FLUSH_FREE_VU0);

	u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	// Runtime: skip if VU0 not running
	a64::Label skipSync;
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_VPU_STAT]));
	armAsm->Tbz(RWSCRATCH, 0, &skipSync);

	// Flush + re-derive the cycle delta around the C call (see comment above).
	armFlushCycleDelta();

	if (needsSync)
		// Non-interlocked catch-up: run a 16-cycle minimum to amortize the mVU
		// dispatch envelope over small blocks (6dc5087cb). If the block also
		// contains an interlocked op, fall back to the exact sync.
		armEmitCall((void*)(s_nBlockInterlocked ? vu0Sync : vu0SyncRunAhead));
	else
		armEmitCall((void*)_vu0FinishMicro);

	armReloadCycleDelta();

	armAsm->Bind(&skipSync);
}

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

// QMFC2: cpuRegs.GPR[rt] = VU0.VF[fs] (128-bit copy, VF → EE GPR)
void recCOP2_QMFC2()
{
	iFlushCall(FLUSH_EVERYTHING);
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0FinishMicro);

	if (_Rt_ == 0) return;
	GPR_DEL_CONST(_Rt_);

	// 128-bit copy: VU0.VF[fs] → cpuRegs.GPR.r[rt]
	armAsm->Ldr(RQSCRATCH, armVU0Mem(&VU0.VF[_Rd_]));
	armStoreEEGPRQuad(RQSCRATCH, _Rt_);
}

// QMTC2: VU0.VF[fs] = cpuRegs.GPR[rt] (128-bit copy, EE GPR → VF)
void recCOP2_QMTC2()
{
	iFlushCall(FLUSH_EVERYTHING);
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0WaitMicro);

	if (_Rd_ == 0) return; // VF[0] is read-only

	// 128-bit copy: cpuRegs.GPR.r[rt] → VU0.VF[fs]
	if (GPR_IS_CONST1(_Rt_))
	{
		armMoveAddressToReg(RSCRATCHADDR, &g_cpuConstRegs[_Rt_]);
		armAsm->Ldr(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));
	}
	else
	{
				armAsm->Ldr(RQSCRATCH, armCpuRegMem(&cpuRegs.GPR.r[_Rt_]));
	}
	armAsm->Str(RQSCRATCH, armVU0Mem(&VU0.VF[_Rd_]));
}

// CFC2: cpuRegs.GPR[rt] = sign_extend_32_to_64(VU0.VI[fs])
void recCOP2_CFC2()
{
	iFlushCall(FLUSH_EVERYTHING);
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0FinishMicro);

	if (_Rt_ == 0) return;
	GPR_DEL_CONST(_Rt_);

	if (_Rd_ == REG_R)
	{
		// REG_R: mask to 23 bits, write only UL[0]
		armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_R]));
		armAsm->And(RWSCRATCH, RWSCRATCH, 0x7FFFFF);
		armStoreEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
	}
	else
	{
		// General VI: load 32-bit, sign-extend to UL[0]+UL[1]
		armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[_Rd_]));
		armStoreEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[0]);
		// Sign-extend: UL[1] = (UL[0] & 0x80000000) ? 0xFFFFFFFF : 0
		armAsm->Asr(RWSCRATCH, RWSCRATCH, 31);
		armStoreEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rt_].UL[1]);
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

	// For all other cases: flush + conditional sync, then inline write
	iFlushCall(FLUSH_EVERYTHING);
	cop2EmitConditionalSync(cpuRegs.code & 1, _vu0WaitMicro);

	// Load source value from cpuRegs.GPR[rt].UL[0]
	if (GPR_IS_CONST1(_Rt_))
	{
		armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rt_].UL[0]);
	}
	else
	{
				armAsm->Ldr(RWSCRATCH, armCpuRegMem(&cpuRegs.GPR.r[_Rt_].UL[0]));
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
		armAsm->Mov(a64::w4, 0x3cf0000); // not a valid logical-imm; materialize
		armAsm->And(a64::w3, a64::w3, a64::w4);
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

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2ApplyDestMask(_Ft_cop2);
}

// VMR32: rotate VF[fs] lanes right by one, store to VF[ft] (masked)
// x=y, y=z, z=w, w=x (rotate left in element order)
void recCOP2_VMR32()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	const int xyzw = _XYZW_cop2;
	if (xyzw == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	// EXT rotates: target lane order is [y,z,w,x] from [x,y,z,w]
	// That's a left rotation by 1 lane = EXT #4 (4 bytes)
	armAsm->Ext(RQSCRATCH.V16B(), RQSCRATCH.V16B(), RQSCRATCH.V16B(), 4);
	cop2ApplyDestMask(_Ft_cop2);
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

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	armAsm->Fabs(RQSCRATCH.V4S(), RQSCRATCH.V4S());
	cop2ApplyDestMask(_Ft_cop2);
}

// ========================================================================
//  VEC_ARITH template: VADD, VSUB, VMUL
//  Pattern: VF[fd] = VF[fs] OP VF[ft] (masked by dest)
// ========================================================================

void recCOP2_VADD()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);
	armAsm->Fadd(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());
	cop2ClampResult();
	cop2EmitFlagUpdate(_XYZW_cop2);
	cop2ApplyDestMask(_Fd_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VSUB()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);
	armAsm->Fsub(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());
	cop2ClampResult();
	cop2EmitFlagUpdate(_XYZW_cop2);
	cop2ApplyDestMask(_Fd_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VMUL()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);
	armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());
	cop2ClampResult();
	cop2EmitFlagUpdate(_XYZW_cop2);
	cop2ApplyDestMask(_Fd_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VMAX()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Fd_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);
	cop2EmitIntegerMax(_Fs_cop2);
	cop2ApplyDestMask(_Fd_cop2);
}

void recCOP2_VMINI()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Fd_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);
	cop2EmitIntegerMin(_Fs_cop2);
	cop2ApplyDestMask(_Fd_cop2);
}

// ========================================================================
//  Broadcast helpers for _BC variants
// ========================================================================

// Load VF[ft] and broadcast lane 'bc' (0=x, 1=y, 2=z, 3=w) to all lanes
static void cop2LoadBroadcast(const a64::VRegister& qreg, int vfReg, int bc)
{
	cop2LoadVF(qreg, vfReg);
	armAsm->Dup(qreg.V4S(), qreg.V4S(), bc);
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		if (mulClamp) cop2EmitMulInputClamp(); \
		armAsm->neonOp(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
		cop2ApplyDestMask(_Fd_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		cop2EmitIntegerMax(_Fs_cop2); \
		cop2ApplyDestMask(_Fd_cop2); \
	}

// MINIx/y/z/w — PS2 integer comparison, not IEEE FMIN
#define COP2_BC_MINI(name, bc) \
	void recCOP2_V##name() \
	{ \
		cop2EmitConditionalSync(false, _vu0FinishMicro); \
		if (_Fd_cop2 == 0) return; \
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		cop2EmitIntegerMin(_Fs_cop2); \
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

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]);
	cop2EmitIntegerMax(_Fs_cop2);
	cop2ApplyDestMask(_Fd_cop2);
}

void recCOP2_VMINIi()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Fd_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]);
	cop2EmitIntegerMin(_Fs_cop2);
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->neonOp(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
		cop2ApplyDestMask(_Fd_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->neonOp(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
		cop2ApplyDestMask(_Fd_cop2); \
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

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);
	armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());
	cop2LoadACC(RQSCRATCH3);
	armAsm->Fadd(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S());
	cop2ClampResult();
	cop2EmitFlagUpdate(_XYZW_cop2);
	cop2ApplyDestMask(_Fd_cop2);

	endMacroOp_arm64(0x110);
}

void recCOP2_VMSUB()
{
	if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return;
	setupMacroOp_arm64(0x110);

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);
	armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());
	cop2LoadACC(RQSCRATCH3);
	armAsm->Fsub(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S());
	cop2ClampResult();
	cop2EmitFlagUpdate(_XYZW_cop2);
	cop2ApplyDestMask(_Fd_cop2);

	endMacroOp_arm64(0x110);
}

// MADD/MSUB broadcast variants: separate FMUL + FADD/FSUB.
//
// MADDx/y/z/w pre-clamp Fs before the multiply (clampFs=true): mVU_MADDx passes
// cFs, and the interpreter routes Fs through vuDouble, so an Inf/NaN Fs against
// a zero broadcast Ft must become FLT_MAX*0 = 0 rather than Inf*0 = NaN folded
// to +/-FLT_MAX by the result clamp. MSUBx/y/z/w use mVU_FMACd (clampType=0,
// no cFs) — that Fs divergence is shared/by-design, so MSUB keeps clampFs=false.
// The MADDw extras (cACC|cFt) are a separate concern. RQSCRATCH2/RQSCRATCH3 are
// free as the ±FLT_MAX bounds here (Ft/ACC are loaded after the clamp).
#define COP2_MADD_BC(name, addOp, bc, clampFs) \
	void recCOP2_V##name() \
	{ \
		if (_Fd_cop2 == 0 && _XYZW_cop2 == 0) return; \
		setupMacroOp_arm64(0x110); \
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		if (clampFs) \
			cop2ClampReg(RQSCRATCH, RQSCRATCH2, RQSCRATCH3); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2LoadACC(RQSCRATCH3); \
		armAsm->addOp(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
		cop2ApplyDestMask(_Fd_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2LoadACC(RQSCRATCH3); \
		armAsm->addOp(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
		cop2ApplyDestMask(_Fd_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2LoadACC(RQSCRATCH3); \
		armAsm->addOp(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
		cop2ApplyDestMask(_Fd_cop2); \
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

	cop2LoadVF(RQSCRATCH, _Fs_cop2);   // fs = [x,y,z,w]
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);  // ft = [x,y,z,w]
	cop2LoadACC(RQSCRATCH3);            // ACC

	// Build fs.yzx: EXT #4 gives [y,z,w,x], fix lane 2 (w→x)
	a64::VRegister fsRot = a64::VRegister(28, 128);
	armAsm->Ext(fsRot.V16B(), RQSCRATCH.V16B(), RQSCRATCH.V16B(), 4);  // [y,z,w,x]
	armAsm->Ins(fsRot.V4S(), 2, RQSCRATCH.V4S(), 0);                   // [y,z,x,x]

	// Build ft.zxy: RQSCRATCH2 still holds ft from the load above (the fsRot
	// construction and ACC load only touch RQSCRATCH/v28/RQSCRATCH3), so reuse it.
	a64::VRegister ftRot = a64::VRegister(27, 128);
	armAsm->Ext(ftRot.V16B(), RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), 8); // [z,w,x,y]
	armAsm->Ins(ftRot.V4S(), 1, RQSCRATCH2.V4S(), 0);                   // [z,x,x,y]
	armAsm->Ins(ftRot.V4S(), 2, RQSCRATCH2.V4S(), 1);                   // [z,x,y,y]

	// ACC - fs.yzx * ft.zxy (separate FMUL+FSUB for PS2 rounding)
	armAsm->Fmul(RQSCRATCH.V4S(), fsRot.V4S(), ftRot.V4S());
	armAsm->Fsub(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S());
	cop2ClampResult();
	// OPMSUB always updates XYZ flags only (0xE), W MAC flag cleared.
	// PS2 hardware ignores the W bit of the instruction's dest field —
	// only XYZ are ever written. Force the mask to XYZ regardless of encoding.
	cop2EmitFlagUpdate(0xE);
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		cop2LoadVF(RQSCRATCH2, _Ft_cop2); \
		armAsm->neonOp(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		if (mulClamp) cop2EmitMulInputClamp(); \
		armAsm->neonOp(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->neonOp(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->neonOp(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		cop2LoadVF(RQSCRATCH2, _Ft_cop2); \
		armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2LoadACC(RQSCRATCH3); \
		armAsm->addOp(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		cop2LoadBroadcast(RQSCRATCH2, _Ft_cop2, bc); \
		armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2LoadACC(RQSCRATCH3); \
		armAsm->addOp(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_Q]); \
		armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2LoadACC(RQSCRATCH3); \
		armAsm->addOp(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
		cop2LoadVF(RQSCRATCH, _Fs_cop2); \
		armLd1rVU0(RQSCRATCH2.V4S(), &VU0.VI[REG_I]); \
		armAsm->Fmul(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S()); \
		cop2LoadACC(RQSCRATCH3); \
		armAsm->addOp(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S()); \
		cop2ClampResult(); \
		cop2EmitFlagUpdate(_XYZW_cop2); \
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
	cop2LoadVF(RQSCRATCH, _Fs_cop2);   // fs = [x,y,z,w]
	cop2LoadVF(RQSCRATCH2, _Ft_cop2);  // ft = [x,y,z,w]

	// Build fs.yzx: EXT #4 gives [y,z,w,x], fix lane 2 (w→x)
	a64::VRegister fsRot = a64::VRegister(28, 128);
	armAsm->Ext(fsRot.V16B(), RQSCRATCH.V16B(), RQSCRATCH.V16B(), 4);  // [y,z,w,x]
	armAsm->Ins(fsRot.V4S(), 2, RQSCRATCH.V4S(), 0);                   // [y,z,x,x]

	// Build ft.zxy: EXT #8 gives [z,w,x,y], fix lanes 1,2
	a64::VRegister ftRot = a64::VRegister(27, 128);
	armAsm->Ext(ftRot.V16B(), RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), 8); // [z,w,x,y]
	armAsm->Ins(ftRot.V4S(), 1, RQSCRATCH2.V4S(), 0);                   // [z,x,x,y]
	armAsm->Ins(ftRot.V4S(), 2, RQSCRATCH2.V4S(), 1);                   // [z,x,y,y]

	armAsm->Fmul(RQSCRATCH.V4S(), fsRot.V4S(), ftRot.V4S());
	cop2ClampResult();
	// OPMULA always updates XYZ flags only (0xE), W MAC flag cleared.
	// PS2 hardware writes ACC.xyz only; ACC.w is preserved regardless of mask.
	cop2EmitFlagUpdate(0xE);

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

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	armAsm->Scvtf(RQSCRATCH.V4S(), RQSCRATCH.V4S());
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VITOF4()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	armAsm->Scvtf(RQSCRATCH.V4S(), RQSCRATCH.V4S(), 4);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VITOF12()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	armAsm->Scvtf(RQSCRATCH.V4S(), RQSCRATCH.V4S(), 12);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VITOF15()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	armAsm->Scvtf(RQSCRATCH.V4S(), RQSCRATCH.V4S(), 15);
	cop2ApplyDestMask(_Ft_cop2);
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

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2EmitFtoiSaturated(0);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VFTOI4()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2EmitFtoiSaturated(4);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VFTOI12()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
	cop2EmitFtoiSaturated(12);
	cop2ApplyDestMask(_Ft_cop2);
}

void recCOP2_VFTOI15()
{
	cop2EmitConditionalSync(false, _vu0FinishMicro);
	if (_Ft_cop2 == 0) return;

	cop2LoadVF(RQSCRATCH, _Fs_cop2);
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

// Emit SYNCFDIV: copy VU0.q to VU0.VI[REG_Q], update D/I status flags.
// statusflag = (statusflag & 0x3CF) | (statusflag_DI & 0x30) | ((statusflag_DI & 0x30) << 6)
static void cop2EmitSyncFDiv()
{
	// Copy q to VI[REG_Q]
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.q));
	armAsm->Str(RWSCRATCH, armVU0Mem(&VU0.VI[REG_Q]));

	// Update status flag: (old & 0x3CF) | (statusflag & 0x30) | ((statusflag & 0x30) << 6)
	armAsm->Ldr(a64::w1, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));
	armAsm->And(a64::w1, a64::w1, 0x3CF); // clear D/I bits

	armAsm->Ldr(a64::w2, armVU0Mem(&VU0.statusflag));
	armAsm->And(a64::w2, a64::w2, 0x30); // D/I current bits

	armAsm->Orr(a64::w1, a64::w1, a64::w2);               // current D/I
	armAsm->Orr(a64::w1, a64::w1, a64::Operand(a64::w2, a64::LSL, 6)); // sticky D/I

	armAsm->Str(a64::w1, armVU0Mem(&VU0.VI[REG_STATUS_FLAG]));
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

	// Shift clipflag left by 6
	armAsm->Ldr(a64::w4, armVU0Mem(&VU0.clipflag));
	armAsm->Lsl(a64::w4, a64::w4, 6);

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
	armMoveAddressToReg(RSCRATCHADDR, &s_cop2ClipWeightPos);
	armAsm->Ldr(weight, a64::MemOperand(RSCRATCHADDR));     // [1,4,16,0]
	armAsm->And(posMask.V16B(), posMask.V16B(), weight.V16B());
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), weight.V16B());
	armAsm->Shl(RQSCRATCH2.V4S(), RQSCRATCH2.V4S(), 1);     // neg weights = pos << 1
	armAsm->Add(posMask.V4S(), posMask.V4S(), RQSCRATCH2.V4S());
	armAsm->Addv(posMask.S(), posMask.V4S());               // sum lanes → scalar
	armAsm->Umov(a64::w2, posMask.V4S(), 0);                // 6-bit clip field

	// Merge into clipflag and mask to 24 bits
	armAsm->Orr(a64::w4, a64::w4, a64::w2);
	armAsm->And(a64::w4, a64::w4, 0xFFFFFF);

	// Store clipflag and sync to VI[REG_CLIP_FLAG]
	armAsm->Str(a64::w4, armVU0Mem(&VU0.clipflag));
	armAsm->Str(a64::w4, armVU0Mem(&VU0.VI[REG_CLIP_FLAG]));

	// Broadcast the new clipflag into all 4 lanes of micro_clipflags. A
	// subsequent VU0 microprogram loads its clip-flag instances directly from
	// the VURegs::micro_clipflags field in the mVU Execute prologue — without
	// this they would be stale (pre-VCLIP). RQSCRATCH is free here (its earlier
	// fs load is consumed).
	armAsm->Dup(RQSCRATCH.V4S(), a64::w4);
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
