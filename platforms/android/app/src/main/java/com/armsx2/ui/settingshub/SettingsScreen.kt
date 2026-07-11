package com.armsx2.ui.settingshub

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.GameInfo
import com.armsx2.i18n.str
import com.armsx2.navigation.SettingsCategory
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.settings.controllerFocusable
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
import com.armsx2.ui.settings.LocalSettingsScrollState
import com.armsx2.ui.settings.verticalScrollbar

private data class SettingsSection(val category: SettingsCategory, val titleKey: String, val glyph: String)

@Composable
fun SettingsScreen(
    initialCategory: SettingsCategory,
    game: GameInfo?,
    onBack: () -> Unit,
    viewModel: SettingsViewModel = viewModel(),
) {
    var showReset by remember { mutableStateOf(false) }
    val screenScroll = rememberScrollState()
    LaunchedEffect(initialCategory, game?.uri) { viewModel.load(initialCategory, game) }
    val ui = viewModel.uiState.value
    val contentReady = ui.game?.uri?.toString() == game?.uri?.toString()
    val displayedCategory = if (game != null && ui.category == SettingsCategory.General) {
        SettingsCategory.Performance
    } else {
        ui.category
    }

    ArmsBackdrop {
        CompositionLocalProvider(LocalSettingsScrollState provides screenScroll) {
            Column(
                Modifier
                    .fillMaxSize()
                    .verticalScroll(screenScroll)
                    .verticalScrollbar(screenScroll)
                    .padding(bottom = 8.dp),
            ) {
                ArmsTopBar(
                    title = game?.title ?: str("action.settings"),
                    subtitle = if (game == null) str("scope.global") else str("scope.game"),
                    leading = { RoundAction("‹", str("action.back"), onBack) },
                    actions = { RoundAction("↺", str("action.reset"), { showReset = true }) },
                )
                if (contentReady) {
                    SettingsCategoryBar(displayedCategory, gameSpecific = game != null, viewModel::selectCategory)
                    Spacer(Modifier.height(10.dp))
                    SettingsPanel(displayedCategory, viewModel, Modifier.fillMaxWidth())
                    Spacer(Modifier.height(16.dp))
                }
            }
        }
    }

    if (showReset) {
        AlertDialog(
            onDismissRequest = { showReset = false },
            title = { Text(str("action.reset")) },
            text = { Text(if (game == null) str("scope.global") else str("scope.game")) },
            confirmButton = {
                TextButton(onClick = { viewModel.resetCurrentScope(); showReset = false }) {
                    Text(str("action.reset"), color = MaterialTheme.colorScheme.error)
                }
            },
            dismissButton = { TextButton(onClick = { showReset = false }) { Text(str("action.cancel")) } },
        )
    }
}

@Composable
private fun SettingsPanel(
    category: SettingsCategory,
    viewModel: SettingsViewModel,
    modifier: Modifier,
) {
    GlassPanel(modifier = modifier.padding(horizontal = 8.dp), contentPadding = 16.dp) {
        Column(Modifier.fillMaxWidth().background(MaterialTheme.colorScheme.surface)) {
            SectionTitle(categoryTitle(category))
            Spacer(Modifier.height(12.dp))
            Box(Modifier.fillMaxWidth()) {
                CategoryContent(category, viewModel)
            }
        }
    }
}

@Composable
private fun SettingsCategoryBar(
    selected: SettingsCategory,
    gameSpecific: Boolean,
    onSelect: (SettingsCategory) -> Unit,
) {
    LazyRow(
        modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp),
        contentPadding = PaddingValues(horizontal = 8.dp),
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        val sections = settingsSections().filterNot {
            // General is redundant per-game (redirects to Performance); Info only makes
            // sense for a specific game, so hide it in the global settings.
            (gameSpecific && it.category == SettingsCategory.General) ||
                (!gameSpecific && it.category == SettingsCategory.Info)
        }
        items(sections, key = { it.category.name }) { section ->
            val active = section.category == selected
            FilterChip(
                modifier = Modifier.height(44.dp)
                    .controllerFocusable(
                        "settings.chip.${section.category.name}",
                        RoundedCornerShape(13.dp),
                        onConfirm = { onSelect(section.category) },
                    ),
                selected = active,
                onClick = { onSelect(section.category) },
                label = { Text(str(section.titleKey), maxLines = 1) },
                leadingIcon = {
                    Box(Modifier.size(20.dp), contentAlignment = Alignment.Center) {
                        Text(
                            section.glyph,
                            fontSize = 15.sp,
                            color = if (active) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                },
                shape = RoundedCornerShape(13.dp),
                colors = FilterChipDefaults.filterChipColors(
                    containerColor = Color.Transparent,
                    labelColor = MaterialTheme.colorScheme.onSurfaceVariant,
                    iconColor = MaterialTheme.colorScheme.primary,
                    selectedContainerColor = MaterialTheme.colorScheme.primaryContainer,
                    selectedLabelColor = MaterialTheme.colorScheme.onPrimaryContainer,
                    selectedLeadingIconColor = MaterialTheme.colorScheme.onPrimaryContainer,
                ),
            )
        }
    }
}

private fun settingsSections() = listOf(
    SettingsSection(SettingsCategory.General, "tab.app", "⌂"),
    SettingsSection(SettingsCategory.Info, "tab.info", "ⓘ"),
    SettingsSection(SettingsCategory.Performance, "tab.performance", "↯"),
    SettingsSection(SettingsCategory.Graphics, "tab.renderer", "◫"),
    SettingsSection(SettingsCategory.Audio, "tab.audio", "♫"),
    SettingsSection(SettingsCategory.Controls, "tab.controls", "⌁"),
    SettingsSection(SettingsCategory.Hotkeys, "tab.hotkeys", "⌘"),
    SettingsSection(SettingsCategory.Network, "tab.network", "◎"),
    SettingsSection(SettingsCategory.OnScreen, "tab.overlay", "⊕"),
    SettingsSection(SettingsCategory.Skins, "tab.skins", "◈"),
    SettingsSection(SettingsCategory.Advanced, "tab.fixes", "⌘"),
    SettingsSection(SettingsCategory.Recompiler, "tab.recompiler", "⚙"),
)

@Composable
private fun CategoryContent(category: SettingsCategory, viewModel: SettingsViewModel) {
    when (category) {
        SettingsCategory.General -> AppTab()
        SettingsCategory.Info -> com.armsx2.ui.settings.InfoTab(viewModel.uiState.value.game)
        SettingsCategory.Performance -> PerformanceTab(viewModel.settings)
        SettingsCategory.Graphics -> RendererTab(viewModel.settings)
        SettingsCategory.Audio -> AudioTab(viewModel.settings)
        SettingsCategory.Controls -> PadTab(viewModel.settings)
        SettingsCategory.Hotkeys -> HotkeysTab(viewModel.settings)
        SettingsCategory.Network -> NetworkTab(viewModel.settings)
        SettingsCategory.OnScreen -> OverlayTab(viewModel.settings)
        SettingsCategory.Skins -> SkinsTab(viewModel.settings)
        SettingsCategory.Advanced -> FixesTab(viewModel.settings)
        SettingsCategory.Recompiler -> RecompilerTab(viewModel.settings)
    }
}

@Composable
private fun categoryTitle(category: SettingsCategory): String = when (category) {
    SettingsCategory.General -> str("tab.app")
    SettingsCategory.Info -> str("tab.info")
    SettingsCategory.Performance -> str("tab.performance")
    SettingsCategory.Graphics -> str("tab.renderer")
    SettingsCategory.Audio -> str("tab.audio")
    SettingsCategory.Controls -> str("tab.controls")
    SettingsCategory.Hotkeys -> str("tab.hotkeys")
    SettingsCategory.Network -> str("tab.network")
    SettingsCategory.OnScreen -> str("tab.overlay")
    SettingsCategory.Skins -> str("tab.skins")
    SettingsCategory.Advanced -> str("tab.fixes")
    SettingsCategory.Recompiler -> str("tab.recompiler")
}
