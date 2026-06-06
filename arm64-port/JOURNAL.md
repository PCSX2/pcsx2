# JOURNAL — ARM64 Recompiler Port

> Append-only log, **newest entry at the top**. One entry per working session.
> A fresh session reads the top 1–2 entries to recover context.
> Copy the template below for each new entry. Keep entries factual and short.

---

## ENTRY TEMPLATE (copy this)

```
## YYYY-MM-DD — <session title>

**Goal:** <what this session set out to do>

**What changed:**
- <files touched / features added>
- Commits: <short hashes + one-line each>

**Decisions & rationale:**
- <design choice and *why* — this is the part future sessions need most>

**Blockers / open questions:**
- <anything stuck, or "none">

**Next step:** <the single most concrete next task — must match PROGRESS ▶ CURRENT FOCUS>
```

---

## 2026-06-06 — Phase 7.3 (part 1): pass-1 analysis in aVU_Analyze.inl

**Goal:** Task 7.3 — port the arch-neutral pass-1 analysis (`microVU_Analyze.inl`) onto the
already-cloned IR structs, plus the macro layer it needs. Pure infra; microVU stays unselected.

**What changed:**
- `pcsx2/arm64/aVU_Analyze.inl` — **near-verbatim** clone of x86 `microVU_Analyze.inl`. No code
  changes beyond the header comment: the pass is fully arch-neutral (operates on
  `microOp`/`microIR`/`microVFreg`/`microVIreg` + the `microRegInfo` key, zero emitter calls).
- `pcsx2/arm64/aVU_Misc.h` (new) — arch-neutral subset of x86 `microVU_Misc.h`: instruction-field
  extractors, IR-state accessors (`mVUregs`/`iPC`/`incPC`/`mVUup`/`mVUlow`/`sFLAG`/…), `branchSTR`,
  the recompiler-pass signature macros (`mV`/`mP`/`pass1`/`mVUop`/`Fnptr_mVUrecInst`), and the
  optimization-option constexprs. Dropped all x86emitter-coupled bits (xmm typedefs, `mVUglob`,
  host register names, shuffle-imm helpers).
- `pcsx2/arm64/aVU.cpp` — `#include`s both; adds `mVUanalyzeCompileCheck` (never called)
  odr-using every analyzer; removed the two redundant `const bool isVU1` dispatcher locals (now
  the `isVU1` macro). `pcsx2/CMakeLists.txt` — listed the two new files.
- Commit: `863de3e77` ARM64: microVU Phase 7.3 — pass-1 analysis (aVU_Analyze.inl + aVU_Misc.h)

**Decisions & rationale:**
- **`aVU_Analyze.inl` ported with no edits.** The analysis pass is the cleanest arch-neutral
  piece in microVU — it builds the pipeline-state key + per-op read/write/flag info that pass-2
  emission later consumes. Keeping it byte-identical to x86 (modulo the header comment) means the
  ground-truth semantics carry over verbatim and future x86 fixes are trivial to mirror.
- **Macro layer split into its own header (`aVU_Misc.h`)** rather than expanded inline in
  `aVU.cpp` (as 7.2c did for the handful of `microVU_Misc.h` macros it needed). The analysis pass
  needs ~50 macros, and the 7.4 compile driver + 7.5 emit handlers will need the same set, so a
  shared header avoids re-expanding them three times. Only the arch-neutral macros are included;
  the x86 register/emit macros are deliberately absent (ARM map is in `aVU_IR.h`).
- **`isVU1`/`isVU0` as macros collided with 7.2d dispatcher locals** of the same name. The macro
  (`mVU.index != 0`) is identical to the locals (`mVU.index == 1`), so I just deleted the locals.
- **Scope trim, documented in PROGRESS:** the 7.3 bullet also named `microVU_Tables.inl` and the
  Compile.inl pipeline helpers. The **tables** reference 256+ per-op emit handlers that don't
  exist yet → moved to **7.5** (they can't compile standalone). The **Compile.inl pipeline/cycle/
  flag helpers** are arch-neutral but interleaved with the emit-coupled compile driver in that
  file, so they come over with the **7.4** driver port. This keeps the commit a clean,
  buildable, analysis-only slice.

**Blockers / open questions:** None. Still unselected ⇒ analysis is compiled (via the check
function) but not yet driven; first real exercise is when the 7.4 driver calls `mVUopL/mVUopU`
pass1 → these analyzers.

**Next step:** Task 7.3 part 2 / 7.4 — port the arch-neutral pipeline/cycle/flag-analysis helpers
from `microVU_Compile.inl` (`mVUsetupRange`/`mVUincCycles`/`mVUsetCycles`/`mVUoptimizePipeState` +
`eBitPass1`/`branchWarning`/`mVUcheckBadOp`) into `aVU.cpp`, deferring the emit-coupled compile
driver + opcode tables to the 7.5 VIXL emit handlers.

---

## 2026-06-05 — Phase 7.2d: microVU dispatcher in aVU.cpp

**Goal:** Task 7.2d — port the microVU dispatcher + helper thunks (x86
`microVU_Execute.inl` 23–315) and `mVUexecute`/`mVUcleanUp` to VIXL, replacing the 7.2c
`pxFailRel` stubs. Pin the final ARM64 microVU ABI; wire `mVUreset`'s real emitter setup.
Pure infra; microVU stays unselected.

**What changed:**
- `pcsx2/arm64/aVU.cpp` — replaced the five dispatcher/thunk stubs + the `mVUreset` emitter
  placeholder with real VIXL emission:
  - **`mVUdispatcherAB`** / **`mVUdispatcherCD`** — entry/exit + xgkick resume/exit dispatchers.
  - **`mVUGenerateWaitMTVU`** / **`CopyPipelineState`** / **`CompareState`** — the helper thunks.
  - **`mVUexecute<v>`/`mVUcleanUp<v>`** + the `mVUexecuteVU0/1`/`mVUcleanUpVU0/1` wrappers, and
    the local C helper `mVUwaitMTVU`.
  - **`mVUgenerateDispatchers`** — `armSetAsmPtr(mVU.cache)` + one `armStartBlock`/`armEndBlock`
    session wrapping all five generators; sets `codeStart`/`codePtr` past the dispatchers.
    `mVUreset` now calls this (was 5 stub calls + a `codeStart = mVU.cache` placeholder).
  - `mVUblockFetch`/`mVUentryGet` remain `pxFailRel` stubs (block compiler = 7.3/7.4).
- Commit: `a0d93ed5c` ARM64: microVU Phase 7.2d — dispatcher (aVU.cpp)

**Decisions & rationale:**
- **AAPCS64 frame = `armBeginStackFrame(true)`** (saves x19–x28, fp/lr, d8–d15 low halves) — the
  ARM equivalent of x86's `xScopedStackFrame(false, true)`. The frame spans entry → `br x0`
  (into the block) → exit label → `ret`, so the block runs inside the dispatcher's frame.
- **FPCR via `msr FPCR`**: on ARM64 `EmuConfig.Cpu.{VU0,VU1,FPU}FPCR.bitmask` is already a *u64
  ARM FPCR value* (`common/FPControl.h`), not an x86 MXCSR — so the dispatcher just loads the
  bitmask and `msr FPCR, xN`. Gated by the ported `mvuNeedsFPCRUpdate` (skip when VU==EE FPCR).
- **PQ NEON reg built directly** instead of replaying the x86 SHUFPS/PSHUFD/MOVSS dance. NEON
  V4S lane order == xmm, so I derived the x86 final layout (VU0 `[Q,pending_q,P,P]`, VU1
  `[Q,pending_q,P,pending_p]`) and build it with `Ldr .S()` + `Ins .V4S()` — same result, far
  less error-prone. Documented the layout so 7.5 (which reads Q/P from these lanes) can mirror
  x86. PQ lives in `v24` (= `mVU_xmmPQ`), backed up to `vecBackup[24]` for xgkick.
- **`compareStateF` returns `w0=0` iff equal** (the block search uses `== 0` as the match test):
  6×128-bit `Cmeq` (0xffffffff per equal lane), AND-reduced, then `Uminv` min-byte (`0xff` iff
  all equal) → `Umov` → `Eor …, 0xff`. Faithful to x86's `movmskps ^ 0xf == 0`.
- **No constant pool for the dispatcher** (`armSetAsmPtr(…, nullptr)`): it only needs
  `adrp/mov` addresses and `mov+blr` C calls (`armEmitCall` with null pool). The block compiler
  (7.4/7.5) will set up the real pool when it emits program blocks past `codeStart`.
- **`mVUexecute` does NOT open the emitter; `mVUcleanUp` does NOT close it.** x86 repositions a
  single global cursor here (`xSetPtr(mVU.prog.x86ptr)` / `mVU.prog.x86ptr = x86Ptr`). On ARM the
  per-block `armStartBlock`/`armEndBlock` — and crucially the **icache flush that must precede
  executing freshly-emitted code** (x86 needs none) — belong to the block compiler. So I left a
  TODO there; `mVUcleanUp`'s cache-limit check uses `mVU.prog.codePtr` (maintained by the block
  compiler) instead of the x86 `xGetPtr()`.
- **`mVUGenerateWaitMTVU` is conservative**: it's a transparent helper the reg-allocator can't
  see, so it saves the full caller-saved VF/PQ pool (`v0-v24`) + VI GPRs (`x0-x15`). Heavier than
  the x86 thunk but correct; only reached under MTVU. Will tighten if profiling matters.

**Blockers / open questions:** None for the ABI. The dispatcher is unselected, so it is
**compiled but not executed** — first real validation is at 7.8 (selection). The two things 7.4
must get right before any execution: the per-block emit lifecycle (own `armStartBlock`/
`armEndBlock`) and the **icache flush before branching into a freshly-emitted block**.

**Next step:** Task 7.3 — port the arch-neutral analysis pass (`microVU_Analyze.inl` +
`microVU_Tables.inl` + the pipeline/flag-analysis helpers in `microVU_Compile.inl`). No emitter
calls, so it ports almost unchanged onto the `microOp`/`microIR` structs already in `aVU.h`.

---

## 2026-06-05 — Phase 7.2c: recompiler shell in aVU.cpp

**Goal:** Task 7.2c — flesh out `pcsx2/arm64/aVU.cpp` mirroring x86 `microVU.cpp`: define the
`microVU0/1` + `CpuMicroVU0/1` globals (now that the allocator is a complete type) and port the
arch-neutral program/block-cache housekeeping + the `recMicroVU0/1` provider methods. Pure infra;
microVU stays unselected.

**What changed:**
- `pcsx2/arm64/aVU.cpp` — replaced the 7.2a/7.2b minimal TU with the full shell:
  - **Globals:** `alignas(16) microVU microVU0/1;` + `recMicroVU0/1 CpuMicroVU0/1;`. The
    `microVU` dtor (destroying `unique_ptr<microRegAlloc>`) now instantiates because aVU_IR.h
    makes `microRegAlloc` complete.
  - **Cache mgmt:** `mVUinit`/`mVUreset`/`mVUclose`/`mVUclear`, `mVUdeleteProg`/`mVUcreateProg`/
    `mVUcacheProg`/`mVUrangesHash`/`mVUprintUniqueRatio`/`mVUcmpProg`, and templated
    `mVUsearchProg` (explicit `<0>`/`<1>` instantiations to keep it compiled until `mVUexecute`
    at 7.2d).
  - **Providers:** all `recMicroVU0/1::*` methods (Reserve/Shutdown/Reset/Step/SetStartPC/
    Execute/Clear/ResumeXGkick).
  - **`SaveStateBase::vuJITFreeze`** now freezes the real `microVU0/1.prog.lpState`.
- `pcsx2/arm64/RecStubs.cpp` — removed its placeholder `vuJITFreeze` (froze 96 empty bytes
  twice) now that aVU.cpp owns the real one; left a pointer comment. (Fixed the duplicate-symbol
  link error this caused.)
- Commit: `532adca92` ARM64: microVU Phase 7.2c — recompiler shell (aVU.cpp)

**Decisions & rationale:**
- **Expanded the x86 `microVU_Misc.h` macros inline** (`mV`/`mVUx`/`_mVUt`/`mVUrange`/
  `doWholeProgCompare`) instead of pulling that header — it is x86emitter-coupled. Kept the TU
  free of x86 dependencies, same approach as 7.2a's aVU.h. `doWholeProgCompare` cloned as a local
  `static constexpr bool = false`; `mVUdumpProg` cloned as the default no-op macro.
- **Renames carried through:** `prog.x86ptr/x86start/x86end` → `codePtr/codeStart/codeEnd` in
  `mVUinit`/`mVUreset`/`mVUcreateProg` (matches aVU.h's `microProgManager`).
- **Stubbed the codegen/compile layer, not faked it.** `mVUreset`/`mVUsearchProg` call into the
  dispatcher generators (`mVUdispatcherAB/CD`, `mVUGenerateWaitMTVU/CopyPipelineState/
  CompareState` — task 7.2d) and the block compiler (`mVUblockFetch`/`mVUentryGet` — later). All
  forward-declared and `pxFailRel`-stubbed at the bottom of the file. Chose `pxFailRel` (loud)
  over silent no-ops: microVU is unselected (VMManager pins `CpuIntVU0/1`) so the providers are
  never `Reserve()`'d/`Reset()`'d/`Execute()`'d on ARM64 — nothing reaches a stub — but if that
  wiring ever lands before the real codegen, it aborts instead of jumping to null `startFunct`.
- **`mVUreset` emitter setup deferred.** x86 does `xSetPtr(mVU.cache)` then
  `codeStart = xGetAlignedCallTarget()` (just past the emitted dispatchers). 7.2c can't emit
  yet, so `codeStart = codePtr = mVU.cache` is a placeholder with a TODO; 7.2d wires
  `armSetAsmPtr(mVU.cache, ...)` and sets codeStart past the real dispatchers.
- **Kept `mVUallocCompileCheck`** — the cache-mgmt code never calls the allocator's *emission*
  members (the block compiler/dispatcher do, not yet ported), so without the explicit odr-use
  their VIXL bodies wouldn't instantiate and emission bugs would hide.

**Blockers / open questions:** None. The shell is inert until 7.2d gives it a dispatcher; the
emitter-setup placeholder in `mVUreset` is the one thing 7.2d must fix before any execution.

**Next step:** Task 7.2d — port the dispatcher (`mVUdispatcherAB`/`mVUdispatcherCD` +
`mVUGenerateWaitMTVU`/`mVUGenerateCopyPipelineState`/`mVUGenerateCompareState`, x86
`microVU_Execute.inl` 23–315) and `mVUexecute`/`mVUcleanUp` to VIXL. Pins the final ARM64
microVU ABI; replace the 7.2c stubs and wire `mVUreset`'s real `armSetAsmPtr` setup.

---

## 2026-06-05 — Phase 7.2b: port microRegAlloc to aVU_IR.h

**Goal:** Task 7.2b — clone the x86 microVU host register allocator (`microRegAlloc`,
`microVU_IR.h` 226–1139) to ARM64 so the dispatcher (7.2d) and opcode emission (7.5) have a
working allocator: VF→NEON v-regs, VI→ARM w-regs. Header-only infra; microVU stays unselected.

**What changed:**
- New `pcsx2/arm64/aVU_IR.h` — the ARM64 `microRegAlloc` + `microMapXMM`/`microMapGPR` structs
  + the NEON emit helpers `mVUloadReg`/`mVUsaveReg`/`mVUmergeRegs`/`loadIreg` (ported from
  `microVU_Misc.inl`) + the provisional ARM64 microVU register-map constants.
- `pcsx2/arm64/aVU.cpp` — `#include "arm64/aVU_IR.h"` + `mVUallocCompileCheck()` (never called)
  that odr-uses the allocator's emission methods so the VIXL codegen is actually compiled.
- `pcsx2/CMakeLists.txt` — registered aVU_IR.h in `pcsx2arm64Headers`.
- Commit: `80747acd0` ARM64: microVU Phase 7.2b — port microRegAlloc (aVU_IR.h)

**Decisions & rationale:**
- **Dropped the COP2/macro-mode path entirely** (`regAllocCOP2`, `_allocVFtoXMMreg`/
  `_allocX86reg`, `pxmmregs`/`x86regs`, `updateCOP2AllocState`, `flushPartialForCOP2`,
  `clearRegCOP2`/`clearGPRCOP2`). That's the EE-side VU0-macro allocator = Phase 7.9; ARM64
  macro ops already run via the Phase 5.3 inline-interp fallback. Removing it makes the port a
  faithful but much smaller mirror of the x86 "normal" (microVU-thread) allocator path and
  removes all the x86-iCore-coupling that wouldn't translate.
- **Addressing model:** x86 uses absolute-address `ptr[&getVF(n)]` (address baked into the
  instruction). ARM64 can't, so every VF/VI/ACC/I access is base+offset against a designated
  state pointer `RVUSTATE=x19` (= `&vuRegs[index]`), with byte offsets via `offsetof(VURegs,...)`.
- **NEON lane semantics:** V4S lane 0 = lowest 32 bits = X, identical to xmm lane order, so the
  `xyzw` mask (bit8=X..bit1=W) maps one-to-one. `mVUloadReg`/`mVUsaveReg`/`mVUmergeRegs` ported
  to preserve the exact x86 lane behaviour incl. the `modXYZW` single-subvector-in-lane0 case.
  Partial stores (Y/Z/W) compute the address into `RVUADDR=x17` first because ARM single-lane
  `St1` only takes a base register, not an immediate offset (X/full use `Str` with offset).
- **All VF pool regs treated caller-saved** (`vfIsCallerSaved` ≡ true): AAPCS64 preserves only
  the *low 64 bits* of v8–v15 across a C call, but VF is 128-bit, so none survive a call intact.
  `flushCallerSavedRegisters` writes back every cached VF reg before a call — same net behaviour
  as x86 SysV (no xmm callee-saved there). GPR caller-saved test reuses `armIsCalleeSavedRegister`
  (callee-saved = reg≥19).
- **Compile-exercised, not "should build":** `mVUallocCompileCheck` constructs the allocator and
  calls `allocReg`/`allocGPR`/`moveVIToGPR`/`flushAll`/`flushCallerSavedRegisters`/`TDwritebackAll`
  so the emission member bodies are instantiated and any VIXL type error surfaces now.

**Blockers / open questions:** The register map (pools, `RVUSTATE`, `gprT1/T2`, `gprF0..3`,
`mVU_xmmPQ`, `RVUADDR`) is provisional — the dispatcher (7.2d) will pin the final ABI and may
shuffle these. The NEON emit helpers' lane correctness is only validated by inspection (vs the
x86 original); first real execution is after 7.2d/7.5.

**Next step:** Task 7.2c — flesh out `pcsx2/arm64/aVU.cpp` mirroring `microVU.cpp`
(`mVUinit`/`mVUreset`/`mVUclose`/`mVUclear`, program-cache mgmt, `recMicroVU0/1::*`) and define
the `microVU0/1` globals (the allocator is complete now, so `microVU`'s `unique_ptr` dtor can be
instantiated). Then 7.2d the dispatcher (`mVUdispatcherAB/CD`).

---

## 2026-06-05 — Phase 7.2a: port arch-neutral microVU structs to aVU.h

**Goal:** Task 7.2a — clone the arch-neutral microVU data structures into a new
`pcsx2/arm64/aVU.h` so later Phase 7 tasks (regalloc, dispatcher, opcode emit) have the IR /
program-cache / pipeline-state types to build on. Header-only infra; microVU stays unselected.

**What changed:**
- New `pcsx2/arm64/aVU.h` — arch-neutral structs cloned from x86 `microVU_IR.h` + `microVU.h`:
  `regCycleInfo`, `microRegInfo` (96-byte pipeline key), `microJumpCache`, `microBlock`,
  `microTempRegInfo`, `microVFreg`/`microVIreg`, `microConstInfo`, `microUpperOp`/`microLowerOp`,
  `microFlagInst`/`microFlagCycles`, `microOp`, `microIR`, `microBlockLink(Ref)`, `microRange`,
  `microProgram(List/Quick)`, `microProgManager`, `microVU`, `microBlockManager`.
- New `pcsx2/arm64/aVU.cpp` — minimal TU (the start of the 7.2c shell): includes the header,
  runs layout `static_assert`s. Exists so the header is actually compiled this session.
- `pcsx2/CMakeLists.txt` — registered aVU.cpp/aVU.h in `pcsx2arm64Sources/Headers`.
- Commit: `c3f398318` ARM64: microVU Phase 7.2a — arch-neutral structs (aVU.h)

**Decisions & rationale:**
- **Renames (per plan):** `x86ptr/x86start/x86end`→`codePtr/codeStart/codeEnd`,
  `x86ptrStart`→`codeStart`. ARM host-reg differences: `xmmBackup[16][4]`→`vecBackup[32][4]`
  (NEON has 32 vector regs vs x86's 16 xmm), `xmmCTemp`→`vecCTemp`.
- **`microRegAlloc` only forward-declared** — it's the arch-specific allocator (7.2b). `microVU`
  holds it by `std::unique_ptr`, so the incomplete type is fine in the header; the `microVU0/1`
  globals are `extern` here and will be *defined* in `aVU.cpp` at 7.2c where the allocator is
  complete (x86 defines them in the header because microVU is one TU — we can't, aVU.cpp may be
  one of several Phase-7 TUs).
- **Shared `microVU_Profiler.h` instead of re-cloning** — that header is 100% arch-neutral (no
  x86emitter, no x86 types; just the `microOpcode` enum + a compiled-out profiler), so aVU.h
  `#include`s it directly. Single source of truth for the opcode enum the tables/analysis need.
- **Inlined the macro use in `microBlockManager::search`** (`mVUsFlagHack` → the underlying
  `mVU.prog.IRinfo.sFlagHack`; kept `doConstProp` as a local `constexpr`) so aVU.h doesn't have
  to pull/define the x86 microVU_Misc macros — keeps the header
  self-contained and free of macro pollution.
- **Real compile check, not "should build":** rather than leave an untested header until 7.2c,
  added the minimal aVU.cpp now. `sizeof(microRegInfo)==96`/`alignof==16` static_asserts pass on
  ARM64 → the union+bitfield+`u128 full128[]` layout ported byte-for-byte. Needed
  `common/AlignedMalloc.h` (for `_aligned_malloc/free` used by microBlockManager) which Common.h
  doesn't transitively pull on this TU.

**Blockers / open questions:** none. The CLAUDE.md test command `ctest --test-dir build` reports
"No tests found"; the suites actually live under `build/tests/ctest/` — run ctest from there
(2/2 pass). (Minor doc drift, not fixed this session.)

**Next step:** Task 7.2b — port `microRegAlloc` (`microVU_IR.h` 226–1139) to ARM64 in
`pcsx2/arm64/aVU_IR.h`: NEON v-regs for VF, ARM w-regs for VI; mirror `allocReg`/`allocGPR`/
`writeBackReg`/`clearNeeded`/`flushAll`. Drop the x86 `regAllocCOP2`/`_allocVFtoXMMreg` path
(EE-side macro mode = Phase 7.9). Then 7.2c fleshes out aVU.cpp + defines the globals.

---

## 2026-06-05 — Phase 7 kickoff: study x86 microVU (task 7.1)

**Goal:** Begin Phase 7 (VU recompilers / microVU) — the last big interpreter-bound component.
User chose this over IOP const-prop/reg-alloc. This session = task 7.1: study the x86 microVU
deeply enough to commit to an architecture and a buildable sub-task decomposition.

**What changed:**
- Docs only (no code yet — 7.1 is a study/planning task). Updated PROGRESS.md (new CURRENT FOCUS
  + expanded Phase 7 checklist into buildable sub-tasks 7.2a…7.9), this JOURNAL entry, and a new
  memory [[arm64-microvu-architecture]].

**What I studied:** all of `pcsx2/x86/microVU*` (~10,600 lines, 18 files) + `VUmicro.h` +
the VU provider wiring in `VMManager.cpp`. Read in full: `microVU.h`, `microVU_IR.h`,
`microVU.cpp`, `microVU_Execute.inl`; surveyed the rest.

**Key findings (the architecture):**
- microVU is a **program-level** recompiler (not block-at-a-time like EE/IOP). `mVUsearchProg`
  caches whole microprograms (4KB/16KB VU micro memory), detects program change via a **96-byte
  `microRegInfo` pipeline-state compare**, recompiles blocks within a program keyed by that state,
  and links blocks in host code (`microBlockManager` quick/full search).
- **Entry contract:** `recMicroVU0/1::Execute` calls `mVU.startFunct` (gen'd by `mVUdispatcherAB`):
  load VU FPCR, the PQ NEON reg (Q/P latency instances), mac/clip/status flag instances → jump to
  block → on exit write flags back + `mVUcleanUp` (cycle accounting, cache-limit reset).
  `mVUdispatcherCD` = XGKICK resume/exit.
- **Arch split:** arch-neutral = structs + `microVU_Analyze.inl` (pipeline/flag-instance analysis)
  + `microVU_Tables.inl`. Arch-specific (VIXL rewrite) = `microRegAlloc`, `Upper.inl`, `Lower.inl`
  (the 2203-line beast), `Flags.inl`, `Branch.inl`, `Execute.inl`, `Misc/Clamp/Alloc.inl`,
  `Macro.inl`.

**Decisions & rationale:**
- **Parallel clone in `pcsx2/arm64/`, never touch x86 microVU** (hard rule #1). microVU.h/IR.h
  intertwine the arch-neutral structs with x86emitter types + the x86 `microRegAlloc` inline in the
  same headers, so I can't cleanly `#include` them on ARM64 — cloning the structs into `aVU.h` is
  the clean path. Cost: duplicating the arch-neutral analysis pass; benefit: zero x86 risk.
- **microVU stays unselected until the rec works.** Unlike the IOP skeleton (which could single-step
  the interpreter per-op), microVU's model is whole-program compilation — there's no cheap
  "compile a block that steps the interpreter". So `CpuVU0/CpuVU1` stay pinned to `CpuIntVU0/1`
  (VMManager) through 7.7, flipped only at 7.8. Mirrors EE Phases 1–4 before `Cpu = &recCpu`.
- **Macro mode (7.9) is lowest priority** — the EE-side COP2/VU0-macro ops already run correctly via
  the Phase 5.3 inline-interp fallback; native macro emission is pure polish.

**Blockers / open questions:** none. The big unknown is sheer scale (Lower.inl + the flag pipeline
+ regalloc are intricate); the decomposition front-loads the skeleton/dispatcher so each later
opcode batch is independently buildable+testable like the EE/IOP generators were.

**Next step:** Task 7.2a — port the arch-neutral microVU structs into `pcsx2/arm64/aVU.h`
(rename `x86ptr/x86start/x86end` → `codePtr/codeStart/codeEnd`, drop x86emitter), then 7.2b the
NEON/ARM `microRegAlloc`, then 7.2c the `aVU.cpp` shell + CMake so it builds (not yet selected).

---

## 2026-06-05 — Fix pause/shutdown hang (fastjmp_set returns_twice)

**Goal:** Investigate a user-reported bug: pressing Pause or Shutdown didn't stop the VM —
emulation "sped up" (ran unthrottled) and never stopped. Reset worked. Pre-existing (not
this session's IOP work); root-caused and fixed.

**What changed:**
- **`common/FastJmp.h`:** mark `fastjmp_set` `__attribute__((returns_twice))` (GCC/Clang).
  Commit `69a3bd77d`.

**Root cause (found by tracing + disassembling `recExecute`):**
- `recExecute()` calls `fastjmp_set()` (sets the exit longjmp target) then enters
  `EnterRecompiledCode()` (the recLUT dispatcher, which only exits via `fastjmp_jmp` back to
  that target). Because `fastjmp_set` was a plain `int` function, Clang didn't know control
  can re-enter after it and **tail-call-optimized** the `EnterRecompiledCode()` call on
  ARM64: it deallocated `recExecute`'s frame (`add sp,#0x30; br x0`) before branching in. But
  `fastjmp_set` had captured SP pointing into that freed frame. The whole EE dispatcher +
  event tests then ran on top of it, clobbering `recExecute`'s saved x30. On pause/shutdown,
  `fastjmp_jmp` restored SP to the dead frame and `recExecute`'s epilogue reloaded a garbage
  x30 → branched back into the dispatcher → infinite loop. VSyncStart skips the frame limiter
  once `IsExecutionInterrupted()` is true, so the loop ran unthrottled ("sped up").
- Why reset seemed to work / pause didn't: same exit path; the loop just manifests as a
  never-ending unthrottled run for any non-Running state.

**Fix:** `returns_twice` makes the compiler keep the caller's frame live across the call and
NOT tail-call the following `EnterRecompiledCode()` — verified in the disassembly: the call
is now `blr x8` (was `add sp,#0x30; br`). Pause + Shutdown both confirmed working live.

**Debugging method (for next time):** added temporary `[EXITDBG]` Console.WriteLn traces
across the exit path (VMManager::SetState, recSafeExitExecution, recEventTest, recExecute
enter/return, VMManager::Execute, EmuThread loop) → the loop showed `recExecute` fastjmp-
return running but never reaching `Execute() returned`. `nm` + `lldb disassemble` of
`recExecute` then revealed the TCO. All trace lines reverted with `git restore`, keeping only
the fix. See [[arm64-fastjmp-returns-twice]].

**Blockers / open questions:** none.

**Next step:** (unchanged) Live game IOP-perf measurement, then IOP opt (const-prop/reg-alloc)
or Phase 7 (VU / microVU).

---

## 2026-06-05 — Phase 6.3 IOP COP0/COP2 inline-interp (Phase 6.3 complete)

**Goal:** Stop the IOP rec breaking the block on coprocessor ops — inline the interpreter
handler (EE Phase 5.1/5.3 trick) for the straight-line COP0/COP2/GTE ops so blocks stay
intact. This completes the IOP rec's native opcode coverage.

**What changed:**
- **`pcsx2/arm64/aR3000A.cpp`:** added recTranslateOp cases 0x10 (COP0), 0x12 (COP2),
  0x32 (LWC2), 0x3a (SWC2) that call `recEmitInterpInline(op)` and return true. RFE
  (COP0 rs==0x10) also emits `armEmitCall(iopTestIntc)`. Added `#include "IopDma.h"`.
- Commit `9095d983b`.

**Decisions & rationale:**
- **The IOP has no COP0/COP2 branches.** Verified against the interpreter tables
  (`psxCP0`/`psxCP2BSC` in R3000AOpcodeTables.cpp): every BC0/BC2 slot is `psxNULL`. So all
  valid COP0/COP2 ops are straight-line and never write pc → safe to inline mid-block. This
  is simpler than the EE case (which had to keep BC0*/BC2* on the single-step path).
- **Mirror the x86 IOP rec exactly.** Its `REC_GTE_FUNC` and `rpsxCP0` table are literally
  "set psxRegs.code, flush, call the interpreter handler" with no block break — identical to
  `recEmitInterpInline`. So inline-interp is the reference behavior, not a shortcut.
- **RFE → iopTestIntc.** The interpreter's `psxRFE` only restores CP0.Status (no intc test);
  the x86 `rpsxRFE` adds `iopTestIntc` to promptly raise pending interrupts after the
  interrupt-enable is restored. I match it. Checked `iopTestIntc` (R3000A.cpp:242): it only
  sets the next branch/event delta — it does NOT vector to the exception handler or touch
  pc — so calling it mid-block is safe. Bonus: the common `jr $k0; rfe` return-from-handler
  (RFE in the JR delay slot) now compiles fully natively.
- **psxCOP0/psxCOP2 self-dispatch.** `recEmitInterpInline` calls `psxBSC[op>>26]` =
  psxCOP0/psxCOP2, which re-dispatch on rs/funct from psxRegs.code — so one case per primary
  opcode covers the whole group (incl. psxNULL for invalid sub-ops, harmless, matches interp).
- **Cycle:** each inlined op charges 1 cycle via the existing block_cycles tail, matching
  execI (1 cycle/op).

**Blockers / open questions:** none. **Phase 6.3 is complete** — the IOP rec now compiles
its entire practical opcode set natively; only SYSCALL/BREAK return false (they raise an
exception + rewrite pc, so single-step is the correct model). Live game perf delta not yet
measured.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites); headless `-batch -bios`
boot reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` (COP0-heavy IOP
interrupt/SIO init) with COP0/COP2 inline live — no crash/abort/unmapped, no Unimplemented-op
warnings, clean log.

**Next step:** Live game IOP-perf measurement (FFX / 2D title) to quantify the full Phase 6.3
win, then IOP optimization (constant-prop / reg-alloc) or Phase 7 (VU / microVU).

---

## 2026-06-05 — Phase 6.3 IOP native branches/jumps

**Goal:** Stop the IOP rec breaking the block at every control-flow op — emit native
branch/jump generators + compile the delay slot so blocks span control flow. This is the
real IOP speedup (until now every J/branch ended the native run and single-stepped).

**What changed:**
- **`pcsx2/arm64/aR3000A.cpp`:** added `recEmitIopBranch` (+ helpers `iopWritePcImm/Reg`,
  `iopWriteLink`, `iopSelectPc`, `iopBranchZero`) for J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ,
  REGIMM BLTZ/BGEZ/BLTZAL/BGEZAL; replaced the stub `recIsHandledBranch`; added `recEmitOp`;
  rewrote the block compiler's branch case to emit branch + delay slot + 2 cycles. Fixed
  `recEmitInterpInline` to use the IOP `psxBSC` dispatch table (was wrongly using the EE
  `R5900::GetInstruction`).
- Commit `13daf73d0`.

**Decisions & rationale:**
- **Mirror the EE branch model (aR5900Branch.cpp), narrowed to 32-bit.** Generators emit
  only the pc/link writes; the block compiler compiles the delay slot after and RETs to the
  C++ dispatcher loop, which re-reads psxRegs.pc. Writing pc before the delay slot is safe
  (no IOP delay-slot op writes pc) and required for JR/JALR (target = GPR[rs] *before* the
  delay slot). Target/fallthrough/link constants use `_PC_ == branchpc+4`, identical to the
  interpreter macros (`_JumpTarget_`/`_BranchTarget_`/`_SetLink`).
- **doBranch quirks omitted, matching the x86 rec.** `doBranch` does an a0-override at target
  0xbfc4a000 and ClearIrxModules at 0x890, but the x86 IOP rec's `psxSetBranchReg`/
  `psxSetBranchImm` do NOT replicate them — they're interpreter-only. So the native
  generators skip them too (verified by reading iR3000A.cpp:1113-1146).
- **IRX-import magic preserved by bailing J.** psxJ checks `delay >> 16 == 0x2400` and runs
  `irxImportExec` (IOP module HLE). The x86 rec moves this to the delay-slot ADDIU(rt=0)
  compile (`psxRecompileIrxImport`), which is a bigger port (HLE plumbing). For this slice I
  bail J-with-magic-delay-slot to the interpreter (correct + identical to interp; rare —
  only at import stubs). All other J + JAL/JR/JALR/branches compile natively.
- **Branch-in-delay-slot bailed.** recEmitOp would inline-interp it via psxBSC → nested
  doBranch (second delay slot + pc write). Illegal MIPS, but guarded.
- **Cycle accounting:** branch + delay = 2 cycles (R3000A is 1 cycle/op), charged via the
  existing block_cycles tail. Event test stays at block boundary (recExecuteBlock), same as
  the EE rec and the prior all-interp IOP model; interp's per-branch iopEventTest timing
  difference is the already-accepted dispatch model.

**Blockers / open questions:** none. Live game IOP-perf delta not yet measured (next).
No IOP gtest harness — logic mirrors the gtested EE branch generators; BIOS boot is the
integration check.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites); headless `-batch -bios`
boot reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` (IOP-heavy, branch-dense
pad/SIO init) with native IOP branches live — no crash/abort/unmapped access, no
Branch-to-0x0, clean log (no rec warnings/errors).

**Next step:** Live game IOP-perf measurement (FFX / a 2D title) to quantify the
branches+load/store win, then Phase 6.3 cont. — IOP COP0 (mfc0/mtc0/rfe) + COP2/GTE
inline-interp (EE-style), or move to Phase 7 (VU).

---

## 2026-06-05 — Phase 6.3 IOP native unaligned load/store (LWL/LWR/SWL/SWR)

**Goal:** Finish the IOP load/store family — native codegen for the unaligned ops
(LWL/LWR/SWL/SWR) that the aligned slice left on the interpreter, wired into
`recTranslateOp`.

**What changed:**
- **`pcsx2/arm64/aR3000A.cpp`:** added `iopEmitLWL/iopEmitLWR/iopEmitSWL/iopEmitSWR` and
  wired opcodes 0x22/0x26 (LWL/LWR) + 0x2A/0x2E (SWL/SWR). They read the aligned word at
  EA&~3 (`iopMemRead32`), merge with GPR[rt] using a runtime byte shift `(EA&3)<<3` and the
  per-offset masks, and (stores) `iopMemWrite32` it back. Byte-exact vs psxLWL/LWR/SWL/SWR.
- Commit `3efc4c40c`.

**Decisions & rationale:**
- **Recompute-after-call instead of spilling.** All inputs (GPR[rs], GPR[rt], imm) live in
  psxRegs and are reachable via the callee-saved RESTATEPTR=x19, and the load doesn't modify
  GPR[rs], so after the `iopMemRead32` call I recompute the address and shift rather than
  preserve them across the call. Only `mem` (the call result, in RWRET) has to flow forward.
  No stack spill, no extra callee-saved reg.
- **Stores do read-modify-write between two calls.** SWL/SWR call `iopMemRead32` then
  `iopMemWrite32` with no intervening call, so RWRET/RWARG2-4/RSCRATCHW are all free to carry
  the merged value; I compute the value into RWARG2 and recompute the aligned addr into
  RWARG1 just before the write.
- **rt==0 still reads** (LWL/LWR), matching the interpreter (I/O side effects); GPR[0] reads
  zero so SWL/SWR need no rt==0 case.
- **Variable shifts use lslv/lsrv** (VIXL `Lsl`/`Lsr` with a register operand). Masks
  (0x00ffffff / 0xffffff00) are `Mov`'d then shifted by the runtime `shift` / `24-shift`.
  Verified the offset-0 and offset-3 corners against the tables in R3000AOpcodeTables.cpp.

**Blockers / open questions:** none. Still no IOP gtest harness — logic mirrors the gtested
EE load/store generators; BIOS boot is the integration check.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites); headless `-batch -bios`
boot reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` with the full native IOP
load/store family live — no crash/abort/unmapped access (emulog.txt).

**Next step:** Phase 6.3 cont. — native IOP **branches/jumps** (J/JAL/JR/JALR,
BEQ/BNE/BLEZ/BGTZ + REGIMM): port the EE `recEmitBranch` narrowed to 32-bit so blocks span
control flow instead of single-stepping every branch (the real IOP speedup).

---

## 2026-06-05 — Phase 6.3 IOP native aligned load/store generators

**Goal:** Replace the IOP rec's interpreter single-step for aligned memory ops with native
ARM64 codegen (LB/LBU/LH/LHU/LW, SB/SH/SW) via the `iopMemRead/Write8/16/32` slow path,
wired into `recTranslateOp`.

**What changed:**
- **`pcsx2/arm64/aR3000A.cpp`:** added `iopEmitEffectiveAddr` (GPR[rs]+(s16)imm → RWARG1,
  mirrors `armEmitEffectiveAddr`), `iopEmitLoad(bits,sign,...)` and `iopEmitStore(bits,...)`,
  and wired opcodes 0x20/0x21/0x23/0x24/0x25 (LB/LH/LW/LBU/LHU) + 0x28/0x29/0x2B (SB/SH/SW)
  into `recTranslateOp`. Unaligned 0x22/0x26 (LWL/LWR) + 0x2A/0x2E (SWL/SWR) still return
  false → interpreter single-step.
- Commit `f104adb25`.

**Decisions & rationale:**
- **Slow-path C++ helpers only (no fastmem).** The IOP has no vtlb fastmem; the x86 IOP rec
  also routes through `iopMemRead/Write`. Each generator computes the EA into RWARG1 (stores
  also put GPR[rt] into RWARG2) and `armEmitCall`s the helper. Loads extend the RWRET result
  (Sxtb/Uxtb/Sxth/Uxth; LW = full 32-bit) into GPR[rt].
- **Read performed even when rt==0.** Matches `psxLB..psxLW`: the access can have I/O side
  effects, so only the GPR write is suppressed, not the read.
- **No cross-call scratch state.** The helper call clobbers caller-saved x16/x17, but
  RESTATEPTR=x19 is callee-saved and survives, and every generator reads its inputs from
  psxRegs fresh. This is the first native multi-op IOP block to call C++ mid-block; the
  prologue's `stp x19,lr,[sp,#-16]!` keeps sp 16-aligned at the BL (AAPCS64), same as EE
  blocks that call vtlb helpers.
- **Load-delay-slots ignored** (as in the x86 IOP rec), so writing GPR[rt] immediately and
  compiling the following op natively is correct.
- **Unaligned LWL/LWR/SWL/SWR deferred.** They read mem, then read-modify-write GPR[rt]/mem
  with a runtime byte shift+mask — needs spilling the EA/value across the `iopMemRead32`
  call. Cleaner as its own atomic commit; left on the interpreter for now.

**Blockers / open questions:** none. No IOP gtest harness yet — the load/store logic mirrors
the gtested EE generators (`aR5900LoadStore.cpp`); BIOS boot is the integration check.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites); headless `-batch -bios`
boot reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` (IOP-heavy pad/SIO init)
with native IOP load/store live — no crash/abort/unmapped access (emulog.txt).

**Next step:** Phase 6.3 cont. — unaligned **LWL/LWR/SWL/SWR**, then **branches/jumps**
(J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ + REGIMM) so the rec stops breaking the block at every
control-flow op (the real IOP speedup).

---

## 2026-06-05 — Phase 6.3 IOP native integer generators

**Goal:** Replace the IOP rec's all-interpreter single-step skeleton with native ARM64
codegen for the R3000A integer subset (the simplest, highest-frequency ops), wired into
`recTranslateOp`.

**What changed:**
- **`pcsx2/arm64/aR3000A.cpp`:** added native generators + rewrote `recTranslateOp` to
  dispatch them. Families: I-type (ADDI/ADDIU/SLTI/SLTIU/ANDI/ORI/XORI/LUI), R-type ALU
  (ADD/ADDU/SUB/SUBU/AND/OR/XOR/NOR/SLT/SLTU), shifts (SLL/SRL/SRA + V variants), HI/LO
  moves (MFHI/MTHI/MFLO/MTLO), mul/div (MULT/MULTU/DIV/DIVU). Everything else still
  returns false → ends the native run, interpreted one-at-a-time.
- Commit `86448e99c`.

**Decisions & rationale:**
- **Mirror the gtested EE generators, narrowed to 32-bit.** The IOP ops are a strict
  subset of `aR5900Arith.cpp` / `aR5900MultDiv.cpp` (already unit-tested), minus the
  sign-extend-to-64 (IOP GPR/HI/LO are u32) and minus the 64-bit D* ops. So I reused the
  exact proven structure (same scratch discipline: x17 manual scratch, x16 plain operand)
  and just stored W-views. No new gtest harness for IOP this pass — the logic is
  byte-identical to gtested EE code; BIOS boot is the integration check.
- **R3000A MULT/MULTU write only HI/LO, no Rd.** Unlike the R5900 3-operand form
  (`emitMult` writes Rd=LO). Confirmed against `psxMULT`/`psxMULTU` in
  `R3000AOpcodeTables.cpp` — they only set GPR.n.lo/hi.
- **DIV/DIVU: ARM SDIV/UDIV reproduce the quirks; only ÷0 fixed up.** SDIV gives
  0x80000000 for INT_MIN/−1 with remainder 0 (matches the x86-overflow branch), and ÷0
  returns 0 so the remainder = dividend = the required HI. Only LO needs the ÷0 fixup
  (signed: (Rs<0)?1:−1 via `Csneg`; unsigned: −1). Same trick the EE `emitDivS/emitDivU`
  use.
- **Control flow / loads / coprocessor stay on the interpreter (return false).** Native
  blocks never compile a branch or its delay slot, so the interpreter's `doBranch`
  handles branch+delay atomically — the model stays correct. Load-delay-slots are ignored
  on the IOP (as in the x86 IOP rec), so compiling the instruction after a load natively
  is safe.

**Blockers / open questions:** none. Live game IOP-perf delta not yet measured — the win
is small until branches/jumps stop breaking the block (next slice), since right now every
control-flow op still ends the native run and single-steps.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (355 core; Arm64EmitEE unaffected);
headless BIOS boot reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished`
(IOP-heavy pad/SIO init) with native IOP integer codegen live — no crash/abort.

**Next step:** Phase 6.3 cont. — native IOP **load/store** generators (LB..SW + LWL/LWR/
SWL/SWR via `iopMemRead/Write8/16/32`), then **branches/jumps** so the rec stops breaking
the block at every control-flow op (the real IOP speedup).

---

## 2026-06-05 — Phase 6.1/6.2 IOP (R3000A) recompiler skeleton

**Goal:** Start Phase 6 — get an ARM64 IOP recompiler that boots, with all opcodes
interpreter-single-stepped (plumbing first, native codegen later). IOP was the biggest
remaining leverage (fully interpreter-bound until now).

**What changed:**
- **`pcsx2/arm64/aR3000A.h`** (new): RESTATEPTR(x19)=&psxRegs contract + psxRegs field
  offsets + `iopExecuteOneInst` decl.
- **`pcsx2/arm64/aR3000A.cpp`** (new): the `psxRec` provider. recLUT (2-level, cloned
  from aR5900.cpp, IOP memory map), `recReserve/recShutdown/recResetIOP/recResetRaw`,
  block compiler `recRecompile` (per-block prologue/epilogue, all-interp single-step for
  now), C++ dispatcher loop `recExecuteBlock` (timeslice + event test), targeted
  `recClearIOP`.
- **`pcsx2/R3000AInterpreter.cpp`**: added `iopExecuteOneInst()` (wraps the file-local
  `execI()`) — the rec's per-instruction interpreter fallback.
- **`pcsx2/CMakeLists.txt`**: registered `arm64/aR3000A.cpp`.
- **`pcsx2/VMManager.cpp`**: ARM64 branches now `psxRec.Reserve()/Shutdown()/Reset()` and
  `psxCpu = CHECK_IOPREC ? &psxRec : &psxInt`.
- Commit: (this commit).

**Decisions & rationale:**
- **C++ dispatcher loop, not the emitted recLUT dispatcher (yet).** The IOP's
  `ExecuteBlock(eeCycles)→s32` timeslice + return-value contract maps cleanly onto a C++
  `while (iopCycleEE > 0)` loop that mirrors `intExecuteBlock` line-for-line (HLE-BIOS
  entry, `iopAddEECycles` PS2 ×8 / PS1 gcd-carry, event test). Lower risk than porting the
  EE's fastjmp/emitted-dispatcher and easy to validate against the interpreter. This is
  the EE's own Phase 4.3 baseline; host-code block chaining (recLUT-style) is a later
  optimization once correctness is proven.
- **Blocks carry their own prologue/epilogue** (`stp x19,lr` / pin RESTATEPTR / `ldp` +
  RET) because they call helpers (interp/iopMem) that clobber LR, and x19 is callee-saved
  vs the C++ loop. No fastjmp needed — blocks RET to the loop.
- **All-interpreter single-step is intentional bring-up:** `recTranslateOp` returns false
  for everything, so each block is one `iopExecuteOneInst` (which charges its own cycle).
  Correct from day one; native generators only add speed. Mirrors how the EE rec started.
- **recLUT slot encoding:** block ptr / 0 (needs compile) / `IOP_UNMAPPED`(=1) sentinel.
  `recClearIOP` resets in-range mapped slots to 0 (targeted, like EE recClear).
- IOP is 1 cycle/op (the interpreter just does `cycle++`), so block_cycles = instr count
  — no per-opcode cycle table needed.

**Blockers / open questions:** Live boot/game confirmation pending (user to run). Headless
`-batch -bios` completed without crash but I couldn't confirm display progress from logs.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites). Headless boot ran w/o
crash/abort.

**Next step:** Phase 6.3 — native IOP integer generators (immediate/reg-reg/shift/move/
mult-div), 32-bit, wired into `recTranslateOp`. Then load/store + branches.

---

## 2026-06-05 — Phase 5.1 COP0 inline fallback (no block break)

**Goal:** Apply the Phase 5.3 COP2 inline-interpreter trick to COP0, the other common
`recTranslateOp`-returns-false family (TLB setup, interrupt-mask writes).

**What changed:**
- **`pcsx2/arm64/aR5900.cpp` `recTranslateOp`:** added `case 0x10` (COP0). Inlines
  `MFC0`/`MTC0` (Rd ∉ {9,25}) and the C0 TLB ops `TLBR/TLBWI/TLBWR/TLBP` via
  `recEmitInterpInline`; everything else returns false (single-step).
- Commit `ffe4f18d1`.

**Decisions & rationale:**
- **COP0 has a hazard COP2 didn't: cpuRegs.cycle staleness.** This rec only flushes
  `cpuRegs.cycle` at the block tail, but the Count register (Rd==9) and PERF counters
  (Rd==25) are computed from it. `COP0.cpp` (line ~40) explicitly warns that two
  `MFC0 Count` in one block before a cycle update return increment 0 → games lock up. So
  Count/PERF MFC0/MTC0 stay single-stepped (the single-step path charges block cycles
  first, then runs the op, so cycle is current). MFC0/MTC0 of any other CP0 reg is a plain
  load/store and inlines fine.
- **PC-writers stay single-stepped:** `BC0*` branches and `ERET` (C0 funct 0x18).
- **Interrupt-gating stays single-stepped:** `EI/DI/WAIT`. The x86 rec deliberately
  `recBranchCall`s EI (and delays DI by one instr) so a freshly-enabled interrupt is taken
  right away; inlining mid-block would defer that arbitrarily, so leave them on the proven
  path. `MTC0 Status/Config` ARE inlined — the x86 rec does *not* branch after them
  (`WriteCP0Status` just does `cpuSetNextEventDelta(4)`), so the interrupt is taken at the
  block-tail event test either way.
- **TLB writes inline safely:** `TLBWI/TLBWR`→`MapTLB`→`Cpu->Clear`→targeted `recClear`,
  which is safe mid-block (resets recLUT slots only; the running block keeps its valid host
  code and recompiles cleared slots on the next dispatch — same property the recLUT boot
  fix relied on).

**Blockers / open questions:** none. No new emit gtests (interpreter call, not a generator).

**Verified:** 273/273 ARM64 emit tests; `pcsx2-qt` builds arm64; BIOS boots with the COP0
inline path live (BIOS hammers COP0 — TLB map + Status writes); user-confirmed working.

**Next step:** IOP rec (Phase 6) for playable 2D — biggest remaining leverage; IOP is still
fully interpreter-single-stepped.

---

## 2026-06-05 — Phase 5.3 COP2/VU0-macro inline fallback (no block break)

**Goal:** Stop COP2 (VU0 macro) ops from fragmenting EE blocks. They were the highest-
frequency `recTranslateOp`-returns-false case in 3D geometry code.

**What changed:**
- **`pcsx2/arm64/aR5900.cpp` `recTranslateOp`:** added `case 0x12` (COP2) — for every
  COP2 op except the `BC2*` branches (rs==0x08) it emits an inline interpreter call
  (`recEmitInterpInline(op)`) and returns true, so the block continues instead of
  breaking + single-stepping via the dispatcher. Also inlined `LQC2`/`SQC2` (primary
  `0x36`/`0x3e`, new `OP_LQC2`/`OP_SQC2` constants). Forward-declared
  `recEmitInterpInline` (defined later among the block-compile helpers).
- Commit `0108025c9`.

**Decisions & rationale:**
- **Why inlining the interpreter is correct here (and not a hack):** the x86 rec wraps
  every COP2 op in `mVUFinishVU0`/`COP2_Interlock` sync because x86 runs VU0 on the
  *async microVU recompiler*. On ARM64 `CpuVU0 == &CpuIntVU0` (synchronous interpreter,
  VMManager.cpp:2742), so VU0 state is always current — there is nothing to sync. Running
  the interpreter's COP2 handler inline is byte-identical to the single-step path
  (`intExecuteOneInst` runs the same `info.interpret` fn); the only difference is we skip
  the block-break + dispatcher round-trip and let the block charge `info.cycles` in
  aggregate. Same pattern already used for delay-slot ops (`recEmitInterpInline`).
- **`BC2*` stay single-stepped** — they write `cpuRegs.pc` and aren't in `recEmitBranch`,
  so returning false lets `recRecompile` end the block before them (unchanged, proven).
- This is the Phase 5.3 "interpreter fallback initially" milestone; native microVU
  emission for COP2 is deferred to Phase 7 (it's the same emitter as VU1).

**Blockers / open questions:** none. No new emit gtests (the path is an interpreter call,
not a generator) — existing 273/273 stay green.

**Verified:** 273/273 ARM64 emit tests; `pcsx2-qt` builds arm64; BIOS headless boot clean;
**FFX boots and runs great (user-confirmed live).**

**Next step:** IOP rec (Phase 6) for playable 2D — biggest remaining leverage; IOP is
still fully interpreter-single-stepped. (Alt: COP0 inline via the same trick; or native
microVU = Phase 7.)

---

## 2026-06-05 — Phase 4.4 recLUT: game-compat smoke test PASSED (FFX, +40% FPS)

**Goal:** Validate the recLUT merge (commit `b7e83bc0b` on `armjit`) on a real game.

**What changed:**
- No code change. User ran the current `armjit` build against **Final Fantasy X**
  (3D, EE+VU-heavy) — it **boots into the game**, not just the BIOS.
- **FPS 10 → 14 (+40%)** vs the prior Phase 4.3 `s_blocks` C++-dispatcher model.

**Decisions & rationale:**
- recLUT block-linking is validated on a live title, not just the BIOS smoke path.
  The +40% is pure EE-dispatch headroom: VU and IOP are still on the interpreter, so
  the win comes entirely from killing the per-block `unordered_map` lookup +
  recompile-on-miss and chaining blocks in host code. Confirms Phase 4.4 was the right
  highest-leverage EE follow-up.

**Blockers / open questions:** none — recLUT is done & validated.

**Next step:** start IOP rec (Phase 6) for playable 2D — biggest remaining leverage now
that EE dispatch is fast and IOP is still single-stepped on the interpreter. (Alt: Phase
5.1 COP0 / 5.3 COP2-VU0 macro.)

---

## 2026-06-05 — Phase 4.4 recLUT RE-ATTEMPT: boots the BIOS (targeted recClear + cache headroom)

**Goal:** Re-attempt "4.4 Block linking + recLUT" (parked on `armjit-reclut-wip`), and
investigate the user's hypothesis that the Phase 4.5 invalidation work conflicts with it.

**What changed:**
- New branch `armjit-reclut-v2` off the current (booting, MMI-complete) `armjit`.
  Cherry-picked the recLUT commit's `aR5900.cpp` changes (7e3404cf4, code only; the
  hunks don't overlap the MMI decode fns, so it merged clean) — the dispatcher/execute
  model is unchanged from the parked attempt. Then fixed the two boot blockers:
- **`pcsx2/arm64/aR5900.cpp` `recClear` → targeted invalidation.** Was: defer + whole-
  cache `recResetRaw`. Now: reset only the recLUT slots covering `[addr,addr+size)` to
  `JITCompile` (guard: skip slots == `UnmappedRecLUTPage`; early-out if `!JITCompile`).
- **`recRecompile` cache-full guard → headroom.** `recPtr >= recPtrEnd` →
  `recPtr >= recPtrEnd - RECOMPILE_HEADROOM` (new 1 MB constant). Stops VIXL from
  realloc-growing the MAP_JIT buffer mid-block.
- Commit `37787a50b`. Trackers updated (PROGRESS 4.4 → [x] on the branch, 4.5 note).

**Decisions & rationale:**
- **The user was right: it was the 4.5 × 4.4 interaction.** Diagnosed by attaching to the
  hung process. `sample <pid>` showed 4271/4277 CPU-thread samples in `recResetRaw` under
  `MapTLB` under `intExecuteOneInst`: a BIOS TLB-write (interpreted) → `MapTLB` →
  `Cpu->Clear(addr<<12, 0x400)` *per mapped page*, and in the recLUT model `recResetRaw`
  rewrites the whole multi-million-entry recLUT every call. In the Phase 4.3 model the
  same `recClear` was a cheap `s_blocks.clear()`, so it never showed. Targeted per-range
  clear mirrors the x86 rec.
- **Second bug surfaced once the hang was gone:** `malloc: pointer being freed was not
  allocated` → `lldb -o 'b malloc_error_break' -o run -o bt` pointed at
  `vixl::CodeBuffer::Grow → realloc` under `armEmitLUI`/`recRecompile`. The cache-full
  check had no headroom for the in-flight block. The recLUT model recompiles far more
  (targeted invalidation thrashes blocks) so it reaches the tail during boot, unlike 4.3.
- **Why targeted recClear is safe mid-execution:** it runs synchronously on the EE thread
  (store fault / interpreted TLBWI); no concurrent block. The in-flight block keeps
  running its still-valid host code and re-dispatches through DispatcherReg, recompiling
  any cleared slot. No need for the old defer + fastjmp-exit dance on a clear.

**Blockers / open questions:**
- Targeted recClear misses blocks that *straddle* into the cleared range from an earlier
  start slot (only the start slot is keyed). Fine for BIOS; revisit for SMC-heavy games.
- `armjit-reclut-wip` (old parked branch) is now superseded by `armjit-reclut-v2`.

**Verified:** unittests green (Arm64EmitEE 270/270, 273 total); BIOS boots live to
`Mode Changed to DVD PAL` + `Pad: DS2 Config Finished`, stays alive, CPU-thread profile
healthy (`_cpuEventTest_Shared` idle-wait + `recRecompile` warmup, no reset thrash).

**Next step:** game-compat smoke test on `armjit-reclut-v2`; if clean, merge Phase 4.4
to `armjit`.

---

## 2026-06-05 — Phase 5.4 MMI second correctness pass: variable shifts + PMADDW voodoo

**Goal:** Re-review `aR5900MMI.cpp` against `MMI.cpp` for any remaining divergences
(the prior pass claimed "bit-exact", but the test oracles were only as good as
their author's reading of the interpreter).

**What changed:**
- **Emit (`pcsx2/arm64/aR5900MMI.cpp`):**
  - **PSLLVW/PSRLVW/PSRAVW were wrong.** They emitted four independent 32-bit lane
    shifts. The interpreter only shifts lanes 0 and 2 and sign-extends each 32-bit
    result to a full 64-bit doubleword (`Rd.SD[k] = (s64)(s32)(Rt.UL[2k] op (Rs.UL[2k]&0x1F))`).
    Rewrote all three to two `Sxtw`'d doubleword stores. (Low word of each dword was
    coincidentally right before; the high words were garbage shifts of Rt.UL[1]/[3].)
  - **PMADDW division voodoo missed the `(Rt&0x7FFFFFFF)==0` trigger.** The `Cbz`
    treated a zero result as "skip", but ==0 *and* ==0x7FFFFFFF both trigger the
    `+0x70000000`. Added a `voodoo_check_rs` label so ==0 falls through to the Rs!=Rt
    check instead of skipping.
- **Tests (`tests/ctest/core/arm64_emit_test.cpp`):**
  - Fixed the `refPSLLVW/PSRLVW/PSRAVW` oracles — they replicated the *buggy*
    4-lane model, so the gtests were green against wrong code. Now mirror MMI.cpp.
  - Added `MMI_PMADDW_VoodooZeroRt` (Rt.UL[0] ∈ {0, 0x80000000}, Rs≠Rt) — the shared
    inA/inB inputs never make `Rt&0x7FFFFFFF==0`, so the ==0 path had zero coverage.

**Decisions & rationale:**
- Lesson reinforced: a passing gtest only proves the emitter matches its *oracle*.
  When an oracle is hand-derived from the interpreter, re-derive it independently
  before trusting "bit-exact". Both bugs hid behind self-consistent-but-wrong tests.

**Blockers / open questions:** none.

**Verified:** `unittests` 100% (Arm64EmitEE 270/270, +1 voodoo test); `pcsx2-qt`
builds arm64. Live game verification still pending.

**Next step:** Phase 4.4 recLUT (parked on `armjit-reclut-wip` until BIOS stall
solved), or game compatibility testing.

---

## 2026-06-05 — Phase 5.4 MMI correctness pass: decode rewrite + emit fixes + tests

**Goal:** Review the 5 unpushed MMI commits + the uncommitted misc-ops batch for
correctness, fix everything, and add the missing tests.

**What changed:**
- **Decode (`pcsx2/arm64/aR5900.cpp`):** `recTranslateMMI0/1/2/3` + the MMI funct
  dispatch had many wrong sub-op indices (the emit gtests passed only because they
  call the generators directly, bypassing decode; BIOS booted because it doesn't hit
  these exact ops). Rewrote all four tables + the funct switch to mirror
  `R5900OpcodeTables.cpp tbl_MMI*` exactly. PLZCW→funct 0x04, PMFHL→0x30, PMTHL→0x31,
  PSLL/SRL/SRA H,W → 0x34/36/37/3C/3E/3F; PMFHI/PMFLO moved to MMI2 0x08/0x09;
  PMTHI/PMTLO→MMI3 0x08/0x09; PMULTW→MMI2 0x0C, PMULTH→0x1C, PMADDH/PMSUBH→0x10/0x14,
  PHMADH/PHMSBH→0x11/0x15, PEXEH/PREVH/PEXEW→0x1A/1B/1E; PEXCW→MMI3 0x1E;
  PADSBH→MMI1 0x04; PEXT5/PPAC5→MMI0 0x1E/0x1F.
- **Emit (`pcsx2/arm64/aR5900MMI.cpp`):**
  - PMADDW/PMSUBW: divided by −1 (`Sxtw` of 0xFFFFFFFF) → now a positive 64-bit
    0xFFFFFFFF divisor; PMADDW no longer folds the voodoo `0x70000000` into the LO
    word; PMSUBW no longer clobbers the divided temp2 in x10 before storing HI.
  - PMADDUW: form the full 64-bit accumulator first so the low-word carry reaches HI.
  - PMULTW/PMULTUW: sign-extend LO/HI into the full 64-bit doubleword (were 32-bit).
  - PMULTH: rewritten as an 8-lane halfword multiply (was a 2-lane word multiply).
  - PHMADH/PHMSBH: overwrite LO/HI (no accumulation) and write the odd lanes
    (firsttemp / ~firsttemp) via a shared `emitPHMPair` helper.
  - PLZCW: dropped the bogus `-1` (ARM64 `Cls` already == CountLeadingSignBits−1).
  - PMFHL: fixed LH lane order, implemented SLW (signed clamp) and SH (16-bit
    saturate), returns bool (false → interpreter for unknown variants).
  - PMTHL: 32-bit stores so the odd LO/HI words are preserved.
  - QFSRV: removed (its shift amount is the runtime SA register `cpuRegs.sa`) — stays
    on the interpreter.
- **Tests (`tests/ctest/core/arm64_emit_test.cpp`):** the committed file did not even
  compile (`get128` undefined; `refPMULTH` used the ambiguous `s16`). Added `get128`,
  fixed `refPMULTH`, and added interpreter-accurate refs + tests for PMADDW, PMADDUW,
  PMSUBW (nonzero HI/LO seeds), PMULTH, PMADDH, PMSUBH, PHMADH, PHMSBH, PLZCW, PADSBH,
  PEXT5, PPAC5, PMFHL (all 5 variants), PMTHL. Arm64EmitEE 252 → 269.
- Commits: pending (working tree; see "Next step").

**Decisions & rationale:**
- **Tables are the single source of truth.** The interpreter *and* the x86 recompiler
  both dispatch through `R5900OpcodeTables.cpp`, so the ARM64 hand-rolled decode must
  mirror those `tbl_MMI*` indices verbatim. The case labels now do.
- **QFSRV → interpreter.** Its amount is `cpuRegs.sa` (set by MTSAB/MTSAH at runtime),
  not the instruction's sa field, and the op is rare — interpreter fallback is correct
  and simplest (CLAUDE.md rule 4).
- **Tests seed nonzero HI/LO** (`runMMIMACAcc`) so the accumulating MAC variants are
  actually exercised; the old `runMMIMAC` zeroed them.

**Blockers / open questions:**
- none. `pcsx2-qt` builds arm64; unittests 100% (Arm64EmitEE 269/269, core 354/354).
  Live game verification still pending.

**Next step:** decide commit strategy for the 5 unpushed MMI commits (fixup vs. new
fix commit), then Phase 4.4 recLUT (parked) or game-compat testing.

## 2026-06-04 — Phase 5.4 MMI COMPLETE: remaining misc ops (PLZCW/PADSBH/QFSRV/PEXT5/PPAC5/PMFHL/PMTHL)

**Goal:** Implement the remaining MMI misc ops to complete Phase 5.4.

**What changed:**
- Extended `pcsx2/arm64/aR5900MMI.cpp` with 7 new generators:
  - `armEmitPLZCW`: count leading sign bits per 32-bit lane using ARM64 `Cls` - 1.
  - `armEmitPADSBH`: subtract low 4 halfwords, add high 4 (no saturation, just truncate).
  - `armEmitQFSRV`: quad byte shift right by sa<<3 bytes (cross-lane shift, sa<64).
  - `armEmitPEXT5`: expand four 5-bit fields to 8-bit fields per 32-bit lane.
  - `armEmitPPAC5`: compress four 8-bit fields to 5-bit fields per lane.
  - `armEmitPMFHL`: move from HI/LO (LW/UW/LH variants implemented; SLW/SH interpret).
  - `armEmitPMTHL`: move to HI/LO (sa=0 only).
- Declarations added to `pcsx2/arm64/aR5900.h`.
- Dispatch wired in `pcsx2/arm64/aR5900.cpp`:
  - `recTranslateMMI0`: sa=0x1C→PEXT5, 0x1D→PPAC5.
  - `recTranslateMMI1`: sa=0x1C→PADSBH, 0x1D→QFSRV.
  - `recTranslateMMI2`: sa=0x1B→PLZCW.
  - `recTranslateMMI3`: sa=0x16→PMFHL, 0x17→PMTHL.
- Trackers: PROGRESS.md flipped Phase 5.4 to COMPLETE.
- Commits: pending.

**Decisions & rationale:**
- **PLZCW uses ARM64 `Cls` (Count Leading Sign bits):** This instruction does exactly
  what the EE needs — count leading sign bits. We subtract 1 to match the interpreter.
- **PADSBH has no saturation:** Despite the "S" in the name, the interpreter (`MMI.cpp`)
  just adds/subtracts and truncates to 16 bits — no saturation. Implemented with scalar
  `Smov`/`Add/Sub`/`Ins` sequence.
- **QFSRV falls back for sa>=64:** The cross-lane 128-bit shift by sa>=64 bytes is
  complex and rare; we implement the common case (sa<64) and fall back to interpreter
  for the rest.
- **PEXT5/PPAC5 process lane-by-lane:** These 5-bit field expand/compress ops don't
  map to single NEON instructions, so we do scalar bit manipulation per lane.
- **PMFHL SLW/SH variants interpret:** The sign-clamp (SLW) and signed-saturate (SH)
  variants have complex logic; implemented LW/UW/LH which cover most cases, SLW/SH
  fall back to interpreter.

**Blockers / open questions:**
- none. Unit tests pass (334/334 core). Phase 5.4 MMI is now complete — all MMI ops
  compile natively.

**Next step:** Phase 4.4 recLUT (debug BIOS stall on `armjit-reclut-wip` branch), or
game compatibility testing to verify the full MMI implementation.

## 2026-06-04 — Phase 5.4 MMI multiply-accumulate family complete (PMULTW/PMADDW/PMULTH/PMFHI/etc.)

**Goal:** Implement the MMI multiply-accumulate family that writes to HI/LO
registers, plus the full 128-bit HI/LO moves, so they stop single-stepping
the interpreter.

**What changed:**
- Extended `pcsx2/arm64/aR5900MMI.cpp` with 13 new generators (~500 lines):
  - **Word multiply-accumulate (lanes 0 and 2):**
    - `armEmitPMULTW`: 32×32→64 signed multiply, HI/LO write, optional GPR[rd].
    - `armEmitPMULTUW`: unsigned variant (sign-extends results per interpreter).
    - `armEmitPMADDW`: Rs*Rt + (HI<<32) with EE division voodoo (÷4294967295,
      lane 0 only has the 0x70000000 fixup quirk).
    - `armEmitPMADDUW`: unsigned variant (no voodoo).
    - `armEmitPMSUBW`: (HI<<32) - Rs*Rt with division fixup.
  - **Halfword multiply-accumulate (8 lanes, alternating LO/HI):**
    - `armEmitPMULTH`: 16×16→32 signed multiply, 4 lanes × 2 iterations.
    - `armEmitPMADDH`: accumulate 8 halfword products to LO/HI.
    - `armEmitPMSUBH`: subtract 8 halfword products from LO/HI.
    - `armEmitPHMADH`: paired multiply-add (Rs[n]*Rt[n] + Rs[n+1]*Rt[n+1]).
    - `armEmitPHMSBH`: paired multiply-subtract.
  - **HI/LO moves (full 128-bit via NEON):**
    - `armEmitPMFHI/PMFLO`: `Ldr q-reg` from HI/LO, `Str q-reg` to GPR[rd].
    - `armEmitPMTHI/PMTLO`: `Ldr q-reg` from GPR[rs], `Str q-reg` to HI/LO.
- Declarations added to `pcsx2/arm64/aR5900.h`.
- Dispatch wired in `pcsx2/arm64/aR5900.cpp`:
  - `recTranslateMMI2`: sa=0x00→PMADDW, 0x01→PMADDH, 0x04→PMSUBW, 0x05→PMSUBH,
    0x08→PMULTW, 0x09→PMULTH, 0x0C→PHMADH, 0x0D→PHMSBH.
  - `recTranslateMMI3`: sa=0x00→PMADDUW, 0x08→PMULTUW, 0x10→PMFHI, 0x11→PMFLO,
    0x14→PMTHI, 0x15→PMTLO.
- Trackers: PROGRESS.md CURRENT FOCUS + Phase 5.4 checkboxes updated.
- Commits: pending.

**Decisions & rationale:**
- **Scalar GPR ops for word multiply-accumulate:** ARM64 has `Smull/Umull` for
  32×32→64, which we use directly. Results are sign-extended to 64 bits per the
  interpreter's semantics (even for unsigned ops).
- **EE division voodoo replicated exactly:** PMADDW has a quirk where lane 0
  adds 0x70000000 under specific conditions ((Rt[0]&0x7FFFFFFF)==0 or ==0x7FFFFFFF,
  and Rs[0]!=Rt[0]), then divides by 4294967295 (not >>32). This matches the
  interpreter's `MMI.cpp:_PMADDW` exactly.
- **NEON q-register loads/stores for 128-bit HI/LO moves:** PMFHI/PMFLO/PMTHI/PMTLO
  move the full 128-bit HI/LO registers. Using `Ldr/Str q-reg` is the most efficient
  approach (single instruction per access) and matches how the existing NEON ops
  access GPRs.
- **Halfword ops use `Smov` + scalar multiply:** For the 8-lane halfword ops, we
  use `Smov` to extract individual halfword lanes, then scalar `Smull/Smaddl` for
  the multiply-accumulate. This is not the most efficient (could use NEON `Smull`
  on multiple lanes at once), but it's correct and matches the interpreter's
  element-by-element semantics. Can be optimized later if profiling shows hotspots.
- **VIXL conditional branches use `B(&label, cond)`:** The EE division voodoo
  check needs actual conditional branches (not `Csel`). VIXL's `B` method takes
  the label first, condition second.

**Blockers / open questions:**
- none. Unit tests pass (334/334 core, 252/252 Arm64EmitEE). Live game
  verification pending (BIOS boot test recommended).

**Next step:** remaining MMI misc ops (`PADSBH/QFSRV/PEXT5/PPAC5/PLZCW/PMFHL/PMTHL`),
or Phase 4.4 recLUT (parked on `armjit-reclut-wip` until BIOS stall solved).

---

## 2026-06-04 — Phase 5.4 MMI variable shifts complete (PSLLVW/PSRLVW/PSRAVW)

**Goal:** Implement the three MMI variable shift ops (`PSLLVW/PSRLVW/PSRAVW`) where
the shift amount comes from GPR[rs] (masked to 5 bits per lane), so they stop
single-stepping the interpreter.

**What changed:**
- Extended `pcsx2/arm64/aR5900MMI.cpp` with 3 new generators:
  - `armEmitPSLLVW`: parallel logical left shift (`Lsl` per lane)
  - `armEmitPSRLVW`: parallel logical right shift (`Lsr` per lane)
  - `armEmitPSRAVW`: parallel arithmetic right shift (`Asr` per lane)
  ARM64 NEON lacks a direct "shift by unsigned vector amount" instruction for
  32-bit lanes, so we use scalar GPR shifts and pack 2 lanes per 64-bit store.
- Declarations added to `pcsx2/arm64/aR5900.h`.
- Dispatch wired in `pcsx2/arm64/aR5900.cpp`:
  - `recTranslateMMI2`: sa=0x02 → PSLLVW, sa=0x03 → PSRLVW
  - `recTranslateMMI3`: sa=0x03 → PSRAVW
- 3 new `Arm64EmitEE.MMI_{PSLLVW,PSRLVW,PSRAVW}` gtests in
  `tests/ctest/core/arm64_emit_test.cpp`: reference functions mirror
  `pcsx2/MMI.cpp` element indexing; both operand orders tested.
- Trackers: PROGRESS.md CURRENT FOCUS + Phase 5.4 checkboxes updated.
- Commits: pending.

**Decisions & rationale:**
- Scalar GPR shifts instead of NEON: ARM64 NEON's `Sshl`/`Ushl` instructions
  expect shift amounts encoded as signed values (positive=left, negative=right)
  in a format incompatible with direct use. Scalar `Lsl`/`Lsr`/`Asr` with
  variable register amounts is simpler and correct.
- Pack 2 lanes per 64-bit store using `Bfi` (bit field insert): the GPR stores
  4 x 32-bit lanes in 128 bits as two 64-bit halves (SD[0]={UL[0],UL[1]},
  SD[1]={UL[2],UL[3]}). After shifting two lanes, `Bfi dst, src, 32, 32` inserts
  the second 32-bit result into the upper half of the first, then we store once.
- Uses caller-saved GPRs x9-x12 as scratch (x16 is VIXL's internal scratch,
  x19-x21 are persistent RESTATEPTR/REFASTMEMBASE/REVTLBPTR).

**Blockers / open questions:**
- none. Unit coverage complete (3/3 tests pass, 334/334 core tests total).

**Next step:** continue Phase 5.4 — multiply-accumulate family
(`PMADDH/PHMADH/PMSUBH/PMULTH/PMADDW/PMSUBW/PMULTW/PMADDUW/PMULTUW` → HI/LO),
or the `PMFHI/PMFLO/PMTHI/PMTLO` HI/LO moves. Phase 4.4 recLUT remains parked
on `armjit-reclut-wip`.

## 2026-06-04 — Phase 5.4 MMI: remaining lane permutes complete (PROT3W/PEXCH/PEXCW)

**Goal:** Finish Phase 5.4 lane permutes by implementing the remaining three ops
(`PROT3W`/`PEXCH`/`PEXCW`) so all MMI lane permutation ops stop single-stepping
the interpreter.

**What changed:**
- Extended `pcsx2/arm64/aR5900MMI.cpp` with 3 new generators:
  - `armEmitPROT3W`: rotate 3 words — `[Rt[1], Rt[2], Rt[0], Rt[3]]` (32-bit lanes)
  - `armEmitPEXCH`: extract even halfwords — swap halfword pairs 1↔2 within each 64-bit half
  - `armEmitPEXCW`: extract even words — swap word pairs 1↔2
  All use `Mov` + `Ins` (lane insert) sequence; no single NEON instruction covers these.
- Declarations added to `pcsx2/arm64/aR5900.h`.
- Dispatch wired in `pcsx2/arm64/aR5900.cpp`:
  - `recTranslateMMI2`: sa=0x1F → `PROT3W`
  - `recTranslateMMI3`: sa=0x1A → `PEXCH`, sa=0x1C → `PEXCW`
- 3 new `Arm64EmitEE.MMI_{PROT3W,PEXCH,PEXCW}` gtests in
  `tests/ctest/core/arm64_emit_test.cpp`: reference functions mirror `pcsx2/MMI.cpp`
  element indexing; both operand orders tested.
- Trackers: PROGRESS.md CURRENT FOCUS + Phase 5.4 checkboxes updated.
- Commits: pending.

**Decisions & rationale:**
- Same mem-to-mem pattern as prior permutes: load GPR[rt] into scratch q-reg (q31),
  compute into q29 using `Mov` + `Ins` lane insert sequence, store to GPR[rd].
  No register allocator needed.
- `PROT3W` is Rt-only (no Rs operand) — matches the interpreter which only reads Rt.
  The rotation `[Rt[0],Rt[1],Rt[2],Rt[3]] → [Rt[1],Rt[2],Rt[0],Rt[3]]` leaves lane 3
  unchanged and rotates lanes 0-2.
- `PEXCH`/`PEXCW` are also Rt-only — they extract even-indexed elements within each
  64-bit half, which for these ops means swapping element pairs 1↔2.
- $zero destination discard: all generators return early if `rd==0`, matching the
  interpreter's `if (!_Rd_) return`.

**Blockers / open questions:**
- none. Unit coverage complete (3/3 tests pass).

**Next step:** continue Phase 5.4 — variable shifts (`PSLLVW/PSRLVW/PSRAVW` — shift
amount from GPR[rs].UL[0] & 0x1F, masked per lane), or the multiply-accumulate family
(`PMADD*/PMSUB*/PMULT*` → HI/LO). Phase 4.4 recLUT remains parked on `armjit-reclut-wip`.

---

## 2026-06-04 — Phase 5.4 MMI: simple lane permutes complete

**Goal:** Extend Phase 5.4 with five lane-permute ops (`PINTH/PINTEH/PEXEH/PEXEW/PREVH`)
so they stop single-stepping the interpreter.

**What changed:**
- New `pcsx2/arm64/aR5900MMI.cpp` generators (5 ops):
  - `armEmitPINTH`: interleave low half of Rt with high half of Rs (halfwords)
  - `armEmitPINTEH`: interleave even-indexed halfwords of Rt and Rs
  - `armEmitPEXEH`: extract even halfwords (swap 0↔2 within each 64-bit half)
  - `armEmitPEXEW`: extract even words (swap 32-bit lanes 0↔2)
  - `armEmitPREVH`: reverse halfwords within each 64-bit half (`Rev64`)
- Declarations in `pcsx2/arm64/aR5900.h`.
- Dispatch wired in `pcsx2/arm64/aR5900.cpp`:
  - `recTranslateMMI2`: sa=0x0A→PINTH, 0x16→PEXEH, 0x17→PREVH, 0x18→PEXEW
  - `recTranslateMMI3`: sa=0x0A→PINTEH
- 5 new `Arm64EmitEE.MMI_{PINTH,PINTEH,PEXEH,PEXEW,PREVH}` gtests in
  `tests/ctest/core/arm64_emit_test.cpp`: reference functions mirror
  `pcsx2/MMI.cpp` element indexing; both operand orders tested.
- Trackers: PROGRESS.md CURRENT FOCUS + Phase 5.4 checkboxes updated.
- Commit: pending.

**Decisions & rationale:**
- Used `Ins` (lane insert) for PINTH/PINTEH/PEXEH/PEXEW: these ops don't map to a
  single NEON permutation instruction. The `Ins` sequence is correct and clear,
  though not optimal (8 instructions for PINTH/PINTEH, 5 for PEXEH/PEXEW). Can be
  optimized later with `Trn`/`Uzp`/`Rev` sequences if profiling shows hotspots.
- `PREVH` uses single `Rev64(V8H)`: this is the exact semantic match (reverse 16-bit
  lanes within each 64-bit half), so it's the most efficient of the bunch.
- Same mem-to-mem pattern as existing MMI ops: load both sources into scratch q-regs
  (q30/q31), compute into q29, store back. No register allocator needed.
- $zero destination discard: all generators return early if `rd==0`, matching the
  interpreter's `if (!_Rd_) return`.

**Blockers / open questions:**
- none. Unit coverage complete (5/5 tests pass, 328/328 core tests total).

**Next step:** continue Phase 5.4 — either (a) variable shifts
(`PSLLVW/PSRLVW/PSRAVW` — shift amount from GPR[rs].UL[0] & 0x1F, masked), or (b) the
remaining permutes (`PROT3W` = rotate 3 words, `PEXCH/PEXCW` = extract even halfwords/
words). Phase 4.4 recLUT remains parked on `armjit-reclut-wip`.

## 2026-06-04 — Phase 5.4 MMI: first NEON-mapped SIMD batch

**Goal:** On the booting Phase 4.3 baseline, start Phase 5.4 (MMI 128-bit int
SIMD → NEON). Compile the MMI ops that map cleanly to single NEON instructions so
they stop single-stepping the interpreter.

**What changed:**
- New `pcsx2/arm64/aR5900MMI.cpp` (+ declarations in `aR5900.h`, registered in
  `pcsx2/CMakeLists.txt`). 46 generators, each: `Ldr` GPR[rs]/GPR[rt] into scratch
  q-regs (q30/q31), one NEON op into q29, `Str` to GPR[rd]; `rd==0` discards.
  Mapping (all verified against pcsx2/MMI.cpp): `PADD*/PSUB*`→`Add/Sub`,
  `PADDS*/PSUBS*`→`Sqadd/Sqsub`, `PADDU*/PSUBU*`→`Uqadd/Uqsub`, `PCGT*`→`Cmgt`,
  `PCEQ*`→`Cmeq`, `PMAX*/PMIN*`→`Smax/Smin`, `PABSW/PABSH`→`Sqabs`,
  `PAND/POR/PXOR`→`And/Orr/Eor`, `PNOR`→`Orr`+`Not`, `PEXTL*/PEXTU*`→`Zip1/Zip2`,
  `PPAC*`→`Uzp1`, `PCPYLD`→`Zip1.2D`, `PCPYUD`→`Zip2.2D`, `PCPYH`→two `Dup`+`Ins`.
- Dispatch: `recTranslateOp` case 0x1C now decodes the MMI0/1/2/3 sub-groups
  (sub-op = `sa`, bits 10:6) via 4 helper tables and calls the generators.
- 47 new `Arm64EmitEE.MMI_*` gtests: a C++ MQ-union replica of MMI.cpp is the
  oracle; each op is checked byte-exact over two edge-case vectors (both operand
  orders), plus an `rd==0` discard test.
- Commit `6b2ceb311` (code+tests). This entry + PROGRESS in a follow-up doc commit.

**Decisions & rationale:**
- Operand order matters: the pack/interleave/PCPYLD ops feed `(rt, rs)` into the
  NEON op because the interpreter takes Rt as the low/even source. PCPYUD keeps
  `(rs, rt)` (Zip2.2D = {Rs.hi, Rt.hi}). Encoded as two macros (`MMI_3OP` /
  `MMI_3OP_TS`) so the order is explicit and unit-checked both ways.
- Guest GPRs are little-endian in cpuRegs, so a 128-bit NEON load puts guest lane
  0 in NEON lane 0 — the interpreter's element indexing maps 1:1, no shuffles.
- Scoped the test oracle's lane types to `int8_t/int16_t/...`: the file's
  `using namespace vixl::aarch64;` defines register objects `s8`/`s16` that shadow
  the pcsx2 aliases and made bare `(s16)` casts ambiguous.
- Deferred the non-NEON-trivial MMI ops (multiply-accumulate to HI/LO, parallel
  shifts, lane permutes, PADSBH/QFSRV/PEXT5/PPAC5/PLZCW/PMFHL/PMTHL) to keep this
  commit atomic and behaviour-neutral; they remain interpreter fallback.

**Blockers / open questions:**
- none. Live game verification still pending (unit coverage is complete).

**Verification:**
- `cmake --build build --target pcsx2-qt -j18` ok, binary `arm64`.
- `Arm64EmitEE.*` 220/220 (was 173, +47); full `core_test` 305/305 (was 258).

**Next step:** continue Phase 5.4 — parallel shifts (`PSLLH/PSRLH/PSRAH/PSLLW/
PSRLW/PSRAW`, immediate `sa` → NEON `Shl/Ushr/Sshr`) and the simple lane permutes
(`PINTH/PINTEH/PREVH/PEXEH/PEXEW` via `Zip/Trn/Rev`). Phase 4.4 recLUT stays
parked on `armjit-reclut-wip`.

---

## 2026-06-04 — Phase 4.4 recLUT root-caused (x19) but BIOS-stall; reverted to Phase 4.3

**Goal:** Resume the previous session's Phase 4.4 Commit A (recLUT execution-model
rewrite), which was written into the working tree but crashed the BIOS at ~0.44s.
Find and fix the crash, get the BIOS booting.

**What changed:**
- Diagnosed the crash to root cause with progressively targeted instrumentation
  (dispatch trace → per-dispatch x19 capture → before/after captures around the
  external calls). **Root cause: `RESTATEPTR = x19 = &cpuRegs` is pinned once in
  EnterRecompiledCode and assumed permanent, but `_cpuEventTest_Shared` (reached via
  DispatcherEvent→recEventTest; services DMA/VIF and runs other ARM64 JIT) clobbers x19
  and returns. The dispatcher then reads `cpuRegs.pc` via the garbage x19 → unmapped
  recLUT page → `recExitUnmapped`.** Proven directly: `eventEntryX19=&cpuRegs` /
  `eventExitX19=garbage` straddling the `recEventTest` call.
- Fixed on the wip branch by re-pinning `RESTATEPTR=&cpuRegs` at the top of
  DispatcherReg (every block/event/compile path funnels through it). Crash gone,
  stable >25s, unit tests green (core 258/258, Arm64EmitEE 173/173).
- **But the BIOS still does not reach display:** EE bursts to ~365k cycles then stalls
  in a wait loop (~0x9fc42b08) while events keep firing; VPS stays 0. Separate Commit-A
  regression vs the Phase 4.3 model, not yet root-caused.
- **Decision (user-directed): revert `armjit` to the booting Phase 4.3 model**
  (`s_blocks` + C++ dispatcher loop) and preserve the recLUT attempt + x19 fix on branch
  `armjit-reclut-wip` (commit `7e3404cf4`). Verified the restored model boots:
  `Mode Changed to DVD PAL`, `GS CRTC DVD PAL 720x480`, `Pad: DS2 Config Finished`,
  no crash, 100% CPU.
- Commits: docs only on `armjit` (this entry + PROGRESS); recLUT code on the wip branch.

**Decisions & rationale:**
- Reverted instead of pushing through the stall: Phase 4.3 already boots and is the
  safer baseline for continued EE work (Phase 5.4 MMI etc.); the recLUT optimisation can
  be resumed later from the wip branch now that its crash is understood.
- The x19 lesson generalises: **no host register survives an external C++/JIT call in
  this codebase** (the VIF/DMA dynarec treats x19-x28 as free scratch). Any future
  pinned-register design must reload at the dispatch funnel or after each external call,
  not pin-once.

**Blockers / open questions:**
- recLUT BIOS-stall (~0x9fc42b08) unsolved — likely EE↔IOP sync or a miscompiled op
  surfaced by the new dispatch/cycle-tail model. Owned by `armjit-reclut-wip`.

**Next step:** On `armjit` (booting baseline), start **Phase 5.4 MMI** (128-bit int
SIMD → NEON, ref `x86/iMMI.cpp`). Resume Phase 4.4 only on `armjit-reclut-wip`, starting
from the stall (do NOT re-derive the x19 crash — it's fixed there).

---

## 2026-06-04 — Phase 5.2b COMPLETE: MADD/MSUB, MAX/MIN, compares, CVT, BC1

**Goal:** Finish the EE FPU single-precision suite — multiply-accumulate, min/max,
the C.* compares, CVT.W/CVT.S conversions, and the BC1F/BC1T branches — so COP1 no
longer single-steps the interpreter for the common float path.

**What changed (3 commits, all mirroring pcsx2/FPU.cpp, all with gtests vs a C++ replica):**
- `19b148eec` MADD/MSUB(/A)_S + MAX_S/MIN_S. Refactored `emitLoadFpuDouble` to expose
  `emitClampFpuDoubleBits` (clamp a value already in a reg) for the MADD/MSUB product
  re-clamp. Reproduced the interpreter asymmetry exactly: the fd-form (MADD/MSUB)
  re-clamps both the accumulator and the product via `fpuDouble`, while the ACC-form
  (MADDA/MSUBA) uses the *raw* stored ACC float and an *unclamped* product. MAX/MIN are
  integer-domain `fp_max`/`fp_min` (both-negative via sign bit of `fs&ft`), clearing O|U.
- `561ab6da8` C.F/C.EQ/C.LT/C.LE + CVT_W/CVT_S. Compares set/clear the FCR31 C bit from
  a `fpuDouble`-clamped `Fcmp` (operands finite post-clamp, so ARM float conds match the
  C++ comparison); C.F always clears. CVT_W is float→s32 with EE saturation (exp-field
  range check, `Fcvtzs` round-to-zero in range, else sign-based saturation); CVT_S is
  `Scvtf` (lives in COP1_W, rs==0x14, funct 0x20 — added that dispatcher arm).
- `dbcc51083` BC1F/BC1T. New generators in `aR5900Branch.cpp` test the FCR31 C bit via
  `Tst` + the existing `emitSelectPc`. Wired into `recEmitBranch` (opcode 0x11, rs==0x08
  BC, rt 0x00/0x01) and `recIsHandledBranch` so the block compiler terminates on them
  and compiles the delay slot. Likely BC1FL/BC1TL stay on interpreter fallback.
- Trackers: PROGRESS.md 5.2/5.2b flipped to `[x]`, CURRENT FOCUS moved to the next EE
  follow-up (Phase 4.4 block linking + recLUT, or Phase 5.4 MMI).

**Decisions & rationale:**
- Same ground-truth-mirroring policy as parts 1–2 (match the interpreter, not iFPUd) —
  every quirk (MADDA raw-ACC asymmetry, CVT_W exp-field saturation, C.* on clamped
  operands) was replicated bit-for-bit and asserted, so moving ops from interp→JIT is
  behaviour-neutral.
- BC1F/BC1T fit the existing branch machinery cleanly; only `recIsHandledBranch` needed
  `rs`-awareness so COP1 *arithmetic* (also opcode 0x11) is not mistaken for a branch.

**Blockers / open questions:**
- none. (Live game verification still pending the duplicate-Qt-bundle workaround noted
  in the RAM-invalidation entry; unit coverage is complete.)

**Verification:**
- `cmake --build build --target pcsx2-qt -j18` succeeded; binary still `arm64`.
- `Arm64EmitEE.*` 173/173; full `core_test` 258/258.

**Next step:** Phase 4.4 — block linking + recLUT (replace the bring-up `s_blocks`
unordered_map + recompile-on-miss), now measurable against a rec that no longer
single-steps the FPU. Alternative: Phase 5.4 MMI (128-bit int SIMD → NEON).

---

## 2026-06-04 — Phase 5.2b (part 2): FPU DIV_S/SQRT_S/RSQRT_S

**Goal:** Land the divide/sqrt family of EE FPU float arithmetic natively (with the
EE's `checkDivideByZero` / signed-zero / negative-input quirks), reusing the part-1
clamp helpers, so they stop single-stepping the interpreter. The code had been
written but left uncommitted from the prior session; this session verified, committed,
and journaled it.

**What changed:**
- `pcsx2/arm64/aR5900FPU.cpp` — `armEmitDIV_S/SQRT_S/RSQRT_S`:
  - DIV_S: tests divisor exponent (denormals count as zero). Zero path RMWs FCR31
    with D|SD (x/0) or I|SI (0/0) and writes `sign(divisor^dividend) | +fmax`. Normal
    path is `fpuDouble`-clamped `Fdiv` + `emitStoreClampedResult(setFlags=false)`.
  - SQRT_S: +/-0/denormal → signed zero with I/D causes cleared; negative input sets
    I|SI then `Fsqrt(Fabs(...))`; clamped store with no flags.
  - RSQRT_S: zero ft → D|SD + `sign(ft)|+fmax`; negative ft → I|SI; else
    `fpuDouble(fs) / Fsqrt(Fabs(fpuDouble(ft)))`, clamped.
  New FCR31 constants FPUflag I/D/SI/SD.
- `pcsx2/arm64/aR5900.h` — three generator decls + comment.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` COP1_S funct switch: 0x03 DIV_S,
  0x04 SQRT_S (ft=rt), 0x16 RSQRT_S.
- `tests/ctest/core/arm64_emit_test.cpp` — `fpuref` replica gains `bits/div/sqrt/
  rsqrt` + `checkDivideByZero`; 10 new `Arm64EmitEE.*` gtests (basic, x/0, 0/0,
  sqrt pos/neg/negzero, rsqrt pos/zero/neg).
- `arm64-port/PROGRESS.md` — CURRENT FOCUS + Phase 5.2b checkbox updated.
- Commit: `fd8342b42 ARM64: EE FPU DIV_S/SQRT_S/RSQRT_S (Phase 5.2b part 2)`

**Decisions & rationale:**
- Same ground-truth-mirroring rationale as part 1: match the interpreter (FPU.cpp),
  not iFPUd. `checkDivideByZero` semantics (denormal-as-zero, sign-xor result) and the
  RSQRT zero path's sign-from-ft were replicated bit-for-bit and asserted against the
  C++ replica, so moving these ops from interp to JIT is behaviour-neutral.
- DIV's normal path intentionally does **not** clear stale I/D causes (matches the
  interpreter, which only sets, never clears, on the non-zero path); SQRT/RSQRT *do*
  clear I/D up front because their interpreter paths recompute those causes.

**Blockers / open questions:**
- none

**Verification:**
- `cmake --build build --target pcsx2-qt -j18` succeeded; binary still `arm64`.
- `unittests` target green; `core_test --gtest_filter=Arm64EmitEE.*`: 149/149 passed.

**Next step:** Phase 5.2b continued — `MADD/MSUB(/A)_S` (temp = fs*ft then ACC±temp),
`MAX_S/MIN_S` (integer fp_max/fp_min, no clamp, clear O|U), then the `C.F/C.EQ/C.LT/
C.LE` compares + `BC1F/BC1T(L)` branches, then `CVT.W/CVT.S`. Helpers
`emitLoadFpuDouble` / `emitStoreClampedResult` remain reusable.

---

## 2026-06-04 — Bring-up fix: mark ARM64 RAM blocks for invalidation

**Goal:** Investigate why the BIOS boots but Final Fantasy X stays black after the
BIOS handoff, where slow execution would be expected instead of no visible progress.

**What changed:**
- `pcsx2/arm64/aR5900.cpp` — added `recProtectCompiledRange(startpc, endpc)` and call
  it after successful block emission. Any compiled block whose source range is in EE
  RAM now calls `mmap_MarkCountedRamPage()` for the covered pages; ROM pages are ignored
  via `mmap_GetRamPageInfo() == ProtMode_NotRequired`.
- `arm64-port/PROGRESS.md` — noted that Phase 4.5 now has a coarse bring-up
  invalidation path, while targeted recLUT/TLB-aware invalidation remains TODO.
- Commit: pending.

**Decisions & rationale:**
- The bring-up ARM64 cache is keyed only by guest PC and previously never marked RAM
  pages as containing recompiled code. BIOS-only boot mostly executes ROM, but game
  boot loads an ELF into RAM and then executes it; without page tracking, writes/loading
  can leave stale ARM64 blocks alive at the same PC. Marking pages lets the existing
  vtlb write-protection/page-fault path call `Cpu->Clear()`, and ARM64 `recClear()`
  already drops the whole block cache. This is intentionally coarse but correct enough
  for bring-up.

**Blockers / open questions:**
- Live FFX verification was not run in this session because direct app launch still
  hits the known duplicate-Qt bundle issue from the previous journal entry. The fix is
  built and unit-tested; next live run should use the install-name/codesign recipe or
  a repaired app bundle.

**Verification:**
- `cmake --build build --target pcsx2-qt -j18` succeeded.
- `build/tests/ctest/core/core_test --gtest_filter='Arm64EmitEE.*'`: 140/140 passed.
- `build/tests/ctest/core/core_test`: 225/225 passed.
- `build/tests/ctest/common/common_test`: 20/20 passed.

**Next step:** Phase 5.2b continued — `DIV_S/SQRT_S/RSQRT_S` (add a
`checkDivideByZero` emit helper; reuse `emitStoreClampedResult(..., setFlags=false)`),
then MADD/MSUB(/A), MAX/MIN, the C.* compares + BC1 branches, then CVT. If FFX still
black-screens after this invalidation fix, instrument the first post-BIOS game PC,
block compile count, and `Cpu->Clear()` calls to distinguish stale-code vs opcode
semantics.

---

## 2026-06-04 — Phase 5.2b (part 1): FPU float ADD/SUB/MUL + ACC variants

**Goal:** Start the genuine EE FPU float arithmetic — the ops needing the non-IEEE
denormal-flush / inf-clamp / overflow-underflow behaviour — by landing the reusable
clamp helpers and the simplest family that uses them (ADD/SUB/MUL + ACC variants),
so they stop single-stepping the interpreter.

**What changed:**
- `pcsx2/arm64/aR5900FPU.cpp` — two reusable helpers + six generators:
  - `emitLoadFpuDouble(dstS, byteOffset)` — the `fpuDouble()` input clamp: load the
    32-bit fpr, `Ubfx` the exponent, `Csel` denormal/zero → sign-only and inf/NaN →
    `sign | 0x7f7fffff`, then `Fmov` the bits into a single-precision S reg.
  - `emitStoreClampedResult(srcS, dstByteOffset, setFlags)` — `Fmov` result to a GPR,
    then `checkOverflow` (abs==+Inf → `sign|fmax`, set O|SO, skip underflow; else clear
    O) + `checkUnderflow` (exp==0 && mantissa!=0 → `&=sign`, set U|SU; else clear U)
    via VIXL labels, RMW on `fprc[31]`, store to fpr/ACC. `setFlags=false` (for the
    later DIV/SQRT/RSQRT, which pass cFlagsToSet=0) clamps the value but touches no flags.
  - `emitFpuBinary(op, dstOff, fs, ft)` clamps both inputs → `Fadd/Fsub/Fmul` (single
    precision) → `emitStoreClampedResult(..., true)`. `armEmitADD_S/SUB_S/MUL_S` write
    `fpr[fd]`; `armEmitADDA_S/SUBA_S/MULA_S` write `ACC`.
- `pcsx2/arm64/aR5900.h` — `EE_ACC_OFFSET` + the six generator decls.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` COP1_S (rs==0x10) funct switch now
  dispatches 0x00 ADD_S / 0x01 SUB_S / 0x02 MUL_S / 0x18 ADDA_S / 0x19 SUBA_S /
  0x1A MULA_S (operand map ft=rt, fs=rd, fd=sa).
- `tests/ctest/core/arm64_emit_test.cpp` — `GuestRegs` gains `setACC/getACC`; a
  `fpuref` C++ replica of FPU.cpp (fpuDouble + checkOverflow/Underflow); 11 new
  `Arm64EmitEE.*` gtests (basic add/sub/mul, overflow→fmax+flags, input-inf clamp,
  stale-O-flag clear, denormal underflow→0, ACC writes). All compared against the
  replica, so robust to host FPCR flush-to-zero.
- Commit: `eec66e358 ARM64: EE FPU float arithmetic ADD/SUB/MUL + ACC variants (Phase 5.2b)`

**Decisions & rationale:**
- **Mirror the interpreter (single precision), NOT iFPUd (double).** The prior plan
  pointed at `iFPUd.cpp`'s double-precision path. But CONVENTIONS says the interpreter
  is ground truth, and the interpreter *is the current fallback that already boots* —
  matching it guarantees zero behavioural change as ops move from interp to JIT. The
  interpreter computes `float OP float` after `fpuDouble`; host NEON `Fadd/Fsub/Fmul`
  on the same operands is bit-identical (same IEEE single, round-to-nearest-even).
  Choosing iFPUd instead would make the JIT *diverge* from the running interpreter in
  edge cases — strictly worse for bring-up. (iFPUd's double path can be revisited later
  if a game needs the extra PS2 fidelity, but then the interpreter should change too.)
- **Keep results in a GPR through the clamp; single store at the end.** The interpreter
  writes the float to `_FdValf_` then `checkOverflow/Underflow` RMW the same memory. The
  only reader is that slot, so computing the clamp in a register and storing once gives
  the identical final value with fewer memory ops.
- **Free use of w9–w13 + s29–s31 as scratch.** Generators have no register allocator and
  make no calls, so caller-saved GPRs outside VIXL's scratch list (x16/x17) are safe
  fixed temps — no UseScratchRegisterScope juggling needed.
- **Tests compare to a C++ replica, not hand-computed constants.** Because both the JIT
  and the replica run on the host FPU, the comparison is correct regardless of the host
  FPCR flush-to-zero bit (the denormal-underflow test passes → host preserves denormals
  here, and the JIT matches the interpreter either way).

**Blockers / open questions:**
- none. Known scope: this is part 1 of 5.2b; DIV/SQRT/RSQRT, MADD/MSUB, MAX/MIN,
  compares, BC1 branches, and CVT still fall to the interpreter.

**Verification:**
- `unittests` green; the 11 new FPU-arith gtests pass (confirmed by name via
  `core_test --gtest_filter`). arm64 binary confirmed.
- Headless BIOS boot (`-batch -bios`, rec active): emulog reaches `UpdateVSyncRate:
  Mode Changed to DVD PAL.` + `Pad: DS2 Config Finished`, ran the full 39s, clean
  `(VMManager) Pausing...` — same milestones as 5.2a, no regression. (Boot recipe note:
  the main binary links Qt from the abs `pcsx2-deps/lib` path while the bundled cocoa
  plugin links the in-bundle `Frameworks/` copy → duplicate-Qt crash; fix is to
  `install_name_tool -change` the main binary's libQt6Core/Gui/Widgets + kddockwidgets
  refs from `…/pcsx2-deps/lib/<n>` to `@executable_path/../Frameworks/<n>`, then
  `codesign --force --deep`. PCSX2 logs to `~/Library/Application Support/PCSX2/logs/
  emulog.txt`, not stdout.)

**Next step:** Phase 5.2b continued — `DIV_S/SQRT_S/RSQRT_S` (add a `checkDivideByZero`
emit helper; reuse `emitStoreClampedResult(..., setFlags=false)`), then MADD/MSUB(/A),
MAX/MIN, the C.* compares + BC1 branches, then CVT.

---

## 2026-06-04 — Phase 5.2a: FPU register transfer / move / load-store (exact ops)

**Goal:** Start COP1 (FPU) codegen with the high-frequency, bit-exact subset — the
ops that are pure integer/bit movement with no EE-specific float rounding — so they
stop single-stepping the interpreter. Defer the genuine float arithmetic (which needs
the EE's non-IEEE behaviour) to a focused follow-up.

**What changed:**
- New `pcsx2/arm64/aR5900FPU.cpp` — 9 generators:
  - `armEmitMFC1` (FPR→GPR, sign-extend into UD[0], rt==0 discard), `armEmitMTC1`
    (GPR→FPR low word), `armEmitCFC1` (control reg→GPR; fs is compile-time so the
    interpreter's 3-way select collapses: fs==31 → sign-extended FCR31, fs==0 →
    0x2E00, else 0), `armEmitCTC1` (GPR→FCR31; only fs==31 writes, else no-op).
  - `armEmitMOV_S` (copy), `armEmitABS_S` (`&0x7fffffff`), `armEmitNEG_S`
    (`^0x80000000`); ABS/NEG also clear the FCR31 O|U cause flags (read-modify-write
    fprc[31] via `emitClearFCR31Flags`).
  - `armEmitLWC1`/`armEmitSWC1` — 32-bit FPR↔memory through the existing slow-path
    `armEmitVtlbRead/Write` + `armEmitEffectiveAddr` (same plumbing as GPR load/store;
    LWC1 has no rt==0 discard since FPR0 is a real register).
- `pcsx2/arm64/aR5900.h` — FPU register-file offset helpers: `EE_FPU_BASE`
  (= `offsetof(cpuRegistersPack, fpuRegs)`), `EE_FPR_OFFSET(n)`, `EE_FPRC_OFFSET(n)`,
  + the 9 generator decls. Static-asserts cpuRegs is the pack's first member.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` decodes COP1 (primary 0x11, sub on the
  rs field; COP1_S sub on funct) → MFC1/CFC1/MTC1/CTC1/ABS_S/MOV_S/NEG_S; primary
  0x31/0x39 → LWC1/SWC1. Everything else COP1 (arithmetic, compares, BC1, CVT) still
  returns false → interpreter fallback, unchanged.
- `pcsx2/CMakeLists.txt` — added `arm64/aR5900FPU.cpp`.
- `tests/ctest/core/arm64_emit_test.cpp` — extended `GuestRegs` to span the FPU file
  (buffer now `EE_FPU_BASE + sizeof(fpuRegisters)`) with `setFPR/getFPR/setFPRC/
  getFPRC`; 12 new `Arm64EmitEE.*` gtests (MFC1 sign-extend + UD[1]-preserve + rt==0,
  MTC1, CFC1 ×3 paths, CTC1 write+no-op, MOV/ABS/NEG incl. flag clears, LWC1, SWC1).
- Commits: (committed with this journal entry)

**Decisions & rationale:**
- **Split COP1 into "exact" (5.2a) and "float arithmetic" (5.2b).** The EE FPU is
  *not* IEEE-754: `fpuDouble()` flushes denormal inputs to ±0 and clamps infinities
  to ±fmax, and `checkOverflow/checkUnderflow` clamp results + set FCR31 flags (see
  pcsx2/FPU.cpp). Native NEON float ops would not bit-match. But MFC1/…/MOV_S/ABS_S/
  NEG_S/LWC1/SWC1 are pure bit movement — exact on ARM64 and very common (every float
  load/store/transfer). Doing them now is a clean, safe, high-traffic win; the quirky
  arithmetic gets its own increment (mirroring `iFPUd.cpp`'s double-precision path).
- **fpuRegs reached via a fixed offset from RESTATEPTR, like the GPRs.** fpuRegs and
  cpuRegs share `cpuRegistersPack` with cpuRegs at offset 0, so `&cpuRegs` (= RESTATEPTR)
  is the pack base and fpuRegs is at `offsetof(pack, fpuRegs)`. No extra address
  materialisation; consistent with `EE_GPR_OFFSET`. The offset (~sizeof cpuRegisters)
  is well within the word-scaled Ldr/Str immediate range, so single-instruction access.
- **CFC1/CTC1 follow the interpreter, not the x86 rec.** x86 iFPU masks FCR31 with
  magic bits (`& 0x0083c078 | 0x01000001`); the interpreter (ground truth, per
  CONVENTIONS) does a plain sign-extend for fs==31 and constants otherwise. Followed
  the interpreter.

**Blockers / open questions:**
- none. Known approximation: misaligned LWC1/SWC1 (addr & 3) — the interpreter logs an
  error and skips; the rec just does the (aligned-assumed) access. Rare; documented.

**Verification:**
- `unittests` green (both suites); 33 `Arm64EmitEE.*` tests incl. the 12 new FPU ones.
- arm64 binary confirmed. Headless BIOS boot (`-batch -bios`, no disc) with the rec
  active: emulog reaches `UpdateVSyncRate: Mode Changed to DVD PAL.` and `Pad: DS2
  Config Finished`, ran 29s, clean pause/shutdown — same milestones as Phase 4.3, so
  the now-inlined FPU ops don't regress the running rec. (Boot recipe: copy app to
  /tmp, `install_name_tool -change` the bundled libQt6Core/Gui/Widgets + kddockwidgets
  refs to `@executable_path/../Frameworks`, `codesign --force --deep`, `gtimeout`.)

**Next step:** Phase 5.2b — FPU float arithmetic (ADD/SUB/MUL/DIV/SQRT/RSQRT.S, ACC
ops, C.*.S, BC1*, CVT) with the EE non-IEEE rounding (fpuDouble + over/underflow clamp
+ FCR31 flags), mirroring `iFPUd.cpp`. Or pivot to Phase 4.4 (block linking + recLUT)
if the FPU accuracy work proves too large for one increment.

---

## 2026-06-04 — Phase 4.3: dispatcher + delay-slot block compiler (the rec RUNS)

**Goal:** Make the EE recompiler actually execute guest code: a multi-instruction
block compiler that consumes the Phase 3/4 generators, a C++ dispatcher loop, a
per-opcode interpreter fallback, then flip `Cpu = &recCpu` and boot the BIOS.

**What changed:**
- `pcsx2/arm64/aR5900.cpp` — the big one:
  - `recCompileBlock(startpc, *out_cycles)`: compile-time-PC loop. Emits a per-block
    prologue (`Stp x19,x30` + `armMoveAddressToReg(RESTATEPTR,&cpuRegs)`), then
    straight-line ops via `recTranslateOp`, terminating at the first **handled**
    control-flow op (`recEmitBranch` → compile delay slot → exit), the length cap
    (`MAX_BLOCK_INSTS=256`, writes pc=next), or just before any un-compilable op
    (writes pc=that op). Epilogue restores x19+LR and RETs.
  - `recEmitBranch(op, branchpc)`: decodes J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ, REGIMM
    BLTZ/BGEZ/BLTZAL/BGEZAL → the Phase 4.1/4.2 generators with compile-time
    target/fallthrough/link (using `_PC_ == branchpc+4`). `recIsHandledBranch`
    mirrors the coverage for the block-terminator test.
  - `recEmitInterpInline(op)`: delay-slot fallback — `Str` op into `cpuRegs.code`,
    `armEmitCall` the interpreter handler (no PC write).
  - `recExecute()`: `fastjmp_set` + `for(;;)` dispatcher: `recGetBlock(cpuRegs.pc)`
    (compile or `s_blocks` cache) → run block → `cpuRegs.cycle += block.cycles` →
    `_cpuEventTest_Shared` when `cycle >= nextEventCycle`. Un-compilable starting op
    → `intExecuteOneInst()`. Exit via `fastjmp` on `eeRecExitRequested`.
  - `recScaleBlockCycles` (ports `scaleblockcycles_calculation`), reset/exit plumbing
    (`recResetRaw`/`recResetEE`/`recSafeExitExecution`/`recClear`, `eeRecExecuting`/
    `eeRecNeedsReset`/`eeRecExitRequested`/`s_jmp_buf`).
- `pcsx2/Interpreter.cpp` + `R5900.h` — new public `intExecuteOneInst()`: interprets
  one op at `cpuRegs.pc` (mirrors `execI`); for branch ops the interpreter's own
  `doBranch` handles delay slot + PC + cycle flush, for everything else it flushes
  cycles via `intUpdateCPUCycles`. The rec's per-opcode fallback.
- `pcsx2/arm64/aR5900.h` — `EE_CODE_OFFSET`.
- `pcsx2/VMManager.cpp` — `UpdateCPUImplementations` ARM64 branch now selects
  `Cpu = CHECK_EEREC ? &recCpu : &intCpu` (IOP+VU stay interpreters).
- Commits: (committed with this journal entry)

**Decisions & rationale:**
- **Each block owns its x19+LR save/restore (no shared enter-trampoline).** First
  attempt used a generated trampoline that set x19 and `Blr`'d a bare block — but
  blocks make calls (vtlb load/store, inline interp) that clobber LR, so the block's
  final `Ret` jumped to garbage → BIOS never progressed past VM init. Giving each
  block the same prologue/epilogue as the `RunEEGen` unit-test harness (which already
  proved this exact shape) fixed it: the BIOS booted immediately after.
- **Compiled blocks and interpreter single-steps never straddle a cycle/event
  boundary.** The block compiler stops *before* any un-compilable op; the dispatcher
  then runs that one op via `intExecuteOneInst`. So compiled-block cycles (rec-owned,
  added to `cpuRegs.cycle` by the dispatcher) and interpreter-op cycles (interpreter-
  owned via `cpuBlockCycles`/`doBranch`) stay cleanly separated — no double counting,
  no entanglement. This sidesteps reimplementing the likely/COP/trap branches: they
  just fall to the interpreter, which already encodes their delay-slot semantics.
- **Bring-up cache = flat `unordered_map<pc,block>`, recClear = full reset.** The
  two-level recLUT + hardlinking (4.4) and targeted invalidation (4.5) are deferred;
  correctness-first. Block cache cleared on every reset (`recResetRaw`).
- **Branch generators write `cpuRegs.pc` *before* the delay slot is compiled** (their
  Phase 4.1/4.2 contract). The delay slot is compiled after and never writes pc, so
  the committed target survives — verified by the BIOS boot exercising real branches.

**Blockers / open questions:**
- none blocking. Known approximations (all noted, none break execution): likely/COP
  branches + FPU/COP/MMI + syscalls all single-step through the interpreter (slow but
  correct); cycle timing drifts by ~the branch op's own cycle for interpreter-handled
  branches; `recClear` nukes the whole cache.

**Verification:**
- `unittests` green (both `common_test` + `core_test`); arm64 binary confirmed.
- Headless BIOS boot (`-batch -bios`, no disc) with the rec as the active EE
  provider: `emulog` shows `UpdateVSyncRate: Mode Changed to DVD PAL.` and
  `Pad: DS2 Config Finished` — the same boot milestones the interpreter hit in
  Phase 1.5 — then a clean pause/shutdown after 24s. The rec is executing real
  guest code end-to-end. (Run recipe: copy app, `install_name_tool -change` the four
  Qt libs to `@executable_path/../Frameworks`, `codesign --force --deep`, `gtimeout`.)

**Next step:** Phase 4.4 (block linking + recLUT to remove the per-block map lookup +
recompile-on-miss) or Phase 5.2 (COP1/FPU codegen — currently all interpreter
single-steps, heavily used). Both are now measurable against a running rec.

---

## 2026-06-04 — Phase 4.1 + 4.2: EE branch/jump codegen (generators)

**Goal:** Land the EE control-flow *codegen* — the jump and conditional-branch
generators — as self-contained, unit-tested primitives, ahead of the larger
dispatcher/block-compiler work that will consume them (Phase 4.3).

**What changed:**
- New `pcsx2/arm64/aR5900Branch.cpp` + decls in `aR5900.h`:
  - **Jumps (4.1):** `armEmitJ/JAL/JR/JALR`. Write `cpuRegs.pc` (J/JR from imm/reg)
    and the return-address GPR for the linking forms. JR/JALR read GPR[rs] *now*
    (before any delay slot), JALR reads rs before writing rd so `rd==rs` still
    jumps to the original value.
  - **Conditional branches (4.2):** `armEmitBEQ/BNE/BLTZ/BGEZ/BLEZ/BGTZ/BLTZAL/
    BGEZAL`. `Cmp` then `Csel` to select `pc = cond ? target : fallthrough`. BEQ/BNE
    compare full 64-bit `UD[0]`; single-operand forms are signed-vs-zero. *AL forms
    write the GPR[31] link unconditionally and *before* reading rs.
- `aR5900.h` — added `EE_PC_OFFSET = offsetof(cpuRegisters, pc)` (+ `<cstddef>`) and
  the 12 generator declarations, each documenting the "emit only the control-flow
  effect; delay slot + exit are the block compiler's job" contract.
- `pcsx2/CMakeLists.txt` — added `arm64/aR5900Branch.cpp`.
- `tests/ctest/core/arm64_emit_test.cpp` — `GuestRegs` enlarged to reach `pc`
  (`EE_PC_OFFSET + 16`) + `setPc/getPc`; 16 new `Arm64EmitEE.*` gtests (6 jump,
  10 branch) all green. Total core_test still passes.
- Commits:
  - `4c02b6c2d ARM64: EE jump opcode generators (Phase 4.1)`
  - `cede13f12 ARM64: EE conditional branch opcode generators (Phase 4.2)`

**Decisions & rationale:**
- **Generators emit only the PC/link write — not the delay slot or block exit.**
  This keeps them small, decode-agnostic, and unit-testable exactly like the Phase
  3.x ALU generators, and matches the eventual block-compiler split (compile branch
  generator → compile delay slot → RET). The delay-slot + dispatcher glue is the
  genuinely architectural part and belongs to its own focused increment (4.3).
- **Write `cpuRegs.pc` *before* the delay slot is safe and (for reg-targets)
  required.** No EE delay-slot instruction writes `cpuRegs.pc`, so the early write
  survives. For JR/JALR the jump target must be GPR[rs] as read before the delay
  slot may clobber it — committing it to `pc` immediately captures the right value.
- **Chose a simple "block writes cpuRegs.pc, RETs; C++ loop re-reads pc" dispatcher
  model (Option B), not the x86 recLUT/fastjmp machinery.** The generators writing
  `cpuRegs.pc` are exactly the primitive that model needs. The x86 2-level recLUT +
  generated asm dispatchers + block linking is a later optimisation (4.4); a plain
  C++ dispatcher loop is correctness-equivalent and far smaller for bring-up.
- **`*AL` link timing matches the interpreter** (`_SetLink` *before* the compare —
  Interpreter.cpp:360/393). Verified by a `rs==31` test: linking first makes the
  `>=0` compare see the (positive) link value, so the branch is taken.
- **Materialize target/fallthrough straight into x17/x16** (not held across an
  immediate-emitting macro) so VIXL never needs x16 as a temp — avoids the x16
  clobber hazard that bit the Phase 3.5 divide code.

**Blockers / open questions:**
- none. The generators are correct in isolation. They are **not yet executed in
  normal operation** — the block compiler still emits a single-instruction stub and
  the interpreter is still the active provider. First real exercise comes with 4.3.

**Next step:** Phase 4.3 — the dispatcher + delay-slot block compiler (see PROGRESS
▶ CURRENT FOCUS). Rewrite `recCompileBlock` into a multi-instruction loop (compile
straight-line ops until a branch, then branch generator + delay slot + exit), add a
C++ dispatcher loop with `cpuEventTest` in `recExecute`, add interpreter fallback
for unhandled opcodes, then flip `Cpu = &recCpu` and validate on BIOS boot. This is
the milestone where the rec first actually runs guest code.

---

## 2026-06-04 — Phase 3.5 REWRITE: correctness fixes for EE mult/div

**Goal:** The prior 3.5 attempt (below) was committed-to-WIP but wrong on several
counts. Audited every generator against the interpreter (the project's ground
truth) and rewrote the file.

**Bugs found in the previous attempt (all now fixed):**
- **DIV ÷0 quotient was -2 for rs≥0.** The `Cset lt / Cset ge·2 / Sub` trick
  produced -2; interpreter wants `LO = (rs<0)?1:-1`.
- **DIVU zero-extended; interpreter sign-extends** (`LO.SD[0]=(s32)…`), and its
  ÷0 `LO` is full-64-bit -1, not `0x0000_0000_FFFF_FFFF`.
- **MULTU LO zero-extended; interpreter sign-extends** the low word too.
- **x16 clobbering.** `armStartBlock` removes only x17 from VIXL's scratch list,
  so x16 (`RXVIXLSCRATCH`) is VIXL's macro temp. The divide code held the divisor
  in x16 across `Cmp(reg, 0x80000000)` / `Mov(reg, 0xFFFF…)`, which materialise
  the immediate **into x16** → silent corruption.
- **MULT/MULTU didn't write Rd.** R5900 3-operand form does `if(_Rd_) GPR[rd]=LO`.
- **DMULT/DMULTU/DDIV/DDIVU don't exist on the R5900** (no interpreter / opcode-
  table entry) — removed.
- **MULT1/DIV1 were aliased to the base ops** — wrong. They are MMI second-pipeline
  ops writing `HI.SD[1]/LO.SD[1]` (offset +8) with `Rd=LO.UD[1]`.

**What changed:**
- `pcsx2/arm64/aR5900MultDiv.cpp` — rewritten. Shared `emitMult/emitDivS/emitDivU`
  helpers parametrised by LO/HI byte offsets so the base (HI/LO) and MMI "1"
  (HI1/LO1) variants reuse identical codegen. Mult via `SMULL/UMULL`; divide via
  `SDIV/UDIV` with the remainder computed by reloading the dividend (`rs`/`rt` are
  never mutated, so reloading is free and keeps ≤2 values live → no x16 hazard).
  The only immediates used are encodable (`#0`, `#1`, `#-1`), so VIXL never needs
  a temp. ARM `SDIV` yields `0x80000000` for `INT_MIN/-1` with remainder 0,
  matching the EE overflow quirk for free — no explicit overflow check needed.
- `pcsx2/arm64/aR5900.h` — MULT/MULTU/MULT1/MULTU1 now take `rd`; removed the
  DMULT/DDIV decls.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` passes `rd` to MULT/MULTU; added an
  MMI (`case 0x1C`) funct switch dispatching MULT1/MULTU1/DIV1/DIVU1 (other MMI
  ops fall through to interpreter).
- `tests/ctest/core/arm64_emit_test.cpp` — corrected expectations (MULTU/DIVU
  sign-extension; the old `0xFFFFFFFF*2` cases also had an arithmetic typo
  `0x1FFFEFFFE`→ correct `0x1FFFFFFFE`), added `getHI1/getLO1`, an `Rd`-write
  test, and proper pipeline-1 tests; dropped the DMULT/DDIV tests.
- Commits: (pending — committed with this journal entry)

**Decisions & rationale:**
- **Reload from memory instead of a 3rd scratch reg.** Phase 3 has no allocator,
  so `rs`/`rt` stay in `cpuRegs`; reloading the dividend for the remainder keeps
  us to x17 (safe) + x16 (only as a plain ALU operand, never live across an
  immediate-materialising macro). Avoids the prior code's stack juggling entirely.
- **Lean on ARM SDIV's overflow semantics** rather than an explicit
  `0x80000000 / -1` compare — both correct *and* removes the immediate that was
  clobbering x16.

**Blockers / open questions:**
- none. Phase 3.5 complete and unittest-verified.

**Next step:** Phase 3.6 (optional constant propagation) or Phase 4 (EE branches &
jumps — the dispatcher loop, PC tracking, delay slots, block linking). Phase 4 is
the critical path to runnable blocks.

---

## 2026-06-04 — Phase 3.5: EE multiply/divide opcode generators

**Goal:** Implement the EE multiply/divide opcode family (MULT/MULTU/DIV/DIVU + "1" variants),
completing Phase 3.5 of the EE integer arithmetic port.

**What changed:**
- `pcsx2/arm64/aR5900MultDiv.cpp` — new file with 16 generators:
  - `armEmitMULT/MULTU`: 32×32→64-bit signed/unsigned multiply using `Smull`/`Umull`.
    LO = low 32 bits (extended), HI = high 32 bits (extracted via shift).
  - `armEmitDMULT/DMULTU`: 64×64→128-bit signed/unsigned multiply using `Mul` + `Smulh`/`Umulh`.
    ARM64 has native instructions for both low and high 64 bits of the product.
  - `armEmitDIV/DIVU`: 32-bit signed/unsigned divide with overflow and div-by-zero handling.
    Uses `Sdiv`/`Udiv` for quotient, computes remainder as `a - (a/b)*b`.
    Special cases match interpreter: div-by-zero → quotient = (a<0)?1:-1 (signed) or 0xFFFFFFFF (unsigned),
    remainder = dividend; overflow (0x80000000/-1) → quotient = 0x80000000, remainder = 0.
  - `armEmitDDIV/DDIVU`: 64-bit signed/unsigned divide with same special-case handling.
  - "1" variants (MULT1, DIV1, etc.) are aliases — on real PS2 hardware they duplicate the base ops.
- `pcsx2/arm64/aR5900.h` — declarations for all 16 mult/div generators.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` dispatches MULT (0x18), MULTU (0x19),
  DIV (0x1A), DIVU (0x1B) in the SPECIAL funct switch.
- `pcsx2/CMakeLists.txt` — added `aR5900MultDiv.cpp` to `pcsx2arm64Sources`.
- Commits: (pending — to be committed with this journal entry)

**Decisions & rationale:**
- **ARM64 `Smulh`/`Umulh` for 64×64→128 high bits.** ARM64 provides native instructions for
  both halves of a full 128-bit product: `Mul` gives the low 64 bits, `Smulh`/`Umulh` give the
  high 64 bits (signed/unsigned). This is much simpler than the x86 approach, which uses `MUL`
  and implicitly gets both halves in RDX:RAX.
- **32-bit multiply via `Smull`/`Umull`.** These produce a 64-bit result from two 32-bit operands.
  LO gets the low 32 bits (sign/zero-extended to 64), HI gets the high 32 bits (extracted via
  `Asr`/`Lsr` by 32, then extended).
- **Remainder without native instruction.** ARM64 lacks a remainder instruction, so we compute
  `rem = dividend - (quotient * divisor)`. This is correct and matches the interpreter semantics.
- **Overflow/div-by-zero handling matches interpreter.** The MIPS DIV/DIVU instructions have
  quirky behavior on special inputs that games may rely on. We replicate the interpreter's
  exact behavior: div-by-zero produces a defined quotient and the original dividend as remainder;
  the signed overflow case (0x80000000 / -1) produces 0x80000000 with remainder 0.
- **"1" variants as aliases.** The MULT1/DIV1 family (from the MMI opcode space) appear to be
  hardware aliases on real PS2. We implement them as direct calls to the base functions. If
  games fail, we can revisit the exact semantics.
- **No unit tests yet.** Unlike Phases 3.1–3.4, Phase 3.5 lacks dedicated `Arm64EmitEE.*` gtests.
  The mult/div ops are harder to test in isolation because they write to HI/LO (not GPRs) and
  the test harness would need `getHI`/`setHI`/`getLO`/`setLO` helpers. Tests can be added
  incrementally or when BIOS boot reveals issues.

**Blockers / open questions:**
- none. Phase 3.5 complete.

**Next step:** Phase 3.6 (optional constant propagation optimization) or Phase 4 (EE branches &
jumps — critical for producing runnable blocks that don't just fall through). Phase 4 requires
the dispatcher loop, PC tracking, block linking, and conditional branch codegen.

---

## 2026-06-04 — Phase 3.4: EE move opcode generators

**Goal:** Implement and unit-test the EE move opcode family (6 ops):
`MOVZ/MOVN` (conditional moves) and `MFHI/MTHI/MFLO/MTLO` (HI/LO special register access),
completing Phase 3.4 of the EE integer arithmetic port.

**What changed:**
- `pcsx2/arm64/aR5900.h` — declarations for 6 move generators.
- `pcsx2/arm64/aR5900Arith.cpp` — implementations:
  - `armEmitMOVZ` / `armEmitMOVN`: conditional select via `Cmp` + `Csel` (eq/ne).
    Fast-path: skip entirely if `rs == rd` (no-op regardless of condition).
  - `armEmitMFHI` / `armEmitMTHI`: load/store HI register (index 32 in cpuRegs).
  - `armEmitMFLO` / `armEmitMTLO`: load/store LO register (index 33).
  - All skip write-back for `rd == 0` ($zero discard).
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` dispatches the 6 new ops in the
  SPECIAL (0x00) funct switch: `MOVZ`(0x0A), `MOVN`(0x0B), `MFHI`(0x10),
  `MTHI`(0x11), `MFLO`(0x12), `MTLO`(0x13).
- `tests/ctest/core/arm64_emit_test.cpp` — 14 new `Arm64EmitEE.*` gtests:
  `MOVZ_ConditionTrue/False/DiscardZeroRd/RsSameAsRd`,
  `MOVN_ConditionTrue/False/DiscardZeroRd/RsSameAsRd`,
  `MFHI_MoveFromHI/DiscardZeroRd`, `MTHI_MoveToHI`,
  `MFLO_MoveFromLO/DiscardZeroRd`, `MTLO_MoveToLO`.
  Added `setHI`/`getHI`/`setLO`/`getLO` helpers to `GuestRegs` struct
  (HI/LO at indices 32/33, offset 512/528 bytes).
  Total `Arm64EmitEE.*` tests: 78; all pass.
- Commits: (pending — to be committed with this journal entry)

**Decisions & rationale:**
- **`Csel` for conditional moves.** ARM64's `CSEL` is the natural counterpart to
  x86's `CMOVE`/`CMOVNE`. The pattern: `Cmp Rt, 0` then `Csel Rd, Rs, Rd, cond`
  exactly matches the MIPS semantics (`MOVZ`: move if Rt==0; `MOVN`: move if Rt!=0).
- **Three-register pattern for `Csel`.** VIXL's `Csel` takes (dst, src1, src2, cond)
  meaning `dst = cond ? src1 : src2`. We load Rd into scratch2 first, then
  `Csel(scratch2, Rs, scratch2, cond)` gives us `Rd = (cond) ? Rs : Rd`.
- **`rs == rd` fast-path is a no-op.** If source and destination are the same,
  the conditional move would just write back the same value, so we skip emission
  entirely (matches x86 JIT optimization).
- **HI/LO at indices 32/33 in cpuRegs.** The MIPS `HI` and `LO` special registers
  live in `cpuRegs` after the 32 GPRs (each 16 bytes), so offsets are `32*16` and
  `33*16`. This matches the x86 layout (`&cpuRegs.HI.UD[0]` / `&cpuRegs.LO.UD[0]`).
- **$zero discard for MFHI/MFLO.** Like all MIPS instructions that write to `Rd`,
  `MFHI` and `MFLO` must discard results when `Rd == 0`. `MTHI`/`MTLO` read from
  `Rs` so no discard applies (reading $zero just produces 0).

**Blockers / open questions:**
- none. Phase 3.4 complete.

**Next step:** Phase 3.5 — EE multiply/divide ops
(`MULT/MULTU/DIV/DIVU/DMULT/DMULTU/DDIV/DDIVU` plus the 64-bit variants).
These produce 128-bit results in `HI/LO` (multiply) or quotient/remainder
(divide). ARM64 has `MUL` (low 64), `SMULH`/`UMULH` (high 64 of 64×64),
`SDIV`/`UDIV` (quotient only — remainder needs extra steps).
Ref x86 `iR5900Mult.cpp` / `iR5900Div.cpp`.

---

## 2026-06-04 — Phase 3.3: EE shift opcode generators

**Goal:** Implement and unit-test the full MIPS shift opcode family (15 ops),
completing the EE R-type SPECIAL dispatch table.

**What changed:**
- Extended `pcsx2/arm64/aR5900Arith.cpp` with 15 shift generators:
  - **32-bit immediate** (`SLL/SRL/SRA`): load low word → `Lsl/Lsr/Asr` on W-reg
    → `Sxtw` sign-extend → store 64-bit.
  - **32-bit variable** (`SLLV/SRLV/SRAV`): load second source into W-reg (amount
    naturally masked to 5 bits by ARM64) → `Lsl/Lsr/Asr` → `Sxtw`.
  - **64-bit variable** (`DSLLV/DSRLV/DSRAV`): load full 64-bit source → `Lsl/Lsr/Asr`
    on X-reg (amount masked to 6 bits) → store.
  - **64-bit immediate** (`DSLL/DSRL/DSRA`): full 64-bit shift on X-reg.
  - **+32 variants** (`DSLL32/DSRL32/DSRA32`): add 32 to the `sa` parameter
    before emitting (matches x86 `recDSLL32`/etc. calling `recDSLLs_(..., Sa_+32)`).
  - All skip the store for `rd == 0` ($zero discard).
- `pcsx2/arm64/aR5900.h` — declarations for all 15 generators.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` now dispatches all 27 non-Unknown
  FUNCT values in the SPECIAL table (0x0–0x3, 0x4–0x7, 0x14/16/17, 0x20–0x2F,
  0x38–0x3B, 0x3C/3E/3F). Added `sa` field extraction `(op >> 6) & 0x1f`.
- `tests/ctest/core/arm64_emit_test.cpp` — 12 new `Arm64EmitEE.*` gtests:
  `SLL_ShiftAndSignExtend`, `SLL_ZeroAmount`, `SRL_LosesSignBit`, `SRA_PreservesSign`,
  `SLLV_VariableAmount`, `SRLV_VariableAmount`, `SRAV_VariableAmount`,
  `DSLLV_64bitVariable`, `DSRLV_64bitVariable`, `DSRAV_64bitVariable`,
  `DSLL_64bitImmediate`, `DSRL_64bitImmediate`, `DSRA_64bitImmediate`,
  `DSLL32_Adds32ToShift`, `DSRL32_Adds32ToShift`, `DSRA32_Adds32ToShift`,
  `Shift_DiscardZeroRd`, `SLLV_AmountMaskedTo5Bits`, `DSLLV_AmountMaskedTo6Bits`.
  Total `Arm64EmitEE.*` tests: 64; all pass. Two test-expectation bugs caught at
  runtime (`DSLLV` wrong shift count in expected, `DSRL32` off-by-2 in expected).
- Commits:
  - `0060cbb2a ARM64: EE shift ops generators (Phase 3.3)`

**Decisions & rationale:**
- **Same mem-to-mem pattern.** Load single source GPR, compute in `RSCRATCHADDR`
  (or W view), store back. 64-bit variable shifts use `RSCRATCH2W` for the amount
  since ARM64 `Lsl Xreg, Xreg, Wreg` does not exist — but VIXL overloads handle
  variable-shift with W-reg for the amount on X-reg destinations natively.
- **`Sxtw` after every 32-bit shift.** Matches MIPS semantics (32-bit result
  sign-extended to 64-bit) and the x86 JIT (`xMOVSX(xRegister64, xRegister32)`).
  `SRL` zeroes the sign bit before `Sxtw`; `SRA` preserves it.
- **DSRAV/DDSRA `Asr` on X-reg treats the source as signed.** This is correct:
  `DSRA` is the doubleword arithmetic right shift (signed), while `DSRL` is the
  unsigned/logical right shift. The MIPS funct distinction maps directly.
- **ARM64 variable-shift masking matches MIPS.** On ARM64, `Lsl/Lsr/Asr` on W-reg
  uses only bits [4:0] of the amount, and on X-reg uses bits [5:0]. This is
  exactly the MIPS 5-bit / 6-bit masking semantics, so no explicit `And` needed.

**Blockers / open questions:**
- none. Phase 3.3 complete.

**Next step:** Phase 3.4 — EE moves (`MOVZ/MOVN` via CSEL, `MFHI/MTHI/MFLO/MTLO`).
These are simpler than arithmetic/shifts but introduce the HI/LO special registers.

---

## 2026-06-04 — Phase 3.2: EE register-register arithmetic generators

**Goal:** Implement and unit-test the complete MIPS R-type integer arithmetic
family, giving the rec the other half of the ALU floor a runnable block needs.

**What changed:**
- Extended `pcsx2/arm64/aR5900Arith.cpp` with 14 R-type generators:
  - `armEmitADD`/`ADDU` — 32-bit add of GPR[rs]+GPR[rt], `Sxtw` sign-extend.
  - `armEmitSUB`/`SUBU` — 32-bit sub, `rs==rt` zero fast-path, `Sxtw`.
  - `armEmitDADD`/`DADDU` — 64-bit add (no sign-extend needed).
  - `armEmitDSUB`/`DSUBU` — 64-bit sub, `rs==rt` zero fast-path.
  - `armEmitAND`/`OR`/`XOR`/`NOR` — 64-bit logical; `rs==rt` fast paths
    (AND/OR identity, XOR zero, NOR not).
  - `armEmitSLT` — signed 64-bit `Cmp + Cset(lt)`.
  - `armEmitSLTU` — unsigned 64-bit `Cmp + Cset(lo)`.
  - All skip the store for `rd == 0` ($zero discard).
- `pcsx2/arm64/aR5900.h` — declarations for all 14 R-type generators.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` gains a `SPECIAL` (opcode 0x00)
  inner switch on the funct field (`rd`, `rs`, `rt`, `funct` now all decoded).
  Dispatches all 14 R-type ops plus keeps existing I-type + load/store.
  Also fixed a latent LUI bug: was passing `(op>>16)&0x1f` (only `rt` / 5 bits)
  instead of the full 16-bit `static_cast<u16>(op)`.
- `tests/ctest/core/arm64_emit_test.cpp` — 15 new `Arm64EmitEE.*` gtests:
  `ADD_SignExtend32Wrap`, `ADD_RSsameRT`, `ADD_DiscardZeroRd`, `ADDU_IsSameAsADD`,
  `DADD_64bitAdd`, `DADD_RSsameRT`, `SUB_SignExtend32Wrap`, `SUB_RSsameRT`,
  `SUBU_IsSameAsSUB`, `DSUB_64bitSub`, `DSUB_RSsameRT`, `AND_Standard`,
  `AND_RSsameRT`, `OR_Standard`, `OR_RSsameRT`, `XOR_Standard`, `XOR_RSsameRT`,
  `NOR_Standard`, `NOR_RSsameRT`, `SLT_SignedLess`, `SLT_SignedGreater`,
  `SLTU_UnsignedLess`, `SLTU_UnsignedGreaterWithSignBit`, `LUI_Full16BitImm`.
  Total `Arm64EmitEE.*` tests: 46; all pass (both `common_test` + `core_test`).
- Commits:
  - `46940afc9 ARM64: EE register-register arithmetic generators (Phase 3.2)`

**Decisions & rationale:**
- **Same mem-to-mem pattern as 3.1.** Load both sources from `[RESTATEPTR+off]`,
  compute in `RSCRATCHADDR` (x17) with `RXVIXLSCRATCH` (x16) as the second operand,
  store to `[RESTATEPTR+rd*16]`. No register allocator yet.
- **`armEmitXOR` zero-fast-path for `rs==rt`.** The x86 JIT does the same
  (`xXOR(EEREC_D, EEREC_D)`), relying on `rs==rt` being common for zeroing.
- **`ADD` and `SUB` sign-extend 32-bit results to 64.** Matches the x86
  `xMOVSX(xRegister64, xRegister32)` pattern; the EE defines the result of
  32-bit ops as sign-extended into the 64-bit GPR (even ADDU/SUBU, which differ
  only in that they don't trap on overflow).
- **`rs==rt` fast paths save a load.** For ADD/DADD/AND/OR/XOR the value is
  already in RSCRATCH; for SUB/DSUB we emit `Mov 0`; for NOR we `Orr` with
  itself then `Mvn`. These are small codegen wins that mirror the x86 JIT's
  special-case handling.
- **LUI bug fix is a correctness fix, not a refactor.** The old code
  `(op >> 16) & 0x1f` passed only the Rt field (bits 16-20), so e.g.
  `LUI $t0, 0x8000` became `LUI $t0, 0` — all high-immediate loads were broken.
  Caught when adding the `LUI_Full16BitImm` regression test.

**Blockers / open questions:**
- none. Phase 3.2 complete.

**Next step:** Phase 3.3 — EE shift ops
(`SLL/SRL/SRA/SLLV/SRLV/SRAV/DSLLV/DSRLV/DSRAV/DSLL/DSRL/DSRA/DSLL32/DSRL32/DSRA32`).
Mixed R-type and variant-width, immediate (`sa` field) and variable shifts.
These are simpler than arithmetic but need careful attention to 32- vs 64-bit,
and variable-shift amount masking (MIPS shifts are masked to 5/6 bits).

---

## 2026-06-04 — Phase 3.1: EE immediate arithmetic generators

**Goal:** Implement and unit-test the complete EE I-type immediate arithmetic
opcode family, giving `recTranslateOp` enough codegen to handle basic integer
ALU instructions.

**What changed:**
- New `pcsx2/arm64/aR5900Arith.cpp` — 10 immediate-opcode generators:
  - `armEmitADDI`/`ADDIU` — 32-bit add + `Sxtw` (same for both; overflows ignored).
  - `armEmitDADDI`/`DADDIU` — 64-bit add (same for both).
  - `armEmitSLTI` — 64-bit signed `Cmp + Cset(lt)`.
  - `armEmitSLTIU` — 64-bit unsigned `Cmp + Cset(lo)`.
  - `armEmitANDI`/`ORI`/`XORI` — 64-bit logical with zero-extended 16-bit imm
    (materialized via `RXVIXLSCRATCH`; includes imm==0 fast-path).
  - `armEmitLUI` — sign-extended `imm << 16` via `Mov` + `Sxtw`.
  - All skip the store for `rt == 0` ($zero discard).
- `pcsx2/arm64/aR5900.h` — declarations for all 10 generators.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` dispatches by primary opcode:
  0x08 ADDI, 0x09 ADDIU, 0x0A SLTI, 0x0B SLTIU, 0x0C ANDI, 0x0D ORI,
  0x0E XORI, 0x0F LUI, 0x18 DADDI, 0x19 DADDIU.
- `tests/ctest/core/arm64_emit_test.cpp` — 15 new `Arm64EmitEE.*` gtests:
  `ADDI_SignExtend32`, `ADDI_ZeroImmIdentity`, `ADDI_DiscardZero`,
  `DADDI_64bitAdd`, `DADDI_NegativeImm`, `SLTI_SignedLessThan`,
  `SLTIU_UnsignedLessThan`, `ANDI_ZeroExtended`, `ANDI_ZeroImm`,
  `ORI_ZeroExtended`, `ORI_ZeroImmIdentity`, `XORI_ZeroExtended`,
  `LUI_SignExtend`, `LUI_PositiveImm`, `LUI_ZeroImm`.
  All 31 `Arm64EmitEE.*` tests pass (16 existing + 15 new).
- `pcsx2/CMakeLists.txt` — added `arm64/aR5900Arith.cpp` to `pcsx2arm64Sources`.
- Commits:
  - `a4f4c8e80 ARM64: EE immediate arithmetic generators (Phase 3.1)`

**Decisions & rationale:**
- **Mem-to-mem pattern, same as load/store generators.** Load GPR[rs] from
  `RESTATEPTR + offset`, compute in `RSCRATCHADDR` (x17, caller-saved scratch),
  store back. No reg allocator yet. keps Phase 3.1 self-contained and directly
  testable with the `RunEEGen` harness from Phase 2.4.
- **ADDI ≡ ADDIU in the JIT.** The interpreter traps on 32-bit overflow for ADDI;
  the x86 JIT also skips that check and just does `ADD` + `MOVSX`. Matching x86
  keeps us compatible with games that don't rely on the (never-handled) overflow
  trap.
- **Logical-immediate materialization through `RXVIXLSCRATCH`.** The 16-bit
  zero-extended immediates for ANDI/ORI/XORI are not guaranteed to be valid ARM64
  logical immediates (the encoding is an 8/64-bit pattern with rotation). Using
  `Mov(reg, imm)` lets VIXL handle any value, then `And/Orr/Eor` with two regs.
  The imm==0/imm==0xFFFF fast-paths avoid a pointless instruction.
- **LUI is `Mov(w, imm<<16)` then `Sxtw`, not `Mov(x, sign-extended32)`** —
  VIXL's `Mov(x, s32)` might have different selection rules. Explicit `Mov` on W
  then `Sxtw` exactly matches the `cpuRegs.code << 16` as 32-bit then sign-extend
  semantics.

**Blockers / open questions:**
- none. Phase 3.1 complete.

**Next step:** Phase 3.2 — EE register-register arithmetic ops
(`ADD/ADDU/SUB/SUBU/SLT/SLTU/AND/OR/XOR/NOR/DADD/DADDU/DSUB/DSUBU`) in a new or
extended `aR5900Arith.cpp`. Same mem-to-mem pattern but read two source GPRs
instead of one + imm.

---

## 2026-06-04 — Phase 2.4: full guest-memory round-trip validation

**Goal:** Prove the scalar + quad load/store generators actually read/write the
right guest bytes at runtime — not just compute the right address — closing
Phase 2.4 without waiting on the Phase 4 dispatcher.

**What changed:**
- `tests/ctest/core/arm64_emit_test.cpp` — 6 new `Arm64EmitEE.*` round-trip gtests
  + the harness behind them:
  - `VtlbMapping` RAII: allocates a full 4 GB/4 KB `vtlbdata.vmap` (8 MB) and writes
    direct-pointer entries (`VTLBVirtual::fromPointer`) mapping a local host buffer
    at guest `0x0010_0000`; restores the prior `vmap` on destruction.
  - `GuestRegs` (32×16-byte cpuRegs-shaped file) + `RunEEGen()` which emits
    `void f(void* regfile)` with a prologue saving `RESTATEPTR`(x19)+LR, points
    `RESTATEPTR` at the regfile, runs the generator, restores, and calls it.
  - Tests: `StoreThenLoadWord`, `LoadByteSignAndZeroExtend` (sign vs zero),
    `StoreLoadDoubleword`, `LoadStoreWritesToZeroRegDiscarded` ($zero stays 0),
    `StoreThenLoadQuad`, `QuadAccessForcesAlignment` (0x37 → aligns to 0x30).
  - 13 `Arm64EmitEE.*` total, all pass; both ctest suites green.
- Commits:
  - `32c331a6d ARM64: Guest-memory round-trip tests for load/store family (Phase 2.4)`

**Decisions & rationale:**
- **Hand-built direct-pointer `vmap` entry, NOT full vtlb init.** With the default
  `EmuConfig` (`EnableEE=true` ⇒ `CHECK_EEREC` true, `CHECK_CACHE` false) both
  `vtlb_memRead<T>`/`vtlb_memWrite<T>` and the quad variants reduce to
  `*reinterpret_cast<T*>(vmap[addr>>12].assumePtr(addr))` — no handler dispatch,
  no `CheckCache`. So mapping a buffer needs only one `vmap` entry per page
  (`fromPointer(host - vaddr)`); `vtlb_Core_Alloc` (SysMemory reservation, fastmem
  area, page-fault handler) and `vtlb_VMapBuffer` (CHECK_FASTMEM branch) are all
  avoidable. This is the "option (a), standalone vtlb" the last two entries flagged
  — confirmed feasible and far smaller than booting the VM.
- **`armEmitCall` is pool-safe with `nullptr`.** When the pool is null (or target is
  >±128 MB) it falls back to `Mov(x16,target); Blr(x16)`, so the test's
  `armSetAsmPtr(..., nullptr)` handles the real `vtlb_memRead/Write` calls fine.
- **Verified the marshalling that addr-only tests couldn't:** sub-word sign/zero
  extension into the 64-bit GPR, 64-bit data through `RXARG2`, the 128-bit q0
  in/out for quad, and the `& ~0xF` quad align — all against ground-truth bytes.

**Blockers / open questions:**
- none. Phase 2 slow-path load/store is functionally complete and proven. (Fastmem
  fast path 2.2 still deferred — optimisation, not correctness.)

**Next step:** Phase 3.1 — EE immediate integer ops (`ADDI/ADDIU/SLTI/SLTIU/ANDI/
ORI/XORI/LUI/DADDI/DADDIU`): add generators in a new `aR5900Arith.cpp` (or extend
the load/store TU), dispatch from `recTranslateOp`, unit-test the math directly
(no vtlb needed). Ref x86 `iR5900Arit.cpp`/`iR5900AritImm.cpp`. Alternatively the
deferred unaligned `LWL/LWR/LDL/LDR`+`SWL/...` family (byte-merge codegen) — but
Phase 3 is higher-leverage toward a runnable block.

---

## 2026-06-04 — Phase 2.3 complete: full scalar + quad load/store family

**Goal:** Finish the EE aligned load/store family — wire the rest of the scalar
ops into `recTranslateOp` and add the 128-bit `LQ`/`SQ` path — completing Phase 2.3.

**What changed:**
- `pcsx2/arm64/aR5900LoadStore.cpp` + `aR5900.h` — new quad generators:
  - `armEmitLoadQuad(rt, rs, imm)` — effective addr → `And ~0x0F` (16-byte
    align) → `armEmitVtlbReadQuad(RQSCRATCH, …)` → `Str q30, [RESTATEPTR+rt*16]`
    (writes the full 128-bit reg; skipped for `rt==0`, load still runs for side
    effects).
  - `armEmitStoreQuad(rt, rs, imm)` — `Ldr q30, [RESTATEPTR+rt*16]` → effective
    addr → `And ~0x0F` → `armEmitVtlbWriteQuad`. `rt==0` reads zero from cpuRegs,
    no special case.
- `pcsx2/arm64/aR5900.cpp` — `recTranslateOp` now dispatches the whole aligned
  family by primary opcode: `LB`(0x20)/`LBU`(0x24)/`LH`(0x21)/`LHU`(0x25)/
  `LW`(0x23)/`LWU`(0x27)/`LD`(0x37) → `armEmitLoadGpr(bits,sign,…)`;
  `SB`(0x28)/`SH`(0x29)/`SW`(0x2b)/`SD`(0x3f) → `armEmitStoreGpr`; `LQ`(0x1e)/
  `SQ`(0x1f) → the new quad generators. Opcode numbers cross-checked against
  `R5900OpcodeTables.cpp` + the canonical EE primary-opcode map.
- `tests/ctest/core/arm64_emit_test.cpp` — 2 new gtests `Arm64EmitEE.QuadAddr*`
  proving the `& ~0x0F` alignment mask at runtime (mirrors the address sequence
  the quad generators emit). 7 `Arm64EmitEE.*` total, all pass; both ctest
  suites green.
- Commits:
  - `ARM64: Complete scalar + quad load/store family (Phase 2.3)`

**Decisions & rationale:**
- **`LWU` = `armEmitLoadGpr(32, false, …)` (zero-extend), `LD`/`SD` = 64-bit.**
  The existing `bits`/`sign` params already cover the whole scalar family, so the
  rest of 2.3 was pure dispatch wiring — no new generator logic, exactly as the
  prior journal predicted.
- **Quad align mirrors x86 `recLoadQuad`/`recStore` `xAND(arg1regd, ~0x0F)`.** EE
  silently aligns 128-bit accesses; the mask is correctness, applied after the
  effective-address add and before the access.
- **Quad uses `RQSCRATCH` (q30) for the 128-bit GPR transfer.** ReadQuad does its
  `Mov(dst, q0)` *after* the helper call, and WriteQuad moves `data`→q0 *before*
  the call, so q30 is never live across the clobbering call in either direction.
- **Unaligned LWL/LWR/LDL/LDR + SWL/SWR/SDL/SDR deferred.** They need byte-offset
  masking/merge codegen (x86 `recLWL` shifts + OR), not just dispatch — a separate
  chunk best done after the round-trip harness exists to validate the merge.
- **Still unit-test only the address/align codegen.** The full
  load/store/quad round-trip calls the vtlb helpers, which need a live VM — same
  constraint as before; deferred to 2.4. The vtlb call layer is already proven
  (2.1 helpers + `armEmitCall`), so dispatch wiring is compile+addr-test validated.

**Blockers / open questions:**
- **Phase 2.4 (full memory round-trip) still needs an execution context** — a
  standalone vtlb-init harness (option a) or the Phase 4 dispatcher (option b).
  Decide when picking up 2.4; (a) is the smaller ground-truth step if vtlb can be
  initialized standalone — worth a look first.

**Next step:** Phase 2.4 — full guest-memory round-trip validation of the scalar +
quad generators (vtlb-init gtest harness against real RAM, or wait for the Phase 4
dispatcher). After that: deferred unaligned LWL/LWR/… family, or Phase 3 (EE int).

---

## 2026-06-04 — Phase 2.3: EE GPR load/store generators + LW/SW decode

**Goal:** First vertical slice of EE load/store codegen — turn decoded MIPS
load/store fields into ARM64 that computes the EE address mode, accesses guest
GPRs, and routes memory through the Phase 2.1 slow path; wire `LW`/`SW` into the
block decode loop.

**What changed:**
- `pcsx2/arm64/aR5900LoadStore.cpp` + `aR5900.h` — new generators:
  - `armEmitEffectiveAddr(dst, rs, imm)` → `dst.W() = GPR[rs].UL[0] + imm`
    (loads the low word from `[RESTATEPTR + rs*16]`, adds the sign-extended imm;
    `rs==0` → just `Mov dst, imm`). `EE_GPR_OFFSET(n) = n*16` with static_asserts
    on `sizeof(GPR_reg)==16` and `offsetof(cpuRegisters,GPR)==0`.
  - `armEmitLoadGpr(bits, sign, rt, rs, imm)` — addr→`RWARG1`, `armEmitVtlbRead`,
    then `Str RXRET, [RESTATEPTR + rt*16]` (writes the full extended 64-bit result
    to `GPR[rt].UD[0]`, upper dword untouched; skipped for `rt==0`, but the load
    still runs for I/O side effects).
  - `armEmitStoreGpr(bits, rt, rs, imm)` — load `GPR[rt]` low bits into `RWARG2/
    RXARG2`, addr→`RWARG1`, `armEmitVtlbWrite`.
- `pcsx2/arm64/aR5900.cpp` — `recCompileBlock` now `memRead32(cpuRegs.pc)` →
  `cpuRegs.code` → `recTranslateOp`: decodes `opcode/rs/rt/imm`, dispatches
  `LW`(0x23)→`armEmitLoadGpr(32,true,…)`, `SW`(0x2b)→`armEmitStoreGpr(32,…)`, else
  NOP. Replaced the fixed NOP/NOP body.
- `tests/ctest/core/arm64_emit_test.cpp` — 5 new gtests `Arm64EmitEE.*` exercising
  `armEmitEffectiveAddr` at runtime (base+imm, zero imm, negative imm, `$zero`,
  low-word-only). All pass.
- Commits:
  - `ARM64: Add EE GPR load/store generators + addr-calc test (Phase 2.3)`
  - `ARM64: Single-instruction MIPS decode + LW/SW dispatch (Phase 2.3)`

**Decisions & rationale:**
- **Explicit (rs,rt,imm) params, not the `_Rs_`/`_Rt_` macros, on the generators.**
  Keeps them decode-agnostic and unit-testable without a live `cpuRegs.code`; the
  decode loop reads the macros' worth of fields and passes them in. Mirrors how the
  x86 `recLoad/recStore` factor address vs. value, minus the register allocator.
- **Loads write the full 64-bit extended result to `UD[0]` only.** Matches the EE
  interpreter (`LW: GPR[rt].SD[0] = (s32)temp`) — scalar loads define just the low
  doubleword of the 128-bit reg; the extend happens inside `armEmitVtlbRead`.
- **Unit-test only the address-mode codegen.** That is the genuinely-new, vtlb-free
  part of 2.3. The full `armEmitLoadGpr/StoreGpr` round-trip needs a live vtlb
  (real RAM mapping), which a standalone gtest doesn't have — deferred to 2.4. The
  vtlb call layer itself is already proven (Phase 2.1 helpers + `armEmitCall`).
- **`recCompileBlock` decode is intentionally inert.** `recExecute` is still never
  entered (interpreter is the active provider) and the block doesn't set up
  `RESTATEPTR` / PC / cycles — that enter-trampoline + dispatcher is Phase 4. This
  is groundwork validated by compile-clean + the generator unit tests, exactly the
  cadence Phase 1.4 used for the original block loop.

**Blockers / open questions:**
- **Phase 2.4 (full memory round-trip) needs an execution context.** Two options:
  (a) a lightweight vtlb-init test harness so a gtest can `armEmitLoadGpr/StoreGpr`
  against real RAM, or (b) wait for the Phase 4 dispatcher to enter `recExecute`
  for real. Decide when picking up 2.4. (a) is the smaller, ground-truth-friendly
  step if vtlb can be initialized standalone — worth a look first.

**Next step:** Finish the scalar load/store family in `recTranslateOp`
(`LB/LBU/LH/LHU/LWU/LD`, `SB/SH/SD` — dispatch wiring over the existing `bits`/
`sign` params), then `LQ/SQ` via the Quad helpers (+ `~0xF` align, 128-bit GPR
access), then Phase 2.4 end-to-end validation.

---

## 2026-06-04 — Phase 2.1: slow-path vtlb load/store codegen (+ re-scope)

**Goal:** Stand up the EE rec's memory-access codegen — the gateway every
load/store opcode needs — using the simplest correct path before any reg
allocator exists.

**What changed:**
- New `pcsx2/arm64/aR5900LoadStore.cpp` — slow-path generators:
  - `armEmitVtlbRead(bits, sign, dst, addr)` — move addr→`RWARG1`, `armEmitCall`
    `vtlb_memRead<T>`, then extend the result into the 64-bit `dst` (Sxtb/Uxtb/
    Sxth/Uxth/Sxtw/Mov per size+sign).
  - `armEmitVtlbWrite(bits, addr, data)` — stage data via the VIXL scratch to dodge
    arg-reg aliasing, set `RWARG1`=addr / `RxARG2`=data, call `vtlb_memWrite<T>`.
  - `armEmitVtlbReadQuad/WriteQuad` — 128-bit via `vtlb_memRead128`/`vtlb_memWrite128`
    (r128 = `uint32x4_t` in `q0`).
- `aR5900.h` — declarations + the clobber/ABI contract comment.
- `pcsx2/CMakeLists.txt` — added the new source.
- Commits: <code> ARM64: Add slow-path vtlb load/store codegen (Phase 2.1)

**Decisions & rationale:**
- **Call the C++ `vtlb_memRead/Write` helpers directly instead of porting the x86
  inline-vtlb sequence + indirect dispatchers.** Those helpers ARE the interpreter's
  memory path, so the JIT output is correct by construction (ground truth), and they
  need no register allocator, no `vtlbdata.vmap` codegen, no dispatcher trampolines.
  Perfect for bring-up; the inline-fastmem version is a later optimisation (2.2).
- **Sub-word loads MUST be explicitly extended.** AAPCS64 leaves the high bits of a
  `u8`/`u16`/`u32` return undefined, and EE loads write the full 64-bit GPR
  (sign- or zero-extended). So the extend is correctness, not perf.
- **RE-SCOPED Phase 2.1.** The previous focus said "implement
  `vtlb_DynBackpatchLoadStore`, slow path first". But that function is invoked
  *only* from the SIGSEGV handler (`vtlb.cpp:1514 → vtlb_BackpatchLoadStore →
  vtlb_DynBackpatchLoadStore`) and *only* under `CHECK_FASTMEM`. Under "slow path
  first / no fastmem" it is never reached — implementing it now would be untested
  dead code, violating the interpreter-is-ground-truth discipline. The substance the
  note wanted ("call the vtlb handler directly") is exactly the slow-path generators,
  which is what I built. Backpatch + fastmem moved to Phase 2.2 (stub untouched).
- **Helpers take explicit VIXL regs** (not the x86 reg-allocator interface) so they
  compose cleanly once the EE decode loop + GPR access land in 2.3.

**Blockers / open questions:**
- No runtime test yet: exercising these needs the EE rec to actually decode a MIPS
  load/store and access `cpuRegs` (none of that exists — `recCompileBlock` still
  emits NOP/NOP/RET). First real validation comes with Phase 2.3/2.4 via BIOS boot.
  The emit primitives used (`armEmitCall`, Mov/Sxt*) are individually proven by the
  Phase 0.6 harness + the VIF dynarec, so compile-clean is acceptable here.

**Next step:** Phase 2.3 groundwork — give `recCompileBlock` a minimal single-MIPS
decode + guest-GPR access (load base from `[RESTATEPTR + GPRoff]`, add sign-extended
imm, call `armEmitVtlbRead/Write`, store result back to `cpuRegs`), land `LW`/`SW`,
and validate end-to-end (2.4). Ref x86 `recRecompile` + `iR5900LoadStore.cpp`.

---

## 2026-06-04 — Phase 1.5: wire recCpu into VMManager + first real BIOS-boot verification

**Goal:** Let the ARM64 build actually call the EE rec's Reserve/Reset/Shutdown on
VM lifecycle, without selecting it as the active Cpu provider, and confirm a real
BIOS boot survives it.

**What changed:**
- `pcsx2/VMManager.cpp` — added `#else` (ARM64) branches to three `_M_X86` guards:
  - `InitializeCPUProviders()` (~2677): `recCpu.Reserve();` before `vu1Thread.Open()`.
  - `ShutdownCPUProviders()` (~2701): `recCpu.Shutdown();` before the MTVU wait.
  - `ClearCPUExecutionCaches()` (~2740): `recCpu.Reset();` (the `Cpu->Reset()` above
    only hits the interpreter on ARM64, so the rec needs an explicit reset).
  - `UpdateCPUImplementations()` deliberately untouched: `Cpu = &intCpu;` stays.
- Commits: af7ddc180 ARM64: Wire EE recCpu into VMManager (Phase 1.5)

**Decisions & rationale:**
- **Reserve/Reset/Shutdown only — NOT provider selection.** `recCpu` is now set up
  and torn down on the real VM lifecycle (code cache carved, const pool init'd,
  cursor reset), but `Cpu` still points at `intCpu`. The interpreter remains ground
  truth; `recExecute` is never entered (would `pxFailRel` — it's still a stub). This
  is the safe "rec is alive but inert" milestone before any guest codegen.
- **`recCpu.Reset()` added explicitly in ClearCPUExecutionCaches**, mirroring how
  x86 gets it for free via `Cpu->Reset()` when `Cpu == &recCpu`. Since we keep the
  interpreter selected, the only way recResetEE runs each VM reset is a direct call.

**First real runtime verification (new for this project):**
- Booted the PS2 BIOS (`-bios`, no disc) and confirmed via `emulog.txt`:
  `VM subsystems initialized in 163ms` (→ Reserve+Reset ran), ELF load, `Mode
  Changed to DVD PAL`, `Pad: DS2 Config Finished` — the interpreter ran BIOS code
  for ~24s with the EE rec reserved-but-unselected, **no crash / assert / pxFailRel**.

**Blockers / open questions — IMPORTANT for all future phases:**
- **Running the dev binary directly is broken by a Qt double-load**, NOT by our
  code: the main binary links Qt by absolute `/pcsx2-deps/lib/libQt6*.dylib` install
  names, while the bundled `PlugIns/platforms/libqcocoa.dylib` loads the
  `@executable_path/../Frameworks/libQt6*` copies → two Qt sets → cocoa platform
  plugin aborts (SIGABRT 134) at GUI init, before any VM code runs. There is no
  macdeployqt/install_name_tool fixup target in CMake.
- **Workaround that WORKS (used to verify this phase):** copy the .app, then
  `install_name_tool -change /…/pcsx2-deps/lib/<lib> @executable_path/../Frameworks/<lib>`
  for `libQt6Core.6 / libQt6Gui.6 / libQt6Widgets.6 / libkddockwidgets-qt6.3`, then
  `codesign --force --deep --sign - <app>`, then run `<app>/Contents/MacOS/PCSX2`.
  (Boot headless with `gtimeout 25 … -bios`; `timeout` isn't on macOS — use
  `gtimeout` from coreutils.) Phases 2+ need real execution to validate, so this
  run recipe is now essential. Consider scripting it or adding a proper deploy step.

**Next step:** Phase 2.1 — implement `vtlb_DynBackpatchLoadStore` in
`pcsx2/arm64/RecStubs.cpp` (currently `pxFailRel`). Slow path first (call the vtlb
memory handler directly, no fastmem backpatch yet); reference
`x86/ix86-32/recVTLB.cpp` for the trampoline/calling-convention design.

---

## 2026-06-04 — Phase 1.4: minimal EE block compile loop

**Goal:** Exercise the production emission lifecycle end-to-end against the real EE
code cache — emit a block, enter it, return — before any real guest codegen.

**What changed:**
- `pcsx2/arm64/aR5900.cpp`:
  - Added `recCompileBlock()`: resets the cache if `recPtr >= recPtrEnd`, then
    `armSetAsmPtr(recPtr, recPtrEnd-recPtr, &s_const_pool)` → `armStartBlock()` →
    emit `Nop/Nop/Ret` placeholder → `recPtr = armEndBlock()`; returns the entry ptr.
  - Rewrote `recExecute()` (was `pxFailRel`): compiles one block via
    `recCompileBlock()`, casts the entry to `void(*)()`, calls it, returns.
- Commits: a269894be ARM64: Minimal EE block compile loop (Phase 1.4)

**Decisions & rationale:**
- **Followed the VIF dynarec lifecycle verbatim** (`Vif_Dynarec.cpp:487-496`):
  `armSetAsmPtr` → `armStartBlock` (returns fn ptr) → emit → `armAsm->Ret()` →
  `armEndBlock` (returns advanced write ptr). `armStartBlock`/`armEndBlock` already
  do the Apple-Silicon-critical `BeginCodeWrite`/`EndCodeWrite` (JIT W^X toggle) +
  `FlushInstructionCache`, so the block is immediately callable.
- **Deliberately NOT the x86 dispatcher/LUT machinery.** x86's `recExecute` enters
  a generated `EnterRecompiledCode` trampoline and `fastjmp`s back out, driving a
  block-LUT dispatcher loop (`iR5900.cpp:374-496,715`). That (block map, linking,
  `recEventTest`, exit via fastjmp) is Phase 4. Phase 1.4 runs exactly one empty
  block and returns via a plain `RET` — the smallest honest proof of emit+enter.
- **`recExecute` is dead code in normal operation right now** and that's intended:
  `recCpu` is defined but interpreter stays the active `Cpu` provider until Phase
  1.5 wires VMManager (and even then `Cpu = &intCpu` holds until real codegen). So
  this path isn't hit on BIOS boot yet; it's validated by compilation + the fact
  the emit lifecycle is identical to the proven VIF/scratch-harness path.

**Blockers / open questions:**
- Top-level `ctest --test-dir build` reports "No tests were found" — tests are
  registered under `build/tests/ctest/`. Correct invocation:
  `ctest --test-dir build/tests/ctest` (both `common_test` + `core_test` pass).
  CLAUDE.md/CONVENTIONS say `ctest --test-dir build`; note the discrepancy.

**Next step:** Phase 1.5 — wire `recCpu` into `VMManager.cpp` (extend the `_M_X86`
guards at ~2671/2695/2720/2740 so ARM64 calls Reserve/Reset/Shutdown). Keep
`Cpu = &intCpu;` — do NOT select `&recCpu` yet. Confirm Reserve/Reset run on VM
startup without crashing.

---

## 2026-06-04 — Phase 1.3: EE code-cache reservation + constant pool

**Goal:** Give the EE rec a real code buffer and constant pool so Phase 1.4 can
emit blocks. No guest code compiled yet.

**What changed:**
- `pcsx2/arm64/aR5900.cpp`:
  - Added file-scope `recPtr`/`recPtrEnd` cursor + `static ArmConstantPool s_const_pool`.
  - `recReserve()`: `recPtr = SysMemory::GetEERec()`, `recPtrEnd = GetEERecEnd() -
    EE_CONSTPOOL_SIZE`, `s_const_pool.Init(recPtrEnd, EE_CONSTPOOL_SIZE)`.
  - `recShutdown()`: `s_const_pool.Destroy()` + null the cursors.
  - `recResetEE()`: rewind `recPtr` to `GetEERec()`, `s_const_pool.Reset()`.
  - `EE_CONSTPOOL_SIZE = 1 MB` reserved at the tail of the 64 MB EE rec region.
  - Added `#include "Memory.h"` + `"common/Pcsx2Defs.h"`.
- Commits: 7f16e2cde ARM64: Reserve EE code cache + constant pool (Phase 1.3)

**Decisions & rationale:**
- **Do NOT allocate the code cache ourselves — SysMemory already reserves it.** The
  64 MB EE rec region (`HostMemoryMap::EErec*`) is mapped at startup; x86's
  `recReserve` likewise just takes `GetEERec()`/`GetEERecEnd()`. So this is a
  carve, not an `HostSys::Alloc`. (PROGRESS.md said "via HostSys" — corrected: it's
  via `SysMemory::GetEERec()`, same as the VIF dynarec uses `GetVIFUnpackRec()`.)
- **Constant pool lives in a 1 MB tail of the EE region.** ARM64 needs PC-relative
  far-jump trampolines + 64/128-bit literals (x86 inlines immediates and has no
  pool). Carving the tail keeps pool literals within ±128 MB of all emitted code
  (the region is 64 MB, well within ADR/LDR-literal range) without a second
  allocation. 1 MB is a provisional size; revisit if trampolines/literals overflow.
- **Skipped the x86 LUT/block-map machinery (`recRAMCopy`, `recLutReserve_RAM`,
  dispatchers).** That belongs to Phase 1.4/4.x (block compile + linking +
  invalidation), not to cache reservation. Kept 1.3 to exactly "buffer + pool".
- **`recPtr` is written but not yet read** (only `recReserve`/`recResetEE` set it);
  no unused-variable warning for file-scope statics. First reader is Phase 1.4.

**Blockers / open questions:**
- none. Builds, links, binary confirmed arm64, both unittests pass.

**Next step:** Phase 1.4 — minimal block compile loop: `armSetAsmPtr(recPtr,
recPtrEnd-recPtr, &s_const_pool)` → `armStartBlock()` → emit NOP(s) → `armEndBlock()`
→ advance `recPtr` (reset cache if `>= recPtrEnd`); then have `recExecute()` enter
emitted code and return (ref x86 `_DynGen_EnterRecompiledCode` + `recRecompile`).

---

## 2026-06-04 — Phase 1.1/1.2: EE recompiler skeleton (recCpu)

**Goal:** Stand up the ARM64 EE recompiler translation unit so `recCpu` is defined
and links, giving later phases a place to add codegen. No guest code compiled yet.

**What changed:**
- New `pcsx2/arm64/aR5900.h` — register-allocation contract: `RESTATEPTR`=x19
  (`&cpuRegs`), `REFASTMEMBASE`=x20 (fastmem base), `REVTLBPTR`=x21 (vtlb base,
  wired in Phase 2). Callee-saved (x19-x28) so they survive C ABI calls.
- New `pcsx2/arm64/aR5900.cpp` — defines the `R5900cpu recCpu` provider (matches
  R5900.h's `extern`). All entry points are stubs: `recExecute` is `pxFailRel`
  (loud if ever reached before it's real), the rest are no-ops with phase-tagged
  TODOs. `recClear` kept `static` (no external refs yet) to dodge -Wmissing-prototypes.
- `pcsx2/CMakeLists.txt` — added both files to `pcsx2arm64Sources`/`pcsx2arm64Headers`.
- Commits: <filled at commit>

**Decisions & rationale:**
- **Skeleton defines `recCpu` but does NOT wire it into VMManager yet.** On ARM64,
  `recCpu` is currently referenced only inside `#ifdef _M_X86` blocks in
  VMManager.cpp (2672/2700/2721), so it was never linked. Defining it now is
  inert/safe; flipping VMManager to actually call Reserve/Reset/Shutdown is Phase
  1.5, and switching `Cpu` to `&recCpu` waits until codegen works (interpreter
  stays ground truth). Kept the change minimal + non-behavioral on purpose.
- **Trimmed the header to just the reg map.** Dropped speculative `extern pc/
  g_branch/target` and `recClear` decls — nothing on ARM64 references them yet, and
  `pc`/`target` are too generic to export prematurely. Add them in Phase 1.4 when
  the compile loop needs them.

**Blockers / open questions:**
- none. Builds, links, binary confirmed arm64, both unittests pass.

**Next step:** Phase 1.3 — implement `recReserve`/`recShutdown`: allocate the EE
code cache (`HostSys`/`SysMemory`, ref `recReserveRAM` in x86 and how
`Vif_Dynarec.cpp` uses `SysMemory::GetVIFUnpackRec()`) and init `ArmConstantPool`.

---

## 2026-06-04 — Phase 0.5/0.6: VIXL emit+execute scratch harness

**Goal:** Prove the ARM64 JIT toolchain end-to-end (emit + execute) before writing
any recompiler, and internalize the existing `arm64/AsmHelpers` emission lifecycle.

**What changed:**
- New `tests/ctest/core/arm64_emit_test.cpp` — gtest suite `Arm64Emit.*` (3 tests):
  `AddTwoArgs` (w0+w1), `ReturnConstant64` (64-bit imm materialization),
  `LoadStoreThroughPointer` (str through x0). Each mmaps a `MAP_JIT` buffer and
  runs it through the *real* emit path: `armSetAsmPtr` → `armStartBlock` →
  `armAsm->...` → `armEndBlock`, then calls the emitted function.
- `tests/ctest/core/CMakeLists.txt` — adds the file to `core_test` under
  `if(ARCH_ARM64)`.
- Commit: 4a90f609d ARM64: Add VIXL emit+execute scratch harness (Phase 0.6)

**Decisions & rationale:**
- **Reused the production lifecycle, not a one-off mmap+memcpy.** `armStartBlock`/
  `armEndBlock` already do the Apple-Silicon-critical bits: `BeginCodeWrite`/
  `EndCodeWrite` (`pthread_jit_write_protect_np` toggle) and
  `FlushInstructionCache`. Testing through them validates exactly what the EE rec
  will rely on, and confirms `armAsm` (thread_local) + scratch-reg setup works.
- **Guard with `__aarch64__`, gate the source in CMake with `ARCH_ARM64`.** Key
  finding: `_M_ARM64` is **not** defined on this clang/macOS build — cmake only adds
  `_M_X86=1` for x86 (`BuildParameters.cmake:86`); `ARCH_ARM64` comes from
  `Pcsx2Defs.h` via `__aarch64__`. CLAUDE.md's `#ifdef _M_ARM64` advice is therefore
  wrong for this toolchain; use `ARCH_ARM64` / `#ifndef _M_X86` for guards instead.
- **MAP_JIT mmap mirrors `SharedMemoryMappingArea::Create(.., jit=true)`** in
  `LnxHostSys.cpp` (which is the POSIX/macOS path). Works in the unsigned test
  binary (no hardened runtime → no JIT entitlement needed).

**Blockers / open questions:**
- none. Toolchain confirmed. `core_test` + `common_test` both green via `unittests`.

**Next step:** Phase 1.1 — create `pcsx2/arm64/aR5900.h` (recCpu extern, reg-alloc
structs, REC_FUNC macros) mirroring `pcsx2/x86/iR5900.h`; then 1.2 empty `aR5900.cpp`
added to `pcsx2arm64Sources`, confirm it still builds.

---

## 2026-06-04 — Agentic workflow setup + gap verification

**Goal:** Make the project resumable across fresh Claude Code sessions, and verify
precisely what recompiler code is missing before any implementation.

**What changed:**
- Added `CLAUDE.md` (root) as the auto-loaded entry point + resume protocol.
- Created `arm64-port/` workflow: `PROGRESS.md` (roadmap), `JOURNAL.md` (this log),
  `CONVENTIONS.md` (technical contract).
- Moved reference docs into `arm64-port/reference/` (`apple-silicon-analysis.md`,
  `ARM64_RECOMPILER_PLAN.md`) via `git mv` (history preserved).
- Commits: <to be filled when committed>

**Decisions & rationale:**
- **File-based tracking, not Claude's private memory.** The trackers must be in the
  repo so they're portable, committed, and visible to any fresh session/machine.
- **Three-file model** (PROGRESS = what's next, JOURNAL = recent context,
  CONVENTIONS = how) keeps each file single-purpose and small enough to read fast.
- **Verified gap (the key technical finding):** Searched all branches/history.
  `pcsx2/arm64/` has only ever received two commits — `fe9399612` (VIF dynarec) and
  `8a18403fe` (EE/VU/IOP *stubs*). There is **no dormant EE/IOP/VU recompiler** to
  revive anywhere in this repo. This is a genuine from-scratch port (~35.8k lines of
  x86 rec to reimplement; ARM64 currently 1.8k lines = VIF + AsmHelpers + stubs).
  Corrected an earlier mis-statement that upstream had substantial ARM64 rec code.
- Confirmed exact fallback/stub locations: `VMManager.cpp:2721-2731` (interpreter
  forced under `#else` of `#ifdef _M_X86`); `arm64/RecStubs.cpp:11-14`
  (`vtlb_DynBackpatchLoadStore` = `pxFailRel`); `RecStubs.cpp:16-30` (`vuJITFreeze`
  hack). Build is confirmed native arm64; deps at
  `/Users/isztld/Documents/projects/pcsx2/pcsx2-deps`; build dir configured.

**Blockers / open questions:**
- Optional, unresolved: whether an external ARM64 fork (Android / community macOS)
  exists that's worth adapting instead of pure from-scratch. Not investigated yet —
  would need a web search, not git. Park until/unless we want to revisit strategy.

**Next step:** Phase 0.5–0.6 → study VIXL MacroAssembler API and build a scratch
harness that emits + executes ARM64 code, then begin Phase 1.1 (`aR5900.h` skeleton).
