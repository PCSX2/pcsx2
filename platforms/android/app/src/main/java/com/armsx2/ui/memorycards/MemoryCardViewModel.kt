package com.armsx2.ui.memorycards

import android.app.Application
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.config.ConfigStore
import com.armsx2.config.SettingsScope
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

    fun create(name: String, type: Int, sizeType: Int) {
        if (!MainActivityRuntime.nativeReady.value) {
            state.value = state.value.copy(error = "The emulator core is still starting.")
            return
        }
        val safe = name.trim().replace(Regex("[\\/:*?\"<>|]"), "_").ifBlank { "MemoryCard" }
        val cardName: String
        val success = if (type == 2) {
            // FOLDER card = a directory holding an empty `_pcsx2_superblock` marker (the core
            // identifies a folder card by that marker). Build it with the JAVA file API, NOT
            // the native path: the libc fopen the core's FileMcd_CreateNewCard uses to write
            // the superblock is DENIED on the FUSE-backed memcard storage (the same denial the
            // folder mkdir hit — and only CreateDirectoryPath got a Java fallback, file writes
            // didn't), so the native path leaves an empty, invalid 0-byte folder. Java file ops
            // work here. No .ps2 suffix; size is ignored for folder cards.
            cardName = safe
            runCatching {
                val dir = File(cardDirectory(), safe).apply { mkdirs() }
                File(dir, "_pcsx2_superblock").createNewFile()
                dir.isDirectory
            }.getOrDefault(false)
        } else {
            cardName = if (safe.endsWith(".ps2", true)) safe else "$safe.ps2"
            runCatching { NativeApp.createMemoryCard(cardName, 1, sizeType.coerceIn(1, 4)) }.getOrDefault(false)
        }
        state.value = if (success) state.value.copy(message = "Created $cardName.") else state.value.copy(error = "Unable to create $cardName.")
        refresh()
    }

    fun import(uri: Uri) {
        val context = getApplication<Application>()
        val name = DocumentFile.fromSingleUri(context, uri)?.name?.ifBlank { null } ?: "Imported.ps2"

        // A FOLDER memory card is a directory of save folders plus a "_pcsx2_superblock"
        // marker, so it cannot travel as a single file — people share them zipped. The old
        // path appended ".ps2" to whatever was picked and copied the bytes verbatim, so a
        // zip landed as "card.zip.ps2" full of zip data: the core read it as an unformatted
        // file card, and formatting it then failed too. Unpack instead, into a real folder
        // card. (Directory picking is handled by importFolder below — OpenDocument() can
        // only return files.)
        if (name.endsWith(".zip", true)) {
            importZip(uri, name)
            return
        }

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

    /** Unpack a zipped folder memory card into cards/<name>/. Tolerates the two common
     *  shapes: entries at the archive root, or nested under a single top-level directory. */
    private fun importZip(uri: Uri, zipName: String) {
        val context = getApplication<Application>()
        val target = uniqueFile(cardDirectory(), zipName.substringBeforeLast('.'))
        val ok = runCatching {
            target.mkdirs()
            context.contentResolver.openInputStream(uri)?.use { raw ->
                java.util.zip.ZipInputStream(raw.buffered()).use { zin ->
                    var entry = zin.nextEntry
                    while (entry != null) {
                        // Strip a single wrapping directory so "Mcd001/_pcsx2_superblock"
                        // and "_pcsx2_superblock" both land correctly.
                        val rel = entry.name.replace('\\', '/').trimStart('/')
                        val stripped = if (rel.count { it == '/' } > 0 && !rel.startsWith("_pcsx2_"))
                            rel.substringAfter('/') else rel
                        if (stripped.isNotBlank()) {
                            val out = File(target, stripped)
                            // Zip-slip guard: never write outside the card directory.
                            if (out.canonicalPath.startsWith(target.canonicalPath + File.separator) ||
                                out.canonicalPath == target.canonicalPath) {
                                if (entry.isDirectory) {
                                    out.mkdirs()
                                } else {
                                    out.parentFile?.mkdirs()
                                    out.outputStream().use { zin.copyTo(it) }
                                }
                            }
                        }
                        zin.closeEntry()
                        entry = zin.nextEntry
                    }
                }
            } ?: error("Unable to read the selected file.")
            // A folder card is only valid with its superblock; without it the core would
            // report "unformatted" exactly as before, so fail loudly here instead.
            File(target, "_pcsx2_superblock").isFile
        }.getOrDefault(false)
        if (!ok) target.deleteRecursively()
        state.value = if (ok) {
            state.value.copy(message = "Imported folder card ${target.name}.")
        } else {
            state.value.copy(error = "That zip isn't a folder memory card (no _pcsx2_superblock inside).")
        }
        refresh()
    }

    /** Import a folder memory card straight from a directory the user picks. */
    fun importFolder(uri: Uri) {
        val context = getApplication<Application>()
        val source = DocumentFile.fromTreeUri(context, uri)
        if (source == null || !source.isDirectory) {
            state.value = state.value.copy(error = "Unable to open the selected folder.")
            return
        }
        val target = uniqueFile(cardDirectory(), source.name?.ifBlank { null } ?: "Imported")
        val ok = runCatching {
            target.mkdirs()
            copyTree(source, target)
            File(target, "_pcsx2_superblock").isFile
        }.getOrDefault(false)
        if (!ok) target.deleteRecursively()
        state.value = if (ok) {
            state.value.copy(message = "Imported folder card ${target.name}.")
        } else {
            state.value.copy(error = "That folder isn't a memory card (no _pcsx2_superblock inside).")
        }
        refresh()
    }

    private fun copyTree(source: DocumentFile, destination: File) {
        source.listFiles().forEach { child ->
            val childName = child.name ?: return@forEach
            if (child.isDirectory) {
                copyTree(child, File(destination, childName).apply { mkdirs() })
            } else if (child.isFile) {
                getApplication<Application>().contentResolver.openInputStream(child.uri)?.use { input ->
                    File(destination, childName).outputStream().use(input::copyTo)
                }
            }
        }
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

    /**
     * Per-game memory card (#137, NetherSX2-style): store the card as a per-game
     * override in the `config.game.<serial>` tier. MainActivityRuntime.applyRendererPrefs
     * already resolves and applies this at every boot, so no native call is needed —
     * it takes effect the next time the game starts.
     */
    fun assignToGame(serial: String, slot: Int, item: MemoryCardItem) {
        val resolved = ConfigStore.resolveForGame(serial)
        val updated = if (slot == 1) {
            resolved.copy(memoryCardSlot1Enabled = true, memoryCardSlot1Filename = item.file.name)
        } else {
            resolved.copy(memoryCardSlot2Enabled = true, memoryCardSlot2Filename = item.file.name)
        }
        val ok = runCatching { ConfigStore.save(SettingsScope.Game, serial, updated) }.isSuccess
        state.value = if (ok) {
            state.value.copy(message = "${item.file.name} set for this game (slot $slot). Restart the game to apply.")
        } else {
            state.value.copy(error = "Unable to set a per-game card.")
        }
        refresh()
    }

    /**
     * Drop this game's per-game card for [slot] so it falls back to the global choice.
     *
     * There was previously no way to undo a per-game assignment at all. ConfigStore.save
     * stores a DIFF against global, so writing the global value back makes the key vanish
     * from the override blob rather than pinning it to the same value.
     */
    fun clearGameCard(serial: String, slot: Int) {
        val global = ConfigStore.loadGlobal()
        val resolved = ConfigStore.resolveForGame(serial)
        val updated = if (slot == 1) {
            resolved.copy(
                memoryCardSlot1Enabled = global.memoryCardSlot1Enabled,
                memoryCardSlot1Filename = global.memoryCardSlot1Filename,
            )
        } else {
            resolved.copy(
                memoryCardSlot2Enabled = global.memoryCardSlot2Enabled,
                memoryCardSlot2Filename = global.memoryCardSlot2Filename,
            )
        }
        val ok = runCatching { ConfigStore.save(SettingsScope.Game, serial, updated) }.isSuccess
        state.value = if (ok) {
            state.value.copy(message = "Slot $slot follows the global card again. Restart the game to apply.")
        } else {
            state.value.copy(error = "Unable to clear the per-game card.")
        }
        refresh()
    }

    /** The card filename this game currently resolves to (for the "in use" chip). */
    fun perGameCard(serial: String?, slot: Int): String? {
        serial ?: return null
        val resolved = runCatching { ConfigStore.resolveForGame(serial) }.getOrNull() ?: return null
        return if (slot == 1) {
            resolved.memoryCardSlot1Filename.takeIf { resolved.memoryCardSlot1Enabled }
        } else {
            resolved.memoryCardSlot2Filename.takeIf { resolved.memoryCardSlot2Enabled }
        }
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

    /** Copy a (file) memory card out to a user-chosen SAF location for backup / moving to another
     *  device. Folder cards aren't a single file, so the UI only offers Export for file cards. */
    fun export(src: File, uri: Uri) {
        val ok = runCatching {
            getApplication<Application>().contentResolver.openOutputStream(uri)?.use { out ->
                src.inputStream().use { it.copyTo(out) }
            } ?: error("Unable to open destination.")
        }.isSuccess
        state.value = if (ok) state.value.copy(message = "Exported ${src.name}.")
                      else state.value.copy(error = "Export failed for ${src.name}.")
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
