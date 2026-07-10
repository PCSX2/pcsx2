package com.armsx2.ui.patches

import android.app.Application
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.config.ConfigStore
import com.armsx2.config.Settings
import java.io.File
import kr.co.iefriends.pcsx2.NativeApp

data class PatchManagerUiState(
    val settings: Settings = Settings(),
    val files: List<File> = emptyList(),
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
        if (success) reloadCore()
        refresh()
    }

    fun delete(file: File) {
        val success = runCatching { file.delete() }.getOrDefault(false)
        state.value = if (success) state.value.copy(message = "Deleted ${file.name}.") else state.value.copy(error = "Unable to delete ${file.name}.")
        if (success) reloadCore()
        refresh()
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
}
