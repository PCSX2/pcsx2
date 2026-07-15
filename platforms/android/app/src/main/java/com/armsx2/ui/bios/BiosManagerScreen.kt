package com.armsx2.ui.bios

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
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
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.GameInfo
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.settings.controllerFocusable
import com.armsx2.ui.theme.Success

@Composable
fun BiosManagerScreen(onBack: () -> Unit, game: GameInfo? = null, viewModel: BiosManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    var deleteTarget by remember { mutableStateOf<InstalledBios?>(null) }
    var actionsMenuExpanded by remember { mutableStateOf(false) }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let(viewModel::import)
    }
    // Item 7: folder-import — point at a folder of BIOSes; all valid ones are imported and shown
    // in the list below to choose between (refresh parity).
    val folderPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        uri?.let(viewModel::importFolder)
    }
    // Point the per-game controls at the long-pressed game (its settingsKey) when opened from
    // the library; null (drawer) falls back to the loaded game. setGameContext also refreshes.
    LaunchedEffect(game?.settingsKey) { viewModel.setGameContext(game?.settingsKey) }

    ArmsBackdrop {
        // Column + verticalScroll (not LazyColumn): controller nav registers rows via a SideEffect
        // when they compose, and LazyColumn never composes off-screen rows — so on a pad the
        // selection would stick partway down and the camera wouldn't follow. Composing every row
        // keeps registry nav + bringIntoView working. The BIOS list is short, so there's no cost.
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 8.dp),
            verticalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            ArmsTopBar(
                title = str("setup.page.bios.title"),
                leading = { RoundAction("←", str("action.back"), onBack) },
                actions = {
                    Box {
                        RoundAction(
                            glyph = "⋮",
                            description = str("games.toolbar.more"),
                            onClick = { actionsMenuExpanded = true },
                            selected = actionsMenuExpanded,
                        )
                        BiosActionsMenu(
                            expanded = actionsMenuExpanded,
                            onDismiss = { actionsMenuExpanded = false },
                            onImportFile = { picker.launch(arrayOf("application/octet-stream", "*/*")) },
                            onImportFolder = { folderPicker.launch(null) },
                            onRefresh = viewModel::refresh,
                        )
                    }
                },
                horizontalPadding = 0.dp,
            )
            if (state.items.isEmpty() && !state.busy) {
                EmptyState(
                    title = str("setup.bios.error.noneFound"),
                    message = str("setup.step.bios.description"),
                    actionLabel = str("action.import"),
                    onAction = { picker.launch(arrayOf("application/octet-stream", "*/*")) },
                    modifier = Modifier.fillMaxWidth().height(280.dp)
                        .controllerFocusable(
                            "bios.empty.import",
                            RoundedCornerShape(24.dp),
                            onConfirm = { picker.launch(arrayOf("application/octet-stream", "*/*")) },
                        ),
                )
            } else {
                if (state.busy) CircularProgressIndicator(Modifier.size(22.dp), strokeWidth = 2.dp)
                state.items.forEach { item ->
                    BiosRow(
                        item = item,
                        showGameAssign = state.gameKey != null,
                        perGameActive = state.perGameBios?.equals(item.file.name, ignoreCase = true) == true,
                        onSelect = { viewModel.select(item.file) },
                        onAssignGame = { viewModel.assignToGame(item) },
                        onClearGame = { viewModel.clearGameBios() },
                        onDelete = { deleteTarget = item },
                    )
                }
            }
            Spacer(Modifier.height(12.dp))
        }
    }

    deleteTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text(str("action.delete")) },
            text = { Text(item.file.name) },
            confirmButton = {
                TextButton(onClick = { viewModel.delete(item); deleteTarget = null }) {
                    Text(str("action.delete"), color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = { TextButton(onClick = { deleteTarget = null }) { Text(str("action.cancel")) } },
        )
    }
    state.error?.let { error ->
        AlertDialog(
            onDismissRequest = viewModel::dismissError,
            title = { Text(str("setup.page.bios.title")) },
            text = { Text(error) },
            confirmButton = { TextButton(onClick = viewModel::dismissError) { Text(str("action.ok")) } },
        )
    }
}

@Composable
private fun BiosActionsMenu(
    expanded: Boolean,
    onDismiss: () -> Unit,
    onImportFile: () -> Unit,
    onImportFolder: () -> Unit,
    onRefresh: () -> Unit,
) {
    fun closeThen(action: () -> Unit) {
        onDismiss()
        action()
    }

    DropdownMenu(
        expanded = expanded,
        onDismissRequest = onDismiss,
        modifier = Modifier.widthIn(min = 280.dp, max = 340.dp),
        shape = RoundedCornerShape(22.dp),
        containerColor = MaterialTheme.colorScheme.surface,
        tonalElevation = 8.dp,
        shadowElevation = 14.dp,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
    ) {
        Text(
            text = str("setup.page.bios.title"),
            modifier = Modifier.padding(horizontal = 18.dp, vertical = 10.dp),
            style = MaterialTheme.typography.labelLarge,
            color = MaterialTheme.colorScheme.primary,
            fontWeight = FontWeight.Bold,
        )
        BiosActionMenuItem("＋", str("action.import")) { closeThen(onImportFile) }
        BiosActionMenuItem("▣", str("action.importFolder")) { closeThen(onImportFolder) }
        BiosActionMenuItem("↻", str("games.card.refresh")) { closeThen(onRefresh) }
    }
}

@Composable
private fun BiosActionMenuItem(glyph: String, label: String, onClick: () -> Unit) {
    DropdownMenuItem(
        text = {
            Text(
                text = label,
                style = MaterialTheme.typography.titleSmall,
                fontWeight = FontWeight.Medium,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
        },
        onClick = onClick,
        leadingIcon = {
            Surface(
                modifier = Modifier.size(36.dp),
                shape = RoundedCornerShape(11.dp),
                color = MaterialTheme.colorScheme.primaryContainer,
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Text(
                        text = glyph,
                        fontSize = 18.sp,
                        fontWeight = FontWeight.Bold,
                        color = MaterialTheme.colorScheme.onPrimaryContainer,
                    )
                }
            }
        },
        contentPadding = PaddingValues(horizontal = 14.dp, vertical = 4.dp),
    )
}

@Composable
private fun BiosRow(
    item: InstalledBios,
    showGameAssign: Boolean,
    perGameActive: Boolean,
    onSelect: () -> Unit,
    onAssignGame: () -> Unit,
    onClearGame: () -> Unit,
    onDelete: () -> Unit,
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        color = if (item.selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, if (item.selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
    ) {
        Column(Modifier.padding(14.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(item.info.regionFlag, fontSize = 30.sp)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(item.file.name, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(
                        "${item.info.description} · ${item.info.versionString} · ${item.info.zone}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
                if (item.selected) StatusChip(str("backend.driver.active"), Success)
                if (perGameActive) {
                    Spacer(Modifier.width(6.dp))
                    StatusChip(str("bios.thisGame.active"), Success)
                }
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                // Per-game BIOS: pin this BIOS to the loaded game, or revert it to global.
                if (showGameAssign) {
                    if (perGameActive) {
                        TextButton(
                            onClick = onClearGame,
                            modifier = Modifier.controllerFocusable("bios.clearGame.${item.file.absolutePath}", onConfirm = onClearGame),
                        ) { Text(str("bios.useGlobal")) }
                    } else {
                        OutlinedButton(
                            onClick = onAssignGame,
                            shape = RoundedCornerShape(12.dp),
                            modifier = Modifier.controllerFocusable("bios.thisGame.${item.file.absolutePath}", RoundedCornerShape(12.dp), onConfirm = onAssignGame),
                        ) { Text(str("bios.thisGame")) }
                    }
                    Spacer(Modifier.width(8.dp))
                }
                if (!item.selected) OutlinedButton(
                    onClick = onSelect,
                    shape = RoundedCornerShape(12.dp),
                    modifier = Modifier.controllerFocusable(
                        "bios.use.${item.file.absolutePath}",
                        RoundedCornerShape(12.dp),
                        onConfirm = onSelect,
                    ),
                ) { Text(str("setup.bios.useSelected")) }
                Spacer(Modifier.width(8.dp))
                TextButton(
                    onClick = onDelete,
                    enabled = !item.selected,
                    modifier = Modifier.controllerFocusable(
                        if (item.selected) null else "bios.delete.${item.file.absolutePath}",
                        onConfirm = onDelete,
                    ),
                ) { Text(str("action.delete")) }
            }
        }
    }
}
