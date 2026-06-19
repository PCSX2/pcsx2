// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "arm64/aR5900Analysis.h" // g_pCurInstInfo + the M1 EEINST_COP2_* flag bits

// ARM64 microVU — macro mode native ALU emission (Phase 7.9 / M5).
//
// This is the parallel clone of the *emitter-coupled* half of
// pcsx2/x86/microVU_Macro.inl: setupMacroOp / endMacroOp + the REC_COP2_mVU0
// dispatch that reuses the microVU0 single-op NEON emitters (mVU_ABS, mVU_ITOFx,
// ...) to execute a COP2 SPECIAL ALU op natively on the EE thread, against
// vuRegs[0] in memory. It lives in the aVU translation unit (included by aVU.cpp
// after aVU_Upper.inl / aVU_Lower.inl) because those emitters are static to that
// TU — the same reason x86 hosts microVU_Macro.inl inside microVU.cpp.
//
// Split vs x86 (intentional, agreed in MACRO_MODE_PLAN §3 / this session):
//   * The EE<->VU0 sync prologue (mVUFinishVU0 on EEINST_COP2_{SYNC,FINISH}_VU0)
//     and the cycle accounting stay in aR5900.cpp — that TU owns the M1 flags
//     (g_pCurInstInfo), the M2 sync helpers, and s_cop2RawCycles. aR5900.cpp
//     classifies a COP2 SPECIAL op as Mode-0 (recCOP2IsMode0), emits the FINISH
//     prologue itself, then calls recVUMacroEmitMode0() here for the pure VU
//     register emit. So this file is x86's recCOP2_SPEC1 *minus* the sync line.
//   * The x86 register-allocator COP2 path (regAlloc->reset(true)/cop2 sharing of
//     xmm/x86 regs, flushPartialForCOP2) is replaced by the existing thread-mode
//     allocator run memory-backed: we repoint the VU-state base register
//     (RVUSTATE = x19) from &cpuRegs to &vuRegs[0] for the op, reset()/emit/
//     flushAll(), then restore x19. The EE GPR cache is flushed before the op and
//     killed after (recCacheFlushAll/recCacheApplyNativeEffects), so memory-backed
//     VF/VI access is correct.
//
// M5.1 ported the **Mode-0** ops (mode == 0x0: no flags, no Q, no analysis pass —
// pure allocReg -> op -> clearNeeded). M5.2 added the **flag** ops (mode == 0x110:
// the ADD/SUB/MUL/MADD/MSUB/OPMULA/OPMSUB families + their ACC forms) — these set
// the status/MAC flags, so setupMacroOp/endMacroOp gained the mode&0x10 branches that
// denormalize the status flag into gprF0 before the op (mVUallocSFLAGd) and
// normalize it back to memory after (mVUallocSFLAGc), gated by the M1
// EEINST_COP2_{,DE,}NORMALIZE/STATUS/MAC flags exactly as x86 setupMacroOp/
// endMacroOp do. M5.3 adds the **Q** ops: the *q ALU forms (mode 0x111 = 0x110|0x01,
// flags + Q read: ADDq/MULq/MADDq/SUBq/MSUBq + ACC) and DIV/SQRT/RSQRT (mode 0x112 =
// 0x110|0x02, flags + Q write — the divide also folds its D/I flags into gprF0) plus
// the empty WAITQ/NOP. setupMacroOp/endMacroOp gain the mode&0x01 (load VI[REG_Q] into
// the Q lane of mVU_xmmPQ=v24) and mode&0x02 (store it back) branches — faithful to x86
// setupMacroOp 38-40 / endMacroOp 79-82. The CLIP bit (0x08) is still unused (M5.4). All
// of these are NEON + flag-reg + PQ only (no allocGPR), so the unsafe VI GPR pool stays
// untouched until M5.4 (the VI ALU ops).

// Host-register safety (Mode-0):
//   * GPRs: Mode-0 ops are float/NEON only — they never call regAlloc->allocGPR,
//     so the allocator's VI GPR pool (which currently still includes x20/x21/x22 =
//     the EE GPR-cache regs + REVTLBPTR) is never exercised. That pool collision is
//     real but only matters for the VI ALU ops in M5.4, which must exclude the EE
//     pinned regs before they can allocate GPRs. Mode-0 is unaffected.
//   * NEON: the EE entry saves d8-d15 (armBeginStackFrame), and no EE NEON state is
//     live across ops, so using the full v0-v23 VF pool is safe — endMacroOp's
//     flushAll() writes every cached VF back to vuRegs[0] before x19 is restored.

//------------------------------------------------------------------
// Macro op setup / teardown (mode-0 subset of x86 setupMacroOp/endMacroOp)
//------------------------------------------------------------------

static void setupMacroOp(int mode)
{
	// Repoint the VU-state base register (RVUSTATE = x19) from &cpuRegs to
	// &vuRegs[0] for the duration of this op; endMacroOp restores it. The mVU
	// emitters address all VF/VI/ACC/I through x19, so this is what makes the
	// thread-mode allocator operate on VU0 state from inside the EE rec.
	armMoveAddressToReg(RVUSTATE, &::vuRegs[0]);

	// Enter COP2/macro mode BEFORE reset() so the allocator's reset() sees cop2 == 1
	// and excludes the EE rec's pinned GPRs (x20/x21/x22) from the VI GPR pool — see
	// microRegAlloc::reset() in aVU_IR.h. (x86 calls regAlloc->reset(true) for this;
	// the ARM64 reset() reads mVU.cop2 instead of taking a flag.)
	microVU0.cop2 = 1;

	// Set up reg allocation (x86: regAlloc->reset(true)).
	microVU0.regAlloc->reset();

	microVU0.prog.IRinfo.curPC = 0;
	microVU0.code = cpuRegs.code;
	std::memset(&microVU0.prog.IRinfo.info[0], 0, sizeof(microVU0.prog.IRinfo.info[0]));

	// M5.3 Q read (x86 setupMacroOp 38-40). The *q ALU forms (0x111) read Q from
	// memory before running; DIV/SQRT/RSQRT (0x112) only write Q, so they skip this.
	// x86's `if (mode & 0x03) _freeXMMreg(xmmPQ.Id)` (line 28-29) has no ARM64 analogue:
	// mVU_xmmPQ (v24) lives OUTSIDE the allocator's VF pool (v0-v23), so it is never
	// allocated and there is nothing to free. mvuLdrSS = Ldr(v24.S(), ..) loads VI[REG_Q]
	// into Q lane 0 and zero-extends the upper 96 bits (AArch64 32-bit load into S clears
	// [127:32]) — bit-identical to x86 xMOVSSZX(xmmPQ, mem). The *q emitters read Q via
	// getQreg(.., mVUinfo.readQ); readQ == 0 here (info memset above), so lane 0 is correct.
	if (mode & 0x01) // Q-Reg will be Read
		mvuLdrSS(mVU_xmmPQ, &::vuRegs[0].VI[REG_Q].UL);

	// M5.4 CLIP (x86 setupMacroOp 42-46). Tell the emitter to write the clip flag: with
	// write/lastWrite == 0xff (>= 4) mVUallocCFLAGa/b take the memory-backed "macroVU"
	// path (VI[REG_CLIP_FLAG] in vuRegs[0]), so CLIP round-trips its flag through memory
	// — no flag-instance register needed (unlike the status flag's gprF0). Gated on
	// CHECK_VU_FLAGHACK + the M1 EEINST_COP2_CLIP_FLAG exactly as x86.
	if (mode & 0x08 && (!CHECK_VU_FLAGHACK || (g_pCurInstInfo->info & EEINST_COP2_CLIP_FLAG))) // Clip Instruction
	{
		microVU0.prog.IRinfo.info[0].cFlag.write     = 0xff;
		microVU0.prog.IRinfo.info[0].cFlag.lastWrite = 0xff;
	}

	// The mode & 0x10
	// status/MAC branch below is the M5.2 work: a faithful port of x86 setupMacroOp
	// lines 47-74. Each branch is gated on CHECK_VU_FLAGHACK exactly as x86 does —
	// when the flag hack is off, the flags are always updated (the !FLAGHACK
	// fallthrough); when on, only when the M1 analysis flagged this op as needing it.
	if (mode & 0x10 && (!CHECK_VU_FLAGHACK || (g_pCurInstInfo->info & EEINST_COP2_STATUS_FLAG))) // Update Status Flag
	{
		microVU0.prog.IRinfo.info[0].sFlag.doFlag      = true;
		microVU0.prog.IRinfo.info[0].sFlag.doNonSticky = true;
		microVU0.prog.IRinfo.info[0].sFlag.write       = 0;
		microVU0.prog.IRinfo.info[0].sFlag.lastWrite   = 0;
	}
	if (mode & 0x10 && (!CHECK_VU_FLAGHACK || (g_pCurInstInfo->info & EEINST_COP2_MAC_FLAG))) // Update Mac Flags
	{
		microVU0.prog.IRinfo.info[0].mFlag.doFlag = true;
		microVU0.prog.IRinfo.info[0].mFlag.write  = 0xff;
	}
	if (mode & 0x10 && (!CHECK_VU_FLAGHACK || (g_pCurInstInfo->info & (EEINST_COP2_STATUS_FLAG | EEINST_COP2_DENORMALIZE_STATUS_FLAG))))
	{
		// x86 _freeX86reg(gprF0) has no ARM64 analogue — gprF0 (w23) is a fixed
		// Status-flag instance register, not an allocated EE GPR, so nothing to spill.
		if (!CHECK_VU_FLAGHACK || (g_pCurInstInfo->info & EEINST_COP2_DENORMALIZE_STATUS_FLAG))
		{
			// flags are normalized in memory — denormalize into gprF0 before the op
			// (mVUallocSFLAGd destroys reg/tmp1/tmp2; tmp1/tmp2 = the mVU emit scratch).
			mVUallocSFLAGd(&::vuRegs[0].VI[REG_STATUS_FLAG].UL, getFlagReg(0), gprT1, gprT2);
		}
		else
		{
			// flags already denormalized in memory — just load into gprF0 (the x86
			// "ideally we'd keep this in a register, but 32-bit" comment doesn't apply
			// on ARM64, but the lazy memory round-trip mirrors x86 1:1).
			armMoveAddressToReg(RSCRATCHADDR, &::vuRegs[0].VI[REG_STATUS_FLAG].UL);
			armAsm->Ldr(getFlagReg(0), a64::MemOperand(RSCRATCHADDR));
		}
	}

	pxAssert(mode == 0x0 || mode == 0x100 || mode == 0x104 || mode == 0x108 ||
	         mode == 0x110 || mode == 0x111 || mode == 0x112);
}

static void endMacroOp(int mode)
{
	// M5.3 Q writeback (x86 endMacroOp 79-82) — emitted FIRST, before flushAll and the
	// flag normalize, matching x86 order. The *q forms (0x111) only read Q so they skip
	// this; DIV/SQRT/RSQRT (0x112) write the divide result into Q lane 0 (writeQreg with
	// writeQ == 0) and store it back here. mvuStrSS = Str(v24.S(), ..) stores Q lane 0 —
	// bit-identical to x86 xMOVSS(mem, xmmPQ). Absolute-addressed (RSCRATCHADDR), so it is
	// order-independent of x19; v24 is outside the VF pool, so flushAll below won't touch it.
	if (mode & 0x02) // Q-Reg was Written To
		mvuStrSS(&::vuRegs[0].VI[REG_Q].UL, mVU_xmmPQ);

	// flushAll() is the memory-backed equivalent of x86's flushPartialForCOP2():
	// write every cached VF/VI back to vuRegs[0] (the next op reads from memory)
	// and clear the allocator state. Emitted while x19 still == &vuRegs[0]. (The flag
	// math below is absolute-addressed, so it is order-independent of x19; we keep
	// x86's flush-then-normalize order. gprF0, holding the op's updated denormalized
	// status, is untouched by flushAll — it only writes back VF/VI-int regs.)
	microVU0.regAlloc->flushAll();

	// M5.2: normalize / back up the status flag (x86 endMacroOp lines 86-100). gprF0
	// holds the denormalized status the op just updated; turn it back into the packed
	// VI[STATUS] layout for the next reader. Gated on CHECK_VU_FLAGHACK as in x86.
	if (mode & 0x10)
	{
		if (!CHECK_VU_FLAGHACK || (g_pCurInstInfo->info & EEINST_COP2_NORMALIZE_STATUS_FLAG))
		{
			// Normalize gprF0 -> packed status, store to memory (x86: mVUallocSFLAGc(eax,
			// gprF0, 0) then xMOV mem,eax). gprT1 = output, gprT2 = the scratch the
			// allocator loads gprF0 into; both are dead mVU emit temps here.
			mVUallocSFLAGc(gprT1, gprT2, 0);
			armMoveAddressToReg(RSCRATCHADDR, &::vuRegs[0].VI[REG_STATUS_FLAG].UL);
			armAsm->Str(gprT1, a64::MemOperand(RSCRATCHADDR));
		}
		else if (g_pCurInstInfo->info & (EEINST_COP2_STATUS_FLAG | EEINST_COP2_DENORMALIZE_STATUS_FLAG))
		{
			// Back up the denormalized flags for the next instruction (re-normalized
			// before the reg is next read). x86: xMOV mem, gprF0.
			armMoveAddressToReg(RSCRATCHADDR, &::vuRegs[0].VI[REG_STATUS_FLAG].UL);
			armAsm->Str(getFlagReg(0), a64::MemOperand(RSCRATCHADDR));
		}
	}

	microVU0.cop2 = 0;
	microVU0.regAlloc->reset();

	// Restore the EE rec's base register for the rest of the block. RVUSTATE and the
	// EE rec's RESTATEPTR are the same physical register (x19); this TU only knows it
	// as RVUSTATE (aR5900.h isn't included here), so point x19 back at &cpuRegs.
	armMoveAddressToReg(RVUSTATE, &cpuRegs);
	pxAssert(mode == 0x0 || mode == 0x100 || mode == 0x104 || mode == 0x108 ||
	         mode == 0x110 || mode == 0x111 || mode == 0x112);
}

//------------------------------------------------------------------
// Mode-0 op generators (x86: REC_COP2_mVU0(f, .., 0x0) -> recV##f).
// mode-0 has no `& 4` (analysis) bit, so REC_COP2_mVU0 runs only pass2
// (recompile, recPass == 1) of the single mVU emitter.
//------------------------------------------------------------------

static void recVABS() { setupMacroOp(0x0); mVU_ABS(microVU0, 1); endMacroOp(0x0); }

static void recVITOF0()  { setupMacroOp(0x0); mVU_ITOF0 (microVU0, 1); endMacroOp(0x0); }
static void recVITOF4()  { setupMacroOp(0x0); mVU_ITOF4 (microVU0, 1); endMacroOp(0x0); }
static void recVITOF12() { setupMacroOp(0x0); mVU_ITOF12(microVU0, 1); endMacroOp(0x0); }
static void recVITOF15() { setupMacroOp(0x0); mVU_ITOF15(microVU0, 1); endMacroOp(0x0); }
static void recVFTOI0()  { setupMacroOp(0x0); mVU_FTOI0 (microVU0, 1); endMacroOp(0x0); }
static void recVFTOI4()  { setupMacroOp(0x0); mVU_FTOI4 (microVU0, 1); endMacroOp(0x0); }
static void recVFTOI12() { setupMacroOp(0x0); mVU_FTOI12(microVU0, 1); endMacroOp(0x0); }
static void recVFTOI15() { setupMacroOp(0x0); mVU_FTOI15(microVU0, 1); endMacroOp(0x0); }

static void recVMAXx()  { setupMacroOp(0x0); mVU_MAXx (microVU0, 1); endMacroOp(0x0); }
static void recVMAXy()  { setupMacroOp(0x0); mVU_MAXy (microVU0, 1); endMacroOp(0x0); }
static void recVMAXz()  { setupMacroOp(0x0); mVU_MAXz (microVU0, 1); endMacroOp(0x0); }
static void recVMAXw()  { setupMacroOp(0x0); mVU_MAXw (microVU0, 1); endMacroOp(0x0); }
static void recVMAXi()  { setupMacroOp(0x0); mVU_MAXi (microVU0, 1); endMacroOp(0x0); }
static void recVMAX()   { setupMacroOp(0x0); mVU_MAX  (microVU0, 1); endMacroOp(0x0); }
static void recVMINIx() { setupMacroOp(0x0); mVU_MINIx(microVU0, 1); endMacroOp(0x0); }
static void recVMINIy() { setupMacroOp(0x0); mVU_MINIy(microVU0, 1); endMacroOp(0x0); }
static void recVMINIz() { setupMacroOp(0x0); mVU_MINIz(microVU0, 1); endMacroOp(0x0); }
static void recVMINIw() { setupMacroOp(0x0); mVU_MINIw(microVU0, 1); endMacroOp(0x0); }
static void recVMINIi() { setupMacroOp(0x0); mVU_MINIi(microVU0, 1); endMacroOp(0x0); }
static void recVMINI()  { setupMacroOp(0x0); mVU_MINI (microVU0, 1); endMacroOp(0x0); }

static void recVMOVE() { setupMacroOp(0x0); mVU_MOVE(microVU0, 1); endMacroOp(0x0); }
static void recVMR32() { setupMacroOp(0x0); mVU_MR32(microVU0, 1); endMacroOp(0x0); }

//------------------------------------------------------------------
// Flag op generators (mode 0x110 — status/MAC, x86: REC_COP2_mVU0(f, .., 0x110)).
// Same shape as the Mode-0 generators, but mode 0x110 drives the setup/teardown
// flag denormalize/normalize. mode 0x110 has no 0x04 (analysis) bit, so only pass2
// of the mVU emitter runs (recPass == 1) — same as Mode-0.
//------------------------------------------------------------------

// ADD family (M5.2 commit 1)
static void recVADD()   { setupMacroOp(0x110); mVU_ADD  (microVU0, 1); endMacroOp(0x110); }
static void recVADDx()  { setupMacroOp(0x110); mVU_ADDx (microVU0, 1); endMacroOp(0x110); }
static void recVADDy()  { setupMacroOp(0x110); mVU_ADDy (microVU0, 1); endMacroOp(0x110); }
static void recVADDz()  { setupMacroOp(0x110); mVU_ADDz (microVU0, 1); endMacroOp(0x110); }
static void recVADDw()  { setupMacroOp(0x110); mVU_ADDw (microVU0, 1); endMacroOp(0x110); }
static void recVADDi()  { setupMacroOp(0x110); mVU_ADDi (microVU0, 1); endMacroOp(0x110); }
static void recVADDA()  { setupMacroOp(0x110); mVU_ADDA (microVU0, 1); endMacroOp(0x110); }
static void recVADDAx() { setupMacroOp(0x110); mVU_ADDAx(microVU0, 1); endMacroOp(0x110); }
static void recVADDAy() { setupMacroOp(0x110); mVU_ADDAy(microVU0, 1); endMacroOp(0x110); }
static void recVADDAz() { setupMacroOp(0x110); mVU_ADDAz(microVU0, 1); endMacroOp(0x110); }
static void recVADDAw() { setupMacroOp(0x110); mVU_ADDAw(microVU0, 1); endMacroOp(0x110); }
static void recVADDAi() { setupMacroOp(0x110); mVU_ADDAi(microVU0, 1); endMacroOp(0x110); }

// SUB family (M5.2 commit 2)
static void recVSUB()   { setupMacroOp(0x110); mVU_SUB  (microVU0, 1); endMacroOp(0x110); }
static void recVSUBx()  { setupMacroOp(0x110); mVU_SUBx (microVU0, 1); endMacroOp(0x110); }
static void recVSUBy()  { setupMacroOp(0x110); mVU_SUBy (microVU0, 1); endMacroOp(0x110); }
static void recVSUBz()  { setupMacroOp(0x110); mVU_SUBz (microVU0, 1); endMacroOp(0x110); }
static void recVSUBw()  { setupMacroOp(0x110); mVU_SUBw (microVU0, 1); endMacroOp(0x110); }
static void recVSUBi()  { setupMacroOp(0x110); mVU_SUBi (microVU0, 1); endMacroOp(0x110); }
static void recVSUBA()  { setupMacroOp(0x110); mVU_SUBA (microVU0, 1); endMacroOp(0x110); }
static void recVSUBAx() { setupMacroOp(0x110); mVU_SUBAx(microVU0, 1); endMacroOp(0x110); }
static void recVSUBAy() { setupMacroOp(0x110); mVU_SUBAy(microVU0, 1); endMacroOp(0x110); }
static void recVSUBAz() { setupMacroOp(0x110); mVU_SUBAz(microVU0, 1); endMacroOp(0x110); }
static void recVSUBAw() { setupMacroOp(0x110); mVU_SUBAw(microVU0, 1); endMacroOp(0x110); }
static void recVSUBAi() { setupMacroOp(0x110); mVU_SUBAi(microVU0, 1); endMacroOp(0x110); }

// MUL family (M5.2 commit 3)
static void recVMUL()   { setupMacroOp(0x110); mVU_MUL  (microVU0, 1); endMacroOp(0x110); }
static void recVMULx()  { setupMacroOp(0x110); mVU_MULx (microVU0, 1); endMacroOp(0x110); }
static void recVMULy()  { setupMacroOp(0x110); mVU_MULy (microVU0, 1); endMacroOp(0x110); }
static void recVMULz()  { setupMacroOp(0x110); mVU_MULz (microVU0, 1); endMacroOp(0x110); }
static void recVMULw()  { setupMacroOp(0x110); mVU_MULw (microVU0, 1); endMacroOp(0x110); }
static void recVMULi()  { setupMacroOp(0x110); mVU_MULi (microVU0, 1); endMacroOp(0x110); }
static void recVMULA()  { setupMacroOp(0x110); mVU_MULA (microVU0, 1); endMacroOp(0x110); }
static void recVMULAx() { setupMacroOp(0x110); mVU_MULAx(microVU0, 1); endMacroOp(0x110); }
static void recVMULAy() { setupMacroOp(0x110); mVU_MULAy(microVU0, 1); endMacroOp(0x110); }
static void recVMULAz() { setupMacroOp(0x110); mVU_MULAz(microVU0, 1); endMacroOp(0x110); }
static void recVMULAw() { setupMacroOp(0x110); mVU_MULAw(microVU0, 1); endMacroOp(0x110); }
static void recVMULAi() { setupMacroOp(0x110); mVU_MULAi(microVU0, 1); endMacroOp(0x110); }

// MADD family (M5.2 commit 4)
static void recVMADD()   { setupMacroOp(0x110); mVU_MADD  (microVU0, 1); endMacroOp(0x110); }
static void recVMADDx()  { setupMacroOp(0x110); mVU_MADDx (microVU0, 1); endMacroOp(0x110); }
static void recVMADDy()  { setupMacroOp(0x110); mVU_MADDy (microVU0, 1); endMacroOp(0x110); }
static void recVMADDz()  { setupMacroOp(0x110); mVU_MADDz (microVU0, 1); endMacroOp(0x110); }
static void recVMADDw()  { setupMacroOp(0x110); mVU_MADDw (microVU0, 1); endMacroOp(0x110); }
static void recVMADDi()  { setupMacroOp(0x110); mVU_MADDi (microVU0, 1); endMacroOp(0x110); }
static void recVMADDA()  { setupMacroOp(0x110); mVU_MADDA (microVU0, 1); endMacroOp(0x110); }
static void recVMADDAx() { setupMacroOp(0x110); mVU_MADDAx(microVU0, 1); endMacroOp(0x110); }
static void recVMADDAy() { setupMacroOp(0x110); mVU_MADDAy(microVU0, 1); endMacroOp(0x110); }
static void recVMADDAz() { setupMacroOp(0x110); mVU_MADDAz(microVU0, 1); endMacroOp(0x110); }
static void recVMADDAw() { setupMacroOp(0x110); mVU_MADDAw(microVU0, 1); endMacroOp(0x110); }
static void recVMADDAi() { setupMacroOp(0x110); mVU_MADDAi(microVU0, 1); endMacroOp(0x110); }

// MSUB family (M5.2 commit 5)
static void recVMSUB()   { setupMacroOp(0x110); mVU_MSUB  (microVU0, 1); endMacroOp(0x110); }
static void recVMSUBx()  { setupMacroOp(0x110); mVU_MSUBx (microVU0, 1); endMacroOp(0x110); }
static void recVMSUBy()  { setupMacroOp(0x110); mVU_MSUBy (microVU0, 1); endMacroOp(0x110); }
static void recVMSUBz()  { setupMacroOp(0x110); mVU_MSUBz (microVU0, 1); endMacroOp(0x110); }
static void recVMSUBw()  { setupMacroOp(0x110); mVU_MSUBw (microVU0, 1); endMacroOp(0x110); }
static void recVMSUBi()  { setupMacroOp(0x110); mVU_MSUBi (microVU0, 1); endMacroOp(0x110); }
static void recVMSUBA()  { setupMacroOp(0x110); mVU_MSUBA (microVU0, 1); endMacroOp(0x110); }
static void recVMSUBAx() { setupMacroOp(0x110); mVU_MSUBAx(microVU0, 1); endMacroOp(0x110); }
static void recVMSUBAy() { setupMacroOp(0x110); mVU_MSUBAy(microVU0, 1); endMacroOp(0x110); }
static void recVMSUBAz() { setupMacroOp(0x110); mVU_MSUBAz(microVU0, 1); endMacroOp(0x110); }
static void recVMSUBAw() { setupMacroOp(0x110); mVU_MSUBAw(microVU0, 1); endMacroOp(0x110); }
static void recVMSUBAi() { setupMacroOp(0x110); mVU_MSUBAi(microVU0, 1); endMacroOp(0x110); }

// Outer-product ops (M5.2 commit 6) — OPMULA (SPECIAL2), OPMSUB (SPECIAL1)
static void recVOPMULA() { setupMacroOp(0x110); mVU_OPMULA(microVU0, 1); endMacroOp(0x110); }
static void recVOPMSUB() { setupMacroOp(0x110); mVU_OPMSUB(microVU0, 1); endMacroOp(0x110); }

//------------------------------------------------------------------
// Q-op generators (mode 0x111 = 0x110|0x01 — flags + Q read; M5.3). Same shape as the
// flag generators, but mode 0x111 also drives the setup Q-load (the *q emitters read Q
// from lane 0 of mVU_xmmPQ). x86: REC_COP2_mVU0(f, .., 0x111). No 0x04 (analysis) bit,
// so only pass2 of the mVU emitter runs.
//------------------------------------------------------------------

// *q ALU forms (M5.3 commit 1) — SPECIAL1 *q
static void recVADDq()  { setupMacroOp(0x111); mVU_ADDq (microVU0, 1); endMacroOp(0x111); }
static void recVSUBq()  { setupMacroOp(0x111); mVU_SUBq (microVU0, 1); endMacroOp(0x111); }
static void recVMULq()  { setupMacroOp(0x111); mVU_MULq (microVU0, 1); endMacroOp(0x111); }
static void recVMADDq() { setupMacroOp(0x111); mVU_MADDq(microVU0, 1); endMacroOp(0x111); }
static void recVMSUBq() { setupMacroOp(0x111); mVU_MSUBq(microVU0, 1); endMacroOp(0x111); }
// *q ACC forms (M5.3 commit 1) — SPECIAL2 *q
static void recVADDAq()  { setupMacroOp(0x111); mVU_ADDAq (microVU0, 1); endMacroOp(0x111); }
static void recVSUBAq()  { setupMacroOp(0x111); mVU_SUBAq (microVU0, 1); endMacroOp(0x111); }
static void recVMULAq()  { setupMacroOp(0x111); mVU_MULAq (microVU0, 1); endMacroOp(0x111); }
static void recVMADDAq() { setupMacroOp(0x111); mVU_MADDAq(microVU0, 1); endMacroOp(0x111); }
static void recVMSUBAq() { setupMacroOp(0x111); mVU_MSUBAq(microVU0, 1); endMacroOp(0x111); }

//------------------------------------------------------------------
// EFU divide ops (mode 0x112 = 0x110|0x02 — flags + Q write; M5.3 commit 2). DIV/SQRT/
// RSQRT compute Q = Fs/Ft etc. and (under mVU.cop2) fold their D/I divide flags into
// gprF0 — so the mode&0x10 status round-trip (M5.2 machinery) carries the divide flags,
// and the mode&0x02 endMacroOp branch stores the Q result. The emitters write the result
// into Q lane 0 (writeQreg with writeQ == 0); they do NOT read Q (0x112 has no 0x01 bit),
// matching x86. x86: REC_COP2_mVU0(f, .., 0x112). (mVUanalyzeFDIV runs in pass1; only
// pass2 is emitted here, same as the other families.)
//------------------------------------------------------------------

static void recVDIV()   { setupMacroOp(0x112); mVU_DIV  (microVU0, 1); endMacroOp(0x112); }
static void recVSQRT()  { setupMacroOp(0x112); mVU_SQRT (microVU0, 1); endMacroOp(0x112); }
static void recVRSQRT() { setupMacroOp(0x112); mVU_RSQRT(microVU0, 1); endMacroOp(0x112); }

// WAITQ / NOP (M5.3 commit 3) — empty bodies, exactly like x86 recVWAITQ()/recVNOP() {}.
// They emit no VU codegen (no setup/endMacroOp), but still route through this decode so
// recVUMacroIsMode0 returns true: the EE rec then emits the FINISH prologue around them,
// mirroring x86 where they reach recCOP2_SPEC2 only via the recCOP2_SPEC1 FINISH wrapper.
// Macro-mode COP2 is synchronous (one-shot ops), so WAITQ has nothing to wait for —
// there is never a pending Q here; the FINISH prologue handles any VU0-micro sync.
static void recVWAITQ() {}
static void recVNOP()   {}

//------------------------------------------------------------------
// VI ALU / load-store / RNG / CLIP generators (M5.4). These are the first macro ops
// that call regAlloc->allocGPR, so they depend on the cop2-conditional VI GPR pool
// exclusion (microRegAlloc::reset, committed first in M5.4). Mode bits per x86
// microVU_Macro.inl 269-288: the 0x04 bit ("requires analysis pass") makes the op run
// pass-0 (analysis) first, then pass-1 only if it isn't a NOP (x86 REC_COP2_mVU0 macro
// lines 127-133). 0x104 ops carry 0x04; the 0x100/0x108 ops do not (pass-1 only).
//------------------------------------------------------------------

// CLIP (mode 0x108 = 0x100|0x08; SPECIAL2 idx2 0x1f). No 0x04 bit → pass-1 only. The
// setup CLIP branch armed cFlag.write/lastWrite = 0xff so mVU_CLIP round-trips the clip
// flag through VI[REG_CLIP_FLAG] in memory. CLIP uses gprT1/gprT2 (fixed scratch), not
// allocGPR, so it is GPR-pool-safe on its own — but it lands here with the rest of M5.4.
static void recVCLIP() { setupMacroOp(0x108); mVU_CLIP(microVU0, 1); endMacroOp(0x108); }

// Analysis-pass generator (mode & 0x04 set, e.g. 0x104): a faithful port of x86
// REC_COP2_mVU0's `if (_mode & 4)` branch (microVU_Macro.inl 127-133) — run pass-0
// (analyze) first, then pass-1 (emit) only if the op is not a NOP. The NOP-skip matters
// for the VI integer ALU ops that write VI[0] (mVUanalyzeIALU* set lOp.isNOP when the
// dest is vi00); allocGPR already turns a viWriteReg==0 into a discard temp, but skipping
// the emit entirely also matches x86 exactly (no dead code, and lOp.backupVI is whatever
// the analysis decides, not the memset default). info[0] is the macro-op slot (curPC == 0,
// memset in setupMacroOp). pass-1-only ops (0x100/0x108) keep the plain one-liner form.
#define REC_COP2_MACRO_ANALYZE(f, modeval)                                \
	static void recV##f()                                                 \
	{                                                                     \
		setupMacroOp(modeval);                                            \
		mVU_##f(microVU0, 0);                                             \
		if (!microVU0.prog.IRinfo.info[0].lOp.isNOP)                      \
			mVU_##f(microVU0, 1);                                         \
		endMacroOp(modeval);                                              \
	}

// VI integer ALU (mode 0x104; SPECIAL1 funct IADD 0x30 / ISUB 0x31 / IADDI 0x32 /
// IAND 0x34 / IOR 0x35). All carry the 0x04 analysis bit.
REC_COP2_MACRO_ANALYZE(IADD,  0x104)
REC_COP2_MACRO_ANALYZE(IADDI, 0x104)
REC_COP2_MACRO_ANALYZE(ISUB,  0x104)
REC_COP2_MACRO_ANALYZE(IAND,  0x104)
REC_COP2_MACRO_ANALYZE(IOR,   0x104)

// VI load/store (SPECIAL2; ILWR 0x3e / ISWR 0x3f / LQI 0x34 / LQD 0x36 / SQI 0x35 /
// SQD 0x37). These touch VU0 micro memory (vuRegs[0].Mem, addressed by the emitters via
// the absolute mVU.regs().Mem pointer) and VI/VF (addressed via x19 = &vuRegs[0]) — both
// correct because setupMacroOp points x19 at vuRegs[0], exactly as the standalone microVU0
// rec does. The loads (ILWR/LQI/LQD = mode 0x104) carry the 0x04 analysis bit (NOP-skip
// when writing vi00); the stores (ISWR/SQI/SQD = mode 0x100) are pass-1-only and never NOP.
REC_COP2_MACRO_ANALYZE(ILWR, 0x104)
REC_COP2_MACRO_ANALYZE(LQI,  0x104)
REC_COP2_MACRO_ANALYZE(LQD,  0x104)
static void recVISWR() { setupMacroOp(0x100); mVU_ISWR(microVU0, 1); endMacroOp(0x100); }
static void recVSQI()  { setupMacroOp(0x100); mVU_SQI (microVU0, 1); endMacroOp(0x100); }
static void recVSQD()  { setupMacroOp(0x100); mVU_SQD (microVU0, 1); endMacroOp(0x100); }

//------------------------------------------------------------------
// Dispatch — the native subset of x86's recCOP2SPECIAL1t / recCOP2SPECIAL2t.
//------------------------------------------------------------------

// Decode a COP2 SPECIAL op to its native macro-ALU emitter, or nullptr if it is not
// a ported op (Mode-0 from M5.1, the mode-0x110 flag families from M5.2, or the mode
// 0x111/0x112 Q ops from M5.3). Each generator bakes its own mode, so this decode only
// needs to return the fn pointer.
// The predicate (recVUMacroIsMode0) and the emit entry (recVUMacroEmitMode0) both go
// through this one decode so the EE rec's classify and the actual emit can never
// drift (a mismatch would FINISH-then-drop the op). The "Mode0" names are retained
// from M5.1 — the EE-rec contract (FINISH-only prologue, no cycle commit, no Q/CLIP/
// interlock) is identical for the 0x110 flag ops, so the predicate is reused as-is.
static void (*cop2Mode0Emitter(u32 op))()
{
	const u32 funct = op & 0x3f;
	if (funct >= 0x3c) // SPECIAL2 sub-table (x86: recCOP2_SPEC2)
	{
		switch ((op & 3) | ((op >> 4) & 0x7c))
		{
			// ADDA family (mode 0x110, M5.2 commit 1)
			case 0x00: return recVADDAx;  // ADDAx
			case 0x01: return recVADDAy;  // ADDAy
			case 0x02: return recVADDAz;  // ADDAz
			case 0x03: return recVADDAw;  // ADDAw
			case 0x22: return recVADDAi;  // ADDAi
			case 0x28: return recVADDA;   // ADDA
			// SUBA family (mode 0x110, M5.2 commit 2)
			case 0x04: return recVSUBAx;  // SUBAx
			case 0x05: return recVSUBAy;  // SUBAy
			case 0x06: return recVSUBAz;  // SUBAz
			case 0x07: return recVSUBAw;  // SUBAw
			case 0x26: return recVSUBAi;  // SUBAi
			case 0x2c: return recVSUBA;   // SUBA
			// MULA family (mode 0x110, M5.2 commit 3)
			case 0x18: return recVMULAx;  // MULAx
			case 0x19: return recVMULAy;  // MULAy
			case 0x1a: return recVMULAz;  // MULAz
			case 0x1b: return recVMULAw;  // MULAw
			case 0x1e: return recVMULAi;  // MULAi
			case 0x2a: return recVMULA;   // MULA
			// MADDA family (mode 0x110, M5.2 commit 4)
			case 0x08: return recVMADDAx; // MADDAx
			case 0x09: return recVMADDAy; // MADDAy
			case 0x0a: return recVMADDAz; // MADDAz
			case 0x0b: return recVMADDAw; // MADDAw
			case 0x23: return recVMADDAi; // MADDAi
			case 0x29: return recVMADDA;  // MADDA
			// MSUBA family (mode 0x110, M5.2 commit 5)
			case 0x0c: return recVMSUBAx; // MSUBAx
			case 0x0d: return recVMSUBAy; // MSUBAy
			case 0x0e: return recVMSUBAz; // MSUBAz
			case 0x0f: return recVMSUBAw; // MSUBAw
			case 0x27: return recVMSUBAi; // MSUBAi
			case 0x2d: return recVMSUBA;  // MSUBA
			case 0x2e: return recVOPMULA; // OPMULA (mode 0x110, M5.2 commit 6)
			// *q ACC forms (mode 0x111, M5.3 commit 1)
			case 0x1c: return recVMULAq;  // MULAq
			case 0x20: return recVADDAq;  // ADDAq
			case 0x21: return recVMADDAq; // MADDAq
			case 0x24: return recVSUBAq;  // SUBAq
			case 0x25: return recVMSUBAq; // MSUBAq
			// ITOF/FTOI/ABS/MOVE/MR32 (Mode-0, M5.1)
			case 0x10: return recVITOF0;  // ITOF0
			case 0x11: return recVITOF4;  // ITOF4
			case 0x12: return recVITOF12; // ITOF12
			case 0x13: return recVITOF15; // ITOF15
			case 0x14: return recVFTOI0;  // FTOI0
			case 0x15: return recVFTOI4;  // FTOI4
			case 0x16: return recVFTOI12; // FTOI12
			case 0x17: return recVFTOI15; // FTOI15
			case 0x1d: return recVABS;    // ABS
			case 0x1f: return recVCLIP;   // CLIP (mode 0x108, M5.4)
			case 0x30: return recVMOVE;   // MOVE
			case 0x31: return recVMR32;   // MR32
			// VI load/store (M5.4; LQI/SQI/LQD/SQD mode 0x104/0x100/0x104/0x100,
			// ILWR 0x104, ISWR 0x100)
			case 0x34: return recVLQI;    // LQI
			case 0x35: return recVSQI;    // SQI
			case 0x36: return recVLQD;    // LQD
			case 0x37: return recVSQD;    // SQD
			case 0x3e: return recVILWR;   // ILWR
			case 0x3f: return recVISWR;   // ISWR
			// DIV/SQRT/RSQRT (mode 0x112, M5.3 commit 2)
			case 0x38: return recVDIV;    // DIV
			case 0x39: return recVSQRT;   // SQRT
			case 0x3a: return recVRSQRT;  // RSQRT
			case 0x3b: return recVWAITQ;  // WAITQ (empty, M5.3 commit 3)
			case 0x2f: return recVNOP;    // NOP   (empty, M5.3 commit 3)
			default: return nullptr;
		}
	}
	// SPECIAL1 ops (x86: recCOP2SPECIAL1t, dispatched by funct = op & 0x3f). The
	// flag-free MAX*/MINI* family is Mode-0 (M5.1); the ADD/SUB/MUL/MADD/MSUB/OPMSUB
	// families are flag ops (mode 0x110, M5.2); the *q forms are mode 0x111 (M5.3);
	// the VI integer ALU (IADD/ISUB/IADDI/IAND/IOR) is mode 0x104 (M5.4). CALLMS/CALLMSR
	// stay on inline-interp until M5.5.
	switch (funct)
	{
		// ADD family (mode 0x110, M5.2 commit 1)
		case 0x00: return recVADDx;  // ADDx
		case 0x01: return recVADDy;  // ADDy
		case 0x02: return recVADDz;  // ADDz
		case 0x03: return recVADDw;  // ADDw
		case 0x22: return recVADDi;  // ADDi
		case 0x28: return recVADD;   // ADD
		// SUB family (mode 0x110, M5.2 commit 2)
		case 0x04: return recVSUBx;  // SUBx
		case 0x05: return recVSUBy;  // SUBy
		case 0x06: return recVSUBz;  // SUBz
		case 0x07: return recVSUBw;  // SUBw
		case 0x26: return recVSUBi;  // SUBi
		case 0x2c: return recVSUB;   // SUB
		// MUL family (mode 0x110, M5.2 commit 3)
		case 0x18: return recVMULx;  // MULx
		case 0x19: return recVMULy;  // MULy
		case 0x1a: return recVMULz;  // MULz
		case 0x1b: return recVMULw;  // MULw
		case 0x1e: return recVMULi;  // MULi
		case 0x2a: return recVMUL;   // MUL
		// MADD family (mode 0x110, M5.2 commit 4)
		case 0x08: return recVMADDx; // MADDx
		case 0x09: return recVMADDy; // MADDy
		case 0x0a: return recVMADDz; // MADDz
		case 0x0b: return recVMADDw; // MADDw
		case 0x23: return recVMADDi; // MADDi
		case 0x29: return recVMADD;  // MADD
		// MSUB family (mode 0x110, M5.2 commit 5)
		case 0x0c: return recVMSUBx; // MSUBx
		case 0x0d: return recVMSUBy; // MSUBy
		case 0x0e: return recVMSUBz; // MSUBz
		case 0x0f: return recVMSUBw; // MSUBw
		case 0x27: return recVMSUBi; // MSUBi
		case 0x2d: return recVMSUB;  // MSUB
		case 0x2e: return recVOPMSUB; // OPMSUB (mode 0x110, M5.2 commit 6)
		// *q ALU forms (mode 0x111, M5.3 commit 1)
		case 0x1c: return recVMULq;  // MULq
		case 0x20: return recVADDq;  // ADDq
		case 0x21: return recVMADDq; // MADDq
		case 0x24: return recVSUBq;  // SUBq
		case 0x25: return recVMSUBq; // MSUBq
		// MAX/MINI family (Mode-0, M5.1)
		case 0x10: return recVMAXx;  // MAXx
		case 0x11: return recVMAXy;  // MAXy
		case 0x12: return recVMAXz;  // MAXz
		case 0x13: return recVMAXw;  // MAXw
		case 0x14: return recVMINIx; // MINIx
		case 0x15: return recVMINIy; // MINIy
		case 0x16: return recVMINIz; // MINIz
		case 0x17: return recVMINIw; // MINIw
		case 0x1d: return recVMAXi;  // MAXi
		case 0x1f: return recVMINIi; // MINIi
		case 0x2b: return recVMAX;   // MAX
		case 0x2f: return recVMINI;  // MINI
		// VI integer ALU (mode 0x104, M5.4)
		case 0x30: return recVIADD;  // IADD
		case 0x31: return recVISUB;  // ISUB
		case 0x32: return recVIADDI; // IADDI
		case 0x34: return recVIAND;  // IAND
		case 0x35: return recVIOR;   // IOR
		default: return nullptr;
	}
}

// True if `op` is a natively-ported COP2 SPECIAL macro-ALU op (Mode-0 M5.1, the 0x110
// flag families M5.2, or the 0x111/0x112 Q ops M5.3). The EE rec gates the FINISH
// prologue + native emit on this, falling back to inline-interp otherwise.
bool recVUMacroIsMode0(u32 op)
{
	return cop2Mode0Emitter(op) != nullptr;
}

// Emit `op` natively via its Mode-0 microVU0 emitter; returns false (no codegen) if
// `op` is not a ported Mode-0 op. Called only after recVUMacroIsMode0(op) == true.
bool recVUMacroEmitMode0(u32 op)
{
	void (*fn)() = cop2Mode0Emitter(op);
	if (!fn)
		return false;
	fn();
	return true;
}
