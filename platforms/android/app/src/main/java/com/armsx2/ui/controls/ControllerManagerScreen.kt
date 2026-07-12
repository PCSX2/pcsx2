package com.armsx2.ui.controls

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
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
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.SettingSwitchRow
import com.armsx2.ui.settings.controllerFocusable

@Composable
fun ControllerManagerScreen(onBack: () -> Unit, viewModel: ControllerManagerViewModel = viewModel()) {
    val state = viewModel.state.value
    LaunchedEffect(Unit) { viewModel.refresh() }
    DisposableEffect(viewModel) { onDispose(viewModel::cancelCapture) }

    ArmsBackdrop {
        Column(
            Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState())
                .onPreviewKeyEvent { event ->
                    if (state.capturingAction != null && event.type == KeyEventType.KeyDown) {
                        viewModel.captureKey(event.nativeKeyEvent.keyCode)
                    } else false
                },
        ) {
            ArmsTopBar(
                title = str("tab.controls"),
                subtitle = str("pad.editing.description"),
                leading = { RoundAction("‹", str("action.back"), onBack) },
                actions = { RoundAction("↺", str("action.reset"), viewModel::resetPlayer) },
            )
            BoxWithConstraints(Modifier.fillMaxWidth()) {
                val compact = maxWidth < 840.dp
                if (compact) {
                    Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp)) {
                        ControllerOptions(state, viewModel, Modifier.fillMaxWidth(), compact = true)
                        Spacer(Modifier.padding(top = 10.dp))
                        ControllerBindings(state, viewModel, Modifier.fillMaxWidth())
                    }
                } else {
                    Row(
                        Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp),
                        horizontalArrangement = Arrangement.spacedBy(14.dp),
                    ) {
                        ControllerOptions(state, viewModel, Modifier.width(286.dp), compact = false)
                        ControllerBindings(state, viewModel, Modifier.weight(1f))
                    }
                }
            }
        }
    }

    state.capturingAction?.let { action ->
        AlertDialog(
            onDismissRequest = viewModel::cancelCapture,
            title = { Text("${str("pad.action.bind")}: ${action.label}") },
            text = { Text(str("hotkeys.capturePrompt")) },
            confirmButton = { TextButton(onClick = viewModel::cancelCapture) { Text(str("action.cancel")) } },
        )
    }
}

@Composable
private fun ControllerOptions(
    state: ControllerManagerUiState,
    viewModel: ControllerManagerViewModel,
    modifier: Modifier,
    compact: Boolean,
) {
    GlassPanel(modifier) {
        Column(verticalArrangement = Arrangement.spacedBy(7.dp)) {
            if (!compact) SectionTitle(str("pad.section.playerRumble"), str("pad.editing.description"))
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                ChoiceRow(str("pad.player1"), state.player == 0, { viewModel.setPlayer(0) }, Modifier.weight(1f).controllerFocusable("controls.player1", RoundedCornerShape(14.dp), onConfirm = { viewModel.setPlayer(0) }))
                ChoiceRow(str("pad.player2"), state.player == 1, { viewModel.setPlayer(1) }, Modifier.weight(1f).controllerFocusable("controls.player2", RoundedCornerShape(14.dp), onConfirm = { viewModel.setPlayer(1) }))
            }
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                ChoiceRow(str("pad.section.buttonMapping"), state.section == ControllerSection.Buttons, { viewModel.setSection(ControllerSection.Buttons) }, Modifier.weight(1f).controllerFocusable("controls.section.buttons", RoundedCornerShape(14.dp), onConfirm = { viewModel.setSection(ControllerSection.Buttons) }))
                ChoiceRow(str("tab.hotkeys"), state.section == ControllerSection.Hotkeys, { viewModel.setSection(ControllerSection.Hotkeys) }, Modifier.weight(1f).controllerFocusable("controls.section.hotkeys", RoundedCornerShape(14.dp), onConfirm = { viewModel.setSection(ControllerSection.Hotkeys) }))
            }
            if (!compact) Spacer(Modifier.weight(1f))
            SettingSwitchRow(
                str("pad.rumble.label"), str("pad.rumble.description"), state.rumble, viewModel::setRumble,
                modifier = Modifier.controllerFocusable(
                    "controls.rumble",
                    onConfirm = { viewModel.setRumble(!state.rumble) },
                    onLeft = { if (state.rumble) viewModel.setRumble(false) },
                    onRight = { if (!state.rumble) viewModel.setRumble(true) },
                ),
            )
            SettingSwitchRow(
                str("pad.multitap.label"), str("pad.multitap.description"), state.multitap, viewModel::setMultitap,
                modifier = Modifier.controllerFocusable(
                    "controls.multitap",
                    onConfirm = { viewModel.setMultitap(!state.multitap) },
                    onLeft = { if (state.multitap) viewModel.setMultitap(false) },
                    onRight = { if (!state.multitap) viewModel.setMultitap(true) },
                ),
            )
            SettingSwitchRow(
                str("pad.dpadAsLeftStick.label"), str("pad.dpadAsLeftStick.description"), state.dpadAsStick, viewModel::setDpadAsStick,
                modifier = Modifier.controllerFocusable(
                    "controls.dpadAsStick",
                    onConfirm = { viewModel.setDpadAsStick(!state.dpadAsStick) },
                    onLeft = { if (state.dpadAsStick) viewModel.setDpadAsStick(false) },
                    onRight = { if (!state.dpadAsStick) viewModel.setDpadAsStick(true) },
                ),
            )
        }
    }
}

@Composable
private fun ControllerBindings(state: ControllerManagerUiState, viewModel: ControllerManagerViewModel, modifier: Modifier) {
    Column(modifier) {
        SectionTitle(
            if (state.section == ControllerSection.Buttons) str("pad.section.buttonMapping") else str("hotkeys.header"),
            if (state.section == ControllerSection.Buttons) str("pad.instruction.tapThenPress") else str("hotkeys.help"),
        )
        if (state.section == ControllerSection.Buttons) {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                ControllerMappings.actions.forEach { action ->
                    BindingRow(
                        controllerId = "controls.button.${action.id}",
                        label = action.label,
                        binding = ControllerMappings.labelForKey(ControllerMappings.physicalFor(action, state.player)),
                        capturing = state.capturingAction == action,
                        onBind = { viewModel.beginCapture(action) },
                        onClear = { viewModel.clear(action) },
                    )
                }
            }
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                ControllerMappings.SysHotkey.entries.forEach { hotkey ->
                    BindingRow(
                        controllerId = "controls.hotkey.${hotkey.name}",
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

@Composable
private fun ChoiceRow(label: String, selected: Boolean, onClick: () -> Unit, modifier: Modifier = Modifier) {
    Surface(
        onClick = onClick,
        modifier = modifier.defaultMinSize(minHeight = 54.dp),
        shape = RoundedCornerShape(14.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
    ) {
        Text(label, Modifier.padding(horizontal = 13.dp, vertical = 11.dp), style = MaterialTheme.typography.labelLarge)
    }
}

@Composable
private fun BindingRow(controllerId: String, label: String, binding: String, capturing: Boolean, onBind: () -> Unit, onClear: () -> Unit) {
    Surface(
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(17.dp),
        color = if (capturing) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, if (capturing) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
    ) {
        Column(Modifier.padding(12.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(label, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                Text(
                    binding.ifBlank { str("hotkeys.notSet") },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                )
            }
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.End) {
                TextButton(onClick = onClear, modifier = Modifier.controllerFocusable("$controllerId.clear", onConfirm = onClear)) { Text(str("pad.action.clear")) }
                OutlinedButton(onClick = onBind, modifier = Modifier.controllerFocusable("$controllerId.bind", onConfirm = onBind)) { Text(if (capturing) str("hotkeys.capturePrompt") else str("pad.action.bind")) }
            }
        }
    }
}
