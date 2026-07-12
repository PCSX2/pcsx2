package com.armsx2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
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
import androidx.compose.ui.unit.sp
import com.armsx2.EmuState
import com.armsx2.Main

object WindowImpl {
    val toolbarVisible = mutableStateOf(true)
    // Toggled by the LoadGameButton in the toolbar — when true, GamesList
    // is overlaid on top of the surface so the library is accessible while
    // a game is running. GameCard taps clear this back to false (after the
    // launchGame restart). When stopped the library renders unconditionally
    // via Main.kt's idle-state branch, so this flag is only consulted when
    // a game is actively running.
    val showLibrary = mutableStateOf(false)
    // In-game overlay (long-press while running pops it open). Mirrors the
    // PCSX2 FullscreenUI pause menu — Resume / Save+Load State / Change
    // Disc / Library / Renderer / Frame Limit / Reset / Close Game / Show
    // Toolbar. Auto-pauses VM on open; close paths handle resume themselves.
    // Suppresses the side toolbar and library overlay while open so we
    // don't pile three layers of UI on top of the surface.
    val overlayVisible = mutableStateOf(false)
    @Composable
    fun Window(content: @Composable () -> Unit) {
        //Container
        Box(Modifier.fillMaxSize().background(Color.DarkGray)) {
            //Container Layout
            Row(Modifier.fillMaxSize()) {
                //Game View
                Box(Modifier.weight(1f).background(Color.Transparent)) {
                    content.invoke()
                    // Library overlay — shown only while a game is running
                    // (when stopped, Main.kt's STOPPED branch already paints
                    // GamesList over the same area, so overlaying again would
                    // be redundant). Suppressed while the in-game overlay is
                    // up so we don't double-stack.
                    if (showLibrary.value && Main.eState.value == EmuState.RUNNING && !overlayVisible.value) {
                        // Partial-alpha backdrop so the live game surface
                        // shows through behind the library — covers and
                        // text inside the cards stay fully opaque.
                        Box(Modifier.fillMaxSize().background(Color(0xFF101010).copy(alpha = 0.5f))) {
                            ScaledUi { GamesList.GamesRow() }
                            // Top-right close button. Only relevant in this
                            // overlay path (when stopped, GamesList is the
                            // primary screen and there's nothing to "close
                            // back to" — the toolbar is always visible).
                            Box(
                                modifier = Modifier
                                    .align(Alignment.TopEnd)
                                    .padding(16.dp)
                                    .size(36.dp)
                                    .clip(CircleShape)
                                    .background(Color(0xFF222222))
                                    .clickable { showLibrary.value = false },
                                contentAlignment = Alignment.Center,
                            ) {
                                Text(
                                    "✕",
                                    color = Color.White,
                                    fontSize = 18.sp,
                                    fontWeight = FontWeight.Bold,
                                )
                            }
                        }
                    }
                    // Touch controls layer — sits between the game surface
                    // and the pause/library overlays. Self-gates on
                    // eState + visibility latch, so safe to always invoke.
                    // Force LTR: the overlay positions buttons/stick with
                    // Modifier.offset(x=…) which Compose auto-mirrors under an
                    // RTL locale (Arabic), flipping the controls left<->right
                    // (visual desynced from the fractional-coord hit-testing).
                    // The on-screen controls are an absolute layout, never RTL.
                    CompositionLocalProvider(LocalLayoutDirection provides LayoutDirection.Ltr) {
                        com.armsx2.ui.touch.TouchControlsOverlay()
                    }
                    // In-game overlay paints last so it's on top of both
                    // the surface and any library showing underneath.
                    if (overlayVisible.value) {
                        ScaledUi { InGameOverlay.Render() }
                    }
                    if (MemoryCardManager.visible.value) {
                        ScaledUi { MemoryCardManager.Render() }
                    }
                }
                // The stopped-state library now owns its navigation chrome.
                // Keeping the legacy side toolbar here makes the redesigned
                // shelf UI look like the old app with a new panel bolted on.
            }
        }
    }
}
