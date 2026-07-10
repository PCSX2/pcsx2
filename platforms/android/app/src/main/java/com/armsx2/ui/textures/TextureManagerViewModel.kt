package com.armsx2.ui.textures

import android.app.Application
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.config.ConfigStore
import com.armsx2.config.Settings
import java.io.File

data class TexturePackItem(val serial: String, val directory: File, val fileCount: Int, val size: Long)

data class TextureManagerUiState(
    val settings: Settings = Settings(),
    val packs: List<TexturePackItem> = emptyList(),
    val activeSerial: String? = null,
    val busy: Boolean = false,
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
            activeSerial = MainActivityRuntime.currentGame.value?.serial,
        )
    }

    fun update(transform: (Settings) -> Settings) {
        val updated = transform(state.value.settings)
        ConfigStore.saveGlobal(updated)
        if (MainActivityRuntime.nativeReady.value) runCatching { updated.applyTo() }
        state.value = state.value.copy(settings = updated)
    }

    fun importFolder(uri: Uri) {
        val serial = state.value.activeSerial
        if (serial.isNullOrBlank()) {
            state.value = state.value.copy(error = "Launch the target game before importing its texture pack.")
            return
        }
        val context = getApplication<Application>()
        val source = DocumentFile.fromTreeUri(context, uri)
        if (source == null) {
            state.value = state.value.copy(error = "Unable to open the selected folder.")
            return
        }
        state.value = state.value.copy(busy = true)
        val destination = File(textureRoot(), "$serial/replacements").apply { mkdirs() }
        val copied = runCatching { copyTree(source, destination) }.getOrDefault(0)
        state.value = if (copied > 0) state.value.copy(busy = false, message = "Imported $copied texture files for $serial.") else state.value.copy(busy = false, error = "No texture files were imported.")
        refresh()
    }

    fun delete(pack: TexturePackItem) {
        val serialDirectory = pack.directory.parentFile ?: return
        val deleted = runCatching { serialDirectory.deleteRecursively() }.getOrDefault(false)
        state.value = if (deleted) state.value.copy(message = "Deleted texture pack ${pack.serial}.") else state.value.copy(error = "Unable to delete ${pack.serial}.")
        refresh()
    }

    fun dismissMessage() {
        state.value = state.value.copy(message = null, error = null)
    }

    private fun copyTree(source: DocumentFile, destination: File): Int {
        var copied = 0
        source.listFiles().forEach { child ->
            val name = child.name?.replace(Regex("[/:*?\"<>|]"), "_") ?: return@forEach
            if (child.isDirectory) {
                copied += copyTree(child, File(destination, name).apply { mkdirs() })
            } else if (child.isFile) {
                getApplication<Application>().contentResolver.openInputStream(child.uri)?.use { input ->
                    File(destination, name).outputStream().use(input::copyTo)
                }
                copied++
            }
        }
        return copied
    }

    private fun textureRoot(): File = File(MainActivityRuntime.assetCopyRoot(getApplication()), "textures")
}

