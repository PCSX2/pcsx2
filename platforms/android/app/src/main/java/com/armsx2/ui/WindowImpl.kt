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
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalLayoutDirection
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import com.armsx2.EmuState
import com.armsx2.runtime.MainActivityRuntime
import kotlinx.coroutines.flow.first

/** A full manager screen shown as an overlay over the paused game (in-game menu). */
enum class InGameScreen { Settings, Achievements, Memcard, Patches, Controls, Skins, Textures, SaveState, LoadState }

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
        // Interface scaling (On-Screen tab: UI Size / UI Font Size). Applied HERE because
        // this is the one place every frontend surface passes through — the library, the
        // pause menu, the in-game manager screens and the shader editor are all inside
        // this Box. ScaledUi existed but had no call site at all, so both sliders wrote a
        // pref that nothing ever read: they moved, they persisted, and nothing resized.
        val baseDensity = LocalDensity.current
        ScaledUi {
            Box(Modifier.fillMaxSize().background(Color.Black)) {
                content()

                // The touch controls keep the REAL density. Their size and position come
                // from the user's own touch layout, so rescaling them would drag the
                // buttons out from under the player's thumbs — which is why the setting
                // says it doesn't touch them. The game surface needs no such guard: it is
                // fillMaxSize, so density can't move it.
                CompositionLocalProvider(
                    LocalDensity provides baseDensity,
                    LocalLayoutDirection provides LayoutDirection.Ltr,
                ) {
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
                        // Texture packs were only reachable via All Settings -> Renderer,
                        // which is the worst place for them: the pack folder must match the
                        // RUNNING game's serial, so the screen is only meaningful with a game
                        // loaded. Open it directly over the paused game like the other managers.
                        InGameScreen.Textures -> com.armsx2.ui.textures.TextureManagerScreen(onBack = dismiss)
                        InGameScreen.SaveState -> com.armsx2.ui.saves.SaveStatePickerScreen(
                            mode = com.armsx2.ui.saves.SaveMode.Save, onBack = dismiss,
                        )
                        InGameScreen.LoadState -> com.armsx2.ui.saves.SaveStatePickerScreen(
                            mode = com.armsx2.ui.saves.SaveMode.Load, onBack = dismiss,
                        )
                    }
                }
            }

                // LAST = topmost. The shader-parameter editor is opened FROM the settings
                // tab and from the pause menu, so it has to draw over both; hosting it here
                // rather than inside ShaderChainSection is also what lets it be full-screen
                // instead of a block inside a scrolling pane.
                com.armsx2.ui.common.ShaderParamsEditorHost()

                // THE on-screen keyboard host — exactly one, here, above everything.
                //
                // It used to be hosted per-screen (library + shader editor). That breaks the
                // moment a caller isn't one of those: Settings is a NAVIGATION DESTINATION
                // (AppNavigation: AppRoute.Settings), a sibling of AppRoute.Home, so opening it
                // unmounts HomeScreen and takes its host with it. A keyboard opened from the
                // per-game Info tab therefore had nothing rendering it, and only appeared once
                // the user backed out to Home and remounted the host — which is exactly how the
                // bug read: "the keyboard shows when I exit the settings menu".
                //
                // Hosting it once at the top of the Box every surface passes through makes it
                // reachable from any screen, and placing it AFTER the shader editor keeps it
                // above the one other full-screen layer that opens it.
                com.armsx2.ui.home.LibraryKeyboard.Overlay(this)

                // Transient top-left "Welcome Back!" banner (and any future brief note) — hosted
                // here for the same reason as the keyboard: reachable above every surface.
                com.armsx2.ui.WelcomeBannerOverlay(this)
            }
        }
    }
}
