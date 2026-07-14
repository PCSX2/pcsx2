package com.armsx2.ui.achievements

import android.app.Application
import androidx.lifecycle.AndroidViewModel
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
        )
    }

    override fun onCleared() {
        scope.cancel()
        super.onCleared()
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
                ),
            )
        }
    }
    // Keep RetroAchievements' native list order (its display/progression order = the
    // story/unlock sequence). We used to re-sort unlocked-first then alphabetically by
    // title, which made it impossible to see what to unlock next; restore progression.
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

