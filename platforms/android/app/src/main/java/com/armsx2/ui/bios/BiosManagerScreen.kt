package com.armsx2.ui.bios

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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.CircularProgressIndicator
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.theme.Success

@Composable
fun BiosManagerScreen(onBack: () -> Unit, viewModel: BiosManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    var deleteTarget by remember { mutableStateOf<InstalledBios?>(null) }
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let(viewModel::import)
    }
    LaunchedEffect(Unit) { viewModel.refresh() }

    ArmsBackdrop {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(10.dp),
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 0.dp),
        ) {
            item {
                ArmsTopBar(
                    title = str("setup.page.bios.title"),
                    leading = { RoundAction("‹", str("action.back"), onBack) },
                    actions = {
                        RoundAction("＋", str("action.import"), { picker.launch(arrayOf("application/octet-stream", "*/*")) })
                        RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                    },
                    horizontalPadding = 0.dp,
                )
            }
            if (state.items.isEmpty() && !state.busy) {
                item {
                    EmptyState(
                        title = str("setup.bios.error.noneFound"),
                        message = str("setup.step.bios.description"),
                        actionLabel = str("action.import"),
                        onAction = { picker.launch(arrayOf("application/octet-stream", "*/*")) },
                        modifier = Modifier.fillMaxWidth().height(280.dp),
                    )
                }
            } else {
                    if (state.busy) item { CircularProgressIndicator(Modifier.size(22.dp), strokeWidth = 2.dp) }
                    items(state.items, key = { it.file.absolutePath }) { item ->
                        BiosRow(
                            item = item,
                            onSelect = { viewModel.select(item.file) },
                            onDelete = { deleteTarget = item },
                        )
                    }
            }
            item { Spacer(Modifier.height(12.dp)) }
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
private fun BiosRow(item: InstalledBios, onSelect: () -> Unit, onDelete: () -> Unit) {
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
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                if (!item.selected) OutlinedButton(onClick = onSelect, shape = RoundedCornerShape(12.dp)) { Text(str("setup.bios.useSelected")) }
                Spacer(Modifier.width(8.dp))
                TextButton(onClick = onDelete, enabled = !item.selected) { Text(str("action.delete")) }
            }
        }
    }
}
