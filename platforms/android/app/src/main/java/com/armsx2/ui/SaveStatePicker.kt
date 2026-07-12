package com.armsx2.ui

import android.graphics.BitmapFactory
import android.widget.Toast
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyHorizontalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.SideEffect
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
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.Main
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Save-state slot picker, doubling as the per-game Save Manager.
 *
 * 10 numbered slots — matches PCSX2 convention. Each tile previews via
 * `NativeApp.getImageSlot(slot)` (PNG-encoded snapshot) plus the on-disk
 * `.p2s` file from `getGamePathSlot(slot)`. Slots are inherently per-game
 * (the filename encodes serial + CRC), so this whole screen is scoped to
 * the running game.
 *
 * Save mode: tap a tile to write that slot. Load mode = the manager:
 *   - tap a non-empty tile to load it,
 *   - the red ✕ deletes a slot (with confirm),
 *   - Backup snapshots every existing slot into `savestates/backups/`,
 *     Restore copies the last snapshot back (with confirm),
 *   - the "Auto-save on exit" switch (global pref) makes Close Game write
 *     the autosave slot automatically instead of prompting.
 * File ops run in Kotlin against the emulator data folder off the main
 * thread; `refreshTick` re-probes the tiles afterward.
 *
 * Autosave tile (Load mode only, shown above slot 0 when present): the
 * "Save State And Exit" action writes a dedicated `.autosave.p2s` rather
 * than slot 0, so numbered slots stay user-controlled.
 */
object SaveStatePicker {
    enum class Mode { Save, Load }

    // Controller-navigation bridge: Render() publishes the current tile count and
    // an activator (flat tile index -> save/load action) so the in-game overlay's
    // D-pad + confirm can drive the grid without touch. Flat order matches the
    // grid: [autosave (Load+present)] then slots 0..9.
    // Three controller focus zones: the manager Header switches (Load mode only),
    // the slot Grid, and the Back button. Up from the top grid row enters Header;
    // Down from the bottom grid row enters Back.
    enum class NavZone { Header, Grid, Back }
    val zone = mutableStateOf(NavZone.Grid)
    val controllerSel = mutableStateOf(0)   // grid tile index
    val headerSel = mutableStateOf(0)       // header control index
    // Manager-header switch state, shared so both taps and the controller (header
    // zone) drive the same toggles and the UI reflects either.
    val autoSaveOnExit = mutableStateOf(false)
    val autoLoadOnBoot = mutableStateOf(false)
    private var tileCount = 0
    private var headerCount = 0             // 0 in Save mode (no manager header)
    private var activator: ((Int) -> Unit)? = null
    private var headerActivator: ((Int) -> Unit)? = null
    private var backAction: (() -> Unit)? = null

    fun resetControllerSel() {
        controllerSel.value = 0; headerSel.value = 0; zone.value = NavZone.Grid
        autoSaveOnExit.value = Main.prefs.getBoolean("autoSaveOnExit", false)
        autoLoadOnBoot.value = Main.prefs.getBoolean("autoLoadOnBoot", false)
    }

    fun move(dx: Int, dy: Int) {
        when (zone.value) {
            NavZone.Grid -> {
                val i = controllerSel.value
                if (dy < 0 && i % 2 == 0 && headerCount > 0) {
                    zone.value = NavZone.Header; headerSel.value = 0; return   // land on Auto-save
                }
                if (dy > 0 && i % 2 == 1) { zone.value = NavZone.Back; return }
                moveGrid(dx, dy)
            }
            // Header layout is 2D: the two switches are STACKED on the left
            // (0=Auto-save on top, 1=Auto-load below); Backup(2)/Restore(3) sit on the
            // top-right. Match that visually so Up/Down flip the switches and Right
            // reaches Backup/Restore. Down off the bottom control returns to the grid.
            NavZone.Header -> {
                val cur = headerSel.value
                when {
                    dy > 0 && cur == 0 -> headerSel.value = 1        // Auto-save ↓ Auto-load
                    dy > 0 -> zone.value = NavZone.Grid              // Auto-load/Backup/Restore ↓ grid
                    dy < 0 && cur == 1 -> headerSel.value = 0        // Auto-load ↑ Auto-save
                    dx > 0 && cur < 2 -> headerSel.value = 2         // a switch → Backup
                    dx > 0 && cur == 2 -> headerSel.value = 3        // Backup → Restore
                    dx < 0 && cur == 3 -> headerSel.value = 2        // Restore → Backup
                    dx < 0 && cur == 2 -> headerSel.value = 0        // Backup → Auto-save
                }
            }
            NavZone.Back -> if (dy < 0) zone.value = NavZone.Grid
        }
    }

    // 2-row, column-major grid: flat index i sits at row = i%2, col = i/2. Left/Right
    // step a whole column (±2); Up/Down flip the row within the current column.
    private fun moveGrid(dx: Int, dy: Int) {
        if (tileCount <= 0) return
        val i = controllerSel.value
        val next = when {
            dx > 0 -> i + 2
            dx < 0 -> i - 2
            dy > 0 -> if (i % 2 == 0) i + 1 else i
            dy < 0 -> if (i % 2 == 1) i - 1 else i
            else -> i
        }
        controllerSel.value = next.coerceIn(0, tileCount - 1)
    }

    fun confirm() {
        when (zone.value) {
            NavZone.Grid -> activator?.invoke(controllerSel.value)
            NavZone.Header -> headerActivator?.invoke(headerSel.value)
            NavZone.Back -> backAction?.invoke()
        }
    }

    private const val SLOTS = 10
    // Fixed tile width — LazyHorizontalGrid computes tile height from the
    // grid's intrinsic height ÷ rows. Width is up to the tile to set.
    private const val TILE_WIDTH_DP = 180

    // ---- File-backed manager ops (savestates live under a MANAGE_EXTERNAL_
    // STORAGE path, so plain File access works) -------------------------------

    /** Directory that holds this game's `.p2s` slots (parent of any slot path). */
    private fun savestateDir(): File? =
        runCatching { File(NativeApp.getGamePathSlot(0)).parentFile }.getOrNull()

    private fun backupDir(): File? = savestateDir()?.let { File(it, "backups") }

    /** The slot's `.p2s` file iff it exists on disk, else null. */
    private fun existingSlotFile(slot: Int): File? =
        runCatching { File(NativeApp.getGamePathSlot(slot)) }.getOrNull()?.takeIf { it.exists() }

    private fun deleteSlot(slot: Int): Boolean =
        runCatching { existingSlotFile(slot)?.delete() ?: false }.getOrDefault(false)

    /** Copy every existing slot for this game into backups/. Returns the count. */
    private fun backupAll(): Int {
        val bdir = backupDir()?.apply { mkdirs() } ?: return 0
        var n = 0
        (0 until SLOTS).forEach { s ->
            val f = existingSlotFile(s) ?: return@forEach
            if (runCatching { f.copyTo(File(bdir, f.name), overwrite = true) }.isSuccess) n++
        }
        return n
    }

    /** Copy the last backup of each slot back over the live slots. */
    private fun restoreAll(): Int {
        val dir = savestateDir() ?: return 0
        val bdir = backupDir() ?: return 0
        var n = 0
        (0 until SLOTS).forEach { s ->
            val name = runCatching { File(NativeApp.getGamePathSlot(s)).name }.getOrNull() ?: return@forEach
            val bak = File(bdir, name)
            if (bak.exists() && runCatching { bak.copyTo(File(dir, name), overwrite = true) }.isSuccess) n++
        }
        return n
    }

    @Composable
    fun Render(mode: Mode, onDone: () -> Unit, onBack: () -> Unit) {
        // Save/load run on Dispatchers.IO — Main.invoke would have queued
        // the task behind the VM thread (single-threaded eDispatcher is
        // permanently blocked inside runVMThread's main loop), so the save
        // would never actually fire. IO pool is a separate thread, JNI
        // handles thread attachment fine. onDone hops back to Main for
        // the overlay state mutation.
        val scope = rememberCoroutineScope()
        val context = LocalContext.current
        val manage = mode == Mode.Load
        // Bumped after a delete/restore so the slot tiles re-probe disk.
        var refreshTick by remember { mutableStateOf(0) }
        var pendingDelete by remember { mutableStateOf<Int?>(null) }
        var pendingRestore by remember { mutableStateOf(false) }
        // Probe the autosave slot once at composition (Load mode only).
        val hasAutosave by produceState<Boolean>(initialValue = false, mode) {
            value = if (mode == Mode.Load) withContext(Dispatchers.IO) {
                NativeApp.hasAutosaveState()
            } else false
        }

        // Destructive-action confirmations.
        pendingDelete?.let { slot ->
            ConfirmDialog(
                title = "Delete Slot $slot?",
                message = "Permanently removes the save in slot $slot for this game.",
                confirmLabel = str("action.delete"),
                onConfirm = {
                    pendingDelete = null
                    scope.launch(Dispatchers.IO) {
                        deleteSlot(slot)
                        withContext(Dispatchers.Main) { refreshTick++ }
                    }
                },
                onDismiss = { pendingDelete = null },
            )
        }
        if (pendingRestore) {
            ConfirmDialog(
                title = str("savestate.restoreBackup.confirmTitle"),
                message = str("savestate.restoreBackup.confirmMessage"),
                confirmLabel = str("action.restore"),
                onConfirm = {
                    pendingRestore = false
                    scope.launch(Dispatchers.IO) {
                        val n = restoreAll()
                        withContext(Dispatchers.Main) {
                            refreshTick++
                            Toast.makeText(
                                context,
                                if (n > 0) "Restored $n save(s)" else I18n.get("savestate.noBackupFound"),
                                Toast.LENGTH_SHORT,
                            ).show()
                        }
                    }
                },
                onDismiss = { pendingRestore = false },
            )
        }

        Column(Modifier.fillMaxSize()) {
            Text(
                if (mode == Mode.Save) str("savestate.title.save") else str("savestate.title.loadManage"),
                color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.SemiBold,
            )
            if (manage) {
                Spacer(Modifier.height(8.dp))
                ManagerHeader(
                    scope, context,
                    focused = if (zone.value == NavZone.Header) headerSel.value else -1,
                ) { pendingRestore = true }
            }
            Spacer(Modifier.height(8.dp))
            // Publish the flat tile list to the controller bridge: [autosave?] then
            // slots 0..9. The overlay's D-pad moves controllerSel; confirm calls this.
            val autosaveShown = mode == Mode.Load && hasAutosave
            val offset = if (autosaveShown) 1 else 0
            SideEffect {
                tileCount = offset + SLOTS
                // Header zone (Load mode): 0=Auto-save switch, 1=Auto-load switch,
                // 2=Backup, 3=Restore. Save mode has no header.
                headerCount = if (manage) 4 else 0
                headerActivator = { idx ->
                    when (idx) {
                        0 -> { val v = !autoSaveOnExit.value; autoSaveOnExit.value = v
                               Main.prefs.edit().putBoolean("autoSaveOnExit", v).apply() }
                        1 -> { val v = !autoLoadOnBoot.value; autoLoadOnBoot.value = v
                               Main.prefs.edit().putBoolean("autoLoadOnBoot", v).apply() }
                        2 -> scope.launch(Dispatchers.IO) {
                            val n = backupAll()
                            withContext(Dispatchers.Main) {
                                Toast.makeText(context, if (n > 0) "Backed up $n save(s)" else I18n.get("savestate.noSavesToBackUp"), Toast.LENGTH_SHORT).show()
                            }
                        }
                        3 -> pendingRestore = true
                    }
                }
                backAction = onBack
                activator = { idx ->
                    if (autosaveShown && idx == 0) {
                        scope.launch(Dispatchers.IO) {
                            NativeApp.loadAutosaveState()
                            withContext(Dispatchers.Main) { onDone() }
                        }
                    } else {
                        val slot = (idx - offset).coerceIn(0, SLOTS - 1)
                        scope.launch(Dispatchers.IO) {
                            when (mode) {
                                Mode.Save -> NativeApp.saveStateToSlot(slot)
                                Mode.Load -> NativeApp.loadStateFromSlot(slot)
                            }
                            withContext(Dispatchers.Main) { onDone() }
                        }
                    }
                }
            }
            // 2-row horizontal grid. Autosave (Load mode only) is the
            // leading tile, then numbered slots 0-9 flow column-by-column.
            LazyHorizontalGrid(
                rows = GridCells.Fixed(2),
                contentPadding = PaddingValues(2.dp),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.weight(1f).fillMaxWidth(),
            ) {
                if (mode == Mode.Load && hasAutosave) {
                    item(key = "autosave") {
                        AutosaveTile(highlighted = zone.value == NavZone.Grid && controllerSel.value == 0) {
                            scope.launch(Dispatchers.IO) {
                                NativeApp.loadAutosaveState()
                                withContext(Dispatchers.Main) { onDone() }
                            }
                        }
                    }
                }
                items((0 until SLOTS).toList(), key = { "slot_$it" }) { slot ->
                    SlotTile(
                        slot = slot,
                        mode = mode,
                        refreshTick = refreshTick,
                        highlighted = zone.value == NavZone.Grid && controllerSel.value == slot + offset,
                        onPick = { selected ->
                            when (mode) {
                                Mode.Save -> scope.launch(Dispatchers.IO) {
                                    NativeApp.saveStateToSlot(selected)
                                    withContext(Dispatchers.Main) { onDone() }
                                }
                                Mode.Load -> scope.launch(Dispatchers.IO) {
                                    NativeApp.loadStateFromSlot(selected)
                                    withContext(Dispatchers.Main) { onDone() }
                                }
                            }
                        },
                        onDelete = { pendingDelete = it },
                    )
                }
            }
            Spacer(Modifier.height(8.dp))
            BackRow(highlighted = zone.value == NavZone.Back, onBack = onBack)
        }
    }

    // Manager controls: auto-save-on-exit switch + Backup / Restore.
    @Composable
    private fun ManagerHeader(
        scope: CoroutineScope,
        context: android.content.Context,
        focused: Int,
        onRestoreRequest: () -> Unit,
    ) {
        val glow = Color(0xFF3DA5FF)
        // Controller-focus ring around the header control at index [focused]
        // (0=auto-save, 1=auto-load, 2=Backup, 3=Restore; -1 = none).
        fun ring(i: Int): Modifier =
            if (focused == i) Modifier.border(1.5.dp, glow, RoundedCornerShape(6.dp)) else Modifier
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Row(ring(0).padding(2.dp), verticalAlignment = Alignment.CenterVertically) {
                Text(str("savestate.autoSaveOnExit"), color = Color.White, fontSize = 12.sp)
                Spacer(Modifier.width(6.dp))
                Switch(
                    checked = autoSaveOnExit.value,
                    onCheckedChange = {
                        autoSaveOnExit.value = it
                        Main.prefs.edit().putBoolean("autoSaveOnExit", it).apply()
                    },
                )
            }
            Spacer(Modifier.weight(1f))
            PillButton(str("savestate.backup"), highlighted = focused == 2) {
                scope.launch(Dispatchers.IO) {
                    val n = backupAll()
                    withContext(Dispatchers.Main) {
                        Toast.makeText(
                            context,
                            if (n > 0) "Backed up $n save(s)" else I18n.get("savestate.noSavesToBackUp"),
                            Toast.LENGTH_SHORT,
                        ).show()
                    }
                }
            }
            Spacer(Modifier.width(6.dp))
            PillButton(str("savestate.restore"), highlighted = focused == 3, onClick = onRestoreRequest)
        }
        Row(
            Modifier.fillMaxWidth().padding(top = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Row(ring(1).padding(2.dp), verticalAlignment = Alignment.CenterVertically) {
                Text(str("savestate.autoLoadOnBoot"), color = Color.White, fontSize = 12.sp)
                Spacer(Modifier.width(6.dp))
                Switch(
                    checked = autoLoadOnBoot.value,
                    onCheckedChange = {
                        autoLoadOnBoot.value = it
                        Main.prefs.edit().putBoolean("autoLoadOnBoot", it).apply()
                    },
                )
            }
        }
    }

    @Composable
    private fun PillButton(label: String, highlighted: Boolean = false, onClick: () -> Unit) {
        Box(
            Modifier
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xFF272525).copy(alpha = 0.6f))
                .border(
                    if (highlighted) 1.5.dp else 1.dp,
                    if (highlighted) Color(0xFF3DA5FF) else Color(0xFF3A3A3A).copy(alpha = 0.7f),
                    RoundedCornerShape(8.dp),
                )
                .clickable(onClick = onClick)
                .padding(horizontal = 12.dp, vertical = 6.dp),
        ) {
            Text(label, color = Color(0xFFAACCFF), fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
        }
    }

    @Composable
    private fun ConfirmDialog(
        title: String,
        message: String,
        confirmLabel: String,
        onConfirm: () -> Unit,
        onDismiss: () -> Unit,
    ) {
        AlertDialog(
            onDismissRequest = onDismiss,
            containerColor = Color(0xFF1B1A1A),
            titleContentColor = Color.White,
            textContentColor = Color.White,
            title = { Text(title, fontWeight = FontWeight.Bold, fontSize = 15.sp) },
            text = { Text(message, fontSize = 13.sp) },
            confirmButton = { TextButton(onClick = onConfirm) { Text(confirmLabel) } },
            dismissButton = { TextButton(onClick = onDismiss) { Text(str("action.cancel")) } },
        )
    }

    @Composable
    private fun AutosaveTile(highlighted: Boolean = false, onPick: () -> Unit) {
        val gamePath by produceState<String?>(initialValue = null) {
            value = withContext(Dispatchers.IO) { NativeApp.getAutosaveGamePath() }
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
            backgroundColor = Color(0xFF2F2820).copy(alpha = 0.45f),
            highlighted = highlighted,
            onClick = onPick,
        ) {
            val bmp = image
            if (bmp != null) {
                Image(
                    bitmap = bmp.asImageBitmap(),
                    contentDescription = str("savestate.autosave.screenshotDesc"),
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize(),
                )
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
    private fun SlotTile(slot: Int, mode: Mode, refreshTick: Int, highlighted: Boolean = false, onPick: (Int) -> Unit, onDelete: (Int) -> Unit) {
        // Re-probe whenever a delete/restore bumps the tick. File existence is
        // the authoritative "empty" signal (getGamePathSlot builds a name even
        // for empty slots).
        val tick = refreshTick
        val file by produceState<File?>(initialValue = null, slot, tick) {
            value = withContext(Dispatchers.IO) { existingSlotFile(slot) }
        }
        val image by produceState<android.graphics.Bitmap?>(initialValue = null, slot, tick) {
            value = withContext(Dispatchers.IO) {
                runCatching {
                    val bytes = NativeApp.getImageSlot(slot) ?: return@runCatching null
                    if (bytes.isEmpty()) null else BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
                }.getOrNull()
            }
        }

        val empty = file == null
        // Load mode: empty slots are disabled. Save mode: tap = overwrite,
        // empty slots are valid targets.
        val enabled = mode == Mode.Save || !empty
        val meta = remember(file) {
            file?.let { f ->
                val date = SimpleDateFormat("dd/MM/yy HH:mm", Locale.getDefault()).format(Date(f.lastModified()))
                val mb = f.length() / 1024.0 / 1024.0
                "$date  •  ${"%.1f".format(mb)} MB"
            }
        }
        TileFrame(
            borderColor = Color(0xFF3A3A3A).copy(alpha = 0.6f),
            backgroundColor = Color(0xFF272525).copy(alpha = 0.3f),
            enabled = enabled,
            highlighted = highlighted,
            onClick = { onPick(slot) },
        ) {
            val bmp = image
            if (bmp != null) {
                Image(
                    bitmap = bmp.asImageBitmap(),
                    contentDescription = "Slot $slot screenshot",
                    contentScale = ContentScale.Crop,
                    modifier = Modifier.fillMaxSize(),
                )
            }
            BottomLabel(
                title = "Slot $slot",
                subtitle = when {
                    !empty -> meta
                    mode == Mode.Save -> str("savestate.slot.emptyTapToSave")
                    else -> null
                },
                titleColor = Color.White,
            )
            // Delete affordance — manager (Load) mode, non-empty slots only.
            if (mode == Mode.Load && !empty) {
                Box(
                    Modifier
                        .align(Alignment.TopEnd)
                        .padding(6.dp)
                        .size(26.dp)
                        .clip(RoundedCornerShape(13.dp))
                        .background(Color(0xCCB00020))
                        .clickable { onDelete(slot) },
                    contentAlignment = Alignment.Center,
                ) {
                    Text("✕", color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
    }

    // Tile chrome shared by AutosaveTile + SlotTile. Body content is rendered
    // edge-to-edge inside the rounded clip; BottomLabel can be dropped into
    // the same scope to overlay the title/subtitle row over the bottom of
    // the image.
    @Composable
    private fun TileFrame(
        borderColor: Color,
        backgroundColor: Color,
        enabled: Boolean = true,
        highlighted: Boolean = false,
        onClick: () -> Unit,
        content: @Composable androidx.compose.foundation.layout.BoxScope.() -> Unit,
    ) {
        // Controller focus draws a bright, thicker ring over the normal border.
        val ringColor = if (highlighted) Color(0xFF3DA5FF) else borderColor
        val ringWidth = if (highlighted) 2.5.dp else 1.dp
        Box(
            Modifier
                .width(TILE_WIDTH_DP.dp)
                .fillMaxHeight()
                .clip(RoundedCornerShape(8.dp))
                .background(backgroundColor)
                .border(ringWidth, ringColor, RoundedCornerShape(8.dp))
                .clickable(enabled = enabled, onClick = onClick),
        ) {
            content()
        }
    }

    // Title + subtitle in the bottom-left of the tile, painted over a
    // left-to-right fade so the text stays readable against any thumbnail.
    // Designed to be invoked inside TileFrame so its Box parent provides
    // BoxScope alignment.
    @Composable
    private fun androidx.compose.foundation.layout.BoxScope.BottomLabel(
        title: String,
        subtitle: String?,
        titleColor: Color,
    ) {
        Box(
            Modifier
                .align(Alignment.BottomStart)
                .fillMaxWidth()
                .background(
                    Brush.horizontalGradient(
                        0.0f to Color.Black.copy(alpha = 0.75f),
                        0.6f to Color.Black.copy(alpha = 0.45f),
                        1.0f to Color.Transparent,
                    )
                )
                .padding(horizontal = 8.dp, vertical = 6.dp),
        ) {
            Column {
                Text(
                    title,
                    color = titleColor,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                )
                if (!subtitle.isNullOrEmpty()) {
                    Spacer(Modifier.height(2.dp))
                    Text(
                        subtitle,
                        color = Color(0xFFAACCFF),
                        fontSize = 10.sp,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
        }
    }

    @Composable
    private fun BackRow(highlighted: Boolean = false, onBack: () -> Unit) {
        Box(
            Modifier
                .fillMaxWidth()
                .height(40.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xFF272525).copy(alpha = 0.3f))
                .border(
                    if (highlighted) 1.5.dp else 1.dp,
                    if (highlighted) Color(0xFF3DA5FF) else Color(0xFF3A3A3A).copy(alpha = 0.6f),
                    RoundedCornerShape(8.dp),
                )
                .clickable(onClick = onBack)
                .padding(horizontal = 14.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Text(str("action.back"), color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
        }
    }
}
