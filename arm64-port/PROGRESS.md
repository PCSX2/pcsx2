# PROGRESS — ARM64 Recompiler Port

> Living roadmap. **This is the source of truth for "what's done" and "what's next."**
> Update it at the end of every session. Status legend:
> `[ ]` not started · `[~]` in progress · `[x]` done · `[!]` blocked (see JOURNAL).

---

## ▶ CURRENT FOCUS

**Phase 7 (VU recompilers / microVU) — task 7.4 part 1 (first-pass init helpers → `aVU.cpp`) DONE.**

`pcsx2/arm64/aVU.cpp` now holds the **emitter-free first-pass initialization** helpers from x86
`microVU_Compile.inl`: `startLoop` (per-loop IR reset + M/D/T-bit dev logging), `mVUinitConstValues`
(vi15 const propagation seed), and `mVUinitFirstPass` (block-start setup: pipeline-state load,
block-manager `add`, needExactMatch/blockType/flag reset). These make **zero VIXL/regAlloc calls**
(memset/memcpy + block-manager `add` only), so they ported verbatim modulo the 7.2a struct rename
(`mVUblock.x86ptrStart` → `codeStart`). `mVUinitFirstPass` takes the start-of-block host code pointer
(x86 `x86Ptr` → ARM64 `armGetCurrentCodePointer()`, passed by the emit driver later) and stashes it
in `mVUblock.codeStart`. `mVUcompileHelpersCheck` (never called) odr-uses all three so their bodies
compile now.

**Rationale for porting these now** (they were deferred in 7.3 as "tied to the block-emit
lifecycle"): they make no emitter calls, so bringing them as a standalone buildable slice respects
the tight build/test loop better than waiting to land the entire emit backend with `mVUcompile` at
once. The actual block-emit lifecycle they participate in (`armStartBlock`/`add`/`codeStart`) is
exercised when the 7.4 driver lands.

**Still NOT ported** (all emit-coupled → rest of 7.4 + the 7.5 emit backend): `mVUcompile` itself,
`mVUexecuteInstruction`/`doUpperOp`/`doLowerOp`/`doSwapOp`/`doIbit`, `mVUtestCycles`,
`mVUDoDBit`/`mVUDoTBit`, `mvuPreloadRegisters`, `handleBadOp`, `mVUdebugPrintBlocks`, and the
`mVUentryGet`/`mVUblockFetch` block-fetch entry points (still `pxFailRel` stubs).

**Verified:** `pcsx2-qt` builds arm64; unittests 2/2 (common + core). Pure infrastructure; microVU
stays **unselected** on ARM64 (VMManager pins `CpuIntVU0/1`). Commit `9b01b4ca2`.

**Next:** Task 7.4 part 2 — the **emit-coupled compile driver**. This is the big-bang slice:
`mVUcompile` won't link until its whole emit backend exists, so it must come up together with (or
just after) the 7.5 emit modules it calls — `mVUopU`/`mVUopL` + `microVU_Tables.inl` (Upper/Lower),
the flag pipeline (`mVUsetFlags`/`mVUsetFlagInfo`/`mVUsetupBranch`/`mVUdivSet`/`copyPLState`), the
program-exit + block-link emit (`mVUendProgram`/`mVUDTendProgram`/`normBranch`/`normJump`/`condBranch`
/`normBranchCompile`/`normJumpCompile`), XGKICK (`mVU_XGKICK_SYNC`/`mVU_XGKICK_DELAY`), and
`mVUbackupRegs`/`mVUrestoreRegs`/`mVUtestCycles`. Recommended order: stand up the Flags + Branch
(program-exit/endProgram) emit first against a minimal `mVUopU`/`mVUopL` (NOP/B only) so `mVUcompile`
can link and emit a trivial block; this is the first task to emit real per-block VIXL — own
`armStartBlock`/`armEndBlock` per block + the **icache flush before branching into freshly-emitted
code** (the two things 7.2d flagged) — then fill in the full opcode tables.

---

### Earlier focus (kept for context)

**Phase 7 (VU recompilers / microVU) — task 7.3 part 1 (pass-1 analysis → `aVU_Analyze.inl`) DONE.**

`pcsx2/arm64/aVU_Analyze.inl` is the **near-verbatim** ARM64 clone of x86
`microVU_Analyze.inl` (the pass-1 analysis: VF/VI read/write hazard tracking, stall
computation, FMAC/IALU/FDIV/EFU/MOVE/LQ-SQ/R-reg/flag/XGKICK/branch analyzers, the
branch-VI-delay + branch-in-delay-slot logic). It is **fully arch-neutral** — it touches only
the IR (`microOp`/`microIR`/`microVFreg`/`microVIreg`) and the 96-byte `microRegInfo`
pipeline-state key, makes **zero emitter calls**, and so ported unchanged.

The macros it needs come from the new **`pcsx2/arm64/aVU_Misc.h`** — the arch-neutral subset of
x86 `microVU_Misc.h`: instruction-field extractors (`_Ft_`/`_Fs_`/`_X`/`_bc_x`/`_Imm5_`/…),
the IR-state accessors (`mVUregs`/`mVUregsTemp`/`iPC`/`incPC`/`mVUup`/`mVUlow`/`sFLAG`/`mFLAG`/
`cFLAG`/`mVUconstReg`/`xPC`/`curI`/`setCode`/`bSaveAddr`/…), `branchSTR`, the recompiler-pass
signature macros (`mV`/`mP`/`mX`/`pass1`/`mVUop`/`Fnptr_mVUrecInst`), and the optimization-option
constexprs (`doBranchInDelaySlot`/`doSFlagInsts`/`doRegAlloc`/…). **Dropped** (all
x86emitter-coupled): the `xmm`/`x32` typedefs, the `mVUglob` emit-constant table, the host
register-name macros (`xmmT1..`/`gprT1..`/`gprF0..` — the ARM64 map lives in `aVU_IR.h`), and
the x86 shuffle-immediate helpers (`shufflePQ`/`shuffleSS`). `doConstProp` stays in `aVU.h` and
`doWholeProgCompare` in `aVU.cpp` (not duplicated).

`aVU.cpp` `#include`s both and adds `mVUanalyzeCompileCheck` (never called) odr-using every
analyzer entry point, so the bodies are type-checked + codegen'd now even though the per-op
`pass1` handlers that *drive* them are task 7.5+. Removed the two redundant `const bool isVU1`
dispatcher locals (the new `isVU1` macro is the same `mVU.index` test).

**Verified:** `pcsx2-qt` builds arm64; unittests 2/2 (common + core). Pure infrastructure;
microVU stays **unselected** on ARM64 (VMManager pins `CpuIntVU0/1`). Commit `863de3e77`.

**Deliberately deferred from the 7.3 bullet:**
- **`microVU_Tables.inl`** → moves to **7.5**. The opcode dispatch tables are 256+ function
  pointers to the per-op emit handlers (`mVU_ADDx`/`mVU_LQ`/…) that don't exist yet, so the
  tables can't compile standalone — they land with the handlers.
- **The pipeline/cycle/flag-analysis helpers in `microVU_Compile.inl`** (`mVUsetupRange`,
  `mVUincCycles`, `mVUsetCycles`, `mVUoptimizePipeState`, `eBitPass1`, `branchWarning`,
  `mVUcheckBadOp`, `optimizeReg`/`calcCycles`/`tCycles`) — these *are* arch-neutral and are the
  natural next slice, but in `microVU_Compile.inl` they sit interleaved with the emit-coupled
  compile driver (`doUpperOp`/`doSwapOp`/`mVUexecuteInstruction` call the emitter + the tables),
  so they come over with the 7.4 compile-driver port rather than the analysis-only `.inl`.

**Next:** Task 7.3 part 2 / 7.4 — port the arch-neutral pipeline/cycle/flag-analysis helpers from
`microVU_Compile.inl` (`mVUsetupRange`/`mVUincCycles`/`mVUsetCycles`/`mVUoptimizePipeState` +
`eBitPass1`/`branchWarning`/`mVUcheckBadOp`) into `aVU.cpp`, leaving the emit-coupled compile
driver (`doUpperOp`/`doLowerOp`/`doSwapOp`/`mVUexecuteInstruction`/`mVUcompile`/`mVUblockFetch`)
and the opcode tables for when the 7.5 VIXL emit handlers exist. These helpers also make no
emitter calls, so they compile against the existing IR + macro layer.

---

### Earlier focus (kept for context)

**Phase 7 (VU recompilers / microVU) — task 7.2b (host register allocator → `aVU_IR.h`) DONE.**

`pcsx2/arm64/aVU_IR.h` now holds the ARM64 port of the x86 `microRegAlloc` class
(`microVU_IR.h` 226–1139): VF regs → NEON `v0`–`v23` (pool; slot i == host `v_i`), VI regs →
ARM `w`-regs, and all VF/VI/ACC/I-reg memory access goes through base+offset addressing
against `RVUSTATE = x19` (= `&vuRegs[index]`) instead of the x86 absolute-address `ptr[...]`.
The COP2 / VU0-macro path (`regAllocCOP2`, `_allocVFtoXMMreg`/`_allocX86reg`, the
`pxmmregs`/`x86regs` interaction, `updateCOP2AllocState`, `flushPartialForCOP2`,
`clearRegCOP2`/`clearGPRCOP2`) is **dropped** — that's the EE-side macro-mode allocator
(Phase 7.9); ARM64 macro ops currently use the Phase 5.3 inline-interp fallback. The rest of
the allocator bookkeeping (`allocReg`/`allocGPR`/`writeBackReg`/`clearNeeded`/`flushAll`/
`clearReg`/`clearGPR`/`unbindAnyVIAllocations`/`moveVIToGPR` + the query helpers) mirrors x86
faithfully. NEON emit helpers `mVUloadReg`/`mVUsaveReg`/`mVUmergeRegs`/`loadIreg` ported with
matching `xyzw` lane semantics (NEON V4S lane 0 = X, same as xmm); partial NEON stores compute
the target address explicitly because ARM single-lane `St1` has no immediate offset.

ARM-specific correctness call: **all VF pool regs are treated caller-saved.** AAPCS64
preserves only the *low 64 bits* of `v8`–`v15` across a C call, but VF is 128-bit, so even
those can't survive a call intact — `flushCallerSavedRegisters` writes back every cached VF
reg before a call, matching x86 SysV (where no xmm is callee-saved).

**Verified for real:** `aVU.cpp` instantiates the allocator (`mVUallocCompileCheck`) and
odr-uses `allocReg`/`allocGPR`/`moveVIToGPR`/`flushAll`/`flushCallerSavedRegisters`/
`TDwritebackAll`/etc. so the VIXL emission paths are actually compiled, not just parsed.
`pcsx2-qt` builds arm64; unittests 2/2. microVU stays *unselected* — pure infrastructure.

**Provisional (to confirm at 7.2d):** the host register map (which v-regs/w-regs are the
pools, `RVUSTATE=x19`, `gprT1=w9`/`gprT2=w10`, `gprF0..3=w23..w26`, `mVU_xmmPQ=v24`,
`RVUADDR=x17` transient) is a reasonable first cut; the dispatcher (7.2d) pins the final ABI.

**Next:** Task 7.2c — flesh out `pcsx2/arm64/aVU.cpp` mirroring `microVU.cpp`
(`mVUinit`/`mVUreset`/`mVUclose`/`mVUclear`, program-cache mgmt, the `recMicroVU0/1::*`
provider methods) and **define** the `microVU0/1` globals (the allocator is now complete, so
`microVU`'s `unique_ptr<microRegAlloc>` dtor can be instantiated). Then 7.2d the dispatcher
(`mVUdispatcherAB/CD`).

---

### Earlier focus (kept for context)

**Phase 7 (VU recompilers / microVU) — task 7.2a (arch-neutral structs → `aVU.h`) DONE.**

`pcsx2/arm64/aVU.h` now holds the ported arch-neutral microVU data structures (cloned from
x86 `microVU_IR.h` + `microVU.h`): `microRegInfo` (the 96-byte pipeline-state key), the IR
structs (`microBlock`/`microOp`/`microIR`/the per-op read-write-flag info), and the program/
block bookkeeping (`microProgram*`/`microProgManager`/`microBlockManager`/`microVU`).
Renames per plan: `x86ptr/x86start/x86end`→`codePtr/codeStart/codeEnd`,
`x86ptrStart`→`codeStart`. `microRegAlloc` is only forward-declared (it's arch-specific →
7.2b); `microVU` holds it by `unique_ptr`, and the `microVU0/1` globals are now `extern`
(defined in `aVU.cpp` at 7.2c where the allocator is complete). The arch-neutral
`microOpcode` enum + (compiled-out) `microProfiler` are shared from `x86/microVU_Profiler.h`
(no x86 dependency) rather than re-cloned. ARM tweak: `xmmBackup[16][4]`→`vecBackup[32][4]`
(sized for NEON v0–v31), `xmmCTemp`→`vecCTemp`.

**Verified for real** (not just "should build"): added a minimal `pcsx2/arm64/aVU.cpp` TU
(the start of the 7.2c shell) that `#include`s the header and runs layout `static_assert`s —
`sizeof(microRegInfo)==96` / `alignof==16` **pass on ARM64**, proving the union/bitfield
layout ported correctly. Wired both files into `pcsx2/CMakeLists.txt`. `pcsx2-qt` builds
arm64; unittests 2/2 (common + core). microVU stays *unselected* (interpreter live) — this is
pure infrastructure.

**Next:** Task 7.2b — port `microRegAlloc` (`microVU_IR.h` 226–1139) to ARM64 in a new
`pcsx2/arm64/aVU_IR.h`: NEON v-regs for VF, ARM w-regs for VI. Mirror `allocReg`/`allocGPR`/
`writeBackReg`/`clearNeeded`/`flushAll`/`clearReg`/`clearGPR` with VIXL loads/stores (drop the
x86 COP2/`_allocVFtoXMMreg` regAllocCOP2 path for now — that's the EE-side macro mode, Phase
7.9). Then 7.2c fleshes out `aVU.cpp` (program cache + `recMicroVU0/1`) and defines the globals.

---

### Earlier focus (kept for context)

**Task 7.1 (study x86 microVU) DONE.**

The user picked Phase 7 (the last big interpreter-bound component) over IOP const-prop/reg-alloc.
Spent this session studying the entire x86 microVU (`pcsx2/x86/microVU*` — ~10,600 lines across
18 files) and the VU provider wiring. Key architectural findings (see [[arm64-microvu-architecture]]):

- **microVU is a *program-level* recompiler**, not block-at-a-time like the EE/IOP recs.
  `mVUsearchProg` caches whole microprograms (the VU's 4KB/16KB micro memory), detects program
  changes by comparing a **96-byte `microRegInfo` pipeline-state key**, and recompiles *blocks*
  within a program (`mVUblockFetch`/`mVUcompile`) keyed by that pipeline state. Blocks chain in
  host code via a per-block search/link (`microBlockManager`).
- **Entry contract** (`recMicroVU0/1::Execute` → `mVUdispatcherAB`): caller puts startPC+cycles in
  arg regs; the dispatcher loads VU FPCR, the PQ (Q/P latency) NEON reg, mac/clip/status flag
  instances, then jumps to the compiled block; on exit it writes flags back + runs `mVUcleanUp`
  (cycle accounting, cache-limit reset). `mVUdispatcherCD` is the XGKICK resume/exit path.
- **Arch-neutral** (port near-verbatim): the structs (`microRegInfo`/`microBlock`/`microOp`/
  `microIR`/`microProgram*`/`microBlockManager`), the deep **pipeline-analysis pass**
  (`microVU_Analyze.inl` — Q/P latency, the 4-instance Status/Mac/Clip flag pipeline, stalls,
  hazards) and `microVU_Tables.inl` (opcode dispatch). These operate on `microOp`/`microIR`, no
  emitter calls.
- **Arch-specific** (full VIXL rewrite): `microRegAlloc` (xmm→NEON v0–v31, x86 GPR→ARM w-regs;
  `microVU_IR.h` lines 226–1139), `microVU_Upper.inl` (FMAC float vector ISA), `microVU_Lower.inl`
  (VI ALU / load-store / EFU DIV·SQRT·RSQRT / MOVE / RANDOM / branches), `microVU_Flags.inl`,
  `microVU_Branch.inl`, `microVU_Execute.inl` (dispatcher), `microVU_Misc/Clamp/Alloc.inl`, and
  `microVU_Macro.inl` (the EE-side COP2 ops — **already covered by the Phase 5.3 inline-interp
  fallback**, so this is the lowest priority piece).
- **Bring-up constraint:** unlike the IOP rec, microVU **cannot** be "wired but single-stepping" —
  its whole model is program compilation, not per-op stepping. So the rec must be functional
  enough to run a program before `CpuVU0/CpuVU1` get flipped off the interpreter in `VMManager.cpp`
  (`UpdateCPUImplementations`, currently hard-pinned to `CpuIntVU0/1` on ARM64). This mirrors how
  the EE rec built Phases 1–4 before `Cpu = &recCpu`. Strategy: **parallel clone** in
  `pcsx2/arm64/` (do NOT touch the x86 microVU — hard rule #1); copy arch-neutral structs/analysis,
  rewrite emit in VIXL.

**Next:** Task 7.2a — port the arch-neutral data structures into `pcsx2/arm64/aVU.h`
(rename `x86ptr/x86start/x86end` → `codePtr/codeStart/codeEnd`, drop the x86emitter include), then
7.2b the NEON/ARM `microRegAlloc`, then 7.2c the `aVU.cpp` shell + CMake wiring so it BUILDS (not
yet selected). See the expanded Phase 7 checklist below.

---

**Phase 6.3 IOP COP0/COP2 inline-interp — DONE (commit `9095d983b`). Phase 6.3 COMPLETE.**

Straight-line IOP coprocessor ops now inline the interpreter handler
(`recEmitInterpInline`) and keep the block intact, instead of breaking the block +
single-stepping. Mirrors the EE Phase 5.1/5.3 trick and the x86 IOP rec
(`REC_GTE_FUNC` / `rpsxCP0` — flush + call handler, no block break). Wired into
`recTranslateOp`:

- **COP0 (0x10):** MFC0/CFC0/MTC0/CTC0/RFE. The IOP interpreter has **no** COP0 branches
  (those `psxCP0` slots are `psxNULL`), so every COP0 op is straight-line / never writes pc.
  **RFE** additionally emits an `iopTestIntc` call (matches x86 `rpsxRFE`) — `iopTestIntc`
  only adjusts the next-event delta, never touches pc, so it's safe mid-block.
- **COP2 (0x12):** GTE ops + MFC2/CFC2/MTC2/CTC2. No COP2 branches either.
- **LWC2 (0x32) / SWC2 (0x3a):** GTE quad load/store.

This also lets the common return-from-handler `jr $k0; rfe` (RFE as the JR delay slot)
compile fully natively.

**Phase 6.3 is now complete** — the IOP rec compiles its entire practical opcode set
natively (integer ALU/shift/muldiv, all loads/stores, all branches/jumps, all coprocessor
ops inline). Only SYSCALL/BREAK still return false (rare, correctly single-stepped — they
raise an exception and rewrite pc, so single-step is the right model).

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites); headless BIOS boot
reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` (COP0-heavy IOP
interrupt/SIO init) with COP0/COP2 inline live — no crash/abort/unmapped, no
Unimplemented-op warnings, clean log.

**Next:** Live game IOP-perf measurement (FFX / a 2D title) to quantify the full Phase 6.3
win now that the IOP rec spans control flow and keeps coprocessor ops in-block. Then either
optimize the IOP (constant-prop / register allocation — today every op round-trips psxRegs
in memory) or move to **Phase 7 (VU recompilers / microVU)**, the last big interpreter-bound
component.

---

**Phase 6.3 IOP native branches/jumps — DONE (commit `13daf73d0`).**

IOP blocks now span control flow natively instead of single-stepping every branch through
the interpreter — the real IOP speedup. Wired into the block compiler (`recRecompile`) in
`pcsx2/arm64/aR3000A.cpp`:

- **Generators (`recEmitIopBranch`):** J/JAL (immediate region target), JR/JALR (register
  target read *before* the delay slot), BEQ/BNE/BLEZ/BGTZ, REGIMM BLTZ/BGEZ/BLTZAL/BGEZAL.
  32-bit counterparts of the EE generators (`aR5900Branch.cpp`); emit only the control-flow
  effect (psxRegs.pc write + GPR[31]/GPR[rd] link). Target/fallthrough/link mirror the
  interpreter macros with `_PC_ == branchpc+4`.
- **Block compiler:** on a handled branch, emit the branch effect, compile the delay slot
  (`recEmitOp`), charge branch+delay = 2 cycles, end the block. The dispatcher re-reads
  psxRegs.pc for the next block. Writing pc before the delay slot is safe (no IOP
  delay-slot op writes pc) and required for JR/JALR.

Correctness boundaries (match the x86 IOP rec):
- doBranch's a0-override (target `0xbfc4a000`) + ClearIrxModules (`0x890`) are
  interpreter-only quirks the x86 rec's psxSetBranchReg/Imm also skip → omitted.
- psxJ's **IRX-import magic** preserved by bailing J-with-magic-delay-slot
  (`delay >> 16 == 0x2400`) to the interpreter (runs `irxImportExec`).
- A delay slot that is itself a branch is bailed to the interpreter (would nest doBranch).
- Also fixed `recEmitInterpInline` to dispatch via the IOP `psxBSC` table (it wrongly used
  the EE `R5900::GetInstruction` table) so non-native delay slots inline-interp correctly.

**Still on interpreter fallback (return false):** SYSCALL/BREAK, COP0 (mfc0/mtc0/rfe),
COP2/GTE. Likely branches don't exist on MIPS-I (R3000A), so the branch set is complete.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites; IOP has no gtest harness
— logic mirrors the gtested EE branch generators); headless BIOS boot reaches
`Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` (IOP-heavy, branch-dense pad/SIO
init) with native IOP branches live — no crash/abort/unmapped/Branch-to-0x0, clean log.

**Next:** Live game IOP-perf measurement (FFX or a 2D title) to quantify the
branches+load/store win now that IOP blocks span control flow. Then Phase 6.3 cont. — IOP
**COP0** (mfc0/mtc0/rfe, likely inline-interp) and **COP2/GTE** (inline-interp, the
EE-style trick), or move to Phase 7 (VU). Optional: native constant-prop / reg-alloc for
the IOP (today every op round-trips psxRegs in memory).

---

**Phase 6.3 IOP native load/store — COMPLETE (aligned `f104adb25` + unaligned `3efc4c40c`).**

The full R3000A load/store family now compiles natively in `recTranslateOp`
(`pcsx2/arm64/aR3000A.cpp`); none of it single-steps the interpreter anymore:

- **Aligned (`f104adb25`):** LB/LBU/LH/LHU/LW + SB/SH/SW via `iopMemRead/Write8/16/32`.
- **Unaligned (`3efc4c40c`):** LWL/LWR (read aligned word, merge into GPR[rt] with runtime
  shift+mask) and SWL/SWR (read-modify-write the aligned word). Mirrors
  `psxLWL/LWR/SWL/SWR`. Only the `iopMemRead32` result flows forward; address+shift are
  recomputed from psxRegs after the call (GPR[rs] unchanged → identical), so nothing is
  spilled. Working regs RWRET/RWARG2-4/RSCRATCHW are all scratch; x19/x16 hold no live
  state across the calls.

Loads perform the read even when rt==0 (I/O side effects), suppressing only the GPR write.
The IOP ignores load-delay-slots (as does the x86 IOP rec), so writing GPR[rt] immediately
and compiling the following op natively is correct.

**Still on interpreter fallback (return false):** all control flow (J/JAL/JR/JALR, REGIMM +
BEQ/BNE/BLEZ/BGTZ, SYSCALL/BREAK) and coprocessor ops (COP0/COP2-GTE).

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites; IOP has no gtest harness
yet — logic mirrors the gtested EE load/store generators); headless BIOS boot reaches
`Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` (IOP-heavy pad/SIO init) with the
full native IOP load/store family live, no crash/abort/unmapped access.

**Next:** Phase 6.3 cont. — native IOP **branches/jumps** (J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ
+ REGIMM BLTZ/BGEZ/BLTZAL/BGEZAL). This is the real IOP speedup: today every control-flow
op ends the native run and single-steps, so blocks never span a branch. Port the EE branch
generator (`recEmitBranch` in aR5900.cpp) narrowed to 32-bit — emit the branch + compile
the delay slot + write psxRegs.pc + end the block. Live game IOP-perf measurement after
branches land.

---

**Phase 6.3 IOP native aligned load/store generators — DONE (commit `f104adb25`).**

Wired native ARM64 codegen for the R3000A aligned load/store subset into
`recTranslateOp` (`pcsx2/arm64/aR3000A.cpp`), so these ops no longer single-step the
interpreter:

- **Loads:** LB/LBU/LH/LHU/LW. Effective address `GPR[rs]+(s16)imm` into RWARG1
  (`iopEmitEffectiveAddr`), call `iopMemRead8/16/32`, then sign/zero-extend (Sxtb/Uxtb/
  Sxth/Uxth; full 32-bit for LW) and store to GPR[rt]. The read is performed even when
  rt==0 (I/O side effects); only the GPR write is suppressed — matches `psxLB..psxLW`.
- **Stores:** SB/SH/SW. Value from GPR[rt] (GPR[0] reads zero) into RWARG2, address into
  RWARG1, call `iopMemWrite8/16/32`.

All go through the `iopMemRead/Write8/16/32` slow path (no vtlb fastmem on the IOP). The
helper call clobbers caller-saved scratch (x16/x17) but RESTATEPTR=x19 is callee-saved and
survives; each generator reads its inputs from psxRegs fresh, so no cross-call state lives
in scratch. The IOP ignores load-delay-slots (as does the x86 IOP rec), so writing GPR[rt]
immediately and compiling the following op natively is correct. This is the first native
multi-op IOP block to make a C++ call mid-block — the prologue's 16-byte/16-aligned
`stp x19,lr` frame keeps sp AAPCS64-aligned at the BL.

**Still on interpreter fallback (return false):** unaligned **LWL/LWR/SWL/SWR** (need
read-modify-write across a mem call → address/value spilling, next slice), all control
flow (J/JAL/JR/JALR, REGIMM + BEQ/BNE/BLEZ/BGTZ, SYSCALL/BREAK), and coprocessor ops.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites; IOP has no gtest
harness yet — logic mirrors the gtested EE load/store generators); headless BIOS boot
reaches `Mode Changed to DVD PAL` + `Pad: DS2 Config Finished` (IOP-heavy pad/SIO init)
with the native IOP load/store path live, no crash/abort/unmapped access.

**Next:** Phase 6.3 cont. — unaligned **LWL/LWR/SWL/SWR**, then **branches/jumps**
(J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ + REGIMM) so the rec stops breaking the block at every
control-flow op — the real IOP speedup. Live game IOP-perf measurement after branches land.

---

**Phase 6.3 IOP native integer generators — DONE (commit `86448e99c`).**

Wired native ARM64 codegen for the R3000A integer subset into `recTranslateOp`
(`pcsx2/arm64/aR3000A.cpp`), so these ops no longer single-step the interpreter.
32-bit MIPS-I — a strict subset of the gtested EE arith/multdiv generators, but
32-bit GPRs (no sign-extend-to-64) and 32-bit HI/LO:

- **I-type:** ADDI/ADDIU, SLTI, SLTIU, ANDI, ORI, XORI, LUI
- **R-type ALU:** ADD/ADDU, SUB/SUBU, AND, OR, XOR, NOR, SLT, SLTU
- **Shifts:** SLL, SRL, SRA, SLLV, SRLV, SRAV
- **HI/LO moves:** MFHI, MTHI, MFLO, MTLO
- **Mul/Div:** MULT, MULTU, DIV, DIVU. Note R3000A MULT/MULTU write **only** HI/LO
  (no Rd write, unlike the R5900 3-operand form). ARM `SDIV` reproduces the x86
  INT_MIN/−1 overflow quirk natively; only ÷0 needs a fixup (signed:
  LO=(Rs<0)?1:−1, HI=Rs; unsigned: LO=−1, HI=Rs). Semantics mirror
  `R3000AOpcodeTables.cpp` exactly.

Scratch discipline matches the EE multdiv generators: x17 (RSCRATCHADDR) is the
safe manual scratch (the only reg armStartBlock removes from VIXL's pool); x16
(RXVIXLSCRATCH) is a plain operand reg, never holding a value across a macro that
materialises an immediate.

**Still single-stepped (return false → end native run, interpret):** control flow
(J/JAL/JR/JALR, REGIMM + BEQ/BNE/BLEZ/BGTZ, SYSCALL/BREAK), loads/stores, and all
coprocessor ops (COP0/COP2-GTE). Native blocks therefore never contain a branch or
its delay slot — the interpreter handles branch+delay atomically, so the model is
correct. The IOP ignores load-delay-slots (as does the x86 IOP rec), so compiling
the post-load op natively is safe.

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (355 core; Arm64EmitEE
unaffected — IOP has no gtest harness yet, the logic mirrors the gtested EE
generators); headless BIOS boot reaches `Mode Changed to DVD PAL` +
`Pad: DS2 Config Finished` (IOP-heavy pad/SIO init) with the native IOP path live.

**Next:** Phase 6.3 cont. — native IOP **load/store** generators (LB/LBU/LH/LHU/LW/
LWL/LWR, SB/SH/SW/SWL/SWR via the `iopMemRead/Write8/16/32` slow path; no vtlb
fastmem on IOP), then **branches/jumps** (J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ +
REGIMM) so the rec stops breaking the block at every control-flow op — the real
speedup. Live game IOP-perf measurement after branches land.

---

**Phase 6.1/6.2 IOP rec skeleton — DONE (boots via interpreter single-step).**

Created `pcsx2/arm64/aR3000A.{h,cpp}` and wired the `psxRec` provider on ARM64
(VMManager reserve/shutdown/reset + `psxCpu = CHECK_IOPREC ? &psxRec : &psxInt`).

Execution model (bring-up, mirrors the EE rec's Phase 4.3 baseline, NOT the emitted
recLUT dispatcher): a **C++ dispatcher loop** `recExecuteBlock(eeCycles)` drives the IOP
timeslice — looks up the host block for `psxRegs.pc` in a 2-level **recLUT** (compile on
miss via `recRecompile`), calls it, then debits the consumed cycles against `iopCycleEE`
(`iopAddEECycles`, PS2 ×8 / PS1 gcd carry) and runs `iopEventTest` when the event cycle
is reached. Exactly mirrors `R3000AInterpreter.cpp intExecuteBlock` (incl. the HLE-BIOS
`a0/b0/c0` entry under `HW_ICFG&8`). Each block has its **own prologue/epilogue**
(`stp x19,lr` / pin `RESTATEPTR=&psxRegs` / `ldp` + `RET`) and ends by writing
`psxRegs.pc`. **All ops currently single-step** through the interpreter
(`iopExecuteOneInst`, added to R3000AInterpreter.cpp): `recTranslateOp` returns false for
everything, so each block is a one-shot interp step — correct but not yet faster than the
interpreter. Native generators (next) make blocks span many ops for the real speedup.

recLUT: cloned from `aR5900.cpp`, IOP memory map (RAM mirrored at seg 0x0000/0x8000/0xa000
masked to size; ROM/ROM1/ROM2). Slot value = block ptr / 0 (compile) / `IOP_UNMAPPED`
sentinel. `recClearIOP(addr,size)` resets in-range mapped slots to 0 (targeted, mirrors
the EE recClear / x86 recClearIOP).

**Verified:** `pcsx2-qt` builds arm64; unittests 100% (2/2 suites, 273 emit tests
unaffected). Headless `-batch -bios` boot run completed without crash/abort. **Live
boot/game confirmation still pending** (user to run).

**Next:** Phase 6.3 native IOP integer generators (immediate/reg-reg/shift/move/mult-div)
— 32-bit MIPS-I, a strict subset of the EE arith generators; wire into `recTranslateOp`.
Then load/store (via `iopMemRead/Write8/16/32` slow path) and branches/jumps.

---

**Phase 5.1 COP0 inline-fallback — DONE (commit `ffe4f18d1`).**

Applied the [[Phase 5.3 COP2]] inline-interpreter trick to COP0 (primary opcode `0x10`):
straight-line COP0 ops now emit an inline `recEmitInterpInline` call instead of breaking
the EE block + single-stepping. COP0 is not a per-op perf item (cf. `x86/iCOP0.cpp`), so
the win is purely killing block fragmentation around TLB setup / interrupt-mask writes.

**Inlined:** `MFC0`/`MTC0` (Rd ∉ {9 Count, 25 PERF}) + `TLBR/TLBWI/TLBWR/TLBP`.
**Kept single-stepped (return false):**
- `BC0F/BC0T/BC0FL/BC0TL` + `ERET` — write `cpuRegs.pc`;
- `MFC0`/`MTC0` of Count(9)/PERF(25) — need a live `cpuRegs.cycle` (this rec flushes it
  only at the block tail; `COP0.cpp` warns two `MFC0 Count` in one block before the cycle
  update return increment 0 → games lock up);
- `EI/DI/WAIT` — interrupt-enable timing the x86 rec deliberately branches after.
`MTC0 Status/Config` are inlined safely (x86 rec doesn't branch after them; the resulting
interrupt is taken at the block-tail event test). TLB writes call `MapTLB→recClear`, safe
mid-block (targeted recLUT reset; the running block keeps its valid host code).

**Verified:** 273/273 ARM64 emit tests green; BIOS boots (heavy COP0: TLB + Status writes),
user-confirmed live.

**Next:** IOP rec (Phase 6) for playable 2D — the biggest remaining leverage; IOP is still
fully interpreter-single-stepped. EE now keeps COP0/COP2/MMI/FPU all in-block.

---

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

- [x] 5.1 COP0: **inline interpreter fallback** (commit `ffe4f18d1`). Straight-line COP0
  ops (`MFC0`/`MTC0` for Rd∉{9,25}, `TLBR/TLBWI/TLBWR/TLBP`) emit inline `recEmitInterpInline`
  instead of breaking the block. PC-writing (`BC0*`,`ERET`), cycle-sensitive (Count/PERF),
  and interrupt-gating (`EI/DI/WAIT`) ops stay single-stepped. Native COP0 codegen not
  needed (COP0 isn't a per-op perf item).
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

- [x] 6.1 Create `pcsx2/arm64/aR3000A.{h,cpp}`.
- [x] 6.2 Implement `psxRec` interface (Reserve/Reset/ExecuteBlock/Clear/Shutdown) —
  C++ dispatcher loop + recLUT + per-block prologue/epilogue; all ops interpreter
  single-stepped (`iopExecuteOneInst`) for now. Correct, not yet fast.
- [x] 6.3 Port integer / load-store / branch / coprocessor generators (simpler than EE).
  - [x] Integer: ADDI/ADDIU/SLTI/SLTIU/ANDI/ORI/XORI/LUI, ADD/ADDU/SUB/SUBU/AND/OR/
    XOR/NOR/SLT/SLTU, SLL/SRL/SRA/SLLV/SRLV/SRAV, MFHI/MTHI/MFLO/MTLO,
    MULT/MULTU/DIV/DIVU (commit `86448e99c`). 32-bit; mirrors the gtested EE generators.
  - [x] Load/store (via iopMemRead/Write8/16/32 slow path — no vtlb fastmem on IOP).
    Aligned LB/LBU/LH/LHU/LW + SB/SH/SW (`f104adb25`); unaligned LWL/LWR/SWL/SWR
    (`3efc4c40c`, runtime shift+mask, recompute-after-call, no spill).
  - [x] Branches/jumps (J/JAL/JR/JALR, BEQ/BNE/BLEZ/BGTZ + REGIMM) — `13daf73d0`.
    Native generators + delay-slot compile in the block compiler; J-with-IRX-magic and
    branch-in-delay-slot bail to the interpreter. No likely branches on MIPS-I.
  - [x] Coprocessor (COP0 MFC0/CFC0/MTC0/CTC0/RFE; COP2/GTE + LWC2/SWC2) — `9095d983b`,
    inline-interp (EE-style, keeps block intact). RFE also emits iopTestIntc. No COP0/COP2
    branches on the IOP. **Phase 6.3 complete** (only SYSCALL/BREAK single-step).
- [x] 6.4 Wire into `VMManager.cpp` (reserve/shutdown/reset + `psxCpu` selection on ARM64).

**Milestone after Phase 6:** playable 2D games expected.

---

## Phase 7 — VU Recompilers (microVU)

> Strategy: **parallel clone** in `pcsx2/arm64/` — never touch `pcsx2/x86/microVU*` (hard rule #1).
> Copy the arch-neutral structs + analysis pass; rewrite all emission in VIXL. microVU stays
> *unselected* (interpreter live) until 7.6 — it's a program-level rec, so it can't single-step
> like the IOP skeleton could. VF→NEON `v0-v31`, VI→ARM `w`-regs, Q/P latency in one NEON reg.
> The full buildable sub-task order:

- [x] 7.1 Study `x86/microVU*.cpp/h` + `microVU_*.inl` — done (see CURRENT FOCUS +
  [[arm64-microvu-architecture]]). Mapped arch-neutral vs arch-specific; understood program/block
  model, the 96-byte `microRegInfo` state key, dispatcher entry contract, flag-instance pipeline.
- [~] 7.2 **Skeleton & infrastructure** (builds + links; NOT yet selected in VMManager):
  - [x] 7.2a `pcsx2/arm64/aVU.h` — ported the arch-neutral structs (`microRegInfo`, `microBlock`,
    `microJumpCache`, `microTempRegInfo`, `microVFreg/microVIreg`, `microConstInfo`,
    `microUpperOp/microLowerOp`, `microFlagInst/Cycles`, `microOp`, `microIR`,
    `microProgram(Manager/Quick/List)`, `microBlockManager`, `microVU`). Renamed
    `x86ptr/x86start/x86end` → `codePtr/codeStart/codeEnd`; dropped `x86emitter.h`.
    `microRegAlloc` forward-declared (7.2b). Verified via a minimal `aVU.cpp` TU: builds arm64,
    `sizeof(microRegInfo)==96` static_assert passes, unittests 2/2.
  - [x] 7.2b Port `microRegAlloc` (`microVU_IR.h` 226–1139) to ARM64 in `pcsx2/arm64/aVU_IR.h`
    — NEON `v0`–`v23` for VF, ARM `w`-regs for VI, base+offset addressing vs `RVUSTATE=x19`.
    Mirrors `allocReg`/`allocGPR`/`writeBackReg`/`clearNeeded`/`flushAll`/`clearReg`/`clearGPR`
    + the NEON emit helpers (`mVUloadReg`/`mVUsaveReg`/`mVUmergeRegs`/`loadIreg`). COP2/macro path
    dropped (Phase 7.9). All VF pool regs treated caller-saved (AAPCS64 only keeps v8–v15 low 64).
    Compile-exercised via `mVUallocCompileCheck` in aVU.cpp. Builds arm64; unittests 2/2.
  - [x] 7.2c `pcsx2/arm64/aVU.cpp` — mirror `microVU.cpp` (mVUinit/reset/close/clear, program
    cache mgmt, `mVUsearchProg`, `recMicroVU0/1::*` methods) + define the `microVU0/1` /
    `CpuMicroVU0/1` globals + real `vuJITFreeze`. Codegen/compile layer (dispatcher,
    block-fetch) `pxFailRel`-stubbed for 7.2d/later. Builds arm64; unittests 2/2. (`532adca92`)
  - [x] 7.2d Dispatcher — ported `mVUdispatcherAB/CD` + `mVUGenerateWaitMTVU/CopyPipelineState/
    CompareState` + `mVUexecute`/`mVUcleanUp` to VIXL (`microVU_Execute.inl` 23–315). AAPCS64
    frame via `armBeginStackFrame(true)`; loads VU FPCR (`msr FPCR`), builds the PQ NEON reg
    (v24) with the x86 lane layout, copies mac/clip flags, loads status GPRs (gprF0-3=w23-w26),
    `br x0` into the block; exit restores EE FPCR + `mVUcleanUp` cycle accounting. `mVUreset`
    now does the real emitter setup (`armSetAsmPtr(mVU.cache)` + one start/end block) and sets
    `codeStart`/`codePtr` past the dispatchers. Unselected ⇒ compiled but not executed yet.
    Builds arm64; unittests 2/2. (`a0d93ed5c`)
- [x] 7.3 **Analysis pass** (arch-neutral, near-verbatim copy) — `microVU_Analyze.inl` +
  the pipeline/flag analysis helpers in `microVU_Compile.inl`. Operates on `microOp`/`microIR`;
  no emitter calls, so it ported almost unchanged. (`microVU_Tables.inl` moved to 7.5.)
  - [x] `microVU_Analyze.inl` → `pcsx2/arm64/aVU_Analyze.inl` (pass-1 analysis) + the arch-neutral
    macro layer `pcsx2/arm64/aVU_Misc.h`. Compile-exercised via `mVUanalyzeCompileCheck`. Builds
    arm64; unittests 2/2. (`863de3e77`)
  - [x] Pipeline/cycle/flag-analysis helpers from `microVU_Compile.inl` (`mVUsetupRange`/
    `mVUincCycles`/`mVUsetCycles`/`mVUoptimizePipeState`/`eBitPass1`/`branchWarning`/`eBitWarning`/
    `mVUcheckBadOp` + `optimizeReg`/`calcCycles`/`tCycles`/`incP`/`incQ`/`cmpVFregs`/`mVUcheckIsSame`)
    → `pcsx2/arm64/aVU.cpp`. Arch-neutral; compile-exercised via `mVUcompileHelpersCheck`. Builds
    arm64; unittests 2/2. (`30ee0b64b`)
  - [ ] `microVU_Tables.inl` — MOVED to 7.5 (the dispatch tables reference the per-op emit
    handlers, which don't exist until the VIXL emission task).
- [~] 7.4 **Compile driver** — port `microVU_Compile.inl` (`mVUcompile`/`mVUblockFetch`/block
  search+link/`mVUsetupRange`). Interleaves 7.3 analysis with emit; ends blocks on E-bit/branch.
  - [x] First-pass init helpers (emitter-free): `startLoop`/`mVUinitConstValues`/`mVUinitFirstPass`
    → `pcsx2/arm64/aVU.cpp`. memset/memcpy + block-manager `add` only (no VIXL); `x86ptrStart`→
    `codeStart`. Compile-exercised via `mVUcompileHelpersCheck`. Builds arm64; unittests 2/2. (`9b01b4ca2`)
  - [ ] Emit-coupled driver `mVUcompile` + `mVUexecuteInstruction`/`doUpperOp`/`doLowerOp`/`doSwapOp`/
    `doIbit`, `mVUtestCycles`, `mVUDoDBit`/`mVUDoTBit`, `mvuPreloadRegisters`, and the
    `mVUentryGet`/`mVUblockFetch` entry points (still `pxFailRel`). Big-bang: links only once the 7.5
    emit backend (Upper/Lower + Flags + Branch/endProgram + XGKICK) exists — bring up vs. NOP/B first.
- [ ] 7.5 **VIXL emission for the VU ISA:**
  - [ ] 7.5a Upper (FMAC float vector → NEON): ADD/SUB/MUL/MADD/MSUB/MAX/MINI/FTOI/ITOF/CLIP/
    ABS/OPMULA/OPMSUB/NOP + EE non-IEEE 4-lane clamp (reuse the Phase 5.2 FPU clamp insight).
    `microVU_Upper.inl`.
  - [ ] 7.5b Lower — VI integer ALU (IADD/ISUB/IADDI/IADDIU/IAND/IOR), load/store (LQ/SQ/ILW/ISW/
    LQI/SQI/LQD/SQD/ILWR/ISWR), EFU (DIV/SQRT/RSQRT + WAITQ/WAITP), MOVE/MFIR/MTIR/MR32/MFP,
    RANDOM (RINIT/RGET/RNEXT/RXOR), FSAND/FSEQ/FSSET/FMAND/FCxxx, ELENG/ESQRT/etc.
    `microVU_Lower.inl` (the big one — 2203 lines x86).
- [ ] 7.6 **Flags** — port `microVU_Flags.inl` (Status/Mac/Clip 4-instance flag pipeline,
  sticky/non-sticky, FSSET, div flag → status).
- [ ] 7.7 **Branches** — port `microVU_Branch.inl` (B/BAL/IBEQ/IBGEZ/IBGTZ/IBLEZ/IBLTZ/IBNE/JR/
  JALR, plus the badBranch/evilBranch branch-in-delay-slot handling).
- [ ] 7.8 **Wire selection + validate** — flip `CpuVU0/CpuVU1` to `CpuMicroVU0/1` on ARM64 in
  `VMManager.cpp` (Init/Shutdown/Update/Clear); XGKICK→GIF path; MTVU thread. Test ladder:
  BIOS → 2D → IOP-heavy → FFX (VU1-heavy 3D).
- [ ] 7.9 **Macro mode** (lowest priority) — port `microVU_Macro.inl` so the EE rec emits COP2/VU0
  macro ops natively instead of the Phase 5.3 inline-interp fallback. Optional perf polish.

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
