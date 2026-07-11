package com.armsx2.ui.touch

import android.view.KeyEvent
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import com.armsx2.EmuState
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.InGameOverlay
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import androidx.core.content.edit

/**
 * On-screen touch controls — state, persistence, and the runtime
 * input-mode latch.
 *
 * Three storage tiers in MainActivityRuntime.prefs (single "ARMSX2" SharedPreferences):
 *   - `touch.profiles`            JSON array of {name, layout}
 *   - `touch.active`              Currently-selected profile name
 *
 * The "active layout" is always a copy of the active profile's layout —
 * edits live there until the user explicitly saves them back into a
 * profile (Save / Save As New).
 */
object TouchControls {
    private const val KEY_PROFILES = "touch.profiles"
    private const val KEY_ACTIVE = "touch.active"
    private const val KEY_OPACITY = "touch.opacity"
    private const val KEY_FACE_MULTI = "touch.faceMulti"
    private const val KEY_TOUCH_GLIDING = "touch.gliding"
    private const val KEY_TOUCH_HAPTICS = "touch.haptics"
    private const val KEY_MULTI_RADIUS = "touch.multiRadius"
    private const val KEY_DPAD_SPACING = "touch.dpadSpacing"
    private const val KEY_FLOATING_STICK = "touch.floatingStick"
    private const val KEY_VIS_MODE = "touch.visibilityMode"
    // One-shot 2.4.7 defaults migration for EXISTING users (saved prefs/layouts
    // predate the default changes, so the new defaults wouldn't otherwise apply).
    private const val KEY_DEFAULTS_MIGRATED_247 = "touch.defaults.migrated.247"
    // Per-game active-profile override: touch.active.game.<serial> -> profile name.
    private const val KEY_ACTIVE_GAME_PREFIX = "touch.active.game."
    // Per-game CUSTOM layout: touch.layout.game.<serial> -> layout JSON. This is
    // an independent per-serial layout (NOT a shared profile object), so editing
    // one game's layout never mutates what another game loads.
    private const val KEY_LAYOUT_GAME_PREFIX = "touch.layout.game."
    // Portable on-disk mirror of profiles, under <DataRoot>/inputprofiles/.
    private const val PROFILE_FILE_SUFFIX = ".touch.json"

    /** Visible to the user. False when a controller is being used (latched
     *  off in onControllerInputDetected); flipped back on by any screen
     *  touch via onSurfaceTouched. Default true so first-run users see
     *  the controls. */
    val visible = mutableStateOf(true)

    /** Edit mode — buttons are draggable + resizable, JNI pad writes are
     *  suppressed. Toggled from InGameOverlay's "Edit Touch Controls"
     *  row. */
    val editMode = mutableStateOf(false)

    /** Leave layout-edit mode and resume the game. Edit mode is entered from
     *  the (paused) pause overlay and runs with the VM paused for a stable
     *  editing screen, so on exit we must resume — otherwise the game stays
     *  frozen with no overlay shown until the user re-opens the menu and hits
     *  Resume (the "emulator froze after saving a controller profile" report).
     *  No-op if the VM isn't paused. */
    fun exitEditMode() {
        editMode.value = false
        if (MainActivityRuntime.eState.value == EmuState.PAUSED) MainActivityRuntime.resume()
    }

    /** Currently-selected widget in edit mode. When non-null, the edit
     *  toolbar exposes a size slider for the selected widget — useful
     *  for resizing tiny buttons (L3/R3, Start/Select) that are
     *  awkward to pinch-zoom. Tap a widget to select; tap the dim
     *  backdrop to deselect. */
    val selectedButton = mutableStateOf<TouchButtonId?>(null)

    /** Profile picker / save-as dialog shown over the editor. */
    val profileDialogOpen = mutableStateOf(false)

    /** All saved profiles. Mutated via mutators below so observers
     *  recompose. */
    val profiles = mutableStateListOf<TouchProfile>()

    /** Name of the currently-active profile. Persisted. */
    val activeProfileName = mutableStateOf("Default")

    /** Live layout being rendered + edited. Diverges from the saved
     *  profile while editing; "Save" commits it back, "Discard" reloads. */
    val activeLayout = mutableStateOf(TouchLayout.default())

    /** Master opacity 0.20..1.00. Persisted. */
    val opacity = mutableFloatStateOf(0.55f)

    /** When enabled, the face-button diamond has a shared hit layer so a
     *  single thumb can slide/press between Cross/Square/Circle/Triangle and
     *  emit overlapping button presses. */
    // Multi-touch hit-test layer for face + shoulder buttons (lets you press
    // several at once / roll between them, and press them while the stick is
    // held). Persisted under KEY_FACE_MULTI. Default ON.
    val faceMultiTouch = mutableStateOf(true)

    // Touch Gliding (NetherSX2-style): while ON, dragging a finger LATCHES every
    // button it crosses (held until the finger lifts) instead of only the one it's
    // currently over — so you can hold several face/shoulder buttons with one drag.
    // Requires the multi-touch layer (faceMultiTouch). Default OFF. Under KEY_TOUCH_GLIDING.
    val touchGliding = mutableStateOf(false)

    // Touch Haptics (issue #247, PPSSPP/Azahar-style): a short vibration tick on every
    // on-screen button press (via NativeApp.touchHaptic). Independent of game rumble.
    // Default ON. Under KEY_TOUCH_HAPTICS.
    val touchHaptics = mutableStateOf(true)

    // Multi-touch hit radius as a fraction of a button's size (GGPO-style). Higher =
    // buttons register a press from further out, so multitouch/rolling works with more
    // space between them. Persisted under KEY_MULTI_RADIUS. Default 0.62.
    val multiTouchRadius = mutableFloatStateOf(0.62f)

    // On-screen D-pad key spacing, as a fraction of the pad's half-size. 0 = the four
    // directions meet at the center (a tight +). Higher pushes each direction OUT toward
    // its edge, opening a visible gap in the middle (NetherSX2-style) and growing the
    // center dead-zone to match. Edited in the Touch Layout editor (select the D-Pad).
    // Persisted under KEY_DPAD_SPACING. Default 0 (normal tight D-pad).
    val dpadSpacing = mutableFloatStateOf(0.0f)

    // Floating on-screen stick: the first touch-down inside a stick's zone becomes
    // its origin (the ring re-centers under your finger) instead of a fixed center —
    // easier to grab without looking. Snaps back to rest on release. Global; persisted
    // under KEY_FLOATING_STICK.
    val floatingStick = mutableStateOf(false)

    /** Held-state for the DS2 pressure-sensitivity modifier. While true, the
     *  pressure-capable buttons report ~50% pressure (PCSX2's
     *  DEFAULT_PRESSURE_MODIFIER) for soft presses (MGS, GTA). Driven by the
     *  on-screen PRESSURE button and the bound physical button. */
    val pressureModifierHeld = mutableStateOf(false)

    /** ~50% of the native 0..32767 range → state≈0.5 in setPadButton. */
    const val PRESSURE_HALF_RANGE = 16383

    // PS2 DualShock2 pressure-sensitive inputs (keycodes match native-lib.cpp's
    // setPadButton map): d-pad, face buttons, L1/L2/R1/R2. Start/Select/L3/R3 are
    // digital-only, so the modifier never touches them.
    private val PRESSURE_KEYCODES = setOf(
        19, 20, 21, 22,        // d-pad up/down/left/right
        96, 97, 99, 100,       // cross/circle/square/triangle
        102, 103, 104, 105,    // L1/R1/L2/R2
    )

    /** Range to send for [keycode] given the current modifier state: a reduced
     *  (soft) range while the modifier is held on a pressure-capable button,
     *  else 0 (full press). */
    fun pressureRangeFor(keycode: Int): Int =
        if (pressureModifierHeld.value && keycode in PRESSURE_KEYCODES) PRESSURE_HALF_RANGE else 0

    /** On-screen touch controls visibility. 0 = Never show (for physical-
     *  controls devices like the RP6 — also hides the settings cog so nothing
     *  overlaps R1); 1..10 = auto-hide after that many seconds of no touch;
     *  11 = Auto — show on screen touch, hide when a controller is used (the
     *  default / legacy behavior). Persisted. */
    val visibilityMode = mutableIntStateOf(11)

    /** Bumped on every touch interaction (screen tap or on-screen button press)
     *  so the auto-hide timer restarts. Not persisted. */
    val interactionTick = mutableIntStateOf(0)

    // ---- On-screen macro / combo buttons (Macro1-4) ----------------------------
    // Each macro fires a user-chosen SET of pad buttons at once (e.g. R1+R2+R3).
    // Stored per macro under touch.macro.<id> = comma-separated TouchButtonId names.
    private const val KEY_MACRO_PREFIX = "touch.macro."

    /** Bumped when any macro's button set changes so the config UI + overlay recompose. */
    val macroBindTick = mutableIntStateOf(0)

    /** The discrete pad buttons a macro may fire (face / shoulders / L3 R3 / Start /
     *  Select). No D-pad/sticks (directional) or pause/macro/fast-forward (not pad
     *  buttons). Order = the config dialog's display order. */
    val macroAssignableButtons: List<TouchButtonId> = listOf(
        TouchButtonId.TRIANGLE, TouchButtonId.CIRCLE, TouchButtonId.CROSS, TouchButtonId.SQUARE,
        TouchButtonId.L1, TouchButtonId.R1, TouchButtonId.L2, TouchButtonId.R2,
        TouchButtonId.L3, TouchButtonId.R3, TouchButtonId.START, TouchButtonId.SELECT,
    )

    /** Buttons macro [id] fires, in config order (empty if unconfigured). */
    fun macroButtons(id: TouchButtonId): List<TouchButtonId> {
        val raw = MainActivityRuntime.prefs.getString(KEY_MACRO_PREFIX + id.name, "").orEmpty()
        if (raw.isEmpty()) return emptyList()
        val set = raw.split(",").mapNotNull { runCatching { TouchButtonId.valueOf(it) }.getOrNull() }.toSet()
        // Keep macroAssignableButtons order, drop anything stale/non-assignable.
        return macroAssignableButtons.filter { it in set }
    }

    fun setMacroButtons(id: TouchButtonId, buttons: List<TouchButtonId>) {
        val csv = macroAssignableButtons.filter { it in buttons.toSet() }.joinToString(",") { it.name }
        MainActivityRuntime.prefs.edit { putString(KEY_MACRO_PREFIX + id.name, csv) }
        macroBindTick.intValue++
    }

    // Optional PHYSICAL-controller trigger per macro: bind a physical button to fire
    // the macro's button set (the same set the on-screen M1-M4 buttons use). Stored
    // separately so a macro can be touch-only, physical-only, or both. KEYCODE_UNKNOWN
    // (0) = no physical trigger.
    private const val KEY_MACRO_PHYS_PREFIX = "touch.macro.phys."

    fun macroPhysicalCode(id: TouchButtonId): Int =
        MainActivityRuntime.prefs.getInt(KEY_MACRO_PHYS_PREFIX + id.name, KeyEvent.KEYCODE_UNKNOWN)

    fun setMacroPhysicalCode(id: TouchButtonId, keycode: Int) {
        MainActivityRuntime.prefs.edit { putInt(KEY_MACRO_PHYS_PREFIX + id.name, keycode)}
        macroBindTick.intValue++
    }

    fun clearMacroPhysicalCode(id: TouchButtonId) = setMacroPhysicalCode(id, KeyEvent.KEYCODE_UNKNOWN)

    /** The macro a physical [keycode] triggers — only if it's bound AND has buttons
     *  configured. Checked in the gameplay key path (Main) before normal pad routing. */
    fun macroForPhysicalCode(keycode: Int): TouchButtonId? {
        if (keycode == KeyEvent.KEYCODE_UNKNOWN) return null
        for (id in listOf(TouchButtonId.MACRO1, TouchButtonId.MACRO2, TouchButtonId.MACRO3, TouchButtonId.MACRO4)) {
            if (macroPhysicalCode(id) == keycode && macroButtons(id).isNotEmpty()) return id
        }
        return null
    }

    /** Set true once load() has run — used to avoid clobbering disk state
     *  on first composition. */
    private var loaded = false

    fun ensureLoaded() {
        if (loaded) return
        loaded = true
        load()
    }

    private fun load() {
        val raw = MainActivityRuntime.prefs.getString(KEY_PROFILES, null)
        val list = mutableListOf<TouchProfile>()
        if (raw != null) {
            runCatching {
                val arr = JSONArray(raw)
                for (i in 0 until arr.length()) {
                    val obj = arr.getJSONObject(i)
                    list.add(TouchProfile.fromJson(obj))
                }
            }
        }
        // Merge in portable profiles from <DataRoot>/inputprofiles/ that aren't
        // already in prefs — lets profiles survive a data-folder move (the user's
        // pain point) and be shared/hand-dropped. Prefs stays the live source.
        importFolderProfilesInto(list)
        if (list.isEmpty()) {
            list.add(TouchProfile("Default", TouchLayout.default()))
        }
        profiles.clear()
        profiles.addAll(list)

        val active = MainActivityRuntime.prefs.getString(KEY_ACTIVE, list.first().name) ?: list.first().name
        activeProfileName.value = active
        val match = list.firstOrNull { it.name == active } ?: list.first()
        activeLayout.value = match.layout.copy()
        opacity.floatValue = MainActivityRuntime.prefs.getFloat(KEY_OPACITY, 0.55f).coerceIn(0.20f, 1.0f)
        faceMultiTouch.value = MainActivityRuntime.prefs.getBoolean(KEY_FACE_MULTI, true)
        touchGliding.value = MainActivityRuntime.prefs.getBoolean(KEY_TOUCH_GLIDING, false)
        touchHaptics.value = MainActivityRuntime.prefs.getBoolean(KEY_TOUCH_HAPTICS, true)
        multiTouchRadius.floatValue = MainActivityRuntime.prefs.getFloat(KEY_MULTI_RADIUS, 0.62f).coerceIn(0.50f, 0.95f)
        dpadSpacing.floatValue = MainActivityRuntime.prefs.getFloat(KEY_DPAD_SPACING, 0.0f).coerceIn(0.0f, 0.35f)
        floatingStick.value = MainActivityRuntime.prefs.getBoolean(KEY_FLOATING_STICK, false)
        visibilityMode.intValue = MainActivityRuntime.prefs.getInt(KEY_VIS_MODE, 11).coerceIn(0, 11)
        if (visibilityMode.intValue == 0) visible.value = false

        // One-shot 2.4.7 defaults migration: existing users have saved prefs/layouts
        // that predate the new defaults (multi-touch was off, the Pressure button was
        // visible), so the default flips above don't reach them. Apply once; after this
        // the user's own choices stick.
        if (!MainActivityRuntime.prefs.getBoolean(KEY_DEFAULTS_MIGRATED_247, false)) {
            faceMultiTouch.value = true
            fun hidePressure(layout: TouchLayout): TouchLayout = layout.copy(
                buttons = layout.buttons.map {
                    if (it.id.kind == TouchButtonId.Kind.PRESSURE) it.copy(enabled = false) else it
                }
            )
            for (i in profiles.indices) profiles[i] = profiles[i].copy(layout = hidePressure(profiles[i].layout))
            activeLayout.value = hidePressure(activeLayout.value)
            MainActivityRuntime.prefs.edit { putBoolean(KEY_DEFAULTS_MIGRATED_247, true) }
            persist()
        }
    }

    private fun persist() {
        val arr = JSONArray()
        for (p in profiles) arr.put(p.toJson())
        MainActivityRuntime.prefs.edit {
            putString(KEY_PROFILES, arr.toString())
                .putString(KEY_ACTIVE, activeProfileName.value)
                .putFloat(KEY_OPACITY, opacity.floatValue)
                .putBoolean(KEY_FACE_MULTI, faceMultiTouch.value)
                .putBoolean(KEY_TOUCH_GLIDING, touchGliding.value)
                .putBoolean(KEY_TOUCH_HAPTICS, touchHaptics.value)
                .putFloat(KEY_MULTI_RADIUS, multiTouchRadius.floatValue)
                .putFloat(KEY_DPAD_SPACING, dpadSpacing.floatValue)
                .putBoolean(KEY_FLOATING_STICK, floatingStick.value)
                .putInt(KEY_VIS_MODE, visibilityMode.intValue)
        }
        syncFolder()
    }

    /** Set the on-screen controls visibility mode (see [visibilityMode]). */
    fun setVisibilityMode(mode: Int) {
        visibilityMode.intValue = mode.coerceIn(0, 11)
        // Reflect immediately: Never hides; any other mode shows.
        visible.value = visibilityMode.intValue != 0
        interactionTick.intValue++
        persist()
    }

    /** Note a touch interaction (screen tap or on-screen button press): show
     *  the controls (unless disabled) and restart the auto-hide timer. */
    fun noteTouchInteraction() {
        if (visibilityMode.intValue == 0) return
        if (!visible.value) visible.value = true
        interactionTick.intValue++
    }

    /** Commit the live edit. When a game is running, store the edited layout as
     *  that game's OWN per-serial layout (touch.layout.game.<serial>) so it is
     *  isolated from every other game and from the shared profiles — this is what
     *  stops one game's edit from bleeding into the next. When no game is running
     *  (library/global edit), fall back to overwriting the active profile so the
     *  global Default still reflects the edit. */
    fun saveLiveLayoutToActive() {
        // gameIsRunning() is the OUTER gate. The per-serial/CRC isolation paths
        // must only run when a VM is actually up — keying off a merely-non-null
        // serial was wrong because MainActivityRuntime.currentGame (hence runningSerial()) stays
        // stale after Close Game, so a Global-Default edit from the main menu
        // would silently write into the LAST-PLAYED game's per-serial key.
        if (gameIsRunning()) {
            val serial = runningSerial()
            if (serial != null) {
                // In-game with a resolved serial -> isolated per-serial layout.
                // Never touches any shared profile.
                MainActivityRuntime.prefs.edit {
                    putString(
                        KEY_LAYOUT_GAME_PREFIX + serial,
                        activeLayout.value.toJson().toString(),
                    )
                }
            } else {
                // In-game but no serial (homebrew / BIOS / serial-less disc).
                // Key by disc CRC so it stays isolated; if even the CRC is
                // unknown, no-op with a warning rather than corrupting Default.
                val crc = runCatching { NativeApp.getGameCRC() }.getOrNull()
                    ?.trim()?.uppercase()?.takeIf { it.isNotEmpty() && it != "00000000" }
                if (crc != null) {
                    MainActivityRuntime.prefs.edit {
                        putString(
                            KEY_LAYOUT_GAME_PREFIX + "crc." + crc,
                            activeLayout.value.toJson().toString(),
                        )
                    }
                } else {
                    println(
                        "@@ARMSX2_TOUCH@@ refusing to save in-game layout to global " +
                            "Default: no serial/CRC resolved for running game"
                    )
                }
            }
        } else {
            // No game running (library / Global Default edit) -> overwrite the
            // active profile.
            val idx = profiles.indexOfFirst { it.name == activeProfileName.value }
            if (idx >= 0) {
                profiles[idx] = profiles[idx].copy(layout = activeLayout.value.copy())
                persist()
            }
        }
        selectedButton.value = null
    }

    /** Persist the live edit state under a new profile name. If the name
     *  collides, the existing profile is overwritten. Switches to the new
     *  profile. */
    fun saveAsNewProfile(name: String) {
        val trimmed = name.trim().ifEmpty { return }
        val newProf = TouchProfile(trimmed, activeLayout.value.copy())
        val existing = profiles.indexOfFirst { it.name == trimmed }
        if (existing >= 0) profiles[existing] = newProf
        else profiles.add(newProf)
        activeProfileName.value = trimmed
        // Bind the new profile to the game currently running so it only applies
        // to THAT game (otherwise it becomes the globally-active profile and
        // bleeds into every other game's boot). No game running -> global only.
        // Gate on gameIsRunning() so a stale MainActivityRuntime.currentGame (after Close Game)
        // can't bind this profile to the last-played game from the library.
        val serial = if (gameIsRunning()) runningSerial() else null
        if (serial != null)
            MainActivityRuntime.prefs.edit { putString(KEY_ACTIVE_GAME_PREFIX + serial, trimmed) }
        persist()
    }

    fun switchProfile(name: String) {
        val match = profiles.firstOrNull { it.name == name } ?: return
        activeProfileName.value = name
        activeLayout.value = match.layout.copy()
        // When a game is running, remember this profile FOR that game so it
        // auto-applies on the next boot (per-game tier). With no game (library),
        // it's just the global default, persisted via KEY_ACTIVE in persist().
        // Gate on gameIsRunning() so a stale MainActivityRuntime.currentGame (after Close Game)
        // can't bind this from the library to the last-played game.
        val serial = if (gameIsRunning()) runningSerial() else null
        if (serial != null)
            MainActivityRuntime.prefs.edit { putString(KEY_ACTIVE_GAME_PREFIX + serial, name) }
        persist()
    }

    fun deleteProfile(name: String) {
        if (profiles.size <= 1) return  // never delete the last profile
        val idx = profiles.indexOfFirst { it.name == name }
        if (idx < 0) return
        profiles.removeAt(idx)
        if (activeProfileName.value == name) {
            val fallback = profiles.first()
            activeProfileName.value = fallback.name
            activeLayout.value = fallback.layout.copy()
        }
        // Drop any per-game overrides that pointed at the deleted profile.
        clearGameOverridesFor(name)
        persist()
    }

    fun resetActiveToDefault() {
        activeLayout.value = TouchLayout.default()
    }

    /** True when a VM is up (RUNNING or PAUSED) — i.e. an in-game edit, where we
     *  must NEVER fall back to overwriting the shared Default profile. */
    private fun gameIsRunning(): Boolean =
        MainActivityRuntime.eState.value == EmuState.RUNNING || MainActivityRuntime.eState.value == EmuState.PAUSED

    /** Authoritative serial of the booted disc straight from the core. Returns a
     *  clean "AAAA-NNNNN" with no CRC/paren formatting, regardless of how the
     *  game was launched (library card, raw path, file picker, external). Empty
     *  string from native = no disc loaded. */
    private fun coreSerial(): String? =
        runCatching { NativeApp.getGameSerial() }.getOrNull()?.takeIf { it.isNotEmpty() }

    /** Serial of the game currently running. Source order:
     *   1. launch-time GameInfo.serial (set before the VM starts) — lets per-game
     *      binding work from the first edit;
     *   2. the AUTHORITATIVE core disc serial (VMManager::GetDiscSerial via JNI),
     *      which knows the serial even when the filename had no serial token and
     *      the overlay never populated InGameOverlay.currentSerial;
     *   3. InGameOverlay.currentSerial as a last resort (may be a formatted
     *      "SERIAL (CRC)" string — only use if the cleaner sources are empty). */
    private fun runningSerial(): String? =
        MainActivityRuntime.currentGame.value?.serial?.takeIf { it.isNotEmpty() }
            ?: coreSerial()
            ?: InGameOverlay.currentSerial.value?.takeIf { it.isNotEmpty() }

    // ---- Per-game tier + portable inputprofiles/ folder ----

    /** Apply this game's touch layout on boot. Precedence:
     *   1. A per-serial CUSTOM layout saved by the drag-and-Save editor
     *      (touch.layout.game.<serial>) — fully isolated per game.
     *   2. A legacy per-game profile-NAME binding (touch.active.game.<serial>),
     *      written by the Profiles dialog — load that profile's layout.
     *   3. No binding at all -> reset to the "Default" profile so an un-customized
     *      game shows the default layout, NOT whatever named profile is globally
     *      active (otherwise a "GT4" profile bleeds into every other game).
     *  Does NOT persist (auto-apply must not overwrite the global selection). */
    fun applyForSerial(serial: String?) {
        // Fall back to the authoritative core serial when the caller's serial is
        // null/empty (filename had no serial token) so a per-serial layout still
        // re-applies regardless of launch path.
        val effSerial = serial?.takeIf { it.isNotEmpty() } ?: coreSerial()
        if (effSerial == null) {
            // No serial — try the per-disc CRC layout key used by the in-game save
            // safety net before giving up.
            val crc = runCatching { NativeApp.getGameCRC() }.getOrNull()
                ?.trim()?.uppercase()?.takeIf { it.isNotEmpty() && it != "00000000" }
            if (crc != null) {
                val rawCrc = MainActivityRuntime.prefs.getString(KEY_LAYOUT_GAME_PREFIX + "crc." + crc, null)
                if (rawCrc != null) {
                    runCatching { TouchLayout.fromJson(JSONObject(rawCrc)) }.getOrNull()?.let {
                        activeProfileName.value = "Default"
                        activeLayout.value = it
                        return
                    }
                }
            }
            return
        }
        // (1) Per-serial custom layout.
        val rawLayout = MainActivityRuntime.prefs.getString(KEY_LAYOUT_GAME_PREFIX + effSerial, null)
        if (rawLayout != null) {
            runCatching { TouchLayout.fromJson(JSONObject(rawLayout)) }.getOrNull()?.let {
                activeProfileName.value = "Default"
                activeLayout.value = it
                return
            }
        }
        // (2) Legacy per-game profile-name binding (Profiles dialog).
        val name = MainActivityRuntime.prefs.getString(KEY_ACTIVE_GAME_PREFIX + effSerial, null)
        if (name != null) {
            val match = profiles.firstOrNull { it.name == name }
            if (match != null) {
                activeProfileName.value = name
                activeLayout.value = match.layout.copy()
                return
            }
        }
        // (3) No per-game record: reset to the "Default" profile (NOT the globally
        //     active profile) so an un-customized game never inherits another
        //     game's named layout.
        val def = profiles.firstOrNull { it.name == "Default" } ?: profiles.firstOrNull()
        if (def != null) {
            activeProfileName.value = def.name
            activeLayout.value = def.layout.copy()
        } else {
            activeProfileName.value = "Default"
            activeLayout.value = TouchLayout.default()
        }
    }

    private fun clearGameOverridesFor(profileName: String) {
        MainActivityRuntime.prefs.edit {
            for ((k, v) in MainActivityRuntime.prefs.all) {
                if (k.startsWith(KEY_ACTIVE_GAME_PREFIX) && v == profileName) remove(k)
            }
        }
    }

    /** Clear any per-serial custom layout for [serial] so the game reverts to the
     *  active/Default profile on next boot (used by the editor's Reset chip). */
    fun clearGameLayout(serial: String?) {
        if (serial == null) return
        MainActivityRuntime.prefs.edit {remove(KEY_LAYOUT_GAME_PREFIX + serial) }
    }

    /** Reset chip: only clear the running game's per-serial layout when a VM is
     *  actually up. From the library (Global Default edit) there is no per-game
     *  key to clear, and MainActivityRuntime.currentGame may still point at the last-played
     *  game — so resolving a serial there would wrongly delete that game's
     *  custom layout. */
    fun clearGameLayoutIfRunning() {
        if (gameIsRunning()) clearGameLayout(runningSerial())
    }

    /** `<DataRoot>/inputprofiles/` (native creates it on init). Null when no
     *  system dir is configured yet. */
    private fun profilesDir(): File? {
        // systemDirPosix() is null for the DEFAULT (private app folder) — fall
        // back to getExternalFilesDir (= Android/data/<pkg>/files), which is
        // exactly where the native core puts EmuFolders::InputProfiles.
        val root = MainActivityRuntime.systemDirPosix()
            ?: MainActivityRuntime.instance?.applicationContext?.getExternalFilesDir(null)?.absolutePath
            ?: return null
        val dir = File(root, "inputprofiles")
        if (!dir.exists()) runCatching { dir.mkdirs() }
        return if (dir.isDirectory) dir else null
    }

    private fun fileNameFor(name: String): String =
        name.replace(Regex("[^A-Za-z0-9 _.-]"), "_") + PROFILE_FILE_SUFFIX

    /** Mirror the in-memory profiles to the portable folder: write each one and
     *  delete orphaned files (covers delete/rename). Best-effort. */
    private fun syncFolder() {
        val dir = profilesDir() ?: return
        runCatching {
            val want = HashMap<String, TouchProfile>()
            for (p in profiles) want[fileNameFor(p.name)] = p
            for ((fn, p) in want) File(dir, fn).writeText(p.toJson().toString())
            dir.listFiles { f -> f.name.endsWith(PROFILE_FILE_SUFFIX) }?.forEach { f ->
                if (f.name !in want) runCatching { f.delete() }
            }
        }
    }

    /** Add portable folder profiles not already present (by name) into [list]. */
    private fun importFolderProfilesInto(list: MutableList<TouchProfile>) {
        val dir = profilesDir() ?: return
        runCatching {
            val have = list.map { it.name }.toHashSet()
            dir.listFiles { f -> f.name.endsWith(PROFILE_FILE_SUFFIX) }
                ?.sortedBy { it.name }
                ?.forEach { f ->
                    runCatching {
                        val prof = TouchProfile.fromJson(JSONObject(f.readText()))
                        if (prof.name !in have) { list.add(prof); have.add(prof.name) }
                    }
                }
        }
    }

    /** Reload the live layout from the saved active profile, discarding
     *  any unsaved edits. */
    fun discardEdits() {
        val match = profiles.firstOrNull { it.name == activeProfileName.value }
        if (match != null) activeLayout.value = match.layout.copy()
        selectedButton.value = null
    }

    fun setOpacity(o: Float) {
        opacity.floatValue = o.coerceIn(0.20f, 1.0f)
        persist()
    }

    fun setFaceMultiTouch(enabled: Boolean) {
        faceMultiTouch.value = enabled
        persist()
    }

    fun setTouchGliding(enabled: Boolean) {
        touchGliding.value = enabled
        persist()
    }

    fun setTouchHaptics(enabled: Boolean) {
        touchHaptics.value = enabled
        persist()
    }

    fun setMultiTouchRadius(v: Float) {
        multiTouchRadius.floatValue = v.coerceIn(0.50f, 0.95f)
        persist()
    }

    fun setDpadSpacing(v: Float) {
        dpadSpacing.floatValue = v.coerceIn(0.0f, 0.35f)
        persist()
    }

    fun setFloatingStick(enabled: Boolean) {
        floatingStick.value = enabled
        persist()
    }

    /** Update a single button in the live layout. */
    fun updateButton(id: TouchButtonId, transform: (TouchButtonCfg) -> TouchButtonCfg) {
        val current = activeLayout.value
        val newButtons = current.buttons.map { if (it.id == id) transform(it) else it }
        activeLayout.value = current.copy(buttons = newButtons)
    }

    /** Latched off the touch controls when a controller key/axis fires.
     *  Only in "Auto" mode (11) — when an auto-hide timeout is set (1..10) the
     *  timer owns hiding, and "Never" (0) is already hidden. Idempotent. */
    fun onControllerInputDetected() {
        if (visibilityMode.intValue == 11 && visible.value) visible.value = false
    }

    /** Latched on by any pointer-down on the surface so a controller user
     *  who touches the screen sees the controls again (and restarts the
     *  auto-hide timer). No-op when controls are disabled. */
    fun onSurfaceTouched() {
        noteTouchInteraction()
    }
}

/** Stable id for a touch widget. The keycode is the canonical primary
 *  keycode the widget emits (digital buttons emit one code; the DPad +
 *  sticks emit four codes derived from the four cardinal directions —
 *  the keycode here is the "up" / first code for serialization id
 *  purposes only, the rendering layer maps internally). */
enum class TouchButtonId(val label: String, val keycode: Int, val kind: Kind) {
    CROSS("✕", KeyEvent.KEYCODE_BUTTON_A, Kind.FACE),
    CIRCLE("○", KeyEvent.KEYCODE_BUTTON_B, Kind.FACE),
    SQUARE("□", KeyEvent.KEYCODE_BUTTON_X, Kind.FACE),
    TRIANGLE("△", KeyEvent.KEYCODE_BUTTON_Y, Kind.FACE),
    L1("L1", KeyEvent.KEYCODE_BUTTON_L1, Kind.SHOULDER),
    R1("R1", KeyEvent.KEYCODE_BUTTON_R1, Kind.SHOULDER),
    L2("L2", KeyEvent.KEYCODE_BUTTON_L2, Kind.SHOULDER),
    R2("R2", KeyEvent.KEYCODE_BUTTON_R2, Kind.SHOULDER),
    START("Start", KeyEvent.KEYCODE_BUTTON_START, Kind.MENU),
    SELECT("Select", KeyEvent.KEYCODE_BUTTON_SELECT, Kind.MENU),
    // L3 / R3 — separate stick-CLICK buttons (the press-the-thumbstick
    // action). The L_STICK / R_STICK widgets only emit axis movement;
    // these emit the THUMBL / THUMBR keycodes.
    L3("L3", KeyEvent.KEYCODE_BUTTON_THUMBL, Kind.MENU),
    R3("R3", KeyEvent.KEYCODE_BUTTON_THUMBR, Kind.MENU),
    DPAD("D-Pad", KeyEvent.KEYCODE_DPAD_UP, Kind.DPAD),
    L_STICK("L-Stick", 110, Kind.STICK),
    R_STICK("R-Stick", 120, Kind.STICK),
    // Invisible long-press hotspot that opens the in-game pause overlay.
    // Replaced the old long-press-anywhere-on-the-surface gesture, which
    // fired on accidental presses in empty space mid-game. Emits no pad
    // keycode (0 = unused); renders nothing in play mode, shows an
    // outlined "PAUSE" box in edit mode so it can be moved/resized.
    PAUSE("Pause", 0, Kind.PAUSE),

    // On-screen fast-forward (Turbo) toggle. Emits no PS2 keycode; tapping it
    // toggles locked fast-forward via MainActivityRuntime.toggleFastForward() — the same action
    // as the FAST_FORWARD_TOGGLE hotkey. Opt-in: disabled in the default layout.
    // Rendered by FastForwardWidget.
    FAST_FORWARD("▶▶", 0, Kind.FASTFORWARD),

    // On-screen quick save-state / load-state buttons (to the active slot). Emit no
    // PS2 keycode; tapping calls MainActivityRuntime.saveState() / MainActivityRuntime.loadState() — the same actions
    // as the SAVE_STATE/LOAD_STATE hotkeys. Opt-in (disabled in the default layout).
    SAVE_STATE("SAVE", 0, Kind.STATEACTION),
    LOAD_STATE("LOAD", 0, Kind.STATEACTION),

    // Macro / combo buttons: each fires a user-chosen SET of pad buttons at once
    // (e.g. R1+R2+R3). Emits no keycode of its own; the set is configured per macro
    // (TouchControls.macroButtons) and dispatched by MacroWidget. Opt-in (disabled in
    // the default layout).
    MACRO1("M1", 0, Kind.MACRO),
    MACRO2("M2", 0, Kind.MACRO),
    MACRO3("M3", 0, Kind.MACRO),
    MACRO4("M4", 0, Kind.MACRO),

    // Pressure-sensitivity modifier. Emits no PS2 keycode; while held it sets
    // TouchControls.pressureModifierHeld so pressure-capable buttons report a
    // soft (~50%) press. Rendered by PressureButtonWidget.
    PRESSURE("P½", 0, Kind.PRESSURE);

    enum class Kind { FACE, SHOULDER, MENU, DPAD, STICK, PAUSE, PRESSURE, FASTFORWARD, MACRO, STATEACTION }
}

/** Position + size for a single widget. xFrac / yFrac are anchor-point
 *  fractions of screen width/height (0..1, 0,0 = top-left). sizeDp is
 *  the widget's outer diameter / largest side. */
data class TouchButtonCfg(
    val id: TouchButtonId,
    val xFrac: Float,
    val yFrac: Float,
    val sizeDp: Float,
    val enabled: Boolean = true,
    /** Tap-to-hold / latch: a tap toggles the button held (stays pressed until
     *  tapped again) instead of momentary press. Per-button, opt-in. */
    val tapToHold: Boolean = false,
) {
    fun toJson(): JSONObject = JSONObject().apply {
        put("id", id.name)
        put("x", xFrac.toDouble())
        put("y", yFrac.toDouble())
        put("size", sizeDp.toDouble())
        put("on", enabled)
        put("hold", tapToHold)
    }

    companion object {
        fun fromJson(json: JSONObject): TouchButtonCfg? {
            val idName = json.optString("id", "") ?: ""
            val id = runCatching { TouchButtonId.valueOf(idName) }.getOrNull() ?: return null
            return TouchButtonCfg(
                id = id,
                xFrac = json.optDouble("x", 0.5).toFloat().coerceIn(0f, 1f),
                yFrac = json.optDouble("y", 0.5).toFloat().coerceIn(0f, 1f),
                sizeDp = json.optDouble("size", 64.0).toFloat().coerceIn(28f, 220f),
                enabled = json.optBoolean("on", true),
                tapToHold = json.optBoolean("hold", false),
            )
        }
    }
}

data class TouchLayout(val buttons: List<TouchButtonCfg>) {
    fun toJson(): JSONObject = JSONObject().apply {
        val arr = JSONArray()
        for (b in buttons) arr.put(b.toJson())
        put("buttons", arr)
    }

    companion object {
        fun fromJson(json: JSONObject): TouchLayout {
            val arr = json.optJSONArray("buttons") ?: return default()
            val list = mutableListOf<TouchButtonCfg>()
            for (i in 0 until arr.length()) {
                val obj = arr.optJSONObject(i) ?: continue
                TouchButtonCfg.fromJson(obj)?.let { list.add(it) }
            }
            // If the persisted layout is missing any new buttons added
            // since it was saved, splice in their defaults so the user
            // gets the new widget without having to reset. Matches
            // Settings.kt's optKey + fallback pattern.
            val have = list.map { it.id }.toSet()
            val merged = list + default().buttons.filter { it.id !in have }
            return TouchLayout(merged)
        }

        /** Landscape-tuned default. Coordinates assume a 16:9-ish layout
         *  and are clamped to safe areas on edges. The user can drag in
         *  edit mode to fit their device — this is just the starting
         *  point. */
        fun default(): TouchLayout = TouchLayout(
            buttons = listOf(
                // DPad cluster — lower left of screen, above L-stick room
                TouchButtonCfg(TouchButtonId.DPAD,     0.10f, 0.55f, 150f),
                // Face button diamond — lower right
                TouchButtonCfg(TouchButtonId.TRIANGLE, 0.86f, 0.45f, 58f),
                TouchButtonCfg(TouchButtonId.SQUARE,   0.80f, 0.55f, 58f),
                TouchButtonCfg(TouchButtonId.CIRCLE,   0.92f, 0.55f, 58f),
                TouchButtonCfg(TouchButtonId.CROSS,    0.86f, 0.65f, 58f),
                // Shoulders stacked vertically on each side: L2 / R2 on
                // top (further trigger), L1 / R1 directly below them.
                // Gap is ~16% of screen height — on a 390dp landscape
                // height that's 62dp center-to-center, ~6dp visible gap
                // between the 56dp buttons. Tight enough to read as a
                // pair without overlapping.
                TouchButtonCfg(TouchButtonId.L2,       0.08f, 0.10f, 56f),
                TouchButtonCfg(TouchButtonId.L1,       0.08f, 0.23f, 56f),
                TouchButtonCfg(TouchButtonId.R2,       0.92f, 0.10f, 56f),
                TouchButtonCfg(TouchButtonId.R1,       0.92f, 0.23f, 56f),
                // Start / Select centered at the bottom
                TouchButtonCfg(TouchButtonId.SELECT,   0.45f, 0.92f, 48f),
                TouchButtonCfg(TouchButtonId.START,    0.55f, 0.92f, 48f),
                // Fast-forward + macro buttons — OPT-IN (disabled by default, so they
                // splice into existing layouts without changing them). Parked in a row
                // across the upper-middle (y 0.40) — clear of the edit-mode toolbar at
                // the top (so they're reachable to grab/enable) and above the main
                // controls. Users enable + reposition them in the editor; configure each
                // macro's button set in Pad settings → Touch Macros.
                TouchButtonCfg(TouchButtonId.FAST_FORWARD, 0.30f, 0.40f, 44f, enabled = false),
                TouchButtonCfg(TouchButtonId.MACRO1, 0.40f, 0.40f, 42f, enabled = false),
                TouchButtonCfg(TouchButtonId.MACRO2, 0.48f, 0.40f, 42f, enabled = false),
                TouchButtonCfg(TouchButtonId.MACRO3, 0.56f, 0.40f, 42f, enabled = false),
                TouchButtonCfg(TouchButtonId.MACRO4, 0.64f, 0.40f, 42f, enabled = false),
                // Quick save/load-state buttons — also OPT-IN (disabled). Second row.
                TouchButtonCfg(TouchButtonId.SAVE_STATE, 0.30f, 0.54f, 44f, enabled = false),
                TouchButtonCfg(TouchButtonId.LOAD_STATE, 0.38f, 0.54f, 44f, enabled = false),
                // Analog sticks — bottom inside, between DPad/face cluster
                // and the center, so thumb travel is short.
                TouchButtonCfg(TouchButtonId.L_STICK,  0.28f, 0.80f, 130f),
                TouchButtonCfg(TouchButtonId.R_STICK,  0.72f, 0.80f, 130f),
                // L3 / R3 stick-click buttons, anchored at the lower
                // outside corner of each thumbstick (away from the
                // screen center so the user's hand resting on the stick
                // doesn't accidentally press them).
                TouchButtonCfg(TouchButtonId.L3,       0.18f, 0.93f, 42f),
                TouchButtonCfg(TouchButtonId.R3,       0.82f, 0.93f, 42f),
                // Invisible pause hotspot — dead center between the DPad
                // (0.10) and the face diamond (0.86), on their shared row,
                // clear of the sticks (y 0.80) and Start/Select (y 0.92).
                TouchButtonCfg(TouchButtonId.PAUSE,    0.48f, 0.50f, 120f),
                // Pressure-modifier button — tucked under the D-pad (left side),
                // clear of the action. Hold it, then press a face/shoulder/d-pad
                // button for a ~50% (soft) press. Movable in overlay edit mode.
                TouchButtonCfg(TouchButtonId.PRESSURE, 0.10f, 0.78f, 44f, enabled = false),
            ),
        )
    }
}

data class TouchProfile(val name: String, val layout: TouchLayout) {
    fun toJson(): JSONObject = JSONObject().apply {
        put("name", name)
        put("layout", layout.toJson())
    }

    companion object {
        fun fromJson(json: JSONObject): TouchProfile {
            return TouchProfile(
                name = json.optString("name", "Profile"),
                layout = json.optJSONObject("layout")?.let { TouchLayout.fromJson(it) }
                    ?: TouchLayout.default(),
            )
        }
    }
}
