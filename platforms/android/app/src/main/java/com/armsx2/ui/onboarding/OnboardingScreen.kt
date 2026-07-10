package com.armsx2.ui.onboarding

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
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
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.theme.Success

@Composable
fun OnboardingScreen(viewModel: OnboardingViewModel = viewModel()) {
    val state = viewModel.state.value
    val biosPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        uri?.let(viewModel::importBios)
    }
    val folderPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        uri?.let(viewModel::addGameFolder)
    }

    LaunchedEffect(Unit) { viewModel.load() }

    ArmsBackdrop {
        Row(
            Modifier
                .fillMaxSize()
                .padding(22.dp),
            horizontalArrangement = Arrangement.spacedBy(22.dp),
        ) {
            OnboardingRail(state.page, viewModel::goTo, Modifier.width(250.dp).fillMaxHeight())
            GlassPanel(Modifier.weight(1f).fillMaxHeight(), contentPadding = 0.dp) {
                Column(Modifier.fillMaxSize()) {
                    AnimatedContent(
                        targetState = state.page,
                        modifier = Modifier.weight(1f),
                        transitionSpec = { fadeIn(tween(180)) togetherWith fadeOut(tween(120)) },
                        label = "setup-page",
                    ) { page ->
                        Box(Modifier.fillMaxSize().padding(28.dp)) {
                            when (page) {
                                0 -> WelcomePage()
                                1 -> StoragePage(state, viewModel::selectStorage)
                                2 -> BiosPage(state) { biosPicker.launch(arrayOf("application/octet-stream", "*/*")) }
                                3 -> GamesPage(state, { folderPicker.launch(null) }, viewModel::removeGameFolder)
                                else -> ReadyPage(state)
                            }
                        }
                    }
                    NavigationBar(
                        page = state.page,
                        canContinue = viewModel.canContinue(),
                        busy = state.busy,
                        onBack = viewModel::previous,
                        onNext = if (state.page == 4) viewModel::finish else viewModel::next,
                    )
                }
            }
        }
    }

    state.error?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissError,
            title = { Text("ARMSX2 setup") },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissError) { Text("OK") } },
        )
    }
}

@Composable
private fun OnboardingRail(page: Int, onPage: (Int) -> Unit, modifier: Modifier) {
    val labels = listOf("Welcome", "Storage", "BIOS", "Games", "Ready")
    GlassPanel(modifier, contentPadding = 18.dp) {
        Column {
            ArmsLogo()
            Spacer(Modifier.height(22.dp))
            Text("First-time setup", style = MaterialTheme.typography.headlineSmall)
            Text("Everything needed before your first game", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(20.dp))
            labels.forEachIndexed { index, label ->
                val active = page == index
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable { if (index <= page || MainActivityRuntime.setupComplete.value) onPage(index) }
                        .padding(vertical = 8.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Surface(
                        shape = CircleShape,
                        color = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                    ) {
                        Box(Modifier.size(30.dp), contentAlignment = Alignment.Center) {
                            Text("${index + 1}", color = if (active) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelMedium)
                        }
                    }
                    Spacer(Modifier.width(11.dp))
                    Text(label, style = MaterialTheme.typography.labelLarge, color = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface)
                }
            }
            Spacer(Modifier.weight(1f))
            Text("ARMSX2", color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelSmall)
        }
    }
}

@Composable
private fun WelcomePage() {
    Box(
        Modifier
            .fillMaxSize()
            .background(
                Brush.radialGradient(
                    listOf(MaterialTheme.colorScheme.primary.copy(alpha = 0.24f), Color.Transparent),
                    radius = 680f,
                ),
            ),
        contentAlignment = Alignment.Center,
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            ArmsLogo(showWordmark = true)
            Spacer(Modifier.height(24.dp))
            Text("PlayStation 2, built for ARM64", style = MaterialTheme.typography.headlineLarge)
            Spacer(Modifier.height(8.dp))
            Text(
                "Choose storage, add a BIOS, and select your game folders. You can change everything later.",
                style = MaterialTheme.typography.bodyLarge,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }
}

@Composable
private fun StoragePage(state: OnboardingUiState, onSelect: (StorageLocation) -> Unit) {
    SetupPage("Storage", "Choose where emulator data, memory cards, and save states are stored.") {
        Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
            ChoiceCard(
                title = "Internal storage",
                detail = "Fast, private, and recommended",
                glyph = "▣",
                selected = state.systemLocation == StorageLocation.Internal,
                onClick = { onSelect(StorageLocation.Internal) },
                modifier = Modifier.weight(1f),
            )
            ChoiceCard(
                title = "SD card",
                detail = "Use the app folder on removable storage",
                glyph = "▤",
                selected = state.systemLocation == StorageLocation.SdCard,
                onClick = { onSelect(StorageLocation.SdCard) },
                modifier = Modifier.weight(1f),
            )
        }
    }
}

@Composable
private fun BiosPage(state: OnboardingUiState, onPick: () -> Unit) {
    SetupPage("PlayStation 2 BIOS", "A legally dumped BIOS from your own console is required.") {
        if (state.biosInfo == null) {
            ChoiceCard("Select BIOS file", "BIN and ROM dumps are supported", "◉", false, onPick, Modifier.fillMaxWidth())
        } else {
            Surface(
                modifier = Modifier.fillMaxWidth(),
                shape = RoundedCornerShape(20.dp),
                color = MaterialTheme.colorScheme.primaryContainer,
                border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.55f)),
            ) {
                Row(Modifier.padding(18.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(state.biosInfo.regionFlag, fontSize = 34.sp)
                    Spacer(Modifier.width(14.dp))
                    Column(Modifier.weight(1f)) {
                        Text(state.biosName ?: "Installed BIOS", style = MaterialTheme.typography.titleLarge, maxLines = 1, overflow = TextOverflow.Ellipsis)
                        Text("${state.biosInfo.description} · ${state.biosInfo.versionString}", color = MaterialTheme.colorScheme.onPrimaryContainer, style = MaterialTheme.typography.bodySmall)
                    }
                    StatusChip("VALID", Success)
                    Spacer(Modifier.width(10.dp))
                    OutlinedButton(onClick = onPick) { Text("Replace") }
                }
            }
        }
        if (state.busy) {
            Spacer(Modifier.height(16.dp))
            CircularProgressIndicator()
        }
    }
}

@Composable
private fun GamesPage(state: OnboardingUiState, onAdd: () -> Unit, onRemove: (String) -> Unit) {
    SetupPage("Game folders", "Add one or more folders. Subfolders are scanned automatically.") {
        Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(9.dp)) {
            state.gameFolders.forEach { raw ->
                val label = Uri.parse(raw).lastPathSegment?.substringAfterLast(':')?.ifBlank { null } ?: raw
                Surface(
                    shape = RoundedCornerShape(16.dp),
                    color = MaterialTheme.colorScheme.surfaceVariant,
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    Row(Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("▦", color = MaterialTheme.colorScheme.primary, fontSize = 20.sp)
                        Spacer(Modifier.width(12.dp))
                        Text(label, modifier = Modifier.weight(1f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                        TextButton(onClick = { onRemove(raw) }) { Text("Remove", color = MaterialTheme.colorScheme.error) }
                    }
                }
            }
            OutlinedButton(onClick = onAdd, shape = RoundedCornerShape(14.dp)) { Text("Add game folder") }
        }
    }
}

@Composable
private fun ReadyPage(state: OnboardingUiState) {
    SetupPage("Ready to play", "Review your setup. ARMSX2 will scan the library after startup.") {
        Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
            SummaryCard("Storage", if (state.systemLocation == StorageLocation.Internal) "Internal" else "SD card", Modifier.weight(1f))
            SummaryCard("BIOS", state.biosInfo?.versionString ?: "Missing", Modifier.weight(1f))
            SummaryCard("Folders", state.gameFolders.size.toString(), Modifier.weight(1f))
        }
    }
}

@Composable
private fun SetupPage(title: String, description: String, content: @Composable () -> Unit) {
    Column(Modifier.fillMaxSize()) {
        Text(title, style = MaterialTheme.typography.headlineLarge)
        Spacer(Modifier.height(6.dp))
        Text(description, style = MaterialTheme.typography.bodyLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(24.dp))
        content()
    }
}

@Composable
private fun ChoiceCard(
    title: String,
    detail: String,
    glyph: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    Surface(
        onClick = onClick,
        modifier = modifier.height(150.dp),
        shape = RoundedCornerShape(22.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(2.dp, if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
    ) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.Center) {
            Text(glyph, color = MaterialTheme.colorScheme.primary, fontSize = 28.sp, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(10.dp))
            Text(title, style = MaterialTheme.typography.titleLarge)
            Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun SummaryCard(title: String, value: String, modifier: Modifier) {
    Surface(modifier, shape = RoundedCornerShape(20.dp), color = MaterialTheme.colorScheme.surfaceVariant) {
        Column(Modifier.padding(18.dp)) {
            Text(title, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(6.dp))
            Text(value, style = MaterialTheme.typography.titleLarge)
        }
    }
}

@Composable
private fun NavigationBar(
    page: Int,
    canContinue: Boolean,
    busy: Boolean,
    onBack: () -> Unit,
    onNext: () -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.58f))
            .padding(horizontal = 24.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (page > 0) OutlinedButton(onClick = onBack, enabled = !busy) { Text("Back") }
        Spacer(Modifier.weight(1f))
        Button(
            onClick = onNext,
            enabled = canContinue && !busy,
            shape = RoundedCornerShape(14.dp),
            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary),
        ) {
            Text(if (page == 4) "Open ARMSX2" else "Continue", fontWeight = FontWeight.Bold)
        }
    }
}
