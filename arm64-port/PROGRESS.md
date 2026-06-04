# PROGRESS — ARM64 Recompiler Port

> Living roadmap. **This is the source of truth for "what's done" and "what's next."**
> Update it at the end of every session. Status legend:
> `[ ]` not started · `[~]` in progress · `[x]` done · `[!]` blocked (see JOURNAL).

---

## ▶ CURRENT FOCUS

**Phase 0 — Tooling & gap verification: COMPLETE (incl. proven emit+execute harness).**
Next concrete task: **Phase 1.1** — create `pcsx2/arm64/aR5900.h` (register-alloc
structs, `recCpu` extern, REC_FUNC-style macros), then `aR5900.cpp` (1.2) with empty
`recCpu` functions added to `pcsx2arm64Sources`, mirroring `pcsx2/x86/iR5900.{h,cpp}`.

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

- [ ] 1.1 Create `pcsx2/arm64/aR5900.h` (register-alloc structs, `recCpu` extern, REC_FUNC-style macros).
- [ ] 1.2 Create `pcsx2/arm64/aR5900.cpp` with empty `recCpu` functions; add to `pcsx2/CMakeLists.txt` (`pcsx2arm64Sources`); confirm it still builds.
- [ ] 1.3 `recCpu.Reserve()`: allocate code cache via `HostSys` (ref `x86/BaseblockEx.cpp`).
- [ ] 1.4 Minimal block compile loop: read 1–2 MIPS ops, emit NOP via VIXL, return; `recCpu.Execute()` jumps in and back.
- [ ] 1.5 Wire `recCpu` into `VMManager.cpp` (let ARM64 call Reserve/Reset/Shutdown). Keep `Cpu = &intCpu;` until the rec actually works.

**Done when:** ARM64 build compiles + runs with the rec reserved/active, even if it
still defers all real work to the interpreter.

---

## Phase 2 — vtlb Fast Memory & Load/Store  *(highest priority after skeleton)*

- [ ] 2.1 Implement `vtlb_DynBackpatchLoadStore` in `pcsx2/arm64/RecStubs.cpp` (currently `pxFailRel`). Slow path first, then fast-path patching.
- [ ] 2.2 Port vtlb backpatch trampoline logic (ref `x86/ix86-32/recVTLB.cpp`); design the ARM64 trampoline calling convention.
- [ ] 2.3 EE load/store generators: `recLB/LH/LW/LD/LBU/LHU/LWU`, `recSB/SH/SW/SD`, `recLQ/SQ` (128-bit via NEON `ld1`/`st1`).
- [ ] 2.4 Test: compile a simple MIPS load/store block, verify correct memory access.

**Done when:** EE memory ops go through the JIT fastmem path and read/write correctly.

---

## Phase 3 — EE Integer Arithmetic

- [ ] 3.1 Immediate ops: `ADDI/ADDIU/SLTI/SLTIU/ANDI/ORI/XORI/LUI/DADDI/DADDIU`.
- [ ] 3.2 Reg-reg ops: `ADD/ADDU/SUB/SUBU/SLT/SLTU/AND/OR/XOR/NOR/DADD/DADDU/DSUB/DSUBU`.
- [ ] 3.3 Shifts: `SLL/SRL/SRA/SLLV/SRLV/SRAV/DSLL.../DSRA32`.
- [ ] 3.4 Moves: `MOVZ/MOVN` (→ `CSEL`), `MFHI/MTHI/MFLO/MTLO`.
- [ ] 3.5 Mul/Div: `MULT/MULTU/DIV/DIVU/DMULT.../DDIVU` (→ `MUL/SMULH/UMULH/SDIV/UDIV`).
- [ ] 3.6 Constant propagation (`EE_CONST_PROP`): track known-constant GPRs, emit immediate forms.

---

## Phase 4 — EE Branches & Jumps

- [ ] 4.1 Jumps: `J/JAL/JR/JALR` + PC update + delay slots.
- [ ] 4.2 Conditional branches: `BEQ/BNE/BLEZ/BGTZ/BLTZ/BGEZ/BLTZAL/BGEZAL` + likely variants.
- [ ] 4.3 Delay-slot compilation (inline; conditional for "likely" branches).
- [ ] 4.4 Block linking (direct branch to already-compiled targets).
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
