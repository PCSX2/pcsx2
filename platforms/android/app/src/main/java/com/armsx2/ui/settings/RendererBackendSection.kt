package com.armsx2.ui.settings

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.armsx2.CustomDriver
import com.armsx2.config.Settings
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.InGameOverlay
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.theme.Success
import com.armsx2.i18n.str

@Composable
fun RendererBackendSection(state: MutableState<Settings>) {
    val settings = state.value
    val rendererIds = listOf("auto", "opengl", "vulkan", "software")
    SegmentedRow(
        label = str("backend.graphicsApi.label"),
        options = listOf(str("backend.renderer.auto"), "OpenGL", "Vulkan", str("backend.renderer.software")),
        selectedIndex = rendererIds.indexOf(settings.renderer).coerceAtLeast(0),
        onChange = { index ->
            val renderer = rendererIds[index]
            InGameOverlay.saveSettings(settings.copy(renderer = renderer))
            MainActivityRuntime.renderer.value = renderer
            if (renderer != "vulkan") selectDriver(null)
        },
    )

    if (settings.renderer == "vulkan") {
        SettingsDivider()
        com.armsx2.ui.common.DriverManagerSection()
    } else if (settings.renderer == "opengl") {
        // OpenGL's "custom driver" is ANGLE — the GL analogue of the Vulkan driver list.
        SettingsDivider()
        com.armsx2.ui.common.AngleDriverSection(settings.useAngleOpenGL) { on ->
            InGameOverlay.saveSettings(settings.copy(useAngleOpenGL = on))
        }
    }

    SettingsDivider()
    Row(
        modifier = Modifier.fillMaxWidth().padding(horizontal = 6.dp, vertical = 8.dp),
        horizontalArrangement = Arrangement.End,
    ) {
        OutlinedButton(onClick = MainActivityRuntime::restart, shape = RoundedCornerShape(13.dp)) {
            Text(str("backend.applyRestart"))
        }
    }
}

private fun selectDriver(id: String?) {
    // UI mirror; persist scope-aware (per-game when the settings scope is Game). The driver
    // load itself happens at the next renderer (re)start via applyRendererPrefs.
    MainActivityRuntime.customDriverId.value = id
    InGameOverlay.saveSettings(InGameOverlay.settingsState.value.copy(customDriverId = id ?: ""))
}
