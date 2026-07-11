package com.armsx2.ui.common

import androidx.compose.foundation.border
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.withFrameNanos
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.focus.onFocusChanged
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.unit.dp
import androidx.compose.runtime.LaunchedEffect

/**
 * Shared gamepad-focus helpers for the "plain Compose focus" frontend screens —
 * every library/manager/Save-Load/settings-hub screen that is navigated through
 * Compose's own focus system rather than the [com.armsx2.ui.settings.SettingsControllerNav]
 * registry. The controller router in MainActivityRuntime bridges the physical pad
 * to Compose focus (D-pad → focus move, A → DPAD_CENTER activate, B → back), which
 * already reaches any focusable control (Button / Surface(onClick) / .clickable).
 * These helpers add the two things that bridge can't: a clearly VISIBLE focus ring
 * so the user can see where they are, and INITIAL focus so the first press lands on
 * a sensible control instead of being spent waking the focus system.
 */

private val FocusRingColor = Color(0xFF3DA5FF)

/**
 * Visible focus ring drawn when this element (or a focusable child of it) holds
 * gamepad focus. Place it BEFORE the element's own `.clickable`/`.background` so the
 * ring reads against the fill. Safe on any element that is (or contains) a
 * focusable — for a non-focusable element it simply never lights up.
 */
fun Modifier.padFocusRing(
    shape: Shape = RoundedCornerShape(14.dp),
    width: androidx.compose.ui.unit.Dp = 2.5.dp,
): Modifier = composed {
    var focused by remember { mutableStateOf(false) }
    this
        .onFocusChanged { focused = it.isFocused || it.hasFocus }
        .then(
            if (focused)
                // Driver-safe focus ring: solid inner + translucent outer border. We avoid
                // Modifier.shadow with a custom ambient/spot color — Adreno / Mali / Turnip
                // drivers commonly ignore the tint and render the elevation shadow as an
                // opaque BLACK box (visible only under controller focus, which is what lights
                // this up; touch never does). Borders render identically on every driver.
                Modifier
                    .border(width + 2.dp, FocusRingColor.copy(alpha = 0.30f), shape)
                    .border(width, FocusRingColor, shape)
            else Modifier,
        )
}

/**
 * A [FocusRequester] that grabs focus once its target is attached. Apply the returned
 * requester with `Modifier.focusRequester(...)` to the control that should be focused
 * when the screen opens (usually the first interactive item). Retries across a few
 * frames because the target is not attached on the very first composition. Re-runs
 * when any [keys] change (e.g. a tab switch) so focus follows content swaps.
 */
@Composable
fun rememberInitialFocusRequester(vararg keys: Any?): FocusRequester {
    val requester = remember { FocusRequester() }
    // Single stable key: Unit when the caller passed none (fire once per mount),
    // else the key list (re-fire when it changes). Avoids the zero-vararg
    // LaunchedEffect overload, which is a compile error.
    val effectKey: Any = if (keys.isEmpty()) Unit else keys.toList()
    LaunchedEffect(effectKey) {
        repeat(6) {
            withFrameNanos {}
            if (runCatching { requester.requestFocus() }.isSuccess) return@LaunchedEffect
        }
    }
    return requester
}

/**
 * Convenience: mark this element as the initial-focus target. Equivalent to
 * `Modifier.focusRequester(rememberInitialFocusRequester(...))` but reads cleanly at
 * the call site. Combine with [padFocusRing] for the visible ring.
 */
@Composable
fun Modifier.initialPadFocus(vararg keys: Any?): Modifier =
    this.focusRequester(rememberInitialFocusRequester(*keys))
