package com.armsx2.navigation

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
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.theme.ThemePreferences

private data class DrawerItem(
    val title: String,
    val glyph: String,
    val destination: AppRoute,
)

@Composable
fun NavigationDrawer(
    visible: Boolean,
    selected: AppRoute,
    onDismiss: () -> Unit,
    onNavigate: (AppRoute) -> Unit,
) {
    BackHandler(enabled = visible, onBack = onDismiss)
    AnimatedVisibility(
        visible = visible,
        enter = fadeIn(tween(160)),
        exit = fadeOut(tween(140)),
    ) {
        Box(Modifier.fillMaxSize()) {
            Box(
                Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.54f))
                    .clickable(onClick = onDismiss),
            )
            AnimatedVisibility(
                visible = visible,
                enter = slideInHorizontally(tween(240)) { -it },
                exit = slideOutHorizontally(tween(190)) { -it },
            ) {
                Surface(
                    modifier = Modifier
                        .fillMaxHeight()
                        .widthIn(min = 286.dp, max = 340.dp)
                        .fillMaxWidth(0.42f),
                    shape = RoundedCornerShape(topEnd = 30.dp, bottomEnd = 30.dp),
                    color = MaterialTheme.colorScheme.surface,
                    border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.55f)),
                    shadowElevation = 18.dp,
                ) {
                    DrawerContent(selected, onNavigate)
                }
            }
        }
    }
}

@Composable
private fun DrawerContent(selected: AppRoute, onNavigate: (AppRoute) -> Unit) {
    val primary = listOf(
        DrawerItem("Library", "▦", AppRoute.Home),
        DrawerItem("Achievements", "★", AppRoute.Achievements),
        DrawerItem("Settings", "⚙", AppRoute.Settings()),
    )
    val managers = listOf(
        DrawerItem("BIOS", "◉", AppRoute.BiosManager),
        DrawerItem("Memory cards", "▤", AppRoute.MemoryCardManager),
        DrawerItem("Save states", "↧", AppRoute.SaveManager),
        DrawerItem("Controllers", "⌁", AppRoute.ControllerManager),
        DrawerItem("Patches & cheats", "✦", AppRoute.PatchManager),
        DrawerItem("Textures", "▧", AppRoute.TextureManager),
    )

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(18.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            ArmsLogo(Modifier.weight(1f))
            StatusChip(if (MainActivityRuntime.nativeReady.value) "READY" else "STARTING")
        }
        Spacer(Modifier.height(22.dp))
        DrawerSection("Main", primary, selected, onNavigate)
        Spacer(Modifier.height(14.dp))
        HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.45f))
        Spacer(Modifier.height(14.dp))
        DrawerSection("Managers", managers, selected, onNavigate)
        Spacer(Modifier.height(14.dp))
        HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.45f))
        Spacer(Modifier.height(14.dp))
        DrawerRow(
            title = if (ThemePreferences.mode.value == com.armsx2.ui.theme.ThemeMode.Dark) "Light theme" else "Dark theme",
            glyph = if (ThemePreferences.mode.value == com.armsx2.ui.theme.ThemeMode.Dark) "☀" else "◐",
            selected = false,
            onClick = ThemePreferences::toggle,
        )
        DrawerRow("About ARMSX2", "ⓘ", selected is AppRoute.About, onClick = { onNavigate(AppRoute.About) })
        Spacer(Modifier.height(8.dp))
        DrawerRow("Exit", "⏻", false, MainActivityRuntime::exitApp, danger = true)
    }
}

@Composable
private fun DrawerSection(
    title: String,
    items: List<DrawerItem>,
    selected: AppRoute,
    onNavigate: (AppRoute) -> Unit,
) {
    Text(
        title.uppercase(),
        style = MaterialTheme.typography.labelSmall,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        letterSpacing = 1.2.sp,
        modifier = Modifier.padding(horizontal = 10.dp, vertical = 4.dp),
    )
    items.forEach { item ->
        DrawerRow(
            title = item.title,
            glyph = item.glyph,
            selected = sameDestination(selected, item.destination),
            onClick = { onNavigate(item.destination) },
        )
    }
}

@Composable
private fun DrawerRow(
    title: String,
    glyph: String,
    selected: Boolean,
    onClick: () -> Unit,
    danger: Boolean = false,
) {
    val contentColor = when {
        danger -> MaterialTheme.colorScheme.error
        selected -> MaterialTheme.colorScheme.onPrimaryContainer
        else -> MaterialTheme.colorScheme.onSurface
    }
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().padding(vertical = 3.dp),
        shape = RoundedCornerShape(14.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else Color.Transparent,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text(glyph, color = contentColor, fontSize = 18.sp, fontWeight = FontWeight.Bold, modifier = Modifier.width(24.dp))
            Text(title, color = contentColor, style = MaterialTheme.typography.labelLarge)
        }
    }
}

private fun sameDestination(current: AppRoute, target: AppRoute): Boolean = when (target) {
    AppRoute.Home -> current is AppRoute.Home
    is AppRoute.Settings -> current is AppRoute.Settings
    AppRoute.BiosManager -> current is AppRoute.BiosManager
    AppRoute.MemoryCardManager -> current is AppRoute.MemoryCardManager
    AppRoute.SaveManager -> current is AppRoute.SaveManager
    AppRoute.ControllerManager -> current is AppRoute.ControllerManager
    AppRoute.PatchManager -> current is AppRoute.PatchManager
    AppRoute.TextureManager -> current is AppRoute.TextureManager
    AppRoute.Achievements -> current is AppRoute.Achievements
    AppRoute.About -> current is AppRoute.About
}
