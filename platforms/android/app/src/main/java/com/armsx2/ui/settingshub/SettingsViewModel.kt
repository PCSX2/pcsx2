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
        uiState.value = SettingsUiState(category, game)
    }

    fun selectCategory(category: SettingsCategory) {
        uiState.value = uiState.value.copy(category = category)
    }

    fun resetCurrentScope() {
        val game = uiState.value.game
        val serial = game?.settingsKey
        if (serial != null) {
            ConfigStore.clearOverrides(serial)
            settings.value = ConfigStore.resolveForGame(serial)
        } else {
            settings.value = Settings()
            ConfigStore.saveGlobal(settings.value)
        }
    }
}
