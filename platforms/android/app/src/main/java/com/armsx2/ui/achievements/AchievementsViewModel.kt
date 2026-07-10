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
    val loading: Boolean = false,
    val error: String? = null,
)

class AchievementsViewModel(application: Application) : AndroidViewModel(application) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)

    var state = androidx.compose.runtime.mutableStateOf(AchievementsUiState())
        private set

    fun refresh() {
        val snapshot = runCatching { parse(NativeApp.getAchievementsJSON().orEmpty()) }.getOrNull()
        if (snapshot != null) state.value = snapshot.copy(richPresence = runCatching { NativeApp.getRichPresence() }.getOrDefault(""))
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

    fun toggleHardcore() {
        val enabled = !state.value.hardcore
        NativeApp.setHardcoreMode(enabled)
        state.value = state.value.copy(hardcore = enabled)
    }

    fun dismissError() {
        state.value = state.value.copy(error = null)
    }

    private fun parse(json: String): AchievementsUiState {
        if (json.isBlank()) return state.value
        val root = JSONObject(json)
        val array = root.optJSONArray("achievements")
        val items = buildList {
            if (array != null) repeat(array.length()) { index ->
                val item = array.getJSONObject(index)
                add(
                    AchievementItem(
                        id = item.optInt("id"),
                        title = item.optString("title"),
                        description = item.optString("description"),
                        points = item.optInt("points"),
                        unlocked = item.optBoolean("unlocked"),
                        progress = item.optString("measuredProgress"),
                        iconUrl = item.optString("iconUrl"),
                    ),
                )
            }
        }
        return AchievementsUiState(
            loggedIn = root.optBoolean("loggedIn"),
            userName = root.optString("userName"),
            hardcore = root.optBoolean("hardcore"),
            score = root.optLong("score").coerceAtLeast(0),
            items = items.sortedWith(compareByDescending<AchievementItem> { it.unlocked }.thenBy { it.title }),
        )
    }

    override fun onCleared() {
        scope.cancel()
        super.onCleared()
    }
}

