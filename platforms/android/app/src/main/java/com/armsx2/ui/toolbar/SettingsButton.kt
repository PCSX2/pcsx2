package com.armsx2.ui.toolbar

import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.vector.ImageVector
import com.armsx2.EmuState
import com.armsx2.Main
import com.armsx2.ui.SetupImpl
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.CogSolid

/**
 * Re-opens the setup wizard so the user can change their BIOS or ROMs folder
 * after the initial run-through. Stops the VM first if it's running so the
 * BIOS reload (next time the emu boots) sees the new file. Setup state is
 * reset to Welcome; the wizard's BIOS-and-ROMs pages allow keeping the
 * existing prefs (Next is gated on `new selection || existing pref set`),
 * so users who only want to change one of the two don't have to re-pick the
 * other.
 */
class SettingsButton : ToolbarButton() {
    override var icon = mutableStateOf<ImageVector?>(LineAwesomeIcons.CogSolid)

    override fun isVisible(): Boolean = (Main.eState.value != EmuState.RUNNING)

    override fun action() {
        if (Main.eState.value == EmuState.RUNNING || Main.eState.value == EmuState.PAUSED)
            Main.stop()

        // Reset wizard navigation state so it opens at Welcome again.
        SetupImpl.resetForReentry()
        Main.reopenSetup()
    }
}
