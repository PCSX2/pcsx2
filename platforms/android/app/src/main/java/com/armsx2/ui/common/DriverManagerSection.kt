package com.armsx2.ui.common

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.core.content.edit
import com.armsx2.CustomDriver
import com.armsx2.i18n.str
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.settings.controllerFocusable
import com.armsx2.ui.theme.Success
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Vulkan custom-driver manager: pick the system driver or an installed one, download
 * fresh builds from the bundled sources (K11MCH1 / MrPurple / StevenMXZ / crueter),
 * import a local .zip, or delete an installed driver. Self-contained (plain Material3)
 * so it drops into both the full Settings renderer tab and the in-game renderer pane.
 * Selecting a driver only takes effect on the next renderer init — the caller shows an
 * "Apply & Restart" affordance.
 */
@Composable
fun DriverManagerSection() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val installed = remember { mutableStateListOf<CustomDriver.InstalledDriver>() }
    var remote by remember { mutableStateOf<List<CustomDriver.RemoteDriver>?>(null) }
    var loadingRemote by remember { mutableStateOf(false) }
    var showRemote by remember { mutableStateOf(false) }
    var busyId by remember { mutableStateOf<String?>(null) }
    var message by remember { mutableStateOf<String?>(null) }
    // str() is @Composable, so resolve the messages the coroutines set up-front.
    val installedMsg = str("backend.driver.installed")
    val importErr = str("backend.driver.importError")
    val fetchErr = str("backend.driver.fetchError")
    val installedOkMsg = str("backend.driver.installedOk")
    val importFailedMsg = str("backend.driver.importFailed")
    val downloadFailedMsg = str("backend.driver.downloadFailed")
    // Which source groups are expanded in the online list (collapsed by default).
    val expandedSources = remember { mutableStateMapOf<String, Boolean>() }

    fun refreshInstalled() {
        installed.clear()
        installed.addAll(CustomDriver.listInstalled(context))
    }
    LaunchedEffect(Unit) { refreshInstalled() }

    val importLauncher = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) scope.launch {
            busyId = "import"; message = null
            val d = withContext(Dispatchers.IO) { CustomDriver.installFromUri(context, uri) }
            busyId = null
            if (d != null) { refreshInstalled(); selectDriver(d.id); message = "$installedOkMsg ${d.name}" }
            else message = importFailedMsg
        }
    }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        SectionTitle(str("backend.gpuDriver.label"), str("backend.gpuDriver.description"))

        DriverRow(
            controllerId = "driver.system",
            title = str("backend.driver.systemVulkan"),
            subtitle = str("renderer.orientation.device"),
            selected = MainActivityRuntime.customDriverId.value == null,
            onClick = { selectDriver(null) },
        )
        installed.forEach { driver ->
            DriverRow(
                controllerId = "driver.${driver.id}",
                title = driver.name,
                subtitle = listOf(driver.vendor, driver.version).filter(String::isNotBlank).joinToString(" · ")
                    .ifBlank { str("backend.driver.installed") },
                selected = MainActivityRuntime.customDriverId.value == driver.id,
                onClick = { selectDriver(driver.id) },
                onDelete = {
                    val wasSelected = MainActivityRuntime.customDriverId.value == driver.id
                    CustomDriver.delete(driver)
                    if (wasSelected) selectDriver(null)
                    refreshInstalled()
                },
            )
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(
                shape = RoundedCornerShape(13.dp),
                modifier = Modifier.controllerFocusable(
                    "driver.toggleDownloads",
                    RoundedCornerShape(13.dp),
                    onConfirm = {
                        showRemote = !showRemote
                        if (showRemote && remote == null && !loadingRemote) {
                            loadingRemote = true
                            scope.launch {
                                val r = withContext(Dispatchers.IO) { CustomDriver.fetchRemote() }
                                remote = r; loadingRemote = false
                            }
                        }
                    },
                ),
                onClick = {
                    showRemote = !showRemote
                    if (showRemote && remote == null && !loadingRemote) {
                        loadingRemote = true
                        scope.launch {
                            val r = withContext(Dispatchers.IO) { CustomDriver.fetchRemote() }
                            remote = r; loadingRemote = false
                        }
                    }
                },
            ) { Text(if (showRemote) str("backend.driver.hideDownloads") else str("backend.driver.download")) }
            OutlinedButton(
                shape = RoundedCornerShape(13.dp),
                modifier = Modifier.controllerFocusable(
                    "driver.import",
                    RoundedCornerShape(13.dp),
                    onConfirm = { importLauncher.launch(arrayOf("application/zip", "application/octet-stream", "*/*")) },
                ),
                onClick = { importLauncher.launch(arrayOf("application/zip", "application/octet-stream", "*/*")) },
            ) { Text(str("backend.driver.import")) }
        }

        if (showRemote) {
            when {
                loadingRemote -> Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(18.dp), strokeWidth = 2.dp)
                    Spacer8()
                    Text(str("backend.driver.fetching"), color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                remote.isNullOrEmpty() -> Text(str("backend.driver.none"), color = MaterialTheme.colorScheme.onSurfaceVariant)
                // Group the online drivers by source (K11MCH1 / MrPurple / StevenMXZ /
                // crueter…), each an expandable, controller-navigable section.
                else -> remote!!.groupBy { it.source }.forEach { (source, drivers) ->
                    val expanded = expandedSources[source] ?: false
                    DriverSourceGroup(
                        source = source,
                        count = drivers.size,
                        expanded = expanded,
                        onToggle = { expandedSources[source] = !expanded },
                    ) {
                        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                            drivers.forEach { rd ->
                                RemoteDriverRow(
                                    controllerId = "driver.remote.${rd.id}",
                                    title = rd.releaseName.ifBlank { rd.assetName },
                                    subtitle = listOf(rd.source, rd.tagName).filter(String::isNotBlank).joinToString(" · "),
                                    busy = busyId == rd.id,
                                    onDownload = {
                                        scope.launch {
                                            busyId = rd.id; message = null
                                            val d = withContext(Dispatchers.IO) { CustomDriver.download(context, rd) }
                                            busyId = null
                                            if (d != null) { refreshInstalled(); selectDriver(d.id); message = "$installedOkMsg ${d.name}" }
                                            else message = downloadFailedMsg
                                        }
                                    },
                                )
                            }
                        }
                    }
                }
            }
        }

        if (busyId == "import") Text(str("backend.driver.installing"), color = MaterialTheme.colorScheme.onSurfaceVariant)
        message?.let { Text(it, color = MaterialTheme.colorScheme.primary) }
    }
}

@Composable
private fun DriverSourceGroup(
    source: String,
    count: Int,
    expanded: Boolean,
    onToggle: () -> Unit,
    content: @Composable () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Surface(
            onClick = onToggle,
            modifier = Modifier.fillMaxWidth()
                .controllerFocusable("driver.group.$source", RoundedCornerShape(14.dp), onConfirm = onToggle),
            shape = RoundedCornerShape(14.dp),
            color = MaterialTheme.colorScheme.surfaceVariant,
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.4f)),
        ) {
            Row(Modifier.padding(horizontal = 14.dp, vertical = 11.dp), verticalAlignment = Alignment.CenterVertically) {
                Text(if (expanded) "▾" else "▸", color = MaterialTheme.colorScheme.primary)
                Spacer(Modifier.width(10.dp))
                Text(source, style = MaterialTheme.typography.titleSmall, modifier = Modifier.weight(1f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text("$count", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        if (expanded) content()
    }
}

@Composable
private fun DriverRow(
    controllerId: String,
    title: String,
    subtitle: String,
    selected: Boolean,
    onClick: () -> Unit,
    onDelete: (() -> Unit)? = null,
) {
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().controllerFocusable(controllerId, RoundedCornerShape(16.dp), onConfirm = onClick),
        shape = RoundedCornerShape(16.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(1.dp, if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
    ) {
        Row(Modifier.padding(13.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                Text(title, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            if (selected) StatusChip(str("backend.driver.active"), Success)
            if (onDelete != null) TextButton(onClick = onDelete, modifier = Modifier.controllerFocusable("$controllerId.delete", onConfirm = onDelete)) { Text(str("action.delete"), color = MaterialTheme.colorScheme.error) }
        }
    }
}

@Composable
private fun RemoteDriverRow(controllerId: String, title: String, subtitle: String, busy: Boolean, onDownload: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
    ) {
        Row(Modifier.padding(13.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                Text(title, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (subtitle.isNotBlank()) Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            if (busy) CircularProgressIndicator(Modifier.size(18.dp), strokeWidth = 2.dp)
            else OutlinedButton(onClick = onDownload, shape = RoundedCornerShape(12.dp), modifier = Modifier.controllerFocusable(controllerId, RoundedCornerShape(12.dp), onConfirm = onDownload)) { Text(str("backend.driver.get")) }
        }
    }
}

@Composable
private fun Spacer8() = androidx.compose.foundation.layout.Spacer(Modifier.width(10.dp))

private fun selectDriver(id: String?) {
    MainActivityRuntime.customDriverId.value = id
    MainActivityRuntime.prefs.edit { putString("customDriverId", id.orEmpty()) }
}
