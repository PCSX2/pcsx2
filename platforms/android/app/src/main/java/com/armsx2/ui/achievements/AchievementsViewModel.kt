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
)

data class AchievementsUiState(
    val loggedIn: Boolean = false,
    val userName: String = "",
    val hardcore: Boolean = false,
    val score: Long = 0,
    val items: List<AchievementItem> = emptyList(),
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
        // Enabling hardcore only takes hold on a system reset (upstream design). The
        // VM is paused behind this panel, so the native "will be enabled on system
        // reset" toast would just sit there forever. Reboot the game now — so
        // "Enable & restart" actually restarts — and drop back to it so the reset is
        // visible. Disabling needs no reboot (casual mode applies live).
        if (target) {
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
            hardcore = root.optBoolean("hardcore"),
            score = root.optLong("score").coerceAtLeast(0),
            items = parseAchievementItems(json),
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
                ),
            )
        }
    }.sortedWith(compareByDescending<AchievementItem> { it.unlocked }.thenBy { it.title })
}

