package com.armsx2.ui.emulation

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.config.Settings
import com.armsx2.i18n.I18n
import com.armsx2.input.ControllerMappings
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.InGameOverlay
import com.armsx2.ui.achievements.AchievementItem
import com.armsx2.ui.achievements.parseAchievementItems
import kr.co.iefriends.pcsx2.NativeApp

enum class EmulationMenuTab(val titleKey: String) {
    Session("games.info.inGameMenu.title"),
    Graphics("tab.renderer"),
    Fixes("tab.fixes"),
    Performance("tab.performance"),
    Controls("tab.controls"),
    Options("action.settings"),
    Achievements("ra.title"),
}

data class EmulationMenuUiState(
    val tab: EmulationMenuTab = EmulationMenuTab.Session,
    val selectedAction: Int = 0,
    val saveSlot: Int = 0,
    val settings: Settings = Settings(),
    val touchControlsVisible: Boolean = true,
    val rumbleEnabled: Boolean = true,
    val multitapEnabled: Boolean = false,
    val hardcore: Boolean = false,
    // Non-null while the hardcore confirm dialog is up; holds the target state.
    val pendingHardcore: Boolean? = null,
    val achievementSummary: String = I18n.get("ra.status.noAchievements.title"),
    // RA account line for the pause-menu panel (empty / 0 when not logged in).
    val raUserName: String = "",
    val raScore: Long = 0,
    val raSoftcoreScore: Long = 0,
    val raAvatarUrl: String = "",
    val achievements: List<AchievementItem> = emptyList(),
    // RetroAchievements rich-presence line ("what you're doing right now"); shown in the
    // pause-menu header when a set is loaded. Empty when RA is off / no set.
    val richPresence: String = "",
)

class EmulationMenuViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(EmulationMenuUiState())
        private set

    var dismissHandler: (() -> Unit)? = null

    fun load(initialTab: EmulationMenuTab?) {
        val settings = InGameOverlay.settingsState.value
        // The native JSON emits the unlock list under "items"; count from that rather
        // than the non-existent "unlocked"/"total" keys the old code read (which always
        // fell through to rich presence). Fall back to rich presence when no set loaded.
        val raJson = runCatching { NativeApp.getAchievementsJSON().orEmpty() }.getOrDefault("")
        val items = runCatching { parseAchievementItems(raJson) }.getOrDefault(emptyList())
        val raRoot = runCatching { org.json.JSONObject(raJson) }.getOrNull()
        val richPresence = runCatching { NativeApp.getRichPresence().orEmpty() }.getOrDefault("")
        val summary = if (items.isNotEmpty()) {
            "${items.count { it.unlocked }} / ${items.size}"
        } else {
            richPresence.ifBlank { I18n.get("ra.status.noAchievements.title") }
        }
        state.value = state.value.copy(
            tab = initialTab ?: state.value.tab,
            selectedAction = 0,
            saveSlot = MainActivityRuntime.currentSaveSlot.value,
            settings = settings,
            touchControlsVisible = com.armsx2.ui.touch.TouchControls.visible.value,
            rumbleEnabled = ControllerMappings.rumbleEnabled(),
            multitapEnabled = ControllerMappings.multitapEnabled(),
            hardcore = runCatching { NativeApp.isHardcoreMode() }.getOrDefault(false),
            achievementSummary = summary,
            raUserName = raRoot?.optString("userName").orEmpty(),
            raScore = (raRoot?.optLong("score") ?: 0L).coerceAtLeast(0),
            raSoftcoreScore = (raRoot?.optLong("softcoreScore") ?: 0L).coerceAtLeast(0),
            raAvatarUrl = raRoot?.optString("avatarUrl").orEmpty(),
            achievements = items,
            richPresence = richPresence,
        )
    }

    fun selectTab(tab: EmulationMenuTab) {
        state.value = state.value.copy(tab = tab, selectedAction = 0)
    }

    fun cycleTab(delta: Int) {
        val tabs = EmulationMenuTab.entries
        val current = tabs.indexOf(state.value.tab)
        selectTab(tabs[(current + delta).floorMod(tabs.size)])
    }

    fun moveSelection(delta: Int) {
        val max = actionCount(state.value.tab) - 1
        state.value = state.value.copy(
            selectedAction = (state.value.selectedAction + delta).coerceIn(0, max.coerceAtLeast(0)),
        )
    }

    fun selectAction(index: Int) {
        state.value = state.value.copy(selectedAction = index)
    }

    fun activateSelection() {
        when (state.value.tab) {
            EmulationMenuTab.Session -> when (state.value.selectedAction) {
                0 -> resume()
                1 -> MainActivityRuntime.restart()
                2 -> MainActivityRuntime.promptSwapDisc()
                3 -> MainActivityRuntime.closeGame()
            }
            EmulationMenuTab.Graphics -> when (state.value.selectedAction) {
                0 -> setRenderer("auto")
                1 -> setRenderer("vulkan")
                2 -> setRenderer("opengl")
                3 -> setRenderer("software")
            }
            // Fixes is a registry-driven pane (its controls self-navigate), so it has
            // no discrete action grid — nothing to activate here.
            EmulationMenuTab.Fixes -> Unit
            EmulationMenuTab.Performance -> when (state.value.selectedAction) {
                0 -> updateSettings { it.copy(frameLimitEnable = !it.frameLimitEnable) }
                1 -> setSpeed(it = state.value.settings.nominalSpeedPercent + 5)
                2 -> setFrameSkip((state.value.settings.frameSkip + 1) % 6)
            }
            EmulationMenuTab.Controls -> when (state.value.selectedAction) {
                0 -> editTouchControls()
                1 -> toggleTouchControls()
            }
            EmulationMenuTab.Options -> when (state.value.selectedAction) {
                0 -> updateSettings { it.copy(enablePatches = !it.enablePatches) }
                1 -> updateSettings { it.copy(enableCheats = !it.enableCheats) }
                2 -> updateSettings { it.copy(enableWideScreenPatches = !it.enableWideScreenPatches) }
                3 -> updateSettings { it.copy(enableNoInterlacingPatches = !it.enableNoInterlacingPatches) }
            }
            EmulationMenuTab.Achievements -> when (state.value.selectedAction) {
                0 -> requestToggleHardcore()
                1 -> openAchievements()
            }
        }
    }

    fun resume() {
        dismissHandler?.invoke() ?: resumeImmediately()
    }

    fun resumeImmediately() {
        InGameOverlay.toggle()
    }

    /**
     * Item 3: the in-game compact menu only exposes a reduced set of settings. This opens the
     * FULL per-game settings (every category — OSD, Skins, Audio, Hotkeys, Network, Recompiler,
     * ...) over the running game via the app-nav's showLibrary layer, scoped to the current game.
     * Changes live-apply through the settings system while the VM is running.
     */
    fun openFullSettings() = com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.Settings)

    /** In-game access to the manager screens the library drawer exposes. */
    fun openMemcard() = com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.Memcard)

    fun openPatches() = com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.Patches)

    fun openControlsManager() = com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.Controls)

    fun openTextures() = com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.Textures)

    fun openSkins() = com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.Skins)

    fun saveState() {
        MainActivityRuntime.instance?.saveState()
    }

    fun loadState() {
        // Resume/dismiss only after the state has actually loaded (avoids the race
        // where the menu resumed the VM before the load landed).
        MainActivityRuntime.instance?.loadState { resume() }
    }

    fun previousSlot() = setSaveSlot((state.value.saveSlot - 1).floorMod(10))

    fun nextSlot() = setSaveSlot((state.value.saveSlot + 1) % 10)

    fun setSaveSlot(slot: Int) {
        val normalized = slot.coerceIn(0, 9)
        MainActivityRuntime.currentSaveSlot.value = normalized
        state.value = state.value.copy(saveSlot = normalized)
    }

    fun setRenderer(renderer: String) {
        updateSettings { it.copy(renderer = renderer) }
        MainActivityRuntime.renderer.value = renderer
        when (renderer) {
            "vulkan" -> MainActivityRuntime.renderVulkan()
            "opengl" -> MainActivityRuntime.renderOpenGL()
            "software" -> MainActivityRuntime.renderSoftware()
            else -> NativeApp.renderAuto()
        }
    }

    fun setUpscale(value: Float) {
        // Allow sub-native (0.25x–0.75x, issue #207) — the in-game menu offers them via
        // UPSCALE_OPTIONS, so don't clamp them up to Native like the old 1f floor did (that
        // made every below-Native pick silently apply as Native). Matches the settings tab.
        val normalized = value.coerceIn(0.25f, 8f)
        updateSettings { it.copy(upscaleFloat = normalized) }
        MainActivityRuntime.upscale.value = normalized
        NativeApp.renderUpscalemultiplier(normalized)
    }

    fun setAspectRatio(value: Int) = updateSettings { it.copy(aspectRatio = value.coerceIn(0, 4)) }

    fun setTextureFiltering(value: Int) = updateSettings { it.copy(textureFiltering = value.coerceIn(0, 3)) }

    fun setBlending(value: Int) = updateSettings { it.copy(accurateBlendingUnit = value.coerceIn(0, 5)) }

    fun setTexturePreloading(value: Int) = updateSettings { it.copy(texturePreloading = value.coerceIn(0, 2)) }

    fun setHardwareDownloadMode(value: Int) = updateSettings { it.copy(hardwareDownloadMode = value.coerceIn(0, 4)) }

    fun setEeCycleRate(value: Int) = updateSettings { it.copy(eeCycleRate = value.coerceIn(-3, 3)) }

    fun setEeCycleSkip(value: Int) = updateSettings { it.copy(eeCycleSkip = value.coerceIn(0, 3)) }

    fun setSpeed(it: Int) = updateSettings { settings -> settings.copy(nominalSpeedPercent = it.coerceIn(50, 200)) }

    fun setFpsLimit(value: Int) = updateSettings { it.copy(fpsLimit = value.coerceIn(0, 240)) }

    fun setFrameSkip(value: Int) = updateSettings { it.copy(frameSkip = value.coerceIn(0, 5)) }

    fun setVolume(value: Int) = updateSettings { it.copy(audioVolume = value.coerceIn(0, 200)) }

    fun setAudioBuffer(value: Int) = updateSettings { it.copy(audioBufferMs = value.coerceIn(10, 200)) }

    /** Universal on-screen-display toggle (old-UI style): flips the perf stats as a
     *  group; the granular per-stat toggles stay in All Settings. Notifications are
     *  left alone so achievement/message popups aren't affected. */
    // The one-tap OSD master mirrors what refresh showed by default: FPS/VPS, speed,
    // the EE/GS/VU/GPU perf lines, resolution, GS + GPU pipeline stats, frame times,
    // the hardware-info line, and the "ARMSX2 <version>" banner. Granular control of
    // each stays in All Settings; the bottom Settings-summary / Inputs overlays are
    // left out so this can't force those debug strips on.
    fun setOsdMaster(enabled: Boolean) = updateSettings {
        it.copy(
            osdShowFps = enabled,
            osdShowVps = enabled,
            osdShowSpeed = enabled,
            osdShowCpu = enabled,
            osdShowGpu = enabled,
            osdShowResolution = enabled,
            osdShowGsStats = enabled,
            osdShowFrameTimes = enabled,
            osdShowHardwareInfo = enabled,
            osdShowGpuStats = enabled,
            osdShowVersion = enabled,
        )
    }

    /** Simple OSD: just the FPS/VPS counter, none of the verbose CPU/GPU/GS/frame-time/
     *  hardware lines. Mutually exclusive with the full OSD through the shared osdShow*
     *  fields — the menu reads "FPS on + everything-else off" as the simple state. */
    fun setOsdSimple(enabled: Boolean) = updateSettings {
        it.copy(
            osdShowFps = enabled,
            osdShowVps = false,
            osdShowSpeed = false,
            osdShowCpu = false,
            osdShowGpu = false,
            osdShowResolution = false,
            osdShowGsStats = false,
            osdShowFrameTimes = false,
            osdShowHardwareInfo = false,
            osdShowGpuStats = false,
            osdShowVersion = false,
        )
    }

    fun setRumble(enabled: Boolean) {
        ControllerMappings.setRumbleEnabled(enabled)
        NativeApp.sRumbleEnabled = enabled
        state.value = state.value.copy(rumbleEnabled = enabled)
    }

    fun setMultitap(enabled: Boolean) {
        ControllerMappings.setMultitapEnabled(enabled)
        state.value = state.value.copy(multitapEnabled = enabled)
    }

    fun editTouchControls() {
        dismissHandler = null
        InGameOverlay.editTouchLayout()
    }

    fun toggleTouchControls() {
        val enabled = !state.value.touchControlsVisible
        com.armsx2.ui.touch.TouchControls.visible.value = enabled
        state.value = state.value.copy(touchControlsVisible = enabled)
    }

    // Hardcore toggle is confirmed (both directions): enabling restarts the game and
    // disables save states/cheats; disabling drops to casual so unlocks stop counting.
    fun requestToggleHardcore() {
        state.value = state.value.copy(pendingHardcore = !state.value.hardcore)
    }

    fun confirmToggleHardcore() {
        val target = state.value.pendingHardcore ?: return
        NativeApp.setHardcoreMode(target)
        state.value = state.value.copy(hardcore = target, pendingHardcore = null)
        // Enabling hardcore only takes hold on a system reset, and the VM is paused
        // behind this menu — so the native "will be enabled on system reset" toast
        // just sits there. Reboot now so "Enable & restart" actually restarts.
        // (Disabling stays live — casual mode applies immediately.)
        if (target) MainActivityRuntime.restart()
    }

    fun cancelToggleHardcore() {
        state.value = state.value.copy(pendingHardcore = null)
    }

    /** Open the full RetroAchievements screen (list + options) over the paused game. */
    fun openAchievements() = com.armsx2.ui.WindowImpl.openInGameScreen(com.armsx2.ui.InGameScreen.Achievements)

    fun updateSettings(transform: (Settings) -> Settings) {
        val updated = transform(state.value.settings)
        InGameOverlay.saveSettings(updated)
        state.value = state.value.copy(settings = updated)
    }

    private fun actionCount(tab: EmulationMenuTab): Int = when (tab) {
        EmulationMenuTab.Session -> 4
        EmulationMenuTab.Graphics -> 4
        EmulationMenuTab.Fixes -> 0
        EmulationMenuTab.Performance -> 3
        EmulationMenuTab.Controls -> 2
        EmulationMenuTab.Options -> 5
        EmulationMenuTab.Achievements -> 2
    }

    private fun Int.floorMod(modulus: Int): Int = ((this % modulus) + modulus) % modulus
}

object EmulationMenuInputController {
    private var owner: EmulationMenuViewModel? = null
    private var pendingTab: EmulationMenuTab? = null

    // Two-zone nav. The pause menu is a vertical TAB column on the left and a
    // CONTENT pane on the right. `inContent` = false means the D-pad walks the tab
    // column (Up/Down between tabs, which switches the shown pane); Right (or A)
    // steps into the content pane, where every control is a SettingsControllerNav
    // registry item and the router drives it (Up/Down move, Left/Right adjust, A
    // confirm). B (or Left off the first control) returns to the tab column.
    val inContent = androidx.compose.runtime.mutableStateOf(false)
    private val nav get() = com.armsx2.ui.settings.SettingsControllerNav

    fun bind(viewModel: EmulationMenuViewModel) {
        owner = viewModel
        viewModel.load(pendingTab)
        pendingTab = null
        inContent.value = false
    }

    fun unbind(viewModel: EmulationMenuViewModel) {
        if (owner === viewModel) owner = null
    }

    fun open(tab: EmulationMenuTab = EmulationMenuTab.Session) {
        pendingTab = tab
        if (!com.armsx2.ui.WindowImpl.overlayVisible.value) InGameOverlay.open()
        owner?.selectTab(tab)
        inContent.value = false
    }

    private fun enterContent() {
        inContent.value = true
        nav.clearSelection()
        nav.move(1) // select the first content control so the highlight appears
    }

    private fun exitContent() {
        inContent.value = false
        nav.clearSelection()
    }

    fun move(dx: Int, dy: Int): Boolean {
        val viewModel = owner ?: return false
        if (!inContent.value) {
            // Tab column (vertical): Up/Down switch tabs; Right steps into content.
            when {
                dy < 0 -> viewModel.cycleTab(-1)
                dy > 0 -> viewModel.cycleTab(1)
                dx > 0 -> enterContent()
            }
            return true
        }
        // Content pane: registry-driven.
        when {
            dy != 0 -> nav.moveSpatial(0, dy)
            dx < 0 -> if (!nav.adjust(-1) && !nav.moveSpatial(-1, 0)) exitContent()
            dx > 0 -> if (!nav.adjust(1)) nav.moveSpatial(1, 0)
        }
        return true
    }

    /** L1 / R1 always cycle tabs, snapping back to the tab column. */
    fun tab(delta: Int): Boolean {
        val viewModel = owner ?: return false
        if (inContent.value) exitContent()
        viewModel.cycleTab(delta)
        return true
    }

    fun confirm(): Boolean {
        owner ?: return false
        if (!inContent.value) { enterContent(); return true }
        nav.confirm()
        return true
    }

    fun back(): Boolean {
        if (inContent.value) { exitContent(); return true }
        owner?.resume() ?: return false
        return true
    }
}
