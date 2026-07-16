package com.armsx2.navigation

import android.content.ActivityNotFoundException
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.widget.Toast
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
import androidx.compose.ui.platform.LocalContext
import android.content.res.Configuration
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.StatusChip
import com.armsx2.ui.settings.controllerFocusable

// Warm gold for the RetroAchievements trophy, matching the old (refresh) UI's
// gold trophy rather than the flat monochrome nav glyphs.
private val TrophyGold = Color(0xFFFFC93C)

// Community/project links for the drawer's About section. Plain https on purpose: Android App
// Links hand these to the Discord/GitHub apps when they're installed and fall back to the
// browser when they aren't, so there's no app-specific scheme to special-case.
private const val DiscordUrl = "https://discord.gg/2Tynvwhc4A"
private const val GithubUrl = "https://github.com/ARMSX2/ARMSX2"
private const val WebsiteUrl = "https://armsx2.net/"

/**
 * Opens an external link, telling the user when nothing on the device can handle it.
 *
 * Deliberately NOT Compose's LocalUriHandler.openUri(): AndroidUriHandler catches
 * ActivityNotFoundException internally and rethrows it as IllegalArgumentException, so a
 * try/catch on the documented type catches nothing and a browser-less device takes an
 * uncaught crash instead of a Toast (the existing AboutScreen links have that hole).
 *
 * Equally deliberate: no resolveActivity()/queryIntentActivities() pre-check. Those ARE subject
 * to Android 11+ package-visibility filtering (we declare no <queries>), so a "can anything
 * handle this?" guard can read null for a link that would in fact open — turning a working row
 * into a silent no-op. startActivity() is NOT filtered, so launch it and catch the real miss.
 */
private fun openExternalUrl(context: Context, url: String) {
    try {
        context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(url)))
    } catch (_: ActivityNotFoundException) {
        // Built outside composition, so I18n.get rather than str().
        Toast.makeText(context, I18n.get("about.openFailed"), Toast.LENGTH_LONG).show()
    }
}

private data class DrawerItem(
    val titleKey: String,
    val glyph: String,
    // A nav destination OR an action (onAction). Action rows (e.g. Boot BIOS) run onAction and
    // close the drawer instead of navigating to a screen.
    val destination: AppRoute? = null,
    val iconRes: Int? = null,
    // Null = tint the icon like the row's text. Only the trophy pins a fixed colour.
    val iconTint: Color? = null,
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
    val context = LocalContext.current
    // Intuitive glyphs that read as what they do — matching the in-game overlay's emoji-icon style
    // (the old box-drawing characters like ▦ ◉ ⌁ ✦ were unclear per tester feedback).
    val primary = listOf(
        DrawerItem("games.section.library", "🎮", AppRoute.Home),
        // Boot straight into the PS2 system BIOS with no disc — distinct from "BIOS Location"
        // below, which only points the emulator at your BIOS file.
        DrawerItem("bios.boot.title", "▶️", onAction = { MainActivityRuntime.startBios(); onDismiss() }),
        DrawerItem("ra.title", "🏆", AppRoute.Achievements, iconRes = com.armsx2.R.drawable.ic_trophy,
            iconTint = TrophyGold),
        DrawerItem("action.settings", "⚙️", AppRoute.Settings()),
    )
    val managers = listOf(
        DrawerItem("setup.step.bios.title", "📀", AppRoute.BiosManager()),
        DrawerItem("memcard.title", "💾", AppRoute.MemoryCardManager),
        DrawerItem("savestate.title.loadManage", "📥", AppRoute.SaveManager),
        DrawerItem("tab.controls", "🕹️", AppRoute.ControllerManager),
        DrawerItem("patches.dialog.patchesAndCheats", "🪄", AppRoute.PatchManager),
        DrawerItem("renderer.section.texturePacks", "🖌️", AppRoute.TextureManager),
    )
    // Link-out rows: they reuse the existing onAction path (like Boot BIOS) rather than a
    // destination, so they leave the drawer via startActivity and close it behind them.
    val about = listOf(
        DrawerItem("about.discord", "💬", iconRes = com.armsx2.R.drawable.ic_discord,
            onAction = { openExternalUrl(context, DiscordUrl); onDismiss() }),
        DrawerItem("about.github", "🐙", iconRes = com.armsx2.R.drawable.ic_github,
            onAction = { openExternalUrl(context, GithubUrl); onDismiss() }),
        DrawerItem("about.website", "🌐", onAction = { openExternalUrl(context, WebsiteUrl); onDismiss() }),
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
        Spacer(Modifier.height(14.dp))
        HorizontalDivider(color = MaterialTheme.colorScheme.outline.copy(alpha = 0.45f))
        Spacer(Modifier.height(14.dp))
        DrawerSection(str("about.section.header"), about, selected, onNavigate)
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
            iconTint = item.iconTint,
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
    // Null tints the icon like the row's text. Only the trophy wants a fixed brand colour;
    // the About rows' marks must follow the row so they don't render gold.
    iconTint: Color? = null,
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
                    Icon(painterResource(iconRes), contentDescription = null, tint = iconTint ?: contentColor, modifier = Modifier.size(24.dp))
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
    is AppRoute.BiosManager -> current is AppRoute.BiosManager
    AppRoute.MemoryCardManager -> current is AppRoute.MemoryCardManager
    AppRoute.SaveManager -> current is AppRoute.SaveManager
    AppRoute.ControllerManager -> current is AppRoute.ControllerManager
    AppRoute.PatchManager -> current is AppRoute.PatchManager
    AppRoute.TextureManager -> current is AppRoute.TextureManager
    AppRoute.Achievements -> current is AppRoute.Achievements
    AppRoute.Language -> current is AppRoute.Language
    AppRoute.About -> current is AppRoute.About
}
