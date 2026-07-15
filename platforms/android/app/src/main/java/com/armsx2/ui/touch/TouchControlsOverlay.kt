package com.armsx2.ui.touch

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.draw.scale
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.ColorFilter
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.graphics.painter.Painter
import androidx.compose.ui.input.pointer.PointerInputChange
import androidx.compose.ui.input.pointer.changedToDown
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.zIndex
import com.armsx2.ControllerSkinStore
import com.armsx2.EmuState
import com.armsx2.R
import com.armsx2.i18n.str
import com.armsx2.input.ControllerMappings
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.Colors
import com.armsx2.ui.InGameOverlay
import com.armsx2.ui.WindowImpl
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.CogSolid
import kotlinx.coroutines.delay
import kr.co.iefriends.pcsx2.NativeApp
import kotlin.math.abs
import kotlin.math.hypot
import kotlin.math.min

/** Root entry point. Place this in the same fillMaxSize() container as
 *  the AndroidView surface. Renders nothing when the VM isn't running
 *  or controls are hidden by the controller-mode latch (unless in edit
 *  mode, which forces the buttons visible). */
@Composable
fun TouchControlsOverlay() {
    TouchControls.ensureLoaded()
    // Auto-apply the per-game touch profile when a game boots (serial becomes
    // known). Placed before the early-returns below so it fires regardless of
    // overlay visibility; keyed on the serial so it only re-applies on a change.
    // Prefer the launch-time serial (MainActivityRuntime.currentGame is set in launchGame BEFORE
    // the VM starts) so the per-game profile applies from the first frame, not only
    // after the pause overlay opens (which is what populated InGameOverlay.currentSerial).
    // Re-key on eState so it re-applies on the STOPPED->RUNNING transition.
    val gameSerial = MainActivityRuntime.currentGame.value?.serial?.takeIf { it.isNotEmpty() }
        ?: InGameOverlay.currentSerial.value
    LaunchedEffect(gameSerial, MainActivityRuntime.eState.value) {
        if (MainActivityRuntime.eState.value == EmuState.RUNNING || MainActivityRuntime.eState.value == EmuState.PAUSED)
            TouchControls.applyForSerial(gameSerial)
    }
    // ---- Gyroscope / motion controls -------------------------------------
    // Drive a PS2 analog stick from the device's motion sensors while the game runs.
    // AIM (mode 1) -> RIGHT stick, STEERING (mode 2) -> LEFT stick. The reader emits a
    // normalized (x, y); we split each axis into its pair of native direction keycodes
    // (mirroring applyStickDiff below), so the gyro rides the same pad path as the
    // on-screen sticks. Kept ABOVE the visibility early-returns so it isn't torn down
    // when the pause/library overlay shows. Keyed on eState + the gyro tunables so it
    // re-registers when the game starts/stops or a setting changes, and unregisters
    // (with a 0,0 release) on pause/exit — sensors are battery-hungry and a stuck stick
    // would otherwise linger. setPadButton is mutex-guarded natively; callbacks arrive
    // on the main looper, so no extra sync here.
    val context = LocalContext.current
    val gyro = remember {
        com.armsx2.input.AndroidGyroscopeInput(context) { mode, x, y ->
            // Fold the gyro into the shared analog-merge layer as a SIGNED addend on
            // the physical stick that shares its axis (aim -> right or the user-chosen
            // left for RE4-style games; steer -> left) instead of writing the stick
            // codes raw. That lets coarse stick aim and fine gyro adjustment run at
            // once — before, whichever moved last clobbered the other. The combine
            // (and the P1 physical-stick state it sums with) lives in the runtime.
            MainActivityRuntime.instance?.onGyroAnalog(mode, x, y)
        }
    }
    val gyroMode = ControllerMappings.gyroMode()
    DisposableEffect(MainActivityRuntime.eState.value, gyroMode,
            ControllerMappings.gyroSensitivity(),
            ControllerMappings.gyroSmoothing(),
            ControllerMappings.gyroInvertX(),
            ControllerMappings.gyroInvertY()) {
        if (MainActivityRuntime.eState.value == EmuState.RUNNING && gyroMode != 0) {
            gyro.start(gyroMode,
                ControllerMappings.gyroSensitivity(),
                ControllerMappings.gyroSmoothing(),
                ControllerMappings.gyroInvertX(),
                ControllerMappings.gyroInvertY())
        } else {
            gyro.stop()
        }
        onDispose { gyro.stop() }
    }
    val edit = TouchControls.editMode.value
    val running = MainActivityRuntime.eState.value == EmuState.RUNNING ||
                  MainActivityRuntime.eState.value == EmuState.PAUSED
    // Edit mode renders even with no game running, so the touch-layout editor can be
    // opened from the main-menu Pad settings — not only in-game. (Lines below already
    // let the overlay paint over the library while editing.)
    if (!running && !edit) return
    // Hide while the pause overlay is up so the pause menu owns the screen.
    // In edit mode we ignore overlayVisible — the user enters edit mode
    // from the pause menu, and the overlay closes itself when toggling on.
    if (WindowImpl.overlayVisible.value && !edit) return
    // Same for the game library overlay (Load Game button while a game
    // is running) — it sits on top of the surface, the touch buttons
    // shouldn't paint over the library cards.
    if (WindowImpl.showLibrary.value && !edit) return

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val w = maxWidth
        val h = maxHeight
        val density = LocalDensity.current
        val widthPx = with(density) { w.toPx() }
        val heightPx = with(density) { h.toPx() }
        // Dim the cluttered library/menu behind the editor when it's opened from the
        // main menu (no game running). In-game the paused frame is a fine backdrop, so
        // the scrim is library-mode only.
        if (edit && !running) {
            Box(Modifier.fillMaxSize().background(Color(0xF2101015)))
        }
        LaunchedEffect(widthPx, heightPx) {
            OverlayDims.last = OverlayDims.Dims(widthPx, heightPx)
        }
        val layout = TouchControls.activeLayout.value
        // Buttons the single UnifiedTouchLayer currently holds down (face + shoulder +
        // menu digital buttons). The covered widgets read this to paint their pressed
        // visual — they're render-only when multi-touch is on (inputEnabled=false).
        // Generalizes the old per-region facePressed / lShoulderPressed / rShoulderPressed
        // trio into one published set.
        var unifiedPressed by remember { mutableStateOf<Set<TouchButtonId>>(emptySet()) }

        // Tap-to-reveal settings cog (top-center). Moved off the top-right corner
        // so it no longer sits under the R1/R2 on-screen cluster (the "behind R2"
        // complaint). Kept available even when on-screen controls = Never: it's an
        // INVISIBLE top-center tap zone (no clutter, doesn't overlap R1), so a
        // controller user can hide every gameplay button yet still tap the gear to
        // pause / open settings — no need to map a physical menu button for it.
        if (!edit) {
            var showSettingsCog by remember { mutableStateOf(false) }
            LaunchedEffect(showSettingsCog) {
                if (showSettingsCog) {
                    delay(3000)
                    showSettingsCog = false
                }
            }
            Box(
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    // Above the full-screen UnifiedTouchLayer (glide/multi-touch), which is
                    // composed later and would otherwise sit z-ON-TOP of this tap zone and
                    // swallow the top-center tap. The old per-region touch layers never covered
                    // the top-center, so the cog was always reachable; zIndex restores that.
                    .zIndex(1f)
                    .size(CogTapZoneDp)
                    .clickable(
                        indication = null,
                        interactionSource = remember { MutableInteractionSource() },
                    ) { showSettingsCog = true },
            )
            if (showSettingsCog) {
                InGameSettingsButton(
                    modifier = Modifier
                        .align(Alignment.TopCenter)
                        .zIndex(1f)
                        .padding(14.dp),
                    onClick = {
                        showSettingsCog = false
                        InGameOverlay.open()
                    },
                )
            }
        }

        // Auto-hide timer: in modes 1..10, hide the controls after that many
        // seconds with no touch interaction. Restarts whenever interactionTick
        // changes (screen tap / on-screen button press) and when they reappear.
        val visMode = TouchControls.visibilityMode.intValue
        val tick = TouchControls.interactionTick.intValue
        if (visMode in 1..10 && TouchControls.visible.value && !edit) {
            LaunchedEffect(visMode, tick) {
                delay(visMode * 1000L)
                TouchControls.visible.value = false
            }
        }

        val showPad = edit || (visMode != 0 && TouchControls.visible.value)
        if (!showPad) return@BoxWithConstraints

        if (edit) {
            // Dim backdrop. Two jobs:
            //   1. Consume every pointer change so long-press on empty
            //      space can't leak to the AndroidView and re-open the
            //      pause menu mid-edit.
            //   2. Clear the current widget selection if the user taps
            //      empty space (gesture is a tap when the pointer comes
            //      up without significant motion).
            Box(
                Modifier
                    .fillMaxSize()
                    .background(Color(0xFF000000).copy(alpha = 0.35f))
                    .pointerInput(Unit) {
                        awaitPointerEventScope {
                            while (true) {
                                val downEv = awaitPointerEvent()
                                val firstChange = downEv.changes.firstOrNull { it.pressed }
                                downEv.changes.forEach { it.consume() }
                                if (firstChange == null) continue
                                val downPos = firstChange.position
                                var moved = false
                                // Track until the pointer comes up.
                                while (true) {
                                    val ev = awaitPointerEvent()
                                    ev.changes.forEach { it.consume() }
                                    val c = ev.changes.firstOrNull { it.id == firstChange.id }
                                        ?: break
                                    val dx = c.position.x - downPos.x
                                    val dy = c.position.y - downPos.y
                                    if (dx * dx + dy * dy > 64f) moved = true
                                    if (!c.pressed) break
                                }
                                if (!moved) {
                                    TouchControls.selectedButton.value = null
                                }
                            }
                        }
                    },
            )
        }

        val faceMulti = !edit && TouchControls.faceMultiTouch.value
        if (!faceMulti) {
            if (unifiedPressed.isNotEmpty()) unifiedPressed = emptySet()
        }
        // Buttons currently held via the unified multi-touch hit-test layer.
        val multiPressed = unifiedPressed

        // ONE full-screen hit-test layer for every glide-eligible digital control
        // (FACE + SHOULDER + MENU, enabled && !tapToHold). Placed FIRST in
        // composition so it sits z-BELOW the visual widgets: the d-pad, sticks and
        // tap/long-press widgets (rendered by the loop below) are composed ABOVE it
        // and claim their own DOWN first, so a finger that starts on them never
        // becomes ours (see UnifiedTouchLayer's foreign-DOWN guard). Owning the
        // whole screen is what lets a finger glide ACROSS widget boundaries (e.g.
        // Cross -> empty space -> L1), which the old per-region layers couldn't do.
        if (faceMulti) {
            UnifiedTouchLayer(
                layout = layout,
                widthPx = widthPx,
                heightPx = heightPx,
                glide = TouchControls.touchGliding.value,
                onPressedChange = { unifiedPressed = it },
            )
        }
        for (cfg in layout.buttons) {
            if (!cfg.enabled && !edit) continue
            val size = cfg.sizeDp.dp
            val cx = w * cfg.xFrac
            val cy = h * cfg.yFrac
            val left = cx - size / 2
            val top = cy - size / 2
            Box(
                modifier = Modifier
                    .offset(x = left, y = top)
                    .size(size)
                    .alpha(if (edit && !cfg.enabled) 0.4f else 1f),
            ) {
                when (cfg.id.kind) {
                    TouchButtonId.Kind.DPAD -> DpadWidget(cfg, edit)
                    TouchButtonId.Kind.STICK -> StickWidget(cfg, edit)
                    TouchButtonId.Kind.PAUSE -> PauseWidget(cfg, edit)
                    TouchButtonId.Kind.PRESSURE -> PressureButtonWidget(cfg, edit)
                    TouchButtonId.Kind.FASTFORWARD -> FastForwardWidget(cfg, edit)
                    TouchButtonId.Kind.MACRO -> MacroWidget(cfg, edit)
                    TouchButtonId.Kind.STATEACTION -> StateActionWidget(cfg, edit)
	                    else -> ButtonWidget(
	                        cfg = cfg,
	                        edit = edit,
	                        // Latched (tap-to-hold) buttons stay on their own pressGestures
	                        // handler even with multi-touch on, so the latch logic runs
	                        // (the shared layer has no latch); the changedToDown fix keeps
	                        // them multi-touch-correct anyway.
	                        inputEnabled = !(faceMulti && isMultiTouchKind(cfg.id.kind) && !cfg.tapToHold),
	                        forcedPressed = faceMulti && cfg.id in multiPressed,
	                    )
                }
                if (edit && !cfg.enabled) DisabledMarker()
            }
        }

        if (edit) {
            EditToolbar(
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 12.dp),
            )
        }

        if (TouchControls.profileDialogOpen.value) {
            ProfilePicker(onDismiss = { TouchControls.profileDialogOpen.value = false })
        }
    }
}

/* -------------------------------------------------------------------- */
/*  In-game settings shortcut                                            */
/* -------------------------------------------------------------------- */

@Composable
private fun InGameSettingsButton(modifier: Modifier = Modifier, onClick: () -> Unit) {
    Box(
        modifier = modifier
            .size(52.dp)
            .clip(CircleShape)
            .background(Color(0xFF111111).copy(alpha = 0.55f))
            .border(1.dp, Color.White.copy(alpha = 0.20f), CircleShape)
            .clickable { onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Icon(
            imageVector = LineAwesomeIcons.CogSolid,
            contentDescription = str("touch.settingsButton.description"),
            tint = Color.White.copy(alpha = 0.92f),
            modifier = Modifier.size(32.dp),
        )
    }
}

/* -------------------------------------------------------------------- */
/*  Digital button widget                                                */
/* -------------------------------------------------------------------- */

@Composable
private fun ButtonWidget(
    cfg: TouchButtonCfg,
    edit: Boolean,
    inputEnabled: Boolean = true,
    forcedPressed: Boolean = false,
) {
    var localPressed by remember(cfg.id) { mutableStateOf(false) }
    val pressed = forcedPressed || localPressed
    val opacity = TouchControls.opacity.floatValue
    val mod = Modifier
        .fillMaxSize()
        .let {
            if (edit) it.editGestures(cfg)
            else if (inputEnabled) it.pressGestures(cfg.id.keycode, cfg.tapToHold) { p -> localPressed = p }
            else it
        }
    // Pressed feedback: every button shrinks a hair AND darkens.
    // - Buttons with a pressed sprite (most) get the darken via the
    //   already-darker PNG swap.
    // - CIRCLE / SQUARE don't ship pressed sprites, so they get the
    //   darken via ColorFilter.tint + BlendMode.Modulate (multiplies
    //   each pixel by the mid-gray tint).
    // Both groups share the same scale-down so the shrink-on-press
    // feel is consistent.
    val darkenOnPress = pressed && !hasPressedSprite(cfg.id)
    Box(modifier = mod, contentAlignment = Alignment.Center) {
        Image(
            painter = skinPainter(skinKeyFor(cfg.id)) ?: painterResource(drawableFor(cfg.id, pressed)),
            contentDescription = cfg.id.label,
            contentScale = ContentScale.Fit,
            alpha = opacity,
            colorFilter = if (darkenOnPress)
                ColorFilter.tint(Color(0xFFB0B0B0), BlendMode.Modulate)
            else null,
            modifier = Modifier
                .fillMaxSize()
                // CIRCLE / SQUARE shrink more (0.85) because they lack
                // a pressed PNG — the rest get a smaller 0.92 nudge on
                // top of the pressed-sprite swap, which itself depicts
                // a slightly inset button.
                .scale(
                    when {
                        !pressed -> 1f
                        hasPressedSprite(cfg.id) -> 0.92f
                        else -> 0.85f
                    }
                ),
        )
        if (edit) EditAdornment(cfg.id)
    }
}

/** True if the asset ships a separate pressed variant. CIRCLE and
 *  SQUARE only have the resting sprite. */
private fun hasPressedSprite(id: TouchButtonId): Boolean = when (id) {
    TouchButtonId.CIRCLE, TouchButtonId.SQUARE -> false
    else -> true
}

/** Map a button id + press state to the bundled PNG. CIRCLE / SQUARE
 *  ship without a separate pressed sprite, so they reuse their default
 *  for both states. */
/** Active custom-skin painter for a logical [key] (e.g. "cross", "up",
 *  "analog_base"), or null to fall back to the built-in drawable. Decode is cached
 *  in [ControllerSkinStore]; skins have no pressed variant so [pressed] is ignored
 *  for the override (the built-in fallback keeps its pressed art). */
@Composable
private fun skinPainter(key: String?): Painter? {
    if (key == null) return null
    val active = ControllerSkinStore.activeSkinId.value ?: return null
    val ctx = LocalContext.current
    val bmp = remember(active, key) { ControllerSkinStore.bitmapForKey(ctx, key) } ?: return null
    return remember(bmp) { BitmapPainter(bmp) }
}

/** Logical skin key for a button, or null for buttons with no skin slot. */
private fun skinKeyFor(id: TouchButtonId): String? = when (id) {
    TouchButtonId.CROSS -> "cross"
    TouchButtonId.CIRCLE -> "circle"
    TouchButtonId.SQUARE -> "square"
    TouchButtonId.TRIANGLE -> "triangle"
    TouchButtonId.L1 -> "l1"
    TouchButtonId.L2 -> "l2"
    TouchButtonId.L3 -> "l3"
    TouchButtonId.R1 -> "r1"
    TouchButtonId.R2 -> "r2"
    TouchButtonId.R3 -> "r3"
    TouchButtonId.START -> "start"
    TouchButtonId.SELECT -> "select"
    else -> null
}

private fun drawableFor(id: TouchButtonId, pressed: Boolean): Int = when (id) {
    TouchButtonId.CROSS    -> if (pressed) R.drawable.pad_cross_pressed    else R.drawable.pad_cross
    TouchButtonId.CIRCLE   -> R.drawable.pad_circle
    TouchButtonId.SQUARE   -> R.drawable.pad_square
    TouchButtonId.TRIANGLE -> if (pressed) R.drawable.pad_triangle_pressed else R.drawable.pad_triangle
    TouchButtonId.L1       -> if (pressed) R.drawable.pad_l1_pressed       else R.drawable.pad_l1
    TouchButtonId.L2       -> if (pressed) R.drawable.pad_l2_pressed       else R.drawable.pad_l2
    TouchButtonId.R1       -> if (pressed) R.drawable.pad_r1_pressed       else R.drawable.pad_r1
    TouchButtonId.R2       -> if (pressed) R.drawable.pad_r2_pressed       else R.drawable.pad_r2
    TouchButtonId.START    -> if (pressed) R.drawable.pad_start_pressed    else R.drawable.pad_start
    TouchButtonId.SELECT   -> if (pressed) R.drawable.pad_select_pressed   else R.drawable.pad_select
    TouchButtonId.L3       -> if (pressed) R.drawable.pad_l3_pressed       else R.drawable.pad_l3
    TouchButtonId.R3       -> if (pressed) R.drawable.pad_r3_pressed       else R.drawable.pad_r3
    // DPad / sticks render their own composed sprites; PAUSE / FAST_FORWARD / macros render their own.
    TouchButtonId.DPAD, TouchButtonId.L_STICK, TouchButtonId.R_STICK,
    TouchButtonId.PAUSE, TouchButtonId.PRESSURE, TouchButtonId.FAST_FORWARD,
    TouchButtonId.MACRO1, TouchButtonId.MACRO2, TouchButtonId.MACRO3, TouchButtonId.MACRO4,
    TouchButtonId.SAVE_STATE, TouchButtonId.LOAD_STATE -> R.drawable.pad_cross
}

/** Pressure-sensitivity modifier button. Emits no PS2 keycode; while held it
 *  sets TouchControls.pressureModifierHeld so pressure-capable buttons report a
 *  soft (~50%) press. Tints blue while held. */
@Composable
private fun PressureButtonWidget(cfg: TouchButtonCfg, edit: Boolean) {
    val held = TouchControls.pressureModifierHeld.value
    val opacity = TouchControls.opacity.floatValue
    val mod = Modifier
        .fillMaxSize()
        .let { it ->
            if (edit) it.editGestures(cfg)
            else it.pointerInput(cfg.id) {
                awaitPointerEventScope {
                    while (true) {
                        val ev = awaitPointerEvent()
                        val change = ev.changes.firstOrNull() ?: continue
                        if (!change.pressed) continue
                        TouchControls.pressureModifierHeld.value = true
                        TouchControls.noteTouchInteraction()
                        while (true) {
                            val next = awaitPointerEvent()
                            val nc = next.changes.firstOrNull { it.id == change.id }
                            if (nc == null || !nc.pressed) break
                        }
                        TouchControls.pressureModifierHeld.value = false
                    }
                }
            }
        }
    Box(modifier = mod, contentAlignment = Alignment.Center) {
        Box(
            Modifier
                .fillMaxSize()
                .clip(CircleShape)
                .background(Color(if (held) 0xFF3A6EA5 else 0xFF1A1A1A).copy(alpha = opacity))
                .border(1.dp, Color.White.copy(alpha = 0.35f * opacity), CircleShape),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                "P½",
                color = Color.White.copy(alpha = opacity),
                fontSize = 13.sp,
                fontWeight = FontWeight.Bold,
            )
        }
        if (edit) EditAdornment(cfg.id)
    }
}

/** Size of the invisible top-center tap zone that reveals the settings cog (see
 *  TouchControlsOverlay). Shared so UnifiedTouchLayer can carve the exact same rect out
 *  of its hit region, guaranteeing a top-center tap reaches the cog, not the glide layer. */
private val CogTapZoneDp = 74.dp

@Composable
private fun UnifiedTouchLayer(
    layout: TouchLayout,
    widthPx: Float,
    heightPx: Float,
    glide: Boolean = false,
    onPressedChange: (Set<TouchButtonId>) -> Unit,
) {
    if (widthPx <= 0f || heightPx <= 0f) return

    // Glide-eligible digital controls: FACE + SHOULDER + MENU, enabled and NOT
    // tap-to-hold (latched buttons keep their own pressGestures handler so the
    // latch logic runs — the shared layer has no latch). Same rect+circle hit math
    // as the old per-region FaceMultiTouchLayer: center + radius = sizePx * 0.62.
    val hitButtons = layout.buttons.filter {
        it.enabled && isMultiTouchKind(it.id.kind) && !it.tapToHold
    }
    // Foreign regions: DPAD / STICK / tap+long-press kinds (PAUSE / FASTFORWARD /
    // MACRO / STATEACTION / PRESSURE) and any tap-to-hold digital button. Those
    // widgets are composed ABOVE this layer and consume their own DOWN, so a finger
    // that starts on them normally never reaches us — but we ALSO reject a fresh
    // DOWN whose position lands inside one of these rects (defensive, in case the
    // above-layer consume ever races), so a tap on the stick / Pause can't leak
    // into a face button.
    val foreignRects = layout.buttons.filter { cfg ->
        cfg.enabled && (!isMultiTouchKind(cfg.id.kind) || cfg.tapToHold)
    }

    val density = LocalDensity.current
    val buttonRects = hitButtons.map { cfg ->
        val sizePx = with(density) { cfg.sizeDp.dp.toPx() }
        val cx = widthPx * cfg.xFrac
        val cy = heightPx * cfg.yFrac
        UnifiedHit(id = cfg.id, cx = cx, cy = cy, radius = sizePx * TouchControls.multiTouchRadius.floatValue)
    }
    val cogPx = with(density) { CogTapZoneDp.toPx() }
    val foreignBounds = foreignRects.map { cfg ->
        val sizePx = with(density) { cfg.sizeDp.dp.toPx() }
        val cx = widthPx * cfg.xFrac
        val cy = heightPx * cfg.yFrac
        UnifiedRect(
            left = cx - sizePx / 2f,
            top = cy - sizePx / 2f,
            right = cx + sizePx / 2f,
            bottom = cy + sizePx / 2f,
        )
    } + UnifiedRect(
        // Carve out the top-center settings-cog tap zone (align(TopCenter).size(CogTapZoneDp) in
        // TouchControlsOverlay) so a DOWN there is treated as foreign — the glide layer never owns
        // or consumes it, so it reaches the cog's clickable. This is the load-bearing guard:
        // Compose hit-tests overlapping siblings exclusively (they don't share a pointer by
        // default), so zIndex on the cog only reorders WHICH sibling wins; this geometric carve-out
        // makes the glide layer stand down regardless of z-order. No glide button sits at
        // dead-top-center, so it costs nothing.
        left = widthPx / 2f - cogPx / 2f,
        top = 0f,
        right = widthPx / 2f + cogPx / 2f,
        bottom = cogPx,
    )
    val dims = widthPx to heightPx

    Box(
        modifier = Modifier
            .fillMaxSize()
            .pointerInput(buttonRects, foreignBounds, dims, glide) {
                var pressed = emptySet<TouchButtonId>()
                fun updatePressed(next: Set<TouchButtonId>) {
                    if (pressed == next) return
                    pressed = next
                    onPressedChange(next)
                }
                // Full-screen: positions are already in global (layer) coordinates.
                fun hits(pos: Offset): Set<TouchButtonId> =
                    buttonRects
                        .filter { hit ->
                            val dx = pos.x - hit.cx
                            val dy = pos.y - hit.cy
                            dx * dx + dy * dy <= hit.radius * hit.radius
                        }
                        .map { it.id }
                        .toSet()
                fun inForeignRegion(pos: Offset): Boolean =
                    foreignBounds.any {
                        pos.x >= it.left && pos.x <= it.right &&
                        pos.y >= it.top && pos.y <= it.bottom
                    }
                fun releaseAll() {
                    pressed.forEach { sendDigital(it.keycode, false) }
                    updatePressed(emptySet())
                }
                awaitPointerEventScope {
                    // Per-finger state:
                    //   latched  — every button this finger has crossed since DOWN
                    //              (glide mode; held until lift).
                    //   current  — the button(s) directly under this finger right now
                    //              (non-glide mode; released on leave).
                    //   foreign  — fingers whose DOWN was consumed by a widget above
                    //              us or landed inside a d-pad/stick/tap-hold region;
                    //              never contribute to the aggregate.
                    val latched = mutableMapOf<androidx.compose.ui.input.pointer.PointerId, MutableSet<TouchButtonId>>()
                    val current = mutableMapOf<androidx.compose.ui.input.pointer.PointerId, Set<TouchButtonId>>()
                    val foreign = mutableSetOf<androidx.compose.ui.input.pointer.PointerId>()
                    try {
                        while (true) {
                            val ev = awaitPointerEvent()
                            for (ch in ev.changes) {
                                // A fresh DOWN decides ownership of this finger once.
                                if (ch.changedToDown()) {
                                    if (ch.isConsumed || inForeignRegion(ch.position)) {
                                        foreign.add(ch.id)
                                    } else {
                                        foreign.remove(ch.id)
                                    }
                                }
                                if (!ch.pressed) {
                                    // Lift / cancel: drop this finger's state entirely.
                                    latched.remove(ch.id)
                                    current.remove(ch.id)
                                    foreign.remove(ch.id)
                                    continue
                                }
                                if (ch.id in foreign) continue
                                val h = hits(ch.position)
                                if (glide) {
                                    latched.getOrPut(ch.id) { mutableSetOf() }.addAll(h)
                                } else {
                                    current[ch.id] = h
                                }
                            }
                            val agg = if (glide) {
                                latched.values.flatten().toSet()
                            } else {
                                current.values.flatten().toSet()
                            }
                            (pressed - agg).forEach { sendDigital(it.keycode, false) }
                            (agg - pressed).forEach { sendDigital(it.keycode, true) }
                            // Per-finger consume: only claim changes for fingers WE own
                            // (mapped to >=1 control). Never blanket-consume the whole
                            // event — that would starve co-occurring gestures like the
                            // Pause long-press on a finger we don't own.
                            for (ch in ev.changes) {
                                if (ch.id in foreign) continue
                                val owns = if (glide)
                                    !latched[ch.id].isNullOrEmpty()
                                else
                                    !current[ch.id].isNullOrEmpty()
                                if (owns) ch.consume()
                            }
                            updatePressed(agg)
                        }
                    } finally {
                        releaseAll()
                    }
                }
            },
    )
}

private data class UnifiedHit(
    val id: TouchButtonId,
    val cx: Float,
    val cy: Float,
    val radius: Float,
)

private data class UnifiedRect(
    val left: Float,
    val top: Float,
    val right: Float,
    val bottom: Float,
)

/* -------------------------------------------------------------------- */
/*  Pause hotspot — invisible long-press zone that opens the overlay    */
/* -------------------------------------------------------------------- */

/** Invisible in play mode; long-press opens the in-game pause overlay.
 *  Replaced the old long-press-anywhere surface gesture (see MainActivityRuntime.kt),
 *  which paused on accidental presses in empty screen space. The default
 *  spot sits between the DPad and the face-button diamond; in edit mode
 *  it renders an outlined "PAUSE" box so it can be dragged/resized like
 *  any other widget. */
@Composable
private fun PauseWidget(cfg: TouchButtonCfg, edit: Boolean) {
    if (edit) {
        Box(
            modifier = Modifier.fillMaxSize().editGestures(cfg),
            contentAlignment = Alignment.Center,
        ) {
            EditAdornment(cfg.id)
            Text(
                str("touch.pause.editLabel"),
                color = Color.White.copy(alpha = 0.75f),
                fontSize = 12.sp,
                fontWeight = FontWeight.SemiBold,
            )
        }
    } else {
        Box(
            modifier = Modifier
                .fillMaxSize()
                .pointerInput(cfg.id) {
                    detectTapGestures(
                        onLongPress = { InGameOverlay.open() },
                    )
                },
        )
    }
}

/** On-screen fast-forward (Turbo) toggle. Edit mode renders an outlined "▶▶" box
 *  so it can be dragged/resized like any widget; in play mode a tap calls
 *  MainActivityRuntime.toggleFastForward() — the same action as the FAST_FORWARD_TOGGLE hotkey.
 *  Opt-in (disabled in the default layout). */
@Composable
private fun FastForwardWidget(cfg: TouchButtonCfg, edit: Boolean) {
    if (edit) {
        Box(
            modifier = Modifier.fillMaxSize().editGestures(cfg),
            contentAlignment = Alignment.Center,
        ) {
            EditAdornment(cfg.id)
            Text("▶▶", color = Color.White.copy(alpha = 0.75f), fontSize = 18.sp, fontWeight = FontWeight.Bold)
        }
    } else {
        val opacity = TouchControls.opacity.floatValue
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.30f * opacity))
                .pointerInput(cfg.id) {
                    detectTapGestures(onTap = { MainActivityRuntime.instance?.toggleFastForward() })
                },
            contentAlignment = Alignment.Center,
        ) {
            Text(
                "▶▶",
                color = Color.White.copy(alpha = opacity.coerceIn(0.35f, 1f)),
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

/** Press gesture that fires a SET of pad keycodes at once (down on press, up on
 *  release) — the macro/combo dispatch. Mirrors pressGestures but multi-keycode. */
private fun Modifier.macroPressGestures(keycodes: List<Int>) =
    pointerInput(keycodes) {
        awaitPointerEventScope {
            while (true) {
                val ev = awaitPointerEvent()
                val change = ev.changes.firstOrNull() ?: continue
                if (!change.pressed) continue
                keycodes.forEach { sendDigital(it, true) }
                TouchControls.noteTouchInteraction()
                while (true) {
                    val next = awaitPointerEvent()
                    val nc = next.changes.firstOrNull { it.id == change.id }
                    if (nc == null || !nc.pressed) break
                }
                keycodes.forEach { sendDigital(it, false) }
            }
        }
    }

/** Macro / combo button. Edit mode renders an outlined "M#" box; in play mode a
 *  press fires every pad button configured for this macro (TouchControls.macroButtons)
 *  and releases them on lift. Opt-in (disabled in the default layout); configure the
 *  button set in Pad settings → Touch Macros. An unconfigured macro is a no-op. */
@Composable
private fun MacroWidget(cfg: TouchButtonCfg, edit: Boolean) {
    if (edit) {
        Box(
            modifier = Modifier.fillMaxSize().editGestures(cfg),
            contentAlignment = Alignment.Center,
        ) {
            EditAdornment(cfg.id)
            Text(cfg.id.label, color = Color.White.copy(alpha = 0.75f), fontSize = 16.sp, fontWeight = FontWeight.Bold)
        }
    } else {
        val opacity = TouchControls.opacity.floatValue
        val keycodes = remember(cfg.id, TouchControls.macroBindTick.intValue) {
            TouchControls.macroButtons(cfg.id).map { it.keycode }
        }
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.30f * opacity))
                .macroPressGestures(keycodes),
            contentAlignment = Alignment.Center,
        ) {
            Text(
                cfg.id.label,
                color = Color.White.copy(alpha = opacity.coerceIn(0.35f, 1f)),
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

/** On-screen Save-State / Load-State button. Edit mode renders an outlined
 *  "SAVE"/"LOAD" box; in play mode a tap opens the pause overlay's slot picker so the
 *  user chooses which slot to save to / load from (the physical SAVE_STATE/LOAD_STATE
 *  hotkeys remain quick-to-current-slot). Opt-in (disabled in the default layout). */
@Composable
private fun StateActionWidget(cfg: TouchButtonCfg, edit: Boolean) {
    val label = if (cfg.id == TouchButtonId.SAVE_STATE) str("touch.stateAction.save") else str("touch.stateAction.load")
    if (edit) {
        Box(
            modifier = Modifier.fillMaxSize().editGestures(cfg),
            contentAlignment = Alignment.Center,
        ) {
            EditAdornment(cfg.id)
            Text(label, color = Color.White.copy(alpha = 0.75f), fontSize = 12.sp, fontWeight = FontWeight.Bold)
        }
    } else {
        val opacity = TouchControls.opacity.floatValue
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(CircleShape)
                .background(Color.Black.copy(alpha = 0.30f * opacity))
                .pointerInput(cfg.id) {
                    detectTapGestures(onTap = {
                        if (cfg.id == TouchButtonId.SAVE_STATE) InGameOverlay.openSaveStatePicker()
                        else InGameOverlay.openLoadStatePicker()
                    })
                },
            contentAlignment = Alignment.Center,
        ) {
            Text(
                label,
                color = Color.White.copy(alpha = opacity.coerceIn(0.35f, 1f)),
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

/* -------------------------------------------------------------------- */
/*  DPad widget — single 4-way pad emitting up/down/left/right          */
/* -------------------------------------------------------------------- */

@Composable
private fun DpadWidget(cfg: TouchButtonCfg, edit: Boolean) {
    val active = remember(cfg.id) { mutableStateOf(DpadState()) }
    val opacity = TouchControls.opacity.floatValue

    val pressMod: Modifier = if (edit) {
        Modifier.editGestures(cfg)
    } else {
        Modifier.pointerInput(cfg.id) {
            awaitPointerEventScope {
                while (true) {
                    val ev = awaitPointerEvent()
                    val change = ev.changes.firstOrNull() ?: continue
                    if (!change.pressed) {
                        if (active.value.any()) {
                            releaseDpad(active.value)
                            active.value = DpadState()
                        }
                        continue
                    }
                    val pos = change.position
                    val cx = size.width / 2f
                    val cy = size.height / 2f
                    val dx = pos.x - cx
                    val dy = pos.y - cy
                    // Dead-center grows with the key spacing so the empty middle gap
                    // between the spread-apart arms registers nothing (a small base gap
                    // is always present to avoid center-jitter at spacing 0).
                    val deadR = min(cx, cy) * (0.08f + TouchControls.dpadSpacing.floatValue)
                    val r = hypot(dx, dy)
                    // 8-way with cardinal-biased sectors: the minor axis
                    // only fires when its magnitude is at least
                    // `diagBias` of the major axis. With diagBias=0.55
                    // that's an angle within ~29° of 45° — a ~58° wedge
                    // around each diagonal; everything outside snaps to
                    // the dominant cardinal so a slightly-angled press
                    // doesn't fire two axes by accident.
                    val target = if (r < deadR) DpadState() else {
                        val absDx = abs(dx)
                        val absDy = abs(dy)
                        val diagBias = 0.55f
                        val keepX = absDx >= absDy * diagBias
                        val keepY = absDy >= absDx * diagBias
                        DpadState(
                            up    = keepY && dy < 0f,
                            down  = keepY && dy > 0f,
                            left  = keepX && dx < 0f,
                            right = keepX && dx > 0f,
                        )
                    }
                    if (target != active.value) {
                        applyDpadDiff(active.value, target)
                        active.value = target
                    }
                }
            }
        }
    }

    // Up / Left / Right ship as bundled sprites — each is the arm of
    // the DPad pointing inward to the center. Down reuses the up sprite
    // rotated 180° (so the asset author only had to ship 3 arms).
    // Each arm fills ~half the DPad's width or height and aligns to its
    // edge so the four arms compose into a + shape.
    // Aspect ratios from the tight-cropped sprite dimensions (see
    // crop_labels.py). All sprites are now tight-cropped so the
    // arm-to-canvas ratio is consistent across U/D/L/R — without this
    // the un-labelled up sprite rendered ~half size next to its
    // label-stripped L/R siblings.
    val upRatio    = 45f / 52f
    val lrRatio    = 53f / 44f
    // D-pad key spacing (NetherSX2-style): each arm normally fills half the pad and meets
    // at center; subtracting the gap pulls it back toward its edge, opening a middle gap so
    // the four directions read as spaced-apart keys. Clamped so an arm never vanishes.
    val dpadGap = TouchControls.dpadSpacing.floatValue.coerceIn(0f, 0.35f)
    val armFill = 0.5f - dpadGap
    Box(
        modifier = Modifier.fillMaxSize().then(pressMod),
    ) {
        // Custom-skin arm overrides (null = built-in). Computed once and reused for
        // the painter and to drop the built-in down-arm 180° rotation — a skin's
        // "down" image is already oriented correctly.
        val skUp = skinPainter("up")
        val skDown = skinPainter("down")
        val skLeft = skinPainter("left")
        val skRight = skinPainter("right")
        // Only the UP arm needs a nudge — the tight crop trimmed some
        // AA off the outer flat edge so without this it reads "pushed
        // inwards" toward the center. The down arm uses the same
        // sprite rotated 180° and sat correctly already.
        Image(
            painter = skUp ?: painterResource(
                if (active.value.up) R.drawable.pad_dpad_up_pressed else R.drawable.pad_dpad_up
            ),
            contentDescription = str("touch.dpad.up.description"),
            contentScale = ContentScale.Fit,
            alpha = opacity,
            modifier = Modifier
                .align(Alignment.TopCenter)
                .offset(y = (-4).dp)
                .fillMaxHeight(armFill)
                .aspectRatio(upRatio),
        )
        Image(
            painter = skDown ?: painterResource(
                if (active.value.down) R.drawable.pad_dpad_up_pressed else R.drawable.pad_dpad_up
            ),
            contentDescription = str("touch.dpad.down.description"),
            contentScale = ContentScale.Fit,
            alpha = opacity,
            modifier = Modifier
                .align(Alignment.BottomCenter)
                .fillMaxHeight(armFill)
                .aspectRatio(upRatio)
                .rotate(if (skDown != null) 0f else 180f),
        )
        Image(
            painter = skLeft ?: painterResource(
                if (active.value.left) R.drawable.pad_dpad_left_pressed else R.drawable.pad_dpad_left
            ),
            contentDescription = str("touch.dpad.left.description"),
            contentScale = ContentScale.Fit,
            alpha = opacity,
            modifier = Modifier
                .align(Alignment.CenterStart)
                .fillMaxWidth(armFill)
                .aspectRatio(lrRatio),
        )
        Image(
            painter = skRight ?: painterResource(
                if (active.value.right) R.drawable.pad_dpad_right_pressed else R.drawable.pad_dpad_right
            ),
            contentDescription = str("touch.dpad.right.description"),
            contentScale = ContentScale.Fit,
            alpha = opacity,
            modifier = Modifier
                .align(Alignment.CenterEnd)
                .fillMaxWidth(armFill)
                .aspectRatio(lrRatio),
        )
        if (edit) EditAdornment(cfg.id)
    }
}

private data class DpadState(
    val up: Boolean = false,
    val down: Boolean = false,
    val left: Boolean = false,
    val right: Boolean = false,
) {
    fun any() = up || down || left || right
}

private fun applyDpadDiff(prev: DpadState, next: DpadState) {
    if (prev.up    != next.up)    sendDigital(19, next.up)
    if (prev.down  != next.down)  sendDigital(20, next.down)
    if (prev.left  != next.left)  sendDigital(21, next.left)
    if (prev.right != next.right) sendDigital(22, next.right)
}

private fun releaseDpad(state: DpadState) {
    if (state.up)    sendDigital(19, false)
    if (state.down)  sendDigital(20, false)
    if (state.left)  sendDigital(21, false)
    if (state.right) sendDigital(22, false)
}

/* -------------------------------------------------------------------- */
/*  Analog stick widget                                                  */
/* -------------------------------------------------------------------- */

@Composable
private fun StickWidget(cfg: TouchButtonCfg, edit: Boolean) {
    val thumb = remember(cfg.id) { mutableStateOf(Offset.Zero) }
    // Floating-stick state: captured touch-down origin (null = released) and the
    // visual shift of the ring + thumb from the widget center to that origin.
    val origin = remember(cfg.id) { mutableStateOf<Offset?>(null) }
    val baseShift = remember(cfg.id) { mutableStateOf(Offset.Zero) }
    val lastEmit = remember(cfg.id) { mutableStateOf(StickEmit()) }
    val opacity = TouchControls.opacity.floatValue
    val density = LocalDensity.current

    // See MainActivityRuntime.dispatchGenericMotionEvent for the L / R axis mappings —
    // posCode / negCode per axis.
    val codes = when (cfg.id) {
        TouchButtonId.L_STICK -> StickCodes(xPos = 111, xNeg = 113, yPos = 112, yNeg = 110)
        else -> StickCodes(xPos = 121, xNeg = 123, yPos = 122, yNeg = 120)
    }

    val pressMod: Modifier = if (edit) {
        Modifier.editGestures(cfg)
    } else {
        Modifier.pointerInput(cfg.id) {
            val radiusPx = with(density) { (cfg.sizeDp / 2f).dp.toPx() }
            // Visual thumb caps inside the ring; force is normalized against the
            // same cap. Hoisted so the floating-origin clamp can reuse it.
            val capPx = radiusPx * 0.66f
            awaitPointerEventScope {
                // Lock the gesture onto the pointer that started it. Tracking
                // ev.changes.firstOrNull() instead would let a SECOND finger
                // landing in/lifting from the stick trigger a spurious recenter
                // or release mid-gesture (worse with the floating origin, which
                // would then re-capture at the surviving finger's position).
                var activeId: androidx.compose.ui.input.pointer.PointerId? = null
                while (true) {
                    val ev = awaitPointerEvent()
                    val tracked = if (activeId == null)
                        ev.changes.firstOrNull { it.pressed }
                    else
                        ev.changes.firstOrNull { it.id == activeId }
                    // Release: our tracked pointer lifted or is gone.
                    if (tracked == null || !tracked.pressed) {
                        if (activeId != null) {
                            thumb.value = Offset.Zero
                            origin.value = null
                            baseShift.value = Offset.Zero
                            activeId = null
                            if (lastEmit.value.any()) {
                                releaseStick(codes, lastEmit.value)
                                lastEmit.value = StickEmit()
                            }
                        }
                        continue
                    }
                    // Start of gesture: lock onto this pointer's id.
                    if (activeId == null) activeId = tracked.id
                    val cxLocal = size.width / 2f
                    val cyLocal = size.height / 2f
                    // Floating stick: the FIRST touch-down point of a gesture becomes
                    // the origin (ring re-centers under the finger); fixed center when
                    // off. Snap-back on release is unchanged either way.
                    if (origin.value == null) {
                        if (TouchControls.floatingStick.value) {
                            // Clamp the captured origin so the cap circle (and the
                            // visible ring) stays fully within the widget, keeping
                            // full deflection reachable in every direction.
                            val hiX = (size.width - capPx).coerceAtLeast(capPx)
                            val hiY = (size.height - capPx).coerceAtLeast(capPx)
                            val ox = tracked.position.x.coerceIn(capPx, hiX)
                            val oy = tracked.position.y.coerceIn(capPx, hiY)
                            origin.value = Offset(ox, oy)
                            baseShift.value = Offset(ox - cxLocal, oy - cyLocal)
                        } else {
                            origin.value = Offset(cxLocal, cyLocal)
                            baseShift.value = Offset.Zero
                        }
                    }
                    val o = origin.value!!
                    val dx = tracked.position.x - o.x
                    val dy = tracked.position.y - o.y
                    val r = hypot(dx, dy)
                    val scale = if (r > capPx) capPx / r else 1f
                    val capDx = dx * scale
                    val capDy = dy * scale
                    thumb.value = Offset(capDx, capDy)
                    var nx = (capDx / capPx).coerceIn(-1f, 1f)
                    var ny = (capDy / capPx).coerceIn(-1f, 1f)
                    // Honor the per-stick axis correction (swap first, then inverts)
                    // exactly like the physical-pad path (MainActivityRuntime.dispatchStick) — the
                    // tester's "Right Stick Invert Y works on a gamepad but the touch
                    // stick is still upside-down".
                    val leftStick = cfg.id == TouchButtonId.L_STICK
                    if (ControllerMappings.stickSwapXY(leftStick)) { val t = nx; nx = ny; ny = t }
                    if (ControllerMappings.stickInvertX(leftStick)) nx = -nx
                    if (ControllerMappings.stickInvertY(leftStick)) ny = -ny
                    val emit = computeStickEmit(nx, ny, leftStick)
                    if (emit != lastEmit.value) {
                        applyStickDiff(codes, lastEmit.value, emit)
                        lastEmit.value = emit
                    }
                }
            }
        }
    }

    // Textured base + thumb. The base PNG paints the static ring; the
    // thumb is `pad_thumb` — a textured circle moved by the user's
    // finger. The L3 / R3 stick-CLICK action is a separate button id
    // (see TouchButtonId.L3 / R3), not the stick widget itself, so the
    // thumb sprite has no pressed variant.
    // Outer Box is NOT clipped, so when the finger is past the cap the
    // thumb can render OVER the ring.
    Box(
        modifier = Modifier.fillMaxSize().then(pressMod),
    ) {
        Image(
            painter = skinPainter("analog_base") ?: painterResource(R.drawable.pad_stick_base),
            contentDescription = cfg.id.label + " base",
            contentScale = ContentScale.Fit,
            alpha = opacity,
            modifier = Modifier
                .fillMaxSize()
                .offset(
                    x = with(density) { baseShift.value.x.toDp() },
                    y = with(density) { baseShift.value.y.toDp() },
                ),
        )
        val thumbSizeDp = cfg.sizeDp * 0.62f
        Image(
            painter = skinPainter("analog_stick") ?: painterResource(R.drawable.pad_thumb),
            contentDescription = cfg.id.label + " thumb",
            contentScale = ContentScale.Fit,
            alpha = opacity,
            modifier = Modifier
                .align(Alignment.Center)
                .offset(
                    x = with(density) { (baseShift.value.x + thumb.value.x).toDp() },
                    y = with(density) { (baseShift.value.y + thumb.value.y).toDp() },
                )
                .size(thumbSizeDp.dp),
        )
        if (edit) EditAdornment(cfg.id)
    }
}

private data class StickCodes(val xPos: Int, val xNeg: Int, val yPos: Int, val yNeg: Int)

private data class StickEmit(
    val xPos: Int = 0,
    val xNeg: Int = 0,
    val yPos: Int = 0,
    val yNeg: Int = 0,
) {
    fun any() = xPos != 0 || xNeg != 0 || yPos != 0 || yNeg != 0
}

/** Apply the user-configurable PER-STICK analog deadzone and re-normalize past it
 *  so the on-screen stick responds from low values without a jump — matching the
 *  physical-stick path (MainActivityRuntime.shapeStickMag). */
private fun shapeTouchAxis(m: Float, left: Boolean): Float {
    val dz = ControllerMappings.stickDeadzone(left)
    if (m <= dz) return 0f
    return (if (dz < 1f) (m - dz) / (1f - dz) else 0f).coerceIn(0f, 1f)
}

private fun computeStickEmit(nx: Float, ny: Float, left: Boolean): StickEmit {
    val scaleX = (shapeTouchAxis(abs(nx), left) * 32767f).toInt()
    val scaleY = (shapeTouchAxis(abs(ny), left) * 32767f).toInt()
    return StickEmit(
        xPos = if (nx > 0) scaleX else 0,
        xNeg = if (nx < 0) scaleX else 0,
        yPos = if (ny > 0) scaleY else 0,
        yNeg = if (ny < 0) scaleY else 0,
    )
}

private fun applyStickDiff(codes: StickCodes, prev: StickEmit, next: StickEmit) {
    if (prev.xPos != next.xPos) NativeApp.setPadButton(codes.xPos, next.xPos, next.xPos > 0)
    if (prev.xNeg != next.xNeg) NativeApp.setPadButton(codes.xNeg, next.xNeg, next.xNeg > 0)
    if (prev.yPos != next.yPos) NativeApp.setPadButton(codes.yPos, next.yPos, next.yPos > 0)
    if (prev.yNeg != next.yNeg) NativeApp.setPadButton(codes.yNeg, next.yNeg, next.yNeg > 0)
}

private fun releaseStick(codes: StickCodes, last: StickEmit) {
    if (last.xPos != 0) NativeApp.setPadButton(codes.xPos, 0, false)
    if (last.xNeg != 0) NativeApp.setPadButton(codes.xNeg, 0, false)
    if (last.yPos != 0) NativeApp.setPadButton(codes.yPos, 0, false)
    if (last.yNeg != 0) NativeApp.setPadButton(codes.yNeg, 0, false)
}

/* -------------------------------------------------------------------- */
/*  Gesture helpers                                                      */
/* -------------------------------------------------------------------- */

private fun sendDigital(keycode: Int, pressed: Boolean) {
    // Pressure modifier: send a soft (~50%) range for pressure-capable buttons
    // while the modifier is held; 0 (full press) otherwise. native-lib.cpp's
    // setPadButton turns the range into a 0..1 pressure value.
    val range = if (pressed) TouchControls.pressureRangeFor(keycode) else 0
    NativeApp.setPadButton(keycode, range, pressed)
    // Touch haptics (#247): a short vibration tick when a button goes DOWN. Press-only
    // (release stays silent); gated by the Touch Haptics setting (default on).
    if (pressed && TouchControls.touchHaptics.value) NativeApp.touchHaptic()
}

/** Buttons covered by the unified multi-touch hit-test layer: face diamond +
 *  shoulders + menu digital buttons (Start / Select / L3 / R3). These are the
 *  glide-eligible digital kinds the single UnifiedTouchLayer owns; when it's on,
 *  the visual widgets for these go render-only and read their pressed state from
 *  the layer's published unifiedPressed set. */
private fun isMultiTouchKind(kind: TouchButtonId.Kind): Boolean =
    kind == TouchButtonId.Kind.FACE ||
    kind == TouchButtonId.Kind.SHOULDER ||
    kind == TouchButtonId.Kind.MENU

/** Press/release pointerInput for a single digital button. Emits the
 *  keycode on down, releases on up or pointer cancel.
 *
 *  Claims ONLY a finger that just went down on THIS button (changedToDown),
 *  never a pointer already held elsewhere — grabbing the analog stick's pointer
 *  via a blind firstOrNull() was the stick+button multi-touch bug (#244). Then
 *  follows that pointer by id until it lifts.
 *
 *  [tapToHold]: latch mode — a tap toggles the button held (stays pressed +
 *  visually down) until the next tap, instead of momentary press. Released on
 *  dispose so a latched button can't get stuck down in the emulator. */
private fun Modifier.pressGestures(
    keycode: Int,
    tapToHold: Boolean = false,
    onPressedChange: (Boolean) -> Unit,
) =
    pointerInput(keycode, tapToHold) {
        var latched = false
        try {
            awaitPointerEventScope {
                while (true) {
                    val ev = awaitPointerEvent()
                    val down: PointerInputChange =
                        ev.changes.firstOrNull { it.changedToDown() } ?: continue
                    val id = down.id
                    // Keep the controls awake while the user is actively tapping.
                    TouchControls.noteTouchInteraction()
                    if (tapToHold) {
                        // Toggle the latch on this tap-down.
                        latched = !latched
                        onPressedChange(latched)
                        sendDigital(keycode, latched)
                        // Consume this finger's lifetime so the same press can't
                        // re-toggle; ignore other pointers.
                        while (true) {
                            val next = awaitPointerEvent()
                            val nc = next.changes.firstOrNull { it.id == id }
                            if (nc == null || !nc.pressed) break
                        }
                    } else {
                        onPressedChange(true)
                        sendDigital(keycode, true)
                        while (true) {
                            val next = awaitPointerEvent()
                            val nc = next.changes.firstOrNull { it.id == id }
                            if (nc == null || !nc.pressed) break
                        }
                        onPressedChange(false)
                        sendDigital(keycode, false)
                    }
                }
            }
        } finally {
            // Disposed/reconfigured while latched → don't leave the key stuck down.
            if (latched) {
                sendDigital(keycode, false)
                onPressedChange(false)
            }
        }
    }

/** Edit-mode gestures — tap selects the widget (so the toolbar can
 *  expose a size slider for it), pan moves, pinch resizes.
 *
 *  Two pointerInputs because Compose dispatches the same touch events
 *  to both: the onPress fires on initial pointer-down (used for
 *  selection), and detectTransformGestures fires on movement past
 *  touch-slop (used for pan/pinch). They cooperate cleanly — onPress
 *  doesn't consume, so the transform handler still sees the same
 *  events.
 *
 *  Note: the `pointerInput(cfg.id)` key never changes, so the captured
 *  `cfg` here is frozen at first composition and stays stale across
 *  drags. Always read live state from TouchControls inside the
 *  transform lambda. */
private fun Modifier.editGestures(cfg: TouchButtonCfg): Modifier =
    pointerInput(cfg.id, "press") {
        detectTapGestures(
            onPress = { TouchControls.selectedButton.value = cfg.id },
        )
    }.pointerInput(cfg.id) {
        detectTransformGestures(panZoomLock = false) { _, pan, zoom, _ ->
            val overlay = OverlayDims.last ?: return@detectTransformGestures
            TouchControls.updateButton(cfg.id) { current ->
                val newX = (current.xFrac + pan.x / overlay.widthPx).coerceIn(0.02f, 0.98f)
                val newY = (current.yFrac + pan.y / overlay.heightPx).coerceIn(0.02f, 0.98f)
                val newSize = (current.sizeDp * zoom).coerceIn(28f, 220f)
                current.copy(xFrac = newX, yFrac = newY, sizeDp = newSize)
            }
        }
    }

private object OverlayDims {
    @Volatile var last: Dims? = null
    data class Dims(val widthPx: Float, val heightPx: Float)
}

/* -------------------------------------------------------------------- */
/*  Edit-mode adornments + toolbar                                       */
/* -------------------------------------------------------------------- */

/** Outline drawn over every widget while in edit mode. Brighter +
 *  thicker for the currently-selected widget so the user can confirm
 *  which one the toolbar size slider operates on. */
@Composable
private fun DisabledMarker() {
    androidx.compose.foundation.Canvas(Modifier.fillMaxSize()) {
        val c = Color(0xFFFF5555)
        val sw = size.minDimension * 0.06f
        drawLine(c, Offset(size.width * 0.2f, size.height * 0.2f),
            Offset(size.width * 0.8f, size.height * 0.8f), strokeWidth = sw)
        drawLine(c, Offset(size.width * 0.8f, size.height * 0.2f),
            Offset(size.width * 0.2f, size.height * 0.8f), strokeWidth = sw)
    }
}

/** Edit-mode marker for a hidden (disabled) button — a red X so it can be found and
 *  re-enabled; in play mode the button isn't drawn at all (render-skip at the loop). */
@Composable
private fun EditAdornment(id: TouchButtonId? = null) {
    val isSelected = id != null && TouchControls.selectedButton.value == id
    val color = if (isSelected) Color(0xFFFFD33A) else Colors.pasx2_blue
    val width = if (isSelected) 3.dp else 2.dp
    Box(
        Modifier
            .fillMaxSize()
            .border(width, color, RoundedCornerShape(8.dp)),
    )
}

@Composable
private fun EditToolbar(modifier: Modifier = Modifier) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(Color(0xCC000000))
            .padding(horizontal = 12.dp, vertical = 8.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        // Scope hint: with no game running the editor edits the GLOBAL Default
        // layout (per-game layouts need a running disc).
        Text(
            if (MainActivityRuntime.eState.value == EmuState.RUNNING || MainActivityRuntime.eState.value == EmuState.PAUSED)
                str("touch.editor.scopeGame")
            else str("touch.editor.scopeGlobal"),
            color = Color(0xFFFFD33A), fontSize = 11.sp, fontWeight = FontWeight.SemiBold,
        )
        // Action chips up top — save commits the live layout into the
        // active profile, discard reverts to the saved version, reset
        // restores the default, profiles opens the picker.
        Row(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            ToolbarChip(str("action.save")) {
                TouchControls.saveLiveLayoutToActive()
                TouchControls.exitEditMode()
            }
            ToolbarChip(str("touch.editor.discard")) {
                TouchControls.discardEdits()
                TouchControls.exitEditMode()
            }
            ToolbarChip(str("action.reset")) {
                TouchControls.resetActiveToDefault()
                // Only clear a per-game key when a VM is actually running. From
                // the library this is a Global Default edit; resolving a serial
                // off the (possibly stale) MainActivityRuntime.currentGame would wrongly delete
                // the last-played game's per-serial layout.
                TouchControls.clearGameLayoutIfRunning()
            }
            ToolbarChip(str("touch.editor.profiles")) { TouchControls.profileDialogOpen.value = true }
            ToolbarChip(if (TouchControls.faceMultiTouch.value) str("touch.editor.multiTouchOn") else str("touch.editor.multiTouchOff")) {
                TouchControls.setFaceMultiTouch(!TouchControls.faceMultiTouch.value)
            }
            // Touch Gliding: drag a finger to hold every button it crosses (NetherSX2-style).
            ToolbarChip(if (TouchControls.touchGliding.value) str("touch.editor.glidingOn") else str("touch.editor.glidingOff")) {
                TouchControls.setTouchGliding(!TouchControls.touchGliding.value)
            }
            ToolbarChip(if (TouchControls.floatingStick.value) str("touch.editor.floatingStickOn") else str("touch.editor.floatingStickOff")) {
                TouchControls.setFloatingStick(!TouchControls.floatingStick.value)
            }
        }
        // Opacity slider — controls the live HUD alpha so the user sees
        // the change immediately while editing. Range 0.20..1.00 mirrors
        // TouchControls.setOpacity's clamp.
        Row(
            // Wrap-content width + Column's CenterHorizontally alignment
            // centers the alpha bar horizontally inside the toolbar
            // regardless of how wide the action-chip row above ends up.
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text("α", color = Color(0xFFAAAAAA), fontSize = 12.sp)
            androidx.compose.material3.Slider(
                value = TouchControls.opacity.floatValue,
                onValueChange = { TouchControls.setOpacity(it) },
                valueRange = 0.20f..1.0f,
                modifier = Modifier
                    .width(280.dp)
                    .height(28.dp),
                colors = androidx.compose.material3.SliderDefaults.colors(
                    thumbColor = Colors.pasx2_blue,
                    activeTrackColor = Colors.pasx2_blue,
                    inactiveTrackColor = Color(0xFF333344),
                ),
            )
            Text(
                "${(TouchControls.opacity.floatValue * 100).toInt()}%",
                color = Color(0xFFAAAAAA),
                fontSize = 11.sp,
                modifier = Modifier.width(40.dp),
            )
        }
        // Size slider — only present when a widget is selected. Lets
        // the user resize tiny buttons (L3/R3, Start/Select) that are
        // awkward to pinch-zoom directly. Selection is cleared by
        // tapping the dim backdrop.
        val selected = TouchControls.selectedButton.value
        val selectedCfg = if (selected != null)
            TouchControls.activeLayout.value.buttons.firstOrNull { it.id == selected }
        else null
        if (selectedCfg != null) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                Text(
                    selectedCfg.id.label + " size",
                    color = Color(0xFFFFD33A),
                    fontSize = 12.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                androidx.compose.material3.Slider(
                    value = selectedCfg.sizeDp,
                    onValueChange = { newSize ->
                        TouchControls.updateButton(selectedCfg.id) {
                            it.copy(sizeDp = newSize.coerceIn(28f, 220f))
                        }
                    },
                    valueRange = 28f..220f,
                    modifier = Modifier
                        .width(240.dp)
                        .height(28.dp),
                    colors = androidx.compose.material3.SliderDefaults.colors(
                        thumbColor = Color(0xFFFFD33A),
                        activeTrackColor = Color(0xFFFFD33A),
                        inactiveTrackColor = Color(0xFF444433),
                    ),
                )
                Text(
                    "${selectedCfg.sizeDp.toInt()}dp",
                    color = Color(0xFFAAAAAA),
                    fontSize = 11.sp,
                    modifier = Modifier.width(48.dp),
                )
                ToolbarChip(if (selectedCfg.enabled) str("touch.editor.hide") else str("touch.editor.show")) {
                    TouchControls.updateButton(selectedCfg.id) { it.copy(enabled = !it.enabled) }
                }
                // Tap-to-hold (latch) only applies to the digital action buttons that
                // run through pressGestures (face diamond + shoulders) — e.g. hold R1
                // for crouch without keeping a thumb down.
                if (selectedCfg.id.kind == TouchButtonId.Kind.FACE ||
                    selectedCfg.id.kind == TouchButtonId.Kind.SHOULDER
                ) {
                    ToolbarChip(if (selectedCfg.tapToHold) str("touch.editor.tapHoldOn") else str("touch.editor.tapHoldOff")) {
                        TouchControls.updateButton(selectedCfg.id) { it.copy(tapToHold = !it.tapToHold) }
                    }
                }
            }
            // D-Pad key spacing — only when the D-Pad is selected. Spreads the four
            // directions apart (opens a center gap), NetherSX2-style, with live preview.
            if (selectedCfg.id == TouchButtonId.DPAD) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    Text(
                        "D-Pad spacing",
                        color = Color(0xFFFFD33A),
                        fontSize = 12.sp,
                        fontWeight = FontWeight.SemiBold,
                    )
                    androidx.compose.material3.Slider(
                        value = TouchControls.dpadSpacing.floatValue,
                        onValueChange = { TouchControls.setDpadSpacing(it) },
                        valueRange = 0f..0.35f,
                        modifier = Modifier
                            .width(240.dp)
                            .height(28.dp),
                        colors = androidx.compose.material3.SliderDefaults.colors(
                            thumbColor = Color(0xFFFFD33A),
                            activeTrackColor = Color(0xFFFFD33A),
                            inactiveTrackColor = Color(0xFF444433),
                        ),
                    )
                    Text(
                        "${(TouchControls.dpadSpacing.floatValue * 100).toInt()}%",
                        color = Color(0xFFAAAAAA),
                        fontSize = 11.sp,
                        modifier = Modifier.width(48.dp),
                    )
                }
            }
        }
    }
}

@Composable
private fun ToolbarChip(label: String, onClick: () -> Unit) {
    Box(
        Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(Color(0xFF1F1F2C))
            .clickable(onClick = onClick)
            .padding(horizontal = 10.dp, vertical = 6.dp),
    ) {
        Text(label, color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
    }
}

/* -------------------------------------------------------------------- */
/*  Profile picker                                                       */
/* -------------------------------------------------------------------- */

@Composable
private fun ProfilePicker(onDismiss: () -> Unit) {
    var newName by remember { mutableStateOf("") }
    Box(
        Modifier
            .fillMaxSize()
            .background(Color(0xCC000000))
            .clickable(onClick = onDismiss),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            modifier = Modifier
                .width(360.dp)
                .clip(RoundedCornerShape(8.dp))
                .background(Color(0xFF1A1A24))
                // Eat taps inside the dialog so onDismiss only fires on
                // the backdrop. clickable with no onClick — Compose
                // requires an onClick lambda — so use an empty lambda.
                .clickable(enabled = true, onClick = {})
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(
                str("touch.profiles.title"),
                color = Color.White,
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
            )
            Text(
                if (InGameOverlay.currentSerial.value != null)
                    str("touch.profiles.infoGame")
                else
                    str("touch.profiles.infoGlobal"),
                color = Color(0xFFB0B0B0),
                fontSize = 11.sp,
            )
            Spacer(Modifier.height(4.dp))
            for (p in TouchControls.profiles) {
                val active = p.name == TouchControls.activeProfileName.value
                Row(
                    Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(6.dp))
                        .background(if (active) Colors.pasx2_blue.copy(alpha = 0.35f) else Color(0xFF202030))
                        .clickable { TouchControls.switchProfile(p.name) }
                        .padding(horizontal = 10.dp, vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        if (active) "● ${p.name}" else "  ${p.name}",
                        color = Color.White,
                        fontSize = 13.sp,
                        modifier = Modifier.weight(1f),
                    )
                    if (TouchControls.profiles.size > 1) {
                        Text(
                            str("action.delete"),
                            color = Color(0xFFFF6B6B),
                            fontSize = 11.sp,
                            modifier = Modifier
                                .clickable { TouchControls.deleteProfile(p.name) }
                                .padding(horizontal = 6.dp),
                        )
                    }
                }
            }
            Spacer(Modifier.height(8.dp))
            Text(str("touch.profiles.saveNewLabel"), color = Color(0xFFAAAAAA), fontSize = 12.sp)
            Row(
                Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                androidx.compose.material3.OutlinedTextField(
                    value = newName,
                    onValueChange = { newName = it },
                    singleLine = true,
                    placeholder = { Text(str("touch.profiles.namePlaceholder"), color = Color(0xFF888888)) },
                    colors = androidx.compose.material3.OutlinedTextFieldDefaults.colors(
                        focusedTextColor = Color.White,
                        unfocusedTextColor = Color.White,
                        focusedBorderColor = Colors.pasx2_blue,
                        unfocusedBorderColor = Color(0xFF444455),
                    ),
                    modifier = Modifier.weight(1f),
                )
                Box(
                    Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(Colors.pasx2_blue)
                        .clickable(enabled = newName.isNotBlank()) {
                            TouchControls.saveAsNewProfile(newName)
                            newName = ""
                        }
                        .padding(horizontal = 12.dp, vertical = 10.dp),
                ) {
                    Text(str("touch.profiles.saveAs"), color = Color.White, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                }
            }
            Spacer(Modifier.height(6.dp))
            Box(
                Modifier
                    .align(Alignment.End)
                    .clip(RoundedCornerShape(6.dp))
                    .background(Color(0xFF333344))
                    .clickable(onClick = onDismiss)
                    .padding(horizontal = 16.dp, vertical = 8.dp),
            ) {
                Text(str("action.close"), color = Color.White, fontSize = 12.sp)
            }
        }
    }
}
