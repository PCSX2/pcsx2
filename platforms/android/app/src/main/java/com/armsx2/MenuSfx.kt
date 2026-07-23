package com.armsx2

import android.content.Context
import android.media.AudioAttributes
import android.media.SoundPool
import android.net.Uri
import android.os.SystemClock
import android.util.Log
import androidx.compose.runtime.mutableStateOf
import androidx.core.content.edit
import androidx.documentfile.provider.DocumentFile
import com.armsx2.runtime.MainActivityRuntime
import java.io.File

/**
 * UI sound effects for the library/menu — the "console dashboard" blips (select, back, toggle…).
 *
 * Ships a bundled default set: short interface blips trimmed from obsydianx's "Interface SFX Pack
 * 1", released CC0 / public domain (https://obsydianx.itch.io/interface-sfx-pack-1). CC0 imposes no
 * attribution requirement, but we credit it in the About screen anyway.
 *
 * Each event can be OVERRIDDEN by importing a folder of named clips (see [Event.fileName]): a user
 * file wins over the built-in for that event, everything else keeps the default. Imported files are
 * copied into app-private storage (no persisted SAF grant needed) — nothing is redistributed, same
 * model as importing a texture pack, a skin, or a music track.
 *
 * Playback is a SoundPool for near-zero-latency, overlap-friendly one-shots, kept OFF the emulator's
 * Oboe/SPU2 path (its own USAGE_GAME attributes) so it never contends with SPU2 output (cf. the
 * LibraryMusic note on Android reclaiming Oboe, #333).
 */
object MenuSfx {
    private const val TAG = "MenuSfx"
    private const val EnabledKey = "ui.menuSfx"
    private const val VolumeKey = "ui.menuSfx.volume"
    private const val PackNameKey = "ui.menuSfx.packName"
    // 15% by default — these fire on every interaction, so they should sit well under the UI (same
    // reasoning as LibraryMusic's default). Applies to fresh installs; existing users keep their set value.
    private const val DefaultVolumePercent = 15

    /** UI events a sound binds to. [fileName] is the base name to use when importing a custom pack
     *  (case- and extension-insensitive); [defaultRes] is the bundled fallback clip. */
    enum class Event(val fileName: String, val defaultRes: Int) {
        NAV("nav", R.raw.sfx_cursor),            // controller moves the selection highlight
        SELECT("select", R.raw.sfx_select),      // confirm / launch a game
        SUBMENU("submenu", R.raw.sfx_submenu),   // open a settings menu / sub-screen
        MENU_OPEN("menu", R.raw.sfx_menu),       // open the in-game pause menu
        BACK("back", R.raw.sfx_back),
        TOGGLE_ON("toggle_on", R.raw.sfx_toggle_on),
        TOGGLE_OFF("toggle_off", R.raw.sfx_toggle_off),
        RESET("reset", R.raw.sfx_reset),
        SLIDER("slider", R.raw.sfx_cursor),      // shares the cursor tick with NAV
        SLEEP("sleep", R.raw.sfx_sleep),         // DS-lid-style chime when the device sleeps
        WAKE("wake", R.raw.sfx_wake),            // chime when waking back to the app
    }

    /** On by default — the bundled sounds give the launcher its "personality" out of the box. */
    val enabled = mutableStateOf(true)
    val volumePercent = mutableStateOf(DefaultVolumePercent)
    /** Name of an imported custom pack folder, or null when using the bundled defaults. */
    val packName = mutableStateOf<String?>(null)

    private fun gain(): Float = (volumePercent.value.coerceIn(0, 100)) / 100f

    private var pool: SoundPool? = null
    /** event -> loaded SoundPool sample id. */
    private val sampleIds = HashMap<Event, Int>()
    /** Per-event last-play time, to throttle rapid emitters (a slider drag fires onChange many
     *  times/second — without this it buzzes instead of ticking). */
    private val lastPlayMs = HashMap<Event, Long>()
    /** Uptime of the last sound that ACTUALLY played (post-throttle). Lets a generic dispatch like
     *  SettingsControllerNav.confirm() tell whether the action it invoked already made its own
     *  sound, so it only adds a select blip when the action was silent. */
    @Volatile private var lastPlayedMs = 0L
    fun lastPlayedAt(): Long = lastPlayedMs

    /** App-private dir any imported clips live in — copied here so playback needs no persisted SAF
     *  grant and survives the source moving. Stored extension-less; SoundPool sniffs the format. */
    private fun sfxDir(context: Context): File =
        File(context.filesDir, "menusfx").apply { mkdirs() }

    private fun clipFile(context: Context, event: Event): File = File(sfxDir(context), event.fileName)

    fun load(context: Context) {
        enabled.value = MainActivityRuntime.prefs.getBoolean(EnabledKey, true)
        volumePercent.value = MainActivityRuntime.prefs.getInt(VolumeKey, DefaultVolumePercent)
        packName.value = MainActivityRuntime.prefs.getString(PackNameKey, null)
        if (enabled.value) rebuildPool(context)
    }

    fun set(context: Context, value: Boolean) {
        enabled.value = value
        MainActivityRuntime.prefs.edit { putBoolean(EnabledKey, value) }
        if (value) rebuildPool(context) else releasePool()
    }

    fun setVolume(percent: Int) {
        val p = percent.coerceIn(0, 100)
        volumePercent.value = p
        MainActivityRuntime.prefs.edit { putInt(VolumeKey, p) }
    }

    /** Which events are currently served by an imported custom clip (vs the bundled default) —
     *  drives the per-event status shown in settings. */
    fun customEvents(context: Context): Set<Event> =
        Event.entries.filter { clipFile(context, it).length() > 0L }.toSet()

    /**
     * Import a user-picked folder: copy every file whose base name matches an [Event] into
     * app-private storage (overriding that event's bundled default), then rebuild the SoundPool.
     * Returns how many clips were imported.
     */
    fun importFromTree(context: Context, treeUri: Uri): Int {
        val tree = DocumentFile.fromTreeUri(context, treeUri) ?: return 0
        var copied = 0
        val dir = sfxDir(context)
        tree.listFiles().forEach { doc ->
            val event = doc.name?.let(::eventForFilename) ?: return@forEach
            val ok = runCatching {
                context.contentResolver.openInputStream(doc.uri)?.use { ins ->
                    File(dir, event.fileName).outputStream().use { ins.copyTo(it) }
                } != null
            }.getOrDefault(false)
            if (ok) copied++
        }
        if (copied > 0) {
            val name = tree.name ?: "Sound pack"
            packName.value = name
            MainActivityRuntime.prefs.edit { putString(PackNameKey, name) }
            rebuildPool(context)
        }
        return copied
    }

    /** Drop the imported pack and go back to the bundled defaults. */
    fun clear(context: Context) {
        Event.entries.forEach { runCatching { clipFile(context, it).delete() } }
        packName.value = null
        MainActivityRuntime.prefs.edit { remove(PackNameKey) }
        if (enabled.value) rebuildPool(context) else releasePool()
    }

    /** Fire an event's sound, if enabled. Cheap no-op otherwise — safe to call from any UI
     *  callback. Throttled per event so a fast slider drag ticks rather than buzzes. */
    fun play(event: Event) {
        if (!enabled.value) return
        val p = pool ?: return
        val id = sampleIds[event] ?: return
        val now = SystemClock.uptimeMillis()
        // NAV and SLIDER fire on every highlight move / value step — a longer floor keeps a fast
        // D-pad walk or drag ticking pleasantly instead of buzzing.
        val minGap = if (event == Event.SLIDER || event == Event.NAV) 45L else 18L
        if (now - (lastPlayMs[event] ?: 0L) < minGap) return
        lastPlayMs[event] = now
        lastPlayedMs = now
        val g = gain()
        runCatching { p.play(id, g, g, 1, 0, 1f) }
    }

    /** Map an incoming filename to an event by its base name (case/extension-insensitive). */
    private fun eventForFilename(name: String): Event? {
        val base = name.substringBeforeLast('.').lowercase().trim()
        return Event.entries.firstOrNull { it.fileName == base }
    }

    private fun rebuildPool(context: Context) {
        releasePool()
        val sp = SoundPool.Builder()
            .setMaxStreams(6)
            .setAudioAttributes(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                    .build()
            )
            .build()
        Event.entries.forEach { ev ->
            runCatching {
                val custom = clipFile(context, ev)
                val id = if (custom.length() > 0L) sp.load(custom.absolutePath, 1)
                         else sp.load(context, ev.defaultRes, 1)
                if (id != 0) sampleIds[ev] = id
            }.onFailure { Log.w(TAG, "load failed for ${ev.fileName}", it) }
        }
        pool = sp
    }

    private fun releasePool() {
        sampleIds.clear()
        lastPlayMs.clear()
        pool?.let { runCatching { it.release() } }
        pool = null
    }
}
