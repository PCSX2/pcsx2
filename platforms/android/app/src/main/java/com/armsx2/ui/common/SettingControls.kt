package com.armsx2.ui.common

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.i18n.str
import com.armsx2.ui.settings.controllerFocusable

@Composable
fun SettingSwitchRow(
    title: String,
    description: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier,
) {
    // Match ToggleRow: menu SFX on every flip, and a "toggle:$title" nav id so the controller can
    // reach the row (the updater switches were touch-only before) and settings-search can jump to it.
    val emit: (Boolean) -> Unit = {
        com.armsx2.MenuSfx.play(if (it) com.armsx2.MenuSfx.Event.TOGGLE_ON else com.armsx2.MenuSfx.Event.TOGGLE_OFF)
        onCheckedChange(it)
    }
    Surface(
        onClick = { emit(!checked) },
        modifier = modifier.fillMaxWidth()
            .controllerFocusable(
                controllerId = "toggle:$title",
                shape = RoundedCornerShape(22.dp),
                onConfirm = { emit(!checked) },
                onLeft = { if (checked) emit(false) },
                onRight = { if (!checked) emit(true) },
            ),
        shape = RoundedCornerShape(22.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.7f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.42f)),
    ) {
        Row(
            Modifier.defaultMinSize(minHeight = 78.dp).padding(horizontal = 16.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Column(Modifier.weight(1f)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(title, style = MaterialTheme.typography.titleMedium, modifier = Modifier.weight(1f))
                    SettingInfoHint(title, description)
                }
                Text(description, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Spacer(Modifier.width(16.dp))
            Switch(checked = checked, onCheckedChange = emit)
        }
    }
}

@Composable
private fun SettingInfoHint(title: String, description: String) {
    var open by remember { mutableStateOf(false) }
    Box(
        Modifier
            .padding(start = 8.dp)
            .size(22.dp)
            .clip(androidx.compose.foundation.shape.CircleShape)
            .background(MaterialTheme.colorScheme.primaryContainer)
            .clickable { open = true },
        contentAlignment = Alignment.Center,
    ) {
        Text("i", color = MaterialTheme.colorScheme.primary, fontSize = 13.sp, fontWeight = FontWeight.Bold)
    }
    if (open) {
        AlertDialog(
            onDismissRequest = { open = false },
            title = { Text(title) },
            text = { Text(description) },
            confirmButton = { TextButton(onClick = { open = false }) { Text(str("action.close")) } },
        )
    }
}
