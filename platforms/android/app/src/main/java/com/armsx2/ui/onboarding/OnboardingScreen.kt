package com.armsx2.ui.onboarding

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedContentTransitionScope
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
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
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.common.padFocusRing
import com.armsx2.ui.theme.Success

private val setupStepKeys = listOf(
    "setup.page.welcome.title",
    "setup.step.appData.title",
    "setup.page.bios.title",
    "setup.page.roms.title",
    "setup.button.applyFinish",
)

@Composable
fun OnboardingScreen(viewModel: OnboardingViewModel = viewModel()) {
    val state = viewModel.state.value
    val canContinue = viewModel.canContinue()
    var swipeDistance by remember { mutableFloatStateOf(0f) }
    // Item 7: BIOS onboarding is folder-based (refresh parity) — pick a folder and
    // every valid BIOS inside is imported and made available here and in the BIOS
    // settings tab. The button already reads "Pick a different folder"; this makes
    // the action match. Single-file import still lives in the BIOS settings tab.
    val biosPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        uri?.let(viewModel::importBiosFolder)
    }
    val folderPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        uri?.let(viewModel::addGameFolder)
    }
    // github flavor only: a third "Custom folder" data-root, with all-files access
    // (MANAGE_EXTERNAL_STORAGE) like the old UI. The Play build stays SAF-scoped
    // (Internal / SD only). Flow: grant all-files access if needed → pick a folder →
    // resolve the tree URI to a POSIX path the native core can write to directly.
    val context = androidx.compose.ui.platform.LocalContext.current
    val customFolderPicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri ->
        uri?.let { u ->
            runCatching {
                context.contentResolver.takePersistableUriPermission(
                    u,
                    android.content.Intent.FLAG_GRANT_READ_URI_PERMISSION or
                        android.content.Intent.FLAG_GRANT_WRITE_URI_PERMISSION,
                )
            }
            com.armsx2.runtime.MainActivityRuntime.resolveTreeUriToPosix(u.toString())
                ?.let(viewModel::selectCustomStorage)
        }
    }
    val allFilesLauncher = rememberLauncherForActivityResult(ActivityResultContracts.StartActivityForResult()) {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R &&
            android.os.Environment.isExternalStorageManager()
        ) {
            customFolderPicker.launch(null)
        }
    }
    val onCustomStorage: () -> Unit = {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R &&
            !android.os.Environment.isExternalStorageManager()
        ) {
            val manageIntent = android.content.Intent(
                android.provider.Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                android.net.Uri.parse("package:${context.packageName}"),
            )
            runCatching { allFilesLauncher.launch(manageIntent) }.onFailure {
                runCatching {
                    allFilesLauncher.launch(
                        android.content.Intent(android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION),
                    )
                }
            }
        } else {
            customFolderPicker.launch(null)
        }
    }

    LaunchedEffect(Unit) { viewModel.load() }

    ArmsBackdrop {
        BoxWithConstraints(
            Modifier
                .fillMaxSize()
                .statusBarsPadding()
                .pointerInput(state.page, state.busy, canContinue) {
                    detectHorizontalDragGestures(
                        onDragStart = { swipeDistance = 0f },
                        onHorizontalDrag = { change, amount ->
                            change.consume()
                            swipeDistance += amount
                        },
                        onDragEnd = {
                            val threshold = size.width * 0.16f
                            when {
                                swipeDistance > threshold && state.page > 0 && !state.busy -> viewModel.previous()
                                swipeDistance < -threshold && state.page < setupStepKeys.lastIndex && canContinue && !state.busy -> viewModel.next()
                            }
                            swipeDistance = 0f
                        },
                        onDragCancel = { swipeDistance = 0f },
                    )
                },
        ) {
            val landscape = maxWidth > maxHeight && maxWidth >= 600.dp
            if (landscape) {
                Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(16.dp)) {
                    LandscapeHero(
                        page = state.page,
                        modifier = Modifier.weight(0.92f).fillMaxHeight().padding(start = 8.dp, top = 16.dp, bottom = 16.dp),
                    )
                    Column(Modifier.weight(1.08f).fillMaxHeight().padding(end = 8.dp)) {
                        Spacer(Modifier.height(18.dp))
                        PageIndicator(state.page, Modifier.align(Alignment.CenterHorizontally))
                        Spacer(Modifier.height(8.dp))
                        AnimatedContent(
                            targetState = state.page,
                            modifier = Modifier.weight(1f),
                            transitionSpec = {
                                val direction = if (targetState > initialState) {
                                    AnimatedContentTransitionScope.SlideDirection.Left
                                } else {
                                    AnimatedContentTransitionScope.SlideDirection.Right
                                }
                                (slideIntoContainer(direction, tween(320)) + fadeIn(tween(220))) togetherWith
                                    (slideOutOfContainer(direction, tween(280)) + fadeOut(tween(180)))
                            },
                            label = "onboarding-landscape-page",
                        ) { page ->
                            PageViewport(compact = false) {
                                WizardPage(page, state, viewModel, showWelcomeLogo = false, biosPicker = {
                                    biosPicker.launch(null)
                                }, folderPicker = { folderPicker.launch(null) }, onCustomStorage = onCustomStorage)
                            }
                        }
                        NavigationBar(
                            page = state.page,
                            canContinue = viewModel.canContinue(),
                            busy = state.busy,
                            compact = false,
                            onBack = viewModel::previous,
                            onNext = if (state.page == setupStepKeys.lastIndex) viewModel::finish else viewModel::next,
                        )
                    }
                }
            } else {
                Column(Modifier.fillMaxSize()) {
                    Row(
                        Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 12.dp),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        ArmsLogo()
                        Spacer(Modifier.weight(1f))
                        PageIndicator(state.page)
                    }
                    AnimatedContent(
                        targetState = state.page,
                        modifier = Modifier.weight(1f),
                        transitionSpec = {
                            val direction = if (targetState > initialState) {
                                AnimatedContentTransitionScope.SlideDirection.Left
                            } else {
                                AnimatedContentTransitionScope.SlideDirection.Right
                            }
                            (slideIntoContainer(direction, tween(320)) + fadeIn(tween(220))) togetherWith
                                (slideOutOfContainer(direction, tween(280)) + fadeOut(tween(180)))
                        },
                        label = "onboarding-portrait-page",
                    ) { page ->
                        PageViewport(compact = true) {
                            WizardPage(page, state, viewModel, showWelcomeLogo = true, biosPicker = {
                                biosPicker.launch(null)
                            }, folderPicker = { folderPicker.launch(null) }, onCustomStorage = onCustomStorage)
                        }
                    }
                    NavigationBar(
                        page = state.page,
                        canContinue = viewModel.canContinue(),
                        busy = state.busy,
                        compact = true,
                        onBack = viewModel::previous,
                        onNext = if (state.page == setupStepKeys.lastIndex) viewModel::finish else viewModel::next,
                    )
                }
            }
        }
    }

    state.error?.let { message ->
        AlertDialog(
            onDismissRequest = viewModel::dismissError,
            title = { Text(str("setup.welcome.heading")) },
            text = { Text(message) },
            confirmButton = { TextButton(onClick = viewModel::dismissError) { Text(str("action.ok")) } },
        )
    }
}

@Composable
private fun WizardPage(
    page: Int,
    state: OnboardingUiState,
    viewModel: OnboardingViewModel,
    showWelcomeLogo: Boolean,
    biosPicker: () -> Unit,
    folderPicker: () -> Unit,
    onCustomStorage: () -> Unit,
) {
    when (page) {
        0 -> WelcomePage(compact = true, showLogo = showWelcomeLogo)
        1 -> StoragePage(state, compact = true, viewModel::selectStorage, onCustomStorage)
        2 -> BiosPage(state, compact = true, onPick = biosPicker, onSelectBios = viewModel::selectBiosCandidate)
        3 -> GamesPage(state, folderPicker, viewModel::removeGameFolder)
        else -> ReadyPage(state, compact = true)
    }
}

@Composable
private fun LandscapeHero(page: Int, modifier: Modifier = Modifier) {
    Column(modifier, verticalArrangement = Arrangement.Center, horizontalAlignment = Alignment.CenterHorizontally) {
        Surface(
            modifier = Modifier.size(82.dp),
            shape = RoundedCornerShape(26.dp),
            color = MaterialTheme.colorScheme.primaryContainer,
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.5f)),
        ) { Box(contentAlignment = Alignment.Center) { ArmsLogo(showWordmark = false) } }
        Spacer(Modifier.height(24.dp))
        Text(
            str(setupStepKeys[page]),
            style = MaterialTheme.typography.displaySmall,
            color = MaterialTheme.colorScheme.onBackground,
            textAlign = TextAlign.Center,
        )
        Spacer(Modifier.height(10.dp))
        Text(
            str(if (page == 0) "setup.welcome.subheading" else "setup.recommended.subtitle"),
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = TextAlign.Center,
        )
    }
}

@Composable
private fun PageIndicator(page: Int, modifier: Modifier = Modifier) {
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        setupStepKeys.indices.forEach { index ->
            Surface(
                modifier = Modifier.width(if (index == page) 26.dp else 8.dp).height(8.dp),
                shape = CircleShape,
                color = if (index == page) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outlineVariant,
            ) {}
        }
    }
}

@Composable
private fun PageViewport(compact: Boolean, content: @Composable () -> Unit) {
    Box(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(
                horizontal = 8.dp,
                vertical = if (compact) 18.dp else 22.dp,
            ),
    ) {
        Box(Modifier.align(Alignment.TopCenter).fillMaxWidth().widthIn(max = 860.dp)) {
            content()
        }
    }
}

@Composable
private fun WelcomePage(compact: Boolean, showLogo: Boolean) {
    Column(
        Modifier.fillMaxWidth(),
        horizontalAlignment = if (compact) Alignment.Start else Alignment.CenterHorizontally,
    ) {
        if (showLogo) {
            Surface(
                modifier = Modifier.size(if (compact) 72.dp else 82.dp),
                shape = RoundedCornerShape(if (compact) 18.dp else 22.dp),
                color = MaterialTheme.colorScheme.primaryContainer,
                border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.55f)),
            ) {
                Box(contentAlignment = Alignment.Center) { ArmsLogo(showWordmark = false) }
            }
            Spacer(Modifier.height(if (compact) 18.dp else 24.dp))
        }
        Text(
            str("setup.welcome.heading"),
            style = MaterialTheme.typography.displayLarge,
            color = MaterialTheme.colorScheme.onSurface,
            textAlign = if (compact) TextAlign.Start else TextAlign.Center,
        )
        Spacer(Modifier.height(9.dp))
        Text(
            str("setup.systemDir.intro"),
            style = MaterialTheme.typography.bodyLarge,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            textAlign = if (compact) TextAlign.Start else TextAlign.Center,
        )
        Spacer(Modifier.height(if (compact) 22.dp else 30.dp))
        if (compact) {
            Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
                SetupBenefit("01", str("setup.step.appData.title"))
                SetupBenefit("02", str("setup.step.bios.title"))
                SetupBenefit("03", str("setup.step.rom.title"))
            }
        } else {
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                SetupBenefit("01", str("setup.step.appData.title"), Modifier.weight(1f))
                SetupBenefit("02", str("setup.step.bios.title"), Modifier.weight(1f))
                SetupBenefit("03", str("setup.step.rom.title"), Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun SetupBenefit(number: String, text: String, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier.fillMaxWidth().defaultMinSize(minHeight = 66.dp),
        shape = RoundedCornerShape(16.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.68f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.35f)),
    ) {
        Row(Modifier.padding(horizontal = 18.dp, vertical = 16.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(number, color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.labelLarge)
            Spacer(Modifier.width(11.dp))
            Text(text, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurface)
        }
    }
}

@Composable
private fun StoragePage(
    state: OnboardingUiState,
    compact: Boolean,
    onSelect: (StorageLocation) -> Unit,
    onCustom: () -> Unit,
) {
    SetupPage(str("setup.step.appData.title"), str("setup.step.appData.description.play")) {
        if (compact) {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                StorageChoices(state, onSelect, onCustom)
            }
        } else {
            Row(horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                StorageChoices(state, onSelect, onCustom)
            }
        }
    }
}

@Composable
private fun androidx.compose.foundation.layout.ColumnScope.StorageChoices(
    state: OnboardingUiState,
    onSelect: (StorageLocation) -> Unit,
    onCustom: () -> Unit,
) {
    ChoiceCard(
        title = str("setup.storageChooser.internalShort"),
        detail = str("setup.systemDir.appPrivateSubtitle"),
        glyph = "▣",
        selected = state.systemLocation == StorageLocation.Internal,
        onClick = { onSelect(StorageLocation.Internal) },
        modifier = Modifier.fillMaxWidth(),
    )
    ChoiceCard(
        title = str("setup.systemDir.sdCard"),
        detail = str("setup.step.appData.description.play"),
        glyph = "▤",
        selected = state.systemLocation == StorageLocation.SdCard,
        onClick = { onSelect(StorageLocation.SdCard) },
        modifier = Modifier.fillMaxWidth(),
    )
    // github APK only: custom folder with all-files access (like the old UI).
    if (com.armsx2.BuildConfig.STORAGE_ALL_FILES) {
        ChoiceCard(
            title = str("setup.storageChooser.customShort"),
            detail = str("setup.storageChooser.customSubtitle"),
            glyph = "▦",
            selected = state.systemLocation == StorageLocation.Custom,
            onClick = onCustom,
            modifier = Modifier.fillMaxWidth(),
        )
    }
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.StorageChoices(
    state: OnboardingUiState,
    onSelect: (StorageLocation) -> Unit,
    onCustom: () -> Unit,
) {
    ChoiceCard(
        title = str("setup.storageChooser.internalShort"),
        detail = str("setup.systemDir.appPrivateSubtitle"),
        glyph = "▣",
        selected = state.systemLocation == StorageLocation.Internal,
        onClick = { onSelect(StorageLocation.Internal) },
        modifier = Modifier.weight(1f),
    )
    ChoiceCard(
        title = str("setup.systemDir.sdCard"),
        detail = str("setup.step.appData.description.play"),
        glyph = "▤",
        selected = state.systemLocation == StorageLocation.SdCard,
        onClick = { onSelect(StorageLocation.SdCard) },
        modifier = Modifier.weight(1f),
    )
    if (com.armsx2.BuildConfig.STORAGE_ALL_FILES) {
        ChoiceCard(
            title = str("setup.storageChooser.customShort"),
            detail = str("setup.storageChooser.customSubtitle"),
            glyph = "▦",
            selected = state.systemLocation == StorageLocation.Custom,
            onClick = onCustom,
            modifier = Modifier.weight(1f),
        )
    }
}

@Composable
private fun BiosPage(state: OnboardingUiState, compact: Boolean, onPick: () -> Unit, onSelectBios: (BiosCandidate) -> Unit) {
    SetupPage(str("setup.page.bios.title"), str("setup.step.bios.description")) {
        when {
            state.biosInfo == null -> {
                ChoiceCard(
                    title = str("setup.bios.selectTitle"),
                    detail = str("setup.step.bios.description"),
                    glyph = "◉",
                    selected = false,
                    onClick = onPick,
                    modifier = Modifier.fillMaxWidth(),
                )
            }
            // A folder import turned up several BIOSes — let the user pick the active
            // one right here instead of accepting the auto-selected first entry.
            state.biosOptions.size > 1 -> {
                Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
                    Text(
                        "${state.biosOptions.size} ${str("setup.bios.multipleFound")}",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    state.biosOptions.forEach { candidate ->
                        BiosOptionRow(
                            candidate = candidate,
                            selected = candidate.path == state.selectedBiosPath,
                            onClick = { onSelectBios(candidate) },
                        )
                    }
                    OutlinedButton(onClick = onPick, modifier = Modifier.fillMaxWidth()) { Text(str("setup.button.pickDifferentFolder")) }
                }
            }
            else -> {
                Surface(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(20.dp),
                    color = MaterialTheme.colorScheme.primaryContainer,
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.55f)),
                ) {
                    if (compact) {
                        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                            BiosDetails(state)
                            OutlinedButton(onClick = onPick, modifier = Modifier.fillMaxWidth()) { Text(str("setup.button.pickDifferentFolder")) }
                        }
                    } else {
                        Row(Modifier.padding(18.dp), verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f)) { BiosDetails(state) }
                            Spacer(Modifier.width(14.dp))
                            OutlinedButton(onClick = onPick) { Text(str("setup.button.choose")) }
                        }
                    }
                }
            }
        }
        if (state.busy) {
            Spacer(Modifier.height(18.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                Spacer(Modifier.width(10.dp))
                Text(str("setup.bios.scanning"), color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}

@Composable
private fun BiosOptionRow(candidate: BiosCandidate, selected: Boolean, onClick: () -> Unit) {
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(16.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
        border = BorderStroke(
            1.dp,
            if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.6f) else MaterialTheme.colorScheme.outline.copy(alpha = 0.4f),
        ),
    ) {
        Row(Modifier.padding(horizontal = 14.dp, vertical = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(candidate.info.regionFlag, fontSize = 26.sp)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(candidate.name, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(
                    listOfNotNull(candidate.info.description, candidate.info.versionString).joinToString(" · "),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            if (selected) {
                Spacer(Modifier.width(10.dp))
                StatusChip(str("setup.status.biosSelected"), Success)
            }
        }
    }
}

@Composable
private fun BiosDetails(state: OnboardingUiState, modifier: Modifier = Modifier) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        Text(state.biosInfo?.regionFlag.orEmpty(), fontSize = 32.sp)
        Spacer(Modifier.width(13.dp))
        Column(Modifier.weight(1f)) {
            Text(state.biosName ?: str("setup.bios.selectTitle"), style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
            Text(
                listOfNotNull(state.biosInfo?.description, state.biosInfo?.versionString).joinToString(" · "),
                color = MaterialTheme.colorScheme.onPrimaryContainer,
                style = MaterialTheme.typography.bodySmall,
                maxLines = 2,
            )
        }
        StatusChip(str("setup.status.biosSelected"), Success)
    }
}

@Composable
private fun GamesPage(state: OnboardingUiState, onAdd: () -> Unit, onRemove: (String) -> Unit) {
    SetupPage(str("setup.page.roms.title"), str("setup.step.rom.description")) {
        Column(verticalArrangement = Arrangement.spacedBy(9.dp)) {
            state.gameFolders.forEach { raw ->
                val label = Uri.parse(raw).lastPathSegment?.substringAfterLast(':')?.ifBlank { null } ?: raw
                Surface(
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(16.dp),
                    color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.35f)),
                ) {
                    Row(Modifier.padding(14.dp), verticalAlignment = Alignment.CenterVertically) {
                        Text("▦", color = MaterialTheme.colorScheme.primary, fontSize = 20.sp)
                        Spacer(Modifier.width(12.dp))
                        Text(label, modifier = Modifier.weight(1f), maxLines = 1, overflow = TextOverflow.Ellipsis)
                        TextButton(onClick = { onRemove(raw) }) { Text(str("setup.button.remove"), color = MaterialTheme.colorScheme.error) }
                    }
                }
            }
            OutlinedButton(onClick = onAdd, modifier = Modifier.fillMaxWidth(), shape = RoundedCornerShape(14.dp)) {
                Text(if (state.gameFolders.isEmpty()) str("setup.button.pickRomsFolder") else str("setup.button.addAnotherFolder"))
            }
        }
    }
}

@Composable
private fun ReadyPage(state: OnboardingUiState, compact: Boolean) {
    SetupPage(str("setup.button.applyFinish"), str("games.scanningRoms")) {
        if (compact) {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                SummaryCard(str("setup.step.appData.title"), when (state.systemLocation) { StorageLocation.Internal -> str("setup.storageChooser.internalShort"); StorageLocation.SdCard -> str("setup.systemDir.sdCard"); StorageLocation.Custom -> str("setup.storageChooser.customShort") })
                SummaryCard(str("setup.step.bios.title"), state.biosInfo?.versionString ?: str("setup.status.notSelected"))
                SummaryCard(str("setup.step.rom.title"), state.gameFolders.size.toString())
            }
        } else {
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                SummaryCard(str("setup.step.appData.title"), when (state.systemLocation) { StorageLocation.Internal -> str("setup.storageChooser.internalShort"); StorageLocation.SdCard -> str("setup.systemDir.sdCard"); StorageLocation.Custom -> str("setup.storageChooser.customShort") }, Modifier.weight(1f))
                SummaryCard(str("setup.step.bios.title"), state.biosInfo?.versionString ?: str("setup.status.notSelected"), Modifier.weight(1f))
                SummaryCard(str("setup.step.rom.title"), state.gameFolders.size.toString(), Modifier.weight(1f))
            }
        }
    }
}

@Composable
private fun SetupPage(title: String, description: String, content: @Composable () -> Unit) {
    Column(Modifier.fillMaxWidth()) {
        Text(title, style = MaterialTheme.typography.headlineMedium, color = MaterialTheme.colorScheme.onSurface)
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
        modifier = modifier.defaultMinSize(minHeight = 96.dp)
            .padFocusRing(RoundedCornerShape(20.dp)),
        shape = RoundedCornerShape(20.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(
            if (selected) 2.dp else 1.dp,
            if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.42f),
        ),
    ) {
        Row(Modifier.padding(horizontal = 18.dp, vertical = 16.dp), verticalAlignment = Alignment.CenterVertically) {
            Surface(
                modifier = Modifier.size(56.dp),
                shape = CircleShape,
                color = if (selected) MaterialTheme.colorScheme.primary.copy(alpha = 0.16f) else MaterialTheme.colorScheme.surface,
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Text(glyph, color = MaterialTheme.colorScheme.primary, fontSize = 23.sp, fontWeight = FontWeight.Bold)
                }
            }
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) {
                Text(title, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
                Spacer(Modifier.height(2.dp))
                Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (selected) {
                Spacer(Modifier.width(10.dp))
                Text("✓", color = MaterialTheme.colorScheme.primary, fontWeight = FontWeight.Bold)
            }
        }
    }
}

@Composable
private fun SummaryCard(title: String, value: String, modifier: Modifier = Modifier) {
    Surface(
        modifier = modifier.fillMaxWidth().defaultMinSize(minHeight = 72.dp),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.74f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.36f)),
    ) {
        Row(Modifier.padding(17.dp), verticalAlignment = Alignment.CenterVertically) {
            Text(title, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.weight(1f))
            Text(value, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
        }
    }
}

@Composable
private fun NavigationBar(
    page: Int,
    canContinue: Boolean,
    busy: Boolean,
    compact: Boolean,
    onBack: () -> Unit,
    onNext: () -> Unit,
) {
    Row(
        Modifier.fillMaxWidth().padding(
            horizontal = 8.dp,
            vertical = if (compact) 12.dp else 15.dp,
        ),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (page > 0) {
            TextButton(onClick = onBack, enabled = !busy) { Text(str("action.back")) }
        } else {
            Text(
                str("setup.welcome.subheading"),
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
        Spacer(Modifier.weight(1f))
        Button(
            onClick = onNext,
            enabled = canContinue && !busy,
            modifier = Modifier.defaultMinSize(minHeight = 56.dp),
            shape = RoundedCornerShape(14.dp),
            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary),
        ) {
            if (busy) {
                CircularProgressIndicator(Modifier.size(16.dp), strokeWidth = 2.dp, color = MaterialTheme.colorScheme.onPrimary)
                Spacer(Modifier.width(8.dp))
            }
            Text(if (page == setupStepKeys.lastIndex) str("setup.button.letsGo") else str("action.confirm"), fontWeight = FontWeight.Bold)
        }
    }
}
