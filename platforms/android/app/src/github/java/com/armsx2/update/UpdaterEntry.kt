package com.armsx2.update

import android.content.Context
import android.content.Intent
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.FileProvider
import com.armsx2.BuildConfig
import com.armsx2.i18n.str
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.SettingSwitchRow
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL

/**
 * In-app updater — GitHub sideload flavor ONLY. Renders a "Check for updates" panel in the About
 * screen: it hits the GitHub releases/latest API, compares the latest STABLE release tag against the
 * installed build, and offers to download + install the APK. The play flavor gets the no-op stub in
 * src/play, so this code (and REQUEST_INSTALL_PACKAGES / the FileProvider) never enters the AAB —
 * build-play-aab.sh also fails closed if the permission ever leaks in.
 *
 * Nightly-safe: nightly builds use versionCode = Unix seconds (> 1e6), so their version is always
 * far ahead of any stable release. We short-circuit those to "up to date" and never prompt a nightly
 * user to a stable — comparison is by the numeric versionCode magnitude, not the version string.
 */

private const val LATEST_URL = "https://api.github.com/repos/ARMSX2/ARMSX2/releases/latest"
private const val NIGHTLY_VC_THRESHOLD = 1_000_000  // stable VCs are ~1300; nightly = Unix seconds.

private sealed interface UpdateState {
    data object Idle : UpdateState
    data object Checking : UpdateState
    data object UpToDate : UpdateState
    data class Available(val version: String, val notes: String, val apkUrl: String) : UpdateState
    data class Downloading(val pct: Int) : UpdateState
    data class Error(val msg: String) : UpdateState
}

@Composable
fun UpdaterEntry() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var state by remember { mutableStateOf<UpdateState>(UpdateState.Idle) }
    // str() is @Composable, so resolve the strings the background check / onClick handlers need
    // here and capture them (they run outside composition).
    val checkFailedPrefix = str("update.checkFailed")
    val downloadFailedPrefix = str("update.downloadFailed")

    GlassPanel(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 6.dp)) {
        Column(Modifier.padding(4.dp)) {
            Text(str("update.title"), style = MaterialTheme.typography.titleMedium)
            Text(
                "${str("update.currentVersion")}: ${BuildConfig.VERSION_NAME}",
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Spacer(Modifier.height(8.dp))
            when (val s = state) {
                is UpdateState.Checking -> Row(verticalAlignment = Alignment.CenterVertically) {
                    CircularProgressIndicator(Modifier.size(16.dp), strokeWidth = 2.dp)
                    Spacer(Modifier.width(8.dp))
                    Text(str("update.checking"), style = MaterialTheme.typography.bodySmall)
                }
                is UpdateState.UpToDate -> Text(
                    if (BuildConfig.VERSION_CODE > NIGHTLY_VC_THRESHOLD) str("update.onNightly")
                    else str("update.upToDate"),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.primary,
                )
                is UpdateState.Error -> Text(
                    s.msg, style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.error,
                )
                is UpdateState.Downloading -> Column {
                    Text("${str("update.downloading")} ${s.pct}%", style = MaterialTheme.typography.bodySmall)
                    Spacer(Modifier.height(4.dp))
                    LinearProgressIndicator(
                        progress = { s.pct / 100f },
                        modifier = Modifier.fillMaxWidth(),
                    )
                }
                else -> {}
            }
            Spacer(Modifier.height(8.dp))
            Button(
                enabled = state !is UpdateState.Checking && state !is UpdateState.Downloading,
                onClick = {
                    scope.launch {
                        state = UpdateState.Checking
                        state = checkForUpdate(checkFailedPrefix)
                    }
                },
            ) { Text(str("update.check")) }

            // Opt-in: silently check GitHub for a newer release on every app launch (default off).
            var checkOnLaunch by remember {
                mutableStateOf(MainActivityRuntime.prefs.getBoolean("update.checkOnLaunch", false))
            }
            SettingSwitchRow(
                title = str("update.checkOnLaunch"),
                description = str("update.checkOnLaunch.desc"),
                checked = checkOnLaunch,
                onCheckedChange = {
                    checkOnLaunch = it
                    MainActivityRuntime.prefs.edit().putBoolean("update.checkOnLaunch", it).apply()
                },
            )
        }
    }

    (state as? UpdateState.Available)?.let { avail ->
        AlertDialog(
            onDismissRequest = { state = UpdateState.Idle },
            title = { Text("${str("update.available")}  ${avail.version}") },
            text = {
                Column(Modifier.heightIn(max = 320.dp).verticalScroll(rememberScrollState())) {
                    Text(
                        avail.notes.ifBlank { str("update.notesUnavailable") },
                        style = MaterialTheme.typography.bodySmall,
                    )
                }
            },
            confirmButton = {
                TextButton(onClick = {
                    scope.launch {
                        try {
                            downloadAndInstall(context, avail) { pct -> state = UpdateState.Downloading(pct) }
                            state = UpdateState.Idle
                        } catch (e: Exception) {
                            state = UpdateState.Error("$downloadFailedPrefix: ${e.message}")
                        }
                    }
                }) { Text(str("update.install")) }
            },
            dismissButton = {
                TextButton(onClick = { state = UpdateState.Idle }) { Text(str("action.cancel")) }
            },
        )
    }
}

/**
 * Boot-time auto-check (github flavor only). Mounted once at the app root; when the "check on
 * launch" toggle is on, it runs a single silent GitHub check on start and pops the update prompt
 * ONLY if a newer release exists — no "up to date" popup, no noise on every boot. Reuses the exact
 * check/download/install path as the manual button. Nightly-safe via checkForUpdate's VC guard.
 */
@Composable
fun AutoUpdateGate() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var state by remember { mutableStateOf<UpdateState>(UpdateState.Idle) }
    val checkFailedPrefix = str("update.checkFailed")

    LaunchedEffect(Unit) {
        if (MainActivityRuntime.prefs.getBoolean("update.checkOnLaunch", false)) {
            val result = checkForUpdate(checkFailedPrefix)
            if (result is UpdateState.Available) state = result  // stay silent on up-to-date / errors
        }
    }

    val s = state
    if (s is UpdateState.Available || s is UpdateState.Downloading) {
        val avail = s as? UpdateState.Available
        AlertDialog(
            onDismissRequest = { if (state !is UpdateState.Downloading) state = UpdateState.Idle },
            title = {
                Text(
                    if (state is UpdateState.Downloading) str("update.downloading")
                    else "${str("update.available")}  ${avail?.version.orEmpty()}",
                )
            },
            text = {
                when (val cur = state) {
                    is UpdateState.Available -> Column(Modifier.heightIn(max = 300.dp).verticalScroll(rememberScrollState())) {
                        Text(cur.notes.ifBlank { str("update.notesUnavailable") }, style = MaterialTheme.typography.bodySmall)
                    }
                    is UpdateState.Downloading -> Column {
                        Text("${cur.pct}%", style = MaterialTheme.typography.bodySmall)
                        Spacer(Modifier.height(6.dp))
                        LinearProgressIndicator(progress = { cur.pct / 100f }, modifier = Modifier.fillMaxWidth())
                    }
                    else -> {}
                }
            },
            confirmButton = {
                if (avail != null) {
                    TextButton(onClick = {
                        scope.launch {
                            try {
                                downloadAndInstall(context, avail) { pct -> state = UpdateState.Downloading(pct) }
                            } finally {
                                state = UpdateState.Idle
                            }
                        }
                    }) { Text(str("update.install")) }
                }
            },
            dismissButton = {
                if (state is UpdateState.Available) {
                    TextButton(onClick = { state = UpdateState.Idle }) { Text(str("update.later")) }
                }
            },
        )
    }
}

private suspend fun checkForUpdate(checkFailedPrefix: String): UpdateState = withContext(Dispatchers.IO) {
    // Nightly builds are always ahead of any stable release — never prompt them.
    if (BuildConfig.VERSION_CODE > NIGHTLY_VC_THRESHOLD) return@withContext UpdateState.UpToDate
    try {
        val obj = JSONObject(httpGet(LATEST_URL))
        val tag = obj.getString("tag_name")
        val notes = obj.optString("body", "")
        val assets = obj.getJSONArray("assets")
        var apkUrl = ""
        for (i in 0 until assets.length()) {
            val a = assets.getJSONObject(i)
            if (a.getString("name").endsWith(".apk", ignoreCase = true)) {
                apkUrl = a.getString("browser_download_url"); break
            }
        }
        if (apkUrl.isNotEmpty() && isNewer(tag, BuildConfig.VERSION_NAME))
            UpdateState.Available(tag, notes, apkUrl)
        else UpdateState.UpToDate
    } catch (e: Exception) {
        UpdateState.Error("$checkFailedPrefix: ${e.message}")
    }
}

/** Semantic-version compare of the release tag vs the installed versionName. Non-numeric suffixes
 *  (e.g. the "2.6.4.3.r" tag) are dropped — only the leading dotted integers matter. */
private fun isNewer(remoteTag: String, installed: String): Boolean {
    fun parts(v: String) = v.trim().removePrefix("v").split('.', '-')
        .map { it.takeWhile(Char::isDigit) }.mapNotNull { it.toIntOrNull() }
    val r = parts(remoteTag); val i = parts(installed)
    for (k in 0 until maxOf(r.size, i.size)) {
        val a = r.getOrElse(k) { 0 }; val b = i.getOrElse(k) { 0 }
        if (a != b) return a > b
    }
    return false
}

private fun httpGet(url: String): String {
    val conn = URL(url).openConnection() as HttpURLConnection
    return try {
        conn.connectTimeout = 10_000
        conn.readTimeout = 15_000
        conn.setRequestProperty("Accept", "application/vnd.github+json")
        conn.setRequestProperty("User-Agent", "ARMSX2-Updater")
        conn.inputStream.bufferedReader().use { it.readText() }
    } finally {
        conn.disconnect()
    }
}

private suspend fun downloadAndInstall(context: Context, info: UpdateState.Available, onProgress: (Int) -> Unit) {
    val apk = withContext(Dispatchers.IO) {
        val dir = File(context.externalCacheDir, "updates").apply { mkdirs() }
        dir.listFiles()?.forEach { it.delete() }  // keep only the current download
        val out = File(dir, "armsx2-update.apk")
        val conn = URL(info.apkUrl).openConnection() as HttpURLConnection
        try {
            conn.connectTimeout = 10_000
            conn.readTimeout = 30_000
            conn.setRequestProperty("User-Agent", "ARMSX2-Updater")
            val total = conn.contentLengthLong
            var read = 0L
            var lastPct = -1
            conn.inputStream.use { input ->
                out.outputStream().use { sink ->
                    val buf = ByteArray(64 * 1024)
                    while (true) {
                        val n = input.read(buf)
                        if (n < 0) break
                        sink.write(buf, 0, n)
                        read += n
                        if (total > 0) {
                            val pct = ((read * 100) / total).toInt()
                            if (pct != lastPct) {
                                lastPct = pct
                                withContext(Dispatchers.Main) { onProgress(pct) }
                            }
                        }
                    }
                }
            }
        } finally {
            conn.disconnect()
        }
        out
    }
    // Hand the APK to the system package installer (user confirms the install).
    val uri = FileProvider.getUriForFile(context, "${context.packageName}.updateprovider", apk)
    val intent = Intent(Intent.ACTION_VIEW).apply {
        setDataAndType(uri, "application/vnd.android.package-archive")
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK)
    }
    context.startActivity(intent)
}
