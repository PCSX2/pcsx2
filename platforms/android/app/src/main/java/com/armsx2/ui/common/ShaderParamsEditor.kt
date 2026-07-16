package com.armsx2.ui.common

import android.content.Context
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.defaultMinSize
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.armsx2.ShaderParam
import com.armsx2.ShaderParams
import com.armsx2.i18n.str
import com.armsx2.ui.home.LibraryKeyboard
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

/** One line in the editor. Actions sit in the same list as the parameters so the cursor
 *  walks them with no special-casing — there's one nav model, not two. */
internal sealed interface ParamLine {
    data object ResetAll : ParamLine
    data object SaveAs : ParamLine
    data class Caption(val label: String) : ParamLine
    data class Param(val param: ShaderParam) : ParamLine
}

/**
 * Full-screen shader-parameter editor, modelled on RetroArch's.
 *
 * Two things drove the design, and both are the opposite of how a settings tab works:
 *
 * 1. SIZE. A preset can declare ~900 parameters (MBZ__3__STD__GDV resolves to 909).
 *    Inline rows would need collapsible groups to stay composable; here a LazyColumn just
 *    handles it. That's only possible because the cursor is an INDEX rather than a
 *    [controllerFocusable] registry entry — the registry only sees COMPOSED rows, which is
 *    why Lazy lists are banned elsewhere. Index nav doesn't care.
 *
 * 2. INPUT. State lives in this OBJECT, not in composition, and the runtime drives it
 *    through [move]/[confirm]/[close]. The first cut of this screen listened for Compose
 *    key events and was dead on arrival: on this hardware the D-pad is a **HAT axis**, not
 *    KEYCODE_DPAD_* (see MainActivityRuntime.handleOverlayControllerMotion), so the keys
 *    never came — the menu behind kept the input instead. Every other frontend surface is
 *    driven from MainActivityRuntime's routing chain for exactly this reason, and now so
 *    is this one. Do NOT reintroduce onPreviewKeyEvent here.
 *
 * Text entry (naming a preset) hands off to [LibraryKeyboard], which already sits ahead of
 * this editor in both routing chains — so while it's up it owns input, and no coordination
 * is needed beyond noticing when it closes.
 */
object ShaderParamsEditor {

    /**
     * What the editor is editing, handed over by whoever opened it.
     *
     * [onChange] persists — it belongs to the caller's settings tier (Global vs per-game),
     * exactly like [ShaderChainSection]'s other callbacks; the editor never picks a tier.
     * The editor owns the authoritative map while open and passes the COMPLETE map every
     * time, so a caller whose lambda closed over a stale Settings snapshot still can't lose
     * a parameter: the only field it writes is the one the editor owns, and nothing else
     * can change under a full-screen modal.
     */
    data class Request(
        val preset: String,
        val onChange: (Map<String, Float>) -> Unit,
        val onPresetSaved: (String) -> Unit,
    )

    val request = mutableStateOf<Request?>(null)

    /** Read from dispatchKeyEvent / fireNavMove — a plain state read outside composition. */
    val visible: Boolean get() = request.value != null

    internal val params = mutableStateOf<List<ShaderParam>?>(null)
    internal val overrides = mutableStateOf<Map<String, Float>>(emptyMap())
    internal val lines = mutableStateOf<List<ParamLine>>(emptyList())
    internal val selected = mutableIntStateOf(0)
    /** Non-null while the destructive reset is waiting to be confirmed; the Int is which
     *  button is highlighted (0 = Cancel, 1 = Reset). An in-layout card rather than an
     *  AlertDialog: a Dialog is its own focused window and would swallow the pad. */
    internal val confirmReset = mutableStateOf<Int?>(null)
    /** True while the keyboard is up FOR US, so its close means "save with this name". */
    internal val saving = mutableStateOf(false)

    /** Indices the cursor may land on — captions are skipped. Derived once per [lines]. */
    private val stops: List<Int> get() = lines.value.indices.filter { lines.value[it] !is ParamLine.Caption }

    fun open(
        preset: String,
        overrides: Map<String, Float>,
        onChange: (Map<String, Float>) -> Unit,
        onPresetSaved: (String) -> Unit,
    ) {
        this.overrides.value = overrides
        params.value = null
        lines.value = emptyList()
        selected.intValue = 0
        confirmReset.value = null
        saving.value = false
        request.value = Request(preset, onChange, onPresetSaved)
    }

    fun close() {
        if (saving.value) LibraryKeyboard.close()
        request.value = null
        params.value = null
        lines.value = emptyList()
        confirmReset.value = null
        saving.value = false
    }

    /** Called by the host once the (IO) enumeration lands. */
    internal fun setParams(list: List<ShaderParam>) {
        params.value = list
        // Actions first and ALWAYS present, so indices don't shift under the cursor when
        // the last override is cleared.
        val built = buildList<ParamLine> {
            add(ParamLine.ResetAll)
            add(ParamLine.SaveAs)
            list.forEach { p ->
                // A parameter is a caption only because it CANNOT be adjusted (max == min)
                // — never because of how its description reads. See ShaderParam.isAdjustable.
                if (p.isAdjustable) add(ParamLine.Param(p)) else add(ParamLine.Caption(captionLabel(p.description)))
            }
        }
        lines.value = built
        // Land on the first real parameter, not on Reset All.
        selected.intValue = built.indexOfFirst { it is ParamLine.Param }.takeIf { it >= 0 } ?: 0
    }

    fun valueOf(p: ShaderParam): Float = overrides.value[p.name] ?: p.initial

    private fun commit(next: Map<String, Float>) {
        val req = request.value ?: return
        overrides.value = next
        req.onChange(next)
        ShaderParams.pushEffective(req.preset, params.value.orEmpty(), next)
    }

    /** Nudge the selected parameter by [dir] of its own step. */
    fun adjust(dir: Int) {
        val line = lines.value.getOrNull(selected.intValue) as? ParamLine.Param ?: return
        val p = line.param
        val next = p.valueAt((p.indexOf(valueOf(p)) + dir).coerceIn(0, p.stepCount))
        // Landing back on the author's default DROPS the override rather than storing a
        // value equal to it, so "changed" always means changed.
        commit(if (p.isInitial(next)) overrides.value - p.name else overrides.value + (p.name to next))
    }

    fun selectIndex(index: Int) {
        if (lines.value.getOrNull(index) !is ParamLine.Caption) selected.intValue = index
    }

    /** Vertical moves the cursor, horizontal adjusts. Driven by the runtime for BOTH the
     *  HAT/stick path and the D-pad-as-keys path, so one implementation serves both. */
    fun move(dx: Int, dy: Int) {
        confirmReset.value?.let { at ->
            // While the confirm card is up it owns the cursor.
            if (dx != 0) confirmReset.value = (at + dx).coerceIn(0, 1)
            return
        }
        if (dy != 0) {
            val order = stops
            if (order.isEmpty()) return
            val at = order.indexOf(selected.intValue).takeIf { it >= 0 } ?: 0
            selected.intValue = order[(at + dy).coerceIn(0, order.size - 1)]
        } else if (dx != 0) {
            adjust(dx)
        }
    }

    fun confirm() {
        confirmReset.value?.let { at ->
            if (at == 1) resetAll()
            confirmReset.value = null
            return
        }
        when (val line = lines.value.getOrNull(selected.intValue)) {
            // Confirm on a value row resets it. Confirm has nothing else to do there
            // (there's no "open" on a value), and reset is what people reach for.
            is ParamLine.Param -> commit(overrides.value - line.param.name)
            is ParamLine.ResetAll -> if (overrides.value.isNotEmpty()) confirmReset.value = 0
            is ParamLine.SaveAs -> beginSave()
            else -> {}
        }
    }

    /** B / Back: back out of the confirm card first, then the editor. */
    fun back(): Boolean {
        if (confirmReset.value != null) { confirmReset.value = null; return true }
        close()
        return true
    }

    fun resetAll() = commit(emptyMap())

    internal fun beginSave() {
        if (overrides.value.isEmpty()) return
        saving.value = true
        // The keyboard is ahead of us in the routing chain, so it owns input from here
        // until it closes — which the host watches for to perform the save.
        LibraryKeyboard.open("", onChange = {}, placeholder = str_presetName())
    }

    /** i18n lookup from a non-composable — [str] is @Composable, so the placeholder is
     *  resolved through the same map directly. */
    private fun str_presetName(): String =
        com.armsx2.i18n.I18n.get("renderer.shaderChain.params.namePlaceholder")

}

/**
 * Host — place once, above everything (see WindowImpl). Renders nothing until something
 * calls [ShaderParamsEditor.open].
 */
@Composable
fun ShaderParamsEditorHost() {
    val req = ShaderParamsEditor.request.value ?: return
    val context = LocalContext.current
    val listState = rememberLazyListState()
    val lines = ShaderParamsEditor.lines.value
    val selected = ShaderParamsEditor.selected.intValue

    // librashader parses the whole chain to answer this, so it rides an IO hop.
    LaunchedEffect(req.preset) {
        val list = withContext(Dispatchers.IO) { ShaderParams.read(req.preset) }
        ShaderParamsEditor.setParams(list)
    }

    // The keyboard closing is the "done" signal for a save — it has no callback of its own.
    LaunchedEffect(LibraryKeyboard.visible.value) {
        if (ShaderParamsEditor.saving.value && !LibraryKeyboard.visible.value) {
            ShaderParamsEditor.saving.value = false
            val name = LibraryKeyboard.text.value.trim()
            if (name.isNotEmpty()) {
                val path = withContext(Dispatchers.IO) {
                    ShaderParams.savePreset(context, name, req.preset, ShaderParamsEditor.overrides.value)
                }
                if (path != null) {
                    // Select the saved preset and get out: the editor is now showing the
                    // wrong preset's parameters, and the saved one's ARE these values.
                    req.onPresetSaved(path)
                    ShaderParamsEditor.close()
                }
            }
        }
    }

    // Keep the cursor on screen, a few rows in from the edge.
    LaunchedEffect(selected) {
        runCatching { listState.animateScrollToItem((selected - 3).coerceAtLeast(0)) }
    }

    BackHandler(enabled = true) { ShaderParamsEditor.back() }

    Box(
        Modifier
            .fillMaxSize()
            // Not fully opaque: the point of tweaking a shader is watching the game change
            // while you do it, so the frame stays visible behind the list.
            .background(MaterialTheme.colorScheme.background.copy(alpha = 0.82f))
            // Swallow taps so nothing behind reacts. No focusable/onPreviewKeyEvent — keys
            // arrive through MainActivityRuntime's routing chain (see the object's KDoc).
            .clickable(enabled = false) {},
    ) {
        Column(Modifier.fillMaxSize()) {
            Header(preset = req.preset)
            Box(
                Modifier
                    .fillMaxWidth()
                    .height(1.dp)
                    .background(MaterialTheme.colorScheme.outline.copy(alpha = 0.4f)),
            )

            when {
                ShaderParamsEditor.params.value == null -> Notice(str("renderer.shaderChain.params.loading"))
                lines.none { it is ParamLine.Param } -> Notice(str("renderer.shaderChain.params.empty"))
                else -> {
                    LazyColumn(
                        state = listState,
                        modifier = Modifier.weight(1f).fillMaxWidth(),
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 8.dp),
                    ) {
                        items(lines.size) { index ->
                            when (val line = lines[index]) {
                                is ParamLine.Caption -> CaptionRow(line.label)
                                is ParamLine.ResetAll -> ActionRow(
                                    label = str("renderer.shaderChain.params.resetAll"),
                                    enabled = ShaderParamsEditor.overrides.value.isNotEmpty(),
                                    selected = index == selected,
                                    onClick = {
                                        ShaderParamsEditor.selectIndex(index)
                                        ShaderParamsEditor.confirm()
                                    },
                                )
                                is ParamLine.SaveAs -> ActionRow(
                                    label = str("renderer.shaderChain.params.saveAs"),
                                    enabled = ShaderParamsEditor.overrides.value.isNotEmpty(),
                                    selected = index == selected,
                                    onClick = {
                                        ShaderParamsEditor.selectIndex(index)
                                        ShaderParamsEditor.confirm()
                                    },
                                )
                                is ParamLine.Param -> ParamRow(
                                    param = line.param,
                                    value = ShaderParamsEditor.valueOf(line.param),
                                    modified = ShaderParamsEditor.overrides.value.containsKey(line.param.name),
                                    selected = index == selected,
                                    onSelect = { ShaderParamsEditor.selectIndex(index) },
                                    onAdjust = { dir ->
                                        // Touch: select the row first so the − / + act on
                                        // the row you actually tapped.
                                        ShaderParamsEditor.selectIndex(index)
                                        ShaderParamsEditor.adjust(dir)
                                    },
                                )
                            }
                        }
                    }
                    FooterHints()
                }
            }
        }

        ShaderParamsEditor.confirmReset.value?.let { at -> ResetConfirmCard(at) }
        // Its own copy: the only other host is the library screen, which isn't up here.
        LibraryKeyboard.Overlay(this)
    }
}

@Composable
private fun Header(preset: String) {
    Row(
        Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 14.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f)) {
            Text(
                str("renderer.shaderChain.params.label"),
                color = MaterialTheme.colorScheme.onSurface,
                fontSize = 24.sp,
                fontWeight = FontWeight.Bold,
            )
            Text(
                File(preset).nameWithoutExtension,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 13.sp,
                maxLines = 1,
                overflow = TextOverflow.StartEllipsis,
            )
        }
        Text(
            str("action.close"),
            color = MaterialTheme.colorScheme.primary,
            fontSize = 15.sp,
            fontWeight = FontWeight.SemiBold,
            modifier = Modifier
                .clip(RoundedCornerShape(10.dp))
                .clickable { ShaderParamsEditor.close() }
                .padding(horizontal = 12.dp, vertical = 8.dp),
        )
    }
}

@Composable
private fun Notice(text: String) {
    Text(
        text,
        color = MaterialTheme.colorScheme.onSurfaceVariant,
        fontSize = 15.sp,
        modifier = Modifier.padding(20.dp),
    )
}

@Composable
private fun CaptionRow(label: String) {
    Text(
        label,
        color = MaterialTheme.colorScheme.primary.copy(alpha = 0.85f),
        fontSize = 13.sp,
        fontWeight = FontWeight.Bold,
        modifier = Modifier.padding(start = 8.dp, top = 14.dp, bottom = 4.dp),
    )
}

@Composable
private fun ActionRow(label: String, enabled: Boolean, selected: Boolean, onClick: () -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .defaultMinSize(minHeight = 44.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(
                if (selected) MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.9f)
                else Color.Transparent
            )
            .border(
                width = if (selected) 1.dp else 0.dp,
                color = if (selected) MaterialTheme.colorScheme.primary else Color.Transparent,
                shape = RoundedCornerShape(8.dp),
            )
            .clickable(enabled = enabled) { onClick() }
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            label,
            // Greyed rather than hidden: a row that appears only once you've changed
            // something would shift every index under the cursor.
            color = if (enabled) MaterialTheme.colorScheme.primary
            else MaterialTheme.colorScheme.onSurfaceVariant.copy(alpha = 0.45f),
            fontSize = 16.sp,
            fontWeight = FontWeight.SemiBold,
        )
    }
}

@Composable
private fun ParamRow(
    param: ShaderParam,
    value: Float,
    modified: Boolean,
    selected: Boolean,
    onSelect: () -> Unit,
    onAdjust: (Int) -> Unit,
) {
    Row(
        Modifier
            .fillMaxWidth()
            .defaultMinSize(minHeight = 44.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(
                if (selected) MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.9f)
                else Color.Transparent
            )
            .border(
                width = if (selected) 1.dp else 0.dp,
                color = if (selected) MaterialTheme.colorScheme.primary else Color.Transparent,
                shape = RoundedCornerShape(8.dp),
            )
            .clickable { onSelect() }
            .padding(start = 12.dp, end = 4.dp, top = 4.dp, bottom = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Text(
            param.description,
            color = MaterialTheme.colorScheme.onSurface,
            fontSize = 16.sp,
            maxLines = 1,
            overflow = TextOverflow.Ellipsis,
            modifier = Modifier.weight(1f),
        )
        Spacer(Modifier.width(8.dp))
        Text(
            // Value first, then the range it lives in — RetroArch's shape, and the range
            // is what tells you how much room is left.
            "${param.format(value)}  [${param.format(param.minimum)} ${param.format(param.maximum)}]",
            // A changed value is called out, so a long list still shows what you touched.
            color = if (modified) MaterialTheme.colorScheme.primary
            else MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 15.sp,
            fontWeight = if (modified) FontWeight.Bold else FontWeight.Normal,
        )
        // Touch has no ←→. Without these the whole screen is read-only on a touchscreen,
        // which is how the first cut shipped.
        Spacer(Modifier.width(6.dp))
        StepButton("−") { onAdjust(-1) }
        StepButton("+") { onAdjust(1) }
    }
}

@Composable
private fun StepButton(label: String, onClick: () -> Unit) {
    Box(
        Modifier
            .size(36.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(MaterialTheme.colorScheme.primary.copy(alpha = 0.18f))
            .clickable { onClick() },
        contentAlignment = Alignment.Center,
    ) {
        Text(label, color = MaterialTheme.colorScheme.primary, fontSize = 18.sp, fontWeight = FontWeight.Bold)
    }
}

/** In-layout confirm for the one destructive action. NOT an AlertDialog — that's its own
 *  focused window and would swallow the pad this screen runs on. Nav is the editor's:
 *  ←→ picks a button, A activates, B cancels (see [ShaderParamsEditor.move]/[confirm]). */
@Composable
private fun androidx.compose.foundation.layout.BoxScope.ResetConfirmCard(at: Int) {
    Box(
        Modifier
            .fillMaxSize()
            .background(Color.Black.copy(alpha = 0.66f))
            .clickable(enabled = false) {},
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier
                .padding(24.dp)
                .clip(RoundedCornerShape(16.dp))
                .background(MaterialTheme.colorScheme.surfaceVariant)
                .border(1.dp, MaterialTheme.colorScheme.outline.copy(alpha = 0.5f), RoundedCornerShape(16.dp))
                .padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Text(
                str("renderer.shaderChain.params.resetAll.confirmTitle"),
                color = MaterialTheme.colorScheme.onSurface,
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                textAlign = TextAlign.Center,
            )
            Spacer(Modifier.height(8.dp))
            Text(
                str("renderer.shaderChain.params.resetAll.confirmBody"),
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                fontSize = 14.sp,
                textAlign = TextAlign.Center,
            )
            Spacer(Modifier.height(18.dp))
            Row(horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                ConfirmButton(
                    label = str("action.cancel"),
                    highlighted = at == 0,
                    destructive = false,
                ) { ShaderParamsEditor.confirmReset.value = null }
                ConfirmButton(
                    label = str("action.reset"),
                    highlighted = at == 1,
                    destructive = true,
                ) {
                    ShaderParamsEditor.resetAll()
                    ShaderParamsEditor.confirmReset.value = null
                }
            }
        }
    }
}

@Composable
private fun ConfirmButton(label: String, highlighted: Boolean, destructive: Boolean, onClick: () -> Unit) {
    val tint = if (destructive) Color(0xFFFF6B6B) else MaterialTheme.colorScheme.primary
    Box(
        Modifier
            .clip(RoundedCornerShape(10.dp))
            .background(if (highlighted) tint.copy(alpha = 0.24f) else Color.Transparent)
            .border(1.dp, if (highlighted) tint else tint.copy(alpha = 0.4f), RoundedCornerShape(10.dp))
            .clickable { onClick() }
            .padding(horizontal = 22.dp, vertical = 11.dp),
    ) {
        Text(label, color = tint, fontSize = 15.sp, fontWeight = FontWeight.SemiBold)
    }
}

@Composable
private fun FooterHints() {
    Row(
        Modifier
            .fillMaxWidth()
            .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.55f))
            .padding(horizontal = 20.dp, vertical = 10.dp),
    ) {
        Text(
            str("renderer.shaderChain.params.hint"),
            color = MaterialTheme.colorScheme.onSurfaceVariant,
            fontSize = 13.sp,
        )
    }
}
