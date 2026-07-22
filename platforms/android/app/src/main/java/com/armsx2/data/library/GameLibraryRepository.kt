package com.armsx2.data.library

import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.ParcelFileDescriptor
import androidx.core.content.edit
import androidx.core.net.toUri
import androidx.documentfile.provider.DocumentFile
import com.armsx2.FilenameParser
import com.armsx2.GameInfo
import com.armsx2.GamePlatform
import com.armsx2.runtime.MainActivityRuntime
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

class GameLibraryRepository(private val context: Context) {
    private val gameExtensions = setOf(
        "iso", "chd", "cso", "zso", "gz", "bin", "mdf", "img", "nrg", "dump", "elf",
    )

    // Recent-games export runs off the launch/UI thread; exportLock serialises the file
    // write so a quick play-then-remove can't interleave two writers on the same file.
    private val exportScope = CoroutineScope(Dispatchers.IO)
    private val exportLock = Any()

    fun cacheKey(directories: List<String>): String = directories.sorted().joinToString("|")

    fun loadCached(): CachedLibrary {
        val cachedKey = MainActivityRuntime.prefs.getString("gamesCacheKey", null)
            ?: MainActivityRuntime.prefs.getString("gamesCacheDir", null)
        val json = MainActivityRuntime.prefs.getString("gamesCache", null)
            ?: return CachedLibrary(cachedKey, emptyList())
        val games = runCatching {
            val array = JSONArray(json)
            buildList {
                repeat(array.length()) { index ->
                    val item = array.getJSONObject(index)
                    add(
                        GameInfo(
                            uri = item.getString("uri").toUri(),
                            title = item.getString("title"),
                            serial = if (item.isNull("serial")) null else item.optString("serial").takeIf(String::isNotBlank),
                            compatibility = item.optInt("compat", 0),
                            extension = item.optString("ext").ifBlank {
                                item.getString("uri").substringAfterLast('.', "").uppercase()
                            },
                            platform = GamePlatform.fromKey(item.optString("platform").takeIf(String::isNotBlank)),
                            // Absent in a cache written before #338 — optString gives "",
                            // which reads as "no separate sort key / not translated", so an
                            // old cache degrades to the previous behaviour until a rescan.
                            titleSort = item.optString("titleSort"),
                            titleEn = item.optString("titleEn"),
                        ),
                    )
                }
            }
        }.getOrDefault(emptyList())
        return CachedLibrary(cachedKey, games)
    }

    suspend fun scan(directories: List<String>): List<GameInfo> = withContext(Dispatchers.IO) {
        val collected = linkedMapOf<String, GameInfo>()
        directories.forEach { rawUri ->
            val uri = runCatching { rawUri.toUri() }.getOrNull() ?: return@forEach
            val rawRoot = if (canUseRawStorage()) MainActivityRuntime.resolveTreeUriToPosix(rawUri)?.let(::File) else null
            if (rawRoot?.isDirectory == true) {
                scanRawDirectory(rawRoot, collected, 0)
            } else {
                DocumentFile.fromTreeUri(context, uri)?.let { scanDocumentTree(it, collected, 0) }
            }
        }
        collected.values.sortedBy { it.title.lowercase() }.also { saveCache(directories, it) }
    }

    fun recentGames(allGames: List<GameInfo>): List<GameInfo> {
        val raw = MainActivityRuntime.prefs.getString("recentGameUris", null) ?: return emptyList()
        val order = runCatching {
            val array = JSONArray(raw)
            List(array.length()) { array.getString(it) }
        }.getOrDefault(emptyList())
        val byUri = allGames.associateBy { it.uri.toString() }
        return order.mapNotNull(byUri::get)
    }

    fun markPlayed(game: GameInfo) {
        val uri = game.uri.toString()
        val current = runCatching {
            MainActivityRuntime.prefs.getString("recentGameUris", null)?.let(::JSONArray)?.let { array ->
                MutableList(array.length()) { array.getString(it) }
            }
        }.getOrNull() ?: mutableListOf()
        current.remove(uri)
        current.add(0, uri)
        while (current.size > 12) current.removeAt(current.lastIndex)
        MainActivityRuntime.prefs.edit {
            putString(
                "recentGameUris",
                JSONArray(current).toString()
            )
        }
        val snapshot = current.toList()
        exportScope.launch { exportRecentGamesPublic(snapshot, game) }
    }

    /**
     * Drop a single game from Recently Played without touching the library or the
     * global "Show Recently Played" toggle. It naturally returns to the top of the
     * list the next time it's launched (markPlayed re-adds it).
     */
    fun removeFromRecent(game: GameInfo) {
        val uri = game.uri.toString()
        val current = runCatching {
            MainActivityRuntime.prefs.getString("recentGameUris", null)?.let(::JSONArray)?.let { array ->
                MutableList(array.length()) { array.getString(it) }
            }
        }.getOrNull() ?: return
        if (!current.remove(uri)) return
        MainActivityRuntime.prefs.edit {
            putString(
                "recentGameUris",
                JSONArray(current).toString()
            )
        }
        val snapshot = current.toList()
        exportScope.launch { exportRecentGamesPublic(snapshot) }
    }

    /**
     * Mirrors the recently-played list to a plain `recent_games.json` under the app's data
     * root (the shared-storage folder the user picked, next to gamesettings/ and memcards/;
     * or the app-private externalFilesDir when none was chosen). `recentGameUris` lives in
     * app-private SharedPreferences no other app can read, so this hands companion tools
     * (launchers, offline RA caches) the same "recently played" data they already read from
     * that folder. Runs on exportScope (IO) so the cache parse + write never touch the
     * launch/UI thread; exportLock serialises the write. Feature contributed by misantronic
     * (PR #391), reworked here to run off-thread and to also fire on removal.
     */
    private fun exportRecentGamesPublic(orderedUris: List<String>, justPlayed: GameInfo? = null) {
        val root = MainActivityRuntime.systemDirPosix()
            ?: context.getExternalFilesDir(null)?.absolutePath
            ?: return
        val cached = loadCached().games
        val byUri = (if (justPlayed != null) cached + justPlayed else cached).associateBy { it.uri.toString() }
        val array = JSONArray()
        orderedUris.forEach { uriString ->
            val g = byUri[uriString] ?: return@forEach
            array.put(JSONObject().apply {
                put("uri", g.uri.toString())
                put("title", g.title)
                put("serial", g.serial ?: JSONObject.NULL)
                put("ext", g.extension)
                put("platform", g.platform.key)
            })
        }
        synchronized(exportLock) {
            runCatching { File(root, "recent_games.json").writeText(array.toString()) }
        }
    }

    private fun scanDocumentTree(
        directory: DocumentFile,
        output: MutableMap<String, GameInfo>,
        depth: Int,
    ) {
        if (depth > MaxScanDepth) return
        val children = runCatching { directory.listFiles() }.getOrNull() ?: return
        children.forEach { file ->
            if (file.isDirectory) {
                scanDocumentTree(file, output, depth + 1)
                return@forEach
            }
            val name = file.name ?: return@forEach
            val extension = name.substringAfterLast('.', "").lowercase()
            if (extension !in gameExtensions) return@forEach
            val probe = if (extension in probeExtensions) probeDocument(file.uri) else null
            output.putIfAbsent(file.uri.toString(), createGame(file.uri, name, extension, probe))
        }
    }

    private fun scanRawDirectory(
        directory: File,
        output: MutableMap<String, GameInfo>,
        depth: Int,
    ) {
        if (depth > MaxScanDepth) return
        val children = runCatching { directory.listFiles() }.getOrNull() ?: return
        children.forEach { file ->
            if (file.isDirectory) {
                scanRawDirectory(file, output, depth + 1)
                return@forEach
            }
            val extension = file.extension.lowercase()
            if (extension !in gameExtensions) return@forEach
            val uri = Uri.fromFile(file)
            val probe = if (extension in probeExtensions) probeRaw(file) else null
            output.putIfAbsent(uri.toString(), createGame(uri, file.name, extension, probe))
        }
    }

    private fun createGame(uri: Uri, name: String, extension: String, rawProbe: String?): GameInfo {
        val (probeSerial, probePlatform) = parseProbe(rawProbe)
        val (fileTitle, fileSerial) = FilenameParser.parse(name)
        val serial = probeSerial ?: fileSerial
        val compatibility = serial
            ?.let { runCatching { NativeApp.getCompatibilityForSerial(it) }.getOrDefault(0) }
            ?.minus(1)
            ?.coerceIn(0, 5)
            ?: 0
        // GameDB title first, filename only as the fallback — the same order GameList.cpp
        // uses. The database is the curated name: it drops dump cruft ("(USA) [!] v1.1"),
        // and for a Japanese game it is the ACTUAL Japanese title, which no filename-derived
        // guess can produce. Issue #338.
        val db = serial?.let { dbTitles(it) }
        return GameInfo(
            uri = uri,
            title = db?.name?.takeIf { it.isNotEmpty() } ?: fileTitle,
            serial = serial,
            compatibility = compatibility,
            extension = extension.uppercase(),
            platform = probePlatform ?: GamePlatform.PS2,
            // Only meaningful alongside a DB title; a filename-derived one has no sort key
            // and is not a translation of anything.
            titleSort = db?.sort.orEmpty(),
            titleEn = db?.en.orEmpty(),
        )
    }

    private data class DbTitles(val name: String, val sort: String, val en: String)

    /** GameDB's three titles for [serial], or null when it isn't in the database. */
    private fun dbTitles(serial: String): DbTitles? {
        val raw = runCatching { NativeApp.getTitlesForSerial(serial) }.getOrNull()
        if (raw.isNullOrEmpty()) return null
        // "<name>\n<name-sort>\n<name-en>" — split with a limit so a title can't lose a
        // trailing field, and tolerate a short string from an older core.
        val parts = raw.split('\n')
        val name = parts.getOrNull(0).orEmpty()
        if (name.isEmpty()) return null
        return DbTitles(name, parts.getOrNull(1).orEmpty(), parts.getOrNull(2).orEmpty())
    }

    private fun parseProbe(value: String?): Pair<String?, GamePlatform?> {
        if (value.isNullOrBlank()) return null to null
        val separator = value.indexOf(':')
        if (separator <= 0) return value to null
        return value.substring(separator + 1) to GamePlatform.fromKey(value.substring(0, separator))
    }

    private fun probeDocument(uri: Uri): String? = runCatching {
        val descriptor = context.contentResolver.openFileDescriptor(uri, "r") ?: return null
        NativeApp.getGameSerialFromFd(descriptor.detachFd())
    }.getOrNull()

    private fun probeRaw(file: File): String? = runCatching {
        val descriptor = ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
        NativeApp.getGameSerialFromFd(descriptor.detachFd())
    }.getOrNull()

    private fun saveCache(directories: List<String>, games: List<GameInfo>) {
        val array = JSONArray()
        games.forEach { game ->
            array.put(JSONObject().apply {
                put("uri", game.uri.toString())
                put("title", game.title)
                put("serial", game.serial ?: JSONObject.NULL)
                put("compat", game.compatibility)
                put("ext", game.extension)
                put("platform", game.platform.key)
                put("titleSort", game.titleSort)
                put("titleEn", game.titleEn)
            })
        }
        MainActivityRuntime.prefs.edit {
            putString("gamesCacheKey", cacheKey(directories))
                .putString("gamesCache", array.toString())
            }
    }

    private fun canUseRawStorage(): Boolean =
        Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && Environment.isExternalStorageManager()

    data class CachedLibrary(val key: String?, val games: List<GameInfo>)

    private companion object {
        const val MaxScanDepth = 12
        val probeExtensions = setOf("iso", "bin", "chd", "img", "mdf", "nrg", "dump")
    }
}
