package com.armsx2.ui.textures

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
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.SettingSwitchRow

@Composable
fun TextureManagerScreen(onBack: () -> Unit, viewModel: TextureManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    val folderPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri -> uri?.let(viewModel::importFolder) }
    LaunchedEffect(Unit) { viewModel.refresh() }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize()) {
            ArmsTopBar(
                title = "Texture packs",
                subtitle = state.activeSerial?.let { "Active game: $it" } ?: "Launch a game before importing",
                leading = { RoundAction("‹", "Back", onBack) },
                actions = {
                    RoundAction("＋", "Import folder", { folderPicker.launch(null) })
                    RoundAction("↻", "Refresh", viewModel::refresh)
                },
            )
            Row(
                Modifier.fillMaxSize().padding(start = 22.dp, end = 22.dp, bottom = 18.dp),
                horizontalArrangement = Arrangement.spacedBy(18.dp),
            ) {
                GlassPanel(Modifier.width(330.dp).fillMaxHeight()) {
                    Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
                        SectionTitle("Texture replacement", "Global loading options")
                        SettingSwitchRow("Load replacements", "Use installed replacement textures", state.settings.loadTextureReplacements, onCheckedChange = { value -> viewModel.update { it.copy(loadTextureReplacements = value) } })
                        SettingSwitchRow("Asynchronous loading", "Load textures in the background", state.settings.loadTextureReplacementsAsync, onCheckedChange = { value -> viewModel.update { it.copy(loadTextureReplacementsAsync = value) } })
                        SettingSwitchRow("Precache packs", "Prepare textures when the game starts", state.settings.precacheTextureReplacements, onCheckedChange = { value -> viewModel.update { it.copy(precacheTextureReplacements = value) } })
                    }
                }
                Column(Modifier.weight(1f)) {
                    SectionTitle("Installed packs", "${state.packs.size} games")
                    if (state.busy) CircularProgressIndicator()
                    if (state.packs.isEmpty()) {
                        EmptyState("No texture packs", "Import a folder while its target game is active.", modifier = Modifier.fillMaxSize())
                    } else {
                        LazyColumn(
                            Modifier.fillMaxSize(),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                            contentPadding = PaddingValues(bottom = 20.dp),
                        ) {
                            items(state.packs, key = { it.serial }) { pack -> TexturePackRow(pack) { viewModel.delete(pack) } }
                        }
                    }
                }
            }
        }
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error == null) "Done" else "Texture packs") },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage) { Text("OK") } },
        )
    }
}

@Composable
private fun TexturePackRow(pack: TexturePackItem, onDelete: () -> Unit) {
    Surface(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(17.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.48f)),
    ) {
        Row(Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
            Text("▧", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(pack.serial, style = MaterialTheme.typography.titleMedium)
                Text("${pack.fileCount} files · ${humanSize(pack.size)}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            TextButton(onClick = onDelete) { Text("Delete") }
        }
    }
}

private fun humanSize(bytes: Long): String = if (bytes >= 1024L * 1024L) "%.1f MB".format(bytes / (1024f * 1024f)) else "${bytes / 1024L} KB"
