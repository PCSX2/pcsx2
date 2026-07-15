package com.armsx2

import com.armsx2.runtime.MainActivityRuntime

import android.content.Context
import android.net.Uri
import androidx.compose.runtime.mutableStateOf
import java.io.File

/**
 * Box-art style for the library: 2D flat scans (the "default" mirror, JPG) or
 * 3D rendered cases (the "3d" mirror, PNG). Both come from the same xlenore
 * repos. Persisted in MainActivityRuntime.prefs and read by [GameInfo.coverUrl], so flipping
 * it recomposes the grid and re-downloads covers in the chosen style.
 */
object CoverArtStyle {
    private const val KEY = "library.coverArt3d"
    val use3d = mutableStateOf(false)
    fun load() { use3d.value = MainActivityRuntime.prefs.getBoolean(KEY, false) }
    fun set(value: Boolean) {
        use3d.value = value
        MainActivityRuntime.prefs.edit().putBoolean(KEY, value).apply()
    }
}

/** Show the game title under every cover in the main library grid (the old-UI behaviour),
 *  not only where a name is otherwise shown. Toggled from the library 3-dot overflow menu. */
object GridLabels {
    private const val KEY = "library.showGridNames"
    val show = mutableStateOf(false)
    fun load() { show.value = MainActivityRuntime.prefs.getBoolean(KEY, false) }
    fun set(value: Boolean) {
        show.value = value
        MainActivityRuntime.prefs.edit().putBoolean(KEY, value).apply()
    }
}

/**
 * Games the user has marked hidden from the library (long-press → Hide). Persisted by game URI so
 * it survives rescans. Also the intended way to get rid of stray non-game files that show up in the
 * list (e.g. BIOS dumps kept in a scanned subfolder). "Show hidden" reveals them so they can be
 * unhidden again.
 */
object HiddenGames {
    private const val KEY = "library.hiddenGames"
    private const val SHOW_KEY = "library.showHidden"
    val hidden = mutableStateOf<Set<String>>(emptySet())
    val showHidden = mutableStateOf(false)
    private fun keyOf(game: GameInfo) = game.uri.toString()
    fun load() {
        hidden.value = MainActivityRuntime.prefs.getStringSet(KEY, emptySet())?.toSet() ?: emptySet()
        showHidden.value = MainActivityRuntime.prefs.getBoolean(SHOW_KEY, false)
    }
    fun isHidden(game: GameInfo) = hidden.value.contains(keyOf(game))
    fun setHidden(game: GameInfo, value: Boolean) {
        val next = hidden.value.toMutableSet().apply { if (value) add(keyOf(game)) else remove(keyOf(game)) }
        hidden.value = next
        MainActivityRuntime.prefs.edit().putStringSet(KEY, next).apply()
    }
    fun setShowHidden(value: Boolean) {
        showHidden.value = value
        MainActivityRuntime.prefs.edit().putBoolean(SHOW_KEY, value).apply()
    }
}

/**
 * Library toggle: show the game title under each cover on the shelves. Off by
 * default — the cover already carries the title and a label under every card
 * crowds the shelf UI — but exposed as a quick toggle on the library's left
 * rail for users who keep multiple versions of a game or browse by name.
 */
object LibraryTitles {
    private const val KEY = "library.showTitles"
    val show = mutableStateOf(false)
    fun load() { show.value = MainActivityRuntime.prefs.getBoolean(KEY, false) }
    fun set(value: Boolean) {
        show.value = value
        MainActivityRuntime.prefs.edit().putBoolean(KEY, value).apply()
    }
}

/** Whether the "Recently Played" shelf shows above the library (#263). Off =
 *  a single unified library with no recent shelf. Default on. */
object LibraryRecentShelf {
    private const val KEY = "library.showRecentlyPlayed"
    val show = mutableStateOf(true)
    fun load() { show.value = MainActivityRuntime.prefs.getBoolean(KEY, true) }
    fun set(value: Boolean) {
        show.value = value
        MainActivityRuntime.prefs.edit().putBoolean(KEY, value).apply()
    }
}

/**
 * Library view options: switch between the cover SHELF view and a compact LIST
 * view (game names only) for fast finding on small screens, plus a manual grid
 * size (columns + rows) that drives cover size in shelf view. 0 = Auto.
 * Persisted in MainActivityRuntime.prefs and observed by the library so changes recompose the grid.
 */
object LibraryView {
    private const val KEY_LIST = "library.listMode"
    private const val KEY_COLS = "library.gridColumns"
    private const val KEY_ROWS = "library.gridRows"
    const val MAX_COLS = 6
    const val MAX_ROWS = 5
    /** true = compact name list; false = cover shelves. */
    val listMode = mutableStateOf(false)
    /** Covers per row in shelf view; 0 = auto-fit to screen width. */
    val columns = mutableStateOf(0)
    /** Target visible rows in shelf view (caps cover height); 0 = auto. */
    val rows = mutableStateOf(0)
    fun load() {
        listMode.value = MainActivityRuntime.prefs.getBoolean(KEY_LIST, false)
        columns.value = MainActivityRuntime.prefs.getInt(KEY_COLS, 0).coerceIn(0, MAX_COLS)
        rows.value = MainActivityRuntime.prefs.getInt(KEY_ROWS, 0).coerceIn(0, MAX_ROWS)
    }
    fun setListMode(v: Boolean) {
        listMode.value = v
        MainActivityRuntime.prefs.edit().putBoolean(KEY_LIST, v).apply()
    }
    fun toggleListMode() = setListMode(!listMode.value)
    /** Cycle columns Auto→2→3→…→MAX→Auto (shelf view cover size). */
    fun cycleColumns() {
        val next = when {
            columns.value <= 0 -> 2
            columns.value >= MAX_COLS -> 0
            else -> columns.value + 1
        }
        columns.value = next
        MainActivityRuntime.prefs.edit().putInt(KEY_COLS, next).apply()
    }
    /** Cycle rows Auto→2→3→…→MAX→Auto (caps cover height). */
    fun cycleRows() {
        val next = when {
            rows.value <= 0 -> 2
            rows.value >= MAX_ROWS -> 0
            else -> rows.value + 1
        }
        rows.value = next
        MainActivityRuntime.prefs.edit().putInt(KEY_ROWS, next).apply()
    }
}

/**
 * One row in the games-list screen. Today the title/serial come from
 * filename parsing — game titles like "Final Fantasy X (USA) [SLUS-20312]"
 * are common dump conventions. compatibility is left at 0 (no stars filled)
 * until we add a gamedb JNI bridge.
 *
 * `platform` distinguishes PS1 ("ps1") from PS2 ("ps2") so we hit the
 * right cover repo: xlenore/ps2-covers vs xlenore/psx-covers. Native
 * (getGameSerialFromFd) tags its return with the platform when SYSTEM.CNF
 * is parseable; filename-only fallback defaults to "ps2".
 */
enum class GamePlatform(val key: String) {
    PS2("ps2"),
    PS1("ps1");

    companion object {
        fun fromKey(s: String?): GamePlatform =
            if (s == "ps1") PS1 else PS2
    }
}

data class GameInfo(
    val uri: Uri,
    val title: String,
    val serial: String?,
    val compatibility: Int = 0,    // 0..5 (TODO: pull from gamedb)
    val extension: String = "",    // upper-case container ext, e.g. "ISO", "CHD"
    val platform: GamePlatform = GamePlatform.PS2,
) {
    val coverUrl: String? get() = serial?.let { s ->
        val repo = when (platform) {
            GamePlatform.PS2 -> "ps2-covers"
            GamePlatform.PS1 -> "psx-covers"
        }
        // 3D cases live under covers/3d/*.png; flat 2D scans under
        // covers/default/*.jpg. Coil decodes by content, so the extension
        // mismatch on the cached file is fine.
        if (CoverArtStyle.use3d.value)
            "https://raw.githubusercontent.com/xlenore/$repo/main/covers/3d/$s.png"
        else
            "https://raw.githubusercontent.com/xlenore/$repo/main/covers/default/$s.jpg"
    }

    /** Human-readable region (USA / Europe / Japan / India / China / …). Prefers the
     *  curated GameDB region (so India/China/Korea/HK releases that share a serial PREFIX
     *  with Europe/Japan are labelled correctly — matching PCSX2), falling back to the
     *  serial-prefix heuristic when the serial isn't in the database. Shown under the
     *  cover so users can tell apart multiple regional versions of the same game. */
    val region: String? get() = serial?.let { gameDbRegion(it) ?: regionForSerial(it) }

    /** Region as a flag emoji (🇺🇸 / 🇪🇺 / 🇯🇵 / …) for the cover label, or null.
     *  Rendered ahead of the title so the region is always visible even when a
     *  long name wraps/ellipsizes. */
    val regionFlag: String? get() = region?.let { regionFlagFor(it) }

    /** A short version/edition tag shown under the title so two copies of the
     *  same game can be told apart: a disc-version token parsed from the dump
     *  filename (e.g. "v3.00") when present, otherwise the serial. */
    val versionTag: String? get() {
        val name = uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
        return name?.let { FilenameParser.versionTokenOf(it) } ?: serial
    }

    /** Stable per-game identity used to key per-game SETTINGS (config.game.<key>).
     *  Disc games use their serial (byte-identical to before). Serial-less
     *  ELF/homebrew fall back to a normalized filename stem so their per-game
     *  settings persist across a reboot — a serial-keyed store silently dropped
     *  them to global at boot, which is issue #253. Derived purely from the ROM
     *  path so the boot path and the in-game overlay resolve the SAME key (the
     *  bug was the overlay saving under one key while boot read another). Stem
     *  keys can collide if two ELFs share a filename; acceptable for homebrew. */
    val settingsKey: String? get() = serial?.takeIf { it.isNotBlank() }
        ?: uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
            ?.substringBeforeLast('.')?.trim()?.takeIf { it.isNotEmpty() }
}

/**
 * User-supplied cover overrides for games the online repo can't match — homebrew,
 * ELF ports, obscure dumps with no serial. Stored as image files under
 * `<dataRoot>/covers/custom/`. A cover is matched case-insensitively (png / jpg /
 * jpeg / webp) by the game's serial, its ROM filename, OR its displayed title — so
 * dropping in a file named after the game just works, and the in-app picker writes
 * to the same folder. A custom cover always wins over the online repo cover.
 */
object CustomCovers {
    /** Bumped on set/remove so cover tiles re-resolve. */
    val version = mutableStateOf(0)

    // MainActivityRuntime.assetCopyRoot() can flip between the chosen system dir and the
    // app-private fallback depending on a transient write-probe — so covers got
    // stored under one root and looked up under another ("sometimes there,
    // sometimes not"). Cache the covers root on first resolve so set + load always
    // agree, and share this exact cache with the library cover loader.
    @Volatile
    private var cachedCoversRoot: File? = null
    fun coversRoot(context: Context): File =
        cachedCoversRoot ?: File(MainActivityRuntime.assetCopyRoot(context), "covers").also { cachedCoversRoot = it }

    private fun dir(context: Context): File = File(coversRoot(context), "custom")

    private fun filenameStem(game: GameInfo): String? =
        game.uri.lastPathSegment?.substringAfterLast('/')?.substringAfterLast(':')
            ?.substringBeforeLast('.')?.trim()?.takeIf { it.isNotEmpty() }

    /** Names the user might give the cover file, highest priority first. */
    private fun keys(game: GameInfo): List<String> = buildList {
        game.serial?.takeIf { it.isNotBlank() }?.let { add(it) }
        filenameStem(game)?.let { add(it) }
        game.title.takeIf { it.isNotBlank() }?.let { add(it) }
    }

    /** Load all custom covers as lowercased-stem -> File, in ONE directory
     *  listing. The library preloads this once (and refreshes on [version]) so
     *  cover tiles can resolve synchronously instead of each doing its own I/O
     *  during a scroll — which raced and mis-assigned covers across games. */
    fun loadAll(context: Context): Map<String, File> {
        val files = dir(context).listFiles()?.filter { it.isFile && it.length() > 0L } ?: return emptyMap()
        if (files.isEmpty()) return emptyMap()
        val byStem = HashMap<String, File>(files.size)
        for (f in files) byStem.putIfAbsent(f.nameWithoutExtension.lowercase(), f)
        return byStem
    }

    /** Resolve [game]'s cover against a preloaded [loadAll] map. No I/O.
     *  Look-up keys are run through the same [sanitize] the writer uses, so a
     *  title containing : / \ etc. (which [targetFor] wrote as '_') still
     *  resolves; sanitize is a no-op for clean keys. */
    fun matchIn(map: Map<String, File>, game: GameInfo): File? {
        if (map.isEmpty()) return null
        for (k in keys(game)) map[sanitize(k).lowercase()]?.let { return it }
        return null
    }

    /** An existing custom cover for [game], or null. Single-game convenience
     *  (does its own listing) — used off the scroll path. */
    fun fileFor(context: Context, game: GameInfo): File? = matchIn(loadAll(context), game)

    /** Path the in-app picker writes to (serial if present, else ROM filename). */
    private fun targetFor(context: Context, game: GameInfo): File {
        val key = game.serial?.takeIf { it.isNotBlank() }
            ?: filenameStem(game) ?: game.title.ifBlank { "cover" }
        return File(dir(context), sanitize(key) + ".png")
    }

    /** Copy [source] in as [game]'s cover, replacing any prior one. */
    fun set(context: Context, game: GameInfo, source: Uri): Boolean = runCatching {
        remove(context, game)
        val target = targetFor(context, game)
        target.parentFile?.mkdirs()
        context.contentResolver.openInputStream(source)?.use { ins ->
            target.outputStream().use { outs -> ins.copyTo(outs) }
        }
        (target.isFile && target.length() > 0L).also { if (it) version.value++ }
    }.getOrDefault(false)

    fun remove(context: Context, game: GameInfo): Boolean {
        val f = fileFor(context, game) ?: return false
        return f.delete().also { if (it) version.value++ }
    }

    private fun sanitize(s: String): String =
        s.replace(Regex("""[/\\:*?"<>|\n\r\t]"""), "_").trim().ifEmpty { "cover" }
}

/** Map a PS1/PS2 serial prefix to a region label. */
// GameDB region cache (serial -> mapped label, or "" = looked up & not in DB / no JNI).
// One native lookup per serial, then memoized so the cover/flag render path stays cheap
// during scroll. The GameDatabase is loaded by the time the library shows (the compat
// stars use it too), so this resolves for listed games.
private val gameDbRegionCache = java.util.concurrent.ConcurrentHashMap<String, String>()

/** The mapped region label from PCSX2's GameDB (India / China / Korea / Hong Kong / …),
 *  or null when the serial isn't in the database — the caller then falls back to the
 *  serial-prefix heuristic. Matches PCSX2, which a serial PREFIX can't (SCES = both
 *  Europe AND India, etc.). */
fun gameDbRegion(serial: String): String? {
    val cached = gameDbRegionCache.getOrPut(serial) {
        val raw = runCatching { kr.co.iefriends.pcsx2.NativeApp.getRegionForSerial(serial) }
            .getOrNull().orEmpty()
        mapGameDbRegion(raw) ?: ""
    }
    return cached.takeIf { it.isNotEmpty() }
}

/** Map a PCSX2 GameDB region string ("NTSC-U", "PAL-E", "PAL-IN", "NTSC-C", "NTSC-HK", …)
 *  to our display label. */
private fun mapGameDbRegion(raw: String): String? {
    val u = raw.trim().uppercase()
    if (u.isEmpty()) return null
    return when {
        u == "PAL-IN" || u.contains("INDIA") -> "India"
        u.startsWith("NTSC-U") -> "USA"
        u.startsWith("NTSC-J") -> "Japan"
        u.startsWith("NTSC-K") -> "Korea"
        u.startsWith("NTSC-HK") -> "Hong Kong"
        u.startsWith("NTSC-C") -> "China"          // NTSC-C, NTSC-C-E, NTSC-C-J
        u.startsWith("NTSC-A") || u == "NTSC" -> "Asia"
        u.startsWith("PAL") -> "Europe"            // PAL, PAL-E, PAL-A, … (PAL-IN handled above)
        else -> null
    }
}

fun regionForSerial(serial: String): String? = when (serial.take(4).uppercase()) {
    "SLUS", "SCUS", "PBPX", "LSP0" -> "USA"
    "SLES", "SCES", "SLED", "SCED", "SLPN" -> "Europe"
    "SLPS", "SLPM", "SCPS", "SCAJ", "ALCH", "PAPX", "ROSE", "TCPS", "KOEI", "PCPX", "CPCS" -> "Japan"
    "SLKA", "SCKA" -> "Korea"
    "SLAJ" -> "Asia"
    else -> null
}

/** Map a region label to a flag emoji. Asia falls back to a globe. */
fun regionFlagFor(region: String): String? = when (region) {
    "USA" -> "🇺🇸"
    "Europe" -> "🇪🇺"
    "Japan" -> "🇯🇵"
    "Korea" -> "🇰🇷"
    "India" -> "🇮🇳"
    "China" -> "🇨🇳"
    "Hong Kong" -> "🇭🇰"
    "Asia" -> "🌏"
    else -> null
}

/**
 * Best-effort serial extractor. Recognized dump conventions:
 *   "Game (USA) [SLUS-20312].iso"      → SLUS-20312
 *   "Game (USA) [SLUS_203.12].iso"     → SLUS-20312
 *   "SCUS_972.28 - Game.iso"           → SCUS-97228
 *   "slus_203.12.iso"                  → SLUS-20312
 *
 * The pattern matches 4 letters + optional separator + 3 digits + optional
 * dot + 2 digits, normalized to "AAAA-NNNNN" upper-case.
 */
object FilenameParser {
    private val serialRegex = Regex("""([A-Za-z]{4})[\s_-]?(\d{3})\.?(\d{2})""")
    private val tagsRegex = Regex("""[\[(].*?[\])]""")
    // Disc-version token (e.g. "v3.00", "v 1.0"). The 'v' prefix is required so
    // release years ("(2004)") and unrelated x.y numbers aren't mistaken for it.
    private val versionRegex = Regex("""(?i)\bv\.?\s?(\d{1,2}(?:\.\d{1,2}){1,2})\b""")

    /** A disc-version token like "v3.00" parsed from a filename, or null. Lets
     *  two copies of the same game (same serial, different disc revision) be told
     *  apart when the dump filename carries the version. */
    fun versionTokenOf(filename: String): String? =
        versionRegex.find(filename)?.let { "v" + it.groupValues[1] }
    private val whitespaceRegex = Regex("""\s+""")
    private val nonWordRegex = Regex("""[^a-z0-9]+""")

    private data class FilenameAlias(val title: String, val serial: String)

    private fun aliasFor(filenameWithoutExt: String): FilenameAlias? {
        val normalized = filenameWithoutExt
            .lowercase()
            .replace(nonWordRegex, " ")
            .trim()

        // Some PAL DMC2 CHDs are named by disc character rather than serial,
        // and their raw-CD layout can be awkward to probe. Keep this narrow so
        // broader filename-only games still rely on explicit serial tokens.
        if (!normalized.contains("devil may cry 2"))
            return null

        return when {
            normalized.contains("dante") ->
                FilenameAlias("Devil May Cry 2 [Dante Disc]", "SLES-82011")
            normalized.contains("lucia") ->
                FilenameAlias("Devil May Cry 2 [Lucia Disc]", "SLES-82012")
            else -> null
        }
    }

    fun parse(filename: String): Pair<String, String?> {
        val withoutExt = filename.substringBeforeLast('.')
        val match = serialRegex.find(withoutExt)
        val serial = match?.let {
            "${it.groupValues[1].uppercase()}-${it.groupValues[2]}${it.groupValues[3]}"
        }
        if (serial == null) {
            aliasFor(withoutExt)?.let { return it.title to it.serial }
        }
        // Strip the matched serial token + any [region] / (lang) tags so the
        // displayed title is the game name rather than the full filename.
        var title = withoutExt
        if (match != null) title = title.replace(match.value, "")
        title = title.replace(tagsRegex, "")
            .replace(whitespaceRegex, " ")
            .trim(' ', '-', '_', '.')
        if (title.isEmpty()) title = withoutExt
        return title to serial
    }
}
