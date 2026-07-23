package com.armsx2.navigation

import androidx.compose.runtime.mutableStateOf
import com.armsx2.GameInfo

sealed interface AppRoute {
    data object Home : AppRoute
    data class Settings(
        val category: SettingsCategory = SettingsCategory.General,
        val game: GameInfo? = null,
    ) : AppRoute
    // Carries an optional game so the per-game BIOS picker can key on it directly
    // (from the library long-press) without the game being loaded; null = global,
    // opened from the drawer (falls back to the currently loaded game if any).
    data class BiosManager(val game: GameInfo? = null) : AppRoute
    data object MemoryCardManager : AppRoute
    data object SaveManager : AppRoute
    data object ControllerManager : AppRoute
    data object PatchManager : AppRoute
    data object TextureManager : AppRoute
    data object Achievements : AppRoute
    data object Language : AppRoute
    data object About : AppRoute
}

enum class SettingsCategory {
    General,
    Info,
    Performance,
    Graphics,
    Audio,
    Controls,
    Hotkeys,
    Network,
    OnScreen,
    Skins,
    Advanced,
    Recompiler,
    Patches,
    About,
}

object UiNavigator {
    val route = mutableStateOf<AppRoute>(AppRoute.Home)
    val drawerOpen = mutableStateOf(false)

    fun navigate(destination: AppRoute) {
        val changed = route.value != destination
        route.value = destination
        drawerOpen.value = false
        // "Entering a settings menu / sub-screen" blip — but not for just returning Home.
        if (changed && destination != AppRoute.Home) {
            com.armsx2.MenuSfx.play(com.armsx2.MenuSfx.Event.SUBMENU)
        }
    }

    fun home() = navigate(AppRoute.Home)

    fun back(): Boolean {
        if (drawerOpen.value) {
            drawerOpen.value = false
            return true
        }
        when (route.value) {
            AppRoute.Language -> {
                route.value = AppRoute.Settings(SettingsCategory.General)
                return true
            }
            AppRoute.About -> {
                route.value = AppRoute.Settings(SettingsCategory.General)
                return true
            }
            AppRoute.Home -> Unit
            else -> {
                route.value = AppRoute.Home
                return true
            }
        }
        drawerOpen.value = true
        return true
    }
}
