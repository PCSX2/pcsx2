package com.armsx2.ui.common

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import com.armsx2.ShaderRepo
import com.armsx2.i18n.str
import com.armsx2.ui.settings.controllerFocusable
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Locale
import java.util.concurrent.atomic.AtomicBoolean

/** Busy latch for an import. Not a source id — the import rows aren't sources, but they
 *  share the one "something is running" gate so a download and an import can't overlap. */
private const val IMPORT_BUSY_ID = "__import__"

/**
 * RetroArch shader-pack manager: download a slang-shaders pack, see what's
 * installed, delete one. The shader analogue of [DriverManagerSection], and
 * parameterless for the same reason — it drops into both the full Settings
 * renderer tab and the in-game graphics pane with one call.
 *
 * Installing is the whole integration: packs extract into `<dataroot>/shaders/`,
 * which is exactly where the Renderer tab's existing "Shader Preset" picker
 * scans for `*.slangp`. So a downloaded pack's presets simply appear in that
 * list — nothing here writes a setting.
 *
 * Controller-first, same constraints as [DriverManagerSection]: no AlertDialog
 * (its own focused window, swallows gamepad keys) and no Lazy list
 * (controllerFocusable only registers COMPOSED rows). A plain Column, with the
 * host screen's own verticalScroll doing the scrolling — a second vertical
 * scroll here would measure against infinite constraints and throw.
 */
@Composable
fun ShaderManagerSection() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val installed = remember { mutableStateListOf<ShaderRepo.InstalledPack>() }
    var showSources by remember { mutableStateOf(false) }
    var busyId by remember { mutableStateOf<String?>(null) }
    var downloaded by remember { mutableStateOf<Pair<Long, Long>?>(null) }
    var extracted by remember { mutableStateOf<Pair<Int, Int>?>(null) }
    var message by remember { mutableStateOf<String?>(null) }
    // Distinguishes "nothing installed" from "haven't looked yet" — the first scan walks
    // thousands of files, so without this the empty state flashes on every open.
    var scanned by remember { mutableStateOf(false) }
    // Polled by the worker between chunks/entries. A plain flag rather than
    // Job.cancel(): the download is blocking IO with no suspension points to
    // cancel AT, so the loops have to opt in either way — and this way the
    // coroutine completes normally and the cleanup below always runs.
    val cancelFlag = remember { AtomicBoolean(false) }
    // str() is @Composable, so resolve the messages the coroutines set up-front.
    val installedOkMsg = str("renderer.shaderPack.installedOk")
    val downloadFailedMsg = str("renderer.shaderPack.downloadFailed")
    val cancelledMsg = str("renderer.shaderPack.cancelled")
    val importFailedMsg = str("renderer.shaderPack.import.failed")

    /** Shared by both import pickers: an import has no progress to report (SAF gives no
     *  total up front), so it only needs the busy latch the download rows already use. */
    fun runImport(block: suspend () -> String?) {
        busyId = IMPORT_BUSY_ID
        message = null
        scope.launch {
            val id = block()
            busyId = null
            if (id != null) {
                installed.clear()
                installed.addAll(withContext(Dispatchers.IO) { ShaderRepo.listInstalled(context) })
                message = installedOkMsg
            } else {
                message = importFailedMsg
            }
        }
    }

    val folderLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri != null) runImport { withContext(Dispatchers.IO) { ShaderRepo.importFromTree(context, uri) } }
    }
    val zipLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) runImport { withContext(Dispatchers.IO) { ShaderRepo.importFromZip(context, uri) } }
    }

    fun refreshInstalled() {
        // Off the UI thread: listing walks each pack's tree to count presets,
        // and a full slang-shaders pack is ~5.6k files.
        scope.launch {
            val packs = withContext(Dispatchers.IO) { ShaderRepo.listInstalled(context) }
            installed.clear()
            installed.addAll(packs)
            scanned = true
        }
    }
    LaunchedEffect(Unit) { refreshInstalled() }

    fun startDownload(source: ShaderRepo.ShaderSource) {
        cancelFlag.set(false)
        scope.launch {
            busyId = source.id
            message = null
            downloaded = null
            extracted = null
            val pack = withContext(Dispatchers.IO) {
                ShaderRepo.download(
                    context = context,
                    source = source,
                    onDownload = { done, total -> downloaded = done to total },
                    onExtract = { done, total -> extracted = done to total },
                    isCancelled = { cancelFlag.get() },
                )
            }
            busyId = null
            downloaded = null
            extracted = null
            message = when {
                pack != null -> "$installedOkMsg ${pack.name}"
                cancelFlag.get() -> cancelledMsg
                else -> downloadFailedMsg
            }
            if (pack != null) refreshInstalled()
        }
    }

    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        SectionTitle(str("renderer.shaderPack.label"), str("renderer.shaderPack.description"))

        installed.forEach { pack ->
            InstalledPackRow(
                controllerId = "shaderPack.${pack.id}",
                title = pack.name,
                subtitle = "${pack.presetCount} ${str("renderer.shaderPack.presets")}",
                onDelete = {
                    // Off the UI thread: a full pack is ~5.6k files, and unlinking that
                    // many on the main thread is an ANR, not just jank.
                    scope.launch {
                        withContext(Dispatchers.IO) { ShaderRepo.delete(pack) }
                        refreshInstalled()
                    }
                },
            )
        }
        if (installed.isEmpty() && scanned) {
            Text(str("renderer.shaderPack.noneInstalled"), color = MaterialTheme.colorScheme.onSurfaceVariant)
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedButton(
                shape = RoundedCornerShape(13.dp),
                modifier = Modifier.controllerFocusable(
                    "shaderPack.toggleDownloads",
                    RoundedCornerShape(13.dp),
                    onConfirm = { showSources = !showSources },
                ),
                onClick = { showSources = !showSources },
            ) {
                Text(if (showSources) str("backend.driver.hideDownloads") else str("backend.driver.download"))
            }
        }

        if (showSources) {
            // A fixed, pinned list — unlike the driver manager there's no
            // releases API to poll: libretro publishes the pack at one stable
            // buildbot URL (the same one RetroArch's own updater pulls), so
            // there's nothing to fetch before showing it.
            ShaderRepo.baseSources().forEach { source ->
                RemoteShaderRow(
                    controllerId = "shaderPack.remote.${source.id}",
                    title = source.name,
                    subtitle = source.description,
                    busy = busyId == source.id,
                    enabled = busyId == null,
                    onDownload = { startDownload(source) },
                )
            }
            // Import, right under the one pack we host — for everything we don't: a fork,
            // a forum pack, a tree someone built themselves. Same destination folder, so
            // an imported pack is indistinguishable from a downloaded one afterwards.
            SectionTitle(
                str("renderer.shaderPack.import.label"),
                str("renderer.shaderPack.import.description"),
            )
            RemoteShaderRow(
                controllerId = "shaderPack.import.folder",
                title = str("renderer.shaderPack.import.folder"),
                subtitle = str("renderer.shaderPack.import.folder.description"),
                busy = busyId == IMPORT_BUSY_ID,
                enabled = busyId == null,
                onDownload = { folderLauncher.launch(null) },
                actionLabel = str("renderer.shaderPack.import.browse"),
            )
            RemoteShaderRow(
                controllerId = "shaderPack.import.zip",
                title = str("renderer.shaderPack.import.zip"),
                subtitle = str("renderer.shaderPack.import.zip.description"),
                busy = busyId == IMPORT_BUSY_ID,
                enabled = busyId == null,
                onDownload = {
                    zipLauncher.launch(
                        arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")
                    )
                },
                actionLabel = str("renderer.shaderPack.import.browse"),
            )
            // Companion preset packs, last: they're an add-on to a pack above, not an
            // alternative to it. Each gets its own heading because it's a distinct
            // collection with its own name, and its row disables itself until the pack it
            // is written against is actually installed.
            ShaderRepo.companionSources().forEach { source ->
                // Re-checked when the installed list changes, so it un-greys the moment
                // the base pack lands rather than on the next visit.
                val met = remember(installed.size, source.id) {
                    ShaderRepo.requirementMet(context, source)
                }
                SectionTitle(source.name, str("renderer.shaderPack.companion.description"))
                RemoteShaderRow(
                    controllerId = "shaderPack.remote.${source.id}",
                    title = source.name,
                    subtitle = if (met) source.description
                    else str("renderer.shaderPack.companion.needsBase"),
                    busy = busyId == source.id,
                    enabled = busyId == null && met,
                    onDownload = { startDownload(source) },
                )
            }
        }

        busyId?.let {
            val ex = extracted
            val dl = downloaded
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        when {
                            // Extract wins the label once it starts: the phases are
                            // sequential, so a stale byte count would just confuse.
                            ex != null -> "${str("renderer.shaderPack.extracting")} ${ex.first} / ${ex.second}"
                            dl != null -> "${str("renderer.shaderPack.downloading")} ${formatMb(dl.first)} / ${formatMb(dl.second)}"
                            else -> str("renderer.shaderPack.starting")
                        },
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        modifier = Modifier.weight(1f),
                    )
                    TextButton(
                        onClick = { cancelFlag.set(true) },
                        modifier = Modifier.controllerFocusable(
                            "shaderPack.cancel",
                            onConfirm = { cancelFlag.set(true) },
                        ),
                    ) { Text(str("action.cancel")) }
                }
                val fraction = when {
                    ex != null && ex.second > 0 -> ex.first.toFloat() / ex.second
                    dl != null && dl.second > 0 -> dl.first.toFloat() / dl.second
                    else -> null
                }
                if (fraction != null) {
                    LinearProgressIndicator(progress = { fraction }, modifier = Modifier.fillMaxWidth())
                } else {
                    LinearProgressIndicator(Modifier.fillMaxWidth())
                }
            }
        }

        message?.let { Text(it, color = MaterialTheme.colorScheme.primary) }
        if (installed.isNotEmpty()) {
            Text(
                str("renderer.shaderPack.hint"),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun InstalledPackRow(controllerId: String, title: String, subtitle: String, onDelete: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
    ) {
        Row(Modifier.padding(13.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                Text(title, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            TextButton(
                onClick = onDelete,
                modifier = Modifier.controllerFocusable("$controllerId.delete", onConfirm = onDelete),
            ) { Text(str("action.delete"), color = MaterialTheme.colorScheme.error) }
        }
    }
}

@Composable
private fun RemoteShaderRow(
    controllerId: String,
    title: String,
    subtitle: String,
    busy: Boolean,
    enabled: Boolean,
    onDownload: () -> Unit,
    /** Button text. Defaults to "Get" (a download); the import rows say "Browse", since
     *  nothing is being fetched. */
    actionLabel: String? = null,
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
    ) {
        Row(Modifier.padding(13.dp), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            Column(Modifier.weight(1f)) {
                Text(title, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (subtitle.isNotBlank()) {
                    Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
            }
            if (busy) {
                CircularProgressIndicator(Modifier.size(18.dp), strokeWidth = 2.dp)
            } else {
                OutlinedButton(
                    onClick = onDownload,
                    enabled = enabled,
                    shape = RoundedCornerShape(12.dp),
                    modifier = Modifier.controllerFocusable(controllerId, RoundedCornerShape(12.dp), onConfirm = { if (enabled) onDownload() }),
                ) { Text(actionLabel ?: str("backend.driver.get")) }
            }
        }
    }
}

/** Download progress is only ever read as "how much of a ~51MB pack is left",
 *  so MB with one decimal is the useful resolution. -1 is the no-Content-Length
 *  case. */
private fun formatMb(bytes: Long): String =
    if (bytes < 0) "?" else String.format(Locale.US, "%.1f MB", bytes / 1_048_576.0)
