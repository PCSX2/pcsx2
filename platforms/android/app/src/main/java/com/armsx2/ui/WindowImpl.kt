package com.armsx2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import com.armsx2.EmuState
import com.armsx2.runtime.MainActivityRuntime
import kotlinx.coroutines.flow.first

/** A full manager screen shown as an overlay over the paused game (in-game menu). */
enum class InGameScreen { Settings, Achievements, Memcard, Patches, Controls, Skins, SaveState, LoadState }

object WindowImpl {
    val toolbarVisible = mutableStateOf(true)
    val showLibrary = mutableStateOf(false)
    val overlayVisible = mutableStateOf(false)
    // Full manager screen shown over the (paused) game — null when none is open.
    // Replaces the earlier per-screen booleans; the in-game menu routes here so the
    // library's Settings / RetroAchievements / Memory Cards / Patches / Controls
    // screens are all reachable in-game, each resuming the game on dismiss.
    val inGameScreen = mutableStateOf<InGameScreen?>(null)

    /** True whenever a Compose frontend surface is drawn on top of a running
     *  game (pause menu, an in-game manager/settings/Save-Load screen, the memory
     *  card dialog, or the library). While any of these is up the embedded game
     *  SurfaceView must release Android focus so Compose can receive the
     *  controller D-pad/A/B (see the focus-release effect in MainActivityRuntime).
     *  Read from composable/effect scopes so recomposition tracks the states. */
    val frontendCovers: Boolean
        get() = overlayVisible.value ||
            inGameScreen.value != null ||
            showLibrary.value ||
            com.armsx2.ui.MemoryCardManager.visible.value ||
            // The shader editor can be opened from the pause menu, which CLOSES as it
            // opens — without this the game would take focus back mid-edit and eat the
            // D-pad the editor runs on.
            com.armsx2.ui.common.ShaderParamsEditor.visible

    fun openInGameScreen(screen: InGameScreen) {
        overlayVisible.value = false
        inGameScreen.value = screen
    }

    fun dismissInGameScreen() {
        inGameScreen.value = null
        resumeIfPaused()
    }

    private fun resumeIfPaused() {
        if (MainActivityRuntime.eState.value == EmuState.PAUSED &&
            !com.armsx2.ui.touch.TouchControls.editMode.value
        ) {
            MainActivityRuntime.resume()
        }
    }

    @Composable
    fun Window(content: @Composable () -> Unit) {
        Box(Modifier.fillMaxSize().background(Color.Black)) {
            content()

            CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Ltr) {
                com.armsx2.ui.touch.TouchControlsOverlay()
            }

            if (showLibrary.value && MainActivityRuntime.eState.value == EmuState.RUNNING && !overlayVisible.value) {
                Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.scrim.copy(alpha = 0.56f))) {
                    com.armsx2.navigation.AppNavigation()
                    Box(
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .padding(16.dp)
                            .size(40.dp)
                            .clip(CircleShape)
                            .background(MaterialTheme.colorScheme.surfaceVariant)
                            .clickable { showLibrary.value = false },
                        contentAlignment = Alignment.Center,
                    ) {
                        Text("✕", color = MaterialTheme.colorScheme.onSurface, fontWeight = FontWeight.Bold)
                    }
                }
            }

            if (overlayVisible.value) {
                com.armsx2.ui.emulation.EmulationMenuScreen()
            }

            // Full manager screen over the paused game; dismiss (back or the screen's
            // own back arrow) resumes it. Same overlay pattern as All Settings.
            inGameScreen.value?.let { screen ->
                androidx.activity.compose.BackHandler(enabled = true) { dismissInGameScreen() }
                Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
                    val dismiss = { dismissInGameScreen() }
                    when (screen) {
                        InGameScreen.Settings -> com.armsx2.ui.settingshub.SettingsScreen(
                            initialCategory = com.armsx2.navigation.SettingsCategory.General,
                            game = MainActivityRuntime.currentGame.value,
                            onBack = dismiss,
                        )
                        InGameScreen.Achievements -> com.armsx2.ui.achievements.AchievementsScreen(onBack = dismiss)
                        InGameScreen.Memcard -> com.armsx2.ui.memorycards.MemoryCardScreen(
                            game = MainActivityRuntime.currentGame.value,
                            onBack = dismiss,
                        )
                        InGameScreen.Patches -> com.armsx2.ui.patches.PatchManagerScreen(
                            game = MainActivityRuntime.currentGame.value,
                            onBack = dismiss,
                        )
                        InGameScreen.Controls -> com.armsx2.ui.controls.ControllerManagerScreen(onBack = dismiss)
                        // Straight to the Skins tab of the real settings hub rather than a
                        // bespoke screen: it already has the scope plumbing, so opening it
                        // WITH the running game is what surfaces the per-game skin toggle.
                        InGameScreen.Skins -> com.armsx2.ui.settingshub.SettingsScreen(
                            initialCategory = com.armsx2.navigation.SettingsCategory.Skins,
                            game = MainActivityRuntime.currentGame.value,
                            onBack = dismiss,
                        )
                        InGameScreen.SaveState -> com.armsx2.ui.saves.SaveStatePickerScreen(
                            mode = com.armsx2.ui.saves.SaveMode.Save, onBack = dismiss,
                        )
                        InGameScreen.LoadState -> com.armsx2.ui.saves.SaveStatePickerScreen(
                            mode = com.armsx2.ui.saves.SaveMode.Load, onBack = dismiss,
                        )
                    }
                }
            }

            // LAST = topmost. The shader-parameter editor is opened FROM the settings tab
            // and from the pause menu, so it has to draw over both; hosting it here rather
            // than inside ShaderChainSection is also what lets it be full-screen instead of
            // a block inside a scrolling pane.
            com.armsx2.ui.common.ShaderParamsEditorHost()
        }
    }
}
