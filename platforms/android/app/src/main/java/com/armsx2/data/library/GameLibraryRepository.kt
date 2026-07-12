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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

class GameLibraryRepository(private val context: Context) {
    private val gameExtensions = setOf(
        "iso", "chd", "cso", "zso", "gz", "bin", "mdf", "img", "nrg", "dump", "elf",
    )

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
        val (title, fileSerial) = FilenameParser.parse(name)
        val serial = probeSerial ?: fileSerial
        val compatibility = serial
            ?.let { runCatching { NativeApp.getCompatibilityForSerial(it) }.getOrDefault(0) }
            ?.minus(1)
            ?.coerceIn(0, 5)
            ?: 0
        return GameInfo(
            uri = uri,
            title = title,
            serial = serial,
            compatibility = compatibility,
            extension = extension.uppercase(),
            platform = probePlatform ?: GamePlatform.PS2,
        )
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
