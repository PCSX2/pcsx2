package com.armsx2.ui.achievements

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
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
        Column(Modifier.fillMaxSize()) {
            ArmsTopBar(
                title = "RetroAchievements",
                subtitle = state.richPresence.ifBlank { "Achievement tracking" },
                leading = { RoundAction("‹", "Back", onBack) },
                actions = {
                    if (state.loggedIn) {
                        StatusChip("${state.score} pts", Success)
                        RoundAction("↻", "Refresh", viewModel::refresh)
                    }
                },
            )
            if (!state.loggedIn) {
                LoginPanel(state.loading, viewModel::login, Modifier.fillMaxSize())
            } else {
                Row(
                    Modifier.fillMaxSize().padding(start = 22.dp, end = 22.dp, bottom = 18.dp),
                    horizontalArrangement = Arrangement.spacedBy(18.dp),
                ) {
                    GlassPanel(Modifier.width(300.dp).fillMaxHeight()) {
                        Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                            SectionTitle(state.userName.ifBlank { "Signed in" }, "RetroAchievements account")
                            StatusChip("${state.items.count { it.unlocked }} / ${state.items.size} unlocked")
                            SettingSwitchRow(
                                title = "Hardcore mode",
                                description = "Disables save states and cheats",
                                checked = state.hardcore,
                                onCheckedChange = { viewModel.toggleHardcore() },
                            )
                            Spacer(Modifier.weight(1f))
                            TextButton(onClick = viewModel::logout) { Text("Sign out") }
                        }
                    }
                    Column(Modifier.weight(1f)) {
                        SectionTitle("Current game", "${state.items.size} achievements")
                        if (state.items.isEmpty()) {
                            EmptyState("No achievements", "Launch a supported game to view its achievement set.", modifier = Modifier.fillMaxSize())
                        } else {
                            LazyColumn(
                                Modifier.fillMaxSize(),
                                verticalArrangement = Arrangement.spacedBy(8.dp),
                                contentPadding = PaddingValues(bottom = 20.dp),
                            ) {
                                items(state.items, key = { it.id }) { item -> AchievementRow(item) }
                            }
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
            confirmButton = { TextButton(onClick = viewModel::dismissError) { Text("OK") } },
        )
    }
}

@Composable
private fun LoginPanel(loading: Boolean, onLogin: (String, String) -> Unit, modifier: Modifier) {
    var username by remember { mutableStateOf("") }
    var password by remember { mutableStateOf("") }
    Box(modifier, contentAlignment = Alignment.Center) {
        GlassPanel(Modifier.width(430.dp)) {
            Column {
                SectionTitle("Sign in", "Use your RetroAchievements account")
                Spacer(Modifier.height(16.dp))
                OutlinedTextField(username, { username = it }, label = { Text("Username") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                Spacer(Modifier.height(10.dp))
                OutlinedTextField(
                    value = password,
                    onValueChange = { password = it },
                    label = { Text("Password") },
                    singleLine = true,
                    visualTransformation = PasswordVisualTransformation(),
                    modifier = Modifier.fillMaxWidth(),
                )
                Spacer(Modifier.height(15.dp))
                Button(onClick = { onLogin(username, password) }, enabled = !loading, modifier = Modifier.fillMaxWidth()) {
                    if (loading) {
                        CircularProgressIndicator(Modifier.size(16.dp), strokeWidth = 2.dp)
                        Spacer(Modifier.width(8.dp))
                    }
                    Text(if (loading) "Signing in" else "Sign in")
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
