# ARMSX2 monorepo refactor — status & roadmap

Goal: **one shared PCSX2 core (arm64 JIT) at the repository root**, with the
Android and iOS frontends as thin subfolders that consume it, and CI that builds
every target on every commit.

Branch: `refactor/monorepo` (off `macOS`). Nothing on the remote has been
changed yet.

## Decisions (locked)

| Topic | Decision |
|---|---|
| Canonical core | `macOS` branch (127 unique arm64-JIT commits, newest). Becomes `main`. |
| Core strategy | **Single shared core.** Mobile core forks collapsed; vendored copies deleted. |
| Mobile history | **Snapshot** (squash) into subfolders; full history stays on the archived branches. |
| Layout | `platforms/android/`, `platforms/ios/`. |
| Old branches | `macOS` → default `main`; `master` → `master-archive` (non-destructive); other branches left as-is. |
| Build wiring | Rewire mobile builds onto the root core immediately; delete vendored copies. |

## Branch reality (measured)

- `master`: old upstream PCSX2, 22,653 commits behind `macOS` → archive.
- `refresh-experimental` (Android): full core fork at root **+** a 2nd vendored
  core under `ARMSX2/app/src/main/cpp` **+** a 2nd vendored `3rdparty`.
- `iOS-refresh`: React-Native shell with a full vendored core under
  `app/src/main/cpp` (12,782 files, 93% of the branch).
- Android arm64 JIT: only 7 unique commits (3 real fixes) vs `macOS`'s 127. The
  `arm64/mac/*` IR-VU backend + split `aVU0/aDMAC/aVTLB/aR5900COP*` are a
  **superseded experiment `macOS` already consolidated past → dropped.**

## Done

- [x] `platforms/android/` — Android app snapshotted; vendored core deleted;
      24 Android-only core additions relocated into the root core behind
      `if(ANDROID)`; on-device arm64 test harnesses preserved; thin CMake sources
      the root core.  *(commit: "move Android frontend …")*
- [x] `platforms/ios/` — iOS app snapshotted; vendored core + vendored 3rdparty
      deleted; iOS-only additions relocated behind `ARMSX2_IOS`; NEON SPU2 shared
      by both arm64 mobile targets; cruft dropped; CMake rewired to root core via
      `ARMSX2_ROOT`, iOS-SDK/bundle/entitlement config preserved.
      *(commit: "move iOS frontend …")*
- [x] `.github/workflows/build-all.yml` — one push builds PC macOS-arm64,
      PC Linux-arm64, Android APK, iOS .app; each uploads its own artifact.
- [x] **Android brought compile-green AND up to refresh-experimental parity
      (2026-07-10).** GoW2 boots and runs full-speed (60fps) on RP6 (Adreno 740 /
      QCS8550 / Turnip). Landed this pass — but read the caveats in Remaining #2/#3,
      some of it touches shared code:
      Vulkan render fixes (GS device backends), Oboe audio backend, GameDB
      `armsx2_overrides.yaml` override loader, OGL/GLES fixes, EE+VU mac-port
      recompiler graft, PGO=optimize, and the **VU-slam fix**. The VU slam was
      root-caused to Android thread PINNING hard-locking the VU1 worker off the
      prime core (diagnostic showed `VU=core3` while the X3 sat idle); fixed with an
      Android-only force-float in `VMManager::SetEmuThreadAffinities` — VU 20→14ms,
      82%→100% speed. Also (all `#if __ANDROID__`-guarded / runtime-gated, mac-safe):
      "CPU: Unknown" SoC-name fallback, Mali VK attachment-feedback-loop crash gate,
      MediaTek fbfetch disable.

## Remaining (the long pole — needs real toolchains / CI iteration)

1. **Compile-green each platform.** None of the collapsed-core builds has been
   compiled yet (this was done in a shell without NDK/Xcode/mac-Qt). The
   `build-all.yml` Android + iOS jobs are `continue-on-error: true` on purpose —
   they are the mechanism to surface and fix the remaining issues. Flip to
   blocking once green.
   > ✅ 2026-07-10: **Android is compile-green + runtime-verified** (dual-core 4k/16k
   > APK, PGO, running games on device). PC-macOS-arm64 / PC-Linux-arm64 / iOS still
   > need a CI green pass — and #2/#3 below may have perturbed the shared core, so
   > re-check the mac/Linux arm64 build after this push.
2. **Shared-file reconciliation (~250 files).** The Android core fork also
   *modified* shared files (FullscreenUI, Achievements, GS device backends,
   VMManager, MemoryCardFile). Those deltas are **not** yet merged — only the
   net-new Android/iOS files were relocated. Diff each against the root core and
   fold in the genuinely mobile-specific changes behind platform guards; discard
   stale-upstream noise. Source of truth for the deltas:
   `git diff macOS refresh-experimental -- pcsx2 common`.
   > ⚠️ 2026-07-10: to get Android green fast, the GS device backends (~47 `pcsx2/GS/*`
   > files) + `VMManager`/`GameDatabase`/`AudioStream`/`Config`/`Pcsx2Config`/`MTVU`/
   > `Semaphore`/`Threading` deltas were brought over by **wholesale graft/copy from
   > refresh-experimental, mostly UNGUARDED** — i.e. they now ride into the mac/Linux
   > arm64 builds, not just Android. This is the fast-but-dirty version of this item.
   > The genuinely Android-only pieces (force-float, Oboe, SoC-name, Mali/MediaTek VK
   > gates, OSD label) ARE `#if __ANDROID__`/runtime-gated and mac-safe. The rest still
   > needs the careful per-file pass: keep mobile-specific behind guards, verify PC/mac
   > unaffected, drop stale-upstream noise.
3. **arm64 JIT fixes to port** (3 real Android commits):
   `45b4b68d10` (microVU PQ lanes), `75351e8545` (FTOI NaN / SQRT clamp),
   `ec06302ccf` (skip microVU emit on jump-cache hits).
   > ⚠️ 2026-07-10 — **HEADS-UP, this contradicts the "mac backend dropped" decision.**
   > To reach refresh-experimental EE/VU perf on Android quickly, this pass GRAFTED the
   > mac-port EE+VU backend into the shared canonical files (`pcsx2/arm64/aR5900*`,
   > `aVU*`) — i.e. it re-introduced the "superseded experiment" logic *under the
   > canonical filenames*, UNGUARDED. **This changes the mac/Linux arm64 recompiler too**,
   > so the mac build needs a re-verify after this push.
   > - The **VU graft is now REDUNDANT**: the VU slam was Android thread-pinning, not the
   >   VU codegen, and the fix (force-float, `#if __ANDROID__`) is backend-agnostic. So
   >   `aVU*` can be reverted to canonical with **zero** Android perf loss — recommended.
   > - The **EE graft** is the one genuine Android win (canonical Phase-7 EE was ~98%
   >   slammed pre-graft). Decision needed: guard it Android-only, fold the improvement
   >   into the canonical JIT for all arm64, or bench canonical-EE+PGO to see if the gap
   >   is real. Until decided, mac inherits the mac-port EE.
4. **3rdparty de-duplication.** `platforms/android/.../cpp/3rdparty` (adrenotools
   + others) is still vendored for the NDK build. Keep adrenotools/oboe
   (Android-only); evaluate sourcing the rest from root `3rdparty/` once the
   NDK build is green.
5. **`common/PNGStub.cpp`** (iOS) is relocated but not yet wired into
   `common/CMakeLists.txt` — add under an iOS guard if the iOS build references it.
6. **Windows-on-arm64** PC build: no reusable `windows_build_qt.yml` exists on
   this branch. Add one and a `pc-windows-arm64` job in `build-all.yml`.
7. **Finalize remote (destructive — confirm before pushing):**
   - Promote `macOS`/this branch to the default `main`.
   - Rename `master` → `master-archive` on the remote.
   - Leave the other stale branches untouched.
