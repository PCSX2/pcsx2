# ARMSX2 iOS — Changelog

## Version 2.4.1

A major update that brings iOS to feature parity with the desktop and Android builds, fixes critical crashes, and polishes the interface throughout.

---

### Performance & core

**ARM64 recompiler with hardware-counter timing.** The frame limiter now reads the ARM64 architectural virtual counter (`cntvct_el0`) directly instead of going through `mach_absolute_time`, reducing overhead on every timing sample. The counter frequency is read from `cntfrq_el0`.

**Fastmem with graceful low-memory fallback.** The 4 GB fastmem virtual-address reservation can fail on devices with limited address space (such as the iPhone SE 2 under LiveContainer). Previously this caused a hard crash via a destructor assertion. The app now continues without fastmem — games run slightly slower but boot successfully. A sticky flag ensures settings reloads don't accidentally re-enable fastmem against a null base.

**Host-MMU fastmem.** The ARM64 host-MMU fastmem path is enabled safely with fault thunks for EE load/store. Code patches are written through the writable mirror on W^X systems.

**GS and SPU2 improvements.** The common vertex-kick path has been inlined and supported renderers can submit work before a readback stalls the frame. SPU2 mixing uses NEON for volume, accumulation, and clamping. Apple GPUs use the non-barrier feedback path. The `texture_barrier` feature is correctly disabled on iOS (Apple GPUs synchronize within a render pass).

**Metal renderer.** The Metal surface now reuses the UIView's existing `CAMetalLayer` instead of creating an orphaned one, fixing the half-screen crop. GS wrapped memory uses `mmap` + `vm_remap` instead of `shm_open` (which is blocked by the iOS sandbox).

---

### JIT & W^X

**Four-mode JIT implementation.** The app detects and uses the best available JIT mode at runtime:

| Mode | When | How |
|---|---|---|
| **LuckTXM** | iOS 26+ with TXM (Trusted Execution Monitor) | `brk #0xf00d` protocol with `x16=1` prepare / `x16=0` detach |
| **LuckNoTXM** | JIT available without TXM | `csops` `CS_DEBUGGED` probe, `MAP_JIT` with `PROT_READ|WRITE|EXEC` |
| **Legacy** | Pre-iOS 26 without TXM | `pthread_jit_write_protect_np` toggle |
| **Simulator** | x86 simulator | No W^X restrictions |

Dual-mapping translates the RX pointer to a separate RW mirror via `vm_remap` so the recompiler can write code while the execution page stays read-only.

---

### Crash fixes

- **Fastmem crash on 4 GB devices (iPhone SE 2):** the 4 GB virtual-address reservation failed with `ENOMEM`, causing an abort. Now handled gracefully — see above.
- **Achievements crash:** `HTTPDownloader::Create` no longer asserts when CURL is unavailable. The iOS `NSURLSession`-based downloader is used instead.
- **GS memory crash:** `shm_open("/GS.mem")` is blocked by the iOS sandbox. Replaced with `mmap(MAP_ANONYMOUS)` + `vm_remap`.
- **Metal half-screen crop:** orphaned `CAMetalLayer` was never attached to the view hierarchy. Now reuses the UIView's existing layer.

---

### Interface improvements

**Library background.** The custom wallpaper fills the entire screen edge-to-edge on the Games tab. The safe-area padding no longer clips it into visible black bars on rotation. The boot intro video (`boot_intro.mp4`) is restored and bundled into the app.

**Tab bar.** The iOS 18 floating tab bar no longer collapses to a single pill when a custom background is set. On iOS 26 the new Liquid Glass tab bar is allowed to float and adapt automatically.

**Navigation bar.** GameListView uses an inline title with a transparent toolbar background when a custom wallpaper is active, so the background extends under the navigation bar.

**Game cards.**
- Card text (title, file type, size, region) is now center-aligned to match the centered cover art
- Card padding increased from 8pt to 12pt
- The favorite star has a lighter, smaller background circle
- The "BIOS Only" button is more prominent
- The list-row favorite star meets Apple's 44pt minimum tap target

**Game screen.**
- Portrait game viewport increased from 55% to 60% of screen height — less dead space
- Pause button now uses `pause.circle.fill` (clearer than the previous ellipsis)
- Menu button has better contrast over bright game content

**Pause menu.**
- Two-column layout enabled on wide iPhones in portrait (Plus / Max / Pro Max)
- Game title uses tail-truncation instead of middle-truncation for readability

**Per-game settings.**
- Panel background fixed — no longer reads as flat black in portrait over the library sheet
- Unlocalized English string fixed and wrapped in the translation system
- Warning color now uses the design system token

**RetroAchievements.**
- When a stored account has a dead session (expired token, partial save, server rejection), the UI now shows "Log In Again" instead of only "Log Out"
- Session status row shows "Not connected" so the user knows re-authentication is needed

**Speed control.** The speed panel sheet can now expand to full height (`.large` detent) instead of being stuck at half-height after rotation.

**Settings.** The 14 flat settings links are grouped into five named sections: Interface, Emulation, Input, Storage & Memory, and Features. The JIT diagnostic panel moved below the main settings so it's not the first thing users see.

**BIOS list.** Description text no longer uses monospaced font for prose. The empty state has a single clear call-to-action instead of two competing buttons.

**Overlay contrast.** The secondary text color has been lightened to meet WCAG 2.2 AA contrast requirements (4.5:1 minimum). The frost background tint has been increased for color stability over bright game content.

---

### Build & infrastructure

**Shared monorepo core.** iOS now consumes the shared PCSX2 core at the repository root instead of a vendored copy. The CMake build (`platforms/ios/app/src/main/cpp/CMakeLists.txt`) points at `${ARMSX2_ROOT}` for `common/`, `pcsx2/`, and `3rdparty/`.

**CI.** A self-contained GitHub Actions workflow builds an unsigned IPA for real devices (iphoneos SDK) on every push, named with the commit SHA.

**Compatibility.** Upstream merge compile errors (duplicate function definitions from auto-merge, imgui 1.92.8 API change) resolved. The `WarnAboutUnconfiguredController` OSD warning is properly platform-guarded — desktop users see it, mobile users (who inject pad state through the platform bridge) don't.

---

### What's not changing

- The two-column game grid, rounded cards, and cover artwork
- The custom background system (wallpapers, video backgrounds, fit modes)
- The in-game overlay design system architecture
- The virtual controller and skin system

---

Thank you to everyone who tested the 2.4.1 builds and reported bugs. The feedback on black bars, tab bar collapse, portrait panel sizing, and the fastmem crash on low-memory devices made this release significantly better.
