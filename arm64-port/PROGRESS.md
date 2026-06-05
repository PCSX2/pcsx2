# PROGRESS — ARM64 Recompiler Port

> Living roadmap. **This is the source of truth for "what's done" and "what's next."**
> Update it at the end of every session. Status legend:
> `[ ]` not started · `[~]` in progress · `[x]` done · `[!]` blocked (see JOURNAL).

---

## ▶ CURRENT FOCUS

**Phase 5.3 COP2 / VU0-macro inline-fallback — DONE (commit `0108025c9`).**

COP2 ops (primary opcode `0x12`) and the COP2 quad load/stores `LQC2`/`SQC2` used to
return false from `recTranslateOp`, so every one **broke the EE block** and got
single-stepped through the dispatcher — expensive in VU0-macro-heavy geometry (FFX
interleaves COP2 densely with EE code). They now **inline the interpreter handler**
(`recEmitInterpInline`) and keep the block intact.

Key insight that makes this safe: on ARM64 `CpuVU0 == &CpuIntVU0` (the **synchronous**
VU0 interpreter), so — unlike the x86 rec — there is **no deferred microVU program to
finish/sync** (`mVUFinishVU0`/`COP2_Interlock`) before touching VU0 state. Running the
interpreter's COP2 op inline is therefore byte-for-byte identical to single-stepping it,
just without the block-break + dispatcher round-trip. `BC2F/BC2T/BC2FL/BC2TL` (rs==0x08)
write `cpuRegs.pc`, so they stay on the single-step path.

This is the "interpreter fallback initially" form of Phase 5.3 (full native microVU
emission is Phase 7). **Verified:** 273/273 ARM64 emit tests green; BIOS + FFX boot and
run great (user-confirmed live).

**Next:** IOP rec (Phase 6) for playable 2D — the biggest remaining leverage now that EE
dispatch is fast and EE/COP2 stay in-block; IOP is still fully interpreter-single-stepped.
Alt: native COP2 microVU emission (folded into Phase 7), or Phase 5.1 COP0 inline (same
trick — most COP0 ops are straight-line and can inline instead of breaking the block).

---

**Phase 4.4 recLUT block linking + dispatcher — DONE & MERGED to `armjit`.**

The recLUT execution model (parked since 2026-06-04 on `armjit-reclut-wip`) was
re-derived on top of the current `armjit` (MMI-complete) baseline and **now boots the
BIOS** — reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished`, stays alive,
no hang/crash (verified live 2026-06-05). Commit `37787a50b`, fast-forward merged into
`armjit` — recLUT is now the live EE dispatch model (the Phase 4.3 `s_blocks` loop is
gone).

The parked stall was exactly the **4.5-invalidation × 4.4-recLUT interaction** the user
suspected. Two root causes, both fixed:

1. **Hang → targeted recClear.** `recClear`/every `Cpu->Clear` funnelled into
   `recResetRaw`, which in the recLUT model rewrites the *entire* multi-million-entry
   recLUT. `MapTLB` issues one `Cpu->Clear(addr<<12, 0x400)` per mapped TLB page during
   BIOS setup, so a whole-cache reset per page made boot hang (sampled: 4271/4277
   CPU-thread samples in `recResetRaw` under `MapTLB` under `intExecuteOneInst`).
   `recClear` now resets only the recLUT slots covering `[addr, addr+size)` back to
   `JITCompile` (skips unmapped pages); orphaned host code is reclaimed at the next full
   reset. Mirrors the x86 per-range clear.
2. **Crash → cache headroom.** `recRecompile` reset only at `recPtr >= recPtrEnd`, no
   room for the block being emitted; once the cache filled, VIXL's `CodeBuffer` tried to
   `realloc` the MAP_JIT region → SIGABRT (`pointer being freed was not allocated`). Now
   resets at `recPtrEnd - RECOMPILE_HEADROOM` (1 MB).

**Known limitation (Phase 4.5 follow-up):** targeted `recClear` only invalidates blocks
whose START slot is in the cleared range — a block straddling in from an earlier address
is missed. Fine for BIOS; SMC-heavy games may need overlap-aware invalidation.

**Verified:** unittests green (Arm64EmitEE 270/270, 273 total); BIOS boots live.
**Game-compat smoke test PASSED (2026-06-05):** Final Fantasy X (3D, EE+VU heavy) boots
into the game on the recLUT-backed `armjit`. **FPS 10 → 14 (+40%)** vs the Phase 4.3
`s_blocks` dispatcher — the recLUT block-linking win is real on a live title (VU/IOP
still interpreter-bound, so this is pure EE-dispatch headroom).
**Next:** IOP rec (Phase 6) for playable 2D, or Phase 5.1 COP0 / 5.3 COP2-VU0 macro.
Diagnostic tooling note: lldb attach needs a `get-task-allow` ad-hoc re-sign
(`codesign --force --sign - --entitlements <get-task-allow> --deep PCSX2.app`); `sample
<pid>` + `lldb -o 'breakpoint set -n malloc_error_break' -o run -o bt` found both bugs.

---

**Phase 5.4 MMI 128-bit SIMD — COMPLETE + correctness pass done.**

A full review of the unpushed MMI commits + the misc-ops batch found and fixed a
large set of bugs (all now covered by gtests — the MAC/misc ops were previously
untested, and the committed test file did not even compile):

- **Decode (`aR5900.cpp`):** `recTranslateMMI0/1/2/3` + the MMI funct dispatch had
  many wrong indices. They now mirror `R5900OpcodeTables.cpp tbl_MMI*` exactly.
  Notably: PLZCW is funct 0x04, PMFHL=0x30, PMTHL=0x31, PSLLH/PSRLH/PSRAH=0x34/36/37,
  PSLLW/PSRLW/PSRAW=0x3C/3E/3F; PMFHI/PMFLO live in MMI2 (0x08/0x09), PMTHI/PMTLO in
  MMI3 (0x08/0x09); PMULTW=MMI2 0x0C, PMULTH=0x1C, PMADDH/PMSUBH=0x10/0x14,
  PHMADH/PHMSBH=0x11/0x15, PEXEH/PREVH/PEXEW=0x1A/1B/1E, PEXCW=MMI3 0x1E,
  PADSBH=MMI1 0x04, PEXT5/PPAC5=MMI0 0x1E/0x1F.
- **Emit (`aR5900MMI.cpp`):** PMADDW/PMSUBW divided by −1 (Sxtw of 0xFFFFFFFF) instead
  of 4294967295, and conflated temp/temp2; PMADDUW dropped the carry into HI;
  PMULTW/PMULTUW wrote only 32 bits of LO/HI (no sign-extend); PMULTH was a 2-lane
  word multiply instead of 8-lane halfword; PHMADH/PHMSBH accumulated and skipped the
  odd lanes; PLZCW was off-by-one; PMFHL.LH had swapped lanes and SLW/SH were silent
  no-ops; PMTHL clobbered the odd LO/HI words. All fixed and bit-exact vs MMI.cpp.
- **QFSRV** stays on the interpreter (its shift amount is the runtime SA register
  `cpuRegs.sa`, not an instruction immediate) — the only intentional MMI fallback.

**Second correctness pass (2026-06-05):** a re-review found two more bugs that the
first pass's tests missed because the oracles were self-consistent with the buggy code:
- **PSLLVW/PSRLVW/PSRAVW** emitted four independent 32-bit lane shifts. The interpreter
  shifts only lanes 0 and 2 and sign-extends each 32-bit result to a full 64-bit
  doubleword. Rewrote them; the `refPSxxVW` test oracles (which replicated the bug) were
  corrected to mirror MMI.cpp.
- **PMADDW division voodoo** skipped the `(Rt&0x7FFFFFFF)==0` trigger (only ==0x7FFFFFFF
  was handled). Fixed; added `MMI_PMADDW_VoodooZeroRt` to cover the ==0 path that inA/inB
  never reach.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (Arm64EmitEE 270/270). Live game
verification still pending.

---

### Original Phase 5.4 inventory (kept for context)
All MMI ops now emit inline in `pcsx2/arm64/aR5900MMI.cpp`:

**Multiply-accumulate (HI/LO writes):**
- `PMULTW/PMULTUW`: 32×32→64 signed/unsigned multiply per lane
- `PMADDW/PMADDUW`: multiply-add with EE division voodoo (signed only)
- `PMSUBW`: multiply-subtract with division fixup
- `PMULTH/PMADDH/PMSUBH`: 16×16→32 halfword multiply (8 lanes)
- `PHMADH/PHMSBH`: paired halfword multiply-add/subtract

**HI/LO moves (full 128-bit):**
- `PMFHI/PMFLO/PMTHI/PMTLO`: NEON q-reg load/store
- `PMFHL` (LW/UW/LH variants): extract from HI/LO pairs
- `PMTHL`: pack GPR to HI/LO

**Misc ops:**
- `PLZCW`: count leading sign bits (ARM64 `Cls` - 1)
- `PADSBH`: subtract low 4 / add high 4 halfwords
- `QFSRV`: quad byte shift right (variable cross-lane)
- `PEXT5/PPAC5`: 5-bit field expand/compress per lane

Dispatch wired in `recTranslateMMI0/1/2/3`. **Verified:** `pcsx2-qt` builds arm64;
unit tests 334/334 core, 252/252 Arm64EmitEE. Live game verification pending.

**All Phase 5.4 MMI ops now compiled natively except QFSRV (runtime SA register →
interpreter), and all are gtest-covered.**

Next concrete task: Phase 4.4 recLUT (parked on `armjit-reclut-wip` until BIOS
stall solved), or game compatibility testing.

---

### Earlier focus (kept for context)

**Phase 4.4 (recLUT execution model) ATTEMPTED & PARKED — booting Phase 4.3 model restored.**
The full x86-style rewrite (recLUT page table + emitted DispatcherReg/Event/JITCompile/
Enter/Unmapped stubs + emitted cycle/event tail, x19 pinned once, `s_blocks` removed) was
written on a branch and debugged to root cause, but it does **not** boot the BIOS, so
`armjit` has been reverted to the known-good Phase 4.3 dispatcher (`s_blocks` + C++ loop)
which boots cleanly (verified: `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished`).
The recLUT work is preserved on branch **`armjit-reclut-wip`** (commit `7e3404cf4`).

Two bugs were found in the recLUT attempt:
1. **CRASH (root-caused + fixed on the wip branch):** crash at ~0.44s, `recExitUnmapped`
   with PC=0x9fc43144. Cause: the model pins `RESTATEPTR=x19=&cpuRegs` once in
   EnterRecompiledCode and assumes it survives forever, but **`_cpuEventTest_Shared`
   (reached via DispatcherEvent→recEventTest; it services DMA/VIF and runs other ARM64
   JIT) does NOT preserve x19** across the C++ call. The dispatcher then reads
   `cpuRegs.pc` through a garbage x19 and lands on an unmapped recLUT page. Fix on the
   wip branch: re-pin `RESTATEPTR=&cpuRegs` at the top of DispatcherReg every dispatch.
   **Lesson for any pinned-register design: no host register survives an external
   C++/JIT call here — reload it at the dispatch funnel (or after each external call).**
2. **STALL (open, the reason for the revert):** with the crash fixed the BIOS still does
   not reach display — the EE bursts to ~365k cycles then stalls in a wait loop
   (~0x9fc42b08) with events still firing; VPS stays 0. Separate Commit-A regression vs
   the Phase 4.3 model, not yet root-caused. **Resume the recLUT effort on
   `armjit-reclut-wip` by diagnosing this stall (suspect EE↔IOP sync / a miscompiled op
   surfaced by the new dispatch model), NOT by re-deriving the x19 crash.**

Next concrete task on `armjit`: continue the EE work on the booting Phase 4.3 baseline —
**Phase 5.4 MMI** (128-bit SIMD → NEON, many games lean on it) is the recommended
highest-leverage follow-up. The recLUT/hardlinking optimisation (Phase 4.4) stays parked
until the stall above is solved.

---

### Earlier focus (kept for context)

**Phase 5.2b COMPLETE — the EE FPU single-precision suite is compiled natively.**
All COP1 single-precision ops now emit inline against the interpreter's non-IEEE
semantics (pcsx2/FPU.cpp ground truth): ADD/SUB/MUL(+ACC), DIV/SQRT/RSQRT,
MADD/MSUB(+ACC), MAX/MIN, the C.F/C.EQ/C.LT/C.LE compares, CVT.W/CVT.S, and the
BC1F/BC1T branches. Shared helpers `emitLoadFpuDouble` / `emitClampFpuDoubleBits` /
`emitStoreClampedResult` carry the `fpuDouble` input clamp + checkOverflow/Underflow
result clamp + FCR31 flag side-effects; compares/branches test the FCR31 C bit.
`Arm64EmitEE.*` covers every op against a C++ replica of FPU.cpp. **Verified:**
`pcsx2-qt` builds, binary arm64, `Arm64EmitEE.*` 173/173, full core suite 258/258.

**Still on interpreter fallback (intentional):** the double-precision W/L-format
paths and the *likely* FP branches BC1FL/BC1TL (delay-slot nullification), matching
the policy for the other likely branches.

**Interlude fix:** FFX black-after-BIOS was likely stale EE blocks for RAM code:
`recCompileBlock()` now marks compiled RAM pages with `mmap_MarkCountedRamPage()`, so
writes to loaded ELF/game code fault through the existing vtlb protection path and the
bring-up `recClear()` drops the whole ARM64 block cache. This is coarse but correct for
bring-up; targeted invalidation remains Phase 4.5.

Next concrete task: pick the next highest-leverage EE follow-up — recommend
**Phase 4.4 block linking + recLUT** (kill the per-block `unordered_map` lookup +
recompile-on-miss now that the FPU no longer single-steps), or **Phase 5.4 MMI**
(128-bit SIMD → NEON) which many games lean on. Phase 5.1 (COP0) and 5.3 (COP2/VU0
macro) remain interpreter fallbacks for now.

---

### Earlier focus (kept for context)

**Phase 0–4.3 COMPLETE. The EE recompiler RUNS — `Cpu = &recCpu` boots the BIOS.**
The dispatcher + delay-slot block compiler are live in `pcsx2/arm64/aR5900.cpp`:
- `recCompileBlock(startpc)` compiles a straight-line run into one self-contained
  host block (per-block prologue saves x19+LR and sets RESTATEPTR=&cpuRegs; epilogue
  restores + RET). It stops at the first **handled** control-flow op (emits the
  branch generator → compiles the delay slot → ends the block), at the length cap
  (`MAX_BLOCK_INSTS`, writes pc=next), or just before any op it can't compile
  (writes pc=that op).
- `recExecute()` is a C++ dispatcher loop: read `cpuRegs.pc` → `recGetBlock` (compile
  or `s_blocks` cache hit) → run block → `cpuRegs.cycle += block.cycles` →
  `_cpuEventTest_Shared` when the event cycle is hit. Exit via `fastjmp` on
  `eeRecExitRequested` (set by `recSafeExitExecution`).
- **Per-opcode interpreter fallback:** any op the rec can't compile (COP/FPU/MMI,
  syscalls, traps, likely & COP branches, ...) is run one-at-a-time by the new
  `intExecuteOneInst()` (Interpreter.cpp) — it mirrors `execI`, and for branch ops
  the interpreter's own `doBranch` handles the delay slot + PC + cycle flush.
- Cycle scaling (`recScaleBlockCycles`) mirrors `iR5900.cpp` `scaleblockcycles`.
- **Verified:** BIOS boots through `Mode Changed to DVD PAL` + `Pad: DS2 Config
  Finished` with the rec as the active EE provider; unittests green; arm64 binary.

Key bug fixed during bring-up: blocks make calls (vtlb/interp) that clobber LR, so
each block must save/restore LR itself (the bare-block + shared-trampoline design
crashed on the block's final RET). Now every block has its own x19+LR prologue/epilogue
(matching the `RunEEGen` test harness).

Next concrete task: **Phase 4.3-likely or Phase 5.2 (FPU)** — pick the highest-leverage
follow-up. Candidates: (a) compile the "likely" branches (BEQL/BNEL/BLEZL/BGTZL/
BLTZL/BGEZL/BLTZALL/BGEZALL — delay-slot nullification) so they stop hitting the
interpreter; (b) **Phase 4.4 block linking + recLUT** to kill the per-block
`unordered_map` lookup + recompile-on-miss cost; (c) **Phase 5.2 COP1/FPU** (BIOS +
most games lean on it heavily and it's currently all interpreter single-steps).
Recommend (b) or (c) — both are now measurable against a running rec.

> When you finish a task, move this pointer to the next one and flip the box below.

---

## Phase 0 — Prerequisites & Tooling

- [x] 0.1 Build native ARM64 binary; verify it launches (`build/pcsx2-qt/PCSX2.app`, confirmed `arm64`).
- [x] 0.2 Confirm CPU-provider fallbacks on ARM64 (`VMManager.cpp:2727-2731` force interpreters).
- [x] 0.3 Gap audit: verified NO dormant EE/IOP/VU rec code exists in repo history — full port required. (See JOURNAL #0.)
- [x] 0.4 Read existing `pcsx2/arm64/` infra (`AsmHelpers`, `Vif_Dynarec`, `Vif_UnpackNEON`).
- [x] 0.5 Study VIXL MacroAssembler API + existing `arm64/AsmHelpers.{h,cpp}` block lifecycle (`armSetAsmPtr`/`armStartBlock`/`armEndBlock`, reg-alloc macros, constant pool).
- [x] 0.6 VIXL scratch harness: `tests/ctest/core/arm64_emit_test.cpp` (gtest `Arm64Emit.*`) mmaps a `MAP_JIT` buffer, emits add/const/load-store via `armAsm`, executes it, asserts results. **All 3 pass.** Proves emit+execute end-to-end.

**Done when:** we can JIT-emit and execute arbitrary ARM64 from a test, and we
understand the existing `pcsx2/arm64/` patterns well enough to copy them.

---

## Phase 1 — EE Recompiler Skeleton

- [x] 1.1 Created `pcsx2/arm64/aR5900.h` — pins persistent host regs: `RESTATEPTR`=x19 (`&cpuRegs`), `REFASTMEMBASE`=x20 (fastmem base), `REVTLBPTR`=x21 (vtlb base, Phase 2).
- [x] 1.2 Created `pcsx2/arm64/aR5900.cpp` — defines `recCpu` with stub provider fns (recExecute = `pxFailRel`, rest no-op); added both files to `pcsx2arm64Sources`/`Headers`. Builds + links; binary still arm64; unittests green.
- [x] 1.3 `recReserve()`/`recShutdown()`/`recResetEE()`: carve the SysMemory-reserved EE rec region (`GetEERec()`..`GetEERecEnd()`) into a code area + a 1 MB tail `ArmConstantPool`; `recPtr`/`recPtrEnd` cursor. Builds, links, arm64, unittests green.
- [x] 1.4 Minimal block compile loop: `recCompileBlock()` emits NOP/NOP/RET via VIXL through the real `armSetAsmPtr`/`armStartBlock`/`armEndBlock` lifecycle on the EE code cache, advances `recPtr` (resets cache past `recPtrEnd`); `recExecute()` enters the block and returns. Builds, links, arm64, unittests green.
- [x] 1.5 Wired `recCpu` into `VMManager.cpp`: added `#else` (ARM64) branches to the `_M_X86` guards in `InitializeCPUProviders` (`recCpu.Reserve()`), `ShutdownCPUProviders` (`recCpu.Shutdown()`), and `ClearCPUExecutionCaches` (`recCpu.Reset()`). `UpdateCPUImplementations` left untouched — `Cpu = &intCpu;` (rec reserved, not selected). Builds, links, arm64, unittests green; **verified on real BIOS boot** (no crash).

**Done when:** ARM64 build compiles + runs with the rec reserved/active, even if it
still defers all real work to the interpreter. ✅ **DONE** (BIOS boot verified).

---

## Phase 2 — vtlb Fast Memory & Load/Store  *(highest priority after skeleton)*

- [x] 2.1 Slow-path vtlb load/store codegen in `pcsx2/arm64/aR5900LoadStore.cpp`: `armEmitVtlbRead/Write` + `...Quad`, emitting calls to the C++ `vtlb_memRead/Write` helpers (interpreter-equivalent path). Builds, links, arm64, unittests green. *(Re-scoped from "implement `vtlb_DynBackpatchLoadStore`" — that is fastmem-only and is now Phase 2.2; see JOURNAL.)*
- [ ] 2.2 **Fastmem fast path:** implement `vtlb_DynBackpatchLoadStore` in `pcsx2/arm64/RecStubs.cpp` (currently `pxFailRel`) + the direct `REFASTMEMBASE`-relative load/store emit + `vtlb_AddLoadStoreInfo`. Single-instruction backpatch (overwrite the faulting op with `B thunk`); thunk does the slow path then branches back. Ref `x86/ix86-32/recVTLB.cpp`.
- [x] 2.3 EE load/store opcode generators: decode + guest-GPR access wired onto `armEmitVtlbRead/Write[Quad]`. `armEmitEffectiveAddr`/`armEmitLoadGpr`/`armEmitStoreGpr` + the 128-bit `armEmitLoadQuad`/`armEmitStoreQuad` (`~0xF` align + NEON q access). `recTranslateOp` dispatches the full aligned family: `LB/LBU/LH/LHU/LW/LWU/LD`, `SB/SH/SW/SD`, `LQ/SQ`. Unaligned `LWL/LWR/LDL/LDR`/`SWL/SWR/SDL/SDR` deferred (byte-merge codegen). Runtime-proven addr+align via 7 `Arm64EmitEE.*` gtests.
- [x] 2.4 Test: full guest-memory round-trip through the scalar + quad generators. 6 new `Arm64EmitEE.*` gtests map a host buffer into the vtlb `vmap` (hand-built direct-pointer entry — no SysMemory/fastmem/page-fault-handler needed since default `EmuConfig` makes `vtlb_memRead/Write` a plain `*vmap[addr>>12].assumePtr(addr)`) and assert store→load, byte sign/zero-extend, doubleword, `$zero`-discard, and quad store/load + align-down all read/write the right bytes. Validates address calc + AAPCS64 marshalling + extension end-to-end.

**Done when:** EE memory ops go through the JIT fastmem path and read/write correctly.

---

## Phase 3 — EE Integer Arithmetic

- [x] 3.1 Immediate ops: `ADDI/ADDIU/SLTI/SLTIU/ANDI/ORI/XORI/LUI/DADDI/DADDIU`.
- [x] 3.2 Reg-reg ops: `ADD/ADDU/SUB/SUBU/SLT/SLTU/AND/OR/XOR/NOR/DADD/DADDU/DSUB/DSUBU`.
- [x] 3.3 Shifts: `SLL/SRL/SRA/SLLV/SRLV/SRAV/DSLLV/DSRLV/DSRAV/DSLL/DSRL/DSRA/DSLL32/DSRL32/DSRA32`.
- [x] 3.4 Moves: `MOVZ/MOVN` (→ `CSEL`), `MFHI/MTHI/MFLO/MTLO`.
- [x] 3.5 Mul/Div: `MULT/MULTU/DIV/DIVU` (+ MMI `MULT1/MULTU1/DIV1/DIVU1` → HI1/LO1). Uses `SMULL/UMULL` (32×32→64) and `SDIV/UDIV` + reload-based remainder. ARM `SDIV` reproduces the EE overflow quirk natively; only ÷0 needs a fixup. MULT/MULTU honour the R5900 3-operand `Rd=LO` write.
- [ ] 3.6 Constant propagation (`EE_CONST_PROP`): track known-constant GPRs, emit immediate forms.

---

## Phase 4 — EE Branches & Jumps

- [x] 4.1 Jumps: `J/JAL/JR/JALR` — codegen + wired into the block compiler (decoded
  by `recEmitBranch`, delay slot compiled after, block exits to dispatcher).
- [~] 4.2 Conditional branches: `BEQ/BNE/BLEZ/BGTZ/BLTZ/BGEZ/BLTZAL/BGEZAL` —
  codegen + wired (`recEmitBranch`). "Likely" variants (BEQL/BNEL/BLEZL/BGTZL/
  BLTZL/BGEZL/BLTZALL/BGEZALL — delay-slot nullification) run via interpreter
  fallback for now; native codegen still TODO.
- [x] 4.3 **Dispatcher + delay-slot block compiler** — DONE & BIOS-boot verified.
  Multi-instruction `recCompileBlock`; branch generator + delay slot + exit; C++
  dispatcher loop in `recExecute` (pc→block→`_cpuEventTest_Shared`); per-opcode
  interpreter fallback via `intExecuteOneInst`; `Cpu = &recCpu` on ARM64.
- [x] 4.4 Block linking + recLUT (replaces the bring-up `s_blocks` unordered_map +
  recompile-on-miss). **DONE & MERGED to `armjit` (commit `37787a50b`).** recLUT page
  table + emitted DispatcherReg/Event/JITCompile/Enter/Unmapped stubs + inline
  cycle/event tail; blocks chain in host code. The parked `armjit-reclut-wip` stall was
  the 4.5-invalidation × recLUT interaction: fixed with targeted `recClear` (per-range
  slot reset, not whole-cache `recResetRaw`) + a 1 MB cache-emit headroom (VIXL was
  realloc'ing the MAP_JIT buffer). See CURRENT FOCUS.
- [~] 4.5 Block invalidation on TLB-mapping change. RAM-code page marking landed
  earlier (`recProtectCompiledRange` → `mmap_MarkCountedRamPage`). On `armjit-reclut-v2`
  the coarse whole-cache reset is replaced by **recLUT-backed targeted invalidation**:
  `recClear(addr,size)` resets only the slots in range to `JITCompile`. Still TODO:
  overlap-aware invalidation (a block straddling into the cleared range from an earlier
  start slot is currently missed) and TLB-aware page identity.

---

## Phase 5 — EE Coprocessors

- [ ] 5.1 COP0: interpreter fallback (`recCall(Interp::...)`) initially.
- [x] 5.2 COP1 (FPU) — single-precision suite compiled natively:
  - [x] 5.2a Bit-exact transfer/move/load-store: `MFC1/MTC1/CFC1/CTC1`,
    `MOV_S/ABS_S/NEG_S`, `LWC1/SWC1` (`aR5900FPU.cpp`). No EE float quirks → exact.
  - [x] 5.2b Float arithmetic — EE non-IEEE rounding (`fpuDouble` input clamp +
    checkOverflow/checkUnderflow result clamp + FCR31 flags). Mirrors the **interpreter**
    (single precision, FPU.cpp), not iFPUd. Shared helpers `emitLoadFpuDouble` /
    `emitClampFpuDoubleBits` / `emitStoreClampedResult` in `aR5900FPU.cpp`.
    - [x] `ADD_S/SUB_S/MUL_S` + ACC `ADDA_S/SUBA_S/MULA_S`.
    - [x] `DIV_S/SQRT_S/RSQRT_S` (+ `checkDivideByZero`; over/underflow with no flags).
    - [x] `MADD/MSUB(/A)_S`, `MAX_S/MIN_S`.
    - [x] `C.F/C.EQ/C.LT/C.LE` compares, `BC1T/BC1F` branches, `CVT.S/CVT.W`.
    - [ ] Remaining on interpreter fallback: double W/L-format, likely `BC1FL/BC1TL`.
- [x] 5.3 COP2 (VU0 macro): **inline interpreter fallback** (commit `0108025c9`). COP2
  ops (`0x12`) + `LQC2`/`SQC2` emit an inline `recEmitInterpInline` call instead of
  breaking the block; safe because ARM64 `CpuVU0` is the synchronous VU0 interpreter (no
  microVU sync needed). `BC2*` branches stay single-stepped. Native microVU emission =
  Phase 7.
- [x] 5.4 MMI (128-bit int SIMD): map to NEON where possible (ref `x86/iMMI.cpp`).
  **COMPLETE** — every MMI op compiles natively except QFSRV (runtime SA register →
  interpreter). All decode indices match `tbl_MMI*`; full gtest coverage (269 Arm64EmitEE).
  - [x] First batch (`aR5900MMI.cpp`, commit `6b2ceb311`): NEON-mapped ops —
    `PADD*/PSUB*` (+ signed/unsigned saturating), `PCGT*/PCEQ*`, `PMAX*/PMIN*`,
    `PABSW/PABSH`, `PAND/POR/PXOR/PNOR`, `PEXTL*/PEXTU*`, `PPAC*`,
    `PCPYLD/PCPYUD/PCPYH`. 47 byte-exact gtests vs MMI.cpp.
  - [x] Parallel immediate shifts (`PSLLH/W`, `PSRLH/W`, `PSRAH/W`) — NEON
    `Shl/Ushr/Sshr` with sa=0 fast-path (`Mov`). 18 gtests, all pass.
  - [x] Lane permutes (`PINTH/PINTEH/PEXEH/PEXEW/PREVH/PROT3W/PEXCH/PEXCW`) — NEON
    `Ins` (lane insert), `Rev64` (PREVH). 8 gtests, all pass.
  - [x] Variable shifts (`PSLLVW/PSRLVW/PSRAVW` — amount from GPR[rs], 5-bit masked
    per lane). Scalar GPR shifts (`Lsl`/`Lsr`/`Asr`), 2 lanes per 64-bit store.
    3 gtests, all pass.
  - [x] Multiply-accumulate to HI/LO (`PMADD*/PMSUB*/PMULT*/PHMADH/PHMSBH` + the
    `PMFHI/PMFLO/PMTHI/PMTLO` moves). Word ops use scalar `Smull/Umull` + the EE
    division voodoo (positive 0xFFFFFFFF divisor); halfword ops do 8 lanes to
    LO/HI.UL[0..3]. gtests seed nonzero HI/LO to exercise the accumulate path.
  - [x] Misc (`PADSBH`, `PEXT5/PPAC5`, `PLZCW` [ARM64 `Cls`], `PMFHL` [LW/UW/SLW/LH/SH],
    `PMTHL`). `QFSRV` stays on the interpreter (shift amount = runtime SA register).

---

## Phase 6 — IOP Recompiler (R3000A)

- [ ] 6.1 Create `pcsx2/arm64/aR3000A.{h,cpp}`.
- [ ] 6.2 Implement `psxRec` interface (Reserve/Reset/ExecuteBlock/Clear/Shutdown).
- [ ] 6.3 Port integer / load-store / branch / coprocessor generators (simpler than EE).
- [ ] 6.4 Wire into `VMManager.cpp`.

**Milestone after Phase 6:** playable 2D games expected.

---

## Phase 7 — VU Recompilers (microVU)  *(defer until EE+IOP solid)*

- [ ] 7.1 Study `x86/microVU*.cpp/h` + `microVU_*.inl`.
- [ ] 7.2 Create ARM64 `microVU*` cloning the (arch-agnostic) analysis pass.
- [ ] 7.3 Reimplement VIXL emission for VU upper/lower instructions.
- [ ] 7.4 Map VF regs → NEON `v0-v31`; VI regs → GPRs.
- [ ] 7.5 Handle VU flag regs (`Status`, `MAC`, `Clip`).
- [ ] 7.6 Test VU1-heavy 3D games.

---

## Phase 8 — Integration, Testing & Polish

- [ ] 8.1 Remove the ARM64 "UNSUPPORTED CONFIGURATION" warning (`CMakeLists.txt:84-93`).
- [ ] 8.2 Real `SaveStateBase::vuJITFreeze()` (replace the empty-byte hack in `RecStubs.cpp`).
- [ ] 8.3 Full unit-test suite green on ARM64.
- [ ] 8.4 Game-compat matrix: 2D first, then 3D.
- [ ] 8.5 Profile + optimize hot paths.
- [ ] 8.6 Re-enable LTO for ARM64 if stable.
- [ ] 8.7 macOS specifics (entitlements, Metal shader compile, MoltenVK).

---

## Test ladder (use to validate each milestone)

1. `unittests` target stays green.
2. PS2 BIOS boots (no disc) — fewer complex ops than games.
3. Simple 2D game (e.g. Gradius III/IV).
4. IOP-heavy title (e.g. a PS1-mode game) — exercises IOP rec.
5. 3D game (e.g. Final Fantasy X) — heavy EE + VU.
