package com.armsx2.ui.bios

import android.app.Application
import android.net.Uri
import android.os.ParcelFileDescriptor
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import com.armsx2.BiosInfo
import com.armsx2.config.ConfigStore
import com.armsx2.config.SettingsScope
import com.armsx2.runtime.MainActivityRuntime
import java.io.File
import kr.co.iefriends.pcsx2.NativeApp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class InstalledBios(
    val file: File,
    val info: BiosInfo,
    val selected: Boolean,
)

data class BiosManagerUiState(
    val items: List<InstalledBios> = emptyList(),
    val busy: Boolean = false,
    val error: String? = null,
    // Per-game BIOS (mirrors the per-game memory card): the target game's per-game key
    // (settingsKey — serial for discs, filename stem for serial-less), or null when there's
    // no game context (which hides the per-game controls), and the BIOS filename it's
    // currently pinned to — null means it inherits the global BIOS.
    val gameKey: String? = null,
    val perGameBios: String? = null,
)

class BiosManagerViewModel(application: Application) : AndroidViewModel(application) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    var state = androidx.compose.runtime.mutableStateOf(BiosManagerUiState())
        private set

    // Per-game BIOS target: set from the library long-press (that game's settingsKey) so the
    // picker keys on it WITHOUT the game being loaded. Null = fall back to the loaded game.
    private var gameContextKey: String? = null

    /** Point the per-game controls at [key] (a game's settingsKey), or null to fall back to the
     *  currently loaded game. Called by the screen from its optional `game` argument. */
    fun setGameContext(key: String?) {
        gameContextKey = key
        refresh()
    }

    fun refresh() {
        scope.launch {
            state.value = state.value.copy(busy = true, error = null)
            val selectedPath = MainActivityRuntime.bios.value
            val key = gameContextKey?.takeIf { it.isNotBlank() }
                ?: MainActivityRuntime.currentGame.value?.settingsKey?.takeIf { it.isNotBlank() }
            val result = withContext(Dispatchers.IO) {
                MainActivityRuntime.internalBiosDir(getApplication()).apply { mkdirs() }
                    .listFiles()
                    .orEmpty()
                    .filter(File::isFile)
                    .mapNotNull { file -> probe(file)?.let { InstalledBios(file, it, file.absolutePath == selectedPath) } }
                    .sortedWith(compareByDescending<InstalledBios> { it.selected }.thenBy { it.file.name.lowercase() })
            }
            val perGame = key?.let {
                runCatching { ConfigStore.resolveForGame(it).biosFilename.takeIf { f -> f.isNotBlank() } }.getOrNull()
            }
            state.value = state.value.copy(items = result, busy = false, gameKey = key, perGameBios = perGame)
        }
    }

    /** Pin [item] as THIS game's BIOS (per-game override in the ConfigStore Game tier),
     *  mirroring the per-game memory card. applyRendererPrefs applies it at the next boot,
     *  so restart the game to take effect. No-op when no game is loaded. */
    fun assignToGame(item: InstalledBios) {
        val key = state.value.gameKey ?: return
        val resolved = ConfigStore.resolveForGame(key)
        val ok = runCatching {
            ConfigStore.save(SettingsScope.Game, key, resolved.copy(biosFilename = item.file.name))
        }.isSuccess
        if (!ok) state.value = state.value.copy(error = "Unable to set a per-game BIOS.")
        refresh()
    }

    /** Clear this game's per-game BIOS so it falls back to the global selection. */
    fun clearGameBios() {
        val key = state.value.gameKey ?: return
        val resolved = ConfigStore.resolveForGame(key)
        runCatching { ConfigStore.save(SettingsScope.Game, key, resolved.copy(biosFilename = "")) }
        refresh()
    }

    fun import(uri: Uri) {
        scope.launch {
            state.value = state.value.copy(busy = true, error = null)
            val result = withContext(Dispatchers.IO) { importFile(uri) }
            result.onSuccess { file -> select(file) }
                .onFailure { state.value = state.value.copy(error = it.message ?: "BIOS import failed.") }
            refresh()
        }
    }

    /**
     * Item 7: import every valid PS2 BIOS from a chosen folder in one shot (refresh parity — the
     * old UI let you point at a BIOS folder and pick between them, not import one file at a time).
     * Non-BIOS files are silently skipped (importFile validates via getBiosInfoFromFd); the existing
     * list + "Use selected" UI then lets the user choose between the imported BIOSes.
     */
    fun importFolder(treeUri: Uri) {
        scope.launch {
            state.value = state.value.copy(busy = true, error = null)
            val imported = withContext(Dispatchers.IO) {
                val context = getApplication<Application>()
                var count = 0
                DocumentFile.fromTreeUri(context, treeUri)?.listFiles()?.forEach { child ->
                    if (child.isFile && importFile(child.uri).isSuccess) count++
                }
                count
            }
            if (imported == 0) {
                state.value = state.value.copy(busy = false, error = "No valid PlayStation 2 BIOS found in that folder.")
            } else {
                refresh()
            }
        }
    }

    fun select(file: File) {
        MainActivityRuntime.bios.value = file.absolutePath
        MainActivityRuntime.prefs.edit().putString("bios", file.absolutePath).apply()
        refresh()
    }

    fun delete(item: InstalledBios) {
        if (item.selected) {
            state.value = state.value.copy(error = "Select another BIOS before deleting the active one.")
            return
        }
        val deleted = runCatching { item.file.delete() }.getOrDefault(false)
        if (!deleted) {
            state.value = state.value.copy(error = "Unable to delete ${item.file.name}.")
            return
        }
        refresh()
    }

    fun dismissError() {
        state.value = state.value.copy(error = null)
    }

    private fun importFile(uri: Uri): Result<File> = runCatching {
        val context = getApplication<Application>()
        val name = DocumentFile.fromSingleUri(context, uri)?.name?.takeIf(String::isNotBlank) ?: "bios.bin"
        val descriptor = context.contentResolver.openFileDescriptor(uri, "r") ?: error("Unable to open the file.")
        NativeApp.getBiosInfoFromFd(descriptor.detachFd()) ?: error("Not a valid PlayStation 2 BIOS.")
        val directory = MainActivityRuntime.internalBiosDir(context).apply { mkdirs() }
        val safeName = name.replace(Regex("[^A-Za-z0-9._-]"), "_")
        val target = uniqueFile(directory, safeName)
        context.contentResolver.openInputStream(uri)?.use { input ->
            target.outputStream().use(input::copyTo)
        } ?: error("Unable to read the file.")
        if (target.length() == 0L) {
            target.delete()
            error("The BIOS file is empty.")
        }
        target
    }

    private fun uniqueFile(directory: File, requested: String): File {
        val first = File(directory, requested)
        if (!first.exists()) return first
        val name = requested.substringBeforeLast('.', requested)
        val extension = requested.substringAfterLast('.', "").let { if (it.isBlank()) "" else ".$it" }
        var index = 2
        while (true) {
            val candidate = File(directory, "$name ($index)$extension")
            if (!candidate.exists()) return candidate
            index++
        }
    }

    private fun probe(file: File): BiosInfo? = runCatching {
        val descriptor = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
        NativeApp.getBiosInfoFromFd(descriptor.detachFd())
    }.getOrNull()

    override fun onCleared() {
        scope.cancel()
        super.onCleared()
    }
}
