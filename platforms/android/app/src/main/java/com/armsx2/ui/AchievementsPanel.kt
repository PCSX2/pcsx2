package com.armsx2.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.gestures.animateScrollBy
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.derivedStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import coil.compose.SubcomposeAsyncImage
import coil.request.ImageRequest
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import com.armsx2.ui.settings.controllerFocusable
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONObject

/**
 * Achievements list shown in the right column of the in-game overlay.
 *
 * Snapshots [NativeApp.getAchievementsJSON] when the panel composes and
 * polls every few seconds while open so freshly-unlocked entries surface
 * without the user closing/reopening the overlay. Style mirrors the
 * setup-wizard BIOS bubble — rounded card with an icon column and
 * title/description stack — for visual consistency.
 *
 * Empty / no-game / not-logged-in states render a single status card
 * instead of a hard hide, so the column doesn't visually disappear and
 * confuse users who expect achievements to always show up.
 */
data class AchievementSnapshot(
    val items: List<Achievement>,
    val active: Boolean,
    val loggedIn: Boolean,
    val hardcore: Boolean,
    val userName: String,
    /** Hardcore total points; -1 when unknown (no game loaded, so the
     *  persistent rc_client that holds the score isn't available). */
    val score: Long = -1,
    /** Softcore total points; -1 when unknown. */
    val softcoreScore: Long = -1,
    // Global RA presentation options, mirrored from [Achievements] settings so
    // the panel can show + toggle them. Default on, matching native defaults.
    val notifications: Boolean = true,
    val leaderboardNotifications: Boolean = true,
    val overlays: Boolean = true,
    val lbOverlays: Boolean = true,
    val soundEffects: Boolean = true,
)

data class Achievement(
    val id: Int,
    val title: String,
    val description: String,
    val points: Int,
    val unlocked: Boolean,
    val bucket: Int,
    /** % of players who own this achievement (0..100). Lower = rarer. Drives
     *  the tier label + border tint via [rarityTier]. */
    val rarity: Float,
    val measuredProgress: String,
    /** 0..100 numeric counterpart to [measuredProgress]; drives the progress
     *  bar at the bottom of locked rows with active measurement. */
    val measuredPercent: Float,
    /** rcheevos type: 0=Standard, 1=Missable, 2=Progression, 3=Win. Shown as
     *  a small chip beside the title — Standard renders nothing. */
    val type: Int,
    /** SOFTCORE/HARDCORE bitmask (RC_CLIENT_ACHIEVEMENT_UNLOCKED_*). Bit 1
     *  set ⇒ earned in hardcore; we show an "HC" pill on the badge corner. */
    val unlockedMask: Int,
    /** Unix-seconds when the user unlocked the achievement; 0 if locked.
     *  Rendered as a relative timestamp ("3d ago") under the title. */
    val unlockTime: Long,
    /** RA badge image URL for the achievement's current state — unlocked
     *  variant or greyscale `_lock` variant. Stable per badge_name, so Coil's
     *  disk cache holds it across sessions. Empty when the native side
     *  couldn't build a URL (no badge configured); panel then falls back
     *  to the glyph. */
    val iconUrl: String,
)

/** Tier classification driven by `rarity` (% of players who own it). Lower
 *  rarity = scarcer; we map to a five-tier scheme that aligns with the
 *  Common/Uncommon/Rare/Epic/Legendary vernacular RA leaderboards use. The
 *  colour ramps cool→warm as the tier rises so unlocked entries get a
 *  noticeable "trophy" without overpowering the unlocked-blue tint. */
private enum class RarityTier(val label: String, val color: Color) {
    Legendary("Legendary", Color(0xFFFFD24A)),
    Epic("Epic",          Color(0xFFC58CFF)),
    Rare("Rare",          Color(0xFF6FA8FF)),
    Uncommon("Uncommon",  Color(0xFF7CD68A)),
    Common("Common",      Color(0xFF9A9A9A)),
}

private fun rarityTier(rarity: Float): RarityTier = when {
    rarity <= 0f   -> RarityTier.Common      // rarity not reported yet
    rarity < 2f    -> RarityTier.Legendary
    rarity < 5f    -> RarityTier.Epic
    rarity < 10f   -> RarityTier.Rare
    rarity < 25f   -> RarityTier.Uncommon
    else           -> RarityTier.Common
}

/** Short label + accent colour for the type chip beside the title. Returns
 *  null for Standard (no chip rendered) so callers can guard with a null
 *  check instead of inflating an empty Row. */
private fun typeChip(type: Int): Pair<String, Color>? = when (type) {
    1 -> "MISSABLE"    to Color(0xFFFFAA55) // amber — warns "can be permanently lost"
    2 -> "PROGRESSION" to Color(0xFF7AB8FF) // soft blue — story beats
    3 -> "WIN"         to Color(0xFFFFD24A) // gold — game complete
    else -> null
}

/** Compact relative-time formatter: "just now" / "12m ago" / "3h ago" /
 *  "5d ago" / "Mar 5" / "Mar 5 2024". Mirrors the granularity the RA
 *  website uses on the achievement feed. */
private fun formatRelativeUnlock(unlockTimeSec: Long): String {
    if (unlockTimeSec <= 0) return ""
    val nowSec = System.currentTimeMillis() / 1000
    val delta = nowSec - unlockTimeSec
    if (delta < 0) return I18n.get("ra.time.justNow")
    val minute = 60L
    val hour = 60L * minute
    val day = 24L * hour
    return when {
        delta < minute     -> I18n.get("ra.time.justNow")
        delta < hour       -> "${delta / minute}m ago"
        delta < day        -> "${delta / hour}h ago"
        delta < 7 * day    -> "${delta / day}d ago"
        else -> {
            val cal = java.util.Calendar.getInstance().apply { timeInMillis = unlockTimeSec * 1000 }
            val sameYear = cal.get(java.util.Calendar.YEAR) ==
                java.util.Calendar.getInstance().get(java.util.Calendar.YEAR)
            val pattern = if (sameYear) "MMM d" else "MMM d yyyy"
            java.text.SimpleDateFormat(pattern, java.util.Locale.US)
                .format(java.util.Date(unlockTimeSec * 1000))
        }
    }
}

private const val UNLOCKED_HARDCORE_BIT = 1 shl 1

/** Fades the top and bottom edges of a scrolling container so rows ease
 *  in and out instead of being cut off by a hard rectangular boundary.
 *  Uses an offscreen compositing strategy so the [BlendMode.DstIn] mask
 *  applies to the layer as a whole rather than per-draw. */
private fun Modifier.fadingEdges(fadeHeight: Dp): Modifier = this
    .graphicsLayer { compositingStrategy = CompositingStrategy.Offscreen }
    .drawWithContent {
        drawContent()
        val fadePx = fadeHeight.toPx().coerceAtMost(size.height * 0.5f)
        if (fadePx <= 0f) return@drawWithContent
        val topStop = (fadePx / size.height).coerceIn(0f, 0.5f)
        drawRect(
            brush = Brush.verticalGradient(
                colorStops = arrayOf(
                    0f to Color.Transparent,
                    topStop to Color.Black,
                    1f - topStop to Color.Black,
                    1f to Color.Transparent,
                ),
            ),
            blendMode = BlendMode.DstIn,
        )
    }

private fun parseSnapshot(json: String): AchievementSnapshot {
    return try {
        val root = JSONObject(json)
        val arr = root.optJSONArray("items")
        val rawItems = if (arr == null) emptyList() else List(arr.length()) { i ->
            val o = arr.getJSONObject(i)
            Achievement(
                id = o.optInt("id", 0),
                title = o.optString("title", ""),
                description = o.optString("description", ""),
                points = o.optInt("points", 0),
                unlocked = o.optBoolean("unlocked", false),
                bucket = o.optInt("bucket", -1),
                rarity = o.optDouble("rarity", 0.0).toFloat(),
                measuredProgress = o.optString("measuredProgress", ""),
                measuredPercent = o.optDouble("measuredPercent", 0.0).toFloat(),
                type = o.optInt("type", 0),
                unlockedMask = o.optInt("unlockedMask", 0),
                unlockTime = o.optLong("unlockTime", 0L),
                iconUrl = o.optString("iconUrl", ""),
            )
        }
        // Sort by unlock date, newest first. Locked entries all carry
        // unlockTime=0, so the stable sort keeps the native bucket order
        // (Active Challenge → Almost There → Locked → …) beneath the
        // unlocked group without any extra tie-breaker logic.
        val items = rawItems.sortedByDescending { it.unlockTime }
        AchievementSnapshot(
            items = items,
            active = root.optBoolean("active", false),
            loggedIn = root.optBoolean("loggedIn", false),
            // With NO game running the live rcheevos flag is always off, which made the
            // global (home-screen) Hardcore toggle read + show as off even when the user
            // had it enabled. Fall back to the PERSISTED Achievements/ChallengeMode setting
            // (what engages on the next boot) so the global toggle reflects reality.
            hardcore = if (com.armsx2.Main.eState.value == com.armsx2.EmuState.STOPPED)
                runCatching { NativeApp.isHardcorePersisted() }.getOrDefault(false)
            else root.optBoolean("hardcore", false),
            userName = root.optString("userName", ""),
            score = root.optLong("score", -1),
            softcoreScore = root.optLong("softcoreScore", -1),
            notifications = root.optBoolean("notifications", true),
            leaderboardNotifications = root.optBoolean("leaderboardNotifications", true),
            overlays = root.optBoolean("overlays", true),
            lbOverlays = root.optBoolean("lbOverlays", true),
            soundEffects = root.optBoolean("soundEffects", true),
        )
    } catch (_: Exception) {
        AchievementSnapshot(emptyList(), active = false, loggedIn = false,
            hardcore = false, userName = "")
    }
}

/**
 * Headless poller. Keeps the overlay's shared achievement-derived state
 * (hardcore flag → save/load dimming, renderer pill, rich-presence subtitle)
 * in sync every 4s while the overlay is open, regardless of which tab is shown
 * or whether the achievements panel itself is on screen. Render it once in the
 * overlay root so removing the inline Play-tab panel doesn't stall these.
 */
@Composable
fun AchievementsSync() {
    LaunchedEffect(Unit) {
        while (true) {
            val json = withContext(Dispatchers.IO) {
                runCatching { NativeApp.getAchievementsJSON() }.getOrNull() ?: ""
            }
            val s = parseSnapshot(json)
            InGameOverlay.hardcoreOn.value = s.hardcore
            if (InGameOverlay.rendererMode.value != InGameOverlay.RendererMode.Auto) {
                runCatching {
                    InGameOverlay.rendererMode.value =
                        if (NativeApp.isHardwareRenderer())
                            InGameOverlay.RendererMode.Hardware
                        else InGameOverlay.RendererMode.Software
                }
            }
            InGameOverlay.richPresence.value = if (s.active) {
                runCatching {
                    withContext(Dispatchers.IO) { NativeApp.getRichPresence() }
                }.getOrNull() ?: ""
            } else ""
            delay(4000)
        }
    }
}

@Composable
fun AchievementsPanel(
    modifier: Modifier = Modifier,
    onSignInClick: () -> Unit = {},
    onHardcoreToggle: (() -> Unit)? = null,
) {
    var snapshot by remember {
        mutableStateOf(AchievementSnapshot(emptyList(), false, false, false, ""))
    }

    // Poll on open + every 4s while the composable is alive (overlay
    // open). Achievements::GetAchievementsAsJSON locks rcheevos +
    // re-creates the bucket list each call, so cap the rate. JNI string
    // marshalling on the Main thread can stutter the overlay if the list
    // is large, so dispatch on IO and assign back via setState.
    LaunchedEffect(Unit) {
        while (true) {
            val json = withContext(Dispatchers.IO) {
                runCatching { NativeApp.getAchievementsJSON() }.getOrNull() ?: ""
            }
            val s = parseSnapshot(json)
            snapshot = s
            // Mirror the live hardcore flag to the overlay-level state
            // that drives Save / Load State row dimming. Doing it here so
            // we don't add a second polling loop.
            InGameOverlay.hardcoreOn.value = s.hardcore
            // Same idea for the renderer pill — keep the HW/SW label in
            // sync with the actual GS state (emucore may swap independently,
            // e.g. SoftwareRendererFMVHack during FMVs). Auto is sticky:
            // once the user picks Auto, the pill stays "Auto" regardless of
            // what GS resolved it to underneath.
            if (InGameOverlay.rendererMode.value != InGameOverlay.RendererMode.Auto) {
                runCatching {
                    InGameOverlay.rendererMode.value =
                        if (NativeApp.isHardwareRenderer())
                            InGameOverlay.RendererMode.Hardware
                        else InGameOverlay.RendererMode.Software
                }
            }
            // Rich-presence read — written into the shared overlay state so
            // GameInfoHeader (the disc-ID / star-rating row) can surface it
            // as a one-line subtitle. Skip the JNI marshal when the client
            // isn't active (no game / not logged in).
            InGameOverlay.richPresence.value = if (s.active) {
                runCatching {
                    withContext(Dispatchers.IO) { NativeApp.getRichPresence() }
                }.getOrNull() ?: ""
            } else ""
            delay(4000)
        }
    }

    // Only the scrollable achievement list should consume the full height of
    // its host box; the status cards ("Not signed in" / "No achievements" /
    // "Loading") must wrap so the host's bottom alignment keeps them tucked in
    // the corner instead of floating up into the action grid (looked broken on
    // wide phone screens).
    val showsList = snapshot.loggedIn && snapshot.active && snapshot.items.isNotEmpty()
    Column(modifier.then(if (showsList) Modifier.fillMaxHeight() else Modifier)) {
        // Compact account header (username + points + hardcore + logout), shown
        // whenever signed in. No "Achievements" heading — it overlapped the
        // status line drawn behind the panel and added nothing (the overlay's
        // RetroAchievements chip already labels this view).
        if (snapshot.loggedIn) {
            AchievementsAccountRow(
                snapshot = snapshot,
                onHardcoreToggle = onHardcoreToggle,
                onLoggedOut = {
                    runCatching { NativeApp.logoutAchievements() }
                    // Snapshot refreshes on the next poll (≤ 4s); render
                    // immediate feedback now.
                    snapshot = AchievementSnapshot(
                        emptyList(), active = false, loggedIn = false,
                        hardcore = false, userName = "",
                    )
                },
            )
            // Options live behind a collapsed "Options" row so they never bury
            // the achievement list (the user opens them only when needed).
            AchievementsOptions(
                snapshot = snapshot,
                onOptimistic = { snapshot = it },
            )
        }

        when {
            !snapshot.loggedIn -> StatusCard(
                title = str("ra.status.notSignedIn.title"),
                body = str("ra.status.notSignedIn.body"),
                controllerId = "ach:signin",
                onClick = onSignInClick,
            )
            !snapshot.active -> StatusCard(
                title = str("ra.status.noAchievements.title"),
                body = str("ra.status.noAchievements.body"),
            )
            snapshot.items.isEmpty() -> StatusCard(
                title = str("ra.status.loading.title"),
                body = str("ra.status.loading.body"),
            )
            else -> {
                val unlocked = snapshot.items.count { it.unlocked }
                // Stack the "X / Y unlocked" header on top of the list so
                // rows that scroll up dissolve under it via the fadingEdges
                // mask — the header stays fully opaque (it's drawn AFTER
                // the LazyColumn in the Box), giving a "frosted" header
                // look without us having to paint a backdrop.
                // Controller scrolling: the overlay bumps achievementsScroll by
                // ±1 per D-pad/stick step (no Compose focus — touch mode blocks
                // it), and we translate the delta into a list scroll here.
                val listState = rememberLazyListState()
                val density = LocalDensity.current
                var lastScroll by remember { mutableStateOf(InGameOverlay.achievementsScroll.value) }
                LaunchedEffect(InGameOverlay.achievementsScroll.value) {
                    val cur = InGameOverlay.achievementsScroll.value
                    val diff = cur - lastScroll
                    lastScroll = cur
                    if (diff != 0) runCatching {
                        listState.animateScrollBy(diff * with(density) { 96.dp.toPx() })
                    }
                }
                // Tell the overlay nav when the list is at the very top, so Up only
                // jumps to the Softcore/Hardcore toggle once there's nothing left to
                // scroll (otherwise Up scrolls the list up).
                val atTop by remember {
                    derivedStateOf {
                        listState.firstVisibleItemIndex == 0 &&
                            listState.firstVisibleItemScrollOffset == 0
                    }
                }
                LaunchedEffect(atTop) { InGameOverlay.achievementsAtTop.value = atTop }
                Box(modifier = Modifier.fillMaxWidth()) {
                    LazyColumn(
                        state = listState,
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                        contentPadding = PaddingValues(top = 16.dp),
                        modifier = Modifier
                            .padding(top = 10.dp)
                            .fadingEdges(18.dp),
                    ) {
                        items(snapshot.items, key = { it.id }) { ach ->
                            AchievementRow(ach)
                        }
                    }
                    Text(
                        text = "$unlocked / ${snapshot.items.size} unlocked",
                        color = Color(0xFFCCCCCC),
                        fontSize = 11.sp,
                        modifier = Modifier.align(Alignment.TopStart),
                    )
                }
            }
        }
    }
}

/** One-line account header: username + total points on the left, the
 *  Softcore/Hardcore toggle and Logout on the right. Shown whenever signed in.
 *  Points come from the persistent rc_client (only available once a game with
 *  achievements is loaded); when unknown (score < 0) just the name shows. Both
 *  buttons are controller-focusable so they join the panel's nav stack. */
@Composable
private fun AchievementsAccountRow(
    snapshot: AchievementSnapshot,
    onHardcoreToggle: (() -> Unit)?,
    onLoggedOut: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .padding(top = 6.dp, bottom = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            val signedInLabel = str("ra.account.signedIn")
            Text(
                text = snapshot.userName.ifEmpty { signedInLabel },
                color = Color(0xFFAACCFF),
                fontSize = 14.sp,
                fontWeight = FontWeight.Bold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
            val pts = buildString {
                if (snapshot.score >= 0) append("${formatPoints(snapshot.score)} pts")
                if (snapshot.softcoreScore > 0) {
                    if (isNotEmpty()) append(" · ")
                    append("${formatPoints(snapshot.softcoreScore)} casual")
                }
            }
            if (pts.isNotEmpty()) {
                Text(pts, color = Color(0xFFFFCC66), fontSize = 11.sp)
            }
        }
        // Hardcore toggle (red HARDCORE when on, grey CASUAL when off). Tap/A
        // routes to the host overlay's confirm → reset, so enabling is deliberate.
        if (onHardcoreToggle != null) {
            val active = snapshot.hardcore
            val bg = if (active) Color(0xFFB22222) else Color(0xFF333333)
            val fg = if (active) Color.White else Color(0xFF888888)
            val border = if (active) Color(0xFFFF6B6B) else Color(0xFF555555)
            Row(
                modifier = Modifier
                    .clip(RoundedCornerShape(6.dp))
                    .background(bg)
                    .border(1.dp, border, RoundedCornerShape(6.dp))
                    .controllerFocusable(controllerId = "ach:hardcore", onConfirm = onHardcoreToggle)
                    .clickable(onClick = onHardcoreToggle)
                    .padding(horizontal = 8.dp, vertical = 4.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    text = if (active) str("ra.mode.hardcore") else str("ra.mode.casual"),
                    color = fg,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
            Spacer(Modifier.width(8.dp))
        }
        Text(
            text = str("ra.account.logout"),
            color = Color(0xFFFF8888),
            fontSize = 11.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier
                .clip(RoundedCornerShape(4.dp))
                .controllerFocusable(controllerId = "ach:logout", onConfirm = onLoggedOut)
                .clickable(onClick = onLoggedOut)
                .padding(horizontal = 6.dp, vertical = 2.dp),
        )
    }
}

private fun formatPoints(p: Long): String =
    java.text.NumberFormat.getIntegerInstance(java.util.Locale.US).format(p)

/** Global RetroAchievements presentation toggles, behind a collapsed "Options"
 *  disclosure so they don't bury the achievement list (the default state shows
 *  just the one-line header). Each row is tap- and controller-toggleable and
 *  writes through [NativeApp.setAchievementsOption]; the optimistic snapshot
 *  copy keeps the UI responsive ahead of the 4s poll. */
@Composable
private fun AchievementsOptions(
    snapshot: AchievementSnapshot,
    onOptimistic: (AchievementSnapshot) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(bottom = 6.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp),
    ) {
        // Disclosure header — A / tap toggles the section open. Kept always
        // focusable so controller users can reach the options without a touch.
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .clip(RoundedCornerShape(6.dp))
                .controllerFocusable(controllerId = "ach:options", onConfirm = { expanded = !expanded })
                .clickable { expanded = !expanded }
                .padding(horizontal = 4.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(
                text = str("ra.options.header"),
                color = Color(0xFFCCCCCC),
                fontSize = 12.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.weight(1f),
            )
            Text(
                text = if (expanded) "▾" else "▸",
                color = Color(0xFFCCCCCC),
                fontSize = 12.sp,
            )
        }
        if (!expanded) return@Column

        OptionToggleRow(str("ra.options.unlockNotifications"), snapshot.notifications, "ach:opt:notifications") {
            val nv = !snapshot.notifications
            runCatching { NativeApp.setAchievementsOption("notifications", nv) }
            onOptimistic(snapshot.copy(notifications = nv))
        }
        OptionToggleRow(str("ra.options.leaderboardNotifications"), snapshot.leaderboardNotifications, "ach:opt:lbnotif") {
            val nv = !snapshot.leaderboardNotifications
            runCatching { NativeApp.setAchievementsOption("leaderboardNotifications", nv) }
            onOptimistic(snapshot.copy(leaderboardNotifications = nv))
        }
        OptionToggleRow(str("ra.options.inGameIndicators"), snapshot.overlays, "ach:opt:overlays") {
            val nv = !snapshot.overlays
            runCatching { NativeApp.setAchievementsOption("overlays", nv) }
            onOptimistic(snapshot.copy(overlays = nv))
        }
        OptionToggleRow(str("ra.options.leaderboardTrackers"), snapshot.lbOverlays, "ach:opt:lboverlays") {
            val nv = !snapshot.lbOverlays
            runCatching { NativeApp.setAchievementsOption("lbOverlays", nv) }
            onOptimistic(snapshot.copy(lbOverlays = nv))
        }
        OptionToggleRow(str("ra.options.soundEffects"), snapshot.soundEffects, "ach:opt:sound") {
            val nv = !snapshot.soundEffects
            runCatching { NativeApp.setAchievementsOption("soundEffects", nv) }
            onOptimistic(snapshot.copy(soundEffects = nv))
        }
    }
}

@Composable
private fun OptionToggleRow(
    label: String,
    enabled: Boolean,
    controllerId: String,
    onToggle: () -> Unit,
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(6.dp))
            .background(Color(0xFF1F2123))
            .border(1.dp, Color.White.copy(alpha = 0.10f), RoundedCornerShape(6.dp))
            .controllerFocusable(controllerId = controllerId, onConfirm = onToggle)
            .clickable(onClick = onToggle)
            .padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            text = label,
            color = Color.White,
            fontSize = 12.sp,
            modifier = Modifier.weight(1f),
        )
        val pillBg = if (enabled) Colors.pasx2_blue else Color(0xFF444444)
        val pillFg = if (enabled) Color.White else Color(0xFFAAAAAA)
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(4.dp))
                .background(pillBg)
                .padding(horizontal = 8.dp, vertical = 2.dp),
        ) {
            Text(
                text = if (enabled) "ON" else "OFF",
                color = pillFg,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

@Composable
private fun StatusCard(
    title: String,
    body: String,
    controllerId: String? = null,
    onClick: (() -> Unit)? = null,
) {
    // Matches the Playing-Now bubble surface (0xFF1F2123 + 10%-white
    // hairline border) so the right-column achievements panel reads as
    // the same material as the left-column action grid.
    var base = Modifier
        .fillMaxWidth()
        .clip(RoundedCornerShape(8.dp))
        .background(Color(0xFF1F2123))
        .border(1.dp, Color.White.copy(alpha = 0.10f), RoundedCornerShape(8.dp))
    if (onClick != null && controllerId != null)
        base = base.controllerFocusable(controllerId, onConfirm = onClick)
    val withClick = if (onClick != null) base.clickable(onClick = onClick) else base
    Column(modifier = withClick.padding(12.dp)) {
        Text(title, color = Color.White, fontSize = 14.sp, fontWeight = FontWeight.Bold)
        Spacer(Modifier.height(4.dp))
        Text(body, color = Color(0xFFCCCCCC), fontSize = 12.sp)
    }
}

@Composable
private fun AchievementRow(ach: Achievement) {
    val tier = rarityTier(ach.rarity)
    val tierColor = tier.color
    // Locked rows match the Playing-Now bubble surface (0xFF1F2123) so the
    // achievements column reads as the same material as the action grid.
    // Unlocked rows keep the pasx2_blue trophy tint to signal "earned",
    // and the locked border keeps its rarity tint so common→legendary
    // still reads as a colour gradient.
    val bg = if (ach.unlocked) Colors.pasx2_blue.copy(alpha = 0.30f) else Color(0xFF1F2123)
    val border = when {
        ach.unlocked -> Colors.pasx2_blue
        else -> tierColor.copy(alpha = 0.45f)
    }
    val hardcoreUnlock = ach.unlocked && (ach.unlockedMask and UNLOCKED_HARDCORE_BIT) != 0

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .background(bg)
            .border(2.dp, border, RoundedCornerShape(8.dp))
            .padding(10.dp),
    ) {
        Row(verticalAlignment = Alignment.Top) {
            // ── Badge column ─────────────────────────────────────────────
            // Coil fetches the RA badge URL (unlocked vs greyscale `_lock`
            // variant — chosen native-side per current state) and caches it
            // on disk for life. Glyph fallback: ✓ unlocked / 🔒 locked /
            // ⏳ measured in-progress so the column never collapses.
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                val glyph = when {
                    ach.unlocked -> "✓"
                    ach.measuredProgress.isNotEmpty() -> "⏳"
                    else -> "🔒"
                }
                val glyphColor = if (ach.unlocked) Colors.pasx2_blue else Color(0xFFAAAAAA)
                val context = LocalContext.current
                val badgeSize = 44.dp
                Box(
                    modifier = Modifier.size(badgeSize),
                    contentAlignment = Alignment.Center,
                ) {
                    if (ach.iconUrl.isNotEmpty()) {
                        SubcomposeAsyncImage(
                            model = ImageRequest.Builder(context)
                                .data(ach.iconUrl)
                                .crossfade(true)
                                .build(),
                            contentDescription = ach.title,
                            contentScale = ContentScale.Fit,
                            modifier = Modifier
                                .size(badgeSize)
                                .clip(RoundedCornerShape(4.dp)),
                            loading = { Text(glyph, fontSize = 18.sp, color = glyphColor) },
                            error = { Text(glyph, fontSize = 18.sp, color = glyphColor) },
                        )
                    } else {
                        Text(glyph, fontSize = 18.sp, color = glyphColor)
                    }
                    // Hardcore corner pill — only when the user earned the
                    // achievement in hardcore. Offset half a step off-badge
                    // so it reads as an applied seal rather than overlap.
                    if (hardcoreUnlock) {
                        Box(
                            modifier = Modifier
                                .align(Alignment.TopEnd)
                                .offset(x = 4.dp, y = (-4).dp)
                                .clip(RoundedCornerShape(4.dp))
                                .background(Color(0xFFB22222))
                                .border(1.dp, Color(0xFFFF8080), RoundedCornerShape(4.dp))
                                .padding(horizontal = 3.dp, vertical = 1.dp),
                        ) {
                            Text(
                                "HC",
                                color = Color.White,
                                fontSize = 8.sp,
                                fontWeight = FontWeight.Bold,
                            )
                        }
                    }
                }
                if (ach.points > 0) {
                    Spacer(Modifier.height(2.dp))
                    Text(
                        "${ach.points}",
                        color = Color(0xFFFFCC66),
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                    )
                }
            }
            Spacer(Modifier.width(10.dp))

            // ── Title + meta column ─────────────────────────────────────
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(
                        ach.title,
                        color = Color.White,
                        fontSize = 13.sp,
                        fontWeight = FontWeight.Bold,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f, fill = false),
                    )
                    typeChip(ach.type)?.let { (label, color) ->
                        val chipText = when (ach.type) {
                            1 -> str("ra.typeChip.missable")
                            2 -> str("ra.typeChip.progression")
                            3 -> str("ra.typeChip.win")
                            else -> label
                        }
                        Spacer(Modifier.width(6.dp))
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(3.dp))
                                .background(color.copy(alpha = 0.20f))
                                .border(1.dp, color.copy(alpha = 0.55f), RoundedCornerShape(3.dp))
                                .padding(horizontal = 4.dp, vertical = 1.dp),
                        ) {
                            Text(
                                chipText,
                                color = color,
                                fontSize = 8.sp,
                                fontWeight = FontWeight.Bold,
                            )
                        }
                    }
                }
                if (ach.description.isNotEmpty()) {
                    Text(
                        ach.description,
                        color = Color(0xFFCCCCCC),
                        fontSize = 11.sp,
                        maxLines = 2,
                        overflow = TextOverflow.Ellipsis,
                    )
                }

                // Meta strip: tier label + rarity percentage on the left,
                // unlock time or measured progress on the right. Tier name
                // is shown only when rarity is reported (rarity > 0); the
                // colour matches the card border so the cue is consistent.
                Spacer(Modifier.height(3.dp))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    if (ach.rarity > 0f) {
                        val tierLabel = when (tier) {
                            RarityTier.Legendary -> str("ra.tier.legendary")
                            RarityTier.Epic -> str("ra.tier.epic")
                            RarityTier.Rare -> str("ra.tier.rare")
                            RarityTier.Uncommon -> str("ra.tier.uncommon")
                            RarityTier.Common -> str("ra.tier.common")
                        }
                        Text(
                            tierLabel.uppercase() + " · " +
                                String.format(java.util.Locale.US, "%.1f%%", ach.rarity),
                            color = tierColor,
                            fontSize = 9.sp,
                            fontWeight = FontWeight.Bold,
                        )
                    }
                    Spacer(Modifier.weight(1f))
                    val trailing = when {
                        ach.unlocked && ach.unlockTime > 0L ->
                            "Unlocked " + formatRelativeUnlock(ach.unlockTime)
                        !ach.unlocked && ach.measuredProgress.isNotEmpty() ->
                            ach.measuredProgress
                        else -> ""
                    }
                    if (trailing.isNotEmpty()) {
                        Text(
                            trailing,
                            color = if (ach.unlocked) Color(0xFFAACCFF) else Color(0xFFCCCCCC),
                            fontSize = 10.sp,
                        )
                    }
                }
            }
        }

        // ── Progress bar ─────────────────────────────────────────────────
        // Only shown when the achievement is measured and not yet earned.
        // Tracks the rarity tier colour so the eye associates "purple bar
        // close to full" with "epic-tier almost there".
        if (!ach.unlocked && ach.measuredPercent > 0f) {
            Spacer(Modifier.height(6.dp))
            val fraction = (ach.measuredPercent / 100f).coerceIn(0f, 1f)
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(3.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(Color.White.copy(alpha = 0.12f)),
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(fraction)
                        .fillMaxHeight()
                        .clip(RoundedCornerShape(2.dp))
                        .background(tierColor),
                )
            }
        }
    }
}
