package com.armsx2.ui.about

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.armsx2.i18n.str
import com.armsx2.ui.common.ArmsBackdrop
import com.armsx2.ui.common.ArmsLogo
import com.armsx2.ui.common.ArmsTopBar
import com.armsx2.ui.common.GlassPanel
import com.armsx2.ui.common.RoundAction

private const val RepositoryUrl = "https://github.com/ARMSX2/ARMSX2"

@Composable
fun AboutScreen(onBack: () -> Unit, viewModel: AboutViewModel = viewModel()) {
    val state = viewModel.state.value
    val uriHandler = LocalUriHandler.current
    LaunchedEffect(Unit) { viewModel.load() }

    ArmsBackdrop {
        Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState())) {
            ArmsTopBar(
                title = str("about.title"),
                leading = { RoundAction("‹", str("action.back"), onBack) },
            )
            BoxWithConstraints(Modifier.fillMaxWidth()) {
                val compact = maxWidth < 760.dp
                val overview: @Composable (Modifier) -> Unit = { panelModifier ->
                    GlassPanel(panelModifier) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            ArmsLogo(showWordmark = true)
                            Spacer(Modifier.height(12.dp))
                            Text(
                                str("about.tagline"),
                                style = MaterialTheme.typography.bodyLarge,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
                    }
                }
                val information: @Composable (Modifier) -> Unit = { panelModifier ->
                    GlassPanel(panelModifier) {
                        Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                            InfoRow(str("about.appVersion"), state.appVersion)
                            InfoRow(str("about.coreVersion"), state.coreVersion)
                            InfoRow(str("about.device"), state.device)
                            InfoRow(str("about.androidVersion"), state.androidVersion)
                        }
                    }
                }
                Column(
                    Modifier.fillMaxWidth().padding(start = 8.dp, end = 8.dp, bottom = 8.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                ) {
                    if (compact) {
                        overview(Modifier.fillMaxWidth())
                        information(Modifier.fillMaxWidth())
                    } else {
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(14.dp)) {
                            overview(Modifier.weight(1f))
                            information(Modifier.weight(1f))
                        }
                    }
                    RepositoryCard { uriHandler.openUri(RepositoryUrl) }
                }
            }
        }
    }
}

@Composable
private fun RepositoryCard(onOpen: () -> Unit) {
    Surface(
        onClick = onOpen,
        modifier = Modifier.fillMaxWidth(),
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.58f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.primary.copy(alpha = 0.48f)),
    ) {
        Row(
            modifier = Modifier.padding(horizontal = 15.dp, vertical = 14.dp),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Surface(
                modifier = Modifier.size(48.dp),
                shape = RoundedCornerShape(14.dp),
                color = MaterialTheme.colorScheme.primary.copy(alpha = 0.14f),
            ) {
                Box(contentAlignment = Alignment.Center) {
                    Text("⌘", color = MaterialTheme.colorScheme.primary, fontSize = 22.sp, fontWeight = FontWeight.Bold)
                }
            }
            Spacer(Modifier.size(12.dp))
            Column(Modifier.weight(1f)) {
                Text(str("about.repository.title"), style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                Text(
                    "ARMSX2/ARMSX2",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.primary,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Text(
                    str("about.repository.description"),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Text("↗", color = MaterialTheme.colorScheme.primary, fontSize = 22.sp)
        }
    }
}

@Composable
private fun InfoRow(label: String, value: String) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Text(
            label,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            modifier = Modifier.weight(1f),
        )
        Text(
            value,
            style = MaterialTheme.typography.bodyMedium,
            fontWeight = FontWeight.SemiBold,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
        )
    }
}
