package com.armsx2.ui

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.delay

/**
 * A small transient banner in the TOP-LEFT — currently the "Welcome Back!" note shown on wake.
 * Android Toasts can't be reliably positioned on API 30+ (setGravity is ignored), so this is a
 * Compose overlay mounted once in [WindowImpl] so it floats above any screen (in-game or library).
 */
object WelcomeBanner {
    val text = mutableStateOf<String?>(null)
    // Bumped on each show() so the auto-dismiss timer restarts even for an identical message.
    val token = mutableIntStateOf(0)

    fun show(message: String) {
        text.value = message
        token.intValue += 1
    }
}

@Composable
fun WelcomeBannerOverlay(scope: BoxScope) {
    val msg = WelcomeBanner.text.value ?: return
    LaunchedEffect(WelcomeBanner.token.intValue) {
        delay(2600)
        WelcomeBanner.text.value = null
    }
    with(scope) {
        Surface(
            modifier = Modifier
                .align(Alignment.TopStart)
                .windowInsetsPadding(WindowInsets.safeDrawing)
                .padding(start = 14.dp, top = 12.dp),
            shape = RoundedCornerShape(14.dp),
            color = MaterialTheme.colorScheme.surface.copy(alpha = 0.92f),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.5f)),
            shadowElevation = 8.dp,
        ) {
            Text(
                text = msg,
                modifier = Modifier.padding(horizontal = 16.dp, vertical = 10.dp),
                color = MaterialTheme.colorScheme.onSurface,
                fontSize = 16.sp,
                fontWeight = FontWeight.SemiBold,
            )
        }
    }
}
