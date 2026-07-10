package com.armsx2

import android.content.Intent
import android.content.res.Configuration
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.BackHandler
import androidx.activity.compose.setContent
import androidx.compose.animation.core.Animatable
import androidx.compose.animation.core.FastOutSlowInEasing
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.displayCutoutPadding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.theme.ArmsBlue
import com.armsx2.ui.theme.ArmsCyan
import com.armsx2.ui.theme.DayBackground
import com.armsx2.ui.theme.DaySurface
import com.armsx2.ui.theme.DayText
import com.armsx2.ui.theme.NightBackground
import com.armsx2.ui.theme.NightSurface
import com.armsx2.ui.theme.NightText
import androidx.compose.ui.text.font.FontWeight
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch

class BootSplashActivity : ComponentActivity() {
    private var launchedMain = false

    override fun onCreate(savedInstanceState: Bundle?) {
        val savedTheme = getSharedPreferences("ARMSX2", MODE_PRIVATE).getString("ui.theme.mode", "System")
        val systemDark = resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK == Configuration.UI_MODE_NIGHT_YES
        val darkSplash = when (savedTheme) {
            "Light" -> false
            "Dark" -> true
            else -> systemDark
        }
        setTheme(if (darkSplash) R.style.Theme_ARMSX2_Boot_Dark else R.style.Theme_ARMSX2_Boot_Light)
        super.onCreate(savedInstanceState)
        applyImmersiveUi()
        if (playedThisProcess) {
            launchMainAndFinish()
            return
        }
        playedThisProcess = true
        setContent {
            MaterialTheme(
                colorScheme = if (darkSplash) darkColorScheme(
                    primary = ArmsBlue, secondary = ArmsCyan,
                    background = NightBackground, onBackground = NightText,
                    surface = NightSurface, onSurface = NightText,
                ) else lightColorScheme(
                    primary = Color(0xFF245DAD), secondary = Color(0xFF087C91),
                    background = DayBackground, onBackground = DayText,
                    surface = DaySurface, onSurface = DayText,
                    primaryContainer = Color(0xFFD8E6FF), onPrimaryContainer = Color(0xFF0A2B58),
                ),
            ) {
                LaunchAnimation(dark = darkSplash, onFinished = ::launchMainAndFinish)
            }
        }
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) applyImmersiveUi()
    }

    private fun applyImmersiveUi() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun launchMainAndFinish() {
        if (launchedMain) return
        launchedMain = true
        val launch = Intent(this, Main::class.java)
        intent?.let { source ->
            launch.action = source.action
            if (source.data != null || source.type != null) launch.setDataAndType(source.data, source.type)
            source.categories?.forEach(launch::addCategory)
            source.extras?.let(launch::putExtras)
            source.clipData?.let(launch::setClipData)
            launch.addFlags(
                source.flags and (
                    Intent.FLAG_GRANT_READ_URI_PERMISSION or
                        Intent.FLAG_GRANT_WRITE_URI_PERMISSION or
                        Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION or
                        Intent.FLAG_GRANT_PREFIX_URI_PERMISSION
                    ),
            )
        }
        launch.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP)
        startActivity(launch)
        finish()
        overrideActivityTransition(OVERRIDE_TRANSITION_CLOSE, 0, 0)
    }

    private companion object {
        var playedThisProcess = false
    }
}

@Composable
private fun LaunchAnimation(dark: Boolean, onFinished: () -> Unit) {
    val alpha = remember { Animatable(0f) }
    val scale = remember { Animatable(0.72f) }
    val translation = remember { Animatable(-240f) }
    val rotation = remember { Animatable(-24f) }
    val wordmarkAlpha = remember { Animatable(0f) }
    val progress = remember { Animatable(0f) }

    LaunchedEffect(Unit) {
        launch { alpha.animateTo(1f, tween(240)) }
        launch { translation.animateTo(0f, tween(620, easing = FastOutSlowInEasing)) }
        launch { rotation.animateTo(0f, tween(620, easing = FastOutSlowInEasing)) }
        launch { scale.animateTo(1f, tween(620, easing = FastOutSlowInEasing)) }
        launch { wordmarkAlpha.animateTo(1f, tween(360, delayMillis = 300)) }
        progress.animateTo(1f, tween(900, delayMillis = 260, easing = FastOutSlowInEasing))
        delay(160)
        onFinished()
    }
    BackHandler(onBack = onFinished)

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(if (dark) NightBackground else DayBackground)
            .clickable(onClick = onFinished)
            .displayCutoutPadding(),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.Center,
        ) {
            Surface(
                modifier = Modifier
                    .size(96.dp)
                    .graphicsLayer {
                        this.alpha = alpha.value
                        translationX = translation.value
                        rotationZ = rotation.value
                        scaleX = scale.value
                        scaleY = scale.value
                    },
                shape = RoundedCornerShape(30.dp),
                color = MaterialTheme.colorScheme.primaryContainer,
                shadowElevation = 12.dp,
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Box(Modifier.graphicsLayer { scaleX = 1.7f; scaleY = 1.7f }) {
                        ArmsLogo(showWordmark = false)
                    }
                }
            }
            Spacer(Modifier.height(18.dp))
            Text(
                "ARMSX2",
                color = MaterialTheme.colorScheme.onBackground,
                fontSize = 25.sp,
                fontWeight = FontWeight.Black,
                letterSpacing = 2.sp,
                modifier = Modifier.graphicsLayer { this.alpha = wordmarkAlpha.value },
            )
            Spacer(Modifier.height(18.dp))
            Box(
                Modifier
                    .width(128.dp)
                    .height(3.dp)
                    .clip(RoundedCornerShape(50))
                    .background(MaterialTheme.colorScheme.onBackground.copy(alpha = 0.12f)),
            ) {
                Box(
                    Modifier
                        .fillMaxWidth(progress.value)
                        .height(3.dp)
                        .background(Brush.horizontalGradient(listOf(ArmsBlue, ArmsCyan))),
                )
            }
        }
    }
}
