package com.armsx2.ui.home

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.runtime.mutableStateOf
import com.armsx2.runtime.MainActivityRuntime

/**
 * Optional user-chosen library background image (#9). Stores a persisted content
 * URI; when unset the library looks exactly as before (nothing renders), so this
 * is inert unless the user picks a background. Static images and animated GIF/WebP
 * both work — the library already loads via Coil with animated-image decoders.
 */
object LibraryBackground {
    private const val PREF = "library.background.uri"
    val uri = mutableStateOf<String?>(null)
    private var loaded = false

    fun ensureLoaded() {
        if (loaded) return
        loaded = true
        uri.value = runCatching { MainActivityRuntime.prefs.getString(PREF, null) }.getOrNull()
    }

    fun set(context: Context, value: Uri) {
        runCatching {
            context.contentResolver.takePersistableUriPermission(value, Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        uri.value = value.toString()
        runCatching { MainActivityRuntime.prefs.edit().putString(PREF, value.toString()).apply() }
    }

    fun clear() {
        uri.value = null
        runCatching { MainActivityRuntime.prefs.edit().remove(PREF).apply() }
    }
}
