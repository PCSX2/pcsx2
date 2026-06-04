# PCSX2 Native Apple Silicon (ARM64) Build Analysis

> Analysis Date: 2026-06-03
> PCSX2 Commit: 57e46f07f (current HEAD at time of analysis)

## Executive Summary

**PCSX2 can already build natively for Apple Silicon (ARM64)**, and there is active CI infrastructure for it. However, it is **functionally incomplete** for playable performance. The native ARM64 build compiles and runs, but it falls back to CPU interpreters for the core PS2 processors (EE, IOP, VU0, VU1) because the JIT recompilers — which provide the performance necessary for full-speed emulation — have not been ported from x86-64 to ARM64.

This means:
- You **can** build a native Apple Silicon binary today (`cmake -DCMAKE_OSX_ARCHITECTURES=arm64 ...`).
- It **will run** without Rosetta.
- Games **will run extremely slowly** because there are no EE/VU/IOP recompilers.

---

## What Already Works (Apple Silicon / ARM64)

### 1. Build System Support

The CMake build system fully recognizes ARM64 on macOS:

- `cmake/BuildParameters.cmake:113-123` detects `arm64` / `aarch64` and sets `ARCH_ARM64=TRUE`.
- It compiles with `-march=armv8.4-a -mcpu=apple-m1` on Apple Silicon.
- CI already builds and tests ARM64 macOS binaries (`.github/workflows/macos_build_matrix.yml`, `.github/workflows/macos_build.yml`).
- The build uses `DISABLE_ADVANCE_SIMD=ON` and `CMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF` for arm64.

### 2. Graphics Subsystem (GS) — Mostly Complete

The Graphics Synthesizer (GS) emulator has solid ARM64 support:

| Component | ARM64 Status | Notes |
|-----------|-------------|-------|
| **Metal Renderer** | ✅ Complete | Full `GSDeviceMTL.mm` implementation (~108 KB) with all shaders |
| **Vulkan Renderer** | ✅ Supported | Uses `VK_USE_PLATFORM_METAL_EXT` for MoltenVK on macOS |
| **GS SW Renderer (ARM64 JIT)** | ✅ Implemented | `GSDrawScanlineCodeGenerator.arm64.cpp` (~58 KB) and `GSSetupPrimCodeGenerator.arm64.cpp` (~8.7 KB) |
| **NEON Vector Math** | ✅ Implemented | `GSVector4i_arm64.h`, `GSVector4_arm64.h` replace SSE/AVX intrinsics |
| **Multi-ISA** | ✅ Not needed | ARM64 doesn't use the x86 Multi-ISA dispatch system |

### 3. VIF (Vector Interface) Dynarec — Implemented

The VIF unpack dynarec (which unpacks PS2 geometry data) **has been ported** to ARM64:

- `pcsx2/arm64/Vif_Dynarec.cpp` — JIT code generator for VIF unpack operations.
- `pcsx2/arm64/Vif_UnpackNEON.cpp` — NEON-accelerated unpack routines.
- `pcsx2/arm64/Vif_UnpackNEON.h` — Header definitions.

This is a meaningful optimization: VIF unpacking is used heavily in GS-intensive games.

### 4. Common / Platform Layer — Complete

- `common/Darwin/DarwinMisc.cpp` has ARM64-specific code (e.g., for memory barriers, signal handling).
- `common/Darwin/DarwinThreads.cpp` handles ARM64 thread context.
- Page size and cache line size detection works on Apple Silicon.
- `FastJmp`, `FPControl`, `HostSys` all have ARM64 paths.

### 5. CPU Interpreter Fallbacks — Exist and Work

The core CPU interpreters are architecture-agnostic and run fine on ARM64:
- `Interpreter.cpp` — EE (R5900) interpreter.
- `R3000AInterpreter.cpp` — IOP (PS1 CPU) interpreter.
- `VU0microInterp.cpp` / `VU1microInterp.cpp` — VU interpreters.

These are what the ARM64 build currently uses, but they are **orders of magnitude slower** than the recompilers.

---

## What Is MISSING (The Blockers for Playable Performance)

### 1. EE (Emotion Engine / R5900) Recompiler — NOT IMPLEMENTED

The EE recompiler translates PS2 MIPS instructions to native host machine code at runtime. This is **the single most important** performance component.

| Architecture | Lines of Code | Status |
|-------------|---------------|--------|
| **x86-64** | ~27,000+ lines (`pcsx2/x86/`, `pcsx2/x86/ix86-32/`) | ✅ Complete, heavily optimized |
| **ARM64** | **0 lines** (only stubs) | ❌ Missing entirely |

**Stub evidence:**
- `VMManager.cpp:2727` on ARM64: `Cpu = &intCpu;` (forces interpreter, no recompiler option).
- `VMManager.cpp:2671`: `#ifdef _M_X86 // TODO(Stenzek): Remove me once EE/VU/IOP recs are added.`
- `pcsx2/arm64/RecStubs.cpp:13`: `pxFailRel("Not implemented.");` for `vtlb_DynBackpatchLoadStore`.

The x86 EE recompiler includes:
- `x86/iR5900*.cpp/h` — Main R5900 recompiler (arith, branch, load/store, MMI, COP0, COP1, etc.).
- `x86/ix86-32/iR5900.cpp` — 32-bit x86 backend (still used in 64-bit builds for some constraints).
- `x86/microVU*.cpp/h` — microVU0/microVU1 recompilers (tightly coupled with EE).

### 2. IOP (IO Processor / R3000A) Recompiler — NOT IMPLEMENTED

The IOP recompiler is similarly x86-only:

- `x86/iR3000A.cpp` — IOP recompiler (~3,000+ lines).
- ARM64 fallback: `psxCpu = &psxInt;` (interpreter only).

### 3. VU (Vector Unit) Recompilers — NOT IMPLEMENTED

Both VU0 and VU1 recompilers are missing on ARM64:

- `x86/microVU.cpp` + `microVU_*.inl` — microVU recompiler (~15,000+ lines of inline/template-heavy JIT code).
- ARM64 fallback: `CpuVU0 = &CpuIntVU0; CpuVU1 = &CpuIntVU1;` (interpreter only).

**Impact:** The VU recompilers are critical for 3D geometry processing. Without them, games crawl.

### 4. vtlb (Virtual TLB) Fast Memory Path — NOT IMPLEMENTED

The `vtlb_DynBackpatchLoadStore` function in `pcsx2/arm64/RecStubs.cpp` is stubbed:

```cpp
void vtlb_DynBackpatchLoadStore(...)
{
  pxFailRel("Not implemented.");
}
```

This function is part of the fast memory access system that the recompilers use. On x86, it patches generated code at runtime to use direct memory access instead of going through the virtual TLB lookup. Without it, even if a recompiler existed, memory accesses would be slower.

### 5. Save State Compatibility — PARTIALLY STUBBED

`SaveStateBase::vuJITFreeze()` in `pcsx2/arm64/RecStubs.cpp` is hacked:

```cpp
bool SaveStateBase::vuJITFreeze()
{
    ...
    Console.Warning("recompiler state is stubbed in arm64!");
    // HACK!!
    std::array<u8,96> empty_data{};
    Freeze(empty_data);
    Freeze(empty_data);
    return true;
}
```

This means save states work on ARM64, but they don't preserve any JIT state (which is fine because there is no JIT to preserve yet). Once recompilers are added, this will need real implementation.

---

## Summary Table: ARM64 Completeness

| Subsystem | ARM64 Status | Effort to Complete |
|-----------|-------------|-------------------|
| Build System | ✅ Done | — |
| Common / Platform | ✅ Done | — |
| GS Metal Renderer | ✅ Done | — |
| GS Vulkan Renderer | ✅ Done | — |
| GS SW Renderer (JIT) | ✅ Done | — |
| GS Vector Math (NEON) | ✅ Done | — |
| VIF Dynarec | ✅ Done | — |
| **EE Recompiler** | ❌ **Missing** | **Very Large** (~15-20k lines) |
| **IOP Recompiler** | ❌ **Missing** | **Medium** (~3-5k lines) |
| **VU0/VU1 Recompilers** | ❌ **Missing** | **Very Large** (~15-20k lines) |
| vtlb Backpatch | ❌ **Stubbed** | **Medium** |
| Save State (JIT freeze) | ⚠️ Stubbed | Small (after recs exist) |

---

## Key Code Metrics

```
Architecture   Total LOC   Recompiler LOC   Status
x86-64         ~35,800     ~27,000+         Complete
ARM64          ~1,800      ~0 (stubs only)  Incomplete
```

The ARM64 directory (`pcsx2/arm64/`) contains only:
- `AsmHelpers.cpp/h` — VIXL assembler helpers (~19 KB).
- `Vif_Dynarec.cpp` — VIF unpack JIT (~15 KB).
- `Vif_UnpackNEON.cpp/h` — NEON unpack routines (~13 KB).
- `RecStubs.cpp` — Stubs for missing recompiler interfaces (~4 KB).

This is dwarfed by the x86 recompiler codebase, which is roughly **20× larger**.

---

## Can You Build Native Apple Silicon Today?

**Yes**, with these steps (based on the CI workflow):

```bash
# Install dependencies (Homebrew)
brew install ccache nasm cmake

# Build dependencies (the project uses a wrapper script)
.github/workflows/scripts/macos/build-dependencies-universal.sh "$HOME/deps"

# Configure
cmake -DCMAKE_PREFIX_PATH="$HOME/deps" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64" \
      -DDISABLE_ADVANCE_SIMD=ON \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
      -DUSE_LINKED_FFMPEG=ON \
      -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
      -B build .

# Build
make -C build -j$(sysctl -n hw.ncpu)
```

The resulting `PCSX2.app` in `build/pcsx2-qt/PCSX2.app` is a native ARM64 binary.

### Important Caveat

CMake prints a deliberate warning at configure time (`CMakeLists.txt:84-93`):

```
*************** UNSUPPORTED CONFIGURATION ***************
Apple Silicon support in PCSX2 is INCOMPLETE. There are
currently no EE/VU/IOP recompilers, and games will run
VERY slow. There is no date for completion yet, you
should set -DCMAKE_OSX_ARCHITECTURES=x86_64 for now,
unless you want to work on the recompilers.
*********************************************************
```

---

## Conclusion

The "missing piece" for a **native** Apple Silicon PCSX2 is not the build system, the graphics stack, or the platform layer — all of those are done. The missing piece is the **core JIT recompilers** for the PS2's processors (EE, IOP, VU0, VU1).

These recompilers represent the bulk of the architecture-specific code in PCSX2 (~35,000 lines on x86). Porting them to ARM64 is a **massive undertaking** requiring:
1. Deep understanding of the PS2's MIPS/VU instruction sets.
2. Deep understanding of ARM64 assembly and the VIXL assembler library (already in use).
3. Careful handling of PS2 memory model semantics, TLB, and FPU behavior.
4. Extensive testing across a wide game compatibility matrix.

For now, the recommended path for users is still the **x86_64 build under Rosetta 2**, which leverages the mature x86 recompilers and runs at full speed.
