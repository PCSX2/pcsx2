package com.armsx2.ui

import androidx.compose.runtime.mutableStateOf
import com.armsx2.EmuState
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.config.ConfigStore
import com.armsx2.config.Settings
import com.armsx2.config.SettingsScope
import kr.co.iefriends.pcsx2.NativeApp

object InGameOverlay {
    val settingsState = mutableStateOf(Settings())
    val settingsScope = mutableStateOf(SettingsScope.Global)
    val currentSerial = mutableStateOf<String?>(null)
    val hardcoreOn = mutableStateOf(false)
    val frameLimitOn = mutableStateOf(true)

    /** OSD hotkey mode. The hotkey now CYCLES rather than plain on/off (Cotcho): Full (every
     *  stat) → Min (fps + CPU/EE/VU line) → Custom (the user's saved per-stat selection) → Off →
     *  Full. Transient and in-game only; never mutates the saved Settings selection, so Custom
     *  always reflects exactly what the user chose. Starts at Custom so a fresh boot shows their
     *  stats and the first press advances to Off, matching the old "first press hides". */
    enum class OsdMode { Full, Min, Custom, Off }
    val osdMode = mutableStateOf(OsdMode.Custom)
    private val osdCycle = listOf(OsdMode.Full, OsdMode.Min, OsdMode.Custom, OsdMode.Off)
    private const val OsdModeKey = "ui.osdMode"
    private var osdLoaded = false

    private fun ensureOsdLoaded() {
        if (osdLoaded) return
        osdLoaded = true
        val name = runCatching { MainActivityRuntime.prefs.getString(OsdModeKey, null) }.getOrNull()
        osdMode.value = OsdMode.entries.firstOrNull { it.name == name } ?: OsdMode.Custom
    }

    /** Set the OSD mode (from the in-game menu selector or the hotkey), apply it live, and
     *  persist it so it survives a relaunch — matching the old per-stat toggles' persistence. */
    fun setOsdMode(mode: OsdMode) {
        ensureOsdLoaded()
        applyOsdMode(mode)
        runCatching { MainActivityRuntime.prefs.edit().putString(OsdModeKey, mode.name).apply() }
    }

    /** Re-assert the stored OSD mode after a game's settings apply on boot, so a non-Custom
     *  choice (Full / Min / Off) isn't reset to the per-stat selection every launch. */
    fun applyStoredOsdMode() {
        ensureOsdLoaded()
        // Custom = "the user's saved per-stat flags". At boot settingsState is NOT yet populated
        // with THIS game's resolved settings, so reading it here applied stale/empty flags — which
        // hid an enabled stat until a reset repopulated it (#385). Resolve the current game's
        // settings ourselves. (A fresh install has every stat defaulting off, so this also cleanly
        // shows nothing by default rather than whatever stale state was left in settingsState.)
        if (osdMode.value == OsdMode.Custom) {
            applyOsdFlags(
                com.armsx2.config.ConfigStore.resolveForGame(
                    MainActivityRuntime.currentGame.value?.settingsKey,
                ),
            )
        } else {
            applyOsdMode(osdMode.value)
        }
    }

    /** Short label for [mode], shown by the hotkey toast and the menu selector. */
    fun osdModeLabel(mode: OsdMode): String = when (mode) {
        OsdMode.Full -> "Full"
        OsdMode.Min -> "Minimal"
        OsdMode.Custom -> "Custom"
        OsdMode.Off -> "Off"
    }

    /** Per-tab scroll offset (px) of the in-game pause menu, retained across menu open/close so
     *  reopening a tab — especially the long Fixes list — returns to where you were instead of
     *  snapping back to the top. Keyed by EmulationMenuTab.name to avoid coupling to that enum. */
    val menuTabScroll = HashMap<String, Int>()

    fun saveSettings(updated: Settings) {
        val previous = settingsState.value
        settingsState.value = updated
        // `previous` matters: it's how ConfigStore tells a field the user just changed in
        // Game scope from one they never touched, so setting a per-game value that happens
        // to equal global still pins it instead of vanishing.
        ConfigStore.save(settingsScope.value, currentSerial.value, updated, previous)
        frameLimitOn.value = updated.frameLimitEnable

        if (MainActivityRuntime.nativeReady.value) {
            runCatching {
                if (previous.frameLimitEnable != updated.frameLimitEnable) {
                    NativeApp.setSetting("EmuCore/GS", "FrameLimitEnable", "bool", updated.frameLimitEnable.toString())
                    NativeApp.speedhackLimitermode(if (updated.frameLimitEnable) 0 else 3)
                    MainActivityRuntime.fastForwardToggleActive = false
                }
                if (previous.upscaleFloat != updated.upscaleFloat &&
                    MainActivityRuntime.eState.value != EmuState.STOPPED
                ) {
                    NativeApp.renderUpscalemultiplier(updated.upscaleFloat.coerceIn(0.25f, 8.0f))
                    MainActivityRuntime.upscale.value = updated.upscaleFloat.coerceIn(0.25f, 8.0f)
                }
                if (MainActivityRuntime.eState.value != EmuState.STOPPED) {
                    updated.applyTo()
                    // Regenerate the native per-game INI (gamesettings/<serial>_<CRC>.ini) from the
                    // resolved settings so a stale key there can't shadow the base layer. Without this a
                    // legacy per-game key — e.g. TVShader=3 from a reused data folder — survives every
                    // "Off": VMManager::ApplySettings reloads EmuConfig.GS from base∘game each commit/boot
                    // and the game layer wins. gameIniBeginWrite uses a fresh (no-Load) interface, so keys
                    // the user no longer overrides (TVShader once it equals global) are dropped and the
                    // file is deleted when empty. No-op when no VM (gameIniBeginWrite early-returns).
                    currentSerial.value?.takeIf { it.isNotBlank() }?.let { serial ->
                        ConfigStore.resolveForGame(serial).writeGameSettingsIni(ConfigStore.loadGlobal())
                    }
                }
            }
        }
    }

    fun open() {
        if (WindowImpl.overlayVisible.value) return
        val serial = MainActivityRuntime.currentGame.value?.settingsKey
            ?: runCatching { NativeApp.getPauseGameSerial() }.getOrNull()?.takeIf(String::isNotBlank)
        currentSerial.value = serial
        settingsScope.value = if (serial == null) SettingsScope.Global else SettingsScope.Game
        settingsState.value = ConfigStore.resolveForGame(serial)
        frameLimitOn.value = settingsState.value.frameLimitEnable
        hardcoreOn.value = runCatching { NativeApp.isHardcoreMode() }.getOrDefault(false)
        if (MainActivityRuntime.eState.value != EmuState.STOPPED) MainActivityRuntime.pauseForOverlay()
        WindowImpl.overlayVisible.value = true
    }

    fun toggle() {
        if (WindowImpl.overlayVisible.value) closeAndResume() else open()
    }

    /** Advance the OSD cycle one step, apply it live, and return a short label for the
     *  on-screen note. Applies live-only — the saved per-stat Settings are never mutated. */
    fun cycleOsd(): String {
        ensureOsdLoaded()
        val next = osdCycle[(osdCycle.indexOf(osdMode.value) + 1) % osdCycle.size]
        setOsdMode(next)
        return "OSD: " + osdModeLabel(next)
    }

    private fun applyOsdMode(mode: OsdMode) {
        osdMode.value = mode
        // Every mode also drives the GPU pipeline-stats line (VSI/PSI) explicitly: it has its
        // own setter outside osdApplyFlags' 12 flags, so leaving it out is what let it survive
        // the old "off" toggle and stay on screen (Cotcho).
        when (mode) {
            OsdMode.Full -> {
                NativeApp.osdApplyFlags(true, true, true, true, true, true, true, true, true, true, true, true)
                NativeApp.osdShowGpuStats(true)
            }
            OsdMode.Min -> {
                // fps + the CPU line (EE/VU/GS breakdown) — the at-a-glance set Cotcho described.
                NativeApp.osdApplyFlags(true, false, false, true, false, false, false, false, false, false, false, false)
                NativeApp.osdShowGpuStats(false)
            }
            OsdMode.Custom -> applyOsdFlags(settingsState.value)
            OsdMode.Off -> {
                NativeApp.osdApplyFlags(false, false, false, false, false, false, false, false, false, false, false, false)
                NativeApp.osdShowGpuStats(false)
            }
        }
    }

    /** Push a Settings object's saved per-stat OSD selection to native — the Custom mode. Split
     *  out so applyStoredOsdMode can feed it the boot-resolved settings (settingsState isn't ready
     *  yet at boot), while the live path feeds it settingsState. */
    private fun applyOsdFlags(s: com.armsx2.config.Settings) {
        osdMode.value = OsdMode.Custom
        NativeApp.osdApplyFlags(
            s.osdShowFps, s.osdShowVps, s.osdShowSpeed, s.osdShowCpu, s.osdShowGpu,
            s.osdShowResolution, s.osdShowGsStats, s.osdShowFrameTimes, s.osdShowHardwareInfo,
            s.osdShowVersion, s.osdShowSettings, s.osdShowInputs,
        )
        NativeApp.osdShowGpuStats(s.osdShowGpuStats)
    }

    fun editTouchLayout() {
        com.armsx2.ui.touch.TouchControls.ensureLoaded()
        com.armsx2.ui.touch.TouchControls.editMode.value = true
        WindowImpl.overlayVisible.value = false
    }

    fun openSaveStatePicker() {
        // Freeze the game behind the (opaque) picker; dismiss resumes it. The
        // pause-menu path is already paused, so this only bites the on-screen
        // touch-button path that fires mid-game.
        if (MainActivityRuntime.eState.value != EmuState.STOPPED) MainActivityRuntime.pauseForOverlay()
        WindowImpl.openInGameScreen(InGameScreen.SaveState)
    }

    fun openLoadStatePicker() {
        if (MainActivityRuntime.eState.value != EmuState.STOPPED) MainActivityRuntime.pauseForOverlay()
        WindowImpl.openInGameScreen(InGameScreen.LoadState)
    }

    private fun closeAndResume() {
        WindowImpl.overlayVisible.value = false
        // Resume on PAUSED *or* RUNNING. Opening the menu queues the pause asynchronously —
        // eState only flips to PAUSED once Host::OnVMPaused fires on the CPU thread — so a quick
        // open→close can reach here with eState still RUNNING, and gating on == PAUSED skipped
        // the resume, leaving the game stuck paused once the queued pause landed. resume() is safe
        // in both cases: it's ordered after the pending pause (FIFO on the CPU thread), and it's a
        // no-op when the VM is genuinely still running.
        val st = MainActivityRuntime.eState.value
        if ((st == EmuState.PAUSED || st == EmuState.RUNNING) && !WindowImpl.showLibrary.value &&
            !com.armsx2.ui.touch.TouchControls.editMode.value
        ) {
            MainActivityRuntime.resume()
        }
    }
}
