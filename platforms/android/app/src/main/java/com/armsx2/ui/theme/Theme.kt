package com.armsx2.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.Color
import com.armsx2.runtime.MainActivityRuntime
import androidx.core.content.edit

enum class ThemeMode { Dark, Light }

object ThemePreferences {
    private const val PreferenceKey = "ui.theme.mode"

    val mode = mutableStateOf(ThemeMode.Dark)

    fun load() {
        mode.value = when (MainActivityRuntime.prefs.getString(PreferenceKey, ThemeMode.Dark.name)) {
            ThemeMode.Light.name -> ThemeMode.Light
            else -> ThemeMode.Dark
        }
    }

    fun set(value: ThemeMode) {
        mode.value = value
        MainActivityRuntime.prefs.edit { putString(PreferenceKey, value.name) }
    }

    fun toggle() = set(if (mode.value == ThemeMode.Dark) ThemeMode.Light else ThemeMode.Dark)
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
    MaterialTheme(
        colorScheme = if (ThemePreferences.mode.value == ThemeMode.Dark) NightScheme else DayScheme,
        typography = ArmsTypography,
        content = content,
    )
}
