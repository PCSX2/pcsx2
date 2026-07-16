package com.armsx2.ui.settings

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.ControllerSkinStore
import com.armsx2.config.Settings
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import com.armsx2.ui.Colors
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Custom on-screen controller skins (v1): import a folder or .zip of
 * `ic_controller_*.png` images (iOS skin packs work), then pick the active skin.
 * Visuals only — applies to the on-screen touch controls; falls back to the
 * built-in look for any image a skin doesn't provide.
 */
@Composable
fun SkinsTab(@Suppress("UNUSED_PARAMETER") state: MutableState<Settings>) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    val scroll = settingsScrollState()
    ControllerAutoScroll(scroll)
    val refresh = remember { mutableIntStateOf(0) }
    val busy = remember { mutableStateOf(false) }
    val status = remember { mutableStateOf<String?>(null) }
    val skins = remember(refresh.intValue) { ControllerSkinStore.list(ctx) }

    // Skin scope. Like the Pad tab, the tier comes from HOW Settings was opened: for a
    // game (Game scope) or globally. Inside Game scope the user still chooses whether
    // this game pins its own skin or just follows the global one — that's [perGame],
    // which is the presence of the per-game override, not a separate preference.
    val skinSerial = com.armsx2.ui.InGameOverlay.currentSerial.value?.takeIf { it.isNotEmpty() }
    val gameScope = com.armsx2.ui.InGameOverlay.settingsScope.value ==
        com.armsx2.config.SettingsScope.Game && skinSerial != null
    val perGame = remember(refresh.intValue, skinSerial) {
        gameScope && ControllerSkinStore.hasGameOverride(skinSerial)
    }
    // The tier the rows below read AND write. null = global.
    val editSerial: String? = if (perGame) skinSerial else null
    // Read the tier straight from prefs rather than the RESOLVED activeSkinId: the hub
    // can be open for a game that isn't running, where the resolved value is still the
    // global one and would tick the wrong row.
    val activeId = remember(refresh.intValue, editSerial, ControllerSkinStore.activeSkinId.value) {
        ControllerSkinStore.activeForScope(ctx, editSerial)
    }

    fun onImported(id: String?, sourceLabel: String) {
        busy.value = false
        if (id != null) {
            ControllerSkinStore.setActive(ctx, id, editSerial)
            refresh.intValue++
            status.value = I18n.get("skins.status.importedAndSelected")
        } else {
            status.value = "No ic_controller_*.png images found in that $sourceLabel."
        }
    }

    val folderLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        busy.value = true; status.value = null
        scope.launch {
            val id = withContext(Dispatchers.IO) { ControllerSkinStore.importFromTree(ctx, uri) }
            onImported(id, "folder")
        }
    }
    val zipLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        busy.value = true; status.value = null
        scope.launch {
            val id = withContext(Dispatchers.IO) { ControllerSkinStore.importFromZip(ctx, uri) }
            onImported(id, ".zip")
        }
    }

    Column(
        Modifier
            .fillMaxWidth()
            .padding(horizontal = 12.dp, vertical = 8.dp),
    ) {
        Text(str("skins.title"), color = MaterialTheme.colorScheme.onSurface, fontSize = 15.sp, fontWeight = FontWeight.Bold)
        Text(
            str("skins.description"),
            color = MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 14.sp,
            modifier = Modifier.padding(top = 2.dp, bottom = 8.dp),
        )

        ActionRow(str("skins.importFolder"), "skins-import-folder") { folderLauncher.launch(null) }
        SettingsDivider()
        ActionRow(str("skins.importZip"), "skins-import-zip") {
            zipLauncher.launch(
                arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")
            )
        }
        SettingsDivider()

        if (busy.value) {
            Text(str("skins.importing"), color = Color(0xFFAACCFF), fontSize = 15.sp,
                modifier = Modifier.padding(vertical = 4.dp))
        }
        status.value?.let {
            Text(it, color = Color(0xFFAACCFF), fontSize = 14.sp,
                modifier = Modifier.padding(vertical = 2.dp))
        }

        Spacer(Modifier.height(10.dp))
        // Only offered with a game in hand — a per-game switch in the global screen
        // would have no game to be "per".
        if (gameScope && skinSerial != null) {
            ToggleRow(
                str("skins.perGame.label"),
                perGame,
                description = str("skins.perGame.description"),
            ) { on ->
                if (on) {
                    // Seed the override with what the game already shows, so turning this
                    // on changes NOTHING until a row below is picked. Flipping straight to
                    // the built-in look would be a surprise edit, not a scope change.
                    ControllerSkinStore.setActive(
                        ctx,
                        ControllerSkinStore.activeForScope(ctx, null),
                        skinSerial,
                    )
                } else {
                    ControllerSkinStore.clearGameOverride(ctx, skinSerial)
                }
                refresh.intValue++
            }
            SettingsDivider()
        }
        Text(
            if (perGame) str("skins.activeSkin.game") else str("skins.activeSkin"),
            color = MaterialTheme.colorScheme.onSurface, fontSize = 16.sp, fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(bottom = 4.dp),
        )

        SkinRow(
            name = str("skins.builtinDefault"),
            selected = activeId == null,
            controllerId = "skin-builtin",
            onSelect = { ControllerSkinStore.setActive(ctx, null, editSerial); refresh.intValue++ },
            onDelete = null,
        )
        for (b in ControllerSkinStore.BUILTIN) {
            SettingsDivider()
            SkinRow(
                name = b.name,
                selected = activeId == b.id,
                controllerId = "skin-${b.id}",
                onSelect = { ControllerSkinStore.setActive(ctx, b.id, editSerial); refresh.intValue++ },
                onDelete = null,
            )
        }
        for (s in skins) {
            SettingsDivider()
            SkinRow(
                name = "${s.name}  ·  ${s.imageCount} images",
                selected = activeId == s.id,
                controllerId = "skin-${s.id}",
                onSelect = { ControllerSkinStore.setActive(ctx, s.id, editSerial); refresh.intValue++ },
                onDelete = {
                    ControllerSkinStore.delete(ctx, s.id)
                    refresh.intValue++
                },
            )
        }
    }
}

/** A full-width tappable action row, matching the Pad-tab reset-button style. */
@Composable
private fun ActionRow(label: String, controllerId: String, onClick: () -> Unit) {
    Box(
        Modifier
            .fillMaxWidth()
            .height(56.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(rowAura())
            .clickable { onClick() }
            .controllerFocusable(controllerId = controllerId, onConfirm = { onClick() })
            .padding(horizontal = 6.dp),
        contentAlignment = Alignment.CenterStart,
    ) {
        Text(label, color = Colors.pasx2_blue, fontSize = 16.sp, fontWeight = FontWeight.Bold)
    }
}

/** One skin entry: name on the left, a Use/Active pill and an optional Delete. */
@Composable
private fun SkinRow(
    name: String,
    selected: Boolean,
    controllerId: String,
    onSelect: () -> Unit,
    onDelete: (() -> Unit)?,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(64.dp)
            .clickable { onSelect() }
            .controllerFocusable(controllerId = controllerId, onConfirm = { onSelect() })
            .padding(horizontal = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            (if (selected) "● " else "○ ") + name,
            color = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
            fontSize = 16.sp,
            fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
            modifier = Modifier.weight(1f),
        )
        if (onDelete != null) {
            Spacer(Modifier.width(8.dp))
            Box(
                Modifier
                    .height(48.dp)
                    .clip(RoundedCornerShape(14.dp))
                    .background(Color(0x33FF5555))
                    .clickable { onDelete() }
                    .controllerFocusable(controllerId = "$controllerId-del", onConfirm = { onDelete() })
                    .padding(horizontal = 10.dp),
                contentAlignment = Alignment.Center,
            ) {
                Text(str("action.delete"), color = Color(0xFFFF8888), fontSize = 14.sp, fontWeight = FontWeight.Bold)
            }
        }
    }
}
