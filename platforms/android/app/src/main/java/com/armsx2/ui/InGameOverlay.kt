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

    /** Master OSD visibility for the on/off hotkey. A transient, in-game-only hide that does NOT
     *  touch the user's per-stat Settings selection — so toggling it back on restores exactly the
     *  stats they had chosen (not "everything"). Resets to visible each game boot. */
    val osdHidden = mutableStateOf(false)

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

    fun toggleOsd() {
        // Toggle OSD *visibility* only. Previously this overwrote every per-stat flag in Settings
        // with the master on/off value, so turning the OSD off then on lost the user's chosen
        // subset (it came back as "all stats on"). Now we flip a transient master flag and apply
        // live-only: on hide, push all-off; on show, push the user's saved selection back — the
        // saved Settings/store are never mutated, so the selection is preserved.
        val hide = !osdHidden.value
        osdHidden.value = hide
        if (hide) {
            NativeApp.osdApplyFlags(false, false, false, false, false, false, false, false, false, false, false, false)
        } else {
            val s = settingsState.value
            NativeApp.osdApplyFlags(
                s.osdShowFps, s.osdShowVps, s.osdShowSpeed, s.osdShowCpu, s.osdShowGpu,
                s.osdShowResolution, s.osdShowGsStats, s.osdShowFrameTimes, s.osdShowHardwareInfo,
                s.osdShowVersion, s.osdShowSettings, s.osdShowInputs,
            )
        }
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
        if (MainActivityRuntime.eState.value == EmuState.PAUSED && !WindowImpl.showLibrary.value &&
            !com.armsx2.ui.touch.TouchControls.editMode.value
        ) {
            MainActivityRuntime.resume()
        }
    }
}
