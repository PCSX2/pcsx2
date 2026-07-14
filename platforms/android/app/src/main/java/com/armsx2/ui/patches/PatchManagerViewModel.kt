package com.armsx2.ui.patches

import android.app.Application
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.armsx2.GameInfo
import com.armsx2.PatchRepo
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.config.ConfigStore
import com.armsx2.config.Settings
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp

data class PatchManagerUiState(
    val settings: Settings = Settings(),
    val files: List<File> = emptyList(),
    // Online patch/cheat browser (backed by PatchRepo — GitHub PNACH repos by serial).
    val onlineLoading: Boolean = false,
    val onlineTitle: String = "",
    val onlineEntries: List<PatchRepo.Entry> = emptyList(),
    val onlineSelected: Set<String> = emptySet(),
    val onlineSerial: String = "",
    val onlineCrc: String = "",
    // Local per-cheat manager: the expanded file's parsed cheats (name/enabled).
    val localExpandedPath: String? = null,
    val localCheats: List<PatchRepo.LocalCheat> = emptyList(),
    val message: String? = null,
    val error: String? = null,
)

class PatchManagerViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(PatchManagerUiState())
        private set

    fun refresh() {
        val files = patchDirectories().flatMap { directory ->
            if (!directory.isDirectory) emptyList() else directory.walkTopDown().filter { it.isFile && it.extension.equals("pnach", true) }.toList()
        }.distinctBy { it.absolutePath }.sortedBy { it.name.lowercase() }
        state.value = state.value.copy(settings = ConfigStore.loadGlobal(), files = files)
        // Reflect every file's on-disk enabled cheats into the native Enable list so
        // labelled cheats apply even for imported/pre-enabled files the user never
        // toggled in-app (see syncAllEnableLists / pushEnableList).
        syncAllEnableLists(files)
    }

    fun update(transform: (Settings) -> Settings) {
        val updated = transform(state.value.settings)
        ConfigStore.saveGlobal(updated)
        if (MainActivityRuntime.nativeReady.value) runCatching { updated.applyTo() }
        state.value = state.value.copy(settings = updated)
    }

    fun import(uri: Uri) {
        val context = getApplication<Application>()
        val original = DocumentFile.fromSingleUri(context, uri)?.name?.takeIf(String::isNotBlank) ?: "imported.pnach"
        val requested = if (original.endsWith(".pnach", true)) original else "$original.pnach"
        val directory = patchDirectories().first().apply { mkdirs() }
        val target = uniqueFile(directory, requested)
        val success = runCatching {
            context.contentResolver.openInputStream(uri)?.use { input -> target.outputStream().use(input::copyTo) }
                ?: error("Unable to read file")
            target.length() > 0L
        }.getOrDefault(false)
        if (!success) target.delete()
        state.value = if (success) state.value.copy(message = "Imported ${target.name}.") else state.value.copy(error = "Patch import failed.")
        if (success) {
            // Register the imported file's enabled (labelled) cheats in the native Enable
            // list BEFORE reloading, or the first reload skips them (see syncEnableListForFile).
            syncEnableListForFile(target)
            reloadCore()
        }
        refresh()
    }

    fun delete(file: File) {
        val success = runCatching { file.delete() }.getOrDefault(false)
        state.value = if (success) state.value.copy(message = "Deleted ${file.name}.") else state.value.copy(error = "Unable to delete ${file.name}.")
        if (success) reloadCore()
        refresh()
    }

    // -- Online browser: fetch PNACH patch/cheat entries for the current game from
    //    the community GitHub repos (all logic already in PatchRepo), let the user
    //    tick the ones they want, then assemble + install a .pnach. --

    fun fetchOnline(game: GameInfo?) {
        val serial = game?.serial?.takeIf { it.isNotBlank() }
            ?: runCatching { NativeApp.getGameSerial() }.getOrNull()?.takeIf { it.isNotBlank() }
        if (serial == null) {
            state.value = state.value.copy(error = "No game serial to look up patches for. Open this from a game.")
            return
        }
        state.value = state.value.copy(onlineLoading = true, error = null, onlineEntries = emptyList())
        viewModelScope.launch {
            val result = withContext(Dispatchers.IO) {
                val crc = runCatching { NativeApp.getGameCRC() }.getOrNull()?.takeIf { it.length == 8 }
                if (crc != null) PatchRepo.fetchForGame(serial, crc) else PatchRepo.fetchForSerial(serial)
            }
            state.value = state.value.copy(
                onlineLoading = false,
                onlineTitle = result.gametitle,
                onlineEntries = result.entries,
                onlineSelected = emptySet(),
                onlineSerial = result.serial.ifBlank { serial },
                onlineCrc = result.crc,
                error = result.error,
            )
        }
    }

    fun toggleOnline(name: String) {
        val selected = state.value.onlineSelected.toMutableSet()
        if (!selected.add(name)) selected.remove(name)
        state.value = state.value.copy(onlineSelected = selected)
    }

    fun installSelected() {
        val snapshot = state.value
        val chosen = snapshot.onlineEntries.filter { it.name in snapshot.onlineSelected }
        if (chosen.isEmpty()) {
            state.value = snapshot.copy(error = "Select at least one patch or cheat first.")
            return
        }
        viewModelScope.launch {
            val ok = withContext(Dispatchers.IO) {
                runCatching {
                    val pnach = PatchRepo.buildPnach(snapshot.onlineTitle, chosen)
                    val dir = patchDirectories().first().apply { mkdirs() }
                    val fileName = if (snapshot.onlineCrc.isNotBlank()) {
                        "${snapshot.onlineSerial}_${snapshot.onlineCrc}.pnach"
                    } else {
                        "${snapshot.onlineSerial}.pnach"
                    }
                    File(dir, fileName).writeText(pnach)
                    true
                }.getOrDefault(false)
            }
            if (ok) {
                update { it.copy(enableCheats = true) }
                // Freshly-installed cheats are written as labelled groups; add their names
                // to the [Cheats] Enable list or EnablePatches skips them despite
                // EnableCheats being on (see pushEnableList / toggleLocalCheat). Installed
                // into the cheats folder, so the Cheats section.
                val names = chosen.mapNotNull { it.name.takeIf(String::isNotBlank) }.distinct().toTypedArray()
                if (names.isNotEmpty()) runCatching { NativeApp.setEnabledPatches(true, names, names) }
                reloadCore()
                state.value = state.value.copy(
                    message = "Installed ${chosen.size} item(s) for ${snapshot.onlineSerial}. Restart the game if it's running.",
                    onlineSelected = emptySet(),
                )
            } else {
                state.value = state.value.copy(error = "Couldn't install the selected patches.")
            }
            refresh()
        }
    }

    // -- Local manager: expand an installed .pnach to reveal its individual cheats and
    //    toggle each one. A toggle rewrites just that cheat's `patch=` lines on disk
    //    (comment out to disable, uncomment to enable), then asks the core to reload.
    //    Rewriting the file — rather than a runtime enable-list call keyed to the loaded
    //    game — keeps the change persistent AND avoids clobbering a different game's
    //    active cheats when the browsed file isn't the one currently running. --

    fun expandLocal(file: File) {
        if (state.value.localExpandedPath == file.absolutePath) {
            state.value = state.value.copy(localExpandedPath = null, localCheats = emptyList())
            return
        }
        // Show the row immediately; fill in cheats once parsed.
        state.value = state.value.copy(localExpandedPath = file.absolutePath, localCheats = emptyList())
        viewModelScope.launch {
            val cheats = withContext(Dispatchers.IO) {
                runCatching {
                    val source = file.parentFile?.name ?: "cheats"
                    PatchRepo.parseInstalled(file.readText(), source).second
                }.getOrDefault(emptyList())
            }
            // Ignore if the user collapsed/switched files while we were parsing.
            if (state.value.localExpandedPath == file.absolutePath) {
                state.value = state.value.copy(localCheats = cheats)
            }
        }
    }

    fun toggleLocalCheat(name: String) {
        val path = state.value.localExpandedPath ?: return
        val target = state.value.localCheats.firstOrNull { it.name == name } ?: return
        val nowEnabled = !target.enabled
        val newBody = setBodyEnabled(target.body, nowEnabled)
        // Optimistic UI — flip the switch AND advance the in-memory body so a second
        // toggle of the same cheat still finds its (now-rewritten) block on disk.
        state.value = state.value.copy(
            localCheats = state.value.localCheats.map { if (it.name == name) it.copy(enabled = nowEnabled, body = newBody) else it },
        )
        if (newBody == target.body) return // no patch= lines changed; nothing to write
        viewModelScope.launch {
            val ok = withContext(Dispatchers.IO) {
                runCatching {
                    val file = File(path)
                    val original = file.readText()
                    val updated = original.replaceFirst(target.body, newBody)
                    if (updated == original) return@runCatching false // body not found verbatim
                    file.writeText(updated)
                    true
                }.getOrDefault(false)
            }
            if (ok) {
                // PCSX2 only applies a LABELLED patch group ([Name]) whose name is in the
                // [Cheats]/[Patches] "Enable" list (Patch.cpp::EnablePatches auto-enables
                // ONLY unlabelled groups). Uncommenting the patch= lines alone therefore
                // does nothing for the common case of named community cheats. Mirror this
                // file's per-cheat enabled state into that Enable list so labelled cheats
                // actually take effect. This enable-list write was lost in the monorepo UI
                // migration, which kept only the body-comment rewrite — the cause of the
                // "cheats don't work" regression. setEnabledPatches drops only THIS file's
                // names before re-adding the enabled subset, so other games' active cheats
                // (different names) are preserved.
                pushEnableList(path)
                reloadCore()
            } else {
                // Revert both the flip and the body — the file wasn't rewritten.
                state.value = state.value.copy(
                    localCheats = state.value.localCheats.map { if (it.name == name) it.copy(enabled = target.enabled, body = target.body) else it },
                    error = "Couldn't update ${File(path).name} (unusual formatting). Edit it as text instead.",
                )
            }
        }
    }

    /** Comment out (disable) or uncomment (enable) every `patch=` line in a cheat block. */
    private fun setBodyEnabled(body: String, enable: Boolean): String =
        body.lines().joinToString("\n") { line ->
            if (!PatchRepo.isPatchCommand(line)) return@joinToString line
            val lead = line.takeWhile { it == ' ' || it == '\t' }
            val bare = line.substring(lead.length).trimStart('/').trimStart()
            if (enable) lead + bare else "$lead//$bare"
        }

    fun dismissMessage() {
        state.value = state.value.copy(message = null, error = null)
    }

    private fun patchDirectories(): List<File> {
        val root = File(MainActivityRuntime.assetCopyRoot(getApplication()))
        return listOf(File(root, "cheats"), File(root, "patches"), File(root, "cheats_ws"))
    }

    private fun uniqueFile(directory: File, requested: String): File {
        val base = requested.substringBeforeLast('.', requested)
        val extension = requested.substringAfterLast('.', "").let { if (it.isBlank()) "" else ".$it" }
        var file = File(directory, requested)
        var index = 2
        while (file.exists()) file = File(directory, "$base-$index$extension").also { index++ }
        return file
    }

    private fun reloadCore() {
        if (!MainActivityRuntime.nativeReady.value) return
        kotlin.concurrent.thread(name = "armsx2-patch-reload") {
            runCatching { NativeApp.reloadPatches() }
        }
    }

    /** Mirror the currently-expanded file's per-cheat enabled state into the native
     *  [Cheats]/[Patches] "Enable" list so LABELLED groups apply (Patch.cpp only
     *  auto-enables unlabelled groups). Files in the cheats folder use the [Cheats]
     *  section; patches/widescreen use [Patches]. Only this file's names are dropped
     *  then re-added, so a different game's enabled cheats are left intact. */
    private fun pushEnableList(path: String) {
        if (!MainActivityRuntime.nativeReady.value) return
        val cheatsSection = File(path).parentFile?.name == "cheats"
        val all = state.value.localCheats.mapNotNull { it.name.takeIf(String::isNotBlank) }.distinct().toTypedArray()
        if (all.isEmpty()) return
        val enabled = state.value.localCheats.filter { it.enabled }
            .mapNotNull { it.name.takeIf(String::isNotBlank) }.distinct().toTypedArray()
        runCatching { NativeApp.setEnabledPatches(cheatsSection, all, enabled) }
    }

    /** Reflect one file's on-disk body state into the native Enable list (parses it).
     *  Cheats folder → [Cheats] section, else [Patches]. Only this file's names are
     *  dropped then re-added, so other games' enabled cheats are preserved. Synchronous
     *  so callers can sequence it before reloadPatches. */
    private fun syncEnableListForFile(file: File) {
        if (!MainActivityRuntime.nativeReady.value) return
        val cheatsSection = file.parentFile?.name == "cheats"
        val parsed = runCatching {
            PatchRepo.parseInstalled(file.readText(), file.parentFile?.name ?: "cheats").second
        }.getOrDefault(emptyList())
        val all = parsed.mapNotNull { it.name.takeIf(String::isNotBlank) }.distinct().toTypedArray()
        if (all.isEmpty()) return
        val enabled = parsed.filter { it.enabled }
            .mapNotNull { it.name.takeIf(String::isNotBlank) }.distinct().toTypedArray()
        runCatching { NativeApp.setEnabledPatches(cheatsSection, all, enabled) }
    }

    /** Reflect every installed file's on-disk body state into the native Enable lists
     *  (off-thread — parses each pnach). The file body stays the persistent source of
     *  truth; this reconciles the runtime list PCSX2 requires for labelled groups so
     *  imported/pre-enabled cheats apply without an in-app toggle. */
    private fun syncAllEnableLists(files: List<File>) {
        if (!MainActivityRuntime.nativeReady.value || files.isEmpty()) return
        kotlin.concurrent.thread(name = "armsx2-cheat-enable-sync") {
            runCatching { for (file in files) syncEnableListForFile(file) }
        }
    }
}
