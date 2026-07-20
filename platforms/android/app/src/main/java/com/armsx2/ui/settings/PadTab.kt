package com.armsx2.ui.settings

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.focusable
import androidx.compose.foundation.layout.Arrangement
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
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.OutlinedTextFieldDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.config.Settings
import com.armsx2.i18n.str
import com.armsx2.input.ControllerMappings
import com.armsx2.ui.Colors
import com.armsx2.ui.touch.TouchButtonId
import com.armsx2.ui.touch.TouchControls
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp

@Composable
fun PadTab(@Suppress("UNUSED_PARAMETER") state: MutableState<Settings>) {
    val scroll = settingsScrollState()
    ControllerAutoScroll(scroll)
    val capture = remember { mutableStateOf<ControllerMappings.Action?>(null) }
    // Local co-op: which player's mapping this tab is editing (0 = P1, 1 = P2).
    val editPlayer = remember { mutableIntStateOf(0) }
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
    val refreshToken = remember { mutableIntStateOf(0) }
    val focusRequester = remember { FocusRequester() }
    // Which macro is capturing a physical-controller trigger button (null = none).
    val macroCapture = remember { mutableStateOf<TouchButtonId?>(null) }

    val stickCapture = ControllerMappings.captureStickDir
    LaunchedEffect(capture.value, stickCapture.value, macroCapture.value) {
        // Tell MainActivityRuntime.dispatchKeyEvent to stop intercepting controller buttons for
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
                    refreshToken.intValue++
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
                        else ControllerMappings.stickCodeForPhysical(ncode, editPlayer.intValue)
                    if (target != null) {
                        ControllerMappings.setCustomStickCode(sc.first, sc.second, target, editPlayer.intValue, liveEditSerial())
                        ControllerMappings.endStickCapture()
                        refreshToken.intValue++
                    }
                    // If the pressed button isn't mapped to any pad Action or hotkey,
                    // keep waiting (swallow) rather than binding nothing.
                    return@onPreviewKeyEvent true
                }
                // Regular button capture — the menu button is captured in
                // MainActivityRuntime.dispatchKeyEvent so it can grab BACK / back-paddle keys.
                val action = capture.value ?: return@onPreviewKeyEvent false
                if (event.type != KeyEventType.KeyDown)
                    return@onPreviewKeyEvent true
                val nativeKeyCode = event.key.nativeKeyCode
                if (nativeKeyCode == android.view.KeyEvent.KEYCODE_UNKNOWN)
                    return@onPreviewKeyEvent true
                ControllerMappings.bind(action, nativeKeyCode, editPlayer.intValue, liveEditSerial())
                capture.value = null
                refreshToken.intValue++
                true
            },
    ) {
        Text(
            str("pad.instruction.tapThenPress"),
            color = Color(0xFFBBBBBB),
            fontSize = 15.sp,
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
            fontSize = 14.sp,
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
                .height(56.dp)
                .clip(RoundedCornerShape(16.dp))
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
                color = Colors.pasx2_blue, fontSize = 16.sp, fontWeight = FontWeight.Bold,
            )
        }
        SettingsDivider()
        // Loose state reads kept at the top of the grouped area so they always
        // recompose the tab regardless of which sections are collapsed.
        @Suppress("UNUSED_EXPRESSION") TouchControls.macroBindTick.value
        @Suppress("UNUSED_EXPRESSION")
        refreshToken.intValue
        // Also recompose when the mappings change externally (e.g. the global
        // "Reset to defaults" calls ControllerMappings.resetTunables, which bumps this)
        // so the feel sliders / stick modes refresh without re-opening the tab.
        @Suppress("UNUSED_EXPRESSION")
        ControllerMappings.stickBindTick.value
        // Macros section — extracted so the in-game Controls tab can reuse it. Here in the
        // full Pad tab we pass the physical-trigger capture host, so the "Bind" column is live.
        MacrosSection(
            macroCapture = macroCapture,
            onArmCapture = { mid ->
                macroCapture.value = if (macroCapture.value == mid) null else mid
                capture.value = null
                ControllerMappings.captureStickDir.value = null
            },
        )
        CollapsibleSection(str("pad.section.playerRumble"), initiallyExpanded = false) {
            // Local co-op: pick which player's buttons / stick mode you're editing. P2 is
            // the second controller to press a button in-game (auto-assigned). Stick
            // feel (deadzone / sensitivity / acceleration) and the D-pad-as-stick toggle
            // below are shared by both players.
            SegmentedRow(
                label = str("pad.editing.label"),
                options = listOf(str("pad.player1"), str("pad.player2")),
                selectedIndex = editPlayer.intValue,
                description = str("pad.editing.description"),
                onChange = {
                    editPlayer.intValue = it
                    capture.value = null
                    ControllerMappings.captureStickDir.value = null
                    refreshToken.intValue++
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
                refreshToken.intValue++
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
                refreshToken.intValue++
            }
            SettingsDivider()
            // Buzz the selected player's controller and report whether Android can drive
            // its rumble — separates a routing problem from a pad whose haptics simply
            // aren't exposed to Android (common for DualSense/DS4 over Bluetooth).
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(56.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(rowAura())
                    .clickable {
                        NativeApp.testRumble(editPlayer.intValue)
                        android.widget.Toast.makeText(
                            ctx, NativeApp.rumbleStatusForPort(editPlayer.intValue),
                            android.widget.Toast.LENGTH_LONG,
                        ).show()
                    }
                    .controllerFocusable(
                        controllerId = "pad-test-rumble",
                        onConfirm = {
                            NativeApp.testRumble(editPlayer.intValue)
                            android.widget.Toast.makeText(
                                ctx, NativeApp.rumbleStatusForPort(editPlayer.intValue),
                                android.widget.Toast.LENGTH_LONG,
                            ).show()
                        },
                    )
                    .padding(horizontal = 6.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                Text(
                    if (editPlayer.intValue == 0) str("pad.testRumble.player1") else str("pad.testRumble.player2"),
                    color = Colors.pasx2_blue, fontSize = 16.sp, fontWeight = FontWeight.Bold,
                )
            }
            SettingsDivider()
        }
        CollapsibleSection(str("pad.section.analogSticks"), initiallyExpanded = false) {
            // Analog stick remapping — make a physical stick act as the D-pad or the
            // face buttons (great for fighting games on analog-centric controllers).
            run {
                val stickOpts = ControllerMappings.StickMode.entries.map { it.label }
                SegmentedRow(
                    label = str("pad.leftStick.label"),
                    options = stickOpts,
                    selectedIndex = ControllerMappings.leftStickModeScope(editPlayer.intValue, editSerial).ordinal,
                    description = str("pad.leftStick.description"),
                    onChange = {
                        ControllerMappings.setLeftStickMode(ControllerMappings.StickMode.entries[it], editPlayer.intValue, editSerial)
                        refreshToken.intValue++
                    },
                )
                SettingsDivider()
                if (ControllerMappings.leftStickModeScope(editPlayer.intValue, editSerial) == ControllerMappings.StickMode.CUSTOM) {
                    ControllerMappings.StickDir.entries.forEach { dir ->
                        StickDirPickerRow(leftStick = true, dir = dir, player = editPlayer.intValue, serial = editSerial, onChanged = { refreshToken.intValue++ })
                        SettingsDivider()
                    }
                }
                // Axis correction for the LEFT stick — fixes pads that read mirrored/rotated.
                ToggleRow(str("pad.leftStick.swapXY.label"), ControllerMappings.stickSwapXYScope(true, editSerial),
                    description = str("pad.leftStick.swapXY.description")) {
                    ControllerMappings.setStickSwapXY(true, it, editSerial); refreshToken.intValue++
                }
                SettingsDivider()
                ToggleRow(str("pad.leftStick.invertX.label"), ControllerMappings.stickInvertXScope(true, editSerial),
                    description = str("pad.leftStick.invertX.description")) {
                    ControllerMappings.setStickInvertX(true, it, editSerial); refreshToken.intValue++
                }
                SettingsDivider()
                ToggleRow(str("pad.leftStick.invertY.label"), ControllerMappings.stickInvertYScope(true, editSerial),
                    description = str("pad.leftStick.invertY.description")) {
                    ControllerMappings.setStickInvertY(true, it, editSerial); refreshToken.intValue++
                }
                SettingsDivider()
                SegmentedRow(
                    label = str("pad.rightStick.label"),
                    options = stickOpts,
                    selectedIndex = ControllerMappings.rightStickModeScope(editPlayer.intValue, editSerial).ordinal,
                    description = str("pad.rightStick.description"),
                    onChange = {
                        ControllerMappings.setRightStickMode(ControllerMappings.StickMode.entries[it], editPlayer.intValue, editSerial)
                        refreshToken.intValue++
                    },
                )
                SettingsDivider()
                if (ControllerMappings.rightStickModeScope(editPlayer.intValue, editSerial) == ControllerMappings.StickMode.CUSTOM) {
                    ControllerMappings.StickDir.entries.forEach { dir ->
                        StickDirPickerRow(leftStick = false, dir = dir, player = editPlayer.intValue, serial = editSerial, onChanged = { refreshToken.intValue++ })
                        SettingsDivider()
                    }
                }
                // Axis correction for the RIGHT stick — e.g. the tester's "down is up, left is right".
                ToggleRow(str("pad.rightStick.swapXY.label"), ControllerMappings.stickSwapXYScope(false, editSerial),
                    description = str("pad.rightStick.swapXY.description")) {
                    ControllerMappings.setStickSwapXY(false, it, editSerial); refreshToken.intValue++
                }
                SettingsDivider()
                ToggleRow(str("pad.rightStick.invertX.label"), ControllerMappings.stickInvertXScope(false, editSerial),
                    description = str("pad.rightStick.invertX.description")) {
                    ControllerMappings.setStickInvertX(false, it, editSerial); refreshToken.intValue++
                }
                SettingsDivider()
                ToggleRow(str("pad.rightStick.invertY.label"), ControllerMappings.stickInvertYScope(false, editSerial),
                    description = str("pad.rightStick.invertY.description")) {
                    ControllerMappings.setStickInvertY(false, it, editSerial); refreshToken.intValue++
                }
                SettingsDivider()
                ToggleRow(
                    str("pad.dpadAsLeftStick.label"),
                    ControllerMappings.dpadAsLeftStickScope(editSerial),
                    description = str("pad.dpadAsLeftStick.description"),
                ) {
                    ControllerMappings.setDpadAsLeftStick(it, editSerial)
                    refreshToken.intValue++
                }
                SettingsDivider()
                // Stick FEEL is per-stick now (tester: lowering sensitivity for
                // camera aim also slowed walking). Existing single-value settings
                // migrate to both sticks automatically.
                StickFeelSliders(left = true, title = str("pad.leftStickFeel.title"), refreshToken = refreshToken)
                StickFeelSliders(left = false, title = str("pad.rightStickFeel.title"), refreshToken = refreshToken)
            }
        }
        // Motion / gyroscope controls. Shared with the in-game pause menu's Controls tab
        // (com.armsx2.ui.settings.GyroSection). Here it follows the Pad tab's Global/Game
        // scope (editSerial) and shares the tab's refreshToken so it re-reads live.
        GyroSection(editSerial = editSerial, externalRefresh = refreshToken)
        CollapsibleSection(str("pad.section.buttonMapping"), initiallyExpanded = false) {
            ControllerMappings.actions.forEach { action ->
                val physical = ControllerMappings.physicalForScope(action, editPlayer.intValue, editSerial)
                PadBindingRow(
                    action = action,
                    physical = physical,
                    capturing = capture.value == action,
                    onClick = { capture.value = action },
                    onClear = {
                        ControllerMappings.clearAction(action, editPlayer.intValue, editSerial)
                        if (capture.value == action) capture.value = null
                        refreshToken.intValue++
                    },
                )
                // Turbo / rapid-fire toggle — only meaningful once the button is
                // bound to a physical controller button.
                if (physical != android.view.KeyEvent.KEYCODE_UNKNOWN) {
                    val turbo = remember(action.id, editPlayer.intValue, refreshToken.intValue) {
                        mutableStateOf(ControllerMappings.isTurboAction(action, editPlayer.intValue))
                    }
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .clickable {
                                val nv = !turbo.value
                                turbo.value = nv
                                ControllerMappings.setTurboAction(action, editPlayer.intValue, nv)
                            }
                            .padding(start = 18.dp, end = 10.dp, top = 2.dp, bottom = 4.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(
                            "↳ Turbo (rapid-fire while held)",
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            fontSize = 14.sp,
                            modifier = Modifier.weight(1f),
                        )
                        Text(
                            if (turbo.value) "ON" else "OFF",
                            color = if (turbo.value) Color(0xFF4DA3FF)
                            else Color(0xFF808080),
                            fontSize = 15.sp,
                            fontWeight = FontWeight.SemiBold,
                        )
                    }
                }
                SettingsDivider()
            }
            // Reset clears this scope's binds: Global wipes the global map; Game
            // removes this game's per-game overrides (button binds AND stick maps),
            // reverting it to the global map.
            val resetMappings: () -> Unit = {
                if (editSerial != null) ControllerMappings.clearGameOverrides(editSerial, editPlayer.intValue)
                else ControllerMappings.reset(editPlayer.intValue)
                capture.value = null
                refreshToken.intValue++
            }
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(56.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(rowAura())
                    .clickable { resetMappings() }
                    .controllerFocusable(
                        controllerId = "pad-reset",
                        onConfirm = resetMappings,
                    )
                    .padding(horizontal = 6.dp),
                contentAlignment = Alignment.CenterStart,
            ) {
                val who = str(if (editPlayer.intValue == 0) "pad.player1" else "pad.player2")
                Text(
                    "${str("action.reset")} · $who${if (editSerial != null) " · ${str("scope.game")}" else ""}",
                    color = Colors.pasx2_blue, fontSize = 16.sp, fontWeight = FontWeight.Bold,
                )
            }
        }
        // Named mapping profiles (#186). Sits right under the mapping rows because it
        // acts on exactly what they show: the CURRENT player and the CURRENT scope.
        // Save snapshots that map; applying a profile overwrites it. Feel settings
        // (deadzone/sensitivity/rumble) are deliberately not part of a profile — see
        // ControllerMappings' profile section for why.
        CollapsibleSection(str("pad.section.padProfiles"), initiallyExpanded = false) {
            SettingsDivider()
            HelpText(str("pad.padProfiles.info"))
            val newName = remember { mutableStateOf("") }
            val profiles = remember { mutableStateOf<List<String>>(emptyList()) }
            // listProfiles() reads the portable inputprofiles/ folder, so it rides an IO
            // hop rather than landing in composition. Re-runs when a profile is
            // added/removed (padProfileTick).
            LaunchedEffect(ControllerMappings.padProfileTick.value) {
                profiles.value = withContext(Dispatchers.IO) { ControllerMappings.listProfiles() }
            }
            if (profiles.value.isEmpty()) {
                HelpText(str("pad.padProfiles.none"))
            }
            profiles.value.forEach { name ->
                val apply: () -> Unit = {
                    ControllerMappings.applyProfile(name, editPlayer.intValue, liveEditSerial())
                    capture.value = null
                    // The binding rows above read straight from prefs, so they only show
                    // the applied map once this bumps.
                    refreshToken.intValue++
                }
                Row(
                    Modifier
                        .fillMaxWidth()
                        .height(52.dp)
                        .clip(RoundedCornerShape(16.dp))
                        .background(rowAura())
                        .clickable { apply() }
                        .controllerFocusable(
                            controllerId = "pad-profile:$name",
                            onConfirm = apply,
                        )
                        .padding(horizontal = 12.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(
                        name,
                        color = Color.White,
                        fontSize = 15.sp,
                        fontWeight = FontWeight.SemiBold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f),
                    )
                    Text(
                        str("action.delete"),
                        color = Color(0xFFFF6B6B),
                        fontSize = 12.sp,
                        modifier = Modifier
                            .clickable { ControllerMappings.deleteProfile(name) }
                            .padding(horizontal = 8.dp, vertical = 6.dp),
                    )
                }
                Spacer(Modifier.height(6.dp))
            }
            Spacer(Modifier.height(4.dp))
            Text(str("pad.padProfiles.saveNewLabel"), color = Color(0xFFAAAAAA), fontSize = 12.sp)
            Spacer(Modifier.height(6.dp))
            Row(
                Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                // Inline field, never a dialog: a Dialog is its own focused window and
                // swallows the pad keys this whole tab exists to configure.
                OutlinedTextField(
                    value = newName.value,
                    onValueChange = { newName.value = it },
                    singleLine = true,
                    placeholder = { Text(str("pad.padProfiles.namePlaceholder"), color = Color(0xFF888888)) },
                    colors = OutlinedTextFieldDefaults.colors(
                        focusedTextColor = Color.White,
                        unfocusedTextColor = Color.White,
                        focusedBorderColor = Colors.pasx2_blue,
                        unfocusedBorderColor = Color(0xFF444455),
                    ),
                    modifier = Modifier.weight(1f),
                )
                val save: () -> Unit = {
                    if (ControllerMappings.saveProfile(newName.value, editPlayer.intValue, liveEditSerial()))
                        newName.value = ""
                }
                Box(
                    Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(Colors.pasx2_blue)
                        .clickable(enabled = newName.value.isNotBlank()) { save() }
                        .controllerFocusable(
                            controllerId = "pad-profile-save",
                            onConfirm = save,
                        )
                        .padding(horizontal = 14.dp, vertical = 12.dp),
                ) {
                    Text(
                        str("pad.padProfiles.saveAs"),
                        color = Color.White,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.SemiBold,
                    )
                }
            }
        }
        CollapsibleSection(str("pad.section.onScreenControls"), initiallyExpanded = false) {
            // Controller hotkeys now live in their own dedicated "Hotkeys" tab
            // (see HotkeysTab) so they're easier to find than buried under Pad.
            SettingsDivider()
            val visibilityOff = str("setup.toggle.off")
            val visibilityAuto = str("backend.renderer.auto")
            IntSliderRow(
                label = str("pad.onScreenControls.label"),
                value = TouchControls.visibilityMode.value,
                min = 0,
                max = 11,
                description = str("pad.onScreenControls.description"),
                valueFormatter = { when (it) { 0 -> visibilityOff; 11 -> visibilityAuto; else -> "${it}s" } },
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
                label = str("touch.editor.multiTouchOn"),
                value = (TouchControls.multiTouchRadius.value * 100f).toInt(),
                min = 50,
                max = 95,
                description = str("pad.multiTouch.description"),
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
private fun StickFeelSliders(left: Boolean, title: String, refreshToken: MutableState<Int>) {
    // Subscribe this composable to the token. Each slider's `value` is read from a
    // raw pref (ControllerMappings.stick*), which Compose can't observe — so without
    // this read a bump (fired in every onChange) wouldn't recompose StickFeelSliders
    // (its params are unchanged, so Compose skips it) and the thumb/number would only
    // catch up on menu re-entry. Reading .value here puts the whole function in the
    // token's restart scope, so each drag step refreshes the displayed value live.
    @Suppress("UNUSED_EXPRESSION")
    refreshToken.value
    CollapsibleSection(title, initiallyExpanded = false) {
        SegmentedRow(
            label = str("pad.stickFeel.responseCurve.label"),
            options = listOf(
                str("pad.stickFeel.curve.linear"), str("pad.stickFeel.curve.light"),
                str("pad.stickFeel.curve.medium"), str("pad.stickFeel.curve.strong"),
            ),
            selectedIndex = ControllerMappings.stickResponseCurve(left),
            description = str("pad.stickFeel.responseCurve.description"),
            onChange = { ControllerMappings.setStickResponseCurve(left, it); refreshToken.value++ },
        )
        SettingsDivider()
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
            .height(64.dp)
            .clip(RoundedCornerShape(16.dp))
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
            color = MaterialTheme.colorScheme.onSurface,
            fontSize = 16.sp,
            fontWeight = FontWeight.SemiBold,
        )
        Spacer(Modifier.weight(1f))
        Text(
            str("pad.action.clear"),
            color = Color(0xFFE57373),
            fontSize = 15.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier
                .clickable { clear() }
                .padding(horizontal = 8.dp, vertical = 2.dp),
        )
        Spacer(Modifier.width(4.dp))
        Text(
            ControllerMappings.stickTargetLabel(code),
            color = Color(0xFFCCCCCC),
            fontSize = 15.sp,
            fontWeight = FontWeight.Bold,
        )
    }
    if (showPicker.value) {
        StickTargetPickerDialog(
            title = "${str(if (leftStick) "pad.leftStick.label" else "pad.rightStick.label")} — ${dir.id.replaceFirstChar { it.uppercase() }}",
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
        containerColor = MaterialTheme.colorScheme.surface,
        titleContentColor = MaterialTheme.colorScheme.onSurface,
        textContentColor = MaterialTheme.colorScheme.onSurface,
        title = { Text(title, color = MaterialTheme.colorScheme.onSurface, fontWeight = FontWeight.Bold) },
        text = {
            Column(Modifier.verticalScroll(remember { ScrollState(0) })) {
                Text(
                    str("pad.stickTarget.intro"),
                    color = Color(0xFFBBBBBB), fontSize = 15.sp,
                )
                Spacer(Modifier.height(8.dp))
                StickPickItem(str("pad.stickTarget.analogDefault"), current in 110..123) { onPick(null) }
                Spacer(Modifier.height(6.dp))
                Text(str("pad.stickTarget.ps2Buttons"), color = Colors.pasx2_blue, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                ControllerMappings.stickTargets.forEach { t ->
                    StickPickItem(t.label, current == t.code) { onPick(t.code) }
                }
                Spacer(Modifier.height(6.dp))
                Text(str("pad.stickTarget.hotkeys"), color = Colors.pasx2_blue, fontSize = 14.sp, fontWeight = FontWeight.Bold)
                ControllerMappings.SysHotkey.entries.forEach { h ->
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
            fontSize = 16.sp,
        )
        Text(
            label,
            color = MaterialTheme.colorScheme.onSurface,
            fontSize = 16.sp,
            fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
        )
    }
}

/**
 * Motion / gyroscope controls, shared by the Pad settings tab and the in-game pause
 * menu's Controls tab. [editSerial] selects Global (null) vs per-game scope. Pass
 * [externalRefresh] to share a parent's live-refresh token (the Pad tab does so its
 * scope toggle refreshes every section together); otherwise an internal token drives
 * live re-reads of the raw-pref values Compose can't observe on its own.
 */
@Composable
internal fun GyroSection(
    editSerial: String? = null,
    externalRefresh: androidx.compose.runtime.MutableIntState? = null,
) {
    val ctx = androidx.compose.ui.platform.LocalContext.current
    val localToken = remember { androidx.compose.runtime.mutableIntStateOf(0) }
    val refreshToken = externalRefresh ?: localToken
    CollapsibleSection(str("pad.gyro.section"), initiallyExpanded = false) {
        @Suppress("UNUSED_EXPRESSION")
        refreshToken.intValue
        val gyroMode = ControllerMappings.gyroModeScope(editSerial)
        SegmentedRow(
            label = str("pad.gyro.mode.label"),
            options = listOf(
                str("pad.gyro.mode.off"),
                str("pad.gyro.mode.aim"),
                str("pad.gyro.mode.steering"),
            ),
            selectedIndex = gyroMode,
            onChange = {
                ControllerMappings.setGyroMode(it, editSerial)
                refreshToken.intValue++
            },
        )
        // Which analog stick Aim mode drives — Right for most FPS, Left for games that
        // aim with the left stick (e.g. Resident Evil 4). Only shown in Aim mode.
        if (gyroMode == ControllerMappings.GYRO_AIM) {
            SegmentedRow(
                label = str("pad.gyro.aimStick.label"),
                options = listOf(str("pad.gyro.aimStick.right"), str("pad.gyro.aimStick.left")),
                selectedIndex = ControllerMappings.gyroAimStickScope(editSerial),
                onChange = {
                    ControllerMappings.setGyroAimStick(it, editSerial)
                    refreshToken.intValue++
                },
            )
        }
        // Warn when the picked mode's sensor is missing on this device (aim needs a
        // gyroscope, steering the game rotation vector). The manifest declares the
        // feature not-required, so such devices still install.
        if (gyroMode != 0 &&
            !com.armsx2.input.AndroidGyroscopeInput.isModeAvailable(ctx, gyroMode)) {
            HelpText(str("pad.gyro.unavailable"))
        }
        SettingsDivider()
        IntSliderRow(
            label = str("pad.gyro.sensitivity.label"),
            value = ControllerMappings.gyroSensitivityScope(editSerial),
            min = 25,
            max = 300,
            valueFormatter = { "${it}%" },
            onChange = {
                ControllerMappings.setGyroSensitivity(it, editSerial)
                refreshToken.intValue++
            },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("pad.gyro.smoothing.label"),
            value = ControllerMappings.gyroSmoothingScope(editSerial),
            min = 0,
            max = 90,
            valueFormatter = { "${it}%" },
            onChange = {
                ControllerMappings.setGyroSmoothing(it, editSerial)
                refreshToken.intValue++
            },
        )
        SettingsDivider()
        ToggleRow(
            str("pad.gyro.invertX.label"),
            ControllerMappings.gyroInvertXScope(editSerial),
        ) {
            ControllerMappings.setGyroInvertX(it, editSerial)
            refreshToken.intValue++
        }
        SettingsDivider()
        ToggleRow(
            str("pad.gyro.invertY.label"),
            ControllerMappings.gyroInvertYScope(editSerial),
        ) {
            ControllerMappings.setGyroInvertY(it, editSerial)
            refreshToken.intValue++
        }
        SettingsDivider()
    }
}

/**
 * Macros — 4 combo buttons, each firing a chosen SET of pad buttons at once (e.g. R1+R2+R3).
 * Shared between the full Pad settings tab and the in-game Controls tab. Tap a row to pick
 * its buttons (the M1-M4 on-screen buttons + any physical trigger fire that set).
 *
 * Physical-trigger binding needs a capture host (the Pad tab's root key listener), so the
 * "Bind"/"Clear" column only renders when [onArmCapture] is supplied. In the in-game quick
 * menu both params are null: you can still edit each macro's button set, just not bind a
 * controller button to it (do that from All Settings › Controls).
 */
@Composable
internal fun MacrosSection(
    macroCapture: MutableState<TouchButtonId?>? = null,
    onArmCapture: ((TouchButtonId) -> Unit)? = null,
) {
    val macroDialogFor = remember { mutableStateOf<TouchButtonId?>(null) }
    // Recompose when any macro's button set / physical bind changes.
    @Suppress("UNUSED_EXPRESSION")
    TouchControls.macroBindTick.value
    val physicalSupported = onArmCapture != null
    CollapsibleSection(str("pad.section.macros"), initiallyExpanded = false) {
        Text(
            str("pad.macros.header"),
            color = Color(0xFFBBBBBB),
            fontSize = 15.sp,
            modifier = Modifier.padding(horizontal = 6.dp, vertical = 4.dp),
        )
        listOf(TouchButtonId.MACRO1, TouchButtonId.MACRO2, TouchButtonId.MACRO3, TouchButtonId.MACRO4).forEach { mid ->
            val buttons = TouchControls.macroCodes(mid)
            val summary = if (buttons.isEmpty()) str("pad.macro.notSet")
            else buttons.joinToString(" + ") { TouchControls.macroTargetFor(it)?.label ?: "?" }
            val physCode = TouchControls.macroPhysicalCode(mid)
            val capturingThis = macroCapture?.value == mid
            Row(
                Modifier
                    .fillMaxWidth()
                    .height(72.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(rowAura())
                    .clickable { macroDialogFor.value = mid }
                    .controllerFocusable(
                        controllerId = "pad-macro-${mid.name}",
                        onConfirm = { macroDialogFor.value = mid },
                    )
                    .padding(horizontal = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(mid.label, color = Colors.pasx2_blue, fontSize = 16.sp, fontWeight = FontWeight.Bold)
                Spacer(Modifier.width(10.dp))
                Column(Modifier.weight(1f)) {
                    Text(summary, color = Color(0xFFCCCCCC), fontSize = 15.sp)
                    if (physicalSupported) {
                        Text(
                            when {
                                capturingThis -> str("pad.pressControllerButton")
                                physCode != android.view.KeyEvent.KEYCODE_UNKNOWN ->
                                    "Controller: ${ControllerMappings.labelForKey(physCode)}"
                                else -> str("pad.controller.notBound")
                            },
                            color = if (capturingThis) Color(0xFFFFD33A) else Color(0xFF999999),
                            fontSize = 14.sp,
                        )
                    }
                }
                if (physicalSupported) {
                    if (physCode != android.view.KeyEvent.KEYCODE_UNKNOWN && !capturingThis) {
                        Text(
                            str("pad.action.clear"),
                            color = Color(0xFFFF6B6B), fontSize = 14.sp, fontWeight = FontWeight.Bold,
                            modifier = Modifier
                                .clickable { TouchControls.clearMacroPhysicalCode(mid) }
                                .padding(end = 10.dp),
                        )
                    }
                    Text(
                        if (capturingThis) str("action.cancel") else str("pad.action.bind"),
                        color = Colors.pasx2_blue, fontSize = 14.sp, fontWeight = FontWeight.Bold,
                        modifier = Modifier
                            .clickable { onArmCapture?.invoke(mid) }
                            .padding(end = 10.dp),
                    )
                }
                Text(str("pad.action.edit"), color = Colors.pasx2_blue, fontSize = 14.sp, fontWeight = FontWeight.Bold)
            }
            // Frequency (turbo). Only once the macro fires something — a rate for a macro
            // with no buttons is a row that does nothing. Lives HERE rather than in the
            // edit dialog because a Dialog is its own focused window and swallows the pad;
            // as a normal row it's reachable with a controller like everything else.
            if (buttons.isNotEmpty()) {
                val freq = TouchControls.macroFrequency(mid)
                // str() is @Composable and valueFormatter is a plain lambda, so resolve
                // both up-front rather than calling into it from there.
                val holdLabel = str("pad.macro.frequency.hold")
                val everyLabel = str("pad.macro.frequency.every")
                IntSliderRow(
                    label = str("pad.macro.frequency.label"),
                    value = freq,
                    min = 0,
                    max = TouchControls.MACRO_FREQ_MAX,
                    description = str("pad.macro.frequency.description"),
                    valueFormatter = { if (it == 0) holdLabel else everyLabel.format(it) },
                    onReset = if (freq == 0) null else ({ TouchControls.setMacroFrequency(mid, 0) }),
                    onChange = { TouchControls.setMacroFrequency(mid, it) },
                )
            }
            SettingsDivider()
        }
        macroDialogFor.value?.let { mid ->
            MacroConfigDialog(
                macroId = mid,
                onSaved = { },
                onDismiss = { macroDialogFor.value = null },
            )
        }
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
        mutableStateListOf<Int>().apply { addAll(TouchControls.macroCodes(macroId)) }
    }
    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = MaterialTheme.colorScheme.surface,
        titleContentColor = MaterialTheme.colorScheme.onSurface,
        textContentColor = MaterialTheme.colorScheme.onSurface,
        title = { Text("${str("pad.action.edit")}: ${macroId.label}", color = MaterialTheme.colorScheme.onSurface, fontWeight = FontWeight.Bold) },
        text = {
            Column(Modifier.verticalScroll(remember { ScrollState(0) })) {
                Text(
                    str("pad.macroConfig.intro"),
                    color = Color(0xFFBBBBBB), fontSize = 15.sp,
                )
                Spacer(Modifier.height(8.dp))
                TouchControls.macroAssignableTargets.forEach { t ->
                    val on = t.code in selected
                    Row(
                        Modifier
                            .fillMaxWidth()
                            .height(52.dp)
                            .clickable { if (on) selected.remove(t.code) else selected.add(t.code) }
                            .padding(horizontal = 4.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        Text(
                            if (on) "☑" else "☐",
                            color = if (on) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                            fontSize = 16.sp,
                        )
                        Spacer(Modifier.width(12.dp))
                        Text(t.label, color = MaterialTheme.colorScheme.onSurface, fontSize = 14.sp)
                    }
                }
            }
        },
        confirmButton = {
            TextButton(onClick = {
                TouchControls.setMacroCodes(macroId, selected.toList())
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
            .height(64.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(rowAura())
            .clickable(onClick = onClick)
            .controllerFocusable(
                controllerId = "pad:${action.id}",
                onConfirm = onClick,
            )
            .padding(horizontal = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(action.label, color = MaterialTheme.colorScheme.onSurface, fontSize = 16.sp, fontWeight = FontWeight.SemiBold)
        Spacer(Modifier.weight(1f))
        // "Clear" unbinds the button (leaves it blank, free to assign as a hotkey) —
        // mirrors the Hotkeys tab. Shown only when bound and not mid-capture.
        if (!capturing && physical != android.view.KeyEvent.KEYCODE_UNKNOWN) {
            Text(
                str("pad.action.clear"),
                color = Color(0xFFFF6B6B),
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier
                    .clickable(onClick = onClear)
                    .padding(end = 10.dp),
            )
        }
        Text(
            if (capturing) str("pad.pressButton") else ControllerMappings.labelForKey(physical),
            color = if (capturing) Color(0xFFFFD33A) else Color(0xFFCCCCCC),
            fontSize = 15.sp,
            fontWeight = FontWeight.Bold,
        )
    }
}
