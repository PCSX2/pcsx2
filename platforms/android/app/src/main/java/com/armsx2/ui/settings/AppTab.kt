package com.armsx2.ui.settings

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str

/**
 * App-level settings. Currently the Language picker — the live-translation control.
 *
 * Picking a language calls [I18n.setLanguage], which flips the [I18n.current] snapshot State and
 * recomposes every [str] call site instantly (no restart). This tab's own labels use [str], so
 * switching language re-texts them live — the visible proof the system works. English is the
 * source of truth; other languages fall back to English per-key for any untranslated string.
 */
@Composable
fun AppTab() {
    val context = LocalContext.current
    val scroll = remember { ScrollState(0) }
    ControllerAutoScroll(scroll)
    val current = I18n.current // subscribe: re-highlight the selected row on change

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .verticalScroll(scroll)
            .verticalScrollbar(scroll),
    ) {
        Text(
            str("app.language"),
            color = MaterialTheme.colorScheme.onSurface,
            fontSize = 15.sp,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier.padding(bottom = 2.dp),
        )
        Text(
            str("app.language.desc"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 11.sp,
            modifier = Modifier.padding(bottom = 10.dp),
        )
        I18n.languages.forEach { lang ->
            val selected = lang.code == current
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(
                        if (selected) MaterialTheme.colorScheme.primaryContainer
                        else MaterialTheme.colorScheme.surface.copy(alpha = 0f),
                    )
                    .controllerFocusable(
                        controllerId = "lang:${lang.code}",
                        shape = RoundedCornerShape(8.dp),
                        onConfirm = { I18n.setLanguage(context, lang.code) },
                    )
                    .clickable { I18n.setLanguage(context, lang.code) }
                    .padding(horizontal = 12.dp, vertical = 10.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    lang.nativeName,
                    color = if (selected) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurface,
                    fontSize = 14.sp,
                    fontWeight = if (selected) FontWeight.SemiBold else FontWeight.Normal,
                    modifier = Modifier.weight(1f),
                )
                Text(
                    lang.englishName,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontSize = 11.sp,
                )
                if (selected) {
                    Text(
                        "  ✓",
                        color = MaterialTheme.colorScheme.primary,
                        fontSize = 14.sp,
                    )
                }
            }
            SettingsDivider()
        }
    }
}
