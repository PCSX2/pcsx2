package com.armsx2.ui.memorycards

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
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
fun MemoryCardScreen(onBack: () -> Unit, game: GameInfo? = null, viewModel: MemoryCardViewModel = viewModel()) {
    val state = viewModel.state.value
    val serial = game?.serial?.takeIf { it.isNotBlank() }
    var createDialog by remember { mutableStateOf(false) }
    var deleteTarget by remember { mutableStateOf<MemoryCardItem?>(null) }
    val importer = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let(viewModel::import) }
    // Folder memory cards are directories, which OpenDocument() cannot return — without
    // this there was no way to import one at all, and zipping it produced a "card.zip.ps2"
    // that read as unformatted.
    val folderImporter = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri -> uri?.let(viewModel::importFolder) }
    // Export a (file) card out to a user-chosen location — backup, or move to another device.
    var exportPending by remember { mutableStateOf<MemoryCardItem?>(null) }
    val exporter = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/octet-stream")) { uri ->
        val src = exportPending; exportPending = null
        if (uri != null && src != null) viewModel.export(src.file, uri)
    }
    LaunchedEffect(Unit) { viewModel.refresh() }

    ArmsBackdrop {
        LazyColumn(
            Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(10.dp),
            contentPadding = PaddingValues(horizontal = 8.dp),
        ) {
            item {
                ArmsTopBar(
                    title = str("memcard.title"),
                    leading = { RoundAction("←", str("action.back"), onBack) },
                    actions = {
                        RoundAction("＋", str("memcard.newCard"), { createDialog = true })
                        RoundAction("⇩", str("action.import"), { importer.launch(arrayOf("application/octet-stream", "*/*")) })
                        RoundAction("▣", str("memcard.importFolder"), { folderImporter.launch(null) })
                        RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                    },
                    horizontalPadding = 0.dp,
                )
            }
            if (state.cards.isEmpty()) {
                item { EmptyState(str("memcard.empty"), str("memcard.size.hint"), str("memcard.create"), { createDialog = true }, Modifier.fillMaxWidth().height(280.dp).controllerFocusable("memcard.empty.create", onConfirm = { createDialog = true })) }
            } else {
                items(state.cards, key = { it.file.absolutePath }) { item ->
                    // Scope-aware: with a game in context the slot buttons write a PER-GAME
                    // override; from the library they set the global default. Previously BOTH
                    // slot buttons were always global and the separate "This game" button was
                    // hardcoded to slot 1 — so "MC2 in slot 2 for this game" was impossible to
                    // express, and every game just kept whatever card was assigned globally last.
                    val gameSlot1 = serial?.let { viewModel.perGameCard(it, 1) }
                    val gameSlot2 = serial?.let { viewModel.perGameCard(it, 2) }
                    MemoryCardRow(
                        item = item,
                        perGame = serial != null,
                        slot1Active = if (serial != null) gameSlot1.equals(item.file.name, true) else item.slot1,
                        slot2Active = if (serial != null) gameSlot2.equals(item.file.name, true) else item.slot2,
                        onSlot1 = { if (serial != null) viewModel.assignToGame(serial, 1, item) else viewModel.assign(1, item) },
                        onSlot2 = { if (serial != null) viewModel.assignToGame(serial, 2, item) else viewModel.assign(2, item) },
                        onClearSlot1 = serial?.let { s -> { viewModel.clearGameCard(s, 1) } },
                        onClearSlot2 = serial?.let { s -> { viewModel.clearGameCard(s, 2) } },
                        onExport = item.takeIf { !it.file.isDirectory }?.let { card -> { exportPending = card; exporter.launch(card.file.name) } },
                        onDelete = { deleteTarget = item },
                    )
                }
            }
            item { Spacer(Modifier.height(12.dp)) }
        }
    }

    if (createDialog) {
        CreateCardDialog(
            onDismiss = { createDialog = false },
            onCreate = { name, type, size -> viewModel.create(name, type, size); createDialog = false },
        )
    }
    deleteTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text(str("memcard.delete.confirm")) },
            text = { Text(item.file.name) },
            confirmButton = { TextButton(onClick = { viewModel.delete(item); deleteTarget = null }, modifier = Modifier.controllerFocusable("memcard.delete.confirm", onConfirm = { viewModel.delete(item); deleteTarget = null })) { Text(str("action.delete"), color = MaterialTheme.colorScheme.error) } },
            dismissButton = { TextButton(onClick = { deleteTarget = null }, modifier = Modifier.controllerFocusable("memcard.delete.cancel", onConfirm = { deleteTarget = null })) { Text(str("action.cancel")) } },
        )
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error != null) str("memcard.title") else str("action.ok")) },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage, modifier = Modifier.controllerFocusable("memcard.message.ok", onConfirm = viewModel::dismissMessage)) { Text(str("action.ok")) } },
        )
    }
}

@Composable
private fun MemoryCardRow(
    item: MemoryCardItem,
    perGame: Boolean,
    slot1Active: Boolean,
    slot2Active: Boolean,
    onSlot1: () -> Unit,
    onSlot2: () -> Unit,
    onClearSlot1: (() -> Unit)?,
    onClearSlot2: (() -> Unit)?,
    onExport: (() -> Unit)?,
    onDelete: () -> Unit,
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(19.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
    ) {
        Column(Modifier.padding(13.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(if (item.file.isDirectory) "🗀" else "▤", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.headlineMedium)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(item.file.name, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(if (item.file.isDirectory) "Folder" else humanSize(item.size), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            Row(
                Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.End,
                verticalAlignment = Alignment.CenterVertically,
            ) {
                // The slot buttons ARE the per-game control when a game is in context, so the
                // old slot-1-only "This game" button is gone. "Use global" undoes a per-game
                // pick — previously there was no way to undo one at all.
                if (slot1Active) StatusChip(str("memcard.slot1.active"), Success) else OutlinedButton(onClick = onSlot1, modifier = Modifier.controllerFocusable("memcard.${item.file.name}.slot1", onConfirm = onSlot1)) { Text(str("memcard.slot1")) }
                if (perGame && slot1Active && onClearSlot1 != null) {
                    Spacer(Modifier.width(7.dp))
                    TextButton(onClick = onClearSlot1, modifier = Modifier.controllerFocusable("memcard.${item.file.name}.slot1.global", onConfirm = onClearSlot1)) { Text(str("memcard.useGlobal")) }
                }
                Spacer(Modifier.width(7.dp))
                if (slot2Active) StatusChip(str("memcard.slot2.active"), Success) else OutlinedButton(onClick = onSlot2, modifier = Modifier.controllerFocusable("memcard.${item.file.name}.slot2", onConfirm = onSlot2)) { Text(str("memcard.slot2")) }
                if (perGame && slot2Active && onClearSlot2 != null) {
                    Spacer(Modifier.width(7.dp))
                    TextButton(onClick = onClearSlot2, modifier = Modifier.controllerFocusable("memcard.${item.file.name}.slot2.global", onConfirm = onClearSlot2)) { Text(str("memcard.useGlobal")) }
                }
                if (onExport != null) {
                    Spacer(Modifier.width(7.dp))
                    TextButton(onClick = onExport, modifier = Modifier.controllerFocusable("memcard.${item.file.name}.export", onConfirm = onExport)) { Text(str("action.export")) }
                }
                Spacer(Modifier.width(7.dp))
                TextButton(onClick = onDelete, enabled = !item.slot1 && !item.slot2, modifier = Modifier.controllerFocusable("memcard.${item.file.name}.delete", onConfirm = onDelete)) { Text(str("action.delete")) }
            }
        }
    }
}

@Composable
private fun CreateCardDialog(onDismiss: () -> Unit, onCreate: (String, Int, Int) -> Unit) {
    var name by remember { mutableStateOf("MemoryCard") }
    var size by remember { mutableIntStateOf(1) }
    var type by remember { mutableIntStateOf(1) } // 1 = File, 2 = Folder
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(str("memcard.newCard.title")) },
        text = {
            Column {
                OutlinedTextField(name, { name = it }, label = { Text(str("memcard.cardName.label")) }, singleLine = true)
                Spacer(Modifier.height(12.dp))
                Text(str("memcard.type.label"))
                Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    listOf(1 to str("memcard.type.file"), 2 to str("memcard.type.folder")).forEach { (id, label) ->
                        Surface(onClick = { type = id }, modifier = Modifier.controllerFocusable("memcard.create.type$id", RoundedCornerShape(10.dp), onConfirm = { type = id }), shape = RoundedCornerShape(10.dp), color = if (type == id) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant) {
                            Text(label, Modifier.padding(horizontal = 10.dp, vertical = 8.dp))
                        }
                    }
                }
                if (type == 1) {
                    Spacer(Modifier.height(12.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                        listOf(1 to "8 MB", 2 to "16 MB", 3 to "32 MB", 4 to "64 MB").forEach { (id, label) ->
                            Surface(onClick = { size = id }, modifier = Modifier.controllerFocusable("memcard.create.size$id", RoundedCornerShape(10.dp), onConfirm = { size = id }), shape = RoundedCornerShape(10.dp), color = if (size == id) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant) {
                                Text(label, Modifier.padding(horizontal = 10.dp, vertical = 8.dp))
                            }
                        }
                    }
                }
            }
        },
        confirmButton = { Button(onClick = { onCreate(name, type, size) }, modifier = Modifier.controllerFocusable("memcard.create", onConfirm = { onCreate(name, type, size) })) { Text(str("memcard.create")) } },
        dismissButton = { TextButton(onClick = onDismiss, modifier = Modifier.controllerFocusable("memcard.create.cancel", onConfirm = onDismiss)) { Text(str("action.cancel")) } },
    )
}

private fun humanSize(bytes: Long): String = if (bytes >= 1024L * 1024L) "%.1f MB".format(bytes / (1024f * 1024f)) else "${bytes / 1024L} KB"
