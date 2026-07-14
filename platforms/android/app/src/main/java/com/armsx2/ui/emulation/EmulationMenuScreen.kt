package com.armsx2.ui.emulation

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.EaseIn
import androidx.compose.animation.core.EaseOut
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.relocation.BringIntoViewRequester
import androidx.compose.foundation.relocation.bringIntoViewRequester
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.layout.layout
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import coil.compose.AsyncImage
import com.armsx2.i18n.str
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.InGameOverlay
import com.armsx2.ui.achievements.AchievementItem
import com.armsx2.ui.common.GameCoverArt
import com.armsx2.ui.settings.controllerFocusable
import com.armsx2.ui.theme.Danger
import com.armsx2.ui.theme.Success
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

@Composable
fun EmulationMenuScreen(viewModel: EmulationMenuViewModel = viewModel()) {
    val state = viewModel.state.value
    val scope = rememberCoroutineScope()
    var shown by remember { mutableStateOf(false) }
    var dismissing by remember { mutableStateOf(false) }
    val closeMenu: () -> Unit = remember(viewModel, scope) {
        {
            if (!dismissing) {
                dismissing = true
                shown = false
                scope.launch {
                    delay(220)
                    viewModel.dismissHandler = null
                    viewModel.resumeImmediately()
                }
            }
        }
    }

    DisposableEffect(viewModel, closeMenu) {
        viewModel.dismissHandler = closeMenu
        EmulationMenuInputController.bind(viewModel)
        onDispose {
            viewModel.dismissHandler = null
            EmulationMenuInputController.unbind(viewModel)
        }
    }
    LaunchedEffect(Unit) { shown = true }
    BackHandler(onBack = closeMenu)

    state.pendingHardcore?.let { enabling ->
        AlertDialog(
            onDismissRequest = viewModel::cancelToggleHardcore,
            title = { Text(str(if (enabling) "ra.hardcore.enable.title" else "ra.hardcore.disable.title")) },
            text = { Text(str(if (enabling) "ra.hardcore.enable.body" else "ra.hardcore.disable.body")) },
            confirmButton = {
                TextButton(onClick = viewModel::confirmToggleHardcore) {
                    Text(str(if (enabling) "ra.hardcore.enable.confirm" else "ra.hardcore.disable.confirm"))
                }
            },
            dismissButton = { TextButton(onClick = viewModel::cancelToggleHardcore) { Text(str("action.cancel")) } },
        )
    }

    BoxWithConstraints(Modifier.fillMaxSize()) {
        val compact = maxWidth < 700.dp
        AnimatedVisibility(
            visible = shown,
            enter = fadeIn(tween(190, easing = EaseOut)),
            exit = fadeOut(tween(190, easing = EaseIn)),
        ) {
            Box(
                Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.62f))
                    .clickable(onClick = closeMenu),
            )
        }
        AnimatedVisibility(
            visible = shown,
            enter = slideInHorizontally(tween(320, easing = EaseOut)) { it },
            exit = slideOutHorizontally(tween(220, easing = EaseIn)) { it },
            modifier = Modifier.align(Alignment.CenterEnd),
        ) {
            if (compact) {
                Surface(
                    modifier = Modifier
                        .fillMaxHeight()
                        .fillMaxWidth(0.96f)
                        .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Vertical)),
                    shape = RoundedCornerShape(topStart = 28.dp, bottomStart = 28.dp),
                    color = MaterialTheme.colorScheme.surface,
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
                    shadowElevation = 22.dp,
                ) {
                    MenuPage(state, viewModel, compact = true, modifier = Modifier.fillMaxSize())
                }
            } else {
                Row(
                    modifier = Modifier
                        .fillMaxHeight()
                        .fillMaxWidth(0.64f)
                        .widthIn(max = 900.dp)
                        .windowInsetsPadding(WindowInsets.safeDrawing.only(WindowInsetsSides.Vertical))
                        .padding(top = 14.dp, end = 12.dp, bottom = 14.dp),
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    Surface(
                        modifier = Modifier.weight(1f).fillMaxHeight(),
                        shape = RoundedCornerShape(28.dp),
                        color = MaterialTheme.colorScheme.surface,
                        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
                        shadowElevation = 22.dp,
                    ) {
                        MenuPage(
                            state = state,
                            viewModel = viewModel,
                            compact = false,
                            modifier = Modifier.fillMaxSize(),
                        )
                    }
                    Surface(
                        modifier = Modifier.width(76.dp).fillMaxHeight(),
                        shape = RoundedCornerShape(28.dp),
                        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.94f),
                        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
                        shadowElevation = 18.dp,
                    ) {
                        MenuRail(state.tab, viewModel::selectTab, viewModel::openFullSettings)
                    }
                }
            }
        }
    }
}

@Composable
private fun MenuPage(
    state: EmulationMenuUiState,
    viewModel: EmulationMenuViewModel,
    compact: Boolean,
    modifier: Modifier,
) {
    val tabScrollStates = remember {
        EmulationMenuTab.entries.associateWith {
            ScrollState(initial = InGameOverlay.menuTabScroll[it.name] ?: 0)
        }
    }
    // Remember each tab's scroll offset when the menu closes so reopening a tab (especially
    // the long Fixes list) returns to where you were instead of snapping back to the top.
    androidx.compose.runtime.DisposableEffect(Unit) {
        onDispose {
            tabScrollStates.forEach { (tab, ss) -> InGameOverlay.menuTabScroll[tab.name] = ss.value }
        }
    }
    val scrollState = tabScrollStates.getValue(state.tab)
    // Provide the pane's scroll state to the settings widgets so the Fixes pane's
    // right-stick free-scroll (settingsScrollState / ControllerAutoScroll) drives the
    // pane the user is actually looking at. Per-control bring-into-view handles the
    // primary "keep selection on screen" via the nearest scrollable ancestor already.
    androidx.compose.runtime.CompositionLocalProvider(
        com.armsx2.ui.settings.LocalSettingsScrollState provides scrollState,
    ) {
        Column(
            modifier
                .verticalScroll(scrollState)
                .padding(bottom = 18.dp),
        ) {
            if (compact) CompactMenuTabs(state.tab, viewModel::selectTab)
            MenuHeader(compact, state.hardcore, state.richPresence)
            HorizontalDivider(
                modifier = Modifier.padding(horizontal = 8.dp),
                color = MaterialTheme.colorScheme.outline.copy(alpha = 0.34f),
            )
            Column(
                Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 12.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                when (state.tab) {
                    EmulationMenuTab.Session -> SessionPane(state, viewModel)
                    EmulationMenuTab.Graphics -> GraphicsPane(state, viewModel)
                    // The full Fixes settings tab, live-applying via InGameOverlay's
                    // shared Settings state (same as the rest of the overlay); its
                    // controls are SettingsControllerNav items, so the pause menu's
                    // content-pane nav drives them for free.
                    EmulationMenuTab.Fixes -> com.armsx2.ui.settings.FixesTab(InGameOverlay.settingsState)
                    EmulationMenuTab.Performance -> PerformancePane(state, viewModel)
                    EmulationMenuTab.Controls -> ControlsPane(state, viewModel)
                    EmulationMenuTab.Options -> OptionsPane(state, viewModel)
                    EmulationMenuTab.Achievements -> AchievementsPane(state, viewModel)
                }
            }
        }
    }
}

@Composable
private fun CompactMenuTabs(selected: EmulationMenuTab, onSelect: (EmulationMenuTab) -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .horizontalScroll(rememberScrollState())
            .padding(horizontal = 8.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        EmulationMenuTab.entries.forEach { tab ->
            MenuTab(tab, tab == selected, onSelect)
        }
    }
}

@Composable
private fun MenuRail(
    selected: EmulationMenuTab,
    onSelect: (EmulationMenuTab) -> Unit,
    onAllSettings: () -> Unit,
) {
    Column(
        Modifier
            .fillMaxHeight()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 8.dp, vertical = 10.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        EmulationMenuTab.entries.forEach { tab ->
            MenuRailTab(tab, tab == selected, onSelect)
        }
        // Always-visible shortcut to the full settings screen — otherwise only reachable via the
        // Options tab. Sits at the bottom of the rail, set apart from the tab buttons.
        HorizontalDivider(
            modifier = Modifier.padding(vertical = 2.dp).width(40.dp),
            color = MaterialTheme.colorScheme.outline.copy(alpha = 0.34f),
        )
        MenuRailAction("⤢", str("action.allSettings"), onAllSettings)
    }
}

@Composable
private fun MenuRailAction(glyph: String, label: String, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        modifier = Modifier.size(56.dp).semantics { contentDescription = label },
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.55f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.5f)),
    ) {
        Box(contentAlignment = Alignment.Center) {
            Text(
                text = glyph,
                fontSize = 22.sp,
                fontWeight = FontWeight.Bold,
                color = MaterialTheme.colorScheme.primary,
            )
        }
    }
}

@Composable
private fun MenuRailTab(tab: EmulationMenuTab, active: Boolean, onSelect: (EmulationMenuTab) -> Unit) {
    val bring = remember { BringIntoViewRequester() }
    val label = str(tab.titleKey)
    LaunchedEffect(active) { if (active) runCatching { bring.bringIntoView() } }
    Surface(
        onClick = { onSelect(tab) },
        modifier = Modifier.size(56.dp).bringIntoViewRequester(bring).semantics { contentDescription = label },
        shape = RoundedCornerShape(18.dp),
        color = if (active) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface.copy(alpha = 0.5f),
        border = BorderStroke(
            if (active) 2.dp else 1.dp,
            if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.34f),
        ),
    ) {
        Box(contentAlignment = Alignment.Center) {
            Text(
                text = tabGlyph(tab),
                fontSize = 23.sp,
                fontWeight = FontWeight.Bold,
                color = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun MenuTab(tab: EmulationMenuTab, active: Boolean, onSelect: (EmulationMenuTab) -> Unit) {
    // Keep the active tab scrolled into view so controller nav reaches tabs that fall off
    // the rail on short screens — e.g. the 7th "Achievements" (RA) tab on a Retroid Pocket
    // in landscape. Mirrors the settings-hub / library camera-follow. Resolves against the
    // nearest scrollable ancestor, so it works for both the vertical rail and the compact
    // horizontal strip.
    val bring = remember { BringIntoViewRequester() }
    LaunchedEffect(active) { if (active) runCatching { bring.bringIntoView() } }
    Surface(
        onClick = { onSelect(tab) },
        modifier = Modifier
            .widthIn(min = 132.dp, max = 210.dp)
            .padding(vertical = 3.dp)
            .bringIntoViewRequester(bring),
        shape = RoundedCornerShape(14.dp),
        color = if (active) MaterialTheme.colorScheme.primaryContainer
        else MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.56f),
        border = BorderStroke(
            if (active) 1.5.dp else 1.dp,
            if (active) MaterialTheme.colorScheme.primary.copy(alpha = 0.72f)
            else MaterialTheme.colorScheme.outline.copy(alpha = 0.32f),
        ),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 11.dp, vertical = 9.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Surface(
                modifier = Modifier.size(30.dp),
                shape = RoundedCornerShape(9.dp),
                color = if (active) MaterialTheme.colorScheme.primary.copy(alpha = 0.14f)
                else MaterialTheme.colorScheme.surface.copy(alpha = 0.72f),
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Text(
                        text = tabGlyph(tab),
                        fontSize = 16.sp,
                        fontWeight = FontWeight.Bold,
                        color = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            }
            Text(
                text = str(tab.titleKey),
                color = if (active) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.labelLarge,
                fontWeight = if (active) FontWeight.Bold else FontWeight.Medium,
                maxLines = 2,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

// Rail tab icons. No monochrome Unicode exists for gamepad/wrench/trophy/display, so those
// use color emoji (the bundled NotoColorEmoji renders them); Session/Performance keep their
// clean text glyphs. Options carries the settings gear; the full-settings shortcut below the
// rail divider uses a distinct "open" glyph so there aren't two gears.
private fun tabGlyph(tab: EmulationMenuTab): String = when (tab) {
    EmulationMenuTab.Session -> "☰"
    EmulationMenuTab.Graphics -> "🖥️"
    EmulationMenuTab.Fixes -> "🔧"
    EmulationMenuTab.Performance -> "↯"
    EmulationMenuTab.Controls -> "🎮"
    EmulationMenuTab.Options -> "⚙"
    EmulationMenuTab.Achievements -> "🏆"
}

@Composable
private fun MenuHeader(compact: Boolean, hardcore: Boolean, richPresence: String) {
    val game = MainActivityRuntime.currentGame.value
    Row(
        Modifier.fillMaxWidth().padding(horizontal = if (compact) 12.dp else 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (game != null) {
            GameCoverArt(game, Modifier.width(if (compact) 38.dp else 44.dp).height(if (compact) 52.dp else 60.dp))
            Spacer(Modifier.width(11.dp))
        }
        Column(Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    game?.title ?: "PlayStation 2",
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    color = MaterialTheme.colorScheme.onSurface,
                    modifier = Modifier.weight(1f, fill = false),
                )
                if (hardcore) {
                    Spacer(Modifier.width(8.dp))
                    HardcoreBadge()
                }
                // File-type chip after the HC badge (ISO / CHD / …), mirroring the library
                // list view so the pause/RA header shows the same at-a-glance file info.
                game?.let { g ->
                    Spacer(Modifier.width(6.dp))
                    com.armsx2.ui.common.StatusChip(g.extension.ifBlank { g.platform.key.uppercase() })
                }
            }
            if (!game?.serial.isNullOrBlank()) {
                Spacer(Modifier.height(2.dp))
                Text(
                    game?.serial.orEmpty(),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            // RetroAchievements rich presence — the live "what you're doing" line
            // (e.g. "Pooh & Piglet are in a Scaring Contest"). Restored from the old UI.
            if (richPresence.isNotBlank()) {
                Spacer(Modifier.height(2.dp))
                Text(
                    richPresence,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}

@Composable
private fun SessionPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    ActionGrid(
        actions = listOf(
            MenuAction(str("action.resume"), str("action.play"), "▶", Success, viewModel::resume),
            MenuAction(str("memcard.restart"), str("action.reset"), "↻", null, MainActivityRuntime::restart),
            MenuAction(str("action.swapDisc"), str("action.swapDisc.detail"), "⏏", null, MainActivityRuntime::promptSwapDisc),
            MenuAction(str("action.close"), MainActivityRuntime.currentGame.value?.title.orEmpty(), "■", Danger) {
                MainActivityRuntime.stop(false)
            },
        ),
        selected = state.selectedAction,
        onSelect = viewModel::selectAction,
    )
    // On-screen display — a single universal on/off (old-UI style); the per-stat
    // toggles live in All Settings. Plus a frame-limit switch so fast-forward is one
    // tap away.
    SectionCard(str("tab.overlay")) {
        // Full OSD = any verbose stat line (FPS is shared with Simple, so it's excluded
        // from the "full is on" test); Simple OSD = FPS only. Reading the same osdShow*
        // fields two ways keeps the two toggles mutually exclusive.
        val osdFullOn = with(state.settings) {
            osdShowVps || osdShowSpeed || osdShowCpu || osdShowGpu || osdShowResolution ||
                osdShowGsStats || osdShowFrameTimes || osdShowHardwareInfo || osdShowGpuStats || osdShowVersion
        }
        val osdSimpleOn = state.settings.osdShowFps && !osdFullOn
        MenuSwitchRow(str("overlay.master.label"), osdFullOn) { viewModel.setOsdMaster(it) }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow(str("overlay.simple.label"), osdSimpleOn) { viewModel.setOsdSimple(it) }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow(str("perf.frameLimit.label"), state.settings.frameLimitEnable) { value ->
            viewModel.updateSettings { it.copy(frameLimitEnable = value) }
        }
    }
    SectionCard(str("savestate.title.loadManage")) {
        Text(
            "${str("memcard.slot1").substringBefore(' ')} ${state.saveSlot + 1}",
            style = MaterialTheme.typography.titleMedium,
            color = MaterialTheme.colorScheme.onSurface,
        )
        Spacer(Modifier.height(8.dp))
        Row(
            Modifier
                .fillMaxWidth()
                .bleedHorizontal(13.dp)
                .horizontalScroll(rememberScrollState())
                .padding(horizontal = 13.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            repeat(10) { slot ->
                OptionChip(
                    label = "${slot + 1}",
                    selected = slot == state.saveSlot,
                    controllerId = "pause.saveslot.$slot",
                    onClick = { viewModel.setSaveSlot(slot) },
                )
            }
        }
        Spacer(Modifier.height(9.dp))
        // Save / Load open the rich slot picker (thumbnails + autosave + the
        // auto-save/-load toggles), matching the old UI. The slot chips above stay
        // the quick-slot selector used by the on-screen / hotkey quick-save.
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            CompactAction(str("savestate.title.save"), "↥", Modifier.weight(1f)) {
                com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.SaveState)
            }
            CompactAction(str("touch.stateAction.load"), "↧", Modifier.weight(1f)) {
                com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.LoadState)
            }
        }
    }
}

@Composable
private fun GraphicsPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    val settings = state.settings
    HorizontalOptions(
        title = str("tab.renderer"),
        options = listOf(
            "auto" to str("backend.renderer.auto"),
            "vulkan" to "Vulkan",
            "opengl" to "OpenGL",
            "software" to str("backend.renderer.software"),
        ),
        selected = settings.renderer,
        onSelect = viewModel::setRenderer,
    )
    // GPU driver manager (download/import/select) — Vulkan only — plus Apply &
    // Restart, since renderer + driver changes only take effect on renderer init.
    if (settings.renderer == "vulkan") {
        com.armsx2.ui.common.DriverManagerSection()
    }
    CompactAction(str("backend.applyRestart"), "↻", Modifier.fillMaxWidth(), MainActivityRuntime::restart)
    HorizontalOptions(
        title = str("renderer.upscale.label"),
        // Share the full settings-tab list so the sub-native 0.25/0.5/0.75/Native
        // options aren't dropped in the in-game quick menu.
        options = com.armsx2.ui.settings.UPSCALE_OPTIONS.map { it.value to it.label },
        selected = settings.upscaleFloat,
        onSelect = viewModel::setUpscale,
    )
    HorizontalOptions(
        title = str("renderer.displayMode.label"),
        options = listOf(
            0 to str("setup.aspect.stretch"),
            1 to str("setup.aspect.auto"),
            2 to "4:3",
            3 to "16:9",
            4 to "10:7",
        ),
        selected = settings.aspectRatio,
        onSelect = viewModel::setAspectRatio,
    )
    HorizontalOptions(
        title = str("renderer.blendingAccuracy.label"),
        options = listOf("Minimum", "Basic", "Medium", "High", str("fixes.opt.full"), str("fixes.opt.max")).mapIndexed { index, label -> index to label },
        selected = settings.accurateBlendingUnit,
        onSelect = viewModel::setBlending,
    )
    HorizontalOptions(
        title = str("renderer.textureFiltering.label"),
        options = listOf("Nearest", str("fixes.opt.forced"), "PS2", str("fixes.opt.sprite")).mapIndexed { index, label -> index to label },
        selected = settings.textureFiltering,
        onSelect = viewModel::setTextureFiltering,
    )
    HorizontalOptions(
        title = str("renderer.texturePreloading.label"),
        options = listOf(str("fixes.opt.off"), "Partial", str("fixes.opt.full")).mapIndexed { index, label -> index to label },
        selected = settings.texturePreloading,
        onSelect = viewModel::setTexturePreloading,
    )
    HorizontalOptions(
        title = str("renderer.hardwareDownloadMode.label"),
        options = listOf("Accurate", "Force Full", "No Readbacks", "Unsync", "Disabled").mapIndexed { index, label -> index to label },
        selected = settings.hardwareDownloadMode,
        onSelect = viewModel::setHardwareDownloadMode,
    )
    HorizontalOptions(
        title = str("renderer.deinterlacing.label"),
        options = listOf("Auto", "Off", "Weave TFF", "Weave BFF", "Bob TFF", "Bob BFF", "Blend TFF", "Blend BFF", "Adapt TFF", "Adapt BFF")
            .mapIndexed { index, label -> index to label },
        selected = settings.deinterlaceMode,
        onSelect = { value -> viewModel.updateSettings { it.copy(deinterlaceMode = value) } },
    )
    HorizontalOptions(
        title = str("renderer.displayFilter.label"),
        options = listOf("Nearest", "Smooth", "Sharp").mapIndexed { index, label -> index to label },
        selected = settings.displayBilinear,
        onSelect = { value -> viewModel.updateSettings { it.copy(displayBilinear = value) } },
    )
    HorizontalOptions(
        title = str("renderer.tvShader.label"),
        options = listOf("Off", "Scanline", "Diagonal", "Tri", "Wave", "Lottes", "4xRGSS", "NxAGSS")
            .mapIndexed { index, label -> index to label },
        selected = settings.tvShader,
        onSelect = { value -> viewModel.updateSettings { it.copy(tvShader = value) } },
    )
    HorizontalOptions(
        title = str("fixes.dithering.label"),
        options = listOf(str("fixes.opt.off"), str("fixes.opt.scaled"), str("fixes.opt.unscaled"), str("fixes.opt.force32"))
            .mapIndexed { index, label -> index to label },
        selected = settings.dithering,
        onSelect = { value -> viewModel.updateSettings { it.copy(dithering = value) } },
    )
    MenuSwitchRow(str("renderer.hwMipmapping.label"), settings.hwMipmap) {
        viewModel.updateSettings { current -> current.copy(hwMipmap = it) }
    }
    MenuSwitchRow(str("fixes.integerScaling.label"), settings.integerScaling) {
        viewModel.updateSettings { current -> current.copy(integerScaling = it) }
    }
    MenuSwitchRow("VSync", settings.vsyncEnable) {
        viewModel.updateSettings { current -> current.copy(vsyncEnable = it) }
    }
    MenuSwitchRow(str("renderer.shadeboost.label"), settings.shadeBoost) {
        viewModel.updateSettings { current -> current.copy(shadeBoost = it) }
    }
    MenuSwitchRow(str("fixes.antiBlur.label"), settings.antiBlur) {
        viewModel.updateSettings { current -> current.copy(antiBlur = it) }
    }
    MenuSwitchRow(str("fixes.screenOffsets.label"), settings.screenOffsets) {
        viewModel.updateSettings { current -> current.copy(screenOffsets = it) }
    }
    MenuSwitchRow(str("fixes.showOverscan.label"), settings.showOverscan) {
        viewModel.updateSettings { current -> current.copy(showOverscan = it) }
    }
    MenuSwitchRow(str("fixes.syncToHostRefresh.label"), settings.syncToHostRefresh) {
        viewModel.updateSettings { current -> current.copy(syncToHostRefresh = it) }
    }
    MenuSwitchRow(str("renderer.loadTexturePacks.label"), settings.loadTextureReplacements) {
        viewModel.updateSettings { current -> current.copy(loadTextureReplacements = it) }
    }
    MenuSwitchRow(str("renderer.asyncTextureLoading.label"), settings.loadTextureReplacementsAsync) {
        viewModel.updateSettings { current -> current.copy(loadTextureReplacementsAsync = it) }
    }
    MenuSwitchRow(str("renderer.precacheTexturePacks.label"), settings.precacheTextureReplacements) {
        viewModel.updateSettings { current -> current.copy(precacheTextureReplacements = it) }
    }
}

@Composable
private fun PerformancePane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    val settings = state.settings
    SectionCard(str("perf.speedLimit.label")) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(
                if (settings.frameLimitEnable) "${settings.nominalSpeedPercent}%" else str("setup.toggle.off"),
                modifier = Modifier.weight(1f),
                color = MaterialTheme.colorScheme.primary,
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold,
            )
            Switch(
                checked = settings.frameLimitEnable,
                onCheckedChange = { enabled ->
                    viewModel.updateSettings { it.copy(frameLimitEnable = enabled) }
                },
            )
        }
        Spacer(Modifier.height(8.dp))
        HorizontalOptionRow(
            options = listOf(50, 75, 90, 100, 110, 125, 150, 200).map { it to "$it%" },
            selected = settings.nominalSpeedPercent,
            keyPrefix = str("perf.speedLimit.label"),
            onSelect = viewModel::setSpeed,
        )
    }
    HorizontalOptions(
        title = str("perf.displayFpsCap.label"),
        options = listOf(0, 20, 30, 45, 60, 90, 120).map {
            it to if (it == 0) str("setup.toggle.off") else "$it FPS"
        },
        selected = settings.fpsLimit,
        onSelect = viewModel::setFpsLimit,
    )
    HorizontalOptions(
        title = str("perf.frameSkip.label"),
        options = (0..5).map { it to if (it == 0) str("setup.toggle.off") else "$it" },
        selected = settings.frameSkip,
        onSelect = viewModel::setFrameSkip,
    )
    FramerateSlider(
        title = str("perf.ntscFramerate.label"),
        value = settings.framerateNtsc,
        onValue = { value -> viewModel.updateSettings { it.copy(framerateNtsc = value) } },
    )
    FramerateSlider(
        title = str("perf.palFramerate.label"),
        value = settings.frameratePal,
        onValue = { value -> viewModel.updateSettings { it.copy(frameratePal = value) } },
    )
    HorizontalOptions(
        title = str("perf.eeCycleRate.label"),
        options = (-3..3).map { it to if (it > 0) "+$it" else "$it" },
        selected = settings.eeCycleRate,
        onSelect = viewModel::setEeCycleRate,
    )
    HorizontalOptions(
        title = str("perf.eeCycleSkip.label"),
        options = (0..3).map { it to "$it" },
        selected = settings.eeCycleSkip,
        onSelect = viewModel::setEeCycleSkip,
    )
    HorizontalOptions(
        title = str("perf.eeFpuClamping.label"),
        options = listOf(str("perf.clamp.none"), str("perf.clamp.normal"), str("perf.clamp.extra"), str("perf.clamp.full"))
            .mapIndexed { index, label -> index to label },
        selected = settings.eeClampMode,
        onSelect = { value -> viewModel.updateSettings { it.copy(eeClampMode = value) } },
    )
    HorizontalOptions(
        title = str("perf.vuClamping.label"),
        options = listOf(str("perf.clamp.none"), str("perf.clamp.normal"), str("perf.clamp.extra"), str("perf.clamp.extraSign"))
            .mapIndexed { index, label -> index to label },
        selected = settings.vuClampMode,
        onSelect = { value -> viewModel.updateSettings { it.copy(vuClampMode = value) } },
    )
    HorizontalOptions(
        title = str("perf.eeFpuRoundMode.label"),
        options = listOf(str("perf.round.nearest"), str("perf.round.negative"), str("perf.round.positive"), str("perf.round.chop"))
            .mapIndexed { index, label -> index to label },
        selected = settings.eeFpuRoundMode,
        onSelect = { value -> viewModel.updateSettings { it.copy(eeFpuRoundMode = value) } },
    )
    MenuSwitchRow(str("perf.hack.mtvu"), settings.mtvu) {
        viewModel.updateSettings { current -> current.copy(mtvu = it) }
    }
    MenuSwitchRow(str("perf.hack.instantVu1"), settings.vu1Instant) {
        viewModel.updateSettings { current -> current.copy(vu1Instant = it) }
    }
    MenuSwitchRow(str("perf.hack.fastCdvd"), settings.fastCDVD) {
        viewModel.updateSettings { current -> current.copy(fastCDVD = it) }
    }
    MenuSwitchRow(str("perf.hack.skipDupeFrames"), settings.skipDuplicateFrames) {
        viewModel.updateSettings { current -> current.copy(skipDuplicateFrames = it) }
    }
    MenuSwitchRow(str("perf.hack.vuFlagHack"), settings.vuFlagHack) {
        viewModel.updateSettings { current -> current.copy(vuFlagHack = it) }
    }
    MenuSwitchRow(str("perf.hack.intcStat"), settings.intcStat) {
        viewModel.updateSettings { current -> current.copy(intcStat = it) }
    }
    MenuSwitchRow(str("perf.hack.waitLoop"), settings.waitLoop) {
        viewModel.updateSettings { current -> current.copy(waitLoop = it) }
    }
    SectionCard(str("tab.recompiler")) {
        MenuSwitchRow("EE (R5900)", settings.recEE) { value -> viewModel.updateSettings { it.copy(recEE = value) } }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow("IOP (R3000)", settings.recIOP) { value -> viewModel.updateSettings { it.copy(recIOP = value) } }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow("VU0", settings.recVU0) { value -> viewModel.updateSettings { it.copy(recVU0 = value) } }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow("VU1", settings.recVU1) { value -> viewModel.updateSettings { it.copy(recVU1 = value) } }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow("Fastmem", settings.enableFastmem) { value -> viewModel.updateSettings { it.copy(enableFastmem = value) } }
    }
    SectionCard(str("tab.overlay")) {
        MenuSwitchRow(str("overlay.toggle.fps"), settings.osdShowFps) { value ->
            viewModel.updateSettings { it.copy(osdShowFps = value) }
        }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow(str("overlay.toggle.emulationSpeed"), settings.osdShowSpeed) { value ->
            viewModel.updateSettings { it.copy(osdShowSpeed = value) }
        }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow(str("overlay.toggle.cpuUsage"), settings.osdShowCpu) { value ->
            viewModel.updateSettings { it.copy(osdShowCpu = value) }
        }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow(str("overlay.toggle.gpuUsage"), settings.osdShowGpu) { value ->
            viewModel.updateSettings { it.copy(osdShowGpu = value) }
        }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow(str("overlay.toggle.internalResolution"), settings.osdShowResolution) { value ->
            viewModel.updateSettings { it.copy(osdShowResolution = value) }
        }
        Spacer(Modifier.height(6.dp))
        MenuSwitchRow(str("overlay.toggle.onScreenNotifications"), settings.osdShowMessages) { value ->
            viewModel.updateSettings { it.copy(osdShowMessages = value) }
        }
    }
}

@Composable
private fun ControlsPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    MenuSwitchRow(str("pad.onScreenControls.label"), state.touchControlsVisible) {
        viewModel.toggleTouchControls()
    }
    MenuSwitchRow(
        title = str("pad.rumble.label"),
        checked = state.rumbleEnabled,
        onCheckedChange = viewModel::setRumble,
    )
    MenuSwitchRow(str("pad.multitap.label"), state.multitapEnabled, onCheckedChange = viewModel::setMultitap)
    MenuSwitchRow(str("network.emulateUsbKeyboard"), state.settings.usbKeyboard) {
        viewModel.updateSettings { current -> current.copy(usbKeyboard = it) }
    }
    CompactAction(str("pad.controllerMapping"), "⌁", Modifier.fillMaxWidth(), viewModel::openControlsManager)
    Spacer(Modifier.height(6.dp))
    CompactAction(str("pad.editTouchLayout"), "✥", Modifier.fillMaxWidth(), viewModel::editTouchControls)
    // Motion / gyroscope controls in-game (mode, sensitivity, smoothing, invert). Global scope
    // to match the rumble/multitap toggles above; the per-game scope lives in All Settings › Controls.
    com.armsx2.ui.settings.GyroSection()
    // Macros — edit each M1-M4 button set here in-game too (physical-trigger binding stays
    // in All Settings › Controls, which hosts the key-capture listener).
    com.armsx2.ui.settings.MacrosSection()
}

@Composable
private fun OptionsPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    val settings = state.settings
    // Item 3: gateway to the full per-game settings (all categories the compact menu omits:
    // OSD, Skins, Audio, Hotkeys, Network, Recompiler, ...).
    CompactAction(str("action.allSettings"), "⚙", Modifier.fillMaxWidth(), viewModel::openFullSettings)
    Spacer(Modifier.height(6.dp))
    // In-game access to the manager screens (the library drawer's Memory Cards /
    // Patches & Cheats / Controller mapping) — open over the paused game.
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        CompactAction(str("memcard.title"), "▤", Modifier.weight(1f), viewModel::openMemcard)
        CompactAction(str("patches.dialog.patchesAndCheats"), "✦", Modifier.weight(1f), viewModel::openPatches)
    }
    Spacer(Modifier.height(6.dp))
    MenuSwitchRow(str("patches.enablePatches.label"), settings.enablePatches) {
        viewModel.updateSettings { current -> current.copy(enablePatches = it) }
    }
    MenuSwitchRow(
        if (state.hardcore) str("patches.cheats.labelHardcore") else str("patches.cheats.label"),
        settings.enableCheats && !state.hardcore,
        enabled = !state.hardcore,
    ) {
        viewModel.updateSettings { current -> current.copy(enableCheats = it) }
    }
    MenuSwitchRow(str("patches.widescreen.label"), settings.enableWideScreenPatches) {
        viewModel.updateSettings { current -> current.copy(enableWideScreenPatches = it) }
    }
    MenuSwitchRow(str("patches.noInterlacing.label"), settings.enableNoInterlacingPatches) {
        viewModel.updateSettings { current -> current.copy(enableNoInterlacingPatches = it) }
    }
    MenuSwitchRow(str("perf.fix.skipBios"), settings.enableFastBoot) {
        viewModel.updateSettings { current -> current.copy(enableFastBoot = it) }
    }
    MenuSwitchRow(str("perf.fix.gamedbFixes"), settings.enableGameFixes) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = it) }
    }
    MenuSwitchRow(str("perf.fix.skipMpeg"), settings.gamefixSkipMpeg) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = true, gamefixSkipMpeg = it) }
    }
    MenuSwitchRow(str("perf.fix.fmvSoftware"), settings.gamefixSoftwareRendererFmv) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = true, gamefixSoftwareRendererFmv = it) }
    }
    MenuSwitchRow(str("perf.fix.eeTiming"), settings.gamefixEETiming) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = true, gamefixEETiming = it) }
    }
    MenuSwitchRow(str("perf.fix.instantDma"), settings.gamefixInstantDma) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = true, gamefixInstantDma = it) }
    }
    MenuSwitchRow(str("perf.fix.blitFps"), settings.gamefixBlitInternalFps) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = true, gamefixBlitInternalFps = it) }
    }
    MenuSwitchRow(str("perf.fix.vuAddSub"), settings.gamefixVuAddSub) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = true, gamefixVuAddSub = it) }
    }
    MenuSwitchRow(str("perf.fix.vuSync"), settings.gamefixVuSync) {
        viewModel.updateSettings { current -> current.copy(enableGameFixes = true, gamefixVuSync = it) }
    }
}

@Composable
private fun AchievementsPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    // Gateway to the full RetroAchievements screen (unlock list + presentation options).
    CompactAction(str("ra.viewAchievements"), "★", Modifier.fillMaxWidth(), viewModel::openAchievements)
    Spacer(Modifier.height(4.dp))
    SectionCard("RetroAchievements") {
        Text(
            state.achievementSummary,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        Spacer(Modifier.height(8.dp))
        MenuSwitchRow(
            str(if (state.hardcore) "ra.mode.hardcore" else "ra.mode.casual"),
            state.hardcore,
            onCheckedChange = { viewModel.requestToggleHardcore() },
        )
    }
    // Inline unlock list, right below the hardcore toggle — no need to open the full
    // screen (it's still available via the button above).
    state.achievements.forEach { item -> InGameAchievementRow(item) }
}

@Composable
private fun InGameAchievementRow(item: AchievementItem) {
    Surface(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        color = if (item.unlocked) MaterialTheme.colorScheme.primaryContainer
        else MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.42f),
        border = BorderStroke(
            1.dp,
            if (item.unlocked) MaterialTheme.colorScheme.primary.copy(alpha = 0.5f)
            else MaterialTheme.colorScheme.outline.copy(alpha = 0.3f),
        ),
    ) {
        Row(Modifier.padding(10.dp), verticalAlignment = Alignment.CenterVertically) {
            if (item.iconUrl.isNotBlank()) {
                AsyncImage(
                    item.iconUrl,
                    item.title,
                    Modifier.size(40.dp).clip(RoundedCornerShape(9.dp)),
                    contentScale = ContentScale.Crop,
                )
            } else {
                Box(
                    Modifier.size(40.dp).clip(RoundedCornerShape(9.dp))
                        .background(MaterialTheme.colorScheme.surface),
                    contentAlignment = Alignment.Center,
                ) { Text(if (item.unlocked) "★" else "☆") }
            }
            Spacer(Modifier.width(10.dp))
            Column(Modifier.weight(1f)) {
                Text(item.title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(item.description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2, overflow = TextOverflow.Ellipsis)
                if (item.progress.isNotBlank()) {
                    Text(item.progress, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.primary)
                }
            }
            Spacer(Modifier.width(8.dp))
            Text(
                "${item.points}",
                style = MaterialTheme.typography.labelMedium,
                color = if (item.unlocked) Success else MaterialTheme.colorScheme.onSurfaceVariant,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

@Composable
private fun HardcoreBadge() {
    // Firebrick red to match the old UI's hardcore pill (theme Danger reads pink here).
    Surface(shape = RoundedCornerShape(6.dp), color = Color(0xFFB22222)) {
        Text(
            "HC",
            modifier = Modifier.padding(horizontal = 7.dp, vertical = 2.dp),
            color = Color.White,
            style = MaterialTheme.typography.labelSmall,
            fontWeight = FontWeight.Bold,
        )
    }
}

private data class MenuAction(
    val title: String,
    val detail: String,
    val glyph: String,
    val accent: Color?,
    val action: () -> Unit,
)

@Composable
private fun ActionGrid(actions: List<MenuAction>, selected: Int, onSelect: (Int) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        actions.forEachIndexed { index, item ->
            val active = index == selected
            Surface(
                onClick = { onSelect(index); item.action() },
                modifier = Modifier
                    .fillMaxWidth()
                    .controllerFocusable("pause.action.$index", onConfirm = { onSelect(index); item.action() }),
                shape = RoundedCornerShape(16.dp),
                color = if (active) MaterialTheme.colorScheme.primary.copy(alpha = 0.14f)
                else MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.48f),
                border = BorderStroke(
                    1.dp,
                    if (active) MaterialTheme.colorScheme.primary.copy(alpha = 0.72f)
                    else MaterialTheme.colorScheme.outline.copy(alpha = 0.34f),
                ),
            ) {
                Row(Modifier.padding(horizontal = 13.dp, vertical = 11.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        item.glyph,
                        color = item.accent ?: MaterialTheme.colorScheme.primary,
                        fontSize = 19.sp,
                        fontWeight = FontWeight.Bold,
                        modifier = Modifier.width(30.dp),
                    )
                    Column(Modifier.weight(1f)) {
                        Text(item.title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold)
                        if (item.detail.isNotBlank()) {
                            Text(
                                item.detail,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                maxLines = 1,
                                overflow = TextOverflow.Ellipsis,
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun SectionCard(title: String, content: @Composable ColumnScope.() -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(17.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.42f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.34f)),
    ) {
        Column(Modifier.padding(13.dp)) {
            Text(title, style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(8.dp))
            content()
        }
    }
}

@Composable
private fun <T> HorizontalOptions(
    title: String,
    options: List<Pair<T, String>>,
    selected: T,
    onSelect: (T) -> Unit,
) {
    SectionCard(title) {
        HorizontalOptionRow(options, selected, keyPrefix = title, onSelect = onSelect)
    }
}

// Free-choice framerate cap (30–120 Hz) instead of a couple of fixed chips. The
// default (59.94 / 50) is kept exactly until the user drags; dragging snaps to
// whole Hz so common targets (50/60/72/90/120) are easy to hit.
@Composable
private fun FramerateSlider(title: String, value: Float, onValue: (Float) -> Unit) {
    SectionCard(title) {
        Column(
            Modifier.fillMaxWidth().controllerFocusable(
                "pause.framerate.$title",
                onLeft = { onValue((value - 1f).coerceAtLeast(20f)) },
                onRight = { onValue((value + 1f).coerceAtMost(120f)) },
            ),
        ) {
            val label = if (value % 1f == 0f) "${value.toInt()} Hz" else "%.2f Hz".format(value)
            Text(label, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.primary)
            Slider(
                value = value.coerceIn(20f, 120f),
                onValueChange = { onValue(Math.round(it).toFloat()) },
                valueRange = 20f..120f,
            )
        }
    }
}

@Composable
private fun <T> HorizontalOptionRow(
    options: List<Pair<T, String>>,
    selected: T,
    keyPrefix: String,
    onSelect: (T) -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .bleedHorizontal(13.dp)
            .horizontalScroll(rememberScrollState())
            .padding(horizontal = 13.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        options.forEach { (value, label) ->
            OptionChip(label, selected == value, controllerId = "pause.$keyPrefix.$label") { onSelect(value) }
        }
    }
}

@Composable
private fun OptionChip(label: String, selected: Boolean, controllerId: String? = null, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        modifier = Modifier.controllerFocusable(controllerId, RoundedCornerShape(12.dp), onConfirm = onClick),
        shape = RoundedCornerShape(12.dp),
        color = if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.17f)
        else MaterialTheme.colorScheme.surface,
        border = BorderStroke(
            1.dp,
            if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.38f),
        ),
    ) {
        Text(
            label,
            modifier = Modifier.padding(horizontal = 13.dp, vertical = 9.dp),
            color = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant,
            style = MaterialTheme.typography.labelLarge,
            fontWeight = if (selected) FontWeight.Bold else FontWeight.Medium,
            maxLines = 2,
        )
    }
}

@Composable
private fun MenuSwitchRow(
    title: String,
    checked: Boolean,
    enabled: Boolean = true,
    onCheckedChange: (Boolean) -> Unit,
) {
    Surface(
        onClick = { if (enabled) onCheckedChange(!checked) },
        modifier = Modifier
            .fillMaxWidth()
            .controllerFocusable(
                "pause.switch.$title",
                onConfirm = { if (enabled) onCheckedChange(!checked) },
                onLeft = { if (enabled) onCheckedChange(false) },
                onRight = { if (enabled) onCheckedChange(true) },
            ),
        enabled = enabled,
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = if (enabled) 0.42f else 0.24f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.34f)),
    ) {
        Row(
            Modifier.padding(horizontal = 13.dp, vertical = 9.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                title,
                modifier = Modifier.weight(1f),
                style = MaterialTheme.typography.titleSmall,
                color = if (enabled) MaterialTheme.colorScheme.onSurface else MaterialTheme.colorScheme.onSurfaceVariant,
                maxLines = 2,
            )
            Spacer(Modifier.width(10.dp))
            Switch(checked = checked, onCheckedChange = if (enabled) onCheckedChange else null)
        }
    }
}

@Composable
private fun CompactAction(title: String, glyph: String, modifier: Modifier, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        modifier = modifier.controllerFocusable("pause.compact.$title", onConfirm = onClick),
        shape = RoundedCornerShape(14.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
    ) {
        Row(
            Modifier.padding(horizontal = 12.dp, vertical = 11.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(glyph, color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
            Text(title, style = MaterialTheme.typography.labelLarge, maxLines = 2)
        }
    }
}

private fun Modifier.bleedHorizontal(edge: androidx.compose.ui.unit.Dp): Modifier = layout { measurable, constraints ->
    val edgePx = edge.roundToPx()
    val expandedMin = (constraints.minWidth + edgePx * 2).coerceAtMost(constraints.maxWidth + edgePx * 2)
    val expandedMax = constraints.maxWidth + edgePx * 2
    val placeable = measurable.measure(
        constraints.copy(
            minWidth = expandedMin,
            maxWidth = expandedMax,
        ),
    )
    layout(constraints.maxWidth, placeable.height) {
        placeable.placeRelative(-edgePx, 0)
    }
}
