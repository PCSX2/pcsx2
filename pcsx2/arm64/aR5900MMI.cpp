// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — MMI 128-bit SIMD codegen (Phase 5.4).
//
// The R5900's MMI group operates on the full 128-bit GPRs as packed vectors of
// bytes / halfwords / words / doublewords. These map almost one-for-one onto
// ARM64 NEON, so each generator loads GPR[rs]/GPR[rt] into scratch q-registers,
// runs a single NEON instruction, and stores the q-register back to GPR[rd].
//
// Guest GPRs are stored little-endian in cpuRegs, so a NEON 128-bit load places
// guest element 0 (UL[0]/US[0]/UC[0]/UD[0]) into NEON lane 0 — the lane ordering
// matches the interpreter's index ordering exactly (no shuffling needed).
//
// Ground truth is pcsx2/MMI.cpp (the interpreter). Each mapping below is chosen
// to reproduce that behaviour bit-for-bit:
//   PADD*/PSUB*       -> Add / Sub            (wrapping element add/subtract)
//   PADDS*/PSUBS*     -> Sqadd / Sqsub        (signed saturating)
//   PADDU*/PSUBU*     -> Uqadd / Uqsub        (unsigned saturating)
//   PCGT*             -> Cmgt (signed)        (Rs > Rt -> all-ones mask)
//   PCEQ*             -> Cmeq                 (Rs == Rt -> all-ones mask)
//   PMAX*/PMIN*       -> Smax / Smin          (signed)
//   PABSW/PABSH       -> Sqabs               (saturating abs: 0x8000.. -> 0x7FFF..)
//   PAND/POR/PXOR     -> And / Orr / Eor
//   PNOR              -> Orr then Not
//   PEXTL*/PEXTU*     -> Zip1 / Zip2 (rt,rs)  (interleave low/high halves)
//   PPAC*             -> Uzp1 (rt,rs)         (pack: keep even-indexed elements)
//   PCPYLD            -> Zip1 .2D (rt,rs)     (Rd = Rs.lo : Rt.lo)
//   PCPYUD            -> Zip2 .2D (rs,rt)     (Rd = Rt.hi : Rs.hi)
//   PCPYH             -> broadcast US[0]/US[4] into the low/high doublewords
//
// Operand order matters for the non-commutative ops: the pack/interleave/PCPYLD
// generators feed (rt, rs) into the NEON op because the interpreter takes Rt as
// the low/even source. $zero destination writes are discarded.

#include "aR5900.h"

#include "R5900.h"

namespace a64 = vixl::aarch64;

// Scratch q-registers (caller-saved NEON, not held across any external call):
//   VS = GPR[rs], VT = GPR[rt], VD = result.  Same physical regs as the shared
//   RQSCRATCH/RQSCRATCH2/RQSCRATCH3 (q30/q31/q29).
static const a64::VRegister VS = a64::VRegister(30, 128);
static const a64::VRegister VT = a64::VRegister(31, 128);
static const a64::VRegister VD = a64::VRegister(29, 128);

static void loadQ(const a64::VRegister& v, u32 n)
{
	armAsm->Ldr(v.Q(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(n)));
}

static void storeQ(const a64::VRegister& v, u32 n)
{
	armAsm->Str(v.Q(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(n)));
}

// Binary op with the interpreter's natural (rs, rt) operand order:
//   GPR[rd] = OP(GPR[rs], GPR[rt])   over the lanes given by VIEW.
#define MMI_3OP(NAME, OP, VIEW)                                  \
	void armEmit##NAME(u32 rd, u32 rs, u32 rt)                   \
	{                                                           \
		if (rd == 0)                                            \
			return;                                             \
		loadQ(VS, rs);                                          \
		loadQ(VT, rt);                                          \
		armAsm->OP(VD.VIEW(), VS.VIEW(), VT.VIEW());            \
		storeQ(VD, rd);                                         \
	}

// Binary op with swapped (rt, rs) operand order — for the pack/interleave ops
// where the interpreter uses Rt as the low/even source operand.
#define MMI_3OP_TS(NAME, OP, VIEW)                               \
	void armEmit##NAME(u32 rd, u32 rs, u32 rt)                   \
	{                                                           \
		if (rd == 0)                                            \
			return;                                             \
		loadQ(VS, rs);                                          \
		loadQ(VT, rt);                                          \
		armAsm->OP(VD.VIEW(), VT.VIEW(), VS.VIEW());            \
		storeQ(VD, rd);                                         \
	}

// --- Parallel add / subtract (wrapping) -------------------------------------
MMI_3OP(PADDW, Add, V4S)
MMI_3OP(PADDH, Add, V8H)
MMI_3OP(PADDB, Add, V16B)
MMI_3OP(PSUBW, Sub, V4S)
MMI_3OP(PSUBH, Sub, V8H)
MMI_3OP(PSUBB, Sub, V16B)

// --- Parallel add / subtract with signed saturation -------------------------
MMI_3OP(PADDSW, Sqadd, V4S)
MMI_3OP(PADDSH, Sqadd, V8H)
MMI_3OP(PADDSB, Sqadd, V16B)
MMI_3OP(PSUBSW, Sqsub, V4S)
MMI_3OP(PSUBSH, Sqsub, V8H)
MMI_3OP(PSUBSB, Sqsub, V16B)

// --- Parallel add / subtract with unsigned saturation -----------------------
MMI_3OP(PADDUW, Uqadd, V4S)
MMI_3OP(PADDUH, Uqadd, V8H)
MMI_3OP(PADDUB, Uqadd, V16B)
MMI_3OP(PSUBUW, Uqsub, V4S)
MMI_3OP(PSUBUH, Uqsub, V8H)
MMI_3OP(PSUBUB, Uqsub, V16B)

// --- Parallel compares (produce an all-ones / all-zeros mask per lane) -------
MMI_3OP(PCGTW, Cmgt, V4S)
MMI_3OP(PCGTH, Cmgt, V8H)
MMI_3OP(PCGTB, Cmgt, V16B)
MMI_3OP(PCEQW, Cmeq, V4S)
MMI_3OP(PCEQH, Cmeq, V8H)
MMI_3OP(PCEQB, Cmeq, V16B)

// --- Parallel signed min / max ----------------------------------------------
MMI_3OP(PMAXW, Smax, V4S)
MMI_3OP(PMAXH, Smax, V8H)
MMI_3OP(PMINW, Smin, V4S)
MMI_3OP(PMINH, Smin, V8H)

// --- Parallel bitwise logic -------------------------------------------------
MMI_3OP(PAND, And, V16B)
MMI_3OP(POR, Orr, V16B)
MMI_3OP(PXOR, Eor, V16B)

void armEmitPNOR(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VS, rs);
	loadQ(VT, rt);
	armAsm->Orr(VD.V16B(), VS.V16B(), VT.V16B());
	armAsm->Not(VD.V16B(), VD.V16B());
	storeQ(VD, rd);
}

// --- Interleave (extend) low / high halves ----------------------------------
// PEXTL* interleaves the low halves of Rt (even output lanes) and Rs (odd);
// PEXTU* does the same for the high halves. Rt is the first NEON operand.
MMI_3OP_TS(PEXTLW, Zip1, V4S)
MMI_3OP_TS(PEXTLH, Zip1, V8H)
MMI_3OP_TS(PEXTLB, Zip1, V16B)
MMI_3OP_TS(PEXTUW, Zip2, V4S)
MMI_3OP_TS(PEXTUH, Zip2, V8H)
MMI_3OP_TS(PEXTUB, Zip2, V16B)

// --- Pack (keep even-indexed elements of Rt then Rs) ------------------------
MMI_3OP_TS(PPACW, Uzp1, V4S)
MMI_3OP_TS(PPACH, Uzp1, V8H)
MMI_3OP_TS(PPACB, Uzp1, V16B)

// --- Doubleword copy combines ------------------------------------------------
// PCPYLD: Rd = { Rt.UD[0], Rs.UD[0] }  -> Zip1.2D(Rt, Rs)
// PCPYUD: Rd = { Rs.UD[1], Rt.UD[1] }  -> Zip2.2D(Rs, Rt)
MMI_3OP_TS(PCPYLD, Zip1, V2D)
MMI_3OP(PCPYUD, Zip2, V2D)

// --- Parallel saturating absolute value (Rt only) ---------------------------
void armEmitPABSW(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Sqabs(VD.V4S(), VT.V4S()); // 0x80000000 -> 0x7FFFFFFF, matching the clamp
	storeQ(VD, rd);
}

void armEmitPABSH(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Sqabs(VD.V8H(), VT.V8H()); // 0x8000 -> 0x7FFF
	storeQ(VD, rd);
}

// --- PCPYH: broadcast Rt.US[0] into the low doubleword, Rt.US[4] into the high.
void armEmitPCPYH(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Dup(VS.V8H(), VT.V8H(), 0); // all 8 halfwords = US[0]
	armAsm->Dup(VD.V8H(), VT.V8H(), 4); // all 8 halfwords = US[4]
	armAsm->Ins(VD.V2D(), 0, VS.V2D(), 0); // low doubleword <- US[0]x4; high stays US[4]x4
	storeQ(VD, rd);
}

// =============================================================================
// Parallel shifts by immediate (Phase 5.4 continuation)
// =============================================================================
// Each lane is shifted independently by the same immediate amount `sa`.
// ARM64 NEON provides single-instruction forms for all three shift types:
//   Shl  — shift left (zero-extend out bits)
//   Ushr — unsigned/logical shift right (zero-fill from the left)
//   Sshr — signed/arithmetic shift right (sign-extend from the left)
//
// The guest GPRs are little-endian, so NEON lane 0 holds guest element 0 —
// the lane indexing matches the interpreter's element indexing exactly.

// --- PSLLH/PSLLW: parallel logical shift left by `sa` ------------------------
void armEmitPSLLH(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	u32 shift = sa & 0x0F;
	if (shift == 0) {
		armAsm->Mov(VD.V16B(), VT.V16B()); // no-op shift, just copy
	} else {
		armAsm->Shl(VD.V8H(), VT.V8H(), shift); // 16-bit lanes
	}
	storeQ(VD, rd);
}

void armEmitPSLLW(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	u32 shift = sa & 0x1F;
	if (shift == 0) {
		armAsm->Mov(VD.V16B(), VT.V16B()); // no-op shift, just copy
	} else {
		armAsm->Shl(VD.V4S(), VT.V4S(), shift); // 32-bit lanes
	}
	storeQ(VD, rd);
}

// --- PSRLH/PSRLW: parallel logical (unsigned) shift right by `sa` ------------
void armEmitPSRLH(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	u32 shift = sa & 0x0F;
	if (shift == 0) {
		armAsm->Mov(VD.V16B(), VT.V16B()); // no-op shift, just copy
	} else {
		armAsm->Ushr(VD.V8H(), VT.V8H(), shift); // zero-fill from left
	}
	storeQ(VD, rd);
}

void armEmitPSRLW(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	u32 shift = sa & 0x1F;
	if (shift == 0) {
		armAsm->Mov(VD.V16B(), VT.V16B()); // no-op shift, just copy
	} else {
		armAsm->Ushr(VD.V4S(), VT.V4S(), shift); // zero-fill from left
	}
	storeQ(VD, rd);
}

// --- PSRAH/PSRAW: parallel arithmetic (signed) shift right by `sa` -----------
void armEmitPSRAH(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	u32 shift = sa & 0x0F;
	if (shift == 0) {
		armAsm->Mov(VD.V16B(), VT.V16B()); // no-op shift, just copy
	} else {
		armAsm->Sshr(VD.V8H(), VT.V8H(), shift); // sign-extend from left
	}
	storeQ(VD, rd);
}

void armEmitPSRAW(u32 rd, u32 rt, u32 sa)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	u32 shift = sa & 0x1F;
	if (shift == 0) {
		armAsm->Mov(VD.V16B(), VT.V16B()); // no-op shift, just copy
	} else {
		armAsm->Sshr(VD.V4S(), VT.V4S(), shift); // sign-extend from left
	}
	storeQ(VD, rd);
}


// =============================================================================
// Parallel lane permutes (Phase 5.4 continuation)
// =============================================================================
// These rearrange the halfword/word lanes within the 128-bit GPR. They don't
// map to single NEON instructions, so we use lane-by-lane insertion (Ins).

// --- PINTH: interleave halfwords ---------------------------------------------
// Output: [Rt[0], Rs[4], Rt[1], Rs[5], Rt[2], Rs[6], Rt[3], Rs[7]]
void armEmitPINTH(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	loadQ(VS, rs);
	// Build result lane-by-lane using Ins.
	armAsm->Mov(VD.V8H(), VT.V8H(), 0);  // VD[0] = Rt[0]
	armAsm->Ins(VD.V8H(), 1, VS.V8H(), 4); // VD[1] = Rs[4]
	armAsm->Ins(VD.V8H(), 2, VT.V8H(), 1); // VD[2] = Rt[1]
	armAsm->Ins(VD.V8H(), 3, VS.V8H(), 5); // VD[3] = Rs[5]
	armAsm->Ins(VD.V8H(), 4, VT.V8H(), 2); // VD[4] = Rt[2]
	armAsm->Ins(VD.V8H(), 5, VS.V8H(), 6); // VD[5] = Rs[6]
	armAsm->Ins(VD.V8H(), 6, VT.V8H(), 3); // VD[6] = Rt[3]
	armAsm->Ins(VD.V8H(), 7, VS.V8H(), 7); // VD[7] = Rs[7]
	storeQ(VD, rd);
}

// --- PINTEH: interleave even halfwords ---------------------------------------
// Output: [Rt[0], Rs[0], Rt[2], Rs[2], Rt[4], Rs[4], Rt[6], Rs[6]]
void armEmitPINTEH(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	loadQ(VS, rs);
	armAsm->Mov(VD.V8H(), VT.V8H(), 0);  // VD[0] = Rt[0]
	armAsm->Ins(VD.V8H(), 1, VS.V8H(), 0); // VD[1] = Rs[0]
	armAsm->Ins(VD.V8H(), 2, VT.V8H(), 2); // VD[2] = Rt[2]
	armAsm->Ins(VD.V8H(), 3, VS.V8H(), 2); // VD[3] = Rs[2]
	armAsm->Ins(VD.V8H(), 4, VT.V8H(), 4); // VD[4] = Rt[4]
	armAsm->Ins(VD.V8H(), 5, VS.V8H(), 4); // VD[5] = Rs[4]
	armAsm->Ins(VD.V8H(), 6, VT.V8H(), 6); // VD[6] = Rt[6]
	armAsm->Ins(VD.V8H(), 7, VS.V8H(), 6); // VD[7] = Rs[6]
	storeQ(VD, rd);
}

// --- PEXEH: extract even halfwords (swap 0<->2 in each 64-bit half) ----------
// Output: [Rt[2], Rt[1], Rt[0], Rt[3], Rt[6], Rt[5], Rt[4], Rt[7]]
void armEmitPEXEH(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Mov(VD.V8H(), VT.V8H(), 2);  // VD[0] = Rt[2]
	armAsm->Ins(VD.V8H(), 1, VT.V8H(), 1); // VD[1] = Rt[1]
	armAsm->Ins(VD.V8H(), 2, VT.V8H(), 0); // VD[2] = Rt[0]
	armAsm->Ins(VD.V8H(), 3, VT.V8H(), 3); // VD[3] = Rt[3]
	armAsm->Ins(VD.V8H(), 4, VT.V8H(), 6); // VD[4] = Rt[6]
	armAsm->Ins(VD.V8H(), 5, VT.V8H(), 5); // VD[5] = Rt[5]
	armAsm->Ins(VD.V8H(), 6, VT.V8H(), 4); // VD[6] = Rt[4]
	armAsm->Ins(VD.V8H(), 7, VT.V8H(), 7); // VD[7] = Rt[7]
	storeQ(VD, rd);
}

// --- PEXEW: extract even words (swap 32-bit lanes 0<->2) ---------------------
// Output: [Rt[2], Rt[1], Rt[0], Rt[3]]  (32-bit lanes)
void armEmitPEXEW(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Mov(VD.V4S(), VT.V4S(), 2);  // VD[0] = Rt[2]
	armAsm->Ins(VD.V4S(), 1, VT.V4S(), 1); // VD[1] = Rt[1]
	armAsm->Ins(VD.V4S(), 2, VT.V4S(), 0); // VD[2] = Rt[0]
	armAsm->Ins(VD.V4S(), 3, VT.V4S(), 3); // VD[3] = Rt[3]
	storeQ(VD, rd);
}

// --- PREVH: reverse halfwords within each 64-bit half ------------------------
// Output: [Rt[3], Rt[2], Rt[1], Rt[0], Rt[7], Rt[6], Rt[5], Rt[4]]
void armEmitPREVH(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Rev64(VD.V8H(), VT.V8H()); // Single instruction!
	storeQ(VD, rd);
}

// =============================================================================
// Remaining lane permutes (Phase 5.4 continuation)
// =============================================================================

// --- PROT3W: rotate 3 words (Rt-only, Rt = {UL[0],UL[1],UL[2],UL[3]} ) --------
// Output: [Rt[1], Rt[2], Rt[0], Rt[3]]  (32-bit lanes; lane 3 is unchanged)
void armEmitPROT3W(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Mov(VD.V4S(), VT.V4S(), 1);  // VD[0] = Rt[1]
	armAsm->Ins(VD.V4S(), 1, VT.V4S(), 2); // VD[1] = Rt[2]
	armAsm->Ins(VD.V4S(), 2, VT.V4S(), 0); // VD[2] = Rt[0]
	armAsm->Ins(VD.V4S(), 3, VT.V4S(), 3); // VD[3] = Rt[3]  (unchanged)
	storeQ(VD, rd);
}

// --- PEXCH: extract even halfwords within each 64-bit half -------------------
// Swaps halfword pairs (1<->2) within each 64-bit half.
// Output: [Rt[0], Rt[2], Rt[1], Rt[3], Rt[4], Rt[6], Rt[5], Rt[7]]
void armEmitPEXCH(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Mov(VD.V8H(), VT.V8H(), 0);  // VD[0] = Rt[0]
	armAsm->Ins(VD.V8H(), 1, VT.V8H(), 2); // VD[1] = Rt[2]
	armAsm->Ins(VD.V8H(), 2, VT.V8H(), 1); // VD[2] = Rt[1]
	armAsm->Ins(VD.V8H(), 3, VT.V8H(), 3); // VD[3] = Rt[3]
	armAsm->Ins(VD.V8H(), 4, VT.V8H(), 4); // VD[4] = Rt[4]
	armAsm->Ins(VD.V8H(), 5, VT.V8H(), 6); // VD[5] = Rt[6]
	armAsm->Ins(VD.V8H(), 6, VT.V8H(), 5); // VD[6] = Rt[5]
	armAsm->Ins(VD.V8H(), 7, VT.V8H(), 7); // VD[7] = Rt[7]
	storeQ(VD, rd);
}

// --- PEXCW: extract even words (swap word pairs 1<->2) -----------------------
// Output: [Rt[0], Rt[2], Rt[1], Rt[3]]  (32-bit lanes)
void armEmitPEXCW(u32 rd, u32 rt)
{
	if (rd == 0)
		return;
	loadQ(VT, rt);
	armAsm->Mov(VD.V4S(), VT.V4S(), 0);  // VD[0] = Rt[0]
	armAsm->Ins(VD.V4S(), 1, VT.V4S(), 2); // VD[1] = Rt[2]
	armAsm->Ins(VD.V4S(), 2, VT.V4S(), 1); // VD[2] = Rt[1]
	armAsm->Ins(VD.V4S(), 3, VT.V4S(), 3); // VD[3] = Rt[3]
	storeQ(VD, rd);
}

// =============================================================================
// Parallel variable shifts (Phase 5.4 continuation)
// =============================================================================
// These shift each 32-bit lane by the amount specified in the corresponding
// lane of GPR[rs]. The shift amount is masked to 5 bits per lane (& 0x1F).
//
// ARM64 NEON does not have a direct "shift each lane by unsigned vector amount"
// instruction for 32-bit lanes. We use scalar GPR operations for correctness:
// load each 32-bit lane, shift by the corresponding amount, and store back.
//
// The GPR stores 4 x 32-bit lanes in a 128-bit register. We pack two 32-bit
// results into each 64-bit store (SD[0] = {UL[0], UL[1]}, SD[1] = {UL[2], UL[3]}).
//
// Caller-saved GPRs used as scratch: x9-x12 (avoiding x16 which is VIXL scratch).

// --- PSLLVW: parallel logical shift left by GPR[rs] -------------------------
// Output[i] = Rt[i] << (Rs[i] & 0x1F)  for 32-bit lanes i=0..3
void armEmitPSLLVW(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	// Process lanes 0 and 1 -> pack into SD[0]
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 4));
	armAsm->Ldr(a64::w11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 4));

	armAsm->Lsl(a64::w9, a64::w9, a64::w11);
	armAsm->Lsl(a64::w10, a64::w10, a64::w12);

	// Pack two 32-bit results into one 64-bit register: result[0] | (result[1] << 32)
	armAsm->Bfi(a64::x9, a64::x10, 32, 32);
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	// Process lanes 2 and 3 -> pack into SD[1]
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 12));
	armAsm->Ldr(a64::w11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 12));

	armAsm->Lsl(a64::w9, a64::w9, a64::w11);
	armAsm->Lsl(a64::w10, a64::w10, a64::w12);

	armAsm->Bfi(a64::x9, a64::x10, 32, 32);
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
}

// --- PSRLVW: parallel logical (unsigned) shift right by GPR[rs] -------------
// Output[i] = Rt[i] >> (Rs[i] & 0x1F)  (zero-fill from left)
void armEmitPSRLVW(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	// Process lanes 0 and 1 -> pack into SD[0]
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 4));
	armAsm->Ldr(a64::w11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 4));

	armAsm->Lsr(a64::w9, a64::w9, a64::w11);
	armAsm->Lsr(a64::w10, a64::w10, a64::w12);

	armAsm->Bfi(a64::x9, a64::x10, 32, 32);
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	// Process lanes 2 and 3 -> pack into SD[1]
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 12));
	armAsm->Ldr(a64::w11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 12));

	armAsm->Lsr(a64::w9, a64::w9, a64::w11);
	armAsm->Lsr(a64::w10, a64::w10, a64::w12);

	armAsm->Bfi(a64::x9, a64::x10, 32, 32);
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
}

// --- PSRAVW: parallel arithmetic (signed) shift right by GPR[rs] ------------
// Output[i] = Rt[i] >> (Rs[i] & 0x1F)  (sign-extend from left)
void armEmitPSRAVW(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	// Process lanes 0 and 1 -> pack into SD[0]
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 4));
	armAsm->Ldr(a64::w11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 4));

	armAsm->Asr(a64::w9, a64::w9, a64::w11);
	armAsm->Asr(a64::w10, a64::w10, a64::w12);

	armAsm->Bfi(a64::x9, a64::x10, 32, 32);
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	// Process lanes 2 and 3 -> pack into SD[1]
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 12));
	armAsm->Ldr(a64::w11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 12));

	armAsm->Asr(a64::w9, a64::w9, a64::w11);
	armAsm->Asr(a64::w10, a64::w10, a64::w12);

	armAsm->Bfi(a64::x9, a64::x10, 32, 32);
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
}

// =============================================================================
// Multiply-accumulate family (Phase 5.4 continuation)
// =============================================================================
// These ops multiply pairs of elements and accumulate into the HI/LO special
// registers. The result is also written to GPR[rd] for some ops.
//
// HI/LO offsets (matching aR5900MultDiv.cpp):
//   HI  = 32 * 16 = 512  (HI.UD[0])
//   LO  = 33 * 16 = 528  (LO.UD[0])
//   HI1 = HI + 8 = 520   (HI.UD[1])
//   LO1 = LO + 8 = 536   (LO.UD[1])
static constexpr u32 EE_HI_OFFSET = 32u * 16u;
static constexpr u32 EE_LO_OFFSET = 33u * 16u;
static constexpr u32 EE_HI1_OFFSET = EE_HI_OFFSET + 8u;
static constexpr u32 EE_LO1_OFFSET = EE_LO_OFFSET + 8u;

// Scratch registers for multiply-accumulate (caller-saved GPRs):
//   w9-w12: 32-bit lane data and intermediates
//   x13:   64-bit accumulate result
// Use the VIXL register objects directly (w9, w10, etc. are already WRegister objects).
#define WTEMP1 a64::w9
#define WTEMP2 a64::w10
#define WTEMP3 a64::w11
#define WTEMP4 a64::w12
#define XTEMP a64::x13

// -----------------------------------------------------------------------------
// PMULTW: Word multiply (lanes 0 and 2)
// -----------------------------------------------------------------------------
//   LO.SD[0] = (s32)(Rs[0] * Rt[0])
//   HI.SD[0] = (s32)((Rs[0] * Rt[0]) >> 32)
//   if (Rd) GPR[rd].SD[0] = Rs[0] * Rt[0] (full 64-bit)
//   LO.SD[1] = (s32)(Rs[2] * Rt[2])
//   HI.SD[1] = (s32)((Rs[2] * Rt[2]) >> 32)
//   if (Rd) GPR[rd].SD[1] = Rs[2] * Rt[2]
void armEmitPMULTW(u32 rd, u32 rs, u32 rt)
{
	// Lane 0
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Smull(XTEMP, WTEMP1, WTEMP2); // 32x32->64 signed multiply

	// Store LO.SD[0] (low 32 bits, sign-extended to 64)
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// Store HI.SD[0] (high 32 bits, sign-extended to 64)
	armAsm->Asr(XTEMP, XTEMP, 32);
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// Store GPR[rd].SD[0] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
		armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Smull(XTEMP, WTEMP1, WTEMP2);
		armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
	}

	// Lane 2 (index 2 = offset 8 for low word)
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Smull(XTEMP, WTEMP1, WTEMP2);

	// Store LO.SD[1]
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));

	// Store HI.SD[1]
	armAsm->Asr(XTEMP, XTEMP, 32);
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));

	// Store GPR[rd].SD[1] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
		armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
		armAsm->Smull(XTEMP, WTEMP1, WTEMP2);
		armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMULTUW: Unsigned word multiply (lanes 0 and 2)
// -----------------------------------------------------------------------------
// Same as PMULTW but unsigned multiply.
void armEmitPMULTUW(u32 rd, u32 rs, u32 rt)
{
	// Lane 0
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Umull(XTEMP, WTEMP1, WTEMP2); // 32x32->64 unsigned multiply

	// Store LO.SD[0] (low 32 bits, sign-extended to 64 per interpreter)
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// Store HI.SD[0] (high 32 bits, sign-extended to 64 per interpreter)
	armAsm->Asr(XTEMP, XTEMP, 32);
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// Store GPR[rd].SD[0] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
		armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		armAsm->Umull(XTEMP, WTEMP1, WTEMP2);
		armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
	}

	// Lane 2
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Umull(XTEMP, WTEMP1, WTEMP2);

	// Store LO.SD[1]
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));

	// Store HI.SD[1]
	armAsm->Asr(XTEMP, XTEMP, 32);
	armAsm->Sxtw(WTEMP1, XTEMP.W());
	armAsm->Str(WTEMP1, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));

	// Store GPR[rd].SD[1] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
		armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
		armAsm->Umull(XTEMP, WTEMP1, WTEMP2);
		armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMADDUW: Unsigned word multiply-add
// -----------------------------------------------------------------------------
// For each lane (dd=0/ss=0 and dd=1/ss=2):
//   temp = Rs[ss] * Rt[ss] (unsigned 32x32->64)
//   temp2 = temp + (HI[ss] << 32)
//   LO.SD[dd] = (s32)(temp & 0xffffffff) + LO[ss]
//   HI.SD[dd] = (s32)(temp2 >> 32)  (no division voodoo for unsigned)
//   if (Rd) { GPR[rd].UL[dd*2] = LO.UL[dd*2]; GPR[rd].UL[dd*2+1] = HI.UL[dd*2]; }
void armEmitPMADDUW(u32 rd, u32 rs, u32 rt)
{
	// Lane 0 (dd=0, ss=0)
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Umull(a64::x9, WTEMP1, WTEMP2); // temp = Rs[0] * Rt[0] (unsigned)

	// temp2 = temp + (HI[0] << 32)
	armAsm->Ldr(WTEMP3, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Lsl(XTEMP, XTEMP, 32);
	armAsm->Add(a64::x9, a64::x9, XTEMP);

	// LO.SD[0] = (s32)(temp & 0xffffffff) + LO[0]
	armAsm->And(WTEMP1, a64::x9, 0xFFFFFFFF);
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Add(WTEMP1, WTEMP1, WTEMP2);
	armAsm->Sxtw(XTEMP, WTEMP1);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// HI.SD[0] = (s32)(temp2 >> 32)
	armAsm->Lsr(a64::x10, a64::x9, 32);
	armAsm->Sxtw(XTEMP, a64::x10);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// GPR[rd] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
	}

	// Lane 2 (dd=1, ss=2)
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Umull(a64::x9, WTEMP1, WTEMP2);

	// temp2 = temp + (HI[2] << 32)
	armAsm->Ldr(WTEMP3, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Lsl(XTEMP, XTEMP, 32);
	armAsm->Add(a64::x9, a64::x9, XTEMP);

	// LO.SD[1] = (s32)(temp & 0xffffffff) + LO[2]
	armAsm->And(WTEMP1, a64::x9, 0xFFFFFFFF);
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
	armAsm->Add(WTEMP1, WTEMP1, WTEMP2);
	armAsm->Sxtw(XTEMP, WTEMP1);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));

	// HI.SD[1] = (s32)(temp2 >> 32)
	armAsm->Lsr(a64::x10, a64::x9, 32);
	armAsm->Sxtw(XTEMP, a64::x10);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));

	// GPR[rd] if rd != 0 (second doubleword)
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMADDW: Word multiply-add with EE division voodoo
// -----------------------------------------------------------------------------
// For each lane (dd=0/ss=0 and dd=1/ss=2):
//   temp = Rs[ss] * Rt[ss]  (64-bit)
//   temp2 = temp + (HI[ss] << 32)
//   // EE division voodoo for lane 0 only:
//   if (ss==0 && ((Rt[0]&0x7FFFFFFF)==0 || (Rt[0]&0x7FFFFFFF)==0x7FFFFFFF) && Rs[0]!=Rt[0])
//     temp2 += 0x70000000
//   temp2 = (s32)(temp2 / 4294967295)  // off-by-1 multiplication error
//   LO.SD[dd] = (s32)(temp & 0xffffffff) + LO[ss]
//   HI.SD[dd] = (s32)temp2
//   if (Rd) { GPR[rd].UL[dd*2] = LO.UL[dd*2]; GPR[rd].UL[dd*2+1] = HI.UL[dd*2]; }
void armEmitPMADDW(u32 rd, u32 rs, u32 rt)
{
	a64::Label voodoo_done;

	// -------------------------------------------------------------------------
	// Lane 0 (dd=0, ss=0)
	// -------------------------------------------------------------------------
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Smull(a64::x9, WTEMP1, WTEMP2); // temp = Rs[0] * Rt[0] in x9

	// EE division voodoo check for lane 0 only
	// Condition: ((Rt[0] & 0x7FFFFFFF) == 0 || (Rt[0] & 0x7FFFFFFF) == 0x7FFFFFFF) && Rs[0] != Rt[0]
	armAsm->And(WTEMP3, WTEMP2, 0x7FFFFFFF);
	armAsm->Cmp(WTEMP3, 0);
	armAsm->B(&voodoo_done, a64::eq); // (Rt[0] & 0x7FFFFFFF) == 0
	armAsm->Cmp(WTEMP3, 0x7FFFFFFF);
	armAsm->B(&voodoo_done, a64::ne); // Neither 0 nor 0x7FFFFFFF

	// Check Rs[0] != Rt[0]
	armAsm->Cmp(WTEMP1, WTEMP2);
	armAsm->B(&voodoo_done, a64::eq); // Rs[0] == Rt[0]

	// Add 0x70000000 to temp (x9)
	armAsm->Mov(WTEMP3, 0x7000);
	armAsm->Lsl(WTEMP3, WTEMP3, 16); // WTEMP3 = 0x70000000
	armAsm->Add(a64::x9, a64::x9, a64::Operand(WTEMP3));

	armAsm->Bind(&voodoo_done);

	// temp2 = temp + (HI[0] << 32)
	armAsm->Ldr(WTEMP3, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Lsl(XTEMP, XTEMP, 32);
	armAsm->Add(a64::x9, a64::x9, XTEMP);

	// temp2 = temp2 / 4294967295
	armAsm->Mov(WTEMP3, 0xFFFFFFFF);
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Sdiv(a64::x10, a64::x9, XTEMP); // temp2 / 4294967295 in x10

	// LO.SD[0] = (s32)(temp & 0xffffffff) + LO[0]
	armAsm->And(WTEMP1, a64::x9, 0xFFFFFFFF);
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Add(WTEMP1, WTEMP1, WTEMP2);
	armAsm->Sxtw(XTEMP, WTEMP1);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// HI.SD[0] = (s32)temp2
	armAsm->Sxtw(XTEMP, a64::x10);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// GPR[rd] if rd != 0 (combine LO[0] and HI[0] into 64-bit)
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
	}

	// -------------------------------------------------------------------------
	// Lane 2 (dd=1, ss=2) - no voodoo
	// -------------------------------------------------------------------------
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Smull(a64::x9, WTEMP1, WTEMP2);

	// temp2 = temp + (HI[2] << 32)
	armAsm->Ldr(WTEMP3, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Lsl(XTEMP, XTEMP, 32);
	armAsm->Add(a64::x9, a64::x9, XTEMP);

	// temp2 = temp2 / 4294967295
	armAsm->Mov(WTEMP3, 0xFFFFFFFF);
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Sdiv(a64::x10, a64::x9, XTEMP);

	// LO.SD[1] = (s32)(temp & 0xffffffff) + LO[2]
	armAsm->And(WTEMP1, a64::x9, 0xFFFFFFFF);
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
	armAsm->Add(WTEMP1, WTEMP1, WTEMP2);
	armAsm->Sxtw(XTEMP, WTEMP1);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));

	// HI.SD[1] = (s32)temp2
	armAsm->Sxtw(XTEMP, a64::x10);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));

	// GPR[rd] if rd != 0 (second doubleword)
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMSUBW: Word multiply-subtract
// -----------------------------------------------------------------------------
// For each lane (dd=0/ss=0 and dd=1/ss=2):
//   temp = Rs[ss] * Rt[ss]
//   temp2 = (HI[ss] << 32) - temp
//   temp2 = (s32)(temp2 / 4294967295)
//   LO.SD[dd] = LO[ss] - (s32)(temp & 0xffffffff)
//   HI.SD[dd] = (s32)temp2
//   if (Rd) { GPR[rd].UL[dd*2] = LO.UL[dd*2]; GPR[rd].UL[dd*2+1] = HI.UL[dd*2]; }
void armEmitPMSUBW(u32 rd, u32 rs, u32 rt)
{
	// Lane 0 (dd=0, ss=0)
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Smull(a64::x9, WTEMP1, WTEMP2); // temp = Rs[0] * Rt[0]

	// temp2 = (HI[0] << 32) - temp
	armAsm->Ldr(WTEMP3, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Lsl(XTEMP, XTEMP, 32);
	armAsm->Sub(XTEMP, XTEMP, a64::x9);

	// temp2 = temp2 / 4294967295
	armAsm->Mov(WTEMP3, 0xFFFFFFFF);
	armAsm->Sxtw(a64::x10, WTEMP3);
	armAsm->Sdiv(a64::x10, XTEMP, a64::x10);

	// LO.SD[0] = LO[0] - (s32)(temp & 0xffffffff)
	armAsm->And(WTEMP1, a64::x9, 0xFFFFFFFF);
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Sub(WTEMP2, WTEMP2, WTEMP1);
	armAsm->Sxtw(XTEMP, WTEMP2);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// HI.SD[0] = (s32)temp2
	armAsm->Sxtw(XTEMP, a64::x10);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// GPR[rd] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
	}

	// Lane 2 (dd=1, ss=2)
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Smull(a64::x9, WTEMP1, WTEMP2);

	// temp2 = (HI[2] << 32) - temp
	armAsm->Ldr(WTEMP3, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
	armAsm->Sxtw(XTEMP, WTEMP3);
	armAsm->Lsl(XTEMP, XTEMP, 32);
	armAsm->Sub(XTEMP, XTEMP, a64::x9);

	// temp2 = temp2 / 4294967295
	armAsm->Mov(WTEMP3, 0xFFFFFFFF);
	armAsm->Sxtw(a64::x10, WTEMP3);
	armAsm->Sdiv(a64::x10, XTEMP, a64::x10);

	// LO.SD[1] = LO[2] - (s32)(temp & 0xffffffff)
	armAsm->And(WTEMP1, a64::x9, 0xFFFFFFFF);
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
	armAsm->Sub(WTEMP2, WTEMP2, WTEMP1);
	armAsm->Sxtw(XTEMP, WTEMP2);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));

	// HI.SD[1] = (s32)temp2
	armAsm->Sxtw(XTEMP, a64::x10);
	armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));

	// GPR[rd] if rd != 0 (second doubleword)
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMULTH: Halfword multiply (8 lanes, alternating LO/HI)
// -----------------------------------------------------------------------------
// For n = 0,2,4,6:
//   LO.SD[n/2] = (s32)(Rs.SS[n] * Rt.SS[n])
//   HI.SD[n/2] = (s32)((Rs.SS[n] * Rt.SS[n]) >> 32)
//   if (Rd) GPR[rd].SD[n/2] = Rs.SS[n] * Rt.SS[n]
void armEmitPMULTH(u32 rd, u32 rs, u32 rt)
{
	// Load full 128-bit Rs and Rt into scratch registers
	loadQ(VS, rs);
	loadQ(VT, rt);

	// We'll compute lane by lane using scalar operations for clarity
	// Lane 0 (n=0) -> LO.SD[0], HI.SD[0]
	armAsm->Smov(a64::w9, VT.V8H(), 0); // Rt.SS[0]
	armAsm->Smov(a64::w10, VS.V8H(), 0); // Rs.SS[0]
	armAsm->Smull(a64::x11, a64::w9, a64::w10); // temp = Rs[0] * Rt[0]

	// LO.SD[0] = low 32 bits sign-extended
	armAsm->Sxtw(a64::x12, a64::x11);
	armAsm->Str(a64::x12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// HI.SD[0] = high 32 bits sign-extended
	armAsm->Asr(a64::x12, a64::x11, 32);
	armAsm->Str(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// GPR[rd].SD[0] if rd != 0
	if (rd != 0) {
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
	}

	// Lane 2 (n=2) -> LO.SD[1], HI.SD[1]
	armAsm->Smov(a64::w9, VT.V8H(), 2);
	armAsm->Smov(a64::w10, VS.V8H(), 2);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);

	armAsm->Sxtw(a64::x12, a64::x11);
	armAsm->Str(a64::x12, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));

	armAsm->Asr(a64::x12, a64::x11, 32);
	armAsm->Str(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));

	if (rd != 0) {
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMADDH: Halfword multiply-add (8 lanes)
// -----------------------------------------------------------------------------
// For n = 0,1,2,3,4,5,6,7:
//   temp = LO/HI.UL[n] + Rs.SS[n] * Rt.SS[n]
//   LO/HI.UL[n] = temp (alternating: n even -> LO, n odd -> HI)
//   if (Rd) { GPR[rd].UL[0]=LO.UL[0], GPR[rd].UL[1]=HI.UL[0],
//             GPR[rd].UL[2]=LO.UL[2], GPR[rd].UL[3]=HI.UL[2] }
void armEmitPMADDH(u32 rd, u32 rs, u32 rt)
{
	loadQ(VS, rs);
	loadQ(VT, rt);

	a64::Label skip_rd;

	// Process 8 halfword lanes, accumulating into LO/HI
	// n=0 -> LO.UL[0], n=1 -> LO.UL[1], n=2 -> HI.UL[0], n=3 -> HI.UL[1]
	// n=4 -> LO.UL[2], n=5 -> LO.UL[3], n=6 -> HI.UL[2], n=7 -> HI.UL[3]

	// We need to load each halfword, multiply, accumulate, and store
	// For simplicity, do scalar operations lane by lane

	// n=0: LO.UL[0] += Rs.SS[0] * Rt.SS[0]
	armAsm->Smov(a64::w9, VT.V8H(), 0);
	armAsm->Smov(a64::w10, VS.V8H(), 0);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// n=1: LO.UL[1] += Rs.SS[1] * Rt.SS[1]
	armAsm->Smov(a64::w9, VT.V8H(), 1);
	armAsm->Smov(a64::w10, VS.V8H(), 1);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 4));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 4));

	// n=2: HI.UL[0] += Rs.SS[2] * Rt.SS[2]
	armAsm->Smov(a64::w9, VT.V8H(), 2);
	armAsm->Smov(a64::w10, VS.V8H(), 2);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// n=3: HI.UL[1] += Rs.SS[3] * Rt.SS[3]
	armAsm->Smov(a64::w9, VT.V8H(), 3);
	armAsm->Smov(a64::w10, VS.V8H(), 3);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 4));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 4));

	// n=4: LO.UL[2] += Rs.SS[4] * Rt.SS[4]
	armAsm->Smov(a64::w9, VT.V8H(), 4);
	armAsm->Smov(a64::w10, VS.V8H(), 4);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));

	// n=5: LO.UL[3] += Rs.SS[5] * Rt.SS[5]
	armAsm->Smov(a64::w9, VT.V8H(), 5);
	armAsm->Smov(a64::w10, VS.V8H(), 5);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 12));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 12));

	// n=6: HI.UL[2] += Rs.SS[6] * Rt.SS[6]
	armAsm->Smov(a64::w9, VT.V8H(), 6);
	armAsm->Smov(a64::w10, VS.V8H(), 6);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));

	// n=7: HI.UL[3] += Rs.SS[7] * Rt.SS[7]
	armAsm->Smov(a64::w9, VT.V8H(), 7);
	armAsm->Smov(a64::w10, VS.V8H(), 7);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 12));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 12));

	// GPR[rd] if rd != 0: {LO.UL[2], HI.UL[2], LO.UL[0], HI.UL[0]}
	// Actually per interpreter: UL[0]=LO.UL[0], UL[1]=HI.UL[0], UL[2]=LO.UL[2], UL[3]=HI.UL[2]
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Ldr(a64::x11, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
		armAsm->Ldr(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
		armAsm->Bfi(a64::x11, a64::x12, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMSUBH: Halfword multiply-subtract (8 lanes)
// -----------------------------------------------------------------------------
// For n = 0,1,2,3,4,5,6,7:
//   temp = LO/HI.UL[n] - Rs.SS[n] * Rt.SS[n]
//   LO/HI.UL[n] = temp (alternating: n even -> LO, n odd -> HI)
//   if (Rd) { GPR[rd].UL[0]=LO.UL[0], GPR[rd].UL[1]=HI.UL[0],
//             GPR[rd].UL[2]=LO.UL[2], GPR[rd].UL[3]=HI.UL[2] }
void armEmitPMSUBH(u32 rd, u32 rs, u32 rt)
{
	loadQ(VS, rs);
	loadQ(VT, rt);

	// n=0: LO.UL[0] -= Rs.SS[0] * Rt.SS[0]
	armAsm->Smov(a64::w9, VT.V8H(), 0);
	armAsm->Smov(a64::w10, VS.V8H(), 0);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// n=1: LO.UL[1] -= Rs.SS[1] * Rt.SS[1]
	armAsm->Smov(a64::w9, VT.V8H(), 1);
	armAsm->Smov(a64::w10, VS.V8H(), 1);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 4));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 4));

	// n=2: HI.UL[0] -= Rs.SS[2] * Rt.SS[2]
	armAsm->Smov(a64::w9, VT.V8H(), 2);
	armAsm->Smov(a64::w10, VS.V8H(), 2);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// n=3: HI.UL[1] -= Rs.SS[3] * Rt.SS[3]
	armAsm->Smov(a64::w9, VT.V8H(), 3);
	armAsm->Smov(a64::w10, VS.V8H(), 3);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 4));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 4));

	// n=4: LO.UL[2] -= Rs.SS[4] * Rt.SS[4]
	armAsm->Smov(a64::w9, VT.V8H(), 4);
	armAsm->Smov(a64::w10, VS.V8H(), 4);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));

	// n=5: LO.UL[3] -= Rs.SS[5] * Rt.SS[5]
	armAsm->Smov(a64::w9, VT.V8H(), 5);
	armAsm->Smov(a64::w10, VS.V8H(), 5);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 12));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 12));

	// n=6: HI.UL[2] -= Rs.SS[6] * Rt.SS[6]
	armAsm->Smov(a64::w9, VT.V8H(), 6);
	armAsm->Smov(a64::w10, VS.V8H(), 6);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));

	// n=7: HI.UL[3] -= Rs.SS[7] * Rt.SS[7]
	armAsm->Smov(a64::w9, VT.V8H(), 7);
	armAsm->Smov(a64::w10, VS.V8H(), 7);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 12));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 12));

	// GPR[rd] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Ldr(a64::x11, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
		armAsm->Ldr(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
		armAsm->Bfi(a64::x11, a64::x12, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PHMADH: Packed halfword multiply-add (8 lanes, paired)
// -----------------------------------------------------------------------------
// For n = 0,2,4,6:
//   temp = Rs.SS[n]*Rt.SS[n] + Rs.SS[n+1]*Rt.SS[n+1]
//   if (n%4==0) LO.UL[n/2] += temp; else HI.UL[n/2] += temp
//   if (Rd) { GPR[rd].UL[0]=LO.UL[0], GPR[rd].UL[1]=HI.UL[0], ... }
void armEmitPHMADH(u32 rd, u32 rs, u32 rt)
{
	loadQ(VS, rs);
	loadQ(VT, rt);

	// n=0,1: LO.UL[0] += Rs.SS[0]*Rt.SS[0] + Rs.SS[1]*Rt.SS[1]
	armAsm->Smov(a64::w9, VT.V8H(), 0);
	armAsm->Smov(a64::w10, VS.V8H(), 0);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 1);
	armAsm->Smov(a64::w10, VS.V8H(), 1);
	armAsm->Smaddl(a64::x11, a64::w9, a64::w10, a64::x11);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// n=2,3: HI.UL[0] += Rs.SS[2]*Rt.SS[2] + Rs.SS[3]*Rt.SS[3]
	armAsm->Smov(a64::w9, VT.V8H(), 2);
	armAsm->Smov(a64::w10, VS.V8H(), 2);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 3);
	armAsm->Smov(a64::w10, VS.V8H(), 3);
	armAsm->Smaddl(a64::x11, a64::w9, a64::w10, a64::x11);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// n=4,5: LO.UL[2] += Rs.SS[4]*Rt.SS[4] + Rs.SS[5]*Rt.SS[5]
	armAsm->Smov(a64::w9, VT.V8H(), 4);
	armAsm->Smov(a64::w10, VS.V8H(), 4);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 5);
	armAsm->Smov(a64::w10, VS.V8H(), 5);
	armAsm->Smaddl(a64::x11, a64::w9, a64::w10, a64::x11);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));

	// n=6,7: HI.UL[2] += Rs.SS[6]*Rt.SS[6] + Rs.SS[7]*Rt.SS[7]
	armAsm->Smov(a64::w9, VT.V8H(), 6);
	armAsm->Smov(a64::w10, VS.V8H(), 6);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 7);
	armAsm->Smov(a64::w10, VS.V8H(), 7);
	armAsm->Smaddl(a64::x11, a64::w9, a64::w10, a64::x11);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));

	// GPR[rd] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Ldr(a64::x11, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
		armAsm->Ldr(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
		armAsm->Bfi(a64::x11, a64::x12, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PHMSBH: Packed halfword multiply-subtract (8 lanes, paired)
// -----------------------------------------------------------------------------
// For n = 0,2,4,6:
//   temp = Rs.SS[n]*Rt.SS[n] - Rs.SS[n+1]*Rt.SS[n+1]
//   if (n%4==0) LO.UL[n/2] += temp; else HI.UL[n/2] += temp
void armEmitPHMSBH(u32 rd, u32 rs, u32 rt)
{
	loadQ(VS, rs);
	loadQ(VT, rt);

	// n=0,1: LO.UL[0] += Rs.SS[0]*Rt.SS[0] - Rs.SS[1]*Rt.SS[1]
	armAsm->Smov(a64::w9, VT.V8H(), 0);
	armAsm->Smov(a64::w10, VS.V8H(), 0);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 1);
	armAsm->Smov(a64::w10, VS.V8H(), 1);
	armAsm->Smull(a64::x12, a64::w9, a64::w10);
	armAsm->Sub(a64::x11, a64::x11, a64::x12);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));

	// n=2,3: HI.UL[0] += Rs.SS[2]*Rt.SS[2] - Rs.SS[3]*Rt.SS[3]
	armAsm->Smov(a64::w9, VT.V8H(), 2);
	armAsm->Smov(a64::w10, VS.V8H(), 2);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 3);
	armAsm->Smov(a64::w10, VS.V8H(), 3);
	armAsm->Smull(a64::x12, a64::w9, a64::w10);
	armAsm->Sub(a64::x11, a64::x11, a64::x12);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));

	// n=4,5: LO.UL[2] += Rs.SS[4]*Rt.SS[4] - Rs.SS[5]*Rt.SS[5]
	armAsm->Smov(a64::w9, VT.V8H(), 4);
	armAsm->Smov(a64::w10, VS.V8H(), 4);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 5);
	armAsm->Smov(a64::w10, VS.V8H(), 5);
	armAsm->Smull(a64::x12, a64::w9, a64::w10);
	armAsm->Sub(a64::x11, a64::x11, a64::x12);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));

	// n=6,7: HI.UL[2] += Rs.SS[6]*Rt.SS[6] - Rs.SS[7]*Rt.SS[7]
	armAsm->Smov(a64::w9, VT.V8H(), 6);
	armAsm->Smov(a64::w10, VS.V8H(), 6);
	armAsm->Smull(a64::x11, a64::w9, a64::w10);
	armAsm->Smov(a64::w9, VT.V8H(), 7);
	armAsm->Smov(a64::w10, VS.V8H(), 7);
	armAsm->Smull(a64::x12, a64::w9, a64::w10);
	armAsm->Sub(a64::x11, a64::x11, a64::x12);
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
	armAsm->Add(a64::w12, a64::w12, a64::w11);
	armAsm->Str(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));

	// GPR[rd] if rd != 0
	if (rd != 0) {
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Ldr(a64::x11, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
		armAsm->Ldr(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
		armAsm->Bfi(a64::x11, a64::x12, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
	}
}

// -----------------------------------------------------------------------------
// PMFHI/PMFLO/PMTHI/PMTLO: MMI HI/LO moves (full 128-bit)
// -----------------------------------------------------------------------------
// PMFHI: Rd = HI (full 128-bit)
// PMFLO: Rd = LO (full 128-bit)
// PMTHI: HI = Rs (full 128-bit)
// PMTLO: LO = Rs (full 128-bit)
void armEmitPMFHI(u32 rd)
{
	if (rd == 0)
		return;
	// Load full 128-bit HI into q-register
	armAsm->Ldr(VD.Q(), a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Str(VD.Q(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

void armEmitPMFLO(u32 rd)
{
	if (rd == 0)
		return;
	armAsm->Ldr(VD.Q(), a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Str(VD.Q(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
}

void armEmitPMTHI(u32 rs)
{
	armAsm->Ldr(VD.Q(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Str(VD.Q(), a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
}

void armEmitPMTLO(u32 rs)
{
	armAsm->Ldr(VD.Q(), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Str(VD.Q(), a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
}
