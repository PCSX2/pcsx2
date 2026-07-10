package com.armsx2.ui.about

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction

@Composable
fun AboutScreen(onBack: () -> Unit, viewModel: AboutViewModel = viewModel()) {
    val state = viewModel.state.value
    LaunchedEffect(Unit) { viewModel.load() }
    ArmsBackdrop {
        Column(Modifier.fillMaxSize()) {
            ArmsTopBar("About ARMSX2", "Native ARM64 PlayStation 2 emulation", leading = { RoundAction("‹", "Back", onBack) })
            Row(
                Modifier.fillMaxSize().padding(start = 22.dp, end = 22.dp, bottom = 18.dp),
                horizontalArrangement = Arrangement.spacedBy(18.dp),
            ) {
                GlassPanel(Modifier.weight(1f)) {
                    Column {
                        ArmsLogo()
                        Spacer(Modifier.height(18.dp))
                        Text("ARMSX2", style = MaterialTheme.typography.headlineLarge)
                        Text("Open-source PlayStation 2 emulator for ARM64 devices.", style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
                GlassPanel(Modifier.weight(1f)) {
                    Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        InfoRow("Application", state.appVersion)
                        InfoRow("Emulation core", state.coreVersion)
                        InfoRow("Device", state.device)
                        InfoRow("System", state.androidVersion)
                    }
                }
            }
        }
    }
}

@Composable
private fun InfoRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth()) {
        Text(label, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.weight(1f))
        Text(value, style = MaterialTheme.typography.bodyMedium)
    }
}
