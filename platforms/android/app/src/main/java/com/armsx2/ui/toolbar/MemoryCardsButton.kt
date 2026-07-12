package com.armsx2.ui.toolbar

import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.vector.ImageVector
import com.armsx2.EmuState
import com.armsx2.Main
import com.armsx2.ui.MemoryCardManager
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.SaveSolid

class MemoryCardsButton : ToolbarButton() {
    override var icon = mutableStateOf<ImageVector?>(LineAwesomeIcons.SaveSolid)

    override fun isVisible(): Boolean = (Main.eState.value != EmuState.RUNNING)

    override fun action() {
        MemoryCardManager.visible.value = true
    }
}
