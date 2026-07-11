package com.armsx2.ui.saves

import android.graphics.BitmapFactory
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
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
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyHorizontalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.i18n.str
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.initialPadFocus
import com.armsx2.ui.common.padFocusRing
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp

/** Which flow the picker drives — Save writes a slot, Load restores one. */
enum class SaveMode { Save, Load }

private const val SLOTS = 10
private const val TILE_WIDTH_DP = 200

/**
 * In-game save-state slot picker, the rich replacement for the pause menu's
 * quick Save / Load buttons (matches the old Refresh UI). Both modes show the
 * 10 numbered slots as thumbnail tiles previewed via `getImageSlot(slot)`; the
 * game path from `getGamePathSlot(slot)` marks a slot as occupied.
 *
 *  - Save: any tile is a valid target (tap = write/overwrite), then close +
 *    resume via onBack.
 *  - Load: only occupied slots are enabled; the leading Autosave tile (shown
 *    when `hasAutosaveState`) restores the on-exit state. Load runs to
 *    completion on the IO pool *before* onBack resumes the VM, so the game
 *    never resumes at the pre-load frame (the quick-load race).
 *
 * Load mode also surfaces the two persistence toggles the old UI had:
 * auto-save-on-exit and auto-load-last-state-on-boot (backed by the
 * `autoSaveOnExit` / `autoLoadOnBoot` prefs honoured in MainActivityRuntime).
 */
@Composable
fun SaveStatePickerScreen(mode: SaveMode, onBack: () -> Unit) {
    // Save/load run on Dispatchers.IO, not the VM thread — the single-threaded
    // eDispatcher is parked inside the VM main loop, so a Main-queued task would
    // never fire. onBack hops back to Main (it mutates overlay state + resumes).
    val scope = rememberCoroutineScope()
    // Probe the autosave slot once (Load only) — hasAutosaveState touches disk.
    val hasAutosave by produceState(initialValue = false, mode) {
        value = if (mode == SaveMode.Load) withContext(Dispatchers.IO) {
            runCatching { NativeApp.hasAutosaveState() }.getOrDefault(false)
        } else false
    }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize()) {
            ArmsTopBar(
                title = if (mode == SaveMode.Save) str("savestate.title.save")
                else str("savestate.title.loadManage"),
                leading = { RoundAction("‹", str("action.back"), onBack) },
            )
            if (mode == SaveMode.Load) {
                AutoOptions(Modifier.fillMaxWidth().padding(horizontal = 8.dp))
                Spacer(Modifier.height(10.dp))
            }
            LazyHorizontalGrid(
                rows = GridCells.Fixed(2),
                modifier = Modifier.weight(1f).fillMaxWidth().padding(horizontal = 8.dp),
                contentPadding = PaddingValues(vertical = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                if (mode == SaveMode.Load && hasAutosave) {
                    item(key = "autosave") {
                        AutosaveTile(initialFocus = true) {
                            scope.launch(Dispatchers.IO) {
                                NativeApp.loadAutosaveState()
                                withContext(Dispatchers.Main) { onBack() }
                            }
                        }
                    }
                }
                items((0 until SLOTS).toList(), key = { "slot_$it" }) { slot ->
                    SlotTile(slot, mode, initialFocus = slot == 0 && !(mode == SaveMode.Load && hasAutosave)) { selected ->
                        scope.launch(Dispatchers.IO) {
                            when (mode) {
                                SaveMode.Save -> NativeApp.saveStateToSlot(selected)
                                SaveMode.Load -> NativeApp.loadStateFromSlot(selected)
                            }
                            withContext(Dispatchers.Main) { onBack() }
                        }
                    }
                }
            }
        }
    }
}

/** Auto-save-on-exit + auto-load-last-state-on-boot persistence toggles. */
@Composable
private fun AutoOptions(modifier: Modifier = Modifier) {
    val prefs = MainActivityRuntime.prefs
    var autoSave by remember { mutableStateOf(prefs.getBoolean("autoSaveOnExit", false)) }
    var autoLoad by remember { mutableStateOf(prefs.getBoolean("autoLoadOnBoot", false)) }
    GlassPanel(modifier = modifier, contentPadding = 12.dp) {
        Column {
            ToggleRow(str("savestate.autoSaveOnExit"), autoSave) { value ->
                autoSave = value
                prefs.edit().putBoolean("autoSaveOnExit", value).apply()
            }
            Spacer(Modifier.height(6.dp))
            ToggleRow(str("savestate.autoLoadOnBoot"), autoLoad) { value ->
                autoLoad = value
                prefs.edit().putBoolean("autoLoadOnBoot", value).apply()
            }
        }
    }
}

@Composable
private fun ToggleRow(label: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth().clickable { onChange(!checked) },
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            label,
            modifier = Modifier.weight(1f),
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurface,
        )
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

@Composable
private fun AutosaveTile(initialFocus: Boolean = false, onPick: () -> Unit) {
    val gamePath by produceState<String?>(initialValue = null) {
        value = withContext(Dispatchers.IO) { runCatching { NativeApp.getAutosaveGamePath() }.getOrNull() }
    }
    val image by produceState<android.graphics.Bitmap?>(initialValue = null) {
        value = withContext(Dispatchers.IO) {
            runCatching {
                val bytes = NativeApp.getAutosaveImage() ?: return@runCatching null
                if (bytes.isEmpty()) null else BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
            }.getOrNull()
        }
    }
    TileFrame(
        borderColor = Color(0xFFFFB347).copy(alpha = 0.7f),
        backgroundColor = Color(0xFF2F2820),
        initialFocus = initialFocus,
        onClick = onPick,
    ) {
        image?.let {
            Image(it.asImageBitmap(), str("savestate.autosave.screenshotDesc"),
                Modifier.fillMaxSize(), contentScale = ContentScale.Crop)
        }
        BottomLabel(
            title = str("savestate.autosave.title"),
            subtitle = gamePath?.substringAfterLast('/')?.substringBeforeLast('.')
                ?: str("savestate.autosave.savedOnExit"),
            titleColor = Color(0xFFFFB347),
        )
    }
}

@Composable
private fun SlotTile(slot: Int, mode: SaveMode, initialFocus: Boolean = false, onPick: (Int) -> Unit) {
    val gamePath by produceState<String?>(initialValue = null, slot) {
        value = withContext(Dispatchers.IO) { runCatching { NativeApp.getGamePathSlot(slot) }.getOrNull() }
    }
    val image by produceState<android.graphics.Bitmap?>(initialValue = null, slot) {
        value = withContext(Dispatchers.IO) {
            runCatching {
                val bytes = NativeApp.getImageSlot(slot) ?: return@runCatching null
                if (bytes.isEmpty()) null else BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
            }.getOrNull()
        }
    }
    val empty = gamePath.isNullOrEmpty()
    // Load: empty slots disabled. Save: any slot is a valid target.
    val enabled = mode == SaveMode.Save || !empty
    TileFrame(
        borderColor = MaterialTheme.colorScheme.outline.copy(alpha = 0.6f),
        backgroundColor = MaterialTheme.colorScheme.surfaceVariant,
        enabled = enabled,
        initialFocus = initialFocus,
        onClick = { onPick(slot) },
    ) {
        image?.let {
            Image(it.asImageBitmap(), "Slot ${slot + 1}",
                Modifier.fillMaxSize(), contentScale = ContentScale.Crop)
        }
        BottomLabel(
            title = "${str("memcard.slot1").substringBefore(' ')} ${slot + 1}",
            subtitle = when {
                !empty -> gamePath?.substringAfterLast('/')?.substringBeforeLast('.') ?: ""
                mode == SaveMode.Save -> str("savestate.slot.emptyTapToSave")
                else -> null
            },
            titleColor = Color.White,
        )
    }
}

@Composable
private fun TileFrame(
    borderColor: Color,
    backgroundColor: Color,
    enabled: Boolean = true,
    initialFocus: Boolean = false,
    onClick: () -> Unit,
    content: @Composable BoxScope.() -> Unit,
) {
    Box(
        Modifier
            .width(TILE_WIDTH_DP.dp)
            .fillMaxHeight()
            .then(if (initialFocus) Modifier.initialPadFocus() else Modifier)
            .padFocusRing(RoundedCornerShape(12.dp))
            .clip(RoundedCornerShape(12.dp))
            .background(backgroundColor)
            .border(1.dp, borderColor, RoundedCornerShape(12.dp))
            .clickable(enabled = enabled, onClick = onClick),
        content = content,
    )
}

@Composable
private fun BoxScope.BottomLabel(title: String, subtitle: String?, titleColor: Color) {
    Box(
        Modifier
            .align(Alignment.BottomStart)
            .fillMaxWidth()
            .background(
                Brush.verticalGradient(
                    0.0f to Color.Transparent,
                    1.0f to Color.Black.copy(alpha = 0.82f),
                ),
            )
            .padding(horizontal = 10.dp, vertical = 8.dp),
    ) {
        Column {
            Text(title, color = titleColor, fontSize = 13.sp, fontWeight = FontWeight.Bold)
            if (!subtitle.isNullOrEmpty()) {
                Spacer(Modifier.height(2.dp))
                Text(
                    subtitle,
                    color = Color(0xFFCFE0FF),
                    fontSize = 11.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}
