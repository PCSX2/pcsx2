package com.armsx2.ui.patches

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
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
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.SettingSwitchRow
import java.io.File

@Composable
fun PatchManagerScreen(onBack: () -> Unit, viewModel: PatchManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let(viewModel::import) }
    LaunchedEffect(Unit) { viewModel.refresh() }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
            ArmsTopBar(
                title = str("patches.dialog.patchesAndCheats"),
                subtitle = str("patches.applyAtBoot"),
                leading = { RoundAction("‹", str("action.back"), onBack) },
                actions = {
                    RoundAction("＋", str("action.import"), { picker.launch(arrayOf("text/plain", "application/octet-stream", "*/*")) })
                    RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                },
            )
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

@Composable
private fun PatchOptions(state: PatchManagerUiState, viewModel: PatchManagerViewModel, modifier: Modifier) {
    GlassPanel(modifier) {
        Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
            SectionTitle(str("ra.options.header"), str("scope.global"))
            SettingSwitchRow(str("patches.enablePatches.label"), str("patches.applyAtBoot"), state.settings.enablePatches, onCheckedChange = { value -> viewModel.update { it.copy(enablePatches = value) } })
            SettingSwitchRow(str("patches.cheats.label"), str("patches.pasteImportHint"), state.settings.enableCheats, onCheckedChange = { value -> viewModel.update { it.copy(enableCheats = value) } })
            SettingSwitchRow(str("patches.widescreen.label"), str("patches.applyAtBoot"), state.settings.enableWideScreenPatches, onCheckedChange = { value -> viewModel.update { it.copy(enableWideScreenPatches = value) } })
            SettingSwitchRow(str("patches.noInterlacing.label"), str("patches.applyAtBoot"), state.settings.enableNoInterlacingPatches, onCheckedChange = { value -> viewModel.update { it.copy(enableNoInterlacingPatches = value) } })
        }
    }
}

@Composable
private fun PatchFiles(state: PatchManagerUiState, viewModel: PatchManagerViewModel, modifier: Modifier) {
    Column(modifier) {
        SectionTitle(str("patches.installedHeader"), state.files.size.toString())
        if (state.files.isEmpty()) {
            EmptyState(str("patches.noFilesInstalled"), str("patches.pasteImportHint"), modifier = Modifier.fillMaxWidth().height(220.dp))
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                state.files.forEach { file -> PatchFileRow(file) { viewModel.delete(file) } }
            }
        }
    }
}

@Composable
private fun PatchFileRow(file: File, onDelete: () -> Unit) {
    Surface(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
    ) {
        Row(Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("✦", color = MaterialTheme.colorScheme.primary)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(file.name, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(file.parentFile?.name.orEmpty(), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            TextButton(onClick = onDelete) { Text(str("action.delete")) }
        }
    }
}
