package com.armsx2.ui.theme

import android.os.Build
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.material3.ColorScheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import com.armsx2.runtime.MainActivityRuntime
import androidx.core.content.edit

/**
 * "Dark" was renamed to [Blue] because that is what it always was — a blue-tinted dark theme —
 * which made it a confusing sibling once other hues existed. The stored preference is the enum
 * NAME, so an existing user's saved "Dark" no longer matches any entry and falls through to the
 * [Blue] default in [ThemePreferences.load]: same colours, no migration step, nothing to reset.
 */
enum class ThemeMode { System, MaterialYou, Rgb, Custom, Light, Blue, Purple, Pink, Red, Orange, Green, Teal, Cyan, Black, Oled;

    /** Wallpaper-derived dynamic colour. Needs Android 12; the picker hides it below that. */
    val requiresDynamicColor: Boolean get() = this == MaterialYou

    /**
     * True when this theme leaves light/dark to the OS instead of committing to one. Both
     * System and MaterialYou do — Material You follows the system setting as well as the
     * wallpaper — which is what the system-bar contrast logic keys off.
     */
    val followsSystemDarkMode: Boolean get() = this == System || this == MaterialYou
}

object ThemePreferences {
    private const val PreferenceKey = "ui.theme.mode"

    val mode = mutableStateOf(ThemeMode.System)

    fun load() {
        // Name-matched rather than hand-enumerated, so adding a colour needs no change here.
        // Anything unrecognised — including the legacy "Dark" — resolves to Blue, which is
        // exactly what "Dark" used to render as.
        val stored = MainActivityRuntime.prefs.getString(PreferenceKey, ThemeMode.System.name)
        mode.value = ThemeMode.entries.firstOrNull { it.name == stored } ?: ThemeMode.Blue
        loadCustomColor()
    }

    fun set(value: ThemeMode) {
        mode.value = value
        MainActivityRuntime.prefs.edit { putString(PreferenceKey, value.name) }
    }

    /** Accent for [ThemeMode.Custom], as packed ARGB. Defaults to the Cyan accent. */
    private const val CustomColorKey = "ui.theme.customColor"
    val customColor = mutableStateOf(DefaultCustomColor)

    fun loadCustomColor() {
        customColor.value = MainActivityRuntime.prefs.getInt(CustomColorKey, DefaultCustomColor)
    }

    fun setCustomColor(argb: Int) {
        customColor.value = argb
        MainActivityRuntime.prefs.edit { putInt(CustomColorKey, argb) }
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
    private const val OpacityKey = "ui.library.opacity"

    val showSearch = mutableStateOf(false)
    val showRecents = mutableStateOf(true)
    // Card/list translucency over the wallpaper, as a percent (20–100). 100 = the old
    // fully-opaque look; lower lets the library background show through the game rows.
    val libraryOpacity = mutableStateOf(100)

    fun load() {
        showSearch.value = MainActivityRuntime.prefs.getBoolean(SearchKey, false)
        showRecents.value = MainActivityRuntime.prefs.getBoolean(RecentsKey, true)
        libraryOpacity.value = MainActivityRuntime.prefs.getInt(OpacityKey, 100).coerceIn(20, 100)
    }

    fun setShowSearch(value: Boolean) {
        showSearch.value = value
        MainActivityRuntime.prefs.edit { putBoolean(SearchKey, value) }
    }

    fun setShowRecents(value: Boolean) {
        showRecents.value = value
        MainActivityRuntime.prefs.edit { putBoolean(RecentsKey, value) }
    }

    fun setLibraryOpacity(value: Int) {
        val v = value.coerceIn(20, 100)
        libraryOpacity.value = v
        MainActivityRuntime.prefs.edit { putInt(OpacityKey, v) }
    }
}

/** Custom color for the animated library "wave" background — the procedural XMB gradient drawn
 *  by [com.armsx2.ui.home.XmbGlView]. The theme accent ([ThemePreferences.customColor]) only
 *  colors the UI chrome; this colors the backdrop itself. The user picks the vivid gradient stop
 *  (what fills most of the frame); the deep near-black top stop is derived from it so the gradient
 *  keeps its XMB depth. The white wave ribbon is left untinted.
 *
 *  Threading: the wave renders on XmbGlView's OWN EGL thread, not Compose's — so the shader-facing
 *  values are held as @Volatile float triplets it can read every frame without touching Compose
 *  state cross-thread. [color] backs the settings picker; [glTop]/[glBottom] feed the shader
 *  (null = fall back to XmbGlView's built-in constants). 0 = not customized, default untouched. */
object LibraryBackgroundColorPreferences {
    private const val Key = "ui.library.bgColor"

    /** The built-in wave color (matches XmbGlView.BG_BOT), shown in the picker when unset. */
    const val DefaultDisplayColor: Int = 0xFF2E75F5.toInt()

    /** Quick-pick palette for the settings swatches (XMB-flavored); the RGB sliders stay for custom
     *  colors. First entry is the built-in default. */
    val PRESETS: List<Int> = listOf(
        0xFF2E75F5.toInt(), // royal blue (default)
        0xFF00B4D8.toInt(), // aqua
        0xFF19C37D.toInt(), // green
        0xFF16A085.toInt(), // teal
        0xFF9B59B6.toInt(), // purple
        0xFFE84393.toInt(), // magenta
        0xFFE74C3C.toInt(), // red
        0xFFF39C12.toInt(), // orange
        0xFFF1C40F.toInt(), // gold
        0xFFAF601A.toInt(), // bronze
        0xFF95A5A6.toInt(), // silver
        0xFF34495E.toInt(), // slate
    )

    /** Packed ARGB for the settings UI. 0 = not customized (use the built-in default). */
    val color = mutableStateOf(0)

    /** GL-thread gradient stops (linear 0..1 RGB); null = use XmbGlView's own BG_TOP/BG_BOT. */
    @Volatile var glTop: FloatArray? = null
        private set
    @Volatile var glBottom: FloatArray? = null
        private set

    /** When on, the wave cycles the hue wheel continuously (RGB-peripheral style), overriding the
     *  fixed color. XmbGlView reads [rgbCycleGl] straight from its GL thread. */
    private const val RgbKey = "ui.library.bgRgb"
    val rgbCycle = mutableStateOf(false)
    @Volatile var rgbCycleGl = false
        private set

    fun load() {
        val stored = MainActivityRuntime.prefs.getInt(Key, 0)
        color.value = stored
        apply(stored)
        val rgb = MainActivityRuntime.prefs.getBoolean(RgbKey, false)
        rgbCycle.value = rgb
        rgbCycleGl = rgb
    }

    /** Set the wave color (packed ARGB; alpha forced opaque). Applies live on the next GL frame. */
    fun set(argb: Int) {
        val v = if (argb == 0) 0 else (argb or (0xFF shl 24))
        color.value = v
        MainActivityRuntime.prefs.edit { putInt(Key, v) }
        apply(v)
    }

    /** Revert to the built-in default wave color. */
    fun reset() {
        color.value = 0
        MainActivityRuntime.prefs.edit { remove(Key) }
        apply(0)
    }

    /** Toggle the continuous RGB hue-cycle (matches the theme's RGB mode). Applies on the next frame. */
    fun setRgbCycle(on: Boolean) {
        rgbCycle.value = on
        rgbCycleGl = on
        MainActivityRuntime.prefs.edit { putBoolean(RgbKey, on) }
    }

    private fun apply(argb: Int) {
        if (argb == 0) { glTop = null; glBottom = null; return }
        val r = ((argb shr 16) and 0xFF) / 255f
        val g = ((argb shr 8) and 0xFF) / 255f
        val b = (argb and 0xFF) / 255f
        glBottom = floatArrayOf(r, g, b)
        // 0.20 keeps roughly the built-in blue's top/bottom ratio: a deep, near-black anchor.
        glTop = floatArrayOf(r * 0.20f, g * 0.20f, b * 0.20f)
    }
}

/** Rotation for the launcher/library UI only, kept separate from the per-game renderer rotation
 *  (Settings.orientation) — AetherSX2-style split. Values match the renderer's 0/1/2/3 -> ActivityInfo
 *  mapping in MainActivityRuntime.applyEmulationOrientation: 0=Device(default), 1=Landscape, 2=Portrait,
 *  3=Auto-Rotate. Uses its own pref key (NOT the legacy "ui.orientation" migration key). */
object LauncherOrientationPreferences {
    private const val Key = "ui.launcherOrientation"

    val mode = mutableStateOf(0)  // 0 = Device (system default) — preserves prior behavior

    fun load() { mode.value = MainActivityRuntime.prefs.getInt(Key, 0).coerceIn(0, 3) }

    fun set(value: Int) {
        val v = value.coerceIn(0, 3)
        mode.value = v
        MainActivityRuntime.prefs.edit { putInt(Key, v) }
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
// Deliberately bluer and brighter than Teal above, which leans green — sat next to each other
// in the picker they need to be tellable apart at a glance.
private val CyanScheme = tintedDark(
    accent = Color(0xFF6FE3F5), accentContainer = Color(0xFF104A57), onAccentContainer = Color(0xFFBEF1FA),
    background = Color(0xFF08151A), surface = Color(0xFF0D1D23), surfaceRaised = Color(0xFF152A32),
    outline = Color(0xFF26454F),
)

/** Default accent for [ThemeMode.Custom] — the Cyan accent, so it starts somewhere sane. */
private const val DefaultCustomColor: Int = 0xFF6FE3F5.toInt()

/**
 * Scheme derived from a user-picked colour.
 *
 * Using the raw RGB as the accent is the obvious implementation and the wrong one: pick a dark
 * navy, a muddy brown or near-black and you get unreadable text on unreadable chips, which comes
 * back later as a bug report rather than as "I chose a bad colour". So the picked HUE is kept
 * exactly, while saturation and brightness are clamped into a band that stays legible on dark
 * surfaces. The colour you chose is still clearly the colour you get; the illegible corners of
 * the space are simply unreachable.
 *
 * The surfaces take the same hue at low saturation, matching how the hand-tuned palettes are
 * built — otherwise you get a pink accent sitting on blue chrome.
 */
private fun hueScheme(hue: Float, sat: Float, value: Float): ColorScheme {
    fun shade(s: Float, v: Float) =
        Color(android.graphics.Color.HSVToColor(floatArrayOf(hue, s.coerceIn(0f, 1f), v.coerceIn(0f, 1f))))
    return tintedDark(
        accent = shade(sat.coerceAtMost(0.72f), value),
        accentContainer = shade(sat * 0.75f, 0.34f),
        onAccentContainer = shade(sat * 0.30f, 0.94f),
        background = shade(sat * 0.35f, 0.085f),
        surface = shade(sat * 0.32f, 0.115f),
        surfaceRaised = shade(sat * 0.30f, 0.17f),
        outline = shade(sat * 0.30f, 0.32f),
    )
}

private fun customScheme(argb: Int): ColorScheme {
    val hsv = FloatArray(3)
    android.graphics.Color.colorToHSV(argb, hsv)
    return hueScheme(hsv[0], hsv[1].coerceIn(0.35f, 0.95f), hsv[2].coerceIn(0.78f, 1f))
}

/**
 * RGB peripheral-style rainbow: the accent cycles the hue wheel continuously, the way gaming
 * keyboards and cases do. The surfaces cycle with it, so the whole UI drifts through the
 * spectrum rather than a lone coloured button sitting on static chrome.
 *
 * The hue is QUANTISED before building the scheme. Rebuilding a ColorScheme re-runs
 * MaterialTheme and recomposes the entire tree, so animating it per frame would repaint every
 * screen at display rate for a decorative effect. Stepping the hue keeps the cycle smooth to
 * the eye while cutting rebuilds to a few per second.
 */
private const val RgbCycleMillis = 14_000
private const val RgbHueStep = 4f

@Composable
fun Armsx2Theme(content: @Composable () -> Unit) {
    val context = LocalContext.current
    val scheme = when (ThemePreferences.mode.value) {
        ThemeMode.System -> if (isSystemInDarkTheme()) NightScheme else DayScheme
        // Wallpaper-derived palette. Falls back rather than crashing if the preference somehow
        // arrives on a pre-Android-12 device (restored backup, sideloaded prefs) — the picker
        // hides the option there, so this is belt-and-braces.
        ThemeMode.MaterialYou -> when {
            Build.VERSION.SDK_INT < Build.VERSION_CODES.S -> NightScheme
            isSystemInDarkTheme() -> dynamicDarkColorScheme(context)
            else -> dynamicLightColorScheme(context)
        }
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
        ThemeMode.Cyan -> CyanScheme
        ThemeMode.Custom -> customScheme(ThemePreferences.customColor.value)
        ThemeMode.Rgb -> {
            val cycle = rememberInfiniteTransition(label = "rgb")
            val hue by cycle.animateFloat(
                initialValue = 0f,
                targetValue = 360f,
                animationSpec = infiniteRepeatable(
                    animation = tween(RgbCycleMillis, easing = LinearEasing),
                    repeatMode = RepeatMode.Restart,
                ),
                label = "rgbHue",
            )
            // Only rebuild when the STEPPED hue changes - see RgbHueStep.
            val step = (hue / RgbHueStep).toInt()
            remember(step) { hueScheme(step * RgbHueStep, 0.62f, 0.92f) }
        }
    }
    MaterialTheme(
        colorScheme = scheme,
        typography = ArmsTypography,
        content = content,
    )
}
