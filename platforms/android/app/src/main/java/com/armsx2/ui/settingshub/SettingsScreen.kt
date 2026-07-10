package com.armsx2.ui.settingshub

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.GameInfo
import com.armsx2.navigation.SettingsCategory
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.settings.AppTab
import com.armsx2.ui.settings.AudioTab
import com.armsx2.ui.settings.FixesTab
import com.armsx2.ui.settings.HotkeysTab
import com.armsx2.ui.settings.NetworkTab
import com.armsx2.ui.settings.OverlayTab
import com.armsx2.ui.settings.PadTab
import com.armsx2.ui.settings.PerformanceTab
import com.armsx2.ui.settings.RecompilerTab
import com.armsx2.ui.settings.RendererTab
import com.armsx2.ui.settings.SkinsTab
import com.armsx2.ui.theme.ThemeMode
import com.armsx2.ui.theme.ThemePreferences

private data class SettingsSection(val category: SettingsCategory, val title: String, val glyph: String)

@Composable
fun SettingsScreen(
    initialCategory: SettingsCategory,
    game: GameInfo?,
    onBack: () -> Unit,
    viewModel: SettingsViewModel = viewModel(),
) {
    var showReset by remember { mutableStateOf(false) }
    LaunchedEffect(initialCategory, game?.uri) { viewModel.load(initialCategory, game) }
    val ui = viewModel.uiState.value

    ArmsBackdrop {
        Column(Modifier.fillMaxSize()) {
            ArmsTopBar(
                title = game?.title ?: "Settings",
                subtitle = if (game == null) "Global emulator configuration" else "Per-game configuration",
                leading = { RoundAction("‹", "Back", onBack) },
                actions = {
                    if (game != null) StatusChip("GAME PROFILE") else StatusChip("GLOBAL")
                    RoundAction("↺", "Reset", { showReset = true })
                },
            )
            Row(
                Modifier
                    .fillMaxSize()
                    .padding(start = 22.dp, end = 22.dp, bottom = 18.dp),
                horizontalArrangement = Arrangement.spacedBy(18.dp),
            ) {
                SettingsRail(
                    selected = ui.category,
                    onSelect = viewModel::selectCategory,
                    modifier = Modifier.width(228.dp).fillMaxHeight(),
                )
                GlassPanel(
                    modifier = Modifier.weight(1f).fillMaxHeight(),
                    contentPadding = 18.dp,
                ) {
                    Column(Modifier.fillMaxSize()) {
                        SectionTitle(
                            title = categoryTitle(ui.category),
                            detail = categoryDescription(ui.category),
                        )
                        Spacer(Modifier.height(16.dp))
                        Box(Modifier.weight(1f).fillMaxWidth()) {
                            CategoryContent(ui.category, viewModel)
                        }
                    }
                }
            }
        }
    }

    if (showReset) {
        AlertDialog(
            onDismissRequest = { showReset = false },
            title = { Text(if (game == null) "Reset global settings?" else "Reset game settings?") },
            text = { Text(if (game == null) "All emulator settings will return to their defaults." else "This game will use the global settings again.") },
            confirmButton = {
                TextButton(onClick = { viewModel.resetCurrentScope(); showReset = false }) {
                    Text("Reset", color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = { TextButton(onClick = { showReset = false }) { Text("Cancel") } },
        )
    }
}

@Composable
private fun SettingsRail(
    selected: SettingsCategory,
    onSelect: (SettingsCategory) -> Unit,
    modifier: Modifier = Modifier,
) {
    val sections = listOf(
        SettingsSection(SettingsCategory.General, "General", "⌂"),
        SettingsSection(SettingsCategory.Performance, "Performance", "↯"),
        SettingsSection(SettingsCategory.Graphics, "Graphics", "◫"),
        SettingsSection(SettingsCategory.Audio, "Audio", "♫"),
        SettingsSection(SettingsCategory.Controls, "Controls", "⌁"),
        SettingsSection(SettingsCategory.Network, "Network", "◎"),
        SettingsSection(SettingsCategory.OnScreen, "On-screen", "⊕"),
        SettingsSection(SettingsCategory.Advanced, "Advanced", "⌘"),
    )
    GlassPanel(modifier, contentPadding = 12.dp) {
        Column {
            sections.forEach { section ->
                val active = selected == section.category
                Surface(
                    onClick = { onSelect(section.category) },
                    modifier = Modifier.fillMaxWidth().padding(vertical = 3.dp),
                    shape = RoundedCornerShape(14.dp),
                    color = if (active) MaterialTheme.colorScheme.primaryContainer else Color.Transparent,
                    border = if (active) BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.45f)) else null,
                ) {
                    Row(
                        Modifier.padding(horizontal = 13.dp, vertical = 11.dp),
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(11.dp),
                    ) {
                        Text(section.glyph, color = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant, fontSize = 18.sp, modifier = Modifier.width(24.dp))
                        Text(section.title, style = MaterialTheme.typography.labelLarge, fontWeight = if (active) FontWeight.Bold else FontWeight.Medium)
                    }
                }
            }
            Spacer(Modifier.weight(1f))
            Surface(
                onClick = ThemePreferences::toggle,
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(14.dp),
                color = MaterialTheme.colorScheme.surfaceVariant,
            ) {
                Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(if (ThemePreferences.mode.value == ThemeMode.Dark) "☀" else "◐", fontSize = 18.sp)
                    Spacer(Modifier.width(10.dp))
                    Text(if (ThemePreferences.mode.value == ThemeMode.Dark) "Light theme" else "Dark theme", style = MaterialTheme.typography.labelLarge)
                }
            }
        }
    }
}

@Composable
private fun CategoryContent(category: SettingsCategory, viewModel: SettingsViewModel) {
    when (category) {
        SettingsCategory.General -> AppTab()
        SettingsCategory.Performance -> PerformanceTab(viewModel.settings)
        SettingsCategory.Graphics -> RendererTab(viewModel.settings)
        SettingsCategory.Audio -> AudioTab(viewModel.settings)
        SettingsCategory.Controls -> Column {
            PadTab(viewModel.settings)
            HotkeysTab(viewModel.settings)
        }
        SettingsCategory.Network -> NetworkTab(viewModel.settings)
        SettingsCategory.OnScreen -> Column {
            OverlayTab(viewModel.settings)
            SkinsTab(viewModel.settings)
        }
        SettingsCategory.Advanced -> Column {
            FixesTab(viewModel.settings)
            RecompilerTab(viewModel.settings)
        }
    }
}

private fun categoryTitle(category: SettingsCategory): String = when (category) {
    SettingsCategory.General -> "General"
    SettingsCategory.Performance -> "Performance"
    SettingsCategory.Graphics -> "Graphics"
    SettingsCategory.Audio -> "Audio"
    SettingsCategory.Controls -> "Controls"
    SettingsCategory.Network -> "Network"
    SettingsCategory.OnScreen -> "On-screen display"
    SettingsCategory.Advanced -> "Advanced"
}

private fun categoryDescription(category: SettingsCategory): String = when (category) {
    SettingsCategory.General -> "Language, appearance, and application behavior"
    SettingsCategory.Performance -> "Speed profiles, CPU scheduling, and frame pacing"
    SettingsCategory.Graphics -> "Renderer, resolution, blending, and visual fixes"
    SettingsCategory.Audio -> "Volume, synchronization, latency, and output"
    SettingsCategory.Controls -> "Gamepads, bindings, hotkeys, and vibration"
    SettingsCategory.Network -> "Ethernet emulation and network adapter settings"
    SettingsCategory.OnScreen -> "Performance HUD and touch control appearance"
    SettingsCategory.Advanced -> "Game fixes and recompiler diagnostics"
}
