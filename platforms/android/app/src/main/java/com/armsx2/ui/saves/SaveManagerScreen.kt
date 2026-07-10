package com.armsx2.ui.saves

import androidx.compose.foundation.BorderStroke
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
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.StatusChip
import java.io.File

@Composable
fun SaveManagerScreen(onBack: () -> Unit, viewModel: SaveManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    LaunchedEffect(Unit) { viewModel.refresh() }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize()) {
            ArmsTopBar(
                title = "Save states",
                subtitle = state.gameTitle ?: "Saved state archive",
                leading = { RoundAction("‹", "Back", onBack) },
                actions = {
                    RoundAction("▣", "Backup", viewModel::backupAll)
                    RoundAction("↻", "Refresh", viewModel::refresh)
                },
            )
            LazyColumn(
                Modifier.fillMaxSize().padding(horizontal = 22.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
                contentPadding = PaddingValues(bottom = 22.dp),
            ) {
                item { SectionTitle("Current game", if (state.gameTitle == null) "Launch a game to use numbered slots" else "Ten save slots") }
                items(state.slots, key = { "slot-${it.slot}" }) { slot ->
                    SaveSlotRow(slot, state.gameTitle != null, { viewModel.save(slot.slot) }, { viewModel.load(slot.slot) }, { slot.file?.let(viewModel::delete) })
                }
                item { Spacer(Modifier.height(8.dp)); SectionTitle("All saved states", "${state.archived.size} files on this device") }
                if (state.archived.isEmpty()) {
                    item { EmptyState("No saved states", "Save states created during gameplay will appear here.", modifier = Modifier.fillMaxWidth().height(170.dp)) }
                } else {
                    items(state.archived.take(100), key = { it.absolutePath }) { file ->
                        ArchiveRow(file) { viewModel.delete(file) }
                    }
                }
            }
        }
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error == null) "Done" else "Save states") },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage) { Text("OK") } },
        )
    }
}

@Composable
private fun SaveSlotRow(slot: SaveSlotItem, gameActive: Boolean, onSave: () -> Unit, onLoad: () -> Unit, onDelete: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(17.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.48f)),
    ) {
        Row(Modifier.padding(13.dp), verticalAlignment = Alignment.CenterVertically) {
            StatusChip("${slot.slot + 1}")
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text("Slot ${slot.slot + 1}", style = MaterialTheme.typography.titleMedium)
                Text(slot.file?.let { formatTimestamp(it.lastModified()) } ?: "Empty", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            OutlinedButton(onClick = onSave, enabled = gameActive) { Text(if (slot.file == null) "Save" else "Overwrite") }
            Spacer(Modifier.width(7.dp))
            OutlinedButton(onClick = onLoad, enabled = gameActive && slot.file != null) { Text("Load") }
            if (slot.file != null) TextButton(onClick = onDelete) { Text("Delete") }
        }
    }
}

@Composable
private fun ArchiveRow(file: File, onDelete: () -> Unit) {
    Surface(Modifier.fillMaxWidth(), shape = RoundedCornerShape(15.dp), color = MaterialTheme.colorScheme.surfaceVariant) {
        Row(Modifier.padding(13.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("↧", color = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(file.name, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(formatTimestamp(file.lastModified()), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            TextButton(onClick = onDelete) { Text("Delete") }
        }
    }
}

private fun formatTimestamp(value: Long): String = java.text.DateFormat.getDateTimeInstance().format(java.util.Date(value))
