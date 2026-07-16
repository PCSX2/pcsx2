package com.armsx2.config

import com.armsx2.runtime.MainActivityRuntime
import org.json.JSONObject
import androidx.core.content.edit
import java.io.File

/**
 * Persistence + resolution for emu [Settings].
 *
 * Two storage tiers, both in `MainActivityRuntime.prefs`:
 *   - **Global** under `config.global` — the user's baseline. Stored as a
 *     full Settings JSON.
 *   - **Per-game** under `config.game.<serial>` — sparse JSON containing
 *     ONLY the fields the user explicitly overrode for that title. Sparse
 *     storage means future global tweaks still flow through fields the
 *     user hasn't touched per-game.
 *
 * [resolveForGame] is the only method anyone outside the settings UI
 * should call. It returns the merged Settings to push at VM launch.
 *
 * No caching — load on demand. Writes are rare (user clicked Save) and
 * reads happen at game launch (once per launch). Both well within
 * SharedPreferences' overhead.
 */
/**
 * Where overlay setting changes land. The overlay header picks one at
 * runtime via a switch; default is [Game] when a game is loaded,
 * [Global] otherwise.
 *
 *   - [Global] writes the full Settings to `config.global`. Affects every
 *     future game launch that doesn't have its own per-game override.
 *   - [Game] writes the SPARSE diff (only fields differing from current
 *     global) to `config.game.<serial>`. Global stays untouched; only
 *     this title sees the change.
 */
enum class SettingsScope { Global, Game }

object ConfigStore {
    private const val KEY_GLOBAL = "config.global"
    private const val KEY_BLEND_BASIC_MIGRATED = "config.migrated.blendBasic"
    // One-time seed of the (now per-game) renderer/upscale fields from the legacy
    // global prefs, so updating doesn't reset everyone's backend/resolution.
    private const val KEY_RENDERER_MIGRATED = "config.migrated.rendererUpscale"
    // One-time flip of existing saves to the new Adreno framebuffer-fetch default-on.
    private const val KEY_ADRENO_FBFETCH_MIGRATED = "config.migrated.adrenoFbFetchOn"
    // One-time flip of existing all-on OSD saves to the new default-off.
    private const val KEY_OSD_OFF_MIGRATED = "config.migrated.osdDefaultOff"
    // One-time reconcile for the fresh-install + reused-data-folder case (people who
    // can't update in place and re-point setup at their old folder). See reconcileReusedFolder.
    private const val KEY_FOLDER_RECONCILE = "config.migrated.folderReconcile"
    // Mirror of the settings, written INTO the data folder so a later fresh install that
    // reuses the same folder can restore them (SharedPreferences don't survive uninstall).
    private const val BACKUP_FILENAME = "armsx2-settings.json"
    private fun keyForGame(serial: String) = "config.game.$serial"

    fun loadGlobal(): Settings {
        val raw = MainActivityRuntime.prefs.getString(KEY_GLOBAL, null)
        var parsed = if (raw != null) {
            try { Settings.fromJson(JSONObject(raw)) } catch (_: Exception) { Settings() }
        } else {
            Settings()
        }
        var dirty = false

        // Legacy: "Basic" blending migration.
        if (raw != null && !MainActivityRuntime.prefs.getBoolean(KEY_BLEND_BASIC_MIGRATED, false) &&
            parsed.accurateBlendingUnit == 4) {
            parsed = parsed.copy(accurateBlendingUnit = 1)
            dirty = true
        }
        if (!MainActivityRuntime.prefs.getBoolean(KEY_BLEND_BASIC_MIGRATED, false)) {
            MainActivityRuntime.prefs.edit { putBoolean(KEY_BLEND_BASIC_MIGRATED, true) }
        }

        // Seed renderer/upscale from the legacy global prefs (where they used to
        // live) into the Settings tier, once. After this they're scope-aware like
        // every other setting; the old prefs become vestigial.
        if (!MainActivityRuntime.prefs.getBoolean(KEY_RENDERER_MIGRATED, false)) {
            MainActivityRuntime.prefs.getString("renderer", null)?.takeIf { it.isNotBlank() }?.let {
                parsed = parsed.copy(renderer = it)
                dirty = true
            }
            legacyUpscalePref()?.let {
                parsed = parsed.copy(upscaleFloat = it)
                dirty = true
            }
            MainActivityRuntime.prefs.edit { putBoolean(KEY_RENDERER_MIGRATED, true) }
        }

        // Adreno framebuffer-fetch is now default-on. Flip existing global saves that
        // still carry the old default-off ONCE, so updating users get the fast
        // accurate-blending path too (they can turn it back off in the Renderer tab).
        if (raw != null && !MainActivityRuntime.prefs.getBoolean(KEY_ADRENO_FBFETCH_MIGRATED, false) &&
            !parsed.adrenoFbFetch) {
            parsed = parsed.copy(adrenoFbFetch = true)
            dirty = true
        }
        if (!MainActivityRuntime.prefs.getBoolean(KEY_ADRENO_FBFETCH_MIGRATED, false)) {
            MainActivityRuntime.prefs.edit { putBoolean(KEY_ADRENO_FBFETCH_MIGRATED, true) }
        }

        // The perf OSD (FPS/stats counters) now defaults OFF — it read as clutter.
        // Flip existing saves that still sit at the old all-on default ONCE; a user
        // who'd already turned any element off is left untouched, and the in-game
        // "OSD" toggle re-enables everything. (The bottom-left/right summaries are
        // handled by their own absent-key default for pre-2.6 saves.)
        if (raw != null && !MainActivityRuntime.prefs.getBoolean(KEY_OSD_OFF_MIGRATED, false) &&
            parsed.osdShowFps && parsed.osdShowVps && parsed.osdShowSpeed &&
            parsed.osdShowCpu && parsed.osdShowGpu && parsed.osdShowResolution &&
            parsed.osdShowGsStats && parsed.osdShowFrameTimes &&
            parsed.osdShowHardwareInfo && parsed.osdShowVersion) {
            parsed = parsed.copy(
                osdShowFps = false, osdShowVps = false, osdShowSpeed = false,
                osdShowCpu = false, osdShowGpu = false, osdShowResolution = false,
                osdShowGsStats = false, osdShowFrameTimes = false,
                osdShowHardwareInfo = false, osdShowVersion = false,
            )
            dirty = true
        }
        if (!MainActivityRuntime.prefs.getBoolean(KEY_OSD_OFF_MIGRATED, false)) {
            MainActivityRuntime.prefs.edit { putBoolean(KEY_OSD_OFF_MIGRATED, true) }
        }

        if (dirty) saveGlobal(parsed)
        return parsed
    }

    /** Read the legacy global upscale pref (float, or older int/string), or null. */
    private fun legacyUpscalePref(): Float? {
        val all = MainActivityRuntime.prefs.all
        fun coerce(raw: Any?): Float? = when (raw) {
            is Float -> raw
            is Double -> raw.toFloat()
            is Int -> raw.toFloat()
            is Long -> raw.toFloat()
            is String -> raw.toFloatOrNull()
            else -> null
        }?.coerceIn(0.25f, 8.0f)
        return coerce(all["upscaleFloat"]) ?: coerce(all["upscale"])
    }

    fun saveGlobal(s: Settings) {
        MainActivityRuntime.prefs.edit { putString(KEY_GLOBAL, s.toJson().toString()) }
        writeBackupMirror()
    }

    /** Load the sparse per-game override blob, or null if there are none. */
    fun loadOverrides(serial: String): JSONObject? {
        val raw = MainActivityRuntime.prefs.getString(keyForGame(serial), null) ?: return null
        return try {
            JSONObject(raw)
        } catch (_: Exception) {
            null
        }
    }

    fun saveOverrides(serial: String, overrides: JSONObject) {
        MainActivityRuntime.prefs.edit { putString(keyForGame(serial), overrides.toString()) }
        writeBackupMirror()
    }

    fun clearOverrides(serial: String) {
        MainActivityRuntime.prefs.edit { remove(keyForGame(serial)) }
        writeBackupMirror()
    }

    /**
     * Resolve effective Settings for a VM launch:
     *   per-game override (if present) ∘ global ∘ defaults.
     *
     * Pass null serial to skip the per-game tier (BIOS boots, anonymous
     * launches via Swap/Boot Disc when no GameInfo was carried through).
     */
    fun resolveForGame(serial: String?): Settings {
        val global = loadGlobal()
        if (serial == null) return global
        val overrides = loadOverrides(serial) ?: return global
        return Settings.merge(global, overrides)
    }

    /**
     * Single entry point for overlay tabs to persist a settings change.
     * Scope picks the storage tier; serial may be null (Global is the
     * only valid scope in that case).
     *
     * Game scope stores a SPARSE override: a field the user never touched for this title
     * is absent, so a later global tweak still reaches it. That inheritance is the point
     * and is unchanged.
     *
     * What IS new: an override is STICKY once it exists. The old rule — store exactly the
     * fields that differ from global right now — could not tell "the user set this for
     * this game, and it happens to match global" from "the user never touched it", so it
     * stored nothing in both cases. Set Cheats on for a game while global also had them
     * on, later turn global off, and the game silently lost its setting; that's the
     * reported "global overwrites per-game and vice versa". Now a field is pinned to the
     * game once it's overridden — by differing from global, by the user changing it here
     * ([previous]), or by already being pinned — and only [clearOverrides] unpins it.
     */
    fun save(scope: SettingsScope, serial: String?, updated: Settings, previous: Settings? = null) {
        if (scope == SettingsScope.Game && serial != null) {
            val global = loadGlobal()
            val overrides = Settings.diff(global, updated)
            // Every field, so a pinned key can be given its CURRENT value even when that
            // value equals global's (the diff above necessarily omits it).
            val full = updated.toJson()
            val pinned = LinkedHashSet<String>()
            loadOverrides(serial)?.keys()?.forEach { pinned.add(it) }
            // What the user just changed, pinned even if it landed on global's value —
            // otherwise editing a field in Game scope could silently un-pin it.
            previous?.let { Settings.diff(it, updated).keys().forEach { k -> pinned.add(k) } }
            pinned.forEach { key ->
                if (!overrides.has(key) && full.has(key)) overrides.put(key, full.get(key))
            }
            saveOverrides(serial, overrides)
        } else {
            saveGlobal(updated)
        }
    }

    // ---- Fresh-install + reused-data-folder recovery (#9) ----
    //
    // SharedPreferences (where config.global lives) are WIPED when the app is
    // uninstalled. People who can't update in place — different signing key between the
    // old-UI and new-UI builds — must uninstall+reinstall, then re-point setup at their
    // existing data folder to keep their games/BIOS/saves. That folder still holds their
    // old native config (PCSX2-Android.ini) + per-game gamesettings/*.ini, so the core
    // applies old settings while the new UI shows defaults and then clobbers them.
    //
    // Fix: mirror settings INTO the folder on every save, and on first run — when there
    // is NO config.global in prefs — restore from that mirror (lossless) or, failing that,
    // seed from the old PCSX2-Android.ini (best-effort). The `config.global == null` guard
    // means anyone ALREADY on the new UI is never touched: with settings present there is
    // nothing to recover, so this can only ADD when there are none, never overwrite.

    private fun backupFile(): File? {
        val root = MainActivityRuntime.currentInitDataRoot()?.takeIf { it.isNotBlank() } ?: return null
        return File(root, BACKUP_FILENAME)
    }

    /** Write the in-folder settings mirror (global + every per-game blob). Cheap; called
     *  on each save. Silently no-ops until the data root is known. */
    private fun writeBackupMirror() {
        val file = backupFile() ?: return
        runCatching {
            val root = JSONObject()
            MainActivityRuntime.prefs.getString(KEY_GLOBAL, null)?.let { root.put("global", JSONObject(it)) }
            val games = JSONObject()
            for ((k, v) in MainActivityRuntime.prefs.all) {
                if (k.startsWith("config.game.") && v is String) {
                    runCatching { games.put(k.removePrefix("config.game."), JSONObject(v)) }
                }
            }
            if (games.length() > 0) root.put("games", games)
            file.parentFile?.mkdirs()
            file.writeText(root.toString())
        }
    }

    /** One-time, guarded recovery for the fresh-install + reused-folder case. Call once at
     *  app init (after the data root is resolved). Ordered: (1) restore losslessly from the
     *  in-folder mirror a prior new-UI install left; (2) else seed config.global from the
     *  folder's PCSX2-Android.ini. NEVER runs when config.global already exists. */
    fun reconcileReusedFolder() {
        if (MainActivityRuntime.prefs.getBoolean(KEY_FOLDER_RECONCILE, false)) return
        MainActivityRuntime.prefs.edit { putBoolean(KEY_FOLDER_RECONCILE, true) }
        // Hard guard: an existing new-UI user (has config.global) is off-limits.
        if (MainActivityRuntime.prefs.getString(KEY_GLOBAL, null) != null) return

        // (1) Lossless restore from the in-folder mirror (written by a prior new-UI install).
        val mirror = backupFile()
        if (mirror != null && mirror.exists() && mirror.length() > 0L) {
            val restored = runCatching {
                val root = JSONObject(mirror.readText())
                root.optJSONObject("global")?.let { g ->
                    MainActivityRuntime.prefs.edit { putString(KEY_GLOBAL, g.toString()) }
                }
                root.optJSONObject("games")?.let { games ->
                    val it = games.keys()
                    while (it.hasNext()) {
                        val serial = it.next()
                        games.optJSONObject(serial)?.let { g ->
                            MainActivityRuntime.prefs.edit { putString(keyForGame(serial), g.toString()) }
                        }
                    }
                }
                MainActivityRuntime.prefs.getString(KEY_GLOBAL, null) != null
            }.getOrDefault(false)
            if (restored) return
        }

        // (2) Best-effort seed from the folder's old native PCSX2-Android.ini (old-UI case).
        val root = MainActivityRuntime.currentInitDataRoot()?.takeIf { it.isNotBlank() } ?: return
        val ini = File(root, "PCSX2-Android.ini")
        if (!ini.exists() || ini.length() == 0L) return
        runCatching {
            val map = parseIni(ini.readText())
            if (map.isNotEmpty()) saveGlobal(Settings().readFromIni(map))
        }
    }

    /** Minimal INI reader: "[Section]" + "Key = Value" -> map keyed "Section/Key". Comments
     *  (# / ;) and blank lines ignored. Section text is taken verbatim (it can itself contain
     *  slashes, e.g. "EmuCore/GS"), matching the (section,key) applyTo/readFromIni use. */
    private fun parseIni(text: String): Map<String, String> {
        val map = HashMap<String, String>()
        var section = ""
        for (raw in text.lineSequence()) {
            val line = raw.trim()
            if (line.isEmpty() || line.startsWith("#") || line.startsWith(";")) continue
            if (line.startsWith("[") && line.endsWith("]")) {
                section = line.substring(1, line.length - 1).trim()
                continue
            }
            val eq = line.indexOf('=')
            if (eq <= 0) continue
            val key = line.substring(0, eq).trim()
            val value = line.substring(eq + 1).trim()
            if (key.isNotEmpty()) map["$section/$key"] = value
        }
        return map
    }
}
