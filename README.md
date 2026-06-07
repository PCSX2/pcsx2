# ARMSX2 — Native ARM64 JIT Fork

> 🍎 **This fork is dedicated to bringing native ARM64 JIT recompilers to ARMSX2 so that Apple Silicon Macs (and other ARM64 platforms) can run PS2 games at playable speed without Rosetta 2.**
>
> It tracks upstream ARMSX2 closely and adds a complete ARM64 recompiler backend.

[![MacOS Build Status](https://img.shields.io/github/actions/workflow/status/isztldav/pcsx2/macos_build_matrix.yml?branch=macOS&label=%F0%9F%8D%8E%20MacOS%20Builds)](https://github.com/isztldav/pcsx2/actions/workflows/macos_build_matrix.yml)

ARMSX2 is a free and open-source PlayStation 2 (PS2) emulator. Its purpose is to emulate the PS2's hardware, using a combination of MIPS CPU [Interpreters](<https://en.wikipedia.org/wiki/Interpreter_(computing)>), [Recompilers](https://en.wikipedia.org/wiki/Dynamic_recompilation) and a [Virtual Machine](https://en.wikipedia.org/wiki/Virtual_machine) which manages hardware states and PS2 system memory. This allows you to play PS2 games on your PC, with many additional features and benefits.

## 🍎 About This Fork

[![Project Demo](https://img.youtube.com/vi/6Vm1rQ5AR3Y/maxresdefault.jpg)](https://www.youtube.com/watch?v=6Vm1rQ5AR3Y)

The upstream PCSX2 project ships an ARM64 *interpreter* build for macOS, but its high-performance **JIT recompilers** (EE, IOP, VU0, VU1, and vtlb fast memory) are x86-64 only. On Apple Silicon that means either running under Rosetta 2 (emulated x86-64, slower and deprecated by Apple) or falling back to the interpreter core (orders of magnitude too slow for most games).

**This fork exists to close that gap.** It is a line-for-line ARM64 port of the existing, battle-tested x86-64 JIT recompilers. The recompiler *architecture*, *block model*, *analysis passes*, and *JIT logic* are intentionally kept identical to upstream — what changes is the backend emitter: x86-64 assembly (x86emitter) is translated to ARM64 assembly via [VIXL](https://github.com/Linaro/vixl).

**Current status:**
- ✅ EE (Emotion Engine) recompiler — integer, float, MMI, COP0/COP1/COP2, branches, load/store
- ✅ IOP (I/O Processor / R3000A) recompiler — full integer, load/store, branches, coprocessors
- ✅ VU (Vector Unit) recompiler — microVU skeleton + Upper FMAC vector ISA complete; Lower ISA and runtime complete
- 🔄 vtlb fast memory — slow path works; fastmem backpatch still TODO
- ✅ Native ARM64 binary builds and boots the PS2 BIOS
- ✅ 2D games are already playable
- ✅ PS1 games (IOP mode) run at full speed — e.g. *Gran Turismo 2* is fully playable
- ✅ 3D games run (if crash try disabling MTVU)

Native Apple Silicon builds will be provided as automated releases soon. For now you must build manually (see **Building on Apple Silicon** below).

### Why LLMs / AI Were Used

A word on methodology, because this comes up a lot.

The x86-64 JIT code in upstream ARMSX2 is **already proven correct** — it has run thousands of PS2 titles for years. The challenge in this port is not emulator design or JIT theory; it is **mechanical translation** of a large, well-understood x86-64 assembly codebase into equivalent ARM64 assembly (via VIXL) while preserving the exact same register-allocation contracts, block lifecycle, and recompiler semantics.

Large language models (LLMs) were used as an **accelerant for this translation work** — pattern-matching x86 JIT boilerplate to ARM64 equivalents, scaffolding emit routines, and keeping the porting velocity high. The JIT *logic* (block compiler, dispatcher, analysis passes, flag pipelines, clamping rules, Tri-Ace hacks, etc.) is taken directly from the upstream x86 implementation and validated against it. **Nothing was hallucinated from scratch.**

In other words: the hard engineering was done by the ARMSX2 team over two decades. The hard *typing* — translating ~50k lines of x86 emitter code into ARM64 — is what AI helped compress.

## Project Details

PCSX2 has been in development for more than 20 years. Past versions could only run a few public domain game demos, but newer versions can run most games at full speed, including popular titles such as Final Fantasy X and Devil May Cry 3. Visit the [PCSX2 compatibility list](https://pcsx2.net/compat/) to check the latest compatibility status of games (with more than 2500 titles tested).

Installers and binaries for both stable and nightly builds are available from [our website](https://pcsx2.net/downloads/).

## System Requirements

ARMSX2 supports Windows, Linux, and Mac platforms. Our [setup documentation page](https://pcsx2.net/docs/setup/requirements) contains additional details on software and hardware requirements.

Please note that a BIOS dump from a legitimately-owned PS2 console is required to use the emulator. For more information, visit [this page](https://pcsx2.net/docs/setup/bios/).

### Apple Silicon (macOS)

| Requirement | Notes |
|---|---|
| macOS 12+ | Monterey or later |
| Apple Silicon (M1/M2/M3/M4) | Native ARM64 JIT; Rosetta 2 not required |
| 8 GB RAM minimum | 16 GB recommended for heavier titles |
| BIOS | Same requirement as x86-64 builds |

## Building on Apple Silicon

> Pre-built releases are coming soon. Until then, build from source.

Prerequisites: Xcode command-line tools, CMake, Qt6, and the ARMSX2 dependency bundle.

```bash
# 1. Dependencies can be built using
bash .github/workflows/scripts/macos/build-dependencies-universal.sh "path/to/pcsx2-deps"
```

```bash
# 2. Configure (one-time)
cmake -DCMAKE_PREFIX_PATH="/path/to/pcsx2-deps" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64" \
      -DDISABLE_ADVANCE_SIMD=ON \
      -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
      -DUSE_LINKED_FFMPEG=ON \
      -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON \
      -B build .

# 3. Build
cmake --build build --target pcsx2-qt -j$(sysctl -n hw.ncpu)

# 4. Post-process macOS bundle (required!)
cmake --build build --target pcsx2-postprocess-bundle
codesign --force --deep --sign - build/pcsx2-qt/ARMSX2.app

# 5. Run
open build/pcsx2-qt/ARMSX2.app
```

See `arm64-port/CONVENTIONS.md` for the full build/test/debug loop used by the port team.

## Roadmap

| Phase | Status | Description |
|---|---|---|
| 0 | ✅ Done | Build, tooling, VIXL scratch harness |
| 1 | ✅ Done | EE recompiler skeleton (dispatcher, block compiler, constant pool) |
| 2 | 🔄 Partial | vtlb fast memory — slow path done, fastmem backpatch TODO |
| 3 | ✅ Done | EE integer arithmetic (ALU, shifts, mul/div, MMI) |
| 4 | ✅ Done | EE branches, jumps, delay slots, recLUT + block linking |
| 5 | ✅ Done | EE coprocessors (COP0 inline, COP1 FPU, COP2 macro fallback, MMI SIMD) |
| 6 | ✅ Done | IOP recompiler (R3000A: integer, load/store, branches, COP0/COP2) |
| 7 | ✅ Done | VU recompiler (microVU) — Upper ISA done, Lower ISA done |
| 8 | 📋 Planned | Integration, testing, profiling, polish, release builds |
