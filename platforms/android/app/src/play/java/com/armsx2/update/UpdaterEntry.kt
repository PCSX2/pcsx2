package com.armsx2.update

import androidx.compose.runtime.Composable

/**
 * Play-flavor no-op stub of the in-app updater. Play forbids self-updating apps, so the real
 * implementation (network check, APK download, REQUEST_INSTALL_PACKAGES install) lives ONLY in
 * src/github. Shared code (AboutScreen) calls UpdaterEntry() gated on BuildConfig.IN_APP_UPDATER;
 * this stub lets the play flavor compile that reference while shipping nothing. The play build must
 * never gain the updater code or permission — build-play-aab.sh fails closed if it does.
 */
@Composable
fun UpdaterEntry() {
    // intentionally empty
}
