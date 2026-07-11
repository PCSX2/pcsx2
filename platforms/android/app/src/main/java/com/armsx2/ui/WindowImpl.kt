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

object WindowImpl {
    val toolbarVisible = mutableStateOf(true)
    val showLibrary = mutableStateOf(false)
    val overlayVisible = mutableStateOf(false)

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
        }
    }
}
