package com.armsx2.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.Color
import com.armsx2.runtime.MainActivityRuntime
import androidx.core.content.edit

enum class ThemeMode { System, Dark, Light }

object ThemePreferences {
    private const val PreferenceKey = "ui.theme.mode"

    val mode = mutableStateOf(ThemeMode.System)

    fun load() {
        mode.value = when (MainActivityRuntime.prefs.getString(PreferenceKey, ThemeMode.System.name)) {
            ThemeMode.System.name -> ThemeMode.System
            ThemeMode.Light.name -> ThemeMode.Light
            else -> ThemeMode.Dark
        }
    }

    fun set(value: ThemeMode) {
        mode.value = value
        MainActivityRuntime.prefs.edit { putString(PreferenceKey, value.name) }
    }

}

/** Whether the animated intro video plays on cold boot. Read by
 *  [com.armsx2.BootSplashActivity] straight from the "ARMSX2" prefs (key
 *  "ui.bootLogo") before Compose is up; this holder just backs the App-tab
 *  toggle and keeps that same key in sync. */
object BootLogoPreferences {
    private const val PreferenceKey = "ui.bootLogo"

    val enabled = mutableStateOf(true)

    fun load() {
        enabled.value = MainActivityRuntime.prefs.getBoolean(PreferenceKey, true)
    }

    fun set(value: Boolean) {
        enabled.value = value
        MainActivityRuntime.prefs.edit { putBoolean(PreferenceKey, value) }
    }
}

/** Whether the library view toolbar is pinned to the bottom of the screen (App
 *  setting). Default false = the toolbar sits at the top. */
object ToolbarPositionPreferences {
    private const val PreferenceKey = "ui.toolbarBottom"

    val atBottom = mutableStateOf(false)

    fun load() {
        atBottom.value = MainActivityRuntime.prefs.getBoolean(PreferenceKey, false)
    }

    fun set(value: Boolean) {
        atBottom.value = value
        MainActivityRuntime.prefs.edit { putBoolean(PreferenceKey, value) }
    }
}

/** Visibility of optional library-home sections. Both default to visible so
 * existing users keep the current layout until they explicitly opt out. */
object LibraryChromePreferences {
    private const val SearchKey = "ui.library.showSearch"
    private const val RecentsKey = "ui.library.showRecents"

    val showSearch = mutableStateOf(false)
    val showRecents = mutableStateOf(true)

    fun load() {
        showSearch.value = MainActivityRuntime.prefs.getBoolean(SearchKey, false)
        showRecents.value = MainActivityRuntime.prefs.getBoolean(RecentsKey, true)
    }

    fun setShowSearch(value: Boolean) {
        showSearch.value = value
        MainActivityRuntime.prefs.edit { putBoolean(SearchKey, value) }
    }

    fun setShowRecents(value: Boolean) {
        showRecents.value = value
        MainActivityRuntime.prefs.edit { putBoolean(RecentsKey, value) }
    }
}

private val NightScheme = darkColorScheme(
    primary = ArmsBlueBright,
    onPrimary = Color(0xFF07101F),
    primaryContainer = Color(0xFF183B73),
    onPrimaryContainer = Color(0xFFD9E7FF),
    secondary = ArmsCyan,
    onSecondary = Color(0xFF001F25),
    secondaryContainer = Color(0xFF123944),
    onSecondaryContainer = Color(0xFFB9F3FF),
    tertiary = ArmsViolet,
    background = NightBackground,
    onBackground = NightText,
    surface = NightSurface,
    onSurface = NightText,
    surfaceVariant = NightSurfaceRaised,
    onSurfaceVariant = NightTextMuted,
    outline = NightOutline,
    outlineVariant = NightOutline,
    error = Danger,
    scrim = Color.Black,
    surfaceTint = Color.Transparent,
)

private val DayScheme = lightColorScheme(
    primary = Color(0xFF245DAD),
    onPrimary = Color.White,
    primaryContainer = Color(0xFFD8E6FF),
    onPrimaryContainer = Color(0xFF0A2B58),
    secondary = Color(0xFF087C91),
    onSecondary = Color.White,
    secondaryContainer = Color(0xFFCAF3FA),
    onSecondaryContainer = Color(0xFF00363F),
    tertiary = Color(0xFF5E51B5),
    background = DayBackground,
    onBackground = DayText,
    surface = DaySurface,
    onSurface = DayText,
    surfaceVariant = DaySurfaceRaised,
    onSurfaceVariant = DayTextMuted,
    outline = DayOutline,
    outlineVariant = DayOutline,
    error = Color(0xFFB3263E),
    scrim = Color.Black,
    surfaceTint = Color.Transparent,
)

@Composable
fun Armsx2Theme(content: @Composable () -> Unit) {
    val dark = when (ThemePreferences.mode.value) {
        ThemeMode.System -> isSystemInDarkTheme()
        ThemeMode.Dark -> true
        ThemeMode.Light -> false
    }
    MaterialTheme(
        colorScheme = if (dark) NightScheme else DayScheme,
        typography = ArmsTypography,
        content = content,
    )
}
