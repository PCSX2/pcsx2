# JIT Resilience Layer — Keepalive, Interpreter Fallback, and Boot Watchdog

**Date:** 2026-07-12
**Branch:** `ios/bring-up`
**Depends on:** existing JIT lifecycle code in `common/Darwin/DarwinMisc.cpp`, `platforms/ios/app/src/main/cpp/IOS/SceneDelegate.mm`, `pcsx2/Memory.cpp`

---

## Problem Statement

iOS revokes the `CS_DEBUGGED` JIT grant approximately 30–60 seconds after the app becomes inactive. When the user then attempts to launch a game, one of two things happens:

1. **Hard block** — `checkJITAndStartVM` detects JIT is unavailable and shows "JIT Unavailable". User must force-quit and relaunch. Confusing.
2. **Silent black screen** (worse) — `CS_DEBUGGED` lingers in the `csops()` check but the underlying TXM/dual-map grant is already dead. The persistent VM thread is signaled (bypassing `CPUThreadInitialize`), the recompiler writes into a dead RW alias, and the game never executes → permanent black screen.

Additionally, on first boot the Universal TXM protocol (`brk #0xf00d`) can hang on large (~161 MB) code regions with no timeout, producing another permanent black screen (confirmed in ARMSX2 PR #202 and issue #200).

There is currently **no keepalive mechanism, no boot-time JIT revalidation, no interpreter fallback, and no init watchdog**.

---

## Design Decisions (from brainstorming + research)

1. **Periodic canary write + csops check (user choice).** A 12-second dispatch timer writes a canary byte to the JIT RW alias and re-checks `CS_DEBUGGED`. Runs only when the VM is idle (not during gameplay — JIT is in constant use then).
2. **Interpreter fallback when JIT dies.** Instead of blocking boot, the app falls back to the pure EE/IOP/VU interpreter so users can at least test games. The existing `iPSX2_FORCE_EE_INTERP` flag is wired up (it was declared but never set). Performance is single-digit FPS — usable for testing, not for playing.
3. **VM init watchdog + TXM failsafe (from PR #202 audit).** A 15-second watchdog catches TXM-prepare hangs. The Universal TXM prepare runs on a worker thread with an 8-second timeout; if it hangs, falls back to Legacy (`brk #0x69`).

---

## Component 1: JIT Validation Helper

**File:** `common/Darwin/DarwinMisc.h` + `common/Darwin/DarwinMisc.cpp`

### New function: `ValidateJITAlive()`

```cpp
// DarwinMisc.h (add after IsJITAvailable, around line 108)
/// Re-checks whether JIT is still usable after initial acquisition.
/// Combines a CS_DEBUGGED re-probe with a canary write/read to the RW alias.
/// Returns false if iOS has revoked the JIT grant since boot.
bool ValidateJITAlive();
```

```cpp
// DarwinMisc.cpp (add after IsJITAvailable, around line 740)
bool DarwinMisc::ValidateJITAlive()
{
#if TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    // Check 1: CS_DEBUGGED still set?
    u32 cs_flags = 0;
    const int rv = csops(getpid(), 0, &cs_flags, sizeof(cs_flags));
    const bool cs_debugged = (rv == 0) && ((cs_flags & 0x10000000u) != 0);
    if (!cs_debugged)
    {
        std::fprintf(stderr, "@@JIT_KEEPALIVE@@ alive=0 reason=cs_debugged_revoked\n");
        std::fflush(stderr);
        return false;
    }

    // Check 2: RW alias still writable? Write a canary, read it back.
    if (g_code_rw_base != 0 && g_code_rw_size > 0)
    {
        volatile u8* canary = reinterpret_cast<volatile u8*>(g_code_rw_base);
        const u8 saved = *canary;
        *canary = 0x42;
        const u8 readback = *canary;
        *canary = saved; // restore so we don't corrupt the first code byte
        if (readback != 0x42)
        {
            std::fprintf(stderr, "@@JIT_KEEPALIVE@@ alive=0 reason=rw_alias_dead readback=0x%02x\n", readback);
            std::fflush(stderr);
            return false;
        }
    }

    std::fprintf(stderr, "@@JIT_KEEPALIVE@@ alive=1 cs_debugged=1 canary=ok\n");
    std::fflush(stderr);
    return true;
#else
    return true; // macOS and Simulator always have JIT
#endif
}
```

**Why two checks:** `CS_DEBUGGED` can lag — the flag may still be set while the underlying TXM grant is already dead. The canary write catches this case. `CS_DEBUGGED` catches the common case where the debugger enabler has fully detached.

**Why `volatile`:** prevents the compiler from optimizing away the write/read cycle.

**Why restore the original byte:** `g_code_rw_base` points at the start of the code region, which may contain already-compiled dispatcher code. We save and restore the first byte so we don't corrupt it.

---

## Component 2: Keepalive Timer

**File:** `platforms/ios/app/src/main/cpp/IOS/SceneDelegate.mm`

### New static state

```objc
// Near the existing s_vmThread* statics (around line 630)
static dispatch_source_t s_jitKeepaliveTimer = nil;
static std::atomic<bool> s_jitExpired{false};
```

### Timer start/stop functions

```objc
// Start the keepalive timer. Called after CPUThreadInitialize succeeds (JIT acquired)
// and whenever the VM shuts down (user returns to menu — JIT idle, vulnerable).
static void ARMSX2StartJITKeepalive()
{
    if (s_jitKeepaliveTimer) return;
    dispatch_queue_t queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0);
    s_jitKeepaliveTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue);
    // 12-second interval, 0 leeway (fire promptly — this is safety-critical)
    dispatch_source_set_timer(s_jitKeepaliveTimer,
                              dispatch_time(DISPATCH_TIME_NOW, 12 * NSEC_PER_SEC),
                              12 * NSEC_PER_SEC,
                              0);
    dispatch_source_set_event_handler(s_jitKeepaliveTimer, ^{
        // Skip while VM is running — the recompiler constantly writes code,
        // so JIT cannot expire during active gameplay.
        if (s_vmThreadActive.load(std::memory_order_relaxed))
            return;

        if (!DarwinMisc::ValidateJITAlive())
        {
            s_jitExpired.store(true);
            // Post notification so the Swift UI can show a toast/alert
            dispatch_async(dispatch_get_main_queue(), ^{
                [[NSNotificationCenter defaultCenter]
                    postNotificationName:@"ARMSX2iOSJITExpired" object:nil];
            });
            ARMSX2StopJITKeepalive();
        }
    });
    dispatch_resume(s_jitKeepaliveTimer);
    NSLog(@"@@JIT_KEEPALIVE@@ timer_started interval=12s");
}

static void ARMSX2StopJITKeepalive()
{
    if (s_jitKeepaliveTimer)
    {
        dispatch_source_cancel(s_jitKeepaliveTimer);
        s_jitKeepaliveTimer = nil;
        NSLog(@"@@JIT_KEEPALIVE@@ timer_stopped");
    }
}
```

### Integration points in the existing boot/shutdown loop

**After `CPUThreadInitialize` succeeds** (SceneDelegate.mm, after line ~675 "ok=1"):
```objc
ARMSX2StartJITKeepalive(); // JIT acquired — start monitoring
```

**When VM shuts down** (inside the boot loop, after `VMManager::Shutdown(false)`):
```objc
// VM stopped — JIT is idle and vulnerable to revocation. Restart keepalive.
s_vmThreadActive.store(false);
ARMSX2StartJITKeepalive();
```

**When VM boots** (before `VMManager::Initialize`):
```objc
// VM about to run — JIT is in active use, keepalive not needed.
ARMSX2StopJITKeepalive();
```

---

## Component 3: Interpreter Fallback

This is the most complex component. It activates when JIT is dead and cannot be recovered.

### 3a: Wire up `iPSX2_FORCE_EE_INTERP` in the boot gate

**File:** `platforms/ios/app/src/main/cpp/IOS/SceneDelegate.mm` — `checkJITAndStartVM`

Current code (line 602) blocks boot when JIT is unavailable. New behavior: fall back to interpreter.

```objc
- (void)checkJITAndStartVM {
#if !TARGET_OS_SIMULATOR
    ARMSX2ApplyJITScriptProtocol("jit-gate");

    // Re-validate JIT even if it was available at launch. iOS can revoke
    // CS_DEBUGGED after ~30-60s of inactivity.
    const bool jitAlive = DarwinMisc::IsJITAvailable() && DarwinMisc::ValidateJITAlive();

    if (jitAlive) {
        std::fprintf(stderr, "@@BOOT_JIT_GATE@@ available=1 mode=jit_alloc\n");
        std::fflush(stderr);
        DarwinMisc::iPSX2_FORCE_EE_INTERP = 0;
        [self startVMThread];
        return;
    }

    // JIT is dead. Offer interpreter fallback instead of blocking.
    std::fprintf(stderr, "@@BOOT_JIT_GATE@@ available=0 fallback=interpreter\n");
    std::fflush(stderr);

    DarwinMisc::iPSX2_FORCE_EE_INTERP = 1;
    // Force all CPU providers to interpreter mode.
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", false);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", false);
    s_settings_interface->Save();

    // OSD notification so the user understands what's happening.
    Host::AddIconOSDMessage("JITExpired", ICON_FA_TRIANGLE_EXCLAMATION,
        "JIT session expired — booting in interpreter mode (much slower). "
        "Relaunch the app to re-enable JIT.",
        15.0f);

    [self startVMThread];
#else
    [self startVMThread];
#endif
}
```

### 3b: Skip code-memory allocation in interpreter mode

**File:** `pcsx2/Memory.cpp` — `AllocateMemoryMap()` (around line 120)

The interpreter does not generate native code. When `iPSX2_FORCE_EE_INTERP` is set, skip `MmapCodeDualMap` entirely — the interpreter's `intCpu::Reserve()` does not call `recReserve()` or touch `s_code_memory`.

```cpp
#if defined(__APPLE__) && TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR
    // In forced-interpreter mode (JIT unavailable/expired), skip executable
    // code allocation entirely. The interpreter does not generate native code.
    if (!DarwinMisc::iPSX2_FORCE_EE_INTERP)
    {
        if ((s_code_memory = static_cast<u8*>(DarwinMisc::MmapCodeDualMap(HostMemoryMap::CodeSize))) == nullptr)
        {
            Host::ReportErrorAsync("Error", "Failed to allocate iOS executable code memory.");
            ReleaseMemoryMap();
            return false;
        }
        Console.WriteLn("@@P43_OFFSET@@ g_code_rw_offset=%ld rw_base=%p size=%zu",
            (long)DarwinMisc::g_code_rw_offset, reinterpret_cast<void*>(DarwinMisc::g_code_rw_base),
            static_cast<size_t>(DarwinMisc::g_code_rw_size));
    }
    else
    {
        Console.WriteLn("[iOS] Skipping code-memory allocation — interpreter-only mode (iPSX2_FORCE_EE_INTERP=1)");
        s_code_memory = nullptr;
    }
```

**What about `SetJitRange`?** The `SetJitRange` call at line 156 receives `s_code_memory` and `CodeSize`. When `s_code_memory` is null, `SetJitRange` must be a no-op. Verify: `SetJitRange` (DarwinMisc.cpp) should be guarded to check for null:

```cpp
void DarwinMisc::SetJitRange(void* ptr, size_t size)
{
    if (!ptr || size == 0) return; // interpreter-only mode: no code region
    // ... existing code
}
```

### 3c: Fix `applyFullInterpreterPreset()` in Swift

**File:** `platforms/ios/app/src/main/swift/Models/SettingsStore.swift` (line 2228)

The current implementation sets `eeCoreType = 1` (Interpreter) but this is an iOS-UI-only concept that the C++ core ignores. The core uses `EnableEE`, which is never set to false. Fix:

```swift
func applyFullInterpreterPreset() {
    eeCoreType = 1
    // Core uses EnableEE (not CoreType) to select interpreter vs recompiler.
    // Must write EnableEE=false to actually force the EE interpreter.
    ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "EnableEE", value: false)
    iopRecompiler = false
    vu0Recompiler = false
    vu1Recompiler = false
    vu1Instant = false
    mtvu = false
    fastmem = false
}
```

Also add the inverse to `applyARM64JITPreset()` (or wherever the JIT preset restores):
```swift
ARMSX2Bridge.setINIBool("EmuCore/CPU/Recompiler", key: "EnableEE", value: true)
```

### 3d: Restore JIT on next app launch

When the app is force-quit and relaunched, `IsJITAvailable()` runs fresh (the debugger enabler re-attaches). If JIT is available again, the boot gate clears `iPSX2_FORCE_EE_INTERP` and restores the recompiler settings:

In `checkJITAndStartVM`, the `jitAlive` path already sets `iPSX2_FORCE_EE_INTERP = 0`. We also need to restore the recompiler flags:

```objc
if (jitAlive) {
    // ... existing code ...
    DarwinMisc::iPSX2_FORCE_EE_INTERP = 0;
    // Restore recompiler settings if we previously forced interpreter.
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", true);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", true);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", true);
    s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", true);
    s_settings_interface->Save();
    [self startVMThread];
    return;
}
```

---

## Component 4: VM Init Watchdog + TXM Failsafe

Adapted from PR #202 (felipebasurto). These prevent permanent black screens from TXM prepare hangs.

### 4a: VM init watchdog (15 seconds)

**File:** `platforms/ios/app/src/main/cpp/IOS/SceneDelegate.mm`

If `CPUThreadInitialize()` does not complete within 15 seconds (TXM hang), the app shows an error and returns to menu instead of hanging forever.

Wrap the `CPUThreadInitialize` call (around line 663) in a watchdog:

```objc
// Before the thread creation, set up a watchdog
static std::atomic<bool> s_vmInitComplete{false};

// Inside the VM thread, before CPUThreadInitialize:
s_vmInitComplete.store(false);
std::thread watchdog([]() {
    for (int i = 0; i < 150; i++) // 15 seconds, checking every 100ms
    {
        if (s_vmInitComplete.load(std::memory_order_relaxed))
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // Timeout reached — init hung (likely TXM)
    if (!s_vmInitComplete.load(std::memory_order_relaxed))
    {
        std::fprintf(stderr, "@@BOOT_FAIL@@ reason=vm_init_timeout stage=cpu_thread_initialize\n");
        std::fflush(stderr);
        dispatch_async(dispatch_get_main_queue(), ^{
            Host::ReportErrorAsync("JIT Init Timeout",
                "JIT memory setup took too long. This is a known issue with the Universal TXM "
                "protocol on iOS 26. Try Settings → Emulator → JIT Script → Legacy, or relaunch "
                "via StikDebug.");
            [[NSNotificationCenter defaultCenter] postNotificationName:@"ARMSX2iOSReturnToMenu" object:nil];
        });
        // Reset thread state so next boot attempt creates a new thread
        std::lock_guard<std::mutex> lk(s_vmMutex);
        s_vmThreadCreated = false;
    }
});

// After CPUThreadInitialize returns (success or fail):
s_vmInitComplete.store(true);
watchdog.detach(); // let it finish naturally
```

### 4b: TXM prepare timeout (8 seconds)

**File:** `common/Darwin/DarwinMisc.cpp` — `MmapCodeDualMap`

The current TXM prepare/detach cycle runs inline with a `sigsetjmp`/`siglongjmp` SIGTRAP handler. If the `brk #0xf00d` instruction hangs (doesn't trap, just blocks), there's no timeout.

The fix from PR #202: run the TXM prepare on a worker `std::thread` with a timeout. If the worker doesn't finish within 8 seconds, fall back to the Legacy `brk #0x69` protocol.

```cpp
// Inside MmapCodeDualMap, replacing the inline TXM prepare section:

// Try Universal TXM with timeout. If it hangs, fall back to Legacy.
bool txm_ok = false;
{
    std::atomic<bool> prepare_done{false};
    std::thread txm_worker([&]() {
        // Existing JIT26PrepareRegion + JIT26Detach code here
        // (the sigsetjmp/siglongjmp SIGTRAP handler)
        // ...
        prepare_done.store(true);
    });
    txm_worker.detach();

    // Wait up to 8 seconds
    for (int i = 0; i < 80; i++)
    {
        if (prepare_done.load(std::memory_order_relaxed))
        {
            txm_ok = /* result from worker */;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

if (!txm_ok)
{
    std::fprintf(stderr, "@@JIT_ALLOC@@ txm_universal_timeout — falling back to legacy brk #0x69\n");
    std::fflush(stderr);
    // Fall back to Legacy protocol (brk #0x69)
    // ... existing legacy TXM code ...
}
```

**Note:** The worker thread accessing the SIGTRAP handler from a different thread is safe because `sigaction` is process-wide. The `sigjmp_buf` must be `thread_local` (it already is in the current code: `static thread_local sigjmp_buf s_brk_jmp`).

### 4c: `@@BOOT_FAIL@@` error reporting

**File:** `pcsx2/Memory.cpp` — `AllocateMemoryMap`

When `MmapCodeDualMap` returns null, emit the `@@BOOT_FAIL@@` marker with richer diagnostics (adapted from PR #202):

```cpp
if ((s_code_memory = ...) == nullptr)
{
    std::fprintf(stderr, "@@BOOT_FAIL@@ reason=ios_code_alloc_failed stage=code_dualmap\n");
    std::fflush(stderr);
    Host::ReportErrorAsync("Error",
        "Failed to allocate iOS executable code memory. "
        "Try Settings → Emulator → JIT Script → Legacy, or relaunch via StikDebug.");
    ReleaseMemoryMap();
    return false;
}
```

---

## Component 5: Boot-Time Revalidation (re-boot path fix)

**File:** `platforms/ios/app/src/main/cpp/IOS/SceneDelegate.mm` — `startVMThread`

The critical gap: on re-boot (when `s_vmThreadCreated` is true), the code just signals the existing thread and returns — no JIT revalidation. Fix:

```objc
// In startVMThread, BEFORE the "Signal the persistent thread" block:
if (s_vmThreadCreated) {
    // Re-validate JIT before signaling the existing thread.
    // The persistent thread bypasses CPUThreadInitialize, so it reuses
    // the JIT memory allocated at first boot. If iOS revoked the grant,
    // that memory is dead and the recompiler would write into a void.
    if (!DarwinMisc::iPSX2_FORCE_EE_INTERP && !DarwinMisc::ValidateJITAlive())
    {
        std::fprintf(stderr, "@@BOOT_JIT_GATE@@ revalidate=0 fallback=interpreter\n");
        std::fflush(stderr);
        // Fall back to interpreter — force settings and reset thread
        DarwinMisc::iPSX2_FORCE_EE_INTERP = 1;
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableEE", false);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableIOP", false);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU0", false);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableVU1", false);
        s_settings_interface->SetBoolValue("EmuCore/CPU/Recompiler", "EnableFastmem", false);
        s_settings_interface->Save();

        Host::AddIconOSDMessage("JITExpired", ICON_FA_TRIANGLE_EXCLAMATION,
            "JIT session expired — booting in interpreter mode (much slower). "
            "Relaunch the app to re-enable JIT.",
            15.0f);

        // Reset the persistent thread so next boot goes through CPUThreadInitialize.
        // The existing thread will exit (its wait-loop sees s_vmThreadCreated=false
        // on next iteration — we need to signal it to wake and check).
        {
            std::lock_guard<std::mutex> lk(s_vmMutex);
            s_vmThreadCreated = false;
            s_requestVMBoot.store(false);
        }
        // Fall through to create a new thread (first-boot path)
    }
    else
    {
        // JIT is alive — normal re-boot path
        std::fprintf(stderr, "@@BOOT_START_THREAD@@ active=0 created=1 action=signal\n");
        std::fflush(stderr);
        s_vmCV.notify_one();
        return;
    }
}
```

**Important subtlety:** When we reset `s_vmThreadCreated = false`, the old persistent thread is still blocked on `s_vmCV.wait()`. We must wake it so it can check the state and exit cleanly. Add `s_vmCV.notify_one()` before falling through. The old thread's wait-loop condition (`s_requestVMBoot`) is false, so it needs a separate "should exit" flag.

**Cleaner approach:** Add an `s_vmThreadShouldExit` atomic. Set it when resetting, notify the CV, and have the thread check it in its wait predicate:

```objc
static std::atomic<bool> s_vmThreadShouldExit{false};

// In the persistent boot loop's wait condition:
s_vmCV.wait(lk, [] { return s_requestVMBoot.load() || s_vmThreadShouldExit.load(); });
if (s_vmThreadShouldExit.load(std::memory_order_relaxed))
{
    s_vmThreadShouldExit.store(false);
    break; // exit the while(true) loop — thread ends
}
```

When resetting for interpreter fallback:
```objc
s_vmThreadShouldExit.store(true);
s_vmCV.notify_one();
// Wait briefly for old thread to exit, then fall through to create new thread
```

---

## Data Flow Summary

```
                          ┌──────────────────────────┐
                          │   App launches            │
                          │   JIT enabler attaches    │
                          │   CS_DEBUGGED = 1         │
                          └───────────┬──────────────┘
                                      ▼
                          ┌──────────────────────────┐
                          │ checkJITAndStartVM        │
                          │ IsJITAvailable() = true   │
                          │ ValidateJITAlive() = true │
                          │ → startVMThread (first)   │
                          └───────────┬──────────────┘
                                      ▼
                          ┌──────────────────────────┐
                          │ CPUThreadInitialize       │
                          │ ├─ MmapCodeDualMap        │
                          │ │  ├─ TXM prepare (8s TO) │
                          │ │  │  └─ hang? → Legacy   │
                          │ │  └─ vm_remap RW alias   │
                          │ ├─ 15s watchdog running   │
                          │ └─ JIT acquired ✓         │
                          │                            │
                          │ ARMSX2StartJITKeepalive()  │
                          │ (12s timer, VM idle only)  │
                          └───────────┬──────────────┘
                                      ▼
                          ┌──────────────────────────┐
                          │ User plays game           │
                          │ Keepalive paused (VM busy)│
                          └───────────┬──────────────┘
                                      ▼
                          ┌──────────────────────────┐
                          │ User returns to menu      │
                          │ VM shuts down             │
                          │ Keepalive resumes         │
                          └───────────┬──────────────┘
                                      ▼
                          ┌──────────────────────────┐
                          │ User browses settings     │
                          │ ~30-60s passes            │
                          │ iOS revokes CS_DEBUGGED   │
                          └───────────┬──────────────┘
                                      ▼
                          ┌──────────────────────────┐
                          │ Keepalive timer fires     │
                          │ ValidateJITAlive() = false│
                          │ Posts JITExpired notice   │
                          │ Timer stops               │
                          └───────────┬──────────────┘
                                      ▼
                          ┌──────────────────────────┐
                          │ User taps game            │
                          │ checkJITAndStartVM:       │
                          │ JIT dead → fallback       │
                          │ ├─ iPSX2_FORCE_EE_INTERP=1│
                          │ ├─ EnableEE=false, etc.   │
                          │ ├─ OSD: "Interpreter mode"│
                          │ └─ startVMThread          │
                          │    └─ new thread (reset)  │
                          │       └─ CPUThreadInit    │
                          │          └─ skips code mem│
                          │             (interpreter) │
                          │ Game boots (slow, ~5 fps) │
                          └──────────────────────────┘

Next app launch:
  JIT enabler re-attaches → CS_DEBUGGED = 1
  checkJITAndStartVM: JIT alive → restore EnableEE=true, etc.
  Normal JIT boot
```

---

## Availability Edge Cases

| Scenario | Behavior |
|----------|----------|
| JIT available at launch, stays available | Normal JIT boot, keepalive timer runs but always passes |
| JIT available at launch, revoked after 45s | Keepalive detects, interpreter fallback on next boot |
| JIT available at launch, TXM prepare hangs | 8s timeout → Legacy fallback, or 15s watchdog → error dialog |
| JIT unavailable at launch | Interpreter fallback immediately, OSD notification |
| App backgrounded 60s, foregrounded, JIT dead | Revalidation in `checkJITAndStartVM` catches it |
| Re-boot (2nd game) with JIT still alive | `ValidateJITAlive` passes, normal re-boot path |
| Re-boot (2nd game) with JIT dead | `ValidateJITAlive` fails, thread reset, interpreter fallback |
| iOS Simulator | `IsJITAvailable` returns true, `ValidateJITAlive` returns true, keepalive is a no-op |
| User manually selects interpreter in Settings | Works as before — `iPSX2_FORCE_EE_INTERP` not set (user's choice via `EnableEE=false`) |

---

## Files Changed

| File | Component | Lines (approx) |
|------|-----------|------|
| `common/Darwin/DarwinMisc.h` | 1 | Add `ValidateJITAlive()` declaration |
| `common/Darwin/DarwinMisc.cpp` | 1, 4b | `ValidateJITAlive()` impl, TXM timeout worker |
| `platforms/ios/app/src/main/cpp/IOS/SceneDelegate.mm` | 2, 3a, 4a, 5 | Keepalive timer, interpreter fallback in boot gate, VM init watchdog, re-boot revalidation, `s_vmThreadShouldExit` |
| `pcsx2/Memory.cpp` | 3b, 4c | Skip code alloc when `iPSX2_FORCE_EE_INTERP`, `@@BOOT_FAIL@@` reporting |
| `platforms/ios/app/src/main/swift/Models/SettingsStore.swift` | 3c | Fix `applyFullInterpreterPreset()` to write `EnableEE=false` |
| `docs/ios-troubleshooting-jit.md` (new) | — | User-facing troubleshooting guide |

---

## What This Design Does NOT Include (YAGNI)

- **No IR/cached interpreter.** PCSX2 does not have one. Building one is a multi-month effort, out of scope.
- **No debugger re-attach automation.** Triggering StikDebug/LiveContainer deep links is fragile and depends on a specific enabler app. A user-visible "relaunch" message is more reliable.
- **No background task keepalive.** `beginBackgroundTaskWithExpirationHandler` gives ~30s and drains battery. The foreground timer + boot-time revalidation covers the primary use case.
- **No interpreter performance optimization.** The pure interpreter is used as-is. Single-digit FPS is expected.
- **No per-game interpreter setting via the new UI.** The existing per-game `EnableEE=false` path already works once 3c is fixed.

---

## Implementation Order

1. **Component 1** (ValidateJITAlive) — self-contained, can be tested independently
2. **Component 3b** (Memory.cpp skip) — small, self-contained
3. **Component 3c** (Swift preset fix) — small, self-contained
4. **Component 3a + 3d** (boot gate interpreter fallback + restore) — depends on 1, 3b
5. **Component 5** (re-boot revalidation) — depends on 1, 3a
6. **Component 2** (keepalive timer) — depends on 1
7. **Component 4a** (VM init watchdog) — self-contained
8. **Component 4b** (TXM timeout) — self-contained but delicate (threading + signal handling)
9. **Docs** (troubleshooting guide)

Each component compiles independently. Components 1, 3b, 3c, 4a can land first as a minimum viable safety net. Components 2, 4b, 5 are enhancements.

---

## Testing Checklist

- [ ] Launch with JIT, play game, return to menu, wait 45s, launch another game → JIT mode (keepalive should keep JIT alive or detect revocation)
- [ ] Launch with JIT, wait 60s without playing, launch game → interpreter fallback with OSD message
- [ ] Force-quit, relaunch → JIT re-acquired, interpreter flags restored
- [ ] Manually select "Full Interpreter" preset in Settings → game boots in interpreter mode (EnableEE=false confirmed in log)
- [ ] Manually select "ARM64 JIT" preset after interpreter → EnableEE restored to true
- [ ] On iOS Simulator → keepalive no-op, no JIT validation failures
- [ ] TXM Universal hang simulation (if testable) → 8s timeout → Legacy fallback
- [ ] VM init timeout (if testable) → 15s watchdog → error dialog → return to menu
- [ ] Boot game after interpreter fallback → no crash from null s_code_memory
- [ ] `@@JIT_KEEPALIVE@@` log lines appear at 12s intervals when VM idle
- [ ] `@@BOOT_FAIL@@` log line appears on code allocation failure
