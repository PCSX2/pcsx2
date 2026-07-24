package com.armsx2.ui.achievements

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import coil.compose.AsyncImage
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.EmptyState
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction
import com.armsx2.ui.common.SectionTitle
import com.armsx2.ui.common.SettingSwitchRow
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.settings.controllerFocusable
import com.armsx2.ui.theme.Danger
import androidx.compose.ui.graphics.Color
import com.armsx2.ui.theme.Success
import kotlinx.coroutines.delay

@Composable
fun AchievementsScreen(onBack: () -> Unit, viewModel: AchievementsViewModel = viewModel()) {
    val state = viewModel.state.value
    LaunchedEffect(Unit) {
        while (true) {
            viewModel.refresh()
            delay(3000)
        }
    }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
            ArmsTopBar(
                title = "RetroAchievements",
                leading = { RoundAction("←", str("action.back"), onBack) },
                actions = {
                    if (state.loggedIn) {
                        if (state.avatarUrl.isNotBlank()) {
                            AsyncImage(
                                state.avatarUrl,
                                state.userName,
                                Modifier.size(34.dp).clip(CircleShape),
                                contentScale = ContentScale.Crop,
                            )
                        }
                        // Both totals: hardcore points (red) and softcore points (blue).
                        StatusChip("${state.score} HC", Danger)
                        StatusChip("${state.softcoreScore} SC")
                        RoundAction("↻", str("games.card.refresh"), viewModel::refresh)
                    }
                },
            )
            if (!state.loggedIn) {
                LoginPanel(state.loading, viewModel::login, Modifier.fillMaxWidth())
            } else {
                BoxWithConstraints(Modifier.fillMaxWidth()) {
                    val compact = maxWidth < 820.dp
                    if (compact) {
                        Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp)) {
                            AchievementAccount(state, viewModel, Modifier.fillMaxWidth(), compact = true)
                            Spacer(Modifier.height(10.dp))
                            AchievementList(state, Modifier.fillMaxWidth())
                        }
                    } else {
                        Row(
                            Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp),
                            horizontalArrangement = Arrangement.spacedBy(14.dp),
                        ) {
                            AchievementAccount(state, viewModel, Modifier.width(286.dp), compact = false)
                            AchievementList(state, Modifier.weight(1f))
                        }
                    }
                }
            }
        }
    }

    state.error?.let { error ->
        AlertDialog(
            onDismissRequest = viewModel::dismissError,
            title = { Text("RetroAchievements") },
            text = { Text(error) },
            confirmButton = { TextButton(onClick = viewModel::dismissError) { Text(str("action.ok")) } },
        )
    }

    state.pendingHardcore?.let { enabling ->
        androidx.compose.runtime.DisposableEffect(Unit) {
            com.armsx2.MenuSfx.play(com.armsx2.MenuSfx.Event.POPUP_OPEN)
            onDispose { com.armsx2.MenuSfx.play(com.armsx2.MenuSfx.Event.POPUP_CLOSE) }
        }
        AlertDialog(
            onDismissRequest = viewModel::cancelToggleHardcore,
            title = { Text(str(if (enabling) "ra.hardcore.enable.title" else "ra.hardcore.disable.title")) },
            text = { Text(str(if (enabling) "ra.hardcore.enable.body" else "ra.hardcore.disable.body")) },
            confirmButton = {
                TextButton(onClick = viewModel::confirmToggleHardcore) {
                    Text(str(if (enabling) "ra.hardcore.enable.confirm" else "ra.hardcore.disable.confirm"))
                }
            },
            dismissButton = { TextButton(onClick = viewModel::cancelToggleHardcore) { Text(str("action.cancel")) } },
        )
    }
}

@Composable
private fun AchievementAccount(
    state: AchievementsUiState,
    viewModel: AchievementsViewModel,
    modifier: Modifier,
    compact: Boolean,
) {
    val soundPicker = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument(),
    ) { uri -> if (uri != null) viewModel.setUnlockSound(uri) }
    GlassPanel(modifier) {
        Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                SectionTitle(state.userName.ifBlank { str("ra.account.signedIn") }, str("ra.options.header"), Modifier.weight(1f))
                StatusChip("${state.items.count { it.unlocked }} / ${state.items.size}")
            }
            SettingSwitchRow(
                title = str("ra.mode.hardcore"),
                description = str("patches.hardcoreNoticeCheatsDisabled"),
                checked = state.hardcore,
                onCheckedChange = { viewModel.requestToggleHardcore() },
                modifier = Modifier.controllerFocusable(
                    "ra.hardcore",
                    onConfirm = { viewModel.requestToggleHardcore() },
                    onLeft = { if (state.hardcore) viewModel.requestToggleHardcore() },
                    onRight = { if (!state.hardcore) viewModel.requestToggleHardcore() },
                ),
            )
            SettingSwitchRow(
                title = str("ra.options.unlockNotifications"),
                description = str("ra.options.unlockNotifications.desc"),
                checked = state.notifications,
                onCheckedChange = { viewModel.setOption("notifications", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.notifications",
                    onConfirm = { viewModel.setOption("notifications", !state.notifications) },
                    onLeft = { if (state.notifications) viewModel.setOption("notifications", false) },
                    onRight = { if (!state.notifications) viewModel.setOption("notifications", true) },
                ),
            )
            // How long an unlock toast lingers (seconds). Only meaningful while unlock toasts are on,
            // so it slides in under the toggle like the sound-volume row does.
            if (state.notifications) {
                com.armsx2.ui.settings.IntSliderRow(
                    label = str("ra.options.notifDuration"),
                    value = state.notificationsDuration,
                    min = 3,
                    max = 30,
                    description = str("ra.options.notifDuration.desc"),
                    valueFormatter = { "${it}s" },
                    onChange = { viewModel.setOptionInt("notificationsDuration", it) },
                )
            }
            SettingSwitchRow(
                title = str("ra.options.leaderboardNotifications"),
                description = str("ra.options.leaderboardNotifications.desc"),
                checked = state.leaderboardNotifications,
                onCheckedChange = { viewModel.setOption("leaderboardNotifications", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.leaderboardNotifications",
                    onConfirm = { viewModel.setOption("leaderboardNotifications", !state.leaderboardNotifications) },
                    onLeft = { if (state.leaderboardNotifications) viewModel.setOption("leaderboardNotifications", false) },
                    onRight = { if (!state.leaderboardNotifications) viewModel.setOption("leaderboardNotifications", true) },
                ),
            )
            if (state.leaderboardNotifications) {
                com.armsx2.ui.settings.IntSliderRow(
                    label = str("ra.options.lbDuration"),
                    value = state.leaderboardsDuration,
                    min = 3,
                    max = 30,
                    description = str("ra.options.lbDuration.desc"),
                    valueFormatter = { "${it}s" },
                    onChange = { viewModel.setOptionInt("leaderboardsDuration", it) },
                )
            }
            // Where unlock/leaderboard toasts appear on screen (native OsdOverlayPos). Shown whenever
            // either notification type is on.
            if (state.notifications || state.leaderboardNotifications) {
                PositionGridPicker(
                    label = str("ra.options.notifLocation"),
                    idPrefix = "ra.notifPos",
                    current = state.notificationPosition,
                    values = OSD_OVERLAY_POS_VALUES,
                    onSelect = { viewModel.setOptionInt("notificationPosition", it) },
                )
            }
            SettingSwitchRow(
                title = str("ra.options.inGameIndicators"),
                description = str("ra.options.inGameIndicators.desc"),
                checked = state.overlays,
                onCheckedChange = { viewModel.setOption("overlays", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.overlays",
                    onConfirm = { viewModel.setOption("overlays", !state.overlays) },
                    onLeft = { if (state.overlays) viewModel.setOption("overlays", false) },
                    onRight = { if (!state.overlays) viewModel.setOption("overlays", true) },
                ),
            )
            SettingSwitchRow(
                title = str("ra.options.leaderboardTrackers"),
                description = str("ra.options.leaderboardTrackers.desc"),
                checked = state.lbOverlays,
                onCheckedChange = { viewModel.setOption("lbOverlays", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.lbOverlays",
                    onConfirm = { viewModel.setOption("lbOverlays", !state.lbOverlays) },
                    onLeft = { if (state.lbOverlays) viewModel.setOption("lbOverlays", false) },
                    onRight = { if (!state.lbOverlays) viewModel.setOption("lbOverlays", true) },
                ),
            )
            // Where challenge indicators / leaderboard trackers sit (native AchievementOverlayPosition).
            // Shown whenever either indicator type is on.
            if (state.overlays || state.lbOverlays) {
                PositionGridPicker(
                    label = str("ra.options.indicatorLocation"),
                    idPrefix = "ra.indicatorPos",
                    current = state.overlayPosition,
                    values = ACH_OVERLAY_POS_VALUES,
                    onSelect = { viewModel.setOptionInt("overlayPosition", it) },
                )
            }
            SettingSwitchRow(
                title = str("ra.options.soundEffects"),
                description = str("ra.options.soundEffects.desc"),
                checked = state.soundEffects,
                onCheckedChange = { viewModel.setOption("soundEffects", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.soundEffects",
                    onConfirm = { viewModel.setOption("soundEffects", !state.soundEffects) },
                    onLeft = { if (state.soundEffects) viewModel.setOption("soundEffects", false) },
                    onRight = { if (!state.soundEffects) viewModel.setOption("soundEffects", true) },
                ),
            )
            // Volume of that unlock sound — only meaningful while the effect is on, so it slides
            // in right under the toggle. App-side (MediaPlayer), no .wav editing needed.
            if (state.soundEffects) {
                com.armsx2.ui.settings.IntSliderRow(
                    label = str("ra.options.soundVolume"),
                    value = state.soundVolume,
                    min = 0,
                    max = 100,
                    description = str("ra.options.soundVolume.desc"),
                    valueFormatter = { if (it == 0) "Muted" else "${it}%" },
                    onChange = { viewModel.setSoundVolume(it) },
                )
            }
            // Achievement modes. Toggling reloads the RA session (no VM reset); the native
            // rc_client setters already exist, so these are plain option toggles.
            SettingSwitchRow(
                title = str("ra.options.encoreMode"),
                description = str("ra.options.encoreMode.desc"),
                checked = state.encoreMode,
                onCheckedChange = { viewModel.setOption("encoreMode", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.encoreMode",
                    onConfirm = { viewModel.setOption("encoreMode", !state.encoreMode) },
                    onLeft = { if (state.encoreMode) viewModel.setOption("encoreMode", false) },
                    onRight = { if (!state.encoreMode) viewModel.setOption("encoreMode", true) },
                ),
            )
            SettingSwitchRow(
                title = str("ra.options.spectatorMode"),
                description = str("ra.options.spectatorMode.desc"),
                checked = state.spectatorMode,
                onCheckedChange = { viewModel.setOption("spectatorMode", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.spectatorMode",
                    onConfirm = { viewModel.setOption("spectatorMode", !state.spectatorMode) },
                    onLeft = { if (state.spectatorMode) viewModel.setOption("spectatorMode", false) },
                    onRight = { if (!state.spectatorMode) viewModel.setOption("spectatorMode", true) },
                ),
            )
            SettingSwitchRow(
                title = str("ra.options.unofficialTestMode"),
                description = str("ra.options.unofficialTestMode.desc"),
                checked = state.unofficialTestMode,
                onCheckedChange = { viewModel.setOption("unofficialTestMode", it) },
                modifier = Modifier.controllerFocusable(
                    "ra.unofficialTestMode",
                    onConfirm = { viewModel.setOption("unofficialTestMode", !state.unofficialTestMode) },
                    onLeft = { if (state.unofficialTestMode) viewModel.setOption("unofficialTestMode", false) },
                    onRight = { if (!state.unofficialTestMode) viewModel.setOption("unofficialTestMode", true) },
                ),
            )
            Surface(
                onClick = { soundPicker.launch(arrayOf("audio/*")) },
                modifier = Modifier.fillMaxWidth().controllerFocusable(
                    "ra.unlockSound",
                    onConfirm = { soundPicker.launch(arrayOf("audio/*")) },
                ),
                shape = RoundedCornerShape(22.dp),
                color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.7f),
                border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
            ) {
                Row(
                    Modifier.defaultMinSize(minHeight = 78.dp).padding(horizontal = 16.dp, vertical = 14.dp),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Column(Modifier.weight(1f)) {
                        Text(str("ra.options.unlockSound"), style = MaterialTheme.typography.titleMedium)
                        Text(
                            state.unlockSoundName ?: str("ra.options.unlockSound.default"),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                        )
                    }
                    Spacer(Modifier.width(12.dp))
                    if (state.unlockSoundName != null) {
                        TextButton(
                            onClick = { viewModel.clearUnlockSound() },
                            modifier = Modifier.controllerFocusable(
                                "ra.unlockSound.reset",
                                onConfirm = { viewModel.clearUnlockSound() },
                            ),
                        ) { Text(str("action.reset")) }
                    } else {
                        Text("›", style = MaterialTheme.typography.titleLarge, color = MaterialTheme.colorScheme.primary)
                    }
                }
            }
            TextButton(
                onClick = viewModel::logout,
                modifier = Modifier.controllerFocusable("ra.logout", onConfirm = { viewModel.logout() }),
            ) { Text(str("ra.account.logout")) }
        }
    }
}

@Composable
private fun AchievementList(state: AchievementsUiState, modifier: Modifier) {
    // Show subset tabs (Base Set / Bonus Subset / …) only when a game actually has more than
    // one subset; otherwise the list looks exactly as before. The selection survives the 3s
    // poll (rememberSaveable), re-keyed on the subset id set so it resets when the game changes.
    val hasSubsets = state.subsets.size > 1
    var selectedSubset by rememberSaveable(state.subsets.joinToString { it.id.toString() }) {
        mutableStateOf(state.subsets.firstOrNull()?.id ?: 0)
    }
    // Unlocked/locked filter (DuckStation-style). Applied within the selected subset; survives the
    // 3s poll. The pill counts come from the subset-scoped list so they track what's actually shown.
    var filter by rememberSaveable { mutableStateOf(AchFilter.ALL) }
    val bySubset = if (hasSubsets) state.items.filter { it.subsetId == selectedSubset } else state.items
    val shown = when (filter) {
        AchFilter.ALL -> bySubset
        AchFilter.UNLOCKED -> bySubset.filter { it.unlocked }
        AchFilter.LOCKED -> bySubset.filter { !it.unlocked }
        AchFilter.MISSABLE -> bySubset.filter { it.type == 1 }
    }
    Column(modifier) {
        SectionTitle(str("scope.game"), shown.size.toString())
        if (hasSubsets) {
            SubsetTabs(state.subsets, selectedSubset, onSelect = { selectedSubset = it })
            Spacer(Modifier.height(10.dp))
        }
        if (bySubset.isNotEmpty()) {
            AchievementFilterTabs(filter, bySubset, onSelect = { filter = it })
            Spacer(Modifier.height(10.dp))
        }
        if (shown.isEmpty()) {
            EmptyState(str("ra.status.noAchievements.title"), str("ra.status.noAchievements.body"), modifier = Modifier.fillMaxWidth().height(220.dp))
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                shown.forEach { item -> AchievementRow(item) }
            }
        }
    }
}

@Composable
private fun SubsetTabs(subsets: List<Subset>, selectedId: Int, onSelect: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        subsets.forEach { sub ->
            val selected = sub.id == selectedId
            Surface(
                shape = RoundedCornerShape(999.dp),
                color = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                contentColor = if (selected) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier
                    .clip(RoundedCornerShape(999.dp))
                    .clickable { onSelect(sub.id) }
                    .controllerFocusable("ra.subset.${sub.id}", onConfirm = { onSelect(sub.id) }),
            ) {
                Text(
                    text = sub.title.ifBlank { str("ra.subset.base") },
                    modifier = Modifier.padding(horizontal = 14.dp, vertical = 7.dp),
                    fontWeight = if (selected) FontWeight.Bold else FontWeight.Normal,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}

// OsdOverlayPos int values in 3x3 grid order (None=0 skipped): TopLeft=1 .. BottomRight=9.
private val OSD_OVERLAY_POS_VALUES = listOf(1, 2, 3, 4, 5, 6, 7, 8, 9)
// AchievementOverlayPosition int values in 3x3 grid order: TopLeft=0 .. BottomRight=8.
private val ACH_OVERLAY_POS_VALUES = listOf(0, 1, 2, 3, 4, 5, 6, 7, 8)

/** A compact 3x3 position picker matching on-screen placement. `values` are the native enum ints in
 *  row-major grid order (TL,TC,TR, CL,C,CR, BL,BC,BR); the two RA position enums use different int
 *  bases, so each caller passes its own list. A dot sits in the cell's screen position so the choice
 *  reads at a glance. Fully controller-navigable — each cell is controllerFocusable. */
@Composable
private fun PositionGridPicker(
    label: String,
    idPrefix: String,
    current: Int,
    values: List<Int>,
    onSelect: (Int) -> Unit,
) {
    Column(Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Text(label, style = MaterialTheme.typography.titleSmall)
        Spacer(Modifier.height(6.dp))
        Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
            for (row in 0..2) {
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    for (col in 0..2) {
                        val idx = row * 3 + col
                        val v = values[idx]
                        val sel = v == current
                        val dotAlign = when (row) {
                            0 -> when (col) { 0 -> Alignment.TopStart; 1 -> Alignment.TopCenter; else -> Alignment.TopEnd }
                            1 -> when (col) { 0 -> Alignment.CenterStart; 1 -> Alignment.Center; else -> Alignment.CenterEnd }
                            else -> when (col) { 0 -> Alignment.BottomStart; 1 -> Alignment.BottomCenter; else -> Alignment.BottomEnd }
                        }
                        Surface(
                            shape = RoundedCornerShape(8.dp),
                            color = if (sel) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                            border = BorderStroke(1.dp, if (sel) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.outline.copy(alpha = 0.4f)),
                            modifier = Modifier
                                .size(width = 46.dp, height = 32.dp)
                                .clip(RoundedCornerShape(8.dp))
                                .clickable { onSelect(v) }
                                .controllerFocusable("$idPrefix.$idx", onConfirm = { onSelect(v) }),
                        ) {
                            Box(Modifier.fillMaxSize().padding(5.dp), contentAlignment = dotAlign) {
                                Box(
                                    Modifier
                                        .size(7.dp)
                                        .clip(CircleShape)
                                        .background(if (sel) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant),
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

private enum class AchFilter { ALL, UNLOCKED, LOCKED, MISSABLE }

/** All / Unlocked / Locked pills over the achievement list. Same pill styling and controller
 *  focus wiring as SubsetTabs; each pill shows its count so progress is visible at a glance. */
@Composable
private fun AchievementFilterTabs(selected: AchFilter, items: List<AchievementItem>, onSelect: (AchFilter) -> Unit) {
    val unlocked = items.count { it.unlocked }
    val missable = items.count { it.type == 1 }
    val tabs = buildList {
        add(Triple(AchFilter.ALL, str("ra.filter.all"), items.size))
        add(Triple(AchFilter.UNLOCKED, str("ra.filter.unlocked"), unlocked))
        add(Triple(AchFilter.LOCKED, str("ra.filter.locked"), items.size - unlocked))
        // Only surface the Missable filter when the set actually has missables (like RA's site),
        // so games with none don't get a noisy always-empty tab.
        if (missable > 0) add(Triple(AchFilter.MISSABLE, str("ra.filter.missable"), missable))
    }
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        tabs.forEach { (mode, label, count) ->
            val sel = mode == selected
            Surface(
                shape = RoundedCornerShape(999.dp),
                color = if (sel) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                contentColor = if (sel) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier
                    .clip(RoundedCornerShape(999.dp))
                    .clickable { onSelect(mode) }
                    .controllerFocusable("ra.filter.${mode.name}", onConfirm = { onSelect(mode) }),
            ) {
                Text(
                    text = "$label  $count",
                    modifier = Modifier.padding(horizontal = 14.dp, vertical = 7.dp),
                    fontWeight = if (sel) FontWeight.Bold else FontWeight.Normal,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
        }
    }
}

@Composable
private fun LoginPanel(loading: Boolean, onLogin: (String, String) -> Unit, modifier: Modifier) {
    var username by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    Box(modifier, contentAlignment = Alignment.Center) {
        GlassPanel(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 12.dp).widthIn(max = 460.dp)) {
            Column {
                SectionTitle(str("ralogin.title"), str("ra.status.notSignedIn.body"))
                Spacer(Modifier.height(16.dp))
                OutlinedTextField(
                    username,
                    { username = it },
                    label = { Text(str("ralogin.username.label")) },
                    singleLine = true,
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(10.dp))
                OutlinedTextField(
                    value = password,
                    onValueChange = { password = it },
                    label = { Text(str("ralogin.password.label")) },
                    singleLine = true,
                    visualTransformation = PasswordVisualTransformation(),
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(15.dp))
                Button(
                    onClick = { onLogin(username, password) },
                    enabled = !loading,
                    shape = RoundedCornerShape(18.dp),
                    modifier = Modifier.fillMaxWidth().controllerFocusable("ra.login", RoundedCornerShape(18.dp), onConfirm = { onLogin(username, password) }),
                ) {
                    if (loading) {
                        CircularProgressIndicator(Modifier.size(16.dp), strokeWidth = 2.dp)
                        Spacer(Modifier.width(8.dp))
                    }
                    Text(if (loading) str("ralogin.signingIn") else str("ralogin.signIn"))
                }
                // Escape hatch for a stuck custom/offline RA server override ([Achievements] Host,
                // set by loopback-proxy tools via RetroAchievementsHostOverrideReceiver). With the
                // proxy gone, every request dies with "No response" and there was no in-app way
                // out. Clearing an absent override is a harmless no-op.
                Spacer(Modifier.height(6.dp))
                TextButton(
                    onClick = { runCatching { kr.co.iefriends.pcsx2.NativeApp.clearAchievementsHostOverride() } },
                    modifier = Modifier.fillMaxWidth().controllerFocusable(
                        "ra.serverReset",
                        onConfirm = { runCatching { kr.co.iefriends.pcsx2.NativeApp.clearAchievementsHostOverride() } },
                    ),
                ) { Text(str("ra.server.reset")) }
                Text(
                    str("ra.server.reset.desc"),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

@Composable
private fun AchievementRow(item: AchievementItem) {
    Surface(
        Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(17.dp),
        color = if (item.unlocked) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, if (item.unlocked) MaterialTheme.colorScheme.primary.copy(alpha = 0.55f) else MaterialTheme.colorScheme.outline.copy(alpha = 0.45f)),
    ) {
        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
            if (item.iconUrl.isNotBlank()) {
                AsyncImage(item.iconUrl, item.title, Modifier.size(52.dp).clip(RoundedCornerShape(12.dp)), contentScale = ContentScale.Crop)
            } else {
                Surface(Modifier.size(52.dp), shape = RoundedCornerShape(12.dp), color = MaterialTheme.colorScheme.surfaceVariant) {
                    Box(contentAlignment = Alignment.Center) { Text("★") }
                }
            }
            Spacer(Modifier.width(13.dp))
            Column(Modifier.weight(1f)) {
                Text(item.title, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text(item.description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2, overflow = TextOverflow.Ellipsis)
                if (item.progress.isNotBlank()) Text(item.progress, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.primary)
            }
            Column(horizontalAlignment = Alignment.End, verticalArrangement = Arrangement.spacedBy(4.dp)) {
                // Type badge beside the points — restores the "Missable"/Progression/Win pill lost
                // in the 2026-07 UI rebuild (b970da7e). Standard (type 0) shows no badge.
                when (item.type) {
                    1 -> StatusChip(str("ra.typeChip.missable"), Color(0xFFF5A623))
                    2 -> StatusChip(str("ra.typeChip.progression"), MaterialTheme.colorScheme.primary)
                    3 -> StatusChip(str("ra.typeChip.win"), Color(0xFFF5C451))
                }
                StatusChip("${item.points} pts", if (item.unlocked) Success else MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
