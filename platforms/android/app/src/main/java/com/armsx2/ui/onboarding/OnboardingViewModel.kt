package com.armsx2.ui.onboarding

import android.app.Application
import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.armsx2.BiosInfo
import com.armsx2.runtime.MainActivityRuntime
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp

/** One importable BIOS the user can pick from the setup list. */
data class BiosCandidate(val name: String, val path: String, val info: BiosInfo)

data class OnboardingUiState(
    val page: Int = 0,
    val systemLocation: StorageLocation = StorageLocation.Internal,
    val biosName: String? = null,
    val biosInfo: BiosInfo? = null,
    val biosCount: Int = 0,
    // Every BIOS a folder import turned up, so the user can pick right here (not
    // just accept the auto-picked first one). Empty for a single-file import.
    val biosOptions: List<BiosCandidate> = emptyList(),
    val selectedBiosPath: String? = null,
    val gameFolders: List<String> = emptyList(),
    val busy: Boolean = false,
    val error: String? = null,
)

enum class StorageLocation { Internal, SdCard, Custom }

class OnboardingViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(OnboardingUiState())
        private set

    fun load() {
        val existingBios = MainActivityRuntime.bios.value?.let(::File)?.takeIf(File::isFile)
        val info = existingBios?.let(::probeFile)
        val dir = MainActivityRuntime.systemDir.value
        state.value = state.value.copy(
            systemLocation = when {
                dir == null -> StorageLocation.Internal
                dir == MainActivityRuntime.sdCardDataDir(getApplication()) -> StorageLocation.SdCard
                else -> StorageLocation.Custom
            },
            biosName = existingBios?.name,
            biosInfo = info,
            selectedBiosPath = existingBios?.absolutePath,
            gameFolders = MainActivityRuntime.romsDirs.value,
            page = if (MainActivityRuntime.setupComplete.value) 1 else 0,
        )
    }

    fun next() {
        if (canContinue()) state.value = state.value.copy(page = (state.value.page + 1).coerceAtMost(4), error = null)
    }

    fun previous() {
        state.value = state.value.copy(page = (state.value.page - 1).coerceAtLeast(0), error = null)
    }

    fun goTo(page: Int) {
        state.value = state.value.copy(page = page.coerceIn(0, 4), error = null)
    }

    fun selectStorage(location: StorageLocation) {
        when (location) {
            StorageLocation.Internal -> {
                MainActivityRuntime.systemDir.value = null
                MainActivityRuntime.prefs.edit().remove("systemDir").apply()
                state.value = state.value.copy(systemLocation = location, error = null)
            }
            StorageLocation.SdCard -> {
                val path = MainActivityRuntime.sdCardDataDir(getApplication())
                if (path == null) {
                    state.value = state.value.copy(error = "No writable SD card was found.")
                    return
                }
                MainActivityRuntime.systemDir.value = path
                MainActivityRuntime.prefs.edit().putString("systemDir", path).apply()
                state.value = state.value.copy(systemLocation = location, error = null)
            }
            // Custom is driven by the UI (all-files-access grant + folder pick, github
            // flavor only) — the resolved POSIX path arrives via selectCustomStorage.
            StorageLocation.Custom -> {}
        }
    }

    /** Point the data root at a user-picked folder (github flavor: all-files access).
     *  [path] is a resolved POSIX path under shared storage; the native core writes
     *  memcards / saves / configs there. Requires MANAGE_EXTERNAL_STORAGE, which the
     *  onboarding UI secures before calling this. */
    fun selectCustomStorage(path: String) {
        if (path.isBlank()) {
            state.value = state.value.copy(error = "Couldn't resolve that folder.")
            return
        }
        MainActivityRuntime.systemDir.value = path
        MainActivityRuntime.prefs.edit().putString("systemDir", path).apply()
        state.value = state.value.copy(systemLocation = StorageLocation.Custom, error = null)
    }

    fun importBios(uri: Uri) {
        state.value = state.value.copy(busy = true, error = null)
        val context = getApplication<Application>()
        runCatching { context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }
        val imported = importBiosFile(context, uri)
        state.value = if (imported != null) {
            val (target, info) = imported
            selectBios(context, target)
            state.value.copy(
                biosName = target.name,
                biosInfo = info,
                biosCount = 1,
                biosOptions = listOf(BiosCandidate(target.name, target.absolutePath, info)),
                selectedBiosPath = target.absolutePath,
                busy = false,
            )
        } else {
            state.value.copy(busy = false, error = "The selected file is not a valid PlayStation 2 BIOS.")
        }
    }

    // Item 7: point at a folder and import every valid BIOS inside it (refresh
    // parity), instead of one file at a time. Runs off the main thread — copying
    // dozens of BIOS files would otherwise freeze setup for ~10s / risk an ANR.
    // Every valid BIOS is kept as a selectable option; we auto-pick the first so
    // setup can proceed, but the user can choose a different one right on this page
    // (or later in BIOS settings).
    fun importBiosFolder(treeUri: Uri) {
        state.value = state.value.copy(busy = true, error = null)
        val context = getApplication<Application>()
        runCatching { context.contentResolver.takePersistableUriPermission(treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }
        viewModelScope.launch {
            val options = withContext(Dispatchers.IO) {
                val children = DocumentFile.fromTreeUri(context, treeUri)?.listFiles()?.filter(DocumentFile::isFile).orEmpty()
                children.mapNotNull { child ->
                    val (target, info) = importBiosFile(context, child.uri) ?: return@mapNotNull null
                    BiosCandidate(target.name, target.absolutePath, info)
                }.sortedBy { it.name.lowercase() }
            }
            val chosen = options.firstOrNull()
            state.value = if (chosen != null) {
                selectBios(context, File(chosen.path))
                state.value.copy(
                    biosName = chosen.name,
                    biosInfo = chosen.info,
                    biosCount = options.size,
                    biosOptions = options,
                    selectedBiosPath = chosen.path,
                    busy = false,
                )
            } else {
                state.value.copy(busy = false, error = "No PlayStation 2 BIOS files were found in that folder.")
            }
        }
    }

    /** Switch the active BIOS to one the user tapped in the setup list. */
    fun selectBiosCandidate(candidate: BiosCandidate) {
        selectBios(getApplication(), File(candidate.path))
        state.value = state.value.copy(
            biosName = candidate.name,
            biosInfo = candidate.info,
            selectedBiosPath = candidate.path,
        )
    }

    // Copy+validate a single BIOS into the internal BIOS dir. Returns the stored
    // file and its parsed info, or null for anything that isn't a valid BIOS (so
    // folder import can silently skip non-BIOS files).
    private fun importBiosFile(context: Context, uri: Uri): Pair<File, BiosInfo>? = runCatching {
        val name = DocumentFile.fromSingleUri(context, uri)?.name?.takeIf(String::isNotBlank) ?: "bios.bin"
        val descriptor = context.contentResolver.openFileDescriptor(uri, "r") ?: return@runCatching null
        val info = NativeApp.getBiosInfoFromFd(descriptor.detachFd()) ?: return@runCatching null
        val directory = MainActivityRuntime.internalBiosDir(context).apply { mkdirs() }
        val safeName = name.replace(Regex("[^A-Za-z0-9._-]"), "_")
        val target = File(directory, safeName)
        val temporary = File(directory, ".$safeName.import")
        context.contentResolver.openInputStream(uri)?.use { input ->
            temporary.outputStream().use(input::copyTo)
        } ?: return@runCatching null
        if (temporary.length() == 0L) {
            temporary.delete()
            return@runCatching null
        }
        if (target.exists()) target.delete()
        if (!temporary.renameTo(target)) {
            temporary.copyTo(target, overwrite = true)
            temporary.delete()
        }
        target to info
    }.getOrNull()

    private fun selectBios(context: Context, target: File) {
        MainActivityRuntime.bios.value = target.absolutePath
        MainActivityRuntime.biosDir.value = null
        MainActivityRuntime.prefs.edit().putString("bios", target.absolutePath).remove("biosDir").apply()
    }

    fun addGameFolder(uri: Uri) {
        val context = getApplication<Application>()
        runCatching {
            context.contentResolver.takePersistableUriPermission(
                uri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
            )
        }
        val updated = (state.value.gameFolders + uri.toString()).distinct()
        MainActivityRuntime.setRomsDirs(updated)
        state.value = state.value.copy(gameFolders = updated, error = null)
    }

    fun removeGameFolder(uri: String) {
        val updated = state.value.gameFolders - uri
        MainActivityRuntime.setRomsDirs(updated)
        state.value = state.value.copy(gameFolders = updated)
    }

    fun canContinue(): Boolean = when (state.value.page) {
        0, 1 -> true
        2 -> state.value.biosInfo != null
        3 -> state.value.gameFolders.isNotEmpty()
        else -> state.value.biosInfo != null && state.value.gameFolders.isNotEmpty()
    }

    fun finish() {
        if (!canContinue()) return
        val previousRoot = MainActivityRuntime.currentInitDataRoot()
        MainActivityRuntime.finishSetup()
        val selectedRoot = MainActivityRuntime.systemDirPosix()
        if (MainActivityRuntime.nativeReady.value && previousRoot != null && previousRoot != selectedRoot) {
            MainActivityRuntime.restartApp(getApplication())
        }
    }

    fun dismissError() {
        state.value = state.value.copy(error = null)
    }

    private fun probeFile(file: File): BiosInfo? = runCatching {
        val descriptor = android.os.ParcelFileDescriptor.open(file, android.os.ParcelFileDescriptor.MODE_READ_ONLY)
        NativeApp.getBiosInfoFromFd(descriptor.detachFd())
    }.getOrNull()
}

