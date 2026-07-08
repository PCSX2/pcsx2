// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <optional>

// ARM64 microVU — misc emit helpers (Phase 7, task 7.5). VIXL port of the
// emit-coupled tail of pcsx2/x86/microVU_Misc.inl.
//
// Already ported elsewhere:
//   * the arch-neutral reg shufflers (mVUunpack_xyzw/mVUloadReg/mVUsaveReg/
//     mVUmergeRegs) — aVU_IR.h (task 7.2b / 7.4 part 1b);
//   * the branch/T-Bit/E-Bit/waitMTVU C helpers — aVU.cpp.
//
// Deferred:
//   * mVUoptimizeConstantAddr — its return-type contract (how a constant host
//     address is handed to a load/store op) is defined by its only consumer, the
//     Lower load/store handlers in task 7.5b.

// Computes the destination PC (byte address) of a relative VU branch from the
// current lower-op PC + the signed 11-bit immediate. x86: microVU_Misc.inl
// branchAddr. Pure (no emit) — used by the branch op handlers (B/BAL/IBxx) and
// the branch drivers in aVU_Branch.inl.
static inline u32 branchAddr(const mV)
{
	pxAssumeMsg(islowerOP, "MicroVU: Expected Lower OP code for valid branch addr.");
	return ((((iPC + 2) + (_Imm11_ * 2)) & mVU.progMemMask) * 4);
}

//------------------------------------------------------------------
// Small absolute-address memory-access helpers (shared by Tables/Flags/Branch)
//------------------------------------------------------------------
// x86 folded an absolute &global into a ptr32/ptr128 memory operand; ARM64 must
// first materialize the address. armMoveAddressToReg clobbers RSCRATCH (x16/x17),
// so the value regs passed in must never be x16/x17 (callers use w9/w10 or the
// flag GPRs / NEON scratch).

static inline void mvuStr32(const void* addr, const a64::Register& wreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(wreg.W(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdr32(const a64::Register& wreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(wreg.W(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuStrImm32(const void* addr, u32 imm, const a64::Register& tmp)
{
	armAsm->Mov(tmp.W(), imm);
	mvuStr32(addr, tmp);
}

static inline void mvuStrSS(const void* addr, const a64::VRegister& vreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(vreg.S(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdrSS(const a64::VRegister& vreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(vreg.S(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuLdrQ(const a64::VRegister& vreg, const void* addr)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(vreg.Q(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuStrQ(const void* addr, const a64::VRegister& vreg)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Str(vreg.Q(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuMemAndImm32(const void* addr, u32 imm, const a64::Register& tmp)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(tmp.W(), a64::MemOperand(RSCRATCHADDR));
	armAsm->And(tmp.W(), tmp.W(), imm);
	armAsm->Str(tmp.W(), a64::MemOperand(RSCRATCHADDR));
}

static inline void mvuMemOrImm32(const void* addr, u32 imm, const a64::Register& tmp)
{
	armMoveAddressToReg(RSCRATCHADDR, addr);
	armAsm->Ldr(tmp.W(), a64::MemOperand(RSCRATCHADDR));
	armAsm->Orr(tmp.W(), tmp.W(), imm);
	armAsm->Str(tmp.W(), a64::MemOperand(RSCRATCHADDR));
}

//------------------------------------------------------------------
// Volatile-register backup / restore around opaque C calls
//------------------------------------------------------------------
// x86: microVU_Misc.inl mVUbackupRegs/mVUrestoreRegs.
//   * !toMemory (the common case, e.g. normJumpCompile): flush the reg cache and
//     stash only the PQ NEON reg in mVU.vecBackup — the dispatcher's XGKICK resume
//     path (mVUdispatcherCD) reloads it from there.
//   * toMemory (debug-only: handleBadOp / mVUdebugPrintBlocks / DumpVUState): the
//     regAlloc isn't told about the call, so push every caller-saved host reg that
//     can hold live VU state to the stack and pop it after. We save the full fixed
//     caller-saved set (GPR x0-x15, NEON v0-v24 = VF pool + PQ) rather than the x86
//     "onlyNeeded" subset — over-saving is correct and these paths are rare. The
//     frame layout here MUST match mVUrestoreRegs.
static constexpr int kBakGprSave = 16; // x0..x15
static constexpr int kBakVecSave = 25; // v0..v24 (VF pool + PQ)
static constexpr int kBakGprBytes = kBakGprSave * 8;
static constexpr int kBakFrame = kBakGprBytes + ((kBakVecSave * 16 + 15) & ~15);

__fi void mVUbackupRegs(microVU& mVU, bool toMemory = false, bool onlyNeeded = false)
{
	(void)onlyNeeded;
	if (toMemory)
	{
		armAsm->Sub(a64::sp, a64::sp, kBakFrame);
		for (int i = 0; i < kBakGprSave; i += 2)
			armAsm->Stp(armXRegister(i), armXRegister(i + 1), a64::MemOperand(a64::sp, i * 8));
		int voff = kBakGprBytes;
		for (int i = 0; i < kBakVecSave - 1; i += 2, voff += 32)
			armAsm->Stp(armQRegister(i), armQRegister(i + 1), a64::MemOperand(a64::sp, voff));
		armAsm->Str(armQRegister(kBakVecSave - 1), a64::MemOperand(a64::sp, voff));
	}
	else
	{
		mVU.regAlloc->flushAll(); // Flush Regalloc
		armMoveAddressToReg(RSCRATCHADDR, &mVU.vecBackup[mVU_xmmPQ.GetCode()][0]);
		armAsm->Str(mVU_xmmPQ.Q(), a64::MemOperand(RSCRATCHADDR));
	}
}

__fi void mVUrestoreRegs(microVU& mVU, bool fromMemory = false, bool onlyNeeded = false)
{
	(void)onlyNeeded;
	if (fromMemory)
	{
		int voff = kBakGprBytes;
		for (int i = 0; i < kBakVecSave - 1; i += 2, voff += 32)
			armAsm->Ldp(armQRegister(i), armQRegister(i + 1), a64::MemOperand(a64::sp, voff));
		armAsm->Ldr(armQRegister(kBakVecSave - 1), a64::MemOperand(a64::sp, voff));
		for (int i = 0; i < kBakGprSave; i += 2)
			armAsm->Ldp(armXRegister(i), armXRegister(i + 1), a64::MemOperand(a64::sp, i * 8));
		armAsm->Add(a64::sp, a64::sp, kBakFrame);
	}
	else
	{
		armMoveAddressToReg(RSCRATCHADDR, &mVU.vecBackup[mVU_xmmPQ.GetCode()][0]);
		armAsm->Ldr(mVU_xmmPQ.Q(), a64::MemOperand(RSCRATCHADDR));
	}
}

// Transforms the VU address (a quadword index) in gprReg into a byte offset into
// VU memory, applying the VU0/VU1 wrap and the VU0->VU1 register-window remap.
// x86: mVUaddrFix. Modifies gprReg in place (and tmpReg on the far-offset path).
// gprReg/tmpReg are 64-bit (X) registers; the masking steps use the 32-bit view
// (W-writes zero bits [63:32], matching the x86 32-bit AND).
__fi void mVUaddrFix(mV, const a64::Register& gprReg, const a64::Register& tmpReg)
{
	if (isVU1)
	{
		armAsm->And(gprReg.W(), gprReg.W(), 0x3ff); // wrap around (1024 quadwords)
		armAsm->Lsl(gprReg.W(), gprReg.W(), 4);     // * 16 -> byte offset
	}
	else
	{
		a64::Label jmpA, jmpB;
		armAsm->Tst(gprReg.W(), 0x400);
		armAsm->B(&jmpA, a64::ne); // if (addr & 0x400): reads VU1's VF/VI regs
			armAsm->And(gprReg.W(), gprReg.W(), 0xff); // else wrap (256 quadwords)
			armAsm->B(&jmpB);
		armAsm->Bind(&jmpA);
			if (THREAD_VU1)
				armEmitCall(reinterpret_cast<const void*>(mVU.waitMTVU));
			armAsm->And(gprReg.W(), gprReg.W(), 0x3f); // ToDo: VU0 may override VU1's VF0/VI0
			const sptr offset = (u128*)VU1.VF - (u128*)VU0.Mem;
			if (offset == static_cast<s32>(offset))
			{
				armAsm->Add(gprReg, gprReg, static_cast<s32>(offset));
			}
			else
			{
				armAsm->Mov(tmpReg, offset);
				armAsm->Add(gprReg, gprReg, tmpReg);
			}
		armAsm->Bind(&jmpB);
		armAsm->Lsl(gprReg, gprReg, 4); // * 16 -> byte offset (64-bit)
	}
}

// If the VU address source register is the always-zero VI0, the effective VU
// memory address is a compile-time constant — return the absolute host pointer
// so the load/store handler can skip the runtime moveVIToGPR + mVUaddrFix dance.
// x86 returned an xAddressVoid (a folded ptr operand); ARM64 has no implicit
// memory operands, so the contract is "absolute host address, materialized by
// the caller via armMoveAddressToReg". std::nullopt ⇒ must compute at runtime.
__fi std::optional<const void*> mVUoptimizeConstantAddr(mV, u32 srcreg, s32 offset, s32 offsetSS_)
{
	// if we had const prop for VIs, we could do that here..
	if (srcreg != 0)
		return std::nullopt;

	const s32 addr = 0 + offset;
	if (isVU1)
	{
		return (const void*)(mVU.regs().Mem + ((addr & 0x3FFu) << 4) + offsetSS_);
	}
	else
	{
		if (addr & 0x400)
			return std::nullopt;

		return (const void*)(mVU.regs().Mem + ((addr & 0xFFu) << 4) + offsetSS_);
	}
}

//------------------------------------------------------------------
// Micro VU - Custom SSE Instructions (x86: microVU_Misc.inl SSE_*)
//------------------------------------------------------------------
// VIXL port of the VU FMAC arithmetic primitives. The VU's MIN/MAX are NOT IEEE
// min/max — they're a signed-magnitude *integer* compare on the float bit pattern,
// so MIN_MAX_PS uses the integer-comparison path (the x86 `if (0)` double path is
// dropped). MIN_MAX_SS keeps the double-precision trick (it has no integer form):
// the two float lanes are packed into a finite normal double whose ordering matches
// the float magnitude/sign ordering, so plain FMIN/FMAX on .V2D() is exact (the
// constructed doubles are never NaN, so NEON's NaN-propagation is never hit).
//
// The add/sub/mul/div primitives go through mVUclampedArith: when the extra-overflow
// gamefix (clampE) is on, operands are sign-clamped before and the result range-
// clamped after. clampE is off by default, so it normally emits just the NEON op.

alignas(16) static const u32 mVU_MIN_MAX_1[4] = {0xffffffff, 0x80000000, 0xffffffff, 0x80000000};
alignas(16) static const u32 mVU_MIN_MAX_2[4] = {0x00000000, 0x40000000, 0x00000000, 0x40000000};
alignas(16) static const u32 mVU_ADD_SS[4]    = {0x80000000, 0xffffffff, 0xffffffff, 0xffffffff};

// Warning: Modifies t1 and t2
static void MIN_MAX_PS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1in, const a64::VRegister& t2in, bool min)
{
	const bool t1b = t1in.IsNone();
	const bool t2b = t2in.IsNone();
	const a64::VRegister t1 = t1b ? mVU.regAlloc->allocReg() : t1in;
	const a64::VRegister t2 = t2b ? mVU.regAlloc->allocReg() : t2in;

	// integer comparison (signed-magnitude transform of the float bit pattern)
	const a64::VRegister& c1 = min ? t2 : t1;
	const a64::VRegister& c2 = min ? t1 : t2;

	armAsm->Mov (t1.V16B(), to.V16B());
	armAsm->Sshr(t1.V4S(), t1.V4S(), 31);
	armAsm->Ushr(t1.V4S(), t1.V4S(), 1);
	armAsm->Eor (t1.V16B(), t1.V16B(), to.V16B());

	armAsm->Mov (t2.V16B(), from.V16B());
	armAsm->Sshr(t2.V4S(), t2.V4S(), 31);
	armAsm->Ushr(t2.V4S(), t2.V4S(), 1);
	armAsm->Eor (t2.V16B(), t2.V16B(), from.V16B());

	armAsm->Cmgt(c1.V4S(), c1.V4S(), c2.V4S());      // c1 = (c1 > c2) ? -1 : 0 (signed)
	armAsm->And (to.V16B(), to.V16B(), c1.V16B());
	armAsm->Bic (c1.V16B(), from.V16B(), c1.V16B()); // c1 = from & ~c1 (x86 PANDN)
	armAsm->Orr (to.V16B(), to.V16B(), c1.V16B());

	if (t1b) mVU.regAlloc->clearNeeded(t1);
	if (t2b) mVU.regAlloc->clearNeeded(t2);
}

// Warning: Modifies to's upper 3 vectors, and t1
static void MIN_MAX_SS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1in, bool min)
{
	const bool t1b = t1in.IsNone();
	const a64::VRegister t1 = t1b ? mVU.regAlloc->allocReg() : t1in;

	// to = { to0, to0, from0, from0 }  (x86 xSHUF.PS(to, from, 0))
	armAsm->Ins(to.V4S(), 1, to.V4S(), 0);
	armAsm->Ins(to.V4S(), 2, from.V4S(), 0);
	armAsm->Ins(to.V4S(), 3, from.V4S(), 0);

	mvuLdrQ(RQSCRATCH, mVU_MIN_MAX_1);
	armAsm->And(to.V16B(), to.V16B(), RQSCRATCH.V16B());
	mvuLdrQ(RQSCRATCH, mVU_MIN_MAX_2);
	armAsm->Orr(to.V16B(), to.V16B(), RQSCRATCH.V16B());

	mVUshufflePS(t1, to, 0xee); // t1 = { to2, to3, to2, to3 }
	if (min) armAsm->Fmin(to.V2D(), to.V2D(), t1.V2D());
	else     armAsm->Fmax(to.V2D(), to.V2D(), t1.V2D());

	if (t1b) mVU.regAlloc->clearNeeded(t1);
}

// Turns out only this is needed to get TriAce games booting with mVU.
// Modifies from's lower vector. (x86: ADD_SS_TriAceHack)
static void ADD_SS_TriAceHack(mV, const a64::VRegister& to, const a64::VRegister& from)
{
	armAsm->Fmov(gprT1.W(), to.S());
	armAsm->Fmov(gprT2.W(), from.S());
	armAsm->Lsr (gprT1.W(), gprT1.W(), 23);
	armAsm->Lsr (gprT2.W(), gprT2.W(), 23);
	armAsm->And (gprT1.W(), gprT1.W(), 0xff);
	armAsm->And (gprT2.W(), gprT2.W(), 0xff);
	armAsm->Sub (gprT2.W(), gprT2.W(), gprT1.W()); // exponent difference

	a64::Label case_neg_big, case_end1, case_end2;

	armAsm->Cmp(gprT2.W(), -25);
	armAsm->B(&case_neg_big, a64::le);
	armAsm->Cmp(gprT2.W(), 25);
	armAsm->B(&case_end1, a64::lt);

	// case_pos_big:
	mvuLdrQ(RQSCRATCH, mVU_ADD_SS);
	armAsm->And(to.V16B(), to.V16B(), RQSCRATCH.V16B());
	armAsm->B(&case_end2);

	armAsm->Bind(&case_neg_big);
	mvuLdrQ(RQSCRATCH, mVU_ADD_SS);
	armAsm->And(from.V16B(), from.V16B(), RQSCRATCH.V16B());

	armAsm->Bind(&case_end1);
	armAsm->Bind(&case_end2);

	armAsm->Fadd(to.S(), to.S(), from.S());
}

// to (op)= from, sign-clamping operands and range-clamping the result when the
// extra-overflow gamefix is enabled (x86: clampOp macro). isPS selects 4-lane vs
// single-scalar. t1 is the caller-provided clamp scratch (xEmptyReg ⇒ RQSCRATCH).
enum mVUarithOp { mVU_ADD_OP, mVU_SUB_OP, mVU_MUL_OP, mVU_DIV_OP };

static void mVUclampedArith(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1, int op, bool isPS)
{
	const a64::VRegister ct = t1.IsNone() ? RQSCRATCH : t1;
	const int xyzw = isPS ? 0xf : 0x8;
	mVUclamp3(mVU, to, ct, xyzw);
	mVUclamp3(mVU, from, ct, xyzw);
	if (isPS)
	{
		switch (op)
		{
			case mVU_ADD_OP: armAsm->Fadd(to.V4S(), to.V4S(), from.V4S()); break;
			case mVU_SUB_OP: armAsm->Fsub(to.V4S(), to.V4S(), from.V4S()); break;
			case mVU_MUL_OP: armAsm->Fmul(to.V4S(), to.V4S(), from.V4S()); break;
			case mVU_DIV_OP: armAsm->Fdiv(to.V4S(), to.V4S(), from.V4S()); break;
		}
	}
	else
	{
		// AArch64 scalar FP ops write the result to Sd and ZERO the upper bits
		// [127:32] of the V register — unlike x86 ADDSS/MULSS, which preserve the
		// upper 3 lanes. The microVU single-scalar (_XYZW_SS) model shuffles the
		// target lane into lane0, operates, then shuffles back, and DEPENDS on the
		// other lanes surviving (e.g. mVU_FMACb's MADDAw.z accumulates into ACC.z
		// while ACC.x/.y must be preserved). Writing `to.S()` in place would wipe
		// them. Compute into a scratch and insert only lane0 of `to`.
		const a64::VRegister sres = RQSCRATCH;
		switch (op)
		{
			case mVU_ADD_OP: armAsm->Fadd(sres.S(), to.S(), from.S()); break;
			case mVU_SUB_OP: armAsm->Fsub(sres.S(), to.S(), from.S()); break;
			case mVU_MUL_OP: armAsm->Fmul(sres.S(), to.S(), from.S()); break;
			case mVU_DIV_OP: armAsm->Fdiv(sres.S(), to.S(), from.S()); break;
		}
		armAsm->Ins(to.V4S(), 0, sres.V4S(), 0);
	}
	mVUclamp4(mVU, to, ct, xyzw);
}

static void SSE_MAXPS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { MIN_MAX_PS(mVU, to, from, t1, t2, false); }
static void SSE_MINPS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { MIN_MAX_PS(mVU, to, from, t1, t2, true); }
static void SSE_MAXSS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { MIN_MAX_SS(mVU, to, from, t1, false); }
static void SSE_MINSS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { MIN_MAX_SS(mVU, to, from, t1, true); }
static void SSE_ADD2SS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg)
{
	if (!CHECK_VUADDSUBHACK) mVUclampedArith(mVU, to, from, t1, mVU_ADD_OP, false);
	else                     ADD_SS_TriAceHack(mVU, to, from);
}
// Does same as SSE_ADDPS since tri-ace games only need SS implementation of VUADDSUBHACK...
static void SSE_ADD2PS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_ADD_OP, true); }
static void SSE_ADDPS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_ADD_OP, true); }
static void SSE_ADDSS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_ADD_OP, false); }
static void SSE_SUBPS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_SUB_OP, true); }
static void SSE_SUBSS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_SUB_OP, false); }
static void SSE_MULPS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_MUL_OP, true); }
static void SSE_MULSS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_MUL_OP, false); }
static void SSE_DIVPS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_DIV_OP, true); }
static void SSE_DIVSS(mV, const a64::VRegister& to, const a64::VRegister& from, const a64::VRegister& t1 = xEmptyReg, const a64::VRegister& t2 = xEmptyReg) { mVUclampedArith(mVU, to, from, t1, mVU_DIV_OP, false); }
