package com.armsx2.ui.common

import android.content.Context
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.ShaderParam
import com.armsx2.ShaderParams
import com.armsx2.ShaderRepo
import com.armsx2.i18n.str
import com.armsx2.ui.settings.HelpText
import com.armsx2.ui.settings.IntSliderRow
import com.armsx2.ui.settings.SettingsDivider
import com.armsx2.ui.settings.ToggleRow
import com.armsx2.ui.settings.controllerFocusable
import com.armsx2.ui.theme.Success
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/** One `.slangp` on disk. [label] is the bare filename (the folder it came from is on its
 *  group header); [path] is the absolute filesystem path stored in
 *  EmuCore/GS/ShaderChainPreset; [passes] is the resolved pass count — null when the file
 *  never yields one (see [resolvePasses]).
 *
 *  [passes] is reported as a FACT on the row, never used to rank or classify. Pass count is
 *  not a cost proxy and we're not going to pretend it is: xBRZ does enormous per-pixel
 *  neighbourhood comparison in ~5 passes and runs heavy, while koko-aio's ambilight chain
 *  runs ~19 passes at tiny scale and runs great. Cost is dominated by per-pass resolution
 *  and per-pass ALU, neither of which is knowable from the preset file — so the number is
 *  shown and the judgement is left to the user. Do NOT resurrect a Lightweight/Heavy split
 *  on top of it: a wrong "Lightweight" badge misleads exactly the user who picked that tier
 *  because they wanted cheap. */
private data class ShaderPreset(val label: String, val path: String, val passes: Int?)

/** One collapsible sub-folder inside a family.
 *
 *  [key] is the folder's path relative to the shaders ROOT, which is what makes it unique
 *  across the whole tree — the expansion map and controller ids key on it, and a
 *  relative-to-family key would collide ("Presets" exists under both Mega Bezel and
 *  scanline-classic) and make the two copies toggle as one. [label] is the same path
 *  relative to its FAMILY, so the family name isn't repeated on every row inside it
 *  ("Presets/Base_CRT_Presets", not "bezel/Mega_Bezel/Presets/Base_CRT_Presets").
 *
 *  Why sub-folders at all, and not presets straight under the family? Because a family
 *  alone is not bounded: Mega Bezel is 660 presets and scanline-classic 602, and a plain
 *  Column composes every row it is given — 660 rows would stall the frame and hand the
 *  D-pad 660 stops, the exact blocker this grouping exists to kill. Splitting each family
 *  by its own sub-folders caps the largest leaf at 115 (measured on the stock pack). */
private data class ShaderGroup(val key: String, val label: String, val presets: List<ShaderPreset>)

/** One shader family — a human-meaningful pack or category, and the top level of the
 *  picker: "Mega Bezel", "koko-aio", "CRT", "Handheld".
 *
 *  [direct] is the presets sitting in the family's OWN folder; [groups] is its sub-folders.
 *  A family has one, the other, or both: `crt` is 100 loose presets plus 6 sub-folders,
 *  Mega Bezel is 25 sub-folders and nothing loose, `misc` is 36 loose presets and no
 *  sub-folders at all — so a family without sub-folders lists its presets directly rather
 *  than burying them under a pointless "Uncategorised" row. */
private data class ShaderFamily(
    val key: String,
    val label: String,
    val count: Int,
    val direct: List<ShaderPreset>,
    val groups: List<ShaderGroup>,
)

/** Result of one scan: the folder we looked in (so the empty state can name it) plus every
 *  preset found under it, already costed, grouped into families and sorted — all of it done
 *  on the scan's IO thread, never in composition. */
private data class ShaderScan(val dir: String, val families: List<ShaderFamily>)

/** A preset plus the path segments of its folder, relative to the shaders root. */
private class FoundPreset(val preset: ShaderPreset, val segments: List<String>)

/** Depth cap for #reference chains. The deepest real chain in the stock pack is 7 hops
 *  (bezel/koko-aio/Presets-4.1/FXAA-bloom-immersive.slangp), so this is pure headroom for a
 *  future pack; the actual loop guard is the visited set in [resolvePasses]. */
private const val MAX_REFERENCE_DEPTH = 16

/** How big a top-level folder must be before we will treat it as a mere CONTAINER of
 *  families rather than a family itself. See [containerDirs] — this gate is what separates
 *  `bezel/` (1490 presets across four unrelated packs) from `edge-smoothing/` (113 presets
 *  across 20 sibling variants of one idea). */
private const val FAMILY_CONTAINER_MIN = 200

/** `shaders = 12` or `shaders = "12"` — 423 presets in the stock pack quote the value, so
 *  the quotes are not optional to handle. */
private val SHADERS_RE = Regex("""^\s*shaders\s*=\s*"?(\d+)"?""", RegexOption.IGNORE_CASE)

/** `#reference "../../Root_Presets/MBZ__3__STD__GDV.slangp"` — a preset that inherits its
 *  whole chain from another file and only overrides parameters. */
private val REFERENCE_RE = Regex("""^\s*#reference\s+(.+?)\s*$""", RegexOption.IGNORE_CASE)

/** Folder-name tokens that are initialisms, so [familyLabel] renders "CRT" not "Crt". */
private val FAMILY_ACRONYMS = setOf(
    "crt", "gpu", "hdr", "ntsc", "pal", "vhs", "nes", "bfi", "fsr", "lcd", "nis", "3d",
)

/** Folder names whose own branding is lowercase, where the mechanical title-casing in
 *  [familyLabel] would be wrong. Deliberately tiny: everything else in the stock pack
 *  humanises correctly by rule, and a hand-written table of all 39 names would rot the
 *  moment someone installs a pack we have never seen. */
private val FAMILY_NAME_OVERRIDES = mapOf("koko-aio" to "koko-aio", "uborder" to "uborder")

/** Renders a caption parameter's description as a group heading: "[ --- BLACK TINT --- ]:"
 *  reads "BLACK TINT". Cosmetic only — a parameter is identified as a caption by the fact
 *  that it cannot be adjusted ([ShaderParam.isAdjustable]), never by how it's written.
 *  Reading brackets to decide what a caption IS would hide real controls: the stock pack
 *  ships working sliders described "[ Adaptive Strobe (≈BFI) Strength: ]" and "[IMG]
 *  Contrast (squared) [luma]". Shared with the full-screen editor. */
internal fun captionLabel(description: String): String =
    description.trim().trim('[', ']', ':', '-', ' ').trim('-', ' ').ifBlank { description.trim() }

/**
 * RetroArch (.slangp) shader-chain rows: a master toggle plus an inline preset picker.
 *
 * Presentation only — the caller wires [onEnabledChange] / [onPresetChange] to its OWN
 * settings tier (the same split as [AngleDriverSection]). That's what lets one definition
 * serve both hosts without either knowing about the other's save path:
 *   - the Settings renderer tab passes its `apply()`, so the rows honour the Global / Game
 *     scope like every other row on that tab;
 *   - the in-game pause menu passes `viewModel.updateSettings`, which routes through
 *     InGameOverlay.saveSettings → ConfigStore.save(scope, serial) + Settings.applyTo().
 * Do NOT reach for a settings tier from in here — the tier is the caller's to decide.
 *
 * The picker is an INLINE expandable list modelled on DriverManagerSection. Two things it
 * deliberately is NOT:
 *   - an AlertDialog: a dialog is its own focused WINDOW and swallows gamepad key events,
 *     which would strand the list behind a controller.
 *   - a Lazy list: controllerFocusable only registers COMPOSED rows, so a LazyColumn would
 *     leave every off-screen preset out of the nav registry.
 * A plain Column composes every row, and the HOST's own verticalScroll does the scrolling
 * (the settings hub's LocalSettingsScrollState, or the pause menu's pane scroll) — nesting
 * a second vertical scroll here would measure against infinite height constraints and throw.
 */
@Composable
fun ShaderChainSection(
    enabled: Boolean,
    preset: String,
    params: Map<String, Map<String, Float>>,
    onEnabledChange: (Boolean) -> Unit,
    onPresetChange: (String) -> Unit,
    onParamsChange: (Map<String, Map<String, Float>>) -> Unit,
) {
    ToggleRow(
        str("renderer.shaderChain.label"),
        enabled,
        description = str("renderer.shaderChain.description"),
    ) {
        onEnabledChange(it)
    }
    // Gate: the picker only exists while the chain is on — the same shape as the
    // Shadeboost sliders / CAS sharpness rows above, and it keeps a dead row out of the
    // controller focus registry rather than parking focus on something inert.
    if (enabled) {
        SettingsDivider()
        ShaderPresetPicker(preset, onPresetChange)
        // Same gate one level down: with no preset there is nothing to enumerate, and the
        // section would be a row that opens onto an empty list.
        if (preset.isNotBlank()) {
            SettingsDivider()
            ShaderParamPicker(preset, params, onParamsChange, onPresetChange)
        }
    }
}

/**
 * The tweakable-parameter list for the selected preset.
 *
 * Values are written straight through to the caller's settings tier (same contract as the
 * preset picker) AND pushed at the running chain here, because those are two different
 * jobs and only one of them can be left to [com.armsx2.config.Settings.applyTo]:
 *  - persisting is the caller's, via [onParamsChange];
 *  - the LIVE apply has to send the effective value of every parameter, not just the
 *    overrides, so that resetting one restores the author's default on a chain that is
 *    already running. applyTo can't do that — it has no enumeration to read initials from.
 */
@Composable
private fun ShaderParamPicker(
    preset: String,
    allParams: Map<String, Map<String, Float>>,
    onParamsChange: (Map<String, Map<String, Float>>) -> Unit,
    onPresetChange: (String) -> Unit,
) {
    val overrides = allParams[preset].orEmpty()

    /** Drop the preset's entry entirely once nothing is overridden, so a reset-to-default
     *  leaves no trace in the config rather than an empty object reading as "tweaked". */
    fun persist(next: Map<String, Float>) {
        onParamsChange(if (next.isEmpty()) allParams - preset else allParams + (preset to next))
    }

    val open: () -> Unit = { ShaderParamsEditor.open(preset, overrides, ::persist, onPresetChange) }

    // Note what this row does NOT do: enumerate. The count of CHANGED parameters is just
    // the override map's size, and the total only matters once you're looking at the list —
    // so the (heavy) chain parse happens when the editor opens, not on every visit to this
    // tab.
    Surface(
        onClick = open,
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 5.dp)
            .controllerFocusable(
                controllerId = "shaderChain:params",
                shape = RoundedCornerShape(22.dp),
                onConfirm = open,
            ),
        shape = RoundedCornerShape(22.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .defaultMinSize(minHeight = 78.dp)
                .padding(horizontal = 16.dp, vertical = 12.dp),
        ) {
            Column(Modifier.weight(1f)) {
                Text(
                    str("renderer.shaderChain.params.label"),
                    color = MaterialTheme.colorScheme.onSurface,
                    fontSize = 18.sp,
                    lineHeight = 23.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                Spacer(Modifier.height(3.dp))
                Text(
                    if (overrides.isEmpty()) str("renderer.shaderChain.params.description")
                    else str("renderer.shaderChain.params.countModified").format(overrides.size),
                    color = MaterialTheme.colorScheme.primary,
                    fontSize = 14.sp,
                    lineHeight = 19.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            Spacer(Modifier.width(12.dp))
            // Opens a screen rather than expanding, so it points forward, not down.
            Text(
                "›",
                color = MaterialTheme.colorScheme.primary,
                fontSize = 20.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }

    // Reset-all and Save-as live INSIDE the editor, not here. They belong with the list
    // they act on, and both need input this pane can't give them: the reset wants a
    // confirmation, and naming a preset wants a keyboard a controller can actually drive.
    // The editor is a surface that owns the pad, so it can do both.
}

@Composable
private fun ShaderPresetPicker(preset: String, onPresetChange: (String) -> Unit) {
    val context = LocalContext.current
    val expanded = remember { mutableStateOf(false) }
    val scan = remember { mutableStateOf<ShaderScan?>(null) }
    val scanning = remember { mutableStateOf(false) }
    // Which families / folders are open. Collapsed by default — that's the whole point: a
    // full pack is ~2.5k presets, and composing them all would both stall the frame and
    // give the D-pad 2.5k stops to walk through. Fully collapsed this picker composes 40
    // rows: None + 39 family headers.
    val expandedFamilies = remember { mutableStateMapOf<String, Boolean>() }
    // Keyed on the folder's path relative to the shaders root, which is unique tree-wide.
    val expandedGroups = remember { mutableStateMapOf<String, Boolean>() }

    // Rescan on every open: packs get dropped in with a file manager (or the Shader Packs
    // downloader) while the app is alive, so a one-shot scan at first composition goes
    // stale. Off the UI thread — a slang-shaders pack is a deep tree of thousands of files,
    // and the costing/grouping/sorting rides the same IO hop rather than landing in
    // composition.
    LaunchedEffect(expanded.value) {
        if (!expanded.value) return@LaunchedEffect
        scanning.value = true
        val result = withContext(Dispatchers.IO) { scanShaderPresets(context) }
        scan.value = result
        // Open the family AND folder holding the active preset so reopening the picker
        // SHOWS the current selection instead of making the user hunt for it. Seeded per
        // scan, so a user collapsing it afterwards stays collapsed until the next rescan.
        result.families.forEach { family ->
            val group = family.groups.firstOrNull { g -> g.presets.any { it.path == preset } }
            if (group != null) {
                expandedFamilies[family.key] = true
                expandedGroups[group.key] = true
            } else if (family.direct.any { it.path == preset }) {
                expandedFamilies[family.key] = true
            }
        }
        scanning.value = false
    }

    val families = scan.value?.families.orEmpty()
    // Where the active preset lives, resolved ONCE per (scan, preset) rather than by
    // re-walking 2.5k presets from every header on every recomposition. Second element is
    // the folder key, or null when the preset sits directly in its family's own folder.
    val activeAt: Pair<String, String?>? = remember(scan.value, preset) {
        if (preset.isBlank()) null
        else scan.value?.families?.firstNotNullOfOrNull { f ->
            val group = f.groups.firstOrNull { g -> g.presets.any { it.path == preset } }
            when {
                group != null -> f.key to group.key
                f.direct.any { it.path == preset } -> f.key to null
                else -> null
            }
        }
    }

    Surface(
        onClick = { expanded.value = !expanded.value },
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 5.dp)
            .controllerFocusable(
                controllerId = "shaderChain:preset",
                shape = RoundedCornerShape(22.dp),
                onConfirm = { expanded.value = !expanded.value },
            ),
        shape = RoundedCornerShape(22.dp),
        color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.72f),
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.46f)),
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .defaultMinSize(minHeight = 78.dp)
                .padding(horizontal = 16.dp, vertical = 12.dp),
        ) {
            Column(Modifier.weight(1f)) {
                Text(
                    str("renderer.shaderChain.preset.label"),
                    color = MaterialTheme.colorScheme.onSurface,
                    fontSize = 18.sp,
                    lineHeight = 23.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                Spacer(Modifier.height(3.dp))
                Text(
                    // Filename without extension — the folder-qualified label is only
                    // needed inside the list, where two packs can share a basename.
                    if (preset.isBlank()) str("renderer.shaderChain.preset.none")
                    else File(preset).nameWithoutExtension,
                    color = MaterialTheme.colorScheme.primary,
                    fontSize = 14.sp,
                    lineHeight = 19.sp,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
            }
            Spacer(Modifier.width(12.dp))
            Text(
                if (expanded.value) "▾" else "▸",
                color = MaterialTheme.colorScheme.primary,
                fontSize = 16.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }

    if (expanded.value) {
        Column(
            modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
            verticalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            // "None" clears the setting; the chain then no-ops even with the toggle on.
            ShaderPresetRow(
                controllerId = "shaderChain:preset:none",
                label = str("renderer.shaderChain.preset.none"),
                passes = null,
                showPasses = false,
                selected = preset.isBlank(),
                onClick = { onPresetChange("") },
            )
            val uncategorised = str("renderer.shaderChain.uncategorised")
            families.forEach { family ->
                val familyOpen = expandedFamilies[family.key] ?: false
                ShaderFamilyRow(
                    controllerId = "shaderChain:family:${family.key}",
                    // "" = presets sitting loose at the shaders root; they get a real home
                    // rather than being dropped on the floor.
                    label = family.label.ifEmpty { uncategorised },
                    count = family.count,
                    expanded = familyOpen,
                    holdsActive = activeAt?.first == family.key,
                    onToggle = { expandedFamilies[family.key] = !familyOpen },
                ) {
                    // Sub-folders first, then the family's own loose presets: the folder
                    // rows are compact and navigational, and putting them last would strand
                    // them under a 100-row wall in families like CRT.
                    family.groups.forEach { group ->
                        val groupOpen = expandedGroups[group.key] ?: false
                        ShaderGroupRow(
                            controllerId = "shaderChain:group:${group.key}",
                            label = group.label,
                            count = group.presets.size,
                            expanded = groupOpen,
                            holdsActive = activeAt?.second == group.key,
                            onToggle = { expandedGroups[group.key] = !groupOpen },
                        ) {
                            group.presets.forEach { p -> PresetRow(p, preset, onPresetChange) }
                        }
                    }
                    family.direct.forEach { p -> PresetRow(p, preset, onPresetChange) }
                }
            }
            if (families.isEmpty() && !scanning.value) {
                HelpText(str("renderer.shaderChain.empty") + "\n\n" + scan.value?.dir.orEmpty())
            }
        }
    }
}

/** Picking deliberately does NOT collapse the list: the row would dispose and unregister
 *  itself, and SettingsControllerNav re-points the selection at the FIRST row of the whole
 *  tab — throwing a controller user back to the top of Renderer. Same as
 *  DriverManagerSection: the list stays open and only the chip moves. */
@Composable
private fun PresetRow(p: ShaderPreset, preset: String, onPresetChange: (String) -> Unit) {
    ShaderPresetRow(
        controllerId = "shaderChain:preset:${p.path}",
        label = p.label,
        passes = p.passes,
        selected = p.path == preset,
        onClick = { onPresetChange(p.path) },
    )
}

/**
 * One collapsible shader family — the top level of the picker. Same ▸/▾ affordance and
 * controller-focusable header as [ShaderGroupRow], drawn a step louder because it heads the
 * list rather than sitting in it. Only the expanded family composes its [content], which is
 * what keeps the collapsed picker 40 rows instead of 2542.
 */
@Composable
private fun ShaderFamilyRow(
    controllerId: String,
    label: String,
    count: Int,
    expanded: Boolean,
    holdsActive: Boolean,
    onToggle: () -> Unit,
    content: @Composable () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Surface(
            onClick = onToggle,
            modifier = Modifier
                .fillMaxWidth()
                .controllerFocusable(controllerId, RoundedCornerShape(18.dp), onConfirm = onToggle),
            shape = RoundedCornerShape(18.dp),
            color = MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.92f),
            border = BorderStroke(
                1.dp,
                if (holdsActive) MaterialTheme.colorScheme.primary
                else MaterialTheme.colorScheme.outline.copy(alpha = 0.5f),
            ),
        ) {
            Row(
                Modifier.padding(horizontal = 14.dp, vertical = 12.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(
                    if (expanded) "▾" else "▸",
                    color = MaterialTheme.colorScheme.primary,
                    fontWeight = FontWeight.Bold,
                )
                Spacer(Modifier.width(10.dp))
                Text(
                    label,
                    color = MaterialTheme.colorScheme.onSurface,
                    fontSize = 16.sp,
                    lineHeight = 21.sp,
                    fontWeight = FontWeight.SemiBold,
                    modifier = Modifier.weight(1f),
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                )
                Spacer(Modifier.width(8.dp))
                // Marks the family holding the current preset even while collapsed, so the
                // selection is findable without opening all 39.
                if (holdsActive) {
                    StatusChip(str("backend.driver.active"), Success)
                    Spacer(Modifier.width(8.dp))
                }
                Text(
                    "$count",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        if (expanded) content()
    }
}

/**
 * One collapsible sub-folder inside a family. Mirrors DriverManagerSection's
 * DriverSourceGroup — same shape, same ▸/▾ affordance, same controller-focusable header —
 * rather than calling it: that one is file-private (Kotlin `private` at top level is FILE
 * scope, not package), and this needs the extra active-marker the driver list has no use
 * for. Only the expanded group composes its [content], which is what keeps the collapsed
 * family cheap.
 */
@Composable
private fun ShaderGroupRow(
    controllerId: String,
    label: String,
    count: Int,
    expanded: Boolean,
    holdsActive: Boolean,
    onToggle: () -> Unit,
    /** Chip text for [holdsActive]. Defaults to the preset picker's "Active", which is what
     *  a folder holding the current preset means; the parameter list reuses this row to say
     *  "Modified" instead. */
    activeLabel: String = str("backend.driver.active"),
    content: @Composable () -> Unit,
) {
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Surface(
            onClick = onToggle,
            modifier = Modifier
                .fillMaxWidth()
                .controllerFocusable(controllerId, RoundedCornerShape(14.dp), onConfirm = onToggle),
            shape = RoundedCornerShape(14.dp),
            color = MaterialTheme.colorScheme.surfaceVariant,
            border = BorderStroke(
                1.dp,
                if (holdsActive) MaterialTheme.colorScheme.primary
                else MaterialTheme.colorScheme.outline.copy(alpha = 0.4f),
            ),
        ) {
            Row(
                Modifier.padding(horizontal = 14.dp, vertical = 11.dp),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                Text(if (expanded) "▾" else "▸", color = MaterialTheme.colorScheme.primary)
                Spacer(Modifier.width(10.dp))
                Text(
                    label,
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.weight(1f),
                    maxLines = 1,
                    // Folder paths are long and it's the TAIL that distinguishes them
                    // (…/Base_CRT_Presets vs …/Base_CRT_Presets_DREZ), so drop the head.
                    overflow = TextOverflow.StartEllipsis,
                )
                // Marks the folder holding the current preset even while collapsed, so the
                // selection is findable without opening all of them.
                if (holdsActive) {
                    StatusChip(activeLabel, Success)
                    Spacer(Modifier.width(8.dp))
                }
                Text(
                    "$count",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
        if (expanded) content()
    }
}

@Composable
private fun ShaderPresetRow(
    controllerId: String,
    label: String,
    passes: Int?,
    selected: Boolean,
    onClick: () -> Unit,
    showPasses: Boolean = true,
) {
    Surface(
        onClick = onClick,
        modifier = Modifier
            .fillMaxWidth()
            .controllerFocusable(controllerId, RoundedCornerShape(16.dp), onConfirm = onClick),
        shape = RoundedCornerShape(16.dp),
        color = if (selected) MaterialTheme.colorScheme.primaryContainer
                else MaterialTheme.colorScheme.surfaceVariant,
        border = BorderStroke(
            1.dp,
            if (selected) MaterialTheme.colorScheme.primary
            else MaterialTheme.colorScheme.outline.copy(alpha = 0.45f),
        ),
    ) {
        Row(
            Modifier.padding(13.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Text(
                label,
                color = MaterialTheme.colorScheme.onSurface,
                fontSize = 15.sp,
                lineHeight = 20.sp,
                fontWeight = FontWeight.SemiBold,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                modifier = Modifier.weight(1f),
            )
            // A fact the user can read, not a verdict we assign — see [ShaderPreset]. str()
            // takes no format args (see I18n.kt), so the count is concatenated the same way
            // renderer.shaderPack.presets already does it.
            if (showPasses) {
                Text(
                    when {
                        passes == null -> str("renderer.shaderChain.passesUnknown")
                        passes == 1 -> "$passes " + str("renderer.shaderChain.pass")
                        else -> "$passes " + str("renderer.shaderChain.passes")
                    },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1,
                )
            }
            if (selected) StatusChip(str("backend.driver.active"), Success)
        }
    }
}

/** Scan `<DataRoot>/shaders/` recursively for `*.slangp`. The folder is
 *  [ShaderRepo.shadersRoot] — the same one the Shader Packs downloader extracts into (and
 *  it creates the dir on demand, so the empty state can name a folder the user will
 *  actually find in a file manager). That helper resolves through assetCopyRoot, like the
 *  texture / cache / memcard folders, so this follows a moved data folder.
 *
 *  Blocking I/O — call it off the UI thread. It READS every preset (and follows the
 *  #reference chains between them) rather than just listing names, so the one memo cache is
 *  load-bearing: 691 of the stock pack's 2542 presets are thin wrappers pointing at a
 *  handful of shared roots, and without memoising those roots would be re-parsed hundreds
 *  of times each. */
private fun scanShaderPresets(context: Context): ShaderScan {
    val root = ShaderRepo.shadersRoot(context)
    if (!root.isDirectory) return ShaderScan(root.absolutePath, emptyList())
    val cache = HashMap<String, Int?>()
    val found = root.walkTopDown()
        .filter { it.isFile && it.extension.equals("slangp", ignoreCase = true) }
        .map { file ->
            val dir = file.parentFile?.relativeToOrNull(root)?.invariantSeparatorsPath.orEmpty()
                .let { if (it == ".") "" else it }
            FoundPreset(
                preset = ShaderPreset(
                    label = file.nameWithoutExtension,
                    path = file.absolutePath,
                    passes = resolvePasses(file, cache, HashSet(), 0),
                ),
                segments = if (dir.isEmpty()) emptyList() else dir.split('/'),
            )
        }
        .toList()

    val containers = containerDirs(found)
    val families = found.groupBy { familyKeyOf(it.segments, containers) }
        .map { (key, entries) ->
            val depth = if (key.isEmpty()) 0 else key.count { it == '/' } + 1
            ShaderFamily(
                key = key,
                label = familyLabel(key),
                count = entries.size,
                // Alphabetical, case-insensitive — the pass count is on every row, so
                // findability beats any other ordering, and we deliberately do NOT rank by
                // cost (see [ShaderPreset]). lowercase() rather than a plain lexicographic
                // sort, which would scatter mixed-case names by ASCII (every `Bezel` above
                // every `bezel`); sortedBy is stable, so names differing only in case keep
                // a deterministic order.
                direct = entries.filter { it.segments.size == depth }
                    .map { it.preset }
                    .sortedBy { it.label.lowercase() },
                groups = entries.filter { it.segments.size > depth }
                    .groupBy { it.segments.joinToString("/") }
                    .map { (dirKey, es) ->
                        ShaderGroup(
                            key = dirKey,
                            // Path relative to the family, so "Mega Bezel" isn't repeated
                            // on every folder row inside the Mega Bezel section.
                            label = if (key.isEmpty()) dirKey else dirKey.removePrefix("$key/"),
                            presets = es.map { it.preset }.sortedBy { it.label.lowercase() },
                        )
                    }
                    .sortedBy { it.label.lowercase() },
            )
        }
        // Named families first, the loose-at-the-root bucket last — it's a couple of strays
        // and shouldn't head the list.
        .sortedWith(compareBy({ it.key.isEmpty() }, { it.label.lowercase() }))
    return ShaderScan(root.absolutePath, families)
}

/**
 * Top-level folders that are just CONTAINERS of distinct families, and so should be
 * descended one level instead of becoming a family themselves.
 *
 * Derived, not hardcoded. A container is a folder that (a) holds no presets of its own —
 * everything in it belongs to a sub-pack, (b) has at least two sub-packs to tell apart, and
 * (c) is big enough that those sub-packs are separate works rather than variants of one.
 *
 * All three clauses earn their place against the real tree. `bezel/` passes: 1490 presets,
 * zero loose, four genuinely unrelated packs (Mega Bezel 660, scanline-classic 602,
 * koko-aio 179, uborder 49) that users name individually — exactly the split wanted.
 * `presets/` fails (a): its 18 loose files make it a category that merely also has
 * sub-folders, and descending it would shatter a coherent group into 17 fragments like
 * "tvout" and "fsr". `edge-smoothing/` fails only (c): it has no loose presets and 20
 * sub-folders, but at 113 presets those are sibling variants of one idea (xbr, hqx,
 * scalenx…) and promoting them would bury a coherent category under 20 near-empty
 * sections. Size is the only thing separating those last two cases, which is precisely why
 * [FAMILY_CONTAINER_MIN] exists. On the stock pack this returns exactly {bezel}.
 */
private fun containerDirs(found: List<FoundPreset>): Set<String> =
    found.filter { it.segments.isNotEmpty() }
        .groupBy { it.segments[0] }
        .filterValues { entries ->
            entries.none { it.segments.size == 1 } &&
                entries.mapNotNull { it.segments.getOrNull(1) }.distinct().size >= 2 &&
                entries.size > FAMILY_CONTAINER_MIN
        }
        .keys

/** The family a preset belongs to: its top-level folder, or one level deeper when that
 *  folder is a mere [containerDirs] container. "" = loose at the shaders root.
 *
 *  Keeps the container segment in the key ("bezel/Mega_Bezel", not "Mega_Bezel") so two
 *  containers holding a like-named pack can't collide into one section; [familyLabel]
 *  drops it again for display. */
private fun familyKeyOf(segments: List<String>, containers: Set<String>): String = when {
    segments.isEmpty() -> ""
    segments[0] in containers && segments.size >= 2 -> "${segments[0]}/${segments[1]}"
    else -> segments[0]
}

/**
 * Human display name for a family folder: "Mega_Bezel" → "Mega Bezel", "crt" → "CRT",
 * "edge-smoothing" → "Edge Smoothing", "nes_raw_palette" → "NES Raw Palette".
 *
 * Mechanical (split the leaf on _ and -, title-case, uppercase known initialisms) plus a
 * two-entry override map, rather than a hand-written table of all 39 names: the rule gets
 * every folder in the stock pack right except the two packs whose branding is deliberately
 * lowercase, and unlike a table it still does something sensible for a pack nobody has seen
 * yet. Returns "" for the root bucket, which the caller renders as "Uncategorised".
 *
 * NOT routed through str(): these are folder names read off the user's disk — data, not UI
 * chrome — and the picker already renders sub-folder paths raw for the same reason. Only
 * the fixed label for the root bucket is translated.
 */
private fun familyLabel(key: String): String {
    if (key.isEmpty()) return ""
    val leaf = key.substringAfterLast('/')
    FAMILY_NAME_OVERRIDES[leaf.lowercase()]?.let { return it }
    return leaf.split('_', '-', ' ')
        .filter { it.isNotEmpty() }
        .joinToString(" ") { token ->
            if (token.lowercase() in FAMILY_ACRONYMS) token.uppercase()
            else token.replaceFirstChar { it.uppercase() }
        }
        .ifEmpty { leaf }
}

/** What one `.slangp` says about its own cost: its pass count, or the presets it inherits
 *  one from. Never both in the stock pack — a `#reference` preset overrides parameters
 *  only — but [resolvePasses] prefers a local count anyway, which is the RetroArch rule. */
private class PresetFacts(val shaders: Int?, val references: List<String>)

private fun readPreset(file: File): PresetFacts {
    var shaders: Int? = null
    val references = ArrayList<String>(2)
    try {
        file.bufferedReader().useLines { lines ->
            for (line in lines) {
                val hit = SHADERS_RE.find(line)
                if (hit != null) {
                    shaders = hit.groupValues[1].toIntOrNull()
                    // A local count wins outright, so the references can't change the
                    // answer — stop reading. Bails on line 1 for most of the pack.
                    if (shaders != null) return@useLines
                }
                REFERENCE_RE.find(line)?.let {
                    references.add(it.groupValues[1].trim().trim('"', '\''))
                }
            }
        }
    } catch (_: Exception) {
        // Unreadable / vanished mid-scan: the row says "cost unknown" rather than
        // inventing a number.
    }
    return PresetFacts(shaders, references)
}

/**
 * The preset's pass count, following `#reference` chains. null = undeterminable, which the
 * row reports honestly as "cost unknown".
 *
 * The reference hop is the whole reason this function exists rather than a one-line regex.
 * 691 of the stock pack's 2542 presets — essentially all of Mega Bezel and koko-aio — carry
 * no `shaders` key at all; they are thin files whose entire body is `#reference
 * "../some/root.slangp"` plus parameter overrides. Reading `shaders` naively finds nothing
 * for exactly those files, so without this the headline number would be blank on precisely
 * the biggest chains in the pack.
 *
 * Targets resolve against the REFERENCING file's directory (they are written `../../…`).
 * [seen] is the loop guard (canonical paths, so a symlinked pack can't dodge it); [cache]
 * memoises across the whole scan.
 */
private fun resolvePasses(
    file: File,
    cache: MutableMap<String, Int?>,
    seen: MutableSet<String>,
    depth: Int,
): Int? {
    val canonical = runCatching { file.canonicalPath }.getOrDefault(file.absolutePath)
    if (depth > MAX_REFERENCE_DEPTH || !seen.add(canonical)) return null
    if (cache.containsKey(canonical)) return cache[canonical]
    val facts = readPreset(file)
    val result = facts.shaders ?: facts.references.firstNotNullOfOrNull { ref ->
        val target = File(file.parentFile, ref)
        // Multi-reference presets (324 of them) list the .slangp first and .params after;
        // taking the first target that actually yields a count skips the parameter files
        // without having to sniff extensions.
        if (target.isFile) resolvePasses(target, cache, seen, depth + 1) else null
    }
    cache[canonical] = result
    return result
}
