// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

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
// M5.1 ports the **Mode-0** ops only (mode == 0x0: no flags, no Q, no analysis
// pass — setupMacroOp/endMacroOp's mode&0x01/0x02/0x08/0x10 branches are all
// skipped, so it is pure allocReg -> op -> clearNeeded). The flag (M5.2) and Q
// (M5.3) machinery is added to setupMacroOp/endMacroOp when those families land.

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

	// mode-0: mode & 0x01 (Q read), 0x08 (CLIP), 0x10 (status/mac) are all zero,
	// so none of x86 setupMacroOp's flag/Q branches run. They are added here when
	// M5.2 (flags) / M5.3 (Q) port the families that need them.
	pxAssert(mode == 0x0);
}

static void endMacroOp(int mode)
{
	// mode-0: no Q-writeback (mode & 0x02) and no status normalize (mode & 0x10).
	// flushAll() is the memory-backed equivalent of x86's flushPartialForCOP2():
	// write every cached VF/VI back to vuRegs[0] (the next op reads from memory)
	// and clear the allocator state. Emitted while x19 still == &vuRegs[0].
	microVU0.regAlloc->flushAll();

	microVU0.cop2 = 0;
	microVU0.regAlloc->reset();

	// Restore the EE rec's base register for the rest of the block. RVUSTATE and the
	// EE rec's RESTATEPTR are the same physical register (x19); this TU only knows it
	// as RVUSTATE (aR5900.h isn't included here), so point x19 back at &cpuRegs.
	armMoveAddressToReg(RVUSTATE, &cpuRegs);
	pxAssert(mode == 0x0);
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

//------------------------------------------------------------------
// Dispatch — the Mode-0 subset of x86's recCOP2SPECIAL1t / recCOP2SPECIAL2t.
//------------------------------------------------------------------

// Decode a COP2 SPECIAL op to its native Mode-0 emitter, or nullptr if it is not a
// ported Mode-0 op. The predicate (recVUMacroIsMode0) and the emit entry
// (recVUMacroEmitMode0) both go through this one decode so the EE rec's classify
// and the actual emit can never drift (a mismatch would FINISH-then-drop the op).
static void (*cop2Mode0Emitter(u32 op))()
{
	const u32 funct = op & 0x3f;
	if (funct >= 0x3c) // SPECIAL2 sub-table (x86: recCOP2_SPEC2)
	{
		switch ((op & 3) | ((op >> 4) & 0x7c))
		{
			case 0x10: return recVITOF0;  // ITOF0
			case 0x11: return recVITOF4;  // ITOF4
			case 0x12: return recVITOF12; // ITOF12
			case 0x13: return recVITOF15; // ITOF15
			case 0x14: return recVFTOI0;  // FTOI0
			case 0x15: return recVFTOI4;  // FTOI4
			case 0x16: return recVFTOI12; // FTOI12
			case 0x17: return recVFTOI15; // FTOI15
			case 0x1d: return recVABS;    // ABS
			default: return nullptr;
		}
	}
	// SPECIAL1 Mode-0 ops (MAX*/MINI*) are wired in the M5.1 MAX/MINI commit.
	return nullptr;
}

// True if `op` is a ported Mode-0 COP2 SPECIAL op (the EE rec gates the FINISH
// prologue + native emit on this, falling back to inline-interp otherwise).
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
