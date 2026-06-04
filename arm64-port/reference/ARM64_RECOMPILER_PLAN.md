# PCSX2 Native Apple Silicon (ARM64) Recompiler Implementation Plan

> Version: 1.0
> Date: 2026-06-03
> PCSX2 Commit: 57e46f07f

---

## Part 1: Build Environment Investigation

### 1.1 What We Learned from the Source Tree

The user's checkout is a fresh clone of the upstream PCSX2 repo. The project already has **full build infrastructure for ARM64 macOS**:

| Item | Status |
|------|--------|
| CMakeLists.txt ARM64 detection | ✅ Present — `ARCH_ARM64` is set when `CMAKE_HOST_SYSTEM_PROCESSOR` is `arm64` or `aarch64` |
| CI builds ARM64 macOS | ✅ `.github/workflows/macos_build_matrix.yml` has an `arm64` job |
| Dependencies can be built for ARM64 | ✅ `.github/workflows/scripts/macos/build-dependencies-universal.sh` supports `CMAKE_ARCH_ARM64` |
| VIXL assembler library | ✅ In-tree at `3rdparty/vixl/` (used by existing `pcsx2/arm64/` VIF dynarec) |
| CMake warning at configure | ⚠️ Prints strong warning that ARM64 is incomplete and slow |

### 1.2 How to Build Native ARM64 Today

Use these exact steps (derived from the CI workflow):

```bash
# --- Step A: Install system tools ---
# Requires: Homebrew is already installed on this machine (confirmed in /opt/homebrew)
brew install cmake ccache nasm

# --- Step B: Build 3rd-party dependencies ---
# This script downloads and compiles Qt6, SDL3, FFmpeg, shaderc, MoltenVK, etc.
# It takes 30-60 minutes on first run.
.github/workflows/scripts/macos/build-dependencies-universal.sh "$HOME/pcsx2-deps"

# --- Step C: Configure ---
# Note: -DDISABLE_ADVANCE_SIMD=ON is required for ARM64 because Multi-ISA is x86-only.
# Note: -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF avoids LTO issues on ARM64 in CI.
cmake -DCMAKE_PREFIX_PATH="$HOME/pcsx2-deps" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64" \
      -DDISABLE_ADVANCE_SIMD=ON \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
      -DUSE_LINKED_FFMPEG=ON \
      -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
      -B build .

# --- Step D: Compile ---
make -C build -j$(sysctl -n hw.ncpu)

# --- Step E: Sign ---
# Sign the unsigned dylib first
codesign --force --sign - \
  build/pcsx2-qt/PCSX2.app/Contents/Frameworks/libshaderc_shared.1.dylib

# Then re-sign the whole app bundle
codesign --force --deep --sign - \
  build/pcsx2-qt/PCSX2.app

# --- Result ---
# Native ARM64 binary: build/pcsx2-qt/PCSX2.app
# It will run without Rosetta, but games will be unplayably slow.
```

### 1.3 Build Verification Checklist

Before starting any recompiler work, an agent should verify it can build and run:

- [ ] `build-dependencies-universal.sh` completes without errors.
- [ ] `cmake` configuration succeeds (ignore the ARM64 warning).
- [ ] `make` compiles `PCSX2.app` successfully.
- [ ] `file build/pcsx2-qt/PCSX2.app/Contents/MacOS/PCSX2` shows `arm64` architecture.
- [ ] App launches and reaches the main UI.

---

## Part 2: Problem Statement (What Is Actually Missing)

### 2.1 The Gap in Numbers

| Architecture | Recompiler Code | Status |
|-------------|-----------------|--------|
| x86-64 | ~35,000 LOC in `pcsx2/x86/` | Complete, mature, heavily optimized |
| ARM64 | ~1,800 LOC in `pcsx2/arm64/` | Only VIF dynarec + stubs implemented |

**The missing pieces are the JIT recompilers for the PS2's processors:**

1. **EE Recompiler** (Emotion Engine / R5900 MIPS-III/IV) — the main CPU.
2. **IOP Recompiler** (I/O Processor / R3000A) — the PS1-compat CPU that handles I/O.
3. **VU0/VU1 Recompilers** (Vector Units) — custom vector processors for 3D math.
4. **vtlb DynBackpatch** — runtime memory access patching for fastmem.

### 2.2 Where to Look

**Key files defining the interface:**
- `pcsx2/R5900.h:309-353` — `R5900cpu` struct (interface for both interpreter and recompiler).
- `pcsx2/R3000A.h:181-191` — `R3000Acpu` struct (interface for IOP interpreter/recompiler).
- `pcsx2/VUmicro.h` — `BaseVUmicroCPU` and `recMicroVU0`/`recMicroVU1`.
- `pcsx2/VMManager.cpp:2669-2733` — CPU provider initialization; currently forces interpreter on ARM64.

**x86 reference implementations:**
- `pcsx2/x86/iR5900.cpp` — Main EE recompiler dispatcher (~73 KB).
- `pcsx2/x86/ix86-32/iR5900.cpp` — 32-bit x86 backend for core EE instructions (~73 KB).
- `pcsx2/x86/ix86-32/iR5900Arit.cpp` — Arithmetic ops.
- `pcsx2/x86/ix86-32/iR5900Branch.cpp` — Branch ops.
- `pcsx2/x86/ix86-32/iR5900LoadStore.cpp` — Load/store ops.
- `pcsx2/x86/ix86-32/recVTLB.cpp` — Fast memory TLB backpatching (~27 KB).
- `pcsx2/x86/iR3000A.cpp` — IOP recompiler.
- `pcsx2/x86/microVU*.cpp/h` + `microVU_*.inl` — VU recompilers (~15-20 KB of inline-heavy code).

**ARM64 existing infrastructure:**
- `pcsx2/arm64/AsmHelpers.h` — Register naming conventions, scratch registers, `ArmConstantPool`.
- `pcsx2/arm64/AsmHelpers.cpp` — Assembler helpers, block management, jump/call emission, stack frame helpers.
- `pcsx2/arm64/Vif_Dynarec.cpp` — **Already working VIF unpack JIT** (good reference for VIXL patterns).
- `pcsx2/arm64/Vif_UnpackNEON.cpp` — NEON vector unpacks (shows vector register allocation).
- `pcsx2/arm64/RecStubs.cpp` — Stubs that fail at runtime (`pxFailRel`).

**Build system integration:**
- `pcsx2/CMakeLists.txt:1052-1061` — `pcsx2arm64Sources` and `pcsx2arm64Headers` lists.
- `pcsx2/CMakeLists.txt:1078-1080` — Links `vixl` for ARM64.

---

## Part 3: Architecture Design for ARM64 Recompilers

### 3.1 Design Principles (Portable from x86 Design)

The x86 recompilers have these architectural layers:

```
┌─────────────────────────────────────────────┐
│  Instruction dispatcher (per opcode family) │  ← reads MIPS opcode, dispatches to generator
├─────────────────────────────────────────────┤
│  Register allocation / caching layer        │  ← tracks which host regs hold which guest regs
├─────────────────────────────────────────────┤
│  Code generator (emitter)                   │  ← emits host machine code (x86emitter / VIXL)
├─────────────────────────────────────────────┤
│  Memory access / vtlb layer                 │  ← translates guest addresses to host addresses
├─────────────────────────────────────────────┤
│  Block manager / cache                      │  ← manages compiled code cache, linking, invalidation
└─────────────────────────────────────────────┘
```

For ARM64, the **top 3 layers** must be reimplemented. The **bottom 2 layers** (`vtlb`, block cache) are mostly architecture-agnostic; only the backpatch trampolines and jump linking need ARM64 versions.

### 3.2 ARM64 Register Allocation Strategy

**Reference: x86 uses:**
- 8-16 GPRs for MIPS register caching (x86-32 + x86-64).
- 16 XMM registers for 128-bit GPR/FPU/VF caching.
- MMX registers for 64-bit temporaries.

**ARM64 strategy (proposed):**
- **31 GPRs** (`x0`–`x30`), but some are reserved per ABI:
  - `x18` — platform register (avoid on macOS).
  - `x29` (`fp`) — frame pointer.
  - `x30` (`lr`) — link register.
  - `sp` / `xzr` — stack pointer / zero register.
- Reserve a few callee-saved registers for the recompiler state pointer, fastmem base, etc.
- Use **NEON (`v0`–`v31`)** for 128-bit vector caching (equivalent to XMM).
- Use **VIXL `MacroAssembler`** for emitting, just like the existing `Vif_Dynarec.cpp`.

**Suggested reserved registers (from `AsmHelpers.h` conventions):**
- `x16` (`RXVIXLSCRATCH`) — VIXL internal scratch.
- `x17` (`RSCRATCHADDR`) — address calculations.
- `x18` (`x18`) — **avoid** on macOS (used by system).
- Choose 2-3 callee-saved regs for persistent state:
  - e.g., `x19` = pointer to `cpuRegs` struct, `x20` = fastmem base, `x21` = `vtlb` table base.

### 3.3 EE Recompiler (R5900) — High-Level Plan

The EE is a 64-bit MIPS-III/IV + custom coprocessors (COP0, COP1/FPU, COP2/VU).

**Phase 1: Skeleton and Block Manager**
1. Create `pcsx2/arm64/aR5900.h` — header with ARM64 equivalents of `iR5900.h` structs and macros.
2. Create `pcsx2/arm64/aR5900.cpp` — main dispatcher, block compilation loop.
   - Must implement `R5900cpu recCpu` interface (Reserve, Reset, Execute, Clear, Shutdown).
   - Copy the block-level logic from `x86/iR5900.cpp` but emit ARM64 via VIXL.
3. Hook into `VMManager.cpp` — remove `#ifdef _M_X86` guards around `recCpu.Reserve()` etc.

**Phase 2: Memory Access (Load/Store) — HIGHEST PRIORITY**
- The EE does a lot of memory access. These must use `vtlb` fast-path.
- Implement `vtlb_DynBackpatchLoadStore` in `RecStubs.cpp` (currently `pxFailRel`).
- Port `recVTLB.cpp` logic to ARM64: generate backpatch trampolines, emit direct memory loads when page is known.
- ARM64 tip: use `ldp`/`stp` for 128-bit loads (EE GPRs are 128-bit).

**Phase 3: Integer Arithmetic**
- Port `iR5900Arit.cpp`, `iR5900AritImm.cpp`, `iR5900Shift.cpp`, `iR5900Move.cpp`, `iR5900MultDiv.cpp`.
- Most MIPS integer ops have 1:1 ARM64 equivalents (`ADD`, `SUB`, `AND`, `ORR`, `EOR`, `LSL`, `LSR`, `ASR`, `MUL`, `UDIV`, `SDIV`).
- 128-bit GPR ops need NEON for the upper 64 bits.

**Phase 4: Branches & Jumps**
- Port `iR5900Branch.cpp`, `iR5900Jump.cpp`.
- Delay-slot handling is tricky: compile delay slot inline when possible, or use a software branch-delay buffer.
- On ARM64, conditional branches have limited range; use trampolines for far jumps (see `AsmHelpers.cpp` `armEmitJmp`).

**Phase 5: Coprocessors**
- **COP0 (System Control / TLB)** — often calls interpreter fallback (`recCall(Interp::...)`). Can be deferred.
- **COP1 (FPU)** — map MIPS FPU regs to ARM64 `d0-d31` (double) or `s0-s31` (single). Handle MIPS FPU control/flag register.
- **COP2 (VU0 macro mode)** — tightly coupled with VU recompiler. Can start with interpreter fallback.

**Phase 6: MMI Instructions**
- MMI is the EE's SIMD extension (128-bit integer ops). Map to ARM64 NEON where possible.
- `iMMI.cpp`, `iR5900Misc.cpp` contain these.

### 3.4 IOP Recompiler (R3000A) — High-Level Plan

The IOP is a simpler 32-bit MIPS-I CPU.

1. Create `pcsx2/arm64/aR3000A.cpp` and `aR3000A.h`.
2. The structure mirrors the EE recompiler but is simpler:
   - 32-bit GPRs, no 128-bit.
   - No MMI, no COP2.
   - Smaller instruction set.
3. Port `x86/iR3000A.cpp` logic to ARM64 VIXL.
4. Hook `psxRec` into `VMManager.cpp`.

### 3.5 VU Recompilers (microVU) — High-Level Plan

This is the **hardest** part. The VU has a custom instruction set, its own register file, and executes in parallel with the EE.

1. Study `x86/microVU*.cpp/h` and the `.inl` files extensively.
2. microVU uses a two-pass compile model:
   - Pass 1: Analyze the VU microcode block.
   - Pass 2: Emit host code.
3. The analysis pass is **architecture-agnostic** — reuse as-is.
4. The emission pass (`microVU_Compile.inl`, `microVU_Lower.inl`, `microVU_Upper.inl`) is x86-specific.
   - Must be cloned into ARM64 versions.
5. VU registers are 128-bit SIMD. Map to NEON `v0-v31`.
6. VU flag registers (`Status`, `MAC`, `Clip`) need careful handling.

**Recommended approach: Start with VU interpreter fallback.**
The existing `VMManager.cpp` already forces `CpuVU0 = &CpuIntVU0;` on ARM64. Leave this until EE and IOP recompilers are working.

### 3.6 vtlb DynBackpatch — High-Level Plan

This is a small but critical piece for fast memory access.

1. In `pcsx2/arm64/RecStubs.cpp`, replace the stub:
   ```cpp
   void vtlb_DynBackpatchLoadStore(...)
   {
       // Generate a trampoline that:
       // 1. Checks the guest address against the TLB lookup table.
       // 2. If valid, patches the caller to use a direct load/store.
       // 3. If invalid, raises an exception and falls back to slow path.
   }
   ```
2. Study `x86/ix86-32/recVTLB.cpp` for the exact x86 logic.
3. On ARM64, direct memory access from JIT requires:
   - Loading the fastmem base pointer into a register.
   - Adding the guest offset.
   - Using `ldr`/`str` (or NEON variants for 128-bit).
   - If the page isn't mapped, generate an exit trampoline to the C++ handler.

---

## Part 4: Detailed Implementation Roadmap

### Phase 0: Prerequisites & Tooling (1-2 days)

| Task | Details |
|------|---------|
| 0.1 | Build native ARM64 binary using steps in §1.2. Verify it launches. |
| 0.2 | Run unit tests: `make -C build unittests`. Note any failures. |
| 0.3 | Read the entire `pcsx2/x86/` directory. Understand every file's role. |
| 0.4 | Read `pcsx2/arm64/` thoroughly (`AsmHelpers`, `Vif_Dynarec`, `Vif_UnpackNEON`). |
| 0.5 | Study `3rdparty/vixl/include/vixl/aarch64/macro-assembler-aarch64.h` API. |
| 0.6 | Create a scratch test harness: a small standalone C++ program that uses VIXL to emit ARM64 code, maps memory with `HostSys`, and executes it. Verify you can call emitted code and it returns expected values. |

### Phase 1: EE Recompiler Skeleton (3-5 days)

| Task | Details |
|------|---------|
| 1.1 | Create `pcsx2/arm64/aR5900.h` with ARM64 register allocation struct definitions, `R5900cpu recCpu` extern, and macros equivalent to `REC_FUNC`, `REC_FUNC_DEL`, etc. |
| 1.2 | Create `pcsx2/arm64/aR5900.cpp` with empty `recCpu` struct functions. Hook them into `CMakeLists.txt`. Verify build still compiles. |
| 1.3 | Implement `recCpu.Reserve()`: allocate code cache memory via `HostSys`. Study `x86/BaseblockEx.cpp` for block allocation. |
| 1.4 | Implement basic block compile loop: read 1-2 MIPS instructions, emit NOPs via VIXL, return. Implement `recCpu.Execute()`: jump to compiled block, run, return. |
| 1.5 | Hook `recCpu` into `VMManager.cpp`: change `_M_X86` guards so ARM64 also calls `recCpu.Reserve()`, `recCpu.Shutdown()`, `recCpu.Reset()`. Temporarily keep `Cpu = &intCpu;` until recompiler works. |

### Phase 2: vtlb Fast Memory & Load/Store (5-7 days)

| Task | Details |
|------|---------|
| 2.1 | Implement `vtlb_DynBackpatchLoadStore` in `pcsx2/arm64/RecStubs.cpp`. Start with the slow path (call C++ handler), then add fast-path patching. |
| 2.2 | Port the vtlb backpatch trampoline logic. On ARM64, direct loads need a base register + offset. Design a calling convention for trampolines. |
| 2.3 | Implement EE load/store generator (`recLB`, `recLH`, `recLW`, `recLD`, `recLBU`, `recLHU`, `recLWU`, `recLDU`, `recSB`, `recSH`, `recSW`, `recSD`, `recLQ`, `recSQ`). `recLQ`/`recSQ` load/store 128-bit — use NEON `ld1`/`st1`. |
| 2.4 | Write a unit test or inline debug code that compiles a simple MIPS load/store block and verifies it accesses memory correctly. |

### Phase 3: EE Integer Arithmetic (3-5 days)

| Task | Details |
|------|---------|
| 3.1 | Port arithmetic immediate ops: `ADDI`, `ADDIU`, `SLTI`, `SLTIU`, `ANDI`, `ORI`, `XORI`, `LUI`, `DADDI`, `DADDIU`. |
| 3.2 | Port register-register arithmetic: `ADD`, `ADDU`, `SUB`, `SUBU`, `SLT`, `SLTU`, `AND`, `OR`, `XOR`, `NOR`, `DADD`, `DADDU`, `DSUB`, `DSUBU`. |
| 3.3 | Port shift ops: `SLL`, `SRL`, `SRA`, `SLLV`, `SRLV`, `SRAV`, `DSLL`, `DSRL`, `DSRA`, `DSLLV`, `DSRLV`, `DSRAV`, `DSLL32`, `DSRL32`, `DSRA32`. |
| 3.4 | Port move ops: `MOVZ`, `MOVN`, `MFHI`, `MTHI`, `MFLO`, `MTLO`, `MOVN` (the MIPS conditional moves map well to ARM64 `CSEL`). |
| 3.5 | Port multiply/divide: `MULT`, `MULTU`, `DIV`, `DIVU`, `DMULT`, `DMULTU`, `DDIV`, `DDIVU`. Use ARM64 `MUL`/`SMULH`/`UMULH`/`SDIV`/`UDIV`. |
| 3.6 | Implement constant propagation (`EE_CONST_PROP`). Track which guest GPRs are known constants and emit immediate forms. |

### Phase 4: EE Branches & Jumps (3-5 days)

| Task | Details |
|------|---------|
| 4.1 | Implement unconditional jumps: `J`, `JAL`, `JR`, `JALR`. Handle PC updating and delay slots. |
| 4.2 | Implement conditional branches: `BEQ`, `BNE`, `BLEZ`, `BGTZ`, `BLTZ`, `BGEZ`, `BLTZAL`, `BGEZAL`, and their likely variants (`BEQL`, `BNEL`, etc.). |
| 4.3 | Implement delay slot compilation. Strategy: compile delay slot inline before the branch decision. For "likely" branches, conditionally execute the delay slot. |
| 4.4 | Implement block linking: when a block ends with a jump to a known target, emit a direct branch to the already-compiled target block. |
| 4.5 | Implement block invalidation when TLB mappings change. |

### Phase 5: EE Coprocessors (5-7 days, can defer some to interpreter)

| Task | Details |
|------|---------|
| 5.1 | **COP0**: Most COP0 ops are rare on the hot path. Implement as `recCall(Interp::...)` fallbacks initially. |
| 5.2 | **COP1 (FPU)**: Map 32 MIPS FPRs to ARM64 `d0-d31`. Implement `ADD.S`, `SUB.S`, `MUL.S`, `DIV.S`, `SQRT.S`, `ABS.S`, `MOV.S`, `NEG.S`, `CVT.*`, `C.*.S`, `C.*.D`, `BC1T`, `BC1F`. Handle FPU control/status register. |
| 5.3 | **COP2 (VU0 macro)**: Start with interpreter fallback. This is the interface between EE and VU0. |
| 5.4 | **MMI**: Map MMI ops to NEON where possible (128-bit integer SIMD). `x86/iMMI.cpp` is the reference. MMI is heavily used by many games. |

### Phase 6: IOP Recompiler (3-5 days)

| Task | Details |
|------|---------|
| 6.1 | Create `pcsx2/arm64/aR3000A.cpp` + `aR3000A.h`. |
| 6.2 | Implement `psxRec` interface (Reserve, Reset, ExecuteBlock, Clear, Shutdown). |
| 6.3 | Port IOP integer, load/store, branch, and coprocessor instruction generators. The IOP is much simpler than the EE. |
| 6.4 | Hook into `VMManager.cpp`. |

### Phase 7: VU Recompilers (10-15 days — DEFER until EE+IOP work)

| Task | Details |
|------|---------|
| 7.1 | Study `x86/microVU*.cpp/h` and `microVU_*.inl` in depth. |
| 7.2 | Create `pcsx2/arm64/microVU*.cpp/h` cloning the architecture-agnostic analysis pass. |
| 7.3 | Reimplement the VIXL-based emission pass for VU upper/lower instructions. |
| 7.4 | Map VU 128-bit VF registers to NEON `v0-v31`. Map VI registers to ARM64 GPRs. |
| 7.5 | Handle VU flag registers (`Status`, `MAC`, `Clip`) in NEON/GPR. |
| 7.6 | Test with games that heavily use VU1 (most 3D games). |

### Phase 8: Integration, Testing & Polish (ongoing)

| Task | Details |
|------|---------|
| 8.1 | Remove ARM64 warning from `CMakeLists.txt:84-93`. |
| 8.2 | Implement real `SaveStateBase::vuJITFreeze()` once VU recompilers exist. |
| 8.3 | Run the full unit test suite on ARM64. |
| 8.4 | Test game compatibility matrix. Start with simple 2D games, then move to 3D. |
| 8.5 | Profile and optimize hot paths (branch prediction, register allocation heuristics). |
| 8.6 | Enable LTO for ARM64 if it proves stable. |
| 8.7 | Address any macOS-specific issues (entitlements, Metal shader compilation, MoltenVK). |

---

## Part 5: Critical Files for the Agent to Understand

### Must-Read Files (in order)

1. `pcsx2/VMManager.cpp:2669-2733` — How CPU providers are selected and initialized.
2. `pcsx2/R5900.h` — EE CPU state struct, `R5900cpu` interface, opcode table macros.
3. `pcsx2/R3000A.h` — IOP CPU state struct, `R3000Acpu` interface.
4. `pcsx2/VUmicro.h` — VU CPU interface.
5. `pcsx2/x86/iR5900.h` — x86 EE recompiler header (register allocation, macros).
6. `pcsx2/x86/iCore.h` — x86 register allocation structures (`_x86regs`, `_xmmregs`, `EEINST`).
7. `pcsx2/x86/iR5900.cpp` — Main EE recompiler block compiler.
8. `pcsx2/x86/ix86-32/iR5900.cpp` — Core EE instruction emission.
9. `pcsx2/x86/ix86-32/recVTLB.cpp` — Fast memory TLB backpatching.
10. `pcsx2/arm64/AsmHelpers.h/cpp` — Existing ARM64 assembler infrastructure.
11. `pcsx2/arm64/Vif_Dynarec.cpp` — Working example of VIXL-based JIT in PCSX2.
12. `pcsx2/vtlb.cpp` / `pcsx2/vtlb.h` — Virtual TLB implementation.
13. `pcsx2/Interpreter.cpp` — EE interpreter (fallback reference for semantics).
14. `pcsx2/R5900OpcodeTables.cpp` — Opcode dispatch tables.

### Key Design Patterns in x86 Recompilers

- **Register Allocation**: `_allocX86reg`, `_allocXMMreg`, `_checkX86reg`, `_freeX86reg` — track which host registers hold which guest state.
- **Liveness Analysis**: `EEINST` struct per instruction records which registers are read/written, enabling smart allocation.
- **Constant Propagation**: `g_cpuHasConstReg` bitmask tracks known-constant guest registers.
- **Block Compilation**: Recompile a contiguous sequence of MIPS instructions until a branch/jump/exception boundary.
- **Delay Slots**: MIPS branch instructions have a delay slot. The recompiler compiles it inline.
- **Interpreter Fallback**: Complex/rare instructions call back to the C++ interpreter via `recCall(Interp::...)`. This is acceptable and reduces initial work.

---

## Part 6: Agent Workflow Recommendations

### Build/Test Loop

The agent should follow this tight loop:

```
1. Make small, focused changes (1-2 functions at a time).
2. Build: cmake --build build --target pcsx2-qt (or the relevant target).
3. Fix compile errors.
4. Run unit tests.
5. If possible, boot a simple game and check the log for new errors.
6. Commit the change with a descriptive message.
7. Repeat.
```

### Git Hygiene

- Create a feature branch: `git checkout -b arm64-recompilers`.
- Make atomic commits: one commit per opcode family or per subtask.
- Use clear commit messages: `ARM64: Add recLB/recSB load/store generators`.

### Testing Strategy

1. **Unit tests first**: `make -C build unittests` should always pass.
2. **BIOS boot test**: Boot the PS2 BIOS (no disc). It uses fewer complex instructions than games.
3. **Simple 2D game**: e.g., *Gradius III and IV*, *Castlevania: Symphony of the Night* (PS1 mode, uses IOP heavily).
4. **3D game**: e.g., *Final Fantasy X* (heavy EE + VU usage).

### Debugging Tools

- Use `Console.Warning()` and `Console.WriteLn()` liberally during development.
- PCSX2 has a built-in debugger accessible from the Qt UI.
- For low-level JIT debugging, use `armDisassembleAndDumpCode()` from `AsmHelpers.cpp` to dump emitted ARM64 code.
- macOS `lldb` can be attached to PCSX2 to inspect crashes in JIT code.

---

## Part 7: Risk Assessment & Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| VIXL assembler limitations | Low | Medium | VIXL MacroAssembler is mature; edge cases can be worked around with raw instruction encoding. |
| macOS page size differences (4K vs 16K) | Medium | High | Already handled in `cmake/BuildParameters.cmake` and `DarwinMisc.cpp`. Verify at runtime. |
| MMI instruction complexity | High | High | Start with interpreter fallback for MMI. Add NEON implementations incrementally. |
| VU recompiler is massive | High | Very High | Defer until EE+IOP are solid. VU interpreter fallback is acceptable for many games. |
| Save state compatibility | Medium | Medium | `RecStubs.cpp` has a hack. Replace with real freeze once recompilers work. |
| FPU flag differences (MIPS vs IEEE-754) | Medium | High | MIPS FPU has unique NaN/rounding behavior. Test thoroughly with FPU-heavy games. |

---

## Appendix A: Estimated Timeline

| Phase | Estimated Duration | Cumulative |
|-------|-------------------|------------|
| 0: Prerequisites & Tooling | 1-2 days | 2 days |
| 1: EE Skeleton | 3-5 days | 7 days |
| 2: vtlb + Load/Store | 5-7 days | 14 days |
| 3: EE Integer Arithmetic | 3-5 days | 19 days |
| 4: EE Branches & Jumps | 3-5 days | 24 days |
| 5: EE Coprocessors (basic) | 5-7 days | 31 days |
| 6: IOP Recompiler | 3-5 days | 36 days |
| Subtotal (Playable 2D) | ~5 weeks | — |
| 7: VU Recompilers | 10-15 days | 51 days |
| 8: Integration & Polish | 5-10 days | 61 days |
| **Total (Playable 3D)** | **~8-10 weeks** | — |

These are **optimistic** estimates assuming a full-time agent with deep systems programming and assembly knowledge. The actual timeline could be 2-3× longer depending on debugging complexity.

---

## Appendix B: Key Abbreviations & Terminology

| Term | Meaning |
|------|---------|
| EE | Emotion Engine — main PS2 CPU (MIPS-III/IV hybrid, 128-bit GPRs) |
| IOP | I/O Processor — PS1-compatible CPU handling I/O (MIPS-I, 32-bit) |
| VU0 / VU1 | Vector Units — custom SIMD coprocessors for 3D math |
| VIF | Vector Interface — unpacks DMA data into VU memory |
| GS | Graphics Synthesizer — PS2 GPU (emulated, not recompiled) |
| TLB | Translation Lookaside Buffer — virtual-to-physical address cache |
| vtlb | PCSX2's virtual TLB implementation |
| JIT | Just-In-Time (re)compiler |
| Dynarec | Dynamic recompiler (same as JIT) |
| VIXL | ARM's assembler library for generating ARM64 code at runtime |
| NEON | ARM's SIMD instruction set (128-bit, equivalent to SSE/AVX) |
| MMI | Multimedia Instructions — EE's custom 128-bit integer SIMD |
| COP0/1/2 | Coprocessor 0 (system), 1 (FPU), 2 (VU0 macro mode) |
