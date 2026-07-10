package com.armsx2.ui.bios

import android.app.Application
import android.net.Uri
import android.os.ParcelFileDescriptor
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import com.armsx2.BiosInfo
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
)

class BiosManagerViewModel(application: Application) : AndroidViewModel(application) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    var state = androidx.compose.runtime.mutableStateOf(BiosManagerUiState())
        private set

    fun refresh() {
        scope.launch {
            state.value = state.value.copy(busy = true, error = null)
            val selectedPath = MainActivityRuntime.bios.value
            val result = withContext(Dispatchers.IO) {
                MainActivityRuntime.internalBiosDir(getApplication()).apply { mkdirs() }
                    .listFiles()
                    .orEmpty()
                    .filter(File::isFile)
                    .mapNotNull { file -> probe(file)?.let { InstalledBios(file, it, file.absolutePath == selectedPath) } }
                    .sortedWith(compareByDescending<InstalledBios> { it.selected }.thenBy { it.file.name.lowercase() })
            }
            state.value = state.value.copy(items = result, busy = false)
        }
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
        runCatching { item.file.delete() }
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

