package com.armsx2.navigation

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.ExitTransition
import androidx.compose.animation.SizeTransform
import androidx.compose.animation.core.EaseIn
import androidx.compose.animation.core.EaseOut
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.scaleIn
import androidx.compose.animation.scaleOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.Modifier
import com.armsx2.ui.achievements.AchievementsScreen
import com.armsx2.ui.bios.BiosManagerScreen
import com.armsx2.ui.controls.ControllerManagerScreen
import com.armsx2.ui.about.AboutScreen
import com.armsx2.ui.home.HomeScreen
import com.armsx2.ui.memorycards.MemoryCardScreen
import com.armsx2.ui.language.LanguageScreen
import com.armsx2.ui.patches.PatchManagerScreen
import com.armsx2.ui.saves.SaveManagerScreen
import com.armsx2.ui.textures.TextureManagerScreen
import com.armsx2.ui.settingshub.SettingsScreen

@Composable
fun AppNavigation() {
    val route = UiNavigator.route.value
    val drawerOpen = UiNavigator.drawerOpen.value

    BackHandler(enabled = true) { UiNavigator.back() }

    Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
        AnimatedContent(
            targetState = route,
            modifier = Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background),
            transitionSpec = {
                val enter = fadeIn(
                    animationSpec = tween(
                        durationMillis = 260,
                        delayMillis = 70,
                        easing = EaseOut,
                    ),
                ) + scaleIn(
                    initialScale = 0.96f,
                    animationSpec = tween(
                        durationMillis = 260,
                        delayMillis = 70,
                        easing = EaseOut,
                    ),
                )
                val isReturning = targetState is AppRoute.Home ||
                    ((initialState is AppRoute.Language || initialState is AppRoute.About) &&
                        targetState is AppRoute.Settings)
                val exit = if (isReturning) {
                    fadeOut(
                        animationSpec = tween(
                            durationMillis = 110,
                            easing = EaseIn,
                        ),
                    ) + scaleOut(
                        targetScale = 1.0f,
                        animationSpec = tween(
                            durationMillis = 110,
                            easing = EaseIn,
                        ),
                    )
                } else {
                    ExitTransition.None
                }
                (enter togetherWith exit).using(SizeTransform(clip = false))
            },
            label = "app-route",
        ) { destination ->
            when (destination) {
                AppRoute.Home -> HomeScreen(
                    onOpenMenu = { UiNavigator.drawerOpen.value = true },
                    onOpenGameSettings = { UiNavigator.navigate(AppRoute.Settings(game = it)) },
                )
                is AppRoute.Settings -> SettingsScreen(
                    initialCategory = destination.category,
                    game = destination.game,
                    onBack = UiNavigator::home,
                    onOpenAbout = { UiNavigator.navigate(AppRoute.About) },
                )
                is AppRoute.BiosManager -> BiosManagerScreen(onBack = UiNavigator::home, game = destination.game)
                AppRoute.MemoryCardManager -> MemoryCardScreen(onBack = UiNavigator::home)
                AppRoute.SaveManager -> SaveManagerScreen(onBack = UiNavigator::home)
                AppRoute.ControllerManager -> ControllerManagerScreen(onBack = UiNavigator::home)
                AppRoute.PatchManager -> PatchManagerScreen(onBack = UiNavigator::home)
                AppRoute.TextureManager -> TextureManagerScreen(onBack = UiNavigator::home)
                AppRoute.Achievements -> AchievementsScreen(onBack = UiNavigator::home)
                AppRoute.Language -> LanguageScreen(
                    onBack = { UiNavigator.navigate(AppRoute.Settings(SettingsCategory.General)) },
                )
                AppRoute.About -> AboutScreen(
                    onBack = { UiNavigator.navigate(AppRoute.Settings(SettingsCategory.General)) },
                )
            }
        }

        NavigationDrawer(
            visible = drawerOpen,
            selected = route,
            onDismiss = { UiNavigator.drawerOpen.value = false },
            onNavigate = UiNavigator::navigate,
        )
    }
}
