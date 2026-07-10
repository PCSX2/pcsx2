package com.armsx2.ui.controls

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.key.KeyEventType
import androidx.compose.ui.input.key.onPreviewKeyEvent
import androidx.compose.ui.input.key.type
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.input.ControllerMappings
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.SettingSwitchRow

@Composable
fun ControllerManagerScreen(onBack: () -> Unit, viewModel: ControllerManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    LaunchedEffect(Unit) { viewModel.refresh() }
    DisposableEffect(viewModel) { onDispose(viewModel::cancelCapture) }

    ArmsBackdrop {
        Column(
            Modifier
                .fillMaxSize()
                .onPreviewKeyEvent { event ->
                    if (state.capturingAction != null && event.type == KeyEventType.KeyDown) {
                        viewModel.captureKey(event.nativeKeyEvent.keyCode)
                    } else false
                },
        ) {
            ArmsTopBar(
                title = "Controllers",
                subtitle = "Bindings, players, hotkeys, and vibration",
                leading = { RoundAction("‹", "Back", onBack) },
                actions = { RoundAction("↺", "Reset player", viewModel::resetPlayer) },
            )
            Row(
                Modifier.fillMaxSize().padding(start = 22.dp, end = 22.dp, bottom = 18.dp),
                horizontalArrangement = Arrangement.spacedBy(18.dp),
            ) {
                GlassPanel(Modifier.width(300.dp).fillMaxHeight()) {
                    Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
                        SectionTitle("Controller setup", "Configure local players")
                        ChoiceRow("Player 1", state.player == 0) { viewModel.setPlayer(0) }
                        ChoiceRow("Player 2", state.player == 1) { viewModel.setPlayer(1) }
                        ChoiceRow("Button bindings", state.section == ControllerSection.Buttons) { viewModel.setSection(ControllerSection.Buttons) }
                        ChoiceRow("System hotkeys", state.section == ControllerSection.Hotkeys) { viewModel.setSection(ControllerSection.Hotkeys) }
                        Spacer(Modifier.weight(1f))
                        SettingSwitchRow("Vibration", "Route PS2 rumble to the active gamepad", state.rumble, viewModel::setRumble)
                        SettingSwitchRow("Multitap", "Enable additional controller slots", state.multitap, viewModel::setMultitap)
                        SettingSwitchRow("D-pad as left stick", "Map digital directions to analog movement", state.dpadAsStick, viewModel::setDpadAsStick)
                    }
                }
                Column(Modifier.weight(1f)) {
                    SectionTitle(
                        if (state.section == ControllerSection.Buttons) "Player ${state.player + 1} bindings" else "System hotkeys",
                        if (state.section == ControllerSection.Buttons) "Select an action, then press a controller button" else "Hotkeys may use one button or a two-button combo",
                    )
                    if (state.section == ControllerSection.Buttons) {
                        LazyColumn(
                            Modifier.fillMaxSize(),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                            contentPadding = PaddingValues(bottom = 20.dp),
                        ) {
                            items(ControllerMappings.actions, key = { it.id }) { action ->
                                BindingRow(
                                    label = action.label,
                                    binding = ControllerMappings.labelForKey(ControllerMappings.physicalFor(action, state.player)),
                                    capturing = state.capturingAction == action,
                                    onBind = { viewModel.beginCapture(action) },
                                    onClear = { viewModel.clear(action) },
                                )
                            }
                        }
                    } else {
                        LazyColumn(
                            Modifier.fillMaxSize(),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                            contentPadding = PaddingValues(bottom = 20.dp),
                        ) {
                            items(ControllerMappings.SysHotkey.entries, key = { it.prefKey }) { hotkey ->
                                BindingRow(
                                    label = hotkey.label,
                                    binding = ControllerMappings.hotkeyLabel(hotkey),
                                    capturing = ControllerMappings.captureHotkey.value == hotkey,
                                    onBind = { viewModel.beginHotkeyCapture(hotkey) },
                                    onClear = { viewModel.clearHotkey(hotkey) },
                                )
                            }
                        }
                    }
                }
            }
        }
    }

    state.capturingAction?.let { action ->
        AlertDialog(
            onDismissRequest = viewModel::cancelCapture,
            title = { Text("Bind ${action.label}") },
            text = { Text("Press a button on the controller.") },
            confirmButton = { TextButton(onClick = viewModel::cancelCapture) { Text("Cancel") } },
        )
    }
}

@Composable
private fun ChoiceRow(label: String, selected: Boolean, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(14.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
    ) {
        Text(label, Modifier.padding(horizontal = 13.dp, vertical = 11.dp), style = MaterialTheme.typography.labelLarge)
    }
}

@Composable
private fun BindingRow(label: String, binding: String, capturing: Boolean, onBind: () -> Unit, onClear: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(17.dp),
        color = if (capturing) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, if (capturing) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
    ) {
        Row(Modifier.padding(13.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(label, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
            Text(binding.ifBlank { "Unbound" }, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.width(10.dp))
            OutlinedButton(onClick = onBind) { Text(if (capturing) "Listening…" else "Bind") }
            TextButton(onClick = onClear) { Text("Clear") }
        }
    }
}
