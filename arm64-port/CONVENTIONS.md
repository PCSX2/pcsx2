# CONVENTIONS — ARM64 Recompiler Port

> The technical contract. Follow these so every session's code is consistent.
> Grounded in what **already exists** in `pcsx2/arm64/AsmHelpers.h` + `Vif_Dynarec.cpp` —
> not a greenfield proposal. When in doubt, copy the patterns already in those files.

---

## 1. The emitter: VIXL MacroAssembler

All ARM64 codegen goes through VIXL's `MacroAssembler`, accessed via the
thread-local `armAsm` pointer (`AsmHelpers.h:64`). The existing VIF dynarec
(`Vif_Dynarec.cpp`) is the canonical worked example — read it before writing new
emission code.

Block lifecycle (see `AsmHelpers.cpp`):
- `armSetAsmPtr(ptr, capacity, pool)` — point the assembler at a code buffer.
- `armStartBlock()` / `armEndBlock()` — begin/finalize a block; returns code ptr.
- `armGetCurrentCodePointer()` — current emit position.
- `armAlignAsmPtr()` — alignment between blocks.

Helpers you should reuse instead of re-rolling:
- `armEmitJmp(ptr)` / `armEmitCall(ptr)` — far jump/call (handles range via trampolines).
- `armEmitCbnz(reg, ptr)` / `armEmitCondBranch(cond, ptr)` — conditional far branches.
- `armMoveAddressToReg(reg, addr)` — materialize a 64-bit address.
- `armLoadPtr` / `armStorePtr` — load/store a pointer-sized value at an absolute addr.
- `armBeginStackFrame(save_fpr)` / `armEndStackFrame(save_fpr)` — prologue/epilogue.
- `armOffsetMemOperand`, `armGetMemOperandInRegister` — address/offset helpers.
- `armLoadConstant128`, `armEmitVTBL` — 128-bit literal load, NEON table lookup.
- `GetPCDisplacement(cur, tgt)` — PC-relative branch displacement (>>2).

Constant pool: `ArmConstantPool` (`AsmHelpers.h:113`) provides `GetJumpTrampoline`,
`GetLiteral` (u64 / u128 / bytes), and `EmitLoadLiteral`. Use it for far targets
and 128-bit constants rather than inlining.

Debugging: `armDisassembleAndDumpCode(ptr, size)` dumps emitted ARM64 — use it
liberally when JIT output is wrong.

---

## 2. Register allocation map (from `AsmHelpers.h` — already fixed)

These macros are **already defined and in use**. Do not reassign them.

| Macro | Reg | Role |
|---|---|---|
| `RXRET` / `RWRET` / `RQRET` | x0 / w0 / q0 | Return value |
| `RXARG1..4` / `RWARG1..4` | x0–x3 / w0–w3 | Call arguments (AAPCS64) |
| `RXVIXLSCRATCH` / `RWVIXLSCRATCH` | x16 / w16 | VIXL internal scratch — **do not hold state here** |
| `RSCRATCHADDR` | x17 | Address-calculation scratch |
| `RQSCRATCH*` | q30 / d30 / s30 | Vector scratch #1 (also `RQSCRATCHI/F/D` views) |
| `RQSCRATCH2*` | q31 / d31 / s31 | Vector scratch #2 |
| `RQSCRATCH3*` | q29 / d29 / s29 | Vector scratch #3 |

ABI reminders (AAPCS64 / macOS):
- **x18 is reserved by the OS on macOS — never use it.**
- x29 = FP, x30 = LR, sp / xzr special.
- Callee-saved GPRs: **x19–x28** (+ x29/x30). Callee-saved SIMD: **v8–v15** (low 64 bits only).
- `armIsCalleeSavedRegister(reg)` tells you if a reg must be preserved.

**To be assigned by the EE rec (Phase 1, document here when chosen):** persistent
callee-saved registers for hot state — proposed `x19` = `&cpuRegs`, `x20` = fastmem
base, `x21` = vtlb table base. Confirm and record the final choice in this section
once Phase 1.1 lands, so every later phase uses the same regs.

Guest→host mapping intent:
- EE/VU 128-bit registers → NEON `v0–v31` (q regs). 64-bit GPR halves via `ldp/stp`.
- 32-bit MIPS GPRs (IOP) → ARM64 w-registers.
- MIPS FPU (COP1) → `s0–s31` (single) / `d0–d31` (double).

---

## 3. Build / test loop (do this every 1–2 functions)

```bash
cmake --build build --target pcsx2-qt -j18   # incremental
# fix errors, repeat. Then:
cmake --build build --target unittests -j18 && ctest --test-dir build
```

Run with logs visible:
```bash
build/pcsx2-qt/PCSX2.app/Contents/MacOS/PCSX2
```

Validation ladder (see PROGRESS.md "Test ladder"): unittests → BIOS boot → 2D game
→ IOP-heavy game → 3D game.

---

## 4. Correctness discipline

- **Interpreter is ground truth.** When JIT output diverges, diff against the C++
  semantics in `Interpreter.cpp` (EE), `R3000AInterpreter.cpp` (IOP),
  `VU0microInterp.cpp` / `VU1microInterp.cpp` (VU). Opcode dispatch:
  `R5900OpcodeTables.cpp`.
- **Interpreter fallback is allowed.** Rare/complex ops may call back via
  `recCall(Interp::...)` as a first pass; optimize later. Mark such TODOs clearly.
- **x86 is the reference implementation, never the thing to break.** Mirror the
  structure of `pcsx2/x86/` (`iR5900*.cpp`, `recVTLB.cpp`, `iR3000A.cpp`,
  `microVU*`), translating x86emitter calls to VIXL.

---

## 5. Code placement & guards

- New ARM64 rec files live in `pcsx2/arm64/` (`aR5900.{h,cpp}`, `aR3000A.{h,cpp}`,
  `microVU*` etc.) and are registered in `pcsx2/CMakeLists.txt`
  (`pcsx2arm64Sources` / `pcsx2arm64Headers`, ~lines 1052–1061; vixl link ~1078).
- Shared call sites (e.g. `VMManager.cpp`) gate ARM64 paths with `#ifdef _M_ARM64`
  / `#ifndef _M_X86`. **Never** remove or weaken the x86 path.
- The `_M_X86` TODO guards in `VMManager.cpp` (2671, 2695, 2720, 2740) are the
  hook points — extend them to cover ARM64 as each rec comes online.

---

## 6. Git hygiene

- Branch: `armjit`.
- Atomic commits, one opcode family / subtask each.
- Message format: `ARM64: <what>` — e.g. `ARM64: Add EE recompiler skeleton`,
  `ARM64: Implement recLB/recSB load-store`.
- Commit doc updates (PROGRESS + JOURNAL) with or right after the code change.
