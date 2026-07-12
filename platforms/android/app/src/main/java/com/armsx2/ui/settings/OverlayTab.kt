package com.armsx2.ui.settings

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.config.Settings
import com.armsx2.i18n.str
import com.armsx2.ui.InGameOverlay
import com.armsx2.ui.UiScale
import androidx.core.content.edit

/**
 * Performance Overlay element toggles. Lets the user show/hide individual
 * parts of the on-screen stats overlay (the master OSD pill on the Play tab
 * is still the quick all-on/all-off switch).
 *
 * The GPU toggle is special: turning it off also stops the GPU timing
 * queries (timestamp queries + per-frame readback have real overhead), so
 * it's a genuine performance lever, not just a display option — see GS.cpp
 * SetGPUTimingEnabled. Each toggle persists to base AND applies live via the
 * native osdShow* setters (see [InGameOverlay.applySafeLiveDelta]).
 */
@Composable
fun OverlayTab(state: MutableState<Settings>) {
    val s = state.value
    val scroll = settingsScrollState()
    ControllerAutoScroll(scroll)

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)

    Column(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        Text(
            str("overlay.intro.description"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 14.sp,
            modifier = Modifier.padding(bottom = 8.dp),
        )

        IntSliderRow(
            label = str("overlay.uiSize.label"),
            value = s.osdScale,
            min = 50,
            max = 250,
            description = str("overlay.uiSize.description"),
            valueFormatter = { "$it%" },
            onChange = { apply(s.copy(osdScale = it)) },
        )
        SettingsDivider()
        ToggleRow(str("overlay.toggle.gpuUsage"), s.osdShowGpu) {
            apply(s.copy(osdShowGpu = it))
        }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.cpuUsage"), s.osdShowCpu) { apply(s.copy(osdShowCpu = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.fps"), s.osdShowFps) { apply(s.copy(osdShowFps = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.vps"), s.osdShowVps) { apply(s.copy(osdShowVps = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.emulationSpeed"), s.osdShowSpeed) { apply(s.copy(osdShowSpeed = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.internalResolution"), s.osdShowResolution) { apply(s.copy(osdShowResolution = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.gsStatistics"), s.osdShowGsStats) { apply(s.copy(osdShowGsStats = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.frameTimesGraph"), s.osdShowFrameTimes) { apply(s.copy(osdShowFrameTimes = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.hardwareInfo"), s.osdShowHardwareInfo) { apply(s.copy(osdShowHardwareInfo = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.gpuPipelineStats"), s.osdShowGpuStats) { apply(s.copy(osdShowGpuStats = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.emulatorVersion"), s.osdShowVersion) { apply(s.copy(osdShowVersion = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.settingsSummary"), s.osdShowSettings) { apply(s.copy(osdShowSettings = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.controlInputs"), s.osdShowInputs) { apply(s.copy(osdShowInputs = it)) }
        SettingsDivider()
        ToggleRow(str("overlay.toggle.onScreenNotifications"), s.osdShowMessages) { apply(s.copy(osdShowMessages = it)) }
        SettingsDivider()
        // Android hotkey pop-ups (Fast-Forward on/off, etc.) — separate from the emulator
        // OSD, pref-backed. Cancel-previous already stops them stacking; this switches
        // them off entirely for heavy fast-forward users.
        val ffToasts = remember { mutableStateOf(com.armsx2.runtime.MainActivityRuntime.prefs.getBoolean("ui.hotkeyToasts", true)) }
        ToggleRow(str("overlay.toggle.fastForwardPopups"), ffToasts.value) {
            ffToasts.value = it
            com.armsx2.runtime.MainActivityRuntime.prefs.edit { putBoolean("ui.hotkeyToasts", it) }
        }
        SettingsDivider()

        // Interface scaling (global, not per-game): resize the library / menu chrome
        // and text for different screen aspect ratios / handheld sizes. Does NOT
        // touch the game image or the on-screen touch controls.
        Text(
            str("overlay.interfaceScaling.description"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 14.sp,
            modifier = Modifier.padding(top = 10.dp, bottom = 6.dp),
        )
        IntSliderRow(
            label = str("overlay.uiSize.label"),
            value = (UiScale.borderScale.value * 100f).toInt(),
            min = (UiScale.MIN * 100f).toInt(),
            max = (UiScale.BORDER_MAX * 100f).toInt(),
            description = str("overlay.uiSize.description"),
            valueFormatter = { "$it%" },
            onChange = { UiScale.setBorderScale(it / 100f) },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("overlay.uiFontSize.label"),
            value = (UiScale.fontScale.value * 100f).toInt(),
            min = (UiScale.MIN * 100f).toInt(),
            max = (UiScale.MAX * 100f).toInt(),
            description = str("overlay.uiFontSize.description"),
            valueFormatter = { "$it%" },
            onChange = { UiScale.setFontScale(it / 100f) },
        )
    }
}
