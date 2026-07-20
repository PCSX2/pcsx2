package com.armsx2.ui.settingshub

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.GameInfo
import com.armsx2.config.ConfigStore
import com.armsx2.config.Settings
import com.armsx2.config.SettingsScope
import com.armsx2.navigation.SettingsCategory
import com.armsx2.ui.InGameOverlay

data class SettingsUiState(
    val category: SettingsCategory = SettingsCategory.General,
    val game: GameInfo? = null,
)

class SettingsViewModel(application: Application) : AndroidViewModel(application) {
    var uiState = androidx.compose.runtime.mutableStateOf(SettingsUiState())
        private set

    val settings get() = InGameOverlay.settingsState

    fun load(category: SettingsCategory, game: GameInfo?) {
        val serial = game?.settingsKey
        InGameOverlay.currentSerial.value = serial
        InGameOverlay.settingsScope.value = if (serial == null) SettingsScope.Global else SettingsScope.Game
        settings.value = if (serial == null) ConfigStore.loadGlobal() else ConfigStore.resolveForGame(serial)
        uiState.value = SettingsUiState(
            category = if (game != null && category == SettingsCategory.General) SettingsCategory.Performance else category,
            game = game,
        )
    }

    fun selectCategory(category: SettingsCategory) {
        uiState.value = uiState.value.copy(category = category)
    }

    /**
     * Reset ONLY the tab currently being shown.
     *
     * This used to reset the entire scope — `Settings()` globally, or clearing the whole
     * per-game override blob — so pressing Reset on the Renderer page also wiped Audio,
     * Network, Performance and Fixes. (Controller settings survived only because they live
     * in ControllerMappings, not because Reset was scoped.) Categories that own no Settings
     * fields are a no-op; Controls keeps its own reset row for binds/tunables.
     */
    fun resetCurrentScope(category: SettingsCategory) {
        // Takes the category the SCREEN is showing, not uiState.category: the screen remaps
        // General -> Performance under a game scope, so reading it here would reset the wrong
        // (or no) tab.
        if (!categoryHasResettableSettings(category)) return
        val serial = uiState.value.game?.settingsKey
        if (serial != null) {
            // Per-game: drop just this tab's override keys, so those settings fall back to
            // global while every other per-game tweak the user made is preserved.
            ConfigStore.loadOverrides(serial)?.let { overrides ->
                val pruned = pruneOverrides(overrides, categoryOverrideKeys(category))
                if (pruned == null) ConfigStore.clearOverrides(serial)
                else ConfigStore.saveOverrides(serial, pruned)
            }
            settings.value = ConfigStore.resolveForGame(serial)
        } else {
            settings.value = settings.value.resetCategory(category)
            ConfigStore.saveGlobal(settings.value)
        }
    }
}
