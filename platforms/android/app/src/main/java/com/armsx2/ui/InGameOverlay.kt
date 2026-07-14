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

    /** Per-tab scroll offset (px) of the in-game pause menu, retained across menu open/close so
     *  reopening a tab — especially the long Fixes list — returns to where you were instead of
     *  snapping back to the top. Keyed by EmulationMenuTab.name to avoid coupling to that enum. */
    val menuTabScroll = HashMap<String, Int>()

    fun saveSettings(updated: Settings) {
        val previous = settingsState.value
        settingsState.value = updated
        ConfigStore.save(settingsScope.value, currentSerial.value, updated)
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
                if (MainActivityRuntime.eState.value != EmuState.STOPPED) updated.applyTo()
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
        val current = settingsState.value
        val enabled = !(current.osdShowFps || current.osdShowVps || current.osdShowSpeed ||
            current.osdShowCpu || current.osdShowGpu || current.osdShowResolution ||
            current.osdShowGsStats || current.osdShowFrameTimes ||
            current.osdShowHardwareInfo || current.osdShowVersion)
        saveSettings(
            current.copy(
                osdShowFps = enabled,
                osdShowVps = enabled,
                osdShowSpeed = enabled,
                osdShowCpu = enabled,
                osdShowGpu = enabled,
                osdShowResolution = enabled,
                osdShowGsStats = enabled,
                osdShowFrameTimes = enabled,
                osdShowHardwareInfo = enabled,
                osdShowVersion = enabled,
            ),
        )
        NativeApp.osdShowAll(enabled)
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
