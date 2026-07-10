package com.armsx2.ui.patches

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
        Column(Modifier.fillMaxSize()) {
            ArmsTopBar(
                title = "Patches & cheats",
                subtitle = "Compatibility patches and PNACH files",
                leading = { RoundAction("‹", "Back", onBack) },
                actions = {
                    RoundAction("＋", "Import", { picker.launch(arrayOf("text/plain", "application/octet-stream", "*/*")) })
                    RoundAction("↻", "Refresh", viewModel::refresh)
                },
            )
            Row(
                Modifier.fillMaxSize().padding(start = 22.dp, end = 22.dp, bottom = 18.dp),
                horizontalArrangement = Arrangement.spacedBy(18.dp),
            ) {
                GlassPanel(Modifier.width(330.dp).fillMaxHeight()) {
                    Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
                        SectionTitle("Runtime options", "Applied globally")
                        SettingSwitchRow("Game patches", "Use built-in compatibility fixes", state.settings.enablePatches, onCheckedChange = { value -> viewModel.update { it.copy(enablePatches = value) } })
                        SettingSwitchRow("Cheats", "Enable PNACH cheat codes", state.settings.enableCheats, onCheckedChange = { value -> viewModel.update { it.copy(enableCheats = value) } })
                        SettingSwitchRow("Widescreen patches", "Enable 16:9 patches when available", state.settings.enableWideScreenPatches, onCheckedChange = { value -> viewModel.update { it.copy(enableWideScreenPatches = value) } })
                        SettingSwitchRow("No-interlacing patches", "Use progressive display patches", state.settings.enableNoInterlacingPatches, onCheckedChange = { value -> viewModel.update { it.copy(enableNoInterlacingPatches = value) } })
                    }
                }
                Column(Modifier.weight(1f)) {
                    SectionTitle("Local files", "${state.files.size} PNACH files")
                    if (state.files.isEmpty()) {
                        EmptyState("No custom patch files", "Import a .pnach file to add custom codes.", modifier = Modifier.fillMaxSize())
                    } else {
                        LazyColumn(
                            Modifier.fillMaxSize(),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                            contentPadding = PaddingValues(bottom = 20.dp),
                        ) {
                            items(state.files, key = { it.absolutePath }) { file -> PatchFileRow(file) { viewModel.delete(file) } }
                        }
                    }
                }
            }
        }
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error == null) "Done" else "Patches & cheats") },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage) { Text("OK") } },
        )
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
            TextButton(onClick = onDelete) { Text("Delete") }
        }
    }
}
