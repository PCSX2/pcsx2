package kr.co.iefriends.pcsx2;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.ParcelFileDescriptor;
import android.os.Looper;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.system.Os;
import android.system.OsConstants;
import android.view.InputDevice;
import android.view.Surface;

import com.armsx2.BiosInfo;
import com.armsx2.EmuState;
import com.armsx2.runtime.MainActivityRuntime;
import com.armsx2.events.TestResult;

import java.io.File;
import java.lang.ref.WeakReference;

public class NativeApp {
	static {
		String libraryName = selectNativeLibraryName();
		try {
			System.loadLibrary(libraryName);
			hasNoNativeBinary = false;
			System.out.println("PCSX2_LOAD " + libraryName + " pageSize=" + getRuntimePageSize());
		} catch (UnsatisfiedLinkError e) {
			hasNoNativeBinary = true;
			System.err.println("PCSX2_LOAD_FAILED " + libraryName + ": " + e.getMessage());
		}
	}

	public static boolean hasNoNativeBinary;

	private static long getRuntimePageSize() {
		try {
			long pageSize = Os.sysconf(OsConstants._SC_PAGESIZE);
			return pageSize > 0 ? pageSize : 4096;
		} catch (Throwable ignored) {
			return 4096;
		}
	}

	private static String selectNativeLibraryName() {
		return getRuntimePageSize() >= 16384 ? "emucore_16k" : "emucore_4k";
	}


	protected static WeakReference<Context> mContext;
	public static Context getContext() {
		return mContext != null ? mContext.get() : null;
	}

	public static void initializeOnce(Context context) {
		mContext = new WeakReference<>(context);

		// Compute the app's externalFilesDir up front — it's the BIOS
		// folder (always app-owned + writable, where the setup wizard's
		// finishBiosStep deposits the BIOS file) and the fallback for
		// DataRoot when the user hasn't picked one.
		File externalFilesDir = context.getExternalFilesDir(null);
		if (externalFilesDir == null) {
			externalFilesDir = context.getDataDir();
		}

		// DataRoot: prefer the user-chosen system folder only when the SAF tree
		// URI resolves to a POSIX path that native code can actually write.
		// Falls back to externalFilesDir when unset, unresolvable, or blocked
		// by scoped storage.
		String chosen = MainActivityRuntime.Companion.systemDirPosix();
		if (chosen != null && !MainActivityRuntime.Companion.validateSystemDirWritable(chosen)) {
			chosen = null;
		}
		String dataPath = (chosen != null) ? chosen : externalFilesDir.getAbsolutePath();

		// BIOS folder: the directory that actually holds the configured BIOS file.
		// The setup wizard (and the migration in MainActivityRuntime.kickoffEmucoreInit) keep the
		// BIOS in app-private internal storage — NOT under a custom/SD data root —
		// because the native FileSystem APIs can't reliably open a BIOS off a
		// removable/SAF volume on Android 11+ (that made a data-root-on-SD game fail
		// VM init and bounce back to the library). Falls back to externalFilesDir/bios
		// (always app-owned + readable), matching that decoupled-BIOS design.
		String biosFolder = MainActivityRuntime.Companion.biosFolderPosix();
		if (biosFolder == null || biosFolder.isEmpty()) {
			biosFolder = externalFilesDir.getAbsolutePath() + java.io.File.separator + "bios";
		}

		initialize(dataPath, biosFolder, android.os.Build.VERSION.SDK_INT);

		// Replay a host override that arrived via broadcast while the native
		// library was not yet loaded.
		com.armsx2.RetroAchievementsHostOverrideReceiver.applyPending(context);
	}

	public static native void initialize(String path, String biosFolder, int apiVer);

	// PGO instrument build only: flush collected profile counters to disk.
	// No-op in normal builds (the native impl is empty without -fprofile-generate).
	public static native void dumpPgoProfile();

	/** The tweakable parameters a .slangp preset declares, as JSON, or null if it can't be
	 *  read (no librashader in this build, or an unreadable preset). Pure file parsing — no
	 *  renderer or running game needed. */
	public static native String shaderPresetParams(String presetPath);

	/** Queues parameter values for the running shader chain; the GS thread applies them on
	 *  its next frame. Safe to call from any thread, and safe with no VM or renderer up —
	 *  the values just sit there until a chain reads them.
	 *
	 *  [names]/[values] are parallel arrays of assignments, NOT the chain's full state:
	 *  a parameter left out keeps what the chain has, so resetting one means sending its
	 *  initial value rather than omitting it. [presetPath] must be the preset the values
	 *  were read off, so a stale set can't land on a chain that has moved on. */
	public static native void setShaderChainParams(String presetPath, String[] names, float[] values);

	// Save a GS dump (.gs of GPU commands) to the snaps folder for diagnosing
	// rendering bugs. frames <= 0 captures a single frame.
	public static native void captureGsDump(int frames);

	// @@EEDIFF@@ Toggle the EE recompiler-vs-interpreter differential verifier (throwaway
	// diagnostic). Enabling clears the EE block cache so blocks recompile with per-op
	// verify hooks; the first miscompiling guest instruction logs "@@EEDIFF@@ ... DIVERGE".
	// Off by default = zero overhead / normal speed. Debug tool only — heavy slowdown when on.
	public static native void setEeDiffVerify(boolean enabled);

	/**
	 * Push one EmuCore setting into the base settings layer. Mirrors
	 * pcsx2-qt's Settings save flow — Host::SetBase*SettingValue sticks
	 * in s_settings_interface. type ∈ {"bool","int","float","string"};
	 * value is the stringified payload (e.g. "true", "2", "2.5").
	 *
	 * Setting writes here are NOT live until commitSettings() is called.
	 * Batch the writes, then commit once so the VM applies them atomically.
	 */
	public static native void setSetting(String section, String key, String type, String value);

	/**
	 * Apply queued settings to a running VM (and the GS thread). Calls
	 * VMManager::ApplySettings + MTGS::ApplySettings. No-op when no VM
	 * is running — settings still take effect on the next runVMThread
	 * because they were pushed to the persistent base settings layer.
	 *
	 * Some settings need a VM restart (recompiler enables, EE cycle rate);
	 * the UI layer should flag those.
	 */
	public static native void commitSettings();

	/**
	 * Live GS-only reconfigure for a running VM. Reloads the whole EmuCore/GS
	 * section from the base settings layer and pushes it to the GS thread via
	 * MTGS::ApplySettings WITHOUT the heavier VMManager::ApplySettings (no
	 * CPU/JIT rebuild). Call after pushing EmuCore/GS keys via setSetting() so
	 * renderer / hardware-fix / upscaling-fix changes apply mid-game. No-op
	 * when the GS is closed — the keys still take effect on next launch.
	 */
	public static native boolean applyGSSettingsLive();
	public static native int reloadPatches();
	public static native boolean reloadTextureReplacements();

	/**
	 * Set which named patches/cheats are enabled (the [Patches]/[Cheats]
	 * "Enable" list PCSX2 actually applies). Pass ALL of the game's entry names
	 * for that category plus the selected subset; persisted, then call
	 * {@link #reloadPatches()} to apply. Writing the .pnach file alone is NOT
	 * enough — a patch is inert unless its name is enabled here.
	 */
	public static native void setEnabledPatches(boolean cheats, String[] allNames, String[] enabledNames);
	public static native String getGameTitle(String path);
	public static native String getGameSerial();
	public static native String getGameCRC();
	public static native float getFPS();

	/** Build version string from BuildVersion::GitRev — formatted as
	 *  "GitTagHi.GitTagMid.GitTagLo.ARMSX2Build-SNAPSHOT". Used by the
	 *  setup wizard + in-game overlay branding so the displayed version
	 *  tracks the C++ constants without a Kotlin-side hardcoded copy. */
	public static native String getBuildVersion();

	public static native String getPauseGameTitle();
	public static native String getPauseGameSerial();

	/** Snapshot the current game's achievements as JSON for the in-game
	 *  overlay's right-side panel. See Achievements::GetAchievementsAsJSON
	 *  in the C++ side for the schema. Returns the empty-state payload
	 *  (active=false, items=[]) when no game is loaded or not logged in. */
	public static native String getAchievementsJSON();

	/** Live RetroAchievements rich-presence string. Recomputed every
	 *  second on the native side from the game's RAM. Empty when no game,
	 *  no client, or RP not supported by the loaded set. */
	public static native String getRichPresence();

	/** RetroAchievements password login. Returns null on success or a
	 *  human-readable error string. Synchronous — runs the HTTP login
	 *  request to completion, may take a few seconds. Callers MUST
	 *  dispatch off the Main thread (Dispatchers.IO from Compose).
	 *  After success the next getAchievementsJSON poll will reflect
	 *  loggedIn=true; no separate callback wiring needed. */
	public static native String loginAchievements(String username, String password);

	/** RetroAchievements logout. Idempotent. */
	public static native void logoutAchievements();

	/** Toggle RetroAchievements hardcore mode. Writes
	 *  EmuConfig.Achievements.HardcoreMode and triggers ApplySettings —
	 *  enabling hardcore on a running VM resets it on the next frame
	 *  (per upstream's design). Save states / cheats / runahead are
	 *  blocked by Achievements.cpp gates while active. */
	public static native void setHardcoreMode(boolean enabled);

	/** True iff the rcheevos hardcore flag is currently set. The Kotlin
	 *  achievements panel polls this for the badge / button colour. */
	public static native boolean isHardcoreMode();
	public static native boolean isHardcorePersisted();

	/** Toggle a RetroAchievements presentation option. {@code key} is one of
	 *  "notifications", "leaderboardNotifications", "overlays", "lbOverlays",
	 *  "soundEffects" (mapped native-side to the [Achievements] INI key).
	 *  Persists + applies live; current values are reported in
	 *  {@link #getAchievementsJSON}. */
	public static native void setAchievementsOption(String key, boolean enabled);

	// Custom achievement-unlock sound. `path` is an app-private absolute file the
	// MediaPlayer can read; an empty string clears it back to the bundled default.
	public static native void setAchievementsUnlockSound(String path);

	/** Repoint the RetroAchievements client at a loopback proxy. Persists
	 *  the [Achievements] Host setting (read by CreateClient), forces
	 *  hardcore off while active — saving the prior choice — and rebuilds
	 *  the client so a running session picks up the new host. */
	public static native void setAchievementsHostOverride(String host);

	/** Drop the host override set by {@link #setAchievementsHostOverride},
	 *  restoring the saved hardcore choice and rebuilding the client. */
	public static native void clearAchievementsHostOverride();

	/** True iff the GS is currently in a HW renderer (OGL/VK), false for
	 *  SW. Mirrors GSIsHardwareRenderer() from the GS thread. Polled by
	 *  the in-game overlay's renderer pill so emucore-driven swaps
	 *  (e.g. SoftwareRendererFMVHack) stay in sync with the UI. */
	public static native boolean isHardwareRenderer();

	/** Master OSD toggle — flips every OsdShow* bit we enable at first
	 *  init. Backs the in-game overlay's OSD pill. */
	public static native void osdShowAll(boolean enabled);

	// Live-only OSD flag apply (no persist) — lets the OSD on/off hotkey hide/restore stats
	// without clobbering the user's saved per-stat selection.
	public static native void osdApplyFlags(boolean fps, boolean vps, boolean speed, boolean cpu,
		boolean gpu, boolean res, boolean gsStats, boolean frameTimes, boolean hwInfo,
		boolean version, boolean settings, boolean inputs);

	/** Per-element OSD toggles (Performance Overlay tab). Apply live via
	 *  EmuConfig.GS + MTGS::ApplySettings; persistence to base is done on
	 *  the Kotlin side via setSetting. Disabling GPU also stops the GPU
	 *  timing queries (real perf win), see GS.cpp. */
	public static native void osdShowFPS(boolean enabled);
	public static native void osdShowVPS(boolean enabled);
	public static native void osdShowSpeed(boolean enabled);
	public static native void osdShowCPU(boolean enabled);
	public static native void osdShowGPU(boolean enabled);
	public static native void osdShowResolution(boolean enabled);
	public static native void osdShowGSStats(boolean enabled);
	public static native void osdShowFrameTimes(boolean enabled);
	public static native void osdShowHardwareInfo(boolean enabled);
	public static native void osdShowMessages(boolean enabled);
	public static native void osdShowGpuStats(boolean enabled);
	public static native void osdShowVersion(boolean enabled);
	public static native void osdShowSettings(boolean enabled);
	public static native void osdShowInputs(boolean enabled);
	/** OSD size (percentage; 25–500, 100 = normal). Applies live via MTGS. */
	public static native void osdSetScale(float scale);

	/** Per-game settings export — writes only the keys that differ from global
	 *  into gamesettings/<serial>_<CRC>.ini for the running game (sparse, like
	 *  PCSX2's desktop UI). Stream: gameIniBeginWrite() once, gameIniPut() per
	 *  override key, gameIniCommitWrite() to save (or delete when empty). */
	public static native boolean gameIniBeginWrite();
	public static native void gameIniPut(String section, String key, String value);
	public static native boolean gameIniCommitWrite();

	/** Pin a custom Vulkan driver (e.g. Mesa Turnip) for the next VM
	 *  start. Must be called BEFORE MainActivityRuntime.start() — the first MTGS::Open
	 *  triggers Vulkan::LoadVulkanLibrary which reads these paths. Pass
	 *  empty strings to revert to the system loader.
	 *
	 *  driverDir:    /data/.../files/drivers/&lt;id&gt;/ (trailing slash required)
	 *  driverName:   e.g. "libvulkan_freedreno.so"
	 *  redirectDir:  /data/.../files/drivers/&lt;id&gt;/cache/ — Turnip shader cache target
	 *  hookLibDir:   ApplicationInfo.nativeLibraryDir — where the adrenotools
	 *                hook .so's (main_hook etc.) were extracted. */
	public static native void setCustomVulkanDriver(
		String driverDir, String driverName,
		String redirectDir, String hookLibDir);

	public static native void setPadVibration(boolean isonoff);
	public static native void setPadButton(int index, int range, boolean iskeypressed);
	/** Local co-op: like setPadButton but routes to PS2 controller port 0 (Player 1)
	 *  or 1 (Player 2). The plain setPadButton above stays port-0 for touch controls. */
	public static native void setPadButtonForPort(int port, int index, int range, boolean iskeypressed);
	/** Local co-op: hot-plug a 2nd DualShock2 into PS2 port 2 when a second physical
	 *  controller joins. Idempotent; briefly parks the VM to rebuild the pad list. */
	public static native void enablePad2();
	/** PS2 Multitap: enable/disable the 3 extra pad slots on one physical port
	 *  (port 0 = PS2 port 1 / unified slots 2,3,4; port 1 = PS2 port 2 / slots 5,6,7).
	 *  Idempotent; briefly parks the VM (up to ~3s) to rebuild the pad list, so call
	 *  it OFF the UI thread. */
	public static native void setMultitap(int port, boolean enabled);
	public static native void resetKeyStatus();

	// ---- USB keyboard (#254: EQOA / Konami-keyboard games) ----
	/** Attach ({@code true}) or detach ({@code false}) an emulated USB HID
	 *  keyboard on USB port {@code port} (0 = USB1, 1 = USB2). Persists
	 *  [USB{port+1}] Type = hidkbd/None and, when a VM is running, recreates the
	 *  device live so the game sees the (dis)connect. Call off the UI thread — a
	 *  live change briefly parks the emulation pipeline. */
	public static native void usbSetKeyboardEnabled(int port, boolean enabled);
	/** Feed one Android hardware {@link android.view.KeyEvent} to the emulated USB
	 *  keyboard on {@code port}. {@code androidKeyCode} is {@code KeyEvent.keyCode};
	 *  {@code pressed} is the down/up state. Returns {@code true} iff a USB keyboard
	 *  is attached to that port AND the key mapped to a HID usage — i.e. the event
	 *  was consumed by the emulated keyboard and should NOT also drive the pad /
	 *  frontend. No-op (returns {@code false}) otherwise. */
	public static native boolean usbKeyboardKey(int port, int androidKeyCode, boolean pressed);

	// ---- Controller rumble (BT/USB gamepads via Android InputDevice) ----
	// Device id of the most-recently-used gamepad, set from MainActivityRuntime.dispatchKeyEvent.
	public static volatile int sRumbleDeviceId = -1;
	// Master enable (default on).
	public static volatile boolean sRumbleEnabled = true;
	// One-shot length; re-issued when the game changes intensity, cancelled on
	// zero. Long enough to cover sustained rumble between intensity changes.
	private static final int RUMBLE_MS = 3000;

	/** Called from native (IOP thread) when PS2 pad motor intensity changes for
	 *  [pad] (unified slot: 0 = Player 1, 1 = Player 2). largeMotor/smallMotor are
	 *  0..255. Local co-op: routes the rumble to THAT player's controller. Falls
	 *  back to the last-used gamepad when the port isn't claimed yet (single-player,
	 *  or before first input) — solo play is unchanged. No-op with no vibrator. */
	public static void onPadRumble(int pad, int largeMotor, int smallMotor) {
		if (!sRumbleEnabled) return;
		int devId = com.armsx2.input.PadRouter.INSTANCE.deviceIdForPort(pad);
		if (devId < 0) devId = sRumbleDeviceId;
		// devId may stay -1 for touch-only Player 1 (no gamepad); vibrateDevice still
		// drives the device's own haptic for P1 (issue #241). P2 with no pad has no target.
		if (devId < 0 && pad != 0) return;
		float low = Math.max(0f, Math.min(1f, largeMotor / 255f));   // low-frequency / large
		float high = Math.max(0f, Math.min(1f, smallMotor / 255f));  // high-frequency / small
		vibrateDevice(devId, low, high, RUMBLE_MS, pad == 0);
	}

	// ---- Achievement / notification sound playback ----
	// Called from native Common::PlaySoundAsync (RetroAchievements unlock/info/
	// leaderboard-submit .wav). Fire-and-forget; must never throw back to JNI.
	// Uses MediaPlayer, not SoundPool: SoundPool decoded/resampled the 44.1 kHz
	// stereo PCM oddly and it came out "weird". MediaPlayer plays the .wav straight,
	// matching desktop PCSX2. One short-lived player per shot, released on complete.
	// Strong references to the currently-playing sound players. Without this the
	// MediaPlayer below is a pure local; once start() returns and the worker thread
	// exits, nothing roots it, so the GC (very active under a running emulator) could
	// finalize and release it MID-PLAYBACK — that's why unlock sounds dropped at random
	// with no error logged. Held from before start() until the completion/error callback.
	private static final java.util.Set<android.media.MediaPlayer> sActiveSounds =
			java.util.Collections.synchronizedSet(new java.util.HashSet<>());

	public static void playSound(String path) {
		if (path == null || path.isEmpty()) return;
		// Cap concurrent players — a burst of simultaneous unlocks (combo/milestone) could
		// otherwise exhaust the device's MediaPlayer/codec pool and make start() no-op.
		if (sActiveSounds.size() >= 4) return;
		new Thread(() -> {
			android.media.MediaPlayer mp = null;
			try {
				mp = new android.media.MediaPlayer();
				mp.setAudioAttributes(new android.media.AudioAttributes.Builder()
						.setUsage(android.media.AudioAttributes.USAGE_ASSISTANCE_SONIFICATION)
						.setContentType(android.media.AudioAttributes.CONTENT_TYPE_SONIFICATION)
						.build());
				mp.setDataSource(path);
				mp.setOnCompletionListener(m -> { sActiveSounds.remove(m); try { m.release(); } catch (Throwable ignore) {} });
				mp.setOnErrorListener((m, what, extra) -> { sActiveSounds.remove(m); try { m.release(); } catch (Throwable ignore) {} return true; });
				sActiveSounds.add(mp);
				mp.prepare();
				mp.start();
			} catch (Throwable t) {
				if (mp != null) { sActiveSounds.remove(mp); try { mp.release(); } catch (Throwable ignore) {} }
				android.util.Log.e("ARMSX2", "playSound failed: " + path, t);
			}
		}, "armsx2-ra-sound").start();
	}

	/** Drive [devId]'s vibrator(s) with the PS2 large/high motor intensities for [ms].
	 *  When the controller exposes no usable vibrator and [allowSystemFallback] is set,
	 *  drive the device's own haptic motor instead (issue #241 — handhelds like the
	 *  Odin 3 whose built-in gamepad has no rumble actuator, only system haptics). */
	private static void vibrateDevice(int devId, float low, float high, int ms, boolean allowSystemFallback) {
		try {
			// Single combined motor can't reproduce both PS2 actuators, so blend
			// them the way AetherSX2/NetherSX2 do (org.libsdl.app
			// SDLControllerManager): 0.6*large + 0.4*small. The PS2 small motor is
			// BINARY (full-scale 0xff whenever it pulses), so the old Math.max()
			// slammed the lone motor to FULL on every small-motor buzz — it felt
			// like the large motor was firing for small-motor events. The weighted
			// mix keeps a small-only pulse light and distinct from a large pulse.
			float combined = Math.min(1f, low * 0.6f + high * 0.4f);
			boolean drove = false;
			InputDevice dev = (devId >= 0) ? InputDevice.getDevice(devId) : null;
			if (dev != null) {
				if (Build.VERSION.SDK_INT >= 31) {
					VibratorManager vm = dev.getVibratorManager();
					int[] ids = vm.getVibratorIds();
					if (ids.length >= 2) {
						drove = rumbleOne(vm.getVibrator(ids[0]), low, ms);
						drove |= rumbleOne(vm.getVibrator(ids[1]), high, ms);
					} else if (ids.length == 1) {
						drove = rumbleOne(vm.getVibrator(ids[0]), combined, ms);
					} else {
						// Some pads (e.g. certain DualShock/DualSense BT modes) expose 0
						// vibrators to VibratorManager but still drive via the legacy API.
						drove = rumbleOne(dev.getVibrator(), combined, ms);
					}
				} else {
					drove = rumbleOne(dev.getVibrator(), combined, ms);
				}
			}
			// No controller actuator handled it → fall back to the device's built-in
			// haptic (issue #241), when permitted (Player 1 / explicit test) so a
			// vibrator-less P2 pad never buzzes the handheld that P1 is holding.
			if (!drove && allowSystemFallback) {
				rumbleOne(systemVibrator(), combined, ms);
			}
		} catch (Throwable ignored) {
		}
	}

	/** @return true if [v] is a real, usable vibrator that was driven (or cancelled). */
	private static boolean rumbleOne(Vibrator v, float intensity, int ms) {
		if (v == null || !v.hasVibrator()) return false;
		if (intensity <= 0f) {
			try { v.cancel(); } catch (Throwable ignored) {}
			return true;
		}
		int amp = Math.round(intensity * 255f);
		if (amp < 1) amp = 1;
		if (amp > 255) amp = 255;
		try {
			v.vibrate(VibrationEffect.createOneShot(ms, amp));
		} catch (Throwable t) {
			try { v.vibrate(ms); } catch (Throwable ignored) {}
		}
		return true;
	}

	// The device's own haptic motor (system vibrator), resolved once. On handhelds
	// like the Odin 3 the built-in gamepad exposes no rumble actuator — only this —
	// so it's the fallback target when a controller has no usable vibrator (issue #241).
	private static volatile Vibrator sSystemVibrator;
	private static Vibrator systemVibrator() {
		Vibrator v = sSystemVibrator;
		if (v != null) return v;
		try {
			Context ctx = getContext();
			if (ctx != null) {
				if (Build.VERSION.SDK_INT >= 31) {
					VibratorManager vm = (VibratorManager) ctx.getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
					v = (vm != null) ? vm.getDefaultVibrator() : null;
				} else {
					v = (Vibrator) ctx.getSystemService(Context.VIBRATOR_SERVICE);
				}
				if (v != null) sSystemVibrator = v;
			}
		} catch (Throwable ignored) {
		}
		return v;
	}

	// Short crisp haptic "tick" for on-screen touch button presses (issue #247),
	// PPSSPP/Azahar-style. Driven by the device's own vibrator and INDEPENDENT of
	// game rumble. The UI gates it via the Touch Haptics setting, so this is only
	// invoked when enabled. Coalesced: simultaneous multi-touch presses (d-pad +
	// face land in the same frame) collapse to ONE tick, and fast mashing is rate-
	// limited, so the vibrator queue can't be saturated on low-end devices.
	private static volatile long sLastTouchHapticMs = 0L;
	public static void touchHaptic() {
		long now = android.os.SystemClock.uptimeMillis();
		if (now - sLastTouchHapticMs < 24L) return;
		sLastTouchHapticMs = now;
		try { rumbleOne(systemVibrator(), 0.6f, 12); } catch (Throwable ignored) {}
	}

	/** Index (0-based) of the [index]th connected physical gamepad, or -1. Used as a
	 *  fallback so the rumble test works even before a port has been claimed in-game. */
	private static int nthGamepadDeviceId(int index) {
		int n = 0;
		for (int id : InputDevice.getDeviceIds()) {
			InputDevice d = InputDevice.getDevice(id);
			if (d == null) continue;
			int src = d.getSources();
			boolean pad = (src & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
					|| (src & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
			if (!pad) continue;
			if (n == index) return id;
			n++;
		}
		return -1;
	}

	/** Strongly buzz the controller mapped to [port] (0 = P1, 1 = P2) for ~500ms.
	 *  Falls back to the Nth gamepad when no port is claimed yet (tested outside a game). */
	public static void testRumble(int port) {
		int devId = com.armsx2.input.PadRouter.INSTANCE.deviceIdForPort(port);
		if (devId < 0) devId = nthGamepadDeviceId(port);
		// devId may stay -1 (touch-only / Odin built-in with no rumble); vibrateDevice
		// then falls back to the device's own haptic so the test still buzzes (issue #241).
		vibrateDevice(devId, 0.9f, 0.9f, 500, true);
	}

	/** One-line report of [port]'s controller and whether Android exposes any vibrator
	 *  for it (new VibratorManager + legacy API). Lets the Pad tab tell the user whether
	 *  a missing rumble is a routing issue or the pad just isn't drivable by Android. */
	public static String rumbleStatusForPort(int port) {
		int devId = com.armsx2.input.PadRouter.INSTANCE.deviceIdForPort(port);
		boolean mapped = devId >= 0;
		if (devId < 0) devId = nthGamepadDeviceId(port);
		if (devId < 0) return "Player " + (port + 1) + ": no controller found";
		InputDevice d = InputDevice.getDevice(devId);
		String name = (d != null && d.getName() != null) ? d.getName() : ("device " + devId);
		int vmCount = 0;
		boolean legacy = false;
		try {
			Vibrator lv = (d != null) ? d.getVibrator() : null;
			legacy = lv != null && lv.hasVibrator();
		} catch (Throwable ignored) {}
		if (Build.VERSION.SDK_INT >= 31 && d != null) {
			try { vmCount = d.getVibratorManager().getVibratorIds().length; } catch (Throwable ignored) {}
		}
		boolean hasRumble = vmCount > 0 || legacy;
		int motors = Math.max(vmCount, legacy ? 1 : 0);
		return "Player " + (port + 1) + ": " + name
				+ (mapped ? "" : " (not active in-game yet)")
				+ (hasRumble ? " — rumble OK (" + motors + " motor" + (motors == 1 ? "" : "s") + ")"
						: " — NO rumble exposed by Android");
	}

	public static native void setAspectRatio(int type);
	public static native void setFmvAspectRatio(int type);
	public static native void speedhackLimitermode(int value);
	/** Custom speed / FPS cap as a percent of native (100 = full speed).
	 *  Applies live to the running VM's frame pacer. */
	public static native void setNominalSpeed(int percent);
	/** Cap presented frames per second (0 = uncapped). Throttles only the
	 *  display swap, so emulation keeps running at 100% speed while the
	 *  on-screen FPS is limited. Applies live. */
	public static native void setFpsCap(int fps);
	/** Per-region emulated PS2 vsync rate (NTSC / PAL Hz), applied live without a
	 *  restart — recomputes the vsync pacer + target speed. Parks the VM briefly
	 *  (keeps audio alive), so call it off the UI thread (via LiveGsApplyQueue). */
	public static native void applyFramerateLive(float ntsc, float pal);
	/** Frame skip: present 1 frame, skip the next N (0 = off). Display-only
	 *  throttle; applies live. */
	public static native void setFrameSkip(int skip);
	/** SPU2 output volume, percent (0..200). Applies live + persists. */
	public static native void setAudioVolume(int volume);
	/** Mute/unmute SPU2 output. Applies live + persists. */
	public static native void setAudioMuted(boolean muted);
	/** Swap final stereo output channels L&lt;-&gt;R (flipped-speaker devices). Applies live + persists. */
	public static native void setAudioSwapChannels(boolean swap);
	public static native void speedhackEecyclerate(int value);
	public static native void speedhackEecycleskip(int value);
	public static native void setInstantVU1(boolean enabled);

	public static native void renderUpscalemultiplier(float value);
	public static native void renderMipmap(int value);
	public static native void renderHalfpixeloffset(int value);
	public static native void renderTvShader(int value);
	public static native void renderShadeBoost(boolean enabled, int brightness, int contrast, int saturation, int gamma);
	public static native void renderSoftware();
	public static native void renderOpenGL();
	public static native void renderVulkan();
	public static native void renderAuto();
	public static native void renderPreloading(int value);

	/** Flip texture dumping on/off live (PCSX2's ToggleTextureDumping hotkey).
	 *  Returns the new state. Runtime-only; no-op (returns false) with no VM. */
	public static native boolean toggleTextureDumping();

	/** Create a memory card in the memcards folder. type: 1=File, 2=Folder.
	 *  fileType (File only): 1=8MB, 2=16MB, 3=32MB, 4=64MB. Returns success. */
	public static native boolean createMemoryCard(String name, int type, int fileType);
	public static native boolean isMemoryCard(String name);

	public static native void onNativeSurfaceCreated();
	public static native void onNativeSurfaceChanged(Surface surface, int w, int h);
	public static native void onNativeSurfaceDestroyed();
	public static native void setDisplayRefreshRate(float hz);

	public static native boolean runVMThread(String path);
	public static native void pause();
	public static native void resume();
	public static native void shutdown();
	public static native boolean hasActiveVM();

	/** Persist the Vulkan pipeline cache to disk so cold restarts don't have
	 *  to recompile every TFX pipeline. No-op for OpenGL (its cache flushes
	 *  on its own). Called from MainActivityRuntime.onPause so backgrounding the app saves
	 *  the cache before Android can reap the process. Safe to call when no
	 *  Vulkan device is active (becomes a no-op). */
	public static native void flushShaderCache();

	/** Runs ARM64 codegen tests and prints PASS/FAIL to logcat (tag: ARM64CodegenTest). */
	public static native void runCodegenTests();

	/** Runs Patch::ApplyPatches tests and prints PASS/FAIL to logcat (tag: PatchTests). */
	public static native void runPatchTests();

	/** Runs microVU JIT integer-instruction tests and prints PASS/FAIL to logcat (tag: VuJitTests). */
	public static native void runVuJitTests();

	/** Runs R5900 EE interpreter instruction tests and prints PASS/FAIL to logcat (tag: EeJitTests). */
	public static native void runEeJitTests();

	/** Runs VIF UNPACK C++ template tests and prints PASS/FAIL to logcat (tag: VifTests). */
	public static native void runVifTests();

	/** Runs EE multi-instruction sequence tests and prints PASS/FAIL to logcat (tag: EeSeqTests). */
	public static native void runEeSeqTests();

	/** Called from native when a test suite finishes.  Override or observe to surface results in UI. */
	public static void onTestResults(String label, int passed, int total) {
		MainActivityRuntime.Companion.onTestResults(new TestResult(label, passed, total));
	}

	/**
	 * Probe a file descriptor for PS2 BIOS metadata. Used by the setup
	 * wizard's directory-based BIOS selector to enumerate candidates and
	 * show region/version per file. The fd MUST be detached (ownership
	 * transferred to native) before the call — emucore wraps it in a FILE*
	 * and closes it on return either way.
	 *
	 * Returns null if the file isn't a valid BIOS image.
	 */
	public static native BiosInfo getBiosInfoFromFd(int fd);

	/**
	 * Read enough of a PS2 disc image to extract its serial (e.g.
	 * "SLUS-20312"). Walks the ISO9660 directory to find SYSTEM.CNF and
	 * parses the BOOT2 line. Handles flat ISO/raw-sector images and CHDs;
	 * CSO/ZSO/GZ still return null and the caller falls back to filename
	 * parsing. fd is consumed (closed by native).
	 */
	public static native String getGameSerialFromFd(int fd);

	/**
	 * PCSX2 game-database compatibility lookup. Returns the raw 0-6
	 * Compatibility enum value:
	 *   0 Unknown, 1 Nothing, 2 Intro, 3 Menu, 4 InGame, 5 Playable, 6 Perfect
	 * Caller maps to the 5-star display.
	 */
	public static native int getCompatibilityForSerial(String serial);

	/** GameDB region string for a serial ("NTSC-U", "PAL-E", "PAL-IN", "NTSC-C", "NTSC-K",
	 *  "NTSC-HK", ...), or "" if the serial isn't in the database. Lets the library show
	 *  the real region (India/China/Korea/HK) a serial prefix alone can't distinguish. */
	public static native String getRegionForSerial(String serial);

	/** GameDB titles for a serial as "&lt;name&gt;\n&lt;name-sort&gt;\n&lt;name-en&gt;", or "" if the serial
	 *  isn't in the database. name-sort / name-en may be empty; name is set for any entry.
	 *
	 *  One call, one lookup — the library asks for every game it scans. For a Japanese game
	 *  name is the original title, name-sort its kana reading (sort by this, not the kanji),
	 *  and name-en the romanised one. */
	public static native String getTitlesForSerial(String serial);

	public static native boolean saveStateToSlot(int slot);
	public static native boolean loadStateFromSlot(int slot);
	public static native String getGamePathSlot(int slot);
	public static native byte[] getImageSlot(int slot);
	public static native byte[] getSaveStateImage(String path);

	// Hot-swap the CDVD disc on the running VM (keeps the session alive, cycles
	// the tray so the game detects the new disc). Returns false if there's no
	// valid VM or the new image failed to open (in which case the core has
	// already reverted to the previous disc). Used by the in-game Swap Disc
	// picker for CodeBreaker / multi-disc swaps. Call off the main thread — it
	// parks the CPU thread and blocks until the swap completes.
	public static native boolean changeDisc(String path);

	// Autosave-on-exit slot. Backed by a dedicated `.autosave.p2s` filename
	// in the savestate folder (see VMManager::SAVESTATE_SLOT_AUTOSAVE) so the
	// numbered slots 0-9 stay user-controlled. saveAutosaveState is called
	// from the in-game "Save State And Exit" menu; hasAutosaveState gates
	// the load picker's autosave tile.
	public static native boolean saveAutosaveState();
	public static native boolean loadAutosaveState();
	public static native boolean hasAutosaveState();
	public static native byte[] getAutosaveImage();
	public static native String getAutosaveGamePath();
	// Frames the GS has presented since it opened (host-side, not saved in the state). The
	// auto-load-on-boot path waits until this is advancing before restoring, so the load happens
	// once the renderer is actually presenting — otherwise the restored frame never reaches the
	// surface and the screen stays black.
	public static native int getPresentedFrameCount();

	public static void vmSetPaused(boolean paused) {
		new Handler(Looper.getMainLooper()).post(() -> {
			// Pause/resume callbacks can arrive after the user has already
			// requested Close Game / Reset. Do not let a stale resume flip the
			// Compose state back to RUNNING while the native VM is unwinding.
			if (MainActivityRuntime.isVmStopInProgress())
				return;
			if (!paused && MainActivityRuntime.eState.getValue() == EmuState.STOPPED)
				return;
			if (paused) {
				MainActivityRuntime.eState.setValue(EmuState.PAUSED);
			} else {
				MainActivityRuntime.eState.setValue(EmuState.RUNNING);
				// One-shot auto-load of the autosave state, if the user enabled
				// "Auto-load last state on boot" (no-op otherwise).
				MainActivityRuntime.onVmRunning();
			}
		});
	}

	// Call jni
	public static int openContentUri(String uriString) {
		Context _context = getContext();
		if(_context != null) {
			ContentResolver _contentResolver = _context.getContentResolver();
			try {
				ParcelFileDescriptor filePfd = _contentResolver.openFileDescriptor(Uri.parse(uriString), "r");
				if (filePfd != null) {
					return filePfd.detachFd();  // Take ownership of the fd.
				}
			} catch (Exception ignored) {}
		}
		return -1;
	}

	// Fallback directory creation for native FileSystem::CreateDirectoryPath.
	// On Android 11+ FUSE-emulated external storage a raw libc mkdir() can be
	// denied (EACCES/EPERM) for MANAGE_EXTERNAL_STORAGE apps even though the
	// Java File API succeeds — which is why FOLDER memory cards failed to
	// format ("Format failed!") on a custom data folder while file cards
	// worked. Returns true if the directory exists after the call.
	public static boolean createDirectoryPath(String path) {
		if (path == null || path.isEmpty()) return false;
		try {
			java.io.File dir = new java.io.File(path);
			if (dir.isDirectory()) return true;
			dir.mkdirs();
			return dir.isDirectory();
		} catch (Throwable t) {
			return false;
		}
	}
}
