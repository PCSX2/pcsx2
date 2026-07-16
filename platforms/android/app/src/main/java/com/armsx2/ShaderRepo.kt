package com.armsx2

import com.armsx2.runtime.MainActivityRuntime

import android.content.Context
import android.util.Log
import kr.co.iefriends.pcsx2.NativeApp
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipEntry
import java.util.zip.ZipFile

/**
 * Downloadable RetroArch (.slangp) shader packs — the shader analogue of
 * [CustomDriver].
 *
 * Wraps three concerns, in the same shape as the GPU-driver manager:
 *  1. [listInstalled] — enumerate packs extracted under
 *     `<assetCopyRoot>/shaders/<id>/`, with the number of presets each
 *     one contributes.
 *  2. [sources] / [download] — the pinned list of remote packs, and
 *     download + extract of a pick into the shaders dir.
 *  3. …and that's it. There is no [CustomDriver.applyToNative] analogue:
 *     the presets are picked up by the existing Renderer-tab preset
 *     picker (RendererTab.scanShaderPresets walks this same folder for
 *     `*.slangp`), which writes the chosen path to
 *     EmuCore/GS/ShaderChainPreset. Extracting here is the whole
 *     integration.
 *
 * Unlike a driver zip — which [CustomDriver] flattens to the root, since
 * adrenotools wants `meta.json` + the .so side by side — a shader pack's
 * directory structure is LOAD-BEARING: every `.slangp` references its
 * `.slang` stages by relative path. So we preserve the tree, which is why
 * the extract path below carries a real zip-slip guard rather than relying
 * on flattening to a basename.
 *
 * Nothing is bundled in the APK: slang-shaders is a mixed-license
 * collection, which is the same reason RetroArch itself downloads it via
 * the online updater rather than shipping it.
 */
object ShaderRepo {

    private const val TAG = "ShaderRepo"

    /** Folder under the data root that shader packs extract into. The
     *  Renderer tab's preset picker scans this same folder recursively for
     *  `*.slangp` — [shadersRoot] is the one place the path is spelled. */
    const val SHADER_DIR = "shaders"

    /** A downloadable pack. [id] doubles as the folder name under
     *  `shaders/`, so it must stay filename-safe and STABLE — changing one
     *  orphans existing installs (they'd stop matching and show up as an
     *  unnamed pack). */
    data class ShaderSource(
        val name: String,
        val url: String,
        val id: String,
        val description: String,
    )

    /** Sources, in display order.
     *
     *  The buildbot pack is the same artifact RetroArch's own online updater
     *  installs ("Update Slang Shaders"), so what a user gets here matches
     *  what they'd get in RetroArch: ~51MB, ~5.6k files, ~2.5k presets, with
     *  the category dirs (crt/, handheld/, bezel/…) at the ZIP ROOT.
     *
     *  The GitHub archive is the same collection straight from source. It
     *  nests everything under a `slang-shaders-master/` dir; [extract] strips
     *  a single common root so both land with the same layout. */
    private val SHADER_SOURCES = listOf(
        ShaderSource(
            name = "RetroArch · Slang Shaders",
            url = "https://buildbot.libretro.com/assets/frontend/shaders_slang.zip",
            id = "shaders_slang",
            description = "libretro buildbot · ~51 MB",
        ),
        ShaderSource(
            name = "libretro · slang-shaders (git)",
            url = "https://github.com/libretro/slang-shaders/archive/refs/heads/master.zip",
            id = "slang_shaders_git",
            description = "GitHub master · latest",
        ),
    )

    fun sources(): List<ShaderSource> = SHADER_SOURCES

    /** A pack extracted under `<assetCopyRoot>/shaders/<id>/`. */
    data class InstalledPack(
        val id: String,
        val name: String,
        val presetCount: Int,
        val dir: File,
    )

    // ---- Installed packs ----------------------------------------------------

    /** Packs live under the DATA root (not `filesDir`), because that's where
     *  the existing preset picker looks and where a user can also drop a pack
     *  by hand with a file manager. Unlike the driver .so files there's no
     *  dlopen constraint here — these are read as plain data by the GS
     *  renderer. */
    fun shadersRoot(context: Context): File =
        File(MainActivityRuntime.assetCopyRoot(context), SHADER_DIR).apply { mkdirs() }

    /** Enumerate installed packs. Any directory under `shaders/` counts —
     *  both our downloads and hand-dropped packs — so the list matches what
     *  the preset picker actually sees. Blocking IO (it walks the tree to
     *  count presets, which is thousands of files for a full pack): call from
     *  Dispatchers.IO. */
    fun listInstalled(context: Context): List<InstalledPack> {
        val root = shadersRoot(context)
        val dirs = root.listFiles { f -> f.isDirectory && !f.name.startsWith(".") } ?: return emptyList()
        return dirs.map { dir ->
            InstalledPack(
                id = dir.name,
                // A downloaded pack's folder is named for its source id, so the
                // friendly name comes back for free. Hand-dropped packs fall
                // back to the folder name.
                name = SHADER_SOURCES.firstOrNull { it.id == dir.name }?.name ?: dir.name,
                presetCount = countPresets(dir),
                dir = dir,
            )
        }.sortedBy { it.name.lowercase() }
    }

    /** Recursively remove an installed pack. */
    fun delete(pack: InstalledPack) {
        pack.dir.deleteRecursively()
    }

    private fun countPresets(dir: File): Int =
        runCatching {
            dir.walkTopDown().count { it.isFile && it.extension.equals("slangp", ignoreCase = true) }
        }.getOrDefault(0)

    // ---- Download + install -------------------------------------------------

    /**
     * Download [source] and extract it into `<assetCopyRoot>/shaders/<id>/`.
     * Blocking — caller wraps in `withContext(Dispatchers.IO)`.
     *
     * Progress comes back in two phases because they're measured in different
     * units and the pack is big enough that both are a visible wait:
     * [onDownload] gets (bytesRead, totalBytes) — totalBytes is -1 if the
     * server sent no Content-Length — and [onExtract] gets (entriesDone,
     * entriesTotal). Both are throttled, so they're safe to route straight at
     * Compose state.
     *
     * [isCancelled] is polled between chunks and between entries; when it goes
     * true the partial work is discarded and null is returned (same as any
     * other failure — the caller distinguishes by asking who cancelled).
     *
     * Returns the InstalledPack on success, null on any failure.
     */
    fun download(
        context: Context,
        source: ShaderSource,
        onDownload: ((Long, Long) -> Unit)? = null,
        onExtract: ((Int, Int) -> Unit)? = null,
        isCancelled: () -> Boolean = { false },
    ): InstalledPack? {
        // Staging lives OUTSIDE shaders/ so a half-extracted pack is never
        // visible to the preset picker's scan (which walks shaders/ on every
        // open, possibly while this runs). Same filesystem as the target, so
        // the commit below is a rename and not a 51MB copy.
        val staging = File(MainActivityRuntime.assetCopyRoot(context), ".shaderpacks-tmp").apply {
            if (exists()) deleteRecursively()
            mkdirs()
        }
        val tmpZip = File(staging, "${source.id}.zip")
        val tmpDir = File(staging, source.id).apply { mkdirs() }

        try {
            if (!fetchToFile(source.url, tmpZip, onDownload, isCancelled)) return null
            if (isCancelled()) return null
            if (!extract(tmpZip, tmpDir, onExtract, isCancelled)) return null
            if (isCancelled()) return null

            val targetDir = File(shadersRoot(context), source.id)
            if (targetDir.exists()) targetDir.deleteRecursively()
            if (!tmpDir.renameTo(targetDir)) {
                Log.w(TAG, "install: rename $tmpDir -> $targetDir failed")
                return null
            }
            val presets = countPresets(targetDir)
            if (presets == 0) {
                // Not a shader pack (or an empty/HTML error body that unzipped
                // into nothing). Don't leave a dead folder in the picker.
                Log.w(TAG, "install: ${source.id} extracted 0 .slangp presets")
                targetDir.deleteRecursively()
                return null
            }
            Log.i(TAG, "install: ${source.id} -> $targetDir ($presets presets)")
            return InstalledPack(source.id, source.name, presets, targetDir)
        } finally {
            staging.deleteRecursively()
        }
    }

    /**
     * Stream a URL to [dest], reporting bytes as they land.
     *
     * Deliberately NOT kr.co.iefriends.pcsx2.HttpClient: that buffers the
     * whole body into a byte[] and exposes no progress or cancel hook — its
     * own docs call out that a streaming API is wanted once downloads get
     * big. A ~51MB pack is exactly that case; holding it in a byte[] on top
     * of the extract would be a needless 51MB of heap on devices that don't
     * have it to spare. Same HttpURLConnection/system-TLS stack underneath.
     */
    private fun fetchToFile(
        url: String,
        dest: File,
        onProgress: ((Long, Long) -> Unit)?,
        isCancelled: () -> Boolean,
    ): Boolean {
        val userAgent = "ARMSX2/" + runCatching {
            NativeApp.getBuildVersion()
        }.getOrNull().orEmpty().ifEmpty { "dev" }

        var conn: HttpURLConnection? = null
        try {
            conn = (URL(url).openConnection() as HttpURLConnection).apply {
                requestMethod = "GET"
                connectTimeout = 20_000
                // Per-read, not whole-transfer: a 51MB body on a slow link is
                // minutes of legitimate transfer, but a stalled socket still
                // fails fast.
                readTimeout = 30_000
                instanceFollowRedirects = true
                setRequestProperty("User-Agent", userAgent)
            }
            val code = conn.responseCode
            if (code != HttpURLConnection.HTTP_OK) {
                Log.w(TAG, "fetch: $url -> status=$code")
                return false
            }
            val total = conn.contentLengthLong

            var read = 0L
            var reported = 0L
            conn.inputStream.use { input ->
                FileOutputStream(dest).use { out ->
                    val buf = ByteArray(64 * 1024)
                    while (true) {
                        if (isCancelled()) return false
                        val n = input.read(buf)
                        if (n < 0) break
                        out.write(buf, 0, n)
                        read += n
                        // Throttle: ~200 updates over a 51MB pack instead of
                        // ~800 recompositions.
                        if (read - reported >= PROGRESS_BYTES_STEP) {
                            reported = read
                            onProgress?.invoke(read, total)
                        }
                    }
                }
            }
            onProgress?.invoke(read, total)
            return read > 0
        } catch (e: Exception) {
            Log.w(TAG, "fetch: $url failed: ${e.message}")
            return false
        } finally {
            conn?.disconnect()
        }
    }

    /**
     * Extract [zip] into [targetDir], preserving the tree.
     *
     * ★ Zip-slip guarded. These are remote archives with thousands of
     * entries, and unlike the driver zips we can't flatten entry names to a
     * basename (the presets' relative `.slang` references would break), so
     * every entry's resolved path is canonicalised and checked to still be
     * inside the target. A single escaping entry fails the whole install
     * rather than being skipped: no legitimate pack has one, so its presence
     * means the archive isn't what we think it is.
     */
    private fun extract(
        zip: File,
        targetDir: File,
        onProgress: ((Int, Int) -> Unit)?,
        isCancelled: () -> Boolean,
    ): Boolean {
        // Canonical (not absolute): resolves symlinks and `..`, so the
        // startsWith below can't be fooled by a path that only looks contained.
        val targetCanonical = targetDir.canonicalPath
        val guardPrefix = targetCanonical + File.separator

        try {
            ZipFile(zip).use { zf ->
                val entries = zf.entries().toList()
                val total = entries.size
                // The GitHub archive wraps everything in `slang-shaders-master/`;
                // the buildbot pack has the categories at the root. Strip a single
                // common root so both install with the same layout — and so preset
                // labels (which are paths relative to shaders/) don't carry a dead
                // `slang-shaders-master/` segment.
                val strip = commonRootPrefix(entries)
                var done = 0
                var reported = 0

                for (entry in entries) {
                    if (isCancelled()) return false
                    done++
                    val name = entry.name.removePrefix(strip)
                    if (name.isEmpty()) continue

                    val outFile = File(targetDir, name)
                    val canonical = outFile.canonicalPath
                    if (canonical != targetCanonical && !canonical.startsWith(guardPrefix)) {
                        Log.w(TAG, "extract: rejecting zip-slip entry '${entry.name}' -> $canonical")
                        return false
                    }

                    if (entry.isDirectory) {
                        outFile.mkdirs()
                    } else {
                        outFile.parentFile?.mkdirs()
                        zf.getInputStream(entry).use { input ->
                            FileOutputStream(outFile).use { out -> input.copyTo(out) }
                        }
                    }

                    if (done - reported >= PROGRESS_ENTRY_STEP || done == total) {
                        reported = done
                        onProgress?.invoke(done, total)
                    }
                }
            }
            return true
        } catch (e: Exception) {
            Log.w(TAG, "extract: failed: ${e.message}")
            return false
        }
    }

    /** `"slang-shaders-master/"` if every entry sits under one root dir, else
     *  `""`. Only a SINGLE shared root is stripped — a pack whose entries
     *  start at the root (the buildbot one) keeps its category dirs. */
    private fun commonRootPrefix(entries: List<ZipEntry>): String {
        val first = entries.firstOrNull()?.name ?: return ""
        val slash = first.indexOf('/')
        if (slash <= 0) return ""
        val prefix = first.substring(0, slash + 1)
        return if (entries.all { it.name.startsWith(prefix) }) prefix else ""
    }

    private const val PROGRESS_BYTES_STEP = 256L * 1024L
    private const val PROGRESS_ENTRY_STEP = 64
}
