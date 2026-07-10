package com.armsx2.navigation

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.AnimatedContentTransitionScope
import androidx.compose.animation.core.tween
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import com.armsx2.ui.achievements.AchievementsScreen
import com.armsx2.ui.bios.BiosManagerScreen
import com.armsx2.ui.controls.ControllerManagerScreen
import com.armsx2.ui.about.AboutScreen
import com.armsx2.ui.home.HomeScreen
import com.armsx2.ui.memorycards.MemoryCardScreen
import com.armsx2.ui.patches.PatchManagerScreen
import com.armsx2.ui.saves.SaveManagerScreen
import com.armsx2.ui.textures.TextureManagerScreen
import com.armsx2.ui.settingshub.SettingsScreen

@Composable
fun AppNavigation() {
    val route = UiNavigator.route.value
    val drawerOpen = UiNavigator.drawerOpen.value

    BackHandler(enabled = true) { UiNavigator.back() }

    Box(Modifier.fillMaxSize()) {
        AnimatedContent(
            targetState = route,
            modifier = Modifier.fillMaxSize(),
            transitionSpec = {
                val forward = targetState !is AppRoute.Home
                val enter = fadeIn(tween(180)) + slideIntoContainer(
                    towards = if (forward) AnimatedContentTransitionScope.SlideDirection.Left else AnimatedContentTransitionScope.SlideDirection.Right,
                    animationSpec = tween(220),
                    initialOffset = { it / 10 },
                )
                val exit = fadeOut(tween(140))
                enter togetherWith exit
            },
            label = "app-route",
        ) { destination ->
            when (destination) {
                AppRoute.Home -> HomeScreen(
                    onOpenMenu = { UiNavigator.drawerOpen.value = true },
                    onOpenSettings = { UiNavigator.navigate(AppRoute.Settings()) },
                    onOpenGameSettings = { UiNavigator.navigate(AppRoute.Settings(game = it)) },
                )
                is AppRoute.Settings -> SettingsScreen(
                    initialCategory = destination.category,
                    game = destination.game,
                    onBack = UiNavigator::home,
                )
                AppRoute.BiosManager -> BiosManagerScreen(onBack = UiNavigator::home)
                AppRoute.MemoryCardManager -> MemoryCardScreen(onBack = UiNavigator::home)
                AppRoute.SaveManager -> SaveManagerScreen(onBack = UiNavigator::home)
                AppRoute.ControllerManager -> ControllerManagerScreen(onBack = UiNavigator::home)
                AppRoute.PatchManager -> PatchManagerScreen(onBack = UiNavigator::home)
                AppRoute.TextureManager -> TextureManagerScreen(onBack = UiNavigator::home)
                AppRoute.Achievements -> AchievementsScreen(onBack = UiNavigator::home)
                AppRoute.About -> AboutScreen(onBack = UiNavigator::home)
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
