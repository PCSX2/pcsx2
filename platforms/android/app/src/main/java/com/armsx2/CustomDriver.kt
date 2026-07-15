package com.armsx2

import com.armsx2.runtime.MainActivityRuntime

import android.content.Context
import android.net.Uri
import android.util.Log
import kr.co.iefriends.pcsx2.HttpClient
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.util.zip.ZipInputStream

/**
 * Custom Vulkan driver management for Android. Lets a user replace the
 * stock OEM Vulkan ICD with a Mesa Turnip build for the GS Vulkan
 * renderer.
 *
 * Wraps three concerns:
 *  1. [listInstalled] — enumerate drivers extracted under
 *     `<externalFilesDir>/drivers/<id>/`. Each has a meta.json + the
 *     .so file.
 *  2. [fetchRemote] / [download] — pull the list of available drivers
 *     from K11MCH1/AdrenoToolsDrivers GitHub releases and install a
 *     pick by downloading + extracting its zip asset into the drivers
 *     dir.
 *  3. [applyToNative] — push the active selection to the JNI side via
 *     `NativeApp.setCustomVulkanDriver`. Read by VKLoader on the next
 *     MTGS::Open. Pass null to revert to the system loader.
 *
 * Storage layout matches the AdrenoToolsDrivers zip schema (the same
 * one Yuzu/Strato/Vita3K consume) so any driver pack from that ecosystem
 * drops in.
 */
object CustomDriver {

    private const val TAG = "CustomDriver"

    /** Source repos for the remote driver list. Each ships GitHub releases
     *  whose .zip assets are adrenotools driver packs (`meta.json` +
     *  `libvulkan_freedreno.so` at the root; meta.json is synthesized when a
     *  pack omits it). Listed in display order.
     *
     *  `idPrefix` is folded into each driver's id so two repos can't collide
     *  (and so a re-listed driver still matches its install). It is EMPTY for
     *  K11MCH1 so its ids stay byte-identical to older builds — existing
     *  installs keep matching; only new sources get a prefix. */
    private data class DriverSource(
        val label: String,
        val releasesUrl: String,
        val idPrefix: String,
    )
    private val DRIVER_SOURCES = listOf(
        DriverSource(
            "AdrenoToolsDrivers",
            "https://api.github.com/repos/K11MCH1/AdrenoToolsDrivers/releases",
            "",
        ),
        DriverSource(
            "MrPurple · purple-turnip",
            "https://api.github.com/repos/MrPurple666/purple-turnip/releases",
            "purpleturnip",
        ),
        // Scheduled biweekly freedreno/Turnip adrenotools builds (incl. Gen8 + a7xx
        // Qualcomm driver packages). Same GitHub-releases meta.json format as above.
        DriverSource(
            "StevenMXZ · Adreno-Tools",
            "https://api.github.com/repos/StevenMXZ/Adreno-Tools-Drivers/releases",
            "stevenmxz",
        ),
        // crueter's GameHub Turnip packs, tuned for Snapdragon 8 Elite / Adreno 8xx
        // (Gen 8). Same GitHub-releases adrenotools zip format as the sources above.
        DriverSource(
            "crueter · GameHub 8Elite",
            "https://api.github.com/repos/crueter/GameHub-8Elite-Drivers/releases",
            "gamehub8e",
        ),
        // PojavLauncherTeam/freedreno-builder — biweekly Mesa Turnip builds (archived at
        // Mesa ~Jan 2025). Each release ships a bare libvulkan_freedreno.so (no meta.json,
        // synthesized on install). A solid known-good Turnip for older Adreno 6xx /
        // Android 10 devices.
        DriverSource(
            "PojavLauncherTeam · freedreno (A10)",
            "https://api.github.com/repos/PojavLauncherTeam/freedreno-builder/releases",
            "freedrenobuilder",
        ),
        // WearyConcern1165/ExynosTools — driver packs for Samsung Xclipse (Exynos) GPUs.
        // Same GitHub-releases adrenotools .zip format as the sources above.
        DriverSource(
            "WearyConcern1165 · ExynosTools",
            "https://api.github.com/repos/WearyConcern1165/ExynosTools/releases",
            "exynostools",
        ),
    )

    /** Sane default for the driver's library soname when meta.json
     *  doesn't include one. Every Turnip release we care about uses
     *  this name, but the field is technically optional in the schema. */
    private const val DEFAULT_LIBRARY_NAME = "libvulkan_freedreno.so"

    /** A driver extracted under `<externalFilesDir>/drivers/<id>/`. */
    data class InstalledDriver(
        val id: String,
        val name: String,
        val description: String,
        val author: String,
        val vendor: String,
        val version: String,
        val libraryName: String,
        val driverDir: File,
    ) {
        val driverFile: File get() = File(driverDir, libraryName)
        /** Subdirectory the driver may write its shader cache into. The
         *  redirect hook captures the driver's file IO to this prefix
         *  so it doesn't try to write under `/system/`. */
        val redirectDir: File get() = File(driverDir, "cache")
    }

    /** A driver listed on the GitHub releases page (not yet installed). */
    data class RemoteDriver(
        val id: String,
        val releaseName: String,
        val assetName: String,
        val tagName: String,
        val publishedAt: String,
        val assetUrl: String,
        val sizeBytes: Long,
        val source: String = "",
    )

    // ---- Installed drivers --------------------------------------------------

    /** Drivers MUST live under internal app-private storage (`filesDir`,
     *  resolves to `/data/user/0/<pkg>/files/`). `getExternalFilesDir`
     *  returns the sdcard-style `/storage/emulated/0/...` mount which is
     *  hardened against dlopen — dlopen reports "Permission denied"
     *  trying to map shared-object segments from there. The adrenotools
     *  driver.h header explicitly calls this out:
     *  "This path MUST NOT be on sdcard/storage. ... Otherwise you will
     *   get permission denied because dlopen() can't be done on *.so
     *   libraries that any random user or application can manipulate
     *   (which would be a security risk)." */
    private fun driversRoot(context: Context): File =
        File(context.filesDir, "drivers").apply { mkdirs() }

    /** Enumerate installed drivers. Skips dirs that don't have a
     *  meta.json or whose .so file is missing — those are mid-install
     *  or corrupted and the user can re-download. */
    fun listInstalled(context: Context): List<InstalledDriver> {
        val root = driversRoot(context)
        val dirs = root.listFiles { f -> f.isDirectory } ?: return emptyList()
        return dirs.mapNotNull { dir ->
            val metaFile = File(dir, "meta.json")
            if (!metaFile.exists()) return@mapNotNull null
            val text = runCatching { metaFile.readText() }.getOrNull() ?: return@mapNotNull null
            val json = runCatching { JSONObject(text) }.getOrNull() ?: return@mapNotNull null
            val libName = json.optString("libraryName").ifEmpty { DEFAULT_LIBRARY_NAME }
            if (!File(dir, libName).exists()) return@mapNotNull null
            InstalledDriver(
                id = dir.name,
                name = json.optString("name").ifEmpty { dir.name },
                description = json.optString("description"),
                author = json.optString("author"),
                vendor = json.optString("vendor"),
                version = json.optString("driverVersion").ifEmpty { json.optString("packageVersion") },
                libraryName = libName,
                driverDir = dir,
            )
        }.sortedBy { it.name.lowercase() }
    }

    /** Recursively remove an installed driver. */
    fun delete(installed: InstalledDriver) {
        installed.driverDir.deleteRecursively()
    }

    // ---- GitHub release list ------------------------------------------------

    /** Fetch the K11MCH1/AdrenoToolsDrivers releases and flatten into
     *  one RemoteDriver per .zip asset. Suspending — caller must wrap
     *  in `withContext(Dispatchers.IO)` (the JNI call blocks the
     *  calling thread). Returns empty list on any network/parse error;
     *  callers can detect "I had drivers shown a moment ago, now
     *  nothing" by tracking the previous size. */
    fun fetchRemote(): List<RemoteDriver> {
        val userAgent = "ARMSX2/" + runCatching {
            NativeApp.getBuildVersion()
        }.getOrNull().orEmpty().ifEmpty { "dev" }
        val out = mutableListOf<RemoteDriver>()
        val seen = HashSet<String>()
        // Sequential per source; one dead/rate-limited source returns empty and
        // never blanks the others. Dedup by id guards the LazyVerticalGrid key set.
        for (src in DRIVER_SOURCES) {
            for (rd in fetchSource(src, userAgent)) {
                if (seen.add(rd.id)) out += rd
            }
        }
        return out
    }

    /** Fetch one source's releases and flatten into one RemoteDriver per .zip
     *  asset. Empty on any network/parse error so a single failing source can't
     *  blank the whole list. */
    private fun fetchSource(src: DriverSource, userAgent: String): List<RemoteDriver> {
        val resp = runCatching {
            HttpClient.doRequest(src.releasesUrl, "GET", null, userAgent, 15000)
        }.getOrNull() ?: return emptyList()
        if (resp.statusCode != 200 || resp.data.isEmpty()) {
            Log.w(TAG, "fetchRemote(${src.label}): status=${resp.statusCode}, size=${resp.data.size}")
            return emptyList()
        }

        val text = runCatching { String(resp.data, Charsets.UTF_8) }.getOrNull() ?: return emptyList()
        val arr = runCatching { JSONArray(text) }.getOrNull() ?: return emptyList()

        val out = mutableListOf<RemoteDriver>()
        for (i in 0 until arr.length()) {
            val release = arr.optJSONObject(i) ?: continue
            val tag = release.optString("tag_name").ifEmpty { release.optString("name") }
            val releaseName = release.optString("name").ifEmpty { tag }
            val publishedAt = release.optString("published_at")
            val assets = release.optJSONArray("assets") ?: continue
            for (j in 0 until assets.length()) {
                val asset = assets.optJSONObject(j) ?: continue
                val assetName = asset.optString("name")
                if (!assetName.endsWith(".zip", ignoreCase = true)) continue
                val url = asset.optString("browser_download_url")
                if (url.isEmpty()) continue
                val size = asset.optLong("size", 0L)
                // ID scopes by tag (a repo can ship the same asset filename in
                // multiple releases) and by source prefix (so two repos can't
                // collide). idPrefix is "" for K11MCH1 → ids unchanged.
                val idTag = if (src.idPrefix.isEmpty()) tag else "${src.idPrefix}-$tag"
                out += RemoteDriver(
                    id = makeId(idTag, assetName),
                    releaseName = releaseName,
                    assetName = assetName,
                    tagName = tag,
                    publishedAt = publishedAt,
                    assetUrl = url,
                    sizeBytes = size,
                    source = src.label,
                )
            }
        }
        return out
    }

    /** Stable id from `<tag>-<assetBase>`. Strips ".zip" + non-filename
     *  chars so the driver dir name is dlopen-safe (paths get fed straight
     *  to adrenotools, which feeds them to dlopen). The tag prefix
     *  disambiguates assets that share a filename across releases. */
    private fun makeId(tag: String, assetName: String): String {
        val base = assetName.removeSuffix(".zip").removeSuffix(".ZIP")
        val combined = if (tag.isNotEmpty()) "$tag-$base" else base
        return combined.replace(Regex("[^A-Za-z0-9._-]"), "_").lowercase()
    }

    // ---- Download + install -------------------------------------------------

    /** Download a remote driver and extract it into
     *  `<externalFilesDir>/drivers/<id>/`. Suspending — caller wraps in
     *  IO dispatcher. `onProgress` is called with bytesRead/total during
     *  the HTTP download phase (extraction is fast enough to not need
     *  progress). Returns the InstalledDriver on success, null on any
     *  failure (caller checks return). */
    fun download(
        context: Context,
        remote: RemoteDriver,
        onProgress: ((Long, Long) -> Unit)? = null,
    ): InstalledDriver? {
        val userAgent = "ARMSX2/" + runCatching {
            NativeApp.getBuildVersion()
        }.getOrNull().orEmpty().ifEmpty { "dev" }

        // HttpClient.doRequest returns the whole body as byte[]; for a
        // ~30MB driver zip this is fine. If we ever ship drivers >100MB
        // we'd want a streaming download API; not worth it today.
        val resp = runCatching {
            HttpClient.doRequest(remote.assetUrl, "GET", null, userAgent, 60_000)
        }.getOrNull() ?: return null
        if (resp.statusCode != 200 || resp.data.isEmpty()) {
            Log.w(TAG, "download: status=${resp.statusCode}, size=${resp.data.size}")
            return null
        }
        onProgress?.invoke(resp.data.size.toLong(), remote.sizeBytes.coerceAtLeast(resp.data.size.toLong()))

        return installFromStream(context, remote.id, resp.data.inputStream())
    }

    /** Install a driver from a user-picked local .zip URI (via SAF
     *  OpenDocument). Synchronous IO — call from a coroutine on
     *  Dispatchers.IO. Returns the InstalledDriver on success, null
     *  on any extract / validation failure. The id is derived from
     *  the URI's last segment so re-importing the same file is
     *  idempotent. */
    fun installFromUri(context: Context, uri: Uri): InstalledDriver? {
        val filename = uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
            ?: "imported_driver.zip"
        val id = makeId("local", filename)
        val stream = runCatching { context.contentResolver.openInputStream(uri) }.getOrNull()
        if (stream == null) {
            Log.w(TAG, "installFromUri: couldn't open $uri")
            return null
        }
        return stream.use { installFromStream(context, id, it) }
    }

    /** Shared extract+commit path used by both [download] and
     *  [installFromUri]. Reads the entire zip into a tmp dir, validates
     *  meta.json + library .so, then renames into place under
     *  drivers/<id>/. */
    private fun installFromStream(context: Context, id: String, stream: InputStream): InstalledDriver? {
        val targetDir = File(driversRoot(context), id)
        val tmpDir = File(driversRoot(context), "$id.tmp").also {
            if (it.exists()) it.deleteRecursively()
            it.mkdirs()
        }

        try {
            ZipInputStream(stream).use { zin ->
                while (true) {
                    val entry = zin.nextEntry ?: break
                    val name = entry.name
                    if (name.contains("..") || name.startsWith("/")) {
                        Log.w(TAG, "install: skipping suspicious entry $name")
                        continue
                    }
                    // K11MCH1 zips put files at the root. If a zip ever
                    // nests, flatten to root for our adrenotools driverDir
                    // contract (driverName resolves directly inside driverDir).
                    val outName = name.substringAfterLast('/')
                    if (outName.isEmpty() || entry.isDirectory) continue
                    val outFile = File(tmpDir, outName)
                    FileOutputStream(outFile).use { fos ->
                        zin.copyTo(fos)
                    }
                    zin.closeEntry()
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "install: extract failed: ${e.message}")
            tmpDir.deleteRecursively()
            return null
        }

        // Some sources ship a bare libvulkan_*.so with no meta.json (e.g.
        // PojavLauncherTeam/freedreno-builder). Synthesize a minimal manifest so those
        // still install as adrenotools packs — honoring the DRIVER_SOURCES contract above.
        val meta = File(tmpDir, "meta.json")
        if (!meta.exists()) {
            val soFile = tmpDir.listFiles { f -> f.isFile && f.name.endsWith(".so") }?.firstOrNull()
            if (soFile != null) {
                val synthLib = if (File(tmpDir, DEFAULT_LIBRARY_NAME).exists()) DEFAULT_LIBRARY_NAME else soFile.name
                runCatching {
                    meta.writeText(
                        JSONObject().apply {
                            put("schemaVersion", 1)
                            put("name", id)
                            put("description", "Turnip / freedreno (synthesized manifest)")
                            put("author", "freedreno-builder")
                            put("vendor", "Mesa")
                            put("driverVersion", "")
                            put("minApi", 24)
                            put("libraryName", synthLib)
                        }.toString()
                    )
                    Log.i(TAG, "install: synthesized meta.json ($synthLib)")
                }
            }
        }
        if (!meta.exists()) {
            Log.w(TAG, "install: zip missing meta.json")
            tmpDir.deleteRecursively()
            return null
        }
        val libName = runCatching {
            JSONObject(meta.readText()).optString("libraryName").ifEmpty { DEFAULT_LIBRARY_NAME }
        }.getOrDefault(DEFAULT_LIBRARY_NAME)
        if (!File(tmpDir, libName).exists()) {
            Log.w(TAG, "install: zip missing $libName")
            tmpDir.deleteRecursively()
            return null
        }

        if (targetDir.exists()) targetDir.deleteRecursively()
        if (!tmpDir.renameTo(targetDir)) {
            Log.w(TAG, "install: rename $tmpDir -> $targetDir failed")
            tmpDir.deleteRecursively()
            return null
        }
        File(targetDir, "cache").mkdirs()

        return listInstalled(context).firstOrNull { it.id == id }
    }

    // ---- Native bridge ------------------------------------------------------

    /** Push the active selection to native. Pass null to revert to the
     *  system loader. The native side reads these on the next
     *  Vulkan::LoadVulkanLibrary call (first MTGS::Open), so this must
     *  be called BEFORE MainActivityRuntime.start()'s applyRendererPrefs runs the VM. */
    fun applyToNative(context: Context, installed: InstalledDriver?) {
        if (installed == null) {
            NativeApp.setCustomVulkanDriver("", "", "", "")
            return
        }
        // adrenotools' path resolution wants the driver dir to end with
        // a slash. The redirect dir doesn't strictly require it but we
        // pass it the same way for consistency.
        val driverDirPath = installed.driverDir.absolutePath + "/"
        val redirectDirPath = installed.redirectDir.apply { mkdirs() }.absolutePath + "/"
        val hookLibDir = context.applicationInfo.nativeLibraryDir
        NativeApp.setCustomVulkanDriver(
            driverDirPath, installed.libraryName, redirectDirPath, hookLibDir)
    }
}
