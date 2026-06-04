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
