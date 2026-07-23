package com.armsx2.ui.settings

import androidx.compose.foundation.ScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.verticalScroll
import androidx.compose.runtime.Composable
import androidx.compose.runtime.MutableState
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.armsx2.config.Settings
import com.armsx2.i18n.str
import com.armsx2.ui.InGameOverlay
import androidx.core.content.edit
import kotlin.math.roundToInt

/**
 * Performance section of the in-game settings overlay.
 *
 * Mutates the live [Settings] state and routes the write through
 * [InGameOverlay.saveSettings], which picks the global or per-game
 * storage tier based on the overlay's current scope. Applies via
 * [Settings.applyTo] so toggles take effect immediately on a running
 * VM. Every visible setting maps 1-1 onto an EmuCore key (see Settings
 * field comments for the exact `<section>/<key>`).
 *
 * Column + verticalScroll instead of LazyColumn so the tab can sit
 * inside the wrap-content RootTabs container without needing a hard
 * height bound. List is short (~9 rows) so non-lazy is fine.
 */
@Composable
fun PerformanceTab(state: MutableState<Settings>) {
    val s = state.value
    val scroll = settingsScrollState()
    ControllerAutoScroll(scroll)

    fun apply(updated: Settings) = InGameOverlay.saveSettings(updated)

    Column(
        modifier = Modifier
            .fillMaxWidth(),
    ) {
        // Speedhack profile presets. Equality against s.copy(...) means the
        // segment auto-reflects "Custom" once the user tweaks any speedhack below.
        run {
            val safe = s.copy(eeCycleRate = 0, eeCycleSkip = 0, mtvu = true, vu1Instant = true,
                vuFlagHack = true, intcStat = true, waitLoop = true, fastCDVD = false,
                // Restore the GPU-quality levers the Fast/Low-End presets lower, so
                // Optimal is a COMPLETE reset to recommended defaults — not just the
                // speedhacks (e.g. Texture Preloading back to Full, blending to Basic).
                // Resolution is left as-is so upscalers aren't dropped to native.
                accurateBlendingUnit = 1, hwMipmap = true, texturePreloading = 2, hwRov = false)
            // Fast = speed-first: EE cycle skip + fast CDVD, plus render-side wins
            // that are safe for most games (native resolution + Basic blending).
            val fast = s.copy(eeCycleRate = 0, eeCycleSkip = 2, mtvu = true, vu1Instant = true,
                vuFlagHack = true, intcStat = true, waitLoop = true, fastCDVD = true,
                upscaleFloat = 1.0f, accurateBlendingUnit = 1)
            // Low-End = every cheap GPU/CPU lever, MTVU gated on core count. Built
            // from the shared Settings.lowEndPreset so it matches the setup wizard.
            val lowEnd = Settings.lowEndPreset(
                s.copy(eeCycleRate = 0, mtvu = true, vu1Instant = true,
                    vuFlagHack = true, intcStat = true, waitLoop = true, fastCDVD = true),
                mtvu = com.armsx2.DeviceTier.mtvuDefault(),
            )
            // -1 = no preset matches (custom): no segment highlighted.
            val idx = when (s) { safe -> 0; fast -> 1; lowEnd -> 2; else -> -1 }
            SegmentedRow(
                label = str("perf.speedhackProfile.label"),
                options = listOf(str("perf.speedhackProfile.optimal"), str("perf.speedhackProfile.fast"), str("perf.speedhackProfile.lowEnd")),
                selectedIndex = idx,
                onChange = { when (it) { 0 -> apply(safe); 1 -> apply(fast); 2 -> apply(lowEnd) } },
            )
        }
        HelpText(str("perf.speedhackProfile.help"))
        SettingsDivider()
        // ---- Display Resolution (HW scaler), NetherSX2-style ----------------
        // Shrinks the game's OUTPUT surface (hardware-composer upscales to the
        // screen) to cut GPU present cost, heat and battery. Global pref (not a
        // Settings field) applied live to the active output surface.
        run {
            // Observable state seeded from the raw pref so the segmented control reflects
            // the change LIVE — a plain prefs.getInt() read isn't observed by Compose, so
            // the highlight only moved on menu re-entry.
            val hwScaler = remember { androidx.compose.runtime.mutableStateOf(com.armsx2.runtime.MainActivityRuntime.prefs.getInt("ui.hwScaler", 0)) }
            SegmentedRow(
                label = str("perf.displayResolution.label"),
                options = listOf(str("perf.displayResolution.screen"), str("perf.displayResolution.3xPs2"), str("perf.displayResolution.2xPs2"), str("perf.displayResolution.1xPs2")),
                selectedIndex = when (hwScaler.value) { 3 -> 1; 2 -> 2; 1 -> 3; else -> 0 },
                description = str("perf.displayResolution.description"),
                onChange = {
                    val n = when (it) { 1 -> 3; 2 -> 2; 3 -> 1; else -> 0 }
                    hwScaler.value = n
                    com.armsx2.runtime.MainActivityRuntime.prefs.edit { putInt("ui.hwScaler", n) }
                    com.armsx2.runtime.MainActivityRuntime.surface.value?.applyOutputScale()
                },
            )
        }
        SettingsDivider()
        // ---- Screen resolution override (#398) ------------------------------
        // Forces the game's OUTPUT surface to a fixed 16:9 resolution instead of the
        // detected panel size — fixes 16:10 / mis-detected panels (e.g. reported 1920x1200
        // on a 1080p screen) that squish 16:9 games and widescreen patches. Global pref,
        // live-applied to the output surface; composes with the HW scaler above.
        run {
            val res = remember { androidx.compose.runtime.mutableStateOf(com.armsx2.runtime.MainActivityRuntime.prefs.getString("ui.screenResOverride", "auto") ?: "auto") }
            val presets = listOf("auto", "2560x1440", "1920x1080", "1280x720")
            SegmentedRow(
                label = str("perf.screenRes.label"),
                options = listOf(str("perf.screenRes.auto"), "1440p", "1080p", "720p"),
                selectedIndex = presets.indexOf(res.value).let { if (it >= 0) it else 0 },
                description = str("perf.screenRes.description"),
                onChange = { idx ->
                    val v = presets[idx]
                    res.value = v
                    com.armsx2.runtime.MainActivityRuntime.prefs.edit { putString("ui.screenResOverride", v) }
                    com.armsx2.runtime.MainActivityRuntime.surface.value?.applyOutputScale()
                },
            )
        }
        // ---- Sustained Performance (#128) ---------------------------------------
        // Asks Android to hold a steady, thermally-sustainable clock instead of
        // boost-then-throttle. Better for long sessions on handhelds, but it CAPS the
        // peak clock so peak-hungry games can lose fps — a user choice, default Off.
        // Global pref, applied at launch (MainActivityRuntime.onCreate) and live here via the window.
        run {
            val sustained = remember { androidx.compose.runtime.mutableStateOf(com.armsx2.runtime.MainActivityRuntime.prefs.getBoolean("ui.sustainedPerf", false)) }
            SegmentedRow(
                label = str("perf.sustainedPerformance.label"),
                options = listOf(str("common.off"), str("common.on")),
                selectedIndex = if (sustained.value) 1 else 0,
                description = str("perf.sustainedPerformance.description"),
                onChange = {
                    val on = it == 1
                    sustained.value = on
                    com.armsx2.runtime.MainActivityRuntime.prefs.edit {
                        putBoolean(
                            "ui.sustainedPerf",
                            on
                        )
                    }
                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
                        runCatching {
                            (com.armsx2.runtime.MainActivityRuntime.surface.value?.context as? android.app.Activity)
                                ?.window?.setSustainedPerformanceMode(on)
                        }
                    }
                },
            )
        }
        SettingsDivider()
        CollapsibleSection(str("perf.speedhacks.title"), initiallyExpanded = false) {
            IntSliderRow(
                label = str("perf.eeCycleRate.label"),
                value = s.eeCycleRate,
                min = -3,
                max = 3,
                description = str("perf.eeCycleRate.description"),
                valueFormatter = { rate ->
                    when (rate) {
                        -3 -> "50%"
                        -2 -> "60%"
                        -1 -> "75%"
                        0 -> "100%"
                        1 -> "130%"
                        2 -> "180%"
                        3 -> "300%"
                        else -> "$rate"
                    }
                },
                onChange = { apply(s.copy(eeCycleRate = it)) },
            )
            SettingsDivider()
            IntSliderRow(
                label = str("perf.eeCycleSkip.label"),
                value = s.eeCycleSkip,
                min = 0,
                max = 3,
                description = str("perf.eeCycleSkip.description"),
                onChange = { apply(s.copy(eeCycleSkip = it)) },
            )
            SettingsDivider()
            // Recompiler float-clamping accuracy (PCSX2 parity). Higher = more
            // accurate float handling (fixes SPS / missing geometry / VU glitches)
            // at a speed cost. Needs a recompiler reset, so restart the game.
            SegmentedRow(
                label = str("perf.eeFpuClamping.label"),
                options = listOf(str("perf.clamp.none"), str("perf.clamp.normal"), str("perf.clamp.extra"), str("perf.clamp.full")),
                selectedIndex = s.eeClampMode.coerceIn(0, 3),
                description = str("perf.eeFpuClamping.description"),
                onChange = { apply(s.copy(eeClampMode = it)) },
            )
            SettingsDivider()
            SegmentedRow(
                label = str("perf.vuClamping.label"),
                options = listOf(str("perf.clamp.none"), str("perf.clamp.normal"), str("perf.clamp.extra"), str("perf.clamp.extraSign")),
                selectedIndex = s.vuClampMode.coerceIn(0, 3),
                description = str("perf.vuClamping.description"),
                onChange = { apply(s.copy(vuClampMode = it)) },
            )
            SettingsDivider()
            SegmentedRow(
                label = str("perf.eeFpuRoundMode.label"),
                options = listOf(str("perf.round.nearest"), str("perf.round.negative"), str("perf.round.positive"), str("perf.round.chop")),
                selectedIndex = s.eeFpuRoundMode.coerceIn(0, 3),
                description = str("perf.eeFpuRoundMode.description"),
                onChange = { apply(s.copy(eeFpuRoundMode = it)) },
            )
            SettingsDivider()
            SegmentedRow(
                label = str("perf.vu0RoundMode.label"),
                options = listOf(str("perf.round.nearest"), str("perf.round.negative"), str("perf.round.positive"), str("perf.round.chop")),
                selectedIndex = s.vu0RoundMode.coerceIn(0, 3),
                description = str("perf.vu0RoundMode.description"),
                onChange = { apply(s.copy(vu0RoundMode = it)) },
            )
            SettingsDivider()
            SegmentedRow(
                label = str("perf.vu1RoundMode.label"),
                options = listOf(str("perf.round.nearest"), str("perf.round.negative"), str("perf.round.positive"), str("perf.round.chop")),
                selectedIndex = s.vu1RoundMode.coerceIn(0, 3),
                description = str("perf.vu1RoundMode.description"),
                onChange = { apply(s.copy(vu1RoundMode = it)) },
            )
            SettingsDivider()
            // Speed Limit % — caps emulation speed as a % of native (100 = full speed).
            // Arbitrary value; default stays 100. Affects audio pitch / timing / RA.
            IntSliderRow(
                label = str("perf.speedLimit.label"),
                value = s.nominalSpeedPercent.coerceIn(10, 1000),
                min = 10,
                max = 1000,
                description = str("perf.speedLimit.description"),
                valueFormatter = { "$it%" },
                onChange = { apply(s.copy(nominalSpeedPercent = it)) },
            )
            SettingsDivider()
            // Display FPS Cap — caps the PRESENTED frame rate independently of Speed %.
            // The GS thread paces presents (accumulator) to hold ANY target rate while
            // emulation runs full speed (no slowdown). Arbitrary value; 0 = off.
            IntSliderRow(
                label = str("perf.displayFpsCap.label"),
                value = s.fpsLimit.coerceIn(0, 60),
                min = 0,
                max = 60,
                description = str("perf.displayFpsCap.description"),
                valueFormatter = { if (it == 0) com.armsx2.i18n.I18n.get("common.off") else "$it fps" },
                onChange = { apply(s.copy(fpsLimit = it)) },
            )
            SettingsDivider()
            // Per-region emulated vsync rate (PCSX2 EmuCore/GS FramerateNTSC / FrameratePAL)
            // — the PS2 refresh the game targets. Defaults 60/50 Hz (true 59.94/50.00).
            // Speed Limit % is relative to this; this is the rate, not a display cap.
            IntSliderRow(
                label = str("perf.ntscFramerate.label"),
                value = s.framerateNtsc.roundToInt().coerceIn(20, 75),
                min = 20,
                max = 75,
                description = str("perf.ntscFramerate.description"),
                // The true PS2 NTSC rate is 59.94 Hz, which rounds to the "60" stop.
                // Label that stop honestly and snap it to the exact default, so the
                // canonical rate stays recoverable (an integer slider can't dial 59.94).
                valueFormatter = { if (it == 60) "59.94 Hz" else "$it Hz" },
                onChange = { apply(s.copy(framerateNtsc = if (it == 60) 59.94f else it.toFloat())) },
            )
            SettingsDivider()
            IntSliderRow(
                label = str("perf.palFramerate.label"),
                value = s.frameratePal.roundToInt().coerceIn(20, 75),
                min = 20,
                max = 75,
                description = str("perf.palFramerate.description"),
                valueFormatter = { "$it Hz" },
                onChange = { apply(s.copy(frameratePal = it.toFloat())) },
            )
            SettingsDivider()
            IntSliderRow(
                label = str("perf.frameSkip.label"),
                value = s.frameSkip,
                min = 0,
                max = 5,
                description = str("perf.frameSkip.description"),
                valueFormatter = { if (it == 0) com.armsx2.i18n.I18n.get("common.off") else "Skip $it" },
                onChange = { apply(s.copy(frameSkip = it)) },
            )
        }
        SettingsDivider()
        CollapsibleSection(str("perf.gamedbFixes.title")) {
            HelpText(str("perf.gamedbFixes.help"))
            ToggleRow(str("perf.fix.skipBios"), s.enableFastBoot, description = str("perf.fix.skipBios.desc")) { apply(s.copy(enableFastBoot = it)) }
            ToggleRow(str("perf.fix.gamedbFixes"), s.enableGameFixes, description = str("perf.fix.gamedbFixes.desc")) { apply(s.copy(enableGameFixes = it)) }
            ToggleRow(str("perf.fix.skipMpeg"), s.gamefixSkipMpeg, description = str("perf.fix.skipMpeg.desc")) { apply(s.copy(enableGameFixes = true, gamefixSkipMpeg = it)) }
            if (s.gamefixSkipMpeg) HelpText(str("perf.fix.skipMpeg.warning"))
            ToggleRow(str("perf.fix.fmvSoftware"), s.gamefixSoftwareRendererFmv, description = str("perf.fix.fmvSoftware.desc")) { apply(s.copy(enableGameFixes = true, gamefixSoftwareRendererFmv = it)) }
            ToggleRow(str("perf.fix.eeTiming"), s.gamefixEETiming, description = str("perf.fix.eeTiming.desc")) { apply(s.copy(enableGameFixes = true, gamefixEETiming = it)) }
            ToggleRow(str("perf.fix.instantDma"), s.gamefixInstantDma, description = str("perf.fix.instantDma.desc")) { apply(s.copy(enableGameFixes = true, gamefixInstantDma = it)) }
            ToggleRow(str("perf.fix.blitFps"), s.gamefixBlitInternalFps, description = str("perf.fix.blitFps.desc")) { apply(s.copy(enableGameFixes = true, gamefixBlitInternalFps = it)) }
            ToggleRow(str("perf.fix.fpuMultiply"), s.gamefixFpuMul, description = str("perf.fix.fpuMultiply.desc")) { apply(s.copy(enableGameFixes = true, gamefixFpuMul = it)) }
            ToggleRow(str("perf.fix.ophFlag"), s.gamefixOphFlag, description = str("perf.fix.ophFlag.desc")) { apply(s.copy(enableGameFixes = true, gamefixOphFlag = it)) }
            ToggleRow(str("perf.fix.gifFifo"), s.gamefixGifFifo, description = str("perf.fix.gifFifo.desc")) { apply(s.copy(enableGameFixes = true, gamefixGifFifo = it)) }
            ToggleRow(str("perf.fix.dmaBusy"), s.gamefixDmaBusy, description = str("perf.fix.dmaBusy.desc")) { apply(s.copy(enableGameFixes = true, gamefixDmaBusy = it)) }
            ToggleRow(str("perf.fix.vif1Stall"), s.gamefixVif1Stall, description = str("perf.fix.vif1Stall.desc")) { apply(s.copy(enableGameFixes = true, gamefixVif1Stall = it)) }
            ToggleRow(str("perf.fix.iBit"), s.gamefixIbit, description = str("perf.fix.iBit.desc")) { apply(s.copy(enableGameFixes = true, gamefixIbit = it)) }
            ToggleRow(str("perf.fix.fullVu0Sync"), s.gamefixFullVu0Sync, description = str("perf.fix.fullVu0Sync.desc")) { apply(s.copy(enableGameFixes = true, gamefixFullVu0Sync = it)) }
            ToggleRow(str("perf.fix.vuAddSub"), s.gamefixVuAddSub, description = str("perf.fix.vuAddSub.desc")) { apply(s.copy(enableGameFixes = true, gamefixVuAddSub = it)) }
            ToggleRow(str("perf.fix.vuOverflow"), s.gamefixVuOverflow, description = str("perf.fix.vuOverflow.desc")) { apply(s.copy(enableGameFixes = true, gamefixVuOverflow = it)) }
            ToggleRow(str("perf.fix.extraXgkick"), s.gamefixXgkick, description = str("perf.fix.extraXgkick.desc")) { apply(s.copy(enableGameFixes = true, gamefixXgkick = it)) }
            ToggleRow(str("perf.fix.goemonTlb"), s.gamefixGoemonTlb, description = str("perf.fix.goemonTlb.desc")) { apply(s.copy(enableGameFixes = true, gamefixGoemonTlb = it)) }
            ToggleRow(str("perf.fix.vuSync"), s.gamefixVuSync, description = str("perf.fix.vuSync.desc")) { apply(s.copy(enableGameFixes = true, gamefixVuSync = it)) }
        }
        SettingsDivider()
        CollapsibleSection(str("perf.advancedSpeedhacks.title")) {
            Spacer(Modifier.height(8.dp))
            ToggleRow(str("perf.hack.mtvu"), s.mtvu, description = str("perf.hack.mtvu.desc")) { apply(s.copy(mtvu = it)) }
            ToggleRow(str("perf.hack.instantVu1"), s.vu1Instant, description = str("perf.hack.instantVu1.desc")) { apply(s.copy(vu1Instant = it)) }
            ToggleRow(str("perf.hack.vuFlagHack"), s.vuFlagHack, description = str("perf.hack.vuFlagHack.desc")) { apply(s.copy(vuFlagHack = it)) }
            ToggleRow(str("perf.hack.fastCdvd"), s.fastCDVD, description = str("perf.hack.fastCdvd.desc")) { apply(s.copy(fastCDVD = it)) }
            ToggleRow(str("perf.hack.intcStat"), s.intcStat, description = str("perf.hack.intcStat.desc")) { apply(s.copy(intcStat = it)) }
            ToggleRow(str("perf.hack.waitLoop"), s.waitLoop, description = str("perf.hack.waitLoop.desc")) { apply(s.copy(waitLoop = it)) }
            ToggleRow(str("perf.hack.vuNeonFusions"), s.vuNeonFusions, description = str("perf.hack.vuNeonFusions.desc")) { apply(s.copy(vuNeonFusions = it)) }
            ToggleRow(str("perf.hack.skipVuStallSim"), s.vuSkipStallSim, description = str("perf.hack.skipVuStallSim.desc")) { apply(s.copy(vuSkipStallSim = it)) }
            ToggleRow(str("perf.hack.deferVuWrites"), s.vuDeferredWrites, description = str("perf.hack.deferVuWrites.desc")) { apply(s.copy(vuDeferredWrites = it)) }
            ToggleRow(str("perf.hack.skipDupeFrames"), s.skipDuplicateFrames, description = str("perf.hack.skipDupeFrames.desc")) { apply(s.copy(skipDuplicateFrames = it)) }
        }
    }
}
