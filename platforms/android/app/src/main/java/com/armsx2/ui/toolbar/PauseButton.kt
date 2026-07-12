package com.armsx2.ui.toolbar

import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.vector.ImageVector
import com.armsx2.EmuState
import com.armsx2.Main
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.PauseSolid
import compose.icons.lineawesomeicons.PlaySolid

class PauseButton : ToolbarButton() {
    override var icon = mutableStateOf<ImageVector?>(LineAwesomeIcons.PauseSolid)
    override fun isVisible(): Boolean {
        icon.value = if (Main.eState.value == EmuState.PAUSED)
            LineAwesomeIcons.PlaySolid else LineAwesomeIcons.PauseSolid
        return Main.eState.value == EmuState.RUNNING || Main.eState.value == EmuState.PAUSED
    }

    override fun action() {
        if (Main.eState.value == EmuState.PAUSED)
            Main.resume()
        else
            Main.pause()
    }
}
