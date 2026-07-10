package com.armsx2.ui.saves

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.runtime.MainActivityRuntime
import java.io.File
import kr.co.iefriends.pcsx2.NativeApp

data class SaveSlotItem(val slot: Int, val file: File?)

data class SaveManagerUiState(
    val gameTitle: String? = null,
    val slots: List<SaveSlotItem> = List(10) { SaveSlotItem(it, null) },
    val archived: List<File> = emptyList(),
    val message: String? = null,
    val error: String? = null,
)

class SaveManagerViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(SaveManagerUiState())
        private set

    fun refresh() {
        val active = MainActivityRuntime.currentGame.value
        val slots = List(10) { slot ->
            val file = if (active != null) {
                runCatching { NativeApp.getGamePathSlot(slot) }
                    .getOrNull()
                    ?.takeIf(String::isNotBlank)
                    ?.let(::File)
                    ?.takeIf { it.exists() }
            } else null
            SaveSlotItem(slot, file)
        }
        val roots = listOf("sstates", "savestates").map { File(MainActivityRuntime.assetCopyRoot(getApplication()), it) }
        val archived = roots.flatMap { root ->
            if (!root.isDirectory) emptyList() else root.walkTopDown().filter { it.isFile && it.extension.equals("p2s", true) }.toList()
        }.distinctBy { it.absolutePath }.sortedByDescending { it.lastModified() }
        state.value = state.value.copy(gameTitle = active?.title, slots = slots, archived = archived)
    }

    fun save(slot: Int) {
        val ok = runCatching { NativeApp.saveStateToSlot(slot) }.getOrDefault(false)
        state.value = if (ok) state.value.copy(message = "Saved slot ${slot + 1}.") else state.value.copy(error = "Unable to save slot ${slot + 1}.")
        refresh()
    }

    fun load(slot: Int) {
        val ok = runCatching { NativeApp.loadStateFromSlot(slot) }.getOrDefault(false)
        state.value = if (ok) state.value.copy(message = "Loaded slot ${slot + 1}.") else state.value.copy(error = "Unable to load slot ${slot + 1}.")
    }

    fun delete(file: File) {
        val ok = runCatching { file.delete() }.getOrDefault(false)
        state.value = if (ok) state.value.copy(message = "Deleted ${file.name}.") else state.value.copy(error = "Unable to delete ${file.name}.")
        refresh()
    }

    fun backupAll() {
        val files = state.value.slots.mapNotNull(SaveSlotItem::file)
        if (files.isEmpty()) {
            state.value = state.value.copy(error = "There are no active save states to back up.")
            return
        }
        val destination = File(files.first().parentFile, "backups/${System.currentTimeMillis()}").apply { mkdirs() }
        val count = files.count { file -> runCatching { file.copyTo(File(destination, file.name), overwrite = true); true }.getOrDefault(false) }
        state.value = state.value.copy(message = "Backed up $count save states.")
        refresh()
    }

    fun dismissMessage() {
        state.value = state.value.copy(message = null, error = null)
    }
}
