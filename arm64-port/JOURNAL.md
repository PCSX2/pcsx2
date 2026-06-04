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
