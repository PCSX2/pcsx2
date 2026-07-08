package com.armsx2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.armsx2.ui.toolbar.EmulatorSettingsButton
import com.armsx2.ui.toolbar.MemoryCardsButton
import com.armsx2.ui.toolbar.PauseButton
import com.armsx2.ui.toolbar.RestartButton
import com.armsx2.ui.toolbar.SettingsButton
import com.armsx2.ui.toolbar.StopButton
import com.armsx2.ui.toolbar.TestsButton
import com.armsx2.ui.toolbar.ToolbarButton

object ToolbarImpl {

    val upperButtons = mutableListOf<ToolbarButton>()
    val lowerButtons = mutableListOf<ToolbarButton>()

    val expanded = mutableStateOf(0)
    val drawerContext = mutableStateOf<ToolbarButton?>(null)

    init {
        upperButtons.addAll(arrayOf())
        // Settings (cog) and Tests (bug) sit at the bottom of the lower
        // stack so they don't shift the run-control buttons when added.
        // TestsButton uses the drawer pattern (the only button that does),
        // exposing the runtime-test pass/fail counts on demand instead of
        // taking up the main screen.
        lowerButtons.addAll(arrayOf(PauseButton(), RestartButton(), StopButton(), TestsButton(), EmulatorSettingsButton(), MemoryCardsButton(), SettingsButton()))
    }

    @Composable
    fun Toolbar() {
        if (expanded.value > 0 && drawerContext.value != null) {
            Box(Modifier.fillMaxHeight().width(expanded.value.dp).background(Colors.surface.value)) {
                drawerContext.value?.DrawerContent()
            }
        }
        val mod = Modifier.fillMaxHeight().width(38.dp).background(Colors.surfaceDarker.value)
        Box(mod) {
            Column(Modifier.fillMaxSize().padding(2.dp)) {
                upperButtons.forEach {
                    if (it.isVisible()) {
                        it.Button()
                        Spacer(Modifier.height(2.dp))
                    }
                }
                Spacer(Modifier.weight(1f))
                lowerButtons.forEach {
                    if (it.isVisible()) {
                        it.Button()
                        Spacer(Modifier.height(2.dp))
                    }
                }
            }
        }
    }
}
