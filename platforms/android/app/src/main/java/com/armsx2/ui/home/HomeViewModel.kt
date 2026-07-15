package com.armsx2.ui.home

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.GameInfo
import com.armsx2.data.library.GameLibraryRepository
import com.armsx2.runtime.MainActivityRuntime
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import androidx.core.content.edit

enum class HomeSort { Title, RecentlyPlayed, Compatibility }

enum class LibraryLayout { Grid, List, Shelf }

data class HomeUiState(
    val allGames: List<GameInfo> = emptyList(),
    val visibleGames: List<GameInfo> = emptyList(),
    val recentGames: List<GameInfo> = emptyList(),
    val query: String = "",
    val sort: HomeSort = HomeSort.Title,
    val layout: LibraryLayout = LibraryLayout.Grid,
    val scanning: Boolean = false,
    val error: String? = null,
    val selectedIndex: Int = 0,
    val initialized: Boolean = false,
)

class HomeViewModel(application: Application) : AndroidViewModel(application) {
    private val repository = GameLibraryRepository(application)
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var scanJob: Job? = null
    private var loaded = false
    private var pendingInitialScan = false
    private var directories: List<String> = emptyList()

    var state = androidx.compose.runtime.mutableStateOf(HomeUiState())
        private set

    fun load(romDirectories: List<String>, nativeReady: Boolean) {
        directories = romDirectories
        if (!loaded) {
            loaded = true
            val cached = repository.loadCached()
            val layout = runCatching {
                LibraryLayout.valueOf(
                    MainActivityRuntime.prefs.getString(LayoutPreference, LibraryLayout.Grid.name) ?: LibraryLayout.Grid.name,
                )
            }.getOrDefault(LibraryLayout.Grid)
            pendingInitialScan = romDirectories.isNotEmpty() && cached.key != repository.cacheKey(romDirectories)
            state.value = buildState(
                base = state.value.copy(
                    allGames = cached.games,
                    layout = layout,
                    initialized = cached.games.isNotEmpty() || !pendingInitialScan,
                ),
            )
            if (nativeReady && pendingInitialScan) refresh()
        } else if (nativeReady && pendingInitialScan) {
            refresh()
        }
    }

    fun refresh() {
        if (directories.isEmpty() || scanJob?.isActive == true) return
        scanJob = scope.launch {
            val initialScan = pendingInitialScan && state.value.allGames.isEmpty()
            state.value = state.value.copy(
                scanning = true,
                initialized = if (initialScan) false else state.value.initialized,
                error = null,
            )
            val result = runCatching { repository.scan(directories) }
            result.onSuccess { games ->
                pendingInitialScan = false
                state.value = buildState(
                    state.value.copy(allGames = games, scanning = false, initialized = true),
                )
            }.onFailure { failure ->
                pendingInitialScan = false
                state.value = state.value.copy(
                    scanning = false,
                    initialized = true,
                    error = failure.message ?: "Unable to scan the selected folders.",
                )
            }
        }
    }

    fun setQuery(value: String) {
        state.value = buildState(state.value.copy(query = value, selectedIndex = 0))
    }

    fun setSort(value: HomeSort) {
        state.value = buildState(state.value.copy(sort = value, selectedIndex = 0))
    }

    fun toggleLayout() {
        val entries = LibraryLayout.entries
        val next = entries[(state.value.layout.ordinal + 1) % entries.size]
        MainActivityRuntime.prefs.edit { putString(LayoutPreference, next.name) }
        state.value = state.value.copy(layout = next)
    }

    fun moveSelection(delta: Int) {
        val count = state.value.visibleGames.size
        if (count == 0) return
        state.value = state.value.copy(selectedIndex = (state.value.selectedIndex + delta).coerceIn(0, count - 1))
    }

    fun setSelection(index: Int) {
        if (state.value.visibleGames.isEmpty()) return
        state.value = state.value.copy(selectedIndex = index.coerceIn(state.value.visibleGames.indices))
    }

    fun selectedGame(): GameInfo? = state.value.visibleGames.getOrNull(state.value.selectedIndex)

    fun launch(game: GameInfo) {
        repository.markPlayed(game)
        state.value = buildState(state.value)
        val launchPath = if (game.uri.scheme == "file") game.uri.path ?: game.uri.toString() else game.uri.toString()
        MainActivityRuntime.launchGame(launchPath, game)
    }

    /** Mark a game hidden (or un-hidden) and refresh the visible list. */
    fun setHidden(game: GameInfo, value: Boolean) {
        com.armsx2.HiddenGames.setHidden(game, value)
        state.value = buildState(state.value)
    }

    /** Toggle whether hidden games are shown (so they can be un-hidden). */
    fun setShowHidden(value: Boolean) {
        com.armsx2.HiddenGames.setShowHidden(value)
        state.value = buildState(state.value)
    }

    private fun buildState(base: HomeUiState): HomeUiState {
        val recents = repository.recentGames(base.allGames)
        val recentOrder = recents.mapIndexed { index, game -> game.uri.toString() to index }.toMap()
        val filtered = base.allGames.filter { game ->
            val query = base.query.trim()
            // Exclude games the user marked hidden (long-press → Hide), unless "Show hidden" is on.
            (com.armsx2.HiddenGames.showHidden.value || !com.armsx2.HiddenGames.isHidden(game)) &&
                (query.isBlank() ||
                    game.title.contains(query, ignoreCase = true) ||
                    game.serial?.contains(query, ignoreCase = true) == true ||
                    game.extension.contains(query, ignoreCase = true))
        }
        val sorted = when (base.sort) {
            HomeSort.Title -> filtered.sortedBy { it.title.lowercase() }
            HomeSort.RecentlyPlayed -> filtered.sortedWith(
                compareBy<GameInfo> { recentOrder[it.uri.toString()] ?: Int.MAX_VALUE }
                    .thenBy { it.title.lowercase() },
            )
            HomeSort.Compatibility -> filtered.sortedWith(
                compareByDescending<GameInfo> { it.compatibility }.thenBy { it.title.lowercase() },
            )
        }
        return base.copy(
            visibleGames = sorted,
            recentGames = recents,
            selectedIndex = base.selectedIndex.coerceIn(0, (sorted.size - 1).coerceAtLeast(0)),
        )
    }

    override fun onCleared() {
        scope.cancel()
        super.onCleared()
    }

    private companion object {
        // String-valued now (Grid/List/Shelf). New key so the old boolean pref is
        // ignored and everyone starts at Grid rather than mis-parsing "true"/"false".
        const val LayoutPreference = "library.layout.mode"
    }
}
