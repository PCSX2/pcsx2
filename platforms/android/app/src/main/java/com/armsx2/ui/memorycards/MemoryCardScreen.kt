package com.armsx2.ui.memorycards

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
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
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.theme.Success

@Composable
fun MemoryCardScreen(onBack: () -> Unit, viewModel: MemoryCardViewModel = viewModel()) {
    val state = viewModel.state.value
    var createDialog by remember { mutableStateOf(false) }
    var deleteTarget by remember { mutableStateOf<MemoryCardItem?>(null) }
    val importer = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let(viewModel::import) }
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
                    leading = { RoundAction("‹", str("action.back"), onBack) },
                    actions = {
                        RoundAction("＋", str("memcard.newCard"), { createDialog = true })
                        RoundAction("⇩", str("action.import"), { importer.launch(arrayOf("application/octet-stream", "*/*")) })
                        RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                    },
                    horizontalPadding = 0.dp,
                )
            }
            if (state.cards.isEmpty()) {
                item { EmptyState(str("memcard.empty"), str("memcard.size.hint"), str("memcard.create"), { createDialog = true }, Modifier.fillMaxWidth().height(280.dp)) }
            } else {
                items(state.cards, key = { it.file.absolutePath }) { item ->
                    MemoryCardRow(item, { viewModel.assign(1, item) }, { viewModel.assign(2, item) }, { deleteTarget = item })
                }
            }
            item { Spacer(Modifier.height(12.dp)) }
        }
    }

    if (createDialog) {
        CreateCardDialog(
            onDismiss = { createDialog = false },
            onCreate = { name, size -> viewModel.create(name, size); createDialog = false },
        )
    }
    deleteTarget?.let { item ->
        AlertDialog(
            onDismissRequest = { deleteTarget = null },
            title = { Text(str("memcard.delete.confirm")) },
            text = { Text(item.file.name) },
            confirmButton = { TextButton(onClick = { viewModel.delete(item); deleteTarget = null }) { Text(str("action.delete"), color = MaterialTheme.colorScheme.error) } },
            dismissButton = { TextButton(onClick = { deleteTarget = null }) { Text(str("action.cancel")) } },
        )
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error != null) str("memcard.title") else str("action.ok")) },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage) { Text(str("action.ok")) } },
        )
    }
}

@Composable
private fun MemoryCardRow(item: MemoryCardItem, onSlot1: () -> Unit, onSlot2: () -> Unit, onDelete: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(19.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
    ) {
        Column(Modifier.padding(13.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text("▤", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.headlineMedium)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(item.file.name, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                    Text(humanSize(item.size), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End, verticalAlignment = Alignment.CenterVertically) {
                if (item.slot1) StatusChip(str("memcard.slot1.active"), Success) else OutlinedButton(onClick = onSlot1) { Text(str("memcard.slot1")) }
                Spacer(Modifier.width(7.dp))
                if (item.slot2) StatusChip(str("memcard.slot2.active"), Success) else OutlinedButton(onClick = onSlot2) { Text(str("memcard.slot2")) }
                Spacer(Modifier.width(7.dp))
                TextButton(onClick = onDelete, enabled = !item.slot1 && !item.slot2) { Text(str("action.delete")) }
            }
        }
    }
}

@Composable
private fun CreateCardDialog(onDismiss: () -> Unit, onCreate: (String, Int) -> Unit) {
    var name by remember { mutableStateOf("MemoryCard") }
    var size by remember { mutableIntStateOf(1) }
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(str("memcard.newCard.title")) },
        text = {
            Column {
                OutlinedTextField(name, { name = it }, label = { Text(str("memcard.cardName.label")) }, singleLine = true)
                Spacer(Modifier.height(12.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    listOf(1 to "8 MB", 2 to "16 MB", 3 to "32 MB", 4 to "64 MB").forEach { (id, label) ->
                        Surface(onClick = { size = id }, shape = RoundedCornerShape(10.dp), color = if (size == id) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant) {
                            Text(label, Modifier.padding(horizontal = 10.dp, vertical = 8.dp))
                        }
                    }
                }
            }
        },
        confirmButton = { Button(onClick = { onCreate(name, size) }) { Text(str("memcard.create")) } },
        dismissButton = { TextButton(onClick = onDismiss) { Text(str("action.cancel")) } },
    )
}

private fun humanSize(bytes: Long): String = if (bytes >= 1024L * 1024L) "%.1f MB".format(bytes / (1024f * 1024f)) else "${bytes / 1024L} KB"
