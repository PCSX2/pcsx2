// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

//------------------------------------------------------------------
// Micro VU - ARM64 NEON Upper Instructions (FMAC pipeline)
//------------------------------------------------------------------

//------------------------------------------------------------------
// NEON Arithmetic Functions
//------------------------------------------------------------------

static void NEON_ADDPS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	mVUclamp3(mVU, to, RQSCRATCH3, _X_Y_Z_W);
	mVUclamp3(mVU, from, RQSCRATCH3, _X_Y_Z_W);
	armAsm->Fadd(to.V4S(), to.V4S(), from.V4S());
	mVUclamp4(mVU, to, RQSCRATCH3, _X_Y_Z_W);
}

static void NEON_SUBPS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	mVUclamp3(mVU, to, RQSCRATCH3, _X_Y_Z_W);
	mVUclamp3(mVU, from, RQSCRATCH3, _X_Y_Z_W);
	armAsm->Fsub(to.V4S(), to.V4S(), from.V4S());
	mVUclamp4(mVU, to, RQSCRATCH3, _X_Y_Z_W);
}

static void NEON_MULPS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	mVUclamp3(mVU, to, RQSCRATCH3, _X_Y_Z_W);
	mVUclamp3(mVU, from, RQSCRATCH3, _X_Y_Z_W);
	armAsm->Fmul(to.V4S(), to.V4S(), from.V4S());
	mVUclamp4(mVU, to, RQSCRATCH3, _X_Y_Z_W);
}

static void NEON_ADDSS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	mVUclamp3(mVU, to, RQSCRATCH3, 0x8);
	mVUclamp3(mVU, from, RQSCRATCH3, 0x8);
	armAsm->Fadd(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()), a64::SRegister(from.GetCode()));
	mVUclamp4(mVU, to, RQSCRATCH3, 0x8);
}

static void NEON_SUBSS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	mVUclamp3(mVU, to, RQSCRATCH3, 0x8);
	mVUclamp3(mVU, from, RQSCRATCH3, 0x8);
	armAsm->Fsub(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()), a64::SRegister(from.GetCode()));
	mVUclamp4(mVU, to, RQSCRATCH3, 0x8);
}

static void NEON_MULSS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	mVUclamp3(mVU, to, RQSCRATCH3, 0x8);
	mVUclamp3(mVU, from, RQSCRATCH3, 0x8);
	armAsm->Fmul(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()), a64::SRegister(from.GetCode()));
	mVUclamp4(mVU, to, RQSCRATCH3, 0x8);
}

// ADD2 variants — ADDi (opType 5). The PS form needs no special handling; the
// SS form implements the tri-ace VuAddSubHack when the gamefix is enabled.
static void NEON_ADD2PS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	NEON_ADDPS(mVU, to, from);
}

// Port of x86 ADD_SS_TriAceHack (microVU_Misc.inl). Tri-ace games need ADDi to be
// bit-accurate: if the two operands' exponents differ by >= 25, the smaller one is
// flushed to a signed zero (sign bit kept, exponent+mantissa of lane 0 cleared —
// the x86 PAND against {0x80000000, ~0, ~0, ~0}) before the scalar add. Unclamped,
// matching x86. Without the gamefix this is a plain scalar add.
static void NEON_ADD2SS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	if (!CHECK_VUADDSUBHACK)
	{
		NEON_ADDSS(mVU, to, from);
		return;
	}

	// Exponent difference (from_exp - to_exp), bits 23..30 of each lane-0 word.
	armAsm->Umov(gprT1.W(), to.V4S(), 0);
	armAsm->Umov(gprT2.W(), from.V4S(), 0);
	armAsm->Ubfx(gprT1.W(), gprT1.W(), 23, 8);
	armAsm->Ubfx(gprT2.W(), gprT2.W(), 23, 8);
	armAsm->Sub(gprT3.W(), gprT2.W(), gprT1.W());

	a64::Label case_neg_big, case_end;
	armAsm->Cmp(gprT3.W(), -25);
	armAsm->B(&case_neg_big, a64::le); // from much smaller -> flush from
	armAsm->Cmp(gprT3.W(), 25);
	armAsm->B(&case_end, a64::lt);     // within range -> no flush

	// to much smaller -> flush to: keep only the sign bit of lane 0.
	armAsm->Umov(gprT1.W(), to.V4S(), 0);
	armAsm->And(gprT1.W(), gprT1.W(), 0x80000000);
	armAsm->Ins(to.V4S(), 0, gprT1.W());
	armAsm->B(&case_end);

	armAsm->Bind(&case_neg_big);
	armAsm->Umov(gprT1.W(), from.V4S(), 0);
	armAsm->And(gprT1.W(), gprT1.W(), 0x80000000);
	armAsm->Ins(from.V4S(), 0, gprT1.W());

	armAsm->Bind(&case_end);
	armAsm->Fadd(a64::SRegister(to.GetCode()), a64::SRegister(to.GetCode()), a64::SRegister(from.GetCode()));
}

//------------------------------------------------------------------
// MAX/MIN — Integer comparison approach
//------------------------------------------------------------------
// IEEE FMAX/FMIN have NaN propagation issues that don't match PS2 behavior.
// Use the same integer comparison trick as x86: convert float bit patterns
// so that the integer comparison order matches the float comparison order.
// For each lane: t = (val >> 31) ? (val ^ 0x7fffffff) : val
// Then CMGT.4S selects the correct operand.

static void NEON_MAXPS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	const a64::VRegister& t1 = mVU.regAlloc->allocReg();
	const a64::VRegister& t2 = mVU.regAlloc->allocReg();

	// Transform 'to' for integer comparison
	armAsm->Sshr(t1.V4S(), to.V4S(), 31);     // All sign bits replicated
	armAsm->Ushr(t1.V4S(), t1.V4S(), 1);       // 0x7fffffff where negative, 0 where positive
	armAsm->Eor(t1.V16B(), t1.V16B(), to.V16B());

	// Transform 'from' for integer comparison
	armAsm->Sshr(t2.V4S(), from.V4S(), 31);
	armAsm->Ushr(t2.V4S(), t2.V4S(), 1);
	armAsm->Eor(t2.V16B(), t2.V16B(), from.V16B());

	// MAX: select 'to' where t1 > t2, else 'from'. Bif (Bitwise Insert if
	// False) writes 'from' into 'to' where the mask is 0 — fuses BSL+Mov.
	armAsm->Cmgt(t1.V4S(), t1.V4S(), t2.V4S());
	armAsm->Bif(to.V16B(), from.V16B(), t1.V16B());

	mVU.regAlloc->clearNeeded(t1);
	mVU.regAlloc->clearNeeded(t2);
}

static void NEON_MINPS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	const a64::VRegister& t1 = mVU.regAlloc->allocReg();
	const a64::VRegister& t2 = mVU.regAlloc->allocReg();

	// Transform 'to' for integer comparison
	armAsm->Sshr(t1.V4S(), to.V4S(), 31);
	armAsm->Ushr(t1.V4S(), t1.V4S(), 1);
	armAsm->Eor(t1.V16B(), t1.V16B(), to.V16B());

	// Transform 'from' for integer comparison
	armAsm->Sshr(t2.V4S(), from.V4S(), 31);
	armAsm->Ushr(t2.V4S(), t2.V4S(), 1);
	armAsm->Eor(t2.V16B(), t2.V16B(), from.V16B());

	// MIN: select 'to' where t2 > t1 (i.e., to < from), else 'from'.
	// Bif fuses the BSL+Mov into a single insn.
	armAsm->Cmgt(t2.V4S(), t2.V4S(), t1.V4S());
	armAsm->Bif(to.V16B(), from.V16B(), t2.V16B());

	mVU.regAlloc->clearNeeded(t1);
	mVU.regAlloc->clearNeeded(t2);
}

static void NEON_MAXSS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	const a64::VRegister& t1 = mVU.regAlloc->allocReg();

	// Transform to[0] — read 'to' straight into the scratch; no copy needed (the
	// in-place shift would only re-read what Sshr can read directly).
	armAsm->Sshr(RQSCRATCH.V4S(), to.V4S(), 31);
	armAsm->Ushr(RQSCRATCH.V4S(), RQSCRATCH.V4S(), 1);
	armAsm->Eor(RQSCRATCH.V16B(), RQSCRATCH.V16B(), to.V16B());

	// Transform from[0] — read 'from' straight into t1; no copy needed.
	armAsm->Sshr(t1.V4S(), from.V4S(), 31);
	armAsm->Ushr(t1.V4S(), t1.V4S(), 1);
	armAsm->Eor(t1.V16B(), t1.V16B(), from.V16B());

	// Compare lane 0 as integers: if to_xform > from_xform, keep to, else take from
	armAsm->Cmgt(RQSCRATCH.V4S(), RQSCRATCH.V4S(), t1.V4S());
	// Use BSL: where mask=1 keep to, where mask=0 keep from
	// Only lane 0 is relevant — write result into to[0]
	armAsm->Bsl(RQSCRATCH.V16B(), to.V16B(), from.V16B());
	armAsm->Ins(to.V4S(), 0, RQSCRATCH.V4S(), 0);

	mVU.regAlloc->clearNeeded(t1);
}

static void NEON_MINSS(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	const a64::VRegister& t1 = mVU.regAlloc->allocReg();

	// Transform to[0] — read 'to' straight into the scratch; no copy needed (the
	// in-place shift would only re-read what Sshr can read directly).
	armAsm->Sshr(RQSCRATCH.V4S(), to.V4S(), 31);
	armAsm->Ushr(RQSCRATCH.V4S(), RQSCRATCH.V4S(), 1);
	armAsm->Eor(RQSCRATCH.V16B(), RQSCRATCH.V16B(), to.V16B());

	// Transform from[0] — read 'from' straight into t1; no copy needed.
	armAsm->Sshr(t1.V4S(), from.V4S(), 31);
	armAsm->Ushr(t1.V4S(), t1.V4S(), 1);
	armAsm->Eor(t1.V16B(), t1.V16B(), from.V16B());

	// MIN: where from_xform > to_xform (i.e., to is smaller), keep to
	armAsm->Cmgt(t1.V4S(), t1.V4S(), RQSCRATCH.V4S());
	armAsm->Bsl(t1.V16B(), to.V16B(), from.V16B());
	armAsm->Ins(to.V4S(), 0, t1.V4S(), 0);

	mVU.regAlloc->clearNeeded(t1);
}

//------------------------------------------------------------------
// Function Pointer Tables
//------------------------------------------------------------------
// opType: 0=ADD, 1=SUB, 2=MUL, 3=MAX, 4=MIN, 5=ADD2

typedef void (*NEONarithPS)(microVU&, const a64::VRegister&, const a64::VRegister&);

static NEONarithPS const NEON_PS[] = {
	NEON_ADDPS,  // 0
	NEON_SUBPS,  // 1
	NEON_MULPS,  // 2
	NEON_MAXPS,  // 3
	NEON_MINPS,  // 4
	NEON_ADD2PS, // 5
};

static NEONarithPS const NEON_SS[] = {
	NEON_ADDSS,  // 0
	NEON_SUBSS,  // 1
	NEON_MULSS,  // 2
	NEON_MAXSS,  // 3
	NEON_MINSS,  // 4
	NEON_ADD2SS, // 5
};

//------------------------------------------------------------------
// Single Scalar Lane Rotation
//------------------------------------------------------------------
// For _XYZW_SS2 (single scalar, NOT X): rotate the target lane into
// lane 0 for scalar operations, then rotate back afterward.
// Uses EXT to rotate the 128-bit vector by N lanes.

// Rotate vector so that lane 'offsetReg' moves to lane 0.
// offsetReg: 0=X(nop), 1=Y, 2=Z, 3=W
static void shuffleSSto0(const a64::VRegister& reg, int lane)
{
	if (lane == 0) return;
	// EXT #(lane*4) rotates left by lane*4 bytes, bringing lane N to position 0
	armAsm->Ext(reg.V16B(), reg.V16B(), reg.V16B(), lane * 4);
}

// Rotate vector back: undo the rotation done by shuffleSSto0.
static void shuffleSSfrom0(const a64::VRegister& reg, int lane)
{
	if (lane == 0) return;
	// Rotate right by lane*4 bytes = rotate left by (16 - lane*4)
	armAsm->Ext(reg.V16B(), reg.V16B(), reg.V16B(), (4 - lane) * 4);
}

//------------------------------------------------------------------
// Clamp Modes
//------------------------------------------------------------------

enum clampModes
{
	cFt  = 0x01, // Clamp Ft / I-reg / Q-reg
	cFs  = 0x02, // Clamp Fs
	cACC = 0x04, // Clamp ACC
};

//------------------------------------------------------------------
// Logging Helper
//------------------------------------------------------------------

static void mVU_printOP(microVU& mVU, int opCase, microOpcode opEnum, bool isACC)
{
	mVUlog(microOpcodeName[opEnum]);
	opCase1 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogFt(); }
	opCase2 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogBC(); }
	opCase3 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogI();  }
	opCase4 { if (isACC) { mVUlogACC(); } else { mVUlogFd(); } mVUlogQ();  }
}

//------------------------------------------------------------------
// Pass 1 Setup (Analysis — platform-independent)
//------------------------------------------------------------------

static void setupPass1(microVU& mVU, int opCase, bool isACC, bool noFlagUpdate)
{
	opCase1 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, _Ft_); }
	opCase2 { mVUanalyzeFMAC3(mVU, ((isACC) ? 0 : _Fd_), _Fs_, _Ft_); }
	opCase3 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, 0); }
	opCase4 { mVUanalyzeFMAC1(mVU, ((isACC) ? 0 : _Fd_), _Fs_, 0); }

	if (noFlagUpdate) // Max/Min ops
		sFLAG.doFlag = false;
}

//------------------------------------------------------------------
// Safe Subtraction — X minus X = 0 (avoids NaN from inf-inf)
//------------------------------------------------------------------

static bool doSafeSub(microVU& mVU, int opCase, int opType, bool isACC)
{
	opCase1
	{
		if ((opType == 1) && (_Ft_ == _Fs_) && (opCase == 1))
		{
			const a64::VRegister& Fs = mVU.regAlloc->allocReg(-1, isACC ? 32 : _Fd_, _X_Y_Z_W);
			armAsm->Movi(Fs.V4S(), 0); // Set to positive zero
			mVUupdateFlags(mVU, Fs);
			mVU.regAlloc->clearNeeded(Fs);
			return true;
		}
	}
	return false;
}

//------------------------------------------------------------------
// Ft Register Setup for Normal/BC/I/Q Cases
//------------------------------------------------------------------

// bcLane: out-param for the AX-14 lane-indexed FMUL broadcast fold. -1 = Ft is
// a materialized full-width operand (the normal path). 0..3 = Ft is the RAW
// source VF register (read-only allocator mapping, broadcast NOT materialized)
// and the caller's multiply step must fold the lane select into a by-element
// FMUL on that lane. Only set when canLaneFold (the sole Ft consumer is the
// multiply step) and no Ft clamp will be emitted.
static void setupFtReg(microVU& mVU, a64::VRegister& Ft, a64::VRegister& tempFt, int opCase, int clampType, int& bcLane, bool canLaneFold)
{
	bcLane = -1;
	opCase1
	{
		const bool willClamp = (clampE || ((clampType & cFt) && !clampE && (CHECK_VU_OVERFLOW(mVU.index) || CHECK_VU_SIGN_OVERFLOW(mVU.index))));

		if (_XYZW_SS2)      { Ft = mVU.regAlloc->allocReg(_Ft_, 0, _X_Y_Z_W); tempFt = Ft; }
		else if (willClamp) { Ft = mVU.regAlloc->allocReg(_Ft_, 0, 0xf);       tempFt = Ft; }
		else                { Ft = mVU.regAlloc->allocReg(_Ft_);                tempFt = a64::NoVReg; }
	}
	opCase2
	{
		// AX-14 (idea from ARMSX2's tryEmitFmulLaneBroadcast — Tyler Bochard,
		// GPLv3): when the only consumer is the multiply step and no Ft clamp
		// will be emitted, skip materializing the broadcast (scratch allocReg +
		// Dup) and let the consumer emit Fmul Vd.4S, Vn.4S, Vm.S[_bc_] —
		// bit-identical per lane (proven on the full 5818-capture corpus), one
		// fewer insn and one fewer NEON reg. willClamp mirrors the opCase1
		// expression: under !willClamp neither the body's clampType&cFt
		// mVUclamp2 nor NEON_MULPS's clampE-gated mVUclamp3/4 emit anything,
		// so bypassing NEON_MULPS loses no clamps (and the raw guest reg is
		// never clamped in place). x86 SSE cannot express this fold.
		const bool willClamp = (clampE || ((clampType & cFt) && !clampE && (CHECK_VU_OVERFLOW(mVU.index) || CHECK_VU_SIGN_OVERFLOW(mVU.index))));
		if (canLaneFold && !willClamp && !_XYZW_SS)
		{
			Ft     = mVU.regAlloc->allocReg(_Ft_); // read-only mapping, held needed until the consumer's Fmul
			tempFt = a64::NoVReg;
			bcLane = _bc_;
			return;
		}
		tempFt = mVU.regAlloc->allocReg(_Ft_);
		Ft     = mVU.regAlloc->allocReg();
		mVUunpack_xyzw(Ft, tempFt, _bc_);
		mVU.regAlloc->clearNeeded(tempFt);
		tempFt = Ft;
	}
	opCase3
	{
		Ft = mVU.regAlloc->allocReg(33, 0, _X_Y_Z_W);
		tempFt = Ft;
	}
	opCase4
	{
		if (!clampE && _XYZW_SS && !mVUinfo.readQ)
		{
			Ft = qmmPQ;
			tempFt = a64::NoVReg;
		}
		else
		{
			Ft = mVU.regAlloc->allocReg();
			tempFt = Ft;
			getQreg(Ft, mVUinfo.readQ);
		}
	}
}

//------------------------------------------------------------------
// mVU_FMACa — Normal FMAC Opcodes (ADD/SUB/MUL/MAX/MIN and ACC variants)
//------------------------------------------------------------------

static void mVU_FMACa(microVU& mVU, int recPass, int opCase, int opType, bool isACC, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, isACC, ((opType == 3) || (opType == 4))); }
	pass2
	{
		if (doSafeSub(mVU, opCase, opType, isACC))
			return;

		a64::VRegister Fs = a64::NoVReg;
		a64::VRegister Ft = a64::NoVReg;
		a64::VRegister ACC = a64::NoVReg;
		a64::VRegister tempFt = a64::NoVReg;
		int bcLane = -1;

		setupFtReg(mVU, Ft, tempFt, opCase, clampType, bcLane, opType == 2);

		if (isACC)
		{
			Fs  = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
			ACC = mVU.regAlloc->allocReg((_X_Y_Z_W == 0xf) ? -1 : 32, 32, 0xf, false);
			if (_XYZW_SS2)
				shuffleSSto0(ACC, offsetReg); // Rotate target lane to lane 0
		}
		else
		{
			Fs = mVU.regAlloc->allocReg(_Fs_, _Fd_, _X_Y_Z_W);
		}

		if ((clampType & cFt) && bcLane < 0) mVUclamp2(mVU, Ft, a64::NoVReg, _X_Y_Z_W);
		if (clampType & cFs)                 mVUclamp2(mVU, Fs, a64::NoVReg, _X_Y_Z_W);

		// AX-14 fold (== Dup + NEON_MULPS under the no-clamp gate). vm MUST be
		// the scalar .S() view: vixl's by-element emitter keys element size off
		// vm's scalar format, and a V4S vm silently selects the half-precision
		// opcode once Devel strips the VIXL_ASSERT.
		if (bcLane >= 0)   armAsm->Fmul(Fs.V4S(), Fs.V4S(), Ft.S(), bcLane);
		else if (_XYZW_SS) NEON_SS[opType](mVU, Fs, Ft);
		else               NEON_PS[opType](mVU, Fs, Ft);

		if (isACC)
		{
			if (_XYZW_SS)
				armAsm->Ins(ACC.V4S(), 0, Fs.V4S(), 0); // MOVSS equivalent
			else
				mVUmergeRegs(ACC, Fs, _X_Y_Z_W);
			mVUupdateFlags(mVU, ACC, Fs, tempFt);
			if (_XYZW_SS2)
				shuffleSSfrom0(ACC, offsetReg); // Rotate lane 0 back to original position
			mVU.regAlloc->clearNeeded(ACC);
		}
		else if (opType < 3 || opType == 5) // Not Min/Max or is ADDi (opType 5)
		{
			mVUupdateFlags(mVU, Fs, tempFt);
		}

		mVU.regAlloc->clearNeeded(Fs); // Always clear written reg first
		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, isACC); }
	pass4
	{
		if ((opType != 3) && (opType != 4))
			mVUregs.needExactMatch |= 8;
	}
}

//------------------------------------------------------------------
// mVU_FMACb — MADDA/MSUBA Opcodes (MUL then ADD/SUB into ACC)
//------------------------------------------------------------------

static void mVU_FMACb(microVU& mVU, int recPass, int opCase, int opType, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, true, false); }
	pass2
	{
		a64::VRegister Fs = a64::NoVReg;
		a64::VRegister Ft = a64::NoVReg;
		a64::VRegister ACC = a64::NoVReg;
		a64::VRegister tempFt = a64::NoVReg;
		int bcLane = -1;

		setupFtReg(mVU, Ft, tempFt, opCase, clampType, bcLane, true);

		Fs  = mVU.regAlloc->allocReg(_Fs_, 0, _X_Y_Z_W);
		ACC = mVU.regAlloc->allocReg(32, 32, 0xf, false);

		if (_XYZW_SS2)
			shuffleSSto0(ACC, offsetReg); // Rotate target lane to lane 0

		if ((clampType & cFt) && bcLane < 0) mVUclamp2(mVU, Ft, a64::NoVReg, _X_Y_Z_W);
		if (clampType & cFs)                 mVUclamp2(mVU, Fs, a64::NoVReg, _X_Y_Z_W);

		// Step 1: Multiply Fs * Ft
		if (bcLane >= 0)   armAsm->Fmul(Fs.V4S(), Fs.V4S(), Ft.S(), bcLane); // AX-14 fold
		else if (_XYZW_SS) NEON_SS[2](mVU, Fs, Ft);
		else               NEON_PS[2](mVU, Fs, Ft);

		// Step 2: ADD/SUB the product to/from ACC
		if (_XYZW_SS || _X_Y_Z_W == 0xf)
		{
			if (_XYZW_SS)
			{
				// ACC is written back with a full 0xf mask, so its non-target
				// lanes must survive this single-lane accumulate. AArch64 scalar
				// FP writes ZERO the upper lanes of the dest V register (unlike
				// x86 ADDSS/SUBSS, which preserve them — the x86 mVU relies on
				// that). Accumulate on a scratch copy and merge only lane 0 back,
				// mirroring the load+Ins pattern mVU_FMACa uses for its ACC.
				const a64::VRegister& accSS = mVU.regAlloc->allocReg();
				armAsm->Mov(accSS.V16B(), ACC.V16B());
				NEON_SS[opType](mVU, accSS, Fs);
				armAsm->Ins(ACC.V4S(), 0, accSS.V4S(), 0);
				mVU.regAlloc->clearNeeded(accSS);
			}
			else
			{
				NEON_PS[opType](mVU, ACC, Fs);
			}
			mVUupdateFlags(mVU, ACC, Fs, tempFt);
			if (_XYZW_SS && _X_Y_Z_W != 8)
				shuffleSSfrom0(ACC, offsetReg); // Rotate back
		}
		else
		{
			const a64::VRegister& tempACC = mVU.regAlloc->allocReg();
			armAsm->Mov(tempACC.V16B(), ACC.V16B());
			NEON_PS[opType](mVU, tempACC, Fs);
			mVUmergeRegs(ACC, tempACC, _X_Y_Z_W);
			mVUupdateFlags(mVU, ACC, Fs, tempFt);
			mVU.regAlloc->clearNeeded(tempACC);
		}

		mVU.regAlloc->clearNeeded(ACC);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, true); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

//------------------------------------------------------------------
// mVU_FMACc — MADD Opcodes (MUL then ADD from ACC into Fd)
//------------------------------------------------------------------
// No FMA: separate MUL then ADD (matches PS2 rounding)

static void mVU_FMACc(microVU& mVU, int recPass, int opCase, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, false, false); }
	pass2
	{
		a64::VRegister Fs = a64::NoVReg;
		a64::VRegister Ft = a64::NoVReg;
		a64::VRegister ACC = a64::NoVReg;
		a64::VRegister tempFt = a64::NoVReg;
		int bcLane = -1;

		setupFtReg(mVU, Ft, tempFt, opCase, clampType, bcLane, true);

		ACC = mVU.regAlloc->allocReg(32);
		Fs  = mVU.regAlloc->allocReg(_Fs_, _Fd_, _X_Y_Z_W);

		if (_XYZW_SS2)
			shuffleSSto0(ACC, offsetReg); // Rotate target lane to lane 0

		if ((clampType & cFt) && bcLane < 0) mVUclamp2(mVU, Ft,  a64::NoVReg, _X_Y_Z_W);
		if (clampType & cFs)                 mVUclamp2(mVU, Fs,  a64::NoVReg, _X_Y_Z_W);
		if (clampType & cACC)                mVUclamp2(mVU, ACC, a64::NoVReg, _X_Y_Z_W);

		// Step 1: Fs = Fs * Ft
		// Step 2: Fs = Fs + ACC
		if (_XYZW_SS) { NEON_SS[2](mVU, Fs, Ft); NEON_SS[0](mVU, Fs, ACC); }
		else
		{
			if (bcLane >= 0) armAsm->Fmul(Fs.V4S(), Fs.V4S(), Ft.S(), bcLane); // AX-14 fold
			else             NEON_PS[2](mVU, Fs, Ft);
			NEON_PS[0](mVU, Fs, ACC);
		}

		if (_XYZW_SS2)
			shuffleSSfrom0(ACC, offsetReg); // Rotate back

		mVUupdateFlags(mVU, Fs, tempFt);

		mVU.regAlloc->clearNeeded(Fs); // Always clear written reg first
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(ACC);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, false); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

//------------------------------------------------------------------
// mVU_FMACd — MSUB Opcodes (ACC - Fs*Ft into Fd)
//------------------------------------------------------------------

static void mVU_FMACd(microVU& mVU, int recPass, int opCase, microOpcode opEnum, int clampType)
{
	pass1 { setupPass1(mVU, opCase, false, false); }
	pass2
	{
		a64::VRegister Fs = a64::NoVReg;
		a64::VRegister Ft = a64::NoVReg;
		a64::VRegister Fd = a64::NoVReg;
		a64::VRegister tempFt = a64::NoVReg;
		int bcLane = -1;

		setupFtReg(mVU, Ft, tempFt, opCase, clampType, bcLane, true);

		Fs = mVU.regAlloc->allocReg(_Fs_,  0, _X_Y_Z_W);
		Fd = mVU.regAlloc->allocReg(32, _Fd_, _X_Y_Z_W);

		if ((clampType & cFt) && bcLane < 0) mVUclamp2(mVU, Ft, a64::NoVReg, _X_Y_Z_W);
		if (clampType & cFs)                 mVUclamp2(mVU, Fs, a64::NoVReg, _X_Y_Z_W);
		if (clampType & cACC)                mVUclamp2(mVU, Fd, a64::NoVReg, _X_Y_Z_W);

		// Step 1: Fs = Fs * Ft
		// Step 2: Fd = Fd - Fs  (Fd starts as ACC)
		if (_XYZW_SS) { NEON_SS[2](mVU, Fs, Ft); NEON_SS[1](mVU, Fd, Fs); }
		else
		{
			if (bcLane >= 0) armAsm->Fmul(Fs.V4S(), Fs.V4S(), Ft.S(), bcLane); // AX-14 fold
			else             NEON_PS[2](mVU, Fs, Ft);
			NEON_PS[1](mVU, Fd, Fs);
		}

		mVUupdateFlags(mVU, Fd, Fs, tempFt);

		mVU.regAlloc->clearNeeded(Fd); // Always clear written reg first
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3 { mVU_printOP(mVU, opCase, opEnum, false); }
	pass4 { mVUregs.needExactMatch |= 8; }
}

//------------------------------------------------------------------
// ABS Opcode
//------------------------------------------------------------------

mVUop(mVU_ABS)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		if (!_Ft_)
			return;
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));
		// PS2 ABS clears the sign bit per lane. Fabs is the dedicated single
		// insn for that on aarch64 and matches AND-with-0x7fffffff for every
		// IEEE bit pattern (normal, denormal, Inf, NaN — sign cleared, payload
		// preserved).
		armAsm->Fabs(Fs.V4S(), Fs.V4S());
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opABS);
	}
	pass3
	{
		mVUlog("ABS");
		mVUlogFtFs();
	}
}

//------------------------------------------------------------------
// OPMULA Opcode — Cross product multiply into ACC
//------------------------------------------------------------------

mVUop(mVU_OPMULA)
{
	pass1 { mVUanalyzeFMAC1(mVU, 0, _Fs_, _Ft_); }
	pass2
	{
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(_Ft_, 0, _X_Y_Z_W);
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 32, _X_Y_Z_W);

		// OPMULA: ACC.xyz = Fs.yzx * Ft.zxy
		// Shuffle Fs: WXZY (0xC9) — puts Y,Z,X in positions 0,1,2
		// ARM64: Fs must be {Fs.y, Fs.z, Fs.x, Fs.w}

		// Fs shuffle: {Y, Z, X, W} — indices 1,2,0,3
		armAsm->Mov(RQSCRATCH.V16B(), Fs.V16B());
		armAsm->Ins(Fs.V4S(), 0, RQSCRATCH.V4S(), 1); // Fs[0] = Y
		armAsm->Ins(Fs.V4S(), 1, RQSCRATCH.V4S(), 2); // Fs[1] = Z
		armAsm->Ins(Fs.V4S(), 2, RQSCRATCH.V4S(), 0); // Fs[2] = X
		// Fs[3] = W (unchanged)

		// Ft shuffle: {Z, X, Y, W} — indices 2,0,1,3
		armAsm->Mov(RQSCRATCH.V16B(), Ft.V16B());
		armAsm->Ins(Ft.V4S(), 0, RQSCRATCH.V4S(), 2); // Ft[0] = Z
		armAsm->Ins(Ft.V4S(), 1, RQSCRATCH.V4S(), 0); // Ft[1] = X
		armAsm->Ins(Ft.V4S(), 2, RQSCRATCH.V4S(), 1); // Ft[2] = Y
		// Ft[3] = W (unchanged)

		NEON_MULPS(mVU, Fs, Ft);
		mVU.regAlloc->clearNeeded(Ft);
		mVUupdateFlags(mVU, Fs);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opOPMULA);
	}
	pass3
	{
		mVUlog("OPMULA");
		mVUlogACC();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

//------------------------------------------------------------------
// OPMSUB Opcode — Cross product subtract from ACC
//------------------------------------------------------------------

mVUop(mVU_OPMSUB)
{
	pass1 { mVUanalyzeFMAC1(mVU, _Fd_, _Fs_, _Ft_); }
	pass2
	{
		const a64::VRegister& Ft  = mVU.regAlloc->allocReg(_Ft_, 0, 0xf);
		const a64::VRegister& Fs  = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const a64::VRegister& ACC = mVU.regAlloc->allocReg(32, _Fd_, _X_Y_Z_W);

		// Fs shuffle: {Y, Z, X, W}
		armAsm->Mov(RQSCRATCH.V16B(), Fs.V16B());
		armAsm->Ins(Fs.V4S(), 0, RQSCRATCH.V4S(), 1);
		armAsm->Ins(Fs.V4S(), 1, RQSCRATCH.V4S(), 2);
		armAsm->Ins(Fs.V4S(), 2, RQSCRATCH.V4S(), 0);

		// Ft shuffle: {Z, X, Y, W}
		armAsm->Mov(RQSCRATCH.V16B(), Ft.V16B());
		armAsm->Ins(Ft.V4S(), 0, RQSCRATCH.V4S(), 2);
		armAsm->Ins(Ft.V4S(), 1, RQSCRATCH.V4S(), 0);
		armAsm->Ins(Ft.V4S(), 2, RQSCRATCH.V4S(), 1);

		NEON_MULPS(mVU, Fs,  Ft);
		NEON_SUBPS(mVU, ACC, Fs);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVUupdateFlags(mVU, ACC);
		mVU.regAlloc->clearNeeded(ACC);
		mVU.profiler.EmitOp(opOPMSUB);
	}
	pass3
	{
		mVUlog("OPMSUB");
		mVUlogFd();
		mVUlogFt();
	}
	pass4 { mVUregs.needExactMatch |= 8; }
}

//------------------------------------------------------------------
// FTOI0/4/12/15 — Float to Int conversion
//------------------------------------------------------------------
// FCVTZS: converts float to signed int, truncating toward zero.
// For unrepresentable positive values, x86 CVTTPS2DQ returns 0x80000000
// and then XORs with 0xffffffff to get 0x7fffffff. ARM64 FCVTZS
// saturates to INT_MAX (0x7fffffff) or INT_MIN (0x80000000) natively for
// finite overflow and ±Inf, so no correction XOR is needed there.
//
// NaN, however, diverges: ARM64 Fcvtzs(NaN)=0, but the interp's floatToInt
// (VUops.cpp) and x86 PCSX2 mVU both saturate a NaN by its sign bit —
// positive NaN → 0x7fffffff, negative NaN → 0x80000000 (exp 0xFF satisfies
// the interp's `>= 0x4f000000` saturation test). Feeding a NaN into FTOI0
// would otherwise produce JIT=0 vs interp=0x7fffffff. NaN lanes are therefore
// patched to the sign-based INT saturation, matching the interp and x86
// exactly (regression-pinned in vu_ftoi_saturation_tests.cpp).

static void mVU_FTOIx(mP, const float* addr, microOpcode opEnum)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		if (!_Ft_)
			return;
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));

		if (addr)
		{
			// Scale by fixed-point multiplier before conversion
			armAsm->Ldr(RQSCRATCH3, mVUglobMem(addr));
			armAsm->Fmul(Fs.V4S(), Fs.V4S(), RQSCRATCH3.V4S());
		}

		// NaN saturation correction (see header comment): build, BEFORE the
		// convert clobbers Fs, a per-lane mask of which lanes are NaN and the
		// sign-based saturation value for those lanes.
		const a64::VRegister& notNan = mVU.regAlloc->allocReg(); // 0xffffffff where NOT NaN
		const a64::VRegister& sat    = mVU.regAlloc->allocReg(); // sign ? INT_MIN : INT_MAX
		armAsm->Fcmeq(notNan.V4S(), Fs.V4S(), Fs.V4S());         // a==a is false only for NaN
		armAsm->Sshr(sat.V4S(), Fs.V4S(), 31);                   // 0xffffffff if sign set, else 0
		armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.absclip[0])); // 0x7fffffff (INT_MAX bits)
		armAsm->Eor(sat.V16B(), sat.V16B(), RQSCRATCH3.V16B());  // +NaN→0x7fffffff, -NaN→0x80000000

		// Convert float to signed int (truncating toward zero). Finite overflow
		// and ±Inf saturate correctly here; NaN lanes become 0 and are fixed up.
		armAsm->Fcvtzs(Fs.V4S(), Fs.V4S());

		// Where notNan==0 (a NaN lane), replace the Fcvtzs 0 with the saturation
		// value; non-NaN lanes keep the converted result. BIF: dst bit <- src bit
		// where mask bit is 0.
		armAsm->Bif(Fs.V16B(), sat.V16B(), notNan.V16B());

		mVU.regAlloc->clearNeeded(notNan);
		mVU.regAlloc->clearNeeded(sat);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3
	{
		mVUlog(microOpcodeName[opEnum]);
		mVUlogFtFs();
	}
}

//------------------------------------------------------------------
// ITOF0/4/12/15 — Int to Float conversion
//------------------------------------------------------------------

static void mVU_ITOFx(mP, const float* addr, microOpcode opEnum)
{
	pass1 { mVUanalyzeFMAC2(mVU, _Fs_, _Ft_); }
	pass2
	{
		if (!_Ft_)
			return;
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, _Ft_, _X_Y_Z_W, !((_Fs_ == _Ft_) && (_X_Y_Z_W == 0xf)));

		// Convert signed int to float
		armAsm->Scvtf(Fs.V4S(), Fs.V4S());

		if (addr)
		{
			// Scale by fixed-point divisor after conversion
			armAsm->Ldr(RQSCRATCH3, mVUglobMem(addr));
			armAsm->Fmul(Fs.V4S(), Fs.V4S(), RQSCRATCH3.V4S());
		}

		mVU.regAlloc->clearNeeded(Fs);
		mVU.profiler.EmitOp(opEnum);
	}
	pass3
	{
		mVUlog(microOpcodeName[opEnum]);
		mVUlogFtFs();
	}
}

//------------------------------------------------------------------
// CLIP Opcode — Clip flag computation
//------------------------------------------------------------------

mVUop(mVU_CLIP)
{
	pass1 { mVUanalyzeFMAC4(mVU, _Fs_, _Ft_); }
	pass2
	{
		const a64::VRegister& Fs = mVU.regAlloc->allocReg(_Fs_, 0, 0xf);
		const a64::VRegister& Ft = mVU.regAlloc->allocReg(_Ft_, 0, 0x1);
		const a64::VRegister& t1 = mVU.regAlloc->allocReg();

		// Broadcast Ft.w to all lanes
		mVUunpack_xyzw(Ft, Ft, 0);

		// Get previous clip flag and shift left by 6
		mVUallocCFLAGa(mVU, gprT1, cFLAG.lastWrite);
		armAsm->Lsl(gprT1.W(), gprT1.W(), 6);

		// Denormal check: if exponent is zero, treat as zero. Use the
		// immediate-zero form of CMEQ to avoid materialising a zero vector.
		armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.exponent[0]));
		armAsm->And(t1.V16B(), Fs.V16B(), RQSCRATCH3.V16B());
		armAsm->Cmeq(t1.V4S(), t1.V4S(), 0); // All 1s where denormal
		armAsm->Bic(t1.V16B(), Fs.V16B(), t1.V16B()); // Zero out denormals

		// |Ft.w| (absolute value)
		armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.absclip[0]));
		armAsm->And(Ft.V16B(), Ft.V16B(), RQSCRATCH3.V16B());

		// Negate Fs (for -w comparison)
		armAsm->Ldr(RQSCRATCH3, mVUglobMem(&mVUglob.signbit[0]));
		armAsm->Eor(Fs.V16B(), t1.V16B(), RQSCRATCH3.V16B());

		// t1 > Ft means +component > +w: bit set
		armAsm->Cmgt(t1.V4S(), t1.V4S(), Ft.V4S());   // +x>+w, +y>+w, +z>+w, (ignored)
		// Fs > Ft means -component > +w (i.e., component < -w): bit set
		armAsm->Cmgt(Fs.V4S(), Fs.V4S(), Ft.V4S());    // -x>+w, -y>+w, -z>+w, (ignored)

		// Extract bits from the comparison results
		// t1 lanes: [0]=+x>w, [1]=+y>w, [2]=+z>w
		// Fs lanes: [0]=-x>w, [1]=-y>w, [2]=-z>w
		// Required layout: bit0=+x>w, bit1=-x>w, bit2=+y>w, bit3=-y>w, bit4=+z>w, bit5=-z>w

		armAsm->Ushr(t1.V4S(), t1.V4S(), 31);
		armAsm->Ushr(Fs.V4S(), Fs.V4S(), 31);

		// Build clip result in gprT2
		armAsm->Umov(gprT2.W(), t1.V4S(), 0); // +x > w → bit 0
		armAsm->Umov(a64::w12, Fs.V4S(), 0);   // -x > w → bit 1
		armAsm->Orr(gprT2.W(), gprT2.W(), a64::Operand(a64::w12, a64::LSL, 1));

		armAsm->Umov(a64::w12, t1.V4S(), 1);   // +y > w → bit 2
		armAsm->Orr(gprT2.W(), gprT2.W(), a64::Operand(a64::w12, a64::LSL, 2));
		armAsm->Umov(a64::w12, Fs.V4S(), 1);   // -y > w → bit 3
		armAsm->Orr(gprT2.W(), gprT2.W(), a64::Operand(a64::w12, a64::LSL, 3));

		armAsm->Umov(a64::w12, t1.V4S(), 2);   // +z > w → bit 4
		armAsm->Orr(gprT2.W(), gprT2.W(), a64::Operand(a64::w12, a64::LSL, 4));
		armAsm->Umov(a64::w12, Fs.V4S(), 2);   // -z > w → bit 5
		armAsm->Orr(gprT2.W(), gprT2.W(), a64::Operand(a64::w12, a64::LSL, 5));

		// Combine with shifted previous clip flag
		armAsm->And(gprT2.W(), gprT2.W(), 0x3f);
		armAsm->And(gprT1.W(), gprT1.W(), 0xffffff);
		armAsm->Orr(gprT1.W(), gprT1.W(), gprT2.W());

		mVUallocCFLAGb(mVU, gprT1, cFLAG.write);
		mVU.regAlloc->clearNeeded(Fs);
		mVU.regAlloc->clearNeeded(Ft);
		mVU.regAlloc->clearNeeded(t1);
		mVU.profiler.EmitOp(opCLIP);
	}
	pass3
	{
		mVUlog("CLIP");
		mVUlogCLIP();
	}
}

//------------------------------------------------------------------
// Micro VU Micromode Upper Instructions — Opcode Dispatch
//------------------------------------------------------------------

mVUop(mVU_ADD)    { mVU_FMACa(mVU, recPass, 1, 0, false, opADD,    0);  }
mVUop(mVU_ADDi)   { mVU_FMACa(mVU, recPass, 3, 5, false, opADDi,   0);  }
mVUop(mVU_ADDq)   { mVU_FMACa(mVU, recPass, 4, 0, false, opADDq,   0);  }
mVUop(mVU_ADDx)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDx,   0);  }
mVUop(mVU_ADDy)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDy,   0);  }
mVUop(mVU_ADDz)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDz,   0);  }
mVUop(mVU_ADDw)   { mVU_FMACa(mVU, recPass, 2, 0, false, opADDw,   0);  }
mVUop(mVU_ADDA)   { mVU_FMACa(mVU, recPass, 1, 0, true,  opADDA,   0);  }
mVUop(mVU_ADDAi)  { mVU_FMACa(mVU, recPass, 3, 0, true,  opADDAi,  0);  }
mVUop(mVU_ADDAq)  { mVU_FMACa(mVU, recPass, 4, 0, true,  opADDAq,  0);  }
mVUop(mVU_ADDAx)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAx,  0);  }
mVUop(mVU_ADDAy)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAy,  0);  }
mVUop(mVU_ADDAz)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAz,  0);  }
mVUop(mVU_ADDAw)  { mVU_FMACa(mVU, recPass, 2, 0, true,  opADDAw,  0);  }
mVUop(mVU_SUB)    { mVU_FMACa(mVU, recPass, 1, 1, false, opSUB,  (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBi)   { mVU_FMACa(mVU, recPass, 3, 1, false, opSUBi, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBq)   { mVU_FMACa(mVU, recPass, 4, 1, false, opSUBq, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBx)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBx, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBy)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBy, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBz)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBz, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBw)   { mVU_FMACa(mVU, recPass, 2, 1, false, opSUBw, (_XYZW_PS)?(cFs|cFt):0);   } // Clamp (Kingdom Hearts I (VU0))
mVUop(mVU_SUBA)   { mVU_FMACa(mVU, recPass, 1, 1, true,  opSUBA,   0);  }
mVUop(mVU_SUBAi)  { mVU_FMACa(mVU, recPass, 3, 1, true,  opSUBAi,  0);  }
mVUop(mVU_SUBAq)  { mVU_FMACa(mVU, recPass, 4, 1, true,  opSUBAq,  0);  }
mVUop(mVU_SUBAx)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAx,  0);  }
mVUop(mVU_SUBAy)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAy,  0);  }
mVUop(mVU_SUBAz)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAz,  0);  }
mVUop(mVU_SUBAw)  { mVU_FMACa(mVU, recPass, 2, 1, true,  opSUBAw,  0);  }
mVUop(mVU_MUL)    { mVU_FMACa(mVU, recPass, 1, 2, false, opMUL,  (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULi)   { mVU_FMACa(mVU, recPass, 3, 2, false, opMULi, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULq)   { mVU_FMACa(mVU, recPass, 4, 2, false, opMULq, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULx)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULx, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULy)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULy, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULz)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULz, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULw)   { mVU_FMACa(mVU, recPass, 2, 2, false, opMULw, (_XYZW_PS)?(cFs|cFt):cFs); } // Clamp (TOTA, DoM, Ice Age (VU0))
mVUop(mVU_MULA)   { mVU_FMACa(mVU, recPass, 1, 2, true,  opMULA,   0);  }
mVUop(mVU_MULAi)  { mVU_FMACa(mVU, recPass, 3, 2, true,  opMULAi,  0);  }
mVUop(mVU_MULAq)  { mVU_FMACa(mVU, recPass, 4, 2, true,  opMULAq,  0);  }
mVUop(mVU_MULAx)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAx,  cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MULAy)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAy,  cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MULAz)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAz,  cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MULAw)  { mVU_FMACa(mVU, recPass, 2, 2, true,  opMULAw, (_XYZW_PS) ? (cFs | cFt) : cFs); } // Clamp (TOTA, DoM, ...) - Ft for Superman
mVUop(mVU_MADD)   { mVU_FMACc(mVU, recPass, 1,           opMADD,   0); }
mVUop(mVU_MADDi)  { mVU_FMACc(mVU, recPass, 3,           opMADDi,  0); }
mVUop(mVU_MADDq)  { mVU_FMACc(mVU, recPass, 4,           opMADDq,  0); }
mVUop(mVU_MADDx)  { mVU_FMACc(mVU, recPass, 2,           opMADDx,  cFs); } // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDy)  { mVU_FMACc(mVU, recPass, 2,           opMADDy,  cFs); } // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDz)  { mVU_FMACc(mVU, recPass, 2,           opMADDz,  cFs); } // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDw)  { mVU_FMACc(mVU, recPass, 2,           opMADDw, (isCOP2)?(cACC|cFt|cFs):cFs);} // Clamp (ICO (COP2), TOTA, DoM)
mVUop(mVU_MADDA)  { mVU_FMACb(mVU, recPass, 1, 0,        opMADDA,  0);  }
mVUop(mVU_MADDAi) { mVU_FMACb(mVU, recPass, 3, 0,        opMADDAi, 0);  }
mVUop(mVU_MADDAq) { mVU_FMACb(mVU, recPass, 4, 0,        opMADDAq, 0);  }
mVUop(mVU_MADDAx) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAx, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDAy) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAy, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDAz) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAz, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MADDAw) { mVU_FMACb(mVU, recPass, 2, 0,        opMADDAw, cFs);} // Clamp (TOTA, DoM, ...)
mVUop(mVU_MSUB)   { mVU_FMACd(mVU, recPass, 1,           opMSUB,  (isCOP2) ? cFs : 0); } // Clamp (Superman)
mVUop(mVU_MSUBi)  { mVU_FMACd(mVU, recPass, 3,           opMSUBi,  0);  }
mVUop(mVU_MSUBq)  { mVU_FMACd(mVU, recPass, 4,           opMSUBq,  0);  }
mVUop(mVU_MSUBx)  { mVU_FMACd(mVU, recPass, 2,           opMSUBx,  0);  }
mVUop(mVU_MSUBy)  { mVU_FMACd(mVU, recPass, 2,           opMSUBy,  0);  }
mVUop(mVU_MSUBz)  { mVU_FMACd(mVU, recPass, 2,           opMSUBz,  0);  }
mVUop(mVU_MSUBw)  { mVU_FMACd(mVU, recPass, 2,           opMSUBw,  0);  }
mVUop(mVU_MSUBA)  { mVU_FMACb(mVU, recPass, 1, 1,        opMSUBA,  0);  }
mVUop(mVU_MSUBAi) { mVU_FMACb(mVU, recPass, 3, 1,        opMSUBAi, 0);  }
mVUop(mVU_MSUBAq) { mVU_FMACb(mVU, recPass, 4, 1,        opMSUBAq, 0);  }
mVUop(mVU_MSUBAx) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAx, 0);  }
mVUop(mVU_MSUBAy) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAy, 0);  }
mVUop(mVU_MSUBAz) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAz, 0);  }
mVUop(mVU_MSUBAw) { mVU_FMACb(mVU, recPass, 2, 1,        opMSUBAw, 0);  }
mVUop(mVU_MAX)    { mVU_FMACa(mVU, recPass, 1, 3, false, opMAX,    0);  }
mVUop(mVU_MAXi)   { mVU_FMACa(mVU, recPass, 3, 3, false, opMAXi,   0);  }
mVUop(mVU_MAXx)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXx,   0);  }
mVUop(mVU_MAXy)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXy,   0);  }
mVUop(mVU_MAXz)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXz,   0);  }
mVUop(mVU_MAXw)   { mVU_FMACa(mVU, recPass, 2, 3, false, opMAXw,   0);  }
mVUop(mVU_MINI)   { mVU_FMACa(mVU, recPass, 1, 4, false, opMINI,   0);  }
mVUop(mVU_MINIi)  { mVU_FMACa(mVU, recPass, 3, 4, false, opMINIi,  0);  }
mVUop(mVU_MINIx)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIx,  0);  }
mVUop(mVU_MINIy)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIy,  0);  }
mVUop(mVU_MINIz)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIz,  0);  }
mVUop(mVU_MINIw)  { mVU_FMACa(mVU, recPass, 2, 4, false, opMINIw,  0);  }
mVUop(mVU_FTOI0)  { mVU_FTOIx(mX, NULL,                  opFTOI0);      }
mVUop(mVU_FTOI4)  { mVU_FTOIx(mX, mVUglob.FTOI_4,        opFTOI4);      }
mVUop(mVU_FTOI12) { mVU_FTOIx(mX, mVUglob.FTOI_12,       opFTOI12);     }
mVUop(mVU_FTOI15) { mVU_FTOIx(mX, mVUglob.FTOI_15,       opFTOI15);     }
mVUop(mVU_ITOF0)  { mVU_ITOFx(mX, NULL,                  opITOF0);      }
mVUop(mVU_ITOF4)  { mVU_ITOFx(mX, mVUglob.ITOF_4,        opITOF4);      }
mVUop(mVU_ITOF12) { mVU_ITOFx(mX, mVUglob.ITOF_12,       opITOF12);     }
mVUop(mVU_ITOF15) { mVU_ITOFx(mX, mVUglob.ITOF_15,       opITOF15);     }
mVUop(mVU_NOP)    { pass2 { mVU.profiler.EmitOp(opNOP); } pass3 { mVUlog("NOP"); } }
