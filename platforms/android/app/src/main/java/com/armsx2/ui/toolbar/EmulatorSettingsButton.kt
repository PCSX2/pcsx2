package com.armsx2.ui.toolbar

import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.vector.ImageVector
import com.armsx2.EmuState
import com.armsx2.Main
import com.armsx2.ui.InGameOverlay
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.SlidersHSolid

/** Opens global emulator settings without re-entering the setup wizard. */
class EmulatorSettingsButton : ToolbarButton() {
    override var icon = mutableStateOf<ImageVector?>(LineAwesomeIcons.SlidersHSolid)

    override fun isVisible(): Boolean = (Main.eState.value != EmuState.RUNNING)

    override fun action() {
        InGameOverlay.openGlobalSettings()
    }
}
