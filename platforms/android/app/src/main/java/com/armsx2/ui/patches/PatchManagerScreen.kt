package com.armsx2.ui.patches

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.GameInfo
import com.armsx2.PatchRepo
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.SettingSwitchRow
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.settings.controllerFocusable
import java.io.File

@Composable
fun PatchManagerScreen(onBack: () -> Unit, game: GameInfo? = null, viewModel: PatchManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let(viewModel::import) }
    // Keyed on the game (this screen shares one Activity-scoped VM with the settings tab), and
    // resets the online browser first so a previous game's fetched results don't linger here.
    LaunchedEffect(game?.uri) { viewModel.resetOnlineForGame(); viewModel.refresh() }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
            ArmsTopBar(
                title = str("patches.dialog.patchesAndCheats"),
                leading = { RoundAction("←", str("action.back"), onBack) },
                actions = {
                    RoundAction("＋", str("action.import"), { picker.launch(arrayOf("text/plain", "application/octet-stream", "*/*")) })
                    RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                },
            )
            OnlineBrowser(state, viewModel, game, Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp))
            BoxWithConstraints(Modifier.fillMaxWidth()) {
                val compact = maxWidth < 820.dp
                if (compact) {
                    Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp)) {
                        PatchOptions(state, viewModel, Modifier.fillMaxWidth())
                        Spacer(Modifier.padding(top = 10.dp))
                        PatchFiles(state, viewModel, Modifier.fillMaxWidth())
                    }
                } else {
                    Row(
                        Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp),
                        horizontalArrangement = Arrangement.spacedBy(14.dp),
                    ) {
                        PatchOptions(state, viewModel, Modifier.width(310.dp))
                        PatchFiles(state, viewModel, Modifier.weight(1f))
                    }
                }
            }
        }
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error == null) str("action.ok") else str("patches.dialog.patchesAndCheats")) },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage) { Text(str("action.ok")) } },
        )
    }
}

/**
 * Cheats/patches as a Settings-hub tab — lets users manage PNACH cheats outside a game.
 * Reuses the full PatchManager stack (per-cheat toggles + online browser); it just drops
 * the screen chrome (ArmsBackdrop / ArmsTopBar / own scroll) since the settings hub
 * already supplies those, and re-surfaces Import/Refresh as an inline action row.
 */
@Composable
fun PatchesSettingsTab(game: GameInfo? = null, viewModel: PatchManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let(viewModel::import) }
    // Keyed on the game, not Unit: the scope switch above hands this tab a different game
    // (the game, or null for Global), and refresh() is what re-reads the tier. Keyed on
    // Unit it read once and then showed the wrong tier's values for the rest of the visit.
    // resetOnlineForGame() first, so a previous game's fetched cheats/patches don't linger in
    // this game's browser (the fetch is manual, so nothing else clears them).
    LaunchedEffect(game?.uri) { viewModel.resetOnlineForGame(); viewModel.refresh() }

    Column(Modifier.fillMaxWidth()) {
        Row(
            Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            RoundAction("＋", str("action.import"), { picker.launch(arrayOf("text/plain", "application/octet-stream", "*/*")) })
            RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
        }
        OnlineBrowser(state, viewModel, game, Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 4.dp))
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val compact = maxWidth < 820.dp
            if (compact) {
                Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp)) {
                    PatchOptions(state, viewModel, Modifier.fillMaxWidth())
                    Spacer(Modifier.padding(top = 10.dp))
                    PatchFiles(state, viewModel, Modifier.fillMaxWidth())
                }
            } else {
                Row(
                    Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp),
                    horizontalArrangement = Arrangement.spacedBy(14.dp),
                ) {
                    PatchOptions(state, viewModel, Modifier.width(310.dp))
                    PatchFiles(state, viewModel, Modifier.weight(1f))
                }
            }
        }
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error == null) str("action.ok") else str("patches.dialog.patchesAndCheats")) },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage) { Text(str("action.ok")) } },
        )
    }
}

@Composable
private fun PatchOptions(state: PatchManagerUiState, viewModel: PatchManagerViewModel, modifier: Modifier) {
    GlassPanel(modifier) {
        Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
            SectionTitle(str("ra.options.header"), str("scope.global"))
            SettingSwitchRow(
                str("patches.enablePatches.label"), str("patches.applyAtBoot"), state.settings.enablePatches,
                onCheckedChange = { value -> viewModel.update { it.copy(enablePatches = value) } },
                modifier = Modifier.controllerFocusable(
                    "patches.enablePatches",
                    onConfirm = { viewModel.update { it.copy(enablePatches = !state.settings.enablePatches) } },
                    onLeft = { if (state.settings.enablePatches) viewModel.update { it.copy(enablePatches = false) } },
                    onRight = { if (!state.settings.enablePatches) viewModel.update { it.copy(enablePatches = true) } },
                ),
            )
            SettingSwitchRow(
                str("patches.cheats.label"), str("patches.pasteImportHint"), state.settings.enableCheats,
                onCheckedChange = { value -> viewModel.update { it.copy(enableCheats = value) } },
                modifier = Modifier.controllerFocusable(
                    "patches.enableCheats",
                    onConfirm = { viewModel.update { it.copy(enableCheats = !state.settings.enableCheats) } },
                    onLeft = { if (state.settings.enableCheats) viewModel.update { it.copy(enableCheats = false) } },
                    onRight = { if (!state.settings.enableCheats) viewModel.update { it.copy(enableCheats = true) } },
                ),
            )
            SettingSwitchRow(
                // Own description rather than the shared "applies at boot" line: this one silently
                // auto-applies a patch to every game that has one, which reads nothing like
                // "enable widescreen patches" and cost a long GT4 rendering hunt to track down.
                str("patches.widescreen.label"), str("patches.widescreen.description"), state.settings.enableWideScreenPatches,
                onCheckedChange = { value -> viewModel.update { it.copy(enableWideScreenPatches = value) } },
                modifier = Modifier.controllerFocusable(
                    "patches.widescreen",
                    onConfirm = { viewModel.update { it.copy(enableWideScreenPatches = !state.settings.enableWideScreenPatches) } },
                    onLeft = { if (state.settings.enableWideScreenPatches) viewModel.update { it.copy(enableWideScreenPatches = false) } },
                    onRight = { if (!state.settings.enableWideScreenPatches) viewModel.update { it.copy(enableWideScreenPatches = true) } },
                ),
            )
            SettingSwitchRow(
                str("patches.noInterlacing.label"), str("patches.applyAtBoot"), state.settings.enableNoInterlacingPatches,
                onCheckedChange = { value -> viewModel.update { it.copy(enableNoInterlacingPatches = value) } },
                modifier = Modifier.controllerFocusable(
                    "patches.noInterlacing",
                    onConfirm = { viewModel.update { it.copy(enableNoInterlacingPatches = !state.settings.enableNoInterlacingPatches) } },
                    onLeft = { if (state.settings.enableNoInterlacingPatches) viewModel.update { it.copy(enableNoInterlacingPatches = false) } },
                    onRight = { if (!state.settings.enableNoInterlacingPatches) viewModel.update { it.copy(enableNoInterlacingPatches = true) } },
                ),
            )
            // HostFS (host: filesystem) — lets ELF/homebrew and certain advanced mods read
            // from the host. Native field + apply already exist; this restores the toggle.
            SettingSwitchRow(
                str("patches.hostFs.label"), str("patches.hostFs.description"), state.settings.hostFs,
                onCheckedChange = { value -> viewModel.update { it.copy(hostFs = value) } },
                modifier = Modifier.controllerFocusable(
                    "patches.hostFs",
                    onConfirm = { viewModel.update { it.copy(hostFs = !state.settings.hostFs) } },
                    onLeft = { if (state.settings.hostFs) viewModel.update { it.copy(hostFs = false) } },
                    onRight = { if (!state.settings.hostFs) viewModel.update { it.copy(hostFs = true) } },
                ),
            )
        }
    }
}

@Composable
private fun OnlineBrowser(
    state: PatchManagerUiState,
    viewModel: PatchManagerViewModel,
    game: GameInfo?,
    modifier: Modifier,
) {
    GlassPanel(modifier) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            SectionTitle(str("patches.online.header"), game?.title ?: str("scope.game"))
            // Hard game guard: online results belong to the game they were fetched for. The whole
            // patch UI shares one Activity-scoped VM, so a previous game's results must never
            // render in another game's tab (the reported "GT4 shows GTA:SA's cheats").
            val forThisGame = state.onlineForGameKey == (game?.uri?.toString() ?: "")
            val entries = if (forThisGame) state.onlineEntries else emptyList()
            when {
                state.onlineLoading && forThisGame -> Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(10.dp))
                    Text(str("patches.online.loading"))
                }
                entries.isEmpty() -> Button(
                    onClick = { viewModel.fetchOnline(game) },
                    modifier = Modifier.controllerFocusable("patches.online.fetch", onConfirm = { viewModel.fetchOnline(game) }),
                ) {
                    Text(str("patches.online.fetch"))
                }
                else -> {
                    if (state.onlineTitle.isNotBlank()) {
                        Text(state.onlineTitle, style = MaterialTheme.typography.titleSmall)
                    }
                    // Install/Refresh sit ABOVE the lists. They used to render after both sections,
                    // so with a large cheat list (54 for GoW2, often far more) the Install button
                    // was pushed off the bottom of the screen: users ticked a patch, found no way
                    // to apply it, and backed out — and the tick is only a transient selection, so
                    // nothing was ever installed and the tick was gone on return. Pinned above the
                    // lists, the action stays reachable no matter how long they get.
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                        Button(
                            onClick = viewModel::installSelected,
                            enabled = state.onlineSelected.isNotEmpty(),
                            modifier = Modifier.controllerFocusable("patches.online.install", onConfirm = { if (state.onlineSelected.isNotEmpty()) viewModel.installSelected() }),
                        ) {
                            Text("${str("patches.online.install")} (${state.onlineSelected.size})")
                        }
                        TextButton(
                            onClick = { viewModel.fetchOnline(game) },
                            modifier = Modifier.controllerFocusable("patches.online.refresh", onConfirm = { viewModel.fetchOnline(game) }),
                        ) { Text(str("games.card.refresh")) }
                    }
                    // Patches and cheats each get their own collapsible section. Patches (few — the
                    // whole point of searching) expand by default; cheats (often thousands) collapse
                    // by default, which kills BOTH the endless scroll AND the lag: a collapsed
                    // section doesn't compose its rows, and each row otherwise registers a
                    // controller-nav entry. Expand cheats deliberately when you actually want them.
                    val patches = entries.filter { it.source == "patches" }
                    val cheats = entries.filter { it.source != "patches" }
                    if (patches.isNotEmpty()) {
                        CollapsibleOnlineSection("${str("patches.section.patches")} (${patches.size})", initiallyExpanded = true) {
                            patches.forEach { entry ->
                                OnlineEntryRow(entry, entry.name in state.onlineSelected) { viewModel.toggleOnline(entry.name) }
                            }
                        }
                    }
                    if (cheats.isNotEmpty()) {
                        CollapsibleOnlineSection("${str("patches.section.cheats")} (${cheats.size})", initiallyExpanded = false) {
                            cheats.forEach { entry ->
                                OnlineEntryRow(entry, entry.name in state.onlineSelected) { viewModel.toggleOnline(entry.name) }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun OnlineEntryRow(entry: PatchRepo.Entry, checked: Boolean, onToggle: () -> Unit) {
    Surface(
        onClick = onToggle,
        modifier = Modifier.fillMaxWidth().controllerFocusable(
            "patches.online.entry.${entry.name}",
            RoundedCornerShape(14.dp),
            onConfirm = onToggle,
            onLeft = { if (checked) onToggle() },
            onRight = { if (!checked) onToggle() },
        ),
        shape = RoundedCornerShape(14.dp),
        color = if (checked) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
        border = BorderStroke(
            1.dp,
            if (checked) MaterialTheme.colorScheme.primary.copy(alpha = 0.5f) else MaterialTheme.colorScheme.outline.copy(alpha = 0.42f),
        ),
    ) {
        Row(Modifier.padding(horizontal = 10.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            Checkbox(checked = checked, onCheckedChange = { onToggle() })
            Spacer(Modifier.width(6.dp))
            Column(Modifier.weight(1f)) {
                Text(entry.name, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                if (entry.description.isNotBlank()) {
                    Text(entry.description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2, overflow = TextOverflow.Ellipsis)
                }
            }
            Spacer(Modifier.width(8.dp))
            StatusChip(entry.source)
        }
    }
}

/** A real collapsible section for the online browser (the shared CollapsibleSection widget is
 *  header-only — it always composes its content). Only composes [content] when expanded, so a
 *  collapsed cheats list of thousands neither draws nor registers its per-row controller-nav
 *  entries — that's what makes the giant list cheap until you deliberately open it. */
@Composable
private fun CollapsibleOnlineSection(
    title: String,
    initiallyExpanded: Boolean,
    content: @Composable () -> Unit,
) {
    var expanded by remember { mutableStateOf(initiallyExpanded) }
    Surface(
        onClick = { expanded = !expanded },
        shape = RoundedCornerShape(12.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.45f),
        modifier = Modifier.fillMaxWidth().controllerFocusable(
            "patches.online.section.$title", RoundedCornerShape(12.dp), onConfirm = { expanded = !expanded },
        ),
    ) {
        Row(Modifier.padding(horizontal = 12.dp, vertical = 10.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(
                if (expanded) "▾" else "▸",
                style = MaterialTheme.typography.titleMedium,
                color = MaterialTheme.colorScheme.primary,
            )
            Spacer(Modifier.width(10.dp))
            Text(title, style = MaterialTheme.typography.titleSmall, modifier = Modifier.weight(1f))
        }
    }
    if (expanded) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) { content() }
    }
}

@Composable
private fun PatchFiles(state: PatchManagerUiState, viewModel: PatchManagerViewModel, modifier: Modifier) {
    Column(modifier) {
        SectionTitle(str("patches.installedHeader"), state.files.size.toString())
        if (state.files.isEmpty()) {
            PatchFilesEmptyState()
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                state.files.forEach { file ->
                    val expanded = state.localExpandedPath == file.absolutePath
                    PatchFileRow(
                        file = file,
                        expanded = expanded,
                        cheats = if (expanded) state.localCheats else emptyList(),
                        onExpand = { viewModel.expandLocal(file) },
                        onToggleCheat = viewModel::toggleLocalCheat,
                        onDelete = { viewModel.delete(file) },
                    )
                }
            }
        }
    }
}

@Composable
private fun PatchFilesEmptyState() {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
        tonalElevation = 1.dp,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 18.dp, vertical = 18.dp),
            horizontalArrangement = Arrangement.spacedBy(14.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Surface(
                shape = RoundedCornerShape(14.dp),
                color = MaterialTheme.colorScheme.primaryContainer,
            ) {
                BoxWithConstraints(
                    modifier = Modifier.size(48.dp),
                    contentAlignment = Alignment.Center,
                ) {
                    Text(
                        text = "✦",
                        style = MaterialTheme.typography.headlineSmall,
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                }
            }
            Column(Modifier.weight(1f)) {
                Text(
                    text = str("patches.noFilesInstalled"),
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.height(4.dp))
                Text(
                    text = str("patches.pasteImportHint"),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun PatchFileRow(
    file: File,
    expanded: Boolean,
    cheats: List<PatchRepo.LocalCheat>,
    onExpand: () -> Unit,
    onToggleCheat: (String) -> Unit,
    onDelete: () -> Unit,
) {
    Surface(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
    ) {
        Column {
            Row(
                Modifier.fillMaxWidth().clickable(onClick = onExpand).controllerFocusable("patches.file.${file.absolutePath}", onConfirm = onExpand).padding(14.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(if (expanded) "▾" else "▸", color = MaterialTheme.colorScheme.primary)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(file.name, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(file.parentFile?.name.orEmpty(), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                TextButton(onClick = onDelete, modifier = Modifier.controllerFocusable("patches.file.${file.absolutePath}.delete", onConfirm = onDelete)) { Text(str("action.delete")) }
            }
            if (expanded) {
                if (cheats.isEmpty()) {
                    Text(
                        str("patches.local.noCheats"),
                        Modifier.padding(start = 42.dp, end = 14.dp, bottom = 12.dp),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                } else {
                    Column(Modifier.padding(start = 34.dp, end = 12.dp, bottom = 8.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                        cheats.forEach { cheat -> LocalCheatRow(cheat) { onToggleCheat(cheat.name) } }
                    }
                }
            }
        }
    }
}

@Composable
private fun LocalCheatRow(cheat: PatchRepo.LocalCheat, onToggle: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().clickable(onClick = onToggle).controllerFocusable(
            "patches.cheat.${cheat.name}",
            onConfirm = onToggle,
            onLeft = { if (cheat.enabled) onToggle() },
            onRight = { if (!cheat.enabled) onToggle() },
        ).padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(cheat.name, style = MaterialTheme.typography.bodyMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
            if (cheat.description.isNotBlank()) {
                Text(cheat.description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2, overflow = TextOverflow.Ellipsis)
            }
        }
        Spacer(Modifier.width(8.dp))
        Switch(checked = cheat.enabled, onCheckedChange = { onToggle() })
    }
}
