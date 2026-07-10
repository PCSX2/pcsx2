package com.armsx2.ui.emulation

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
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
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.GameCoverArt
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.theme.Danger
import com.armsx2.ui.theme.Success

@Composable
fun EmulationMenuScreen(viewModel: EmulationMenuViewModel = viewModel()) {
    val state = viewModel.state.value
    DisposableEffect(viewModel) {
        EmulationMenuInputController.bind(viewModel)
        onDispose { EmulationMenuInputController.unbind(viewModel) }
    }
    BackHandler { viewModel.resume() }

    AnimatedVisibility(
        visible = true,
        enter = fadeIn(tween(140)),
        exit = fadeOut(tween(110)),
    ) {
        Box(Modifier.fillMaxSize()) {
            Box(
                Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.58f))
                    .clickable(onClick = viewModel::resume),
            )
            AnimatedVisibility(
                visible = true,
                enter = slideInHorizontally(tween(250)) { it },
                exit = slideOutHorizontally(tween(190)) { it },
                modifier = Modifier.align(Alignment.CenterEnd),
            ) {
                Surface(
                    modifier = Modifier
                        .fillMaxHeight()
                        .fillMaxWidth(0.76f)
                        .widthIn(min = 620.dp),
                    shape = RoundedCornerShape(topStart = 30.dp, bottomStart = 30.dp),
                    color = MaterialTheme.colorScheme.surface.copy(alpha = 0.98f),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.6f)),
                    shadowElevation = 24.dp,
                ) {
                    Row(Modifier.fillMaxSize()) {
                        MenuRail(state.tab, viewModel::selectTab)
                        Column(Modifier.weight(1f).fillMaxHeight()) {
                            MenuHeader(viewModel::resume)
                            Box(Modifier.weight(1f).fillMaxWidth().padding(horizontal = 22.dp, vertical = 12.dp)) {
                                when (state.tab) {
                                    EmulationMenuTab.Session -> SessionPane(state, viewModel)
                                    EmulationMenuTab.States -> StatesPane(state, viewModel)
                                    EmulationMenuTab.Graphics -> GraphicsPane(state, viewModel)
                                    EmulationMenuTab.Controls -> ControlsPane(state, viewModel)
                                    EmulationMenuTab.Achievements -> AchievementsPane(state, viewModel)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun MenuRail(selected: EmulationMenuTab, onSelect: (EmulationMenuTab) -> Unit) {
    Column(
        Modifier
            .fillMaxHeight()
            .width(168.dp)
            .background(MaterialTheme.colorScheme.background.copy(alpha = 0.78f))
            .padding(12.dp),
    ) {
        ArmsLogo(showWordmark = false)
        Spacer(Modifier.height(18.dp))
        EmulationMenuTab.entries.forEach { tab ->
            val active = tab == selected
            Surface(
                onClick = { onSelect(tab) },
                modifier = Modifier.fillMaxWidth().padding(vertical = 3.dp),
                shape = RoundedCornerShape(14.dp),
                color = if (active) MaterialTheme.colorScheme.primaryContainer else Color.Transparent,
            ) {
                Text(
                    tab.title,
                    modifier = Modifier.padding(horizontal = 12.dp, vertical = 11.dp),
                    color = if (active) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurfaceVariant,
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = if (active) FontWeight.Bold else FontWeight.Medium,
                )
            }
        }
        Spacer(Modifier.weight(1f))
        Text("L1 / R1 tabs", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}

@Composable
private fun MenuHeader(onClose: () -> Unit) {
    val game = MainActivityRuntime.currentGame.value
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 22.dp, vertical = 15.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (game != null) {
            GameCoverArt(game, Modifier.width(42.dp).height(58.dp))
            Spacer(Modifier.width(12.dp))
        }
        Column(Modifier.weight(1f)) {
            Text(game?.title ?: "PlayStation 2", style = MaterialTheme.typography.titleLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(game?.serial ?: "Paused", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        StatusChip("PAUSED")
        Spacer(Modifier.width(10.dp))
        Surface(onClick = onClose, shape = RoundedCornerShape(12.dp), color = MaterialTheme.colorScheme.surfaceVariant) {
            Text("Resume", modifier = Modifier.padding(horizontal = 14.dp, vertical = 10.dp), color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
        }
    }
}

@Composable
private fun SessionPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    ActionGrid(
        actions = listOf(
            MenuAction("Resume game", "Return to gameplay", "▶", Success, viewModel::resume),
            MenuAction("Game library", "Choose another title", "▦", null, viewModel::openLibrary),
            MenuAction("Restart game", "Reset the virtual console", "↻", null, MainActivityRuntime::restart),
            MenuAction("Close game", "Return to the library", "■", Danger, { MainActivityRuntime.stop(false) }),
        ),
        selected = state.selectedAction,
        onSelect = viewModel::selectAction,
    )
}

@Composable
private fun StatesPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    Column(Modifier.verticalScroll(rememberScrollState())) {
        Text("Slot ${state.saveSlot + 1}", style = MaterialTheme.typography.headlineMedium)
        Text("Choose a slot, then save or load instantly.", color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(16.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
            repeat(10) { slot ->
                Surface(
                    onClick = { viewModel.setSaveSlot(slot) },
                    shape = RoundedCornerShape(11.dp),
                    color = if (slot == state.saveSlot) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
                ) {
                    Text("${slot + 1}", modifier = Modifier.padding(horizontal = 11.dp, vertical = 9.dp), fontWeight = FontWeight.Bold)
                }
            }
        }
        Spacer(Modifier.height(18.dp))
        ActionGrid(
            actions = listOf(
                MenuAction("Save state", "Write slot ${state.saveSlot + 1}", "↥", null, viewModel::saveState),
                MenuAction("Load state", "Restore slot ${state.saveSlot + 1}", "↧", null, viewModel::loadState),
                MenuAction("Previous slot", "Select slot ${((state.saveSlot + 8) % 10) + 1}", "‹", null, viewModel::previousSlot),
                MenuAction("Next slot", "Select slot ${(state.saveSlot + 1) % 10 + 1}", "›", null, viewModel::nextSlot),
            ),
            selected = state.selectedAction,
            onSelect = viewModel::selectAction,
        )
    }
}

@Composable
private fun GraphicsPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    Column(Modifier.verticalScroll(rememberScrollState())) {
        Text("Renderer", style = MaterialTheme.typography.titleLarge)
        Spacer(Modifier.height(10.dp))
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf("auto" to "Auto", "vulkan" to "Vulkan", "opengl" to "OpenGL", "software" to "Software").forEach { (id, label) ->
                Surface(
                    onClick = { viewModel.setRenderer(id) },
                    shape = RoundedCornerShape(13.dp),
                    color = if (state.renderer == id) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
                    border = BorderStroke(1.dp, if (state.renderer == id) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
                ) {
                    Text(label, Modifier.padding(horizontal = 15.dp, vertical = 10.dp), fontWeight = FontWeight.SemiBold)
                }
            }
        }
        Spacer(Modifier.height(18.dp))
        Text("Internal resolution", style = MaterialTheme.typography.titleLarge)
        Spacer(Modifier.height(10.dp))
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            Surface(onClick = { viewModel.adjustUpscale(-1f) }, shape = RoundedCornerShape(12.dp), color = MaterialTheme.colorScheme.surfaceVariant) { Text("−", Modifier.padding(16.dp), fontSize = 20.sp) }
            Text("${state.upscale.toInt()}×", style = MaterialTheme.typography.headlineMedium)
            Surface(onClick = { viewModel.adjustUpscale(1f) }, shape = RoundedCornerShape(12.dp), color = MaterialTheme.colorScheme.surfaceVariant) { Text("＋", Modifier.padding(16.dp), fontSize = 20.sp) }
            Spacer(Modifier.width(12.dp))
            Surface(onClick = viewModel::toggleFrameLimit, shape = RoundedCornerShape(12.dp), color = if (state.frameLimit) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant) {
                Text(if (state.frameLimit) "Frame limit on" else "Frame limit off", Modifier.padding(horizontal = 16.dp, vertical = 12.dp))
            }
        }
    }
}

@Composable
private fun ControlsPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    ActionGrid(
        actions = listOf(
            MenuAction("Edit touch layout", "Move and resize on-screen controls", "✥", null, viewModel::editTouchControls),
            MenuAction("Toggle touch controls", "Show or hide the on-screen pad", "⊕", null, viewModel::toggleTouchControls),
        ),
        selected = state.selectedAction,
        onSelect = viewModel::selectAction,
    )
}

@Composable
private fun AchievementsPane(state: EmulationMenuUiState, viewModel: EmulationMenuViewModel) {
    Column {
        Text("RetroAchievements", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(6.dp))
        Text(state.achievementSummary, style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(18.dp))
        ActionGrid(
            actions = listOf(
                MenuAction(
                    if (state.hardcore) "Disable hardcore" else "Enable hardcore",
                    if (state.hardcore) "Save states and cheats will be restored" else "Disables save states and cheats",
                    "★",
                    if (state.hardcore) Success else null,
                    viewModel::toggleHardcore,
                ),
            ),
            selected = state.selectedAction,
            onSelect = viewModel::selectAction,
        )
    }
}

private data class MenuAction(
    val title: String,
    val detail: String,
    val glyph: String,
    val accent: Color?,
    val action: () -> Unit,
)

@Composable
private fun ActionGrid(actions: List<MenuAction>, selected: Int, onSelect: (Int) -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
        actions.forEachIndexed { index, item ->
            val active = index == selected
            Surface(
                onClick = { onSelect(index); item.action() },
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(17.dp),
                color = if (active) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
                border = BorderStroke(1.dp, if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
            ) {
                Row(Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(item.glyph, color = item.accent ?: MaterialTheme.colorScheme.primary, fontSize = 22.sp, fontWeight = FontWeight.Bold, modifier = Modifier.width(34.dp))
                    Column(Modifier.weight(1f)) {
                        Text(item.title, style = MaterialTheme.typography.titleMedium)
                        Text(item.detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    Text("›", fontSize = 22.sp, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
        }
    }
}
