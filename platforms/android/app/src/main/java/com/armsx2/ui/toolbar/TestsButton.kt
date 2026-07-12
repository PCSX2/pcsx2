package com.armsx2.ui.toolbar

import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.EmuState
import com.armsx2.Main
import com.armsx2.codeGenTests
import com.armsx2.eeJitTests
import com.armsx2.eeSeqTests
import com.armsx2.patchTests
import com.armsx2.vifTests
import com.armsx2.vuJitTests
import compose.icons.LineAwesomeIcons
import compose.icons.lineawesomeicons.BugSolid

/**
 * Bug icon — opens a drawer with the runtime-test suite results
 * (codegen / patch / VU JIT / EE JIT / VIF / EE seq).
 */
class TestsButton : ToolbarButton() {
    override var icon = mutableStateOf<ImageVector?>(LineAwesomeIcons.BugSolid)
    override var drawerSize = mutableIntStateOf(280)

    override fun isVisible(): Boolean = (Main.eState.value != EmuState.RUNNING)

    @Composable
    override fun DrawerContent() {
        Box(Modifier.fillMaxSize().padding(16.dp)) {
            Column {
                Text(
                    "Runtime Tests",
                    color = Color.White,
                    fontSize = 18.sp,
                )
                Spacer(Modifier.height(12.dp))
                TestRow("Patch",          patchTests.value)
                TestRow("ARM64 CodeGen",  codeGenTests.value)
                TestRow("ARM64 JIT VU",   vuJitTests.value)
                TestRow("ARM64 JIT EE",   eeJitTests.value)
                TestRow("VIF UNPACK",     vifTests.value)
                TestRow("EE Sequences",   eeSeqTests.value)
            }
        }
    }

    @Composable
    private fun TestRow(label: String, value: String) {
        // Yellow when no result yet, green if all pass, red otherwise.
        val tint = when {
            value.isEmpty() -> Color.Yellow
            isAllPass(value) -> Color(0xFF4ADE80)
            else -> Color(0xFFFF6B6B)
        }
        Text("$label: ${value.ifEmpty { "—" }}", color = tint)
    }

    /** Result strings come back as "passed/total" from NativeApp.onTestResults. */
    private fun isAllPass(result: String): Boolean {
        val parts = result.split("/")
        if (parts.size != 2) return false
        val passed = parts[0].toIntOrNull() ?: return false
        val total = parts[1].toIntOrNull() ?: return false
        return passed == total && total > 0
    }
}
