package com.armsx2.ui.achievements

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.runtime.MainActivityRuntime
import kr.co.iefriends.pcsx2.NativeApp
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject

data class AchievementItem(
    val id: Int,
    val title: String,
    val description: String,
    val points: Int,
    val unlocked: Boolean,
    val progress: String,
    val iconUrl: String,
    // Which RA subset this achievement belongs to (0 = base/shared set). Used to split
    // base-game vs bonus-subset achievements into tabs.
    val subsetId: Int = 0,
    // ACTIVE CHALLENGE (RA "primed") — a can-do-right-now challenge; sorted to the top.
    val primed: Boolean = false,
)

/** An RA subset (base set or a bonus subset) for the achievements tab selector. */
data class Subset(
    val id: Int,
    val title: String,
    val numAchievements: Int,
)

data class AchievementsUiState(
    val loggedIn: Boolean = false,
    val userName: String = "",
    val hardcore: Boolean = false,
    val score: Long = 0,
    val items: List<AchievementItem> = emptyList(),
    // RA subsets (base + any bonus subsets). >1 entry → the UI shows subset tabs.
    val subsets: List<Subset> = emptyList(),
    val richPresence: String = "",
    // Presentation options (mirrored from getAchievementsJSON, set via setAchievementsOption).
    val notifications: Boolean = true,
    val leaderboardNotifications: Boolean = true,
    val overlays: Boolean = true,
    val lbOverlays: Boolean = true,
    val soundEffects: Boolean = true,
    // Achievement modes (default off). Encore = re-notify already-unlocked achievements;
    // Spectator = treat all as locked, send nothing to the server; Unofficial = list
    // unpromoted test sets (unlocks aren't saved). Native rc_client already supports them.
    val encoreMode: Boolean = false,
    val spectatorMode: Boolean = false,
    val unofficialTestMode: Boolean = false,
    // Display name of the user's custom achievement-unlock sound, or null for the default.
    val unlockSoundName: String? = null,
    // Volume of the unlock sound effect, 0..100 % (app-side, applied in NativeApp.playSound).
    val soundVolume: Int = 100,
    // Non-null while the hardcore confirm dialog is up; holds the target state.
    val pendingHardcore: Boolean? = null,
    val loading: Boolean = false,
    val error: String? = null,
)

class AchievementsViewModel(application: Application) : AndroidViewModel(application) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    var state = androidx.compose.runtime.mutableStateOf(AchievementsUiState())
        private set

    fun refresh() {
        val snapshot = runCatching { parse(NativeApp.getAchievementsJSON().orEmpty()) }.getOrNull()
        if (snapshot != null) state.value = snapshot.copy(
            richPresence = runCatching { NativeApp.getRichPresence() }.getOrDefault(""),
            // Preserve the transient confirm-dialog state across the 3s poll.
            pendingHardcore = state.value.pendingHardcore,
        )
    }

    fun login(userName: String, password: String) {
        if (userName.isBlank() || password.isBlank()) {
            state.value = state.value.copy(error = "Enter your RetroAchievements username and password.")
            return
        }
        scope.launch {
            state.value = state.value.copy(loading = true, error = null)
            val error = withContext(Dispatchers.IO) { NativeApp.loginAchievements(userName.trim(), password) }
            if (error.isNullOrBlank()) {
                refresh()
                state.value = state.value.copy(loading = false)
            } else {
                state.value = state.value.copy(loading = false, error = error)
            }
        }
    }

    fun logout() {
        NativeApp.logoutAchievements()
        state.value = AchievementsUiState()
    }

    // Hardcore toggle is gated behind a confirm dialog (both directions): enabling
    // resets the running game and disables save states/cheats; disabling drops you
    // to casual so new unlocks stop counting for hardcore.
    fun requestToggleHardcore() {
        state.value = state.value.copy(pendingHardcore = !state.value.hardcore)
    }

    fun confirmToggleHardcore() {
        val target = state.value.pendingHardcore ?: return
        NativeApp.setHardcoreMode(target)
        state.value = state.value.copy(hardcore = target, pendingHardcore = null)
        // Enabling hardcore only takes hold on a system reset. Reboot the game now — so
        // "Enable & restart" actually restarts — but ONLY when a game is actually running.
        // This screen is the GLOBAL (library) RA tab too, where there's no VM; restart()
        // would fall into start() and boot the BIOS. With no game, the persisted
        // ChallengeMode simply engages on the next game boot (what isHardcorePersisted
        // drives). Disabling needs no reboot (casual mode applies live).
        if (target && com.armsx2.runtime.MainActivityRuntime.eState.value != com.armsx2.EmuState.STOPPED) {
            com.armsx2.runtime.MainActivityRuntime.restart()
            com.armsx2.ui.WindowImpl.dismissInGameScreen()
        }
        refresh()
    }

    fun cancelToggleHardcore() {
        state.value = state.value.copy(pendingHardcore = null)
    }

    fun setOption(key: String, enabled: Boolean) {
        NativeApp.setAchievementsOption(key, enabled)
        state.value = when (key) {
            "notifications" -> state.value.copy(notifications = enabled)
            "leaderboardNotifications" -> state.value.copy(leaderboardNotifications = enabled)
            "overlays" -> state.value.copy(overlays = enabled)
            "lbOverlays" -> state.value.copy(lbOverlays = enabled)
            "soundEffects" -> state.value.copy(soundEffects = enabled)
            "encoreMode" -> state.value.copy(encoreMode = enabled)
            "spectatorMode" -> state.value.copy(spectatorMode = enabled)
            "unofficialTestMode" -> state.value.copy(unofficialTestMode = enabled)
            else -> state.value
        }
    }

    fun dismissError() {
        state.value = state.value.copy(error = null)
    }

    private fun parse(json: String): AchievementsUiState {
        if (json.isBlank()) return state.value
        val root = JSONObject(json)
        return AchievementsUiState(
            loggedIn = root.optBoolean("loggedIn"),
            userName = root.optString("userName"),
            // Reflect the PERSISTED ChallengeMode (what takes effect on the next boot), not
            // the live rcheevos flag from the JSON — that's always off with no game running,
            // which would make the library RA tab's Hardcore toggle snap back off after you
            // enable it. isHardcorePersisted() is valid with or without a running game.
            hardcore = runCatching { NativeApp.isHardcorePersisted() }.getOrDefault(root.optBoolean("hardcore")),
            score = root.optLong("score").coerceAtLeast(0),
            items = parseAchievementItems(json),
            subsets = parseSubsets(json),
            notifications = root.optBoolean("notifications", true),
            leaderboardNotifications = root.optBoolean("leaderboardNotifications", true),
            overlays = root.optBoolean("overlays", true),
            lbOverlays = root.optBoolean("lbOverlays", true),
            soundEffects = root.optBoolean("soundEffects", true),
            encoreMode = root.optBoolean("encoreMode", false),
            spectatorMode = root.optBoolean("spectatorMode", false),
            unofficialTestMode = root.optBoolean("unofficialTestMode", false),
            unlockSoundName = MainActivityRuntime.prefs.getString(UNLOCK_SOUND_PREF, null),
            soundVolume = MainActivityRuntime.prefs.getInt(SOUND_VOLUME_PREF, 100),
        )
    }

    /** Copy the picked audio file into app-private storage and point the native
     *  [Achievements] UnlockSoundName at it, so achievement unlocks play it. */
    fun setUnlockSound(uri: android.net.Uri) {
        val context = getApplication<Application>()
        scope.launch {
            val name = withContext(Dispatchers.IO) {
                runCatching {
                    val dir = java.io.File(context.filesDir, "achievements").apply { mkdirs() }
                    // Drop any prior custom sound so a new extension can't leave a stale file behind.
                    dir.listFiles()?.forEach { it.delete() }
                    val display = queryDisplayName(context, uri) ?: "unlock_sound.wav"
                    val ext = display.substringAfterLast('.', "wav").ifBlank { "wav" }
                    val out = java.io.File(dir, "unlock_sound.$ext")
                    val copied = context.contentResolver.openInputStream(uri)?.use { input ->
                        out.outputStream().use { input.copyTo(it) }; true
                    } ?: false
                    if (!copied) return@runCatching null
                    NativeApp.setAchievementsUnlockSound(out.absolutePath)
                    MainActivityRuntime.prefs.edit().putString(UNLOCK_SOUND_PREF, display).apply()
                    display
                }.getOrNull()
            }
            if (name != null) state.value = state.value.copy(unlockSoundName = name)
            else state.value = state.value.copy(error = "Couldn't import that sound file.")
        }
    }

    /** Remove the custom unlock sound, reverting to the bundled default. */
    fun clearUnlockSound() {
        val context = getApplication<Application>()
        runCatching { java.io.File(context.filesDir, "achievements").deleteRecursively() }
        NativeApp.setAchievementsUnlockSound("")
        MainActivityRuntime.prefs.edit().remove(UNLOCK_SOUND_PREF).apply()
        state.value = state.value.copy(unlockSoundName = null)
    }

    /** Volume for the unlock/info sound effect, 0..100 %. Applied app-side in
     *  NativeApp.playSound (MediaPlayer.setVolume) — the native core just hands it the .wav path,
     *  so this needs no [Achievements] setting. Takes effect on the next sound. */
    fun setSoundVolume(pct: Int) {
        val clamped = pct.coerceIn(0, 100)
        MainActivityRuntime.prefs.edit().putInt(SOUND_VOLUME_PREF, clamped).apply()
        NativeApp.sSoundVolume = clamped / 100f
        state.value = state.value.copy(soundVolume = clamped)
    }

    private fun queryDisplayName(context: android.content.Context, uri: android.net.Uri): String? =
        runCatching {
            context.contentResolver.query(uri, arrayOf(android.provider.OpenableColumns.DISPLAY_NAME), null, null, null)?.use {
                if (it.moveToFirst()) it.getString(0) else null
            }
        }.getOrNull()

    override fun onCleared() {
        scope.cancel()
        super.onCleared()
    }

    companion object {
        const val UNLOCK_SOUND_PREF = "ra.unlockSoundName"
        const val SOUND_VOLUME_PREF = "ra.soundVolume"

        /** Push the persisted unlock-sound volume into NativeApp at app start, before any
         *  achievement can unlock — otherwise the first sound of the session plays at full
         *  volume regardless of the slider until the RA screen is opened. */
        fun syncSoundVolume() {
            NativeApp.sSoundVolume =
                MainActivityRuntime.prefs.getInt(SOUND_VOLUME_PREF, 100).coerceIn(0, 100) / 100f
        }
    }
}

/**
 * Parse the achievement list out of NativeApp.getAchievementsJSON(). The native
 * side (Achievements::GetAchievementsAsJSON) emits the list under the "items" key —
 * NOT "achievements" — so reading the wrong key silently yielded an empty list and
 * a bogus "No achievements" state even for recognised games. Shared by the full RA
 * screen and the in-game RetroAchievements pane.
 */
fun parseAchievementItems(json: String): List<AchievementItem> {
    if (json.isBlank()) return emptyList()
    val root = runCatching { JSONObject(json) }.getOrNull() ?: return emptyList()
    val array = root.optJSONArray("items") ?: return emptyList()
    return buildList {
        repeat(array.length()) { index ->
            val item = array.optJSONObject(index) ?: return@repeat
            add(
                AchievementItem(
                    id = item.optInt("id"),
                    title = item.optString("title"),
                    description = item.optString("description"),
                    points = item.optInt("points"),
                    unlocked = item.optBoolean("unlocked"),
                    progress = item.optString("measuredProgress"),
                    iconUrl = item.optString("iconUrl", item.optString("badgeUrl")),
                    subsetId = item.optInt("subsetId"),
                    primed = item.optBoolean("primed"),
                ),
            )
        }
    }.sortedBy { if (it.primed) 0 else 1 }
    // Float ACTIVE-CHALLENGE (primed) achievements to the top for quick identification.
    // sortedBy is stable, so every non-primed item keeps RetroAchievements' native list
    // order (its display/progression order = the story/unlock sequence); we used to
    // re-sort unlocked-first then alphabetically, which hid what to unlock next.
}

/** Parse the top-level "subsets" array (base + bonus subsets) emitted by the native side. */
fun parseSubsets(json: String): List<Subset> {
    if (json.isBlank()) return emptyList()
    val root = runCatching { JSONObject(json) }.getOrNull() ?: return emptyList()
    val array = root.optJSONArray("subsets") ?: return emptyList()
    return buildList {
        repeat(array.length()) { index ->
            val o = array.optJSONObject(index) ?: return@repeat
            add(Subset(o.optInt("id"), o.optString("title"), o.optInt("numAchievements")))
        }
    }
}

