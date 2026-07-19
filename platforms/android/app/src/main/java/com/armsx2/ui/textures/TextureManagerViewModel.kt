package com.armsx2.ui.textures

import android.app.Application
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.config.ConfigStore
import com.armsx2.config.Settings
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp

data class TexturePackItem(val serial: String, val directory: File, val fileCount: Int, val size: Long)

data class TextureManagerUiState(
    val settings: Settings = Settings(),
    val packs: List<TexturePackItem> = emptyList(),
    val activeSerial: String? = null,
    val busy: Boolean = false,
    val progress: Int = 0,
    val message: String? = null,
    val error: String? = null,
)

class TextureManagerViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(TextureManagerUiState())
        private set

    fun refresh() {
        val root = textureRoot().apply { mkdirs() }
        val packs = root.listFiles().orEmpty().filter(File::isDirectory).mapNotNull { serialDir ->
            val replacements = File(serialDir, "replacements")
            if (!replacements.isDirectory) return@mapNotNull null
            val files = replacements.walkTopDown().filter(File::isFile).toList()
            TexturePackItem(serialDir.name, replacements, files.size, files.sumOf(File::length))
        }.sortedBy { it.serial }
        state.value = state.value.copy(
            settings = ConfigStore.loadGlobal(),
            packs = packs,
            activeSerial = runtimeSerial(),
        )
    }

    /**
     * The serial the CORE will look under — VMManager::GetDiscSerial(), the same value
     * GSTextureReplacements keys its `textures/<serial>/replacements` path off.
     *
     * This is NOT always the library's serial. Booting a raw .ELF (Persona/Aemulus mod
     * setups do this) takes VMManager's elf-override branch, where the serial becomes
     * Path::GetFileTitle(elf) — e.g. "PERSONA 3 FES" for PERSONA 3 FES.ELF — because with
     * no disc mounted there is no real serial to read. Importing under the library serial
     * ("SLUS-21621") then writes to a folder the core never scans, and the pack silently
     * does nothing. Prefer the running core's answer; fall back to the library entry when
     * nothing is running.
     */
    private fun runtimeSerial(): String? {
        if (MainActivityRuntime.nativeReady.value) {
            val live = runCatching { NativeApp.getGameSerial() }.getOrNull()
            if (!live.isNullOrBlank()) return live
        }
        return MainActivityRuntime.currentGame.value?.serial
    }

    fun update(transform: (Settings) -> Settings) {
        val updated = transform(state.value.settings)
        ConfigStore.saveGlobal(updated)
        if (MainActivityRuntime.nativeReady.value) runCatching { updated.applyTo() }
        state.value = state.value.copy(settings = updated)
    }

    fun importFolder(uri: Uri) {
        val context = getApplication<Application>()
        val source = DocumentFile.fromTreeUri(context, uri)
        if (source == null) {
            state.value = state.value.copy(error = "Unable to open the selected folder.")
            return
        }
        val serial = state.value.activeSerial
            ?.takeIf(String::isNotBlank)
            ?: source.name?.let(::normalizeSerial)
        if (serial.isNullOrBlank()) {
            state.value = state.value.copy(error = "Name the selected folder with the game's serial, for example SLES-52187.")
            return
        }
        // Off the main thread. A real pack is thousands of files (DBZ BT3: 4277 files /
        // 1.8 GB) and copyTree walks it over SAF, where every listFiles()/openInputStream()
        // is an IPC round-trip to the DocumentsProvider. Run inline this blocked the UI
        // thread for minutes — the busy state never even rendered — so the user got a black
        // screen and an ANR, worse still with the folder on SD. Progress is published as it
        // copies so the screen can show something moving.
        state.value = state.value.copy(busy = true, progress = 0, message = null, error = null)
        viewModelScope.launch {
            val copied = withContext(Dispatchers.IO) {
                val destination = File(textureRoot(), "$serial/replacements").apply { mkdirs() }
                val contentRoot = source.findFile("replacements")?.takeIf(DocumentFile::isDirectory) ?: source
                runCatching { copyTree(contentRoot, destination) }.getOrDefault(0)
            }
            state.value = if (copied > 0) {
                state.value.copy(busy = false, progress = 0, message = "Imported $copied texture files for $serial.")
            } else {
                state.value.copy(busy = false, progress = 0, error = "No texture files were imported.")
            }
            if (copied > 0) reloadCore()
            refresh()
        }
    }

    fun delete(pack: TexturePackItem) {
        val serialDirectory = pack.directory.parentFile ?: return
        // Also off the main thread: deleteRecursively over a 1.8 GB / 4000-file pack is
        // just as capable of ANRing as the import was.
        state.value = state.value.copy(busy = true, message = null, error = null)
        viewModelScope.launch {
            val deleted = withContext(Dispatchers.IO) {
                runCatching { serialDirectory.deleteRecursively() }.getOrDefault(false)
            }
            state.value = if (deleted) {
                state.value.copy(busy = false, message = "Deleted texture pack ${pack.serial}.")
            } else {
                state.value.copy(busy = false, error = "Unable to delete ${pack.serial}.")
            }
            if (deleted && pack.serial.equals(state.value.activeSerial, true)) reloadCore()
            refresh()
        }
    }

    fun dismissMessage() {
        state.value = state.value.copy(message = null, error = null)
    }

    private fun copyTree(source: DocumentFile, destination: File, running: IntArray = IntArray(1)): Int {
        var copied = 0
        source.listFiles().forEach { child ->
            val name = child.name?.replace(Regex("[/:*?\"<>|]"), "_") ?: return@forEach
            if (child.isDirectory) {
                copied += copyTree(child, File(destination, name).apply { mkdirs() }, running)
            } else if (child.isFile) {
                val success = getApplication<Application>().contentResolver.openInputStream(child.uri)?.use { input ->
                    File(destination, name).outputStream().use(input::copyTo)
                    true
                } ?: false
                if (success) {
                    copied++
                    // Publish a running count so a multi-thousand-file import shows movement
                    // instead of looking hung. Throttled — this runs on Dispatchers.IO and
                    // state is read by Compose, so every file would be needless recomposition.
                    running[0]++
                    if (running[0] % 25 == 0) {
                        state.value = state.value.copy(progress = running[0])
                    }
                }
            }
        }
        return copied
    }

    private fun textureRoot(): File = File(MainActivityRuntime.assetCopyRoot(getApplication()), "textures")

    private fun normalizeSerial(value: String): String? {
        val match = Regex("([A-Za-z]{4})[-_ ]?(\\d{3})[._ ]?(\\d{2})").find(value) ?: return null
        return "${match.groupValues[1].uppercase()}-${match.groupValues[2]}${match.groupValues[3]}"
    }

    private fun reloadCore() {
        if (!MainActivityRuntime.nativeReady.value) return
        runCatching { NativeApp.reloadTextureReplacements() }
    }
}
