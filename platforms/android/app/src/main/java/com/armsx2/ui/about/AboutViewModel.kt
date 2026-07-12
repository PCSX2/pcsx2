package com.armsx2.ui.about

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import com.armsx2.BuildConfig
import kr.co.iefriends.pcsx2.NativeApp

data class AboutUiState(
    val appVersion: String = BuildConfig.VERSION_NAME,
    val coreVersion: String = "",
    val device: String = "${android.os.Build.MANUFACTURER} ${android.os.Build.MODEL}".trim(),
    val androidVersion: String = "Android ${android.os.Build.VERSION.RELEASE}",
)

class AboutViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(AboutUiState())
        private set

    fun load() {
        state.value = state.value.copy(coreVersion = runCatching { NativeApp.getBuildVersion() }.getOrDefault("Unknown"))
    }
}

