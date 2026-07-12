package com.armsx2.ui.achievements

import androidx.compose.foundation.BorderStroke
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
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
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
                        StatusChip("${state.score} pts", Success)
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
            TextButton(
                onClick = viewModel::logout,
                modifier = Modifier.controllerFocusable("ra.logout", onConfirm = { viewModel.logout() }),
            ) { Text(str("ra.account.logout")) }
        }
    }
}

@Composable
private fun AchievementList(state: AchievementsUiState, modifier: Modifier) {
    Column(modifier) {
        SectionTitle(str("scope.game"), state.items.size.toString())
        if (state.items.isEmpty()) {
            EmptyState(str("ra.status.noAchievements.title"), str("ra.status.noAchievements.body"), modifier = Modifier.fillMaxWidth().height(220.dp))
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                state.items.forEach { item -> AchievementRow(item) }
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
            StatusChip("${item.points} pts", if (item.unlocked) Success else MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
