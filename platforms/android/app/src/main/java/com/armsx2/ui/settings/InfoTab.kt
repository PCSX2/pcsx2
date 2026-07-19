package com.armsx2.ui.settings

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.ClipboardManager
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.dp
import com.armsx2.CustomCovers
import com.armsx2.CustomNames
import com.armsx2.GameInfo
import com.armsx2.config.ConfigStore
import com.armsx2.i18n.str
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.home.LibraryKeyboard
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONObject

/**
 * Per-game info tab: name, serial, region, container, compatibility, path, and — when
 * this game is the one running — its CRC. Each value has a tap-to-copy button, and the
 * per-game settings (config.game.<serial>) can be exported/imported as JSON.
 */
@Composable
fun InfoTab(game: GameInfo?) {
    if (game == null) {
        EmptyState(
            title = str("info.noGame.title"),
            message = str("info.noGame.body"),
            modifier = Modifier.fillMaxWidth().height(220.dp),
        )
        return
    }
    val context = LocalContext.current
    val clipboard = LocalClipboardManager.current
    val serial = game.serial?.takeIf { it.isNotBlank() }
    // CRC only reads true for the currently-running game; skip it otherwise so a
    // library entry doesn't show another game's CRC.
    val crc = remember(game.uri) {
        runCatching {
            if (NativeApp.getGameSerial()?.takeIf { it.isNotBlank() } == serial) {
                NativeApp.getGameCRC()?.takeIf { it.isNotBlank() && it != "00000000" }
            } else {
                null
            }
        }.getOrNull()
    }

    val exporter = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        if (uri != null && serial != null) {
            runCatching {
                val json = ConfigStore.loadOverrides(serial)?.toString() ?: "{}"
                context.contentResolver.openOutputStream(uri)?.use { it.write(json.toByteArray()) }
            }
        }
    }
    val importer = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null && serial != null) {
            runCatching {
                val text = context.contentResolver.openInputStream(uri)?.bufferedReader()?.use { it.readText() }
                if (!text.isNullOrBlank()) ConfigStore.saveOverrides(serial, JSONObject(text))
            }
        }
    }

    // Custom cover art — for serial-less / ELF / homebrew games the online repo
    // can't match. Writes into the shared covers/custom folder, so it shows on the
    // library tile immediately (CustomCovers.version drives re-resolve).
    val coverVersion = CustomCovers.version.value
    val hasCover = remember(coverVersion, game.uri) { CustomCovers.fileFor(context, game) != null }
    val coverPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) CustomCovers.set(context, game, uri)
    }

    // Custom display name. Text entry hands off to LibraryKeyboard rather than a Compose
    // AlertDialog: a modal Dialog owns its own focused window and swallows gamepad keys, so a
    // pad user could open the rename box and then not be able to type in it. The keyboard has
    // no "committed" callback — its closing IS the done signal, same as the shader-preset save.
    val renaming = remember { mutableStateOf(false) }
    LaunchedEffect(LibraryKeyboard.visible.value) {
        if (renaming.value && !LibraryKeyboard.visible.value) {
            renaming.value = false
            // Blank clears the override and restores the parsed title.
            CustomNames.setName(game.settingsKey, LibraryKeyboard.text.value)
        }
    }

    GlassPanel(Modifier.fillMaxWidth()) {
        Column {
            SectionTitle(game.title, str("scope.game"))
            Spacer(Modifier.height(10.dp))
            InfoRow(str("info.title"), game.title, clipboard)
            InfoRow(str("info.serial"), serial ?: "—", clipboard)
            if (crc != null) InfoRow(str("info.crc"), crc, clipboard)
            InfoRow(str("info.region"), regionName(game.serial), clipboard)
            InfoRow(str("info.container"), game.extension.takeIf { it.isNotBlank() } ?: "—", clipboard)
            InfoRow(str("info.platform"), game.platform.name, clipboard)
            InfoRow(str("info.compatibility"), if (game.compatibility > 0) "${game.compatibility} / 5" else "—", clipboard)
            InfoRow(str("info.path"), game.uri.toString(), clipboard)
            if (serial != null) {
                Spacer(Modifier.height(14.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    val doExport = { exporter.launch("$serial-settings.json") }
                    val doImport = { importer.launch(arrayOf("application/json", "*/*")) }
                    OutlinedButton(
                        onClick = doExport,
                        modifier = Modifier.controllerFocusable("info.exportSettings", onConfirm = doExport),
                    ) { Text(str("info.exportSettings")) }
                    OutlinedButton(
                        onClick = doImport,
                        modifier = Modifier.controllerFocusable("info.importSettings", onConfirm = doImport),
                    ) { Text(str("info.importSettings")) }
                }
            }
            // Modded discs report a garbage internal title ("UN6 A35" for a Naruto mod) that the
            // GameDB can't correct, because the serial still belongs to the base game.
            Spacer(Modifier.height(14.dp))
            Text(str("info.customName.label"), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(6.dp))
            // Read version so a rename repaints these buttons; the name itself lives in prefs.
            CustomNames.version.intValue
            val storedName = CustomNames.stored(game.settingsKey)
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                val openRename = {
                    renaming.value = true
                    LibraryKeyboard.open(
                        storedName ?: game.title,
                        onChange = {},
                        placeholder = com.armsx2.i18n.I18n.get("info.customName.placeholder"),
                    )
                }
                OutlinedButton(
                    onClick = openRename,
                    // Every control on this tab needs a controller id or a pad user can't reach
                    // it at all — the registry only knows what registers itself.
                    modifier = Modifier.controllerFocusable("info.customName", onConfirm = openRename),
                ) { Text(str(if (storedName != null) "info.customName.change" else "info.customName.set")) }
                if (storedName != null) {
                    val clearName = { CustomNames.setName(game.settingsKey, null) }
                    OutlinedButton(
                        onClick = clearName,
                        modifier = Modifier.controllerFocusable("info.customName.clear", onConfirm = clearName),
                    ) { Text(str("info.customName.clear")) }
                }
            }

            Spacer(Modifier.height(14.dp))
            Text(str("info.cover.label"), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(6.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                val pickCover = { coverPicker.launch(arrayOf("image/*")) }
                OutlinedButton(
                    onClick = pickCover,
                    modifier = Modifier.controllerFocusable("info.cover", onConfirm = pickCover),
                ) { Text(str(if (hasCover) "info.changeCover" else "info.setCover")) }
                if (hasCover) {
                    // Explicit Unit: CustomCovers.remove returns Boolean, so an inferred lambda
                    // is () -> Boolean and won't fit onClick/onConfirm.
                    val dropCover: () -> Unit = { CustomCovers.remove(context, game) }
                    OutlinedButton(
                        onClick = dropCover,
                        modifier = Modifier.controllerFocusable("info.removeCover", onConfirm = dropCover),
                    ) { Text(str("info.removeCover")) }
                }
            }
        }
    }
}

@Composable
private fun InfoRow(label: String, value: String, clipboard: ClipboardManager) {
    Row(Modifier.fillMaxWidth().padding(vertical = 7.dp), verticalAlignment = Alignment.CenterVertically) {
        Text(
            label,
            modifier = Modifier.width(118.dp),
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Text(value, Modifier.weight(1f), style = MaterialTheme.typography.bodyMedium)
        if (value != "—") {
            Text(
                "⧉",
                modifier = Modifier
                    .padding(start = 8.dp)
                    .clickable { clipboard.setText(AnnotatedString(value)) },
                color = MaterialTheme.colorScheme.primary,
            )
        }
    }
}

/** Coarse region from the serial prefix — enough for an at-a-glance info tab. */
private fun regionName(serial: String?): String {
    val s = serial?.uppercase()?.replace("-", "") ?: return "—"
    return when {
        s.startsWith("SLUS") || s.startsWith("SCUS") || s.startsWith("PUPX") -> "NTSC-U (USA)"
        s.startsWith("SLES") || s.startsWith("SCES") || s.startsWith("SLED") -> "PAL (Europe)"
        s.startsWith("SLPS") || s.startsWith("SLPM") || s.startsWith("SCPS") || s.startsWith("PSXC") -> "NTSC-J (Japan)"
        s.startsWith("SLKA") || s.startsWith("SCKA") -> "NTSC-K (Korea)"
        else -> "—"
    }
}
