package com.armsx2.ui.settings

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import android.widget.Toast
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import com.armsx2.navigation.AppRoute
import com.armsx2.navigation.UiNavigator
import com.armsx2.runtime.MainActivityRuntime
import com.armsx2.ui.theme.BootLogoPreferences
import com.armsx2.ui.theme.ThemeMode
import com.armsx2.ui.theme.ThemePreferences
import com.armsx2.ui.theme.ToolbarPositionPreferences
import com.armsx2.ui.theme.LibraryChromePreferences
import java.io.File

@Composable
fun AppTab() {
    val currentLanguage = I18n.languages.firstOrNull { it.code == I18n.current }

    Column(
        modifier = Modifier.fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(10.dp),
    ) {
        Surface(
            onClick = { UiNavigator.navigate(AppRoute.Language) },
            modifier = Modifier.fillMaxWidth()
                .controllerFocusable("app.language", RoundedCornerShape(20.dp), onConfirm = { UiNavigator.navigate(AppRoute.Language) }),
            shape = RoundedCornerShape(20.dp),
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
            border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
        ) {
            Row(
                modifier = Modifier.fillMaxWidth().padding(horizontal = 14.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Surface(
                    modifier = Modifier.size(46.dp),
                    shape = RoundedCornerShape(14.dp),
                    color = MaterialTheme.colorScheme.primaryContainer,
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center) {
                        Text("◎", color = MaterialTheme.colorScheme.primary, fontSize = 23.sp, fontWeight = FontWeight.Bold)
                    }
                }
                Spacer(Modifier.width(12.dp))
                Column(Modifier.weight(1f)) {
                    Text(str("app.language"), style = MaterialTheme.typography.titleMedium)
                    Text(
                        if (I18n.selected == I18n.SYSTEM_CODE) str("app.language.system") else currentLanguage?.nativeName ?: "English",
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                Text("›", color = MaterialTheme.colorScheme.primary, fontSize = 26.sp)
            }
        }

        SegmentedRow(
            label = str("app.theme"),
            options = listOf(
                str("app.theme.system"), str("app.theme.light"), str("app.theme.dark"),
                str("app.theme.black"), str("app.theme.oled"),
            ),
            selectedIndex = when (ThemePreferences.mode.value) {
                ThemeMode.System -> 0
                ThemeMode.Light -> 1
                ThemeMode.Dark -> 2
                ThemeMode.Black -> 3
                ThemeMode.Oled -> 4
            },
            onChange = { index ->
                ThemePreferences.set(
                    when (index) {
                        1 -> ThemeMode.Light
                        2 -> ThemeMode.Dark
                        3 -> ThemeMode.Black
                        4 -> ThemeMode.Oled
                        else -> ThemeMode.System
                    },
                )
            },
        )

        ToggleRow(
            label = str("app.bootLogo"),
            value = BootLogoPreferences.enabled.value,
            description = str("app.bootLogo.desc"),
            onChange = { BootLogoPreferences.set(it) },
        )

        SegmentedRow(
            label = str("app.toolbarPosition"),
            options = listOf(str("app.toolbarPosition.top"), str("app.toolbarPosition.bottom")),
            selectedIndex = if (ToolbarPositionPreferences.atBottom.value) 1 else 0,
            onChange = { ToolbarPositionPreferences.set(it == 1) },
        )

        ToggleRow(
            label = str("app.library.search"),
            value = LibraryChromePreferences.showSearch.value,
            description = str("app.library.search.desc"),
            onChange = LibraryChromePreferences::setShowSearch,
        )

        ToggleRow(
            label = str("app.library.recents"),
            value = LibraryChromePreferences.showRecents.value,
            description = str("app.library.recents.desc"),
            onChange = LibraryChromePreferences::setShowRecents,
        )

        ClearCacheRow()
    }
}

/** Clear cached, regenerable data: compiled shader/pipeline caches (Vulkan + GL) and
 *  the cover-art image cache. All of it rebuilds automatically, so this only frees space
 *  and forces a clean rebuild — handy after a driver change or if a cache looks corrupt. */
@Composable
private fun ClearCacheRow() {
    val context = LocalContext.current
    var status by remember { mutableStateOf("") }
    Surface(
        onClick = { status = clearAppCaches(context) },
        modifier = Modifier.fillMaxWidth()
            .controllerFocusable("app.clearCache", RoundedCornerShape(20.dp), onConfirm = { status = clearAppCaches(context) }),
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
    ) {
        Row(
            modifier = Modifier.fillMaxWidth().padding(horizontal = 14.dp, vertical = 12.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Surface(
                modifier = Modifier.size(46.dp),
                shape = RoundedCornerShape(14.dp),
                color = MaterialTheme.colorScheme.primaryContainer,
            ) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center) {
                    Text("🧹", fontSize = 21.sp)
                }
            }
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text(str("app.clearCache"), style = MaterialTheme.typography.titleMedium)
                Text(
                    status.ifEmpty { str("app.clearCache.desc") },
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

/** Delete shader/pipeline caches (assetCopyRoot/cache) + the OS cache dir (Coil image
 *  cache, temp files). Returns a human-readable summary for the row + a toast. */
private fun clearAppCaches(context: android.content.Context): String {
    var removed = 0
    var bytes = 0L
    fun wipe(dir: File?) {
        val entries = dir?.listFiles() ?: return
        for (f in entries) {
            val size = if (f.isFile) f.length() else 0L
            val ok = if (f.isDirectory) f.deleteRecursively() else runCatching { f.delete() }.getOrDefault(false)
            if (ok) { removed++; bytes += size }
        }
    }
    // Compiled shader / pipeline caches live under the app-private asset-copy root.
    wipe(File(MainActivityRuntime.assetCopyRoot(context), "cache"))
    // Coil cover-art cache + any transient files in the OS-managed cache dir.
    wipe(context.cacheDir)
    val summary = if (removed > 0) {
        val mb = bytes / (1024.0 * 1024.0)
        if (mb >= 0.1) I18n.get("app.clearCache.done").replace("%s", String.format("%.1f MB", mb))
        else I18n.get("app.clearCache.doneSmall")
    } else {
        I18n.get("app.clearCache.empty")
    }
    Toast.makeText(context, summary, Toast.LENGTH_SHORT).show()
    return summary
}
