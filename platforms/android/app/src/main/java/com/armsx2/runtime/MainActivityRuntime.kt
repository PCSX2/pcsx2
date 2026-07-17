package com.armsx2.runtime

import android.app.ActivityManager
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.res.Configuration
import android.content.pm.ActivityInfo
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Process
import android.os.SystemClock
import android.view.InputDevice
import android.view.KeyCharacterMap
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.activity.ComponentActivity
import androidx.activity.SystemBarStyle
import androidx.activity.addCallback
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.ActivityResult
import androidx.activity.result.contract.ActivityResultContracts.StartActivityForResult
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.focusable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.nativeKeyCode
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.core.view.ViewCompat
import androidx.lifecycle.lifecycleScope
import com.armsx2.BuildConfig
import com.armsx2.EmuState
import com.armsx2.FilenameParser
import com.armsx2.GameInfo
import com.armsx2.PlayTime
import com.armsx2.events.TestResult
import com.armsx2.input.ControllerMappings
import com.armsx2.runtime.MainActivityRuntime.Companion.internalBiosDir
import com.armsx2.runtime.MainActivityRuntime.Companion.romsDirs
import com.armsx2.ui.Colors
import com.armsx2.ui.InGameOverlay
import com.armsx2.ui.WindowImpl
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.Android
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.asCoroutineDispatcher
import kotlinx.coroutines.launch
import kr.co.iefriends.pcsx2.MainActivity
import kr.co.iefriends.pcsx2.NativeApp
import org.libsdl.app.HIDDeviceManager
import org.libsdl.app.SDLControllerManager
import java.io.File
import java.io.IOException
import java.util.concurrent.Executors
import kotlin.math.abs
import kotlin.math.min
import androidx.core.net.toUri
import androidx.core.content.edit

private const val LIGHT_NAVIGATION_BAR_SCRIM = 0x04000000
private const val DARK_NAVIGATION_BAR_SCRIM = 0x0A000000

private const val STICK_DEAD = 0.15f
// Trigger (L2/R2) dead-low: much smaller than the stick deadzone — triggers want fine
// control and full range. Just enough to swallow resting-axis noise on cheaper / non-Xbox
// pads; the value is re-normalized past it (see sendTrigger) so pressure ramps smoothly
// from 0 instead of flickering on/off at a hard threshold — the jitter those pads showed.
private const val TRIGGER_DEAD = 0.06f
// Threshold past which a stick remapped to D-pad / face buttons registers as a
// digital press. Higher than STICK_DEAD so a resting/wobbling stick doesn't fire.
private const val STICK_DIGITAL_THRESHOLD = 0.5f
// Off-axis bleed gate for the RADIAL analog path (accumStickRadial): the minor axis is
// dropped when it's below this fraction of the major axis, so a near-cardinal push on a
// stick that isn't perfectly centered on the other axis doesn't leak a phantom second
// direction ("up also presses right"). 0.15 ≈ snaps only ~<9° diagonals to the cardinal;
// genuine diagonals (minor axis well above this) pass through untouched.
private const val STICK_CROSS_GATE = 0.15f
private const val UI_NAV_DEAD = 0.20f
private const val UI_NAV_RELEASE_DEAD = 0.06f
private const val UI_HAT_DEAD = 0.50f
private const val UI_NAV_DOMINANCE = 1.35f
private const val UI_OVERLAY_RELEASE_MS = 80L
private const val UI_KEY_AXIS_SUPPRESS_MS = 220L
// Hold-to-repeat cadence for controller menu navigation: first auto-repeat
// after the initial hold, then steady repeats while the stick/dpad is held.
private const val NAV_REPEAT_INITIAL_MS = 340L
private const val NAV_REPEAT_INTERVAL_MS = 110L

// During a hotkey capture, a 2nd keycode arriving within this window of the
// first DOWN is treated as part of the SAME physical press (some controllers
// emit two codes per button) rather than a deliberate modifier+key combo.
// A real combo is a held first button + a later second press, well past this.
private const val COMBO_MIN_GAP_MS = 40L

val codeGenTests = mutableStateOf("")
val patchTests = mutableStateOf("")
val vuJitTests = mutableStateOf("")
val eeJitTests = mutableStateOf("")
val vifTests = mutableStateOf("")
val eeSeqTests = mutableStateOf("")

open class MainActivityRuntime : ComponentActivity() {
    private var lastUiNavCode = 0
    private var lastUiNavAt = 0L
    private var lastUiNavWasAxis = false
    private var overlayAxisX = 0
    private var overlayAxisY = 0
    private var overlayHorizontalReleaseAt = 0L
    private var libraryAxisX = 0
    private var libraryAxisY = 0

    companion object {
        var instance: MainActivityRuntime? = null
        lateinit var prefs: SharedPreferences
        val setupComplete = mutableStateOf(false)
        // Set at launch when a restored-but-unusable setup is detected (Auto Backup
        // brought back prefs incl. setupComplete, but the ROMs folder permission
        // didn't survive the reinstall). Drives a one-time explanatory toast; the
        // wizard is re-shown so the user can re-grant folder access.
        val setupRecoveryNeeded = mutableStateOf(false)
        val setupEditorVisible = mutableStateOf(false)
        val nativeReady = mutableStateOf(false)
        // Tree URI of the user-picked PCSX2 system folder (where bios/,
        // memcards/, etc. should live). Persisted as `systemDir` pref.
        // When unset, emucore falls back to getExternalFilesDir(null)
        // (Android/data/<package>/files).
        val systemDir = mutableStateOf<String?>(null)
        val bios = mutableStateOf<String?>(null)
        // Tree URI of the folder the user picked their BIOS from. Persisted
        // separately from `bios` (the path of the copied private file) so
        // re-entering setup can re-scan the original folder without
        // forcing the user to re-pick.
        val biosDir = mutableStateOf<String?>(null)

        /** Persisted list of ROM-folder tree URIs. Replaces the legacy
         *  single-folder `romsDir` pref (kept readable as a one-element
         *  list at load time). The setup wizard's ROMs page lets the user
         *  add/remove entries; the library scans every entry and merges
         *  results de-duplicated by URI. Empty list = no library. */
        val romsDirs = mutableStateOf<List<String>>(emptyList())

        /** Update [romsDirs] state and persist as JSON. Drops the legacy
         *  single-string pref so we don't keep two views in sync forever. */
        fun setRomsDirs(dirs: List<String>) {
            romsDirs.value = dirs
            val arr = org.json.JSONArray()
            for (d in dirs) arr.put(d)
            prefs.edit {
                putString("romsDirs", arr.toString())
                    .remove("roms")
            }
        }

        // Default backend is "auto" — emucore's GSUtil::GetPreferredRenderer
        // picks at runtime per device. The setup wizard no longer asks; the
        // in-game overlay's Renderer tab is where users override (OpenGL /
        // Software cycle, plus Mali/Adreno-specific paths once those land).
        // `upscale` (1.0..5.0) still persists; it's exposed in the in-game
        // overlay's Renderer tab.
        val renderer = mutableStateOf("auto")
        val upscale = androidx.compose.runtime.mutableFloatStateOf(1.0f)

        /** Active custom Vulkan driver id (matches `CustomDriver.InstalledDriver.id`).
         *  Null = system Vulkan loader. Set from the setup wizard's driver
         *  chip. Applied to native via CustomDriver.applyToNative inside
         *  applyRendererPrefs BEFORE runVMThread enters MTGS::Open, which
         *  is when Vulkan::LoadVulkanLibrary reads the pinned path. */
        val customDriverId = mutableStateOf<String?>(null)

        private val eDispatcher = Executors.newSingleThreadExecutor().asCoroutineDispatcher()
        private val eScope = CoroutineScope(eDispatcher)

        /**
         * Resolve the user-chosen system folder (a SAF tree URI persisted
         * as `systemDir`) to a POSIX path emucore can use as
         * `EmuFolders::DataRoot`. Memcards / savestates / configs land
         * under it.
         *
         * Tree URIs from OpenDocumentTree look like
         *   content://com.android.externalstorage.documents/tree/primary%3APCSX2
         * The "primary:" prefix means the volume is the primary external
         * storage (`/storage/emulated/0`); other prefixes are SD-card or
         * removable volume IDs which mount under `/storage/<volumeId>`.
         *
         * Returns null when systemDir is unset, malformed, or this
         * Android build can't translate the tree URI (rare). Caller
         * falls back to the app's externalFilesDir in that case.
         *
         * Caveat: emucore's POSIX FileSystem APIs require the resolved path to
         * be writable without broad shared-storage privileges. On modern
         * Android, that generally means app-private storage.
         */
        fun systemDirPosix(): String? {
            val v = systemDir.value ?: return null
            // Volume-choice model stores an absolute app-specific path directly
            // (e.g. the SD card's Android/data/<pkg>/files). Legacy installs may
            // still hold a SAF tree-URI string; resolve those the old way.
            return if (v.startsWith("content://")) resolveTreeUriToPosix(v) else v
        }

        /** `<DataRoot>/inputprofiles/` — the portable folder both touch-layout and
         *  controller-mapping profiles mirror themselves into, so they survive a
         *  data-folder move and can be shared or hand-dropped. Null when no system
         *  dir is configured yet; created on demand.
         *
         *  Lives here rather than in either profile store because BOTH need it and
         *  the fallback below is the subtle part: systemDirPosix() is null for the
         *  DEFAULT (private app folder), so it falls back to getExternalFilesDir,
         *  which is exactly where the native core puts EmuFolders::InputProfiles.
         *  Two copies of that reasoning would be one copy too many. */
        fun inputProfilesDir(): File? {
            val root = systemDirPosix()
                ?: instance?.applicationContext?.getExternalFilesDir(null)?.absolutePath
                ?: return null
            val dir = File(root, "inputprofiles")
            if (!dir.exists()) runCatching { dir.mkdirs() }
            return if (dir.isDirectory) dir else null
        }

        /** App-specific data dir on a removable/secondary volume (SD card),
         *  e.g. /storage/<volId>/Android/data/<pkg>/files. Always raw-writable
         *  by the native core with NO permission under scoped storage, which is
         *  why it works on the Play build where arbitrary folders cannot.
         *  getExternalFilesDirs()[0] is primary/internal; [1..] are removable
         *  volumes (entries may be null while a card is unmounting). Returns the
         *  first usable secondary path, or null when no SD card is present. */
        fun sdCardDataDir(context: Context): String? {
            val dirs = context.getExternalFilesDirs(null)
            for (i in 1 until dirs.size) {
                val d = dirs[i] ?: continue
                return d.absolutePath
            }
            return null
        }

        /** Directory holding the configured BIOS file, used by
         *  NativeApp.initializeOnce to point EmuFolders::Bios at the real
         *  BIOS location. Null when no BIOS is configured yet —
         *  initializeOnce then falls back to [internalBiosDir]. */
        fun biosFolderPosix(): String? =
            bios.value?.takeIf { it.isNotEmpty() }?.let { File(it).parent }

        /** App-private BIOS folder, ALWAYS readable by the native core regardless
         *  of the chosen data root. The BIOS must live here (NOT under a custom /
         *  SD systemDir): on Android 11+ the native FileSystem APIs can't reliably
         *  open a BIOS that sits on a removable volume or a SAF-picked folder, so a
         *  game booted with the data root on SD failed VM init (BIOS load) and
         *  bounced back to the library. This mirrors the design documented in
         *  native-lib initialize() ("p_szbiosfolder is always externalFilesDir/bios").
         *  Memcards / saves / configs still follow the chosen data root. */
        fun internalBiosDir(context: Context): File =
            File(context.getExternalFilesDir(null) ?: context.dataDir, "bios")

        /** URI-string-independent POSIX resolver. Pulled out of
         *  systemDirPosix so the setup wizard can probe a freshly-picked
         *  URI for writability before persisting it. Returns null if the
         *  URI is malformed or its volume ID isn't translatable. */
        fun resolveTreeUriToPosix(uriString: String?): String? {
            val raw = uriString ?: return null
            val uri = try {
                raw.toUri() } catch (_: Exception) { return null }
            val docId = try {
                android.provider.DocumentsContract.getTreeDocumentId(uri)
            } catch (_: Exception) { null } ?: return null
            val parts = docId.split(":", limit = 2)
            if (parts.size != 2) return null
            val (volumeId, relPath) = parts
            return when (volumeId) {
                "primary" -> "/storage/emulated/0/$relPath"
                else -> "/storage/$volumeId/$relPath"
            }
        }

        /**
         * Probe the resolved POSIX path for emucore-compatible write
         * access. Creates a `.armsx2-write-probe` file, deletes it,
         * returns true on success.
         *
         * Catches the scoped-storage trap: Android lets the SAF tree-URI
         * permission survive the picker, so reads work, but raw `fopen`/`mkdir`
         * from emucore can still fail with EACCES during memcard / savestate /
         * config generation. We probe up-front so the wizard can refuse to
         * advance and keep writable emulator data in app-private storage.
         */
        fun validateSystemDirWritable(posixPath: String): Boolean {
            return try {
                val dir = File(posixPath)
                if (!dir.exists() && !dir.mkdirs()) return false
                if (!dir.isDirectory) return false
                val probe = File(dir, ".armsx2-write-probe")
                val ok = probe.createNewFile()
                if (ok) probe.delete()
                ok
            } catch (_: Exception) {
                false
            }
        }

        val surface = mutableStateOf<EmulationSurface?>(null)

        @JvmField
        val eState = mutableStateOf(EmuState.STOPPED)

        // Active quick save/load slot (0-9), cycled by the "Cycle Save Slot"
        // hotkey. Quick Save/Load State hotkeys read this so users aren't pinned
        // to slot 0.
        val currentSaveSlot = androidx.compose.runtime.mutableIntStateOf(0)

        // Latched state for the "Fast Forward (toggle)" hotkey: each press flips
        // between Turbo and Nominal limiter mode (vs. the hold variant which is
        // momentary). Reset to false whenever a game starts.
        @Volatile var fastForwardToggleActive = false

        // Latched state for the "Slow Down (toggle)" hotkey (LimiterModeType::Slomo).
        // Mutually exclusive with the fast-forward latch; blocked in RA hardcore.
        @Volatile var slowDownToggleActive = false

        // Runtime gyro enable (issue #337), driven by the GYRO_TOGGLE / GYRO_HOLD hotkeys.
        // Session-only by design — a mid-game silence, not a persisted preference, so it
        // never contradicts the Gyro Mode setting the user chose. Compose state:
        // TouchControlsOverlay's DisposableEffect keys on it and starts/stops the sensor.
        // Stopping emits (0,0), which releases the gyro's contribution to the merged
        // stick, so the physical stick is left driving on its own.
        val gyroActive = mutableStateOf(true)

        // #254: whether the emulated USB keyboard is attached for the running
        // game (resolved Settings.usbKeyboard, cached at launch in
        // applyRendererPrefs). Read hot in dispatchKeyEvent to decide whether a
        // physical keyboard's key events should be forwarded to the USB device
        // instead of driving the pad / frontend. Cheap flag so the per-event
        // path doesn't touch ConfigStore.
        @Volatile var usbKeyboardActive = false

        // Cached metadata for the currently-running game. Populated when
        // The library opens a card (so we have title, serial, compatibility,
        // extension and the cover URL ready), cleared when the user
        // launches via paths that don't have a GameInfo handy (Swap/Boot Disc
        // file picker, BIOS-only boot). InGameOverlay reads this for its
        // top-left game info block — falls back to NativeApp.getPause* +
        // a runtime compat lookup when it's null.
        val currentGame = mutableStateOf<GameInfo?>(null)

        val focusRequester = FocusRequester()

        private var m_szGamefile = ""
        private val pendingExternalLaunch = mutableStateOf<String?>(null)
        // A library game tapped before native init finished — deferred and fired once
        // nativeReady. Fixes the first-cold-launch / DeX crash: applyRendererPrefs
        // pushed GS settings before the base settings layer existed → native SIGSEGV.
        private val pendingLaunch = mutableStateOf<Pair<String, GameInfo?>?>(null)

        fun onTestResults(result: TestResult) {
            when (result.name) {
                "VuJitTests" -> vuJitTests.value = "${result.passed}/${result.total}"
                "PatchTests" -> patchTests.value = "${result.passed}/${result.total}"
                "CodegenTests" -> codeGenTests.value = "${result.passed}/${result.total}"
                "EeJitTests" -> eeJitTests.value = "${result.passed}/${result.total}"
                "VifTests" -> vifTests.value = "${result.passed}/${result.total}"
                "EeSeqTests" -> eeSeqTests.value = "${result.passed}/${result.total}"
                else -> println("Test:${result.name}: ${result.passed}/${result.total}")
            }
        }

        fun invoke(task: suspend () -> Unit) {
            eScope.launch {
                task()
            }
        }

        private val vmLifecycleLock = Any()
        @Volatile private var vmStopInProgress = false
        @Volatile private var vmRestartAfterStop = false
        @Volatile private var vmRunLoopActive = false

        // Quit-after-the-VM-stops latch — set by the "Close Game & Quit" hotkey, or by
        // a frontend-launched game's Close Game. One-shot: read+cleared by
        // finishToLauncherIfRequested in whichever terminal STOPPED branch fires first.
        @Volatile var quitAfterStop = false
        // True while the CURRENT game was launched from an external frontend intent.
        @Volatile var launchedExternally = false

        /** Terminal (non-restart) STOPPED branches call this: if a quit was requested,
         *  finish the Activity back to the launcher/frontend AFTER the VM has fully
         *  unwound and flushed (memcards/savestate). Marshalled to the UI thread. */
        private fun finishToLauncherIfRequested() {
            if (quitAfterStop) {
                quitAfterStop = false
                instance?.runOnUiThread { instance?.finishAndRemoveTask() }
            }
        }

        /** Close the running game the way the user asked for. When the game came from an
         *  external frontend (ES-DE / Cocoon / Daijishō) and the opt-in is on, finish the
         *  app so the frontend regains focus instead of dropping the user into the ARMSX2
         *  library.
         *
         *  EVERY close route must come through here. The hotkeys used to inline this check
         *  while the in-game menu's Close action called stop() directly, so the menu
         *  silently ignored "Exit to launcher on close" and users had to bind a hotkey to
         *  work around it. One chokepoint means the two can't drift apart again. */
        @JvmStatic
        fun closeGame(saveAutosave: Boolean = false) {
            if (launchedExternally && prefs.getBoolean("ui.exitToLauncherExternal", true))
                quitAfterStop = true
            stop(saveAutosave = saveAutosave)
        }

        /** Fully exit the app (the library Exit button and hold-back gesture route
         *  here). VM-safe: if a game is running, flush it first (quitAfterStop +
         *  async stop(), which finishes once the VM unwinds via the STOPPED branch);
         *  if already stopped, finish immediately. Never finish inline on a running
         *  VM — stop() is async and inline finish would skip the memcard/savestate
         *  flush (the same reason QUIT_APP uses the latch). */
        @JvmStatic
        fun exitApp() {
            if (eState.value == EmuState.STOPPED && !vmStopInProgress && !vmRunLoopActive) {
                instance?.runOnUiThread { instance?.finishAndRemoveTask() }
            } else {
                quitAfterStop = true
                stop()
            }
        }

        @JvmStatic
        fun isVmStopInProgress(): Boolean = vmStopInProgress

        fun start() {
            synchronized(vmLifecycleLock) {
                if (vmStopInProgress || vmRunLoopActive || eState.value != EmuState.STOPPED) {
                    vmRestartAfterStop = true
                    return
                }
                vmRunLoopActive = true
            }

            invoke {
                try {
                    eState.value = EmuState.RUNNING
                    println("@@ANDROID_START_VM@@ kind=game path=${m_szGamefile.take(240)}")
                    // Local co-op: re-pair controllers each session (first pad = P1,
                    // next = P2) so player slots are deterministic per boot.
                    com.armsx2.input.PadRouter.reset()
                    WindowImpl.showLibrary.value = false
                    WindowImpl.overlayVisible.value = false
                    WindowImpl.toolbarVisible.value = false
                    applyRendererPrefs()
                    NativeApp.runVMThread(m_szGamefile)
                } finally {
                    // runVMThread blocks until the VM exits (Stopping/Shutdown
                    // observed). Drop back to STOPPED only after native has
                    // actually unwound, so users can't launch the next game
                    // while the previous VM is still tearing down.
                    eState.value = EmuState.STOPPED
                    val restartNow = synchronized(vmLifecycleLock) {
                        vmRunLoopActive = false
                        vmStopInProgress = false
                        if (vmRestartAfterStop) {
                            vmRestartAfterStop = false
                            true
                        } else {
                            false
                        }
                    }
                    if (restartNow) {
                        start()
                    } else {
                        WindowImpl.toolbarVisible.value = true
                        WindowImpl.showLibrary.value = false
                        WindowImpl.overlayVisible.value = false
                        finishToLauncherIfRequested()
                    }
                }
            }
        }

        /** Push setup-wizard renderer/upscale choices into emucore's
         *  EmuConfig before runVMThread. ApplySettings inside Initialize
         *  picks them up; if a VM is already up, the JNI helpers also
         *  call MTGS::ApplySettings inline.
         *
         *  Also resolves and applies the per-game / global Settings from
         *  ConfigStore (MTVU and friends) — currentGame.serial picks the
         *  right override tier; null falls back to global. Resolution
         *  order: per-game JSON overlay → global → hardcoded defaults. */
        /** Number of distinct physical gamepads/joysticks connected right now
         *  (excludes virtual devices). Drives the boot-time PS2-port-2 enable for
         *  local co-op — 2+ pads → connect Player 2's controller at VM init. */
        private fun connectedGamepadCount(): Int {
            var n = 0
            for (id in InputDevice.getDeviceIds()) {
                val dev = InputDevice.getDevice(id) ?: continue
                if (dev.isVirtual) continue
                val s = dev.sources
                if ((s and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
                    (s and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) n++
            }
            return n
        }

        private fun applyRendererPrefs() {
            // Resolve per-game (∘ global) settings up front so the renderer backend
            // and internal resolution come from THIS title's tier, not a stale
            // global value. Sync the session state the Renderer UI reads, too.
            // Resolve via settingsKey (serial for discs, filename stem for
            // serial-less ELF/homebrew) so ELF per-game settings survive a reboot
            // instead of falling back to global (issue #253).
            var resolved = com.armsx2.config.ConfigStore.resolveForGame(currentGame.value?.settingsKey)
            // Per-game memory cards (NetherSX2-style): when the global toggle is
            // on, point Slot 1 at a serial-named card (the core auto-creates +
            // formats it at boot) — but ONLY for games still on the factory-default
            // card. A user who assigned a real card (imported/created, globally OR
            // per-game) has a non-default filename, so we must NOT clobber it with a
            // blank serial card. (Bug: the old guard compared against the current
            // global, which a global assignment always equals, so it wiped it.)
            currentGame.value?.serial?.takeIf { it.isNotBlank() }?.let { serial ->
                if (prefs.getBoolean("memcard.perGame", false) &&
                    resolved.memoryCardSlot1Filename.equals("mcd001.ps2", ignoreCase = true)) {
                    resolved = resolved.copy(
                        memoryCardSlot1Filename = "$serial.ps2",
                        memoryCardSlot1Enabled = true,
                    )
                }
            }
            // Per-game BIOS: boot with the game's chosen BIOS if it set one, else fall
            // back to the global BIOS — so a previous game's per-game pick never sticks.
            // The file is in the same app-private BIOS dir as the global one, so only the
            // Filenames/BIOS *filename* changes; commit before the VM's LoadBIOS runs.
            run {
                val effectiveBios = resolved.biosFilename.takeIf { it.isNotBlank() }
                    ?: bios.value?.takeIf { it.isNotEmpty() }?.let { File(it).name }
                if (!effectiveBios.isNullOrBlank()) {
                    NativeApp.setSetting("Filenames", "BIOS", "string", effectiveBios)
                    NativeApp.commitSettings()
                }
            }
            upscale.value = resolved.upscaleFloat
            renderer.value = resolved.renderer
            NativeApp.renderUpscalemultiplier(upscale.value)
            // Pin custom Vulkan driver (if any) BEFORE the renderer write —
            // the renderer JNI may trigger MTGS::ApplySettings which can
            // re-open the GS device and run Vulkan::LoadVulkanLibrary. The
            // VK loader reads the pinned path lazily so the order matters.
            val ctx = instance?.applicationContext
            val picked: com.armsx2.CustomDriver.InstalledDriver? =
                if (ctx != null) customDriverId.value?.let { id ->
                    com.armsx2.CustomDriver.listInstalled(ctx).firstOrNull { it.id == id }
                } else null
            if (ctx != null) com.armsx2.CustomDriver.applyToNative(ctx, picked)
            when (renderer.value) {
                "vulkan" -> NativeApp.renderVulkan()
                "opengl" -> NativeApp.renderOpenGL()
                "software" -> NativeApp.renderSoftware()
                else -> NativeApp.renderAuto()
            }
            resolved.applyTo()
            // #254: cache whether this title runs with the emulated USB keyboard so
            // dispatchKeyEvent can forward physical-keyboard keys to it. applyTo()
            // already pushed [USB1] Type + the live attach (usbSetKeyboardEnabled).
            usbKeyboardActive = resolved.usbKeyboard

            // Neutralize the NATIVE pad analog deadzone before the VM loads [Pad1].
            // A stale [Pad1]/Deadzone in an existing config (from the old, non-saving
            // deadzone slider) imposed a huge ~0.45 fake deadzone on BOTH physical and
            // on-screen sticks (they share PadDualshock2::Set). The app-side "Stick
            // Deadzone" (ControllerMappings.stickDeadzone, re-normalized in
            // shapeStickMag) is the single authority now, so keep the native radial
            // deadzone off so it can't re-deaden the already-shaped input. AxisScale
            // (1.33, helps small sticks reach full deflection) is left untouched.
            runCatching {
                NativeApp.setSetting("Pad1", "Deadzone", "float", "0")
                NativeApp.setSetting("Pad2", "Deadzone", "float", "0")
                // Local co-op: enable PS2 port 2 at BOOT (here, before runVMThread →
                // Pad::LoadConfig) when a second controller is already connected —
                // NOT by hot-plugging it mid-game, which rebuilt the live pad list and
                // crashed. Single controller → "None" (port 2 off; zero change for
                // solo play). So: connect BOTH controllers before launching the game.
                val twoPads = connectedGamepadCount() >= 2
                NativeApp.setSetting("Pad2", "Type", "string", if (twoPads) "DualShock2" else "None")
                if (twoPads) {
                    NativeApp.setSetting("Pad2", "AxisScale", "float", "1.33")
                    NativeApp.setSetting("Pad2", "ButtonDeadzone", "float", "0")
                }
                // PS2 Multitap: when enabled, arm BOTH ports as 4-slot multitaps at BOOT
                // (before runVMThread -> Pad::LoadConfig) so a game launched with 3-8
                // controllers sees them. Flag keys are off-by-one: [Pad] MultitapPort1 ->
                // physical port 0, MultitapPort2 -> port 1. Unified slots: [Pad2] = port-1
                // main, [Pad3..Pad5] = port-0 taps, [Pad6..Pad8] = port-1 taps. Force all
                // 8 slots on for a complete pair of taps; idle slots are harmless (games
                // ignore unused pads). Unconditional-when-ON so a pad joining after boot
                // still lands on a live slot; the Pad-tab toggle covers mid-session enable.
                if (ControllerMappings.multitapEnabled()) {
                    NativeApp.setSetting("Pad", "MultitapPort1", "bool", "true")
                    NativeApp.setSetting("Pad", "MultitapPort2", "bool", "true")
                    for (s in 2..8) {
                        NativeApp.setSetting("Pad$s", "Type", "string", "DualShock2")
                        NativeApp.setSetting("Pad$s", "Deadzone", "float", "0")
                        NativeApp.setSetting("Pad$s", "AxisScale", "float", "1.33")
                        NativeApp.setSetting("Pad$s", "ButtonDeadzone", "float", "0")
                    }
                }
            }

            // Settings.applyTo() above writes the persisted FrameLimitEnable
            // into the BASE settings layer; override it here with the
            // in-session overlay toggle so the user's runtime choice sticks
            // across game launches within one app run. Both writes are
            // needed: native-lib's runVMThread re-reads FrameLimitEnable
            // from the BASE layer right after VMManager::Initialize and
            // calls SetLimiterMode based on that, so a bare
            // speedhackLimitermode() here gets clobbered by VM init.
            // Mode 0 = Nominal (60fps cap), 3 = Unlimited.
            val limit = InGameOverlay.frameLimitOn.value
            NativeApp.setSetting("EmuCore/GS", "FrameLimitEnable", "bool", limit.toString())
            // Preserve an active fast-forward / slow-down latch across a settings apply.
            // Otherwise this re-application of the limiter clobbers mode 1/2 back to the base
            // limit while the toggle state stays ON — so fast-forward is "forgotten" and the
            // user has to toggle off then on again to resync. Re-assert the latched mode.
            NativeApp.speedhackLimitermode(
                when {
                    fastForwardToggleActive -> 1
                    slowDownToggleActive -> 2
                    else -> if (limit) 0 else 3
                }
            )
        }

        /**
         * Set the active game path/URI and restart the VM. Used by
         * Library card taps — the URI comes from the user's persisted
         * ROMs tree (already has read perm) so emucore's FileSystem
         * routines can open it via the content:// JNI bridge.
         *
         * `info` is the GameInfo from the library scan when available;
         * stored on MainActivityRuntime.currentGame so the in-game overlay can show
         * cover art / extension badge / pre-resolved compat stars
         * without re-querying gamedb. Pass null when launching from a
         * path that doesn't have a GameInfo (Swap/Boot Disc file picker).
         */
        fun launchGame(uri: String, info: GameInfo? = null, external: Boolean = false) {
            if (uri.isBlank()) {
                println("@@ANDROID_LAUNCH_REJECT@@ reason=blank_uri title=${info?.title ?: ""}")
                return
            }
            println(
                "@@ANDROID_LAUNCH_GAME@@ title=${info?.title ?: "<direct>"} " +
                    "uri=${uri.take(240)} state=${eState.value} runLoop=$vmRunLoopActive " +
                    "stopping=$vmStopInProgress nativeReady=${nativeReady.value}"
            )
            // Refresh the ANGLE EGL env before the GS thread opens the GL context, so a
            // just-changed AndroidUseAngleOpenGL / renderer choice takes effect on this boot.
            instance?.applicationContext?.let { applyAngleEnv(it) }
            // Native GS/settings calls in start()→applyRendererPrefs null-deref if the
            // base settings layer isn't installed yet (initialize() not finished). On a
            // cold first launch — reliably on Samsung DeX — a fast game tap races init
            // and crashes with no error. Defer until nativeReady; the LaunchedEffect
            // watching pendingLaunch fires it once init completes.
            if (!nativeReady.value) {
                println("@@ANDROID_LAUNCH_DEFER@@ nativeReady=false — queuing '${info?.title ?: uri.take(80)}'")
                pendingLaunch.value = uri to info
                return
            }
            currentGame.value = info
            launchedExternally = external
            // Arm a one-shot auto-load of the autosave state for this boot (fired by
            // onVmRunning once the game's CRC is set). Set here — not in start() — so
            // a manual Reset Game (which re-enters start() directly) doesn't re-load.
            pendingAutoLoadOnBoot = prefs.getBoolean("autoLoadOnBoot", false)
            m_szGamefile = uri
            synchronized(vmLifecycleLock) {
                if (eState.value != EmuState.STOPPED || vmStopInProgress || vmRunLoopActive) {
                    vmRestartAfterStop = true
                }
            }
            if (eState.value == EmuState.STOPPED && !vmStopInProgress && !vmRunLoopActive)
                start()
            else
                stop(restartAfterStop = true)
        }

        private fun launchPendingExternalGameIfReady() {
            if (!setupComplete.value || !nativeReady.value) return
            // A deferred library launch (nativeReady-gated) fires first, keeping its
            // GameInfo so per-game settings / title still apply.
            pendingLaunch.value?.let { (u, i) ->
                pendingLaunch.value = null
                launchGame(u, i)
                return
            }
            val queued = pendingExternalLaunch.value
            if (queued.isNullOrEmpty()) return
            pendingExternalLaunch.value = null
            launchGame(queued, null, external = true)
        }

        /**
         * Boot to BIOS (no game disc). Unlike `start()` this does NOT
         * hide the toolbar — the BIOS action in the library wants the
         * library/toolbar to remain visible so the user can pick a game
         * once BIOS finishes booting.
         */
        fun startBios() {
            currentGame.value = null
            m_szGamefile = ""
            val shouldStart = synchronized(vmLifecycleLock) {
                if (vmStopInProgress || vmRunLoopActive || eState.value != EmuState.STOPPED) {
                    vmRestartAfterStop = true
                    false
                } else {
                    vmRunLoopActive = true
                    true
                }
            }
            if (!shouldStart) {
                stop(restartAfterStop = true)
                return
            }
            invoke {
                try {
                    eState.value = EmuState.RUNNING
                    println("@@ANDROID_START_VM@@ kind=bios path=<empty>")
                    com.armsx2.input.PadRouter.reset()
                    applyRendererPrefs()
                    NativeApp.runVMThread(m_szGamefile)
                } finally {
                    eState.value = EmuState.STOPPED
                    val restartNow = synchronized(vmLifecycleLock) {
                        vmRunLoopActive = false
                        vmStopInProgress = false
                        if (vmRestartAfterStop) {
                            vmRestartAfterStop = false
                            true
                        } else {
                            false
                        }
                    }
                    if (restartNow) {
                        start()
                    }
                }
            }
        }

        // pause/resume run on a dedicated serialized executor, NOT the UI
        // thread. The native side queues the real pause/resume onto the CPU
        // thread, so a fast open→close still lands in the right order without
        // making the Android UI wait for MTVU/MTGS to park.
        // eState is updated by Host::OnVMPaused/Resumed → vmSetPaused, so the
        // UI never claims PAUSED before the VM actually parked.
        private val vmControl = Executors.newSingleThreadExecutor { r ->
            Thread(r, "VMControl")
        }
        private val vmStopControl = Executors.newSingleThreadExecutor { r ->
            Thread(r, "VMStop")
        }

        fun pause() {
            if (vmStopInProgress)
                return
            vmControl.execute {
                if (!vmStopInProgress)
                    NativeApp.pause()
            }
        }

        fun pauseForOverlay() {
            if (vmStopInProgress)
                return
            NativeApp.pause()
        }

        fun resume() {
            if (vmStopInProgress)
                return
            vmControl.execute {
                if (!vmStopInProgress)
                    NativeApp.resume()
            }
        }

        fun stop(saveAutosave: Boolean = false, restartAfterStop: Boolean = false) {
            // Drop any latched fast-forward / slow-down toggle; the next game boots
            // at normal speed. Same for the gyro hotkey latch — a game left with gyro
            // toggled off must not silently start the next one with gyro dead.
            fastForwardToggleActive = false
            slowDownToggleActive = false
            gyroActive.value = true
            val nativeActive = runCatching { NativeApp.hasActiveVM() }.getOrDefault(false)
            val shouldStop = synchronized(vmLifecycleLock) {
                if (restartAfterStop)
                    vmRestartAfterStop = true
                else
                    vmRestartAfterStop = false

                if (vmStopInProgress) {
                    nativeActive
                } else if (eState.value == EmuState.STOPPED && !vmRunLoopActive && !nativeActive) {
                    false
                } else {
                    vmStopInProgress = true
                    true
                }
            }
            if (!shouldStop)
                return

            WindowImpl.overlayVisible.value = false
            WindowImpl.showLibrary.value = false
            // Auto-save-on-exit: any normal close (not a reset/restart) writes the
            // autosave state when the user has the toggle on, so the next boot can
            // auto-load it. An explicit Save-and-Exit still forces it via saveAutosave.
            val doAutosave = saveAutosave ||
                (!restartAfterStop &&
                    runCatching { prefs.getBoolean("autoSaveOnExit", false) }.getOrDefault(false))
            vmStopControl.execute {
                println("@@ANDROID_STOP_JAVA@@ begin saveAutosave=$doAutosave forced=$saveAutosave restart=$restartAfterStop")
                if (doAutosave)
                    NativeApp.saveAutosaveState()
                NativeApp.shutdown()
                println("@@ANDROID_STOP_JAVA@@ shutdown_return active=${NativeApp.hasActiveVM()} runLoop=$vmRunLoopActive state=${eState.value}")
                if (!vmRunLoopActive && (eState.value == EmuState.STOPPED || !NativeApp.hasActiveVM())) {
                    eState.value = EmuState.STOPPED
                    val restartNow = synchronized(vmLifecycleLock) {
                        vmStopInProgress = false
                        if (vmRestartAfterStop) {
                            vmRestartAfterStop = false
                            true
                        } else {
                            false
                        }
                    }
                    if (restartNow) {
                        start()
                    } else {
                        synchronized(vmLifecycleLock) {
                            WindowImpl.toolbarVisible.value = true
                            WindowImpl.showLibrary.value = false
                            WindowImpl.overlayVisible.value = false
                        }
                        // No game is running any more — clear the current-game pointer so the
                        // Settings screen reverts to Global scope. Otherwise the last-played game
                        // lingered here and SettingsScreen's scopeContext (game ?: currentGame)
                        // kept surfacing per-game scope for it after returning to the library.
                        currentGame.value = null
                        finishToLauncherIfRequested()
                    }
                }
            }
        }

        fun restart() {
            synchronized(vmLifecycleLock) {
                vmRestartAfterStop = true
            }
            if (eState.value == EmuState.STOPPED && !vmStopInProgress && !vmRunLoopActive)
                start()
            else
                stop(restartAfterStop = true)
        }

        /** Open a file picker to swap the mounted disc WITHOUT rebooting the VM.
         *  The picked URI is handed to NativeApp.changeDisc (see swapDiscAction),
         *  which parks the CPU thread and cycles the CDVD tray so the running game
         *  detects the new disc — needed for multi-disc titles and cheat discs
         *  (CodeBreaker/GameShark) that hand off to a game disc. Bridges Compose
         *  (the in-game menu) to the Activity-scoped ActivityResult launcher; the
         *  picker + native swap were intact but had no trigger after the monorepo
         *  UI migration, so Swap Disc silently did nothing. */
        fun promptSwapDisc() {
            val activity = instance ?: return
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            runCatching { activity.swapDiscAction.launch(intent) }
        }

        /** ANGLE (GLES-on-Vulkan) for the OpenGL renderer. Ported from sashkinbro/EmuCoreX:
         *  when the AndroidUseAngleOpenGL setting is on AND the renderer is OpenGL, point the
         *  ARMSX2_ANGLE_EGL_LIBRARY / _GLES_LIBRARY env vars at the bundled ANGLE .so in the
         *  native-lib dir; GLContextEGL::LoadEGL (native) then loads ANGLE's EGL instead of the
         *  system GLES driver — useful where the native GLES stack is broken (e.g. some MediaTek
         *  Mali). Cleared otherwise. Env vars are read by native getenv in this same process, so
         *  this Kotlin call is the whole hook. MUST run before the GS thread opens the GL context,
         *  so it's invoked at emucore init and before each game launch. Renderer restart applies a
         *  live toggle (like the GPU-profile override). Uses the GLOBAL settings; per-game renderer
         *  overrides are out of scope for v1. */
        fun applyAngleEnv(context: Context) {
            val settings = runCatching { com.armsx2.config.ConfigStore.loadGlobal() }.getOrNull()
            val eligible = settings?.useAngleOpenGL == true && settings.renderer == "opengl"
            val libDir = context.applicationInfo.nativeLibraryDir
            val egl = File(libDir, "libEGL_angle.so")
            val gles = File(libDir, "libGLESv2_angle.so")
            try {
                if (eligible && egl.exists() && gles.exists()) {
                    android.system.Os.setenv("ARMSX2_ANGLE_EGL_LIBRARY", egl.absolutePath, true)
                    android.system.Os.setenv("ARMSX2_ANGLE_GLES_LIBRARY", gles.absolutePath, true)
                    android.util.Log.i("ARMSX2", "ANGLE OpenGL enabled: ${egl.absolutePath}")
                } else {
                    runCatching { android.system.Os.unsetenv("ARMSX2_ANGLE_EGL_LIBRARY") }
                    runCatching { android.system.Os.unsetenv("ARMSX2_ANGLE_GLES_LIBRARY") }
                }
            } catch (e: Exception) {
                android.util.Log.e("ARMSX2", "applyAngleEnv failed: ${e.message}")
            }
        }

        // Armed per-launch in launchGame when "Auto-load last state on boot" is on;
        // consumed once by onVmRunning. Set in launchGame (NOT start) so a manual
        // Reset Game — which re-enters start() directly — never re-loads the state.
        @Volatile
        var pendingAutoLoadOnBoot = false

        @Volatile
        private var pendingSlotLoadOnBoot: Int? = null

        fun launchCurrentGameFromSaveSlot(slot: Int): Boolean {
            val game = currentGame.value ?: return false
            val launchPath = if (game.uri.scheme == "file") {
                game.uri.path ?: game.uri.toString()
            } else {
                game.uri.toString()
            }
            if (launchPath.isBlank()) return false
            pendingSlotLoadOnBoot = slot
            pendingAutoLoadOnBoot = false
            launchGame(launchPath, game)
            return true
        }

        /**
         * Give an externally-launched game the same identity a library-launched one has.
         *
         * A front-end (Daijisho / Lisi / Cocoon) hands us a bare content:// with no
         * GameInfo, so [handleExternalLaunchIntent] leaves [currentGame] null. That split
         * the game's identity in two: the settings hub keys its Global/Game switch off
         * currentGame, so the switch VANISHED — while the save path resolves the serial
         * from the running core and happily wrote per-game. Hence the report of settings
         * "showing as Global but saving as Per Game" only when launched from a front-end.
         *
         * The core knows the serial once the disc is read, which is the same key the save
         * path uses — so build the missing GameInfo from it and the two agree again.
         */
        /** Minutes between interval autosaves. 0 = off, which is the default: writing a
         *  savestate costs a visible hitch, so it is never turned on for you. */
        const val KEY_AUTOSAVE_INTERVAL_MIN = "autoSaveIntervalMin"

        /** How often the job below wakes to check. Well under the shortest interval (1
         *  minute), so a freshly-lowered setting takes effect promptly without the job
         *  spinning. */
        private const val AUTOSAVE_POLL_MS = 15_000L

        private var autosaveIntervalJob: kotlinx.coroutines.Job? = null

        /**
         * Interval autosave: while a game is actually RUNNING, write the autosave slot
         * every N minutes so a crash or a flat battery costs at most that much progress.
         *
         * Writes the SAME dedicated `.autosave.p2s` that auto-save-on-exit uses, so the
         * numbered slots 0-9 stay entirely the user's and auto-load-on-boot picks this up
         * with no extra plumbing.
         *
         * Started once and self-gating rather than hooked to VM start/stop: the state it
         * cares about (running, not covered by a menu, interval > 0) is all readable here,
         * and a single long-lived job can't be leaked by a boot path that forgets to stop
         * it. It deliberately does NOT fire while paused or while the pause menu / a
         * manager screen is up — the game isn't advancing, so a save then costs a hitch
         * and buys nothing.
         */
        private fun startAutosaveIntervalJob() {
            autosaveIntervalJob?.cancel()
            autosaveIntervalJob = instance?.lifecycleScope?.launch {
                var lastSaveAt = 0L
                while (true) {
                    kotlinx.coroutines.delay(AUTOSAVE_POLL_MS)
                    val minutes = runCatching { prefs.getInt(KEY_AUTOSAVE_INTERVAL_MIN, 0) }.getOrDefault(0)
                    if (minutes <= 0 || eState.value != EmuState.RUNNING || WindowImpl.frontendCovers) {
                        // Reset the clock while it can't fire, so re-entering a game doesn't
                        // immediately dump a save from time that accrued in a menu.
                        lastSaveAt = 0L
                        continue
                    }
                    val now = android.os.SystemClock.elapsedRealtime()
                    if (lastSaveAt == 0L) {
                        lastSaveAt = now
                        continue
                    }
                    if (now - lastSaveAt < minutes * 60_000L) continue
                    runCatching { NativeApp.saveAutosaveState() }
                    // Stamped AFTER the write: a savestate takes real time, and starting
                    // the next interval from before it would make saves creep earlier.
                    lastSaveAt = android.os.SystemClock.elapsedRealtime()
                }
            }
        }

        private fun adoptExternalGameIdentity() {
            if (!launchedExternally || currentGame.value != null) return
            val path = m_szGamefile.takeIf { it.isNotEmpty() } ?: return
            val handler = android.os.Handler(android.os.Looper.getMainLooper())
            handler.post(object : Runnable {
                var attempts = 0
                override fun run() {
                    // A library launch that lands mid-poll wins: it has the real entry.
                    if (vmStopInProgress || eState.value == EmuState.STOPPED) return
                    if (currentGame.value != null) return
                    // "00000000" is the placeholder the core reports before the disc is
                    // read — the same value TouchControls.coreSerial() rejects.
                    val serial = runCatching { NativeApp.getGameSerial() }.getOrNull()
                        ?.trim()?.uppercase()?.takeIf { it.isNotEmpty() && it != "00000000" }
                    if (serial == null) {
                        // ~10s of looking. A serial-less boot (ELF/homebrew) just never
                        // adopts one, and settingsKey's filename fallback still applies.
                        if (++attempts < 40) handler.postDelayed(this, 250)
                        return
                    }
                    val uri = runCatching { Uri.parse(path) }.getOrNull() ?: return
                    val name = uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
                        ?: path.substringAfterLast('/')
                    val (title, _) = FilenameParser.parse(name)
                    currentGame.value = GameInfo(
                        uri = uri,
                        title = title,
                        serial = serial,
                        extension = name.substringAfterLast('.', "").uppercase(),
                    )
                }
            })
        }

        /** Fired when the VM reaches RUNNING (from NativeApp.vmSetPaused). If the
         *  user enabled auto-load-on-boot, restore the autosave state once — but only
         *  after the renderer is actually presenting frames (polls getPresentedFrameCount),
         *  because restoring before the present loop is flowing leaves a black screen.
         *  Polls every 250ms, giving up after ~15s if the game never starts presenting. */
        @JvmStatic
        fun onVmRunning() {
            adoptExternalGameIdentity()
            val requestedSlot = pendingSlotLoadOnBoot
            val loadAutosave = pendingAutoLoadOnBoot && requestedSlot == null
            if (requestedSlot == null && !loadAutosave) return
            pendingSlotLoadOnBoot = null
            pendingAutoLoadOnBoot = false
            val handler = android.os.Handler(android.os.Looper.getMainLooper())
            val tryLoad = object : Runnable {
                var attempts = 0
                var lastFrame = -1
                var advancingPolls = 0
                override fun run() {
                    if (vmStopInProgress || eState.value == EmuState.STOPPED) return
                    // Wait until the renderer is actually PRESENTING frames before restoring the
                    // state. A boot-time load that fires as soon as the disc CRC is known — before
                    // the present loop is flowing — leaves the restored frame undisplayed (a black
                    // screen); loading the same state manually works only because the game is
                    // already rendering by then. The present counter can read stale-high across a
                    // re-launch (the GS may not fully reset between games), so gate on SUSTAINED
                    // advancement rather than an absolute value: require frames to have grown
                    // across a few consecutive polls (~0.75s of continuous presenting). (Native
                    // then forces one present of the restored frame so it shows immediately.)
                    val frame = runCatching { NativeApp.getPresentedFrameCount() }.getOrDefault(0)
                    advancingPolls = if (lastFrame in 0 until frame) advancingPolls + 1 else 0
                    lastFrame = frame
                    if (advancingPolls < 3) {
                        if (++attempts < 60) handler.postDelayed(this, 250)
                        return
                    }
                    val loaded = runCatching {
                        if (requestedSlot != null) NativeApp.loadStateFromSlot(requestedSlot)
                        else NativeApp.loadAutosaveState()
                    }.getOrDefault(false)
                    if (!loaded && ++attempts < 60)
                        handler.postDelayed(this, 250)
                }
            }
            handler.postDelayed(tryLoad, 250)
        }

        fun finishSetup() {
            prefs.edit { putBoolean("setupComplete", true) }
            setupComplete.value = true
            setupEditorVisible.value = false
        }

        fun reopenSetup() {
            setupEditorVisible.value = true
        }

        /** The data root that NativeApp.initialize() was actually handed
         *  (captured in kickoffEmucoreInit). EmuFolders::DataRoot is pinned
         *  ONCE per process, so the setup wizard compares against this to know
         *  whether a storage-location change actually needs a process restart
         *  to take effect (it can't be hot-swapped while the process lives). */
        private var lastInitDataRoot: String? = null
        fun currentInitDataRoot(): String? = lastInitDataRoot

        /** Cold-restart the app so native re-runs initialize() with the newly
         *  chosen data root. Used after the user moves app data between Internal
         *  and SD in the setup wizard. */
        fun restartApp(context: Context) {
            val intent = context.packageManager.getLaunchIntentForPackage(context.packageName)
            if (intent != null) {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                context.startActivity(intent)
            }
            Runtime.getRuntime().exit(0)
        }

        fun renderOpenGL() {
            NativeApp.renderOpenGL()
        }

        fun renderVulkan() {
            NativeApp.renderVulkan()
        }

        fun renderSoftware() {
            NativeApp.renderSoftware()
        }

        /** Resolved root that bundled APK assets (resources/, bios/) are
         *  copied to. This prefers a custom systemDir only when it resolves to
         *  a writable POSIX path; otherwise it falls back to app-private
         *  storage. Game folders are separate and accessed through SAF. */
        /** True if at least one configured ROMs folder is actually reachable right
         *  now. content:// folders need a live persisted SAF read permission;
         *  file:// (all-files build) needs the path readable. Android Auto Backup
         *  restores the folder URIs but NOT their permissions, so after a reinstall
         *  or device-restore the saved folders can be present yet unreadable — this
         *  is how launch detects that and re-runs setup instead of stranding the
         *  user in an empty, perpetually-scanning library. */
        fun romsAccessible(context: Context, romsDirs: List<String>): Boolean {
            if (romsDirs.isEmpty()) return false
            // content://: still hold the EXACT persisted SAF read grant (string-prefix
            // matching is unsafe — "…ROMs" prefixes "…ROMs2"). The all-files build can
            // ALSO reach a content:// folder by resolving it to a POSIX path under
            // MANAGE_EXTERNAL_STORAGE, so honor that too (checking the grant itself, not
            // raw canRead, so a temporarily-unmounted SD isn't misread as lost access).
            val persisted = runCatching { context.contentResolver.persistedUriPermissions }
                .getOrDefault(emptyList())
                .filter { it.isReadPermission }
                .map { it.uri.toString() }
                .toHashSet()
            val allFiles = Build.VERSION.SDK_INT >= Build.VERSION_CODES.R &&
                android.os.Environment.isExternalStorageManager()
            // R+ scoped storage: a /storage path is only TRULY readable with all-files.
            // File.canRead() FALSE-POSITIVES there — it returns true for a path whose
            // contents scoped storage then refuses to list — which silently defeated this
            // whole check after an Auto Backup restore (folder URI restored, permission not,
            // yet canRead said "fine" → no recovery → empty library). So on R+ trust the
            // all-files grant and never canRead; canRead is only meaningful on pre-R legacy.
            fun posixReadable(path: String?): Boolean {
                if (path == null) return false
                if (allFiles) return true
                return Build.VERSION.SDK_INT < Build.VERSION_CODES.R &&
                    runCatching { File(path).canRead() }.getOrDefault(false)
            }
            return romsDirs.any { raw ->
                when {
                    raw.startsWith("content:") ->
                        raw in persisted || (allFiles && resolveTreeUriToPosix(raw) != null)
                    raw.startsWith("file:") -> posixReadable(raw.toUri().path)
                    else -> posixReadable(raw)
                }
            }
        }

        fun assetCopyRoot(context: Context): String {
            val custom = systemDirPosix()
            return custom?.takeIf { validateSystemDirWritable(it) }
                ?: context.getExternalFilesDir(null)?.absolutePath
                ?: context.dataDir.absolutePath
        }

        fun copyAssetAll(p_context: Context, srcPath: String) {
            copyAssetAll(p_context, srcPath, assetCopyRoot(p_context))
        }

        private fun copyAssetAll(p_context: Context, srcPath: String, rootDir: String) {
            val assetMgr = p_context.assets
            try {
                val destPath = rootDir + File.separator + srcPath
                assetMgr.list(srcPath)?.let {
                    if (it.isEmpty()) {
                        MainActivity.copyFile(p_context, srcPath, destPath)
                    } else {
                        val dir = File(destPath)
                        if (!dir.exists()) dir.mkdirs()
                        for (element in it) {
                            copyAssetAll(p_context, srcPath + File.separator + element, rootDir)
                        }
                    }
                }
            } catch (e: IOException) {
                android.util.Log.e("ARMSX2", "copyAssetAll failed: $srcPath -> $rootDir: ${e.message}")
            }
        }

        private fun sameFilePath(a: File, b: File): Boolean {
            val ca = runCatching { a.canonicalFile }.getOrDefault(a.absoluteFile)
            val cb = runCatching { b.canonicalFile }.getOrDefault(b.absoluteFile)
            return ca == cb
        }

        private fun copyFileViaTemp(src: File, target: File): Boolean {
            if (sameFilePath(src, target))
                return target.exists() && target.length() > 0L
            if (!src.exists() || src.length() <= 0L)
                return false

            val parent = target.parentFile ?: return false
            if (!parent.exists() && !parent.mkdirs())
                return false

            val tmp = File(parent, ".${target.name}.migrate.tmp")
            if (tmp.exists())
                tmp.delete()

            return runCatching {
                src.copyTo(tmp, overwrite = true)
                if (tmp.length() <= 0L)
                    return@runCatching false
                if (target.exists() && !target.delete())
                    return@runCatching false
                val installed = tmp.renameTo(target) || runCatching {
                    tmp.copyTo(target, overwrite = true)
                    true
                }.getOrDefault(false)
                installed && target.exists() && target.length() > 0L
            }.getOrDefault(false).also {
                tmp.delete()
            }
        }

        fun getSupportedGLESVersion(context: Context): Double {
            val am = context.getSystemService(ACTIVITY_SERVICE) as ActivityManager
            val info = am.deviceConfigurationInfo
            return info.glEsVersion.toDouble()
        }

        fun isAndroidEmulator(): Boolean {
            return Build.MODEL.startsWith("sdk_")
        }
    }

    val swapDiscAction = registerForActivityResult(
        StartActivityForResult()
    ) { result: ActivityResult ->
        if (result.resultCode == RESULT_OK) {
            try {
                val intent = result.data
                val uri = intent?.dataString ?: ""
                if (uri.isNotEmpty()) {
                    // Swap the mounted disc instead of rebooting. The old path
                    // (restart()) booted the picked disc as a fresh VM, which
                    // dropped CodeBreaker/multi-disc hand-offs and never showed
                    // a "disc changed" notification. NativeApp.changeDisc keeps
                    // the running VM, cycles the tray so the game detects the
                    // new disc, and emits the on-screen "Disc changed to …" OSD.
                    // Runs off-thread since it parks the CPU thread and blocks.
                    println("@@ANDROID_SWAP_DISC@@ uri=${uri.take(240)}")
                    kotlin.concurrent.thread {
                        val ok = runCatching { NativeApp.changeDisc(uri) }.getOrDefault(false)
                        instance?.runOnUiThread {
                            if (ok) {
                                // changeDisc parks the VM to swap on the CPU
                                // thread; unpause so the game runs and detects
                                // the new disc (otherwise the screen sits frozen
                                // on the paused frame).
                                resume()
                            } else {
                                // Swap Disc is swap-only. If native rejected
                                // the image it already restored the old disc,
                                // so just resume the existing session.
                                resume()
                            }
                        }
                    }
                }
            } catch (_: Exception) { }
        }
    }

    val bootDiscAction = registerForActivityResult(
        StartActivityForResult()
    ) { result: ActivityResult ->
        if (result.resultCode == RESULT_OK) {
            try {
                val uri = result.data?.dataString ?: ""
                if (uri.isNotEmpty()) {
                    println("@@ANDROID_BOOT_DISC@@ uri=${uri.take(240)}")
                    launchGame(uri, null)
                }
            } catch (_: Exception) { }
        }
    }

    init {
        instance = this
    }

    /** Latched on first kickoffEmucoreInit so a second call (e.g. after
     *  the user re-enters setup via the cog) is a no-op. Heavy init —
     *  asset copy, EmuFolders setup, JIT test prelude — must run once
     *  per process. */
    private var emucoreInitDone = false

    /** Latch for the debug-build auto-boot-to-BIOS path. Fires once per
     *  process from kickoffEmucoreInit's tail so JIT tests finish first,
     *  then runs startBios() with no game disc. Used for perfape baseline
     *  captures without manually tapping the BIOS card. */
    private var autoBootBiosFired = false

    /** Build-config flag for the auto-boot-to-BIOS path above. Flip to
     *  true (here, or move to BuildConfig via app/build.gradle.kts if a
     *  variant-level toggle is wanted) to drop straight into the BIOS
     *  shell on app launch — useful for perfape captures. */
    private val AUTO_BOOT_BIOS = false

    /**
     * Run the heavy one-shot emucore init (asset copy + EmuFolders +
     * SDL/HID setup + EE/VIF JIT-test prelude). MUST be called only
     * AFTER the user has finished the setup wizard so `MainActivityRuntime.systemDir`
     * resolves to the chosen path before `NativeApp.initializeOnce`
     * locks `EmuFolders::AppRoot` in for the rest of the process.
     *
     * Idempotent — guarded by emucoreInitDone. Safe to call from both
     * onCreate (returning user, setupComplete already true) and the
     * setContent LaunchedEffect (first-time user, setupComplete just
     * flipped).
     */
    private fun kickoffEmucoreInit() {
        if (emucoreInitDone) return
        emucoreInitDone = true
        // Record the root native is about to pin (same resolution as
        // NativeApp.initializeOnce's dataPath) so a later storage change can be
        // detected and trigger a restart instead of silently not taking effect.
        lastInitDataRoot = assetCopyRoot(applicationContext)

        // #9: one-time recovery for a fresh install that reuses an old data folder — restore
        // settings from the in-folder mirror, or seed from the folder's old PCSX2-Android.ini,
        // BEFORE the core loads/rewrites it. No-op (guarded) for anyone already on the new UI.
        runCatching { com.armsx2.config.ConfigStore.reconcileReusedFolder() }

        // Default resources — shaders, GameIndex, fonts, fullscreenui,
        // patches.zip, controller DB. assetCopyRoot resolves to the
        // user's chosen systemDir (now valid post-setup) so emucore
        // finds them at <systemDir>/resources/...
        copyAssetAll(applicationContext, "bios")
        copyAssetAll(applicationContext, "resources")

        // Point the ANGLE EGL env vars at the bundled libs (or clear them) before the
        // GS thread ever opens a GL context. Re-applied per launch below too.
        applyAngleEnv(applicationContext)

        // Keep the configured BIOS in app-private internal storage (NOT under a
        // custom/SD data root). The native core can't reliably open a BIOS off a
        // removable/SAF volume on Android 11+, so a data-root-on-SD setup failed VM
        // init and bounced back to the library. This also MIGRATES any BIOS an older
        // build moved onto the SD data root back to internal. No-op when no BIOS is
        // set or it's already internal; on copy failure we leave the pref untouched
        // so biosFolderPosix still points emucore at the old (working) location.
        bios.value?.takeIf { it.isNotEmpty() }?.let { current ->
            val src = File(current)
            val target = File(internalBiosDir(applicationContext).apply { mkdirs() }, src.name)
            if (!sameFilePath(target, src)) {
                val present = (target.exists() && target.length() > 0L) ||
                    copyFileViaTemp(src, target)
                if (present) {
                    bios.value = target.absolutePath
                    prefs.edit { putString("bios", target.absolutePath) }
                }
            } else if (target.exists() && target.length() > 0L) {
                bios.value = target.absolutePath
                prefs.edit { putString("bios", target.absolutePath) }
            }
        }

        // (BIOS data-root mirror runs in the background invoke{} block below — it's
        // cosmetic and must not block first paint / risk an ANR on slow SD cards.)

        invoke {
            NativeApp.initializeOnce(applicationContext)
            nativeReady.value = true

            // Pin Filenames/BIOS to the file the setup wizard copied —
            // deferred to here because Host::SetBaseStringSettingValue
            // null-derefs when called before initializeOnce installs the
            // base settings layer. The onboarding flow only
            // persists to MainActivityRuntime.bios + Java prefs; the actual setSetting
            // happens here, AFTER the layer is installed AND
            // SetDefaultSettings has run (so default-empty doesn't
            // overwrite our pin).
            //
            // Without this pin emucore's LoadBIOS falls back to
            // FindBiosImage()'s alphabetical scan, ignoring the wizard's
            // selection — see armsx2_bios_filename_pin memo.
            bios.value?.let { biosPath ->
                val name = File(biosPath).name
                if (name.isNotEmpty()) {
                    NativeApp.setSetting("Filenames", "BIOS", "string", name)
                    NativeApp.commitSettings()
                }
            }

            // Mirror the canonical (app-private) BIOS into the user's data root at
            // <dataRoot>/bios so it's visible/backup-able next to cache/covers/etc.
            // COPY-ONLY (the emulator reads the app-private copy pinned above), so
            // it can never break boot. Runs here on the background init thread so it
            // never blocks first paint / risks an ANR on slow SD cards. The migration
            // block above (inline) has already populated internalBios. Skips dotfiles
            // and ".migrate.tmp" scratch leftovers so junk never lands in the mirror.
            runCatching {
                val rootPosix = systemDirPosix()
                if (!rootPosix.isNullOrEmpty()) {
                    val internalBios = internalBiosDir(applicationContext)
                    val mirrorDir = File(rootPosix, "bios")
                    if (mirrorDir.absolutePath != internalBios.absolutePath) {
                        mirrorDir.mkdirs()
                        internalBios.listFiles { f ->
                            f.isFile && !f.name.startsWith(".") && !f.name.endsWith(".migrate.tmp")
                        }?.forEach { f ->
                            val dst = File(mirrorDir, f.name)
                            if (!dst.exists() || dst.length() != f.length()) copyFileViaTemp(f, dst)
                        }
                    }
                }
            }

            // Set up JNI
            SDLControllerManager.nativeSetupJNI()
            SDLControllerManager.initialize()
            HIDDeviceManager(applicationContext)

            println("PCSX2_INIT")

            // Tests that need VTLB/eeMem — run after init
            NativeApp.runEeJitTests()
            NativeApp.runEeSeqTests()
            NativeApp.runVifTests()

            // Debug-build auto-boot to BIOS. Lets us drop straight into the
            // BIOS shell on app launch for perfape baseline captures —
            // skips tapping through the library. One-shot via latch so
            // re-entering Setup and back doesn't relaunch. Currently
            // gated off via AUTO_BOOT_BIOS — flip to true to re-enable.
            @Suppress("KotlinConstantConditions")
            if (AUTO_BOOT_BIOS && BuildConfig.DEBUG && !autoBootBiosFired &&
                eState.value == EmuState.STOPPED) {
                autoBootBiosFired = true
                startBios()
            }
        }
    }

    // ---- Turbo / rapid-fire ------------------------------------------------
    // While a turbo-flagged physical button is held, alternately press/release its
    // PS2 target at ~15 Hz on the main thread. Keyed by (port, physical keycode) so
    // multiple turbo buttons (and both players) autofire independently.
    private val turboHandler = android.os.Handler(android.os.Looper.getMainLooper())
    private val turboRunnables = HashMap<Long, Runnable>()
    private val turboPressed = HashMap<Long, Boolean>()
    private fun turboMapKey(physicalCode: Int, port: Int) =
        (port.toLong() shl 32) or (physicalCode.toLong() and 0xffffffffL)

    private fun handleTurbo(physicalCode: Int, type: KeyEventType, target: Int, port: Int) {
        val key = turboMapKey(physicalCode, port)
        if (type == KeyEventType.KeyDown) {
            if (turboRunnables.containsKey(key)) return // already firing (auto-repeat DOWNs)
            turboPressed[key] = false
            val r = object : Runnable {
                override fun run() {
                    val pressed = !(turboPressed[key] ?: false)
                    turboPressed[key] = pressed
                    sendKeyAction(if (pressed) KeyEventType.KeyDown else KeyEventType.KeyUp, target, port)
                    turboHandler.postDelayed(this, 33L) // ~15 presses/sec (33ms on, 33ms off)
                }
            }
            turboRunnables[key] = r
            turboHandler.post(r)
        } else {
            turboRunnables.remove(key)?.let { turboHandler.removeCallbacks(it) }
            turboPressed.remove(key)
            sendKeyAction(KeyEventType.KeyUp, target, port) // guarantee released on let-go
        }
    }

    fun sendKeyAction(p_action: KeyEventType, p_keycode_in: Int, port: Int = 0) {
        // Any physical gamepad key event implies the user is on a
        // controller — latch the on-screen touch controls hidden until a
        // screen press flips them back on. Idempotent.
        com.armsx2.ui.touch.TouchControls.onControllerInputDetected()
        // D-pad as left analog stick: a physical d-pad press (arriving as a key,
        // not a HAT) drives the left stick instead of the digital d-pad. The
        // remapped code is >=110 so the analog-force branch below gives a
        // (near-)full deflection. HAT-style d-pads are folded into the stick in
        // dispatchStick instead. 19=Up 20=Down 21=Left 22=Right ->
        // 110=L_Up 112=L_Down 113=L_Left 111=L_Right.
        val p_keycode = if (ControllerMappings.dpadAsLeftStick()) {
            when (p_keycode_in) {
                19 -> 110; 20 -> 112; 21 -> 113; 22 -> 111; else -> p_keycode_in
            }
        } else p_keycode_in
        if (p_action == KeyEventType.KeyDown) {
            var pad_force = 0
            if (p_keycode >= 110) {
                var _abs = 90f // Joystic test value
                _abs = min(_abs, 100f)
                pad_force = (_abs * 32766.0f / 100).toInt()
            } else {
                // Pressure modifier: soft (~50%) press for pressure-capable
                // buttons while the modifier is held; 0 (full press) otherwise.
                pad_force = com.armsx2.ui.touch.TouchControls.pressureRangeFor(p_keycode)
            }
            // KEYS bound to an analog stick code (d-pad-as-left-stick, or a button
            // bound to a "(send)" stick row): register the held deflection with the
            // merge layer so a stick MOTION event can't release it mid-hold.
            if (p_keycode in 110..123 && port in analogKeyHeld.indices)
                analogKeyHeld[port][p_keycode] = pad_force / 32767f
            NativeApp.setPadButtonForPort(port, p_keycode, pad_force, true)
        } else if (p_action == KeyEventType.KeyUp || p_action == KeyEventType.Unknown) {
            if (p_keycode in 110..123 && port in analogKeyHeld.indices)
                analogKeyHeld[port].remove(p_keycode)
            NativeApp.setPadButtonForPort(port, p_keycode, 0, false)
        }
    }

    /** Apply the user's Emulation Screen Orientation choice (global, prefs "ui.orientation").
     *  0=Use Device Setting, 1=Landscape, 2=Portrait, 3=Auto-Rotate. SENSOR_* variants let
     *  the device still flip 180° within the locked axis. Called on launch + on change. */
    fun applyEmulationOrientation() {
        requestedOrientation = when (prefs.getInt("ui.orientation", 0)) {
            1 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            2 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
            3 -> ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
            else -> ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
        }
    }

    private fun applyEdgeToEdge() {
        enableEdgeToEdge(
            statusBarStyle = SystemBarStyle.auto(
                android.graphics.Color.TRANSPARENT,
                android.graphics.Color.TRANSPARENT,
            ),
            navigationBarStyle = SystemBarStyle.auto(
                LIGHT_NAVIGATION_BAR_SCRIM,
                DARK_NAVIGATION_BAR_SCRIM,
            ),
        )
    }

    private fun applySystemBarTheme(darkTheme: Boolean, showSystemBars: Boolean) {
        // Re-apply edge-to-edge along with icon appearance. Some Android versions
        // reset one half of this state when the app switches theme at runtime.
        applyEdgeToEdge()
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            val useDarkIcons = !darkTheme
            isAppearanceLightStatusBars = useDarkIcons
            isAppearanceLightNavigationBars = useDarkIcons
            if (showSystemBars) show(WindowInsetsCompat.Type.systemBars())
            else hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        applyEdgeToEdge()
        super.onCreate(savedInstanceState)
        com.armsx2.navigation.UiNavigator.drawerOpen.value = false
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            window.isStatusBarContrastEnforced = false
            window.isNavigationBarContrastEnforced = false
        }
        // Local co-op: PS2 port 2 is enabled at GAME BOOT (applyRendererPrefs) when a
        // 2nd controller is already connected — NOT hot-plugged mid-game, which crashed
        // by rebuilding the live pad list. So PadRouter only ROUTES P2 input; it must
        // not trigger a runtime Pad::LoadConfig. (onPlayer2Joined left unset = no-op.)
        // Swallow back presses unconditionally. Compose BackHandlers (the
        // in-game overlay's submenu drill-down, the library's eventual
        // back-to-game escape, etc.) register at higher priority and
        // consume the event when they're appropriate; this low-priority
        // no-op catches anything they don't, so the system never falls
        // through to finish() on the activity. Same callback also stops
        // controller "B"/"Circle" buttons that the OS maps to KEYCODE_BACK
        // (Xbox/DualShock default) from killing the app.
        onBackPressedDispatcher.addCallback(this) {
            // intentionally empty — pure stay-alive sentinel
        }
        prefs = applicationContext.getSharedPreferences("ARMSX2", MODE_PRIVATE)
        com.armsx2.i18n.I18n.init(applicationContext)
        applyEmulationOrientation()
        com.armsx2.CoverArtStyle.load()
        com.armsx2.GridLabels.load()
        com.armsx2.EnglishTitles.load()
        com.armsx2.HiddenGames.load()
        com.armsx2.LibraryTitles.load()
        com.armsx2.LibraryRecentShelf.load()
        com.armsx2.LibraryView.load()
        com.armsx2.ui.UiScale.load()
        com.armsx2.ui.theme.ThemePreferences.load()
        com.armsx2.ui.theme.BootLogoPreferences.load()
        com.armsx2.ui.theme.ToolbarPositionPreferences.load()
        com.armsx2.ui.theme.LibraryChromePreferences.load()
        com.armsx2.ControllerSkinStore.load(applicationContext)
        startAutosaveIntervalJob()
        // Restore the saved rumble master toggle into the native gate (NativeApp.onPadRumble).
        NativeApp.sRumbleEnabled = ControllerMappings.rumbleEnabled()
        // Seed the pad-router's multitap gate before any in-game input is dispatched, so
        // slot routing (2 vs 8 slots) is correct from the first controller event.
        com.armsx2.input.PadRouter.multitapEnabled = ControllerMappings.multitapEnabled()
        setupComplete.value = prefs.getBoolean("setupComplete", false)
        systemDir.value = prefs.getString("systemDir", null)
        bios.value = prefs.getString("bios", null)
        biosDir.value = prefs.getString("biosDir", null)
        // Load roms folders. New format: JSON array under "romsDirs" pref.
        // Legacy format: single string under "roms" pref (pre-multi-dir).
        // Migration path: read legacy if present, hoist into the list, keep
        // both keys in sync until the user re-confirms in setup. Once the
        // user adds/removes via the new picker the legacy key is dropped.
        romsDirs.value = run {
            val newJson = prefs.getString("romsDirs", null)
            if (newJson != null) {
                runCatching {
                    val arr = org.json.JSONArray(newJson)
                    List(arr.length()) { arr.getString(it) }
                }.getOrDefault(emptyList())
            } else {
                val legacy = prefs.getString("roms", null)
                if (legacy != null) listOf(legacy) else emptyList()
            }
        }
        // Setup recovery. Auto Backup can restore our prefs (incl. setupComplete + the
        // ROMs URIs) on reinstall, but SAF/all-files PERMISSIONS are never backed up — so
        // a restored setup can point at a folder we can no longer read, which would strand
        // the user in an empty library with the wizard skipped. If no configured ROMs
        // folder is actually reachable, drop setupComplete for this session so the wizard
        // re-runs (and re-requests the permission); finishSetup re-arms it.
        if (setupComplete.value && !romsAccessible(this, romsDirs.value)) {
            setupComplete.value = false
            setupRecoveryNeeded.value = true
        }
        // Renderer + upscale now live in the Settings tier (global ∘ per-game);
        // ConfigStore.loadGlobal() also one-time-seeds them from the legacy
        // "renderer"/"upscaleFloat" prefs. Read the global baseline for the
        // pre-launch UI; applyRendererPrefs re-resolves per-game at boot.
        com.armsx2.config.ConfigStore.loadGlobal().let { g0 ->
            renderer.value = g0.renderer
            upscale.value = g0.upscaleFloat
        }
        customDriverId.value = prefs.getString("customDriverId", null)?.takeIf { it.isNotEmpty() }
        surface.value = EmulationSurface(this)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.statusBarColor = android.graphics.Color.TRANSPARENT
        window.navigationBarColor = android.graphics.Color.TRANSPARENT
        val initialLightSystemBars = when (com.armsx2.ui.theme.ThemePreferences.mode.value) {
            com.armsx2.ui.theme.ThemeMode.System ->
                resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK !=
                    Configuration.UI_MODE_NIGHT_YES
            com.armsx2.ui.theme.ThemeMode.Light -> true
            com.armsx2.ui.theme.ThemeMode.Dark,
            com.armsx2.ui.theme.ThemeMode.Black,
            com.armsx2.ui.theme.ThemeMode.Oled -> false
        }
        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.show(WindowInsetsCompat.Type.systemBars())
            controller.isAppearanceLightStatusBars = initialLightSystemBars
            controller.isAppearanceLightNavigationBars = initialLightSystemBars
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        ViewCompat.requestApplyInsets(window.decorView)

        // Sustained Performance Mode (API 24+): holds a steady, thermally-
        // sustainable clock instead of boost-then-throttle. GOOD for long sessions
        // on thermally-limited devices, but it CAPS the peak clock — which hurts
        // peak-hungry games (e.g. GoW2's VU1) that need max MHz in the moment. So
        // it's OPT-IN, default OFF (pref "ui.sustainedPerf"); most users want peak.
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N &&
            prefs.getBoolean("ui.sustainedPerf", false)) {
            runCatching { window.setSustainedPerformanceMode(true) }
        }

        // Defer asset copy + emucore init until setup is complete. On the
        // first-ever run, `systemDir` isn't picked yet at onCreate time —
        // so initializeOnce would resolve to the app-private fallback and
        // wedge `EmuFolders::AppRoot` there for the rest of the process,
        // even after the user finishes the wizard. Memcards then read
        // from the wrong dir on first boot ("scary empty cards"); next
        // app launch picks up the correct path from prefs and saves
        // re-appear. Gating on setupComplete avoids the misroute.
        //
        // Idempotent guard: kickoffEmucoreInit checks emucoreInitDone so
        // setupComplete flipping multiple times (re-entry via cog button
        // doesn't toggle it back to false, but be defensive) only fires
        // the heavy init once.
        if (setupComplete.value) {
            kickoffEmucoreInit()
        }
        // else: setContent's LaunchedEffect(setupComplete.value) below
        // calls kickoffEmucoreInit when the wizard finishes.

        val glVersion = getSupportedGLESVersion(this)

        if (glVersion < 3.1) {
            eState.value = EmuState.RENDER_UNSUPPORTED
            println("RENDER_UNSUPPORTED")
        }

        if (isAndroidEmulator()) {
            eState.value = EmuState.EMULATOR_UNSUPPORTED
            println("DEVICE_UNSUPPORTED")
        }
        handleExternalLaunchIntent(intent)
        setContent {
            com.armsx2.ui.theme.Armsx2Theme {
            val themedWindowBackground = androidx.compose.material3.MaterialTheme.colorScheme.background
            androidx.compose.runtime.SideEffect {
                window.setBackgroundDrawable(android.graphics.drawable.ColorDrawable(themedWindowBackground.toArgb()))
            }
            // Keep the library/menu immersive (nav bar hidden, swipe-transient) just like
            // in-game, so it doesn't sit on top of the toolbar. Bars stay visible only where
            // reliable system UI is genuinely needed: the setup wizard, the touch-layout
            // editor, and the unsupported-hardware error screens. (Previously `showLibrary`
            // and plain STOPPED forced the bar on for the whole library.)
            val showSystemBars = !setupComplete.value ||
                setupEditorVisible.value ||
                eState.value == EmuState.RENDER_UNSUPPORTED ||
                eState.value == EmuState.EMULATOR_UNSUPPORTED
            val darkTheme = when (com.armsx2.ui.theme.ThemePreferences.mode.value) {
                com.armsx2.ui.theme.ThemeMode.System -> androidx.compose.foundation.isSystemInDarkTheme()
                com.armsx2.ui.theme.ThemeMode.Light -> false
                com.armsx2.ui.theme.ThemeMode.Dark,
                com.armsx2.ui.theme.ThemeMode.Black,
                com.armsx2.ui.theme.ThemeMode.Oled -> true
            }
            androidx.compose.runtime.SideEffect {
                applySystemBarTheme(darkTheme = darkTheme, showSystemBars = showSystemBars)
            }
            // First-time setup deferral: when the wizard finishes and
            // setupComplete flips to true, kick off the heavy emucore
            // init now that `MainActivityRuntime.systemDir` reflects the user's pick.
            // The kickoff helper is idempotent (emucoreInitDone latch),
            // so this firing AFTER an onCreate-time call (returning user
            // with setupComplete already true) is a no-op.
            androidx.compose.runtime.LaunchedEffect(setupComplete.value) {
                if (setupComplete.value) {
                    kickoffEmucoreInit()
                }
            }

            // One-time notice when setup was re-shown because a restored config
            // pointed at a folder we can no longer read (see the recovery check in
            // onCreate). Explains why the wizard reappeared.
            androidx.compose.runtime.LaunchedEffect(setupRecoveryNeeded.value) {
                if (setupRecoveryNeeded.value) {
                    android.widget.Toast.makeText(
                        applicationContext,
                        "Couldn't open your saved game folder — this can happen after reinstalling or restoring a backup. Please re-select it.",
                        android.widget.Toast.LENGTH_LONG,
                    ).show()
                    setupRecoveryNeeded.value = false
                }
            }

            androidx.compose.runtime.LaunchedEffect(
                setupComplete.value,
                nativeReady.value,
                pendingExternalLaunch.value,
                pendingLaunch.value,
            ) {
                launchPendingExternalGameIfReady()
            }

            // Setup wizard runs once. After it persists prefs and flips
            // setupComplete the main emulator UI takes over. Re-entering
            // setup requires clearing app data (or wiping the prefs key).
            if (!setupComplete.value || setupEditorVisible.value) {
                com.armsx2.ui.onboarding.OnboardingScreen()
            } else if (setupComplete.value) {
                // Per-game play-time tracking: count while RUNNING, accumulate on
                // pause / stop / background. Keyed on the serial too so the session
                // re-arms once currentGame resolves shortly after launch.
                androidx.compose.runtime.LaunchedEffect(eState.value, currentGame.value?.serial) {
                    if (eState.value == EmuState.RUNNING)
                        PlayTime.startSession(currentGame.value?.serial)
                    else
                        PlayTime.endSession()
                }
                WindowImpl.Window {
                    if (surface.value != null) {
                        // Pull Compose focus onto the surface as soon as it's
                        // composed AND whenever a game starts running. Without
                        // this the AndroidView starts un-focused, so onKeyEvent
                        // silently drops gamepad input until the user taps the
                        // screen / presses A to grant focus by hand.
                        //
                        // Keying only on surface.value (which is created once at
                        // onCreate and never reassigned) meant focus was grabbed
                        // a single time at startup and never again — so launching
                        // a game from the library left the surface un-focused on
                        // the STOPPED->RUNNING transition. Android's focus
                        // traversal then ate the first few physical face-button
                        // presses (A->confirm, B->back move focus in) before the
                        // surface finally took focus, which is why users had to
                        // mash Cross/Triangle a few times to "wake" the pad and
                        // Square (no traversal fallback) appeared totally dead.
                        // Re-key on eState + showLibrary so focus follows the
                        // launch transition; skip while an overlay is open (the
                        // effect below owns focus in that case).
                        androidx.compose.runtime.LaunchedEffect(
                            surface.value, eState.value, WindowImpl.showLibrary.value
                        ) {
                            if (eState.value == EmuState.RUNNING &&
                                !WindowImpl.showLibrary.value &&
                                !WindowImpl.overlayVisible.value
                            ) {
                                surface.value?.isFocusable = true
                                surface.value?.isFocusableInTouchMode = true
                                runCatching { focusRequester.requestFocus() }
                            }
                        }
                        // Controller menu nav: the embedded game SurfaceView holds
                        // Android-level focus, and while an embedded View has focus
                        // the D-pad bypasses Compose's focus system entirely (so the
                        // pause overlay can never receive it). When the overlay opens,
                        // drop the SurfaceView's View-level focusability + clear its
                        // focus so Android focus moves into the Compose tree and the
                        // overlay's requestFocus can take it. Restore it (and re-grab
                        // game input) when the overlay closes.
                        // The surface must release Android focus whenever the pad
                        // drives the frontend — either a Compose surface covers a
                        // running game (frontendCovers) OR no game is running at all
                        // (the root library + every manager/settings sub-screen). If
                        // the focusable surface kept focus at rest it would swallow
                        // the synthetic D-pad meant for those Compose screens.
                        val frontendOwnsFocus = WindowImpl.frontendCovers ||
                            eState.value != EmuState.RUNNING
                        androidx.compose.runtime.LaunchedEffect(frontendOwnsFocus) {
                            val sv = surface.value
                            if (frontendOwnsFocus) {
                                sv?.isFocusableInTouchMode = false
                                sv?.isFocusable = false
                                sv?.clearFocus()
                            } else {
                                sv?.isFocusable = true
                                sv?.isFocusableInTouchMode = true
                                if (eState.value == EmuState.RUNNING)
                                    runCatching { focusRequester.requestFocus() }
                                // Invariant: the pause overlay is the only thing
                                // that keeps the game paused from the UI. When it
                                // closes, make sure the VM is running again —
                                // several close paths (applying a controller/
                                // settings profile, swap disc) left it PAUSED with
                                // NO overlay shown, so the game looked "frozen"
                                // until the user re-opened the menu and hit Resume.
                                // Library manages its own run state; touch-layout
                                // edit mode is intentionally kept paused for a
                                // stable editing screen (it resumes on exit, see
                                // TouchControls.exitEditMode). No-op if running.
                                if (eState.value == EmuState.PAUSED &&
                                    !WindowImpl.showLibrary.value &&
                                    !com.armsx2.ui.touch.TouchControls.editMode.value
                                ) {
                                    resume()
                                }
                            }
                        }
                        AndroidView(factory = { surface.value!! }, modifier = Modifier
                            // Drop the surface from the focus system while ANY
                            // Compose frontend surface (pause overlay, in-game
                            // manager/Save-Load screen, memcard dialog, library) is
                            // open, or while no game is running, so it can't hold or
                            // steal focus away from that surface's controller nav.
                            .focusable(!frontendOwnsFocus)
                            .focusRequester(focusRequester)
                            .fillMaxSize()
                            .pointerInput(Unit) {
                                // In-game pausing moved OFF the surface-wide
                                // long-press: it fired on accidental presses in
                                // empty screen space. The pause overlay now opens
                                // via long-press on the invisible PAUSE hotspot
                                // widget between the DPad and face buttons (see
                                // TouchControlsOverlay.PauseWidget). Long-press
                                // here only toggles the toolbar when no game is
                                // up (games-list screen).
                                //
                                // onPress fires on every initial pointer down on
                                // the surface (events that don't land on a touch
                                // button — the buttons consume their own touches).
                                // Any such tap means the user is using the screen,
                                // so unlatch any controller-mode hide so the touch
                                // controls reappear. onPress doesn't consume the
                                // gesture; long-press detection continues to run.
                                detectTapGestures(
                                    onPress = {
                                        com.armsx2.ui.touch.TouchControls.onSurfaceTouched()
                                    },
                                    onLongPress = {
                                        if (eState.value != EmuState.RUNNING &&
                                            eState.value != EmuState.PAUSED) {
                                            WindowImpl.toolbarVisible.value = !WindowImpl.toolbarVisible.value
                                        }
                                    },
                                )
                            }
                            .onKeyEvent { event ->
                                if (eState.value != EmuState.RUNNING)
                                    return@onKeyEvent false
                                // Note: the physical menu button is handled in
                                // MainActivityRuntime.dispatchKeyEvent (so it can catch BACK /
                                // back-paddle keys); it never reaches here.
                                // Local co-op: route by the originating device — first
                                // controller = P1 (port 0), next = P2 (port 1) — and
                                // resolve the bind against THAT player's mapping.
                                val port = com.armsx2.input.PadRouter.portForDevice(event.nativeKeyEvent.deviceId)
                                // Physical-controller macro: a bound button fires the
                                // macro's whole button set at once (down on press, up on
                                // release), reusing the macro slots the on-screen M1-M4
                                // buttons use. Checked before normal pad routing so a
                                // macro overrides that button's regular mapping.
                                val macro = com.armsx2.ui.touch.TouchControls.macroForPhysicalCode(event.key.nativeKeyCode)
                                if (macro != null) {
                                    // Through fireMacro so a macro with a Frequency set
                                    // TOGGLES its button set while held (turbo) instead of
                                    // just holding it. Keyed per port, so P1 and P2 can
                                    // hold the same macro without cancelling each other.
                                    com.armsx2.ui.touch.TouchControls.fireMacro(
                                        macro, "pad$port", event.type == KeyEventType.KeyDown,
                                    ) { code, pressed ->
                                        sendKeyAction(
                                            if (pressed) KeyEventType.KeyDown else KeyEventType.KeyUp,
                                            code, port,
                                        )
                                    }
                                    return@onKeyEvent true
                                }
                                val target = ControllerMappings.targetForPhysical(event.key.nativeKeyCode, port)
                                    ?: return@onKeyEvent false
                                // Turbo/rapid-fire: while the physical button is held, the
                                // PS2 button auto-presses at ~15 Hz (see handleTurbo).
                                if (ControllerMappings.isTurboTarget(target, port)) {
                                    handleTurbo(event.key.nativeKeyCode, event.type, target, port)
                                    return@onKeyEvent true
                                }
                                sendKeyAction(event.type, target, port)
                                true
                            })
                    }

                    if (eState.value == EmuState.STOPPED || eState.value == EmuState.RENDER_UNSUPPORTED || eState.value == EmuState.EMULATOR_UNSUPPORTED) {
                        Box(Modifier
                            .fillMaxSize()
                            .background(Colors.surface.value)) {
                            if (eState.value == EmuState.EMULATOR_UNSUPPORTED) {
                                Box(Modifier.align(Alignment.Center)) {
                                    Column {
                                        Image(LineAwesomeIcons.Android, "",
                                            colorFilter = ColorFilter.tint(Colors.pasx2_blue),
                                            modifier = Modifier
                                                .size(150.dp)
                                                .align(Alignment.CenterHorizontally)
                                        )
                                        Text(
                                            "Android Emulator is not supported", fontSize = 22.sp, color = Colors.pasx2_blue,
                                            modifier = Modifier.align(Alignment.CenterHorizontally)
                                        )
                                        Text(
                                            "Please use a physical device", fontSize = 22.sp, color = Colors.pasx2_blue,
                                            modifier = Modifier.align(Alignment.CenterHorizontally)
                                        )
                                    }
                                }
                            } else {
                                // Games list — replaces the old runtime-test panel.
                                // The tests still run automatically on first composition
                                // (above); their results are now available via the bug
                                // toolbar button instead of taking up the main screen.
                                com.armsx2.navigation.AppNavigation()
                            }
                        }
                    }
                }
            }
            }
        }
    }

    // Physical buttons currently held down. Drives two-button hotkey combos
    // (e.g. Select + R1) — kept current at the top of dispatchKeyEvent so a
    // combo's modifier can be checked the instant its main key is pressed.
    private val heldKeys = HashSet<Int>()

    // Hold-BACK-to-exit (Dolphin-style) timer. Instance-scoped because
    // dispatchKeyEvent is an Activity method; the posted runnable is cancelled on
    // BACK release so it only fires on a genuine hold.
    private val backHoldHandler = android.os.Handler(android.os.Looper.getMainLooper())
    private var backHoldRunnable: Runnable? = null

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val kc = event.keyCode
        if (kc != KeyEvent.KEYCODE_UNKNOWN) {
            when (event.action) {
                KeyEvent.ACTION_DOWN -> heldKeys.add(kc)
                KeyEvent.ACTION_UP -> heldKeys.remove(kc)
            }
        }
        // Track the active gamepad so PS2 rumble routes to its vibrator.
        if (event.isFromSource(InputDevice.SOURCE_GAMEPAD) ||
            event.isFromSource(InputDevice.SOURCE_JOYSTICK)) {
            NativeApp.sRumbleDeviceId = event.deviceId
        }
        // #254 Emulated USB keyboard. When a game runs with the USB HID keyboard
        // attached (Settings.usbKeyboard, e.g. EQOA / Konami-keyboard titles),
        // forward physical/Bluetooth keyboard key events to it. Gated so it only
        // fires for real keyboard-source keys while the game is front-and-centre —
        // NOT while (re)binding, and NOT while any menu/overlay is up (those need
        // normal D-pad/confirm nav). Only a mappable keyboard key is consumed;
        // everything else (and all gamepad buttons) falls through to the pad /
        // hotkey / nav handling below unchanged.
        if (forwardKeyToUsbKeyboard(event, kc)) {
            return true
        }
        // System-hotkey capture (from the Hotkeys tab). Handled here, not in
        // Compose, so it can capture KEYCODE_BACK and back-paddle keys (the back
        // dispatcher swallows those before they'd reach onPreviewKeyEvent).
        // Press one button to bind it; press a second while holding the first to
        // bind a combo (first = modifier, second = main key).
        val capturing = ControllerMappings.captureHotkey.value
        if (capturing != null) {
            if (kc != KeyEvent.KEYCODE_UNKNOWN) {
                val buf = ControllerMappings.captureKeys
                if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                    if (buf.isEmpty()) {
                        // First button of this capture.
                        buf.add(kc)
                        ControllerMappings.captureFirstDownMs = event.eventTime
                    } else if (!buf.contains(kc) &&
                        event.eventTime - ControllerMappings.captureFirstDownMs >= COMBO_MIN_GAP_MS
                    ) {
                        // A distinct 2nd button pressed deliberately (held the first,
                        // then pressed this) → modifier combo. The time gate rejects a
                        // 2nd keycode fired ~instantly by one physical press (some pads
                        // emit two codes per button), which would otherwise block
                        // single-button binds entirely.
                        buf.add(kc)
                        ControllerMappings.bindHotkeyCombo(capturing, buf[0], buf[1])
                        ControllerMappings.endHotkeyCapture()
                    }
                } else if (event.action == KeyEvent.ACTION_UP) {
                    // Released before a second button arrived → single-button bind.
                    if (buf.size == 1 && buf.contains(kc)) {
                        ControllerMappings.bindHotkey(capturing, buf[0])
                        ControllerMappings.endHotkeyCapture()
                    }
                }
            }
            return true // swallow down + up while capturing
        }
        // Pad-button capture (Controls screen): bind here — like the hotkey capture above —
        // instead of via Compose's onPreviewKeyEvent, because the bind prompt is no longer a
        // focus-stealing dialog (a Dialog window would swallow controller keys before Compose or
        // this handler saw them — the 2.6.0 "can't remap buttons" bug). Bind the first real key
        // press; swallow down+up so nav (B = exit, A = confirm) can't fire mid-capture.
        val padCapture = ControllerMappings.capturePadAction.value
        if (padCapture != null) {
            if (kc != KeyEvent.KEYCODE_UNKNOWN && event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                padCapture(kc)
            }
            return true
        }
        // Pad-button capture (Pad tab): let every key fall through to Compose's
        // onPreviewKeyEvent so ANY button binds — without this the overlay nav
        // below eats B (exit), A (confirm), Y, D-pad and L1/R1 before they reach
        // the binder. Normal nav resumes the moment capture ends.
        if (ControllerMappings.padCapturing.value) {
            return super.dispatchKeyEvent(event)
        }
        // Hold the hardware/software BACK button to exit the app (Dolphin-style).
        // Scoped to IN-GAME with no overlay/menu up — where a short BACK press does
        // nothing today (it's swallowed) — so it can't disturb library/menu back
        // navigation. Behind a default-on pref. Handles BACK from ANY source
        // (handheld back buttons are often gamepad-sourced). Diagnostic log so a
        // device where it "does nothing" reveals whether BACK even arrives + the gate.
        if (kc == KeyEvent.KEYCODE_BACK) {
            // If the user bound BACK to a hotkey (e.g. Menu), that binding WINS — do
            // not hijack it for hold-to-exit. (Regression fix: hold-back consumed
            // BACK before the hotkey dispatch below, killing a BACK-bound Menu key.)
            val backBoundToHotkey = ControllerMappings.SysHotkey.values().any {
                ControllerMappings.hotkeyCode(it) == KeyEvent.KEYCODE_BACK ||
                    ControllerMappings.hotkeyModCode(it) == KeyEvent.KEYCODE_BACK
            }
            val inGame = eState.value == EmuState.RUNNING &&
                !WindowImpl.overlayVisible.value && !WindowImpl.showLibrary.value
            if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                println("@@ANDROID_HOLDBACK@@ back_down source=0x${Integer.toHexString(event.source)} inGame=$inGame backBound=$backBoundToHotkey pref=${prefs.getBoolean("ui.holdBackToExit", true)}")
            if (!backBoundToHotkey && inGame && prefs.getBoolean("ui.holdBackToExit", true)) {
                when (event.action) {
                    KeyEvent.ACTION_DOWN -> if (event.repeatCount == 0) {
                        backHoldRunnable?.let { backHoldHandler.removeCallbacks(it) }
                        val r = Runnable {
                            // Re-check state at fire time — the game may have been
                            // paused or an overlay opened during the hold.
                            if (eState.value == EmuState.RUNNING &&
                                !WindowImpl.overlayVisible.value &&
                                !WindowImpl.showLibrary.value
                            ) {
                                println("@@ANDROID_HOLDBACK@@ firing exitApp")
                                exitApp()
                            }
                            backHoldRunnable = null
                        }
                        backHoldRunnable = r
                        backHoldHandler.postDelayed(r, 700)
                    }
                    KeyEvent.ACTION_UP -> {
                        val shortPress = backHoldRunnable != null
                        backHoldRunnable?.let { backHoldHandler.removeCallbacks(it) }
                        backHoldRunnable = null
                        if (shortPress) {
                            com.armsx2.ui.emulation.EmulationMenuInputController.open()
                        }
                    }
                }
                return true
            }
        }
        // Pressure modifier (hold): while the bound button is down, pressure-capable
        // PS2 buttons report a soft press (see sendKeyAction / TouchControls). Consume
        // it so it's neither forwarded to the PS2 nor fired as a one-shot hotkey.
        // Single-button binding only (combos keep their normal hotkey behaviour).
        run {
            val pm = ControllerMappings.SysHotkey.PRESSURE_MOD
            val pmKey = ControllerMappings.hotkeyCode(pm)
            if (pmKey != KeyEvent.KEYCODE_UNKNOWN && kc == pmKey &&
                ControllerMappings.hotkeyModCode(pm) == KeyEvent.KEYCODE_UNKNOWN) {
                when (event.action) {
                    KeyEvent.ACTION_DOWN -> com.armsx2.ui.touch.TouchControls.pressureModifierHeld.value = true
                    KeyEvent.ACTION_UP -> com.armsx2.ui.touch.TouchControls.pressureModifierHeld.value = false
                }
                return true
            }
        }
        // Controller search keyboard (library). While it's up it owns the pad —
        // directional input arrives via the motion path (fireNavMove, so the RP6 HAT
        // and the stick both work); here we take the face buttons: A presses the
        // highlighted key, X backspaces, B/Back closes. D-pad keys are handled too for
        // pads that report the D-pad as KEYCODE_DPAD_*. Placed before every other
        // frontend handler so nothing leaks to the grid behind it.
        if (com.armsx2.ui.home.LibraryKeyboard.visible.value) {
            if (event.action == KeyEvent.ACTION_DOWN) {
                when (kc) {
                    KeyEvent.KEYCODE_DPAD_UP -> com.armsx2.ui.home.LibraryKeyboard.move(0, -1)
                    KeyEvent.KEYCODE_DPAD_DOWN -> com.armsx2.ui.home.LibraryKeyboard.move(0, 1)
                    KeyEvent.KEYCODE_DPAD_LEFT -> com.armsx2.ui.home.LibraryKeyboard.move(-1, 0)
                    KeyEvent.KEYCODE_DPAD_RIGHT -> com.armsx2.ui.home.LibraryKeyboard.move(1, 0)
                    KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
                    KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER ->
                        if (event.repeatCount == 0) com.armsx2.ui.home.LibraryKeyboard.press()
                    KeyEvent.KEYCODE_BUTTON_X ->
                        if (event.repeatCount == 0) com.armsx2.ui.home.LibraryKeyboard.backspace()
                    KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK ->
                        if (event.repeatCount == 0) com.armsx2.ui.home.LibraryKeyboard.close()
                }
            }
            return true
        }
        // Settings search, result-browse mode (reached once the on-screen keyboard is dismissed
        // with Done — the keyboard block above owns input while it's up). D-pad moves the result
        // selection, A jumps to the setting, Y re-opens the keyboard, B closes. Owns the pad so
        // nothing leaks to the settings screen behind.
        if (com.armsx2.ui.settingshub.SettingsSearch.visible.value) {
            if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                when (kc) {
                    KeyEvent.KEYCODE_DPAD_UP -> com.armsx2.ui.settingshub.SettingsSearch.move(-1)
                    KeyEvent.KEYCODE_DPAD_DOWN -> com.armsx2.ui.settingshub.SettingsSearch.move(1)
                    KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
                    KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER ->
                        com.armsx2.ui.settingshub.SettingsSearch.activate()
                    KeyEvent.KEYCODE_BUTTON_Y ->
                        com.armsx2.ui.settingshub.SettingsSearch.reopenKeyboard()
                    KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK ->
                        com.armsx2.ui.settingshub.SettingsSearch.close()
                }
            }
            return true
        }
        // Shader-parameter editor. Owns the pad while it's up, ahead of the pause menu it
        // was opened from — otherwise the menu behind keeps the input and the editor is
        // read-only (which is exactly what shipped in vc1150). Placed AFTER the keyboard
        // block above on purpose: naming a preset hands input to LibraryKeyboard, and it
        // must keep it until it closes.
        if (com.armsx2.ui.common.ShaderParamsEditor.visible) {
            val editor = com.armsx2.ui.common.ShaderParamsEditor
            val down = event.action == KeyEvent.ACTION_DOWN
            when (kc) {
                // Repeats allowed on the adjust/move axes: holding a direction should walk
                // a 900-row list and sweep a 2000-step range, not step once per press.
                KeyEvent.KEYCODE_DPAD_UP -> { if (down) editor.move(0, -1); return true }
                KeyEvent.KEYCODE_DPAD_DOWN -> { if (down) editor.move(0, 1); return true }
                KeyEvent.KEYCODE_DPAD_LEFT -> { if (down) editor.move(-1, 0); return true }
                KeyEvent.KEYCODE_DPAD_RIGHT -> { if (down) editor.move(1, 0); return true }
                KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
                KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER -> {
                    if (down && event.repeatCount == 0) editor.confirm()
                    return true
                }
                KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK -> {
                    if (down && event.repeatCount == 0) editor.back()
                    return true
                }
                // Swallow the rest: nothing behind this screen may act on a stray button.
                else -> return true
            }
        }
        // Memory-card dialog (opened from the library). Touch mode blocks Compose
        // D-pad focus, so it's driven by the manual nav model (same as the
        // settings tabs). Any direction steps the control list; A activates; B closes.
        if (com.armsx2.ui.MemoryCardManager.visible.value) {
            val nav = com.armsx2.ui.settings.SettingsControllerNav
            if (event.action == KeyEvent.ACTION_DOWN)
                android.util.Log.d("ARMSX2_MCNAV", "key kc=$kc (${KeyEvent.keyCodeToString(kc)}) repeat=${event.repeatCount}")
            when (kc) {
                KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BACK -> {
                    if (event.action == KeyEvent.ACTION_DOWN)
                        com.armsx2.ui.MemoryCardManager.visible.value = false
                    return true
                }
                KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_DPAD_CENTER,
                KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        nav.confirm()
                    return true
                }
                KeyEvent.KEYCODE_DPAD_UP -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        nav.moveSpatial(0, -1)
                    return true
                }
                KeyEvent.KEYCODE_DPAD_DOWN -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        nav.moveSpatial(0, 1)
                    return true
                }
                KeyEvent.KEYCODE_DPAD_LEFT -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        nav.moveSpatial(-1, 0)
                    return true
                }
                KeyEvent.KEYCODE_DPAD_RIGHT -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        nav.moveSpatial(1, 0)
                    return true
                }
                else -> return super.dispatchKeyEvent(event)
            }
        }
        if (WindowImpl.overlayVisible.value) {
            // Pause menu two-zone nav (EmulationMenuInputController): the left tab
            // column is walked with Up/Down (Right steps into the content pane); the
            // content pane's controls are SettingsControllerNav registry items that
            // move()/confirm() drive. L1/R1 cycle tabs from anywhere; Y jumps to the
            // Options tab; B backs out (content → tabs → resume). Edge-triggered.
            val emu = com.armsx2.ui.emulation.EmulationMenuInputController
            val down = event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0
            val handled = when (kc) {
                KeyEvent.KEYCODE_DPAD_LEFT -> { if (down) emu.move(-1, 0); true }
                KeyEvent.KEYCODE_DPAD_RIGHT -> { if (down) emu.move(1, 0); true }
                KeyEvent.KEYCODE_DPAD_UP -> { if (down) emu.move(0, -1); true }
                KeyEvent.KEYCODE_DPAD_DOWN -> { if (down) emu.move(0, 1); true }
                KeyEvent.KEYCODE_BUTTON_L1 -> { if (down) emu.tab(-1); true }
                KeyEvent.KEYCODE_BUTTON_R1 -> { if (down) emu.tab(1); true }
                KeyEvent.KEYCODE_BUTTON_Y -> { if (down) emu.open(com.armsx2.ui.emulation.EmulationMenuTab.Options); true }
                KeyEvent.KEYCODE_BUTTON_A,
                KeyEvent.KEYCODE_DPAD_CENTER,
                KeyEvent.KEYCODE_ENTER,
                KeyEvent.KEYCODE_NUMPAD_ENTER -> { if (down) emu.confirm(); true }
                KeyEvent.KEYCODE_BUTTON_B,
                KeyEvent.KEYCODE_BACK -> { if (down) emu.back(); true }
                else -> false
            }
            if (handled) return true
        }
        if (controllerDrivesFrontend()) {
            // Library COVER grid/list/shelf gets the dedicated data-driven spatial
            // model (HomeInputController). It must yield when something is layered
            // ON TOP of the library — the nav drawer, an in-game manager screen, or
            // the pause overlay — so those own the pad instead of the grid behind.
            if (!WindowImpl.overlayVisible.value &&
                WindowImpl.inGameScreen.value == null &&
                !com.armsx2.navigation.UiNavigator.drawerOpen.value &&
                com.armsx2.ui.home.HomeInputController.active()
            ) {
                // Square button (or the Menu hotkey) opens settings for the
                // highlighted cover — the controller equivalent of long-press.
                if (ControllerMappings.hotkeyFor(kc) == ControllerMappings.SysHotkey.MENU ||
                    kc == KeyEvent.KEYCODE_BUTTON_X
                ) {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        com.armsx2.ui.home.HomeInputController.openSelectedSettings()
                    return true
                }
                // #267: Y (Triangle) opens the library SEARCH — the requested
                // single-button access. While the panel is open Y is swallowed
                // (the panel owns nav; B closes it).
                if (kc == KeyEvent.KEYCODE_BUTTON_Y) return true
                // Shoulder buttons drive the touch-only toolbar toggles so the
                // whole library is controller-reachable: R1 cycles the view mode
                // (Grid → List → Shelf — "all 3 modes"), L1 cycles the sort order.
                if (kc == KeyEvent.KEYCODE_BUTTON_R1) {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        com.armsx2.ui.home.HomeInputController.cycleLayout()
                    return true
                }
                if (kc == KeyEvent.KEYCODE_BUTTON_L1) {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0)
                        com.armsx2.ui.home.HomeInputController.cycleSort()
                    return true
                }
                val handled = when (kc) {
                    KeyEvent.KEYCODE_DPAD_LEFT -> event.action != KeyEvent.ACTION_DOWN || run {
                        if (event.repeatCount == 0) {
                            val now = SystemClock.uptimeMillis()
                            if (!shouldSuppressUiNav(kc, fromAxis = false, now)) {
                                recordUiNav(kc, fromAxis = false)
                                com.armsx2.ui.home.HomeInputController.move(-1, 0)
                            }
                        }
                        true
                    }
                    KeyEvent.KEYCODE_DPAD_RIGHT -> event.action != KeyEvent.ACTION_DOWN || run {
                        if (event.repeatCount == 0) {
                            val now = SystemClock.uptimeMillis()
                            if (!shouldSuppressUiNav(kc, fromAxis = false, now)) {
                                recordUiNav(kc, fromAxis = false)
                                com.armsx2.ui.home.HomeInputController.move(1, 0)
                            }
                        }
                        true
                    }
                    KeyEvent.KEYCODE_DPAD_UP -> event.action != KeyEvent.ACTION_DOWN || run {
                        if (event.repeatCount == 0) {
                            val now = SystemClock.uptimeMillis()
                            if (!shouldSuppressUiNav(kc, fromAxis = false, now)) {
                                recordUiNav(kc, fromAxis = false)
                                com.armsx2.ui.home.HomeInputController.move(0, -1)
                            }
                        }
                        true
                    }
                    KeyEvent.KEYCODE_DPAD_DOWN -> event.action != KeyEvent.ACTION_DOWN || run {
                        if (event.repeatCount == 0) {
                            val now = SystemClock.uptimeMillis()
                            if (!shouldSuppressUiNav(kc, fromAxis = false, now)) {
                                recordUiNav(kc, fromAxis = false)
                                com.armsx2.ui.home.HomeInputController.move(0, 1)
                            }
                        }
                        true
                    }
                    KeyEvent.KEYCODE_BUTTON_A,
                    KeyEvent.KEYCODE_DPAD_CENTER,
                    KeyEvent.KEYCODE_ENTER,
                    KeyEvent.KEYCODE_NUMPAD_ENTER -> event.action != KeyEvent.ACTION_DOWN ||
                        com.armsx2.ui.home.HomeInputController.confirm()
                    KeyEvent.KEYCODE_BUTTON_B,
                    KeyEvent.KEYCODE_BACK -> event.action != KeyEvent.ACTION_DOWN ||
                        com.armsx2.ui.home.HomeInputController.back()
                    else -> false
                }
                if (handled) return true
            }
            // Everything else layered over/instead of the game — the nav drawer,
            // an in-game manager/Save-Load/settings screen, a library sub-screen,
            // and every root manager/settings screen — is driven through the manual
            // SettingsControllerNav REGISTRY, the exact model the memory-card dialog
            // and the settings rows already use. Compose's own focus system is NOT
            // usable here: the game view is an embedded SurfaceView, so the Compose
            // tree never reliably holds Android focus and synthetic D-pad keys go
            // nowhere. Instead every navigable control on these screens registers
            // itself via Modifier.controllerFocusable(id, onConfirm, onLeft, onRight)
            // (position tracked by onGloballyPositioned), and here we step the
            // registry directly: Up/Down move between rows, Left/Right adjust the
            // focused control's value (falling back to horizontal move when it has
            // no adjust action, e.g. the memcard's Slot 1 / Slot 2), A confirms, B
            // dismisses the topmost surface.
            val nav = com.armsx2.ui.settings.SettingsControllerNav
            when (kc) {
                KeyEvent.KEYCODE_DPAD_UP -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) nav.moveSpatial(0, -1)
                    return true
                }
                KeyEvent.KEYCODE_DPAD_DOWN -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) nav.moveSpatial(0, 1)
                    return true
                }
                KeyEvent.KEYCODE_DPAD_LEFT -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                        if (!nav.adjust(-1)) nav.moveSpatial(-1, 0)
                    }
                    return true
                }
                KeyEvent.KEYCODE_DPAD_RIGHT -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) {
                        if (!nav.adjust(1)) nav.moveSpatial(1, 0)
                    }
                    return true
                }
                KeyEvent.KEYCODE_BUTTON_A,
                KeyEvent.KEYCODE_DPAD_CENTER,
                KeyEvent.KEYCODE_ENTER,
                KeyEvent.KEYCODE_NUMPAD_ENTER -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) nav.confirm()
                    return true
                }
                KeyEvent.KEYCODE_BUTTON_B,
                KeyEvent.KEYCODE_BACK -> {
                    if (event.action == KeyEvent.ACTION_DOWN && event.repeatCount == 0) handleFrontendBack()
                    return true
                }
            }
        }
        // Runtime: bound system hotkeys. Caught here so back-button bindings work
        // (and aren't eaten by the back handler).
        if (eState.value == EmuState.RUNNING && !controllerDrivesFrontend()) {
            val down = event.action == KeyEvent.ACTION_DOWN
            // Combo-aware: on key-up the main key is already out of heldKeys, so
            // re-add it for the match (FAST_FORWARD needs to recognise its own
            // release). heldKeys still carries the modifier either way.
            val matchKeys = if (down) heldKeys else heldKeys + kc
            when (ControllerMappings.matchHotkey(kc, matchKeys)) {
                // Pressure modifier is a hold, handled (and consumed) earlier in
                // dispatchKeyEvent; it never reaches this one-shot action switch.
                ControllerMappings.SysHotkey.PRESSURE_MOD -> {}
                ControllerMappings.SysHotkey.MENU -> {
                    if (down) InGameOverlay.toggle()
                    return true
                }
                ControllerMappings.SysHotkey.SAVE_STATE -> {
                    if (down) {
                        val slot = currentSaveSlot.value
                        kotlin.concurrent.thread { runCatching { NativeApp.saveStateToSlot(slot) } }
                    }
                    return true
                }
                ControllerMappings.SysHotkey.LOAD_STATE -> {
                    if (down) {
                        val slot = currentSaveSlot.value
                        kotlin.concurrent.thread { runCatching { NativeApp.loadStateFromSlot(slot) } }
                    }
                    return true
                }
                ControllerMappings.SysHotkey.CYCLE_SLOT -> {
                    if (down) cycleSaveSlot()
                    return true
                }
                ControllerMappings.SysHotkey.TEXTURE_DUMP -> {
                    if (down) {
                        val on = runCatching { NativeApp.toggleTextureDumping() }.getOrDefault(false)
                        android.widget.Toast.makeText(
                            this,
                            if (on) "Texture dumping ON" else "Texture dumping OFF",
                            android.widget.Toast.LENGTH_SHORT,
                        ).show()
                    }
                    return true
                }
                ControllerMappings.SysHotkey.TOGGLE_OSD -> {
                    if (down && event.repeatCount == 0) InGameOverlay.toggleOsd()
                    return true
                }
                ControllerMappings.SysHotkey.GYRO_TOGGLE -> {
                    if (down && event.repeatCount == 0) toggleGyro()
                    return true
                }
                ControllerMappings.SysHotkey.GYRO_HOLD -> {
                    // "Only while aiming": gyro is live only while the button is held.
                    // Same shape as the FAST_FORWARD hold — act on both edges, ignore
                    // auto-repeat. No toast: it would fire on every aim.
                    if (event.action == KeyEvent.ACTION_DOWN || event.action == KeyEvent.ACTION_UP) {
                        if (event.repeatCount == 0) gyroActive.value = down
                    }
                    return true
                }
                ControllerMappings.SysHotkey.FAST_FORWARD -> {
                    // Hold to fast-forward (Turbo), release to return to the user's
                    // current limiter mode (Nominal if frame-limit is on, else Unlimited)
                    // — not blindly Nominal, which would re-enable a disabled limiter.
                    if (event.action == KeyEvent.ACTION_DOWN || event.action == KeyEvent.ACTION_UP) {
                        if (event.repeatCount == 0) {
                            // Holding FF supersedes any latched FF-toggle.
                            if (down) fastForwardToggleActive = false
                            runCatching { NativeApp.speedhackLimitermode(if (down) 1 else baseLimiterMode()) }
                        }
                    }
                    return true
                }
                ControllerMappings.SysHotkey.FAST_FORWARD_TOGGLE -> {
                    // Press once to lock fast-forward (Turbo) on, press again to return
                    // to the user's current limiter mode. Shared with the on-screen
                    // fast-forward touch button (FastForwardWidget).
                    if (down && event.repeatCount == 0) toggleFastForward()
                    return true
                }
                ControllerMappings.SysHotkey.SLOW_DOWN -> {
                    if (down && event.repeatCount == 0) toggleSlowDown()
                    return true
                }
                ControllerMappings.SysHotkey.RES_UP -> {
                    if (down) stepResolution(1)
                    return true
                }
                ControllerMappings.SysHotkey.RES_DOWN -> {
                    if (down) stepResolution(-1)
                    return true
                }
                ControllerMappings.SysHotkey.ACHIEVEMENTS -> {
                    if (down) com.armsx2.ui.emulation.EmulationMenuInputController.open(com.armsx2.ui.emulation.EmulationMenuTab.Options)
                    return true
                }
                ControllerMappings.SysHotkey.CLOSE_GAME -> {
                    if (down) closeGame()
                    return true
                }
                ControllerMappings.SysHotkey.QUIT_APP -> {
                    // Stop the VM (flushes memcards/savestate), then finish the app once
                    // the VM has fully unwound — never finish inline (stop() is async).
                    if (down) { quitAfterStop = true; stop()
                    }
                    return true
                }
                ControllerMappings.SysHotkey.SAVE_AND_EXIT -> {
                    // Write an autosave state, THEN close the game — the frontend "exit"
                    // case (Cocoon/ES-DE) that returns to the launcher without losing
                    // progress. closeGame() applies the exit-to-launcher opt-in.
                    if (down) closeGame(saveAutosave = true)
                    return true
                }
                ControllerMappings.SysHotkey.RESET_GAME -> {
                    if (down) restart()
                    return true
                }
                null -> {}
            }
        }
        return super.dispatchKeyEvent(event)
    }

    /** #254: forward a hardware keyboard KeyEvent to the emulated USB keyboard.
     *  Returns true (event consumed) only when the game runs with the USB
     *  keyboard attached, the event comes from a real keyboard, no menu/overlay
     *  or binding capture is active, and the native side accepted the key (i.e.
     *  it mapped to a HID usage). Otherwise returns false so the event keeps
     *  flowing to the normal pad / hotkey / nav handling. */
    private fun forwardKeyToUsbKeyboard(event: KeyEvent, kc: Int): Boolean {
        if (!usbKeyboardActive) return false
        if (eState.value != EmuState.RUNNING) return false
        // Don't steal keys the frontend/menus need for navigation, or while
        // (re)binding a pad button / hotkey.
        if (controllerDrivesFrontend()) return false
        if (com.armsx2.ui.MemoryCardManager.visible.value) return false
        if (ControllerMappings.padCapturing.value ||
            ControllerMappings.captureHotkey.value != null) return false
        // Must be a real keyboard key. SOURCE_KEYBOARD is set for hardware/BT
        // keyboards; gamepad buttons (SOURCE_GAMEPAD) share some keyCodes (the
        // D-pad arrows) so require the keyboard source and reject anything that
        // also claims to be a gamepad/joystick, keeping pad input on its own path.
        if (!event.isFromSource(InputDevice.SOURCE_KEYBOARD)) return false
        if (event.isFromSource(InputDevice.SOURCE_GAMEPAD) ||
            event.isFromSource(InputDevice.SOURCE_JOYSTICK)) return false
        if (kc == KeyEvent.KEYCODE_UNKNOWN) return false
        // Never divert the system Back/Home keys into the emulated keyboard —
        // the user still needs Back to open the overlay / leave the game.
        if (kc == KeyEvent.KEYCODE_BACK || kc == KeyEvent.KEYCODE_HOME) return false
        val pressed = when (event.action) {
            KeyEvent.ACTION_DOWN -> true
            KeyEvent.ACTION_UP -> false
            else -> return false // MULTIPLE etc. — ignore
        }
        return runCatching {
            NativeApp.usbKeyboardKey(0, kc, pressed)
        }.getOrDefault(false)
    }

    /** Cycle the active quick save/load slot 0→9→0 with a brief on-screen note. */
    /** The limiter mode that fast-forward should fall back to when it ends:
     *  Nominal (0) when the frame limiter is on, Unlimited (3) when the user has
     *  turned it off. Mirrors the frame-limit toggle so the two stay in sync. */
    private fun baseLimiterMode(): Int =
        if (InGameOverlay.frameLimitOn.value) 0 else 3

    /** Toggle locked fast-forward (Turbo) on/off — shared by the FAST_FORWARD_TOGGLE
     *  hotkey and the on-screen fast-forward touch button (FastForwardWidget). Restores
     *  the user's base limiter mode when turning off so it stays in sync with the
     *  frame-limit toggle. */
    /** Flip the runtime gyro enable (issue #337). Shared by the GYRO_TOGGLE hotkey and the
     *  edge-triggered (stick/combo) path. Only silences the sensor for this session — the
     *  user's Gyro Mode setting is untouched, so re-enabling restores their configured mode. */
    private fun toggleGyro() {
        val on = !gyroActive.value
        gyroActive.value = on
        hotkeyToast(if (on) "Gyro ON" else "Gyro OFF")
    }

    fun toggleFastForward() {
        fastForwardToggleActive = !fastForwardToggleActive
        val on = fastForwardToggleActive
        // Fast-forward supersedes an active slow-down latch (mutually exclusive).
        if (on) slowDownToggleActive = false
        runCatching { NativeApp.speedhackLimitermode(if (on) 1 else baseLimiterMode()) }
        hotkeyToast(if (on) "Fast Forward ON" else "Fast Forward OFF")
    }

    /** Toggle slow motion (native LimiterModeType::Slomo, ~50% speed). BLOCKED in
     *  RetroAchievements hardcore — slow-mo is a banned advantage there (matching
     *  desktop PCSX2's hardcore restrictions); shows a notice instead of engaging. */
    fun toggleSlowDown() {
        if (InGameOverlay.hardcoreOn.value) {
            slowDownToggleActive = false
            hotkeyToast("Slow Down is disabled in RetroAchievements Hardcore mode")
            return
        }
        slowDownToggleActive = !slowDownToggleActive
        val on = slowDownToggleActive
        // Slow-down supersedes an active fast-forward latch (mutually exclusive).
        if (on) fastForwardToggleActive = false
        runCatching { NativeApp.speedhackLimitermode(if (on) 2 else baseLimiterMode()) }
        hotkeyToast(if (on) "Slow Down ON (50%)" else "Slow Down OFF")
    }

    // Hotkey pop-up toasts (Fast-Forward, etc.). Android Toasts QUEUE, so toggling a
    // hotkey rapidly stacks a long backlog that blocks the screen — cancel the previous
    // one before showing the next so only the latest shows. Honors "ui.hotkeyToasts"
    // (default on) so they can be silenced entirely.
    private var lastHotkeyToast: android.widget.Toast? = null
    private fun hotkeyToast(text: String) {
        if (!prefs.getBoolean("ui.hotkeyToasts", true)) return
        lastHotkeyToast?.cancel()
        lastHotkeyToast = android.widget.Toast.makeText(this, text, android.widget.Toast.LENGTH_SHORT)
            .also { it.show() }
    }

    /** Quick save / load to the active slot — shared by the SAVE_STATE/LOAD_STATE
     *  hotkeys and the on-screen Save/Load State touch buttons. Runs off the UI thread. */
    fun saveState() {
        val slot = currentSaveSlot.value
        kotlin.concurrent.thread { runCatching { NativeApp.saveStateToSlot(slot) } }
    }

    fun loadState(onLoaded: (() -> Unit)? = null) {
        val slot = currentSaveSlot.value
        kotlin.concurrent.thread {
            runCatching { NativeApp.loadStateFromSlot(slot) }
            // Resume/dismiss only AFTER the load lands. The caller used to resume
            // immediately, which raced the async load (the menu resumed the VM before
            // the state was restored) — that's why "Load" appeared to do nothing.
            onLoaded?.let { cb -> android.os.Handler(android.os.Looper.getMainLooper()).post(cb) }
        }
    }

    private fun cycleSaveSlot() {
        val next = (currentSaveSlot.value + 1) % 10
        currentSaveSlot.value = next
        android.widget.Toast.makeText(this, "Save slot $next", android.widget.Toast.LENGTH_SHORT).show()
    }

    /** Step the internal resolution multiplier up/down (1x..5x), apply live, and
     *  persist to the running game's tier — per-game when it has a serial — so it
     *  survives a reboot without bleeding into other titles. */
    private fun stepResolution(dir: Int) {
        val next = (upscale.value.toInt() + dir).coerceIn(1, 8)
        val nf = next.toFloat()
        upscale.value = nf
        runCatching { NativeApp.renderUpscalemultiplier(nf) }
        runCatching {
            val serial = currentGame.value?.serial?.takeIf { it.isNotBlank() }
            val resolved = com.armsx2.config.ConfigStore.resolveForGame(serial)
            com.armsx2.config.ConfigStore.save(
                if (serial != null) com.armsx2.config.SettingsScope.Game
                else com.armsx2.config.SettingsScope.Global,
                serial,
                resolved.copy(upscaleFloat = nf),
            )
        }
        android.widget.Toast.makeText(this, "Resolution ${next}x", android.widget.Toast.LENGTH_SHORT).show()
    }

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
        // While (re)binding a pad button or a hotkey, the physical D-pad on many
        // handhelds (AYN Odin 3, RP6, etc.) arrives HERE as a HAT *axis*, never as
        // a key in dispatchKeyEvent — so the capture (which only listens for key
        // events) never saw it, and the HAT instead navigated the settings UI. When
        // a capture is armed, translate the HAT direction into a synthetic D-pad
        // KeyEvent and route it through dispatchKeyEvent (which reaches both the pad
        // capture in Compose and the hotkey capture in dispatchKeyEvent), and
        // consume the motion so nothing navigates.
        if (ControllerMappings.padCapturing.value || ControllerMappings.captureHotkey.value != null) {
            return handleCaptureMotion(ev)
        }
        captureHatX = 0
        captureHatY = 0
        if (captureHeldSynth.isNotEmpty()) {
            // Capture ended while a synthetic direction was still "held": no UP was
            // ever dispatched for it, so also purge it from heldKeys or a stale
            // direction would satisfy combo-modifier checks forever after.
            heldKeys.removeAll(captureHeldSynth)
            captureHeldSynth.clear()
        }
        if (com.armsx2.ui.MemoryCardManager.visible.value) {
            handleMemcardControllerMotion(ev)
            return true
        }
        if (controllerDrivesFrontend() && handleControllerUiMotion(ev)) {
            return true
        }
        if (eState.value == EmuState.RUNNING) {
            // Only true gamepad/joystick motion drives the PS2 pads. A DualSense's
            // touchpad/mouse node also emits generic motion (pointer AXIS_X/Y); reading
            // it as stick input injects garbage AND (via PadRouter) lets a non-pad node
            // grab a player slot — which pushed the real 2nd pad onto Player 1.
            if (!ev.isFromSource(InputDevice.SOURCE_JOYSTICK) &&
                !ev.isFromSource(InputDevice.SOURCE_GAMEPAD)) {
                return super.dispatchGenericMotionEvent(ev)
            }
            // SOURCE_TOUCHSCREEN motion events go through dispatchTouchEvent,
            // not here — generic motion is gamepad / mouse / stylus. So any
            // event reaching this method means a controller (or similar
            // pointing device) is being used; latch touch controls off.
            com.armsx2.ui.touch.TouchControls.onControllerInputDetected()
            // Local co-op: which PS2 port this physical device drives (P1=0 / P2=1).
            // Stick mode + CUSTOM binds are read per-player; emits route to `port`.
            val port = com.armsx2.input.PadRouter.portForDevice(ev.deviceId)
            // Analog sticks → analog (default) OR remapped to the D-pad / face
            // buttons (per ControllerMappings.{left,right}StickMode) — useful for
            // fighting games on analog-centric pads (e.g. left stick = D-pad).
            dispatchStick(ev, ControllerMappings.leftStickMode(port),
                MotionEvent.AXIS_X, MotionEvent.AXIS_Y,
                aXPos = 111, aXNeg = 113, aYPos = 112, aYNeg = 110, // L right/left, down/up
                leftStick = true, port = port)
            dispatchStick(ev, ControllerMappings.rightStickMode(port),
                MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ,
                aXPos = 121, aXNeg = 123, aYPos = 122, aYNeg = 120, // R right/left, down/up
                leftStick = false, port = port)
            // Fire any ARMSX2 hotkey bound (Hotkeys tab) to a stick DIRECTION — lets an
            // unused stick trigger Quick Save/Load etc. The stick still drives the pad.
            fireStickHotkeys(ev, port)
            // D-pad: the physical HAT *and* any stick remapped to D-pad drive the
            // same four PAD buttons. Combine every source and write each direction
            // once — otherwise the centered HAT released the stick-as-D-pad press
            // on the very same motion event (last write wins), so a stick set to
            // D-pad never registered while face-button mapping (different codes)
            // worked fine.
            dispatchDpadCombined(ev, port)
            // Analog triggers (L2/R2). Xbox / DualShock / most modern pads
            // report these as 0..1 motion-axis values, not Key.ButtonL2/R2
            // key events, so the onKeyEvent path above never sees them.
            // AXIS_LTRIGGER/RTRIGGER is the modern path; some controllers
            // (older Moga, certain BT mappings) report via AXIS_BRAKE/GAS
            // instead — take the max so we handle whichever the device
            // actually emits without double-driving when both are present.
            sendTrigger(ev, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE,
                KeyEvent.KEYCODE_BUTTON_L2, port)
            sendTrigger(ev, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS,
                KeyEvent.KEYCODE_BUTTON_R2, port)
            // Physical STICK DIRECTIONS bound to a PS2 control via the "(send)"
            // rows — e.g. R-Stick Down bound to send Square. The analog "(send)"
            // targets contribute to the merge layer like every other writer.
            dispatchStickDirBindings(ev, port)
            // Single write per analog code per event, merged across ALL writers.
            flushAnalogAxes(port)
            debugStickProbe(ev)
            return true
        }
        return super.dispatchGenericMotionEvent(ev)
    }

    // ---- Physical stick-direction → bound PS2 control ("(send)" rows) ------
    // The Pad tab's stick-target rows may be bound to ANY physical input; when the
    // physical side is a stick direction (reserved keycodes 1000-1007), keys never
    // fire for it in gameplay — this pass reads the axes each motion event and
    // drives the bound PS2 target: proportionally for an analog target (via the
    // merge layer), thresholded for a digital one (change-tracked per code so we
    // only write edges, like dispatchDpadCombined).
    private val stickDirDigitalHeld = Array(8) { HashSet<Int>() } // per unified pad slot (multitap)
    private fun dispatchStickDirBindings(ev: MotionEvent, port: Int) {
        for (left in booleanArrayOf(true, false)) {
            // Same axis correction the main dispatch applies (swap, then inverts).
            var vx = ev.getAxisValue(if (left) MotionEvent.AXIS_X else MotionEvent.AXIS_Z)
            var vy = ev.getAxisValue(if (left) MotionEvent.AXIS_Y else MotionEvent.AXIS_RZ)
            if (ControllerMappings.stickSwapXY(left)) { val t = vx; vx = vy; vy = t }
            if (ControllerMappings.stickInvertX(left)) vx = -vx
            if (ControllerMappings.stickInvertY(left)) vy = -vy
            for (dir in ControllerMappings.StickDir.values()) {
                val physCode = ControllerMappings.stickHotkeyKeyCode(left, dir)
                val target = ControllerMappings.targetForPhysical(physCode, port) ?: continue
                val mag = when (dir) {
                    ControllerMappings.StickDir.UP -> -vy
                    ControllerMappings.StickDir.DOWN -> vy
                    ControllerMappings.StickDir.LEFT -> -vx
                    ControllerMappings.StickDir.RIGHT -> vx
                }.coerceAtLeast(0f)
                if (target in 110..123) {
                    accumAnalog(target, shapeStickMag(mag, left))
                } else {
                    val held = stickDirDigitalHeld[port]
                    val on = mag > STICK_DIGITAL_THRESHOLD
                    val was = held.contains(target)
                    if (on != was) {
                        NativeApp.setPadButtonForPort(port, target, if (on) 32767 else 0, on)
                        if (on) held.add(target) else held.remove(target)
                    }
                }
            }
        }
    }

    // Rate-limited raw-axis probe for the right-stick diagonals report (Area 51:
    // camera moves only in a cross pattern on Android). OFF unless the tester sets
    // prefs boolean "debug.stickLog" true. Shows the raw axes, the corrected pair
    // and the shaped radial output in logcat + the exportable emulog.
    private var lastStickProbeMs = 0L
    private fun debugStickProbe(ev: MotionEvent) {
        if (!prefs.getBoolean("debug.stickLog", false)) return
        val now = SystemClock.uptimeMillis()
        if (now - lastStickProbeMs < 250) return
        lastStickProbeMs = now
        val z = ev.getAxisValue(MotionEvent.AXIS_Z)
        val rz = ev.getAxisValue(MotionEvent.AXIS_RZ)
        val rx = ev.getAxisValue(MotionEvent.AXIS_RX)
        val ry = ev.getAxisValue(MotionEvent.AXIS_RY)
        val mag = kotlin.math.hypot(z, rz)
        println("@@STICKPROBE@@ dev=${ev.deviceId} Z=%.3f RZ=%.3f RX=%.3f RY=%.3f mag=%.3f shaped=%.3f".format(
            z, rz, rx, ry, mag, shapeStickMag(mag.coerceAtMost(1f), false)))
    }

    // True whenever a Compose frontend surface is drawn over (or instead of) the
    // game and should own the gamepad. Every navigable surface must be listed
    // here or its D-pad/A/B never reach Compose. The four explicit surfaces cover
    // the in-game overlays (pause menu, Save/Load & manager screens, memcard
    // dialog, the library shown over a running game); when NO game is RUNNING the
    // whole app IS the frontend (root library + every manager/settings sub-screen
    // reached from the drawer), so the pad drives it unconditionally.
    private fun controllerDrivesFrontend(): Boolean =
        WindowImpl.overlayVisible.value ||
            WindowImpl.inGameScreen.value != null ||
            WindowImpl.showLibrary.value ||
            com.armsx2.ui.MemoryCardManager.visible.value ||
            eState.value != EmuState.RUNNING

    // B / BACK from any frontend surface EXCEPT the pause overlay, the memcard
    // dialog and the library cover grid (each consumes its own B earlier). Peels
    // the topmost layer: modal dialog > nav drawer > in-game manager screen >
    // library sub-route (Settings/Bios/... reached inside the in-game library) >
    // the library overlay itself > a root sub-route > (root Home) open the drawer.
    private fun handleFrontendBack() {
        val nav = com.armsx2.navigation.UiNavigator
        val onHome = nav.route.value == com.armsx2.navigation.AppRoute.Home
        when {
            nav.drawerOpen.value -> nav.drawerOpen.value = false
            WindowImpl.inGameScreen.value != null -> WindowImpl.dismissInGameScreen()
            WindowImpl.showLibrary.value && !onHome -> nav.back()
            WindowImpl.showLibrary.value -> WindowImpl.showLibrary.value = false
            !onHome -> nav.back()
            // Root library home with nothing above it: B opens the nav drawer
            // (mirrors the cover-grid B handled in HomeInputController.back()).
            else -> nav.drawerOpen.value = true
        }
    }

    // --- Controller menu nav hold-to-repeat ---------------------------------
    // The per-frame stick handlers below are edge-triggered (one move per push),
    // which makes holding a direction feel dead/clunky. While a direction is
    // held we run a repeat loop so the selection keeps travelling, matching
    // normal D-pad-menu behaviour.
    private var navRepeatJob: kotlinx.coroutines.Job? = null
    private var navRepeatDx = 0
    private var navRepeatDy = 0

    private fun directionKeyCode(dx: Int, dy: Int): Int = when {
        dx < 0 -> KeyEvent.KEYCODE_DPAD_LEFT
        dx > 0 -> KeyEvent.KEYCODE_DPAD_RIGHT
        dy < 0 -> KeyEvent.KEYCODE_DPAD_UP
        dy > 0 -> KeyEvent.KEYCODE_DPAD_DOWN
        else -> 0
    }

    private fun fireNavMove(dx: Int, dy: Int) {
        // Mirror the key-event routing priority so the analog stick drives every
        // surface the D-pad does.
        when {
            com.armsx2.ui.home.LibraryKeyboard.visible.value -> {
                // Controller search keyboard owns the stick/HAT/D-pad while it's up
                // (this is the RP6 path — its D-pad arrives here as a HAT axis).
                com.armsx2.ui.home.LibraryKeyboard.move(dx, dy)
            }
            com.armsx2.ui.settingshub.SettingsSearch.visible.value -> {
                // Settings-search result browse (keyboard dismissed): vertical list nav.
                if (dy != 0) com.armsx2.ui.settingshub.SettingsSearch.move(if (dy < 0) -1 else 1)
            }
            com.armsx2.ui.common.ShaderParamsEditor.visible -> {
                // THE path that matters for this editor: on this hardware the D-pad is a
                // HAT axis, so it arrives here and never as KEYCODE_DPAD_*. Missing this
                // is why vc1150's editor did nothing while the pause menu behind it moved.
                // Rides the shared hold-repeat, so a held direction walks the list and
                // sweeps a value.
                com.armsx2.ui.common.ShaderParamsEditor.move(dx, dy)
            }
            com.armsx2.ui.MemoryCardManager.visible.value -> {
                // Memcard dialog: 2D spatial nav (Slot 1 / Slot 2 / Delete across,
                // cards down). Driven by the hold-repeat job so a held direction
                // keeps moving.
                com.armsx2.ui.settings.SettingsControllerNav.moveSpatial(dx, dy)
            }
            WindowImpl.overlayVisible.value -> {
                // Pause menu — two-zone controller handles both the tab column and
                // the registry-driven content pane.
                com.armsx2.ui.emulation.EmulationMenuInputController.move(dx, dy)
            }
            // Library cover grid — only when it actually owns input (same gate as
            // the key path: not behind the drawer / an in-game screen).
            WindowImpl.inGameScreen.value == null &&
                !com.armsx2.navigation.UiNavigator.drawerOpen.value &&
                com.armsx2.ui.home.HomeInputController.active() -> {
                com.armsx2.ui.home.HomeInputController.move(dx, dy)
            }
            // Drawer, in-game manager/Save-Load screens, library sub-routes and
            // every root manager/settings screen: the manual registry (same as the
            // D-pad path). Left/Right adjust the focused control, else move.
            controllerDrivesFrontend() -> {
                val nav = com.armsx2.ui.settings.SettingsControllerNav
                if (dx != 0 && dy == 0) { if (!nav.adjust(dx)) nav.moveSpatial(dx, 0) }
                else nav.moveSpatial(dx, dy)
            }
            else -> {
                // Menu closed while a direction was held — stop repeating.
                stopNavRepeat()
            }
        }
    }

    private fun startNavRepeat(dx: Int, dy: Int) {
        if (dx == 0 && dy == 0) {
            stopNavRepeat()
            return
        }
        if (navRepeatJob?.isActive == true && navRepeatDx == dx && navRepeatDy == dy) return
        stopNavRepeat()
        navRepeatDx = dx
        navRepeatDy = dy
        fireNavMove(dx, dy)
        navRepeatJob = lifecycleScope.launch {
            kotlinx.coroutines.delay(NAV_REPEAT_INITIAL_MS)
            while (true) {
                fireNavMove(navRepeatDx, navRepeatDy)
                kotlinx.coroutines.delay(NAV_REPEAT_INTERVAL_MS)
            }
        }
    }

    private fun stopNavRepeat() {
        navRepeatJob?.cancel()
        navRepeatJob = null
        navRepeatDx = 0
        navRepeatDy = 0
    }

    private fun handleControllerUiMotion(ev: MotionEvent): Boolean {
        if (!ev.isFromSource(InputDevice.SOURCE_JOYSTICK) &&
            !ev.isFromSource(InputDevice.SOURCE_GAMEPAD)
        ) {
            return false
        }
        NativeApp.sRumbleDeviceId = ev.deviceId  // track active gamepad for rumble

        com.armsx2.ui.touch.TouchControls.onControllerInputDetected()
        return if (WindowImpl.overlayVisible.value) {
            handleOverlayControllerMotion(ev)
        } else {
            handleLibraryControllerMotion(ev)
        }
    }

    private fun handleLibraryControllerMotion(ev: MotionEvent): Boolean {
        val scrollY = uiScrollValue(ev.getAxisValue(MotionEvent.AXIS_RZ))
        handleControllerUiScroll(scrollY)

        // Accept BOTH the left stick and the D-pad (HAT axis on this hardware) so
        // handhelds with or without a stick can browse the library.
        val (stickDx, stickDy) = uiDominantStickDirection(
            ev.getAxisValue(MotionEvent.AXIS_X),
            ev.getAxisValue(MotionEvent.AXIS_Y),
        )
        val dx = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_X))
            .let { if (it != 0) it else stickDx }
        val dy = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_Y))
            .let { if (it != 0) it else stickDy }
        if (dx == 0 && dy == 0) {
            if (libraryAxisX != 0 || libraryAxisY != 0) stopNavRepeat()
            libraryAxisX = 0
            libraryAxisY = 0
            return true
        }

        if (dx != libraryAxisX || dy != libraryAxisY) {
            libraryAxisX = dx
            libraryAxisY = dy
            startNavRepeat(dx, dy)
        }
        return true
    }

    private fun handleOverlayControllerMotion(ev: MotionEvent): Boolean {
        // The overlay accepts BOTH the D-pad and the left analog stick, so
        // handhelds with or without a stick work. On this hardware the D-pad is a
        // HAT axis (not KEYCODE_DPAD_*); the stick is AXIS_X/Y. The adjust
        // skip/stuck bug was in the settings registry (now fixed), not the input
        // layer, so the stick is safe to use again. Right stick scrolls lists.
        handleControllerUiScroll(uiScrollValue(ev.getAxisValue(MotionEvent.AXIS_RZ)))

        val (stickDx, stickDy) = uiDominantStickDirection(
            ev.getAxisValue(MotionEvent.AXIS_X),
            ev.getAxisValue(MotionEvent.AXIS_Y),
        )
        val dirX = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_X))
            .let { if (it != 0) it else stickDx }
        val dirY = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_Y))
            .let { if (it != 0) it else stickDy }

        // Vertical = move between settings; horizontal = adjust the focused setting
        // (slider / segment). BOTH hold-to-repeat now — slider tweaks were previously
        // one-step-per-press, painful on long sliders (deadzone/sensitivity/etc.).
        // One repeat job at a time, so pick the dominant axis (vertical wins a tie);
        // returning to centre stops it. Toggle onLeft/onRight are idempotent (set
        // once then no-op), so repeating a held direction on a toggle is safe.
        when {
            dirY != 0 -> {
                if (dirY != overlayAxisY || overlayAxisX != 0) startNavRepeat(0, dirY)
                overlayAxisY = dirY
                overlayAxisX = 0
            }
            dirX != 0 -> {
                if (dirX != overlayAxisX || overlayAxisY != 0) startNavRepeat(dirX, 0)
                overlayAxisX = dirX
                overlayAxisY = 0
            }
            else -> {
                if (overlayAxisX != 0 || overlayAxisY != 0) stopNavRepeat()
                overlayAxisX = 0
                overlayAxisY = 0
            }
        }
        return true
    }

    private var memcardAxisX = 0
    private var memcardAxisY = 0

    /** Routes the controller stick / D-pad (HAT) to the memory-card dialog's
     *  manual nav (SettingsControllerNav). Touch mode kills Compose D-pad focus,
     *  so the dialog uses the same state-driven model as the settings tabs. Any
     *  direction steps the flat control list; edge-triggered (one move per push). */
    private fun handleMemcardControllerMotion(ev: MotionEvent) {
        val (stickDx, stickDy) = uiDominantStickDirection(
            ev.getAxisValue(MotionEvent.AXIS_X),
            ev.getAxisValue(MotionEvent.AXIS_Y),
        )
        val dirX = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_X))
            .let { if (it != 0) it else stickDx }
        val dirY = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_Y))
            .let { if (it != 0) it else stickDy }
        android.util.Log.d("ARMSX2_MCNAV",
            "motion hatX=${ev.getAxisValue(MotionEvent.AXIS_HAT_X)} hatY=${ev.getAxisValue(MotionEvent.AXIS_HAT_Y)} " +
                "stickX=${ev.getAxisValue(MotionEvent.AXIS_X)} stickY=${ev.getAxisValue(MotionEvent.AXIS_Y)} -> dirX=$dirX dirY=$dirY")
        // Hold-to-repeat 2D nav (one repeat job; vertical wins a diagonal tie),
        // mirroring the overlay so the card grid navigates freely in every direction.
        when {
            dirY != 0 -> {
                if (dirY != memcardAxisY || memcardAxisX != 0) startNavRepeat(0, dirY)
                memcardAxisY = dirY
                memcardAxisX = 0
            }
            dirX != 0 -> {
                if (dirX != memcardAxisX || memcardAxisY != 0) startNavRepeat(dirX, 0)
                memcardAxisX = dirX
                memcardAxisY = 0
            }
            else -> {
                if (memcardAxisX != 0 || memcardAxisY != 0) stopNavRepeat()
                memcardAxisX = 0
                memcardAxisY = 0
            }
        }
    }

    private fun handleControllerUiScroll(velocityY: Float) {
        if (WindowImpl.overlayVisible.value) {
            com.armsx2.ui.settings.SettingsControllerNav.setScrollVelocity(velocityY)
        } else if (com.armsx2.ui.home.HomeInputController.active()) {
            com.armsx2.ui.home.HomeInputController.scroll(velocityY)
        }
    }

    // Last HAT direction seen during an active capture, so a held D-pad binds once
    // (on the neutral→direction transition) instead of repeating. Reset to 0 on any
    // non-capture motion event so each capture session starts fresh.
    private var captureHatX = 0
    private var captureHatY = 0

    /** During a pad/hotkey (re)bind, turn HAT-axis D-pad presses and firm stick
     *  pushes into synthetic KeyEvents routed through the normal capture path.
     *  Always consumes the motion so the D-pad/stick can't navigate the UI while
     *  capturing.
     *
     *  HELD-STATE MODEL (stick/D-pad + button combos): each engaged direction
     *  dispatches a synthetic DOWN when it engages and a synthetic UP only when it
     *  RELEASES — mirroring a real button. The old code fired DOWN+UP instantly,
     *  which (a) finalized every capture as a single-key bind the moment a stick
     *  moved ("the moment you hold the stick it registers just the stick"), and
     *  (b) made a direction unusable as a combo member (the zero eventTime of the
     *  bare KeyEvent constructor failed the combo anti-ghost gap check). Synthetic
     *  events now carry real uptimeMillis timestamps, so hold-direction-then-press-
     *  button and hold-button-then-push-direction both bind combos, and a push
     *  released with nothing else still binds the plain single direction. */
    private val captureHeldSynth = HashSet<Int>()
    private fun handleCaptureMotion(ev: MotionEvent): Boolean {
        // Desired engaged-direction set for this event: at most one per HAT axis
        // pair and one per stick (dominant direction), so sweeping through a
        // diagonal can't spuriously bind a two-direction combo.
        val want = HashSet<Int>()
        val dx = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_X))
        val dy = uiHatDirection(ev.getAxisValue(MotionEvent.AXIS_HAT_Y))
        if (dx != 0) want.add(if (dx > 0) KeyEvent.KEYCODE_DPAD_RIGHT else KeyEvent.KEYCODE_DPAD_LEFT)
        if (dy != 0) want.add(if (dy > 0) KeyEvent.KEYCODE_DPAD_DOWN else KeyEvent.KEYCODE_DPAD_UP)
        captureStickCode(ev, MotionEvent.AXIS_X, MotionEvent.AXIS_Y, true).takeIf { it != 0 }?.let { want.add(it) }
        captureStickCode(ev, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ, false).takeIf { it != 0 }?.let { want.add(it) }
        captureHatX = dx
        captureHatY = dy
        val now = SystemClock.uptimeMillis()
        // Releases first (a direction that flipped is an UP then a DOWN).
        val released = captureHeldSynth.filter { it !in want }
        for (code in released) {
            captureHeldSynth.remove(code)
            // Re-enter dispatchKeyEvent (not super) so it reaches the hotkey
            // capture AND, while padCapturing, falls through to Compose's
            // onPreviewKeyEvent which records the pad bind.
            dispatchKeyEvent(KeyEvent(now, now, KeyEvent.ACTION_UP, code, 0))
        }
        for (code in want) {
            if (captureHeldSynth.add(code))
                dispatchKeyEvent(KeyEvent(now, now, KeyEvent.ACTION_DOWN, code, 0))
        }
        // Binding may have completed mid-loop (endHotkeyCapture); drop any held
        // state so the next capture session starts clean (incl. heldKeys, since no
        // UP will ever arrive for these synthetic codes).
        if (ControllerMappings.captureHotkey.value == null && !ControllerMappings.padCapturing.value) {
            heldKeys.removeAll(captureHeldSynth)
            captureHeldSynth.clear()
        }
        return true
    }

    /** The reserved hotkey keycode for whichever direction of the [left]/right stick is
     *  pushed past a firm threshold during capture, or 0 if centered. */
    private fun captureStickCode(ev: MotionEvent, axisX: Int, axisY: Int, left: Boolean): Int {
        val x = ev.getAxisValue(axisX)
        val y = ev.getAxisValue(axisY)
        val t = 0.7f
        return when {
            y <= -t -> ControllerMappings.stickHotkeyKeyCode(left, ControllerMappings.StickDir.UP)
            y >= t -> ControllerMappings.stickHotkeyKeyCode(left, ControllerMappings.StickDir.DOWN)
            x <= -t -> ControllerMappings.stickHotkeyKeyCode(left, ControllerMappings.StickDir.LEFT)
            x >= t -> ControllerMappings.stickHotkeyKeyCode(left, ControllerMappings.StickDir.RIGHT)
            else -> 0
        }
    }

    private fun uiHatDirection(value: Float): Int = when {
        value > UI_HAT_DEAD -> 1
        value < -UI_HAT_DEAD -> -1
        else -> 0
    }

    private fun uiDominantStickDirection(x: Float, y: Float): Pair<Int, Int> {
        val absX = abs(x)
        val absY = abs(y)
        if (absX < UI_NAV_DEAD && absY < UI_NAV_DEAD)
            return 0 to 0
        return if (absX >= absY)
            (if (x > 0f) 1 else -1) to 0
        else
            0 to (if (y > 0f) 1 else -1)
    }

    private fun uiAxisDirection(value: Float): Int = when {
        value > UI_NAV_DEAD -> 1
        value < -UI_NAV_DEAD -> -1
        else -> 0
    }

    private fun uiScrollValue(value: Float): Float {
        val dead = 0.18f
        return when {
            value > dead -> ((value - dead) / (1f - dead)).coerceIn(0f, 1f)
            value < -dead -> ((value + dead) / (1f - dead)).coerceIn(-1f, 0f)
            else -> 0f
        }
    }

    private fun recordUiNav(keyCode: Int, fromAxis: Boolean) {
        lastUiNavCode = keyCode
        lastUiNavAt = SystemClock.uptimeMillis()
        lastUiNavWasAxis = fromAxis
    }

    private fun shouldSuppressUiNav(keyCode: Int, fromAxis: Boolean, now: Long): Boolean {
        if (lastUiNavCode != keyCode) return false
        val age = now - lastUiNavAt
        return lastUiNavWasAxis != fromAxis && age <= UI_KEY_AXIS_SUPPRESS_MS
    }

    private fun dispatchSyntheticUiKey(keyCode: Int): Boolean {
        val now = SystemClock.uptimeMillis()
        val flags = KeyEvent.FLAG_FROM_SYSTEM or KeyEvent.FLAG_VIRTUAL_HARD_KEY
        val source = InputDevice.SOURCE_KEYBOARD or InputDevice.SOURCE_DPAD
        val down = KeyEvent(
            now, now, KeyEvent.ACTION_DOWN, keyCode, 0, 0,
            KeyCharacterMap.VIRTUAL_KEYBOARD, 0, flags, source
        )
        val up = KeyEvent(
            now, now, KeyEvent.ACTION_UP, keyCode, 0, 0,
            KeyCharacterMap.VIRTUAL_KEYBOARD, 0, flags, source
        )
        val downHandled = super.dispatchKeyEvent(down)
        val upHandled = super.dispatchKeyEvent(up)
        return downHandled || upHandled
    }

    /** Apply the user's stick sensitivity (linear output scale) + acceleration
     *  (exponential response curve) to a post-deadzone magnitude in [0,1]. accel 0
     *  = linear; higher = finer control near center, faster toward full tilt. Only
     *  shapes real analog output (native passthrough + CUSTOM analog targets). */
    private fun shapeStickMag(m: Float, left: Boolean): Float {
        val dz = ControllerMappings.stickDeadzone(left)
        if (m <= dz) return 0f
        // Re-normalize the window [dz, 1-outer] to [0, 1] so output ramps smoothly
        // from 0 past the inner deadzone (no jump), and reaches FULL at (1-outer) —
        // the outer/anti-deadzone lets a short-throw stick that can't physically
        // reach its corners still hit 100%. Then apply the accel curve + sensitivity.
        // ALL feel tunables are PER-STICK now (left/right independent).
        val outer = ControllerMappings.stickOuterDeadzone(left)
        val hi = (1f - outer).coerceAtLeast(dz + 0.01f) // upper edge; guard hi > dz
        val t = ((m - dz) / (hi - dz)).coerceIn(0f, 1f)
        val accel = ControllerMappings.stickAcceleration(left)
        val curved =
            if (accel > 0f) Math.pow(t.toDouble(), (1f + accel).toDouble()).toFloat()
            else t
        val out = (curved * ControllerMappings.stickSensitivity(left)).coerceIn(0f, 1f)
        // Anti-deadzone (output floor): lift ANY non-zero output up to start at the floor,
        // so a game with its own large stick deadzone responds the instant the stick moves
        // and the rest of the travel maps proportionally above it (no dead bottom, no jump).
        // True center (out == 0) stays 0. 0 floor = unchanged behaviour.
        if (out <= 0f) return 0f
        val anti = ControllerMappings.stickAntiDeadzone(left)
        return if (anti > 0f) (anti + out * (1f - anti)).coerceIn(0f, 1f) else out
    }

    // ---- Analog-code merge layer (native codes 110-123) --------------------
    // Several writers can drive the SAME PS2 stick direction in one motion event:
    // the physical stick (ANALOG mode), a CUSTOM direction defaulting to analog,
    // the D-pad HAT fold, a trigger bound to a stick direction, and a stick
    // direction of the OTHER stick bound via the "(send)" rows. Before this layer
    // each writer set the code directly, so whichever wrote LAST (usually the
    // resting real stick, at 0) released everyone else's deflection — the same
    // clobber class as the old dispatchDpadCombined bug. Now every motion-event
    // writer CONTRIBUTES (max per code) and flushAnalogAxes writes each code once.
    // Button-held deflections (sendKeyAction: a KEY bound to an analog code, incl.
    // d-pad-as-left-stick key path) are tracked in [analogKeyHeld] and folded into
    // every flush so stick motion can no longer release a held button-deflection.
    private val analogAccum = HashMap<Int, Float>()
    private val analogPrevSent = Array(8) { HashMap<Int, Float>() } // per unified pad slot (multitap)
    val analogKeyHeld = Array(8) { HashMap<Int, Float>() } // written by sendKeyAction; per unified pad slot

    // ---- Gyro <-> physical-stick ADDITIVE combine (P1 / port 0) -----------
    // The aim/steer gyro drives a PS2 analog stick; so does the physical stick.
    // They used to clobber (raw setPadButton, last-writer-wins), so moving one
    // killed the other. Instead the gyro is folded in as a SIGNED addend on top
    // of the physical stick that shares its axis, then clamped to the unit circle
    // by accumStickRadial — coarse stick aim + fine gyro adjust AT ONCE. Both the
    // MotionEvent path and the sensor callback run on the main looper, so these
    // are read/written without extra locking (volatile documents the sharing).
    @Volatile private var gyroCombineActive = false   // gyro currently deflected
    @Volatile private var gyroCombineLeft = false     // gyro drives left(true)/right(false) stick
    @Volatile private var gyroVecX = 0f               // signed gyro contribution, [-1,1]
    @Volatile private var gyroVecY = 0f
    private val lastPhysStickX = floatArrayOf(0f, 0f) // [0]=left [1]=right, P1 physical analog
    private val lastPhysStickY = floatArrayOf(0f, 0f)

    private fun accumAnalog(code: Int, v: Float) {
        if (v <= 0f) return
        val cur = analogAccum[code] ?: 0f
        if (v > cur) analogAccum[code] = v
    }

    /** Write the merged analog codes for this motion event: union of the fresh
     *  contributions, the key-held deflections, and everything sent last event
     *  (so stale codes release exactly once). */
    private fun flushAnalogAxes(port: Int) {
        val prev = analogPrevSent[port]
        for ((code, held) in analogKeyHeld[port]) accumAnalog(code, held)
        // Release pass: codes we sent before but that have no contribution now.
        for (code in prev.keys) {
            if (!analogAccum.containsKey(code)) {
                NativeApp.setPadButtonForPort(port, code, 0, false)
            }
        }
        for ((code, v) in analogAccum) {
            if (prev[code] != v)
                NativeApp.setPadButtonForPort(port, code, (v * 32767).toInt(), true)
        }
        prev.clear()
        prev.putAll(analogAccum)
        analogAccum.clear()
    }

    /** RADIAL analog-stick shaping: deadzone/curve/sensitivity applied to the
     *  stick's radial magnitude (not per-axis), so diagonals shape identically to
     *  cardinals — a per-axis deadzone was a "square" zone that ate diagonals.
     *  Direction is preserved exactly; only the magnitude is reshaped. */
    private fun accumStickRadial(vx: Float, vy: Float, left: Boolean,
                                 aXPos: Int, aXNeg: Int, aYPos: Int, aYNeg: Int) {
        // Off-axis BLEED gate (fixes the "push up also presses right" regression). Moving
        // the deadzone from per-axis to radial (above) stopped diagonals being eaten, but
        // the old per-axis zone was also silently cleaning up small perpendicular values —
        // so a near-cardinal push on a stick that doesn't sit perfectly centered on the
        // other axis now leaks that value through. Restore the cleanup WITHOUT the square
        // zone: drop the minor axis only when it's a small fraction of the major one. That
        // snaps just very shallow (~<9°) diagonals to the cardinal; genuine diagonals (minor
        // axis well above STICK_CROSS_GATE of the major) are untouched, so 8-way is intact.
        var gx = vx
        var gy = vy
        val ax = abs(gx)
        val ay = abs(gy)
        if (ax >= ay) { if (ay < ax * STICK_CROSS_GATE) gy = 0f }
        else { if (ax < ay * STICK_CROSS_GATE) gx = 0f }
        val mag = kotlin.math.hypot(gx, gy)
        if (mag <= 0f) return
        val shaped = shapeStickMag(mag.coerceAtMost(1f), left)
        val scale = shaped / mag // preserves direction; caps square-gate diagonals at unit circle
        val ox = gx * scale
        val oy = gy * scale
        if (ox > 0f) accumAnalog(aXPos, ox) else if (ox < 0f) accumAnalog(aXNeg, -ox)
        if (oy > 0f) accumAnalog(aYPos, oy) else if (oy < 0f) accumAnalog(aYNeg, -oy)
    }

    /** Gyro (aim mode 1 / steer mode 2) as an ADDITIVE stick contributor. Called
     *  from the sensor callback on the main looper. [gx],[gy] are the signed,
     *  smoothed gyro vector in [-1,1]; (0,0) on settle/stop releases it. The gyro
     *  sums with whichever physical stick shares its axis (aim -> right, or the
     *  user-chosen left for RE4-style games; steer -> left) so coarse stick aim
     *  and fine gyro adjustment work together instead of clobbering each other. */
    fun onGyroAnalog(mode: Int, gx: Float, gy: Float) {
        gyroCombineLeft = mode == 2 ||
            (mode == 1 && ControllerMappings.gyroAimStick() == ControllerMappings.GYRO_STICK_LEFT)
        gyroVecX = gx; gyroVecY = gy
        gyroCombineActive = gx != 0f || gy != 0f
        emitCombinedSticks()
    }

    /** Re-drive BOTH P1 sticks from their last physical vector plus the gyro addend
     *  on the target side, then flush once. Re-contributing the NON-target stick is
     *  what stops flushAnalogAxes' release pass from dropping it when only the gyro
     *  moved (single owner of the analog codes = the shared merge layer). flush only
     *  writes codes whose value changed, so an unchanged stick costs nothing. */
    private fun emitCombinedSticks() {
        val gxL = if (gyroCombineLeft) gyroVecX else 0f
        val gyL = if (gyroCombineLeft) gyroVecY else 0f
        val gxR = if (gyroCombineLeft) 0f else gyroVecX
        val gyR = if (gyroCombineLeft) 0f else gyroVecY
        accumStickRadial(lastPhysStickX[0] + gxL, lastPhysStickY[0] + gyL, true,  111, 113, 112, 110)
        accumStickRadial(lastPhysStickX[1] + gxR, lastPhysStickY[1] + gyR, false, 121, 123, 122, 120)
        flushAnalogAxes(0)
    }

    /** Route one physical stick's two axes to the PS2 pad per [mode]: native analog
     *  stick (default), thresholded digital D-pad / face presses, or per-direction
     *  CUSTOM binds. [leftStick] selects which stick's CUSTOM binds to read. */
    private fun dispatchStick(
        event: MotionEvent, mode: ControllerMappings.StickMode,
        axisX: Int, axisY: Int,
        aXPos: Int, aXNeg: Int, aYPos: Int, aYNeg: Int,
        leftStick: Boolean, port: Int,
    ) {
        // Read the raw axis values once, then apply the per-stick axis correction
        // (swap X/Y first, then invert each) BEFORE any mode dispatch — so it fixes
        // pads that read rotated/mirrored ("down is up, left is right") in Analog,
        // Face and Custom modes alike.
        var vx = event.getAxisValue(axisX)
        var vy = event.getAxisValue(axisY)
        if (ControllerMappings.stickSwapXY(leftStick)) { val t = vx; vx = vy; vy = t }
        if (ControllerMappings.stickInvertX(leftStick)) vx = -vx
        if (ControllerMappings.stickInvertY(leftStick)) vy = -vy
        when (mode) {
            ControllerMappings.StickMode.ANALOG -> {
                // Radial shaping into the merge layer (flushAnalogAxes writes once
                // per event, after every contributor has been folded in).
                // P1 (port 0): remember this stick's PHYSICAL vector and, when the
                // gyro is driving THIS stick, sum the gyro's signed addend on top so
                // coarse stick aim + fine gyro adjust simultaneously (onGyroAnalog).
                // Stored value is pre-gyro so the sensor path can add gyro cleanly.
                var sx = vx; var sy = vy
                if (port == 0) {
                    val si = if (leftStick) 0 else 1
                    lastPhysStickX[si] = vx; lastPhysStickY[si] = vy
                    if (gyroCombineActive && gyroCombineLeft == leftStick) { sx += gyroVecX; sy += gyroVecY }
                }
                accumStickRadial(sx, sy, leftStick, aXPos, aXNeg, aYPos, aYNeg)
                if (leftStick && ControllerMappings.dpadAsLeftStick()) {
                    // Fold the physical D-pad (HAT) into the left stick so the
                    // D-pad drives analog movement — full deflection, unshaped
                    // (a d-pad press is digital). The HAT is gated out of
                    // dispatchDpadCombined while this is on.
                    val hx = event.getAxisValue(MotionEvent.AXIS_HAT_X)
                    val hy = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
                    if (hx > STICK_DEAD) accumAnalog(aXPos, hx) else if (hx < -STICK_DEAD) accumAnalog(aXNeg, -hx)
                    if (hy > STICK_DEAD) accumAnalog(aYPos, hy) else if (hy < -STICK_DEAD) accumAnalog(aYNeg, -hy)
                }
            }
            ControllerMappings.StickMode.FACE -> {
                sendAxisDigital(vx, posCode = 97, negCode = 99, port = port)  // Circle / Square (right/left)
                sendAxisDigital(vy, posCode = 96, negCode = 100, port = port) // Cross / Triangle (down/up)
            }
            ControllerMappings.StickMode.CUSTOM -> {
                // Each direction is bound to any PS2 button (per-player). D-pad targets
                // (19-22) are owned by dispatchDpadCombined() (avoids the release race);
                // emitCustom keeps analog targets proportional, others thresholded.
                emitCustom(ControllerMappings.customStickCode(leftStick, ControllerMappings.StickDir.RIGHT, port),
                    if (vx > 0f) vx else 0f, port, leftStick)
                emitCustom(ControllerMappings.customStickCode(leftStick, ControllerMappings.StickDir.LEFT, port),
                    if (vx < 0f) -vx else 0f, port, leftStick)
                emitCustom(ControllerMappings.customStickCode(leftStick, ControllerMappings.StickDir.DOWN, port),
                    if (vy > 0f) vy else 0f, port, leftStick)
                emitCustom(ControllerMappings.customStickCode(leftStick, ControllerMappings.StickDir.UP, port),
                    if (vy < 0f) -vy else 0f, port, leftStick)
            }
        }
    }

    // CUSTOM stick directions bound to an ARMSX2 hotkey are edge-triggered: this tracks
    // which hotkey codes are currently held past the threshold, per port, so each
    // crossing fires exactly once (re-armed on release).
    private val stickHotkeyHeld = Array(8) { HashSet<Int>() } // per unified pad slot (multitap)

    /** Fire any SysHotkey bound (Hotkeys tab) to a stick DIRECTION, edge-triggered. The
     *  stick still drives the pad, so this is meant for sticks/directions a game doesn't
     *  use. Reuses [stickHotkeyHeld] — the reserved 1000+ stick-hotkey keycodes don't
     *  collide with the Custom-mode 300+ codes also tracked there. */
    private fun fireStickHotkeys(ev: MotionEvent, port: Int) {
        fireStickHotkeyAxis(ev, MotionEvent.AXIS_X, MotionEvent.AXIS_Y, true, port)
        fireStickHotkeyAxis(ev, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ, false, port)
    }
    private fun fireStickHotkeyAxis(ev: MotionEvent, axisX: Int, axisY: Int, left: Boolean, port: Int) {
        val x = ev.getAxisValue(axisX)
        val y = ev.getAxisValue(axisY)
        val held = stickHotkeyHeld[port]
        val dirs = arrayOf(
            ControllerMappings.StickDir.UP to -y, ControllerMappings.StickDir.DOWN to y,
            ControllerMappings.StickDir.LEFT to -x, ControllerMappings.StickDir.RIGHT to x,
        )
        for ((dir, value) in dirs) {
            val code = ControllerMappings.stickHotkeyKeyCode(left, dir)
            if (value > STICK_DIGITAL_THRESHOLD) {
                // Mirror the held direction into heldKeys so it can serve as the
                // MODIFIER of a stick+button combo hotkey (dispatchKeyEvent's
                // matchHotkey consults heldKeys when the button arrives).
                heldKeys.add(code)
                if (held.add(code)) {
                    // Edge: fire a hotkey with this direction as its MAIN key —
                    // combo-aware (e.g. "hold Select + push R-Stick Up"), falling
                    // back to a plain single-direction binding.
                    ControllerMappings.matchHotkey(code, heldKeys)?.let { runStickHotkey(it) }
                }
            } else {
                heldKeys.remove(code)
                held.remove(code)
            }
        }
    }

    /** Fire an ARMSX2 hotkey from a non-key source (a CUSTOM stick direction crossing
     *  its threshold — edge-triggered, treated as a single press). Hold-type hotkeys
     *  (FAST_FORWARD hold, PRESSURE_MOD) are no-ops here — a stick edge has no hold
     *  semantics; the rest mirror the one-shot actions in dispatchKeyEvent. */
    private fun runStickHotkey(h: ControllerMappings.SysHotkey) {
        when (h) {
            ControllerMappings.SysHotkey.MENU -> InGameOverlay.toggle()
            ControllerMappings.SysHotkey.SAVE_STATE -> {
                val slot = currentSaveSlot.value
                kotlin.concurrent.thread { runCatching { NativeApp.saveStateToSlot(slot) } }
            }
            ControllerMappings.SysHotkey.LOAD_STATE -> {
                val slot = currentSaveSlot.value
                kotlin.concurrent.thread { runCatching { NativeApp.loadStateFromSlot(slot) } }
            }
            ControllerMappings.SysHotkey.CYCLE_SLOT -> cycleSaveSlot()
            ControllerMappings.SysHotkey.TEXTURE_DUMP -> {
                val on = runCatching { NativeApp.toggleTextureDumping() }.getOrDefault(false)
                android.widget.Toast.makeText(this,
                    if (on) "Texture dumping ON" else "Texture dumping OFF",
                    android.widget.Toast.LENGTH_SHORT).show()
            }
            ControllerMappings.SysHotkey.FAST_FORWARD_TOGGLE -> {
                fastForwardToggleActive = !fastForwardToggleActive
                val on = fastForwardToggleActive
                runCatching { NativeApp.speedhackLimitermode(if (on) 1 else baseLimiterMode()) }
                hotkeyToast(if (on) "Fast Forward ON" else "Fast Forward OFF")
            }
            ControllerMappings.SysHotkey.GYRO_TOGGLE -> toggleGyro()
            // GYRO_HOLD needs key up/down edges, which this edge-triggered path (stick
            // directions / combos) doesn't provide — behave as a toggle here rather than
            // latching gyro on with no release.
            ControllerMappings.SysHotkey.GYRO_HOLD -> toggleGyro()
            ControllerMappings.SysHotkey.RES_UP -> stepResolution(1)
            ControllerMappings.SysHotkey.RES_DOWN -> stepResolution(-1)
            ControllerMappings.SysHotkey.ACHIEVEMENTS -> com.armsx2.ui.emulation.EmulationMenuInputController.open(com.armsx2.ui.emulation.EmulationMenuTab.Options)
            ControllerMappings.SysHotkey.CLOSE_GAME -> closeGame()
            ControllerMappings.SysHotkey.QUIT_APP -> { quitAfterStop = true; stop()
            }
            ControllerMappings.SysHotkey.SAVE_AND_EXIT -> closeGame(saveAutosave = true)
            ControllerMappings.SysHotkey.RESET_GAME -> restart()
            ControllerMappings.SysHotkey.SLOW_DOWN -> toggleSlowDown()
            ControllerMappings.SysHotkey.TOGGLE_OSD -> InGameOverlay.toggleOsd()
            // Hold-type hotkeys have no one-shot stick-edge meaning.
            ControllerMappings.SysHotkey.FAST_FORWARD,
            ControllerMappings.SysHotkey.PRESSURE_MOD -> {}
        }
    }

    /** Emit one CUSTOM stick-direction binding given its 0..1 deflection [mag]
     *  toward that direction. D-pad codes (19-22) are skipped — dispatchDpadCombined
     *  owns them; analog codes (110-123) stay proportional; others are thresholded. */
    private fun emitCustom(code: Int, mag: Float, port: Int, srcLeft: Boolean) {
        // Bound to an ARMSX2 hotkey? Edge-trigger it (fire once on threshold crossing,
        // re-arm on release) instead of sending a PS2 button.
        ControllerMappings.hotkeyForStickCode(code)?.let { hk ->
            val held = stickHotkeyHeld[port]
            if (mag > STICK_DIGITAL_THRESHOLD) {
                if (held.add(code)) runStickHotkey(hk)
            } else {
                held.remove(code)
            }
            return
        }
        if (code in 19..22) return
        if (code in 110..123) {
            // Analog target: shape with the SOURCE stick's feel settings (the stick
            // being physically moved), and contribute to the merge layer instead of
            // writing directly, so a CUSTOM direction can't fight the other stick's
            // ANALOG writer (or a trigger/button bound to the same direction).
            val m = shapeStickMag(mag, srcLeft)
            accumAnalog(code, m)
        } else {
            NativeApp.setPadButtonForPort(port, code, 32767, mag > STICK_DIGITAL_THRESHOLD)
        }
    }

    /** Stick-as-button: press [posCode] / [negCode] once the axis passes the digital
     *  threshold. setPadButton is a state set, so re-sending the same state is a no-op. */
    // [v] is the already-corrected axis value (swap/invert applied by dispatchStick).
    private fun sendAxisDigital(v: Float, posCode: Int, negCode: Int, port: Int) {
        NativeApp.setPadButtonForPort(port, posCode, 32767, v > STICK_DIGITAL_THRESHOLD)
        NativeApp.setPadButtonForPort(port, negCode, 32767, v < -STICK_DIGITAL_THRESHOLD)
    }

    // D-pad codes (19-22) THIS function last pressed, so it releases only its own
    // presses. Owns the D-pad from ALL non-KeyEvent sources: the physical HAT, a
    // stick in DPAD mode, and any CUSTOM stick direction bound to a D-pad code.
    // PER-PORT (index = player) so P1 and P2 D-pad presses can't release each other.
    private val dpadOwnHeld = Array(8) { HashSet<Int>() } // per unified pad slot (multitap)

    /** True when any CUSTOM-mode stick has a direction bound to a D-pad code, for
     *  the given player. */
    private fun customTargetsDpad(port: Int): Boolean {
        for (isLeft in booleanArrayOf(true, false)) {
            if (ControllerMappings.stickModeFor(isLeft, port) != ControllerMappings.StickMode.CUSTOM) continue
            for (dir in ControllerMappings.StickDir.values())
                if (ControllerMappings.customStickCode(isLeft, dir, port) in 19..22) return true
        }
        return false
    }

    /** Drive the PS2 D-pad from every non-KeyEvent source that can map to it — the
     *  physical HAT, a stick in DPAD mode, and CUSTOM directions bound to a D-pad
     *  code — through ONE change-tracked owner. Writing all four codes every event
     *  (the old approach) released a held direction whenever the stick re-centered
     *  or another stick emitted an event; tracking our own presses avoids that and
     *  never clobbers a physical D-pad arriving as KeyEvents. */
    private fun dispatchDpadCombined(ev: MotionEvent, port: Int) {
        val held = dpadOwnHeld[port]
        // When the D-pad drives the left stick, the HAT is folded into the stick
        // in dispatchStick — ignore it here so it doesn't ALSO press the d-pad.
        val dpadAsStick = ControllerMappings.dpadAsLeftStick()
        val hatX = if (dpadAsStick) 0f else ev.getAxisValue(MotionEvent.AXIS_HAT_X)
        val hatY = if (dpadAsStick) 0f else ev.getAxisValue(MotionEvent.AXIS_HAT_Y)
        val hatActive = hatX != 0f || hatY != 0f
        // DPAD-mode sticks are now written directly in dispatchStick (self-healing,
        // like FACE). Do NOT fold them here, or the combined owner's change-tracked
        // release would fight the direct writer. The combined owner still handles
        // the physical HAT and CUSTOM directions bound to a d-pad code.
        val leftDpad = false
        val rightDpad = false
        // Nothing we own could be active → release what we hold and bail, so we
        // never touch the D-pad bits a KeyEvent-style physical D-pad drives.
        if (!hatActive && !leftDpad && !rightDpad && !customTargetsDpad(port)) {
            if (held.isNotEmpty()) {
                held.forEach { NativeApp.setPadButtonForPort(port, it, 0, false) }
                held.clear()
            }
            return
        }
        // HAT → D-pad only when that direction is still bound. The physical HAT never
        // flows through the keycode binding path (it arrives as a motion axis), so
        // clearing/reassigning a D-pad direction in the Pad tab was previously ignored
        // here. The custom-stick→D-pad fold below is a SEPARATE binding and stays
        // active even when the physical D-pad is cleared.
        val dpRightBound = ControllerMappings.targetForPhysical(KeyEvent.KEYCODE_DPAD_RIGHT, port) != null
        val dpLeftBound = ControllerMappings.targetForPhysical(KeyEvent.KEYCODE_DPAD_LEFT, port) != null
        val dpDownBound = ControllerMappings.targetForPhysical(KeyEvent.KEYCODE_DPAD_DOWN, port) != null
        val dpUpBound = ControllerMappings.targetForPhysical(KeyEvent.KEYCODE_DPAD_UP, port) != null
        var right = hatX > 0.5f && dpRightBound
        var left = hatX < -0.5f && dpLeftBound
        var down = hatY > 0.5f && dpDownBound
        var up = hatY < -0.5f && dpUpBound

        fun foldStick(axisX: Int, axisY: Int) {
            val x = ev.getAxisValue(axisX)
            val y = ev.getAxisValue(axisY)
            right = right || x > STICK_DIGITAL_THRESHOLD
            left = left || x < -STICK_DIGITAL_THRESHOLD
            down = down || y > STICK_DIGITAL_THRESHOLD
            up = up || y < -STICK_DIGITAL_THRESHOLD
        }
        if (leftDpad) foldStick(MotionEvent.AXIS_X, MotionEvent.AXIS_Y)
        if (rightDpad) foldStick(MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ)

        // Fold CUSTOM directions that target a D-pad code so they share this owner.
        fun foldCustom(isLeft: Boolean, axisX: Int, axisY: Int) {
            if (ControllerMappings.stickModeFor(isLeft, port) != ControllerMappings.StickMode.CUSTOM) return
            val x = ev.getAxisValue(axisX)
            val y = ev.getAxisValue(axisY)
            fun mark(dir: ControllerMappings.StickDir, active: Boolean) {
                if (!active) return
                when (ControllerMappings.customStickCode(isLeft, dir, port)) {
                    22 -> right = true
                    21 -> left = true
                    20 -> down = true
                    19 -> up = true
                }
            }
            mark(ControllerMappings.StickDir.RIGHT, x > STICK_DIGITAL_THRESHOLD)
            mark(ControllerMappings.StickDir.LEFT, x < -STICK_DIGITAL_THRESHOLD)
            mark(ControllerMappings.StickDir.DOWN, y > STICK_DIGITAL_THRESHOLD)
            mark(ControllerMappings.StickDir.UP, y < -STICK_DIGITAL_THRESHOLD)
        }
        foldCustom(true, MotionEvent.AXIS_X, MotionEvent.AXIS_Y)
        foldCustom(false, MotionEvent.AXIS_Z, MotionEvent.AXIS_RZ)

        // Write only on change so a resting stick's motion stream can't re-release
        // a direction the physical D-pad is also holding.
        fun apply(code: Int, on: Boolean) {
            val was = held.contains(code)
            if (on == was) return
            NativeApp.setPadButtonForPort(port, code, if (on) 32767 else 0, on)
            if (on) held.add(code) else held.remove(code)
        }
        apply(22, right) // D-pad right
        apply(21, left)  // D-pad left
        apply(20, down)  // D-pad down
        apply(19, up)    // D-pad up
    }

    private fun sendTrigger(event: MotionEvent, axisA: Int, axisB: Int, code: Int, port: Int) {
        // Pads report L2/R2 on AXIS_*TRIGGER or on AXIS_BRAKE/GAS — take the higher of
        // the two, clamping negatives (some non-Xbox pads idle an unused trigger axis at
        // -1). Then apply the SMALL trigger deadzone and re-normalize the remaining range
        // to 0..1, so pressure ramps smoothly from zero to full instead of flicking on/off
        // around the old hard 15% stick-deadzone boundary (the jitter non-Xbox pads showed)
        // — and the low 15% of travel is no longer wasted.
        // Honor the L2/R2 binding: triggers arrive as motion axes, never through the
        // keycode binding path, so clearing/remapping them in the Pad tab was ignored.
        // Resolve the physical trigger keycode to its mapped PS2 target — null = cleared,
        // so the trigger is disabled; otherwise drive the resolved (possibly remapped) code.
        val target = ControllerMappings.targetForPhysical(code, port) ?: return
        val raw = maxOf(event.getAxisValue(axisA), event.getAxisValue(axisB)).coerceIn(0f, 1f)
        val out = if (raw <= TRIGGER_DEAD) 0f else (raw - TRIGGER_DEAD) / (1f - TRIGGER_DEAD)
        if (target in 110..123) {
            // Trigger bound to a PS2 STICK direction ("(send)" rows): contribute the
            // proportional pressure to the merge layer so it can't be released by
            // the target stick's own (resting) ANALOG writer in the same event.
            accumAnalog(target, out)
        } else {
            NativeApp.setPadButtonForPort(port, target, (out * 32767).toInt(), out > 0f)
        }
    }

    override fun onPause() {
        com.armsx2.navigation.UiNavigator.drawerOpen.value = false
        // Leaving the app (home / recents / slide-out) while a game is running:
        // open the pause OVERLAY instead of a silent pause. A bare pause left
        // users staring at a frozen game with no obvious way back — they had to
        // know to open the menu and tap Resume. open() pauses the VM AND shows
        // the pause menu, so returning lands straight on the Resume button.
        // No-op if the overlay is already up (it already paused the game).
        if (eState.value == EmuState.RUNNING)
            InGameOverlay.open()
        // Persist Vulkan pipeline cache before Android can reap the process.
        // ~VKShaderCache only fires on a clean device teardown, but swipe-kill
        // / OOM-kill skip that path — every cold launch would otherwise
        // re-compile every TFX pipeline from scratch. No-op on OpenGL.
        NativeApp.flushShaderCache()
        // PGO instrument build: flush profile counters so a profiling run survives
        // an Android process kill. No-op in normal builds.
        runCatching { NativeApp.dumpPgoProfile() }
        super.onPause()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleExternalLaunchIntent(intent)
    }

    override fun onDestroy() {
        // On a CONFIGURATION-driven recreate (e.g. Samsung DeX moving the activity to
        // an external display, density/uiMode change) Android destroys+recreates us.
        // Do NOT tear down the native VM or hard-kill the process then — that races the
        // recreate and crashes ("this app has a bug"). Only shut down on a real finish.
        if (isChangingConfigurations()) {
            super.onDestroy()
            return
        }
        NativeApp.shutdown()
        super.onDestroy()

        val appPid = Process.myPid()
        Process.killProcess(appPid)
    }

    private fun handleExternalLaunchIntent(intent: Intent?) {
        val uri = extractLaunchUri(intent) ?: return
        persistReadGrant(intent, uri)
        currentGame.value = null
        pendingExternalLaunch.value = uri.toString()
        launchPendingExternalGameIfReady()
    }

    private fun extractLaunchUri(intent: Intent?): Uri? {
        if (intent == null)
            return null

        intent.data?.let { return it }

        val stream: Uri? = if (Build.VERSION.SDK_INT >= 33) {
            intent.getParcelableExtra(Intent.EXTRA_STREAM, Uri::class.java)
        } else {
            @Suppress("DEPRECATION")
            intent.getParcelableExtra(Intent.EXTRA_STREAM) as? Uri
        }
        stream?.let { return it }

        intent.clipData?.takeIf { it.itemCount > 0 }?.getItemAt(0)?.uri?.let { return it }

        for (key in listOf("path", "game", "rom", "uri", "android.intent.extra.STREAM")) {
            val value = intent.getStringExtra(key)?.takeIf { it.isNotBlank() } ?: continue
            return value.toUri()
        }

        return null
    }

    private fun persistReadGrant(intent: Intent?, uri: Uri) {
        if (uri.scheme != "content" || intent == null)
            return

        val flags = intent.flags
        if ((flags and Intent.FLAG_GRANT_READ_URI_PERMISSION) == 0 ||
            (flags and Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION) == 0)
            return

        runCatching {
            contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION,
            )
        }
    }
}
