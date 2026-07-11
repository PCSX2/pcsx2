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
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.config.Settings
import com.armsx2.i18n.str
import com.armsx2.ui.InGameOverlay

/**
 * SPU2 audio output settings. Volume + mute apply live to the open audio
 * stream (NativeApp.setAudioVolume / setAudioMuted) and persist via ConfigStore.
 */
@Composable
fun AudioTab(state: MutableState<Settings>) {
    val s = state.value
    val scroll = settingsScrollState()
    ControllerAutoScroll(scroll)

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)

    Column(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        Text(
            str("audio.header.description"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 14.sp,
            modifier = Modifier.padding(bottom = 8.dp),
        )
        IntSliderRow(
            label = str("audio.volume.label"),
            value = s.audioVolume.coerceIn(0, 150),
            min = 0,
            max = 150,
            description = str("audio.volume.description"),
            valueFormatter = { "$it%" },
            onChange = { apply(s.copy(audioVolume = it)) },
        )
        SettingsDivider()
        ToggleRow(str("audio.mute.label"), s.audioMuted) { apply(s.copy(audioMuted = it)) }
        SettingsDivider()
        ToggleRow(
            str("audio.synchronization.label"),
            s.audioTimeStretch,
            description = str("audio.synchronization.description"),
        ) { apply(s.copy(audioTimeStretch = it)) }
        SettingsDivider()
        IntSliderRow(
            label = str("audio.buffer.label"),
            value = s.audioBufferMs.coerceIn(20, 200),
            min = 20,
            max = 200,
            description = str("audio.buffer.description"),
            valueFormatter = { "$it ms" },
            onChange = { apply(s.copy(audioBufferMs = it)) },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("audio.outputLatency.label"),
            value = s.audioOutputLatencyMs.coerceIn(5, 100),
            min = 5,
            max = 100,
            description = str("audio.outputLatency.description"),
            valueFormatter = { "$it ms" },
            onChange = { apply(s.copy(audioOutputLatencyMs = it)) },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("audio.fastForwardVolume.label"),
            value = s.audioFastForwardVolume.coerceIn(0, 100),
            min = 0,
            max = 100,
            description = str("audio.fastForwardVolume.description"),
            valueFormatter = { "$it%" },
            onChange = { apply(s.copy(audioFastForwardVolume = it)) },
        )
        SettingsDivider()
        ToggleRow(
            str("audio.swapChannels.label"),
            s.audioSwapChannels,
            description = str("audio.swapChannels.description"),
        ) { apply(s.copy(audioSwapChannels = it)) }
        SettingsDivider()
        ToggleRow(
            str("audio.spu2Simd.label"),
            s.spu2NeonReverb,
            description = str("audio.spu2Simd.description"),
        ) { apply(s.copy(spu2NeonReverb = it)) }
    }
}
