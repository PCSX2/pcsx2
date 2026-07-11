package com.armsx2.ui.saves

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.settings.controllerFocusable

@Composable
fun SaveManagerScreen(onBack: () -> Unit, viewModel: SaveManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    LaunchedEffect(Unit) { viewModel.refresh() }

    ArmsBackdrop {
        LazyColumn(
            modifier = Modifier.fillMaxSize(),
            verticalArrangement = Arrangement.spacedBy(10.dp),
            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp),
        ) {
            item {
                ArmsTopBar(
                    title = str("savestate.title.loadManage"),
                    leading = { RoundAction("‹", str("action.back"), onBack) },
                    actions = {
                        if (state.saves.isNotEmpty()) {
                            RoundAction("▣", str("savestate.backup"), viewModel::backupAll)
                        }
                        RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                    },
                    horizontalPadding = 0.dp,
                )
            }

            state.gameTitle?.let { title ->
                item { SectionTitle(title, state.saves.size.toString()) }
            }

            if (state.loading && state.saves.isEmpty()) {
                item {
                    Box(Modifier.fillMaxWidth().height(180.dp), contentAlignment = Alignment.Center) {
                        CircularProgressIndicator()
                    }
                }
            } else if (state.saves.isEmpty()) {
                item {
                    EmptyState(
                        title = str("savestate.noSavesToBackUp"),
                        message = str("savestate.title.save"),
                        modifier = Modifier.fillMaxWidth().height(190.dp),
                    )
                }
            } else {
                items(state.saves, key = { it.file.absolutePath }) { save ->
                    SaveStateCard(
                        save = save,
                        onSave = { save.slot?.let(viewModel::save) },
                        onLoad = { viewModel.load(save) },
                        onDelete = { viewModel.delete(save) },
                    )
                }
            }
            item { Spacer(Modifier.height(12.dp)) }
        }
    }

    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(str("savestate.title.loadManage")) },
            text = { Text(message) },
            confirmButton = {
                TextButton(onClick = viewModel::dismissMessage) { Text(str("action.ok")) }
            },
        )
    }
}

@Composable
private fun SaveStateCard(
    save: SaveStateItem,
    onSave: () -> Unit,
    onLoad: () -> Unit,
    onDelete: () -> Unit,
) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.44f)),
    ) {
        Column(Modifier.padding(10.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                SavePreview(save)
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(
                        text = save.gameTitle,
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.Bold,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                    )
                    Spacer(Modifier.height(3.dp))
                    val slotLabel = save.slot?.let {
                        "${str("memcard.slot1").substringBefore(' ')} ${it + 1}"
                    } ?: save.serial
                    Text(
                        text = slotLabel,
                        style = MaterialTheme.typography.labelLarge,
                        color = MaterialTheme.colorScheme.primary,
                    )
                    Text(
                        text = formatTimestamp(save.file.lastModified()),
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
            Spacer(Modifier.height(9.dp))
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                if (save.canUseWithActiveGame && save.slot != null) {
                    OutlinedButton(
                        onClick = onSave,
                        contentPadding = PaddingValues(horizontal = 14.dp),
                        modifier = Modifier.controllerFocusable("saveMgr.save.${save.file.absolutePath}", onConfirm = onSave),
                    ) {
                        Text(str("action.save"))
                    }
                    Spacer(Modifier.width(7.dp))
                    OutlinedButton(
                        onClick = onLoad,
                        contentPadding = PaddingValues(horizontal = 14.dp),
                        modifier = Modifier.controllerFocusable("saveMgr.load.${save.file.absolutePath}", onConfirm = onLoad),
                    ) {
                        Text(str("touch.stateAction.load"))
                    }
                    Spacer(Modifier.width(3.dp))
                }
                TextButton(
                    onClick = onDelete,
                    modifier = Modifier.controllerFocusable("saveMgr.delete.${save.file.absolutePath}", onConfirm = onDelete),
                ) { Text(str("action.delete")) }
            }
        }
    }
}

@Composable
private fun SavePreview(save: SaveStateItem) {
    val shape = RoundedCornerShape(14.dp)
    Box(
        modifier = Modifier
            .width(132.dp)
            .aspectRatio(16f / 9f)
            .clip(shape)
            .background(
                Brush.linearGradient(
                    listOf(
                        MaterialTheme.colorScheme.primary.copy(alpha = 0.24f),
                        MaterialTheme.colorScheme.surfaceVariant,
                    ),
                ),
            ),
        contentAlignment = Alignment.Center,
    ) {
        val preview = save.preview
        if (preview != null) {
            Image(
                bitmap = preview.asImageBitmap(),
                contentDescription = save.gameTitle,
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop,
            )
        } else {
            ArmsLogo(showWordmark = false)
        }
    }
}

private fun formatTimestamp(value: Long): String =
    java.text.DateFormat.getDateTimeInstance(java.text.DateFormat.MEDIUM, java.text.DateFormat.SHORT)
        .format(java.util.Date(value))
