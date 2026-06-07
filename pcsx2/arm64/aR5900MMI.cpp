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
// IMPORTANT: despite the "VW" name, the interpreter (MMI.cpp PSLLVW/PSRLVW/
// PSRAVW) does NOT shift four independent 32-bit lanes. It shifts only lanes 0
// and 2 of Rt (each by the matching lane of Rs, masked to 5 bits) and writes the
// 32-bit result *sign-extended to a full 64-bit doubleword*:
//
//   Rd.SD[0] = (s64)(s32)(Rt.UL[0] <</>> (Rs.UL[0] & 0x1F));   // fills Rd.UD[0]
//   Rd.SD[1] = (s64)(s32)(Rt.UL[2] <</>> (Rs.UL[2] & 0x1F));   // fills Rd.UD[1]
//
// So each doubleword's high word is the sign fill of its low word, NOT a shift of
// Rt.UL[1]/Rt.UL[3]. We compute each lane in a w-register (the variable shift form
// already masks the amount mod 32 == & 0x1F), Sxtw it to 64 bits, and store the
// whole doubleword.
//
// Caller-saved GPRs used as scratch: x9-x10 (avoiding x16 which is VIXL scratch).

// --- PSLLVW: parallel logical shift left by GPR[rs] -------------------------
void armEmitPSLLVW(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));      // Rt.UL[0]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));     // Rs.UL[0]
	armAsm->Lsl(a64::w9, a64::w9, a64::w10);
	armAsm->Sxtw(a64::x9, a64::w9);                                            // sign-extend to UD[0]
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));  // Rt.UL[2]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8)); // Rs.UL[2]
	armAsm->Lsl(a64::w9, a64::w9, a64::w10);
	armAsm->Sxtw(a64::x9, a64::w9);                                            // sign-extend to UD[1]
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
}

// --- PSRLVW: parallel logical (unsigned) shift right by GPR[rs] -------------
void armEmitPSRLVW(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));      // Rt.UL[0]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));     // Rs.UL[0]
	armAsm->Lsr(a64::w9, a64::w9, a64::w10);
	armAsm->Sxtw(a64::x9, a64::w9);                                            // sign-extend to UD[0]
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));  // Rt.UL[2]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8)); // Rs.UL[2]
	armAsm->Lsr(a64::w9, a64::w9, a64::w10);
	armAsm->Sxtw(a64::x9, a64::w9);                                            // sign-extend to UD[1]
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
}

// --- PSRAVW: parallel arithmetic (signed) shift right by GPR[rs] ------------
void armEmitPSRAVW(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));      // Rt.SL[0]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));     // Rs.UL[0]
	armAsm->Asr(a64::w9, a64::w9, a64::w10);
	armAsm->Sxtw(a64::x9, a64::w9);                                            // sign-extend to UD[0]
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));  // Rt.SL[2]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8)); // Rs.UL[2]
	armAsm->Asr(a64::w9, a64::w9, a64::w10);
	armAsm->Sxtw(a64::x9, a64::w9);                                            // sign-extend to UD[1]
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
// Store a 32x32->64 product (held in XTEMP) to one lane:
//   LO.SD[dd] = (s32)low32 sign-extended, HI.SD[dd] = high32 sign-extended,
//   and the full 64-bit product to GPR[rd].UD[dd] when rd != 0.
static void emitWordMulStore(u32 rd, u32 rdOff, u32 loOff, u32 hiOff)
{
	if (rd != 0)
		armAsm->Str(XTEMP, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + rdOff));
	armAsm->Sxtw(a64::x9, XTEMP.W()); // LO = sign-extend low 32 into 64
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, loOff));
	armAsm->Asr(a64::x9, XTEMP, 32); // HI = arithmetic high 32 (already 64-bit)
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, hiOff));
}

void armEmitPMULTW(u32 rd, u32 rs, u32 rt)
{
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Smull(XTEMP, WTEMP1, WTEMP2);
	emitWordMulStore(rd, /*rdOff*/ 0, EE_LO_OFFSET, EE_HI_OFFSET);

	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Smull(XTEMP, WTEMP1, WTEMP2);
	emitWordMulStore(rd, /*rdOff*/ 8, EE_LO1_OFFSET, EE_HI1_OFFSET);
}

// -----------------------------------------------------------------------------
// PMULTUW: Unsigned word multiply (lanes 0 and 2)
// -----------------------------------------------------------------------------
// Same as PMULTW but unsigned multiply.
void armEmitPMULTUW(u32 rd, u32 rs, u32 rt)
{
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->Umull(XTEMP, WTEMP1, WTEMP2);
	emitWordMulStore(rd, /*rdOff*/ 0, EE_LO_OFFSET, EE_HI_OFFSET);

	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Ldr(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + 8));
	armAsm->Umull(XTEMP, WTEMP1, WTEMP2);
	emitWordMulStore(rd, /*rdOff*/ 8, EE_LO1_OFFSET, EE_HI1_OFFSET);
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
// One PMADDUW lane. Interpreter:
//   tempu = (LO.UL[ss] | (HI.UL[ss] << 32)) + (u64)Rs.UL[ss] * (u64)Rt.UL[ss];
//   LO.SD[dd] = (s32)(tempu & 0xffffffff);  HI.SD[dd] = (s32)(tempu >> 32);
//   if (Rd) GPR[rd].UD[dd] = tempu;
// The whole 64-bit accumulator is formed first so a carry out of the low word
// propagates into the high word.
static void emitPMADDUWLane(u32 rd, u32 rs, u32 rt, u32 srcOff, u32 loOff, u32 hiOff, u32 rdOff)
{
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + srcOff));  // Rs.UL[ss]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + srcOff)); // Rt.UL[ss]
	armAsm->Umull(a64::x11, a64::w9, a64::w10); // product
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, loOff));  // LO.UL[ss] (zero-extended)
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, hiOff)); // HI.UL[ss] (zero-extended)
	armAsm->Add(a64::x11, a64::x11, a64::x9);
	armAsm->Add(a64::x11, a64::x11, a64::Operand(a64::x10, a64::LSL, 32)); // tempu

	if (rd != 0)
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + rdOff)); // full 64-bit

	armAsm->Sxtw(a64::x9, a64::w11); // (s32)(tempu & 0xffffffff)
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, loOff));
	armAsm->Asr(a64::x10, a64::x11, 32);
	armAsm->Sxtw(a64::x10, a64::w10); // (s32)(tempu >> 32)
	armAsm->Str(a64::x10, a64::MemOperand(RESTATEPTR, hiOff));
}

void armEmitPMADDUW(u32 rd, u32 rs, u32 rt)
{
	emitPMADDUWLane(rd, rs, rt, /*srcOff*/ 0, EE_LO_OFFSET, EE_HI_OFFSET, /*rdOff*/ 0);
	emitPMADDUWLane(rd, rs, rt, /*srcOff*/ 8, EE_LO1_OFFSET, EE_HI1_OFFSET, /*rdOff*/ 8);
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
// Emit one PMADDW lane: dd selects LO/HI.UD[dd], srcOff is the GPR byte offset of
// lane `ss` (Rs/Rt low word) and loOff/hiOff the LO/HI lane offsets. `voodoo` adds
// the PS2 lane-0 division quirk.
static void emitPMADDWLane(u32 rd, u32 rs, u32 rt, u32 srcOff, u32 loOff, u32 hiOff,
	u32 rdOff, bool voodoo)
{
	a64::Label voodoo_done;

	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + srcOff));  // Rs[ss]
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + srcOff)); // Rt[ss]
	armAsm->Smull(a64::x11, a64::w9, a64::w10); // temp = (s64)Rs[ss] * (s64)Rt[ss]

	// LO.SD[dd] = (s32)(temp & 0xffffffff) + LO[ss]  — uses the *pure* product.
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, loOff));
	armAsm->Add(a64::w12, a64::w11, a64::w12);
	armAsm->Sxtw(a64::x12, a64::w12);
	armAsm->Str(a64::x12, a64::MemOperand(RESTATEPTR, loOff));

	// temp2 = temp + (HI.SL[ss] << 32), with the lane-0 division voodoo.
	if (voodoo)
	{
		// Condition: ((Rt&0x7FFFFFFF)==0 || ==0x7FFFFFFF) && Rs != Rt
		// Both ==0 and ==0x7FFFFFFF are triggers, so a zero result must fall
		// through to the Rs!=Rt check, not skip the add.
		a64::Label voodoo_check_rs;
		armAsm->And(a64::w12, a64::w10, 0x7FFFFFFF);
		armAsm->Cbz(a64::w12, &voodoo_check_rs);   // ==0 -> still a trigger
		armAsm->Cmp(a64::w12, 0x7FFFFFFF);
		armAsm->B(&voodoo_done, a64::ne);          // neither 0 nor 0x7FFFFFFF -> no voodoo
		armAsm->Bind(&voodoo_check_rs);
		armAsm->Cmp(a64::w9, a64::w10);
		armAsm->B(&voodoo_done, a64::eq);          // Rs == Rt -> no voodoo
		armAsm->Mov(a64::w12, 0x70000000);
		armAsm->Add(a64::x11, a64::x11, a64::x12); // temp2 += 0x70000000
		armAsm->Bind(&voodoo_done);
	}
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, hiOff)); // HI.SL[ss]
	armAsm->Sxtw(a64::x12, a64::w12);
	armAsm->Add(a64::x11, a64::x11, a64::Operand(a64::x12, a64::LSL, 32));

	// temp2 = (s32)(temp2 / 4294967295)  — positive 64-bit divisor.
	armAsm->Mov(a64::x12, 0xFFFFFFFF);
	armAsm->Sdiv(a64::x11, a64::x11, a64::x12);
	armAsm->Sxtw(a64::x11, a64::w11);
	armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, hiOff));

	if (rd != 0)
	{
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, loOff));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, hiOff));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + rdOff));
	}
}

void armEmitPMADDW(u32 rd, u32 rs, u32 rt)
{
	emitPMADDWLane(rd, rs, rt, /*srcOff*/ 0, EE_LO_OFFSET, EE_HI_OFFSET, /*rdOff*/ 0, /*voodoo*/ true);
	emitPMADDWLane(rd, rs, rt, /*srcOff*/ 8, EE_LO1_OFFSET, EE_HI1_OFFSET, /*rdOff*/ 8, /*voodoo*/ false);
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
// One PMSUBW lane. Interpreter:
//   temp = Rs[ss]*Rt[ss];  temp2 = (HI.SL[ss] << 32) - temp;
//   temp2 = (s32)(temp2 / 4294967295);
//   LO.SD[dd] = LO.SL[ss] - (s32)(temp & 0xffffffff);  HI.SD[dd] = (s32)temp2;
static void emitPMSUBWLane(u32 rd, u32 rs, u32 rt, u32 srcOff, u32 loOff, u32 hiOff, u32 rdOff)
{
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + srcOff));
	armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt) + srcOff));
	armAsm->Smull(a64::x11, a64::w9, a64::w10); // temp (pure product)

	// LO.SD[dd] = LO[ss] - (s32)(temp & 0xffffffff)  — from the pure product.
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, loOff));
	armAsm->Sub(a64::w12, a64::w12, a64::w11);
	armAsm->Sxtw(a64::x12, a64::w12);
	armAsm->Str(a64::x12, a64::MemOperand(RESTATEPTR, loOff));

	// temp2 = (HI.SL[ss] << 32) - temp
	armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, hiOff));
	armAsm->Sxtw(a64::x12, a64::w12);
	armAsm->Lsl(a64::x12, a64::x12, 32);
	armAsm->Sub(a64::x11, a64::x12, a64::x11);

	armAsm->Mov(a64::x12, 0xFFFFFFFF);
	armAsm->Sdiv(a64::x11, a64::x11, a64::x12);
	armAsm->Sxtw(a64::x11, a64::w11);
	armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, hiOff));

	if (rd != 0)
	{
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, loOff));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, hiOff));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + rdOff));
	}
}

void armEmitPMSUBW(u32 rd, u32 rs, u32 rt)
{
	emitPMSUBWLane(rd, rs, rt, /*srcOff*/ 0, EE_LO_OFFSET, EE_HI_OFFSET, /*rdOff*/ 0);
	emitPMSUBWLane(rd, rs, rt, /*srcOff*/ 8, EE_LO1_OFFSET, EE_HI1_OFFSET, /*rdOff*/ 8);
}

// -----------------------------------------------------------------------------
// PMULTH: Halfword multiply (8 lanes, alternating LO/HI)
// -----------------------------------------------------------------------------
// For n = 0,2,4,6:
//   LO.SD[n/2] = (s32)(Rs.SS[n] * Rt.SS[n])
//   HI.SD[n/2] = (s32)((Rs.SS[n] * Rt.SS[n]) >> 32)
//   if (Rd) GPR[rd].SD[n/2] = Rs.SS[n] * Rt.SS[n]
// Byte offsets of the 8 halfword-product destinations LO/HI.UL[n] for n=0..7,
// matching the interpreter: LO0,LO1,HI0,HI1,LO2,LO3,HI2,HI3.
static const u32 kHalfwordMacOff[8] = {
	EE_LO_OFFSET + 0, EE_LO_OFFSET + 4, EE_HI_OFFSET + 0, EE_HI_OFFSET + 4,
	EE_LO_OFFSET + 8, EE_LO_OFFSET + 12, EE_HI_OFFSET + 8, EE_HI_OFFSET + 12};

// Pack GPR[rd] = {LO.UL[0], HI.UL[0], LO.UL[2], HI.UL[2]} (shared by the
// halfword multiply-accumulate family).
static void emitHalfwordMacStoreRd(u32 rd)
{
	if (rd == 0)
		return;
	armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Bfi(a64::x9, a64::x10, 32, 32);
	armAsm->Ldr(a64::x11, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 8));
	armAsm->Ldr(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 8));
	armAsm->Bfi(a64::x11, a64::x12, 32, 32);
	armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
	armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
}

void armEmitPMULTH(u32 rd, u32 rs, u32 rt)
{
	loadQ(VS, rs);
	loadQ(VT, rt);

	// 8 independent halfword products: LO/HI.UL[n] = (s32)(Rs.SS[n] * Rt.SS[n]).
	for (int n = 0; n < 8; n++)
	{
		armAsm->Smov(a64::w9, VT.V8H(), n);
		armAsm->Smov(a64::w10, VS.V8H(), n);
		armAsm->Mul(a64::w11, a64::w9, a64::w10);
		armAsm->Str(a64::w11, a64::MemOperand(RESTATEPTR, kHalfwordMacOff[n]));
	}

	emitHalfwordMacStoreRd(rd);
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
// One PHMADH/PHMSBH pair (Rs/Rt already loaded into VS/VT):
//   firsttemp = Rs.SS[n+1] * Rt.SS[n+1]
//   add: temp = firsttemp + Rs.SS[n]*Rt.SS[n];  odd lane = firsttemp
//   sub: temp = firsttemp - Rs.SS[n]*Rt.SS[n];  odd lane = ~firsttemp (undocumented)
// `offTemp`/`offFirst` are the even/odd destination lane offsets. No accumulation
// with the previous LO/HI contents (matches the interpreter).
static void emitPHMPair(int n, u32 offTemp, u32 offFirst, bool sub)
{
	armAsm->Smov(a64::w9, VT.V8H(), n + 1);
	armAsm->Smov(a64::w10, VS.V8H(), n + 1);
	armAsm->Mul(a64::w11, a64::w9, a64::w10); // firsttemp
	armAsm->Smov(a64::w9, VT.V8H(), n);
	armAsm->Smov(a64::w10, VS.V8H(), n);
	armAsm->Mul(a64::w12, a64::w9, a64::w10); // Rs.SS[n] * Rt.SS[n]
	if (sub)
	{
		armAsm->Sub(a64::w9, a64::w11, a64::w12);
		armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, offTemp));
		armAsm->Mvn(a64::w11, a64::w11); // ~firsttemp
	}
	else
	{
		armAsm->Add(a64::w9, a64::w11, a64::w12);
		armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, offTemp));
	}
	armAsm->Str(a64::w11, a64::MemOperand(RESTATEPTR, offFirst));
}

void armEmitPHMADH(u32 rd, u32 rs, u32 rt)
{
	loadQ(VS, rs);
	loadQ(VT, rt);
	emitPHMPair(/*n*/ 0, EE_LO_OFFSET + 0, EE_LO_OFFSET + 4, /*sub*/ false);
	emitPHMPair(/*n*/ 2, EE_HI_OFFSET + 0, EE_HI_OFFSET + 4, /*sub*/ false);
	emitPHMPair(/*n*/ 4, EE_LO_OFFSET + 8, EE_LO_OFFSET + 12, /*sub*/ false);
	emitPHMPair(/*n*/ 6, EE_HI_OFFSET + 8, EE_HI_OFFSET + 12, /*sub*/ false);
	emitHalfwordMacStoreRd(rd);
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
	emitPHMPair(/*n*/ 0, EE_LO_OFFSET + 0, EE_LO_OFFSET + 4, /*sub*/ true);
	emitPHMPair(/*n*/ 2, EE_HI_OFFSET + 0, EE_HI_OFFSET + 4, /*sub*/ true);
	emitPHMPair(/*n*/ 4, EE_LO_OFFSET + 8, EE_LO_OFFSET + 12, /*sub*/ true);
	emitPHMPair(/*n*/ 6, EE_HI_OFFSET + 8, EE_HI_OFFSET + 12, /*sub*/ true);
	emitHalfwordMacStoreRd(rd);
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

// =============================================================================
// Remaining MMI misc ops (Phase 5.4 completion)
// =============================================================================

// -----------------------------------------------------------------------------
// PLZCW: Count leading sign bits (excluding the sign bit itself)
// -----------------------------------------------------------------------------
// GPR[rd].UL[0] = CountLeadingSignBits(Rs.SL[0]) - 1
// GPR[rd].UL[1] = CountLeadingSignBits(Rs.SL[1]) - 1
void armEmitPLZCW(u32 rd, u32 rs)
{
	if (rd == 0)
		return;

	// Interpreter: GPR[rd].UL[n] = CountLeadingSignBits(Rs.SL[n]) - 1.
	// ARM64 CLS counts leading bits equal to the sign bit *excluding* the sign
	// bit, which is exactly CountLeadingSignBits(x) - 1 — so no adjustment.
	// Lane 0 (low 32 bits)
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Cls(WTEMP2, WTEMP1);
	armAsm->Str(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));

	// Lane 1 (high 32 bits of low 64 bits)
	armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 4));
	armAsm->Cls(WTEMP2, WTEMP1);
	armAsm->Str(WTEMP2, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 4));
}

// -----------------------------------------------------------------------------
// PADSBH: Packed add/subtract halfwords (subtract low 4, add high 4)
// -----------------------------------------------------------------------------
// Rd.US[0..3] = Rs.US[0..3] - Rt.US[0..3] (no saturation, just truncate)
// Rd.US[4..7] = Rs.US[4..7] + Rt.US[4..7] (no saturation, just truncate)
void armEmitPADSBH(u32 rd, u32 rs, u32 rt)
{
	if (rd == 0)
		return;

	loadQ(VS, rs);
	loadQ(VT, rt);

	// Lanes 0-3: subtract (low 4 halfwords)
	for (int i = 0; i < 4; i++) {
		armAsm->Smov(a64::w9, VS.V8H(), i);
		armAsm->Smov(a64::w10, VT.V8H(), i);
		armAsm->Sub(a64::w11, a64::w9, a64::w10);
		armAsm->Ins(VD.V8H(), i, a64::w11); // insert low 16 bits (w11 -> halfword lane i)
	}
	// Lanes 4-7: add (high 4 halfwords)
	for (int i = 4; i < 8; i++) {
		armAsm->Smov(a64::w9, VS.V8H(), i);
		armAsm->Smov(a64::w10, VT.V8H(), i);
		armAsm->Add(a64::w11, a64::w9, a64::w10);
		armAsm->Ins(VD.V8H(), i, a64::w11);
	}

	storeQ(VD, rd);
}

// QFSRV is handled by the interpreter (its shift amount is the runtime SA
// register cpuRegs.sa, not an instruction immediate), so there is no generator.

// -----------------------------------------------------------------------------
// PEXT5: Pack 5-bit fields (extract and expand 5-bit fields to 8-bit)
// -----------------------------------------------------------------------------
// For each 32-bit lane:
//   Rd.UL[n] = ((Rt.UL[n] & 0x1F) << 3) | ((Rt.UL[n] & 0x3E0) << 6) |
//              ((Rt.UL[n] & 0x7C00) << 9) | ((Rt.UL[n] & 0x8000) << 16)
// This expands four 5-bit fields (at bits 0-4, 5-9, 10-14, 15) to four 8-bit fields
void armEmitPEXT5(u32 rd, u32 rt)
{
	if (rd == 0)
		return;

	// Load Rt and process each 32-bit lane
	for (int lane = 0; lane < 4; lane++) {
		u32 offset = EE_GPR_OFFSET(rt) + lane * 4;
		u32 outOffset = EE_GPR_OFFSET(rd) + lane * 4;

		armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, offset));
		// Extract and expand each 5-bit field
		// Field 0: bits 0-4 -> bits 0-7 (<< 3)
		armAsm->And(WTEMP2, WTEMP1, 0x1F);
		armAsm->Lsl(WTEMP2, WTEMP2, 3);
		// Field 1: bits 5-9 -> bits 8-15 (<< 6)
		armAsm->And(WTEMP3, WTEMP1, 0x3E0);
		armAsm->Lsl(WTEMP3, WTEMP3, 6);
		armAsm->Orr(WTEMP2, WTEMP2, WTEMP3);
		// Field 2: bits 10-14 -> bits 16-23 (<< 9)
		armAsm->And(WTEMP3, WTEMP1, 0x7C00);
		armAsm->Lsl(WTEMP3, WTEMP3, 9);
		armAsm->Orr(WTEMP2, WTEMP2, WTEMP3);
		// Field 3: bit 15 -> bits 24-31 (<< 16)
		armAsm->And(WTEMP3, WTEMP1, 0x8000);
		armAsm->Lsl(WTEMP3, WTEMP3, 16);
		armAsm->Orr(WTEMP2, WTEMP2, WTEMP3);

		armAsm->Str(WTEMP2, a64::MemOperand(RESTATEPTR, outOffset));
	}
}

// -----------------------------------------------------------------------------
// PPAC5: Unpack 5-bit fields (compress 8-bit fields to 5-bit)
// -----------------------------------------------------------------------------
// For each 32-bit lane:
//   Rd.UL[n] = ((Rt.UL[n] >> 3) & 0x1F) | ((Rt.UL[n] >> 6) & 0x3E0) |
//              ((Rt.UL[n] >> 9) & 0x7C00) | ((Rt.UL[n] >> 16) & 0x8000)
void armEmitPPAC5(u32 rd, u32 rt)
{
	if (rd == 0)
		return;

	// Load Rt and process each 32-bit lane
	for (int lane = 0; lane < 4; lane++) {
		u32 offset = EE_GPR_OFFSET(rt) + lane * 4;
		u32 outOffset = EE_GPR_OFFSET(rd) + lane * 4;

		armAsm->Ldr(WTEMP1, a64::MemOperand(RESTATEPTR, offset));
		// Compress each 8-bit field to 5 bits
		// Field 0: bits 0-7 -> bits 0-4 (>> 3)
		armAsm->Lsr(WTEMP2, WTEMP1, 3);
		armAsm->And(WTEMP2, WTEMP2, 0x1F);
		// Field 1: bits 8-15 -> bits 5-9 (>> 6)
		armAsm->Lsr(WTEMP3, WTEMP1, 6);
		armAsm->And(WTEMP3, WTEMP3, 0x3E0);
		armAsm->Orr(WTEMP2, WTEMP2, WTEMP3);
		// Field 2: bits 16-23 -> bits 10-14 (>> 9)
		armAsm->Lsr(WTEMP3, WTEMP1, 9);
		armAsm->And(WTEMP3, WTEMP3, 0x7C00);
		armAsm->Orr(WTEMP2, WTEMP2, WTEMP3);
		// Field 3: bits 24-31 -> bit 15 (>> 16)
		armAsm->Lsr(WTEMP3, WTEMP1, 16);
		armAsm->And(WTEMP3, WTEMP3, 0x8000);
		armAsm->Orr(WTEMP2, WTEMP2, WTEMP3);

		armAsm->Str(WTEMP2, a64::MemOperand(RESTATEPTR, outOffset));
	}
}

// -----------------------------------------------------------------------------
// PMFHL: Move from HI/LO (multiple variants based on sa field)
// -----------------------------------------------------------------------------
// sa=0x00 (LW):  Rd = {LO.UL[0], HI.UL[0], LO.UL[2], HI.UL[2]}
// sa=0x01 (UW):  Rd = {LO.UL[1], HI.UL[1], LO.UL[3], HI.UL[3]}
// sa=0x02 (SLW): Rd = sign-clamp 64-bit from HI/LO pairs
// sa=0x03 (LH):  Rd = {LO.US[0], LO.US[2], HI.US[0], HI.US[2], LO.US[4], LO.US[6], HI.US[4], HI.US[6]}
// sa=0x04 (SH):  Rd = signed-saturate 16-bit from HI/LO
bool armEmitPMFHL(u32 rd, u32 sa)
{
	if (rd == 0)
		return true; // interpreter also no-ops when rd==0

	switch (sa)
	{
	case 0x00: // LW: Rd = {LO.UL[0], HI.UL[0], LO.UL[2], HI.UL[2]}
		armAsm->Ldr(a64::x9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
		armAsm->Ldr(a64::x10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
		armAsm->Ldr(a64::x11, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
		armAsm->Ldr(a64::x12, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
		armAsm->Bfi(a64::x11, a64::x12, 32, 32);
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
		return true;

	case 0x01: // UW: Rd = {LO.UL[1], HI.UL[1], LO.UL[3], HI.UL[3]}
		armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET + 4));
		armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET + 4));
		armAsm->Bfi(a64::x9, a64::x10, 32, 32);
		armAsm->Str(a64::x9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd)));
		armAsm->Ldr(a64::w11, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET + 4));
		armAsm->Ldr(a64::w12, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET + 4));
		armAsm->Bfi(a64::x11, a64::x12, 32, 32);
		armAsm->Str(a64::x11, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + 8));
		return true;

	case 0x02: // SLW: clamp each 64-bit {HI.UL[2k]:LO.UL[2k]} to signed 32-bit range
	{
		static const u32 loOff[2] = {EE_LO_OFFSET, EE_LO1_OFFSET};
		static const u32 hiOff[2] = {EE_HI_OFFSET, EE_HI1_OFFSET};
		for (int k = 0; k < 2; k++)
		{
			armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, loOff[k]));  // LO.UL
			armAsm->Ldr(a64::w10, a64::MemOperand(RESTATEPTR, hiOff[k])); // HI.UL
			armAsm->Mov(a64::w11, a64::w9);          // zero-extend LO
			armAsm->Bfi(a64::x11, a64::x10, 32, 32); // TempS64 = LO | (HI<<32)
			armAsm->Sxtw(a64::x13, a64::w9);         // default: (s64)LO.SL
			armAsm->Mov(a64::x12, 0x7fffffff);
			armAsm->Cmp(a64::x11, a64::x12);
			armAsm->Csel(a64::x13, a64::x12, a64::x13, a64::ge); // >= 0x7fffffff
			armAsm->Mov(a64::x12, 0xffffffff80000000);
			armAsm->Cmp(a64::x11, a64::x12);
			armAsm->Csel(a64::x13, a64::x12, a64::x13, a64::le); // <= -0x80000000
			armAsm->Str(a64::x13, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + k * 8));
		}
		return true;
	}

	case 0x03: // LH: pack the even halfwords of LO/HI (kHalfwordMacOff order)
		for (int i = 0; i < 8; i++)
		{
			armAsm->Ldrh(WTEMP1, a64::MemOperand(RESTATEPTR, kHalfwordMacOff[i]));
			armAsm->Strh(WTEMP1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + i * 2));
		}
		return true;

	case 0x04: // SH: signed-saturate each LO/HI word to 16 bits (kHalfwordMacOff order)
		for (int i = 0; i < 8; i++)
		{
			armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, kHalfwordMacOff[i]));
			armAsm->Mov(a64::w10, 0x7fff);
			armAsm->Cmp(a64::w9, a64::w10);
			armAsm->Csel(a64::w9, a64::w10, a64::w9, a64::gt); // > 0x7fff -> 0x7fff
			armAsm->Mov(a64::w10, 0xffff8000);
			armAsm->Cmp(a64::w9, a64::w10);
			armAsm->Csel(a64::w9, a64::w10, a64::w9, a64::lt); // < -0x8000 -> 0x8000
			armAsm->Strh(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rd) + i * 2));
		}
		return true;

	default:
		return false; // unknown variant -> interpreter
	}
}

// -----------------------------------------------------------------------------
// PMTHL: Move to HI/LO (sa=0 only)
// -----------------------------------------------------------------------------
// sa=0: LO = {Rs.UL[0], Rs.UL[1], Rs.UL[2], Rs.UL[3]}
//       HI = {Rs.UL[1], Rs.UL[0], Rs.UL[3], Rs.UL[2]}
// Actually per interpreter: LO.UL[0]=Rs.UL[0], HI.UL[0]=Rs.UL[1], LO.UL[2]=Rs.UL[2], HI.UL[2]=Rs.UL[3]
void armEmitPMTHL(u32 rs, u32 sa)
{
	if (sa != 0)
		return; // only PMTHL.LW (sa=0) is defined; interpreter no-ops otherwise

	// The interpreter writes only the even words, leaving LO/HI.UL[1] and [3]
	// untouched — so use 32-bit stores, not 64-bit (which would clobber them).
	//   LO.UL[0]=Rs.UL[0]  HI.UL[0]=Rs.UL[1]  LO.UL[2]=Rs.UL[2]  HI.UL[2]=Rs.UL[3]
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs)));
	armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, EE_LO_OFFSET));
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 4));
	armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, EE_HI_OFFSET));
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 8));
	armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, EE_LO1_OFFSET));
	armAsm->Ldr(a64::w9, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs) + 12));
	armAsm->Str(a64::w9, a64::MemOperand(RESTATEPTR, EE_HI1_OFFSET));
}
