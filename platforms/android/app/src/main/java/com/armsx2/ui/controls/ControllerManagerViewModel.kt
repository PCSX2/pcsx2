package com.armsx2.ui.controls

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.input.ControllerMappings

enum class ControllerSection { Buttons, Hotkeys }

data class ControllerManagerUiState(
    val player: Int = 0,
    val section: ControllerSection = ControllerSection.Buttons,
    val capturingAction: ControllerMappings.Action? = null,
    val tick: Int = 0,
    val rumble: Boolean = true,
    val multitap: Boolean = false,
    val dpadAsStick: Boolean = false,
)

class ControllerManagerViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(ControllerManagerUiState())
        private set

    fun refresh() {
        state.value = state.value.copy(
            tick = state.value.tick + 1,
            rumble = ControllerMappings.rumbleEnabled(),
            multitap = ControllerMappings.multitapEnabled(),
            dpadAsStick = ControllerMappings.dpadAsLeftStick(),
        )
    }

    fun setPlayer(player: Int) {
        cancelCapture()
        state.value = state.value.copy(player = player.coerceIn(0, 1))
    }

    fun setSection(section: ControllerSection) {
        cancelCapture()
        state.value = state.value.copy(section = section)
    }

    fun beginCapture(action: ControllerMappings.Action) {
        ControllerMappings.padCapturing.value = true
        // Bind via the Activity's dispatchKeyEvent (see ControllerMappings.capturePadAction) so any
        // button binds even though the bind prompt is no longer a focus-stealing dialog.
        ControllerMappings.capturePadAction.value = { kc -> captureKey(kc) }
        state.value = state.value.copy(capturingAction = action)
    }

    fun captureKey(keyCode: Int): Boolean {
        val action = state.value.capturingAction ?: return false
        ControllerMappings.bind(action, keyCode, state.value.player)
        ControllerMappings.padCapturing.value = false
        ControllerMappings.capturePadAction.value = null
        state.value = state.value.copy(capturingAction = null, tick = state.value.tick + 1)
        return true
    }

    fun cancelCapture() {
        ControllerMappings.padCapturing.value = false
        ControllerMappings.capturePadAction.value = null
        state.value = state.value.copy(capturingAction = null)
    }

    fun clear(action: ControllerMappings.Action) {
        ControllerMappings.clearAction(action, state.value.player)
        refresh()
    }

    fun resetPlayer() {
        ControllerMappings.reset(state.value.player)
        refresh()
    }

    fun beginHotkeyCapture(hotkey: ControllerMappings.SysHotkey) {
        ControllerMappings.beginHotkeyCapture(hotkey)
        state.value = state.value.copy(tick = state.value.tick + 1)
    }

    fun clearHotkey(hotkey: ControllerMappings.SysHotkey) {
        ControllerMappings.clearHotkey(hotkey)
        refresh()
    }

    fun setRumble(value: Boolean) {
        ControllerMappings.setRumbleEnabled(value)
        refresh()
    }

    fun setMultitap(value: Boolean) {
        ControllerMappings.setMultitapEnabled(value)
        refresh()
    }

    fun setDpadAsStick(value: Boolean) {
        ControllerMappings.setDpadAsLeftStick(value)
        refresh()
    }

    override fun onCleared() {
        cancelCapture()
        super.onCleared()
    }
}

