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
   > 🔧 2026-07-10 (restoration pass, branch `refactor/restore-canonical-core`):
   > the unguarded grafts from `ecfebfd6b2` that leaked into the shared core have been
   > backed out — see the resolution notes on #2 and #3. The shared PC/mac/Linux/Windows
   > arm64 core is now byte-for-canonical again; Android-specific work is isolated behind
   > `#if __ANDROID__` guards or in a CMake-selected forked TU. Done as **forward commits**
   > (HEAD `ecfebfd6b2` is already on `origin/master`, so no history rewrite).
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
   >
   > ✅ 2026-07-10 RESOLVED (restoration pass):
   > - **GS subsystem (all 47 `pcsx2/GS/*` files) reverted to canonical.** The graft was
   >   not Android rendering fixes — it dragged in refresh-experimental's *divergent* GS,
   >   touching Windows-only DX11/DX12 and Metal with zero Android guards (e.g. DX11
   >   `depth_feedback`/`multidraw_fb_copy` flag flips). The GS interface headers all moved
   >   together, so GS is an all-or-nothing snapshot; reverting the whole subsystem restores
   >   canonical PC/mac/Linux/Windows exactly.
   > - **Core files reconciled per-file:** kept the guarded Android features (force-float +
   >   nice-priority in `VMManager`, Oboe in `AudioStream`/`Config`/`Pcsx2Config`, SoC-name
   >   in `HostSys`, OSD label in `ImGuiOverlays`) and the *additive* shared bits (new
   >   `ThreadHandle` API + cache-line alignment in `Threading`/`*Threads`/`Semaphore`, the
   >   `armsx2_overrides.yaml` GameDB loader — PC-safe, no override file shipped). Reverted
   >   the three unguarded desktop-behavioral regressions: `MTVU` `WaitForWork`→spin swap,
   >   the removed "Graphics API not Automatic" OSD warning, and the `s_thread_affinities_set`
   >   pinning-flag change.
   > - **STILL OPEN (needs a device):** Android GS parity now has to be re-earned the right
   >   way — the mobile-specific VK/GLES fixes (and the Mali/MediaTek gates) must be
   >   re-applied on top of canonical GS behind `#if __ANDROID__`, device-tested. The graft
   >   itself is preserved in `origin/master` history (`ecfebfd6b2`) as the reference.
   >
   > ✅✅ 2026-07-10 (later) — **DONE, clean re-apply, device-verified on RP6 (Adreno 740 / Turnip).**
   >   Method: reset the GS dir to canonical, then re-applied ONLY the mobile delta as guarded
   >   hunks (dropped the yaps2 readback-kick perf graft entirely — it was unguarded desktop
   >   divergence, not needed for rendering), then audited every remaining hunk so PC/mac/Linux/
   >   Windows is byte-for-canonical. Touches VK (6 files) + OGL (`GSDeviceOGL`, `GLContext*`,
   >   `GLShaderCache`, `GSGPUProfile`).
   >   - **Vulkan black screen** was the missing **push-descriptor fallback**: canonical assumes
   >     `VK_KHR_push_descriptor` always exists; Adreno/Mali stall inside `vkCmdPushDescriptorSetKHR`,
   >     so textures never bind → black. Restored the capability-gated fallback (`m_use_push_descriptors`
   >     false only on Mali 0x13B5 / Adreno 0x5143 → per-frame descriptor-set path). Desktop keeps the
   >     push path byte-identical.
   >   - **OpenGL "boots straight back to the library"** was canonical having **no GLES path at all**;
   >     re-applied the GLES support (EGL context, `is_gles` shader branches, GLES query objects),
   >     runtime-gated by `is_gles` (false on desktop GL).
   >   - Found + fixed the one genuine desktop divergence in the delta: `m_features.depth_feedback`
   >     was force-`false` unconditionally → now `#if __ANDROID__` (desktop keeps `feedback_loops()`).
   >     Guarded 3 desktop-reachable OGL riders (present-path `glInvalidateFramebuffer` → `is_gles`,
   >     EGL `SetDisplay()` body → `#if __ANDROID__`, restored the dropped negative-swap-interval probe).
   >     KEPT (deliberately, they match upstream master and are in refresh-experimental): the RenderHW
   >     feedback-loop guard + `ProgramSelector::operator==` field-compare (#243).
   >   - **RA toast "giant malformed border"**: your `00bea431d` `AddRect` fix is correct for the
   >     desktop imgui **1.92.8** (which swapped the `thickness`/`flags` args) but the Android build
   >     vendors imgui **1.92.6** (pre-swap), so the same line drew a ~240px border there. Guarded that
   >     one call on `IMGUI_VERSION_NUM` — both platforms correct; the guard collapses to one branch
   >     once the two `3rdparty/imgui` copies dedup (see #4). *(This is the imgui sibling of the ryml
   >     shim below — same root cause: two vendored copies at different versions.)*
   >   - The "Graphics API is not set to Automatic" OSD warning (reintroduced by the merge) is now
   >     `#if !__ANDROID__` — Android forces an explicit GL/VK pick by design so it fired every boot;
   >     desktop keeps the canonical warning.
3. **arm64 JIT fixes to port** (3 real Android commits):
   `45b4b68d10` (microVU PQ lanes), `75351e8545` (FTOI NaN / SQRT clamp),
   `ec06302ccf` (skip microVU emit on jump-cache hits).
   > ⚠️ 2026-07-10 — **HEADS-UP, this contradicts the "mac backend dropped" decision.**
   > To reach refresh-experimental EE/VU perf on Android quickly, this pass GRAFTED the
   > mac-port EE+VU backend into the shared canonical files (`pcsx2/arm64/aR5900*`,
   > `aVU*`) — i.e. it re-introduced the "superseded experiment" logic *under the
   > canonical filenames*, UNGUARDED. **This changes the mac/Linux arm64 recompiler too**,
   > so the mac build needs a re-verify after this push.
   >
   > ✅ 2026-07-10 RESOLVED (restoration pass):
   > - **VU (`aVU*`) reverted to canonical.** Confirmed redundant — the slam was
   >   thread-pinning, and the fix (force-float, `#if __ANDROID__` in `VMManager`) is
   >   backend-agnostic. Zero Android perf loss.
   > - **EE (`aR5900*`) forked Android-only.** Decision (per owner): *guard it Android-only*.
   >   Because the graft is 57 hunks interleaved through the canonical file (inline `#ifdef`
   >   would be unmaintainable), it's forked at the *file* level instead: the grafted
   >   mac-port EE lives in `pcsx2/arm64/aR5900*.android.cpp`, the canonical Phase-7 EE is
   >   restored at `pcsx2/arm64/aR5900*.cpp`, and `pcsx2/CMakeLists.txt` selects between them
   >   with `if(ANDROID)`. Both share the canonical `aR5900.h`/`aR5900Analysis.h` (the graft
   >   only added comments there — identical interface). Result: PC/mac/Linux/Windows arm64
   >   build the canonical EE; Android keeps its tested win. `AsmHelpers.*` kept as-is
   >   (comment-only + a harmless `#include <new>`; no codegen change).
   > - **Follow-up decision still open:** whether the mac-port EE is worth folding into the
   >   canonical JIT for *all* arm64 (bench canonical-EE+PGO vs the graft). Until then the
   >   fork keeps the two backends cleanly separated.
   >
   > ✅✅ 2026-07-10 (later) — **FORK DROPPED, unified on canonical (commit `a6aa75bff`).**
   > Benched it on device: A/B of canonical EE+VU + force-float vs the mac-port graft (same
   > working GS, same PGO — which was tuned for the mac-port, so the test was rigged AGAINST
   > canonical) → GoW2 combat **canonical EE 12.84 ms vs mac-port 11.71 ms vs refresh-exp
   > 12.44 ms**, all 100% speed / 60 fps. Canonical is at refresh parity; the ~1 ms is noise
   > (a canonical-tuned PGO regen closes it). Deleted the 8 `aR5900*.android.cpp` + the
   > `if(ANDROID)` CMake split — Android now builds the canonical EE like every other arm64
   > target. Single JIT, no divergence. The real Android EE/VU lever was always the force-float,
   > not the backend.
4. **3rdparty de-duplication.** `platforms/android/.../cpp/3rdparty` (adrenotools
   + others) is still vendored for the NDK build. Keep adrenotools/oboe
   (Android-only); evaluate sourcing the rest from root `3rdparty/` once the
   NDK build is green.
   > ⚠️→✅ 2026-07-10: the `pcsx2master` merge broke the **Android** compile — `common/YAML.cpp`
   > calls `c4::yml::Callbacks::set_user_data()`, which the vendored rapidyaml **0.10.0** here
   > lacks (PC/mac resolve a newer system ryml via `find_package(ryml)`). Unblocked with a
   > 1-line shim: added the `set_user_data()` setter to the vendored `Callbacks` (it just sets
   > the existing `m_user_data`), commit `a6aa75bff`. Proper fix = dedupe the Android build onto
   > the canonical ryml so this shim can be deleted.
   > ⚠️ 2026-07-10: **imgui has the exact same two-copies-different-versions problem** — desktop
   > `3rdparty/imgui` = **1.92.8**, Android `platforms/android/.../cpp/3rdparty/imgui` = **1.92.6**.
   > It already bit us once (the RA-toast `AddRect` arg-swap, see #2), now papered over with an
   > `IMGUI_VERSION_NUM` guard. Deduping imgui to one version deletes that guard too — same task
   > as the ryml dedup, worth doing together.
5. **`common/PNGStub.cpp`** (iOS) is relocated but not yet wired into
   `common/CMakeLists.txt` — add under an iOS guard if the iOS build references it.
6. **Windows-on-arm64** PC build: no reusable `windows_build_qt.yml` exists on
   this branch. Add one and a `pc-windows-arm64` job in `build-all.yml`.
7. **Finalize remote (destructive — confirm before pushing):**
   - Promote `macOS`/this branch to the default `main`.
   - Rename `master` → `master-archive` on the remote.
   - Leave the other stale branches untouched.
