# PROGRESS — ARM64 Recompiler Port

> Living roadmap. **This is the source of truth for "what's done" and "what's next."**
> Update it at the end of every session. Status legend:
> `[ ]` not started · `[~]` in progress · `[x]` done · `[!]` blocked (see JOURNAL).

---

## ▶ CURRENT FOCUS

**Phase 0–4.3 COMPLETE. The EE recompiler now RUNS — `Cpu = &recCpu` boots the BIOS.**
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
- [ ] 4.4 Block linking (direct branch to already-compiled targets) + recLUT
  (replace the bring-up `s_blocks` unordered_map + recompile-on-miss).
- [ ] 4.5 Block invalidation on TLB-mapping change.

---

## Phase 5 — EE Coprocessors

- [ ] 5.1 COP0: interpreter fallback (`recCall(Interp::...)`) initially.
- [ ] 5.2 COP1 (FPU): map FPRs → `d0-d31`; `ADD.S/SUB.S/MUL.S/DIV.S/SQRT.S/...`, `C.*.S`, `BC1T/BC1F`, FPU control/status reg.
- [ ] 5.3 COP2 (VU0 macro): interpreter fallback initially.
- [ ] 5.4 MMI (128-bit int SIMD): map to NEON where possible (ref `x86/iMMI.cpp`).

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
