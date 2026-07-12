package com.armsx2.ui.settings

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.armsx2.config.Settings
import com.armsx2.i18n.str
import com.armsx2.ui.InGameOverlay

/**
 * Hardware / upscaling compatibility fixes — the PCSX2 "Hardware Fixes" and
 * "Upscaling Fixes" panels. Split out of [RendererTab] so Render keeps only
 * core quality/display settings.
 *
 * Every row writes into [Settings] via [InGameOverlay.saveSettings]; on a
 * running VM that reconfigures the GS live (Settings.applyGsLive → native
 * applyGSSettingsLive) so changes show without a restart. Note PCSX2 masks
 * upscaling hacks at native (1x) resolution and masks every UserHacks_* key
 * unless at least one fix is enabled — both are intentional parity behaviours.
 */
@Composable
fun FixesTab(state: MutableState<Settings>) {
    val s = state.value
    val scroll = settingsScrollState()
    ControllerAutoScroll(scroll)

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)

    Column(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        CollapsibleSection(str("fixes.section.display")) {
        HelpText(
            str("fixes.section.display.help"),
            modifier = Modifier.padding(horizontal = 6.dp),
        )
        SettingsDivider()
        ToggleRow(
            str("fixes.antiBlur.label"),
            s.antiBlur,
            description = str("fixes.antiBlur.desc"),
        ) { apply(s.copy(antiBlur = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.screenOffsets.label"),
            s.screenOffsets,
            description = str("fixes.screenOffsets.desc"),
        ) { apply(s.copy(screenOffsets = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.showOverscan.label"),
            s.showOverscan,
            description = str("fixes.showOverscan.desc"),
        ) { apply(s.copy(showOverscan = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disableInterlaceOffset.label"),
            s.disableInterlaceOffset,
            description = str("fixes.disableInterlaceOffset.desc"),
        ) { apply(s.copy(disableInterlaceOffset = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.syncToHostRefresh.label"),
            s.syncToHostRefresh,
            description = str("fixes.syncToHostRefresh.desc"),
        ) { apply(s.copy(syncToHostRefresh = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disableFramebufferFetch.label"),
            s.disableFramebufferFetch,
            description = str("fixes.disableFramebufferFetch.desc"),
        ) { apply(s.copy(disableFramebufferFetch = it)) }
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.overrideTextureBarriers.label"),
            options = listOf(str("fixes.opt.auto"), str("fixes.opt.off"), str("fixes.opt.on")),
            selectedIndex = (s.overrideTextureBarriers + 1).coerceIn(0, 2),
            description = str("fixes.overrideTextureBarriers.desc"),
            onChange = { apply(s.copy(overrideTextureBarriers = it - 1)) },
        )
        SettingsDivider()
        ToggleRow(
            str("fixes.hwAccurateAlphaTest.label"),
            s.hwAccurateAlphaTest,
            description = str("fixes.hwAccurateAlphaTest.desc"),
        ) { apply(s.copy(hwAccurateAlphaTest = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disableVertexShaderExpand.label"),
            s.disableVertexShaderExpand,
            description = str("fixes.disableVertexShaderExpand.desc"),
        ) { apply(s.copy(disableVertexShaderExpand = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.useBlitSwapChain.label"),
            s.useBlitSwapChain,
            description = str("fixes.useBlitSwapChain.desc"),
        ) { apply(s.copy(useBlitSwapChain = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disableShaderCache.label"),
            s.disableShaderCache,
            description = str("fixes.disableShaderCache.desc"),
        ) { apply(s.copy(disableShaderCache = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.integerScaling.label"),
            s.integerScaling,
            description = str("fixes.integerScaling.desc"),
        ) { apply(s.copy(integerScaling = it)) }
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.dithering.label"),
            // "Force 32bit" (native Dithering==3) removes PS2 16-bit color banding — many
            // games look noticeably cleaner with it on.
            options = listOf(str("fixes.opt.off"), str("fixes.opt.scaled"), str("fixes.opt.unscaled"), str("fixes.opt.force32")),
            selectedIndex = s.dithering.coerceIn(0, 3),
            description = str("fixes.dithering.desc"),
            onChange = { apply(s.copy(dithering = it)) },
        )
        SettingsDivider()
        // PCSX2's "Optimal Frame Pacing" checkbox = force the GS-thread frame queue to 0.
        // On -> tightest pacing + lowest input latency; Off -> the default small queue (2),
        // which is smoother on weaker devices at the cost of a little lag.
        ToggleRow(
            str("fixes.optimalFramePacing.label"),
            s.vsyncQueueSize == 0,
            description = str("fixes.optimalFramePacing.desc"),
        ) { apply(s.copy(vsyncQueueSize = if (it) 0 else 2)) }
        }

        CollapsibleSection(str("fixes.section.upscaling")) {
        HelpText(
            str("fixes.section.upscaling.help"),
            modifier = Modifier.padding(horizontal = 6.dp),
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.upscalingFixes.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.normal"), str("fixes.opt.aggr"), str("fixes.opt.normalPlus"), str("fixes.opt.aggrPlus")),
            selectedIndex = s.nativeScaling.coerceIn(0, 4),
            description = str("fixes.upscalingFixes.desc"),
            onChange = { apply(s.copy(nativeScaling = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.halfPixelOffset.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.normal"), str("fixes.opt.special"), str("fixes.opt.aggr"), str("fixes.opt.native"), str("fixes.opt.nwTex")),
            selectedIndex = s.halfPixelOffset.coerceIn(0, 5),
            description = str("fixes.halfPixelOffset.desc"),
            onChange = { apply(s.copy(halfPixelOffset = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.roundSprite.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.half"), str("fixes.opt.full")),
            selectedIndex = s.roundSprite.coerceIn(0, 2),
            description = str("fixes.roundSprite.desc"),
            onChange = { apply(s.copy(roundSprite = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.bilinearDirty.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.normal"), str("fixes.opt.half"), str("fixes.opt.forced")),
            selectedIndex = s.bilinearUpscale.coerceIn(0, 3),
            description = str("fixes.bilinearDirty.desc"),
            onChange = { apply(s.copy(bilinearUpscale = it)) },
        )
        SettingsDivider()
        ToggleRow(
            str("fixes.alignSprite.label"),
            s.alignSprite,
            description = str("fixes.alignSprite.desc"),
        ) { apply(s.copy(alignSprite = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.mergeSprite.label"),
            s.mergeSprite,
            description = str("fixes.mergeSprite.desc"),
        ) { apply(s.copy(mergeSprite = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.wildArmsOffset.label"),
            s.forceEvenSpritePosition,
            description = str("fixes.wildArmsOffset.desc"),
        ) { apply(s.copy(forceEvenSpritePosition = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.unscaledPaletteDraw.label"),
            s.unscaledPaletteDraw,
            description = str("fixes.unscaledPaletteDraw.desc"),
        ) { apply(s.copy(unscaledPaletteDraw = it)) }
        SettingsDivider()
        IntSliderRow(
            label = str("fixes.textureOffsetX.label"),
            value = s.textureOffsetX.coerceIn(0, 1000),
            min = 0,
            max = 1000,
            description = str("fixes.textureOffsetX.desc"),
            onChange = { apply(s.copy(textureOffsetX = it)) },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("fixes.textureOffsetY.label"),
            value = s.textureOffsetY.coerceIn(0, 1000),
            min = 0,
            max = 1000,
            description = str("fixes.textureOffsetY.desc"),
            onChange = { apply(s.copy(textureOffsetY = it)) },
        )
        }

        CollapsibleSection(str("fixes.section.hardware")) {
        HelpText(
            str("fixes.section.hardware.help"),
            modifier = Modifier.padding(horizontal = 6.dp),
        )
        SettingsDivider()
        ToggleRow(
            str("fixes.manualHardwareFixes.label"),
            s.manualUserHacks,
            description = str("fixes.manualHardwareFixes.desc"),
        ) { apply(s.copy(manualUserHacks = it)) }
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.autoFlush.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.sprites"), str("fixes.opt.on")),
            selectedIndex = s.autoFlush.coerceIn(0, 2),
            description = str("fixes.autoFlush.desc"),
            onChange = { apply(s.copy(autoFlush = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.textureInsideRt.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.inside"), str("fixes.opt.merge")),
            selectedIndex = s.textureInsideRt.coerceIn(0, 2),
            description = str("fixes.textureInsideRt.desc"),
            onChange = { apply(s.copy(textureInsideRt = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.gpuTargetClut.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.inside"), str("fixes.opt.forced")),
            selectedIndex = s.gpuTargetClut.coerceIn(0, 2),
            description = str("fixes.gpuTargetClut.desc"),
            onChange = { apply(s.copy(gpuTargetClut = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.cpuSpriteBw.label"),
            options = listOf(str("fixes.opt.off"), "64", "128", "256"),
            selectedIndex = s.cpuSpriteRenderBw.coerceIn(0, 3),
            description = str("fixes.cpuSpriteBw.desc"),
            onChange = { apply(s.copy(cpuSpriteRenderBw = it)) },
        )
        SettingsDivider()
        SegmentedGridRow(
            label = str("fixes.cpuSpriteRender.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.sprite"), str("fixes.opt.triangle"), str("fixes.opt.aggressive"), str("fixes.opt.full"), str("fixes.opt.max")),
            selectedIndex = s.cpuSpriteRenderLevel.coerceIn(0, 5),
            columns = 3,
            description = str("fixes.cpuSpriteRender.desc"),
            onChange = { apply(s.copy(cpuSpriteRenderLevel = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.cpuClutRender.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.normal"), str("fixes.opt.aggr")),
            selectedIndex = s.cpuClutRender.coerceIn(0, 2),
            description = str("fixes.cpuClutRender.desc"),
            onChange = { apply(s.copy(cpuClutRender = it)) },
        )
        SettingsDivider()
        SegmentedRow(
            label = str("fixes.limit24BitDepth.label"),
            options = listOf(str("fixes.opt.off"), str("fixes.opt.upper"), str("fixes.opt.lower")),
            selectedIndex = s.limit24BitDepth.coerceIn(0, 2),
            description = str("fixes.limit24BitDepth.desc"),
            onChange = { apply(s.copy(limit24BitDepth = it)) },
        )
        SettingsDivider()
        ToggleRow(
            str("fixes.gpuPaletteConversion.label"),
            s.gpuPaletteConversion,
            description = str("fixes.gpuPaletteConversion.desc"),
        ) { apply(s.copy(gpuPaletteConversion = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.cpuFramebufferConversion.label"),
            s.cpuFramebufferConversion,
            description = str("fixes.cpuFramebufferConversion.desc"),
        ) { apply(s.copy(cpuFramebufferConversion = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.readTargetsWhenClosing.label"),
            s.readTargetsWhenClosing,
            description = str("fixes.readTargetsWhenClosing.desc"),
        ) { apply(s.copy(readTargetsWhenClosing = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.preloadFrameData.label"),
            s.preloadFrameData,
            description = str("fixes.preloadFrameData.desc"),
        ) { apply(s.copy(preloadFrameData = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.estimateTextureRegion.label"),
            s.estimateTextureRegion,
            description = str("fixes.estimateTextureRegion.desc"),
        ) { apply(s.copy(estimateTextureRegion = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.drawBuffering.label"),
            s.drawBuffering,
            description = str("fixes.drawBuffering.desc"),
        ) { apply(s.copy(drawBuffering = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disableDepthEmulation.label"),
            s.disableDepthEmulation,
            description = str("fixes.disableDepthEmulation.desc"),
        ) { apply(s.copy(disableDepthEmulation = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disablePartialInvalidation.label"),
            s.disablePartialInvalidation,
            description = str("fixes.disablePartialInvalidation.desc"),
        ) { apply(s.copy(disablePartialInvalidation = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disableSafeFeatures.label"),
            s.disableSafeFeatures,
            description = str("fixes.disableSafeFeatures.desc"),
        ) { apply(s.copy(disableSafeFeatures = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.disableRenderFixes.label"),
            s.disableRenderFixes,
            description = str("fixes.disableRenderFixes.desc"),
        ) { apply(s.copy(disableRenderFixes = it)) }
        SettingsDivider()
        IntSliderRow(
            label = str("fixes.skipDrawStart.label"),
            value = s.skipDrawStart.coerceIn(0, 5000),
            min = 0,
            max = 5000,
            description = str("fixes.skipDrawStart.desc"),
            onChange = { apply(s.copy(skipDrawStart = it)) },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("fixes.skipDrawEnd.label"),
            value = s.skipDrawEnd.coerceIn(0, 5000),
            min = 0,
            max = 5000,
            description = str("fixes.skipDrawEnd.desc"),
            onChange = { apply(s.copy(skipDrawEnd = it)) },
        )
        SettingsDivider()
        ToggleRow(
            str("fixes.spinGpuReadbacks.label"),
            s.spinGpuReadbacks,
            description = str("fixes.spinGpuReadbacks.desc"),
        ) { apply(s.copy(spinGpuReadbacks = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.spinCpuReadbacks.label"),
            s.spinCpuReadbacks,
            description = str("fixes.spinCpuReadbacks.desc"),
        ) { apply(s.copy(spinCpuReadbacks = it)) }
        }

        CollapsibleSection(str("fixes.section.software")) {
        HelpText(
            str("fixes.section.software.help"),
            modifier = Modifier.padding(horizontal = 6.dp),
        )
        SettingsDivider()
        ToggleRow(
            str("fixes.autoFlushSw.label"),
            s.autoFlushSw,
            description = str("fixes.autoFlushSw.desc"),
        ) { apply(s.copy(autoFlushSw = it)) }
        SettingsDivider()
        ToggleRow(
            str("fixes.mipmapSw.label"),
            s.mipmapSw,
            description = str("fixes.mipmapSw.desc"),
        ) { apply(s.copy(mipmapSw = it)) }
        SettingsDivider()
        IntSliderRow(
            label = str("fixes.swThreads.label"),
            value = s.swThreads.coerceIn(0, 10),
            min = 0,
            max = 10,
            description = str("fixes.swThreads.desc"),
            onChange = { apply(s.copy(swThreads = it)) },
        )
        SettingsDivider()
        IntSliderRow(
            label = str("fixes.swThreadTileHeight.label"),
            value = s.swThreadsHeight.coerceIn(0, 8),
            min = 0,
            max = 8,
            description = str("fixes.swThreadTileHeight.desc"),
            onChange = { apply(s.copy(swThreadsHeight = it)) },
        )
        }
        Spacer(Modifier.height(8.dp))
    }
}

// CollapsibleSection now lives in SettingsWidgets.kt (shared by the Fixes / Pad /
// Performance / Renderer tabs).
