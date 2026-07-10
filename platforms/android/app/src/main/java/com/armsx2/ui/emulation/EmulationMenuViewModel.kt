package com.armsx2.ui.emulation

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.InGameOverlay
import com.armsx2.ui.WindowImpl
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONObject

enum class EmulationMenuTab(val title: String) {
    Session("Session"),
    States("Save states"),
    Graphics("Graphics"),
    Controls("Controls"),
    Achievements("Achievements"),
}

data class EmulationMenuUiState(
    val tab: EmulationMenuTab = EmulationMenuTab.Session,
    val selectedAction: Int = 0,
    val saveSlot: Int = 0,
    val renderer: String = "auto",
    val upscale: Float = 1f,
    val frameLimit: Boolean = true,
    val hardcore: Boolean = false,
    val achievementSummary: String = "No achievement data",
)

class EmulationMenuViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(EmulationMenuUiState())
        private set

    fun load(initialTab: EmulationMenuTab?) {
        val settings = InGameOverlay.settingsState.value
        val summary = runCatching {
            val root = JSONObject(NativeApp.getAchievementsJSON().orEmpty())
            val unlocked = root.optInt("unlocked", root.optInt("unlocked_count", 0))
            val total = root.optInt("total", root.optInt("achievement_count", 0))
            if (total > 0) "$unlocked of $total unlocked" else NativeApp.getRichPresence().orEmpty().ifBlank { "No achievement data" }
        }.getOrDefault("No achievement data")
        state.value = state.value.copy(
            tab = initialTab ?: state.value.tab,
            selectedAction = 0,
            saveSlot = MainActivityRuntime.currentSaveSlot.value,
            renderer = settings.renderer,
            upscale = settings.upscaleFloat,
            frameLimit = settings.frameLimitEnable,
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
        state.value = state.value.copy(selectedAction = (state.value.selectedAction + delta).coerceIn(0, max.coerceAtLeast(0)))
    }

    fun selectAction(index: Int) {
        state.value = state.value.copy(selectedAction = index)
    }

    fun activateSelection() {
        when (state.value.tab) {
            EmulationMenuTab.Session -> when (state.value.selectedAction) {
                0 -> resume()
                1 -> openLibrary()
                2 -> MainActivityRuntime.restart()
                3 -> MainActivityRuntime.stop(saveAutosave = false)
            }
            EmulationMenuTab.States -> when (state.value.selectedAction) {
                0 -> saveState()
                1 -> loadState()
                2 -> previousSlot()
                3 -> nextSlot()
            }
            EmulationMenuTab.Graphics -> when (state.value.selectedAction) {
                0 -> setRenderer("auto")
                1 -> setRenderer("vulkan")
                2 -> setRenderer("opengl")
                3 -> setRenderer("software")
                4 -> adjustUpscale(-1f)
                5 -> adjustUpscale(1f)
                6 -> toggleFrameLimit()
            }
            EmulationMenuTab.Controls -> when (state.value.selectedAction) {
                0 -> editTouchControls()
                1 -> toggleTouchControls()
            }
            EmulationMenuTab.Achievements -> when (state.value.selectedAction) {
                0 -> toggleHardcore()
            }
        }
    }

    fun resume() {
        InGameOverlay.toggle()
    }

    fun openLibrary() {
        if (MainActivityRuntime.eState.value == com.armsx2.EmuState.PAUSED) MainActivityRuntime.resume()
        WindowImpl.showLibrary.value = true
        WindowImpl.overlayVisible.value = false
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
        val updated = InGameOverlay.settingsState.value.copy(renderer = renderer)
        InGameOverlay.saveSettings(updated)
        when (renderer) {
            "vulkan" -> MainActivityRuntime.renderVulkan()
            "opengl" -> MainActivityRuntime.renderOpenGL()
            "software" -> MainActivityRuntime.renderSoftware()
        }
        state.value = state.value.copy(renderer = renderer)
    }

    fun adjustUpscale(delta: Float) {
        val value = (state.value.upscale + delta).coerceIn(1f, 8f)
        InGameOverlay.saveSettings(InGameOverlay.settingsState.value.copy(upscaleFloat = value))
        MainActivityRuntime.upscale.value = value
        state.value = state.value.copy(upscale = value)
    }

    fun toggleFrameLimit() {
        val enabled = !state.value.frameLimit
        InGameOverlay.saveSettings(InGameOverlay.settingsState.value.copy(frameLimitEnable = enabled))
        state.value = state.value.copy(frameLimit = enabled)
    }

    fun editTouchControls() {
        InGameOverlay.editTouchLayout()
    }

    fun toggleTouchControls() {
        com.armsx2.ui.touch.TouchControls.visible.value = !com.armsx2.ui.touch.TouchControls.visible.value
        state.value = state.value.copy()
    }

    fun toggleHardcore() {
        val enabled = !state.value.hardcore
        NativeApp.setHardcoreMode(enabled)
        state.value = state.value.copy(hardcore = enabled)
    }

    private fun actionCount(tab: EmulationMenuTab): Int = when (tab) {
        EmulationMenuTab.Session -> 4
        EmulationMenuTab.States -> 4
        EmulationMenuTab.Graphics -> 7
        EmulationMenuTab.Controls -> 2
        EmulationMenuTab.Achievements -> 1
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
        if (!WindowImpl.overlayVisible.value) InGameOverlay.open()
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
