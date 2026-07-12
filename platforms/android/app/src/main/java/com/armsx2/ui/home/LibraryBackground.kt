package com.armsx2.ui.home

import android.content.Context
import android.content.Intent
import android.net.Uri
import androidx.compose.runtime.mutableStateOf
import com.armsx2.runtime.MainActivityRuntime

/**
 * Optional user-chosen library background (#9). Stores a persisted content URI;
 * when unset the library falls back to the bundled default XMB-wave still image
 * (R.drawable.library_bg_xmb, drawn in HomeScreen). The user can pick a still image
 * or an animated GIF/WebP (Coil handles both); `clear()` reverts to the default.
 * (The background used to be a looping MP4 but that hurt performance, so it's a
 * static image now.)
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
