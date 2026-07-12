# iOS bring-up — pre-design brief

*Compiled 2026-07-10 by deep exploration of the monorepo + iOS source-of-truth.
Read this before starting. It supersedes the `ios-handoff-for-jeen.md` claims
that ground-truth contradicts (flagged inline).*

## TL;DR — the work is smaller and different than the handoff implied

The handoff doc (`ios-handoff-for-jeen.md`) describes a near-blank-slate iOS
bring-up. **Ground truth says otherwise.** `platforms/ios/` already exists,
the frontend is ~79% snapshotted, the vendored core is already deleted, and the
CMake already points at the root core. The *real* gap is three things:

1. **The shared core's iOS JIT path is missing** — not just unguarded, *absent*.
   The Android graft (`ecfebfd`) stripped the iOS-specific code from the
   canonical arm64 files. This is the load-bearing job.
2. **CMake has 6 concrete configure-time blockers** that will FATAL before a
   single source file compiles. Mechanical, all identified.
3. **The frontend snapshot is stale** (~20 drifted Swift files + 11 missing)
   and dragging Android/React-Native cruft. Cleanup + refresh, not rebuild.

Everything else (entitlements, GameDB, obj-c bridge, Metal renderer, audio) is
**already fine or nearly fine**.

---

## What's already done (don't redo)

- `platforms/ios/app/src/main/cpp/` — vendored core **deleted**; only the
  thin iOS layer remains (17 files: bridge, `ios_main.mm`, `IOS/` subdir,
  CMakeLists, entitlements, Info.plist.in).
- CMake rewired to root core via `ARMSX2_ROOT` (5 `..` hops, line 81).
  `add_subdirectory(${ARMSX2_ROOT}/common)`, `.../pcsx2` already present.
- `pcsx2/CMakeLists.txt` has iOS scaffolding: `ARMSX2_IOS` sentinel (line 1103),
  shared NEON SPU2 gate `if(ANDROID OR ARMSX2_IOS)` (1108), iOS-only sources
  `if(ARMSX2_IOS)` -> MacOSStubs/QAProbe/TestHarness/SifRingBuffer (1152).
- Obj-C bridge (`ARMSX2Bridge.h/.mm`, bridging header, `main.cpp`, entire `IOS/`
  subdir) — **byte-identical** to source-of-truth.
- `GameIndex.yaml` + all of `assets/resources/` — **identical**.
- `Entitlements.plist` + `Info.plist.in` — present, same location both repos.
- Metal renderer, cubeb audio — present in shared core, no iOS gate needed
  (iOS-specific audio is frontend-layer, per source-of-truth design).

---

## Job 1 — CMake blockers (mechanical, do first; ~half day)

All six will FATAL or mis-compile at configure time. Each is a one-to-few-line
fix mirroring the Android pattern.

| # | File:line | Problem | Fix (mirror Android) |
|---|---|---|---|
| 1 | `cmake/Pcsx2Utils.cmake:12-22` (root) | `detect_operating_system()` has no `IOS` branch -> FATAL | Add `elseif(APPLE AND IOS)` branch. Android vendored a local copy; iOS points at root, so **extend root** (or vendor iOS-local like Android did — decision point). |
| 2 | `cmake/SearchForStuff.cmake:39` (root) | `find_package(PCAP REQUIRED)` in bare non-Windows `else()` -> hard-fail (no libpcap on iOS) | Add `if(ANDROID OR IOS)` skip, or vendor iOS-local SearchForStuff like Android. |
| 3 | `pcsx2/CMakeLists.txt:326` + `:1300` | PCAP guards are `if(NOT ANDROID)` / `elseif(NOT ANDROID)` — iOS is not-Android, so iOS compiles+links PCAP it doesn't have | Widen to `if(NOT ANDROID AND NOT IOS)` / share an `if(ANDROID OR IOS)` exclusion. Note: `PCSX2_NO_PCAP=1` (defined at iOS CMakeLists:340) is a **dead define** — referenced by zero source files. |
| 4 | `common/CMakeLists.txt:152` | Chain is `WIN32/APPLE/ANDROID/else()`. iOS sets `APPLE=true`, so iOS takes the macOS branch (Cocoa/Darwin/IOKit) — wrong | Add `elseif(IOS)`/`elseif(ARMSX2_IOS)` branch selecting iOS-appropriate sources. |
| 5 | `platforms/ios/.../CMakeLists.txt:124` | `add_subdirectory(${ARMSX2_ROOT}/3rdparty ...)` — root `3rdparty/` has **no aggregate CMakeLists.txt** -> FATAL | Android adds each lib individually from its local SearchForStuff. Either vendor iOS-local 3rdparty + SearchForStuff (Android's approach), or add individual `add_subdirectory(${ARMSX2_ROOT}/3rdparty/<lib>)` calls. **This is the big architectural decision** — see Decision Points. |
| 6 | `common/CMakeLists.txt` | `PNGStub.cpp` (REFACTOR_STATUS item #5) relocated but not wired; gated `#if !TARGET_OS_IPHONE` so it's iOS-excluded at source but the iOS build may need the positive counterpart | Add under `if(ARMSX2_IOS)` guard if iOS references it. |

**Latent:** root `cmake/` (13 modules) is missing 4 modules that the source-of-truth's local `cpp/cmake/` has: `FindEGL.cmake`, `ECMFindModuleHelpers.cmake`, `ECMFindModuleHelpersStub.cmake`, `TargetArch.cmake`. If any included module pulls these, configure fails. Carry over if needed.

**The oracle:** build the CMake core for iOS target locally on the Mac and iterate (`ninja -k 0` / Xcode). Don't wait on CI. Android went FATAL -> green in ~4 local rounds.

---

## Job 2 — JIT / W^X port (the real engineering; ~2-4 days)

**This is the load-bearing work.** The shared core has the **macOS baseline
only**. The entire iOS JIT strategy is absent. Source-of-truth: `J1coding/ARMSX2`
(`app/src/main/cpp/`).

### What's missing (port from source-of-truth, gate `#if TARGET_OS_IPHONE`)

**`common/Darwin/DarwinMisc.h` + `.cpp`** — the bulk of the iOS JIT runtime:
- `JitMode` enum: `Simulator` / `Legacy` / `LuckTXM` / `LuckNoTXM`
- `IsJITAvailable()` — `csops(getpid(), CS_OPS_STATUS, ...)` probe for `CS_DEBUGGED` (0x10000000); if not debugger-attached, JIT denied -> interpreter fallback
- `DetectJitMode()` / `GetJitMode()` — reads `kern.osproductversion`; >=26 -> TXM, else Legacy; env override `ARMSX2_FORCE_DUAL_MAP`
- `HasTXM()` — globs `Ap,TrustedExecutionMonitor.img4` (iOS device only)
- `JIT26PrepareRegion()` / `JIT26Detach()` — `brk #0xf00d` inline-asm wrappers (iOS 26 TXM protocol)
- `MmapCodeDualMap(size)` — the central allocator. Non-iOS: plain `mmap(MAP_JIT)`, offset=0. iOS device branches: Legacy (mprotect toggle), LuckTXM (`brk #0x69`/`JIT26PrepareRegion` + `vm_remap` RW alias), LuckNoTXM (pure `vm_remap`). Sets `g_code_rw_offset = rw_ptr - rx_ptr`.
- `MunmapCodeDualMap()` — `vm_deallocate` the RW alias
- `LegacyProtectCodeRange()` — page-aligned mprotect for Legacy mode
- `HostSys::BeginCodeWrite/EndCodeWrite/BeginCodeWriteRange/EndCodeWriteRange` — iOS branches: Legacy mprotect toggle, Simulator dlsym(`pthread_jit_write_protect_np`), dual-map no-op
- `LegacyEnsureExecutable()` — lazy RW->RX flip before JIT dispatch
- `g_code_rw_offset` / `g_code_rw_base` / `g_code_rw_size` globals
- The `iPSX2_*` diagnostics/bisect flag externs + W^X trace rings (optional but port for parity)

**`pcsx2/arm64/AsmHelpers.cpp` + `.h`:**
- `armGetWritableCodePtr(rx_ptr)` — **the iOS dual-map bridge**. `#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR` returns `rx_ptr + g_code_rw_offset`; else `rx_ptr`. **Without this, every block-emit writes a read-only page and SIGBUSes on device.**
- `armStartBlock` / `armEndBlock` already call `BeginCodeWriteRange`/`EndCodeWriteRange`/`FlushInstructionCache` — verify the iOS branches inside those (in DarwinMisc) are present after the port.
- `armEmitJmpPtr` late-patch path — needs dual-map-aware writable pointer translation.

**`pcsx2/Memory.cpp`** (SysMemory impl):
- iOS `MmapCodeDualMap` allocation branch (~line 159 in source) + `SetJitRange` + `@@P43_OFFSET@@` log, gated `#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR`.

**`common/Linux/LnxHostSys.cpp`** (Apple branches live here despite the name):
- `FlushInstructionCache` (`sys_icache_invalidate`) — already present (mac baseline)
- `CreateIOSFileBackedSharedMemory` — iOS-only shm_open fallback (temp-file-backed in `$TMPDIR`)
- `SharedMemoryMappingArea::Create` — iOS `PROT_READ|WRITE|EXEC` MAP_JIT birth-prot branch (iOS rejects `PROT_NONE MAP_JIT`)

### What the Android graft CLOBBERED (re-apply from source-of-truth)

The `ecfebfd` commit grafted the mac-port EE+VU backend into canonical files
**unguarded**, and in doing so **stripped** iOS-specific code that exists in
the source-of-truth:

| File | Source-of-truth has | Monorepo has | Action |
|---|---|---|---|
| `pcsx2/arm64/aR5900.cpp` | 14 `ARMSX2_IOS_EE_PERF_PROBE` blocks; `TARGET_OS_IPHONE` gates (5 sites); `DarwinMisc::LegacyEnsureExecutable()` in `recExecute` (src:5303); `g_code_rw_offset` write path (src:211,349,5328); `__APPLE__` hotpath diagnostics | **0 iOS markers** | Re-apply the iOS blocks from source-of-truth, gated. The `recExecute` -> `LegacyEnsureExecutable()` call is correctness-critical. |
| `pcsx2/arm64/aR5900FPU.cpp` | `TARGET_OS_IPHONE` gates | **0** | Re-apply. |
| `pcsx2/arm64/Vif_Dynarec.cpp` | `TARGET_OS_IPHONE` gates | **0** | Re-apply. |
| `pcsx2/arm64/aVU.h/.cpp` | 0 iOS markers (baseline) | 0 | **Not clobbered** — VU graft is redundant per REFACTOR_STATUS; the `__builtin_memcmp` workaround in `compareState` is shared-Apple, not iOS-specific. No action for iOS. |

### Entitlements (verify, small)
- `platforms/ios/.../Entitlements.plist` has `get-task-allow` + `allow-jit` + `allow-unsigned-executable-memory`. Source-of-truth identical. **Missing `disable-library-validation`** that the macOS `pcsx2/Resources/ARMSX2.entitlements` has — verify whether iOS needs it (likely yes for JIT loading). The extension mismatch (`.plist` vs `.entitlements`) is cosmetic — Xcode accepts both via `CODE_SIGN_ENTITLEMENTS`.

### JIT-unavailable fallback (port the full chain)
`IsJITAvailable()` false -> `iPSX2_FORCE_EE_INTERP` set by `ios_main.mm` -> consumed in `pcsx2/Vif_Unpack.cpp:321,367,500`, `pcsx2/ImGui/ImGuiOverlays.cpp:462`, `ARMSX2Bridge.mm:2249`, `IOS/SceneDelegate.mm:604,825`. **This entire chain is iOS-only and must be present gated** — macOS/Linux never need it.

---

## Job 3 — Core reconciliation (the rest; ~1 day)

The remaining iOS-specific deltas beyond JIT. Source-of-truth has **174
`TARGET_OS_IPHONE` touchpoints across 43 files**; the shared core has **6**.
Hotspots to port (gated `#if TARGET_OS_IPHONE`):

- `common/Darwin/DarwinMisc.cpp` (21) — covered by Job 2
- `pcsx2/VMManager.cpp` (16) — filesystem/sandbox, app-container paths, security-scoped bookmarks
- `pcsx2/GS/Renderers/HW/GSHwHack.cpp` (12) — GS hwfixes iOS variants
- `pcsx2/GameDatabase.cpp` (7), `pcsx2/GS/Renderers/Metal/GSDeviceMTL.mm` (7) — Metal surface hookup from UIKit `CAMetalLayer`
- `pcsx2/DEV9/AdapterUtils.cpp` (7), `pcsx2/Memory.h/.cpp` (10) — covered by Job 2 (Memory) + network
- `pcsx2/SaveState.cpp` (5), `pcsx2/CDVD/Darwin/DriveUtility.cpp` (4) — filesystem
- `common/Image.cpp` (6)

**Fence-correctness pass:** the only real risk is `pcsx2/GS/Renderers/OpenGL/GSDeviceOGL.cpp:1464,1497,1509,1520` — 4 `#if !defined(__ANDROID__)` sites around GL pipeline-statistics queries that would activate on iOS (iOS is not `__ANDROID__`). iOS builds Metal, but if OGL is in the iOS source set, harden to `!defined(__ANDROID__) && !defined(TARGET_OS_IPHONE)`. All other `__ANDROID__` fences are safe given current CMake wiring.

**Correction to handoff:** audio is **cubeb** (`pcsx2/Host/CubebAudioStream.cpp`), not AVAudioEngine. No iOS audio gate needed in core — iOS audio is frontend-layer.

---

## Job 4 — Frontend refresh + validate (~half day)

### Stale snapshot (refresh from `J1coding/ARMSX2`)
- **20 drifted Swift files** — notable: `SettingsStore.swift` (-187 L), `PerGameSettingsPanel.swift` (-150 L), `PatchStore.swift` (-56 L), `TransientBannerController.swift` (half size), `PerGame/{PadTab,CPUTab}.swift`. The monorepo snapshot is an older commit.
- **11 missing Swift files** — 3 localization extensions (`AppLanguage+{Common,Main,UISupplement}Translations.swift`), 7 background-feature files (`Background{Asset,Storage,Validation}.swift`, `Views/Background/{BackgroundContainerView,VideoBackgroundView}.swift`, `Views/Settings/{BackgroundAssetRow,BackgroundSourcePicker}.swift`), 1 `Views/Settings/PerGame/RetroAchievementsTab.swift`. Create `Views/Background/` dir.
- **`AppLanguage.swift` split** — monorepo has the 2297-line monolith; source has the 79-line base + 3 extensions. Replace after copying extensions (else duplicate definitions).
- **`ios_main.mm`** — monorepo uses obsolete per-flag OSD model; source uses OSD-preset model (`OsdPreset` 0-3, `ARMSX2WriteIOSOsdFlagsToSettings()`). Refresh from source or the Swift OSD UI won't function.

### Cruft to delete (no functional impact)
The iOS snapshot dragged in a full Android + React-Native project:
- Root: `App.js`, `index.js`, `babel.config.js`, `metro.config.js`, `react-native.config.js`, `package.json`, `build.gradle`, `settings.gradle`, `gradle.properties`, `gradlew`, `gradlew.bat`, `gradle/`, `check_elf_alignment.sh`, `crowdin.yml` (dup of root)
- `.github/` entire dir (Android workflow + issue templates — real CI is `${root}/.github/workflows/build-all.yml`)
- `app/src/debug/`, `app/src/googlePlay/`, `app/src/reactnative/`
- `app/src/main/AndroidManifest.xml`, `app/src/main/java/` (~33 Java files), `app/src/main/res/` (137 files)

### Validate (on-device only)
Two things only confirmable on a real device (or AltStore/dev-signed path):
1. JIT-actually-grants-RWX under each of the 4 JitModes (especially LuckTXM `brk #0x69` on iOS 26 A15+).
2. Metal surface hookup (`CAMetalLayer` -> `GSDeviceMTL`).

---

## Decision points (need your call before/during work)

1. **3rdparty strategy for iOS** — mirror Android (vendor iOS-local `3rdparty/` + iOS-local `SearchForStuff.cmake`), or extend root modules to handle iOS? Android chose vendor. Recommend mirroring Android for isolation, but it's more disk. **This is the biggest fork in the road.**
2. **cmake modules strategy** — same shape: vendor iOS-local `cmake/` (Android's choice) or extend root `cmake/Pcsx2Utils.cmake` + `SearchForStuff.cmake` to accept IOS? Handoff says "don't edit root desktop modules" -> implies vendor. But the iOS CMakeLists *already* points `CMAKE_MODULE_PATH` at root, so someone assumed extend. Reconcile.
3. **EE graft disposition** — REFACTOR_STATUS #3 leaves open: guard the mac-port EE Android-only, fold into canonical for all arm64, or bench canonical+PGO first? **Not your call alone** — flag to trak/jpolo. But your iOS `recExecute` -> `LegacyEnsureExecutable()` re-application is independent of this decision.
4. **Commit strategy** — handoff says "commit straight to master, don't branch." You prefer PRs. Confirm with jpolo/trak which they want.
5. **JIT diagnostics/trace** — port the full `iPSX2_*` bisect/trace harness (useful for your own debugging) or just the runtime-critical pieces (IsJITAvailable, JitMode, dual-map)? Recommend port all — it's low-risk and you'll want it when on-device JIT misbehaves.

## Suggested sequence (when you're back)

1. **Decisions 1-2** (3rdparty + cmake strategy) — unblocks everything.
2. **Job 1** (CMake blockers) — iterate to configure-green on Mac. ~half day.
3. **Job 4 cruft deletion** — trivial, do alongside Job 1 for a clean tree.
4. **Job 2** (JIT/W^X port) — the real work. Port DarwinMisc first (it's the foundation), then AsmHelpers `armGetWritableCodePtr`, then Memory.cpp allocation, then re-apply the clobbered aR5900/aR5900FPU/Vif_Dynarec blocks.
5. **Job 3** (remaining core reconciliation) — the 174 touchpoints, file by file.
6. **Job 4 frontend refresh** — copy 11 + refresh 20 + ios_main.mm + AppLanguage split.
7. **Validate on-device** — the two things CI can't confirm.

## Sources / references
- Governing doc: `REFACTOR_STATUS.md` (repo root)
- Android worked example: commit `ecfebfd` (HEAD of master)
- iOS source-of-truth: `J1coding/ARMSX2` (your fork, branch `testbuild/ios-2.4.1`)
- Handoff doc: `ios-handoff-for-jeen.md` (note: 2 facts wrong — audio is cubeb not AVAudioEngine; touchpoints are 174 not ~147)
