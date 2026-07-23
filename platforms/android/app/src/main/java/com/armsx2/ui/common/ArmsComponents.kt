package com.armsx2.ui.common

import android.content.res.Configuration

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.focusable
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsFocusedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.statusBars
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.PlatformTextStyle
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.R
import com.armsx2.ui.theme.ArmsBlue
import com.armsx2.ui.theme.ArmsCyan

@Composable
fun ArmsBackdrop(
    backgroundLayer: (@Composable BoxScope.() -> Unit)? = null,
    content: @Composable BoxScope.() -> Unit,
) {
    val colors = MaterialTheme.colorScheme
    val isLandscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
    val safeSides = if (isLandscape) {
        WindowInsetsSides.Bottom
    } else {
        WindowInsetsSides.Horizontal + WindowInsetsSides.Bottom
    }
    Surface(
        modifier = Modifier.fillMaxSize(),
        color = colors.background,
        contentColor = colors.onBackground,
    ) {
        Box(
            modifier = Modifier.fillMaxSize(),
        ) {
            // A full-bleed wallpaper layer (library video/image background) that
            // deliberately ignores the safe-area insets, so it reaches every screen
            // edge — including behind the gesture bar. Kept OUT of the inset-padded
            // content Box below; otherwise it stops short of the bottom edge and the
            // exposed strip reads as a bar (most visibly in landscape).
            if (backgroundLayer != null) {
                Box(modifier = Modifier.fillMaxSize(), content = backgroundLayer)
            }
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .windowInsetsPadding(
                        WindowInsets.safeDrawing.only(safeSides),
                    ),
                content = content,
            )
        }
    }
}

@Composable
fun ArmsLogo(modifier: Modifier = Modifier, showWordmark: Boolean = true, iconSize: Dp = 42.dp) {
    Row(modifier = modifier, verticalAlignment = Alignment.CenterVertically) {
        // The ARMSX2 tower mark (bagas's logo), circle-cropped so its dark square
        // corners don't show — matches the round hero render.
        Image(
            painter = painterResource(id = R.drawable.savetowerforeground),
            contentDescription = "ARMSX2",
            modifier = Modifier.size(iconSize).clip(CircleShape),
        )
        if (showWordmark) {
            Spacer(Modifier.width(12.dp))
            Text(
                text = "ARMSX2",
                color = MaterialTheme.colorScheme.onSurface,
                style = MaterialTheme.typography.titleLarge,
                fontWeight = FontWeight.Black,
                letterSpacing = 1.2.sp,
            )
        }
    }
}

@Composable
fun ArmsTopBar(
    title: String,
    subtitle: String? = null,
    leading: (@Composable () -> Unit)? = null,
    actions: @Composable RowScope.() -> Unit = {},
    horizontalPadding: Dp = 8.dp,
    // When true the bar hugs the BOTTOM of the screen (library toolbar setting): it
    // takes the navigation-bar inset instead of the status-bar inset, but keeps the
    // exact same rounded-pill shape so the top and bottom placements look identical.
    bottomEdge: Boolean = false,
) {
    val statusBarPadding = WindowInsets.statusBars.asPaddingValues().calculateTopPadding()
    val navBarPadding = WindowInsets.navigationBars.asPaddingValues().calculateBottomPadding()
    Surface(
        modifier = Modifier
            .fillMaxWidth()
            .padding(
                start = horizontalPadding,
                end = horizontalPadding,
                top = if (bottomEdge) 4.dp else statusBarPadding + 8.dp,
                bottom = if (bottomEdge) navBarPadding + 8.dp else 4.dp,
            ),
        shape = RoundedCornerShape(26.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.38f)),
        tonalElevation = 0.dp,
        shadowElevation = 5.dp,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 10.dp, vertical = 10.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            leading?.invoke()
            if (leading != null) Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(
                    title,
                    color = MaterialTheme.colorScheme.onSurface,
                    style = MaterialTheme.typography.titleLarge,
                    fontWeight = FontWeight.Bold,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                if (!subtitle.isNullOrBlank()) {
                    Text(
                        text = subtitle,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        style = MaterialTheme.typography.bodySmall,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                    )
                }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), content = actions)
        }
    }
}

@Composable
fun GlassPanel(
    modifier: Modifier = Modifier,
    contentPadding: Dp = 16.dp,
    content: @Composable () -> Unit,
) {
    val panelShape = RoundedCornerShape(28.dp)
    val panelColor = MaterialTheme.colorScheme.surface
    Surface(
        modifier = modifier.clip(panelShape),
        shape = panelShape,
        color = panelColor,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.64f)),
        tonalElevation = 2.dp,
        shadowElevation = 7.dp,
    ) {
        Box(
            Modifier
                .fillMaxWidth()
                .background(panelColor)
                .padding(contentPadding),
        ) { content() }
    }
}

@Composable
fun RoundAction(
    glyph: String,
    description: String,
    onClick: () -> Unit,
    selected: Boolean = false,
    framed: Boolean = true,
    buttonSize: Dp = 42.dp,
    buttonShape: Shape = CircleShape,
    subtleFrame: Boolean = false,
    glyphColor: Color? = null,
) {
    val interaction = remember { MutableInteractionSource() }
    val focused by interaction.collectIsFocusedAsState()
    val actionBorder = if (framed || focused || selected) {
        BorderStroke(
            width = if (focused) 2.dp else 1.dp,
            color = when {
                focused -> MaterialTheme.colorScheme.primary
                selected -> MaterialTheme.colorScheme.primary.copy(alpha = 0.76f)
                else -> MaterialTheme.colorScheme.outline.copy(alpha = if (subtleFrame) 0.30f else 0.58f)
            },
        )
    } else {
        null
    }
    val actionColor = when {
        selected -> MaterialTheme.colorScheme.primaryContainer
        framed -> MaterialTheme.colorScheme.surfaceVariant.copy(alpha = if (subtleFrame) 0.58f else 1f)
        else -> Color.Transparent
    }
    Surface(
        onClick = onClick,
        interactionSource = interaction,
        modifier = Modifier
            .size(buttonSize)
            .semantics { contentDescription = description }
            .focusable(interactionSource = interaction),
        shape = buttonShape,
        color = actionColor,
        border = actionBorder,
    ) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            Text(
                // Empirical nudge. Font metrics alone don't land these glyphs on the optical
                // centre: arrows and symbols like ← ↺ ⌕ carry no descender, so their ink sits
                // above the baseline-derived box centre and they read low once the box itself is
                // centred. Removing the font padding (below) fixes the box; this fixes the ink.
                // Tuned against the back arrow, which is the most-looked-at of the set.
                modifier = Modifier.offset(y = (-1.5).dp),
                text = glyph,
                color = glyphColor
                    ?: if (selected) MaterialTheme.colorScheme.onPrimaryContainer else MaterialTheme.colorScheme.onSurface,
                fontSize = when {
                    glyph.length >= 3 -> 13.sp
                    glyph.length == 2 -> 18.sp
                    else -> 22.sp
                },
                fontWeight = FontWeight.Bold,
                // The Box centres the text's LAYOUT BOX, but Android pads that box with the
                // font's full ascent/descent by default, so a glyph with no descender (←, ↑, ⌕)
                // doesn't land where the eye expects. Dropping the font padding is the part of
                // this that is unambiguously right. A first attempt also added
                // LineHeightStyle(Center + Trim.Both) on top, which overshot and pushed the
                // arrow BELOW centre — trimming the leading and re-centring double-corrects.
                style = TextStyle(platformStyle = PlatformTextStyle(includeFontPadding = false)),
            )
        }
    }
}

@Composable
fun SectionTitle(title: String, detail: String? = null, modifier: Modifier = Modifier) {
    Column(modifier) {
        Text(title, style = MaterialTheme.typography.titleLarge)
        if (!detail.isNullOrBlank()) {
            Spacer(Modifier.height(3.dp))
            Text(detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
fun StatusChip(text: String, color: Color = MaterialTheme.colorScheme.primary) {
    Surface(
        shape = RoundedCornerShape(50),
        color = color.copy(alpha = 0.14f),
        border = BorderStroke(1.dp, color.copy(alpha = 0.35f)),
    ) {
        Text(
            text = text,
            color = color,
            style = MaterialTheme.typography.labelSmall,
            modifier = Modifier.padding(horizontal = 9.dp, vertical = 5.dp),
        )
    }
}

/** Tappable search bar — deliberately NOT an editable TextField, so it never summons the Android
 *  system IME. Tapping it (or the controller's A on the Search zone) opens the app's own D-pad +
 *  touch keyboard (LibraryKeyboard), which owns and edits the query. Shows the current query, or
 *  the placeholder when empty; mirrors the old field's look (rounded, leading ⌕, selected border). */
@Composable
fun SearchField(
    value: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    placeholder: String = "",
    selected: Boolean = false,
) {
    Surface(
        onClick = onClick,
        modifier = modifier.height(56.dp),
        shape = RoundedCornerShape(18.dp),
        color = MaterialTheme.colorScheme.surfaceVariant,
        border = if (selected) BorderStroke(2.dp, MaterialTheme.colorScheme.primary) else null,
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text("⌕", fontSize = 21.sp, fontWeight = FontWeight.Bold)
            Spacer(Modifier.width(12.dp))
            Text(
                text = value.ifEmpty { placeholder },
                color = if (value.isEmpty()) MaterialTheme.colorScheme.onSurfaceVariant
                else MaterialTheme.colorScheme.onSurface,
                fontSize = 16.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
            )
        }
    }
}

@Composable
fun EmptyState(
    title: String,
    message: String,
    actionLabel: String? = null,
    onAction: (() -> Unit)? = null,
    modifier: Modifier = Modifier,
) {
    Surface(
        modifier = modifier,
        shape = RoundedCornerShape(24.dp),
        color = MaterialTheme.colorScheme.surface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f)),
        tonalElevation = 2.dp,
    ) {
        Column(
            modifier = Modifier.fillMaxSize().padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
        ) {
            ArmsLogo(showWordmark = false)
            Spacer(Modifier.height(18.dp))
            Text(title, style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.height(6.dp))
            Text(
                message,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.bodyMedium,
            )
            if (actionLabel != null && onAction != null) {
                Spacer(Modifier.height(18.dp))
                OutlinedButton(
                    onClick = onAction,
                    shape = RoundedCornerShape(14.dp),
                    colors = ButtonDefaults.outlinedButtonColors(contentColor = MaterialTheme.colorScheme.primary),
                ) {
                    Text(actionLabel)
                }
            }
        }
    }
}
