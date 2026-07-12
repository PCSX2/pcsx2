package com.armsx2.ui

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.ParcelFileDescriptor
import android.provider.Settings
import com.armsx2.BuildConfig
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.itemsIndexed
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import com.armsx2.CustomDriver
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.documentfile.provider.DocumentFile
import com.armsx2.BiosInfo
import com.armsx2.Main
import com.armsx2.R
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import kr.co.iefriends.pcsx2.NativeApp
import java.io.File

object SetupImpl {
    val setupState = mutableStateOf(0)
    val allowPrev = mutableStateOf(false)
    val allowNext = mutableStateOf(false)

    /** Build version string read at first use from BuildVersion::GitRev
     *  via NativeApp.getBuildVersion(). Format
     *  "GitTagHi.GitTagMid.GitTagLo.ARMSX2Build-SNAPSHOT". The wizard
     *  shows this under the "ARMSX2" wordmark in the title row so it
     *  matches the pause overlay's brand header without a Kotlin-side
     *  hardcoded copy. */
    private val buildVersionString: String by lazy {
        runCatching { NativeApp.getBuildVersion() }
            .getOrNull()
            ?.takeIf { it.isNotEmpty() }
            ?.let { "v$it" }
            ?: ""
    }

    // -------- BIOS setup state --------
    data class ScannedBios(val uri: Uri, val displayName: String, val info: BiosInfo)
    private val scannedBioses = mutableStateListOf<ScannedBios>()
    /** null = no selection; otherwise a valid index into scannedBioses. */
    private val selectedBiosIdx = mutableStateOf<Int?>(null)
    private val biosScanning = mutableStateOf(false)
    private val biosScanError = mutableStateOf<String?>(null)

    /** Tree URI of the BIOS folder the user picked. Persisted to prefs as
     *  `biosDir` after a successful scan so re-entry can rescan without
     *  forcing a re-pick. Falls back to "no folder selected yet" UI when
     *  null (fresh first-time setup) or when a stored URI's permission
     *  has been revoked between sessions. */
    private val biosDirUri = mutableStateOf<Uri?>(null)

    /** Last folder we issued a scan for. Drives the "fire scan once per
     *  picked folder" guard in SetupBiosContent's LaunchedEffect — without
     *  this we'd double-scan on every recomposition. */
    private val lastScannedDir = mutableStateOf<Uri?>(null)

    /** Dashboard sub-flow: after the user grants access to a BIOS folder,
     *  keep the original BIOS grid so they can choose the exact BIOS image
     *  from that folder before returning to the glass setup page. */
    private val showBiosChooser = mutableStateOf(false)

    /** Cached metadata for the configured BIOS file at `Main.bios.value`,
     *  used as a single-row fallback when we don't have a folder URI to
     *  rescan (e.g. upgrade path from an older build that didn't store
     *  biosDir, or revoked persistable permission). */
    private val configuredBiosInfo = mutableStateOf<BiosInfo?>(null)

    // -------- System dir setup state --------
    private val systemDirUri = mutableStateOf<Uri?>(null)
    private val systemDirDisplay = mutableStateOf<String?>(null)
    /** Sentinel: the app-private fallback is the active system-dir choice
     *  (vs. a SAF folder). Treated as a valid "done" state for advancing
     *  past the system-dir step. DEFAULTS TRUE: app-private needs no
     *  permission and is the only reliable writable root now that
     *  MANAGE_EXTERNAL_STORAGE is gone (Play compliance). Without this,
     *  a fresh first-run leaves appFolderReady()=false — and since a custom
     *  folder now always fails the writability probe, "Let's Go" could never
     *  enable, stranding new users on the setup screen. resetForReentry()
     *  and a custom-folder pick override this as appropriate. */
    private val systemDirUseDefault = mutableStateOf(true)
    /** Surface message shown on the system-dir page when validation fails
     *  (typically scoped-storage write rejection on a non-app-private folder).
     *  null = no error. */
    private val systemDirError = mutableStateOf<String?>(null)

    /** github (all-files) flavor only: the App-Data pick opens a small chooser
     *  (Internal / SD / any folder) instead of going straight to SD. With
     *  MANAGE_EXTERNAL_STORAGE granted, an arbitrary folder is raw-writable by
     *  the core, so the existing write-probe in finishSystemDirStep accepts it.
     *  Never shown on the play build (BuildConfig.STORAGE_ALL_FILES == false). */
    private val showStorageChooser = mutableStateOf(false)

    /** Set when the user changes the data-root (Internal<->SD) AFTER setup is
     *  already complete. The native root is pinned per process, so we show a
     *  confirm + restart the app rather than silently not applying the change. */
    private val storageRestartPending = mutableStateOf(false)

    // -------- ROMs dirs setup state --------
    /** Working list of ROM-folder URIs while the wizard is open. Persists
     *  to Main.romsDirs (and prefs) on Next at the ROMs step. The user
     *  can add multiple folders + remove individual entries. */
    private val romsDirsState = mutableStateListOf<Uri>()

    // -------- Renderer setup state --------
    /** Picked renderer backend, as the canonical `Main.renderer` strings
     *  ("opengl" or "vulkan"). null = nothing picked yet, blocks Next on
     *  this page. Re-entry pre-loads the existing pref so the user sees
     *  their current choice highlighted. There is no "auto" choice in the
     *  wizard — the in-game Renderer pill stays as the override path, but
     *  the wizard forces an explicit GL/VK decision so the SW path knows
     *  which display backend to host the software frame on. */
    private val selectedRenderer = mutableStateOf<String?>(null)

    // -------- Optional "Recommended Settings" step (first-run only) --------
    // Shown as an OVERLAY after the core System/BIOS/ROMs setup completes on first
    // run, before finishSetup — so it can NEVER re-block first-run (it's after the
    // gate and fully skippable). PCSX2-wizard-style display/perf onboarding; choices
    // default to the current global config and are written to global on "Finish".
    private val showRecommended = mutableStateOf(false)
    private val recAspect = mutableStateOf(1)        // 0 Stretch, 1 Auto, 2 4:3, 3 16:9, 4 10:7
    private val recUpscale = mutableStateOf(1)       // internal-resolution multiplier 1..5
    private val recAntiBlur = mutableStateOf(true)
    private val recWidescreen = mutableStateOf(false)
    private val recDeinterlaceOff = mutableStateOf(false) // false = Automatic, true = Off
    // Recommended performance preset: 0 = Optimal (safe), 1 = Fast (aggressive
    // speedhacks + native res), 2 = Low-End (Fast plus every cheap GPU lever — min
    // blending, no mipmaps/palette-conv, partial texture preload, ROV off). Detected
    // low-end devices pre-select Low-End so budget users actually get those defaults
    // instead of leaving 2x on the table in settings they never find (#124).
    private val recPerfPreset = mutableStateOf(0)

    // -------- Custom Vulkan driver state --------
    /** Active driver id (matches `CustomDriver.InstalledDriver.id`). null
     *  = system / default Vulkan loader. Mirrored from / to `Main.customDriverId`
     *  + prefs on commit. Persisted only when the user advances past the
     *  renderer page so abandoning the wizard mid-selection doesn't stomp
     *  a previously-saved pick. */
    private val selectedDriverId = mutableStateOf<String?>(null)

    /** Live list of `<externalFilesDir>/drivers/[id]/` entries. Refreshed
     *  on renderer-page enter, after an install, and after a delete. */
    private val installedDrivers = mutableStateListOf<CustomDriver.InstalledDriver>()

    /** Sheet visibility for the driver browser (the "+ Add" path). */
    private val showDriverBrowser = mutableStateOf(false)

    /** GitHub releases list. null = not yet fetched / loading; empty
     *  list = fetch returned nothing (or failed silently). */
    private val remoteDrivers = mutableStateOf<List<CustomDriver.RemoteDriver>?>(null)

    /** Id of the driver currently downloading + extracting (matches
     *  `CustomDriver.RemoteDriver.id`). null = no install in flight. */
    private val installingDriverId = mutableStateOf<String?>(null)

    /** Last fetch error message, surfaced as a banner at the top of the
     *  driver browser sheet. */
    private val driverBrowserError = mutableStateOf<String?>(null)

    // ---- Persist + advance helpers ----

    /**
     * Copy the user-selected BIOS into the active data-root bios/ directory so
     * emucore (which reads via FileSystem::OpenManagedCFile) can load it from
     * a real path. Returns the absolute path on success, null on failure.
     */
    private fun finishBiosStep(context: Context): String? {
        val idx = selectedBiosIdx.value
        // No selection — keep what's already configured (re-entry path
        // where the scan failed but a previous BIOS was set).
        if (idx == null) {
            Main.bios.value?.let { pinBiosIfReady(File(it)) }
            return Main.bios.value
        }

        val bios = scannedBioses.getOrNull(idx) ?: return null

        // Write to app-private internal storage, NOT the chosen data root. The
        // native core can't reliably open a BIOS off a removable/SAF volume on
        // Android 11+ (a data-root-on-SD setup then failed VM init and bounced
        // back to the library), so the BIOS is decoupled from DataRoot and pinned
        // internal — matching native-lib initialize()'s documented expectation.
        // Memcards/saves/configs still follow the data root.
        val biosDir = Main.internalBiosDir(context).apply { mkdirs() }
        val outFile = File(biosDir, bios.displayName)

        // Same-content fast-path: when the user re-entered setup and the
        // pre-selected row matches the already-configured BIOS by both
        // filename and the existing data-root file already exists, skip the
        // copy. The pref already points at outFile and emucore is happy.
        //
        // We DON'T push setSetting from finishBiosStep on a clean install —
        // kickoffEmucoreInit is gated on setupComplete, so Java_..._initialize
        // hasn't run yet and the base settings layer isn't installed. Calling
        // Host::SetBaseStringSettingValue with no LAYER_BASE installed
        // null-derefs in LayeredSettingsInterface. The pin is now applied
        // post-init via Main.kickoffEmucoreInit's pushBiosFilenamePin().
        if (outFile.absolutePath == Main.bios.value && outFile.exists() && outFile.length() > 0L) {
            configuredBiosInfo.value = bios.info
            copyBiosSiblings(context, bios, biosDir)
            pinBiosIfReady(outFile)
            return outFile.absolutePath
        }

        return try {
            if (!copyBiosSafely(context, bios, outFile))
                return null
            copyBiosSiblings(context, bios, biosDir)

            Main.bios.value = outFile.absolutePath
            Main.prefs.edit().putString("bios", outFile.absolutePath).apply()
            configuredBiosInfo.value = bios.info
            pinBiosIfReady(outFile)
            outFile.absolutePath
        } catch (_: Exception) {
            null
        }
    }

    private fun copyBiosSafely(context: Context, bios: ScannedBios, outFile: File): Boolean {
        // A common first-run flow is picking <systemDir>/bios as the BIOS
        // source folder. In that case outFile is the selected file. Opening an
        // OutputStream to it would truncate the BIOS to 0 B before the read
        // stream can copy anything, so detect that self-copy and simply accept
        // the already-valid file.
        selectedBiosSourceFile(bios)?.let { source ->
            if (sameFilePath(source, outFile))
                return outFile.exists() && outFile.length() > 0L
        }

        val tmp = File(outFile.parentFile, ".${outFile.name}.import.tmp")
        if (tmp.exists())
            tmp.delete()

        val copied = context.contentResolver.openInputStream(bios.uri)?.use { ins ->
            tmp.outputStream().use { outs -> ins.copyTo(outs) }
        } ?: return false

        if (copied <= 0L || tmp.length() <= 0L) {
            tmp.delete()
            return false
        }

        if (outFile.exists() && !outFile.delete()) {
            tmp.delete()
            return false
        }

        val installed = tmp.renameTo(outFile) || runCatching {
            tmp.copyTo(outFile, overwrite = true)
            true
        }.getOrDefault(false)
        tmp.delete()
        return installed && outFile.exists() && outFile.length() > 0L
    }

    // Copy the BIOS's sibling ROM1/ROM2/EROM extension images from the picked
    // source folder into the app-private BIOS dir, alongside the main BIOS. The
    // Chinese BIOS needs its 2MB ROM2 to boot; the emulator's LoadExtraRom looks
    // for "<mainbios>.rom2" / "<base>.rom2" next to the BIOS, but setup previously
    // copied only the main file, so Chinese BIOS/games never booted (Rudrox #). The
    // sibling images have no ROMVER header so they're skipped by the BIOS scanner —
    // we match them here purely by filename against the selected BIOS.
    private fun copyBiosSiblings(context: Context, bios: ScannedBios, biosDir: File) {
        val treeUri = biosDirUri.value ?: lastScannedDir.value ?: return
        val displayName = bios.displayName
        val base = displayName.substringBeforeLast('.', displayName)
        val romExts = setOf("rom1", "rom2", "erom")
        val tree = runCatching { DocumentFile.fromTreeUri(context, treeUri) }.getOrNull() ?: return
        for (f in tree.listFiles()) {
            if (!f.isFile) continue
            val name = f.name ?: continue
            val ext = name.substringAfterLast('.', "").lowercase()
            if (ext !in romExts) continue
            val stem = name.substringBeforeLast('.')
            // Land it under the exact name LoadExtraRom builds (append form for a
            // "<mainbios>.rom2" sibling, replace-ext form for a "<base>.rom2" one),
            // with a lowercased extension so the case-sensitive fs lookup matches.
            val target = when {
                stem.equals(displayName, ignoreCase = true) -> "$displayName.$ext"
                stem.equals(base, ignoreCase = true) -> "$base.$ext"
                else -> continue
            }
            val outFile = File(biosDir, target)
            if (outFile.exists() && outFile.length() > 0L) continue
            runCatching {
                val tmp = File(biosDir, ".$target.import.tmp")
                if (tmp.exists()) tmp.delete()
                val copied = context.contentResolver.openInputStream(f.uri)?.use { ins ->
                    tmp.outputStream().use { outs -> ins.copyTo(outs) }
                } ?: 0L
                if (copied > 0L && tmp.length() > 0L) {
                    outFile.delete()
                    if (!tmp.renameTo(outFile)) {
                        tmp.copyTo(outFile, overwrite = true)
                        tmp.delete()
                    }
                } else {
                    tmp.delete()
                }
            }
        }
    }

    private fun selectedBiosSourceFile(bios: ScannedBios): File? {
        documentUriToPosix(bios.uri)?.let { return File(it) }
        val dir = Main.resolveTreeUriToPosix(biosDirUri.value?.toString()) ?: return null
        return File(dir, bios.displayName)
    }

    private fun documentUriToPosix(uri: Uri): String? {
        val docId = runCatching {
            android.provider.DocumentsContract.getDocumentId(uri)
        }.getOrNull() ?: return null
        val parts = docId.split(":", limit = 2)
        if (parts.size != 2)
            return null
        val (volumeId, relPath) = parts
        return when (volumeId) {
            "primary" -> "/storage/emulated/0/$relPath"
            else -> "/storage/$volumeId/$relPath"
        }
    }

    private fun sameFilePath(a: File, b: File): Boolean {
        val ca = runCatching { a.canonicalFile }.getOrDefault(a.absoluteFile)
        val cb = runCatching { b.canonicalFile }.getOrDefault(b.absoluteFile)
        return ca == cb
    }

    private fun pinBiosIfReady(file: File) {
        if (!Main.nativeReady.value) return
        NativeApp.setSetting("Filenames", "BIOS", "string", file.name)
        NativeApp.commitSettings()
    }

    /**
     * Open the configured BIOS file and ask emucore for its metadata. Used
     * to populate the "Currently configured" row on the BIOS page so the
     * user sees flag/version/description for what's already set up, not
     * just a filename. Local-file path → ParcelFileDescriptor → fd → JNI
     * (the same getBiosInfoFromFd path used during folder scans).
     */
    private fun probeExistingBios(path: String): BiosInfo? {
        return try {
            val pfd = ParcelFileDescriptor.open(File(path), ParcelFileDescriptor.MODE_READ_ONLY)
            val fd = pfd.detachFd()
            NativeApp.getBiosInfoFromFd(fd)
        } catch (_: Exception) {
            null
        }
    }

    /** True when the github build already holds All-Files Access (or is on an
     *  older API where it isn't the gating model). Only meaningful on the
     *  github flavor; the play build never reaches the all-files chooser. */
    private fun allFilesAccessGranted(): Boolean =
        Build.VERSION.SDK_INT < Build.VERSION_CODES.R || Environment.isExternalStorageManager()

    private fun finishSystemDirStep(context: Context): String? {
        // App-private fallback path. Wipe any prior systemDir pref so
        // NativeApp.initializeOnce → Main.systemDirPosix returns null and
        // emucore writes under getExternalFilesDir.
        if (systemDirUseDefault.value) {
            Main.systemDir.value = null
            Main.prefs.edit().remove("systemDir").apply()
            systemDirError.value = null
            return context.getExternalFilesDir(null)?.absolutePath
                ?: context.dataDir.absolutePath
        }

        val uri = systemDirUri.value
        // Re-entry path: keep existing pref if user didn't repick. The
        // persistable grant from a prior session survives across process
        // restarts so we don't need to re-take it.
        if (uri == null) {
            // Re-entry after choosing the app-private/default folder has no
            // SAF URI and no persisted systemDir by design. Treat that as a
            // valid completed system-dir step so Let's Go can close setup.
            if (Main.setupComplete.value && Main.systemDir.value == null) {
                return context.getExternalFilesDir(null)?.absolutePath
                    ?: context.dataDir.absolutePath
            }
            return Main.systemDir.value
        }

        // Validate POSIX writability BEFORE persisting. The SAF tree-URI grant
        // lets us read, but emucore's FileSystem APIs hit raw fopen/mkdir for
        // memcards, savestates, configs, and shader data. On modern Android,
        // those writes are only reliable in app-private storage unless the
        // picked folder resolves to a real native-writable path.
        val posix = Main.resolveTreeUriToPosix(uri.toString())
        if (posix == null || !Main.validateSystemDirWritable(posix)) {
            systemDirError.value = I18n.get("setup.systemDir.error.notWritable")
            return null
        }

        try {
            context.contentResolver.takePersistableUriPermission(
                uri, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
        } catch (_: SecurityException) { /* already persisted, or revoked */ }
        Main.systemDir.value = uri.toString()
        Main.prefs.edit().putString("systemDir", uri.toString()).apply()
        systemDirError.value = null
        return uri.toString()
    }

    private fun finishRomsStep(context: Context): String? {
        // Need at least one ROM folder. Re-entry from the Settings cog
        // pre-loads Main.romsDirs into romsDirsState so the user sees
        // their existing list; if they didn't change anything, we still
        // re-run the persistable-permission take (idempotent, harmless).
        if (romsDirsState.isEmpty()) return null

        for (uri in romsDirsState) {
            try {
                context.contentResolver.takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (_: SecurityException) { /* already persisted, or revoked */ }
        }
        Main.setRomsDirs(romsDirsState.map { it.toString() })
        // Returning the first URI's string keeps the existing String?-returning
        // contract for the wizard's Next gating (non-null = success).
        return romsDirsState.first().toString()
    }

    private fun appFolderReady(): Boolean =
        systemDirUri.value != null ||
        systemDirUseDefault.value ||
        Main.systemDir.value != null ||
        (Main.setupComplete.value && Main.systemDir.value == null)

    private fun biosReady(): Boolean =
        selectedBiosIdx.value != null ||
        Main.bios.value != null

    private fun romsReady(): Boolean = romsDirsState.isNotEmpty()

    private fun dashboardReady(): Boolean = appFolderReady() && biosReady() && romsReady()

    private fun refreshAllowNext() {
        allowNext.value = when (setupState.value) {
            0 -> true
            // Setup dashboard — app folder, BIOS, and ROM folder must all
            // be resolved before the final "Let's Go" commits first-run.
            1 -> dashboardReady()
            // System dir page — Next when the user has a fresh URI selected,
            // an existing pref to keep (re-entry), or has explicitly opted
            // into the app-private fallback.
            2 -> systemDirUri.value != null ||
                 systemDirUseDefault.value ||
                 Main.systemDir.value != null
            // BIOS page — Next when a row is picked, or (fallback for revoked
            // / missing biosDir) an already-configured BIOS we're keeping.
            3 -> selectedBiosIdx.value != null ||
                 (biosDirUri.value == null && Main.bios.value != null)
            // ROMs page — Next when at least one folder is selected.
            4 -> romsDirsState.isNotEmpty()
            else -> false
        }
    }

    /**
     * Reset wizard nav state for re-entry from the Settings (cog) toolbar
     * button. Existing `Main.bios` / `Main.romsDir` prefs stay in place; the
     * ROMs URI is also pre-loaded into the page state so the user visibly
     * sees their current setting (otherwise the page would render as
     * "No ROMs folder selected yet" and look like the dir got wiped).
     * The BIOS list still requires a fresh folder pick to populate
     * `scannedBioses`; the page shows a "Currently configured" card on
     * re-entry so the existing BIOS is visible there too.
     */
    fun resetForReentry() {
        setupState.value = 0
        allowPrev.value = false
        allowNext.value = true
        // Pre-load the saved renderer pick so the wizard tile is highlighted
        // on re-entry. Treat "auto" (the historical default) as "no choice
        // yet" so re-entry from old installs still forces an explicit pick.
        selectedRenderer.value = Main.renderer.value.takeIf {
            it == "opengl" || it == "vulkan"
        }
        // Custom driver state mirrors Main.customDriverId (already hydrated
        // from prefs in onCreate). installedDrivers stays empty here; the
        // renderer page composable refreshes it on first enter so we don't
        // hit disk on every wizard reset.
        selectedDriverId.value = Main.customDriverId.value
        showDriverBrowser.value = false
        installingDriverId.value = null
        driverBrowserError.value = null
        scannedBioses.clear()
        selectedBiosIdx.value = null
        biosScanning.value = false
        biosScanError.value = null
        showBiosChooser.value = false
        // Drop the last-scanned marker so the SetupBiosContent
        // LaunchedEffect re-issues a scan against the (preserved) folder.
        lastScannedDir.value = null

        // Pre-load the saved BIOS folder URI. The page's LaunchedEffect
        // picks this up and rescans; on completion the configured BIOS is
        // auto-selected by filename match. Falls through to single-row
        // configuredBiosInfo render if the URI is null or the rescan fails.
        val savedBiosDir = Main.biosDir.value
        biosDirUri.value = if (savedBiosDir != null) {
            try { Uri.parse(savedBiosDir) } catch (_: Exception) { null }
        } else null

        // Probed BIOS metadata for the fallback "no folder URI" path.
        val existingBios = Main.bios.value
        configuredBiosInfo.value = if (existingBios != null) probeExistingBios(existingBios) else null

        val existingSystem = Main.systemDir.value
        when {
            existingSystem == null -> {
                systemDirUri.value = null
                systemDirDisplay.value = null
                systemDirUseDefault.value = true
            }
            existingSystem.startsWith("content://") -> {
                // Legacy SAF custom folder from an older build.
                try {
                    val uri = Uri.parse(existingSystem)
                    systemDirUri.value = uri
                    systemDirDisplay.value = uri.lastPathSegment ?: existingSystem
                    systemDirUseDefault.value = false
                } catch (_: Exception) {
                    systemDirUri.value = null
                    systemDirDisplay.value = null
                    systemDirUseDefault.value = true
                }
            }
            else -> {
                // SD card app-specific absolute path (volume-choice model).
                systemDirUri.value = null
                systemDirDisplay.value = I18n.get("setup.systemDir.sdCard")
                systemDirUseDefault.value = false
            }
        }
        systemDirError.value = null

        // Pre-load saved ROMs list. Each URI is parsed best-effort —
        // malformed entries (rare, only if prefs was hand-edited) are
        // dropped silently rather than failing the whole list.
        romsDirsState.clear()
        for (s in Main.romsDirs.value) {
            val u = try { Uri.parse(s) } catch (_: Exception) { null } ?: continue
            romsDirsState.add(u)
        }
    }

    /** Page heading shown in the upper-left of the title row. */
    private fun pageTitle(): String = when (setupState.value) {
        0 -> I18n.get("setup.page.welcome.title")
        1 -> I18n.get("setup.page.renderer.title")
        2 -> I18n.get("setup.page.systemDir.title")
        3 -> I18n.get("setup.page.bios.title")
        4 -> I18n.get("setup.page.roms.title")
        else -> ""
    }

    /** Label for the page-local action button (in the nav row). null = no button. */
    private fun midButtonLabel(): String? = when (setupState.value) {
        2 -> if (systemDirUri.value == null) I18n.get("setup.button.pickCustomFolder") else I18n.get("setup.button.pickDifferentFolder")
        // Use the URI presence (not the in-memory list) so the label says
        // "Pick a different folder" immediately on re-entry when we already
        // have a remembered biosDir, even before the auto-rescan finishes.
        3 -> if (biosDirUri.value == null) I18n.get("setup.button.pickBiosFolder") else I18n.get("setup.button.pickDifferentFolder")
        4 -> if (romsDirsState.isEmpty()) I18n.get("setup.button.pickRomsFolder") else I18n.get("setup.button.addAnotherFolder")
        else -> null
    }

    /** Persist the picked renderer to prefs + Main.renderer so applyRendererPrefs
     *  routes to the right JNI on the next VM start. Returns the canonical
     *  string on success, null when no choice was made (Next is gated above
     *  but defensive — re-entry edge cases). */
    private fun finishRendererStep(): String? {
        val pick = selectedRenderer.value ?: return null
        Main.renderer.value = pick
        // Driver pick is only meaningful for Vulkan; clear it on the OpenGL
        // path so re-entry from OGL doesn't leave a stale custom driver
        // pinned (the JNI setter would still fire and adrenotools would
        // resolve a path that gets ignored by the GL backend, but cleaner
        // to drop it).
        val driverId = if (pick == "vulkan") selectedDriverId.value else null
        Main.customDriverId.value = driverId
        Main.prefs.edit()
            .putString("renderer", pick)
            .putString("customDriverId", driverId.orEmpty())
            .apply()
        // Renderer now lives in the Settings tier (resolved by applyRendererPrefs).
        // Mirror the wizard pick into the global config so it's honored, not just
        // left in the legacy pref the one-time migration may have already passed.
        runCatching {
            com.armsx2.config.ConfigStore.saveGlobal(
                com.armsx2.config.ConfigStore.loadGlobal().copy(renderer = pick)
            )
        }
        return pick
    }

    /** Pre-fill the Recommended Settings choices from the current global config so
     *  the overlay shows the existing values (and "Skip" is a true no-op). */
    private fun loadRecommendedFromGlobal() {
        val g = com.armsx2.config.ConfigStore.loadGlobal()
        // Seed the renderer tile from the current config (default Vulkan if it's
        // auto/software) so the wizard shows a concrete OpenGL/Vulkan choice.
        if (selectedRenderer.value == null)
            selectedRenderer.value = if (g.renderer == "opengl") "opengl" else "vulkan"
        recAspect.value = g.aspectRatio.coerceIn(0, 4)
        recUpscale.value = g.upscaleFloat.toInt().coerceIn(1, 8)
        recAntiBlur.value = g.antiBlur
        recWidescreen.value = g.enableWideScreenPatches
        recDeinterlaceOff.value = g.deinterlaceMode == 1 // 0 = Auto, 1 = Off
        // Default the Performance choice from any prior config, but on a fresh
        // low-end device pre-RECOMMEND the full Low-End preset (never forced — the
        // user can flip it on the same screen). The probe is best-effort; a bad read
        // reports "not low-end" and falls back to detecting Fast vs Optimal from the
        // prior config.
        val ctx = Main.instance
        val lowEnd = ctx != null && runCatching { com.armsx2.DeviceTier.isLowEnd(ctx) }.getOrDefault(false)
        recPerfPreset.value = when {
            lowEnd -> 2
            g.eeCycleSkip >= 2 || g.fastCDVD -> 1
            else -> 0
        }
    }

    /** Write the Recommended Settings choices to the GLOBAL config tier. */
    private fun finishRecommendedStep() {
        runCatching {
            val g = com.armsx2.config.ConfigStore.loadGlobal()
            // First-run only: seed MTVU from the CPU rather than the persisted
            // Settings default (which we can't change without bleeding into
            // existing users' saved configs). Multi-threaded VU1 is a net win
            // only when there are spare cores (>= 6); on budget SoCs it costs
            // more than it saves.
            val mtvuDefault = com.armsx2.DeviceTier.mtvuDefault()
            val base = g.copy(
                aspectRatio = recAspect.value.coerceIn(0, 4),
                upscaleFloat = recUpscale.value.coerceIn(1, 8).toFloat(),
                antiBlur = recAntiBlur.value,
                enableWideScreenPatches = recWidescreen.value,
                deinterlaceMode = if (recDeinterlaceOff.value) 1 else 0,
                mtvu = mtvuDefault,
            )
            // Performance preset (mirrors the in-game PerformanceTab presets so the
            // wizard and settings agree):
            //   0 Optimal  — safe defaults.
            //   1 Fast     — EE cycle skip + fast CDVD + native res + Basic blending.
            //   2 Low-End  — the shared Settings.lowEndPreset: Fast plus every cheap
            //                GPU lever (Minimum blending, no mipmaps/palette-conv,
            //                Partial texture preload, ROV off, eeCycleSkip 1), MTVU
            //                already seeded device-aware in [base]. #124: this is what
            //                a detected low-end device now gets by default.
            val resolved = when (recPerfPreset.value) {
                2 -> com.armsx2.config.Settings.lowEndPreset(base.copy(eeCycleRate = 0, fastCDVD = true), mtvuDefault)
                1 -> base.copy(
                    eeCycleRate = 0,
                    eeCycleSkip = 2,
                    fastCDVD = true,
                    upscaleFloat = 1.0f,
                    accurateBlendingUnit = 1,
                )
                else -> base.copy(eeCycleRate = 0, eeCycleSkip = 0, fastCDVD = false)
            }
            com.armsx2.config.ConfigStore.saveGlobal(resolved)
        }
    }

    /** Reusable PS2-blue button colors. */
    @Composable
    private fun ps2Colors() = ButtonDefaults.buttonColors(
        containerColor = Colors.pasx2_blue,
        contentColor = Color.White,
        disabledContainerColor = Colors.pasx2_blue.copy(alpha = 0.30f),
        disabledContentColor = Color.White.copy(alpha = 0.50f),
    )

    @Composable
    fun SetupWindow() {
        val context = LocalContext.current
        val biosLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocumentTree()
        ) { treeUri: Uri? ->
            if (treeUri == null) return@rememberLauncherForActivityResult
            // Take persistable permission so the URI survives across app
            // restarts; the LaunchedEffect in SetupBiosContent picks up the
            // change to biosDirUri and runs the actual scan.
            try {
                context.contentResolver.takePersistableUriPermission(
                    treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
            } catch (_: SecurityException) { /* already persisted */ }
            biosDirUri.value = treeUri
            // Force a re-scan even if biosDirUri is the same Uri value as
            // before — guards against a user picking the same folder
            // through the picker dialog after a "no results" outcome.
            lastScannedDir.value = null
            showBiosChooser.value = true
            scanBiosDirectory(context, treeUri)
        }
        val romsLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocumentTree()
        ) { treeUri: Uri? ->
            if (treeUri != null) {
                // Take the persistable grant immediately so the URI survives
                // across app restarts even if the user closes the wizard
                // before tapping Next. De-duplicate by URI string so picking
                // the same folder twice doesn't add a redundant entry.
                try {
                    context.contentResolver.takePersistableUriPermission(
                        treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                } catch (_: SecurityException) { /* already persisted */ }
                if (romsDirsState.none { it.toString() == treeUri.toString() }) {
                    romsDirsState.add(treeUri)
                }
                refreshAllowNext()
            }
        }

        // github (all-files) only: pick ANY folder as the writable data root.
        // The picked tree URI resolves to a /storage path; with All-Files
        // Access the write-probe passes and finishSystemDirStep persists it.
        val systemFolderLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocumentTree()
        ) { treeUri: Uri? ->
            if (treeUri == null) return@rememberLauncherForActivityResult
            val posix = Main.resolveTreeUriToPosix(treeUri.toString())
            if (posix == null || !Main.validateSystemDirWritable(posix)) {
                systemDirError.value = if (!allFilesAccessGranted())
                    I18n.get("setup.systemDir.error.grantAllFiles")
                else
                    I18n.get("setup.systemDir.error.tryAnother")
                return@rememberLauncherForActivityResult
            }
            try {
                context.contentResolver.takePersistableUriPermission(
                    treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_GRANT_WRITE_URI_PERMISSION)
            } catch (_: SecurityException) { /* already persisted */ }
            systemDirUri.value = treeUri
            systemDirDisplay.value = treeUri.lastPathSegment?.substringAfterLast(':')?.ifBlank { null }
                ?: I18n.get("setup.systemDir.customFolder")
            systemDirUseDefault.value = false
            systemDirError.value = null
            refreshAllowNext()
        }

        // Tracks All-Files Access so the chooser's button label flips to
        // "Custom Folder…" the moment the user returns from granting it.
        // StartActivityForResult fires its callback on return regardless of
        // result code, so we just re-read the permission state then.
        val allFilesGranted = remember { mutableStateOf(allFilesAccessGranted()) }
        val allFilesSettingsLauncher = rememberLauncherForActivityResult(
            ActivityResultContracts.StartActivityForResult()
        ) {
            allFilesGranted.value = allFilesAccessGranted()
        }
        fun launchAllFilesAccess() {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return
            try {
                allFilesSettingsLauncher.launch(
                    Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + context.packageName)))
            } catch (_: Exception) {
                // Some OEMs don't honor the per-app action; fall back to the global list.
                try {
                    allFilesSettingsLauncher.launch(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
                } catch (_: Exception) { /* nothing else to try */ }
            }
        }

        // Internal (app-private) — the no-permission default. Shared by the
        // dashboard's Internal button and the github storage chooser.
        fun applyInternalSystem() {
            systemDirUseDefault.value = true
            systemDirUri.value = null
            systemDirDisplay.value = null
            systemDirError.value = null
            Main.systemDir.value = null
            Main.prefs.edit().remove("systemDir").apply()
            refreshAllowNext()
        }

        // SD Card — the SD's app-specific dir (raw-writable, no permission).
        // Falls fully back to Internal with a visible reason when no card is
        // present. Shared by the dashboard SD pick and the github chooser.
        fun applySdCardSystem() {
            val sd = Main.sdCardDataDir(context)
            if (sd == null) {
                systemDirError.value = I18n.get("setup.systemDir.error.noSdCard")
                systemDirUseDefault.value = true
                systemDirUri.value = null
                systemDirDisplay.value = null
                Main.systemDir.value = null
                Main.prefs.edit().remove("systemDir").apply()
            } else {
                Main.systemDir.value = sd
                Main.prefs.edit().putString("systemDir", sd).apply()
                systemDirUseDefault.value = false
                systemDirDisplay.value = I18n.get("setup.systemDir.sdCard")
                systemDirError.value = null
            }
            refreshAllowNext()
        }

        fun openBiosFlow() {
            if (biosDirUri.value == null) {
                Main.biosDir.value?.let { saved ->
                    biosDirUri.value = runCatching { Uri.parse(saved) }.getOrNull()
                }
            }
            val uri = biosDirUri.value
            if (uri != null) {
                showBiosChooser.value = true
                if (scannedBioses.isEmpty() && !biosScanning.value) {
                    lastScannedDir.value = null
                    scanBiosDirectory(context, uri)
                }
            } else if (scannedBioses.isNotEmpty() || Main.bios.value != null) {
                showBiosChooser.value = true
            } else {
                biosLauncher.launch(null)
            }
        }

        fun finishDashboard() {
            if (!dashboardReady()) {
                refreshAllowNext()
                return
            }
            if (finishSystemDirStep(context) == null) {
                refreshAllowNext()
                return
            }
            if (finishBiosStep(context) == null) {
                refreshAllowNext()
                return
            }
            if (finishRomsStep(context) == null) {
                refreshAllowNext()
                return
            }
            // Re-entry (settings cog) where the data root actually CHANGED:
            // native pinned EmuFolders::DataRoot once at startup and can't
            // hot-swap it, so the new location won't take effect until a cold
            // start. Confirm + restart instead of silently leaving data behind.
            // (First run has no prior init root → currentInitDataRoot() is null
            // and setupComplete is false, so it completes normally.)
            if (Main.setupComplete.value &&
                Main.currentInitDataRoot() != null &&
                Main.assetCopyRoot(context) != Main.currentInitDataRoot()
            ) {
                storageRestartPending.value = true
                return
            }
            // First run only: offer the optional Recommended Settings step before
            // finishing. It's an overlay with its own Skip / Finish, so it can NEVER
            // block first-run. Re-entry (Settings cog) finishes straight away as before.
            if (!Main.setupComplete.value) {
                loadRecommendedFromGlobal()
                showRecommended.value = true
                return
            }
            Main.finishSetup()
            allowPrev.value = false
            allowNext.value = false
        }

        fun finishRecommendedAndComplete(apply: Boolean) {
            if (apply) {
                finishRendererStep() // persist the OpenGL/Vulkan + driver pick
                finishRecommendedStep()
            }
            showRecommended.value = false
            allowPrev.value = false
            allowNext.value = false
            // First-run with a NON-internal data root (SD card / custom folder): bind it
            // via a COLD RESTART rather than the in-process init. native initialize() pins
            // EmuFolders::DataRoot once per process; pinning an SD/custom root in-process on
            // a fresh setup leaves games unable to boot — they bounce straight back to the
            // library. (Switching the root LATER already worked precisely because that path
            // cold-restarts; this mirrors it for first run.) Internal (systemDir == null)
            // binds fine in-process and needs no restart. Commit synchronously first because
            // restartApp's Runtime.exit() skips pending async SharedPreferences applies — and
            // ConfigStore shares Main.prefs, so this one commit also flushes the chosen root,
            // BIOS path, and any Recommended-Settings picks.
            if (Main.systemDir.value != null) {
                Main.prefs.edit().putBoolean("setupComplete", true).commit()
                Main.restartApp(context)
                return
            }
            Main.finishSetup()
        }

        Box(
            Modifier
                .fillMaxSize()
                .background(Color(0xFF030509)),
        ) {
            SetupBackdrop()
            when (setupState.value) {
                0 -> {
                    allowNext.value = true
                    allowPrev.value = false
                    PowerWelcome(
                        onPower = {
                            setupState.value = 1
                            allowPrev.value = true
                            refreshAllowNext()
                        },
                    )
                }
                else -> {
                    if (showRecommended.value) {
                        SetupRecommendedOverlay(
                            onSkip = { finishRecommendedAndComplete(apply = false) },
                            onFinish = { finishRecommendedAndComplete(apply = true) },
                        )
                    } else if (showBiosChooser.value) {
                        BiosChooserOverlay(
                            onBack = {
                                showBiosChooser.value = false
                                refreshAllowNext()
                            },
                            onPickDifferentFolder = { biosLauncher.launch(null) },
                            onUseSelected = {
                                showBiosChooser.value = false
                                refreshAllowNext()
                            },
                        )
                    } else {
                        SetupDashboard(
                            onBack = {
                                setupState.value = 0
                                allowPrev.value = false
                                allowNext.value = true
                            },
                            onUseDefaultSystem = { applyInternalSystem() },
                            onPickSystem = {
                                // play build: SD Card directly — arbitrary folders need
                                // all-files access, which the Play build avoids for policy
                                // compliance. github build: open the chooser so the user
                                // can pick Internal, SD, or ANY folder (All-Files Access).
                                if (BuildConfig.STORAGE_ALL_FILES) {
                                    showStorageChooser.value = true
                                } else {
                                    applySdCardSystem()
                                }
                            },
                            onPickBiosFolder = { openBiosFlow() },
                            onPickRoms = { romsLauncher.launch(null) },
                            onRemoveRoms = {
                                romsDirsState.remove(it)
                                refreshAllowNext()
                            },
                            onFinish = { finishDashboard() },
                        )
                    }
                }
            }
            if (storageRestartPending.value) {
                StorageRestartOverlay(
                    onRestart = {
                        Main.finishSetup()
                        Main.restartApp(context)
                    },
                    onCancel = { storageRestartPending.value = false },
                )
            }
            if (showStorageChooser.value) {
                StorageChooserOverlay(
                    granted = allFilesGranted.value,
                    onInternal = { applyInternalSystem(); showStorageChooser.value = false },
                    onSdCard = { applySdCardSystem(); showStorageChooser.value = false },
                    onCustomFolder = {
                        // Need All-Files Access first; granting it refreshes
                        // allFilesGranted (launcher callback) so the button flips to
                        // "Custom Folder…" and a second tap opens the picker.
                        if (allFilesGranted.value) {
                            showStorageChooser.value = false
                            systemFolderLauncher.launch(null)
                        } else {
                            launchAllFilesAccess()
                        }
                    },
                    onDismiss = { showStorageChooser.value = false },
                )
            }
        }
    }

    @Composable
    private fun StorageRestartOverlay(onRestart: () -> Unit, onCancel: () -> Unit) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.78f))
                // Absorb taps so the setup behind the modal can't be touched.
                .clickable(onClick = {}),
            contentAlignment = Alignment.Center,
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth(0.82f)
                    .clip(RoundedCornerShape(16.dp))
                    .background(Color(0xFF0C1418))
                    .border(1.dp, Color(0xFF38D5CB).copy(alpha = 0.35f), RoundedCornerShape(16.dp))
                    .padding(20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(14.dp),
            ) {
                Text(
                    str("setup.restart.title"),
                    color = Color(0xFF7CF6EF),
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Black,
                )
                Text(
                    str("setup.restart.message"),
                    color = Color.White.copy(alpha = 0.82f),
                    fontSize = 13.sp,
                )
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    SetupMiniButton(
                        text = str("action.cancel"),
                        modifier = Modifier.weight(1f).height(40.dp),
                        onClick = onCancel,
                    )
                    SetupMiniButton(
                        text = str("setup.restart.now"),
                        modifier = Modifier.weight(1f).height(40.dp),
                        onClick = onRestart,
                    )
                }
            }
        }
    }

    /** github (all-files) storage picker. Internal / SD / any folder. The
     *  Custom Folder button first ensures All-Files Access (its label flips to
     *  "Grant All-Files Access…" until granted), then opens the folder picker.
     *  Never shown on the play build. */
    @Composable
    private fun StorageChooserOverlay(
        granted: Boolean,
        onInternal: () -> Unit,
        onSdCard: () -> Unit,
        onCustomFolder: () -> Unit,
        onDismiss: () -> Unit,
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color.Black.copy(alpha = 0.78f))
                // Absorb taps on the scrim so the setup behind can't be touched.
                .clickable(onClick = {}),
            contentAlignment = Alignment.Center,
        ) {
            Column(
                modifier = Modifier
                    .fillMaxWidth(0.82f)
                    .clip(RoundedCornerShape(16.dp))
                    .background(Color(0xFF0C1418))
                    .border(1.dp, Color(0xFF38D5CB).copy(alpha = 0.35f), RoundedCornerShape(16.dp))
                    .padding(20.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                Text(
                    str("setup.storageChooser.title"),
                    color = Color(0xFF7CF6EF),
                    fontSize = 16.sp,
                    fontWeight = FontWeight.Black,
                )
                Text(
                    str("setup.storageChooser.description"),
                    color = Color.White.copy(alpha = 0.82f),
                    fontSize = 12.sp,
                )
                SetupMiniButton(
                    text = str("setup.storageChooser.internal"),
                    modifier = Modifier.fillMaxWidth().height(40.dp),
                    onClick = onInternal,
                )
                SetupMiniButton(
                    text = str("setup.systemDir.sdCard"),
                    modifier = Modifier.fillMaxWidth().height(40.dp),
                    onClick = onSdCard,
                )
                SetupMiniButton(
                    text = if (granted) str("setup.storageChooser.customFolder") else str("setup.storageChooser.grantAllFiles"),
                    modifier = Modifier.fillMaxWidth().height(40.dp),
                    onClick = onCustomFolder,
                )
                SetupMiniButton(
                    text = str("action.cancel"),
                    modifier = Modifier.fillMaxWidth().height(40.dp),
                    onClick = onDismiss,
                )
            }
        }
    }

    @Composable
    private fun SetupBackdrop() {
        Canvas(Modifier.fillMaxSize()) {
            drawRect(
                Brush.radialGradient(
                    colors = listOf(Color(0xFF101928), Color(0xFF030509)),
                    center = Offset(size.width * 0.5f, size.height * 0.24f),
                    radius = size.maxDimension * 0.75f,
                )
            )
            val dotColor = Color.White.copy(alpha = 0.06f)
            val step = 18.dp.toPx()
            var y = 0f
            var row = 0
            while (y < size.height) {
                var x = (row % 2) * step * 0.5f
                while (x < size.width) {
                    drawCircle(dotColor, radius = 0.7.dp.toPx(), center = Offset(x, y))
                    x += step
                }
                y += step
                row++
            }
        }
    }

    @Composable
    private fun PowerWelcome(onPower: () -> Unit) {
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxSize(),
            contentAlignment = Alignment.Center,
        ) {
            val artRatio = 1440f / 3120f
            if (maxWidth > maxHeight) {
                LandscapePowerWelcome(onPower)
                return@BoxWithConstraints
            }
            Image(
                painter = painterResource(id = R.drawable.setup_aero_bg),
                contentDescription = null,
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop,
            )
            val frameModifier = if (maxWidth.value / maxHeight.value > artRatio) {
                Modifier.fillMaxHeight().aspectRatio(artRatio)
            } else {
                Modifier.fillMaxWidth().aspectRatio(artRatio)
            }
            // No contentAlignment here: the default BoxWithConstraints
            // alignment is TopStart, so the offset() below is measured from
            // the frame's top-left — the same convention AssetSetupDashboard
            // relies on. (A previous Alignment.Center pushed the hit target
            // off the card, which is why tapping power did nothing.)
            BoxWithConstraints(frameModifier) {
                Image(
                    painter = painterResource(id = R.drawable.setup_welcome_portrait),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.FillBounds,
                )
                Box(
                    modifier = Modifier
                        .offset(x = maxWidth * 0.405f, y = maxHeight * 0.787f)
                        .size(width = maxWidth * 0.19f, height = maxWidth * 0.19f)
                        .clickable(onClick = onPower),
                )
            }
        }
    }

    @Composable
    private fun LandscapePowerWelcome(onPower: () -> Unit) {
        BoxWithConstraints(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.TopStart,
        ) {
            val frameRatio = 1920f / 1080f
            val screenRatio = maxWidth.value / maxHeight.value
            val renderedWidth = if (screenRatio > frameRatio) maxWidth else maxHeight * frameRatio
            val renderedHeight = if (screenRatio > frameRatio) maxWidth / frameRatio else maxHeight
            val renderedX = (maxWidth - renderedWidth) / 2f
            val renderedY = (maxHeight - renderedHeight) / 2f
            Image(
                painter = painterResource(id = R.drawable.setup_welcome_landscape),
                contentDescription = null,
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Crop,
            )
            Box(
                modifier = Modifier
                    .offset(
                        x = renderedX + renderedWidth * (1240f / 1920f),
                        y = renderedY + renderedHeight * (625f / 1080f),
                    )
                    .size(
                        width = renderedWidth * (260f / 1920f),
                        height = renderedHeight * (274f / 1080f),
                    )
                    .clickable(onClick = onPower),
            )
        }
    }

    @Composable
    private fun HeroLogo(modifier: Modifier = Modifier) {
        Box(modifier = modifier, contentAlignment = Alignment.Center) {
            Image(
                painter = painterResource(id = R.drawable.savetowerforeground),
                contentDescription = null,
                modifier = Modifier
                    .fillMaxHeight(0.72f)
                    .aspectRatio(1f),
            )
        }
    }

    @Composable
    private fun PowerButton(onPower: () -> Unit) {
        Box(
            modifier = Modifier
                .size(116.dp)
                .clip(RoundedCornerShape(14.dp))
                .background(
                    Brush.verticalGradient(
                        listOf(Color(0xFF171D1E), Color(0xFF030405))
                    )
                )
                .border(3.dp, Color.Black.copy(alpha = 0.78f), RoundedCornerShape(14.dp))
                .clickable(onClick = onPower),
            contentAlignment = Alignment.Center,
        ) {
            Canvas(Modifier.fillMaxSize()) {
                val teal = Color(0xFF2FD0C8)
                val center = Offset(size.width / 2f, size.height / 2f + 7.dp.toPx())
                drawCircle(teal.copy(alpha = 0.22f), radius = 42.dp.toPx(), center = center)
                drawArc(
                    color = teal,
                    startAngle = 135f,
                    sweepAngle = 270f,
                    useCenter = false,
                    topLeft = Offset(center.x - 30.dp.toPx(), center.y - 30.dp.toPx()),
                    size = Size(60.dp.toPx(), 60.dp.toPx()),
                    style = Stroke(width = 7.dp.toPx()),
                )
                drawLine(
                    color = teal,
                    start = Offset(center.x, center.y - 43.dp.toPx()),
                    end = Offset(center.x, center.y - 6.dp.toPx()),
                    strokeWidth = 8.dp.toPx(),
                )
                drawCircle(
                    color = Color(0xFF7BFF78),
                    radius = 7.dp.toPx(),
                    center = Offset(size.width - 23.dp.toPx(), 22.dp.toPx()),
                )
                drawCircle(
                    color = Color(0xFF7BFF78).copy(alpha = 0.20f),
                    radius = 14.dp.toPx(),
                    center = Offset(size.width - 23.dp.toPx(), 22.dp.toPx()),
                )
            }
        }
    }

    @Composable
    private fun SetupDashboard(
        onBack: () -> Unit,
        onUseDefaultSystem: () -> Unit,
        onPickSystem: () -> Unit,
        onPickBiosFolder: () -> Unit,
        onPickRoms: () -> Unit,
        onRemoveRoms: (Uri) -> Unit,
        onFinish: () -> Unit,
    ) {
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxSize()
                .padding(18.dp),
        ) {
            val wide = maxWidth > maxHeight
            if (!wide) {
                AssetSetupDashboard(
                    onBack = onBack,
                    onUseDefaultSystem = onUseDefaultSystem,
                    onPickSystem = onPickSystem,
                    onPickBiosFolder = onPickBiosFolder,
                    onPickRoms = onPickRoms,
                    onRemoveRoms = onRemoveRoms,
                    onFinish = onFinish,
                )
                return@BoxWithConstraints
            }
            val horizontalPadding = if (wide) 44.dp else 34.dp
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .clip(RoundedCornerShape(32.dp))
                    .background(Color(0xFF050810).copy(alpha = 0.82f))
                    .border(1.dp, Color.White.copy(alpha = 0.06f), RoundedCornerShape(32.dp))
                    .padding(horizontal = horizontalPadding, vertical = if (wide) 18.dp else 34.dp),
            ) {
                LazyColumn(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(if (wide) 14.dp else 26.dp),
                ) {
                    item {
                        SetupStepCard(
                            step = "1.",
                            title = str("setup.step.appData.title"),
                            description = if (BuildConfig.STORAGE_ALL_FILES)
                                str("setup.step.appData.description.allFiles")
                            else
                                str("setup.step.appData.description.play"),
                            ready = appFolderReady(),
                            status = appFolderStatus(),
                            visual = SetupVisual.Folder,
                            // github build: one button opens the Internal/SD/Custom chooser.
                            // play build: SD Card / Internal as before.
                            onClick = if (BuildConfig.STORAGE_ALL_FILES) onPickSystem else onUseDefaultSystem,
                            primaryLabel = if (BuildConfig.STORAGE_ALL_FILES) str("setup.button.chooseDataLocation") else str("setup.systemDir.sdCard"),
                            onPrimary = onPickSystem,
                            secondaryLabel = if (BuildConfig.STORAGE_ALL_FILES) null else str("setup.storageChooser.internalShort"),
                            onSecondary = if (BuildConfig.STORAGE_ALL_FILES) null else onUseDefaultSystem,
                        )
                    }
                    item {
                        SetupStepCard(
                            step = "2.",
                            title = str("setup.step.bios.title"),
                            description = str("setup.step.bios.description"),
                            ready = biosReady(),
                            status = biosStatus(),
                            visual = SetupVisual.Bios,
                            onClick = onPickBiosFolder,
                            primaryLabel = if (biosScanning.value) str("setup.button.scanning") else str("setup.button.scanFolder"),
                            onPrimary = onPickBiosFolder,
                            secondaryLabel = null,
                            onSecondary = null,
                        )
                    }
                    item {
                        SetupStepCard(
                            step = "3.",
                            title = str("setup.step.rom.title"),
                            description = str("setup.step.rom.description"),
                            ready = romsReady(),
                            status = romsStatus(),
                            visual = SetupVisual.Disc,
                            onClick = onPickRoms,
                            primaryLabel = if (romsDirsState.isEmpty()) str("setup.button.selectFolder") else str("setup.button.addFolder"),
                            onPrimary = onPickRoms,
                            secondaryLabel = null,
                            onSecondary = null,
                        )
                        if (romsDirsState.isNotEmpty()) {
                            Spacer(Modifier.height(8.dp))
                            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                                for (uri in romsDirsState.toList()) {
                                    CompactFolderRow(uri, onRemove = { onRemoveRoms(uri) })
                                }
                            }
                        }
                    }
                }

                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(70.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    CircleBackButton(onBack)
                    Spacer(Modifier.weight(1f))
                    GlowActionButton(
                        text = str("setup.button.letsGo"),
                        enabled = dashboardReady(),
                        onClick = onFinish,
                        modifier = Modifier.fillMaxWidth(if (wide) 0.42f else 0.68f),
                    )
                }
            }
        }
    }

    @Composable
    private fun AssetSetupDashboard(
        onBack: () -> Unit,
        onUseDefaultSystem: () -> Unit,
        onPickSystem: () -> Unit,
        onPickBiosFolder: () -> Unit,
        onPickRoms: () -> Unit,
        onRemoveRoms: (Uri) -> Unit,
        onFinish: () -> Unit,
    ) {
        BoxWithConstraints(
            modifier = Modifier.fillMaxSize(),
            contentAlignment = Alignment.Center,
        ) {
            val artRatio = 1440f / 3120f
            val frameModifier = if (maxWidth.value / maxHeight.value > artRatio) {
                Modifier.fillMaxHeight().aspectRatio(artRatio)
            } else {
                Modifier.fillMaxWidth().aspectRatio(artRatio)
            }
            BoxWithConstraints(frameModifier) {
                Image(
                    painter = painterResource(id = R.drawable.setup_files_portrait),
                    contentDescription = null,
                    modifier = Modifier.fillMaxSize(),
                    contentScale = ContentScale.FillBounds,
                )

                SetupTapZone(
                    modifier = Modifier
                        .offset(x = maxWidth * 0.06f, y = maxHeight * 0.07f)
                        // Height trimmed (0.20→0.17) so this SD-Card hit area no
                        // longer overlaps the "Internal" pill below (at y 0.252+),
                        // which otherwise stole taps near the boundary.
                        .size(width = maxWidth * 0.88f, height = maxHeight * 0.17f),
                    onClick = onPickSystem,
                )
                SetupTapZone(
                    modifier = Modifier
                        .offset(x = maxWidth * 0.06f, y = maxHeight * 0.34f)
                        .size(width = maxWidth * 0.88f, height = maxHeight * 0.22f),
                    onClick = onPickBiosFolder,
                )
                SetupTapZone(
                    modifier = Modifier
                        .offset(x = maxWidth * 0.06f, y = maxHeight * 0.61f)
                        .size(width = maxWidth * 0.88f, height = maxHeight * 0.23f),
                    onClick = onPickRoms,
                )

                SetupStatusChip(
                    text = appFolderStatus(),
                    ready = appFolderReady(),
                    modifier = Modifier
                        .offset(x = maxWidth * 0.11f, y = maxHeight * 0.255f)
                        .size(width = maxWidth * 0.50f, height = maxHeight * 0.032f),
                )
                SetupMiniButton(
                    // github: one chooser entry point; play: the Internal shortcut.
                    text = if (BuildConfig.STORAGE_ALL_FILES) str("setup.button.choose") else str("setup.storageChooser.internalShort"),
                    modifier = Modifier
                        .offset(x = maxWidth * 0.68f, y = maxHeight * 0.252f)
                        .size(width = maxWidth * 0.20f, height = maxHeight * 0.038f),
                    onClick = if (BuildConfig.STORAGE_ALL_FILES) onPickSystem else onUseDefaultSystem,
                )
                // App-data error (e.g. "No SD card detected") — rendered HERE so a
                // failed SD-Card tap is visible. Previously systemDirError only
                // showed on SetupSystemDirContent, which this dashboard never opens,
                // so picking SD on a device with no card appeared to do nothing.
                systemDirError.value?.let { err ->
                    SetupStatusChip(
                        text = err,
                        ready = false,
                        modifier = Modifier
                            .offset(x = maxWidth * 0.11f, y = maxHeight * 0.295f)
                            .size(width = maxWidth * 0.82f, height = maxHeight * 0.04f),
                    )
                }
                SetupStatusChip(
                    text = biosStatus(),
                    ready = biosReady(),
                    modifier = Modifier
                        .offset(x = maxWidth * 0.11f, y = maxHeight * 0.535f)
                        .size(width = maxWidth * 0.68f, height = maxHeight * 0.035f),
                )
                SetupStatusChip(
                    text = romsStatus(),
                    ready = romsReady(),
                    modifier = Modifier
                        .offset(x = maxWidth * 0.11f, y = maxHeight * 0.812f)
                        .size(width = maxWidth * 0.68f, height = maxHeight * 0.035f),
                )
                if (romsDirsState.isNotEmpty()) {
                    SetupMiniButton(
                        text = str("setup.button.clear"),
                        modifier = Modifier
                            .offset(x = maxWidth * 0.78f, y = maxHeight * 0.809f)
                            .size(width = maxWidth * 0.12f, height = maxHeight * 0.038f),
                        onClick = {
                            for (uri in romsDirsState.toList()) {
                                onRemoveRoms(uri)
                            }
                        },
                    )
                }

                SetupTapZone(
                    modifier = Modifier
                        .offset(x = maxWidth * 0.065f, y = maxHeight * 0.90f)
                        .size(width = maxWidth * 0.17f, height = maxWidth * 0.17f),
                    onClick = onBack,
                )
                Box(
                    modifier = Modifier
                        .offset(x = maxWidth * 0.28f, y = maxHeight * 0.895f)
                        .size(width = maxWidth * 0.60f, height = maxHeight * 0.075f)
                        .clip(RoundedCornerShape(12.dp))
                        .clickable(enabled = dashboardReady(), onClick = onFinish),
                ) {
                    if (!dashboardReady()) {
                        Box(
                            Modifier
                                .fillMaxSize()
                                .background(Color.Black.copy(alpha = 0.48f)),
                        )
                    }
                }
            }
        }
    }

    @Composable
    private fun SetupTapZone(modifier: Modifier, onClick: () -> Unit) {
        Box(modifier = modifier.clickable(onClick = onClick))
    }

    @Composable
    private fun SetupStatusChip(text: String, ready: Boolean, modifier: Modifier) {
        Box(
            modifier = modifier
                .clip(RoundedCornerShape(50))
                .background(Color.Black.copy(alpha = 0.46f))
                .border(
                    1.dp,
                    if (ready) Color(0xFF3CEBE0).copy(alpha = 0.60f) else Color.White.copy(alpha = 0.12f),
                    RoundedCornerShape(50),
                )
                .padding(horizontal = 10.dp, vertical = 2.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Text(
                text,
                color = if (ready) Color(0xFFBFFFFA) else Color.White.copy(alpha = 0.70f),
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }

    @Composable
    private fun SetupMiniButton(text: String, modifier: Modifier, onClick: () -> Unit) {
        Box(
            modifier = modifier
                .clip(RoundedCornerShape(50))
                .background(Color(0xFF12191B).copy(alpha = 0.78f))
                .border(1.dp, Color(0xFF38D5CB).copy(alpha = 0.45f), RoundedCornerShape(50))
                .clickable(onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Text(text, color = Color(0xFF7CF6EF), fontSize = 11.sp, fontWeight = FontWeight.Black)
        }
    }

    @Composable
    private fun BiosChooserOverlay(
        onBack: () -> Unit,
        onPickDifferentFolder: () -> Unit,
        onUseSelected: () -> Unit,
    ) {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Color(0xFF030509))
                .padding(18.dp),
        ) {
            SetupBackdrop()
            Column(
                modifier = Modifier
                    .fillMaxSize()
                    .clip(RoundedCornerShape(28.dp))
                    .background(Color(0xFF080B11).copy(alpha = 0.94f))
                    .border(1.dp, Color.White.copy(alpha = 0.10f), RoundedCornerShape(28.dp))
                    .padding(18.dp),
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        str("setup.bios.selectTitle"),
                        color = Color.White,
                        fontSize = 28.sp,
                        fontWeight = FontWeight.Light,
                    )
                    Spacer(Modifier.weight(1f))
                    SmallSetupButton(str("setup.button.scanDifferentFolder"), strong = false, onClick = onPickDifferentFolder)
                }
                Spacer(Modifier.height(12.dp))
                Box(
                    modifier = Modifier
                        .weight(1f)
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp))
                        .background(Color.Black.copy(alpha = 0.26f))
                        .padding(12.dp),
                ) {
                    SetupBiosContent()
                }
                Spacer(Modifier.height(14.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    CircleBackButton(onBack)
                    Spacer(Modifier.weight(1f))
                    GlowActionButton(
                        text = str("setup.bios.useSelected"),
                        enabled = selectedBiosIdx.value != null,
                        onClick = onUseSelected,
                        modifier = Modifier.fillMaxWidth(0.68f),
                    )
                }
            }
        }
    }

    private enum class SetupVisual { Folder, Bios, Disc }

    @Composable
    private fun SetupStepCard(
        step: String,
        title: String,
        description: String,
        ready: Boolean,
        status: String,
        visual: SetupVisual,
        onClick: () -> Unit,
        primaryLabel: String,
        onPrimary: () -> Unit,
        secondaryLabel: String?,
        onSecondary: (() -> Unit)?,
    ) {
        BoxWithConstraints(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(26.dp))
                .background(
                    Brush.verticalGradient(
                        listOf(
                            Color.White.copy(alpha = if (ready) 0.30f else 0.13f),
                            Color.White.copy(alpha = if (ready) 0.08f else 0.04f),
                        )
                    )
                )
                .border(
                    1.dp,
                    if (ready) Colors.pasx2_blue.copy(alpha = 0.52f) else Color.White.copy(alpha = 0.12f),
                    RoundedCornerShape(26.dp)
                )
                .clickable(onClick = onClick)
                .padding(20.dp),
        ) {
            val compact = maxWidth < 640.dp
            if (compact) {
                Column {
                    StepText(step, title, description, status, ready)
                    Spacer(Modifier.height(14.dp))
                    StepVisual(visual, Modifier.fillMaxWidth().height(110.dp))
                    Spacer(Modifier.height(14.dp))
                    StepActions(primaryLabel, onPrimary, secondaryLabel, onSecondary)
                }
            } else {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(modifier = Modifier.weight(1f)) {
                        StepText(step, title, description, status, ready)
                        Spacer(Modifier.height(14.dp))
                        StepActions(primaryLabel, onPrimary, secondaryLabel, onSecondary)
                    }
                    Spacer(Modifier.width(18.dp))
                    StepVisual(visual, Modifier.width(210.dp).height(128.dp))
                }
            }
        }
    }

    @Composable
    private fun StepText(step: String, title: String, description: String, status: String, ready: Boolean) {
        Text(
            "$step $title",
            color = Color.White,
            fontSize = 34.sp,
            fontWeight = FontWeight.Light,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
        Spacer(Modifier.height(7.dp))
        Text(description, color = Color.White.copy(alpha = 0.82f), fontSize = 14.sp, lineHeight = 19.sp)
        Spacer(Modifier.height(8.dp))
        Text(
            status,
            color = if (ready) Color(0xFF75FFF4) else Color.White.copy(alpha = 0.52f),
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            maxLines = 2,
            overflow = TextOverflow.Ellipsis,
        )
    }

    @Composable
    private fun StepActions(
        primaryLabel: String,
        onPrimary: () -> Unit,
        secondaryLabel: String?,
        onSecondary: (() -> Unit)?,
    ) {
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            SmallSetupButton(primaryLabel, strong = true, onClick = onPrimary)
            if (secondaryLabel != null && onSecondary != null) {
                SmallSetupButton(secondaryLabel, strong = false, onClick = onSecondary)
            }
        }
    }

    @Composable
    private fun SmallSetupButton(text: String, strong: Boolean, onClick: () -> Unit) {
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(9.dp))
                .background(if (strong) Colors.pasx2_blue.copy(alpha = 0.75f) else Color.Black.copy(alpha = 0.32f))
                .border(1.dp, Color.White.copy(alpha = 0.12f), RoundedCornerShape(9.dp))
                .clickable(onClick = onClick)
                .padding(horizontal = 14.dp, vertical = 8.dp),
            contentAlignment = Alignment.Center,
        ) {
            Text(text, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold)
        }
    }

    @Composable
    private fun StepVisual(type: SetupVisual, modifier: Modifier = Modifier) {
        // Vivi's page-2 logos (cropped from her Files-Portrait artwork), replacing
        // the old programmatically-drawn icons. Each is shown in a dark rounded
        // tile so the logo's own near-black backdrop blends into the card.
        val res = when (type) {
            SetupVisual.Folder -> R.drawable.setup_logo_folder
            SetupVisual.Bios -> R.drawable.setup_logo_bios
            SetupVisual.Disc -> R.drawable.setup_logo_disc
        }
        Box(
            modifier = modifier
                .clip(RoundedCornerShape(14.dp))
                .background(Color(0xFF090C13)),
            contentAlignment = Alignment.Center,
        ) {
            Image(
                painter = painterResource(id = res),
                contentDescription = null,
                modifier = Modifier.fillMaxSize(),
                contentScale = ContentScale.Fit,
            )
        }
    }

    @Composable
    private fun CircleBackButton(onClick: () -> Unit) {
        Box(
            modifier = Modifier
                .size(54.dp)
                .clip(RoundedCornerShape(50))
                .background(Color.White.copy(alpha = 0.10f))
                .clickable(onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Text("<", color = Color.White, fontSize = 28.sp, fontWeight = FontWeight.Light)
        }
    }

    @Composable
    private fun GlowActionButton(
        text: String,
        enabled: Boolean,
        onClick: () -> Unit,
        modifier: Modifier = Modifier,
    ) {
        Box(
            modifier = modifier
                .height(58.dp)
                .clip(RoundedCornerShape(12.dp))
                .background(
                    Brush.verticalGradient(
                        listOf(
                            Color(0xFF1B2426).copy(alpha = if (enabled) 1f else 0.45f),
                            Color.Black.copy(alpha = if (enabled) 0.92f else 0.45f),
                        )
                    )
                )
                .border(3.dp, Color.Black.copy(alpha = 0.82f), RoundedCornerShape(12.dp))
                .clickable(enabled = enabled, onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                text,
                color = if (enabled) Color(0xFF37D7CE) else Color.White.copy(alpha = 0.32f),
                fontSize = 25.sp,
                fontWeight = FontWeight.Black,
            )
        }
    }

    @Composable
    private fun CompactFolderRow(uri: Uri, onRemove: () -> Unit) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(10.dp))
                .background(Color.Black.copy(alpha = 0.30f))
                .padding(horizontal = 12.dp, vertical = 8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                uri.lastPathSegment ?: uri.toString(),
                color = Color.White.copy(alpha = 0.82f),
                fontSize = 12.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.weight(1f),
            )
            Spacer(Modifier.width(8.dp))
            Text(
                str("setup.button.remove"),
                color = Color(0xFFFF8B8B),
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.clickable(onClick = onRemove),
            )
        }
    }

    private fun appFolderStatus(): String {
        if (systemDirUseDefault.value || (Main.systemDir.value == null && Main.setupComplete.value)) {
            return I18n.get("setup.status.internalStorage")
        }
        return systemDirDisplay.value
            ?: Main.systemDir.value?.let { runCatching { Uri.parse(it).lastPathSegment }.getOrNull() ?: it }
            ?: I18n.get("setup.status.notSelected")
    }

    private fun biosStatus(): String {
        if (biosScanning.value) {
            return I18n.get("setup.status.scanningBios")
        }
        selectedBiosIdx.value?.let { idx ->
            return scannedBioses.getOrNull(idx)?.displayName ?: I18n.get("setup.status.biosSelected")
        }
        if (scannedBioses.isNotEmpty()) {
            return "${scannedBioses.size} BIOS files found - tap to choose"
        }
        return Main.bios.value?.let { File(it).name }
            ?: biosScanError.value
            ?: I18n.get("setup.status.notSelected")
    }

    private fun romsStatus(): String =
        when (val count = romsDirsState.size) {
            0 -> I18n.get("setup.status.notSelected")
            1 -> romsDirsState.first().lastPathSegment ?: I18n.get("setup.status.oneFolderSelected")
            else -> "$count folders selected"
        }

    /** First-run welcome page. Upscale defaults to 1x and is exposed in the
     *  in-game overlay's Renderer tab for runtime override. Renderer is
     *  picked explicitly on the next page so the SW path knows which host
     *  display backend to host the software frame on. */
    @Composable
    fun Welcome() {
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.Center) {
            Text(str("setup.welcome.heading"), Modifier.align(Alignment.CenterHorizontally),
                fontSize = 28.sp, color = Color.White, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(8.dp))
            Text(str("setup.welcome.subheading"), Modifier.align(Alignment.CenterHorizontally),
                fontSize = 14.sp, color = Color.LightGray)
        }
    }

    /** Renderer page — two large tile choices: OpenGL or Vulkan. The pick
     *  is the host display backend used for BOTH HW (OGL / VK) and SW
     *  (software GS, presenting via the chosen backend). The in-game
     *  overlay's Renderer pill still toggles between HW and SW within the
     *  chosen backend; switching backends after first-run requires
     *  re-entering setup from the Settings cog. */
    @Composable
    private fun SetupRendererContent() {
        val context = LocalContext.current
        // First-enter populate of installedDrivers; cheap, only hits disk
        // once per wizard session. Also re-runs when the Vulkan tile
        // becomes selected since the user might have just toggled to it.
        LaunchedEffect(selectedRenderer.value) {
            if (selectedRenderer.value == "vulkan") {
                installedDrivers.clear()
                installedDrivers.addAll(CustomDriver.listInstalled(context))
            }
        }

        // The driver browser takes over the page when open — full content
        // area, mirrors the BIOS-rescan modality.
        if (showDriverBrowser.value) {
            DriverBrowserSheet()
            return
        }

        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                RendererTile(
                    title = "OpenGL",
                    selected = selectedRenderer.value == "opengl",
                    modifier = Modifier.weight(1f),
                    onClick = {
                        selectedRenderer.value = "opengl"
                        refreshAllowNext()
                    },
                )
                RendererTile(
                    title = "Vulkan",
                    selected = selectedRenderer.value == "vulkan",
                    modifier = Modifier.weight(1f),
                    onClick = {
                        selectedRenderer.value = "vulkan"
                        refreshAllowNext()
                    },
                )
            }

            if (selectedRenderer.value == "vulkan") {
                Spacer(Modifier.height(20.dp))
                GpuDriverSection()
            }
        }
    }

    /** Optional first-run "Recommended Settings" page (PCSX2-wizard style). Shown as
     *  an overlay AFTER the core setup completes, so it can't block first-run; every
     *  choice has a sensible default and the whole step is skippable. */
    @Composable
    private fun SetupRecommendedOverlay(onSkip: () -> Unit, onFinish: () -> Unit) {
        // The GPU driver browser takes over the whole page when open (same as the
        // renderer page does), so a custom Vulkan driver can be installed here.
        if (showDriverBrowser.value) {
            DriverBrowserSheet()
            return
        }
        val blue = Color(0xFF1E6FE0)
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 8.dp),
        ) {
            Text(str("setup.recommended.title"), color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Bold)
            Text(
                str("setup.recommended.subtitle"),
                color = Color(0xFFAAAAAA),
                fontSize = 12.sp,
                modifier = Modifier.padding(top = 2.dp, bottom = 14.dp),
            )
            RecChoiceRow(str("setup.recommended.renderer"), listOf("OpenGL", "Vulkan"), if (selectedRenderer.value == "vulkan") 1 else 0, blue) {
                selectedRenderer.value = if (it == 1) "vulkan" else "opengl"
            }
            if (selectedRenderer.value == "vulkan") {
                Spacer(Modifier.height(2.dp))
                GpuDriverSection()
                Spacer(Modifier.height(12.dp))
            }
            RecChoiceRow(str("setup.recommended.aspectRatio"), listOf(str("setup.aspect.stretch"), str("setup.aspect.auto"), "4:3", "16:9", "10:7"), recAspect.value, blue) { recAspect.value = it }
            RecChoiceRow(str("setup.recommended.internalResolution"), listOf("1x", "2x", "3x", "4x", "5x", "6x", "7x", "8x"), recUpscale.value - 1, blue) { recUpscale.value = it + 1 }
            RecChoiceRow(str("setup.recommended.antiBlur"), listOf(str("setup.toggle.off"), str("setup.toggle.on")), if (recAntiBlur.value) 1 else 0, blue) { recAntiBlur.value = it == 1 }
            RecChoiceRow(str("setup.recommended.widescreenPatches"), listOf(str("setup.toggle.off"), str("setup.toggle.on")), if (recWidescreen.value) 1 else 0, blue) { recWidescreen.value = it == 1 }
            RecChoiceRow(str("setup.recommended.deinterlacing"), listOf(str("setup.aspect.auto"), str("setup.toggle.off")), if (recDeinterlaceOff.value) 1 else 0, blue) { recDeinterlaceOff.value = it == 1 }
            RecChoiceRow(str("setup.recommended.performance"), listOf(str("setup.perf.optimal"), str("setup.perf.fast"), str("setup.perf.lowEnd")), recPerfPreset.value.coerceIn(0, 2), blue) { recPerfPreset.value = it }
            Spacer(Modifier.height(20.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Button(
                    onClick = onSkip,
                    colors = ButtonDefaults.buttonColors(containerColor = Color(0xFF2A2A2A), contentColor = Color.White),
                    shape = RoundedCornerShape(8.dp),
                ) { Text(str("setup.button.skip")) }
                Button(
                    onClick = onFinish,
                    colors = ButtonDefaults.buttonColors(containerColor = blue, contentColor = Color.White),
                    shape = RoundedCornerShape(8.dp),
                ) { Text(str("setup.button.applyFinish")) }
            }
            Spacer(Modifier.height(14.dp))
        }
    }

    @Composable
    private fun RecChoiceRow(
        label: String,
        options: List<String>,
        selected: Int,
        accent: Color,
        onSelect: (Int) -> Unit,
    ) {
        Column(Modifier.padding(bottom = 12.dp)) {
            Text(label, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
            Row(
                modifier = Modifier.padding(top = 4.dp),
                horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                options.forEachIndexed { i, opt ->
                    val sel = i == selected
                    Button(
                        onClick = { onSelect(i) },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = if (sel) accent else Color(0xFF24262B),
                            contentColor = Color.White,
                        ),
                        shape = RoundedCornerShape(6.dp),
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                    ) { Text(opt, fontSize = 12.sp) }
                }
            }
        }
    }

    @Composable
    private fun RendererTile(
        title: String,
        selected: Boolean,
        modifier: Modifier = Modifier,
        onClick: () -> Unit,
    ) {
        val bg = if (selected) Colors.pasx2_blue.copy(alpha = 0.30f) else Color(0xFF1F2123)
        val border = if (selected) Colors.pasx2_blue else Color.White.copy(alpha = 0.10f)
        // Smaller tiles than first-pass — the GPU Driver section needs the
        // vertical room below, and the title alone reads fine without a
        // 32dp pad.
        Box(
            modifier = modifier
                .clip(RoundedCornerShape(10.dp))
                .background(bg)
                .border(2.dp, border, RoundedCornerShape(10.dp))
                .clickable(onClick = onClick)
                .padding(vertical = 14.dp),
            contentAlignment = Alignment.Center,
        ) {
            Text(title, color = Color.White, fontSize = 20.sp, fontWeight = FontWeight.Bold)
        }
    }

    /** Horizontal scrolling list of driver chips. First chip is "Default
     *  (system loader)", then each installed driver, then a trailing "+
     *  Add" chip that opens the driver browser. */
    @Composable
    private fun GpuDriverSection() {
        Text(
            str("setup.gpuDriver.title"),
            color = Color.White,
            fontSize = 16.sp,
            fontWeight = FontWeight.Bold,
        )
        Spacer(Modifier.height(4.dp))
        Text(
            str("setup.gpuDriver.description"),
            color = Color(0xFFAAAAAA),
            fontSize = 12.sp,
        )
        Spacer(Modifier.height(10.dp))
        LazyRow(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.spacedBy(10.dp),
        ) {
            item {
                DriverChip(
                    label = str("setup.gpuDriver.default.label"),
                    sublabel = str("setup.gpuDriver.default.sublabel"),
                    detail = str("setup.gpuDriver.default.detail"),
                    selected = selectedDriverId.value == null,
                    onClick = { selectedDriverId.value = null },
                )
            }
            items(installedDrivers, key = { it.id }) { drv ->
                // Pack as much as the meta.json offers — name, vendor +
                // driver version (the differentiator across releases of
                // the same family), description if any.
                val sublabel = buildString {
                    if (drv.vendor.isNotEmpty()) append(drv.vendor)
                    if (drv.version.isNotEmpty()) {
                        if (isNotEmpty()) append(" · ")
                        append(drv.version)
                    }
                    if (isEmpty() && drv.author.isNotEmpty()) append(drv.author)
                    if (isEmpty()) append("Installed")
                }
                DriverChip(
                    label = drv.name,
                    sublabel = sublabel,
                    detail = drv.description.ifBlank { drv.author },
                    selected = selectedDriverId.value == drv.id,
                    onClick = { selectedDriverId.value = drv.id },
                    onDelete = {
                        if (selectedDriverId.value == drv.id) selectedDriverId.value = null
                        CustomDriver.delete(drv)
                        installedDrivers.remove(drv)
                    },
                )
            }
            item {
                AddDriverChip(onClick = {
                    showDriverBrowser.value = true
                    // Kick off the fetch if we haven't already.
                    if (remoteDrivers.value == null) remoteDrivers.value = emptyList()
                })
            }
        }
    }

    @Composable
    private fun DriverChip(
        label: String,
        sublabel: String,
        detail: String,
        selected: Boolean,
        onClick: () -> Unit,
        onDelete: (() -> Unit)? = null,
    ) {
        val bg = if (selected) Colors.pasx2_blue.copy(alpha = 0.30f) else Color(0xFF1F2123)
        val border = if (selected) Colors.pasx2_blue else Color.White.copy(alpha = 0.10f)
        val labelColor = if (selected) Color.White else Color(0xFFE0E0E0)
        // 220dp chip is wide enough to surface the differentiator (Mesa
        // vendor + driverVersion) plus a description preview without
        // ellipsing the driver name itself. The chip row scrolls
        // horizontally so width can grow without crowding the page.
        Box(
            modifier = Modifier
                .width(220.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(bg)
                .border(1.5.dp, border, RoundedCornerShape(10.dp))
                .clickable(onClick = onClick),
        ) {
            Column(modifier = Modifier.padding(horizontal = 14.dp, vertical = 12.dp)) {
                Text(
                    label,
                    color = labelColor,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis,
                )
                Spacer(Modifier.height(3.dp))
                Text(
                    sublabel,
                    color = Colors.pasx2_blue,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis,
                )
                if (detail.isNotEmpty()) {
                    Spacer(Modifier.height(4.dp))
                    Text(
                        detail,
                        color = Color(0xFF999999),
                        fontSize = 10.sp,
                        maxLines = 2,
                        overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis,
                    )
                }
            }
            // Small corner X for delete, only present on user-installed
            // chips (Default has no delete). Tap is independent of the chip
            // body's click handler.
            if (onDelete != null) {
                Box(
                    Modifier
                        .align(Alignment.TopEnd)
                        .padding(4.dp)
                        .size(20.dp)
                        .clip(RoundedCornerShape(10.dp))
                        .background(Color(0xFF3A1A1A))
                        .clickable(onClick = onDelete),
                    contentAlignment = Alignment.Center,
                ) {
                    Text("×", color = Color(0xFFFF6B6B), fontSize = 14.sp, fontWeight = FontWeight.Bold)
                }
            }
        }
    }

    @Composable
    private fun AddDriverChip(onClick: () -> Unit) {
        Box(
            modifier = Modifier
                .width(220.dp)
                .clip(RoundedCornerShape(10.dp))
                .background(Color(0xFF1A1A1A))
                .border(
                    1.5.dp,
                    Colors.pasx2_blue.copy(alpha = 0.5f),
                    RoundedCornerShape(10.dp))
                .clickable(onClick = onClick)
                .padding(vertical = 18.dp),
            contentAlignment = Alignment.Center,
        ) {
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Text("+", color = Colors.pasx2_blue, fontSize = 24.sp, fontWeight = FontWeight.Bold)
                Text(
                    str("setup.gpuDriver.add"),
                    color = Colors.pasx2_blue,
                    fontSize = 12.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
        }
    }

    /** Fullscreen-within-the-content driver browser. Lists releases from
     *  K11MCH1/AdrenoToolsDrivers. Tapping Install downloads + extracts
     *  into the drivers dir; on success the chip row picks it up via
     *  installedDrivers refresh. */
    @Composable
    private fun DriverBrowserSheet() {
        val context = LocalContext.current
        val scope = rememberCoroutineScope()

        // SAF picker for "pick local .zip" — adrenotools driver packs
        // from outside the GitHub list (e.g. user pre-downloaded, sideloaded).
        // MIME filter accepts both application/zip and the catch-all
        // application/octet-stream because some file providers (Drive,
        // Files-by-Google) report .zip downloads as octet-stream.
        val localLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument()
        ) { uri: Uri? ->
            if (uri == null) return@rememberLauncherForActivityResult
            // Mark the sheet as "installing" with a synthetic id so the
            // existing busy-row indicator surfaces. Using a sentinel that
            // can't collide with any real remote id.
            val pendingId = "local-import"
            installingDriverId.value = pendingId
            scope.launch {
                val result = withContext(Dispatchers.IO) {
                    CustomDriver.installFromUri(context, uri)
                }
                installingDriverId.value = null
                if (result != null) {
                    installedDrivers.clear()
                    installedDrivers.addAll(CustomDriver.listInstalled(context))
                    selectedDriverId.value = result.id
                    driverBrowserError.value = null
                    showDriverBrowser.value = false
                } else {
                    driverBrowserError.value = I18n.get("setup.gpuDriver.error.importFailed")
                }
            }
        }

        // Fetch on first entry to the sheet. remoteDrivers stays around
        // for the wizard session so reopening the sheet is instant.
        LaunchedEffect(Unit) {
            if (remoteDrivers.value.isNullOrEmpty()) {
                driverBrowserError.value = null
                val fetched = withContext(Dispatchers.IO) { CustomDriver.fetchRemote() }
                remoteDrivers.value = fetched
                if (fetched.isEmpty()) {
                    driverBrowserError.value = I18n.get("setup.gpuDriver.error.unreachable")
                }
            }
        }

        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(bottom = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    str("setup.gpuDriver.availableTitle"),
                    color = Color.White,
                    fontSize = 18.sp,
                    fontWeight = FontWeight.Bold,
                    modifier = Modifier.weight(1f),
                )
                Box(
                    Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(Colors.pasx2_blue.copy(alpha = 0.30f))
                        .border(1.dp, Colors.pasx2_blue.copy(alpha = 0.65f), RoundedCornerShape(6.dp))
                        .clickable {
                            localLauncher.launch(arrayOf("application/zip", "application/octet-stream", "*/*"))
                        }
                        .padding(horizontal = 12.dp, vertical = 6.dp),
                ) {
                    Text(
                        str("setup.gpuDriver.pickLocalZip"),
                        color = Color.White,
                        fontSize = 12.sp,
                        fontWeight = FontWeight.Bold,
                    )
                }
                Spacer(Modifier.width(8.dp))
                Box(
                    Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(Color(0xFF2A2A2A))
                        .clickable { showDriverBrowser.value = false }
                        .padding(horizontal = 12.dp, vertical = 6.dp),
                ) {
                    Text(str("action.close"), color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                }
            }
            Text(
                str("setup.gpuDriver.sourceNote"),
                color = Color(0xFF888888),
                fontSize = 11.sp,
                modifier = Modifier.padding(bottom = 10.dp),
            )

            val err = driverBrowserError.value
            if (err != null) {
                Row(
                    Modifier.fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(Color(0xFF5A1A1A))
                        .padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("⚠", fontSize = 18.sp, color = Color(0xFFFF6B6B))
                    Spacer(Modifier.width(8.dp))
                    Text(err, color = Color.White, fontSize = 13.sp, modifier = Modifier.weight(1f))
                }
                Spacer(Modifier.height(10.dp))
            }

            val list = remoteDrivers.value
            when {
                list == null -> {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(20.dp),
                            strokeWidth = 2.dp,
                            color = Colors.pasx2_blue,
                        )
                        Spacer(Modifier.width(12.dp))
                        Text(str("setup.gpuDriver.loading"), color = Color.White, fontSize = 13.sp)
                    }
                }
                list.isEmpty() -> {
                    Text(
                        str("setup.gpuDriver.noneAvailable"),
                        color = Color.LightGray,
                    )
                }
                else -> {
                    // 2-column grid mirroring the BIOS picker layout.
                    LazyVerticalGrid(
                        columns = GridCells.Fixed(2),
                        modifier = Modifier.fillMaxSize(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalArrangement = Arrangement.spacedBy(6.dp),
                    ) {
                        itemsIndexed(list, key = { _, it -> it.id }) { _, remote ->
                            val installed = installedDrivers.any { it.id == remote.id }
                            val installing = installingDriverId.value == remote.id
                            val active = installed && selectedDriverId.value == remote.id
                            DriverBrowserRow(
                                remote = remote,
                                installed = installed,
                                installing = installing,
                                active = active,
                                onInstall = {
                                    installingDriverId.value = remote.id
                                    scope.launch {
                                        val result = withContext(Dispatchers.IO) {
                                            CustomDriver.download(context, remote)
                                        }
                                        installingDriverId.value = null
                                        if (result != null) {
                                            // Install also activates: refresh chips,
                                            // set this driver as the active pick, and
                                            // bounce out of the sheet so the user
                                            // immediately sees their selection.
                                            installedDrivers.clear()
                                            installedDrivers.addAll(CustomDriver.listInstalled(context))
                                            selectedDriverId.value = result.id
                                            showDriverBrowser.value = false
                                        } else {
                                            driverBrowserError.value = "Install failed for ${remote.assetName}. The download or extract step errored — try again."
                                        }
                                    }
                                },
                                onSelect = {
                                    selectedDriverId.value = remote.id
                                    showDriverBrowser.value = false
                                },
                            )
                        }
                    }
                }
            }
        }
    }

    @Composable
    private fun DriverBrowserRow(
        remote: CustomDriver.RemoteDriver,
        installed: Boolean,
        installing: Boolean,
        active: Boolean,
        onInstall: () -> Unit,
        onSelect: () -> Unit,
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xFF1F2123))
                .padding(12.dp),
        ) {
            Text(
                run {
                    val base = remote.releaseName.ifBlank { remote.assetName }
                    if (remote.source.isNotEmpty()) "${remote.source} · $base" else base
                },
                color = Color.White,
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis,
            )
            Spacer(Modifier.height(2.dp))
            Text(
                remote.assetName,
                color = Color(0xFFAACCFF),
                fontSize = 11.sp,
                maxLines = 1,
                overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis,
            )
            Spacer(Modifier.height(4.dp))
            Text(
                buildString {
                    if (remote.tagName.isNotEmpty()) append(remote.tagName)
                    if (remote.sizeBytes > 0) {
                        if (isNotEmpty()) append(" · ")
                        append(humanBytes(remote.sizeBytes))
                    }
                    if (remote.publishedAt.isNotEmpty()) {
                        if (isNotEmpty()) append(" · ")
                        append(remote.publishedAt.substringBefore('T'))
                    }
                },
                color = Color(0xFF888888),
                fontSize = 10.sp,
                maxLines = 1,
                overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis,
            )
            Spacer(Modifier.height(10.dp))
            // Action button fills the cell width — visually tighter in a
            // 2-column grid than a right-aligned button.
            when {
                installing -> {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(Color(0xFF2A2A2A))
                            .padding(vertical = 7.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.Center,
                    ) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(14.dp),
                            strokeWidth = 2.dp,
                            color = Colors.pasx2_blue,
                        )
                        Spacer(Modifier.width(8.dp))
                        Text(str("setup.gpuDriver.installing"), color = Color(0xFFCCCCCC), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                    }
                }
                active -> {
                    // Already the active pick — no-op tile, distinct visual
                    // so the user can see "yes you're using this one".
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(Color(0xFF1F3A1F))
                            .padding(vertical = 7.dp),
                        contentAlignment = Alignment.Center,
                    ) {
                        Text(str("setup.gpuDriver.active"), color = Color(0xFF9ED49E), fontSize = 11.sp, fontWeight = FontWeight.Bold)
                    }
                }
                installed -> {
                    // Installed but a different driver is active — Select
                    // makes this the active pick + closes the sheet.
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(Colors.pasx2_blue.copy(alpha = 0.30f))
                            .border(1.dp, Colors.pasx2_blue.copy(alpha = 0.65f), RoundedCornerShape(6.dp))
                            .clickable(onClick = onSelect)
                            .padding(vertical = 7.dp),
                        contentAlignment = Alignment.Center,
                    ) {
                        Text(str("setup.gpuDriver.select"), color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    }
                }
                else -> {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(6.dp))
                            .background(Colors.pasx2_blue)
                            .clickable(onClick = onInstall)
                            .padding(vertical = 7.dp),
                        contentAlignment = Alignment.Center,
                    ) {
                        Text(str("setup.gpuDriver.install"), color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    }
                }
            }
        }
    }

    private fun humanBytes(n: Long): String {
        if (n < 1024) return "${n} B"
        if (n < 1024 * 1024) return "${n / 1024} KB"
        return "%.1f MB".format(n / 1024.0 / 1024.0)
    }

    /** BIOS page content — single scrollable list. The pick button lives in
     *  the nav row. The remembered BIOS folder URI auto-rescans on entry so
     *  the user sees the full list of BIOS images they originally picked
     *  from, with the configured one pre-selected. Fallback to a single-row
     *  view when no folder URI is available (older builds, revoked
     *  permission, or first-launch before any folder has been picked). */
    @Composable
    private fun SetupBiosContent() {
        val context = LocalContext.current

        // Cold-start hydration: process restart with setupComplete=false
        // reaches this composable directly, so biosDirUri may still be null
        // even though Main.biosDir was loaded from prefs in onCreate. Pull
        // it across so the auto-scan effect below picks it up.
        LaunchedEffect(Unit) {
            if (biosDirUri.value == null) {
                Main.biosDir.value?.let { dirStr ->
                    biosDirUri.value = try { Uri.parse(dirStr) } catch (_: Exception) { null }
                }
            }
        }

        // Auto-rescan the remembered folder. lastScannedDir is the guard:
        // resetForReentry / the picker callback both null it out so this
        // effect re-runs once per "user wants a fresh scan" event.
        LaunchedEffect(biosDirUri.value, lastScannedDir.value) {
            val uri = biosDirUri.value
            if (uri != null && uri != lastScannedDir.value && !biosScanning.value) {
                scanBiosDirectory(context, uri)
            }
        }

        // Cold-start: probe the configured BIOS so the fallback single-row
        // view (when biosDirUri is null) has metadata to display.
        val configuredPath = Main.bios.value
        LaunchedEffect(configuredPath) {
            if (configuredPath != null && configuredBiosInfo.value == null) {
                configuredBiosInfo.value = probeExistingBios(configuredPath)
            }
        }

        Column(Modifier.fillMaxSize()) {
            when {
                biosScanning.value -> {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        CircularProgressIndicator(
                            modifier = Modifier.size(20.dp),
                            strokeWidth = 2.dp,
                            color = Colors.pasx2_blue,
                        )
                        Spacer(Modifier.width(12.dp))
                        Text(str("setup.bios.scanning"), color = Color.White)
                    }
                }
                biosScanError.value != null -> {
                    Text(biosScanError.value!!, color = Color(0xFFFF6B6B))
                }
                scannedBioses.isNotEmpty() -> {
                    // 2-column grid — landscape layout has plenty of room and
                    // a single column wastes most of the screen.
                    LazyVerticalGrid(
                        columns = GridCells.Fixed(2),
                        modifier = Modifier.fillMaxSize(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                    ) {
                        itemsIndexed(scannedBioses) { idx, bios ->
                            val selected = selectedBiosIdx.value == idx
                            BiosRow(
                                info = bios.info,
                                displayName = bios.displayName,
                                selected = selected,
                                onClick = {
                                    selectedBiosIdx.value = idx
                                    refreshAllowNext()
                                },
                            )
                        }
                    }
                }
                // Fallback: no folder URI to scan AND no scan running. If a
                // BIOS is already configured, render it as a single tile in
                // the same 2-column grid so it visually matches the picker
                // layout for users re-entering setup.
                configuredPath != null && configuredBiosInfo.value != null -> {
                    LazyVerticalGrid(
                        columns = GridCells.Fixed(2),
                        modifier = Modifier.fillMaxSize(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                    ) {
                        item {
                            BiosRow(
                                info = configuredBiosInfo.value!!,
                                displayName = File(configuredPath).name,
                                selected = true,
                                onClick = { /* no-op — already configured */ },
                            )
                        }
                    }
                }
                else -> {
                    Text(
                        str("setup.bios.noneSelected"),
                        color = Color.LightGray,
                    )
                }
            }
        }
    }

    @Composable
    private fun BiosRow(
        info: BiosInfo,
        displayName: String,
        selected: Boolean,
        onClick: () -> Unit,
    ) {
        val bg = if (selected) Colors.pasx2_blue.copy(alpha = 0.35f) else Color(0xFF333333)
        val border = if (selected) Colors.pasx2_blue else Color.Transparent
        Row(
            Modifier
                .fillMaxWidth()
                .padding(vertical = 4.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(bg)
                .border(2.dp, border, RoundedCornerShape(8.dp))
                .clickable(onClick = onClick)
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(info.regionFlag, fontSize = 28.sp)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        info.zone.ifBlank { str("setup.bios.unknownZone") },
                        color = Color.White, fontSize = 18.sp, fontWeight = FontWeight.Bold,
                    )
                    Spacer(Modifier.width(8.dp))
                    Text(info.versionString, color = Color(0xFFAACCFF), fontSize = 16.sp)
                }
                Spacer(Modifier.height(2.dp))
                Text(displayName, color = Color.LightGray, fontSize = 12.sp)
                Spacer(Modifier.height(2.dp))
                Text(info.description, color = Color(0xFFCCCCCC), fontSize = 11.sp)
            }
        }
    }

    /** System folder page — confirmation of the selected folder. Picker
     *  lives in the nav row. The folder is where emucore will look for
     *  bios/, memcards/, savestates/, etc.; before this page existed,
     *  emucore defaulted to getExternalFilesDir(null). */
    @Composable
    private fun SetupSystemDirContent() {
        Column(Modifier.fillMaxSize()) {
            Text(
                str("setup.systemDir.intro"),
                fontSize = 14.sp, color = Color.LightGray,
                modifier = Modifier.padding(bottom = 12.dp),
            )

            // Validation error banner — surfaces the scoped-storage write
            // rejection so the user knows why Next refused the custom folder.
            val err = systemDirError.value
            if (err != null) {
                Row(
                    Modifier.fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(Color(0xFF5A1A1A))
                        .padding(12.dp)
                        .padding(bottom = 0.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("⚠", fontSize = 22.sp, color = Color(0xFFFF6B6B))
                    Spacer(Modifier.width(8.dp))
                    Text(err, color = Color.White, fontSize = 13.sp,
                        modifier = Modifier.weight(1f))
                }
                Spacer(Modifier.height(12.dp))
            }

            val display = systemDirDisplay.value
            if (display != null && !systemDirUseDefault.value) {
                Row(
                    Modifier.fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(Color(0xFF333333))
                        .padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("📁", fontSize = 24.sp)
                    Spacer(Modifier.width(8.dp))
                    Column(Modifier.weight(1f)) {
                        Text(str("setup.systemDir.selectedLabel"), color = Color.LightGray, fontSize = 12.sp)
                        Text(display, color = Color.White, fontSize = 14.sp)
                    }
                }
            } else if (systemDirUseDefault.value) {
                Row(
                    Modifier.fillMaxWidth()
                        .clip(RoundedCornerShape(8.dp))
                        .background(Color(0xFF1F3A1F))
                        .padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("✅", fontSize = 22.sp)
                    Spacer(Modifier.width(8.dp))
                    Column(Modifier.weight(1f)) {
                        Text(str("setup.systemDir.usingAppPrivate"), color = Color.White, fontSize = 13.sp,
                            fontWeight = FontWeight.Bold)
                        Text(str("setup.systemDir.appPrivateSubtitle"),
                            color = Color.LightGray, fontSize = 11.sp)
                    }
                }
            } else {
                Text(str("setup.systemDir.noneSelected"),
                    color = Color.LightGray)
            }

            Spacer(Modifier.height(12.dp))

            // Escape hatch — guaranteed-writable app-private fallback.
            // Clicking this clears any picked SAF URI + sets the
            // use-default sentinel so refreshAllowNext + finishSystemDirStep
            // know to skip the SAF write probe on Next.
            Button(
                onClick = {
                    systemDirUseDefault.value = true
                    systemDirUri.value = null
                    systemDirDisplay.value = null
                    systemDirError.value = null
                    refreshAllowNext()
                },
                colors = ps2Colors(),
                shape = RoundedCornerShape(8.dp),
            ) {
                Text(
                    if (systemDirUseDefault.value)
                        "✓ Using App-Private Folder"
                    else
                        "Use App-Private Folder (default, always writable)",
                )
            }
        }
    }

    /** ROMs page content — list of selected folders, each with a remove
     *  button. The Pick / Add button lives in the nav row at the bottom
     *  (midButtonLabel returns "Add Another Folder" when the list is
     *  non-empty, "Pick ROMs Folder" when empty). */
    @Composable
    private fun SetupRomsDirContent() {
        Column(Modifier.fillMaxSize()) {
            Text(
                "Pick one or more folders where you keep your PS2 (.iso / .chd / etc.) and PS1 (.bin / .iso) ROM dumps. " +
                "ARMSX2 will list games from every folder you add.",
                fontSize = 14.sp, color = Color.LightGray,
                modifier = Modifier.padding(bottom = 12.dp),
            )

            if (romsDirsState.isEmpty()) {
                Text(
                    "No ROMs folders yet — use the Pick ROMs Folder button below to add one.",
                    color = Color.LightGray,
                )
            } else {
                Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    for (uri in romsDirsState.toList()) {
                        RomsDirRow(
                            uri = uri,
                            onRemove = {
                                romsDirsState.remove(uri)
                                refreshAllowNext()
                            },
                        )
                    }
                }
            }
        }
    }

    @Composable
    private fun RomsDirRow(uri: Uri, onRemove: () -> Unit) {
        val display = uri.lastPathSegment ?: uri.toString()
        Row(
            Modifier.fillMaxWidth()
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xFF333333))
                .padding(12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("📁", fontSize = 22.sp)
            Spacer(Modifier.width(10.dp))
            Column(Modifier.weight(1f)) {
                Text(display, color = Color.White, fontSize = 14.sp,
                    fontWeight = FontWeight.Bold)
                Text(uri.toString(), color = Color(0xFFAAAAAA), fontSize = 10.sp,
                    maxLines = 1)
            }
            Spacer(Modifier.width(8.dp))
            // Tap-to-remove. Tinted PS2-blue-ish so it's obviously
            // interactive without dominating the row.
            Box(
                Modifier
                    .clip(RoundedCornerShape(6.dp))
                    .background(Color(0xFF5A1A1A))
                    .clickable(onClick = onRemove)
                    .padding(horizontal = 10.dp, vertical = 6.dp),
            ) {
                Text("Remove", color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.Bold)
            }
        }
    }

    /**
     * Enumerate `treeUri` via DocumentFile, probe each candidate file via
     * NativeApp.getBiosInfoFromFd, populate scannedBioses with the valid
     * hits. Runs on Main.invoke's background dispatcher. On success the
     * URI is persisted to prefs as `biosDir` and the configured BIOS (if
     * any) is pre-selected in the list by filename match.
     */
    private fun scanBiosDirectory(context: Context, treeUri: Uri) {
        biosScanning.value = true
        biosScanError.value = null
        scannedBioses.clear()
        selectedBiosIdx.value = null
        refreshAllowNext()
        lastScannedDir.value = treeUri

        Main.invoke {
            try {
                val tree = DocumentFile.fromTreeUri(context, treeUri)
                val files = tree?.listFiles() ?: emptyArray()
                for (f in files) {
                    if (!f.isFile) continue
                    val len = f.length()
                    // PS2 BIOS images are 4MB; allow up to 8.5MB to cover
                    // hash-suffixed dumps that slip into the dir. SAF can
                    // return 0/-1 for unknown size — fall through and let
                    // the BIOS probe itself reject non-images.
                    if (len > 0L && (len < 4_000_000L || len > 8_500_000L)) continue

                    val pfd: ParcelFileDescriptor? = try {
                        context.contentResolver.openFileDescriptor(f.uri, "r")
                    } catch (_: Exception) { null }
                    if (pfd == null) continue
                    val fd = pfd.detachFd()
                    val info = NativeApp.getBiosInfoFromFd(fd)
                    if (info != null) {
                        val name = f.name ?: "unknown.bin"
                        scannedBioses.add(ScannedBios(f.uri, name, info))
                    }
                }

                // Remember the folder for next-launch rescan and pre-select
                // the configured BIOS so the user sees their existing choice
                // highlighted on re-entry. Only persist on a non-empty scan
                // — if the folder is genuinely empty / no BIOSes detected
                // we don't want to overwrite a known-good URI with a junk
                // one the user picked by mistake.
                if (scannedBioses.isNotEmpty()) {
                    biosDirUri.value = treeUri
                    Main.biosDir.value = treeUri.toString()
                    Main.prefs.edit().putString("biosDir", treeUri.toString()).apply()

                    val configuredName = Main.bios.value?.let { File(it).name }
                    val matchIdx = if (configuredName != null) {
                        scannedBioses.indexOfFirst { it.displayName == configuredName }
                    } else {
                        -1
                    }
                    selectedBiosIdx.value = when {
                        matchIdx >= 0 -> matchIdx
                        scannedBioses.size == 1 -> 0
                        else -> null
                    }
                } else {
                    biosScanError.value = I18n.get("setup.bios.error.noneFound")
                }
            } catch (e: Exception) {
                biosScanError.value = "Scan failed: ${e.message}"
            } finally {
                biosScanning.value = false
                refreshAllowNext()
            }
        }
    }
}
