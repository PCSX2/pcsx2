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
                subtitle = state.richPresence.ifBlank { str("ra.status.notSignedIn.body") },
                leading = { RoundAction("‹", str("action.back"), onBack) },
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
                onCheckedChange = { viewModel.toggleHardcore() },
            )
            TextButton(onClick = viewModel::logout) { Text(str("ra.account.logout")) }
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
                    modifier = Modifier.fillMaxWidth(),
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
