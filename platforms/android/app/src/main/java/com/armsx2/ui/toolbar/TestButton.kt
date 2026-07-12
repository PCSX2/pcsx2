package com.armsx2.ui.toolbar

import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.graphics.vector.ImageVector
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.BugSolid

class TestButton : ToolbarButton() {
    override var icon = mutableStateOf<ImageVector?>(LineAwesomeIcons.BugSolid)

    override fun action() {
        //NativeApp.runCodegenTests()
        //NativeApp.runPatchTests()
        //NativeApp.runVuJitTests()
    }
}