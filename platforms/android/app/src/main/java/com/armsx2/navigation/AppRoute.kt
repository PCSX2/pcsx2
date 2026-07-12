package com.armsx2.navigation

import androidx.compose.runtime.mutableStateOf
import com.armsx2.GameInfo

sealed interface AppRoute {
    data object Home : AppRoute
    data class Settings(
        val category: SettingsCategory = SettingsCategory.General,
        val game: GameInfo? = null,
    ) : AppRoute
    data object BiosManager : AppRoute
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
        route.value = destination
        drawerOpen.value = false
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
