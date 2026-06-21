// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU - ARM64 NEON Clamp Functions
//------------------------------------------------------------------

// Result clamping: clamp to [minFloat, maxFloat].
// Uses FMINNM/FMAXNM (number-preserving) so NaN inputs clamp to ±maxfloat,
// matching x86 SSE MINPS/MAXPS NaN-eating semantics. Plain FMIN/FMAX are
// IEEE-strict and propagate NaN — using them here lets NaN flow through
// matrix-multiply chains and corrupts vertex output.
void mVUclamp1(microVU& mVU, const a64::VRegister& reg, const a64::VRegister& regT1, int xyzw, bool bClampE = false)
{
	if (((!clampE && CHECK_VU_OVERFLOW(mVU.index)) || (clampE && bClampE)) && mVU.regAlloc->checkVFClamp(reg.GetCode()))
	{
		switch (xyzw)
		{
			case 1: case 2: case 4: case 8:
			{
				armAsm->Ldr(a64::VRegister(RQSCRATCH3.GetCode(), 32), mVUglobMem(&mVUglob.maxvals[0]));
				armAsm->Fminnm(a64::VRegister(reg.GetCode(), 32), a64::VRegister(reg.GetCode(), 32),
				               a64::VRegister(RQSCRATCH3.GetCode(), 32));
				armAsm->Ldr(a64::VRegister(RQSCRATCH3.GetCode(), 32), mVUglobMem(&mVUglob.minvals[0]));
				armAsm->Fmaxnm(a64::VRegister(reg.GetCode(), 32), a64::VRegister(reg.GetCode(), 32),
				               a64::VRegister(RQSCRATCH3.GetCode(), 32));
				break;
			}
			default:
			{
				armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.maxvals[0]));
				armAsm->Fminnm(reg.V4S(), reg.V4S(), RQSCRATCH3.V4S());
				armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.minvals[0]));
				armAsm->Fmaxnm(reg.V4S(), reg.V4S(), RQSCRATCH3.V4S());
				break;
			}
		}
	}
}

// Operand clamping with sign preservation.
// Uses integer SMIN/UMIN to preserve NaN sign bit.
void mVUclamp2(microVU& mVU, const a64::VRegister& reg, const a64::VRegister& regT1in, int xyzw, bool bClampE = false)
{
	if (((!clampE && CHECK_VU_SIGN_OVERFLOW(mVU.index)) || (clampE && bClampE && CHECK_VU_SIGN_OVERFLOW(mVU.index))) && mVU.regAlloc->checkVFClamp(reg.GetCode()))
	{
		// Integer min/max to preserve NaN sign
		// SMIN.4S clamps the signed integer representation
		// UMIN.4S clamps the unsigned integer representation
		armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.maxvals[0]));
		armAsm->Smin(reg.V4S(), reg.V4S(), RQSCRATCH3.V4S());
		armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.minvals[0]));
		armAsm->Umin(reg.V4S(), reg.V4S(), RQSCRATCH3.V4S());
		return;
	}
	else
	{
		mVUclamp1(mVU, reg, regT1in, xyzw, bClampE);
	}
}

// Operand clamping for every arithmetic op (only when extra overflow enabled)
void mVUclamp3(microVU& mVU, const a64::VRegister& reg, const a64::VRegister& regT1, int xyzw)
{
	if (clampE && mVU.regAlloc->checkVFClamp(reg.GetCode()))
		mVUclamp2(mVU, reg, regT1, xyzw, true);
}

// Result clamping for every arithmetic op (when extra overflow but not sign-preserving)
void mVUclamp4(microVU& mVU, const a64::VRegister& reg, const a64::VRegister& regT1, int xyzw)
{
	if (clampE && !CHECK_VU_SIGN_OVERFLOW(mVU.index) && mVU.regAlloc->checkVFClamp(reg.GetCode()))
		mVUclamp1(mVU, reg, regT1, xyzw, true);
}
