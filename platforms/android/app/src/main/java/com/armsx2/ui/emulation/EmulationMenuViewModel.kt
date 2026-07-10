package com.armsx2.ui.emulation

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.config.Settings
import com.armsx2.i18n.I18n
import com.armsx2.input.ControllerMappings
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.InGameOverlay
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONObject

enum class EmulationMenuTab(val titleKey: String) {
    Session("games.info.inGameMenu.title"),
    Graphics("tab.renderer"),
    Performance("tab.performance"),
    Controls("tab.controls"),
    Options("action.settings"),
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
    val achievementSummary: String = I18n.get("ra.status.noAchievements.title"),
)

class EmulationMenuViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(EmulationMenuUiState())
        private set

    var dismissHandler: (() -> Unit)? = null

    fun load(initialTab: EmulationMenuTab?) {
        val settings = InGameOverlay.settingsState.value
        val summary = runCatching {
            val root = JSONObject(NativeApp.getAchievementsJSON().orEmpty())
            val unlocked = root.optInt("unlocked", root.optInt("unlocked_count", 0))
            val total = root.optInt("total", root.optInt("achievement_count", 0))
            if (total > 0) "$unlocked / $total" else NativeApp.getRichPresence().orEmpty()
                .ifBlank { I18n.get("ra.status.noAchievements.title") }
        }.getOrDefault(I18n.get("ra.status.noAchievements.title"))
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
                2 -> MainActivityRuntime.stop(saveAutosave = false)
            }
            EmulationMenuTab.Graphics -> when (state.value.selectedAction) {
                0 -> setRenderer("auto")
                1 -> setRenderer("vulkan")
                2 -> setRenderer("opengl")
                3 -> setRenderer("software")
            }
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
                4 -> toggleHardcore()
            }
        }
    }

    fun resume() {
        dismissHandler?.invoke() ?: resumeImmediately()
    }

    fun resumeImmediately() {
        InGameOverlay.toggle()
    }

    fun saveState() {
        MainActivityRuntime.instance?.saveState()
    }

    fun loadState() {
        MainActivityRuntime.instance?.loadState()
        resume()
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
        val normalized = value.coerceIn(1f, 8f)
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

    fun toggleHardcore() {
        val enabled = !state.value.hardcore
        NativeApp.setHardcoreMode(enabled)
        state.value = state.value.copy(hardcore = enabled)
    }

    fun updateSettings(transform: (Settings) -> Settings) {
        val updated = transform(state.value.settings)
        InGameOverlay.saveSettings(updated)
        state.value = state.value.copy(settings = updated)
    }

    private fun actionCount(tab: EmulationMenuTab): Int = when (tab) {
        EmulationMenuTab.Session -> 3
        EmulationMenuTab.Graphics -> 4
        EmulationMenuTab.Performance -> 3
        EmulationMenuTab.Controls -> 2
        EmulationMenuTab.Options -> 5
    }

    private fun Int.floorMod(modulus: Int): Int = ((this % modulus) + modulus) % modulus
}

object EmulationMenuInputController {
    private var owner: EmulationMenuViewModel? = null
    private var pendingTab: EmulationMenuTab? = null

    fun bind(viewModel: EmulationMenuViewModel) {
        owner = viewModel
        viewModel.load(pendingTab)
        pendingTab = null
    }

    fun unbind(viewModel: EmulationMenuViewModel) {
        if (owner === viewModel) owner = null
    }

    fun open(tab: EmulationMenuTab = EmulationMenuTab.Session) {
        pendingTab = tab
        if (!com.armsx2.ui.WindowImpl.overlayVisible.value) InGameOverlay.open()
        owner?.selectTab(tab)
    }

    fun move(dx: Int, dy: Int): Boolean {
        val viewModel = owner ?: return false
        when {
            dx < 0 -> viewModel.cycleTab(-1)
            dx > 0 -> viewModel.cycleTab(1)
            dy < 0 -> viewModel.moveSelection(-1)
            dy > 0 -> viewModel.moveSelection(1)
        }
        return true
    }

    fun tab(delta: Int): Boolean {
        owner?.cycleTab(delta) ?: return false
        return true
    }

    fun confirm(): Boolean {
        owner?.activateSelection() ?: return false
        return true
    }

    fun back(): Boolean {
        owner?.resume() ?: return false
        return true
    }
}
