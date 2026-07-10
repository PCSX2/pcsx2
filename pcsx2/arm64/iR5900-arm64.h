// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "VU.h"
#include "arm64/iCore-arm64.h"

// Per-category interpreter fallback toggles.
// Comment out a define to enable native ARM64 codegen for that category.
// #define FORCE_INTERP_BRANCH 1
// #define FORCE_INTERP_JUMP 1
// #define FORCE_INTERP_MOVE 1
// #define FORCE_INTERP_SHIFT 1
// #define FORCE_INTERP_ALU 1
// #define FORCE_INTERP_ARITIMM 1
// #define FORCE_INTERP_MULTDIV 1
// #define FORCE_INTERP_MEMORY 1
// #define FORCE_INTERP_COP0 1
// #define FORCE_INTERP_FPU 1
// #define FORCE_INTERP_COP2 1

// Reserved ARM64 registers for the recompiler
// x19: Fastmem base pointer (callee-saved)
#define RFASTMEMBASE vixl::aarch64::x19
// x20: Pointer to cpuRegs struct (callee-saved). Loaded once at JIT entry by
// EnterRecompiledCode and never modified for the duration of JIT execution.
// Use armCpuRegMem() to construct cpuRegs-relative MemOperands cheaply.
#define RSTATE vixl::aarch64::x20
// x24: Pinned &VU0 (callee-saved). Loaded once at JIT entry by
// EnterRecompiledCode; used to address VU0.{VF,VI,ACC,q,statusflag,...}
// fields via single [RVU0, #imm12] load/store in iCOP2-arm64.cpp,
// instead of the 3-mov+ldr abs-addr materialization sequence.
// Survives armEmitCall (callee-saved per AAPCS) and the mVU dispatcher's
// outer Stp/Ldp pair, so cross-EE/mVU dispatches preserve it.
#define RVU0 vixl::aarch64::x24
// x25: Pinned EE cycle DELTA (callee-saved):
//   RECCYCLE = (s64)(cpuRegs.cycle - cpuRegs.nextEventCycle)
// Negative ⇒ the next event is still in the future. Block cycle accounting
// adds into the delta exactly as it used to add into the absolute counter,
// so every block-tail event check is a flag-setting add (or a bare Cmp
// against zero) + B.ge — no nextEventCycle load and no 64-bit compare per
// exit. cpuRegs.cycle in MEMORY stays ABSOLUTE and canonical for all C
// code; armFlushCycleDelta/armReloadCycleDelta convert at the JIT↔C seams.
// nextEventCycle is only written by C code (cpuSetNextEventDelta & co.),
// which can only run inside those seams, so a fresh reload after every
// cycle/event-touching call keeps the delta exact. (The one exception is
// recSafeExitExecution's cross-thread `nextEventCycle = 0` exit poke: the
// in-register delta doesn't see it, so the exit lands at the previously
// scheduled event — worst case ~one hblank later — instead of the very
// next block tail. recEventTest still observes eeRecExitRequested.)
#define RECCYCLE vixl::aarch64::x25
// x22/x23/x29 + x26/x27/x21: Write-through pinned read-cache for the hottest
// guest GPRs (kEEPinTable below is the single source of truth):
//   x22 = cpuRegs.GPR.r[29].UD[0]  ($sp)
//   x23 = cpuRegs.GPR.r[31].UD[0]  ($ra)
//   x29 = cpuRegs.GPR.r[2].UD[0]   ($v0 — hottest EE reg: 20.3% of dynamic
//         refs in the SotC SD865 capture, tools/perf/sotc-regheat-2026-07-05.md)
//   x26 = cpuRegs.GPR.r[3].UD[0]   ($v1, 12.6% of dynamic refs)
//   x27 = cpuRegs.GPR.r[4].UD[0]   ($a0, 7.9%)
//   x21 = cpuRegs.GPR.r[5].UD[0]   ($a1)
//   x12 = cpuRegs.GPR.r[26].UD[0]  ($k0, 6.3% — tier 2)
//   x13 = cpuRegs.GPR.r[16].UD[0]  ($s0)
//   x11 = cpuRegs.GPR.r[1].UD[0]   ($at)
// THE PIN IS AUTHORITATIVE for the lower 64 bits. Under the shipping
// lazy-dirty policy (EE_PIN_LAZY_DIRTY=1, EE-SRA 3 Arm E) guest writes
// update the pin only (armStoreEERegPtr / armStoreEEGPRQuad lane 0) and the
// canonical store is elided; every seam where C code or a JIT-internal
// 128-bit/raw reader could observe GPR memory flushes the pins first
// (armFlushEEGPRPins / armFlushEEClobberedPins, and armMergeEEPinIntoQuad
// for quad reads). Reads that would load UD[0] from memory use the pin
// register instead (armLoadEERegPtr substitution / armEEPinForGPR in the
// scalar templates). The pins are re-read from memory (armReloadEEGPRPins)
// after the C calls that can write guest GPRs — interpreter fallbacks
// (recCall/recBranchCall), recEventTest (savestate load), recRecompile (ELF
// entry hooks), MFC0 rd=25, and eeloadHook/2. (Under the A/B-only
// write-through build, -DEE_PIN_LAZY_DIRTY=0, memory stays canonical and the
// flush helpers are no-ops.) The upper 64 bits of the 128-bit guest reg are
// NOT mirrored; only UD[0] accesses match. All pin host regs are carved out
// of the dynamic allocator pool (ALLOCATABLE_MASK in iCore-arm64.cpp).
//
// x22/x23/x29/x26/x27/x21 (tier 1 — 64.8% of dynamic UD[0] refs) are
// CALLEE-SAVED: every C call preserves them, so only GPR-writing callees
// need a reload. x29 (the AAPCS frame pointer) is additionally safe
// because every path that can run under a live EE JIT session preserves
// it: fastjmp_set/jmp save/restore it around the whole session
// (FastJmp.cpp), C callees preserve it per AAPCS, the mVU dispatcher
// Stp/Ldp's x29/x30 (microVU-arm64.cpp), the IOP dispatcher uses
// armBeginStackFrame, and the VIF unpack dynarec emits no x29 references.
// The known cost: FP-based stack unwinds through JIT frames read a guest
// value as a frame chain — harmless (we profile via perf jitdump, not FP
// walks).
//
// x26/x27 (EE-SRA 3 Arm C) were the allocator's callee-saved temp pool;
// the vtlb unaligned handlers were narrowed to ONE callee-saved temp each
// (Arm B) so x28 alone covers that demand — and that only works while
// each handler's MODE_CALLEESAVED alloc PRECEDES any same-instruction
// _allocArm64GPR call (nothing can hold x28 `needed` at that point; an
// inuse-but-not-needed x28 is evicted normally). mVU MICRO mode may use
// x26/x27 for its VI cache: the mVU dispatcher saves x19-x28. MACRO mode
// (cop2) emits inline in EE blocks with NO save around it, so
// microRegAlloc::reset(cop2mode) excludes x26/x27 from the VI pool there
// (today's 12 macro-routed ops allocate at most two VI slots — first-fit
// x14/x15 — so the exclusion closes a latent hazard, pinned by
// EeVu0Cop2Macro.MacroModeVIPoolExcludesEEPinHosts). Macro emit bodies
// reference no x26/x27 directly (verified across microVU_*-arm64).
//
// x21 doubles as the IOP's RPSXSTATE: IOP EnterRecompiledCode establishes
// it INSIDE armBeginStackFrame's x19-x28 save, and IOP execution is
// reachable from a live EE session only through C seams — so the EE pin
// rides through IOP runs exactly like x22 does. Every save site pairs x21
// with x22 in the same Stp (FastJmp.cpp, armBeginStackFrame, the mVU
// dispatcher prologue). mVU's gprF1 (w21, micro-mode status-flag
// instance) is dispatcher-saved and never referenced by macro-mode emit
// bodies (flag setup/exit thunks are micro-only paths).
//
// x11/x12/x13 are CALLER-SAVED (tier 2 — the callee-saved budget is
// exhausted) but sit inside preserve_most's spared range x9-x15 (EE-SRA 3
// Arm D). The preservation contract:
//   1. vtlb_memRead/Write<T> + the 128-bit variants — the ONLY C calls on
//      warm paths (fastmem backpatch thunk + inline softmem slow paths) —
//      are annotated preserve_most (vtlb.h): the callee preserves x9-x15,
//      which covers ALL tier-2 pins. Those seven call sites (4 recVTLB
//      softmem/128-bit slow paths + 3 RecStubs thunk slow paths) use
//      armReloadEEPinsAfterPreserveMostCall(), which emits NOTHING for
//      this table — warm vtlb seams carry zero pin traffic. This holds
//      even though the dispatchers call arbitrary handlers internally
//      (VIF dynarec, IntCHackCheck, ...): preserve_most is a callee
//      contract, so clang saves x9-x15 in the dispatcher's own prologue.
//      The thunk's own emitted code uses only w8/w9/w10/x0/x17
//      (RecStubs.cpp) and its gpr_bitmask save loop stays pin-free — pins
//      are not allocator state.
//   2. Every other C call reachable inside a live session is followed by
//      armReloadEEClobberedPins() (3 Ldrs, all cold paths): the const-
//      paddr MMIO shortcuts (raw registered handlers are plain AAPCS,
//      NOT preserve_most), dyna_block_discard/page_reset stubs, Goemon
//      TLB hooks, SetBranchReg's vtlb_V2P, Interp::MTC0, FPU/VCALLMS
//      interpreter fallbacks, the vu0Sync family (iCOP2), macro-mode
//      mVUaddrFix's waitMTVU (via armEmitEEClobberedPinReloadForCOP2),
//      and the test-build verify/divtrace hooks.
//   3. GPR-writing callees keep using the full armReloadEEGPRPins().
// mVU-side x11/x12/x13 uses are all MICRO-mode-only, verified 2026-07-09:
// gprT3 (w11) appears in NEON_ADD2SS (tri-ace ADDi hack), mVUsetupFlags'
// 4-distinct shuffle, VU branch emitters, and the shared SFLAGc exit
// thunk — macro mode hand-rolls its arithmetic natively in iCOP2
// (cop2Op_*, incl. native VCLIP whose w12 twin lives in Upper.inl's
// micro CLIP), routes only the 12 Lower.inl ops (zero w11-w13 refs)
// through the mVU adapter, and never emits VU branches or exit thunks.
// mVUallocSFLAGd's w11 default has no callers. Micro-mode clobbers are
// covered by contract 2 (micro runs only behind C seams). x4/x6/x7 (the
// pre-Arm-D tier-2 homes) are plain allocatable again — S3's 0f16948ae
// already removed every hardcoded w4-w7 scratch use.
#define REEPIN_SP vixl::aarch64::x22
#define REEPIN_RA vixl::aarch64::x23
#define REEPIN_V0 vixl::aarch64::x29
#define REEPIN_V1 vixl::aarch64::x26
#define REEPIN_A0 vixl::aarch64::x27
#define REEPIN_K0 vixl::aarch64::x12
#define REEPIN_A1 vixl::aarch64::x21
#define REEPIN_S0 vixl::aarch64::x13
#define REEPIN_AT vixl::aarch64::x11

// The static guest→host pin map. Everything pin-related (lookup, reload,
// flush, write policy) iterates this table; adding a rung = adding a row here,
// clearing the host reg's ALLOCATABLE_MASK bit in iCore-arm64.cpp, and
// covering it in ee_rec_pinned_gpr_tests.cpp. Host regs must be preserved
// by every C-reachable path from inside a block (callee-saved, or shimmed
// at the call sites — see the EE-SRA plan for rung 2+).
struct EEPinnedGPR
{
	u8 gpr;                        // guest GPR index (cpuRegs.GPR.r[gpr])
	vixl::aarch64::Register host;  // 64-bit host mirror of .UD[0]
};
static const EEPinnedGPR kEEPinTable[] = {
	{29, REEPIN_SP},
	{31, REEPIN_RA},
	{2, REEPIN_V0},
	{3, REEPIN_V1},
	{4, REEPIN_A0},
	{26, REEPIN_K0},
	{5, REEPIN_A1},
	{16, REEPIN_S0},
	{1, REEPIN_AT},
};

// Build a MemOperand addressing a cpuRegs field via RSTATE.
// Replaces the 3-instruction `armMoveAddressToReg(RSCRATCHADDR, &cpuRegs.X);
// Ldr/Str ..., [RSCRATCHADDR]` pattern with a single Ldr/Str using a
// signed/unsigned-immediate offset on RSTATE. ARM64 LDR with imm12 covers
// offsets up to 32760 bytes (64-bit) / 16380 bytes (32-bit) — easily larger
// than cpuRegs / fpuRegs combined, so a single instruction suffices for
// every reachable field.
static __fi vixl::aarch64::MemOperand armCpuRegMem(const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&cpuRegs);
	return vixl::aarch64::MemOperand(RSTATE, static_cast<int64_t>(off));
}

// Publish the ABSOLUTE cycle to cpuRegs.cycle before a C call that reads it:
// abs = delta + nextEventCycle. RECCYCLE itself is preserved (still the
// delta); `scratch` is clobbered. Pair with armReloadCycleDelta after any
// call that can advance cpuRegs.cycle or reschedule nextEventCycle.
static __fi void armFlushCycleDelta(const vixl::aarch64::Register& scratch = RXSCRATCH)
{
	armAsm->Ldr(scratch, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Add(scratch, RECCYCLE, scratch);
	armAsm->Str(scratch, armCpuRegMem(&cpuRegs.cycle));
}

// Re-derive the delta from (possibly modified) cpuRegs.cycle/nextEventCycle
// after a C call. Both fields are re-read, so any rescheduling the callee
// did (cpuSetNextEventDelta, IntCHackCheck cycle bumps, vu0 catch-up, ...)
// is captured exactly. `scratch` is clobbered.
static __fi void armReloadCycleDelta(const vixl::aarch64::Register& scratch = RXSCRATCH)
{
	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
	armAsm->Ldr(scratch, armCpuRegMem(&cpuRegs.nextEventCycle));
	armAsm->Sub(RECCYCLE, RECCYCLE, scratch);
}

// Pinned-base load/store helpers: when the target is anywhere inside
// _cpuRegistersPack (cpuRegs + fpuRegs), reach it via [RSTATE, #off] in one
// instruction; otherwise fall back to the generic 4-inst armLoadPtr/StorePtr.
static __fi bool armIsCpuRegPtr(const void* field)
{
	const u8* base = reinterpret_cast<const u8*>(&_cpuRegistersPack);
	const u8* p    = reinterpret_cast<const u8*>(field);
	return p >= base && p < base + sizeof(cpuRegistersPack);
}
// Pin lookup by guest GPR index. Returns the pinned host register mirroring
// GPR.r[gpr].UD[0], or nullptr when gpr is not pinned.
static __fi const vixl::aarch64::Register* armEEPinForGPR(int gpr)
{
	for (const EEPinnedGPR& pin : kEEPinTable)
	{
		if (pin.gpr == gpr)
			return &pin.host;
	}
	return nullptr;
}

// Pin lookup by target pointer: matches any byte within the LOWER 64 bits of
// a pinned guest GPR slot. *offset_in_dword receives the byte offset (0..7)
// of `field` within UD[0]. UD[1]/UL[2]/UL[3] accesses do not match (the
// upper half is not mirrored). Returns the table entry (host reg + guest
// index) so write-through callers can name the canonical memory slot.
static __fi const EEPinnedGPR* armEEPinForPtr(const void* field, int* offset_in_dword)
{
	const u8* p = reinterpret_cast<const u8*>(field);
	for (const EEPinnedGPR& pin : kEEPinTable)
	{
		const ptrdiff_t off = p - reinterpret_cast<const u8*>(&cpuRegs.GPR.r[pin.gpr]);
		if (off >= 0 && off < 8)
		{
			*offset_in_dword = static_cast<int>(off);
			return &pin;
		}
	}
	return nullptr;
}

// Re-read the pin mirrors from canonical memory. Needed after any C call
// that can write guest GPRs, and at every JIT entry (see the REEPIN_* doc).
static __fi void armReloadEEGPRPins()
{
	for (const EEPinnedGPR& pin : kEEPinTable)
		armAsm->Ldr(pin.host, armCpuRegMem(&cpuRegs.GPR.r[pin.gpr].UD[0]));
}

// Re-read only the pins whose host registers are CALLER-saved (clobbered by
// the mere act of a C call). Sufficient after callees that provably do not
// write guest GPR memory — the callee-saved pins still hold correct values
// there. Callees that CAN write guest GPRs need armReloadEEGPRPins instead.
// Future rungs' caller-saved pins join automatically via the table walk.
static __fi void armReloadEEClobberedPins()
{
	for (const EEPinnedGPR& pin : kEEPinTable)
	{
		if (!armIsCalleeSavedRegister(static_cast<int>(pin.host.GetCode())))
			armAsm->Ldr(pin.host, armCpuRegMem(&cpuRegs.GPR.r[pin.gpr].UD[0]));
	}
}

// preserve_most (clang AArch64) spares x9-x15 in the CALLEE: the caller may
// keep values live there across the call even though the registers are
// caller-saved under plain AAPCS.
static __fi bool armIsPreserveMostSparedRegister(int reg)
{
	return reg >= 9 && reg <= 15;
}

// Reload only the pins a preserve_most callee can actually clobber:
// caller-saved AND outside the spared x9-x15 range. For the current table
// (tier-2 pins in x11/x12/x13) this emits NOTHING — warm vtlb seams carry
// zero pin traffic. ONLY valid after calls to preserve_most-annotated
// callees (vtlb_memRead/Write<T>/128); plain-AAPCS callees (raw registered
// MMIO handlers, everything else) need armReloadEEClobberedPins. A future
// pin homed outside x9-x15 joins the reload automatically via the table
// walk.
static __fi void armReloadEEPinsAfterPreserveMostCall()
{
	for (const EEPinnedGPR& pin : kEEPinTable)
	{
		const int reg = static_cast<int>(pin.host.GetCode());
		if (!armIsCalleeSavedRegister(reg) && !armIsPreserveMostSparedRegister(reg))
			armAsm->Ldr(pin.host, armCpuRegMem(&cpuRegs.GPR.r[pin.gpr].UD[0]));
	}
}
// Pin write policy. EE_PIN_LAZY_DIRTY=1 (the shipping default since EE-SRA 3
// Arm E): guest writes to pinned GPRs update the PIN ONLY — the canonical
// store is elided, and every seam where C code (or a JIT-internal 128-bit /
// raw reader) could observe stale GPR memory restores canonicity first via
// the flush helpers below. 0 = write-through (pin + canonical store after
// every guest write) — kept for A/B: build a second binary with
// -DEE_PIN_LAZY_DIRTY=0 (codegen_ab two-binary flow); no runtime toggle —
// mixing modes across blocks is unsound.
//
// Arm E verdict (RK3562, 2026-07-09): lazy removes 4.3% of emitted EE-block
// instructions (write-through Strs 54.8k → 7.9k on the SotC census) and wins
// EE-thread SotC −1.05% / UYA −0.25% against write-through ON THE ARM C/D
// TABLE (callee-saved tier-1 + preserve_most-spared tier-2 = near-zero seam
// bills). WS-B's earlier UYA +0.84% lazy regression was the old caller-saved
// homing paying 6 Str + 6 Ldr at every C seam — the re-home, not the lazy
// idea, was what that verdict measured.
#ifndef EE_PIN_LAZY_DIRTY
#define EE_PIN_LAZY_DIRTY 1
#endif

// Write every pin mirror back to canonical memory. No dirty tracking — the
// lrps2 lesson says compile-time dirty subsets are unsound for runtime-path-
// dependent dirtiness, so seams flush ALL pins (9 Str to one hot line at
// already-expensive boundaries). No-op in write-through mode.
static __fi void armFlushEEGPRPins()
{
	if (!EE_PIN_LAZY_DIRTY)
		return;
	for (const EEPinnedGPR& pin : kEEPinTable)
		armAsm->Str(pin.host, armCpuRegMem(&cpuRegs.GPR.r[pin.gpr].UD[0]));
}

// Flush only the CALLER-saved pins. Required before any C call that is
// followed by armReloadEEClobberedPins: the reload reads canonical memory,
// which under lazy-dirty is stale until flushed — the pair would otherwise
// silently LOSE the in-register writes. (Callee-saved pins ride through the
// call in their registers, so they need neither.) No-op in write-through.
static __fi void armFlushEEClobberedPins()
{
	if (!EE_PIN_LAZY_DIRTY)
		return;
	for (const EEPinnedGPR& pin : kEEPinTable)
	{
		if (!armIsCalleeSavedRegister(static_cast<int>(pin.host.GetCode())))
			armAsm->Str(pin.host, armCpuRegMem(&cpuRegs.GPR.r[pin.gpr].UD[0]));
	}
}

// Lazy-dirty twin of armReloadEEPinsAfterPreserveMostCall: pins the callee
// spares ride through the call in their registers (still newest), so they
// need neither flush nor reload — the same treatment callee-saved pins
// already get at these seams. That is sound for exactly the reason the
// callee-saved treatment is: the preserve_most vtlb dispatchers never READ
// guest GPR memory. Emits nothing for the current table; no-op in
// write-through mode.
static __fi void armFlushEEPinsBeforePreserveMostCall()
{
	if (!EE_PIN_LAZY_DIRTY)
		return;
	for (const EEPinnedGPR& pin : kEEPinTable)
	{
		const int reg = static_cast<int>(pin.host.GetCode());
		if (!armIsCalleeSavedRegister(reg) && !armIsPreserveMostSparedRegister(reg))
			armAsm->Str(pin.host, armCpuRegMem(&cpuRegs.GPR.r[pin.gpr].UD[0]));
	}
}

// Lazy-dirty read-site merge: a 128-bit guest-GPR read from canonical memory
// (NEON dual-residence fill, MMI quad loads, SQ/QMTC2 sources) may see a
// stale lower half when the pin is dirty — Ins the pin into lane 0 after the
// load. The upper 64 bits are never mirrored, so memory is always right for
// them. No-op in write-through mode (memory == pin by construction).
static __fi void armMergeEEPinIntoQuad(const vixl::aarch64::VRegister& q, int gpr)
{
	if (!EE_PIN_LAZY_DIRTY)
		return;
	if (const vixl::aarch64::Register* pin = armEEPinForGPR(gpr))
		armAsm->Ins(q.V2D(), 0, *pin);
}

static __fi void armLoadEERegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	// Pinned guest GPR: serve the read from the mirror register. Write-through
	// keeps mirror == memory; lazy-dirty keeps the mirror NEWEST — either way
	// the mirror is authoritative for the lower 64 bits.
	int off;
	if (const EEPinnedGPR* pin = armEEPinForPtr(field, &off); pin && reg.IsRegister())
	{
		const vixl::aarch64::Register dst(reg);
		if (off == 0 && reg.Is64Bits())
		{
			armAsm->Mov(dst, pin->host);
			return;
		}
		if (off == 0 && reg.Is32Bits())
		{
			armAsm->Mov(dst, pin->host.W());
			return;
		}
		if (off == 4 && reg.Is32Bits())
		{
			armAsm->Lsr(dst.X(), pin->host, 32);
			return;
		}
		// Unusual shapes fall through to the (identical) canonical memory load.
	}
	if (armIsCpuRegPtr(field))
		armAsm->Ldr(reg, armCpuRegMem(field));
	else
		armLoadPtr(reg, field);
}
static __fi void armStoreEERegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	int off;
	const EEPinnedGPR* pin = armEEPinForPtr(field, &off);

	// Lazy-dirty (EE-SRA 2 WS-B3): pinned scalar lane-0 shapes write the PIN
	// ONLY — the canonical store is elided; seams restore memory canonicity
	// (armFlushEEGPRPins/armFlushEEClobberedPins). A store whose source IS
	// the pin (armEEDestForGPR / fastmem pinned load dest) emits NOTHING.
	if (EE_PIN_LAZY_DIRTY && pin && reg.IsRegister())
	{
		const vixl::aarch64::Register src(reg);
		if (off == 0 && reg.Is64Bits())
		{
			if (!src.Is(pin->host))
				armAsm->Mov(pin->host, src);
			return;
		}
		if (off == 0 && reg.Is32Bits())
		{
			armAsm->Bfi(pin->host, src.X(), 0, 32);
			return;
		}
		if (off == 4 && reg.Is32Bits())
		{
			armAsm->Bfi(pin->host, src.X(), 32, 32);
			return;
		}
		// Odd scalar shape: fall through to the store path, which must first
		// flush the (possibly newer) pin so the partial store merges into
		// current bytes rather than stale memory.
	}
	if (EE_PIN_LAZY_DIRTY && pin)
		armAsm->Str(pin->host, armCpuRegMem(&cpuRegs.GPR.r[pin->gpr].UD[0]));

	if (armIsCpuRegPtr(field))
		armAsm->Str(reg, armCpuRegMem(field));
	else
		armStorePtr(reg, field);

	// Write-through: keep the pin mirror equal to the memory just written.
	// (Also the lazy-dirty odd-shape tail: memory was made fully current
	// above, so the reload below re-canonicalizes the mirror.)
	if (pin)
	{
		if (reg.IsRegister())
		{
			const vixl::aarch64::Register src(reg);
			if (off == 0 && reg.Is64Bits())
			{
				if (!src.Is(pin->host))
					armAsm->Mov(pin->host, src);
				return;
			}
			if (off == 0 && reg.Is32Bits())
			{
				armAsm->Bfi(pin->host, src.X(), 0, 32);
				return;
			}
			if (off == 4 && reg.Is32Bits())
			{
				armAsm->Bfi(pin->host, src.X(), 32, 32);
				return;
			}
		}
		// Odd store shape (vector reg / sub-word): reload the mirror from the
		// just-written canonical memory.
		armAsm->Ldr(pin->host, armCpuRegMem(&cpuRegs.GPR.r[pin->gpr].UD[0]));
	}
}

// 128-bit guest-GPR store (MMI/NEON writeback, LQ, QMFC2): store the full
// quad, then refresh the pin mirror from lane 0 when gpr is pinned.
static __fi void armStoreEEGPRQuad(const vixl::aarch64::VRegister& q, int gpr)
{
	armAsm->Str(q, armCpuRegMem(&cpuRegs.GPR.r[gpr].UQ));
	if (const vixl::aarch64::Register* pin = armEEPinForGPR(gpr))
		armAsm->Mov(*pin, q.V2D(), 0);
}

// Destination selection for the scalar templates: when the destination
// guest GPR is pinned, the FINAL result-producing instruction targets the
// pin directly, so the armStoreEERegPtr write-through that follows emits
// just the canonical STR (it skips the mirror Mov when the source IS the
// mirror) — one instruction saved per pinned-dest op.
// Contract: ONLY the final writing instruction of an emission may target
// the returned register, with its guest sources read in that same
// instruction (a pinned dest can alias a pinned source; an earlier write
// would clobber it — the recSQC2/MGS3 aliasing class), and the
// write-through store must follow before anything can observe the reg.
// vixl macro temps are fine: LogicalMacro/AddSubMacro may borrow rd to
// materialize an unencodable immediate, but they exclude rn and the
// transient value is dead by the final instruction.
static __fi vixl::aarch64::Register armEEDestForGPR(int gpr, const vixl::aarch64::Register& scratch)
{
	if (const vixl::aarch64::Register* pin = armEEPinForGPR(gpr))
		return *pin;
	return scratch;
}

// Build a MemOperand addressing a VU0 field via RVU0. VURegs is < 2 KB, so
// every reachable field fits in imm12 for byte/halfword/word/doubleword/quad
// ldr/str. Mirrors armCpuRegMem for VU0 — used by iCOP2-arm64.cpp.
static __fi vixl::aarch64::MemOperand armVU0Mem(const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&VU0);
	return vixl::aarch64::MemOperand(RVU0, static_cast<int64_t>(off));
}

// Emit LD1R from a VU0 field, broadcasting to all lanes. ARM64 LD1R does
// NOT support [base, #imm] addressing — only [base] or post-index. vixl's
// LoadStoreStructAddrModeField silently drops the offset (and the assert
// is gated on VIXL_DEBUG, so Devel builds ship the wrong encoding instead
// of trapping). Materialize the address with a single ADD imm12 instead of
// 3-mov: VURegs fields fit within 4 KB of &VU0, so one ADD suffices.
// Total cost: ADD + LD1R = 2 insns, vs the original 4-insn 3-mov + LD1R.
static __fi void armLd1rVU0(const vixl::aarch64::VRegister& vt, const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&VU0);
	armAsm->Add(RSCRATCHADDR, RVU0, static_cast<int64_t>(off));
	armAsm->Ld1r(vt, vixl::aarch64::MemOperand(RSCRATCHADDR));
}

extern u32 maxrecmem;
extern u32 pc;             // recompiler pc
extern int g_branch;       // set for branch
extern u32 target;         // branch target
extern u32 s_nBlockCycles; // cycles of current block recompiling
extern bool s_nBlockInterlocked; // Current block has VU0 interlocking

//////////////////////////////////////////////////////////////////////////////////////////
// Interpreter fallback macros

#define REC_FUNC(f) \
	void rec##f() \
	{ \
		/* Delete destination register's const/alloc state before interpreter call. \
		 * The interpreter writes directly to cpuRegs, making any cached const or \
		 * allocated register stale. SPECIAL ops (Rd), loads/COP (Rt). */ \
		const u32 _op = cpuRegs.code >> 26; \
		const int _dest = (_op == 0 || _op == 0x1C) ? _Rd_ : _Rt_; \
		if (_dest > 0) \
			_deleteEEreg(_dest, 1); \
		recCall(Interp::f); \
	}

#define REC_FUNC_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recCall(Interp::f); \
	}

#define REC_SYS(f) \
	void rec##f() \
	{ \
		recBranchCall(Interp::f); \
	}

#define REC_SYS_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recBranchCall(Interp::f); \
	}

extern bool g_recompilingDelaySlot;

// LDL/LDR (and later SDL/SDR) pair fusion. An unaligned 64-bit access is emitted
// by the game as an LDL/LDR pair on the same Rt/Rs whose offsets differ by 7;
// together they are exactly one (un)aligned 64-bit access at the lower address,
// which ARM64 performs in a single op. The leading half emits that fused op and
// sets g_eeUnalignedFused; the trailing half consumes the flag and emits nothing.
// Cleared at block start (the gate guarantees the partner is consumed in-block,
// so the flag never legitimately survives a block; the clear only sweeps residue
// from an aborted compile). g_eeUnalignedFuseCount tallies fusions (tests/diag).
extern bool g_eeUnalignedFused;
extern u32 g_eeUnalignedFuseCount;

// Exclusive end PC of the block currently being recompiled. Used by the LDL/LDR
// fusion to confirm the peeked partner instruction is in the same block.
u32 recCurrentBlockEndPC();

// Used for generating backpatch thunks for fastmem
u8* recBeginThunk();
u8* recEndThunk();

// Branch processing
bool TrySwapDelaySlot(u32 rs, u32 rt, u32 rd, bool allow_loadstore);
void SaveBranchState();
void LoadBranchState();

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot);
void SetBranchReg();
void SetBranchImm(u32 imm);

void iFlushCall(int flushtype);
void recBranchCall(void (*func)());
void recCall(void (*func)());
// Emit the post-interpreter-call TLB-miss exception dispatch (defined in
// iR5900-arm64.cpp). DispatcherReg/s_recTlbMissOccurred are file-local there,
// so cross-TU interpreter-call sites (recVTLB-arm64.cpp) route through this.
void recEmitInterpTlbMissCheck();
u32 scaleblockcycles_clear();

// COP2 / VU0 sync emit helper (defined in iCOP2-arm64.cpp).
// interlock=true mirrors x86 COP2_Interlock (CFC2/CTC2/QMFC2/QMTC2 path);
// interlock=false mirrors mVUSyncVU0 / mVUFinishVU0 gating used by LQC2/SQC2
// and the COP2 macro-arithmetic setup. finishFunc is the secondary helper
// to invoke after vu0Sync (typically _vu0FinishMicro or _vu0WaitMicro);
// pass nullptr for "sync only". Emits zero instructions when EEINST analysis
// flags say no sync is needed.
void cop2EmitConditionalSync(bool interlock, void (*finishFunc)());

// COP2 macro-mode microVU0 state setup/teardown (defined in microVU-arm64.cpp).
// Mirrors x86 setupMacroOp/endMacroOp's regAlloc reset, microVU0.cop2 = 1,
// prog.IRinfo.curPC/info[0] init, code = cpuRegs.code, and flag scaffolding.
// Required before invoking any mVU emitter (mVU_LQI/SQI/MFIR/...) from a
// COP2 macro-mode dispatch wrapper. eeinstInfo is g_pCurInstInfo->info (or 0
// when EEINST analysis isn't live for this site).
void mVUmacroSetupCOP2State(int mode, u32 eeinstInfo);
void mVUmacroEndCOP2State();

// COP2 macro-mode setup/teardown wrapper (defined in iCOP2-arm64.cpp).
// Calls cop2EmitConditionalSync, emits status-flag denormalize/normalize when
// mode & 0x10, then runs mVUmacroSetup/EndCOP2State to ready microVU0 for the
// mVU emitter pass. REC_COP2_mVU0_ARM64-style wrappers in iR5900Misc-arm64.cpp
// bracket calls to mVUmacroEmit_<op> with these.
void setupMacroOp_arm64(int mode);
void endMacroOp_arm64(int mode);

// COP2 macro-mode emit adapters (defined in microVU-arm64.cpp). Each runs the
// pass1+pass2 dispatch x86 uses in REC_COP2_mVU0 (microVU_Macro.inl:127-133).
// mode is the same mode bits passed to setupMacroOp_arm64; only bit 0x04
// (requires analysis pass) is observed by the adapter.
void mVUmacroEmit_LQI(int mode);
void mVUmacroEmit_SQI(int mode);
void mVUmacroEmit_LQD(int mode);
void mVUmacroEmit_SQD(int mode);
void mVUmacroEmit_MTIR(int mode);
void mVUmacroEmit_MFIR(int mode);
void mVUmacroEmit_ILWR(int mode);
void mVUmacroEmit_ISWR(int mode);
void mVUmacroEmit_RNEXT(int mode);
void mVUmacroEmit_RGET(int mode);
void mVUmacroEmit_RINIT(int mode);
void mVUmacroEmit_RXOR(int mode);

namespace R5900
{
	namespace Dynarec
	{
		extern void recDoBranchImm(u32 branchTo, u32* jmpSkip, bool isLikely = false, bool swappedDelaySlot = false);
	}
}

////////////////////////////////////////////////////////////////////
// Constant Propagation

#define GPR_IS_CONST1(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))))
#define GPR_IS_CONST2(reg1, reg2) (EE_CONST_PROP && (g_cpuHasConstReg & (1 << (reg1))) && (g_cpuHasConstReg & (1 << (reg2))))
#define GPR_IS_DIRTY_CONST(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))) && (!(g_cpuFlushedConstReg & (1 << (reg)))))
#define GPR_SET_CONST(reg) \
	{ \
		if ((reg) < 32) \
		{ \
			g_cpuHasConstReg |= (1 << (reg)); \
			g_cpuFlushedConstReg &= ~(1 << (reg)); \
		} \
	}

#define GPR_DEL_CONST(reg) \
	{ \
		if ((reg) < 32) \
			g_cpuHasConstReg &= ~(1 << (reg)); \
	}

alignas(16) extern GPR_reg64 g_cpuConstRegs[32];
extern u32 g_cpuHasConstReg, g_cpuFlushedConstReg;

// Move guest GPR value to an ARM64 register
void _eeMoveGPRtoR(const vixl::aarch64::Register& to, int fromgpr, bool allow_preload = true);
// Returns a register currently holding the guest GPR (pin / MODE_READ
// allocator slot — zero insns) or materializes into `scratch`. Post-flush
// contexts only; consume immediately; never write the result. (EE-SRA 2 WS-C)
vixl::aarch64::Register _eeGetGPRSourceReg(const vixl::aarch64::Register& scratch, int fromgpr);

void _eeFlushAllDirty();
void _eeOnWriteReg(int reg, int signext);

// Totally deletes from const, NEON, and GPR entries
// if flush is 1, also flushes to memory
void _deleteEEreg(int reg, int flush);
void _deleteEEreg128(int reg);

void _flushEEreg(int reg, bool clear = false);

//////////////////////////////////////
// Templates for code recompilation //
//////////////////////////////////////

typedef void (*R5900FNPTR)();
typedef void (*R5900FNPTR_INFO)(int info);

// Memory-based templates — no register allocation, all operands via cpuRegs memory.
void eeRecompileCodeRC0_MEM(R5900FNPTR constcode, R5900FNPTR_INFO constscode, R5900FNPTR_INFO consttcode, R5900FNPTR_INFO noconstcode, int xmminfo);
void eeRecompileCodeRC1_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo);
void eeRecompileCodeRC2_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo);

#define EERECOMPILE_CODERC0_MEM(fn, xmminfo) \
	void rec##fn(void) \
	{ \
		eeRecompileCodeRC0_MEM(rec##fn##_const, rec##fn##_consts, rec##fn##_constt, rec##fn##_, (xmminfo)); \
	}

#define EERECOMPILE_CODEX_MEM(codename, fn, xmminfo) \
	void rec##fn(void) \
	{ \
		codename(rec##fn##_const, rec##fn##_, (xmminfo)); \
	}

#define FPURECOMPILE_CONSTCODE(fn, xmminfo) \
	void rec##fn(void) \
	{ \
		if (CHECK_FPU_FULL) \
			eeFPURecompileCode(DOUBLE::rec##fn##_xmm, R5900::Interpreter::OpcodeImpl::COP1::fn, xmminfo); \
		else \
			eeFPURecompileCode(rec##fn##_xmm, R5900::Interpreter::OpcodeImpl::COP1::fn, xmminfo); \
	}

int eeRecompileCodeXMM(int xmminfo);
void eeFPURecompileCode(R5900FNPTR_INFO xmmcode, R5900FNPTR fpucode, int xmminfo);
