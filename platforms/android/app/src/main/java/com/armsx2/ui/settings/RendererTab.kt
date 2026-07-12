package com.armsx2.ui.settings

import android.content.Context
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts.OpenDocument
import androidx.activity.result.contract.ActivityResultContracts.OpenDocumentTree
import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.documentfile.provider.DocumentFile
import com.armsx2.Main
import com.armsx2.config.Settings
import com.armsx2.i18n.I18n
import com.armsx2.i18n.str
import com.armsx2.ui.Colors
import com.armsx2.ui.InGameOverlay
import kr.co.iefriends.pcsx2.NativeApp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.BufferedInputStream
import java.io.File
import java.util.zip.ZipInputStream
import kotlin.math.abs

/**
 * Renderer section of the in-game settings overlay.
 *
 * Most fields write into [Settings] via [InGameOverlay.saveSettings],
 * which honors the overlay's scope toggle (Global / Game). Upscale is
 * the one outlier — it has its own dedicated `Main.upscale` state that's
 * also consumed by `Main.applyRendererPrefs` and the setup wizard. Upscale
 * uses a narrow native GS helper so it can visibly apply while a game is live
 * without running the full settings commit path.
 */
private data class UpscaleOption(val value: Float, val label: String)

private val UPSCALE_OPTIONS = listOf(
    // Sub-native (issue #207) — fewer pixels = big perf win on low/mid devices,
    // at the cost of sharpness. The GS only clamps the upper bound, so these are
    // applied as-is.
    UpscaleOption(0.25f, "0.25x"),
    UpscaleOption(0.5f, "0.5x"),
    UpscaleOption(0.75f, "0.75x"),
    UpscaleOption(1.0f, "Native"),
    UpscaleOption(1.25f, "1.25x"),
    UpscaleOption(1.5f, "1.5x"),
    UpscaleOption(1.75f, "1.75x"),
    UpscaleOption(2.0f, "2x"),
    UpscaleOption(2.25f, "2.25x"),
    UpscaleOption(2.5f, "2.5x"),
    UpscaleOption(2.75f, "2.75x"),
    UpscaleOption(3.0f, "3x"),
    UpscaleOption(3.5f, "3.5x"),
    UpscaleOption(4.0f, "4x"),
    UpscaleOption(5.0f, "5x"),
    UpscaleOption(6.0f, "6x"),
    UpscaleOption(7.0f, "7x"),
    UpscaleOption(8.0f, "8x"),
)

@Composable
fun RendererTab(state: MutableState<Settings>) {
    val s = state.value
    val scroll = remember { ScrollState(0) }
    ControllerAutoScroll(scroll)

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .verticalScroll(scroll)
            .verticalScrollbar(scroll),
    ) {
        CollapsibleSection(str("renderer.section.displayResolution"), initiallyExpanded = false) {
            // Graphics API (OpenGL / Vulkan) + Vulkan custom-driver picker. Ported
            // from the removed first-run setup renderer page into settings.
            RendererBackendSection(state)
            SettingsDivider()
            val upscaleIndex = UPSCALE_OPTIONS
                .indexOfFirst { abs(it.value - s.upscaleFloat) < 0.01f }
                .takeIf { it >= 0 } ?: 0
            SegmentedGridRow(
                label = str("renderer.upscale.label"),
                options = UPSCALE_OPTIONS.map { it.label },
                selectedIndex = upscaleIndex,
                columns = 4,
                description = str("renderer.upscale.description"),
                onChange = { index ->
                    val mult = UPSCALE_OPTIONS[index].value
                    // Persist scope-aware (per-game when the overlay scope is Game);
                    // the live GS apply happens in InGameOverlay's settings delta.
                    if (abs(s.upscaleFloat - mult) >= 0.01f) apply(s.copy(upscaleFloat = mult))
                },
            )
            SettingsDivider()
            SegmentedRow(
                label = str("renderer.displayMode.label"),
                options = listOf("Stretch", "Auto", "4:3", "16:9", "10:7"),
                selectedIndex = s.aspectRatio.coerceIn(0, 4),
                description = str("renderer.displayMode.description"),
                onChange = { apply(s.copy(aspectRatio = it)) },
            )
            SettingsDivider()
            // FMV Aspect Ratio override — applies only during FMVs/cutscenes; "Off" keeps
            // the aspect above. Handy for games that render FMVs at a different ratio.
            SegmentedRow(
                label = str("renderer.fmvAspect.label"),
                options = listOf("Off", "Auto", "4:3", "16:9", "10:7"),
                selectedIndex = s.fmvAspectRatio.coerceIn(0, 4),
                description = str("renderer.fmvAspect.description"),
                onChange = { apply(s.copy(fmvAspectRatio = it)) },
            )
            SettingsDivider()
            // Emulation Screen Orientation — global (Android activity orientation), stored
            // in prefs and applied via Main; not an emucore/per-game setting.
            val orientation = remember { mutableStateOf(com.armsx2.Main.prefs.getInt("ui.orientation", 0)) }
            SegmentedRow(
                label = str("renderer.orientation.label"),
                options = listOf(
                    str("renderer.orientation.device"),
                    str("renderer.orientation.landscape"),
                    str("renderer.orientation.portrait"),
                    str("renderer.orientation.autoRotate"),
                ),
                selectedIndex = orientation.value.coerceIn(0, 3),
                description = str("renderer.orientation.description"),
                onChange = {
                    orientation.value = it
                    com.armsx2.Main.prefs.edit().putInt("ui.orientation", it).apply()
                    com.armsx2.Main.instance?.applyEmulationOrientation()
                },
            )
            SettingsDivider()
            SegmentedGridRow(
                label = str("renderer.deinterlacing.label"),
                options = listOf(
                    "Auto", "Off", "Weave TFF", "Weave BFF", "Bob TFF",
                    "Bob BFF", "Blend TFF", "Blend BFF", "Adapt TFF", "Adapt BFF",
                ),
                selectedIndex = s.deinterlaceMode.coerceIn(0, 9),
                columns = 5,
                description = str("renderer.deinterlacing.description"),
                onChange = { apply(s.copy(deinterlaceMode = it)) },
            )
        }
        SettingsDivider()
        CollapsibleSection(str("renderer.section.texturesFiltering")) {
            SegmentedRow(
                label = str("renderer.textureFiltering.label"),
                options = listOf("Nearest", "Forced", "PS2", "Sprite"),
                selectedIndex = s.textureFiltering.coerceIn(0, 3),
                description = str("renderer.textureFiltering.description"),
                onChange = { apply(s.copy(textureFiltering = it)) },
            )
            SettingsDivider()
            SegmentedRow(
                label = str("renderer.texturePreloading.label"),
                options = listOf("Off", "Partial", "Full"),
                selectedIndex = s.texturePreloading.coerceIn(0, 2),
                description = str("renderer.texturePreloading.description"),
                onChange = { apply(s.copy(texturePreloading = it)) },
            )
            SettingsDivider()
            SegmentedGridRow(
                label = str("renderer.hardwareDownloadMode.label"),
                options = listOf("Accurate", "Force Full", "No Readbacks", "Unsync", "Disabled"),
                selectedIndex = s.hardwareDownloadMode.coerceIn(0, 4),
                columns = 3,
                description = str("renderer.hardwareDownloadMode.description"),
                onChange = { apply(s.copy(hardwareDownloadMode = it)) },
            )
        }
        SettingsDivider()
        CollapsibleSection(str("renderer.section.displayEffects")) {
            SegmentedRow(
                label = str("renderer.displayFilter.label"),
                options = listOf("Nearest", "Smooth", "Sharp"),
                selectedIndex = s.displayBilinear.coerceIn(0, 2),
                description = str("renderer.displayFilter.description"),
                onChange = { apply(s.copy(displayBilinear = it)) },
            )
            SettingsDivider()
            SegmentedGridRow(
                label = str("renderer.tvShader.label"),
                options = listOf("Off", "Scanline", "Diagonal", "Tri", "Wave", "Lottes", "4xRGSS", "NxAGSS"),
                selectedIndex = s.tvShader.coerceIn(0, 7),
                columns = 4,
                description = str("renderer.tvShader.description"),
                onChange = { apply(s.copy(tvShader = it)) },
            )
            SettingsDivider()
            ToggleRow(
                "VSync",
                s.vsyncEnable,
                description = str("renderer.vsync.description"),
            ) {
                apply(s.copy(vsyncEnable = it))
            }
            SettingsDivider()
            ToggleRow(
                str("renderer.shadeboost.label"),
                s.shadeBoost,
                description = str("renderer.shadeboost.description"),
            ) {
                apply(s.copy(shadeBoost = it))
            }
            if (s.shadeBoost) {
                SettingsDivider()
                IntSliderRow(
                    label = str("renderer.brightness.label"),
                    value = s.shadeBoostBrightness.coerceIn(1, 100),
                    min = 1,
                    max = 100,
                    description = str("renderer.shadeboost.fiftyIsNormal"),
                    valueFormatter = { "$it%" },
                    onChange = { apply(s.copy(shadeBoostBrightness = it)) },
                )
                SettingsDivider()
                IntSliderRow(
                    label = str("renderer.contrast.label"),
                    value = s.shadeBoostContrast.coerceIn(1, 100),
                    min = 1,
                    max = 100,
                    description = str("renderer.shadeboost.fiftyIsNormal"),
                    valueFormatter = { "$it%" },
                    onChange = { apply(s.copy(shadeBoostContrast = it)) },
                )
                SettingsDivider()
                IntSliderRow(
                    label = str("renderer.saturation.label"),
                    value = s.shadeBoostSaturation.coerceIn(1, 100),
                    min = 1,
                    max = 100,
                    description = str("renderer.shadeboost.fiftyIsNormal"),
                    valueFormatter = { "$it%" },
                    onChange = { apply(s.copy(shadeBoostSaturation = it)) },
                )
                SettingsDivider()
                IntSliderRow(
                    label = str("renderer.gamma.label"),
                    value = s.shadeBoostGamma.coerceIn(1, 100),
                    min = 1,
                    max = 100,
                    description = str("renderer.shadeboost.fiftyIsNormal"),
                    valueFormatter = { "$it%" },
                    onChange = { apply(s.copy(shadeBoostGamma = it)) },
                )
            }
        }
        SettingsDivider()
        CollapsibleSection(str("renderer.section.texturePacks")) {
            ToggleRow(
                str("renderer.loadTexturePacks.label"),
                s.loadTextureReplacements,
                description = str("renderer.loadTexturePacks.description"),
            ) {
                apply(s.copy(loadTextureReplacements = it))
            }
            SettingsDivider()
            ToggleRow(
                str("renderer.asyncTextureLoading.label"),
                s.loadTextureReplacementsAsync,
                description = str("renderer.asyncTextureLoading.description"),
            ) {
                apply(s.copy(loadTextureReplacementsAsync = it))
            }
            SettingsDivider()
            ToggleRow(
                str("renderer.precacheTexturePacks.label"),
                s.precacheTextureReplacements,
                description = str("renderer.precacheTexturePacks.description"),
            ) {
                apply(s.copy(precacheTextureReplacements = it))
            }
            SettingsDivider()
            TexturePackImportRow()
            SettingsDivider()
            GsDumpCaptureRow()
            SettingsDivider()
            ToggleRow(
                str("renderer.dumpReplaceableTextures.label"),
                s.dumpReplaceableTextures,
                description = str("renderer.dumpReplaceableTextures.description"),
            ) {
                apply(s.copy(dumpReplaceableTextures = it))
            }
            SettingsDivider()
            ToggleRow(
                str("renderer.texturePackOsd.label"),
                s.osdShowTextureReplacements,
                description = str("renderer.texturePackOsd.description"),
            ) {
                apply(s.copy(osdShowTextureReplacements = it))
            }
        }
        SettingsDivider()
        CollapsibleSection(str("renderer.section.blendingAdvanced")) {
            SegmentedRow(
                label = str("renderer.blendingAccuracy.label"),
                options = listOf("Min", "Basic", "Med", "High", "Full", "Max"),
                selectedIndex = s.accurateBlendingUnit.coerceIn(0, 5),
                description = str("renderer.blendingAccuracy.description"),
                onChange = { apply(s.copy(accurateBlendingUnit = it)) },
            )
            // Blending-accuracy companion features (match upstream's grouping under
            // Blending Accuracy). ROV + Accurate Alpha Test apply live; AA1 needs a
            // game restart.
            SettingsDivider()
            ToggleRow(
                str("renderer.rov.label"),
                s.hwRov,
                description = str("renderer.rov.description"),
            ) {
                apply(s.copy(hwRov = it))
            }
            SettingsDivider()
            ToggleRow(
                str("renderer.accurateBlendingFastPath.label"),
                s.adrenoFbFetch,
                description = str("renderer.accurateBlendingFastPath.description"),
            ) {
                apply(s.copy(adrenoFbFetch = it))
            }
            SettingsDivider()
            ToggleRow(
                str("renderer.accurateAlphaTest.label"),
                s.hwAat,
                description = str("renderer.accurateAlphaTest.description"),
            ) {
                apply(s.copy(hwAat = it))
            }
            SettingsDivider()
            ToggleRow(
                str("renderer.hwAa1.label"),
                s.hwAa1,
                description = str("renderer.hwAa1.description"),
            ) {
                apply(s.copy(hwAa1 = it))
            }
            // Hardware & upscaling compatibility fixes now live in the dedicated
            // "Fixes" tab (FixesTab) to keep Render focused on quality/display.
            SettingsDivider()
            ToggleRow(
                str("renderer.hwMipmapping.label"),
                s.hwMipmap,
                description = str("renderer.hwMipmapping.description"),
            ) {
                apply(s.copy(hwMipmap = it))
            }
            SettingsDivider()
            // TriFilter is signed (-1 = Auto). Map enum range onto 0..3.
            val triLabels = listOf("Auto", "Off", "PS2", "Forced")
            val triIdx = (s.triFilter + 1).coerceIn(0, 3)
            SegmentedRow(
                label = str("renderer.trilinear.label"),
                options = triLabels,
                selectedIndex = triIdx,
                description = str("renderer.trilinear.description"),
                onChange = { apply(s.copy(triFilter = it - 1)) },
            )
            SettingsDivider()
            val anisoLabels = listOf("Off", "2x", "4x", "8x", "16x")
            val anisoVals = listOf(0, 2, 4, 8, 16)
            val anisoIdx = anisoVals.indexOf(s.maxAnisotropy).coerceAtLeast(0)
            SegmentedRow(
                label = str("renderer.anisotropic.label"),
                options = anisoLabels,
                selectedIndex = anisoIdx,
                description = str("renderer.anisotropic.description"),
                onChange = { apply(s.copy(maxAnisotropy = anisoVals[it])) },
            )
            SettingsDivider()
            // GPU profile override. Auto resolves at device init via
            // GpuProfileDetector::Resolve (vendor strings + Android ro.soc.*
            // hints). Mali uses ARM_shader_framebuffer_fetch over texture
            // barriers; Adreno uses the EXT fetch / generic path; PowerVR
            // (Imagination) uses EXT/PLS like Adreno but is its own tile-based
            // GPU family. Changing requires a renderer restart — CheckFeatures
            // runs once at device init, so we kick Main.restart() the same way
            // RestartButton does.
            SegmentedRow(
                label = str("renderer.gpuProfile.label"),
                options = listOf("Auto", "Mali", "Adreno", "PowerVR"),
                selectedIndex = s.gpuProfile.coerceIn(0, 3),
                description = str("renderer.gpuProfile.description"),
                onChange = {
                    apply(s.copy(gpuProfile = it))
                },
            )

            SettingsDivider()
            ClearShaderCacheRow()
        }
    }
}

@Composable
private fun TexturePackImportRow() {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    val status = remember { mutableStateOf("") }

    // Shared handler: both the folder and .zip pickers need a booted game (for the
    // serial) and run the copy off the UI thread, reporting the file count the same way.
    fun runImport(doCopy: (String) -> Int) {
        val serial = activeTextureSerial()
        if (serial == null) {
            Toast.makeText(context, I18n.get("renderer.import.bootGameFirst"), Toast.LENGTH_LONG).show()
            return
        }
        scope.launch(Dispatchers.IO) {
            val copied = runCatching { doCopy(serial) }.getOrDefault(-1)
            withContext(Dispatchers.Main) {
                val msg = if (copied >= 0)
                    "Imported $copied texture files for $serial."
                else
                    I18n.get("renderer.import.failed")
                status.value = msg
                Toast.makeText(context, msg, Toast.LENGTH_LONG).show()
            }
        }
    }
    val folderLauncher = rememberLauncherForActivityResult(OpenDocumentTree()) { uri: Uri? ->
        if (uri != null) runImport { s -> importTexturePack(context, uri, s) }
    }
    val zipLauncher = rememberLauncherForActivityResult(OpenDocument()) { uri: Uri? ->
        if (uri != null) runImport { s -> importTexturePackZip(context, uri, s) }
    }

    Column(Modifier.fillMaxWidth()) {
        // Folder import (the pack's folder, with or without a nested "replacements/").
        Box(
            Modifier
                .fillMaxWidth()
                .background(rowAura())
                .clickable { folderLauncher.launch(null) }
                .padding(horizontal = 6.dp, vertical = 5.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Column {
                Text(
                    str("renderer.importTexturePack.label"),
                    color = Color.White,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold,
                )
                Spacer(Modifier.height(2.dp))
                Text(
                    status.value.ifEmpty {
                        activeTextureSerial()?.let { "Copies into textures/$it/replacements" }
                            ?: I18n.get("renderer.importTexturePack.bootFirst")
                    },
                    color = Colors.pasx2_blue,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold,
                )
            }
        }
        // .zip import — extracts the archive into the same per-game replacements folder.
        Box(
            Modifier
                .fillMaxWidth()
                .background(rowAura())
                .clickable {
                    zipLauncher.launch(
                        arrayOf(
                            "application/zip", "application/x-zip-compressed",
                            "application/octet-stream", "*/*",
                        ),
                    )
                }
                .padding(horizontal = 6.dp, vertical = 5.dp),
            contentAlignment = Alignment.CenterStart,
        ) {
            Text(
                str("renderer.importTexturePackZip.label"),
                color = Color.White,
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
            )
        }
    }
}

@Composable
private fun ClearShaderCacheRow() {
    val context = LocalContext.current
    val status = remember { mutableStateOf("") }
    Box(
        Modifier
            .fillMaxWidth()
            .background(rowAura())
            .clickable {
                val n = clearShaderCache(File(Main.assetCopyRoot(context), "cache"))
                status.value = if (n > 0)
                    "Cleared $n shader-cache file${if (n == 1) "" else "s"} — restart the game to rebuild."
                else
                    I18n.get("renderer.clearShaderCache.alreadyEmpty")
                Toast.makeText(context, status.value, Toast.LENGTH_SHORT).show()
            }
            .padding(horizontal = 6.dp, vertical = 5.dp),
        contentAlignment = Alignment.CenterStart,
    ) {
        Column {
            Text(
                str("renderer.clearShaderCache.label"),
                color = Color.White,
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(Modifier.height(2.dp))
            Text(
                status.value.ifEmpty {
                    I18n.get("renderer.clearShaderCache.description")
                },
                color = Colors.pasx2_blue,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

/** Delete the on-disk compiled shader/pipeline caches (Vulkan + GL). They rebuild
 *  on the next renderer init; a stale/mismatched cache (e.g. after a driver change)
 *  can otherwise leave a game rendering corrupt. Returns how many files were removed. */
private fun clearShaderCache(cacheDir: File): Int {
    val names = listOf(
        "vulkan_pipelines.bin", "vulkan_shaders.bin", "vulkan_shaders.idx",
        "gl_programs.bin", "gl_programs.idx",
    )
    var removed = 0
    for (name in names) {
        val f = File(cacheDir, name)
        if (f.isFile && runCatching { f.delete() }.getOrDefault(false)) removed++
    }
    return removed
}

@Composable
private fun GsDumpCaptureRow() {
    val context = LocalContext.current
    Box(
        Modifier
            .fillMaxWidth()
            .background(rowAura())
            .clickable {
                if (Main.eState.value == com.armsx2.EmuState.STOPPED) {
                    Toast.makeText(context, I18n.get("renderer.gsDump.startGameFirst"), Toast.LENGTH_LONG).show()
                } else {
                    runCatching { NativeApp.captureGsDump(1) }
                    Toast.makeText(context, I18n.get("renderer.gsDump.queued"), Toast.LENGTH_LONG).show()
                }
            }
            .padding(horizontal = 6.dp, vertical = 5.dp),
        contentAlignment = Alignment.CenterStart,
    ) {
        Column {
            Text(
                str("renderer.gsDump.label"),
                color = Color.White,
                fontSize = 13.sp,
                fontWeight = FontWeight.SemiBold,
            )
            Spacer(Modifier.height(2.dp))
            Text(
                str("renderer.gsDump.description"),
                color = Colors.pasx2_blue,
                fontSize = 10.sp,
                fontWeight = FontWeight.Bold,
            )
        }
    }
}

private fun activeTextureSerial(): String? {
    return Main.currentGame.value?.serial?.takeIf { it.isNotBlank() }
        ?: runCatching { NativeApp.getGameSerial() }.getOrNull()?.takeIf { it.isNotBlank() }
}

private fun importTexturePack(context: Context, uri: Uri, serial: String): Int {
    val root = DocumentFile.fromTreeUri(context, uri) ?: return -1
    val source = root.findFile("replacements")?.takeIf { it.isDirectory } ?: root
    val dest = File(Main.assetCopyRoot(context), "textures/$serial/replacements")
    if (!dest.exists() && !dest.mkdirs())
        return -1
    return copyDocumentTree(context, source, dest)
}

/** Extract a picked .zip of replacement textures into textures/<serial>/replacements,
 *  preserving the archive's internal folder structure. The native loader scans that
 *  directory RECURSIVELY and matches by hash filename, so a pack that nests its files
 *  (e.g. under its own "replacements/" or a pack-name folder) still resolves. Guards
 *  against Zip-Slip path traversal by rejecting any entry that escapes the dest dir. */
private fun importTexturePackZip(context: Context, zipUri: Uri, serial: String): Int {
    val dest = File(Main.assetCopyRoot(context), "textures/$serial/replacements")
    if (!dest.exists() && !dest.mkdirs())
        return -1
    val destCanon = dest.canonicalPath
    var copied = 0
    context.contentResolver.openInputStream(zipUri)?.use { raw ->
        ZipInputStream(BufferedInputStream(raw)).use { zin ->
            while (true) {
                val e = zin.nextEntry ?: break
                if (!e.isDirectory) {
                    val rel = e.name.replace('\\', '/').trimStart('/')
                    val out = File(dest, rel)
                    // Zip-Slip guard: the resolved path must stay inside dest.
                    val outCanon = out.canonicalPath
                    if (outCanon == destCanon || outCanon.startsWith(destCanon + File.separator)) {
                        out.parentFile?.mkdirs()
                        // Per-entry guard: a mid-copy failure deletes that partial file
                        // and lets the remaining entries still import.
                        val ok = runCatching { out.outputStream().use { zin.copyTo(it) } }.isSuccess
                        if (ok) copied++ else out.delete()
                    }
                }
                zin.closeEntry()
            }
        }
    } ?: return -1
    return copied
}

private fun copyDocumentTree(context: Context, source: DocumentFile, dest: File): Int {
    var copied = 0
    for (child in source.listFiles()) {
        val name = child.name ?: continue
        if (child.isDirectory) {
            val childDest = File(dest, name)
            if (!childDest.exists())
                childDest.mkdirs()
            copied += copyDocumentTree(context, child, childDest)
        } else if (child.isFile) {
            context.contentResolver.openInputStream(child.uri)?.use { input ->
                File(dest, name).outputStream().use { output ->
                    input.copyTo(output)
                }
            } ?: continue
            copied++
        }
    }
    return copied
}
