package com.armsx2.ui.about

import android.app.Application
import android.app.ActivityManager
import android.content.Context
import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.GLES20
import android.os.Build
import android.system.Os
import android.system.OsConstants
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.armsx2.BuildConfig
import kr.co.iefriends.pcsx2.NativeApp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.util.Locale

data class AboutUiState(
    val appVersion: String = BuildConfig.VERSION_NAME,
    val coreVersion: String = "",
    val device: String = "${Build.MANUFACTURER} ${Build.MODEL}".trim(),
    val androidVersion: String = "Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})",
    val soc: String = "",
    val gpu: String = "",
    val cpu: String = "",
    val memory: String = "",
    val display: String = "",
    val architecture: String = "",
    val pageSize: String = "",
)

class AboutViewModel(application: Application) : AndroidViewModel(application) {
    var state = androidx.compose.runtime.mutableStateOf(AboutUiState())
        private set

    fun load() {
        viewModelScope.launch {
            val details = withContext(Dispatchers.Default) { collectDeviceDetails(getApplication()) }
            state.value = state.value.copy(
                coreVersion = runCatching { NativeApp.getBuildVersion() }.getOrDefault("—"),
                soc = details.soc,
                gpu = details.gpu,
                cpu = details.cpu,
                memory = details.memory,
                display = details.display,
                architecture = details.architecture,
                pageSize = details.pageSize,
            )
        }
    }

    private data class DeviceDetails(
        val soc: String,
        val gpu: String,
        val cpu: String,
        val memory: String,
        val display: String,
        val architecture: String,
        val pageSize: String,
    )

    private fun collectDeviceDetails(context: Context): DeviceDetails {
        val activityManager = context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
        val memoryInfo = ActivityManager.MemoryInfo().also(activityManager::getMemoryInfo)
        val metrics = context.resources.displayMetrics
        val pageBytes = runCatching { Os.sysconf(OsConstants._SC_PAGESIZE) }.getOrDefault(4096L)
        val soc = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            listOf(Build.SOC_MANUFACTURER, Build.SOC_MODEL).filter(String::isNotBlank).joinToString(" ")
        } else {
            Build.HARDWARE
        }
        return DeviceDetails(
            soc = soc.ifBlank { "—" },
            gpu = queryGpuRenderer().ifBlank { "—" },
            cpu = Runtime.getRuntime().availableProcessors().toString(),
            memory = String.format(Locale.US, "%.1f GB", memoryInfo.totalMem / 1_073_741_824.0),
            display = "${metrics.widthPixels} × ${metrics.heightPixels} · ${metrics.densityDpi} dpi",
            architecture = Build.SUPPORTED_ABIS.joinToString(", ").ifBlank { "—" },
            pageSize = if (pageBytes >= 16_384L) "${pageBytes / 1024L} KB (16K)" else "${pageBytes / 1024L} KB",
        )
    }

    private fun queryGpuRenderer(): String {
        val display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        if (display == EGL14.EGL_NO_DISPLAY) return ""
        val version = IntArray(2)
        if (!EGL14.eglInitialize(display, version, 0, version, 1)) return ""
        var surface = EGL14.EGL_NO_SURFACE
        var context = EGL14.EGL_NO_CONTEXT
        return try {
            val attributes = intArrayOf(
                EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
                EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
                EGL14.EGL_RED_SIZE, 8,
                EGL14.EGL_GREEN_SIZE, 8,
                EGL14.EGL_BLUE_SIZE, 8,
                EGL14.EGL_NONE,
            )
            val configs = arrayOfNulls<EGLConfig>(1)
            val count = IntArray(1)
            if (!EGL14.eglChooseConfig(display, attributes, 0, configs, 0, 1, count, 0) || count[0] == 0) return ""
            val config = configs[0] ?: return ""
            context = EGL14.eglCreateContext(
                display,
                config,
                EGL14.EGL_NO_CONTEXT,
                intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE),
                0,
            )
            surface = EGL14.eglCreatePbufferSurface(
                display,
                config,
                intArrayOf(EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE),
                0,
            )
            if (context == EGL14.EGL_NO_CONTEXT || surface == EGL14.EGL_NO_SURFACE ||
                !EGL14.eglMakeCurrent(display, surface, surface, context)
            ) return ""
            GLES20.glGetString(GLES20.GL_RENDERER).orEmpty()
        } finally {
            EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT)
            if (surface != EGL14.EGL_NO_SURFACE) EGL14.eglDestroySurface(display, surface)
            if (context != EGL14.EGL_NO_CONTEXT) EGL14.eglDestroyContext(display, context)
            EGL14.eglTerminate(display)
        }
    }
}
