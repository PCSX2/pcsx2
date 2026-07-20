package com.armsx2.input

import android.view.KeyEvent
import androidx.compose.runtime.mutableStateOf
import com.armsx2.runtime.MainActivityRuntime
import androidx.core.content.edit
import org.json.JSONObject
import java.io.File

object ControllerMappings {
    data class Action(
        val id: String,
        val label: String,
        val targetKeyCode: Int,
        val defaultPhysicalKeyCode: Int,
    )

    val actions = listOf(
        Action("dpad_up", "D-Pad Up", KeyEvent.KEYCODE_DPAD_UP, KeyEvent.KEYCODE_DPAD_UP),
        Action("dpad_down", "D-Pad Down", KeyEvent.KEYCODE_DPAD_DOWN, KeyEvent.KEYCODE_DPAD_DOWN),
        Action("dpad_left", "D-Pad Left", KeyEvent.KEYCODE_DPAD_LEFT, KeyEvent.KEYCODE_DPAD_LEFT),
        Action("dpad_right", "D-Pad Right", KeyEvent.KEYCODE_DPAD_RIGHT, KeyEvent.KEYCODE_DPAD_RIGHT),
        Action("cross", "Cross", KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_A),
        Action("circle", "Circle", KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_BUTTON_B),
        Action("square", "Square", KeyEvent.KEYCODE_BUTTON_X, KeyEvent.KEYCODE_BUTTON_X),
        Action("triangle", "Triangle", KeyEvent.KEYCODE_BUTTON_Y, KeyEvent.KEYCODE_BUTTON_Y),
        Action("l1", "L1", KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_BUTTON_L1),
        Action("r1", "R1", KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_BUTTON_R1),
        Action("l2", "L2", KeyEvent.KEYCODE_BUTTON_L2, KeyEvent.KEYCODE_BUTTON_L2),
        Action("r2", "R2", KeyEvent.KEYCODE_BUTTON_R2, KeyEvent.KEYCODE_BUTTON_R2),
        Action("l3", "L3", KeyEvent.KEYCODE_BUTTON_THUMBL, KeyEvent.KEYCODE_BUTTON_THUMBL),
        Action("r3", "R3", KeyEvent.KEYCODE_BUTTON_THUMBR, KeyEvent.KEYCODE_BUTTON_THUMBR),
        Action("select", "Select", KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BUTTON_SELECT),
        Action("start", "Start", KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_BUTTON_START),
        // The DualShock2 Analog/mode button. Target 200 -> native PAD_ANALOG
        // (see native-lib.cpp setPadButton). UNBOUND by default — there's no
        // standard Android keycode for it, so the user binds a button. Lets the
        // few games that require the analog toggle (e.g. Driving Emotion Type-S)
        // actually enable their sticks. Parity with desktop PCSX2.
        Action("analog", "Analog (toggle)", 200, KeyEvent.KEYCODE_UNKNOWN),
        // ---- PS2 analog-stick DIRECTIONS as bindable targets (AetherSX2-style) ----
        // Any physical input — button, trigger, or another stick's direction — can be
        // bound to SEND a PS2 stick direction (e.g. physical L2 -> R-Stick Down for
        // Burnout 3 boost-camera setups). Targets are the native analog codes 110-123;
        // a digital source gives full deflection, an analog trigger stays proportional
        // (sendTrigger resolves through targetForPhysical), and a physical stick
        // direction source is driven proportionally in dispatchStickDirBindings.
        // UNBOUND by default — zero change unless the user binds them. NOTE: binding
        // e.g. physical L2 here does NOT auto-clear the PS2 L2 row; targetForPhysical
        // returns the FIRST action bound to a physical key, so users should Clear the
        // button row they're stealing from (the row description says so).
        Action("ls_up", "L-Stick Up (send)", 110, KeyEvent.KEYCODE_UNKNOWN),
        Action("ls_down", "L-Stick Down (send)", 112, KeyEvent.KEYCODE_UNKNOWN),
        Action("ls_left", "L-Stick Left (send)", 113, KeyEvent.KEYCODE_UNKNOWN),
        Action("ls_right", "L-Stick Right (send)", 111, KeyEvent.KEYCODE_UNKNOWN),
        Action("rs_up", "R-Stick Up (send)", 120, KeyEvent.KEYCODE_UNKNOWN),
        Action("rs_down", "R-Stick Down (send)", 122, KeyEvent.KEYCODE_UNKNOWN),
        Action("rs_left", "R-Stick Left (send)", 123, KeyEvent.KEYCODE_UNKNOWN),
        Action("rs_right", "R-Stick Right (send)", 121, KeyEvent.KEYCODE_UNKNOWN),
    )

    // ---- Analog stick remapping (physical sticks → digital PS2 inputs) ----
    // Lets a physical stick drive the D-pad or the face buttons instead of the PS2
    // analog stick — handy for fighting games on analog-centric pads (e.g. left
    // stick = D-pad). Global, like the button bindings; persisted in MainActivityRuntime.prefs.
    enum class StickMode(val id: String, val label: String) {
        ANALOG("analog", "Analog"),
        FACE("face", "Face"),
        // Drive the PS2 digital D-pad (codes 19-22) from this analog stick — the fixed
        // "stick as D-pad" preset (the nightly's default for Nintendo Joy-Cons, whose
        // solo d-pad often can't be bound directly). Opt-in: nothing changes for other
        // controllers unless a user picks it. Handled by the single d-pad owner
        // (dispatchDpadCombined) so it can't fight the physical HAT.
        DPAD("dpad", "D-Pad"),
        // Per-direction binding: each direction sends any PS2 button (incl. d-pad),
        // captured by "Press a button". This supersedes the old fixed D-Pad preset.
        CUSTOM("custom", "Custom"),
    }

    // ---- Per-player bindings (local co-op) ---------------------------------
    // P1 (0) keeps the existing prefs keys byte-for-byte (empty prefix), so
    // single-player users see ZERO change; P2 (1) namespaces under "p2.". Only
    // WHICH physical button maps to WHICH PS2 button is per-player (button binds,
    // stick mode, custom stick codes). Controller FEEL (sensitivity / accel /
    // deadzone / d-pad-as-left-stick) and SYSTEM hotkeys stay global.
    const val P1 = 0
    const val P2 = 1
    // Mapping tier by unified pad slot. Only slot 1 (P2 main) has its own remap tier
    // ("p2."); slot 0 AND the multitap tap slots 2-7 all share Player 1's mapping ("").
    // This is the single chokepoint every per-player lookup (targetForPhysical, stick
    // modes, turbo, custom-stick) routes through, so clamping here makes the tap slots
    // reuse P1's config without threading a mapping-player through the input dispatch.
    private fun playerPrefix(player: Int) = if (player == 1) "p2." else ""

    // ---- Per-game scope (issue #246) --------------------------------------
    // The INPUT-MAPPING layer — button binds, stick modes, custom stick codes —
    // may be overridden PER GAME, mirroring how renderer/touch already go
    // per-serial. Global keys stay the baseline (single-player users unchanged);
    // a per-game override lives under "game.<serial>." and shadows the global
    // value for that serial ONLY. Controller FEEL (deadzone/sensitivity/accel/
    // invert/rumble/dpad-as-lstick) stays GLOBAL — it describes the physical pad.
    private fun gameKey(serial: String, baseKey: String) = "game.$serial.$baseKey"
    private fun scopedKey(baseKey: String, serial: String?) =
        if (serial.isNullOrEmpty()) baseKey else gameKey(serial, baseKey)

    /** Serial of the running game whose overrides apply at RUNTIME (input
     *  dispatch), or null in menus so use falls back to global. */
    private fun runtimeSerial(): String? = MainActivityRuntime.currentGame.value?.serial?.takeIf { it.isNotEmpty() }

    /** Read an int pref: per-game override (for the active game) first, else global. */
    private fun resolveInt(baseKey: String, default: Int): Int {
        val s = runtimeSerial()
        if (s != null) {
            val gk = gameKey(s, baseKey)
            if (MainActivityRuntime.prefs.contains(gk)) return MainActivityRuntime.prefs.getInt(gk, default)
        }
        return MainActivityRuntime.prefs.getInt(baseKey, default)
    }

    /** Read a string pref: per-game override first, else global. */
    private fun resolveString(baseKey: String, default: String): String {
        val s = runtimeSerial()
        if (s != null) MainActivityRuntime.prefs.getString(gameKey(s, baseKey), null)?.let { return it }
        return MainActivityRuntime.prefs.getString(baseKey, default) ?: default
    }

    /** Read a boolean pref: per-game override (for the active game) first, else global. */
    private fun resolveBoolean(baseKey: String, default: Boolean): Boolean {
        val s = runtimeSerial()
        if (s != null) {
            val gk = gameKey(s, baseKey)
            if (MainActivityRuntime.prefs.contains(gk)) return MainActivityRuntime.prefs.getBoolean(gk, default)
        }
        return MainActivityRuntime.prefs.getBoolean(baseKey, default)
    }

    /** Scope-explicit int read for the Pad UI: the override at [serial]'s tier if
     *  present, else the global baseline (so a fresh per-game row shows the
     *  inherited value instead of blank). serial=null reads the global tier. */
    private fun scopedInt(baseKey: String, serial: String?, default: Int): Int {
        val key = scopedKey(baseKey, serial)
        if (MainActivityRuntime.prefs.contains(key)) return MainActivityRuntime.prefs.getInt(key, default)
        return MainActivityRuntime.prefs.getInt(baseKey, default)
    }

    /** Scope-explicit boolean read for the Pad UI: the override at [serial]'s tier if
     *  present, else the global baseline. serial=null reads the global tier. */
    private fun scopedBoolean(baseKey: String, serial: String?, default: Boolean): Boolean {
        val key = scopedKey(baseKey, serial)
        if (MainActivityRuntime.prefs.contains(key)) return MainActivityRuntime.prefs.getBoolean(key, default)
        return MainActivityRuntime.prefs.getBoolean(baseKey, default)
    }

    private const val KEY_LSTICK = "pad.lstick.mode"
    private const val KEY_RSTICK = "pad.rstick.mode"

    private fun stickModeFromId(id: String?): StickMode =
        StickMode.entries.firstOrNull { it.id == id } ?: StickMode.ANALOG

    // Runtime (per-game aware): used by the input dispatcher.
    fun leftStickMode(player: Int = 0): StickMode =
        stickModeFromId(resolveString(playerPrefix(player) + KEY_LSTICK, StickMode.ANALOG.id))
    fun rightStickMode(player: Int = 0): StickMode =
        stickModeFromId(resolveString(playerPrefix(player) + KEY_RSTICK, StickMode.ANALOG.id))
    /** Mode for the left (true) or right (false) stick — used by the dispatcher. */
    fun stickModeFor(left: Boolean, player: Int = 0): StickMode =
        if (left) leftStickMode(player) else rightStickMode(player)

    // Scope-explicit (Pad UI): read/edit the global tier (serial=null) or a per-game tier.
    fun leftStickModeScope(player: Int, serial: String?): StickMode =
        stickModeFromId(MainActivityRuntime.prefs.getString(scopedKey(playerPrefix(player) + KEY_LSTICK, serial), null)
            ?: MainActivityRuntime.prefs.getString(playerPrefix(player) + KEY_LSTICK, StickMode.ANALOG.id))
    fun rightStickModeScope(player: Int, serial: String?): StickMode =
        stickModeFromId(MainActivityRuntime.prefs.getString(scopedKey(playerPrefix(player) + KEY_RSTICK, serial), null)
            ?: MainActivityRuntime.prefs.getString(playerPrefix(player) + KEY_RSTICK, StickMode.ANALOG.id))
    fun setLeftStickMode(m: StickMode, player: Int = 0, serial: String? = null) =
        MainActivityRuntime.prefs.edit {
            putString(
                scopedKey(
                    playerPrefix(player) + KEY_LSTICK,
                    serial
                ), m.id
            )
        }

    fun setRightStickMode(m: StickMode, player: Int = 0, serial: String? = null) =
        MainActivityRuntime.prefs.edit {
            putString(
                scopedKey(
                    playerPrefix(player) + KEY_RSTICK,
                    serial
                ), m.id
            )
        }

    // ---- Per-stick axis correction (invert / swap) ------------------------
    // Fixes pads whose stick reads rotated or mirrored — e.g. a right stick where
    // "down is up and left is right". Applied to the RAW axis values before any
    // mode dispatch, so it corrects Analog, Face AND Custom modes alike. Swap is
    // applied first, then the inverts. Follows the SAME Global/per-game scope as
    // stick modes and binds (issue #246): the global value is the baseline, and a
    // per-game override shadows it for that serial only — a user reported inverting
    // one game's right stick flipped every game because these wrote global outright.
    private const val KEY_LSTICK_INVX = "pad.lstick.invertX"
    private const val KEY_LSTICK_INVY = "pad.lstick.invertY"
    private const val KEY_LSTICK_SWAP = "pad.lstick.swapXY"
    private const val KEY_RSTICK_INVX = "pad.rstick.invertX"
    private const val KEY_RSTICK_INVY = "pad.rstick.invertY"
    private const val KEY_RSTICK_SWAP = "pad.rstick.swapXY"
    private fun invXKey(left: Boolean) = if (left) KEY_LSTICK_INVX else KEY_RSTICK_INVX
    private fun invYKey(left: Boolean) = if (left) KEY_LSTICK_INVY else KEY_RSTICK_INVY
    private fun swapKey(left: Boolean) = if (left) KEY_LSTICK_SWAP else KEY_RSTICK_SWAP
    // Runtime (per-game aware): read by the input dispatcher / touch overlay.
    fun stickInvertX(left: Boolean): Boolean = resolveBoolean(invXKey(left), false)
    fun stickInvertY(left: Boolean): Boolean = resolveBoolean(invYKey(left), false)
    fun stickSwapXY(left: Boolean): Boolean = resolveBoolean(swapKey(left), false)
    // Scope-explicit (Pad UI): the override at [serial]'s tier if set, else the global baseline.
    fun stickInvertXScope(left: Boolean, serial: String?): Boolean = scopedBoolean(invXKey(left), serial, false)
    fun stickInvertYScope(left: Boolean, serial: String?): Boolean = scopedBoolean(invYKey(left), serial, false)
    fun stickSwapXYScope(left: Boolean, serial: String?): Boolean = scopedBoolean(swapKey(left), serial, false)
    fun setStickInvertX(left: Boolean, on: Boolean, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putBoolean(scopedKey(invXKey(left), serial), on) }
    fun setStickInvertY(left: Boolean, on: Boolean, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putBoolean(scopedKey(invYKey(left), serial), on) }
    fun setStickSwapXY(left: Boolean, on: Boolean, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putBoolean(scopedKey(swapKey(left), serial), on) }

    // Make the physical D-pad drive the LEFT analog stick (full deflection) so it
    // works in games that only read the analog stick. While on, the D-pad no
    // longer sends digital d-pad presses in-game. Same Global/per-game scope as the
    // axis corrections above (it appears on the same per-game Pad screen).
    private const val KEY_DPAD_AS_LSTICK = "pad.dpadAsLeftStick"
    fun dpadAsLeftStick(): Boolean = resolveBoolean(KEY_DPAD_AS_LSTICK, false) // runtime (per-game aware)
    fun dpadAsLeftStickScope(serial: String?): Boolean = scopedBoolean(KEY_DPAD_AS_LSTICK, serial, false)
    fun setDpadAsLeftStick(on: Boolean, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putBoolean(scopedKey(KEY_DPAD_AS_LSTICK, serial), on) }

    // ---- Analog stick response shaping (physical sticks → PS2 analog) ----
    // Sensitivity = a linear output scale. Acceleration = an exponential response
    // curve applied to the post-deadzone magnitude (0 = linear; higher = finer
    // control near center, ramping to full speed at full tilt). Global controller
    // feel, persisted in MainActivityRuntime.prefs; cached so the hot motion path avoids a lookup.
    //
    // PER-STICK since the feature batch that split them: every feel tunable exists
    // for the LEFT and RIGHT stick independently (tester feedback: lowering
    // sensitivity for camera aim also slowed walking). Storage = old key + ".l" /
    // ".r"; the OLD single key is the migration fallback, so an existing user's
    // tuned value carries over to BOTH sticks and nothing changes until they split.
    private const val KEY_STICK_SENS = "pad.stick.sensitivity"
    private const val KEY_STICK_ACCEL = "pad.stick.acceleration"
    const val STICK_SENS_MIN = 0.5f
    const val STICK_SENS_MAX = 2.0f
    const val STICK_ACCEL_MAX = 2.0f

    /** Per-stick float pref with migration: "<base>.l"/".r" if present, else the
     *  legacy single "<base>" key, else [def]. Cached per stick for the hot path. */
    private class PerStickPref(val baseKey: String, val def: Float, val lo: Float, val hi: Float) {
        @Volatile var cacheL = Float.NaN
        @Volatile var cacheR = Float.NaN
        fun key(left: Boolean) = baseKey + if (left) ".l" else ".r"
        fun get(left: Boolean): Float {
            val c = if (left) cacheL else cacheR
            if (!c.isNaN()) return c
            val v = (if (MainActivityRuntime.prefs.contains(key(left))) MainActivityRuntime.prefs.getFloat(key(left), def)
                else MainActivityRuntime.prefs.getFloat(baseKey, def)).coerceIn(lo, hi)
            if (left) cacheL = v else cacheR = v
            return v
        }
        fun set(left: Boolean, v: Float) {
            val c = v.coerceIn(lo, hi)
            if (left) cacheL = c else cacheR = c
            MainActivityRuntime.prefs.edit { putFloat(key(left), c) }
        }
        fun reset(edit: android.content.SharedPreferences.Editor) {
            edit.remove(baseKey).remove(key(true)).remove(key(false))
            cacheL = Float.NaN; cacheR = Float.NaN
        }
    }
    private val prefStickSens = PerStickPref(KEY_STICK_SENS, 1.0f, STICK_SENS_MIN, STICK_SENS_MAX)
    private val prefStickAccel = PerStickPref(KEY_STICK_ACCEL, 0.0f, 0f, STICK_ACCEL_MAX)
    fun stickSensitivity(left: Boolean): Float = prefStickSens.get(left)
    fun setStickSensitivity(left: Boolean, v: Float) = prefStickSens.set(left, v)
    fun stickAcceleration(left: Boolean): Float = prefStickAccel.get(left)
    fun setStickAcceleration(left: Boolean, v: Float) = prefStickAccel.set(left, v)

    // Response curve: an EXTRA exponent applied to the post-deadzone stick magnitude, on top
    // of any acceleration (they compose). Tames twitchy hall-effect sticks (e.g. GTA:SA) —
    // small tilts get finer near center, full tilt still reaches 100%. Per-stick.
    // 0=Linear (unchanged), 1=Light, 2=Medium, 3=Strong.
    const val STICK_CURVE_COUNT = 4
    private val STICK_CURVE_GAMMA = floatArrayOf(0f, 0.5f, 1.0f, 2.0f)
    private const val KEY_STICK_CURVE = "pad.stick.responseCurve"
    private val prefStickCurve = PerStickPref(KEY_STICK_CURVE, 0f, 0f, (STICK_CURVE_COUNT - 1).toFloat())
    fun stickResponseCurve(left: Boolean): Int = prefStickCurve.get(left).toInt()
    fun setStickResponseCurve(left: Boolean, v: Int) = prefStickCurve.set(left, v.toFloat())
    fun stickCurveGamma(left: Boolean): Float =
        STICK_CURVE_GAMMA[stickResponseCurve(left).coerceIn(0, STICK_CURVE_COUNT - 1)]

    // App-side analog stick deadzone (fraction of travel ignored). Kept small by
    // default and user-adjustable down to 0 — handheld "switch" sticks have tiny
    // range, so a big deadzone wastes most of it. Output is re-normalized past the
    // deadzone (see MainActivityRuntime.shapeStickMag) so movement ramps smoothly from 0 instead
    // of jumping. Pairs with forcing the NATIVE pad deadzone to 0.
    private const val KEY_STICK_DZ = "pad.stick.deadzone"
    const val STICK_DZ_MAX = 0.40f
    private val prefStickDz = PerStickPref(KEY_STICK_DZ, 0.05f, 0f, STICK_DZ_MAX)
    fun stickDeadzone(left: Boolean): Float = prefStickDz.get(left)
    fun setStickDeadzone(left: Boolean, v: Float) = prefStickDz.set(left, v)

    // Outer (anti-)deadzone: fraction of travel near the EDGE that maps to full
    // output, so a stick that can't physically reach its corners still hits 100%
    // (short-throw / handheld sticks like the AYN Odin). 0 = off. Applied in
    // MainActivityRuntime.shapeStickMag as the upper edge of the post-deadzone re-normalize window.
    private const val KEY_STICK_OUTER = "pad.stick.outerDeadzone"
    const val STICK_OUTER_MAX = 0.40f
    private val prefStickOuter = PerStickPref(KEY_STICK_OUTER, 0.0f, 0f, STICK_OUTER_MAX)
    fun stickOuterDeadzone(left: Boolean): Float = prefStickOuter.get(left)
    fun setStickOuterDeadzone(left: Boolean, v: Float) = prefStickOuter.set(left, v)

    // Anti-deadzone (output floor): the SMALLEST non-zero analog output sent to the PS2.
    // Many PS2 games have a large built-in stick deadzone (e.g. Cold Fear / Area 51 ignore
    // the stick until ~45%), so with a linear map the game feels dead at the bottom then
    // jumps. Set this near the game's deadzone and ANY stick movement maps to just past it,
    // so the full physical travel maps smoothly onto the game's active range (immediate +
    // proportional, no jump, no slow zone). 0 = off (unchanged). Applied in MainActivityRuntime.shapeStickMag
    // AFTER sensitivity, only to a non-zero magnitude (true center still reads 0).
    private const val KEY_STICK_ANTIDZ = "pad.stick.antiDeadzone"
    const val STICK_ANTIDZ_MAX = 0.60f
    private val prefStickAntiDz = PerStickPref(KEY_STICK_ANTIDZ, 0.0f, 0f, STICK_ANTIDZ_MAX)
    fun stickAntiDeadzone(left: Boolean): Float = prefStickAntiDz.get(left)
    fun setStickAntiDeadzone(left: Boolean, v: Float) = prefStickAntiDz.set(left, v)

    // Master rumble / vibration enable. Gates NativeApp.onPadRumble (controller motors AND
    // the device-haptic fallback). Persisted in prefs and mirrored into the native gate
    // NativeApp.sRumbleEnabled — live on change and at app start (MainActivityRuntime.onCreate). Default on.
    private const val KEY_RUMBLE = "pad.rumble.enabled"
    fun rumbleEnabled(): Boolean = MainActivityRuntime.prefs.getBoolean(KEY_RUMBLE, true)
    fun setRumbleEnabled(on: Boolean) {
        MainActivityRuntime.prefs.edit { putBoolean(KEY_RUMBLE, on) }
        kr.co.iefriends.pcsx2.NativeApp.sRumbleEnabled = on
    }

    // PS2 Multitap master switch. OFF (default) = classic 2-player co-op. ON = up to 8
    // controllers routed to the 2 ports x 4 slots. Extra pads (slots 2-7) reuse the P1
    // button mapping. Also drives PadRouter's routing gate.
    private const val KEY_MULTITAP = "pad.multitap.enabled"
    fun multitapEnabled(): Boolean = MainActivityRuntime.prefs.getBoolean(KEY_MULTITAP, false)
    fun setMultitapEnabled(on: Boolean) {
        MainActivityRuntime.prefs.edit { putBoolean(KEY_MULTITAP, on) }
        com.armsx2.input.PadRouter.multitapEnabled = on
        if (MainActivityRuntime.nativeReady.value) {
            kotlin.concurrent.thread(name = "armsx2-multitap") {
                runCatching {
                    kr.co.iefriends.pcsx2.NativeApp.setMultitap(0, on)
                    kr.co.iefriends.pcsx2.NativeApp.setMultitap(1, on)
                }
            }
        }
    }

    // ---- Gyroscope / motion controls (per-game aware) ---------------------
    // Drive a PS2 analog stick from the device's motion sensors: AIM (mode 1) reads the
    // gyroscope's angular velocity onto the RIGHT stick (camera / look), STEERING
    // (mode 2) reads the game-rotation-vector's roll onto the LEFT stick's X (tilt to
    // steer). OFF (default) leaves the sensors untouched. Sensitivity / smoothing /
    // invert tune the feel. These follow the SAME Global/per-game scope as the button
    // binds (#246): a per-game override lives under "game.<serial>." and shadows the
    // global value for that serial only. The sensor lifecycle in TouchControlsOverlay
    // reads the runtime (per-game aware) getters while the game is running.
    const val GYRO_OFF = 0
    const val GYRO_AIM = 1
    const val GYRO_STEER = 2
    private const val KEY_GYRO_MODE = "pad.gyro.mode"
    private const val KEY_GYRO_SENS = "pad.gyro.sensitivity"
    private const val KEY_GYRO_SMOOTH = "pad.gyro.smoothing"
    private const val KEY_GYRO_INVX = "pad.gyro.invertX"
    private const val KEY_GYRO_INVY = "pad.gyro.invertY"
    // Which analog stick Aim mode drives: 0 = Right (default, most FPS), 1 = Left
    // (games that aim with the left stick, e.g. Resident Evil 4). No effect in Steer mode.
    const val GYRO_STICK_RIGHT = 0
    const val GYRO_STICK_LEFT = 1
    private const val KEY_GYRO_AIM_STICK = "pad.gyro.aimStick"

    // Runtime (per-game aware): read by the sensor lifecycle while a game runs.
    fun gyroMode(): Int = resolveInt(KEY_GYRO_MODE, GYRO_OFF).coerceIn(0, 2)
    fun gyroSensitivity(): Int = resolveInt(KEY_GYRO_SENS, 100).coerceIn(25, 300)
    fun gyroSmoothing(): Int = resolveInt(KEY_GYRO_SMOOTH, 45).coerceIn(0, 90)
    fun gyroInvertX(): Boolean = resolveBoolean(KEY_GYRO_INVX, false)
    fun gyroInvertY(): Boolean = resolveBoolean(KEY_GYRO_INVY, false)
    fun gyroAimStick(): Int = resolveInt(KEY_GYRO_AIM_STICK, GYRO_STICK_RIGHT).coerceIn(0, 1)

    // Scope-explicit (Pad UI): read the global tier (serial=null) or a per-game tier.
    fun gyroModeScope(serial: String?): Int = scopedInt(KEY_GYRO_MODE, serial, GYRO_OFF).coerceIn(0, 2)
    fun gyroSensitivityScope(serial: String?): Int = scopedInt(KEY_GYRO_SENS, serial, 100).coerceIn(25, 300)
    fun gyroSmoothingScope(serial: String?): Int = scopedInt(KEY_GYRO_SMOOTH, serial, 45).coerceIn(0, 90)
    fun gyroInvertXScope(serial: String?): Boolean = scopedBoolean(KEY_GYRO_INVX, serial, false)
    fun gyroInvertYScope(serial: String?): Boolean = scopedBoolean(KEY_GYRO_INVY, serial, false)
    fun gyroAimStickScope(serial: String?): Int = scopedInt(KEY_GYRO_AIM_STICK, serial, GYRO_STICK_RIGHT).coerceIn(0, 1)

    fun setGyroMode(value: Int, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putInt(scopedKey(KEY_GYRO_MODE, serial), value) }
    fun setGyroSensitivity(value: Int, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putInt(scopedKey(KEY_GYRO_SENS, serial), value) }
    fun setGyroSmoothing(value: Int, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putInt(scopedKey(KEY_GYRO_SMOOTH, serial), value) }
    fun setGyroInvertX(on: Boolean, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putBoolean(scopedKey(KEY_GYRO_INVX, serial), on) }
    fun setGyroInvertY(on: Boolean, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putBoolean(scopedKey(KEY_GYRO_INVY, serial), on) }
    fun setGyroAimStick(value: Int, serial: String? = null) =
        MainActivityRuntime.prefs.edit { putInt(scopedKey(KEY_GYRO_AIM_STICK, serial), value) }

    // ---- Custom per-direction stick→button binding (StickMode.CUSTOM) ----

    /** The four directions of a stick, each independently bindable in CUSTOM mode. */
    enum class StickDir(val id: String) { UP("up"), DOWN("down"), LEFT("left"), RIGHT("right") }

    /** A PS2 button a stick direction may map to (digital setPadButton codes from
     *  native-lib.cpp). [label] drives the picker UI. */
    data class PsButton(val code: Int, val label: String)
    val stickTargets = listOf(
        PsButton(19, "D-Pad Up"), PsButton(20, "D-Pad Down"),
        PsButton(21, "D-Pad Left"), PsButton(22, "D-Pad Right"),
        PsButton(96, "Cross"), PsButton(97, "Circle"),
        PsButton(99, "Square"), PsButton(100, "Triangle"),
        PsButton(102, "L1"), PsButton(103, "R1"),
        PsButton(104, "L2"), PsButton(105, "R2"),
        PsButton(106, "L3"), PsButton(107, "R3"),
        PsButton(108, "Start"), PsButton(109, "Select"),
        PsButton(200, "Analog (toggle)"),
    )
    fun stickTargetLabel(code: Int): String =
        hotkeyForStickCode(code)?.let { "Hotkey: ${it.label}" }
            ?: if (code in 110..123) "Analog (default)"
            else stickTargets.firstOrNull { it.code == code }?.label ?: "Code $code"

    // Default per-direction code = the stick's native analog code, so a fresh
    // CUSTOM stick behaves exactly like ANALOG until the user rebinds a direction.
    private fun defaultCustomCode(left: Boolean, dir: StickDir): Int = when {
        left && dir == StickDir.UP -> 110
        left && dir == StickDir.DOWN -> 112
        left && dir == StickDir.LEFT -> 113
        left && dir == StickDir.RIGHT -> 111
        !left && dir == StickDir.UP -> 120
        !left && dir == StickDir.DOWN -> 122
        !left && dir == StickDir.LEFT -> 123
        else -> 121 // right stick, RIGHT
    }
    private fun customKey(left: Boolean, dir: StickDir, player: Int = 0) =
        playerPrefix(player) + "pad.${if (left) "lstick" else "rstick"}.${dir.id}.code"
    // Runtime (per-game aware): used by the stick dispatcher.
    fun customStickCode(left: Boolean, dir: StickDir, player: Int = 0): Int =
        resolveInt(customKey(left, dir, player), defaultCustomCode(left, dir))
    fun setCustomStickCode(left: Boolean, dir: StickDir, code: Int, player: Int = 0, serial: String? = null) =
        MainActivityRuntime.prefs.edit {
            putInt(
                scopedKey(customKey(left, dir, player), serial),
                code
            )
        }
    /** Scope-explicit read for the Pad UI (per-game tier else global baseline). */
    fun customStickCodeScope(left: Boolean, dir: StickDir, player: Int, serial: String?): Int =
        scopedInt(customKey(left, dir, player), serial, defaultCustomCode(left, dir))

    // A CUSTOM stick direction can fire an ARMSX2 hotkey (Quick Save/Load State, etc.)
    // instead of a PS2 button — so a freed-up stick direction (e.g. when the left stick
    // already drives the D-pad) becomes a hotkey trigger. Hotkey targets share the
    // customStickCode storage via a reserved code range (no separate persistence). To
    // bind one, the user — while capturing a direction — presses a physical button they
    // already assigned to that hotkey in the Hotkeys tab; PadTab maps it to these codes.
    // Edge-triggered in MainActivityRuntime.emitCustom (fires once when the direction crosses the
    // digital threshold). [SysHotkey] is defined later in this object — fine, it's an
    // object so member order doesn't matter.
    const val HOTKEY_STICK_CODE_BASE = 300
    fun stickCodeForHotkey(h: SysHotkey): Int = HOTKEY_STICK_CODE_BASE + h.ordinal
    fun hotkeyForStickCode(code: Int): SysHotkey? {
        val i = code - HOTKEY_STICK_CODE_BASE
        return if (i in SysHotkey.values().indices) SysHotkey.values()[i] else null
    }

    /** Active CUSTOM stick-direction capture target, or null. (left, dir). When
     *  non-null the Pad tab is waiting for a physical button to bind to this
     *  direction — same model as [captureHotkey] / the per-Action [padCapturing]
     *  flow. Observed by the row UI for the yellow "Press a button..." text. */
    val captureStickDir = mutableStateOf<Pair<Boolean, StickDir>?>(null)

    /** Bumped after a stick-dir (re)bind so the observing row recomposes. */
    val stickBindTick = mutableStateOf(0)

    /** Resolve a captured physical keycode to the PS2 setPadButton code it drives,
     *  or null if that physical button isn't bound to any pad Action. Same
     *  physical->target lookup the gameplay path uses, e.g. physical-Cross -> 96.
     *  Reusing it means "stick Up = Cross" needs no new table. */
    fun stickCodeForPhysical(physicalKeyCode: Int, player: Int = 0): Int? =
        targetForPhysical(physicalKeyCode, player)

    fun beginStickCapture(left: Boolean, dir: StickDir) { captureStickDir.value = left to dir }
    fun endStickCapture() { captureStickDir.value = null; stickBindTick.value++ }

    /** Clear a direction back to its analog default (the Reset affordance). With a
     *  [serial], clears only that game's per-game override for the direction. */
    fun resetStickCode(left: Boolean, dir: StickDir, player: Int = 0, serial: String? = null) {
        MainActivityRuntime.prefs.edit { remove(scopedKey(customKey(left, dir, player), serial)) }; stickBindTick.value++
    }

    private const val KEY_PREFIX = "pad.map."

    // Runtime (per-game aware): used by targetForPhysical on the input path.
    fun physicalFor(action: Action, player: Int = 0): Int =
        resolveInt(playerPrefix(player) + KEY_PREFIX + action.id, action.defaultPhysicalKeyCode)

    /** Scope-explicit read for the Pad UI: the binding at [serial]'s tier, else
     *  the global baseline. serial=null reads/edits the global tier. */
    fun physicalForScope(action: Action, player: Int, serial: String?): Int =
        scopedInt(playerPrefix(player) + KEY_PREFIX + action.id, serial, action.defaultPhysicalKeyCode)

    // Reserved keycodes for binding an ANALOG STICK DIRECTION to a SysHotkey from the
    // Hotkeys tab (the d-pad already binds via its HAT->key translation; analog sticks
    // didn't). Real Android keycodes top out far below 1000, so this never collides.
    // 8 directions: L then R stick, each in StickDir ordinal order (Up/Down/Left/Right).
    const val STICK_HOTKEY_KEY_BASE = 1000
    fun stickHotkeyKeyCode(left: Boolean, dir: StickDir): Int =
        STICK_HOTKEY_KEY_BASE + (if (left) 0 else 4) + dir.ordinal

    fun labelForKey(keyCode: Int): String = when (keyCode) {
        KeyEvent.KEYCODE_UNKNOWN -> "Not set"
        in STICK_HOTKEY_KEY_BASE until STICK_HOTKEY_KEY_BASE + 8 -> {
            val i = keyCode - STICK_HOTKEY_KEY_BASE
            "${if (i < 4) "L-Stick" else "R-Stick"} ${StickDir.values()[i % 4].id.replaceFirstChar { it.uppercase() }}"
        }
        KeyEvent.KEYCODE_DPAD_UP -> "D-Pad Up"
        KeyEvent.KEYCODE_DPAD_DOWN -> "D-Pad Down"
        KeyEvent.KEYCODE_DPAD_LEFT -> "D-Pad Left"
        KeyEvent.KEYCODE_DPAD_RIGHT -> "D-Pad Right"
        KeyEvent.KEYCODE_BUTTON_A -> "Button A"
        KeyEvent.KEYCODE_BUTTON_B -> "Button B"
        KeyEvent.KEYCODE_BUTTON_X -> "Button X"
        KeyEvent.KEYCODE_BUTTON_Y -> "Button Y"
        KeyEvent.KEYCODE_BUTTON_L1 -> "L1"
        KeyEvent.KEYCODE_BUTTON_R1 -> "R1"
        KeyEvent.KEYCODE_BUTTON_L2 -> "L2"
        KeyEvent.KEYCODE_BUTTON_R2 -> "R2"
        KeyEvent.KEYCODE_BUTTON_THUMBL -> "L3"
        KeyEvent.KEYCODE_BUTTON_THUMBR -> "R3"
        KeyEvent.KEYCODE_BUTTON_SELECT -> "Select"
        KeyEvent.KEYCODE_BUTTON_START -> "Start"
        else -> KeyEvent.keyCodeToString(keyCode).removePrefix("KEYCODE_")
    }

    fun bind(action: Action, physicalKeyCode: Int, player: Int = 0, serial: String? = null) {
        MainActivityRuntime.prefs.edit {
            putInt(
                scopedKey(
                    playerPrefix(player) + KEY_PREFIX + action.id,
                    serial
                ), physicalKeyCode
            )
        }
    }

    /** Unbind a pad button: store KEYCODE_UNKNOWN — the same "unbound" sentinel the
     *  analog-toggle action already uses by default. physicalFor() then returns UNKNOWN
     *  (never matches a real key in targetForPhysical), labelForKey shows "Not set", and
     *  the freed physical button can instead be assigned as an ARMSX2 hotkey. With a
     *  [serial], unbinds the button for that game only (per-game override). */
    fun clearAction(action: Action, player: Int = 0, serial: String? = null) {
        MainActivityRuntime.prefs.edit().putInt(scopedKey(playerPrefix(player) + KEY_PREFIX + action.id, serial), KeyEvent.KEYCODE_UNKNOWN).apply()
    }

    /** Reset button binds for [player]. serial=null clears the GLOBAL binds; a
     *  serial removes that game's per-game button overrides (reverting to global). */
    fun reset(player: Int = 0, serial: String? = null) {
        val edit = MainActivityRuntime.prefs.edit()
        actions.forEach { edit.remove(scopedKey(playerPrefix(player) + KEY_PREFIX + it.id, serial)) }
        edit.apply()
    }

    /** Clear ALL per-game controller overrides for [serial] / [player] — button
     *  binds, stick modes AND custom stick codes — reverting that game fully to
     *  global. Used by the Pad-tab Reset when editing in Game scope. */
    fun clearGameOverrides(serial: String, player: Int) {
        if (serial.isEmpty()) return
        val edit = MainActivityRuntime.prefs.edit()
        actions.forEach { edit.remove(gameKey(serial, playerPrefix(player) + KEY_PREFIX + it.id)) }
        edit.remove(gameKey(serial, playerPrefix(player) + KEY_LSTICK))
            .remove(gameKey(serial, playerPrefix(player) + KEY_RSTICK))
        for (left in booleanArrayOf(true, false))
            for (dir in StickDir.values())
                edit.remove(gameKey(serial, customKey(left, dir, player)))
        edit.apply()
        stickBindTick.value++
    }

    // ---- Named mapping profiles (#186) ------------------------------------
    //
    // "Save my Tomb Raider pad layout and let me pick it again later." A profile is
    // a named snapshot of the MAPPING layer for one player: button binds, stick
    // modes, custom stick codes. That's the same boundary the per-game scope draws
    // (see [clearGameOverrides]) and for the same reason — controller FEEL
    // (deadzone/sensitivity/accel/invert/rumble) describes the physical pad, not a
    // game's control scheme, so it belongs to neither.
    //
    // Snapshots are COMPLETE, not sparse: every key is written with its effective
    // value, defaults included. Applying a profile then fully REPLACES the mapping
    // instead of leaving the previous one showing through the gaps — the same
    // reasoning as the shader parameter push, and the reason "apply" is predictable.
    //
    // Profiles are player-agnostic on disk: a profile captured from P1 applies to
    // whichever player you apply it to, because the stored keys are unprefixed.

    private const val KEY_PAD_PROFILES = "pad.profiles"
    private const val PAD_PROFILE_FILE_SUFFIX = ".pad.json"

    /** Every mapping key for [player], WITHOUT the player prefix — the prefix is
     *  applied on read/write so a profile can move between players. */
    private fun mappingKeys(): List<String> = buildList {
        actions.forEach { add(KEY_PREFIX + it.id) }
        add(KEY_LSTICK)
        add(KEY_RSTICK)
        for (left in booleanArrayOf(true, false))
            for (dir in StickDir.values())
                add("pad.${if (left) "lstick" else "rstick"}.${dir.id}.code")
    }

    /** The mapping [player] currently has at [serial]'s tier, as `key -> value`. */
    fun captureProfile(player: Int, serial: String?): JSONObject {
        val o = JSONObject()
        actions.forEach { o.put(KEY_PREFIX + it.id, physicalForScope(it, player, serial)) }
        o.put(KEY_LSTICK, leftStickModeScope(player, serial).id)
        o.put(KEY_RSTICK, rightStickModeScope(player, serial).id)
        for (left in booleanArrayOf(true, false))
            for (dir in StickDir.values())
                o.put(
                    "pad.${if (left) "lstick" else "rstick"}.${dir.id}.code",
                    customStickCodeScope(left, dir, player, serial),
                )
        return o
    }

    /** Write [values] into [player]'s mapping at [serial]'s tier. Unknown keys are
     *  ignored, so a profile written by a future build with more actions still
     *  applies the parts this build understands. */
    fun applyProfile(values: JSONObject, player: Int, serial: String?) {
        val pfx = playerPrefix(player)
        val edit = MainActivityRuntime.prefs.edit()
        mappingKeys().forEach { base ->
            if (!values.has(base)) return@forEach
            val target = scopedKey(pfx + base, serial)
            when (base) {
                KEY_LSTICK, KEY_RSTICK -> edit.putString(target, values.optString(base))
                else -> edit.putInt(target, values.optInt(base))
            }
        }
        edit.apply()
        stickBindTick.value++
    }

    private fun readProfiles(): JSONObject =
        runCatching { JSONObject(MainActivityRuntime.prefs.getString(KEY_PAD_PROFILES, "{}") ?: "{}") }
            .getOrDefault(JSONObject())

    private fun padFileNameFor(name: String): String =
        name.replace(Regex("[^A-Za-z0-9 _.-]"), "_") + PAD_PROFILE_FILE_SUFFIX

    /** Mirror prefs to `<DataRoot>/inputprofiles/<name>.pad.json`, dropping orphans.
     *  Same portable-folder contract as the touch profiles, different suffix, so the
     *  two kinds coexist in one folder. Best-effort. */
    private fun syncPadProfileFolder() {
        val dir = MainActivityRuntime.inputProfilesDir() ?: return
        runCatching {
            val store = readProfiles()
            val want = HashMap<String, JSONObject>()
            store.keys().forEach { n ->
                store.optJSONObject(n)?.let { v ->
                    want[padFileNameFor(n)] = JSONObject().put("name", n).put("values", v)
                }
            }
            for ((fn, body) in want) File(dir, fn).writeText(body.toString())
            dir.listFiles { f -> f.name.endsWith(PAD_PROFILE_FILE_SUFFIX) }?.forEach { f ->
                if (f.name !in want) runCatching { f.delete() }
            }
        }
    }

    /** Pull in folder profiles prefs doesn't have yet, so a hand-dropped file or a
     *  moved data folder shows up. Prefs stays the live source. */
    private fun importPadProfileFolder(store: JSONObject) {
        val dir = MainActivityRuntime.inputProfilesDir() ?: return
        runCatching {
            dir.listFiles { f -> f.name.endsWith(PAD_PROFILE_FILE_SUFFIX) }
                ?.sortedBy { it.name }
                ?.forEach { f ->
                    runCatching {
                        val o = JSONObject(f.readText())
                        val n = o.optString("name")
                        val v = o.optJSONObject("values")
                        if (n.isNotEmpty() && v != null && !store.has(n)) store.put(n, v)
                    }
                }
        }
    }

    /** Bumped on any profile add/delete so the Pad tab's list recomposes. */
    val padProfileTick = mutableStateOf(0)

    /** Saved profile names, prefs + portable folder merged, A→Z. */
    fun listProfiles(): List<String> {
        val store = readProfiles()
        importPadProfileFolder(store)
        return store.keys().asSequence().toList().sortedBy { it.lowercase() }
    }

    /** Snapshot [player]'s mapping at [serial]'s tier under [name], overwriting a
     *  same-named profile. Returns false if the name is blank — the caller reports
     *  that rather than silently doing nothing. */
    fun saveProfile(name: String, player: Int, serial: String?): Boolean {
        val trimmed = name.trim()
        if (trimmed.isEmpty()) return false
        val store = readProfiles()
        importPadProfileFolder(store)
        store.put(trimmed, captureProfile(player, serial))
        MainActivityRuntime.prefs.edit { putString(KEY_PAD_PROFILES, store.toString()) }
        syncPadProfileFolder()
        padProfileTick.value++
        return true
    }

    /** Apply saved profile [name] to [player] at [serial]'s tier. False if it's gone. */
    fun applyProfile(name: String, player: Int, serial: String?): Boolean {
        val store = readProfiles()
        importPadProfileFolder(store)
        val values = store.optJSONObject(name) ?: return false
        applyProfile(values, player, serial)
        return true
    }

    fun deleteProfile(name: String) {
        val store = readProfiles()
        importPadProfileFolder(store)
        store.remove(name)
        MainActivityRuntime.prefs.edit { putString(KEY_PAD_PROFILES, store.toString()) }
        syncPadProfileFolder()
        padProfileTick.value++
    }

    /** True if [serial] has ANY per-game controller override for [player]. Drives
     *  the Pad-tab "Game" scope badge so the user knows a game-specific map exists. */
    fun hasGameOverrides(serial: String?, player: Int): Boolean {
        if (serial.isNullOrEmpty()) return false
        if (actions.any { MainActivityRuntime.prefs.contains(gameKey(serial, playerPrefix(player) + KEY_PREFIX + it.id)) }) return true
        if (MainActivityRuntime.prefs.contains(gameKey(serial, playerPrefix(player) + KEY_LSTICK))) return true
        if (MainActivityRuntime.prefs.contains(gameKey(serial, playerPrefix(player) + KEY_RSTICK))) return true
        for (left in booleanArrayOf(true, false))
            for (dir in StickDir.values())
                if (MainActivityRuntime.prefs.contains(gameKey(serial, customKey(left, dir, player)))) return true
        return false
    }

    /** Reset the pad TUNABLES to defaults for the global "Reset to defaults" — stick
     *  feel (deadzone/sensitivity/acceleration), D-pad-as-left-stick, stick modes and
     *  CUSTOM stick-direction codes, for BOTH players. Does NOT touch the button binds
     *  (those have their own per-player Reset). Bumps stickBindTick so the Pad tab
     *  recomposes. (The button-bind sliders live outside the Settings object, which is
     *  why the Settings reset alone didn't clear them.) */
    fun resetTunables() {
        MainActivityRuntime.prefs.edit {
            remove(KEY_DPAD_AS_LSTICK)
                .remove(KEY_LSTICK_INVX).remove(KEY_LSTICK_INVY).remove(KEY_LSTICK_SWAP)
                .remove(KEY_RSTICK_INVX).remove(KEY_RSTICK_INVY).remove(KEY_RSTICK_SWAP)
            prefStickSens.reset(this); prefStickAccel.reset(this); prefStickDz.reset(this)
            prefStickOuter.reset(this); prefStickAntiDz.reset(this); prefStickCurve.reset(this)
            for (p in intArrayOf(P1, P2)) {
                remove(playerPrefix(p) + KEY_LSTICK).remove(playerPrefix(p) + KEY_RSTICK)
                for (left in booleanArrayOf(true, false))
                    for (dir in StickDir.values())
                        remove(customKey(left, dir, p))
            }
        }
        stickBindTick.value++
    }

    fun targetForPhysical(physicalKeyCode: Int, player: Int = 0): Int? {
        // Unbound actions store KEYCODE_UNKNOWN; never let a stray UNKNOWN event match
        // one (it would otherwise map to the first unbound action's PS2 button).
        if (physicalKeyCode == KeyEvent.KEYCODE_UNKNOWN) return null
        return actions.firstOrNull { physicalFor(it, player) == physicalKeyCode }?.targetKeyCode
    }

    // ---- Turbo / rapid-fire (per PS2 button, per player) -------------------
    // A turbo-flagged button, while its bound PHYSICAL controller button is held,
    // auto-presses/releases the PS2 button at ~15 Hz (MainActivityRuntime.handleTurbo). Global
    // (not per-game). Only the physical-controller dispatch consults this — the
    // on-screen touch buttons stay normal — matching "hold a back paddle to spam".
    private const val TURBO_PREFIX = "pad.turbo."
    private fun turboKey(action: Action, player: Int) = playerPrefix(player) + TURBO_PREFIX + action.id
    fun isTurboAction(action: Action, player: Int = 0): Boolean =
        MainActivityRuntime.prefs.getBoolean(turboKey(action, player), false)
    fun setTurboAction(action: Action, player: Int, on: Boolean) =
        MainActivityRuntime.prefs.edit { putBoolean(turboKey(action, player), on) }

    /** True when a physical button's PS2 target [targetKeyCode] is turbo-flagged. */
    fun isTurboTarget(targetKeyCode: Int, player: Int = 0): Boolean {
        val action = actions.firstOrNull { it.targetKeyCode == targetKeyCode } ?: return false
        return isTurboAction(action, player)
    }

    // ---- System hotkeys (menu / quick save / quick load) -----------------
    // Physical buttons bound to app actions, NOT forwarded to the PS2. Handled in
    // MainActivityRuntime.dispatchKeyEvent (so they can catch KEYCODE_BACK / back-paddle keys the
    // back dispatcher would otherwise swallow). KEYCODE_UNKNOWN = unbound.
    enum class SysHotkey(val prefKey: String, val label: String) {
        MENU("pad.menu.keycode", "Menu / Pause"),
        SAVE_STATE("pad.savestate.keycode", "Quick Save State"),
        LOAD_STATE("pad.loadstate.keycode", "Quick Load State"),
        CYCLE_SLOT("pad.cycleslot.keycode", "Cycle Save Slot"),
        TEXTURE_DUMP("pad.texdump.keycode", "Toggle Texture Dumping"),
        // Toggles the whole on-screen performance overlay (FPS/CPU/GPU/etc.) via
        // the same path as the on-screen OSD button, so the two stay in sync.
        TOGGLE_OSD("pad.toggleosd.keycode", "Toggle Perf Stats (OSD)"),
        FAST_FORWARD("pad.fastforward.keycode", "Fast Forward (hold)"),
        FAST_FORWARD_TOGGLE("pad.fastforwardtoggle.keycode", "Fast Forward (toggle)"),
        // Slow motion toggle (50% speed, native LimiterModeType::Slomo). DISABLED
        // while RetroAchievements hardcore is active — slowdown is a banned
        // advantage in hardcore, matching desktop PCSX2 (the handler shows an OSD
        // notice instead of engaging).
        SLOW_DOWN("pad.slowdown.keycode", "Slow Down (toggle)"),
        RES_UP("pad.resup.keycode", "Increase Resolution"),
        RES_DOWN("pad.resdown.keycode", "Decrease Resolution"),
        ACHIEVEMENTS("pad.achievements.keycode", "Open Achievements"),
        CLOSE_GAME("pad.closegame.keycode", "Close Game"),
        QUIT_APP("pad.quitapp.keycode", "Close Game & Quit"),
        SAVE_AND_EXIT("pad.saveandexit.keycode", "Save State & Exit"),
        RESET_GAME("pad.resetgame.keycode", "Reset Game"),
        // Hold-type binding: while the bound button is held, pressure-capable PS2
        // buttons report a soft (~50%) press. Handled as a HOLD in
        // MainActivityRuntime.dispatchKeyEvent (sets TouchControls.pressureModifierHeld), not as a
        // one-shot action like the others.
        PRESSURE_MOD("pad.pressuremod.keycode", "Pressure Modifier (hold)"),
        // Gyro on/off (issue #337) — bind any spare button so gyro can be silenced
        // mid-game without opening settings. TOGGLE flips it and stays; HOLD is the
        // "only while aiming" binding (gyro live only while the button is held, so the
        // phone can sit still the rest of the time). Both drive
        // MainActivityRuntime.gyroActive and are session-only, never persisted.
        GYRO_TOGGLE("pad.gyrotoggle.keycode", "Gyro On/Off (toggle)"),
        GYRO_HOLD("pad.gyrohold.keycode", "Gyro (hold to aim)"),
    }

    // A hotkey is either a single button or a two-button combo. The main key is
    // stored under prefKey; an optional modifier (held while the main key is
    // pressed) under prefKey + MOD_SUFFIX. UNKNOWN modifier = single-button.
    private const val MOD_SUFFIX = ".mod"

    fun hotkeyCode(h: SysHotkey): Int =
        MainActivityRuntime.prefs.getInt(h.prefKey, KeyEvent.KEYCODE_UNKNOWN)

    /** Modifier button that must be held with [hotkeyCode], or UNKNOWN for none. */
    fun hotkeyModCode(h: SysHotkey): Int =
        MainActivityRuntime.prefs.getInt(h.prefKey + MOD_SUFFIX, KeyEvent.KEYCODE_UNKNOWN)

    /** Bind a single-button hotkey (clears any modifier). */
    fun bindHotkey(h: SysHotkey, physicalKeyCode: Int) {
        MainActivityRuntime.prefs.edit {
            putInt(h.prefKey, physicalKeyCode)
                .putInt(h.prefKey + MOD_SUFFIX, KeyEvent.KEYCODE_UNKNOWN)
        }
    }

    /** Bind a two-button combo: [modCode] held + [keyCode] pressed. */
    fun bindHotkeyCombo(h: SysHotkey, modCode: Int, keyCode: Int) {
        MainActivityRuntime.prefs.edit {
            putInt(h.prefKey, keyCode)
                .putInt(h.prefKey + MOD_SUFFIX, modCode)
        }
    }

    fun clearHotkey(h: SysHotkey) {
        MainActivityRuntime.prefs.edit {
            putInt(h.prefKey, KeyEvent.KEYCODE_UNKNOWN)
                .putInt(h.prefKey + MOD_SUFFIX, KeyEvent.KEYCODE_UNKNOWN)
        }
    }

    /** Clear ALL system hotkey bindings (the global "Reset to defaults"). Bumps
     *  hotkeyBindTick so the Hotkeys tab recomposes. */
    fun clearAllHotkeys() {
        MainActivityRuntime.prefs.edit {
            SysHotkey.values().forEach {
                putInt(it.prefKey, KeyEvent.KEYCODE_UNKNOWN)
                    .putInt(it.prefKey + MOD_SUFFIX, KeyEvent.KEYCODE_UNKNOWN)
            }
        }
        hotkeyBindTick.value++
    }

    /** Human-readable binding, e.g. "Select + R1" or "L1", or "" if unbound. */
    fun hotkeyLabel(h: SysHotkey): String {
        val key = hotkeyCode(h)
        if (key == KeyEvent.KEYCODE_UNKNOWN) return ""
        val mod = hotkeyModCode(h)
        return if (mod == KeyEvent.KEYCODE_UNKNOWN) labelForKey(key)
        else "${labelForKey(mod)} + ${labelForKey(key)}"
    }

    /** Single-button match (combos excluded). Used by the frontend MENU shortcut. */
    fun hotkeyFor(physicalKeyCode: Int): SysHotkey? {
        if (physicalKeyCode == KeyEvent.KEYCODE_UNKNOWN) return null
        return SysHotkey.values().firstOrNull {
            hotkeyCode(it) == physicalKeyCode && hotkeyModCode(it) == KeyEvent.KEYCODE_UNKNOWN
        }
    }

    /** Combo-aware match for the just-pressed [keyCode] given the set of
     *  currently-held physical keys. Combos (modifier held) win over a plain
     *  single binding on the same key, so e.g. Select+R1 fires its action
     *  instead of a bare-R1 binding while Select is held. */
    fun matchHotkey(keyCode: Int, heldKeys: Set<Int>): SysHotkey? {
        if (keyCode == KeyEvent.KEYCODE_UNKNOWN) return null
        SysHotkey.values().firstOrNull {
            hotkeyCode(it) == keyCode &&
                hotkeyModCode(it) != KeyEvent.KEYCODE_UNKNOWN &&
                heldKeys.contains(hotkeyModCode(it))
        }?.let { return it }
        return SysHotkey.values().firstOrNull {
            hotkeyCode(it) == keyCode && hotkeyModCode(it) == KeyEvent.KEYCODE_UNKNOWN
        }
    }

    // True while the Pad tab is waiting for a button to bind. MainActivityRuntime.dispatchKeyEvent
    // checks this and lets EVERY key fall through to Compose's onPreviewKeyEvent so
    // any button — including B/A/Y/D-pad/L1/R1 the overlay nav would otherwise
    // consume (B = exit) — can be captured. Normal nav resumes when it clears.
    val padCapturing = mutableStateOf(false)

    /** A pad-button capture in progress from the Controls screen (ControllerManagerScreen). While
     *  set, MainActivityRuntime.dispatchKeyEvent binds the next pressed keycode via this lambda —
     *  handled there, like [captureHotkey], instead of a focus-stealing AlertDialog whose separate
     *  window swallowed controller keys before Compose's onPreviewKeyEvent could see them (the
     *  2.6.0 "can't remap buttons" bug). The lambda binds the key and returns true. */
    val capturePadAction = mutableStateOf<((Int) -> Boolean)?>(null)

    // Capture bridge: the Hotkeys tab calls [beginHotkeyCapture]; the next
    // button(s) seen by MainActivityRuntime.dispatchKeyEvent are bound to it. Press one button
    // for a single bind, or two together for a combo. Observed for UI feedback.
    val captureHotkey = mutableStateOf<SysHotkey?>(null)

    /** Ordered buffer of buttons pressed during an active capture (≤2 used). */
    val captureKeys = mutableListOf<Int>()

    /**
     * eventTime (uptimeMillis) of the first DOWN in the current capture. Used to
     * reject a 2nd keycode that arrives near-simultaneously — some controllers
     * emit two keycodes for one physical press, which would otherwise be misread
     * as a 2-button combo and make single-button hotkeys impossible to bind.
     */
    var captureFirstDownMs = 0L

    /** Start capturing a (re)binding for [h]. */
    fun beginHotkeyCapture(h: SysHotkey) {
        captureKeys.clear()
        captureHotkey.value = h
    }

    /** End the current capture session. */
    fun endHotkeyCapture() {
        captureKeys.clear()
        captureHotkey.value = null
        hotkeyBindTick.value++
    }

    /** Bumped after a (re)bind so observing UI recomposes. */
    val hotkeyBindTick = mutableStateOf(0)
}
