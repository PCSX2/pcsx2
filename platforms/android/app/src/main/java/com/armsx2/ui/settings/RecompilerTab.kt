package com.armsx2.ui.settings

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.armsx2.config.Settings
import com.armsx2.i18n.str
import com.armsx2.ui.InGameOverlay
import kr.co.iefriends.pcsx2.NativeApp

@Composable
fun RecompilerTab(state: MutableState<Settings>) {
    val settings = state.value
    ControllerAutoScroll(settingsScrollState())

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)

    Column(Modifier.fillMaxWidth()) {
        Text(
            str("jit.recompiler.warning"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            style = MaterialTheme.typography.bodyMedium,
            modifier = Modifier.padding(bottom = 8.dp),
        )
        ToggleRow("EE (R5900)", settings.recEE) { apply(settings.copy(recEE = it)) }
        ToggleRow("IOP (R3000)", settings.recIOP) { apply(settings.copy(recIOP = it)) }
        ToggleRow("VU0", settings.recVU0) { apply(settings.copy(recVU0 = it)) }
        ToggleRow("VU1", settings.recVU1) { apply(settings.copy(recVU1 = it)) }
        ToggleRow("Fastmem", settings.enableFastmem) { apply(settings.copy(enableFastmem = it)) }

        Spacer(Modifier.height(14.dp))
        Text(
            str("jit.diagnostics.header"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            style = MaterialTheme.typography.titleSmall,
            modifier = Modifier.padding(horizontal = 4.dp, vertical = 4.dp),
        )
        // Seed from the REAL native flag. This was a plain `remember { false }`, so leaving the
        // screen destroyed the state and the switch came back showing OFF while the native flag
        // was still on — the toggle was reporting fiction.
        var eeDiff by remember {
            mutableStateOf(runCatching { NativeApp.isEeDiffVerify() }.getOrDefault(false))
        }
        ToggleRow(
            label = str("jit.eeDiffVerify.label"),
            value = eeDiff,
            description = str("jit.eeDiffVerify.description"),
        ) {
            eeDiff = it
            runCatching { NativeApp.setEeDiffVerify(it) }
        }
    }
}
