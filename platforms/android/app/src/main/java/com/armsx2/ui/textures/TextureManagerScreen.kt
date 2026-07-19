package com.armsx2.ui.textures

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
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp
import com.armsx2.ui.Colors
import com.armsx2.runtime.MainActivityRuntime
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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
import com.armsx2.ui.settings.controllerFocusable

@Composable
fun TextureManagerScreen(onBack: () -> Unit, viewModel: TextureManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    val folderPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri -> uri?.let(viewModel::importFolder) }
    LaunchedEffect(Unit) { viewModel.refresh() }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
            ArmsTopBar(
                title = str("renderer.section.texturePacks"),
                subtitle = state.activeSerial ?: str("games.info.perGameSettings.body"),
                leading = { RoundAction("←", str("action.back"), onBack) },
                actions = {
                    RoundAction("＋", str("action.import"), { folderPicker.launch(null) })
                    RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                },
            )
            BoxWithConstraints(Modifier.fillMaxWidth()) {
                val compact = maxWidth < 820.dp
                if (compact) {
                    Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp)) {
                        TextureOptions(state, viewModel, Modifier.fillMaxWidth())
                        Spacer(Modifier.padding(top = 10.dp))
                        TexturePacks(state, viewModel, Modifier.fillMaxWidth(), onBack)
                    }
                } else {
                    Row(
                        Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp),
                        horizontalArrangement = Arrangement.spacedBy(14.dp),
                    ) {
                        TextureOptions(state, viewModel, Modifier.width(310.dp))
                        TexturePacks(state, viewModel, Modifier.weight(1f), onBack)
                    }
                }
            }
        }
    }
    (state.error ?: state.message)?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissMessage,
            title = { Text(if (state.error == null) str("action.ok") else str("renderer.section.texturePacks")) },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissMessage) { Text(str("action.ok")) } },
        )
    }
}

@Composable
private fun TextureOptions(state: TextureManagerUiState, viewModel: TextureManagerViewModel, modifier: Modifier) {
    GlassPanel(modifier) {
        Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
            SectionTitle(str("renderer.section.texturePacks"), str("scope.global"))
            SettingSwitchRow(
                str("renderer.loadTexturePacks.label"),
                str("renderer.loadTexturePacks.description"),
                state.settings.loadTextureReplacements,
                onCheckedChange = { value -> viewModel.update { it.copy(loadTextureReplacements = value) } },
                modifier = Modifier.controllerFocusable(
                    "textureMgr.opt.loadReplacements",
                    RoundedCornerShape(22.dp),
                    onConfirm = { viewModel.update { it.copy(loadTextureReplacements = !state.settings.loadTextureReplacements) } },
                    onLeft = { viewModel.update { it.copy(loadTextureReplacements = false) } },
                    onRight = { viewModel.update { it.copy(loadTextureReplacements = true) } },
                ),
            )
            SettingSwitchRow(
                str("renderer.asyncTextureLoading.label"),
                str("renderer.asyncTextureLoading.description"),
                state.settings.loadTextureReplacementsAsync,
                onCheckedChange = { value -> viewModel.update { it.copy(loadTextureReplacementsAsync = value) } },
                modifier = Modifier.controllerFocusable(
                    "textureMgr.opt.asyncLoading",
                    RoundedCornerShape(22.dp),
                    onConfirm = { viewModel.update { it.copy(loadTextureReplacementsAsync = !state.settings.loadTextureReplacementsAsync) } },
                    onLeft = { viewModel.update { it.copy(loadTextureReplacementsAsync = false) } },
                    onRight = { viewModel.update { it.copy(loadTextureReplacementsAsync = true) } },
                ),
            )
            SettingSwitchRow(
                str("renderer.precacheTexturePacks.label"),
                str("renderer.precacheTexturePacks.description"),
                state.settings.precacheTextureReplacements,
                onCheckedChange = { value -> viewModel.update { it.copy(precacheTextureReplacements = value) } },
                modifier = Modifier.controllerFocusable(
                    "textureMgr.opt.precache",
                    RoundedCornerShape(22.dp),
                    onConfirm = { viewModel.update { it.copy(precacheTextureReplacements = !state.settings.precacheTextureReplacements) } },
                    onLeft = { viewModel.update { it.copy(precacheTextureReplacements = false) } },
                    onRight = { viewModel.update { it.copy(precacheTextureReplacements = true) } },
                ),
            )
        }
    }
}

@Composable
private fun TexturePacks(state: TextureManagerUiState, viewModel: TextureManagerViewModel, modifier: Modifier, onBack: () -> Unit) {
    Column(modifier) {
        SectionTitle(str("renderer.section.texturePacks"), state.packs.size.toString())
        // A pack is thousands of files over SAF, so show the running count next to the
        // spinner — a bare spinner on a multi-minute copy is indistinguishable from a hang.
        if (state.busy) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                CircularProgressIndicator()
                if (state.progress > 0) Text(str("renderer.texturePacks.copying").format(state.progress))
            }
        }
        // Name the folder the CORE will actually scan. A pack only applies if its folder
        // matches this exactly, and for a raw .ELF boot that is the ELF's filename rather
        // than the disc serial — a mismatch that is otherwise completely invisible and
        // looks identical to "texture packs are broken".
        state.activeSerial?.takeIf(String::isNotBlank)?.let { serial ->
            val matched = state.packs.any { it.serial.equals(serial, ignoreCase = true) }
            Text(
                str("renderer.texturePacks.activeSerial").format(serial),
                color = if (matched) MaterialTheme.colorScheme.onSurfaceVariant else Colors.pasx2_blue,
                fontSize = 13.sp,
                modifier = Modifier.padding(top = 4.dp, bottom = 2.dp),
            )
            if (!matched && state.packs.isNotEmpty()) {
                Text(
                    str("renderer.texturePacks.serialMismatch").format(serial),
                    color = Colors.pasx2_blue,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.padding(bottom = 4.dp),
                )
            }
        }
        if (state.packs.isEmpty()) {
            EmptyState(
                str("renderer.section.texturePacks"),
                str("renderer.loadTexturePacks.description"),
                modifier = Modifier.fillMaxWidth().padding(top = 10.dp).height(220.dp),
            )
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                state.packs.forEach { pack -> TexturePackRow(pack) { viewModel.delete(pack) } }
            }
        }
        // The replacement map is only built in GSTextureReplacements::Initialize / GameChanged
        // — i.e. at boot. Importing a pack or toggling Load Texture Packs mid-session changes
        // nothing on screen until the game restarts, which reads as "texture packs are
        // broken". Spelled-out row rather than another circular-arrow glyph next to Refresh:
        // the earlier icon was both indistinguishable from it and rendered as tofu.
        if (MainActivityRuntime.eState.value != com.armsx2.EmuState.STOPPED) {
            Spacer(Modifier.height(10.dp))
            Text(
                str("renderer.texturePacks.restartHint"),
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 13.sp,
                modifier = Modifier.padding(bottom = 6.dp),
            )
            Button(
                // Dismiss FIRST: restart() alone reboots the VM behind this screen, so the
                // user sees nothing change and concludes the button is dead.
                onClick = { onBack(); MainActivityRuntime.restart() },
                modifier = Modifier.fillMaxWidth(),
            ) { Text(str("renderer.texturePacks.restartNow")) }
        }
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
                Text("${pack.fileCount} · ${humanSize(pack.size)}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            TextButton(
                onClick = onDelete,
                modifier = Modifier.controllerFocusable("textureMgr.delete.${pack.serial}", onConfirm = onDelete),
            ) { Text(str("action.delete")) }
        }
    }
}

private fun humanSize(bytes: Long): String = if (bytes >= 1024L * 1024L) "%.1f MB".format(bytes / (1024f * 1024f)) else "${bytes / 1024L} KB"
