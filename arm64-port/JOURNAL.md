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
