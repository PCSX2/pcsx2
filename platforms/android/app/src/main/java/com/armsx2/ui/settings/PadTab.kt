package com.armsx2.ui.settings

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.nativeKeyCode
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.config.Settings
import com.armsx2.i18n.str
import com.armsx2.input.ControllerMappings
import com.armsx2.ui.Colors
import com.armsx2.ui.touch.TouchButtonId
import com.armsx2.ui.touch.TouchControls
import kr.co.iefriends.pcsx2.NativeApp

@Composable
fun PadTab(@Suppress("UNUSED_PARAMETER") state: MutableState<Settings>) {
    val scroll = remember { ScrollState(0) }
    ControllerAutoScroll(scroll)
    val capture = remember { mutableStateOf<ControllerMappings.Action?>(null) }
    // Local co-op: which player's mapping this tab is editing (0 = P1, 1 = P2).
    val editPlayer = remember { mutableStateOf(0) }
    // Per-game controller settings (issue #246): the input-mapping layer (button
    // binds, stick modes, custom stick codes) follows the SAME Global/Game scope
    // the other tabs use — the ScopeToggle shown above this tab drives it. In Game
    // scope with a known serial, edits write per-game overrides; otherwise global.
    val padScope = com.armsx2.ui.InGameOverlay.settingsScope.value
    val padSerial = com.armsx2.ui.InGameOverlay.currentSerial.value?.takeIf { it.isNotEmpty() }
    val editSerial: String? = if (padScope == com.armsx2.config.SettingsScope.Game) padSerial else null
    // Bindings are CAPTURED asynchronously (arm, then press a button), so the write
    // tier is re-derived LIVE at capture time rather than from the composition-time
    // editSerial — guarantees a scope flip between arming and pressing still lands on
    // the tier shown. (Display rows use editSerial, which is correct at composition.)
    val liveEditSerial: () -> String? = {
        if (com.armsx2.ui.InGameOverlay.settingsScope.value == com.armsx2.config.SettingsScope.Game)
            com.armsx2.ui.InGameOverlay.currentSerial.value?.takeIf { it.isNotEmpty() } else null
    }
    val ctx = LocalContext.current
    val refreshToken = remember { mutableStateOf(0) }
    val focusRequester = remember { FocusRequester() }
    // Which macro's "configure button set" dialog is open (null = none).
    val macroDialogFor = remember { mutableStateOf<TouchButtonId?>(null) }
    // Which macro is capturing a physical-controller trigger button (null = none).
    val macroCapture = remember { mutableStateOf<TouchButtonId?>(null) }

    val stickCapture = ControllerMappings.captureStickDir
    LaunchedEffect(capture.value, stickCapture.value, macroCapture.value) {
        // Tell Main.dispatchKeyEvent to stop intercepting controller buttons for
        // overlay nav while we're capturing, so B/A/Y/etc. reach onPreviewKeyEvent
        // and bind instead of (e.g.) exiting the menu. An Action capture, a stick-
        // direction capture, or a macro physical-trigger capture arms the bypass.
        val capturingNow = capture.value != null || stickCapture.value != null || macroCapture.value != null
        ControllerMappings.padCapturing.value = capturingNow
        if (capturingNow)
            focusRequester.requestFocus()
    }
    // Safety: clear the bypass flag if the tab leaves composition mid-capture.
    DisposableEffect(Unit) {
        onDispose {
            ControllerMappings.padCapturing.value = false
            ControllerMappings.captureStickDir.value = null
        }
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .focusRequester(focusRequester)
            .focusable()
            .onPreviewKeyEvent { event ->
                // Macro physical-trigger capture: bind the pressed physical button to
                // fire this macro's button set. Captures the raw keycode (not a PS2
                // target) since macros override the button's normal mapping.
                val mc = macroCapture.value
                if (mc != null) {
                    if (event.type != KeyEventType.KeyDown)
                        return@onPreviewKeyEvent true
                    val ncode = event.key.nativeKeyCode
                    if (ncode == android.view.KeyEvent.KEYCODE_UNKNOWN)
                        return@onPreviewKeyEvent true
                    TouchControls.setMacroPhysicalCode(mc, ncode)
                    macroCapture.value = null
                    refreshToken.value++
                    return@onPreviewKeyEvent true
                }
                // Stick-direction CUSTOM capture: resolve the pressed physical
                // button to the PS2 code it drives (same physical->target lookup
                // as gameplay) and bind that direction. Shares this focusable and
                // the padCapturing bypass with the per-Action capture below.
                val sc = stickCapture.value
                if (sc != null) {
                    if (event.type != KeyEventType.KeyDown)
                        return@onPreviewKeyEvent true
                    val ncode = event.key.nativeKeyCode
                    if (ncode == android.view.KeyEvent.KEYCODE_UNKNOWN)
                        return@onPreviewKeyEvent true
                    // Prefer an ARMSX2 hotkey if the pressed button is already bound to
                    // one (Hotkeys tab) — that turns a freed-up stick direction into a
                    // Quick Save/Load State (etc.) trigger. Otherwise resolve to the PS2
                    // button the pressed control drives, exactly as before.
                    val hk = ControllerMappings.hotkeyFor(ncode)
                    val target = if (hk != null) ControllerMappings.stickCodeForHotkey(hk)
                        else ControllerMappings.stickCodeForPhysical(ncode, editPlayer.value)
                    if (target != null) {
                        ControllerMappings.setCustomStickCode(sc.first, sc.second, target, editPlayer.value, liveEditSerial())
                        ControllerMappings.endStickCapture()
                        refreshToken.value++
                    }
                    // If the pressed button isn't mapped to any pad Action or hotkey,
                    // keep waiting (swallow) rather than binding nothing.
                    return@onPreviewKeyEvent true
                }
                // Regular button capture — the menu button is captured in
                // Main.dispatchKeyEvent so it can grab BACK / back-paddle keys.
                val action = capture.value ?: return@onPreviewKeyEvent false
                if (event.type != KeyEventType.KeyDown)
                    return@onPreviewKeyEvent true
                val nativeKeyCode = event.key.nativeKeyCode
                if (nativeKeyCode == android.view.KeyEvent.KEYCODE_UNKNOWN)
                    return@onPreviewKeyEvent true
                ControllerMappings.bind(action, nativeKeyCode, editPlayer.value, liveEditSerial())
                capture.value = null
                refreshToken.value++
                true
            }
            .verticalScroll(scroll)
            .verticalScrollbar(scroll),
    ) {
        Text(
            str("pad.instruction.tapThenPress"),
            color = Color(0xFFBBBBBB),
            fontSize = 12.sp,
            modifier = Modifier.padding(horizontal = 6.dp, vertical = 6.dp),
        )
        // Per-game scope hint (#246): the buttons / sticks below follow the
        // Global/Game toggle at the top of the menu. Spell out the current tier
        // here since this tab scrolls far from that toggle.
        Text(
            when {
                editSerial != null -> "● Editing controls for THIS GAME ($editSerial) — switch to Global up top to change all games."
                padSerial != null -> str("pad.scopeHint.globalWithGameHint")
                else -> str("pad.scopeHint.global")
            },
            color = if (editSerial != null) Colors.pasx2_blue else Color(0xFF9A9A9A),
            fontSize = 11.sp,
            fontWeight = if (editSerial != null) FontWeight.Bold else FontWeight.Normal,
            modifier = Modifier.padding(horizontal = 6.dp, vertical = 2.dp),
        )
        SettingsDivider()
        // Open the on-screen touch-layout editor straight from here (no need to be
        // in-game). Closes the settings overlay and drops into edit mode over the
        // game/library. With no game running it edits the Global Default layout.
        Box(
            Modifier
                .fillMaxWidth()
                .height(34.dp)
                .background(rowAura())
                .clickable { com.armsx2.ui.InGameOverlay.editTouchLayout() }
                .controllerFocusable(
                    controllerId = "pad-edit-touch",
                    onConfirm = { com.armsx2.ui.InGameOverlay.editTouchLayout() },
                )
                .padding(horizontal = 6.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Text(
                str("pad.editTouchLayout"),
                color = Colors.pasx2_blue, fontSize = 13.sp, fontWeight = FontWeight.Bold,
            )
        }
        SettingsDivider()
        // Loose state reads kept at the top of the grouped area so they always
        // recompose the tab regardless of which sections are collapsed.
        @Suppress("UNUSED_EXPRESSION") TouchControls.macroBindTick.value
        @Suppress("UNUSED_EXPRESSION")
        refreshToken.value
        // Also recompose when the mappings change externally (e.g. the global
        // "Reset to defaults" calls ControllerMappings.resetTunables, which bumps this)
        // so the feel sliders / stick modes refresh without re-opening the tab.
        @Suppress("UNUSED_EXPRESSION")
        ControllerMappings.stickBindTick.value
        CollapsibleSection(str("pad.section.macros"), initiallyExpanded = false) {
            // Macros — 4 combo buttons, each firing a chosen SET of pad buttons at once
            // (e.g. R1+R2+R3). Tap a row to pick its buttons. Use them on-screen (enable +
            // position the M1-M4 buttons in the layout editor, off by default) and/or bind a
            // PHYSICAL controller button to fire the same macro ("Bind").
            Text(
                str("pad.macros.header"),
                color = Color(0xFFBBBBBB),
                fontSize = 12.sp,
                modifier = Modifier.padding(horizontal = 6.dp, vertical = 4.dp),
            )
            listOf(TouchButtonId.MACRO1, TouchButtonId.MACRO2, TouchButtonId.MACRO3, TouchButtonId.MACRO4).forEach { mid ->
                val buttons = TouchControls.macroButtons(mid)
                val summary = if (buttons.isEmpty()) str("pad.macro.notSet") else buttons.joinToString(" + ") { it.label }
                val physCode = TouchControls.macroPhysicalCode(mid)
                val capturingThis = macroCapture.value == mid
                Row(
                    Modifier
                        .fillMaxWidth()
                        .height(44.dp)
                        .background(rowAura())
                        .clickable { macroDialogFor.value = mid }
                        .controllerFocusable(
                            controllerId = "pad-macro-${mid.name}",
                            onConfirm = { macroDialogFor.value = mid },
                        )
                        .padding(horizontal = 6.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(mid.label, color = Colors.pasx2_blue, fontSize = 13.sp, fontWeight = FontWeight.Bold)
                    Spacer(Modifier.width(10.dp))
                    Column(Modifier.weight(1f)) {
                        Text(summary, color = Color(0xFFCCCCCC), fontSize = 12.sp)
                        Text(
                            when {
                                capturingThis -> str("pad.pressControllerButton")
                                physCode != android.view.KeyEvent.KEYCODE_UNKNOWN ->
                                    "Controller: ${ControllerMappings.labelForKey(physCode)}"
                                else -> str("pad.controller.notBound")
                            },
                            color = if (capturingThis) Color(0xFFFFD33A) else Color(0xFF999999),
                            fontSize = 10.sp,
                        )
                    }
                    if (physCode != android.view.KeyEvent.KEYCODE_UNKNOWN && !capturingThis) {
                        Text(
                            str("pad.action.clear"),
                            color = Color(0xFFFF6B6B), fontSize = 11.sp, fontWeight = FontWeight.Bold,
                            modifier = Modifier
                                .clickable { TouchControls.clearMacroPhysicalCode(mid); refreshToken.value++ }
                                .padding(end = 10.dp),
                        )
                    }
                    Text(
                        if (capturingThis) str("action.cancel") else str("pad.action.bind"),
                        color = Colors.pasx2_blue, fontSize = 11.sp, fontWeight = FontWeight.Bold,
                        modifier = Modifier
                            .clickable {
                                macroCapture.value = if (capturingThis) null else mid
                                capture.value = null
                                ControllerMappings.captureStickDir.value = null
                            }
                            .padding(end = 10.dp),
                    )
                    Text(str("pad.action.edit"), color = Colors.pasx2_blue, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                }
                SettingsDivider()
            }
            macroDialogFor.value?.let { mid ->
                MacroConfigDialog(
                    macroId = mid,
                    onSaved = { refreshToken.value++ },
                    onDismiss = { macroDialogFor.value = null },
                )
            }
        }
        CollapsibleSection(str("pad.section.playerRumble"), initiallyExpanded = false) {
            // Local co-op: pick which player's buttons / stick mode you're editing. P2 is
            // the second controller to press a button in-game (auto-assigned). Stick
            // feel (deadzone / sensitivity / acceleration) and the D-pad-as-stick toggle
            // below are shared by both players.
            SegmentedRow(
                label = str("pad.editing.label"),
                options = listOf(str("pad.player1"), str("pad.player2")),
                selectedIndex = editPlayer.value,
                description = str("pad.editing.description"),
                onChange = {
                    editPlayer.value = it
                    capture.value = null
                    ControllerMappings.captureStickDir.value = null
                    refreshToken.value++
                },
            )
            // Master rumble / vibration enable — gates controller motors AND the device-haptic
            // fallback (NativeApp.onPadRumble). Off = no haptics anywhere.
            ToggleRow(
                str("pad.rumble.label"),
                ControllerMappings.rumbleEnabled(),
                description = str("pad.rumble.description"),
            ) {
                ControllerMappings.setRumbleEnabled(it)
                refreshToken.value++
            }
            SettingsDivider()
            // PS2 Multitap: route up to 8 controllers (both ports become 4-slot taps).
            // The pref drives PadRouter's slot count + the boot-time native arming; when a
            // game is already running we also arm it live. setMultitap parks the VM, so it
            // must run off the UI thread (and is a safe no-op when no VM is active).
            ToggleRow(
                str("pad.multitap.label"),
                ControllerMappings.multitapEnabled(),
                description = str("pad.multitap.description"),
            ) { on ->
                ControllerMappings.setMultitapEnabled(on)
                Thread {
                    NativeApp.setMultitap(0, on)
                    NativeApp.setMultitap(1, on)
                }.start()
                refreshToken.value++
            }
            SettingsDivider()
            // Buzz the selected player's controller and report whether Android can drive
            // its rumble — separates a routing problem from a pad whose haptics simply
            // aren't exposed to Android (common for DualSense/DS4 over Bluetooth).
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(30.dp)
                    .background(rowAura())
                    .clickable {
                        NativeApp.testRumble(editPlayer.value)
                        android.widget.Toast.makeText(
                            ctx, NativeApp.rumbleStatusForPort(editPlayer.value),
                            android.widget.Toast.LENGTH_LONG,
                        ).show()
                    }
                    .controllerFocusable(
                        controllerId = "pad-test-rumble",
                        onConfirm = {
                            NativeApp.testRumble(editPlayer.value)
                            android.widget.Toast.makeText(
                                ctx, NativeApp.rumbleStatusForPort(editPlayer.value),
                                android.widget.Toast.LENGTH_LONG,
                            ).show()
                        },
                    )
                    .padding(horizontal = 6.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                Text(
                    if (editPlayer.value == 0) str("pad.testRumble.player1") else str("pad.testRumble.player2"),
                    color = Colors.pasx2_blue, fontSize = 13.sp, fontWeight = FontWeight.Bold,
                )
            }
            SettingsDivider()
        }
        CollapsibleSection(str("pad.section.analogSticks"), initiallyExpanded = false) {
            // Analog stick remapping — make a physical stick act as the D-pad or the
            // face buttons (great for fighting games on analog-centric controllers).
            run {
                val stickOpts = ControllerMappings.StickMode.values().map { it.label }
                SegmentedRow(
                    label = str("pad.leftStick.label"),
                    options = stickOpts,
                    selectedIndex = ControllerMappings.leftStickModeScope(editPlayer.value, editSerial).ordinal,
                    description = str("pad.leftStick.description"),
                    onChange = {
                        ControllerMappings.setLeftStickMode(ControllerMappings.StickMode.values()[it], editPlayer.value, editSerial)
                        refreshToken.value++
                    },
                )
                SettingsDivider()
                if (ControllerMappings.leftStickModeScope(editPlayer.value, editSerial) == ControllerMappings.StickMode.CUSTOM) {
                    ControllerMappings.StickDir.values().forEach { dir ->
                        StickDirPickerRow(leftStick = true, dir = dir, player = editPlayer.value, serial = editSerial, onChanged = { refreshToken.value++ })
                        SettingsDivider()
                    }
                }
                // Axis correction for the LEFT stick — fixes pads that read mirrored/rotated.
                ToggleRow(str("pad.leftStick.swapXY.label"), ControllerMappings.stickSwapXY(true),
                    description = str("pad.leftStick.swapXY.description")) {
                    ControllerMappings.setStickSwapXY(true, it); refreshToken.value++
                }
                SettingsDivider()
                ToggleRow(str("pad.leftStick.invertX.label"), ControllerMappings.stickInvertX(true),
                    description = str("pad.leftStick.invertX.description")) {
                    ControllerMappings.setStickInvertX(true, it); refreshToken.value++
                }
                SettingsDivider()
                ToggleRow(str("pad.leftStick.invertY.label"), ControllerMappings.stickInvertY(true),
                    description = str("pad.leftStick.invertY.description")) {
                    ControllerMappings.setStickInvertY(true, it); refreshToken.value++
                }
                SettingsDivider()
                SegmentedRow(
                    label = str("pad.rightStick.label"),
                    options = stickOpts,
                    selectedIndex = ControllerMappings.rightStickModeScope(editPlayer.value, editSerial).ordinal,
                    description = str("pad.rightStick.description"),
                    onChange = {
                        ControllerMappings.setRightStickMode(ControllerMappings.StickMode.values()[it], editPlayer.value, editSerial)
                        refreshToken.value++
                    },
                )
                SettingsDivider()
                if (ControllerMappings.rightStickModeScope(editPlayer.value, editSerial) == ControllerMappings.StickMode.CUSTOM) {
                    ControllerMappings.StickDir.values().forEach { dir ->
                        StickDirPickerRow(leftStick = false, dir = dir, player = editPlayer.value, serial = editSerial, onChanged = { refreshToken.value++ })
                        SettingsDivider()
                    }
                }
                // Axis correction for the RIGHT stick — e.g. the tester's "down is up, left is right".
                ToggleRow(str("pad.rightStick.swapXY.label"), ControllerMappings.stickSwapXY(false),
                    description = str("pad.rightStick.swapXY.description")) {
                    ControllerMappings.setStickSwapXY(false, it); refreshToken.value++
                }
                SettingsDivider()
                ToggleRow(str("pad.rightStick.invertX.label"), ControllerMappings.stickInvertX(false),
                    description = str("pad.rightStick.invertX.description")) {
                    ControllerMappings.setStickInvertX(false, it); refreshToken.value++
                }
                SettingsDivider()
                ToggleRow(str("pad.rightStick.invertY.label"), ControllerMappings.stickInvertY(false),
                    description = str("pad.rightStick.invertY.description")) {
                    ControllerMappings.setStickInvertY(false, it); refreshToken.value++
                }
                SettingsDivider()
                ToggleRow(
                    str("pad.dpadAsLeftStick.label"),
                    ControllerMappings.dpadAsLeftStick(),
                    description = str("pad.dpadAsLeftStick.description"),
                ) {
                    ControllerMappings.setDpadAsLeftStick(it)
                    refreshToken.value++
                }
                SettingsDivider()
                // Stick FEEL is per-stick now (tester: lowering sensitivity for
                // camera aim also slowed walking). Existing single-value settings
                // migrate to both sticks automatically.
                StickFeelSliders(left = true, title = str("pad.leftStickFeel.title"), refreshToken = refreshToken)
                StickFeelSliders(left = false, title = str("pad.rightStickFeel.title"), refreshToken = refreshToken)
            }
        }
        CollapsibleSection(str("pad.section.buttonMapping"), initiallyExpanded = false) {
            ControllerMappings.actions.forEach { action ->
                val physical = ControllerMappings.physicalForScope(action, editPlayer.value, editSerial)
                PadBindingRow(
                    action = action,
                    physical = physical,
                    capturing = capture.value == action,
                    onClick = { capture.value = action },
                    onClear = {
                        ControllerMappings.clearAction(action, editPlayer.value, editSerial)
                        if (capture.value == action) capture.value = null
                        refreshToken.value++
                    },
                )
                // Turbo / rapid-fire toggle — only meaningful once the button is
                // bound to a physical controller button.
                if (physical != android.view.KeyEvent.KEYCODE_UNKNOWN) {
                    val turbo = remember(action.id, editPlayer.value, refreshToken.value) {
                        mutableStateOf(ControllerMappings.isTurboAction(action, editPlayer.value))
                    }
                    androidx.compose.foundation.layout.Row(
                        androidx.compose.ui.Modifier
                            .fillMaxWidth()
                            .clickable {
                                val nv = !turbo.value
                                turbo.value = nv
                                ControllerMappings.setTurboAction(action, editPlayer.value, nv)
                            }
                            .padding(start = 18.dp, end = 10.dp, top = 2.dp, bottom = 4.dp),
                        verticalAlignment = androidx.compose.ui.Alignment.CenterVertically,
                    ) {
                        androidx.compose.material3.Text(
                            "↳ Turbo (rapid-fire while held)",
                            color = androidx.compose.ui.graphics.Color(0xFFB0B0B0),
                            fontSize = 11.sp,
                            modifier = androidx.compose.ui.Modifier.weight(1f),
                        )
                        androidx.compose.material3.Text(
                            if (turbo.value) "ON" else "OFF",
                            color = if (turbo.value) androidx.compose.ui.graphics.Color(0xFF4DA3FF)
                            else androidx.compose.ui.graphics.Color(0xFF808080),
                            fontSize = 12.sp,
                            fontWeight = androidx.compose.ui.text.font.FontWeight.SemiBold,
                        )
                    }
                }
                SettingsDivider()
            }
            // Reset clears this scope's binds: Global wipes the global map; Game
            // removes this game's per-game overrides (button binds AND stick maps),
            // reverting it to the global map.
            val resetMappings: () -> Unit = {
                if (editSerial != null) ControllerMappings.clearGameOverrides(editSerial, editPlayer.value)
                else ControllerMappings.reset(editPlayer.value)
                capture.value = null
                refreshToken.value++
            }
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(30.dp)
                    .background(rowAura())
                    .clickable { resetMappings() }
                    .controllerFocusable(
                        controllerId = "pad-reset",
                        onConfirm = resetMappings,
                    )
                    .padding(horizontal = 6.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                val who = if (editPlayer.value == 0) "Player 1" else "Player 2"
                Text(
                    if (editSerial != null) "Reset $who Mappings for This Game"
                    else "Reset $who Mappings",
                    color = Colors.pasx2_blue, fontSize = 13.sp, fontWeight = FontWeight.Bold,
                )
            }
        }
        CollapsibleSection(str("pad.section.onScreenControls"), initiallyExpanded = false) {
            // Controller hotkeys now live in their own dedicated "Hotkeys" tab
            // (see HotkeysTab) so they're easier to find than buried under Pad.
            SettingsDivider()
            IntSliderRow(
                label = str("pad.onScreenControls.label"),
                value = TouchControls.visibilityMode.value,
                min = 0,
                max = 11,
                description = str("pad.onScreenControls.description"),
                valueFormatter = { when (it) { 0 -> "Never"; 11 -> "Auto"; else -> "${it}s" } },
                onChange = { TouchControls.setVisibilityMode(it) },
            )
            SettingsDivider()
            // Touch Haptics (#247): vibrate on on-screen button presses.
            ToggleRow(
                str("pad.touchHaptics.label"),
                TouchControls.touchHaptics.value,
                description = str("pad.touchHaptics.description"),
            ) { TouchControls.setTouchHaptics(it) }
            SettingsDivider()
            // Multi-touch reach: how far from a button's center a touch still counts.
            // Higher = press adjacent buttons together with more space between them.
            IntSliderRow(
                label = "Multi-touch Reach",
                value = (TouchControls.multiTouchRadius.value * 100f).toInt(),
                min = 50,
                max = 95,
                description = "How far from a button's center a touch still registers. Raise this if multi-touch only works when buttons are almost touching.",
                valueFormatter = { "${it}%" },
                onChange = { TouchControls.setMultiTouchRadius(it / 100f) },
            )
            // D-Pad key spacing lives in the Touch Layout editor now: open the editor,
            // tap the D-Pad to select it, and use the "D-Pad spacing" slider to spread
            // the four directions apart (NetherSX2-style) with a live preview.
        }
    }
}

/** The five stick-FEEL sliders (deadzone / outer / anti-deadzone / sensitivity /
 *  acceleration) for ONE stick. Rendered twice — Left and Right — since every
 *  feel tunable is per-stick (a camera-stick sensitivity tweak must not slow the
 *  walk stick). Values migrate from the old shared keys on first read. */
@Composable
private fun StickFeelSliders(left: Boolean, title: String, refreshToken: androidx.compose.runtime.MutableState<Int>) {
    // Subscribe this composable to the token. Each slider's `value` is read from a
    // raw pref (ControllerMappings.stick*), which Compose can't observe — so without
    // this read a bump (fired in every onChange) wouldn't recompose StickFeelSliders
    // (its params are unchanged, so Compose skips it) and the thumb/number would only
    // catch up on menu re-entry. Reading .value here puts the whole function in the
    // token's restart scope, so each drag step refreshes the displayed value live.
    @Suppress("UNUSED_EXPRESSION")
    refreshToken.value
    CollapsibleSection(title, initiallyExpanded = false) {
        IntSliderRow(
            label = str("pad.stickFeel.deadzone.label"),
            value = (ControllerMappings.stickDeadzone(left) * 100f).toInt(), // 0.0..0.4 -> 0..40
            min = 0,
            max = (ControllerMappings.STICK_DZ_MAX * 100f).toInt(),
            description = str("pad.stickFeel.deadzone.description"),
            valueFormatter = { if (it == 0) "Off" else "${it}%" },
            onChange = { ControllerMappings.setStickDeadzone(left, it / 100f); refreshToken.value++ },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("pad.stickFeel.outerDeadzone.label"),
            value = (ControllerMappings.stickOuterDeadzone(left) * 100f).toInt(), // 0.0..0.4 -> 0..40
            min = 0,
            max = (ControllerMappings.STICK_OUTER_MAX * 100f).toInt(),
            description = str("pad.stickFeel.outerDeadzone.description"),
            valueFormatter = { if (it == 0) "Off" else "${it}%" },
            onChange = { ControllerMappings.setStickOuterDeadzone(left, it / 100f); refreshToken.value++ },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("pad.stickFeel.antiDeadzone.label"),
            value = (ControllerMappings.stickAntiDeadzone(left) * 100f).toInt(), // 0.0..0.6 -> 0..60
            min = 0,
            max = (ControllerMappings.STICK_ANTIDZ_MAX * 100f).toInt(),
            description = str("pad.stickFeel.antiDeadzone.description"),
            valueFormatter = { if (it == 0) "Off" else "${it}%" },
            onChange = { ControllerMappings.setStickAntiDeadzone(left, it / 100f); refreshToken.value++ },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("pad.stickFeel.sensitivity.label"),
            value = (ControllerMappings.stickSensitivity(left) * 20f).toInt(), // 0.5..2.0 -> 10..40
            min = 10,
            max = 40,
            description = str("pad.stickFeel.sensitivity.description"),
            valueFormatter = { "${it * 5}%" },
            onChange = { ControllerMappings.setStickSensitivity(left, it / 20f); refreshToken.value++ },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("pad.stickFeel.acceleration.label"),
            value = (ControllerMappings.stickAcceleration(left) * 10f).toInt(), // 0.0..2.0 -> 0..20
            min = 0,
            max = 20,
            description = str("pad.stickFeel.acceleration.description"),
            valueFormatter = { if (it == 0) "Off (linear)" else "+%.1f".format(it / 10f) },
            onChange = { ControllerMappings.setStickAcceleration(left, it / 10f); refreshToken.value++ },
        )
        SettingsDivider()
    }
}

/** One CUSTOM-mode row: a stick direction on the left, its bound target on the
 *  right. Tap (or A) opens a PICKER to choose what the direction sends — a PS2
 *  button (incl. the D-pad), an ARMSX2 hotkey, or Analog (default). A direct
 *  picker (not "press a physical button") means you can assign a target even
 *  after you've UNBOUND that button elsewhere — the old capture resolved the
 *  pressed button through its live mapping, so an unbound D-pad couldn't be
 *  picked. "Clear" (or D-pad-left on a controller) resets to the analog default.
 *  Shown only when the stick is CUSTOM. */
@Composable
private fun StickDirPickerRow(
    leftStick: Boolean,
    dir: ControllerMappings.StickDir,
    player: Int,
    serial: String?,
    onChanged: () -> Unit,
) {
    @Suppress("UNUSED_EXPRESSION")
    ControllerMappings.stickBindTick.value // recompose after a bind/reset
    val code = ControllerMappings.customStickCodeScope(leftStick, dir, player, serial)
    val showPicker = remember { mutableStateOf(false) }
    fun clear() {
        ControllerMappings.resetStickCode(leftStick, dir, player, serial)
        onChanged()
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(30.dp)
            .background(rowAura())
            .clickable { showPicker.value = true }
            .controllerFocusable(
                controllerId = "stickdir:${if (leftStick) "l" else "r"}:${dir.id}",
                onConfirm = { showPicker.value = true },
                onLeft = { clear() },
            )
            .padding(horizontal = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            "    ${dir.id.replaceFirstChar { it.uppercase() }}",
            color = Color.White,
            fontSize = 13.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(Modifier.weight(1f))
        Text(
            str("pad.action.clear"),
            color = Color(0xFFE57373),
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier
                .clickable { clear() }
                .padding(horizontal = 8.dp, vertical = 2.dp),
        )
        Spacer(Modifier.width(4.dp))
        Text(
            ControllerMappings.stickTargetLabel(code),
            color = Color(0xFFCCCCCC),
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
        )
    }
    if (showPicker.value) {
        StickTargetPickerDialog(
            title = "${if (leftStick) "Left" else "Right"} Stick — ${dir.id.replaceFirstChar { it.uppercase() }}",
            current = code,
            onPick = { picked ->
                if (picked == null) ControllerMappings.resetStickCode(leftStick, dir, player, serial)
                else ControllerMappings.setCustomStickCode(leftStick, dir, picked, player, serial)
                showPicker.value = false
                onChanged()
            },
            onDismiss = { showPicker.value = false },
        )
    }
}

/** Direct target picker for a CUSTOM stick direction: PS2 buttons, ARMSX2
 *  hotkeys, or Analog (default). [onPick] receives the chosen setPadButton code,
 *  or null for Analog (default → clears the override). */
@Composable
private fun StickTargetPickerDialog(
    title: String,
    current: Int,
    onPick: (Int?) -> Unit,
    onDismiss: () -> Unit,
) {
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = Colors.surfaceColor,
        titleContentColor = Color.White,
        textContentColor = Color.White,
        title = { Text(title, color = Color.White, fontWeight = FontWeight.Bold) },
        text = {
            Column(Modifier.verticalScroll(remember { ScrollState(0) })) {
                Text(
                    str("pad.stickTarget.intro"),
                    color = Color(0xFFBBBBBB), fontSize = 12.sp,
                )
                Spacer(Modifier.height(8.dp))
                StickPickItem(str("pad.stickTarget.analogDefault"), current in 110..123) { onPick(null) }
                Spacer(Modifier.height(6.dp))
                Text(str("pad.stickTarget.ps2Buttons"), color = Colors.pasx2_blue, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                ControllerMappings.stickTargets.forEach { t ->
                    StickPickItem(t.label, current == t.code) { onPick(t.code) }
                }
                Spacer(Modifier.height(6.dp))
                Text(str("pad.stickTarget.hotkeys"), color = Colors.pasx2_blue, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                ControllerMappings.SysHotkey.values().forEach { h ->
                    val hc = ControllerMappings.stickCodeForHotkey(h)
                    StickPickItem("Hotkey: ${h.label}", current == hc) { onPick(hc) }
                }
            }
        },
        confirmButton = {},
        dismissButton = {
            TextButton(onClick = onDismiss) { Text(str("action.cancel"), color = Colors.pasx2_blue) }
        },
    )
}

@Composable
private fun StickPickItem(label: String, selected: Boolean, onClick: () -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(vertical = 8.dp, horizontal = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            if (selected) "●  " else "○  ",
            color = if (selected) Colors.pasx2_blue else Color(0xFF777777),
            fontSize = 13.sp,
        )
        Text(
            label,
            color = Color.White,
            fontSize = 13.sp,
            fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
        )
    }
}

/** Dialog to choose which pad buttons a macro fires together. */
@Composable
private fun MacroConfigDialog(
    macroId: TouchButtonId,
    onSaved: () -> Unit,
    onDismiss: () -> Unit,
) {
    val selected = remember(macroId) {
        mutableStateListOf<TouchButtonId>().apply { addAll(TouchControls.macroButtons(macroId)) }
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = Colors.surfaceColor,
        titleContentColor = Color.White,
        textContentColor = Color.White,
        title = { Text("Configure ${macroId.label}", color = Color.White, fontWeight = FontWeight.Bold) },
        text = {
            Column(Modifier.verticalScroll(remember { ScrollState(0) })) {
                Text(
                    str("pad.macroConfig.intro"),
                    color = Color(0xFFBBBBBB), fontSize = 12.sp,
                )
                Spacer(Modifier.height(8.dp))
                TouchControls.macroAssignableButtons.forEach { b ->
                    val on = b in selected
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .height(36.dp)
                            .clickable { if (on) selected.remove(b) else selected.add(b) }
                            .padding(horizontal = 4.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(
                            if (on) "☑" else "☐",
                            color = if (on) Colors.pasx2_blue else Color(0xFF888888),
                            fontSize = 16.sp,
                        )
                        Spacer(Modifier.width(12.dp))
                        Text(b.label, color = Color.White, fontSize = 14.sp)
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                TouchControls.setMacroButtons(macroId, selected.toList())
                onSaved()
                onDismiss()
            }) { Text(str("action.save")) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(str("action.cancel")) } },
    )
}

@Composable
private fun PadBindingRow(
    action: ControllerMappings.Action,
    physical: Int,
    capturing: Boolean,
    onClick: () -> Unit,
    onClear: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .height(30.dp)
            .background(rowAura())
            .clickable(onClick = onClick)
            .controllerFocusable(
                controllerId = "pad:${action.id}",
                onConfirm = onClick,
            )
            .padding(horizontal = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(action.label, color = Color.White, fontSize = 13.sp, fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.weight(1f))
        // "Clear" unbinds the button (leaves it blank, free to assign as a hotkey) —
        // mirrors the Hotkeys tab. Shown only when bound and not mid-capture.
        if (!capturing && physical != android.view.KeyEvent.KEYCODE_UNKNOWN) {
            Text(
                str("pad.action.clear"),
                color = Color(0xFFFF6B6B),
                fontSize = 11.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier
                    .clickable(onClick = onClear)
                    .padding(end = 10.dp),
            )
        }
        Text(
            if (capturing) str("pad.pressButton") else ControllerMappings.labelForKey(physical),
            color = if (capturing) Color(0xFFFFD33A) else Color(0xFFCCCCCC),
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold,
        )
    }
}
