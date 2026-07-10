package com.armsx2.ui.onboarding

import android.app.Application
import android.content.Intent
import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.AndroidViewModel
import com.armsx2.BiosInfo
import com.armsx2.runtime.MainActivityRuntime
import java.io.File
import kr.co.iefriends.pcsx2.NativeApp

data class OnboardingUiState(
    val page: Int = 0,
    val systemLocation: StorageLocation = StorageLocation.Internal,
    val biosName: String? = null,
    val biosInfo: BiosInfo? = null,
    val gameFolders: List<String> = emptyList(),
    val busy: Boolean = false,
    val error: String? = null,
)

enum class StorageLocation { Internal, SdCard }

class OnboardingViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(OnboardingUiState())
        private set

    fun load() {
        val existingBios = MainActivityRuntime.bios.value?.let(::File)?.takeIf(File::isFile)
        val info = existingBios?.let(::probeFile)
        state.value = state.value.copy(
            systemLocation = if (MainActivityRuntime.systemDir.value == null) StorageLocation.Internal else StorageLocation.SdCard,
            biosName = existingBios?.name,
            biosInfo = info,
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
        }
    }

    fun importBios(uri: Uri) {
        state.value = state.value.copy(busy = true, error = null)
        val context = getApplication<Application>()
        val result = runCatching {
            runCatching {
                context.contentResolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            val name = DocumentFile.fromSingleUri(context, uri)?.name
                ?.takeIf(String::isNotBlank)
                ?: "bios.bin"
            val descriptor = context.contentResolver.openFileDescriptor(uri, "r")
                ?: error("Unable to open the selected file.")
            val info = NativeApp.getBiosInfoFromFd(descriptor.detachFd())
                ?: error("The selected file is not a valid PlayStation 2 BIOS.")
            val directory = MainActivityRuntime.internalBiosDir(context).apply { mkdirs() }
            val safeName = name.replace(Regex("[^A-Za-z0-9._-]"), "_")
            val target = File(directory, safeName)
            val temporary = File(directory, ".$safeName.import")
            context.contentResolver.openInputStream(uri)?.use { input ->
                temporary.outputStream().use(input::copyTo)
            } ?: error("Unable to read the selected BIOS.")
            if (temporary.length() == 0L) error("The selected BIOS is empty.")
            if (target.exists()) target.delete()
            if (!temporary.renameTo(target)) {
                temporary.copyTo(target, overwrite = true)
                temporary.delete()
            }
            MainActivityRuntime.bios.value = target.absolutePath
            MainActivityRuntime.biosDir.value = null
            MainActivityRuntime.prefs.edit().putString("bios", target.absolutePath).remove("biosDir").apply()
            target.name to info
        }
        state.value = result.fold(
            onSuccess = { (name, info) -> state.value.copy(biosName = name, biosInfo = info, busy = false) },
            onFailure = { state.value.copy(busy = false, error = it.message ?: "BIOS import failed.") },
        )
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

