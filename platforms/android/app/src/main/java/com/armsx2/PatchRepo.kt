// SPDX-License-Identifier: GPL-3.0+
package com.armsx2

import android.util.Log
import kr.co.iefriends.pcsx2.HttpClient
import java.io.File

/**
 * Online patch/cheat fetcher — the patch-side counterpart to [CustomDriver].
 *
 * Pulls a game's `.pnach` from the canonical PCSX2 patch database (the same
 * project the bundled `assets/resources/patches.zip` is built from) and parses
 * it into individual, toggleable [Entry]s for the in-app browser.
 *
 * Repo facts (verified): default branch is **main**; files are named
 * `<SERIAL>_<CRC>.pnach` (modern) or `<CRC>.pnach` (legacy) under `patches/`
 * (widescreen / fixes) and `cheats/` (cheat codes). Each patch/cheat is a
 * `[Section]` block with optional `comment=`/`author=` and one or more `patch=`
 * lines.
 *
 * Network calls are blocking ([HttpClient.doRequest]); call [fetchForGame] off
 * the main thread.
 */
object PatchRepo {
    private const val TAG = "PatchRepo"
    // Widescreen / game-fix patches — the official PCSX2 DB (files named
    // <SERIAL>_<CRC>.pnach under patches/). This repo has NO cheats.
    private const val RAW_BASE = "https://raw.githubusercontent.com/PCSX2/pcsx2_patches/main"
    private const val TREE_URL = "https://api.github.com/repos/PCSX2/pcsx2_patches/git/trees/main?recursive=1"
    private const val USER_AGENT = "ARMSX2"

    // Gabominated's compilation fork — a SECOND patch source that adds improvement
    // patches the official DB lacks (No-Interlace, 60 FPS, and per-game fixes like
    // Mortal Kombat: Shaolin Monks' "Disable Blur/Bloom", which fixes the doubled-image
    // ghosting PCSX2's HW renderer can't avoid). Same <SERIAL>_<CRC>.pnach naming, under
    // a "PCSX2 Patches/" folder (note the space -> %20). Entries are MERGED with the
    // official DB's in the browser (deduped by name; official wins on a name clash).
    private const val GABO_BASE = "https://raw.githubusercontent.com/Gabominated/PCSX2/main"
    private const val GABO_TREE = "https://api.github.com/repos/Gabominated/PCSX2/git/trees/main?recursive=1"
    private const val GABO_DIR = "PCSX2%20Patches"
    @Volatile private var gaboTreeCache: List<String>? = null

    // Cheat codes — the PCSX2 DB hosts none, so we pull from community
    // collections. They use mixed naming (<CRC>.pnach and <SERIAL>_<CRC>.pnach,
    // some in subfolders), so we match against each repo's file tree by CRC
    // (exact) and fall back to serial (handles a dump whose CRC differs from the
    // one the cheat author used — common, and the patch-codes warning covers the
    // revision-mismatch risk). Ordered by coverage.
    private data class CheatSource(val raw: String, val tree: String)
    private val CHEAT_SOURCES = listOf(
        CheatSource(
            "https://raw.githubusercontent.com/shadowninja826/pcsx2_pnach_cheats/main",
            "https://api.github.com/repos/shadowninja826/pcsx2_pnach_cheats/git/trees/main?recursive=1",
        ),
        CheatSource(
            "https://raw.githubusercontent.com/xs1l3n7x/pcsx2_cheats_collection/main",
            "https://api.github.com/repos/xs1l3n7x/pcsx2_cheats_collection/git/trees/main?recursive=1",
        ),
    )
    private val cheatTreeCache = java.util.concurrent.ConcurrentHashMap<String, List<String>>()
    private val CRC_RE = Regex("^[0-9A-Fa-f]{8}$")
    private val SERIAL_RE = Regex("^[A-Z]{4}-\\d{5}$")
    private val TREE_PATH_RE = Regex("\"path\"\\s*:\\s*\"([^\"]+\\.pnach)\"")
    private val SECTION_RE = Regex("^\\s*\\[(.+?)]\\s*$")
    private val COMMENT_RE = Regex("^\\s*comment\\s*=\\s*(.+)$", RegexOption.IGNORE_CASE)
    private val GAMETITLE_RE = Regex("(?m)^\\s*gametitle\\s*=\\s*(.+)$")

    // Cached file listing of the whole patch repo (paths like
    // "patches/SLUS-20946_2C6BE434.pnach"). One API call per app session — the
    // raw .pnach fetches go through the CDN and don't count against the API
    // rate limit, so only this listing does.
    @Volatile private var treeCache: List<String>? = null

    /** A single toggleable patch/cheat. [body] is the verbatim `[Section]…`
     *  block (header + author/comment/patch lines) to write back out. [source]
     *  is "patches" or "cheats" — decides which folder it installs into. */
    data class Entry(
        val name: String,
        val description: String,
        val body: String,
        val source: String,
    )

    data class Result(
        val gametitle: String,
        val entries: List<Entry>,
        val error: String?,
        // Resolved game id for the matched files. [crc] comes from the DB
        // filename when browsing by serial (the disc isn't booted), so the
        // caller can name the saved .pnach so emucore loads it at boot.
        val serial: String = "",
        val crc: String = "",
    )

    /** Fetch + parse this game's patches (PCSX2 DB) AND cheats (community DB),
     *  using the exact serial+CRC known while a game is booted. [bundledZip] is the
     *  offline patch DB (resources/patches.zip) — read FIRST so patches resolve even when
     *  the booted CRC isn't a DB filename or the network is rate-limited; the network below
     *  only supplements. Without it, the in-game manager showed cheats but no patches. */
    fun fetchForGame(serial: String?, crc: String, bundledZip: File? = null): Result {
        val c = crc.trim().uppercase()
        if (!CRC_RE.matches(c))
            return Result("", emptyList(), "No game CRC yet — boot the game first.")

        var gametitle = ""
        val entries = mutableListOf<Entry>()

        // Patches from the BUNDLED DB first (offline, complete — every <SERIAL>_<CRC>.pnach),
        // deduped by name; the network lookups below only add anything fresher. This is why a
        // booted game whose exact CRC differs from the DB's filenames still gets its patches.
        if (!serial.isNullOrBlank()) {
            bundledPatchesForSerial(bundledZip, serial.uppercase()).let { (gt, es, _) ->
                if (es.isNotEmpty()) {
                    if (gametitle.isEmpty()) gametitle = gt
                    entries += es
                }
            }
        }

        // Patches (widescreen / fixes): prefer <serial>_<crc>, fall back to <crc>.
        val patchCandidates = buildList {
            if (!serial.isNullOrBlank()) add("${serial}_$c")
            add(c)
        }
        for (name in patchCandidates) {
            val text = get("$RAW_BASE/patches/$name.pnach") ?: continue
            val (gt, es) = parse(text, "patches")
            if (gametitle.isEmpty()) gametitle = gt
            val seen = entries.mapTo(HashSet()) { it.name }
            for (e in es) if (seen.add(e.name)) entries += e
            break // first existing patches file wins
        }

        // Also merge improvement patches from the Gabominated compilation (No-Blur etc.),
        // skipping any whose name already came from the official DB.
        for (name in patchCandidates) {
            val text = get("$GABO_BASE/$GABO_DIR/$name.pnach") ?: continue
            val (gt, es) = parse(text, "patches")
            if (gametitle.isEmpty()) gametitle = gt
            val seen = entries.mapTo(HashSet()) { it.name }
            for (e in es) if (seen.add(e.name)) entries += e
            break
        }

        // Cheats (community DBs).
        fetchCheats(serial, c)?.let { (gt, es) ->
            if (gametitle.isEmpty()) gametitle = gt
            entries += es
        }

        if (entries.isEmpty())
            return Result("", emptyList(), "No patches or cheats in the database for ${serial ?: c}.")
        return Result(gametitle, entries, null, serial?.uppercase().orEmpty(), c)
    }

    // Dedup key for cheats: case- and whitespace-insensitive. The serial fallback below can
    // match SEVERAL revision files for one game (GTA:SA has 3+), and the same cheat is often
    // spelled slightly differently across them ("Infinite Health" vs "infinite  health") — an
    // exact-name set let those through as near-duplicates, so the list "repeated forever".
    private val WS_RE = Regex("\\s+")
    private fun cheatKey(name: String) = name.trim().lowercase().replace(WS_RE, " ")

    /** Fetch + parse community cheats for a game across all sources. Matches
     *  each repo's tree by CRC (exact) first, then by serial as a fallback;
     *  dedupes entries by normalized name (earlier sources win). Null if nothing found. */
    private fun fetchCheats(serial: String?, crc: String): Pair<String, List<Entry>>? {
        val c = crc.uppercase()
        val s = serial?.uppercase()
        val haveCrc = CRC_RE.matches(c)
        if (!haveCrc && s == null) return null

        var gametitle = ""
        val entries = mutableListOf<Entry>()
        val seenNames = HashSet<String>()
        for (src in CHEAT_SOURCES) {
            val tree = cheatTree(src)
            if (tree.isEmpty()) continue
            var matches = if (haveCrc)
                tree.filter { it.substringAfterLast('/').uppercase().contains(c) }
            else emptyList()
            if (matches.isEmpty() && s != null)
                matches = tree.filter { it.substringAfterLast('/').uppercase().startsWith("${s}_") }
            for (m in matches) {
                val text = get("${src.raw}/${m.replace(" ", "%20")}") ?: continue
                val (gt, es) = parse(text, "cheats")
                if (gametitle.isEmpty()) gametitle = gt
                for (e in es) if (seenNames.add(cheatKey(e.name))) entries += e
            }
        }
        return if (entries.isEmpty()) null else gametitle to entries
    }

    /** Cached file listing for a cheat source. */
    private fun cheatTree(src: CheatSource): List<String> {
        cheatTreeCache[src.raw]?.let { return it }
        val json = get(src.tree) ?: return emptyList()
        val paths = TREE_PATH_RE.findAll(json).map { it.groupValues[1] }.toList()
        if (paths.isNotEmpty()) cheatTreeCache[src.raw] = paths
        return paths
    }

    /** Browse by serial only — for games picked from the library before being
     *  booted, where we have the serial but not the disc CRC. Looks the game up
     *  in the repo file tree to find its `<serial>_<crc>.pnach`; the CRC comes
     *  back in [Result.crc] so the caller can name the saved file correctly. */
    fun fetchForSerial(serial: String?, bundledZip: File? = null): Result {
        val s = serial?.trim()?.uppercase()
        if (s.isNullOrBlank() || !SERIAL_RE.matches(s))
            return Result("", emptyList(), "This game has no serial to search the patch database with.")

        var gametitle = ""
        var resolvedCrc = ""
        val entries = mutableListOf<Entry>()

        // Patches from the BUNDLED patch DB first (offline, complete). The per-game
        // settings view has no booted disc, so it can't take the by-CRC raw-URL path the
        // in-game view uses and instead listed the repo via the GitHub git-tree API — which
        // is rate-limited (60/hr unauth) AND truncates for a repo this size, so a specific
        // serial's patch often wasn't in the returned list. Result: cheats (smaller repos)
        // showed but patches didn't. The app already ships every `<SERIAL>_<CRC>.pnach` in
        // resources/patches.zip, so read the serial's patches straight from it — no network,
        // no rate limit, no truncation. The network below then only SUPPLEMENTS (fresher
        // patches / Gabominated / cheats).
        bundledPatchesForSerial(bundledZip, s).let { (gt, es, crc) ->
            if (es.isNotEmpty()) {
                if (gametitle.isEmpty()) gametitle = gt
                entries += es
                if (resolvedCrc.isEmpty()) resolvedCrc = crc
            }
        }

        val tree = repoTree()

        // Patches: find this serial's file in the PCSX2 tree; its filename also
        // gives us the CRC, which we then reuse to look up cheats. Supplements the
        // bundled patches above (dedup by name), so a truncated/rate-limited tree no
        // longer wipes the whole result.
        val match = tree.firstOrNull { it.startsWith("patches/${s}_", ignoreCase = true) }
        if (match != null) {
            get("$RAW_BASE/$match")?.let { text ->
                val (gt, es) = parse(text, "patches")
                if (gametitle.isEmpty()) gametitle = gt
                val seen = entries.mapTo(HashSet()) { it.name }
                for (e in es) if (seen.add(e.name)) entries += e
            }
            if (resolvedCrc.isEmpty())
                resolvedCrc = match.substringAfterLast('/')
                    .removeSuffix(".pnach")
                    .substringAfter("${s}_", "")
                    .substringBefore('_')
                    .uppercase()
        }

        // Gabominated improvement patches (matched by serial in its file tree).
        gaboTree().firstOrNull { it.substringAfterLast('/').startsWith("${s}_", ignoreCase = true) }
            ?.let { gm ->
                get("$GABO_BASE/${gm.replace(" ", "%20")}")?.let { text ->
                    val (gt, es) = parse(text, "patches")
                    if (gametitle.isEmpty()) gametitle = gt
                    val seen = entries.mapTo(HashSet()) { it.name }
                    for (e in es) if (seen.add(e.name)) entries += e
                }
                if (resolvedCrc.isEmpty())
                    resolvedCrc = gm.substringAfterLast('/').removeSuffix(".pnach")
                        .substringAfter("${s}_", "").substringBefore('_').uppercase()
            }

        // Cheats: matched by CRC (from the patches filename) or by serial.
        fetchCheats(s, resolvedCrc)?.let { (gt, es) ->
            if (gametitle.isEmpty()) gametitle = gt
            entries += es
        }

        if (entries.isEmpty())
            return Result("", emptyList(), "No patches or cheats in the database for $s.")
        return Result(gametitle, entries, null, s, resolvedCrc)
    }

    /** Patches for [serial] read straight from the bundled `resources/patches.zip`
     *  (entries named `<SERIAL>_<CRC>.pnach` at the zip root). Offline, complete, and
     *  immune to the GitHub API's rate limit and tree truncation. Returns
     *  (gametitle, entries, crc); empty when the zip is missing or has no match. */
    private fun bundledPatchesForSerial(zip: File?, serial: String): Triple<String, List<Entry>, String> {
        if (zip == null || !zip.isFile) return Triple("", emptyList(), "")
        return runCatching {
            java.util.zip.ZipFile(zip).use { zf ->
                var gametitle = ""
                var crc = ""
                val entries = mutableListOf<Entry>()
                val seen = HashSet<String>()
                val prefix = "${serial}_"
                for (e in zf.entries()) {
                    val base = e.name.substringAfterLast('/')
                    if (e.isDirectory || !base.startsWith(prefix, ignoreCase = true) ||
                        !base.endsWith(".pnach", ignoreCase = true)
                    ) continue
                    val text = zf.getInputStream(e).use { it.readBytes().toString(Charsets.UTF_8) }
                    val (gt, es) = parse(text, "patches")
                    if (gametitle.isEmpty()) gametitle = gt
                    for (entry in es) if (seen.add(entry.name)) entries += entry
                    if (crc.isEmpty())
                        crc = base.removeSuffix(".pnach").substringAfter(prefix, "").substringBefore('_').uppercase()
                }
                Triple(gametitle, entries, crc)
            }
        }.getOrElse {
            Log.w(TAG, "bundled patches read failed: ${it.message}")
            Triple("", emptyList(), "")
        }
    }

    /** File listing of the Gabominated patch fork, cached for the session. */
    private fun gaboTree(): List<String> {
        gaboTreeCache?.let { return it }
        val json = get(GABO_TREE) ?: return emptyList()
        val paths = TREE_PATH_RE.findAll(json).map { it.groupValues[1] }.toList()
        if (paths.isNotEmpty()) gaboTreeCache = paths
        return paths
    }

    /** File listing of the whole patch repo, cached for the session. */
    private fun repoTree(): List<String> {
        treeCache?.let { return it }
        val json = get(TREE_URL) ?: return emptyList()
        val paths = TREE_PATH_RE.findAll(json).map { it.groupValues[1] }.toList()
        if (paths.isNotEmpty()) treeCache = paths
        return paths
    }

    /** Build a `.pnach` (gametitle + the given entries' blocks) for writing. */
    fun buildPnach(gametitle: String, entries: List<Entry>): String = buildString {
        if (gametitle.isNotEmpty()) append("gametitle=").append(gametitle).append("\n\n")
        entries.forEach { append(it.body.trimEnd()).append("\n\n") }
    }

    private fun get(url: String): String? {
        val resp = runCatching { HttpClient.doRequest(url, "GET", null, USER_AGENT, 15000) }
            .getOrElse { Log.w(TAG, "get $url failed: ${it.message}"); return null }
        if (resp.statusCode != 200 || resp.data.isEmpty()) {
            if (resp.statusCode != 404)
                Log.w(TAG, "get $url: status=${resp.statusCode} size=${resp.data.size}")
            return null
        }
        return String(resp.data, Charsets.UTF_8)
    }

    private fun parse(pnach: String, source: String): Pair<String, List<Entry>> {
        val gametitle = GAMETITLE_RE.find(pnach)?.groupValues?.get(1)?.trim().orEmpty()
        val entries = mutableListOf<Entry>()
        var name: String? = null
        var desc = ""
        val body = StringBuilder()
        fun flush() {
            val n = name
            if (n != null) entries.add(Entry(n, desc, body.toString().trimEnd(), source))
            name = null; desc = ""; body.setLength(0)
        }
        for (line in pnach.lines()) {
            val h = SECTION_RE.find(line)
            if (h != null) {
                flush()
                name = h.groupValues[1].trim()
                body.append(line).append('\n')
            } else if (name != null) {
                body.append(line).append('\n')
                if (desc.isEmpty()) COMMENT_RE.find(line)?.let { desc = it.groupValues[1].trim() }
            }
        }
        flush()
        return gametitle to entries
    }

    // ---- Installed-file editing (per-cheat on/off) ----

    /** A cheat/patch parsed from an on-disk `.pnach`, preserving its current
     *  enabled state so the installed-file editor can round-trip individual
     *  toggles. [enabled] is true when at least one of the block's `patch=`
     *  lines is active (not `//`-commented). [body] is the verbatim block as
     *  found on disk (header + patch lines) so it can be rewritten faithfully. */
    data class LocalCheat(
        val name: String,
        val description: String,
        val enabled: Boolean,
        val body: String,
    )

    // `/{0,2}` = an OPTIONAL leading // (0, 1 or 2 slashes). The old `//?` required
    // at least one slash, so ACTIVE `patch=` lines never matched — the per-cheat
    // editor/browser then saw zero cheats in any normal file, and toggling a cheat
    // off was silently ignored. Must match both active and //-commented patch lines.
    private val PATCH_LINE_RE = Regex("^\\s*/{0,2}\\s*patch\\s*=", RegexOption.IGNORE_CASE)
    private val META_COMMENT_RE = Regex("^[A-Z]{4}-\\d{5}\\s+[0-9A-Fa-f]{8}$")

    /** True if [line] is a `patch=` command (whether active or `//`-commented). */
    fun isPatchCommand(line: String): Boolean = PATCH_LINE_RE.containsMatchIn(line)

    /**
     * Parse an installed `.pnach` into individual cheats for the per-cheat editor.
     * Handles BOTH conventions that show up on disk:
     *   • PCSX2 database `[Section]` blocks, and
     *   • community / ARMSX2-flattened `//Comment` headers followed by `patch=` lines.
     * A cheat starts at a `[Section]` line, or at a plain `//comment` line that
     * follows a completed block. A cheat is [LocalCheat.enabled] when any of its
     * `patch=` lines is active. Blocks with no `patch=` lines (bare headers,
     * gametitle) are dropped. This is the inverse of the editor's rebuild.
     */
    fun parseInstalled(pnach: String, source: String): Pair<String, List<LocalCheat>> {
        val gametitle = GAMETITLE_RE.find(pnach)?.groupValues?.get(1)?.trim().orEmpty()
        val cheats = mutableListOf<LocalCheat>()
        var name: String? = null
        var desc = ""
        val body = StringBuilder()
        var hasPatch = false
        var hasActive = false
        fun flush() {
            val n = name
            if (n != null && hasPatch) cheats.add(LocalCheat(n, desc, hasActive, body.toString().trimEnd()))
            name = null; desc = ""; body.setLength(0); hasPatch = false; hasActive = false
        }
        for (raw in pnach.lines()) {
            val line = raw.trimEnd()
            val trimmed = line.trim()
            if (trimmed.isEmpty()) { if (name != null) body.append('\n'); continue }
            // Skip file-level metadata (gametitle + ARMSX2 import markers).
            if (trimmed.startsWith("gametitle", ignoreCase = true)) continue
            val label = SECTION_RE.find(line)?.groupValues?.get(1)?.trim()
            val isPatch = isPatchCommand(trimmed)
            val commented = trimmed.startsWith("//")
            when {
                label != null -> { flush(); name = label; body.append(line).append('\n') }
                isPatch -> {
                    if (name == null) name = "Unlabelled"
                    hasPatch = true
                    if (!commented) hasActive = true
                    body.append(line).append('\n')
                }
                commented -> {
                    val text = trimmed.trimStart('/').trim()
                    if (text.equals("ARMSX2 manual PNACH", ignoreCase = true) || META_COMMENT_RE.matches(text))
                        continue // our own import markers — not a cheat header
                    // A comment after a completed block starts the next cheat.
                    if (hasPatch) flush()
                    if (name == null) {
                        if (text.isNotEmpty()) { name = text; body.append(line).append('\n') }
                    } else {
                        if (desc.isEmpty()) desc = text
                        body.append(line).append('\n')
                    }
                }
                else -> { if (name != null) body.append(line).append('\n') } // author=, description=, …
            }
        }
        flush()
        return gametitle to cheats
    }
}
