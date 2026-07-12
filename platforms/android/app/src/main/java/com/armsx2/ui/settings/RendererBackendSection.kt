package com.armsx2.ui.settings

import android.content.Context
import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.CustomDriver
import com.armsx2.Main
import com.armsx2.config.Settings
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import com.armsx2.ui.Colors
import com.armsx2.ui.InGameOverlay
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Renderer-backend chooser for the in-game settings Renderer tab. Ports the
 * renderer + custom-driver picker that used to live on the first-run setup
 * page (SetupImpl.SetupRendererContent) into the settings overlay, since that
 * setup screen was removed.
 *
 * - "Graphics API" selects the host display backend: Auto (emucore's
 *   GSUtil::GetPreferredRenderer), OpenGL, or Vulkan. Written to
 *   [Main.renderer] + prefs, applied by [Main.applyRendererPrefs] on the next
 *   VM (re)start.
 * - When Vulkan is selected, a GPU Driver list lets the user replace the
 *   system Vulkan ICD with an AdrenoToolsDrivers pack (Mesa Turnip & friends),
 *   exactly like the old setup page: pick Default, an installed driver, import
 *   a local .zip, or browse the K11MCH1/AdrenoToolsDrivers GitHub releases.
 *
 * Renderer / driver changes only take effect on a renderer (re)open, so the
 * section ends with an "Apply & Restart" action that relaunches the current
 * game (mirrors the toolbar RestartButton).
 */
@Composable
fun RendererBackendSection(state: MutableState<Settings>) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val scope = rememberCoroutineScope()
    val s = state.value

    val rendererIds = listOf("auto", "opengl", "vulkan", "software")
    val rendererLabels = listOf(str("backend.renderer.auto"), "OpenGL", "Vulkan", str("backend.renderer.software"))
    val selIdx = rendererIds.indexOf(s.renderer).coerceAtLeast(0)

    SegmentedRow(
        label = str("backend.graphicsApi.label"),
        options = rendererLabels,
        selectedIndex = selIdx,
        onChange = { idx ->
            val pick = rendererIds[idx]
            if (pick != s.renderer) {
                // Persist scope-aware (per-game when the overlay scope is Game).
                // The backend switch itself happens on the next renderer start
                // (Apply & Restart / next launch → Main.applyRendererPrefs).
                InGameOverlay.saveSettings(s.copy(renderer = pick))
                Main.renderer.value = pick
                // A custom driver is only meaningful for Vulkan — drop the
                // pick on the OGL/Auto path so it doesn't linger as a stale
                // selection if the user toggles back.
                if (pick != "vulkan") {
                    Main.customDriverId.value = null
                    Main.prefs.edit().putString("customDriverId", "").apply()
                }
            }
        },
    )

    if (s.renderer == "vulkan") {
        SettingsDivider()
        GpuDriverSection(context, scope)
    }

    SettingsDivider()
    Box(
        Modifier
            .fillMaxWidth()
            .background(rowAura())
            .padding(horizontal = 6.dp, vertical = 6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Text(
                str("backend.applyNote"),
                color = Color(0xFFAAAAAA),
                fontSize = 11.sp,
                modifier = Modifier.weight(1f),
            )
            Spacer(Modifier.width(8.dp))
            PillButton(text = str("backend.applyRestart"), accent = true) { Main.restart() }
        }
    }
}

@Composable
private fun GpuDriverSection(context: Context, scope: kotlinx.coroutines.CoroutineScope) {
    val installed = remember { mutableStateListOf<CustomDriver.InstalledDriver>() }
    var showBrowser by remember { mutableStateOf(false) }
    var remote by remember { mutableStateOf<List<CustomDriver.RemoteDriver>?>(null) }
    var installingId by remember { mutableStateOf<String?>(null) }
    var error by remember { mutableStateOf<String?>(null) }

    fun refresh() {
        installed.clear()
        installed.addAll(CustomDriver.listInstalled(context))
    }
    LaunchedEffect(Unit) { refresh() }

    fun setDriver(id: String?) {
        Main.customDriverId.value = id
        Main.prefs.edit().putString("customDriverId", id.orEmpty()).apply()
    }

    // SAF picker for a local AdrenoToolsDrivers .zip. octet-stream is included
    // because Drive / Files-by-Google report .zip downloads as octet-stream.
    val importLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        installingId = "__import__"
        error = null
        scope.launch {
            val result = withContext(Dispatchers.IO) { CustomDriver.installFromUri(context, uri) }
            installingId = null
            if (result != null) {
                refresh()
                setDriver(result.id)
            } else {
                error = I18n.get("backend.driver.importError")
            }
        }
    }

    Spacer(Modifier.height(6.dp))
    Text(str("backend.gpuDriver.label"), color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
        modifier = Modifier.padding(horizontal = 6.dp))
    Spacer(Modifier.height(2.dp))
    Text(
        str("backend.gpuDriver.description"),
        color = Color(0xFFAAAAAA),
        fontSize = 11.sp,
        modifier = Modifier.padding(horizontal = 6.dp),
    )
    Spacer(Modifier.height(6.dp))

    DriverRow(
        name = str("backend.driver.default"),
        sub = str("backend.driver.systemVulkan"),
        selected = Main.customDriverId.value == null,
        busy = false,
        onSelect = { setDriver(null) },
        onDelete = null,
    )
    installed.forEach { drv ->
        val sub = buildString {
            if (drv.vendor.isNotEmpty()) append(drv.vendor)
            if (drv.version.isNotEmpty()) {
                if (isNotEmpty()) append(" · ")
                append(drv.version)
            }
            if (isEmpty() && drv.author.isNotEmpty()) append(drv.author)
            if (isEmpty()) append(I18n.get("backend.driver.installed"))
        }
        DriverRow(
            name = drv.name,
            sub = sub,
            selected = Main.customDriverId.value == drv.id,
            busy = false,
            onSelect = { setDriver(drv.id) },
            onDelete = {
                if (Main.customDriverId.value == drv.id) setDriver(null)
                CustomDriver.delete(drv)
                refresh()
            },
        )
    }

    Spacer(Modifier.height(6.dp))
    Row(
        modifier = Modifier.fillMaxWidth().padding(horizontal = 6.dp),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        PillButton(
            text = if (installingId == "__import__") str("backend.driver.importing") else str("backend.driver.importZip"),
            accent = false,
        ) {
            if (installingId == null) {
                importLauncher.launch(arrayOf("application/zip", "application/octet-stream", "*/*"))
            }
        }
        PillButton(text = if (showBrowser) str("backend.driver.hideOnline") else str("backend.driver.browseOnline"), accent = false) {
            showBrowser = !showBrowser
        }
    }

    error?.let {
        Spacer(Modifier.height(4.dp))
        Text(it, color = Color(0xFFFF8B8B), fontSize = 11.sp, modifier = Modifier.padding(horizontal = 6.dp))
    }

    if (showBrowser) {
        LaunchedEffect(showBrowser) {
            if (remote == null) {
                error = null
                val fetched = withContext(Dispatchers.IO) { CustomDriver.fetchRemote() }
                remote = fetched
                if (fetched.isEmpty()) {
                    error = I18n.get("backend.driver.fetchError")
                }
            }
        }
        val list = remote
        Spacer(Modifier.height(6.dp))
        // GPU-based driver recommendation ("driver update assistant", Eden-style):
        // probe the physical GPU once (off the main thread) and, for Adreno, point
        // the user at the source most likely to have a good Turnip pack for it.
        val gpuName = remember { mutableStateOf<String?>(null) }
        LaunchedEffect(Unit) {
            gpuName.value = withContext(Dispatchers.IO) { com.armsx2.GpuInfo.rendererName() }
        }
        gpuName.value?.let { name ->
            val rec = com.armsx2.GpuInfo.recommendation(name)
            Column(Modifier.fillMaxWidth().padding(horizontal = 6.dp, vertical = 4.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text("GPU  ", color = Color(0xFFAAAAAA), fontSize = 11.sp)
                    Text(name, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.SemiBold,
                        maxLines = 1, overflow = TextOverflow.Ellipsis)
                }
                if (rec != null) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text("Recommended  ", color = Color(0xFFAAAAAA), fontSize = 11.sp)
                        Text(rec.sourceLabel, color = Color(0xFF4DA3FF), fontSize = 12.sp,
                            fontWeight = FontWeight.SemiBold)
                    }
                    Text(rec.reason, color = Color(0xFF808080), fontSize = 10.sp)
                }
            }
            Spacer(Modifier.height(4.dp))
        }
        if (list == null) {
            Text(str("backend.driver.loadingList"), color = Color(0xFFAAAAAA), fontSize = 11.sp,
                modifier = Modifier.padding(horizontal = 6.dp))
        } else {
            // No inner scroll / height cap: let the driver rows flow into the tab's own scroll so
            // the position-based controller nav treats them like every other settings row. A nested
            // verticalScroll here breaks D-pad nav (clipped rows report off-viewport positions, so
            // focus jumps to the buttons above or exits the section). Dropdowns keep it compact.
            Column(modifier = Modifier.fillMaxWidth()) {
                // Group the online drivers by uploader (KIMCHI / Mr Purple / Steven) into
                // collapsible dropdowns so the list isn't one long flat scroll.
                val expandedSources = remember { mutableStateMapOf<String, Boolean>() }
                list.groupBy { it.source }.forEach { (source, drivers) ->
                    // Default COLLAPSED so the section reads as a clean list of source headers
                    // (KIMCHI / Mr Purple / StevenMXZ); tap a header to expand that uploader.
                    val open = expandedSources[source] ?: false
                    Box(
                        Modifier
                            .fillMaxWidth()
                            .padding(horizontal = 6.dp, vertical = 3.dp)
                            .clip(RoundedCornerShape(8.dp))
                            .background(Color(0xFF26292B))
                            .clickable { expandedSources[source] = !open }
                            .controllerFocusable(
                                controllerId = "drvsrc:$source",
                                shape = RoundedCornerShape(8.dp),
                                onConfirm = { expandedSources[source] = !open },
                            )
                            .padding(horizontal = 10.dp, vertical = 7.dp),
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
                            Text(if (open) "▾" else "▸", color = Color(0xFFAAAAAA), fontSize = 12.sp,
                                modifier = Modifier.padding(end = 8.dp))
                            Text(friendlyDriverSource(source), color = Color.White, fontSize = 13.sp,
                                fontWeight = FontWeight.SemiBold, modifier = Modifier.weight(1f),
                                maxLines = 1, overflow = TextOverflow.Ellipsis)
                            Text("${drivers.size}", color = Color(0xFFAAAAAA), fontSize = 11.sp)
                        }
                    }
                    if (open) {
                        drivers.forEach { rd ->
                            val isInstalled = installed.any { it.id == rd.id }
                            DriverBrowserRow(
                                remote = rd,
                                installed = isInstalled,
                                installing = installingId == rd.id,
                                onInstall = {
                                    if (installingId != null) return@DriverBrowserRow
                                    installingId = rd.id
                                    error = null
                                    scope.launch {
                                        val result = withContext(Dispatchers.IO) {
                                            CustomDriver.download(context, rd)
                                        }
                                        installingId = null
                                        if (result != null) {
                                            refresh()
                                            setDriver(result.id)
                                            showBrowser = false
                                        } else {
                                            error = "Install failed for ${rd.assetName}. The download or extract step errored — try again."
                                        }
                                    }
                                },
                                onSelect = {
                                    setDriver(rd.id)
                                    showBrowser = false
                                },
                            )
                        }
                    }
                }
            }
        }
    }
}

/** One installed/Default driver row: tap to select, optional trash to delete. */
@Composable
private fun DriverRow(
    name: String,
    sub: String,
    selected: Boolean,
    busy: Boolean,
    onSelect: () -> Unit,
    onDelete: (() -> Unit)?,
) {
    val border = if (selected) Colors.pasx2_blue else Color.White.copy(alpha = 0.08f)
    val bg = if (selected) Colors.pasx2_blue.copy(alpha = 0.18f) else Color(0xFF1F2123).copy(alpha = 0.5f)
    Box(
        Modifier
            .fillMaxWidth()
            .padding(horizontal = 6.dp, vertical = 3.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(bg)
            .border(1.dp, border, RoundedCornerShape(8.dp))
            .clickable(enabled = !busy, onClick = onSelect)
            .controllerFocusable(
                controllerId = "driver:$name",
                shape = RoundedCornerShape(8.dp),
                onConfirm = { if (!busy) onSelect() },
            )
            .padding(horizontal = 10.dp, vertical = 6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f)) {
                Text(name, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold,
                    maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(sub, color = Color(0xFFAAAAAA), fontSize = 11.sp, maxLines = 1,
                    overflow = TextOverflow.Ellipsis)
            }
            if (selected) {
                Text(str("backend.driver.active"), color = Colors.pasx2_blue, fontSize = 11.sp, fontWeight = FontWeight.Bold)
            }
            if (onDelete != null) {
                Spacer(Modifier.width(10.dp))
                Box(
                    Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF3A1A1A))
                        .clickable(onClick = onDelete)
                        .padding(horizontal = 8.dp, vertical = 3.dp),
                ) {
                    Text(str("action.delete"), color = Color(0xFFFF6B6B), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
    }
}

/** One remote (downloadable) driver row from the GitHub release list. */
@Composable
private fun DriverBrowserRow(
    remote: CustomDriver.RemoteDriver,
    installed: Boolean,
    installing: Boolean,
    onInstall: () -> Unit,
    onSelect: () -> Unit,
) {
    Box(
        Modifier
            .fillMaxWidth()
            .padding(horizontal = 6.dp, vertical = 3.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(Color(0xFF1F2123).copy(alpha = 0.5f))
            .border(1.dp, Color.White.copy(alpha = 0.08f), RoundedCornerShape(8.dp))
            // Controller focus: land on each downloadable driver; A installs it (or Uses it if
            // already installed) so the whole list is navigable, not just the inline pill.
            .controllerFocusable(
                controllerId = "remote:${remote.id}",
                shape = RoundedCornerShape(8.dp),
                onConfirm = { if (installed) onSelect() else if (!installing) onInstall() },
            )
            .padding(horizontal = 10.dp, vertical = 6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Column(modifier = Modifier.weight(1f)) {
                Text(remote.assetName, color = Color.White, fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(
                    remote.releaseName,
                    color = Color(0xFFAAAAAA), fontSize = 11.sp,
                    maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            Spacer(Modifier.width(10.dp))
            when {
                installing -> Text(str("backend.driver.installing"), color = Color(0xFFAACCFF), fontSize = 11.sp,
                    fontWeight = FontWeight.Bold)
                installed -> PillButton(text = str("backend.driver.use"), accent = false, onClick = onSelect)
                else -> PillButton(text = str("backend.driver.install"), accent = true, onClick = onInstall)
            }
        }
    }
}

/** Map a driver-source repo label to a short, friendly uploader name for the group headers.
 *  Steven/Purple are matched first so "StevenMXZ · Adreno-Tools" doesn't fall into the KIMCHI
 *  (AdrenoTools) bucket. */
private fun friendlyDriverSource(source: String): String = when {
    source.contains("Steven", ignoreCase = true) -> "StevenMXZ"
    source.contains("Purple", ignoreCase = true) -> "Mr Purple"
    source.contains("K11MCH1", ignoreCase = true) || source.contains("AdrenoTools", ignoreCase = true) -> "KIMCHI"
    source.contains("GameHub", ignoreCase = true) || source.contains("crueter", ignoreCase = true) -> "GameHub 8Elite"
    else -> source.ifEmpty { "Other" }
}

@Composable
private fun PillButton(text: String, accent: Boolean, onClick: () -> Unit) {
    val bg = if (accent) Colors.pasx2_blue else Color(0xFF2A2A2A)
    val fg = if (accent) Color.White else Color(0xFFDDDDDD)
    Box(
        Modifier
            .clip(RoundedCornerShape(6.dp))
            .background(bg)
            .clickable(onClick = onClick)
            .controllerFocusable(
                controllerId = "button:$text",
                shape = RoundedCornerShape(6.dp),
                onConfirm = onClick,
            )
            .padding(horizontal = 12.dp, vertical = 6.dp),
    ) {
        Text(text, color = fg, fontSize = 11.sp, fontWeight = FontWeight.Bold)
    }
}
