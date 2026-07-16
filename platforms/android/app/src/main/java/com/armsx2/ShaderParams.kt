package com.armsx2

import android.content.Context
import android.util.Log
import kr.co.iefriends.pcsx2.NativeApp
import org.json.JSONArray
import java.io.File
import java.util.Locale
import kotlin.math.abs
import kotlin.math.roundToInt

/**
 * One tweakable value a `.slangp` preset declares — a `#pragma parameter` line, as
 * librashader resolves it across the preset's whole chain.
 *
 * The numbers come straight from the shader author and nothing sanitises them, so treat
 * every field as hostile: [step] is routinely 0, and [minimum] can equal [maximum]. Use
 * [stepCount] / [indexOf] / [valueAt] rather than doing the arithmetic at the call site —
 * they are where those two cases are handled once.
 */
data class ShaderParam(
    val name: String,
    val description: String,
    val initial: Float,
    val minimum: Float,
    val maximum: Float,
    val step: Float,
) {
    /** False when the author left no room to move — `max == min`. Such a parameter is not a
     *  control at all: packs declare them purely as captions ("[ BLACK TINT ]:") to break
     *  up RetroArch's flat parameter list. Nothing is lost by rendering one as a caption,
     *  because there is literally no other value it could take.
     *
     *  This is deliberately the ONLY thing we infer about a parameter's role, and it is a
     *  fact rather than a guess. Do NOT "improve" this by reading the description: real
     *  sliders are named "[ Adaptive Strobe (≈BFI) Strength: ]" and "[IMG] Contrast
     *  (squared) [luma]" in the stock pack, so any bracket-matching rule silently HIDES
     *  working controls. A miscaptioned row is cosmetic; a hidden control is a bug. */
    val isAdjustable: Boolean get() = maximum > minimum

    /** Number of discrete positions between [minimum] and [maximum], as a slider index
     *  range of `0..stepCount`. A zero/absent//nonsense [step] falls back to 100 buckets
     *  across the range — RetroArch treats step 0 as "no increment defined" too, and a
     *  slider has to pick something. Capped so a pathological range (0..1000 step 0.0001)
     *  can't hand the D-pad ten million stops. */
    val stepCount: Int
        get() {
            if (!isAdjustable) return 0
            val span = maximum - minimum
            val increment = if (step > 0f && step <= span) step else span / 100f
            return (span / increment).roundToInt().coerceIn(1, 10_000)
        }

    /** Slider index for [value], clamped into range. */
    fun indexOf(value: Float): Int {
        if (!isAdjustable) return 0
        val frac = (value - minimum) / (maximum - minimum)
        return (frac * stepCount).roundToInt().coerceIn(0, stepCount)
    }

    /** The value at slider index [index]. Snaps to [maximum] at the top rather than letting
     *  rounding land a hair short of it. */
    fun valueAt(index: Int): Float {
        if (!isAdjustable) return minimum
        if (index >= stepCount) return maximum
        return minimum + (maximum - minimum) * (index.toFloat() / stepCount.toFloat())
    }

    /** Formats [value] with just enough decimals to distinguish neighbouring steps — a
     *  0..1 step-1 toggle reads "0" / "1", not "0.000". */
    fun format(value: Float): String {
        val span = maximum - minimum
        val increment = if (step > 0f && step <= span) step else span / 100f
        val decimals = when {
            increment >= 1f -> 0
            increment >= 0.1f -> 1
            increment >= 0.01f -> 2
            else -> 3
        }
        return String.format("%.${decimals}f", value)
    }

    /** True when [value] is the author's default, to within half a step — the reset row
     *  and the "modified" mark both key off this rather than on exact float equality,
     *  which round-tripping through a slider index would fail. */
    fun isInitial(value: Float): Boolean {
        val span = maximum - minimum
        val increment = if (step > 0f && step <= span) step else span / 100f
        return abs(value - initial) < (increment / 2f).coerceAtLeast(1e-6f)
    }
}

/**
 * Reads a preset's parameters and hands tweaked values to the GS thread.
 *
 * Two directions, and they are NOT symmetric:
 *  - [read] is pure file parsing (librashader loads its own copy of the preset), so it
 *    needs no VM and no renderer — but it is disk work, so it belongs on an IO thread.
 *  - [push] only queues values; the GS thread applies them on its next frame. It is
 *    deliberately the ONLY way values reach a chain, because a librashader chain is
 *    single-threaded and calling into it from the UI thread corrupts it.
 */
object ShaderParams {

    private const val TAG = "ShaderParams"

    /** The parameters [presetPath] declares, in the order the author declared them, or an
     *  empty list when the preset can't be read (no librashader in this build, a preset
     *  that fails to parse, or one that simply declares none).
     *
     *  Order is load-bearing and must be preserved: it is the only structure a preset
     *  gives us — captions ([ShaderParam.isAdjustable] == false) introduce the run of
     *  parameters that follows them, so sorting this list would strand every caption away
     *  from what it captions.
     *
     *  Blocking disk + parse work. Call from [kotlinx.coroutines.Dispatchers.IO]. */
    fun read(presetPath: String): List<ShaderParam> {
        if (presetPath.isBlank()) return emptyList()

        val json = try {
            NativeApp.shaderPresetParams(presetPath)
        } catch (t: Throwable) {
            Log.w(TAG, "shaderPresetParams threw for $presetPath", t)
            null
        } ?: return emptyList()

        return try {
            val array = JSONArray(json)
            buildList(array.length()) {
                for (i in 0 until array.length()) {
                    val o = array.optJSONObject(i) ?: continue
                    val name = o.optString("name")
                    if (name.isEmpty()) continue
                    add(
                        ShaderParam(
                            name = name,
                            // Authors leave this blank often enough that the UI falls back
                            // to the raw parameter name rather than showing an empty row.
                            description = o.optString("description").ifBlank { name },
                            initial = o.optDouble("initial", 0.0).toFloat(),
                            minimum = o.optDouble("minimum", 0.0).toFloat(),
                            maximum = o.optDouble("maximum", 0.0).toFloat(),
                            step = o.optDouble("step", 0.0).toFloat(),
                        )
                    )
                }
            }
        } catch (t: Throwable) {
            Log.w(TAG, "malformed parameter JSON for $presetPath", t)
            emptyList()
        }
    }

    /** Queues [values] for [presetPath]'s chain; the GS thread picks them up next frame.
     *
     *  [values] is a set of assignments, not the chain's full state — a parameter left out
     *  keeps whatever the chain has. A fresh chain starts at the preset's own initials, so
     *  pushing just the overrides is correct at boot; RESETTING a parameter on a LIVE
     *  chain is not, and must push the initial value explicitly. [pushEffective] is the
     *  call that gets this right, and is what the UI should use. */
    fun push(presetPath: String, values: Map<String, Float>) {
        if (presetPath.isBlank()) return
        val names = values.keys.toTypedArray()
        val floats = FloatArray(names.size) { values.getValue(names[it]) }
        try {
            NativeApp.setShaderChainParams(presetPath, names, floats)
        } catch (t: Throwable) {
            Log.w(TAG, "setShaderChainParams failed for $presetPath", t)
        }
    }

    /** Pushes the effective value of EVERY parameter in [params] — the override if there is
     *  one, otherwise the author's initial.
     *
     *  Sending the initials explicitly is what makes reset work on a live chain: dropping a
     *  parameter from the map would leave the chain sitting on the last value we set,
     *  because librashader has no "unset this parameter" call. Costs one float per
     *  parameter (~900 at the worst end of the stock pack) and only runs when the user
     *  moves something, so it is not worth being clever about. */
    fun pushEffective(presetPath: String, params: List<ShaderParam>, overrides: Map<String, Float>) {
        if (presetPath.isBlank()) return
        push(presetPath, params.associate { it.name to (overrides[it.name] ?: it.initial) })
    }

    // ---- Saving a tweaked preset -------------------------------------------

    /** Characters a preset filename may keep. Everything else becomes '_' — this ends
     *  up as a real file, and a name is whatever the user typed. */
    private val UNSAFE_NAME = Regex("[^A-Za-z0-9 _.-]")

    /** Write [overrides] over [basePreset] as a new preset named [name], returning its
     *  path, or null if it couldn't be written.
     *
     *  The file is a RetroArch "simple preset" — a `#reference` to the base plus the
     *  changed parameters — which is the same form the stock pack ships by the hundred
     *  (everything under `hdr/sony_megatron_v2_presets` is exactly this), so librashader
     *  loads it with no special handling and the preset picker lists it like any other.
     *  That's why saving needs no .slangp WRITER: we are not serialising a shader
     *  chain, we are naming a delta on one that already exists.
     *
     *  Two consequences worth knowing:
     *   - reading the saved preset back reports the overridden values as each
     *     parameter's INITIAL, so its Reset means "back to what I saved";
     *   - deleting or moving the base pack breaks the saved preset, because the
     *     reference is a path, not a copy.
     *
     *  Blocking IO. Call from [kotlinx.coroutines.Dispatchers.IO]. */
    fun savePreset(
        context: Context,
        name: String,
        basePreset: String,
        overrides: Map<String, Float>,
    ): String? {
        val safe = name.trim().replace(UNSAFE_NAME, "_")
        if (safe.isEmpty() || basePreset.isBlank()) return null

        return try {
            val dir = ShaderRepo.userPresetDir(context)
            val out = File(dir, "$safe.slangp")
            val base = File(basePreset)

            // Relative where possible so the pair survives the data folder moving (a
            // real scenario — it's why input profiles are portable too). relativeTo
            // throws when there's no relative route (different volume); absolute is the
            // honest fallback there.
            val reference = runCatching { base.relativeTo(dir).invariantSeparatorsPath }
                .getOrDefault(base.absolutePath)

            val text = buildString {
                append("#reference \"").append(reference).append("\"\n\n")
                // Locale.US is not cosmetic: a locale with a comma decimal separator
                // would emit `1,5` and the preset parser would read it as garbage.
                overrides.forEach { (param, value) ->
                    append(param).append(" = \"")
                        .append(String.format(Locale.US, "%.6f", value)).append("\"\n")
                }
            }
            out.writeText(text)
            out.absolutePath
        } catch (t: Throwable) {
            Log.w(TAG, "could not save preset '$name'", t)
            null
        }
    }

    /** Saved presets, newest-looking first (name order). Blocking IO. */
    fun listSavedPresets(context: Context): List<File> =
        runCatching {
            ShaderRepo.userPresetDir(context)
                .listFiles { f -> f.isFile && f.extension.equals("slangp", ignoreCase = true) }
                ?.sortedBy { it.name.lowercase() }
                .orEmpty()
        }.getOrDefault(emptyList())

    /** True when [presetPath] is one of the user's own saved presets — the UI offers
     *  Delete only for those, never for a pack's. */
    fun isSavedPreset(context: Context, presetPath: String): Boolean =
        presetPath.isNotBlank() &&
            runCatching {
                File(presetPath).parentFile?.canonicalFile == ShaderRepo.userPresetDir(context).canonicalFile
            }.getOrDefault(false)

    fun deleteSavedPreset(context: Context, presetPath: String): Boolean =
        isSavedPreset(context, presetPath) && runCatching { File(presetPath).delete() }.getOrDefault(false)
}
