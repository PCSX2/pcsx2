# ARMSX2 — Native ARM64 JIT Fork

> 🍎 **This fork is dedicated to bringing native ARM64 JIT recompilers to ARMSX2 so that Apple Silicon Macs (and other ARM64 platforms) can run PS2 games at playable speed without Rosetta 2.**
>
> It tracks upstream ARMSX2 closely and adds a complete ARM64 recompiler backend, from **one shared core** that drives the ARM64 PC builds (macOS, Windows, Linux) **and** the Android and iOS apps.

[![MacOS Build Status](https://img.shields.io/github/actions/workflow/status/ARMSX2/ARMSX2/macos_build_matrix.yml?branch=macOS&label=%F0%9F%8D%8E%20MacOS%20Builds)](https://github.com/ARMSX2/ARMSX2/actions/workflows/macos_build_matrix.yml)
[![All Platforms](https://img.shields.io/github/actions/workflow/status/ARMSX2/ARMSX2/build-all.yml?branch=macOS&label=All%20Platforms)](https://github.com/ARMSX2/ARMSX2/actions/workflows/build-all.yml)

> [!IMPORTANT]
> **The repository is being restructured into a single-core monorepo.** The `macOS`
> branch is the canonical core and is becoming `main`; the Android and iOS apps now
> live as thin frontends under `platforms/`. If you are a collaborator, read
> **[Repository Layout](#-repository-layout)** and **[Monorepo Refactor — In Progress](#-monorepo-refactor--in-progress)** below before branching, and see
> [`REFACTOR_STATUS.md`](REFACTOR_STATUS.md) for the full plan.

ARMSX2 is a free and open-source PlayStation 2 (PS2) emulator. Its purpose is to emulate the PS2's hardware, using a combination of MIPS CPU [Interpreters](<https://en.wikipedia.org/wiki/Interpreter_(computing)>), [Recompilers](https://en.wikipedia.org/wiki/Dynamic_recompilation) and a [Virtual Machine](https://en.wikipedia.org/wiki/Virtual_machine) which manages hardware states and PS2 system memory. This allows you to play PS2 games on your PC, with many additional features and benefits.

## 🍎 About This Fork

[![Project Demo](https://img.youtube.com/vi/a1_zydGhVaE/maxresdefault.jpg)](https://www.youtube.com/watch?v=a1_zydGhVaE)

The upstream PCSX2 project ships an ARM64 *interpreter* build for macOS, but its high-performance **JIT recompilers** (EE, IOP, VU0, VU1, and vtlb fast memory) are x86-64 only. On Apple Silicon that means either running under Rosetta 2 (emulated x86-64, slower and deprecated by Apple) or falling back to the interpreter core (orders of magnitude too slow for most games).

**This fork exists to close that gap.** It is a line-for-line ARM64 port of the existing, battle-tested x86-64 JIT recompilers. The recompiler *architecture*, *block model*, *analysis passes*, and *JIT logic* are intentionally kept identical to upstream — what changes is the backend emitter: x86-64 assembly (x86emitter) is translated to ARM64 assembly via [VIXL](https://github.com/Linaro/vixl).

**Current status:**
- ✅ EE (Emotion Engine) recompiler — integer, float, MMI, COP0/COP1/COP2, branches, load/store
- ✅ IOP (I/O Processor / R3000A) recompiler — full integer, load/store, branches, coprocessors
- ✅ VU (Vector Unit) recompiler — microVU skeleton + Upper FMAC vector ISA complete; Lower ISA and runtime complete
- ✅ vtlb fast memory
- ✅ Native ARM64 binary builds and boots the PS2 BIOS
- ✅ 2D games are already playable
- ✅ PS1 games (IOP mode) run at full speed — e.g. *Gran Turismo 2* is fully playable
- ✅ 3D games run (if crash try disabling MTVU)

**Native Apple Silicon** builds are provided.

### Why LLMs / AI Were Used

A word on methodology:

The x86-64 JIT code in upstream ARMSX2 is **already proven correct** — it has run thousands of PS2 titles for years. The challenge in this port is not emulator design or JIT theory; it is **mechanical translation** of a large, well-understood x86-64 assembly codebase into equivalent ARM64 assembly (via VIXL) while preserving the exact same register-allocation contracts, block lifecycle, and recompiler semantics.

Large language models (LLMs) were used as an **accelerant for this translation work** — pattern-matching x86 JIT boilerplate to ARM64 equivalents, scaffolding emit routines, and keeping the porting velocity high. The JIT *logic* (block compiler, dispatcher, analysis passes, flag pipelines, clamping rules, Tri-Ace hacks, etc.) is taken directly from the upstream x86 implementation and validated against it. **Nothing was hallucinated from scratch.**

In other words: the hard engineering was done by the ARMSX2 team over two decades. The hard *typing* — translating ~50k lines of x86 emitter code into ARM64 — is what AI helped compress.

## 🗂 Repository Layout

Everything is built from **one shared PCSX2 core + ARM64 JIT** at the repository
root. Platform frontends are thin subfolders that consume that single core — no
more per-platform forks of the emulator.

```
/                     Shared PCSX2 core + ARM64 JIT (pcsx2/, common/, 3rdparty/, cmake/)
├── pcsx2-qt/         Desktop Qt GUI  →  ARM64 PC builds for macOS · Windows · Linux
├── platforms/
│   ├── android/      Android app (Gradle + JNI); thin CMake sources the root core
│   └── ios/          iOS / iPadOS app (native UIKit + bridge); CMake sources the root core
└── .github/workflows/build-all.yml   Builds every target on every push
```

Platform-specific code that lives in the core is guarded (`if(ANDROID)`,
`ARMSX2_IOS`, `APPLE`, `WIN32`, …); the mobile apps add only their UI, input,
and OS glue on top.

| Target | Where it builds from | Output |
|---|---|---|
| macOS (Apple Silicon) | root + `pcsx2-qt/` | `ARMSX2.app` |
| Windows on ARM64 | root + `pcsx2-qt/` | *(workflow WIP — see status)* |
| Linux on ARM64 | root + `pcsx2-qt/` | AppImage |
| Android | `platforms/android/` | `.apk` |
| iOS / iPadOS | `platforms/ios/` | `.app` |

## 🚧 Monorepo Refactor — In Progress

The single-core restructure is happening on branch **`refactor/monorepo`** (cut
from `macOS`). This section is the short version for collaborators; the full
detail, decisions, and command references are in
[`REFACTOR_STATUS.md`](REFACTOR_STATUS.md).

**Why:** the Android (`refresh-experimental`) and iOS (`iOS-refresh`) branches
each carried their *own* diverged copy of the PCSX2 core (iOS vendored ~12.8k
core files — 93% of the branch). That triple-fork is being collapsed into the
single root core so every platform tracks the same emulator and JIT.

**Done**

- ✅ Android frontend snapshotted to `platforms/android/`; its vendored core
  deleted; the Android-only core additions (Oboe audio, EGL-Android, NEON SPU2,
  Android stubs, etc.) relocated into the root core behind `if(ANDROID)`.
- ✅ iOS frontend snapshotted to `platforms/ios/`; its vendored core **and**
  vendored `3rdparty` deleted; iOS-only additions relocated behind `ARMSX2_IOS`;
  CMake rewired to the root core (all iOS SDK / bundle / JIT-entitlement config
  preserved).
- ✅ Both mobile builds rewired to source the single root `{pcsx2, common, 3rdparty}`.
- ✅ Unified CI (`build-all.yml`) — one push builds PC macOS-arm64, PC Linux-arm64,
  Android APK, and iOS `.app`, each uploading its own artifact.

**Remaining** (help welcome)

1. **Get each platform compiling green.** The collapsed-core builds have not been
   compiled yet — the Android/iOS CI jobs are `continue-on-error` on purpose and
   are the mechanism to surface fixes. Flip them to blocking once green.
2. **Shared-file reconciliation (~250 files).** Only net-new mobile files were
   relocated; Android's *edits to shared files* (FullscreenUI, Achievements, GS
   device backends, VMManager, MemoryCardFile) still need folding in behind
   platform guards. Deltas: `git diff macOS refresh-experimental -- pcsx2 common`.
3. **Port 3 ARM64 JIT fixes** from Android (`45b4b68d10`, `75351e8545`, `ec06302ccf`).
4. **De-duplicate 3rdparty** (`platforms/android/.../cpp/3rdparty`) against root once
   the NDK build is green — keep only Android-only deps (adrenotools, oboe).
5. **Add the Windows-on-ARM64 PC workflow** and a `pc-windows-arm64` job.

**Branch plan** — non-destructive:

- `macOS` / `refactor/monorepo` → default **`main`**.
- `master` (22.6k commits behind, old upstream) → **`master-archive`**.
- `refresh-experimental` and `iOS-refresh` kept as archives (full mobile history
  lives there; the monorepo takes a snapshot, not the history).

## Project Details

PCSX2 has been in development for more than 20 years. Past versions could only run a few public domain game demos, but newer versions can run most games at full speed, including popular titles such as Final Fantasy X and Devil May Cry 3. Visit the [PCSX2 compatibility list](https://pcsx2.net/compat/) to check the latest compatibility status of games (with more than 2500 titles tested).

Installers and binaries for both stable and nightly builds are available from [our website](https://pcsx2.net/downloads/).

## System Requirements

ARMSX2 targets ARM64 across desktop (macOS, Windows, Linux) and mobile (Android, iOS/iPadOS), all from the single shared core. Our [setup documentation page](https://pcsx2.net/docs/setup/requirements) contains additional details on software and hardware requirements.

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

### Mobile builds

The mobile apps build against the same root core:

- **Android** — `platforms/android/` (Gradle): `./gradlew :app:assembleRelease`
  (requires JDK 17 + Android SDK/NDK). Its native `CMakeLists.txt` sources the
  repo-root core.
- **iOS / iPadOS** — `platforms/ios/` (Xcode/CMake): configure
  `platforms/ios/app/src/main/cpp` with `-G Xcode -DCMAKE_SYSTEM_NAME=iOS`.

Both are wired but not yet verified green — see the
[refactor status](#-monorepo-refactor--in-progress). The canonical reference for
each build is the `build-all.yml` workflow.
