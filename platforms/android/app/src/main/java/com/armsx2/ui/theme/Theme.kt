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

/**
 * "Dark" was renamed to [Blue] because that is what it always was — a blue-tinted dark theme —
 * which made it a confusing sibling once other hues existed. The stored preference is the enum
 * NAME, so an existing user's saved "Dark" no longer matches any entry and falls through to the
 * [Blue] default in [ThemePreferences.load]: same colours, no migration step, nothing to reset.
 */
enum class ThemeMode { System, Light, Blue, Purple, Pink, Red, Orange, Green, Teal, Black, Oled }

object ThemePreferences {
    private const val PreferenceKey = "ui.theme.mode"

    val mode = mutableStateOf(ThemeMode.System)

    fun load() {
        // Name-matched rather than hand-enumerated, so adding a colour needs no change here.
        // Anything unrecognised — including the legacy "Dark" — resolves to Blue, which is
        // exactly what "Dark" used to render as.
        val stored = MainActivityRuntime.prefs.getString(PreferenceKey, ThemeMode.System.name)
        mode.value = ThemeMode.entries.firstOrNull { it.name == stored } ?: ThemeMode.Blue
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

// Neutral dark schemes derived from Night — same accents, but black/neutral-grey backgrounds and
// selection highlights instead of Night's blue-tinted ones (users asked for a black, not-blue UI).
// Black = a slightly-lifted neutral dark; Oled = true #000000 for OLED panels (deepest black, less
// power). Only surfaces/containers/outlines change; the primary accent stays for contrast/usability.
private val BlackScheme = NightScheme.copy(
    background = Color(0xFF0B0B0B),
    surface = Color(0xFF111111),
    surfaceVariant = Color(0xFF1B1B1B),
    primaryContainer = Color(0xFF242424),
    onPrimaryContainer = Color(0xFFEAEAEA),
    secondaryContainer = Color(0xFF1A1A1A),
    outline = Color(0xFF333333),
    outlineVariant = Color(0xFF242424),
)

private val OledScheme = NightScheme.copy(
    background = Color.Black,
    surface = Color.Black,
    surfaceVariant = Color(0xFF0E0E0E),
    primaryContainer = Color(0xFF1A1A1A),
    onPrimaryContainer = Color(0xFFEAEAEA),
    secondaryContainer = Color(0xFF121212),
    outline = Color(0xFF262626),
    outlineVariant = Color(0xFF161616),
)

/**
 * A hue-tinted dark scheme, built from Night the same way Black/Oled are.
 *
 * Night IS the blue theme, so a colour variant is "Night with a different hue": the accent
 * changes AND the surfaces carry the same gentle tint of that hue, rather than leaving blue
 * chrome under a pink accent. Only colour-bearing roles are overridden — text, error and scrim
 * stay inherited so contrast behaviour is identical across every hue.
 */
private fun tintedDark(
    accent: Color,
    accentContainer: Color,
    onAccentContainer: Color,
    background: Color,
    surface: Color,
    surfaceRaised: Color,
    outline: Color,
) = NightScheme.copy(
    primary = accent,
    onPrimary = Color(0xFF0B0B12),
    primaryContainer = accentContainer,
    onPrimaryContainer = onAccentContainer,
    secondary = accent,
    onSecondary = Color(0xFF0B0B12),
    secondaryContainer = accentContainer,
    onSecondaryContainer = onAccentContainer,
    tertiary = accent,
    background = background,
    surface = surface,
    surfaceVariant = surfaceRaised,
    outline = outline,
    outlineVariant = outline,
)

private val PurpleScheme = tintedDark(
    accent = Color(0xFFC7A6FF), accentContainer = Color(0xFF3A2A63), onAccentContainer = Color(0xFFEADDFF),
    background = Color(0xFF120E1B), surface = Color(0xFF171223), surfaceRaised = Color(0xFF221A33),
    outline = Color(0xFF3A3050),
)
private val PinkScheme = tintedDark(
    accent = Color(0xFFFFA6D2), accentContainer = Color(0xFF63284A), onAccentContainer = Color(0xFFFFD9E9),
    background = Color(0xFF1A0F16), surface = Color(0xFF22141D), surfaceRaised = Color(0xFF301C29),
    outline = Color(0xFF4E3241),
)
private val RedScheme = tintedDark(
    accent = Color(0xFFFF9A90), accentContainer = Color(0xFF6B2B26), onAccentContainer = Color(0xFFFFDAD5),
    background = Color(0xFF190F0E), surface = Color(0xFF211513), surfaceRaised = Color(0xFF2F1E1B),
    outline = Color(0xFF503533),
)
private val OrangeScheme = tintedDark(
    accent = Color(0xFFFFB77C), accentContainer = Color(0xFF6A3A15), onAccentContainer = Color(0xFFFFDCC2),
    background = Color(0xFF17110A), surface = Color(0xFF1F1710), surfaceRaised = Color(0xFF2D2117),
    outline = Color(0xFF4E3A27),
)
private val GreenScheme = tintedDark(
    accent = Color(0xFF8FD98F), accentContainer = Color(0xFF22512A), onAccentContainer = Color(0xFFCDEFCB),
    background = Color(0xFF0D150F), surface = Color(0xFF121C15), surfaceRaised = Color(0xFF1B291F),
    outline = Color(0xFF2E4634),
)
private val TealScheme = tintedDark(
    accent = Color(0xFF7ED8D0), accentContainer = Color(0xFF174E4A), onAccentContainer = Color(0xFFC2F1EC),
    background = Color(0xFF0A1514), surface = Color(0xFF0F1D1C), surfaceRaised = Color(0xFF172A28),
    outline = Color(0xFF2A4644),
)

@Composable
fun Armsx2Theme(content: @Composable () -> Unit) {
    val scheme = when (ThemePreferences.mode.value) {
        ThemeMode.System -> if (isSystemInDarkTheme()) NightScheme else DayScheme
        ThemeMode.Blue -> NightScheme
        ThemeMode.Light -> DayScheme
        ThemeMode.Black -> BlackScheme
        ThemeMode.Oled -> OledScheme
        ThemeMode.Purple -> PurpleScheme
        ThemeMode.Pink -> PinkScheme
        ThemeMode.Red -> RedScheme
        ThemeMode.Orange -> OrangeScheme
        ThemeMode.Green -> GreenScheme
        ThemeMode.Teal -> TealScheme
    }
    MaterialTheme(
        colorScheme = scheme,
        typography = ArmsTypography,
        content = content,
    )
}
