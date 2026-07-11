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
import androidx.core.content.edit
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
        DriverSelector()
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

@Composable
private fun DriverSelector() {
    val context = androidx.compose.ui.platform.LocalContext.current
    val installed = androidx.compose.runtime.remember { mutableStateListOf<CustomDriver.InstalledDriver>() }
    LaunchedEffect(Unit) {
        installed.clear()
        installed.addAll(CustomDriver.listInstalled(context))
    }
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        SectionTitle(str("backend.gpuDriver.label"), str("backend.gpuDriver.description"))
        DriverChoice(
            title = str("backend.driver.systemVulkan"),
            subtitle = str("renderer.orientation.device"),
            selected = MainActivityRuntime.customDriverId.value == null,
            onClick = { selectDriver(null) },
        )
        installed.forEach { driver ->
            DriverChoice(
                title = driver.name,
                subtitle = listOf(driver.vendor, driver.version).filter(String::isNotBlank).joinToString(" · ").ifBlank { str("backend.driver.installed") },
                selected = MainActivityRuntime.customDriverId.value == driver.id,
                onClick = { selectDriver(driver.id) },
            )
        }
    }
}

@Composable
private fun DriverChoice(
    title: String,
    subtitle: String,
    selected: Boolean,
    onClick: () -> Unit,
) {
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(1.dp, if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
    ) {
        Row(Modifier.padding(13.dp), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Column(Modifier.weight(1f)) {
                Text(title, style = MaterialTheme.typography.titleMedium)
                Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (selected) StatusChip(str("backend.driver.active"), Success)
        }
    }
}

private fun selectDriver(id: String?) {
    MainActivityRuntime.customDriverId.value = id
    MainActivityRuntime.prefs.edit { putString("customDriverId", id.orEmpty()) }
}
