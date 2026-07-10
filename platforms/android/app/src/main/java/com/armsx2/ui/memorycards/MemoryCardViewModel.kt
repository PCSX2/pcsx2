package com.armsx2.ui.memorycards

import android.app.Application
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.config.ConfigStore
import java.io.File
import kr.co.iefriends.pcsx2.NativeApp

data class MemoryCardItem(
    val file: File,
    val slot1: Boolean,
    val slot2: Boolean,
    val size: Long,
)

data class MemoryCardUiState(
    val cards: List<MemoryCardItem> = emptyList(),
    val error: String? = null,
    val message: String? = null,
)

class MemoryCardViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(MemoryCardUiState())
        private set

    fun refresh() {
        val settings = ConfigStore.loadGlobal()
        val cards = cardDirectory().apply { mkdirs() }
            .listFiles()
            .orEmpty()
            .filter { it.isDirectory || it.extension.equals("ps2", true) }
            .map { file ->
                MemoryCardItem(
                    file = file,
                    slot1 = settings.memoryCardSlot1Enabled && file.name.equals(settings.memoryCardSlot1Filename, true),
                    slot2 = settings.memoryCardSlot2Enabled && file.name.equals(settings.memoryCardSlot2Filename, true),
                    size = if (file.isFile) file.length() else directorySize(file),
                )
            }
            .sortedWith(compareByDescending<MemoryCardItem> { it.slot1 || it.slot2 }.thenBy { it.file.name.lowercase() })
        state.value = state.value.copy(cards = cards)
    }

    fun create(name: String, sizeType: Int) {
        if (!MainActivityRuntime.nativeReady.value) {
            state.value = state.value.copy(error = "The emulator core is still starting.")
            return
        }
        val safe = name.trim().replace(Regex("[\\/:*?\"<>|]"), "_").ifBlank { "MemoryCard" }
        val fileName = if (safe.endsWith(".ps2", true)) safe else "$safe.ps2"
        val success = runCatching { NativeApp.createMemoryCard(fileName, 1, sizeType.coerceIn(1, 4)) }.getOrDefault(false)
        state.value = if (success) state.value.copy(message = "Created $fileName.") else state.value.copy(error = "Unable to create $fileName.")
        refresh()
    }

    fun import(uri: Uri) {
        val context = getApplication<Application>()
        val name = DocumentFile.fromSingleUri(context, uri)?.name?.ifBlank { null } ?: "Imported.ps2"
        val requested = if (name.endsWith(".ps2", true)) name else "$name.ps2"
        val target = uniqueFile(cardDirectory(), requested)
        val success = runCatching {
            context.contentResolver.openInputStream(uri)?.use { input -> target.outputStream().use(input::copyTo) }
                ?: error("Unable to read the selected file.")
            target.length() > 0L && NativeApp.isMemoryCard(target.name)
        }.getOrDefault(false)
        if (!success) target.delete()
        state.value = if (success) state.value.copy(message = "Imported ${target.name}.") else state.value.copy(error = "Memory card import failed.")
        refresh()
    }

    fun assign(slot: Int, item: MemoryCardItem) {
        if (!MainActivityRuntime.nativeReady.value) {
            state.value = state.value.copy(error = "The emulator core is still starting.")
            return
        }
        val success = runCatching {
            NativeApp.setSetting("MemoryCards", "Slot${slot}_Enable", "bool", "false")
            NativeApp.setSetting("MemoryCards", "Slot${slot}_Filename", "string", item.file.name)
            NativeApp.setSetting("MemoryCards", "Slot${slot}_Enable", "bool", "true")
            NativeApp.commitSettings()
            val current = ConfigStore.loadGlobal()
            ConfigStore.saveGlobal(
                if (slot == 1) current.copy(memoryCardSlot1Enabled = true, memoryCardSlot1Filename = item.file.name)
                else current.copy(memoryCardSlot2Enabled = true, memoryCardSlot2Filename = item.file.name),
            )
            true
        }.getOrDefault(false)
        state.value = if (success) state.value.copy(message = "${item.file.name} assigned to slot $slot.") else state.value.copy(error = "Unable to change slot $slot.")
        refresh()
    }

    fun delete(item: MemoryCardItem) {
        if (item.slot1 || item.slot2) {
            state.value = state.value.copy(error = "Assign another card before deleting an active card.")
            return
        }
        val deleted = if (item.file.isDirectory) item.file.deleteRecursively() else item.file.delete()
        state.value = if (deleted) state.value.copy(message = "Deleted ${item.file.name}.") else state.value.copy(error = "Unable to delete ${item.file.name}.")
        refresh()
    }

    fun dismissMessage() {
        state.value = state.value.copy(error = null, message = null)
    }

    private fun cardDirectory(): File = File(MainActivityRuntime.assetCopyRoot(getApplication()), "memcards")

    private fun uniqueFile(directory: File, name: String): File {
        directory.mkdirs()
        val base = name.substringBeforeLast('.', name)
        val extension = name.substringAfterLast('.', "").let { if (it.isEmpty()) "" else ".$it" }
        var target = File(directory, name)
        var index = 2
        while (target.exists()) target = File(directory, "$base-$index$extension").also { index++ }
        return target
    }

    private fun directorySize(directory: File): Long = directory.walkTopDown().filter(File::isFile).sumOf(File::length)
}
