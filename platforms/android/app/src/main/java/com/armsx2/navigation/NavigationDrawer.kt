package com.armsx2.navigation

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.EaseIn
import androidx.compose.animation.core.EaseOut
import androidx.compose.animation.core.MutableTransitionState
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
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Icon
import androidx.compose.ui.res.painterResource
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
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalConfiguration
import android.content.res.Configuration
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.settings.controllerFocusable

// Warm gold for the RetroAchievements trophy, matching the old (refresh) UI's
// gold trophy rather than the flat monochrome nav glyphs.
private val TrophyGold = Color(0xFFFFC93C)

private data class DrawerItem(
    val titleKey: String,
    val glyph: String,
    // A nav destination OR an action (onAction). Action rows (e.g. Boot BIOS) run onAction and
    // close the drawer instead of navigating to a screen.
    val destination: AppRoute? = null,
    val iconRes: Int? = null,
    val onAction: (() -> Unit)? = null,
)

@Composable
fun NavigationDrawer(
    visible: Boolean,
    selected: AppRoute,
    onDismiss: () -> Unit,
    onNavigate: (AppRoute) -> Unit,
) {
    BackHandler(enabled = visible, onBack = onDismiss)
    val scrimState = remember { MutableTransitionState(false) }
    val panelState = remember { MutableTransitionState(false) }
    LaunchedEffect(visible) {
        scrimState.targetState = visible
        panelState.targetState = visible
    }
    Box(Modifier.fillMaxSize()) {
        AnimatedVisibility(
            visibleState = scrimState,
            enter = fadeIn(tween(210, easing = EaseOut)),
            exit = fadeOut(tween(250, easing = EaseIn)),
        ) {
            Box(
                Modifier
                    .fillMaxSize()
                    .background(Color.Black.copy(alpha = 0.54f))
                    .clickable(onClick = onDismiss),
            )
        }
        AnimatedVisibility(
            visibleState = panelState,
            enter = slideInHorizontally(tween(320, easing = EaseOut)) { -it },
            exit = slideOutHorizontally(tween(220, easing = EaseIn)) { -it },
            modifier = Modifier.align(Alignment.CenterStart),
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
                DrawerContent(selected, onNavigate, onDismiss)
            }
        }
    }
}

@Composable
private fun DrawerContent(selected: AppRoute, onNavigate: (AppRoute) -> Unit, onDismiss: () -> Unit) {
    val isLandscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
    val primary = listOf(
        DrawerItem("games.section.library", "▦", AppRoute.Home),
        // Boot straight into the PS2 system BIOS with no disc — distinct from "BIOS Location"
        // below, which only points the emulator at your BIOS file.
        DrawerItem("bios.boot.title", "▶", onAction = { MainActivityRuntime.startBios(); onDismiss() }),
        DrawerItem("ra.title", "★", AppRoute.Achievements, iconRes = com.armsx2.R.drawable.ic_trophy),
        DrawerItem("action.settings", "⚙", AppRoute.Settings()),
    )
    val managers = listOf(
        DrawerItem("setup.step.bios.title", "◉", AppRoute.BiosManager),
        DrawerItem("memcard.title", "▤", AppRoute.MemoryCardManager),
        DrawerItem("savestate.title.loadManage", "↧", AppRoute.SaveManager),
        DrawerItem("tab.controls", "⌁", AppRoute.ControllerManager),
        DrawerItem("patches.dialog.patchesAndCheats", "✦", AppRoute.PatchManager),
        DrawerItem("renderer.section.texturePacks", "▧", AppRoute.TextureManager),
    )

    Column(
        Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .windowInsetsPadding(
                WindowInsets.safeDrawing.only(
                    if (isLandscape) WindowInsetsSides.Bottom else WindowInsetsSides.Vertical,
                ),
            )
            .padding(horizontal = 8.dp, vertical = 16.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            ArmsLogo(Modifier.weight(1f))
            StatusChip(if (MainActivityRuntime.nativeReady.value) str("backend.driver.active") else str("memcard.status.coreStarting"))
        }
        Spacer(Modifier.height(20.dp))
        DrawerSection(str("games.section.library"), primary, selected, onNavigate, focusFirst = true)
        Spacer(Modifier.height(14.dp))
        HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.45f))
        Spacer(Modifier.height(14.dp))
        DrawerSection(str("ra.options.header"), managers, selected, onNavigate)
    }
}

@Composable
private fun DrawerSection(
    title: String,
    items: List<DrawerItem>,
    selected: AppRoute,
    onNavigate: (AppRoute) -> Unit,
    focusFirst: Boolean = false,
) {
    Text(
        title.uppercase(),
        style = MaterialTheme.typography.labelMedium,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        letterSpacing = 1.2.sp,
        modifier = Modifier.padding(horizontal = 8.dp, vertical = 6.dp),
    )
    items.forEachIndexed { index, item ->
        DrawerRow(
            controllerId = "drawer.${item.destination?.let { it::class.simpleName } ?: item.titleKey}",
            title = str(item.titleKey),
            glyph = item.glyph,
            iconRes = item.iconRes,
            selected = item.destination != null && sameDestination(selected, item.destination),
            onClick = { item.onAction?.invoke() ?: item.destination?.let(onNavigate) },
        )
    }
}

@Composable
private fun DrawerRow(
    controllerId: String,
    title: String,
    glyph: String,
    iconRes: Int? = null,
    selected: Boolean,
    onClick: () -> Unit,
) {
    val contentColor = when {
        selected -> MaterialTheme.colorScheme.onPrimaryContainer
        else -> MaterialTheme.colorScheme.onSurface
    }
    Surface(
        onClick = onClick,
        modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)
            .controllerFocusable(controllerId, RoundedCornerShape(18.dp), onConfirm = onClick),
        shape = RoundedCornerShape(18.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer else Color.Transparent,
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 12.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            if (iconRes != null) {
                Box(Modifier.width(32.dp), contentAlignment = Alignment.Center) {
                    Icon(painterResource(iconRes), contentDescription = null, tint = TrophyGold, modifier = Modifier.size(24.dp))
                }
            } else {
                Text(glyph, color = contentColor, fontSize = 22.sp, fontWeight = FontWeight.Bold, modifier = Modifier.width(32.dp))
            }
            Text(title, color = contentColor, style = MaterialTheme.typography.titleMedium)
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
    AppRoute.Language -> current is AppRoute.Language
    AppRoute.About -> current is AppRoute.About
}
