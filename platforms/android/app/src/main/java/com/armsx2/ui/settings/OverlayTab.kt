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

/** OSD text colours as 0xRRGGBB, index-aligned with [OSD_COLOR_LABEL_KEYS]. 0 = default white.
 *  Deliberately light/desaturated: the OSD draws over gameplay with only a soft shadow behind
 *  it, so fully-saturated colours read badly on bright scenes.
 *  Internal, not private: the in-game quick menu cycles the same palette, and two copies would
 *  drift the moment one gains a colour. */
internal val OSD_COLORS = listOf(
    0x000000, // default (white — 0 means "unset" to the renderer)
    0x66FF66, // green
    0x66E0FF, // cyan
    0xFFE066, // yellow
    0xFFA64D, // orange
    0xFF6666, // red
    0xFF7AC8, // pink
    0xC08CFF, // purple
)

/** i18n keys for [OSD_COLORS], same order. */
internal val OSD_COLOR_LABEL_KEYS = listOf(
    "overlay.osdColor.default", "overlay.osdColor.green", "overlay.osdColor.cyan",
    "overlay.osdColor.yellow", "overlay.osdColor.orange", "overlay.osdColor.red",
    "overlay.osdColor.pink", "overlay.osdColor.purple",
)

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

        // All three size sliders sit together at the top. The first scales the emulator's OSD
        // (the perf/stat readout drawn over the game); the two below scale the app's own menus.
        // They used to be split across the tab AND shared one label key, so this slider read as
        // "UI Size (borders)" while actually driving osdScale — two settings, one name.
        IntSliderRow(
            label = str("overlay.osdSize.label"),
            value = s.osdScale,
            min = 50,
            max = 250,
            description = str("overlay.osdSize.description"),
            valueFormatter = { "$it%" },
            onChange = { apply(s.copy(osdScale = it)) },
        )
        SettingsDivider()

        // OSD text colour. A preset row rather than an RGB picker: SegmentedRow is already
        // controller-navigable (Left/Right/Confirm), whereas a colour wheel would demand
        // pointer input and strand pad-only devices. 0 = leave it white, so nobody's OSD
        // changes appearance until they choose to.
        SegmentedRow(
            label = str("overlay.osdColor.label"),
            options = OSD_COLOR_LABEL_KEYS.map { str(it) },
            selectedIndex = OSD_COLORS.indexOf(s.osdColor).coerceAtLeast(0),
            description = str("overlay.osdColor.description"),
            onChange = { apply(s.copy(osdColor = OSD_COLORS[it])) },
        )
        SettingsDivider()

        // Interface scaling (global, not per-game): resize the library / menu chrome
        // and text for different screen aspect ratios / handheld sizes. Does NOT
        // touch the game image or the on-screen touch controls.
        Text(
            str("overlay.interfaceScaling.description"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 14.sp,
            modifier = Modifier.padding(top = 4.dp, bottom = 6.dp),
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
    }
}
