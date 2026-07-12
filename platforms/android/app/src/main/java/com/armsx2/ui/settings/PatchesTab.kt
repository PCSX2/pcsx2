package com.armsx2.ui.settings

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.documentfile.provider.DocumentFile
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Checkbox
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.rememberCoroutineScope
import com.armsx2.EmuState
import com.armsx2.Main
import com.armsx2.PatchRepo
import com.armsx2.config.Settings
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import com.armsx2.ui.Colors
import com.armsx2.ui.InGameOverlay
import kr.co.iefriends.pcsx2.NativeApp
import java.io.File

private data class PnachGameId(val serial: String, val crc: String) {
    val prefix: String get() = "${serial}_${crc}"
}

/** An open per-cheat editing session for an already-installed `.pnach`. */
private data class EditSession(
    val file: File,
    val gametitle: String,
    val cheats: List<PatchRepo.LocalCheat>,
)

// One installed local .pnach expanded into its individual cheats/patches, for the
// unified "My local patches & cheats" checkbox browser (mirrors the online one).
private data class LocalFileCheats(
    val file: File,
    val source: String, // "cheats" or "patches"
    val gametitle: String,
    val cheats: List<PatchRepo.LocalCheat>,
)

private val activeGameIdRegex = Regex("""([A-Za-z]{4}-\d{5})\s*\(([0-9A-Fa-f]{8})\)""")
private val serialRegex = Regex("""([A-Za-z]{4})[\s_-]?(\d{3})\.?(\d{2})""")
private val crcRegex = Regex("""(?<![0-9A-Fa-f])([0-9A-Fa-f]{8})(?![0-9A-Fa-f])""")
private val loadableSerialPnachRegex = Regex("""^[A-Z]{4}-\d{5}_[0-9A-F]{8}.*\.pnach$""", RegexOption.IGNORE_CASE)
private val loadableCrcPnachRegex = Regex("""^[0-9A-F]{8}.*\.pnach$""", RegexOption.IGNORE_CASE)

private fun normalizeSerial(value: String?): String? {
    if (value.isNullOrBlank()) return null
    val match = serialRegex.find(value) ?: return null
    return "${match.groupValues[1].uppercase()}-${match.groupValues[2]}${match.groupValues[3]}"
}

private fun activePnachGameId(): PnachGameId? {
    val pauseLabel = runCatching { NativeApp.getPauseGameSerial() }.getOrNull().orEmpty()
    val currentCrc = runCatching { NativeApp.getGameCRC() }.getOrNull()
        ?.trim()
        ?.uppercase()
        ?.takeIf { crcRegex.matches(it) && it != "00000000" }
    activeGameIdRegex.find(pauseLabel)?.let { match ->
        val crc = currentCrc ?: match.groupValues[2].uppercase()
        if (crc != "00000000")
            return PnachGameId(normalizeSerial(match.groupValues[1]) ?: return null, crc)
    }

    val serial = normalizeSerial(Main.currentGame.value?.serial)
        ?: normalizeSerial(runCatching { NativeApp.getGameSerial() }.getOrNull())
    val crc = currentCrc ?: crcRegex.find(pauseLabel)?.groupValues?.get(1)?.uppercase()
        ?.takeIf { it != "00000000" }
    return if (serial != null && crc != null) PnachGameId(serial, crc) else null
}

private fun safePnachStem(rawName: String): String {
    val base = rawName.substringBeforeLast('.')
        .replace(Regex("""[^A-Za-z0-9._-]+"""), "_")
        .trim('_', '.', '-')
        .take(48)
    return base.ifEmpty { "manual" }
}

private fun loadablePnachName(fileName: String): Boolean =
    loadableSerialPnachRegex.matches(fileName) || loadableCrcPnachRegex.matches(fileName)

private fun importPnachFileName(sourceName: String, gameId: PnachGameId?): String {
    val stem = safePnachStem(sourceName)
    if (gameId != null) {
        return if (stem.startsWith(gameId.prefix, ignoreCase = true))
            "$stem.pnach"
        else
            "${gameId.prefix}_${stem}.pnach"
    }

    val serial = normalizeSerial(sourceName)
    val crc = crcRegex.find(sourceName)?.groupValues?.get(1)?.uppercase()
    return when {
        serial != null && crc != null -> "${serial}_${crc}_${stem}.pnach"
        crc != null -> "${crc}_${stem}.pnach"
        sourceName.endsWith(".pnach", ignoreCase = true) -> sourceName
        else -> "$stem.pnach"
    }
}

private fun manualPnachFileName(title: String, gameId: PnachGameId?): String {
    val stem = safePnachStem(title.ifBlank { "manual" })
    return if (gameId != null) "${gameId.prefix}_${stem}.pnach" else "$stem.pnach"
}

private fun pnachTargetFile(dir: File, desiredName: String): File {
    val normalizedName = if (desiredName.endsWith(".pnach", ignoreCase = true)) desiredName else "$desiredName.pnach"
    return File(dir, normalizedName)
}

private fun executablePnachBody(body: String): String =
    body.trim().lines()
        .mapNotNull { rawLine ->
            val line = rawLine.trimEnd()
            val trimmed = line.trim()
            // Android's importer/entry UI means "run this code". PCSX2 treats
            // labelled groups like [60 FPS] as disabled until the label is
            // added to Cheats/Enable, so flatten labels into comments and let
            // patch= lines auto-activate as unlabelled legacy PNACH commands.
            if (trimmed.length > 2 && trimmed.first() == '[' && trimmed.last() == ']')
                "// $trimmed"
            else
                line
        }
        .joinToString("\n")
        .trim()

// Cheat body with [labels] PRESERVED and patch= lines active (uncommented). Unlike
// executablePnachBody (which flattens labels so patches auto-run unlabelled), this
// is the proper pnach-2.0 form: PCSX2 sees a labelled group and gates it by the
// [Cheats] Enable= list (written via NativeApp.setEnabledPatches), so per-cheat
// on/off state lives in PCSX2-Android.ini and survives reset. Used for cheats only.
private fun labelledCheatBody(body: String): String =
    body.trim().lines().joinToString("\n") { raw ->
        val t = raw.trim()
        if (PatchRepo.isPatchCommand(t)) t.replaceFirst(Regex("^//\\s*"), "") else raw.trimEnd()
    }.trim()

private fun manualPnachContents(title: String, body: String, gameId: PnachGameId?): String {
    val header = buildList {
        add("// ARMSX2 manual PNACH")
        if (title.isNotBlank()) add("// $title")
        if (gameId != null) add("// ${gameId.serial} ${gameId.crc}")
    }.joinToString("\n")
    // Imported/manual cheats go to the cheats folder — keep their [Section] labels
    // (pnach-2.0) instead of flattening to //comments (the old method). PCSX2 then
    // sees individual cheat groups: they auto-enable until the user toggles (the
    // has_cheat_selection fallback in Patch.cpp), and the "Edit" per-cheat toggle
    // list can split them. If the file has no labels, labelledCheatBody leaves the
    // bare patch= lines as-is (still auto-enabled, unlabelled — old behavior).
    val normalizedBody = labelledCheatBody(body)
    return "$header\n$normalizedBody\n"
}

// Rebuild an installed .pnach from the per-cheat editor. Each cheat's patch=
// lines are activated (checked) or //-commented (unchecked); any [Section]
// label is flattened to a //comment so enabled patches auto-run as unlabelled
// PNACH (the [Enable] list doesn't persist on Android). Keeps ALL cheats in the
// file — deselected ones are commented out, not dropped — so it round-trips.
private fun rebuildInstalledPnach(
    gametitle: String,
    cheats: List<Pair<PatchRepo.LocalCheat, Boolean>>,
    keepLabels: Boolean = false,
): String = buildString {
    if (gametitle.isNotEmpty()) append("gametitle=").append(gametitle).append("\n\n")
    cheats.forEach { (cheat, enabled) ->
        cheat.body.lines().forEach { raw ->
            val t = raw.trim()
            val out = when {
                // Cheats (keepLabels): preserve [Section] so PCSX2 gates the group
                // by the [Cheats] Enable list. Patches (default): flatten to a
                // //comment so patch= lines auto-run unlabelled (legacy path).
                t.length > 2 && t.first() == '[' && t.last() == ']' ->
                    if (keepLabels) raw.trimEnd() else "// $t"
                PatchRepo.isPatchCommand(t) -> {
                    val bare = t.replaceFirst(Regex("^//\\s*"), "")
                    if (enabled) bare else "// $bare"
                }
                else -> raw
            }
            append(out).append('\n')
        }
        append('\n')
    }
}

/**
 * Patch / cheat toggles + a PNACH importer.
 *
 * Widescreen / no-interlacing come from the bundled patch database (just toggle
 * them). User cheats are `.pnach` files dropped into <dataRoot>/cheats/ — import
 * them here, enable "Cheats (PNACH)", and restart the game. The importer names
 * files from the active game when possible so emucore can find them at boot.
 */
@Composable
fun PatchesTab(state: MutableState<Settings>) {
    val s = state.value
    // RetroAchievements hardcore forbids cheats (the core also refuses to apply
    // them — see Patch.cpp). Recomposes when the achievements poll flips it.
    val hardcore by InGameOverlay.hardcoreOn
    val context = LocalContext.current
    val scroll = remember { ScrollState(0) }
    ControllerAutoScroll(scroll)
    val activeGameId = activePnachGameId()
    // The game this per-game view belongs to: the running game, else the library
    // game opened via long-press. Used to FILTER the installed list so importing a
    // pnach for game A no longer shows it under game B — files are named
    // <SERIAL>_<CRC>...pnach (or legacy <CRC>...pnach), matching how emucore loads them.
    val viewLibraryGame = InGameOverlay.patchPreviewGame
    val viewSerial = (activeGameId?.serial ?: normalizeSerial(viewLibraryGame?.serial))?.uppercase()
    val viewCrc = activeGameId?.crc?.uppercase()
    fun pnachBelongsToView(name: String): Boolean {
        // No game context (global view, nothing running/previewed) -> show everything.
        if (viewSerial == null && viewCrc == null) return true
        val n = name.uppercase()
        // Serial-prefixed file for this game (CRC-lenient: any dump of the same game).
        if (viewSerial != null && n.startsWith("${viewSerial}_")) return true
        // CRC-only (legacy) file for this exact dump.
        if (viewCrc != null && n.startsWith(viewCrc)) return true
        return false
    }

    val cheatsDir = remember { File(Main.assetCopyRoot(context), "cheats").apply { mkdirs() } }
    // Loose patches load from <DataRoot>/patches (EmuFolders::Patches); cheats
    // from <DataRoot>/cheats. Downloaded files land in the matching dir.
    val patchesDir = remember { File(Main.assetCopyRoot(context), "patches").apply { mkdirs() } }
    // List both folders, but only the CURRENT game's pnach (see pnachBelongsToView) so
    // one game's cheats never bleed into another game's per-game view.
    fun listPnach(): List<File> =
        listOf(patchesDir, cheatsDir)
            .flatMap { it.listFiles()?.toList() ?: emptyList() }
            .filter { it.isFile && it.name.endsWith(".pnach", ignoreCase = true) }
            .filter { pnachBelongsToView(it.name) }
            .sortedBy { it.name.lowercase() }
    var pnachFiles: List<File> by remember { mutableStateOf(listPnach()) }
    fun refresh() { pnachFiles = listPnach() }
    var pnachStatus by remember { mutableStateOf("") }
    var showManualDialog by remember { mutableStateOf(false) }
    var showPatchWarning by remember { mutableStateOf(false) }
    var downloading by remember { mutableStateOf(false) }
    var browseResult by remember { mutableStateOf<PatchRepo.Result?>(null) }
    // Game the open browser results belong to (running game, or a library game
    // picked before launch). Drives the saved file name so emucore loads it.
    var browseGameId by remember { mutableStateOf<PnachGameId?>(null) }
    val selected = remember { mutableStateMapOf<Int, Boolean>() } // entry index -> checked
    // Per-cheat editor for an already-installed .pnach (index -> checked).
    var editSession by remember { mutableStateOf<EditSession?>(null) }
    val editSelected = remember { mutableStateMapOf<Int, Boolean>() }
    // "My local patches & cheats": a flat checkbox browser over every cheat/patch
    // found in the installed .pnach files (mirrors the online browser). Key = the
    // "fileIndex:cheatIndex" of each entry.
    var localBrowse by remember { mutableStateOf<List<LocalFileCheats>?>(null) }
    val localSelected = remember { mutableStateMapOf<String, Boolean>() }
    val scope = rememberCoroutineScope()
    // Game whose settings were opened from the library via long-press (null
    // when a game is actually running). Lets us browse before booting.
    val libraryGame = InGameOverlay.patchPreviewGame

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)
    fun activateCheatsAndReload(): Int {
        if (hardcore) {
            pnachStatus = I18n.get("patches.status.cheatsDisabledHardcore")
            return 0
        }
        if (!state.value.enableCheats)
            apply(state.value.copy(enableCheats = true))

        // The overlay's fast live-settings path intentionally skips patch
        // toggles, so push this one directly before reloading PNACH files.
        NativeApp.setSetting("EmuCore", "EnableCheats", "bool", "true")
        NativeApp.commitSettings()
        return NativeApp.reloadPatches()
    }

    fun pnachResultMessage(action: String, savedName: String, activeCheats: Int): String =
        when {
            Main.eState.value == EmuState.STOPPED ->
                "$action $savedName. Cheats are enabled; start the game to load it."
            activeCheats > 0 ->
                "$action $savedName. $activeCheats active cheat patch${if (activeCheats == 1) "" else "es"}."
            activeCheats == 0 ->
                "$action $savedName, but 0 cheats are active. Check PNACH syntax/CRC, then restart."
            else ->
                "$action $savedName. Reload skipped; restart the game to load it."
        }

    // Fetch + parse this game's patch/cheat entries from the PCSX2 database,
    // then open the per-entry browser dialog. Works two ways:
    //   • in-game  — exact serial+CRC are known, fetch the file directly.
    //   • library  — a game was long-pressed but not booted; we only have its
    //                serial, so look it up in the repo file tree by name (the
    //                CRC comes back from the matched filename).
    fun startBrowse() {
        val running = activePnachGameId()
        val librarySerial = libraryGame?.serial?.takeIf { it.isNotBlank() }
        if ((running == null || running.crc.isBlank()) && librarySerial == null) {
            pnachStatus = I18n.get("patches.status.bootOrLongPress")
            return
        }
        downloading = true
        pnachStatus = I18n.get("patches.status.searchingDatabase")
        scope.launch(Dispatchers.IO) {
            val res = if (running != null && running.crc.isNotBlank())
                PatchRepo.fetchForGame(running.serial, running.crc)
            else
                PatchRepo.fetchForSerial(librarySerial)
            val gid = res.takeIf { it.error == null && it.crc.isNotBlank() }
                ?.let { PnachGameId(it.serial.ifBlank { librarySerial ?: running?.serial ?: "" }, it.crc) }
                ?: running
            withContext(Dispatchers.Main) {
                downloading = false
                if (res.error != null) {
                    pnachStatus = res.error
                } else {
                    selected.clear()
                    browseGameId = gid
                    browseResult = res
                    pnachStatus = ""
                }
            }
        }
    }

    // AetherSX2-style gate: warn before browsing/enabling patch codes unless the
    // user opted out ("Don't ask again").
    fun onDownloadClick() {
        if (downloading) return
        if (Main.prefs.getBoolean("patchCodesWarnAck", false)) startBrowse()
        else showPatchWarning = true
    }

    // Write ONLY the checked entries to disk, with their [labels] flattened to
    // comments so the patch= lines auto-run as unlabelled legacy PNACH. PCSX2
    // auto-enables unlabelled patches (Patch.cpp), so this activates exactly the
    // selected items and persists across reset — no [Patches]/[Cheats] "Enable"
    // list needed (that path doesn't survive on Android). Deselecting all for a
    // category removes its file. This is the same mechanism the manual importer
    // uses, which is why it reliably takes effect.
    fun applySelected() {
        val res = browseResult ?: return
        val gid = browseGameId
        val chosen = res.entries.filterIndexed { i, _ -> selected[i] == true }
        browseResult = null
        val base = gid?.let { if (it.serial.isNotBlank()) "${it.serial}_${it.crc}" else it.crc }
            ?.ifBlank { null } ?: "patch"
        var anyCheatChosen = false
        val saved = runCatching {
            val sources = if (hardcore) listOf("patches") else listOf("patches", "cheats")
            sources.forEach { source ->
                val picked = chosen.filter { it.source == source }
                val dir = if (source == "cheats") cheatsDir else patchesDir
                val file = File(dir, "$base.pnach")
                val isCheat = source == "cheats"
                if (picked.isEmpty()) {
                    // The user picked nothing from THIS category — leave it completely
                    // alone. (Previously this deleted the category's file and cleared
                    // its Enable list, so applying an online PATCH wiped the user's
                    // separately-imported CHEATS. Categories are now independent; to
                    // remove an item, uncheck it in "My local patches & cheats" or use
                    // Delete in the installed-file list.)
                    return@forEach
                }
                if (isCheat) {
                    anyCheatChosen = true
                    // Proper pnach-2.0: write the picked cheats with [labels] INTACT
                    // (a stable catalog) and enable them BY NAME via the [Cheats]
                    // Enable list, so per-cheat on/off persists in PCSX2-Android.ini.
                    file.writeText(buildString {
                        if (res.gametitle.isNotEmpty()) append("gametitle=").append(res.gametitle).append("\n\n")
                        picked.forEach { append(labelledCheatBody(it.body)).append("\n\n") }
                    })
                    NativeApp.setEnabledPatches(true,
                        res.entries.filter { it.source == "cheats" }.map { it.name }.toTypedArray(),
                        picked.map { it.name }.toTypedArray())
                } else {
                    // Patches keep the proven flatten-to-unlabelled activation.
                    file.writeText(buildString {
                        if (res.gametitle.isNotEmpty()) append("gametitle=").append(res.gametitle).append("\n\n")
                        picked.forEach { append(executablePnachBody(it.body)).append("\n\n") }
                    })
                }
            }
        }
        if (saved.isFailure) {
            pnachStatus = "Save failed: ${saved.exceptionOrNull()?.message ?: "unknown error"}"
            return
        }
        if (!state.value.enablePatches) apply(state.value.copy(enablePatches = true))
        NativeApp.setSetting("EmuCore", "EnablePatches", "bool", "true")
        val active = if (anyCheatChosen) activateCheatsAndReload() else {
            NativeApp.commitSettings()
            NativeApp.reloadPatches()
        }
        pnachStatus = when {
            chosen.isEmpty() -> "Cleared all patches for ${gid?.serial ?: "this game"}."
            Main.eState.value == EmuState.STOPPED ->
                "Saved ${chosen.size} item${if (chosen.size == 1) "" else "s"} for ${gid?.serial ?: "this game"}. Start the game to load them."
            else ->
                "Enabled ${chosen.size} item${if (chosen.size == 1) "" else "s"} ($active live). Restart the game to (re)load boot-time patches."
        }
        refresh()
    }

    // Open the per-cheat editor for an installed file: parse it into individual
    // cheats (both [Section] and //comment conventions), pre-checking those whose
    // patch= lines are currently active.
    fun openEditor(file: File) {
        val text = runCatching { file.readText() }.getOrNull()
        if (text == null) { pnachStatus = "Couldn't read ${file.name}."; return }
        val source = if (file.parentFile?.name == "cheats") "cheats" else "patches"
        val (gt, cheats) = PatchRepo.parseInstalled(text, source)
        if (cheats.isEmpty()) {
            pnachStatus = "${file.name} has no individual cheats to toggle."
            return
        }
        editSelected.clear()
        cheats.forEachIndexed { i, c -> editSelected[i] = c.enabled }
        editSession = EditSession(file, gt, cheats)
    }

    // Rewrite the file with the new per-cheat on/off states, then reload PNACH.
    fun applyEdit() {
        val sess = editSession ?: return
        editSession = null
        val isCheat = sess.file.parentFile?.name == "cheats"
        val paired = sess.cheats.mapIndexed { i, c -> c to (editSelected[i] ?: c.enabled) }
        // Cheats keep [labels] (gated by the [Cheats] Enable list); patches flatten (legacy).
        val saved = runCatching { sess.file.writeText(rebuildInstalledPnach(sess.gametitle, paired, keepLabels = isCheat)) }
        if (saved.isFailure) {
            pnachStatus = "Save failed: ${saved.exceptionOrNull()?.message ?: "unknown error"}"
            return
        }
        if (!state.value.enablePatches) apply(state.value.copy(enablePatches = true))
        NativeApp.setSetting("EmuCore", "EnablePatches", "bool", "true")
        if (isCheat) {
            // Persist per-cheat on/off BY NAME to the [Cheats] Enable list so it
            // survives reset (proper pnach-2.0); native gates activation on hardcore.
            NativeApp.setEnabledPatches(true,
                sess.cheats.map { it.name }.toTypedArray(),
                paired.filter { it.second }.map { it.first.name }.toTypedArray())
        }
        val active = if (isCheat) activateCheatsAndReload() else {
            NativeApp.commitSettings(); NativeApp.reloadPatches()
        }
        val onCount = paired.count { it.second }
        pnachStatus = when {
            Main.eState.value == EmuState.STOPPED ->
                "Saved ${sess.file.name}: $onCount cheat${if (onCount == 1) "" else "s"} on. Start the game to load."
            else ->
                "Saved ${sess.file.name}: $onCount on ($active live). Restart to (re)load boot-time patches."
        }
        refresh()
    }

    // Expand every installed .pnach (cheats + patches) into its individual entries
    // for the unified checkbox browser. Skips files with no togglable entries.
    fun collectLocalFiles(): List<LocalFileCheats> =
        pnachFiles.mapNotNull { file ->
            val text = runCatching { file.readText() }.getOrNull() ?: return@mapNotNull null
            val source = if (file.parentFile?.name == "cheats") "cheats" else "patches"
            val (gt, cheats) = PatchRepo.parseInstalled(text, source)
            if (cheats.isEmpty()) null else LocalFileCheats(file, source, gt, cheats)
        }

    fun openLocalBrowser() {
        val files = collectLocalFiles()
        if (files.isEmpty()) {
            pnachStatus = I18n.get("patches.status.noLocalEntries")
            return
        }
        localSelected.clear()
        files.forEachIndexed { fi, f -> f.cheats.forEachIndexed { ci, c -> localSelected["$fi:$ci"] = c.enabled } }
        localBrowse = files
    }

    // Write the browser's checkbox states back to each file (cheats keep [labels] +
    // the [Cheats] Enable list; patches flatten to auto-run), then reload PNACH.
    fun applyLocalBrowse() {
        val files = localBrowse ?: return
        localBrowse = null
        val allCheatNames = mutableListOf<String>()
        val enabledCheatNames = mutableListOf<String>()
        var anyCheat = false
        var onCount = 0
        var savedCount = 0
        files.forEachIndexed { fi, f ->
            val isCheat = f.source == "cheats"
            val paired = f.cheats.mapIndexed { ci, c -> c to (localSelected["$fi:$ci"] ?: c.enabled) }
            if (runCatching { f.file.writeText(rebuildInstalledPnach(f.gametitle, paired, keepLabels = isCheat)) }.isSuccess) savedCount++
            onCount += paired.count { it.second }
            if (isCheat) {
                anyCheat = true
                f.cheats.forEach { allCheatNames.add(it.name) }
                paired.filter { it.second }.forEach { enabledCheatNames.add(it.first.name) }
            }
        }
        if (!state.value.enablePatches) apply(state.value.copy(enablePatches = true))
        NativeApp.setSetting("EmuCore", "EnablePatches", "bool", "true")
        if (allCheatNames.isNotEmpty())
            NativeApp.setEnabledPatches(true, allCheatNames.toTypedArray(), enabledCheatNames.toTypedArray())
        val active = if (anyCheat) activateCheatsAndReload() else { NativeApp.commitSettings(); NativeApp.reloadPatches(); 0 }
        pnachStatus = when {
            Main.eState.value == EmuState.STOPPED ->
                "Saved: $onCount entr${if (onCount == 1) "y" else "ies"} on across $savedCount file${if (savedCount == 1) "" else "s"}. Start the game to load."
            else ->
                "Saved: $onCount on ($active live). Restart to (re)load boot-time patches."
        }
        refresh()
    }

    val importLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        val name = DocumentFile.fromSingleUri(context, uri)?.name
            ?.takeIf { it.isNotEmpty() }
            ?: "imported.pnach"
        val outName = importPnachFileName(name, activePnachGameId())
        val result = runCatching {
            val outFile = pnachTargetFile(cheatsDir, outName)
            val importedText = context.contentResolver.openInputStream(uri)?.use { ins ->
                ins.reader().readText()
            } ?: error("could not open selected file")
            outFile.writeText(manualPnachContents(name, importedText, activePnachGameId()))
            val activeCheats = activateCheatsAndReload()
            outFile.name to activeCheats
        }
        pnachStatus = result.fold(
            onSuccess = { (savedName, activeCheats) ->
                if (activePnachGameId() != null || loadablePnachName(savedName))
                    pnachResultMessage("Imported as", savedName, activeCheats)
                else
                    "Imported as $savedName, but it may need SERIAL_CRC naming to load."
            },
            onFailure = { "Import failed: ${it.message ?: "unknown error"}" },
        )
        refresh()
    }

    if (showManualDialog) {
        ManualPnachDialog(
            gameId = activeGameId,
            onDismiss = { showManualDialog = false },
            onSave = { title, body ->
                val result = runCatching {
                    val outFile = pnachTargetFile(cheatsDir, manualPnachFileName(title, activePnachGameId()))
                    outFile.writeText(manualPnachContents(title, body, activePnachGameId()))
                    val activeCheats = activateCheatsAndReload()
                    outFile.name to activeCheats
                }
                pnachStatus = result.fold(
                    onSuccess = { (savedName, activeCheats) ->
                        if (activePnachGameId() != null || loadablePnachName(savedName))
                            pnachResultMessage("Executed", savedName, activeCheats)
                        else
                            "Saved $savedName, but it may need SERIAL_CRC naming to load."
                    },
                    onFailure = { "Save failed: ${it.message ?: "unknown error"}" },
                )
                refresh()
                showManualDialog = false
            },
        )
    }

    if (showPatchWarning) {
        AlertDialog(
            onDismissRequest = { showPatchWarning = false },
            containerColor = Colors.surfaceColor,
            titleContentColor = Color.White,
            textContentColor = Color.White,
            title = { Text(str("patches.warning.title")) },
            text = {
                Text(
                    str("patches.warning.message"),
                    fontSize = 13.sp,
                )
            },
            confirmButton = {
                TextButton(onClick = { showPatchWarning = false; startBrowse() }) { Text(str("patches.action.yes")) }
            },
            dismissButton = {
                Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    TextButton(onClick = {
                        Main.prefs.edit().putBoolean("patchCodesWarnAck", true).apply()
                        showPatchWarning = false
                        startBrowse()
                    }) { Text(str("patches.action.dontAskAgain")) }
                    TextButton(onClick = { showPatchWarning = false }) { Text(str("patches.action.no")) }
                }
            },
        )
    }

    browseResult?.let { res ->
        AlertDialog(
            onDismissRequest = { browseResult = null },
            containerColor = Colors.surfaceColor,
            titleContentColor = Color.White,
            textContentColor = Color.White,
            title = {
                Text(
                    if (res.gametitle.isNotEmpty()) res.gametitle else str("patches.dialog.patchesAndCheats"),
                    fontSize = 15.sp,
                    fontWeight = FontWeight.Bold,
                )
            },
            text = {
                Column(modifier = Modifier.heightIn(max = 380.dp).verticalScroll(rememberScrollState())) {
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        TextButton(onClick = { res.entries.indices.forEach { selected[it] = true } }) { Text(str("patches.action.selectAll")) }
                        TextButton(onClick = { selected.clear() }) { Text(str("patches.action.none")) }
                    }
                    res.entries.forEachIndexed { i, e ->
                        val locked = hardcore && e.source == "cheats"
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .alpha(if (locked) 0.4f else 1f)
                                .clickable(enabled = !locked) { selected[i] = !(selected[i] ?: false) }
                                .padding(vertical = 2.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Checkbox(checked = selected[i] == true, enabled = !locked, onCheckedChange = { if (!locked) selected[i] = it })
                            Column(modifier = Modifier.weight(1f).padding(start = 4.dp)) {
                                Text(e.name, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = Color.White)
                                Text(
                                    listOfNotNull(e.description.takeIf { it.isNotEmpty() }, "[${e.source}]").joinToString("  •  "),
                                    fontSize = 10.sp,
                                    color = Color(0xFF9A9A9A),
                                    maxLines = 2,
                                    overflow = TextOverflow.Ellipsis,
                                )
                            }
                        }
                    }
                }
            },
            confirmButton = { TextButton(onClick = { applySelected() }) { Text(str("patches.action.apply")) } },
            dismissButton = { TextButton(onClick = { browseResult = null }) { Text(str("patches.action.cancel")) } },
        )
    }

    localBrowse?.let { files ->
        AlertDialog(
            onDismissRequest = { localBrowse = null },
            containerColor = Colors.surfaceColor,
            titleContentColor = Color.White,
            textContentColor = Color.White,
            title = { Text(str("patches.dialog.myLocal"), fontSize = 15.sp, fontWeight = FontWeight.Bold) },
            text = {
                Column(modifier = Modifier.heightIn(max = 380.dp).verticalScroll(rememberScrollState())) {
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        TextButton(onClick = {
                            files.forEachIndexed { fi, f ->
                                f.cheats.indices.forEach { ci ->
                                    if (!(hardcore && f.source == "cheats")) localSelected["$fi:$ci"] = true
                                }
                            }
                        }) { Text(str("patches.action.selectAll")) }
                        TextButton(onClick = {
                            files.forEachIndexed { fi, f -> f.cheats.indices.forEach { ci -> localSelected["$fi:$ci"] = false } }
                        }) { Text(str("patches.action.none")) }
                    }
                    files.forEachIndexed { fi, f ->
                        Text(
                            "${f.file.name}  [${f.source}]",
                            fontSize = 10.sp,
                            color = Color(0xFF8CA6C8),
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                            modifier = Modifier.padding(top = 8.dp, bottom = 2.dp),
                        )
                        f.cheats.forEachIndexed { ci, c ->
                            val key = "$fi:$ci"
                            val locked = hardcore && f.source == "cheats"
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .alpha(if (locked) 0.4f else 1f)
                                    .clickable(enabled = !locked) { localSelected[key] = !(localSelected[key] ?: false) }
                                    .padding(vertical = 2.dp),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                Checkbox(checked = localSelected[key] == true, enabled = !locked, onCheckedChange = { if (!locked) localSelected[key] = it })
                                Column(modifier = Modifier.weight(1f).padding(start = 4.dp)) {
                                    Text(c.name, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = Color.White)
                                    if (c.description.isNotEmpty())
                                        Text(
                                            c.description,
                                            fontSize = 10.sp,
                                            color = Color(0xFF9A9A9A),
                                            maxLines = 2,
                                            overflow = TextOverflow.Ellipsis,
                                        )
                                }
                            }
                        }
                    }
                }
            },
            confirmButton = { TextButton(onClick = { applyLocalBrowse() }) { Text(str("patches.action.apply")) } },
            dismissButton = { TextButton(onClick = { localBrowse = null }) { Text(str("patches.action.cancel")) } },
        )
    }

    editSession?.let { sess ->
        AlertDialog(
            onDismissRequest = { editSession = null },
            containerColor = Colors.surfaceColor,
            titleContentColor = Color.White,
            textContentColor = Color.White,
            title = {
                Text(
                    "Edit cheats — ${sess.file.name}",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            },
            text = {
                Column(modifier = Modifier.heightIn(max = 380.dp).verticalScroll(rememberScrollState())) {
                    Text(
                        str("patches.editHint"),
                        fontSize = 10.sp,
                        color = Color(0xFF9A9A9A),
                        modifier = Modifier.padding(bottom = 4.dp),
                    )
                    Row(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                        TextButton(onClick = { sess.cheats.indices.forEach { editSelected[it] = true } }) { Text(str("patches.action.allOn")) }
                        TextButton(onClick = { sess.cheats.indices.forEach { editSelected[it] = false } }) { Text(str("patches.action.allOff")) }
                    }
                    sess.cheats.forEachIndexed { i, c ->
                        Row(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { editSelected[i] = !(editSelected[i] ?: c.enabled) }
                                .padding(vertical = 2.dp),
                            verticalAlignment = Alignment.CenterVertically,
                        ) {
                            Checkbox(checked = editSelected[i] ?: c.enabled, onCheckedChange = { editSelected[i] = it })
                            Column(modifier = Modifier.weight(1f).padding(start = 4.dp)) {
                                Text(c.name, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, color = Color.White)
                                if (c.description.isNotEmpty())
                                    Text(
                                        c.description,
                                        fontSize = 10.sp,
                                        color = Color(0xFF9A9A9A),
                                        maxLines = 2,
                                        overflow = TextOverflow.Ellipsis,
                                    )
                            }
                        }
                    }
                }
            },
            confirmButton = { TextButton(onClick = { applyEdit() }) { Text(str("patches.action.save")) } },
            dismissButton = { TextButton(onClick = { editSession = null }) { Text(str("patches.action.cancel")) } },
        )
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .verticalScroll(scroll)
            .verticalScrollbar(scroll),
    ) {
        Text(
            str("patches.applyAtBoot"),
            color = Color(0xFFB0B0B0),
            fontSize = 11.sp,
            modifier = Modifier.padding(bottom = 8.dp),
        )
        ToggleRow(str("patches.enablePatches.label"), s.enablePatches) { apply(s.copy(enablePatches = it)) }
        SettingsDivider()
        ToggleRow(str("patches.widescreen.label"), s.enableWideScreenPatches) {
            apply(s.copy(enableWideScreenPatches = it))
        }
        SettingsDivider()
        ToggleRow(str("patches.noInterlacing.label"), s.enableNoInterlacingPatches) {
            apply(s.copy(enableNoInterlacingPatches = it))
        }
        SettingsDivider()
        if (hardcore) {
            Box(Modifier.fillMaxWidth().alpha(0.4f)) {
                ToggleRow(str("patches.cheats.labelHardcore"), false) { /* locked */ }
            }
        } else {
            ToggleRow(str("patches.cheats.label"), s.enableCheats) { apply(s.copy(enableCheats = it)) }
        }
        SettingsDivider()
        ToggleRow(str("patches.hostFs.label"), s.hostFs) { apply(s.copy(hostFs = it)) }
        Text(
            str("patches.hostFs.description"),
            color = Color(0xFFB0B0B0),
            fontSize = 11.sp,
            modifier = Modifier.padding(top = 2.dp, bottom = 8.dp),
        )
        SettingsDivider()

        // ---- PNACH importer ----
        Text(
            str("patches.installedHeader"),
            color = Color.White,
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(top = 6.dp, bottom = 2.dp),
        )
        Text(
            when {
                activeGameId != null -> "Active game: ${activeGameId.serial} / CRC ${activeGameId.crc}"
                libraryGame?.serial?.isNotBlank() == true -> "Selected game: ${libraryGame.serial} — browse its patches below."
                else -> str("patches.startOrLongPress")
            },
            color = Color(0xFF8C8C8C),
            fontSize = 10.sp,
            modifier = Modifier.padding(bottom = 4.dp),
        )
        Text(
            if (hardcore) str("patches.hardcoreNoticeCheatsDisabled")
            else str("patches.pasteImportHint"),
            color = Color(0xFF8C8C8C),
            fontSize = 10.sp,
            modifier = Modifier.padding(bottom = 4.dp),
        )
        // Online fetch from the PCSX2 patch database (mirrors the GPU driver
        // downloader). Gated by the AetherSX2 patch-codes warning.
        Box(
            Modifier
                .fillMaxWidth()
                .height(36.dp)
                .background(rowAura())
                .controllerFocusable(
                    controllerId = "patch:browse",
                    onConfirm = { if (!downloading) onDownloadClick() },
                )
                .clickable(enabled = !downloading) { onDownloadClick() }
                .padding(horizontal = 8.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Text(
                if (downloading) str("patches.button.fetching") else str("patches.button.browseOnline"),
                color = Colors.pasx2_blue,
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
            )
        }
        Spacer(Modifier.height(6.dp))
        // Browse the user's OWN cheat files (imported/copied into the cheats
        // folder) and toggle each cheat on/off — the counterpart to the online
        // browser above. Routes straight into the per-cheat editor.
        Box(
            Modifier
                .fillMaxWidth()
                .height(36.dp)
                .background(rowAura())
                .controllerFocusable(
                    controllerId = "patch:local",
                    onConfirm = { openLocalBrowser() },
                )
                .clickable { openLocalBrowser() }
                .padding(horizontal = 8.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Text(
                str("patches.button.myLocalToggle"),
                color = Colors.pasx2_blue,
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
            )
        }
        Spacer(Modifier.height(6.dp))
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Box(
                Modifier
                    .weight(1f)
                    .height(36.dp)
                    .alpha(if (hardcore) 0.4f else 1f)
                    .background(rowAura())
                    .controllerFocusable(
                        controllerId = "patch:import",
                        onConfirm = { if (!hardcore) importLauncher.launch(arrayOf("*/*")) },
                    )
                    .clickable(enabled = !hardcore) { importLauncher.launch(arrayOf("*/*")) }
                    .padding(horizontal = 8.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                Text(str("patches.button.importPnach"), color = Colors.pasx2_blue, fontSize = 13.sp, fontWeight = FontWeight.Bold)
            }
            Box(
                Modifier
                    .weight(1f)
                    .height(36.dp)
                    .alpha(if (hardcore) 0.4f else 1f)
                    .background(rowAura())
                    .controllerFocusable(
                        controllerId = "patch:enter",
                        onConfirm = { if (!hardcore) showManualDialog = true },
                    )
                    .clickable(enabled = !hardcore) { showManualDialog = true }
                    .padding(horizontal = 8.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                Text(str("patches.button.enterCodes"), color = Colors.pasx2_blue, fontSize = 13.sp, fontWeight = FontWeight.Bold)
            }
        }
        if (pnachStatus.isNotEmpty()) {
            Text(
                pnachStatus,
                color = Color(0xFFB0B0B0),
                fontSize = 10.sp,
                modifier = Modifier.padding(vertical = 4.dp, horizontal = 4.dp),
            )
        }
        if (pnachFiles.isEmpty()) {
            Text(
                str("patches.noFilesInstalled"),
                color = Color(0xFF8C8C8C),
                fontSize = 11.sp,
                modifier = Modifier.padding(vertical = 4.dp, horizontal = 4.dp),
            )
        } else {
            Text(
                str("patches.installedFilesHint"),
                color = Color(0xFF9C9C9C),
                fontSize = 10.sp,
                modifier = Modifier.padding(horizontal = 4.dp, vertical = 3.dp),
            )
            pnachFiles.forEach { file ->
                val kind = if (file.parentFile?.name == "cheats") "cheat" else "patch"
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(30.dp)
                        .padding(horizontal = 4.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        "[$kind] ${file.name}",
                        color = Color(0xFFCCCCCC),
                        fontSize = 11.sp,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    Text(
                        str("patches.action.edit"),
                        color = Colors.pasx2_blue,
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier
                            .clickable { openEditor(file) }
                            .padding(start = 8.dp),
                    )
                    Text(
                        str("patches.action.delete"),
                        color = Color(0xFFFF6B6B),
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier
                            .clickable {
                                runCatching { file.delete() }
                                refresh()
                            }
                            .padding(start = 8.dp),
                    )
                }
            }
        }
    }
}

@Composable
private fun ManualPnachDialog(
    gameId: PnachGameId?,
    onDismiss: () -> Unit,
    onSave: (title: String, body: String) -> Unit,
) {
    var title by remember { mutableStateOf("") }
    var body by remember { mutableStateOf("") }
    fun execute() {
        if (body.isNotBlank())
            onSave(title, body)
    }
    val tfColors = TextFieldDefaults.colors(
        focusedTextColor = Color.White,
        unfocusedTextColor = Color.White,
        focusedContainerColor = Color(0xFF111111),
        unfocusedContainerColor = Color(0xFF111111),
        disabledContainerColor = Color(0xFF111111),
        focusedLabelColor = Colors.pasx2_blue,
        unfocusedLabelColor = Color(0xFFAAAAAA),
        focusedIndicatorColor = Colors.pasx2_blue,
        unfocusedIndicatorColor = Color(0xFF555555),
        cursorColor = Colors.pasx2_blue,
    )

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = Color(0xFF151515),
        title = {
            Text(str("patches.dialog.enterCodesTitle"), color = Color.White, fontWeight = FontWeight.Bold)
        },
        text = {
            Column {
                Text(
                    gameId?.let { "Saving for ${it.serial} / ${it.crc}" }
                        ?: str("patches.dialog.noActiveCrc"),
                    color = Color(0xFFAAAAAA),
                    fontSize = 11.sp,
                    modifier = Modifier.padding(bottom = 6.dp),
                )
                OutlinedTextField(
                    value = title,
                    onValueChange = { title = it },
                    label = { Text(str("patches.field.name")) },
                    singleLine = true,
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Next),
                    colors = tfColors,
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(8.dp))
                OutlinedTextField(
                    value = body,
                    onValueChange = { body = it },
                    label = { Text(str("patches.field.pnachLines")) },
                    minLines = 6,
                    maxLines = 10,
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
                    keyboardActions = KeyboardActions(onDone = { execute() }),
                    colors = tfColors,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
        },
        confirmButton = {
            TextButton(
                enabled = body.isNotBlank(),
                onClick = { execute() },
            ) {
                Text(str("patches.action.execute"), color = if (body.isNotBlank()) Colors.pasx2_blue else Color(0xFF777777))
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text(str("patches.action.cancelMixed"), color = Color(0xFFCCCCCC))
            }
        },
    )
}
