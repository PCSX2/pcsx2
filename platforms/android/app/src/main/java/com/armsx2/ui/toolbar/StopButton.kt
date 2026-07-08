package com.armsx2.ui.toolbar

import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import com.armsx2.EmuState
import com.armsx2.Main
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.StopSolid

class StopButton : ToolbarButton() {
    override var icon = mutableStateOf<ImageVector?>(LineAwesomeIcons.StopSolid)
    override var background = mutableStateOf(Color.Red)
    override fun isVisible(): Boolean {
        return Main.eState.value == EmuState.RUNNING || Main.eState.value == EmuState.PAUSED
    }
    override fun action() = Main.stop()
}