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
// pure allocReg -> op -> clearNeeded). M5.2 adds the **flag** ops (mode == 0x110:
// the ADD/SUB/MUL/MADD/MSUB/OPMULA/OPMSUB families + their ACC forms) — these set
// the status/MAC flags, so setupMacroOp/endMacroOp gain the mode&0x10 branches that
// denormalize the status flag into gprF0 before the op (mVUallocSFLAGd) and
// normalize it back to memory after (mVUallocSFLAGc), gated by the M1
// EEINST_COP2_{,DE,}NORMALIZE/STATUS/MAC flags exactly as x86 setupMacroOp/
// endMacroOp do. 0x110 has NO Q bit (0x01/0x02) and NO CLIP bit (0x08) — those
// (and the *q forms, mode 0x111) are added when M5.3 lands. The flag ops are still
// NEON + flag-reg only (no allocGPR), so the unsafe VI GPR pool stays untouched
// until M5.4 (the VI ALU ops).

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

	// Set up reg allocation (x86: regAlloc->reset(true); the ARM64 reset() has no
	// cop2 variant — the COP2 alloc path was dropped, see aVU_IR.h).
	microVU0.regAlloc->reset();

	microVU0.cop2 = 1;
	microVU0.prog.IRinfo.curPC = 0;
	microVU0.code = cpuRegs.code;
	std::memset(&microVU0.prog.IRinfo.info[0], 0, sizeof(microVU0.prog.IRinfo.info[0]));

	// mode & 0x01 (Q read) and mode & 0x08 (CLIP) are zero for both Mode-0 and the
	// M5.2 flag ops (0x110) — those branches are added in M5.3. The mode & 0x10
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

	pxAssert(mode == 0x0 || mode == 0x110);
}

static void endMacroOp(int mode)
{
	// mode & 0x02 (Q-writeback) is zero for Mode-0 and the M5.2 flag ops (added M5.3).
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
	pxAssert(mode == 0x0 || mode == 0x110);
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

//------------------------------------------------------------------
// Dispatch — the native subset of x86's recCOP2SPECIAL1t / recCOP2SPECIAL2t.
//------------------------------------------------------------------

// Decode a COP2 SPECIAL op to its native macro-ALU emitter, or nullptr if it is not
// a ported op (Mode-0 from M5.1, or the mode-0x110 flag families from M5.2). Each
// generator bakes its own mode, so this decode only needs to return the fn pointer.
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
			case 0x30: return recVMOVE;   // MOVE
			case 0x31: return recVMR32;   // MR32
			default: return nullptr;
		}
	}
	// SPECIAL1 ops (x86: recCOP2SPECIAL1t, dispatched by funct = op & 0x3f). The
	// flag-free MAX*/MINI* family is Mode-0 (M5.1); the ADD/SUB/MUL/MADD/MSUB/OPMSUB
	// families are flag ops (mode 0x110, M5.2). The *q forms (0x111), the VI/integer
	// ops, and CALLMS stay on inline-interp until M5.3-M5.5.
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
		default: return nullptr;
	}
}

// True if `op` is a natively-ported COP2 SPECIAL macro-ALU op (Mode-0 M5.1 or the
// 0x110 flag families M5.2). The EE rec gates the FINISH prologue + native emit on
// this, falling back to inline-interp otherwise.
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
