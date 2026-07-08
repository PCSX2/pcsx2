package com.armsx2.ui

import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Density
import com.armsx2.Main

/**
 * Global UI scaling for the Compose chrome (game library + in-game overlay + memory
 * card manager) so the interface fits different screen aspect ratios / handheld
 * sizes (21:9, 16:10, 4:3, small handhelds). Border scale drives `dp` (padding,
 * control sizes); font scale drives `sp` (text). Applied ONLY around those UI
 * surfaces via [ScaledUi] — NEVER the game SurfaceView (renders natively at the real
 * pixel size, unaffected) or the on-screen touch controls (positioned per-game in dp
 * and would shift). Default 1.0 = no change, so the stock layout is untouched until
 * the user opts in. Persisted in Main.prefs.
 */
object UiScale {
    private const val KEY_BORDER = "ui.borderScale"
    private const val KEY_FONT = "ui.fontScale"
    const val MIN = 0.80f
    const val MAX = 1.30f
    // Border (overall UI size) overflows the screen past ~104%, so cap it tighter
    // than the font scale (which tolerates more).
    const val BORDER_MAX = 1.04f
    val borderScale = mutableStateOf(1.0f)
    val fontScale = mutableStateOf(1.0f)

    fun load() {
        borderScale.value = Main.prefs.getFloat(KEY_BORDER, 1.0f).coerceIn(MIN, BORDER_MAX)
        fontScale.value = Main.prefs.getFloat(KEY_FONT, 1.0f).coerceIn(MIN, MAX)
    }

    fun setBorderScale(v: Float) {
        val c = v.coerceIn(MIN, BORDER_MAX)
        borderScale.value = c
        Main.prefs.edit().putFloat(KEY_BORDER, c).apply()
    }

    fun setFontScale(v: Float) {
        val c = v.coerceIn(MIN, MAX)
        fontScale.value = c
        Main.prefs.edit().putFloat(KEY_FONT, c).apply()
    }

    /** Reset UI border + font scale to 1.0 (the global "Reset to defaults"). The
     *  mutableStateOf updates drive ScaledUi + the Overlay-tab sliders to recompose. */
    fun resetToDefaults() {
        setBorderScale(1.0f)
        setFontScale(1.0f)
    }
}

/** Wrap [content] so its `dp` + `sp` scale by the user's UI border / font scales.
 *  A no-op (no extra provider) at the default 1.0/1.0 so nothing changes unless the
 *  user adjusts the sliders. Touch within the wrapped UI stays correct — Compose maps
 *  it through the provided density consistently. */
@Composable
fun ScaledUi(content: @Composable () -> Unit) {
    val base = LocalDensity.current
    val b = UiScale.borderScale.value
    val f = UiScale.fontScale.value
    if (b == 1.0f && f == 1.0f) {
        content()
    } else {
        // dp scales by `b` (density). Text px = density*fontScale*sp, so to make the
        // font scale by exactly `f` independent of the border scale, divide it back
        // out: fontScale = base.fontScale * f / b → net text scale = b * (f/b) = f.
        CompositionLocalProvider(
            LocalDensity provides Density(base.density * b, base.fontScale * f / b),
        ) { content() }
    }
}
