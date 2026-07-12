package com.armsx2.config

import com.armsx2.runtime.MainActivityRuntime
import org.json.JSONObject
import androidx.core.content.edit

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
    }

    fun clearOverrides(serial: String) {
        MainActivityRuntime.prefs.edit { remove(keyForGame(serial)) }
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
     * Game scope writes the sparse diff vs current global, so a later
     * global tweak still propagates to the field unless the user
     * explicitly overrode it for this title.
     */
    fun save(scope: SettingsScope, serial: String?, updated: Settings) {
        if (scope == SettingsScope.Game && serial != null) {
            val global = loadGlobal()
            val diff = Settings.diff(global, updated)
            saveOverrides(serial, diff)
        } else {
            saveGlobal(updated)
        }
    }
}
