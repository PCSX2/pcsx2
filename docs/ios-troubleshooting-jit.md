# ARMSX2 iOS — JIT Troubleshooting

## Symptom: Black screen when booting a game after time in settings

### Cause

iOS revokes the JIT grant (`CS_DEBUGGED` flag) after approximately 30–60 seconds of app inactivity. If you browse settings or leave the app idle before launching a game, the JIT recompiler can no longer write code, and the game fails to boot — producing a permanent black screen.

### What ARMSX2 does about it

As of version 2.4.1, ARMSX2 includes a **JIT resilience layer** that handles this automatically:

1. **Keepalive timer** — A background timer checks every 12 seconds whether JIT is still active. If iOS has revoked it, you'll see a notification.

2. **Interpreter fallback** — If JIT is dead when you launch a game, ARMSX2 falls back to the pure interpreter (much slower — expect single-digit FPS). You can still test games and menus, but gameplay won't be smooth. An on-screen message will tell you this happened.

3. **Boot watchdog** — If JIT initialization hangs (a known issue with the Universal TXM protocol on iOS 26), a 15-second timeout shows an error dialog instead of leaving you with a permanent black screen.

### How to fix it

**Relaunch the app.** Force-quit ARMSX2 and open it again through your JIT enabler (StikDebug, LiveContainer, SideStore, etc.). The enabler re-attaches and re-grants JIT. Once JIT is available again, ARMSX2 automatically restores the recompiler.

---

## Symptom: Black screen on first boot (iOS 26.x)

### Cause

The Universal TXM protocol (`brk #0xf00d` prepare + detach) can hang during registration of large code regions (~161 MB) on iOS 26.5+. The hang is silent — no error, no crash, just a permanent black screen.

### What ARMSX2 does about it

- The Universal TXM prepare now runs on a worker thread with an **8-second timeout**. If it hangs, ARMSX2 automatically falls back to the Legacy protocol (`brk #0x69`), which is reliable on tested devices.
- A **15-second VM init watchdog** catches the case where the entire initialization hangs. If it does, you'll see a dialog: *"JIT memory setup took too long."*

### Manual workaround (if automatic fallback fails)

1. **Settings → Emulator → JIT Script → Legacy**
2. Fully close and reopen the app.

The Legacy protocol uses `brk #0x69` for TXM registration, which has been reliable on:
- iPhone 15 Plus, iOS 26.5
- iPhone 17, iOS 26.5

### Environment variables (advanced)

| Variable | Effect |
|----------|--------|
| `ARMSX2_JIT_PROTOCOL=legacy` | Force Legacy `brk #0x69` protocol |
| `ARMSX2_JIT_PROTOCOL=universal` | Force Universal `brk #0xf00d` protocol |
| `ARMSX2_FORCE_DUAL_MAP=1` | Force dual-mapping without TXM |

---

## Symptom: "JIT session expired — interpreter mode"

### What happened

You spent too long in the settings menu (>60 seconds), iOS revoked the JIT grant, and ARMSX2 fell back to the interpreter so you could still boot the game.

### Performance expectation

The interpreter is **extremely slow** — expect 1–10 FPS depending on the game. This is a PS2 architecture limitation (no cached interpreter mode exists in PCSX2). Use it for testing and compatibility checks, not for gameplay.

### How to restore full speed

Force-quit the app and relaunch through your JIT enabler. ARMSX2 will detect JIT is available again and automatically restore the recompiler.

---

## Diagnostic log markers

Check `Documents/pcsx2_log.txt` for these markers:

| Marker | Meaning |
|--------|---------|
| `@@JIT_DETECT@@ CS_DEBUGGED=1` | JIT grant is active |
| `@@JIT_DETECT@@ CS_DEBUGGED=0` | JIT grant is NOT active |
| `@@JIT_KEEPALIVE@@ alive=1` | Keepalive timer confirms JIT is still working |
| `@@JIT_KEEPALIVE@@ alive=0 reason=cs_debugged_revoked` | iOS revoked the JIT grant |
| `@@JIT_KEEPALIVE@@ alive=0 reason=rw_alias_dead` | The RW memory alias is no longer writable |
| `@@BOOT_JIT_GATE@@ available=1` | JIT is available, booting with recompiler |
| `@@BOOT_JIT_GATE@@ available=0 fallback=interpreter` | JIT is dead, booting with interpreter |
| `@@BOOT_JIT_GATE@@ revalidate=0 fallback=interpreter` | JIT died between boots, falling back |
| `@@BOOT_FAIL@@ reason=vm_init_timeout` | VM initialization hung for >15 seconds |
| `@@BOOT_FAIL@@ reason=ios_code_alloc_failed` | Code memory allocation failed |
| `@@JIT_ALLOC@@ txm_universal_timeout` | Universal TXM hung, fell back to Legacy |
| `@@CPU_CONFIG@@ ee=1` | EE recompiler is enabled |
| `@@CPU_CONFIG@@ ee=0` | EE interpreter is active |
| `@@CPU_CONFIG@@ forced_interp=1` | Interpreter mode forced due to JIT unavailability |

---

## Still having issues?

1. Check the log for `@@BOOT_FAIL@@` or `@@JIT_KEEPALIVE@@ alive=0` markers.
2. Confirm your JIT enabler (StikDebug, LiveContainer) is properly configured.
3. Try Settings → Emulator → JIT Script → Legacy if on iOS 26.
4. If the interpreter fallback is too slow, relaunch the app to re-enable JIT.
