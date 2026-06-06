// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU recompiler — operand/result clamp helpers (Phase 7, task 7.5).
//
// VIXL port of pcsx2/x86/microVU_Clamp.inl (mVUclamp1..4). The VU recs clamp
// float operands/results into the representable single-precision range to mirror
// the PS2 VU's saturation behaviour. Two clamp flavours:
//   * range clamp (mVUclamp1) — MIN/MAX against ±0x7f7fffff (largest finite
//     magnitude). x86 MINPS/MAXPS return the *number* when one operand is NaN, so
//     NaN results collapse to +fmax; the matching NEON op is FMINNM/FMAXNM (the
//     IEEE minNum/maxNum that keep the number, unlike FMIN/FMAX which propagate
//     NaN). Sign of NaN is deliberately not preserved (see x86 comment).
//   * sign-preserving clamp (mVUclamp2) — integer PMINSD/PMINUD on the float bit
//     pattern → NEON Smin/Umin (V4S). Preserves the sign of NaNs.
//
// Translations vs x86:
//   * reg.Id -> reg.GetCode(); the regT1 param is the caller-provided NEON scratch
//     used to hold the loaded constant vector (x86 folded it as a memory operand).
//   * absolute `ptr128[&const]` -> armMoveAddressToReg(RSCRATCHADDR,..) + Ldr(.Q()).
//   * single-subvector (SS) clamps operate on lane0 (.S()); in the microVU SS model
//     the live value sits in lane0 and the upper lanes are scratch, so the NEON
//     scalar-op upper-lane zeroing (vs x86 xMIN.SS preserving them) is benign.

//------------------------------------------------------------------
// Sign-overflow clamp tables (x86: microVU_Clamp.inl sse4_min/maxvals).
// [0] = 1000 (only lane0/X differs), [1] = 1111 (all lanes).
//------------------------------------------------------------------
alignas(16) static const u32 sse4_minvals[2][4] = {
	{0xff7fffff, 0xffffffff, 0xffffffff, 0xffffffff}, //1000
	{0xff7fffff, 0xff7fffff, 0xff7fffff, 0xff7fffff}, //1111
};
alignas(16) static const u32 sse4_maxvals[2][4] = {
	{0x7f7fffff, 0x7fffffff, 0x7fffffff, 0x7fffffff}, //1000
	{0x7f7fffff, 0x7f7fffff, 0x7f7fffff, 0x7f7fffff}, //1111
};

// Result clamping. NaN sign is not preserved (collapses to +fmax).
static void mVUclamp1(mV, const a64::VRegister& reg, const a64::VRegister& regT1, int xyzw, bool bClampE = false)
{
	if (((!clampE && CHECK_VU_OVERFLOW(mVU.index)) || (clampE && bClampE)) && mVU.regAlloc->checkVFClamp(reg.GetCode()))
	{
		const bool ss = (xyzw == 1) || (xyzw == 2) || (xyzw == 4) || (xyzw == 8);

		armMoveAddressToReg(RSCRATCHADDR, mVUglob.maxvals);
		armAsm->Ldr(regT1.Q(), a64::MemOperand(RSCRATCHADDR));
		if (ss)
			armAsm->Fminnm(reg.S(), reg.S(), regT1.S());
		else
			armAsm->Fminnm(reg.V4S(), reg.V4S(), regT1.V4S());

		armMoveAddressToReg(RSCRATCHADDR, mVUglob.minvals);
		armAsm->Ldr(regT1.Q(), a64::MemOperand(RSCRATCHADDR));
		if (ss)
			armAsm->Fmaxnm(reg.S(), reg.S(), regT1.S());
		else
			armAsm->Fmaxnm(reg.V4S(), reg.V4S(), regT1.V4S());
	}
}

// Operand clamping (sign-preserving when sign-overflow clamp is enabled).
static void mVUclamp2(mV, const a64::VRegister& reg, const a64::VRegister& regT1, int xyzw, bool bClampE = false)
{
	if (((!clampE && CHECK_VU_SIGN_OVERFLOW(mVU.index)) || (clampE && bClampE && CHECK_VU_SIGN_OVERFLOW(mVU.index))) && mVU.regAlloc->checkVFClamp(reg.GetCode()))
	{
		const int i = ((xyzw == 1) || (xyzw == 2) || (xyzw == 4) || (xyzw == 8)) ? 0 : 1;

		armMoveAddressToReg(RSCRATCHADDR, &sse4_maxvals[i][0]);
		armAsm->Ldr(regT1.Q(), a64::MemOperand(RSCRATCHADDR));
		armAsm->Smin(reg.V4S(), reg.V4S(), regT1.V4S());

		armMoveAddressToReg(RSCRATCHADDR, &sse4_minvals[i][0]);
		armAsm->Ldr(regT1.Q(), a64::MemOperand(RSCRATCHADDR));
		armAsm->Umin(reg.V4S(), reg.V4S(), regT1.V4S());
		return;
	}
	else
		mVUclamp1(mVU, reg, regT1, xyzw, bClampE);
}

// Operand clamp on every SSE-equivalent arithmetic instruction (add/sub/mul/div).
static void mVUclamp3(mV, const a64::VRegister& reg, const a64::VRegister& regT1, int xyzw)
{
	if (clampE && mVU.regAlloc->checkVFClamp(reg.GetCode()))
		mVUclamp2(mVU, reg, regT1, xyzw, true);
}

// Result clamp on every SSE-equivalent arithmetic instruction. Disabled in
// "preserve sign" mode (operands already clamped by mVUclamp3); precaution only.
static void mVUclamp4(mV, const a64::VRegister& reg, const a64::VRegister& regT1, int xyzw)
{
	if (clampE && !CHECK_VU_SIGN_OVERFLOW(mVU.index) && mVU.regAlloc->checkVFClamp(reg.GetCode()))
		mVUclamp1(mVU, reg, regT1, xyzw, true);
}
