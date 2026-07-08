package com.armsx2

import android.content.Context
import android.graphics.BitmapFactory
import android.net.Uri
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.documentfile.provider.DocumentFile
import java.io.File
import java.util.zip.ZipInputStream

/**
 * Custom on-screen controller skins (v1: visuals only). A skin is a folder (or
 * .zip) of `ic_controller_<button>.png` images using the SAME filename scheme as
 * the iOS build, so iOS skin packs drop in. Imported skins live app-private under
 * `filesDir/controllerskins/<id>/`; one is selected at a time (or none = the
 * built-in look). [TouchControlsOverlay] asks [bitmapForKey] per button and falls
 * back to the bundled drawables when a skin is unset or missing an image — so the
 * default experience is unchanged.
 */
object ControllerSkinStore {

    private const val KEY_ACTIVE = "skin.active"

    /** Selected skin id, or null = built-in. Backed state so the overlay recomposes
     *  when it changes. Lazily hydrated from prefs on first use ([ensureLoaded]). */
    val activeSkinId = mutableStateOf<String?>(null)
    @Volatile private var loaded = false

    /** Logical button key -> filename inside a skin folder (iOS scheme). The face /
     *  shoulder / system / d-pad buttons use `ic_controller_<key>_button.png`; the
     *  analog ring + thumb drop the `_button` suffix. */
    private val FILE: Map<String, String> = buildMap {
        for (k in listOf(
            "cross", "circle", "square", "triangle",
            "l1", "l2", "l3", "r1", "r2", "r3",
            "start", "select", "up", "down", "left", "right",
        )) put(k, "ic_controller_${k}_button.png")
        put("analog_base", "ic_controller_analog_base.png")
        put("analog_stick", "ic_controller_analog_stick.png")
    }
    /** Canonical logical key for an incoming image filename, accepting BOTH the
     *  bundled iOS scheme (`ic_controller_<key>_button.png`) AND the bare names
     *  community / Delta-style packs actually ship (`cross.png`, `L1.png`,
     *  `analog_base.png`). Strips an optional `ic_controller_` prefix + `_button`
     *  suffix, lower-cases, takes the basename, and rejects macOS `._` resource
     *  forks + non-png. Returns null for anything not a known button image. */
    private fun keyForFilename(name: String): String? {
        val n = name.substringAfterLast('/').substringAfterLast('\\').lowercase()
        if (n.startsWith("._") || !n.endsWith(".png")) return null
        var core = n.removeSuffix(".png").removePrefix("ic_controller_").removeSuffix("_button")
        // Newer skin packs split the analog thumb into per-side images
        // (ic_controller_analog_stick_left/right.png). The overlay renders a single
        // thumb for both sticks, so fold both onto the one analog_stick slot (whichever
        // the pack ships — they're normally identical). analog_base + the older single
        // ic_controller_analog_stick.png keep working unchanged.
        if (core == "analog_stick_left" || core == "analog_stick_right") core = "analog_stick"
        return if (FILE.containsKey(core)) core else null
    }

    // Import caps (generous but bounded — phone storage).
    private const val MAX_IMAGES = 24
    private const val MAX_IMAGE_BYTES = 8L * 1024 * 1024

    private fun root(ctx: Context): File = File(ctx.filesDir, "controllerskins").apply { mkdirs() }

    private fun ensureLoaded(ctx: Context) {
        if (loaded) return
        val saved = Main.prefs.getString(KEY_ACTIVE, null)
        activeSkinId.value = saved?.takeIf {
            builtinFor(it) != null || File(root(ctx), it).isDirectory
        }
        loaded = true
    }

    /** Eagerly hydrate the saved selection at app start (Main.onCreate) so the in-game
     *  touch overlay applies the active skin on FIRST render — without it, activeSkinId
     *  stays null until the Skins tab is opened, so the skin only "took" after a
     *  re-select even though it showed as selected. */
    fun load(context: Context) = ensureLoaded(context)

    data class Skin(val id: String, val name: String, val imageCount: Int)

    /** Skins bundled in app assets (under `assets/<assetDir>/`), selectable as
     *  secondary defaults without importing. id is prefixed "builtin:" so it can
     *  never collide with an imported skin's folder name. */
    data class BuiltinSkin(val id: String, val name: String, val assetDir: String)
    val BUILTIN: List<BuiltinSkin> = listOf(
        BuiltinSkin("builtin:nethersx2", "NetherSX2", "controller_skins/nethersx2"),
        BuiltinSkin("builtin:nethersx2_old", "NetherSX2 Old", "controller_skins/nethersx2_old"),
    )
    private fun builtinFor(id: String?): BuiltinSkin? = BUILTIN.firstOrNull { it.id == id }

    /** Imported skins that have at least one recognized image. */
    fun list(ctx: Context): List<Skin> {
        ensureLoaded(ctx)
        val dirs = root(ctx).listFiles { f -> f.isDirectory && !f.name.endsWith(".tmp") } ?: return emptyList()
        return dirs.mapNotNull { dir ->
            val n = dir.listFiles { f -> f.isFile && keyForFilename(f.name) != null }?.size ?: 0
            if (n == 0) null else Skin(dir.name, prettyName(dir.name), n)
        }.sortedBy { it.name.lowercase() }
    }

    fun setActive(ctx: Context, id: String?) {
        ensureLoaded(ctx)
        activeSkinId.value = id
        Main.prefs.edit().apply { if (id == null) remove(KEY_ACTIVE) else putString(KEY_ACTIVE, id) }.apply()
        clearCache()
    }

    fun delete(ctx: Context, id: String) {
        File(root(ctx), id).deleteRecursively()
        if (activeSkinId.value == id) setActive(ctx, null) else clearCache()
    }

    // ---- Runtime image cache (active skin) ----------------------------------
    // Decode-once per (skin,key); null is cached too ("this skin has no image for
    // this button" -> use the built-in). Guarded: read on the UI/draw thread,
    // cleared from the IO import thread.
    private val cache = HashMap<String, ImageBitmap?>()

    private fun clearCache() = synchronized(cache) { cache.clear() }

    /** ImageBitmap for [key] from the active skin, or null to use the built-in. */
    fun bitmapForKey(ctx: Context, key: String): ImageBitmap? {
        ensureLoaded(ctx)
        val id = activeSkinId.value ?: return null
        val fname = FILE[key] ?: return null
        val ck = "$id/$key"
        synchronized(cache) { if (cache.containsKey(ck)) return cache[ck] }
        val builtin = builtinFor(id)
        val bmp = if (builtin != null) {
            // Bundled skin: decode straight from app assets.
            runCatching {
                ctx.assets.open("${builtin.assetDir}/$fname").use {
                    BitmapFactory.decodeStream(it)?.asImageBitmap()
                }
            }.getOrNull()
        } else {
            val f = File(File(root(ctx), id), fname)
            if (f.isFile)
                runCatching { BitmapFactory.decodeFile(f.absolutePath)?.asImageBitmap() }.getOrNull()
            else null
        }
        synchronized(cache) { cache[ck] = bmp }
        return bmp
    }

    // ---- Import -------------------------------------------------------------

    /** Import a skin from a picked FOLDER (SAF tree) of `ic_controller_*.png`.
     *  Returns the new skin id, or null if nothing usable was found. IO — call off
     *  the main thread. */
    fun importFromTree(ctx: Context, treeUri: Uri): String? {
        val tree = DocumentFile.fromTreeUri(ctx, treeUri) ?: return null
        val files = tree.listFiles().filter { it.isFile && keyForFilename(it.name ?: "") != null }
        if (files.isEmpty()) return null
        val id = newId(ctx, tree.name ?: "skin")
        val tmp = File(root(ctx), "$id.tmp").apply { deleteRecursively(); mkdirs() }
        var count = 0
        for (df in files) {
            if (count >= MAX_IMAGES) break
            if (df.length() > MAX_IMAGE_BYTES) continue
            val key = keyForFilename(df.name ?: "") ?: continue
            val out = File(tmp, FILE.getValue(key))
            val ok = runCatching {
                ctx.contentResolver.openInputStream(df.uri)?.use { ins ->
                    out.outputStream().use { ins.copyTo(it) }
                } != null
            }.getOrDefault(false)
            // Delete the partial/truncated file on a mid-copy failure so it
            // isn't promoted by commit() and later counted/decoded as a real image.
            if (ok) count++ else out.delete()
        }
        return commit(ctx, tmp, id, count)
    }

    /** Import a skin from a picked .zip of `ic_controller_*.png` (flattened to root). */
    fun importFromZip(ctx: Context, zipUri: Uri): String? {
        val raw = DocumentFile.fromSingleUri(ctx, zipUri)?.name
            ?.removeSuffix(".zip")?.removeSuffix(".ZIP") ?: "skin"
        val id = newId(ctx, raw)
        val tmp = File(root(ctx), "$id.tmp").apply { deleteRecursively(); mkdirs() }
        var count = 0
        runCatching {
            ctx.contentResolver.openInputStream(zipUri)?.use { stream ->
                ZipInputStream(stream).use { zin ->
                    while (true) {
                        val e = zin.nextEntry ?: break
                        val key = if (e.isDirectory) null else keyForFilename(e.name)
                        if (key != null && count < MAX_IMAGES) {
                            val out = File(tmp, FILE.getValue(key))
                            // Per-entry guard: a mid-copy failure deletes that
                            // entry's partial file (instead of leaving a truncated
                            // one) and lets the remaining entries still import.
                            val ok = runCatching {
                                out.outputStream().use { zin.copyTo(it) }
                            }.isSuccess
                            if (ok) count++ else out.delete()
                        }
                        zin.closeEntry()
                    }
                }
            }
        }
        return commit(ctx, tmp, id, count)
    }

    private fun commit(ctx: Context, tmp: File, id: String, count: Int): String? {
        if (count == 0) { tmp.deleteRecursively(); return null }
        val target = File(root(ctx), id)
        target.deleteRecursively()
        if (!tmp.renameTo(target)) { tmp.deleteRecursively(); return null }
        clearCache()
        return id
    }

    private fun newId(ctx: Context, name: String): String {
        val base = name.lowercase().replace(Regex("[^a-z0-9._-]"), "_")
            .trim('_').ifEmpty { "skin" }
        var id = base
        var i = 1
        while (File(root(ctx), id).exists()) id = "${base}_${i++}"
        return id
    }

    private fun prettyName(id: String) = id.replace('_', ' ').trim()
}
